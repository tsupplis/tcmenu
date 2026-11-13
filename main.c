#define _POSIX_C_SOURCE 200809L
#include "tcmenu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>

#define STATE_START 0
#define STATE_ENTRY 1
#define STATE_DEFAULT 2
#define STATE_FD 3
#define STATE_TITLE 4
#define STATE_THEME 5

static volatile sig_atomic_t interrupted = 0;

static void handle_signal(int sig) { interrupted = sig; }

static int parse_number(const char *text, int *value) {
    char *end;
    long number;
    errno = 0;
    number = strtol(text, &end, 10);
    if (errno || !*text || *end || number < 0 || number > INT_MAX) return 0;
    *value = (int)number;
    return 1;
}

static void print_usage(const char *program_name) {
    fprintf(stderr, "Usage: %s [OPTIONS]\n\n", program_name);
    fprintf(stderr, "A terminal-based menu system for interactive selection.\n\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -t, --title TEXT       Set menu title\n");
    fprintf(stderr, "  -e, --entry            Start menu entries (all following args are entries)\n");
    fprintf(stderr, "  -x, --exit             Allow X to exit\n");
    fprintf(stderr, "  -d, --default N        Return zero-based N (preselect with -i)\n");
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
        strstr(term, "vt") != NULL ||
        strstr(term, "mono") != NULL) {
        return TC_THEME_NOCOLOR;
    }
    
    /* Default to dark theme for modern terminals with color support */
    return TC_THEME_DARK;
}

static int match_arg(const char *arg, const char *short_opt, const char *long_opt) {
    return (strcmp(arg, short_opt) == 0 || strcmp(arg, long_opt) == 0);
}

int main(int argc, char **argv) {
    int result = 0;
    const char *title = "";
    int force_interactive = 0, have_default = 0, support_exit = 0;
    int row, col;
    struct sigaction action, old_int, old_term;
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
    
    setlocale(LC_ALL, "");

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
                force_interactive = 1;
            } else if (match_arg(argv[i], "-x", "--exit")) {
                support_exit = 1;
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
            if (!parse_number(argv[i], &fd) || !(out = fdopen(fd, "w"))) {
                fprintf(stderr, "Invalid output descriptor: %s\n", argv[i]);
                free(choices);
                return 2;
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
            if (!parse_number(argv[i], &default_value)) {
                fprintf(stderr, "Invalid default: %s\n", argv[i]);
                free(choices);
                return 2;
            }
            have_default = 1;
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
    
    if (cmdline_state != STATE_START && cmdline_state != STATE_ENTRY) {
        fprintf(stderr, "Missing option argument\n");
        free(choices);
        return 2;
    }
    if (have_default && default_value >= count) {
        fprintf(stderr, "Default selection is outside the entry range\n");
        free(choices);
        return 2;
    }
    /* Validate we have menu entries */
    if (count == 0) {
        fprintf(out, "-2\n");
        free(choices);
        return -2;
    }
    
    /* Handle silent mode */
    if (have_default && !force_interactive) {
        fprintf(out, "%d\n", default_value);
        free(choices);
        return default_value;
    }
    
    /* NULL-terminate choices array */
    choices[count] = NULL;
    
    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_signal;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, &old_int) < 0) {
        tc_end_display(); free(choices); return 2;
    }
    if (sigaction(SIGTERM, &action, &old_term) < 0) {
        sigaction(SIGINT, &old_int, NULL);
        tc_end_display(); free(choices); return 2;
    }

    /* Handlers must be installed before curses initialization. */
    tc_init_display(theme);

    width = COLS > 200 ? 200 : COLS;
    height = LINES >= 8 ? LINES - 2 : LINES;
    if (height > 15) height = 15;
    row = (LINES - (LINES >= 8 ? 2 : 0) - height) / 2;
    col = (COLS - width) / 2;
    my_menu = tc_create_menu(title, row, col, width, height, choices);
    if (!my_menu) {
        tc_end_display();
        sigaction(SIGINT, &old_int, NULL);
        sigaction(SIGTERM, &old_term, NULL);
        if (!interrupted) fprintf(out, "-2\n");
        free(choices);
        return interrupted ? 128 + interrupted : -2;
    }
    
    if (set_current_item(my_menu, menu_items(my_menu)[default_value]) != E_OK) {
        result = -1;
    } else {
        if (LINES >= 8) {
            mvaddnstr(LINES - 2, 0, "PageUp/PageDown to scroll,", COLS - 1);
            mvaddnstr(LINES - 1, 0, support_exit ?
                      "Arrows to navigate, Enter to select and 'X' exit" :
                      "Arrows to navigate and Enter to select", COLS - 1);
        }
        refresh();
        result = tc_query_menu_interruptible(my_menu, support_exit, &interrupted);
    }

    /* Clean up */
    tc_free_menu_resources(my_menu);
    tc_end_display();
    
    sigaction(SIGINT, &old_int, NULL);
    sigaction(SIGTERM, &old_term, NULL);
    if (interrupted) { free(choices); return 128 + interrupted; }

    /* Output result */
    fprintf(out, "%d\n", result);
    
    free(choices);
    return result;
}
