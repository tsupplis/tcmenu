#include "tcmenu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STATE_START 0
#define STATE_ENTRY 1
#define STATE_DEFAULT 2
#define STATE_FD 3
#define STATE_TITLE 4
#define STATE_THEME 5

static void print_usage(const char *program_name) {
    fprintf(stderr, "Usage: %s [OPTIONS]\n\n", program_name);
    fprintf(stderr, "A terminal-based menu system for interactive selection.\n\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -t, --title TEXT       Set menu title\n");
    fprintf(stderr, "  -e, --entry            Start menu entries (all following args are entries)\n");
    fprintf(stderr, "  -d, --default N        Set default selection to N\n");
    fprintf(stderr, "  -f, --fd N             Output result to file descriptor N\n");
    fprintf(stderr, "  -i, --interactive      Force interactive mode\n");
    fprintf(stderr, "  --theme THEME          Set color theme: dark, light, nocolor, or simple\n");
    fprintf(stderr, "                         (default: auto-detect based on TERM)\n");
    fprintf(stderr, "  -h, --help             Display this help message\n\n");
    fprintf(stderr, "Example:\n");
    fprintf(stderr, "  %s --title \"Select an option\" --entry \"Option 1\" \"Option 2\" \"Option 3\"\n", program_name);
    fprintf(stderr, "  %s --theme light --title \"Menu\" --entry \"Option A\" \"Option B\"\n\n", program_name);
}

static tc_theme_t detect_theme_from_term(void) {
    const char *term = getenv("TERM");
    
    if (!term) {
        return TC_THEME_NOCOLOR;
    }
    
    /* Check for terminals without color support first */
    if (strcmp(term, "dumb") == 0 || 
        strcmp(term, "unknown") == 0 ||
        strcmp(term, "xterm") == 0 ||
        strstr(term, "vt") != NULL ||
        strstr(term, "mono") != NULL) {
        return TC_THEME_NOCOLOR;
    }
    
    /* Check for color terminals that typically use light backgrounds */
    if (strstr(term, "xterm-color") || 
        strstr(term, "xterm-256color") ||
        strcmp(term, "ansi") == 0) {
        return TC_THEME_DARK;
    }
    
    /* Default to dark theme for modern terminals with color support */
    return TC_THEME_NOCOLOR;
}

static int match_arg(const char *arg, const char *short_opt, const char *long_opt) {
    return (strcmp(arg, short_opt) == 0 || strcmp(arg, long_opt) == 0);
}

int main(int argc, char **argv) {
    int result = 0;
    const char *title = "";
    int silent = 0;
    int default_value = 0;
    tc_menu_t *my_menu;
    int width = 50;
    int height = 15;
    char **choices;
    int count = 0;
    int fd = 2; /* Default to stderr */
    int cmdline_state = STATE_START;
    int i;
    FILE *out = stderr;
    tc_theme_t theme = detect_theme_from_term();
    
    /* Allocate choices array */
    choices = (char **)calloc(argc + 1, sizeof(char *));
    if (!choices) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return -2;
    }
    
    /* Parse command line arguments */
    for (i = 1; i < argc; i++) {
        switch (cmdline_state) {
        case STATE_START:
            if (match_arg(argv[i], "-h", "--help")) {
                print_usage(argv[0]);
                free(choices);
                return 0;
            } else if (match_arg(argv[i], "-i", "--interactive")) {
                silent = 0;
            } else if (match_arg(argv[i], "-d", "--default")) {
                cmdline_state = STATE_DEFAULT;
            } else if (match_arg(argv[i], "-f", "--fd")) {
                cmdline_state = STATE_FD;
            } else if (match_arg(argv[i], "-e", "--entry")) {
                cmdline_state = STATE_ENTRY;
            } else if (match_arg(argv[i], "-t", "--title")) {
                cmdline_state = STATE_TITLE;
            } else if (strcmp(argv[i], "--theme") == 0) {
                cmdline_state = STATE_THEME;
            } else {
                fprintf(stderr, "Unknown option: %s\n", argv[i]);
                print_usage(argv[0]);
                free(choices);
                return -1;
            }
            break;
        case STATE_FD:
            fd = atoi(argv[i]);
            out = fdopen(fd, "w");
            if (!out) {
                out = stderr;
            }
            cmdline_state = STATE_START;
            break;
        case STATE_ENTRY:
            choices[count++] = argv[i];
            break;
        case STATE_TITLE:
            title = argv[i];
            cmdline_state = STATE_START;
            break;
        case STATE_DEFAULT:
            default_value = atoi(argv[i]);
            cmdline_state = STATE_START;
            break;
        case STATE_THEME:
            if (strcmp(argv[i], "dark") == 0) {
                theme = TC_THEME_DARK;
            } else if (strcmp(argv[i], "light") == 0) {
                theme = TC_THEME_LIGHT;
            } else if (strcmp(argv[i], "nocolor") == 0) {
                theme = TC_THEME_NOCOLOR;
            } else if (strcmp(argv[i], "simple") == 0) {
                theme = TC_THEME_SIMPLE;
            } else {
                fprintf(stderr, "Unknown theme: %s (use: dark, light, nocolor, or simple)\n", argv[i]);
                free(choices);
                return -1;
            }
            cmdline_state = STATE_START;
            break;
        }
    }
    
    /* Validate we have menu entries */
    if (count == 0) {
        fprintf(out, "-2\n");
        free(choices);
        return -2;
    }
    
    /* Handle silent mode */
    if (silent == 1) {
        fprintf(out, "%d\n", default_value);
        free(choices);
        return default_value;
    }
    
    /* NULL-terminate choices array */
    choices[count] = NULL;
    
    /* Initialize display with selected theme */
    tc_init_display(theme);
    
    /* Calculate menu dimensions */
    width = COLS;
    if (width > 200) {
        width = 200;
    }
    width -= 10;
    height = LINES;
    height -= 12;
    
    /* Ensure minimum dimensions */
    if (height < 10) {
        height = 10;
    }
    if (width < 40) {
        width = 40;
    }
    
    /* Create the menu */
    my_menu = tc_create_menu(title, 5, 5, width, height, choices);
    if (!my_menu) {
        tc_end_display();
        fprintf(out, "-2\n");
        free(choices);
        return -2;
    }
    
    /* Ensure background is painted */
    if (tc_get_theme() != TC_THEME_NOCOLOR && has_colors()) {
        touchwin(stdscr);
        refresh();
    }
    
    /* Display instructions */
    if (tc_get_theme() != TC_THEME_NOCOLOR && has_colors()) {
        attron(COLOR_PAIR(5) | A_BOLD);
    }
    mvprintw(LINES - 4, 2, "Use PageUp and PageDown to scroll down or up a page of items");
    mvprintw(LINES - 3, 2, "Arrow Keys to navigate (X to Exit)");
    if (tc_get_theme() != TC_THEME_NOCOLOR && has_colors()) {
        attroff(COLOR_PAIR(5) | A_BOLD);
    }
    refresh();
    
    /* Query the menu - always support X to exit */
    result = tc_query_menu(my_menu, 1);
    
    /* Clean up */
    tc_free_menu_resources(my_menu);
    tc_end_display();
    
    /* Output result */
    fprintf(out, "%d\n", result);
    
    free(choices);
    return result;
}
