/*
 * WorkPlayMate custom nice!view screen
 *
 * Ghost artwork is stored separately in:
 *   ghost_play.h
 *   ghost_work.h
 *
 * Sprite legend:
 *   W = white
 *   . = black
 *   + = gray/dithered
 */

#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/keymap.h>

#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>

#include "status.h"
#include "ghost_play.h"
#include "ghost_work.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct output_status_state {
    int active_profile_index;
    bool active_profile_connected;
};

struct layer_status_state {
    zmk_keymap_layer_index_t index;
    const char *label;
};

static void clear_canvas(lv_obj_t *canvas) {
    lv_draw_rect_dsc_t background;
    init_rect_dsc(&background, LVGL_BACKGROUND);
    lv_canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &background);
}

static const char *mode_name(const struct status_state *state) {
    return state->layer_index == 0 ? "PLAY" : "WORK";
}

/* 4x7 battery percentage font */
static const uint8_t px7_0[7]   = {0x6,0x9,0x9,0x9,0x9,0x9,0x6};
static const uint8_t px7_1[7]   = {0x2,0x6,0x2,0x2,0x2,0x2,0x7};
static const uint8_t px7_2[7]   = {0xE,0x1,0x1,0x6,0x8,0x8,0xF};
static const uint8_t px7_3[7]   = {0xE,0x1,0x1,0x6,0x1,0x1,0xE};
static const uint8_t px7_4[7]   = {0x9,0x9,0x9,0xF,0x1,0x1,0x1};
static const uint8_t px7_5[7]   = {0xF,0x8,0x8,0xE,0x1,0x1,0xE};
static const uint8_t px7_6[7]   = {0x6,0x8,0x8,0xE,0x9,0x9,0x6};
static const uint8_t px7_7[7]   = {0xF,0x1,0x1,0x2,0x2,0x2,0x2};
static const uint8_t px7_8[7]   = {0x6,0x9,0x9,0x6,0x9,0x9,0x6};
static const uint8_t px7_9[7]   = {0x6,0x9,0x9,0x7,0x1,0x1,0x6};
static const uint8_t px7_pct[7] = {0x9,0x1,0x2,0x2,0x4,0x8,0x9};

static const uint8_t *px7_glyph(char ch) {
    switch (ch) {
    case '0': return px7_0;
    case '1': return px7_1;
    case '2': return px7_2;
    case '3': return px7_3;
    case '4': return px7_4;
    case '5': return px7_5;
    case '6': return px7_6;
    case '7': return px7_7;
    case '8': return px7_8;
    case '9': return px7_9;
    case '%': return px7_pct;
    default: return NULL;
    }
}

static void draw_px7_text(lv_obj_t *canvas, int x, int y, const char *text) {
    lv_draw_rect_dsc_t pixel;
    init_rect_dsc(&pixel, LVGL_FOREGROUND);

    int cursor_x = x;
    for (size_t i = 0; text[i] != '\0'; i++) {
        const uint8_t *glyph = px7_glyph(text[i]);
        if (glyph != NULL) {
            for (int row = 0; row < 7; row++) {
                for (int col = 0; col < 4; col++) {
                    if (glyph[row] & (1 << (3 - col))) {
                        lv_canvas_draw_rect(canvas, cursor_x + col, y + row, 1, 1, &pixel);
                    }
                }
            }
        }
        cursor_x += 5;
    }
}

static void draw_large_battery(lv_obj_t *canvas, uint8_t battery) {
    lv_draw_rect_dsc_t fg;
    init_rect_dsc(&fg, LVGL_FOREGROUND);

    lv_draw_rect_dsc_t bg;
    init_rect_dsc(&bg, LVGL_BACKGROUND);

    lv_canvas_draw_rect(canvas, 1, 3, 15, 10, &fg);
    lv_canvas_draw_rect(canvas, 2, 4, 13, 8, &bg);
    lv_canvas_draw_rect(canvas, 16, 6, 2, 4, &fg);

    uint8_t fill = (battery * 9 + 99) / 100;
    if (fill > 9) {
        fill = 9;
    }

    if (fill > 0) {
        lv_canvas_draw_rect(canvas, 4, 7, fill, 2, &fg);
    }
}

/*
 * Generic renderer for editable ghost headers.
 *
 * W = explicitly white. Because clear_canvas() already made the
 * background white, W normally requires no drawing.
 * . = black
 * + = checkerboard dither to simulate gray
 */
static void draw_ghost(lv_obj_t *canvas,
                       const char *const rows[],
                       int logical_width,
                       int logical_height,
                       int origin_x,
                       int origin_y,
                       int render_width,
                       int render_height) {
    lv_draw_rect_dsc_t fg;
    init_rect_dsc(&fg, LVGL_FOREGROUND);

    for (int row = 0; row < logical_height; row++) {
        int y0 = origin_y + (row * render_height) / logical_height;
        int y1 = origin_y + ((row + 1) * render_height) / logical_height - 1;

        for (int col = 0; col < logical_width; col++) {
            int x0 = origin_x + (col * render_width) / logical_width;
            int x1 = origin_x + ((col + 1) * render_width) / logical_width - 1;

            char px = rows[row][col];

            if (px == '.') {
                lv_canvas_draw_rect(canvas, x0, y0,
                                    x1 - x0 + 1,
                                    y1 - y0 + 1,
                                    &fg);
            } else if (px == '+') {
                for (int y = y0; y <= y1; y++) {
                    for (int x = x0; x <= x1; x++) {
                        if (((x + y) & 1) == 0) {
                            lv_canvas_draw_rect(canvas, x, y, 1, 1, &fg);
                        }
                    }
                }
            }
        }
    }
}

static void draw_top(lv_obj_t *widget, lv_color_t cbuf[],
                     const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 0);
    clear_canvas(canvas);

    draw_large_battery(canvas, state->battery);

    char battery_text[8] = {};
    snprintf(battery_text, sizeof(battery_text), "%u%%", state->battery);
    draw_px7_text(canvas, 21, 4, battery_text);

    lv_draw_label_dsc_t profile_dsc;
    init_label_dsc(&profile_dsc, LVGL_FOREGROUND,
                   &lv_font_montserrat_18, LV_TEXT_ALIGN_CENTER);

    char profile_text[2] = {};
    snprintf(profile_text, sizeof(profile_text),
             "%d", state->active_profile_index + 1);

    lv_canvas_draw_text(canvas, 50, -1, 17,
                        &profile_dsc, profile_text);

    lv_draw_label_dsc_t mode_dsc;
    init_label_dsc(&mode_dsc, LVGL_FOREGROUND,
                   &lv_font_montserrat_18, LV_TEXT_ALIGN_CENTER);

    lv_canvas_draw_text(canvas, 0, 35,
                        CANVAS_SIZE, &mode_dsc, mode_name(state));

    rotate_canvas(canvas, cbuf);
}

static void draw_middle(lv_obj_t *widget, lv_color_t cbuf[],
                        const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 1);
    clear_canvas(canvas);

    if (state->layer_index == 0) {
        draw_ghost(canvas,
                   play_ghost_rows,
                   PLAY_GHOST_WIDTH,
                   PLAY_GHOST_HEIGHT,
                   PLAY_GHOST_X,
                   PLAY_GHOST_Y,
                   PLAY_GHOST_RENDER_WIDTH,
                   PLAY_GHOST_RENDER_HEIGHT);
    } else {
        draw_ghost(canvas,
                   work_ghost_rows,
                   WORK_GHOST_WIDTH,
                   WORK_GHOST_HEIGHT,
                   WORK_GHOST_X,
                   WORK_GHOST_Y,
                   WORK_GHOST_RENDER_WIDTH,
                   WORK_GHOST_RENDER_HEIGHT);
    }

    rotate_canvas(canvas, cbuf);
}

static void draw_bottom(lv_obj_t *widget, lv_color_t cbuf[],
                        const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 2);
    clear_canvas(canvas);

    lv_draw_label_dsc_t status_dsc;
    init_label_dsc(&status_dsc, LVGL_FOREGROUND,
                   &lv_font_montserrat_10, LV_TEXT_ALIGN_CENTER);

    const char *status_text =
        state->active_profile_connected ? "Connected" : "Pairing";

    lv_canvas_draw_text(canvas, 0, 5,
                        CANVAS_SIZE, &status_dsc, status_text);

    rotate_canvas(canvas, cbuf);
}

/* Battery listener */
static void set_battery_status(struct zmk_widget_status *widget,
                               struct battery_status_state state) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    widget->state.charging = state.usb_present;
#endif
    widget->state.battery = state.level;
    draw_top(widget->obj, widget->cbuf, &widget->state);
}

static void battery_status_update_cb(struct battery_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_battery_status(widget, state);
    }
}

static struct battery_status_state
battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev =
        as_zmk_battery_state_changed(eh);

    return (struct battery_status_state) {
        .level = (ev != NULL)
                     ? ev->state_of_charge
                     : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status,
                            struct battery_status_state,
                            battery_status_update_cb,
                            battery_status_get_state)

ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);

#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif

/* Bluetooth listener */
static void set_output_status(struct zmk_widget_status *widget,
                              const struct output_status_state *state) {
    widget->state.active_profile_index = state->active_profile_index;
    widget->state.active_profile_connected =
        state->active_profile_connected;

    draw_top(widget->obj, widget->cbuf, &widget->state);
    draw_bottom(widget->obj, widget->cbuf3, &widget->state);
}

static void output_status_update_cb(struct output_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_output_status(widget, &state);
    }
}

static struct output_status_state
output_status_get_state(const zmk_event_t *_eh) {
    return (struct output_status_state) {
        .active_profile_index = zmk_ble_active_profile_index(),
        .active_profile_connected =
            zmk_ble_active_profile_is_connected(),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status,
                            struct output_status_state,
                            output_status_update_cb,
                            output_status_get_state)

ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);

#if defined(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(widget_output_status,
                 zmk_ble_active_profile_changed);
#endif

/* Layer listener */
static void set_layer_status(struct zmk_widget_status *widget,
                             struct layer_status_state state) {
    widget->state.layer_index = state.index;
    widget->state.layer_label = state.label;

    draw_top(widget->obj, widget->cbuf, &widget->state);
    draw_middle(widget->obj, widget->cbuf2, &widget->state);
}

static void layer_status_update_cb(struct layer_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_layer_status(widget, state);
    }
}

static struct layer_status_state
layer_status_get_state(const zmk_event_t *eh) {
    zmk_keymap_layer_index_t index =
        zmk_keymap_highest_layer_active();

    return (struct layer_status_state) {
        .index = index,
        .label =
            zmk_keymap_layer_name(
                zmk_keymap_layer_index_to_id(index)),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status,
                            struct layer_status_state,
                            layer_status_update_cb,
                            layer_status_get_state)

ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

/* Widget init */
int zmk_widget_status_init(struct zmk_widget_status *widget,
                           lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 160, 68);

    lv_obj_t *top = lv_canvas_create(widget->obj);
    lv_obj_align(top, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_canvas_set_buffer(top, widget->cbuf,
                         CANVAS_SIZE, CANVAS_SIZE,
                         LV_IMG_CF_TRUE_COLOR);

    lv_obj_t *middle = lv_canvas_create(widget->obj);
    lv_obj_align(middle, LV_ALIGN_TOP_LEFT, 24, 0);
    lv_canvas_set_buffer(middle, widget->cbuf2,
                         CANVAS_SIZE, CANVAS_SIZE,
                         LV_IMG_CF_TRUE_COLOR);

    lv_obj_t *bottom = lv_canvas_create(widget->obj);
    lv_obj_align(bottom, LV_ALIGN_TOP_LEFT, -44, 0);
    lv_canvas_set_buffer(bottom, widget->cbuf3,
                         CANVAS_SIZE, CANVAS_SIZE,
                         LV_IMG_CF_TRUE_COLOR);

    sys_slist_append(&widgets, &widget->node);

    widget_battery_status_init();
    widget_output_status_init();
    widget_layer_status_init();

    return 0;
}

lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget) {
    return widget->obj;
}
