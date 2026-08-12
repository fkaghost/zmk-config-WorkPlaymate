/*
 * PLAY ghost
 *
 * EDITABLE PIXEL LEGEND:
 *   W = WHITE
 *   . = BLACK
 *   + = GRAY / DITHERED
 *
 * Keep every row exactly 23 characters wide.
 * Keep exactly 31 rows unless you also change the width/height defines.
 */

#ifndef WORKPLAY_PLAY_GHOST_H
#define WORKPLAY_PLAY_GHOST_H

#define PLAY_GHOST_WIDTH 23
#define PLAY_GHOST_HEIGHT 31

#define PLAY_GHOST_X 7
#define PLAY_GHOST_Y 0
#define PLAY_GHOST_RENDER_WIDTH 54
#define PLAY_GHOST_RENDER_HEIGHT 68

static const char *const play_ghost_rows[] = {
    "WWWWWWWWWWWWWWWWWWWWWWW",
    "WWWWWWWWW.....+WWWWWWWW",
    "WWWWWWW.+WWWWW+.WWWWWWW",
    "WWWWWW.+WWWWWWW+.WWWWWW",
    "WWWWWW.+WWWWWWW+.WWWWWW",
    "WWWWW.WW+++W+++WW.WWWWW",
    "WWWWW.WW+..W..+WW.WWWWW",
    "WWWWW.WW...W...WW.WWWWW",
    "WWWWW.WW...W...WW.WWWWW",
    "WWWWW.WW+..W..+WW.WWWWW",
    "WWWWW.WW+++W+++WW.WWWWW",
    "WWW.++WWWWWWWWWWW.WWWWW",
    "WWW.+WWWWWWWWWWWW.WWWWW",
    "WWW.+WWWWWWWWWWWW++.WWW",
    "WWW.+WWWWWWWWWWWWW+.WWW",
    "WWW.+W+.+WWWWW+.+W+.WWW",
    "WWW.+W+.+WWWWW+.+W+.WWW",
    "WWW.+W+.+WWWWW+.+W+.WWW",
    "WWW.+W+.+WWWWW+.++++.WW",
    "WW.++W+.+WWWWW+.++W+.WW",
    "WW.+WW+.+WWWWW+.++W+.WW",
    "WW.++W+.+WWWWW+.++W+.WW",
    "WW.++W+.+WWWWW+.++W+.WW",
    "WW.++W+.+WWWWW+.++W+.WW",
    "WW.++W+.+WWWWW+.++W+.WW",
    "WW.++++.+WWWWW+.++W+.WW",
    "WW.++++.+WWWWW+.++W+.WW",
    "WW.++++.++W++W+.++W+.WW",
    "WWW.....+W+.+W+.....WWW",
    "WWWWWWWW+..W..+WWWWWWWW",
    "WWWWWWWWWWWWWWWWWWWWWWW"
};

#endif /* WORKPLAY_PLAY_GHOST_H */
