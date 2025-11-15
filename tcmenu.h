#ifndef TCMENU_H
#define TCMENU_H

#include <ncurses.h>
#include <menu.h>

/* Menu API types */
typedef MENU tc_menu_t;
typedef ITEM tc_item_t;
typedef WINDOW tc_window_t;

/* Theme types */
typedef enum {
    TC_THEME_DARK,
    TC_THEME_LIGHT,
    TC_THEME_NOCOLOR
} tc_theme_t;

/**
 * Initialize the display system (ncurses) with a specific theme.
 * Must be called before creating any menus.
 * 
 * @param theme The theme to use (dark, light, or nocolor)
 */
void tc_init_display(tc_theme_t theme);

/**
 * Clean up and end the display system.
 * Should be called before program exit.
 */
void tc_end_display(void);

/**
 * Create a menu with specified parameters.
 * 
 * @param title Menu title to display
 * @param row Row position of the menu window
 * @param col Column position of the menu window
 * @param width Width of the menu window
 * @param height Height of the menu window
 * @param entries NULL-terminated array of menu entry strings
 * @return Pointer to created menu, or NULL on failure
 */
tc_menu_t *tc_create_menu(const char *title, int row, int col, int width, int height, char **entries);

/**
 * Display and interact with a menu.
 * 
 * @param menu The menu to display
 * @param support_exit If non-zero, allow 'X' key to exit with -3
 * @return Selected item index, -3 for exit, or 0 on error
 */
int tc_query_menu(tc_menu_t *menu, int support_exit);

/**
 * Free menu resources.
 * 
 * @param menu The menu to free
 */
void tc_free_menu_resources(tc_menu_t *menu);

/**
 * Get the current theme.
 * 
 * @return The current theme
 */
tc_theme_t tc_get_theme(void);

#endif /* TCMENU_H */
