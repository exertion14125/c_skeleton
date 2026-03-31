#include <stdio.h>

#include "ncurses.h"

int main(int argc, char *argv[])
{
        (void)argc;
        (void)argv;
        printf("\033[2J\033[H"); // Clear screen and move cursor to home position.
        printf("\n\r");
        printf("----------------------------\n\r");
        printf("  EXAMPLE) SKELETON NCURSESS UI  : %s\n\r", __DATE__);
        printf("----------------------------\n\r");
        printf("\n\r");

        return 0;
}