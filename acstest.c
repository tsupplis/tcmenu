#include <ncurses.h>

int main(void) {
    initscr();
    box(stdscr, 0, 0);
    
    mvprintw(1, 2, "ACS_ULCORNER: %c  ACS_URCORNER: %c", ACS_ULCORNER, ACS_URCORNER);
    mvprintw(2, 2, "ACS_LLCORNER: %c  ACS_LRCORNER: %c", ACS_LLCORNER, ACS_LRCORNER);
    mvprintw(3, 2, "ACS_HLINE:    %c  ACS_VLINE:    %c", ACS_HLINE, ACS_VLINE);
    mvprintw(4, 2, "ACS_LTEE:     %c  ACS_RTEE:     %c", ACS_LTEE, ACS_RTEE);
    mvprintw(5, 2, "ACS_TTEE:     %c  ACS_BTEE:     %c", ACS_TTEE, ACS_BTEE);
    mvprintw(6, 2, "ACS_PLUS:     %c  ACS_DIAMOND:  %c", ACS_PLUS, ACS_DIAMOND);
    mvprintw(7, 2, "ACS_BULLET:   %c  ACS_BLOCK:    %c", ACS_BULLET, ACS_BLOCK);
    
    mvprintw(9, 2, "Press any key to exit");
    refresh();
    getch();
    endwin();
    return 0;
}
