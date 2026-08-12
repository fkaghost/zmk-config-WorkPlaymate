/*
 * WORK / WORK2 ghost
 *
 * EDITABLE PIXEL LEGEND:
 *   W = WHITE
 *   . = BLACK
 *   + = GRAY / DITHERED
 *
 * Keep every row exactly 24 characters wide.
 * Keep exactly 31 rows unless you also change the width/height defines.
 */

#ifndef WORKPLAY_WORK_GHOST_H
#define WORKPLAY_WORK_GHOST_H

#define WORK_GHOST_WIDTH 24
#define WORK_GHOST_HEIGHT 31

#define WORK_GHOST_X 5
#define WORK_GHOST_Y 0
#define WORK_GHOST_RENDER_WIDTH 58
#define WORK_GHOST_RENDER_HEIGHT 68

static const char *const work_ghost_rows[] = {
    ".......WWWWWWWWWW......W",
    "......WWWWWWWWWWWW.....W",
    "W.....WWWWWWWWWWWWWW...W",
    "....WWWWWWWWWWWWWWWW...W",
    "....WWWWWWWWWWWWWWWWW..W",
    "...W.......WW........W.W",
    "...W.WW..W....W..WW..W.W",
    "...W.WW..W.WW.W..WW..W.W",
    "...W.WW..W.WW.W..WW..W.W",
    "...W.WWWWW.WW.WWWWWW.W.W",
    "...W.......WW........W.W",
    "...W.WWWWWWWWWWWWWWWW..W",
    "..WWWWWWWWWWWWWWWWWWWW..",
    "..WWWWWWWWWWWWWWWWWWWW.W",
    "..WWWWWWWWWWWWWWWWWWWW.W",
    "..WWWWWWWWWWWWWWWWWWWW.W",
    "..WWWWWW.WWWWW.WWWWWWW.W",
    "..WWWWWW.WWWWW.WWWWWWW.W",
    "..WWWWWW.+WWWW.++WWWWW.W",
    "..WWWWWW.+WWWW.++WWWWW.W",
    ".W+WWWWW.+WWWW.++WWWWWWW",
    ".W+WWWWW.+WWWW.++WWWWWWW",
    ".W+WWWWW.+WWWW.++WWWWWWW",
    ".W+WWWWW.+WWWW.++WWWWWWW",
    ".W+WWWWW.+WWWW.++WWWWWWW",
    ".W++WWWW.+WWWW.++WWWWWWW",
    ".W+++WWW.+WWWW.++WWWWWWW",
    "........WWWWWWW........W",
    ".........+++++.........W",
    "..........WWW..........W",
    "..........WWW..........W"
};

#endif /* WORKPLAY_WORK_GHOST_H */
