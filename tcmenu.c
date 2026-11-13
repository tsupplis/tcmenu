#define _XOPEN_SOURCE 700
#include "tcmenu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <limits.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>

#define MAX_ITEM_LENGTH 256

static int display_initialized = 0;
static int display_owned = 0;
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

void tc_init_display(tc_theme_t theme) {
    if (display_initialized) return;
    initscr();
    display_owned = 1;
    tc_attach_display(theme);
    curs_set(0);
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
}

void tc_attach_display(tc_theme_t theme) {
    if (!display_initialized && stdscr) {
        display_initialized = 1;
        current_theme = theme;
        
        
        /* Only use color pairs after successful initialization. */
        if (theme == TC_THEME_DARK || theme == TC_THEME_LIGHT) {
            short fg = theme == TC_THEME_DARK ? COLOR_WHITE : COLOR_BLACK;
            short bg = theme == TC_THEME_DARK ? COLOR_BLACK : COLOR_WHITE;
            short title_fg = theme == TC_THEME_DARK ? COLOR_GREEN : COLOR_BLACK;
            if (!has_colors() || start_color() == ERR || COLOR_PAIRS <= 5) {
                current_theme = TC_THEME_NOCOLOR;
                return;
            }
            if (theme == TC_THEME_LIGHT && use_default_colors() == OK) bg = -1;
            if (init_pair(COLOR_BACKGROUND, fg, bg) == ERR ||
                init_pair(COLOR_NORMAL, fg, bg) == ERR ||
                init_pair(COLOR_HIGHLIGHT, COLOR_WHITE, COLOR_GREEN) == ERR ||
                init_pair(COLOR_TITLE, title_fg, bg) == ERR ||
                init_pair(COLOR_INSTRUCTIONS, fg, bg) == ERR) {
                current_theme = TC_THEME_NOCOLOR;
                return;
            }
            if (display_owned) {
                bkgd(COLOR_PAIR(COLOR_BACKGROUND));
                clear();
                refresh();
            }
        }

    }
}

void tc_end_display(void) {
    if (display_initialized) {
        if (display_owned) endwin();
        display_owned = 0;
        display_initialized = 0;
    }
}

tc_theme_t tc_get_theme(void) {
    return current_theme;
}

/* Copy complete multibyte characters, measuring terminal columns. */
static char *fit_text(const char *src, int columns, int *used) {
    size_t length = strlen(src), pos = 0, out = 0, n;
    char *dest = malloc(length + (size_t)columns + 4);
    mbstate_t state;
    wchar_t wc;
    int cells = 0, w, limit = columns, truncated = 0;
    if (!dest) return NULL;
    memset(&state, 0, sizeof(state));
    while (pos < length) {
        n = mbrtowc(&wc, src + pos, length - pos, &state);
        if (n == (size_t)-1 || n == (size_t)-2 || n == 0) {
            n = 1; w = 1; memset(&state, 0, sizeof(state));
        } else {
            w = wcwidth(wc);
            if (w < 0) w = 1;
        }
        cells += w; pos += n;
    }
    if (cells > columns) { truncated = 1; limit -= columns >= 3 ? 3 : 0; }
    pos = 0; cells = 0;
    memset(&state, 0, sizeof(state));
    while (pos < length) {
        int invalid = 0;
        n = mbrtowc(&wc, src + pos, length - pos, &state);
        if (n == (size_t)-1 || n == (size_t)-2 || n == 0) {
            n = 1; w = 1; invalid = 1; memset(&state, 0, sizeof(state));
        } else { w = wcwidth(wc); if (w < 0) { w = 1; invalid = 1; } }
        if (cells + w > limit) break;
        if (invalid) dest[out++] = '?';
        else { memcpy(dest + out, src + pos, n); out += n; }
        cells += w; pos += n;
    }
    if (truncated && columns >= 3) { memcpy(dest + out, "...", 3); out += 3; cells += 3; }
    dest[out] = '\0';
    if (used) *used = cells;
    return dest;
}

tc_menu_t *tc_create_menu(const char *title, int row, int col, int width, int height, 
                          char **entries) {
    int i;
    int count;
    char **offset;
    tc_item_t **items;
    tc_menu_t *menu;
    tc_window_t *window;
    tc_window_t *subwindow;
    int max_item_len;
    
    if (!stdscr || !entries || !title || width < 10 || height < 6 ||
        row < 0 || col < 0 || height > LINES - row || width > COLS - col) {
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
    max_item_len = width - 7;
    if (max_item_len < 1) {
        max_item_len = 1;
    }
    if (max_item_len > MAX_ITEM_LENGTH) {
        max_item_len = MAX_ITEM_LENGTH;
    }
    
    /* Create items with proper truncation */
    for (i = 0; i < count; i++) {
        int j;
        char *item_text = fit_text(entries[i], max_item_len, NULL);
        if (!item_text) {
            /* Clean up on allocation failure */
            for (j = 0; j < i; j++) {
                const char *name = item_name(items[j]);
                free_item(items[j]);
                free((void *)name);
            }
            free(items);
            return NULL;
        }
        
        items[i] = new_item(item_text, NULL);
        
        if (!items[i]) {
            int j;
            free(item_text);
            /* Clean up on item creation failure */
            for (j = 0; j < i; j++) {
                const char *name = item_name(items[j]);
                free_item(items[j]);
                free((void *)name);
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
            const char *name = item_name(items[i]);
            free_item(items[i]);
            free((void *)name);
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
            const char *name = item_name(items[i]);
            free_item(items[i]);
            free((void *)name);
        }
        free(items);
        return NULL;
    }
    
    subwindow = derwin(window, height - 4, width - 2, 3, 1);
    if (!subwindow) goto setup_failed;
    if (keypad(window, TRUE) == ERR ||
        set_menu_win(menu, window) != E_OK ||
        set_menu_sub(menu, subwindow) != E_OK ||
        set_menu_format(menu, height - 4, 1) != E_OK ||
        set_menu_mark(menu, " * ") != E_OK) goto setup_failed;
    if ((current_theme == TC_THEME_DARK || current_theme == TC_THEME_LIGHT) && has_colors()) {
        if (set_menu_fore(menu, COLOR_PAIR(COLOR_HIGHLIGHT) | A_BOLD) != E_OK ||
            set_menu_back(menu, COLOR_PAIR(COLOR_NORMAL)) != E_OK ||
            set_menu_grey(menu, COLOR_PAIR(COLOR_NORMAL) | A_DIM) != E_OK ||
            wbkgd(window, COLOR_PAIR(COLOR_NORMAL)) == ERR) goto setup_failed;
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
    
    if ((current_theme == TC_THEME_DARK || current_theme == TC_THEME_LIGHT) && has_colors()) {
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

setup_failed:
    free_menu(menu);
    for (i = 0; i < count; i++) {
        const char *name = item_name(items[i]);
        free_item(items[i]);
        free((void *)name);
    }
    free(items);
    if (subwindow) delwin(subwindow);
    delwin(window);
    return NULL;
}

int tc_query_menu(tc_menu_t *menu, int support_exit) {
    return tc_query_menu_interruptible(menu, support_exit, NULL);
}

int tc_query_menu_interruptible(tc_menu_t *menu, int support_exit,
                                const volatile sig_atomic_t *interrupted) {
    int c, result = -1;
    WINDOW *win;
    
    if (!menu) {
        return -1;
    }
    
    win = menu_win(menu);
    if (post_menu(menu) != E_OK) return -1;
    touchwin(win);
    if (interrupted) wtimeout(win, 100);
    while (!interrupted || !*interrupted) {
        if (wrefresh(win) == ERR) break;
        errno = 0;
        c = wgetch(win);
        if (c == ERR) {
            struct pollfd input;
            input.fd = STDIN_FILENO;
            input.events = POLLIN;
            input.revents = 0;
            if (interrupted && (errno == 0 || errno == EINTR) &&
                poll(&input, 1, 0) == 0 &&
                !(input.revents & (POLLHUP | POLLERR | POLLNVAL))) continue;
            break;
        }
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
                    result = -3;
                    goto done;
                }
                break;
            case 10:        /* Enter key */
            case KEY_ENTER:
                result = item_index(current_item(menu));
                goto done;
        }
    }
    
done:
    if (interrupted) { wtimeout(win, -1); if (*interrupted) result = -1; }
    unpost_menu(menu);
    return result;
}

void tc_free_menu_resources(tc_menu_t *menu) {
    WINDOW *window, *subwindow;
    ITEM **items;
    int count;
    int i;
    
    if (!menu) {
        return;
    }
    
    window = menu_win(menu);
    subwindow = menu_sub(menu);
    items = menu_items(menu);
    count = item_count(menu);
    
    unpost_menu(menu);
    free_menu(menu);
    
    for (i = 0; i < count; i++) {
        if (items[i]) {
            const char *name = item_name(items[i]);
            free_item(items[i]);
            free((void *)name);
        }
    }
    free(items);
    if (subwindow && subwindow != window) delwin(subwindow);
    if (window) delwin(window);
}

/* Internal helper function */
static void print_in_middle(tc_window_t *win, int starty, int startx, int width, 
                           const char *string, chtype color) {
    int length;
    char *text = fit_text(string, width - 2, &length);
    if (!text) return;
    wattron(win, color);
    mvwaddstr(win, starty, startx + (width - length) / 2, text);
    wattroff(win, color);
    free(text);
}
