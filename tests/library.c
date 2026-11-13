#define _XOPEN_SOURCE 700
#include <assert.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include "../tcmenu.h"

static int allocations, windows, fail_subwindow, fail_setup;
static void *tracked_malloc(size_t n) {
    void *p = malloc(n);
    if (p) allocations++;
    return p;
}
static void *tracked_calloc(size_t n, size_t size) {
    void *p = calloc(n, size);
    if (p) allocations++;
    return p;
}
static void tracked_free(void *p) { if (p) allocations--; free(p); }
static WINDOW *tracked_newwin(int h, int w, int y, int x) {
    WINDOW *p = newwin(h, w, y, x);
    if (p) windows++;
    return p;
}
static WINDOW *tracked_derwin(WINDOW *parent, int h, int w, int y, int x) {
    WINDOW *p;
    if (fail_subwindow) return NULL;
    p = derwin(parent, h, w, y, x);
    if (p) windows++;
    return p;
}
static int tracked_delwin(WINDOW *p) {
    int result = delwin(p);
    assert(result == OK);
    windows--;
    return result;
}
static int checked_set_menu_sub(MENU *m, WINDOW *w) {
    return fail_setup ? E_SYSTEM_ERROR : set_menu_sub(m, w);
}
#define malloc tracked_malloc
#define calloc tracked_calloc
#define free tracked_free
#define newwin tracked_newwin
#define derwin tracked_derwin
#define delwin tracked_delwin
#define set_menu_sub checked_set_menu_sub
#include "../tcmenu.c"

int main(void) {
    FILE *input = tmpfile(), *output = tmpfile();
    SCREEN *screen;
    MENU *menu;
    char *entries[] = {"First", "Second", NULL};
    char *text;
    int i, columns;
    volatile sig_atomic_t stop = 0;
    assert(input && output);
    assert(setlocale(LC_ALL, "") != NULL);
    text = fit_text("\303\251\303\251\303\251\303\251", 4, &columns);
    assert(text && strcmp(text, "\303\251\303\251\303\251\303\251") == 0 && columns == 4);
    free(text);
    text = fit_text("\347\225\214\347\225\214\347\225\214", 5, &columns);
    assert(text && strcmp(text, "\347\225\214...") == 0 && columns == 5);
    free(text);
    screen = newterm("xterm", output, input);
    assert(screen);
    tc_attach_display(TC_THEME_SIMPLE);
    for (i = 0; i < 1000; i++) {
        menu = tc_create_menu("Title", 0, 0, 40, 10, entries);
        assert(menu);
        assert(ungetch('\n') == OK);
        assert(tc_query_menu(menu, 0) == 0);
        tc_free_menu_resources(menu);
        assert(allocations == 0 && windows == 0);
    }
    fail_subwindow = 1;
    assert(!tc_create_menu("Title", 0, 0, 40, 10, entries));
    assert(allocations == 0 && windows == 0);
    fail_subwindow = 0; fail_setup = 1;
    assert(!tc_create_menu("Title", 0, 0, 40, 10, entries));
    assert(allocations == 0 && windows == 0);
    fail_setup = 0;
    menu = tc_create_menu("Title", 0, 0, 40, 10, entries);
    assert(menu);
    /* newterm input is at EOF: query must return, not spin. */
    assert(tc_query_menu(menu, 0) == -1);
    assert(tc_query_menu_interruptible(menu, 0, &stop) == -1);
    stop = 1;
    assert(tc_query_menu_interruptible(menu, 0, &stop) == -1);
    tc_free_menu_resources(menu);
    tc_end_display();
    assert(!isendwin()); /* Caller still owns the curses screen. */
    endwin();
    delscreen(screen);
    fclose(input); fclose(output);
    assert(allocations == 0 && windows == 0);
    puts("Library resource and failure checks passed");
    return 0;
}
