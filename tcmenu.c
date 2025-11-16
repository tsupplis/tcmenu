#include "tcmenu.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define MAX_ITEM_LENGTH 256

static int display_initialized = 0;
static tc_theme_t current_theme = TC_THEME_DARK;

/* Color pair definitions */
enum {
    COLOR_BACKGROUND = 1,
    COLOR_NORMAL = 2,
    COLOR_HIGHLIGHT = 3,
    COLOR_TITLE = 4,
    COLOR_INSTRUCTIONS = 5
};

/* Forward declaration for internal function */
static void print_in_middle(tc_window_t *win, int starty, int startx, int width, 
                            const char *string, chtype color);

/* Signal handler wrapper */
static void signal_handler(int sig) {
    (void)sig; /* Unused parameter */
    tc_end_display();
    exit(0);
}

void tc_init_display(tc_theme_t theme) {
    if (!display_initialized) {
        display_initialized = 1;
        current_theme = theme;
        
        initscr();
        curs_set(0);
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        
        /* Initialize colors based on theme */
        if (has_colors() && theme != TC_THEME_NOCOLOR && theme != TC_THEME_SIMPLE) {
            start_color();
            
            if (theme == TC_THEME_DARK) {
                /* Dark theme: bright white on black */
                init_pair(COLOR_BACKGROUND, COLOR_WHITE, COLOR_BLACK);
                init_pair(COLOR_NORMAL, COLOR_WHITE, COLOR_BLACK);
                init_pair(COLOR_HIGHLIGHT, COLOR_WHITE, COLOR_GREEN);
                init_pair(COLOR_TITLE, COLOR_GREEN, COLOR_BLACK);
                init_pair(COLOR_INSTRUCTIONS, COLOR_WHITE, COLOR_BLACK);
                
                /* Paint entire screen with black background */
                assume_default_colors(COLOR_WHITE, COLOR_BLACK);
            } else if (theme == TC_THEME_LIGHT) {
                /* Light theme: black on bright white (using -1 for default terminal white) */
                use_default_colors();
                init_pair(COLOR_BACKGROUND, COLOR_BLACK, -1);
                init_pair(COLOR_NORMAL, COLOR_BLACK, -1);
                init_pair(COLOR_HIGHLIGHT, COLOR_WHITE, COLOR_GREEN);
                init_pair(COLOR_TITLE, COLOR_BLACK, -1);
                init_pair(COLOR_INSTRUCTIONS, COLOR_BLACK, -1);
                
                /* Paint entire screen with terminal's default background (bright white) */
                assume_default_colors(COLOR_BLACK, -1);
            }
            
            /* Clear and fill screen with the default colors */
            bkgd(COLOR_PAIR(0));
            clear();
            refresh();
        }
        
        cbreak();
        noecho();
        keypad(stdscr, TRUE);
    }
}

void tc_end_display(void) {
    if (display_initialized) {
        endwin();
        display_initialized = 0;
    }
}

tc_theme_t tc_get_theme(void) {
    return current_theme;
}

/**
 * Safely truncate a string with ellipsis if it exceeds max_len.
 * Pads with spaces to fill max_len.
 * Ensures the result is properly null-terminated and fits within max_len.
 */
static void truncate_with_ellipsis(char *dest, const char *src, size_t max_len) {
    if (max_len < 4) {
        /* Not enough space even for "..." */
        if (max_len > 0) {
            dest[0] = '\0';
        }
        return;
    }
    
    size_t src_len = strlen(src);
    if (src_len < max_len - 1) {
        /* String fits, copy it and pad with spaces */
        strncpy(dest, src, max_len - 1);
        dest[max_len - 1] = '\0';
        /* Pad with spaces to fill the width */
        for (size_t i = src_len; i < max_len - 1; i++) {
            dest[i] = ' ';
        }
        dest[max_len - 1] = '\0';
    } else {
        /* Truncate and add ellipsis */
        strncpy(dest, src, max_len - 4);
        dest[max_len - 4] = '\0';
        strcat(dest, "...");
    }
}

tc_menu_t *tc_create_menu(const char *title, int row, int col, int width, int height, 
                          char **entries) {
    int i;
    int count;
    char **offset;
    tc_item_t **items;
    tc_menu_t *menu;
    tc_window_t *window;
    
    if (!entries || !title || width < 10 || height < 6) {
        return NULL;
    }
    
    /* Count entries */
    offset = entries;
    count = 0;
    while (*offset) {
        offset++;
        count++;
    }
    
    if (count == 0) {
        return NULL;
    }
    
    /* Allocate items array */
    items = (tc_item_t **)calloc(count + 1, sizeof(tc_item_t *));
    if (!items) {
        return NULL;
    }
    
    /* Calculate maximum item length based on window width */
    /* Account for: mark " * " (3 chars) + padding (2 chars) + borders (2 chars) */
    int max_item_len = width - 7;
    if (max_item_len < 1) {
        max_item_len = 1;
    }
    if (max_item_len > MAX_ITEM_LENGTH) {
        max_item_len = MAX_ITEM_LENGTH;
    }
    
    /* Create items with proper truncation */
    for (i = 0; i < count; i++) {
        char *item_text = (char *)calloc(max_item_len + 1, sizeof(char));
        if (!item_text) {
            /* Clean up on allocation failure */
            for (int j = 0; j < i; j++) {
                free((void *)item_name(items[j]));
                free_item(items[j]);
            }
            free(items);
            return NULL;
        }
        
        truncate_with_ellipsis(item_text, entries[i], max_item_len + 1);
        items[i] = new_item(item_text, NULL);
        
        if (!items[i]) {
            free(item_text);
            /* Clean up on item creation failure */
            for (int j = 0; j < i; j++) {
                free((void *)item_name(items[j]));
                free_item(items[j]);
            }
            free(items);
            return NULL;
        }
    }
    items[count] = NULL;
    
    /* Create menu */
    menu = new_menu(items);
    if (!menu) {
        for (i = 0; i < count; i++) {
            free((void *)item_name(items[i]));
            free_item(items[i]);
        }
        free(items);
        return NULL;
    }
    
    /* Create window */
    window = newwin(height, width, row, col);
    if (!window) {
        unpost_menu(menu);
        free_menu(menu);
        for (i = 0; i < count; i++) {
            free((void *)item_name(items[i]));
            free_item(items[i]);
        }
        free(items);
        return NULL;
    }
    
    keypad(window, TRUE);
    
    /* Configure menu window and subwindow */
    set_menu_win(menu, window);
    set_menu_sub(menu, derwin(window, height - 4, width - 2, 3, 1));
    set_menu_format(menu, height - 4, 1);
    
    /* Set menu mark */
    set_menu_mark(menu, " * ");
    
    /* Set colors based on theme */
    if (current_theme != TC_THEME_NOCOLOR && has_colors()) {
        set_menu_fore(menu, COLOR_PAIR(COLOR_HIGHLIGHT) | A_BOLD);
        set_menu_back(menu, COLOR_PAIR(COLOR_NORMAL));
        set_menu_grey(menu, COLOR_PAIR(COLOR_NORMAL) | A_DIM);
        
        /* Set window background */
        wbkgd(window, COLOR_PAIR(COLOR_NORMAL));
    }
    
    /* Draw border and title */
    if (current_theme != TC_THEME_SIMPLE) {
        mvwaddch(window, 0, 0, ACS_ULCORNER);
        mvwhline(window, 0, 1, ACS_HLINE, width - 2);
        mvwaddch(window, 0, width - 1, ACS_URCORNER);
        mvwvline(window, 1, 0, ACS_VLINE, height - 2);
        mvwvline(window, 1, width - 1, ACS_VLINE, height - 2);
        mvwaddch(window, height - 1, 0, ACS_LLCORNER);
        mvwhline(window, height - 1, 1, ACS_HLINE, width - 2);
        mvwaddch(window, height - 1, width - 1, ACS_LRCORNER);
    }
    
    if (current_theme != TC_THEME_NOCOLOR && has_colors()) {
        print_in_middle(window, 1, 0, width, title, COLOR_PAIR(COLOR_TITLE) | A_BOLD);
    } else {
        print_in_middle(window, 1, 0, width, title, A_BOLD);
    }
    if (current_theme != TC_THEME_SIMPLE) {
        mvwaddch(window, 2, 0, ACS_LTEE);
        mvwhline(window, 2, 1, ACS_HLINE, width - 2);
        mvwaddch(window, 2, width - 1, ACS_RTEE);
    }
    
    return menu;
}

int tc_query_menu(tc_menu_t *menu, int support_exit) {
    int c;
    
    if (!menu) {
        return -1;
    }
    
    post_menu(menu);
    touchwin(menu_win(menu));
    wrefresh(menu_win(menu));
    
    while ((c = wgetch(menu_win(menu))) != 0) {
        switch (c) {
            case KEY_DOWN:
                menu_driver(menu, REQ_DOWN_ITEM);
                break;
            case KEY_UP:
                menu_driver(menu, REQ_UP_ITEM);
                break;
            case KEY_NPAGE:
                menu_driver(menu, REQ_SCR_DPAGE);
                break;
            case KEY_PPAGE:
                menu_driver(menu, REQ_SCR_UPAGE);
                break;
            case 'x':
            case 'X':
                if (support_exit) {
                    return -3;
                }
                break;
            case 10:        /* Enter key */
            case KEY_ENTER:
                return item_index(current_item(menu));
        }
        wrefresh(menu_win(menu));
    }
    
    unpost_menu(menu);
    refresh();
    return 0;
}

void tc_free_menu_resources(tc_menu_t *menu) {
    if (!menu) {
        return;
    }
    
    ITEM **items = menu_items(menu);
    int count = item_count(menu);
    
    unpost_menu(menu);
    free_menu(menu);
    
    for (int i = 0; i < count; i++) {
        if (items[i]) {
            free((void *)item_name(items[i]));
            free_item(items[i]);
        }
    }
}

/* Internal helper function */
static void print_in_middle(tc_window_t *win, int starty, int startx, int width, 
                           const char *string, chtype color) {
    int length, x, y;
    int temp;
    
    if (win == NULL) {
        win = stdscr;
    }
    
    getyx(win, y, x);
    
    if (startx != 0) {
        x = startx;
    }
    if (starty != 0) {
        y = starty;
    }
    if (width == 0) {
        width = 80;
    }
    
    length = strlen(string);
    temp = (width - length) / 2;
    x = startx + temp;
    
    wattron(win, color);
    mvwprintw(win, y, x, "%s", string);
    wattroff(win, color);
}
