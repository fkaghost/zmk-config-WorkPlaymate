/*
 * WorkPlayMate custom nice!view screen
 * Target: ZMK v0.3 + nice!nano v2 + nice!view
 *
 * Physical display layout (68 x 160 after the stock nice!view rotation):
 *
 *   [battery] 87%                         1
 *
 *                 PLAY / WORK
 *
 *                 pixel ghost
 *
 *             Connected / Pairing
 *
 * Notes:
 * - Battery percentage is rendered with a hand-drawn 3x5 pixel font.
 * - Work2 intentionally displays as WORK.
 * - Only the active Bluetooth profile number (1 or 2) is shown.
 * - Any profile that is not currently connected is displayed as Pairing,
 *   per the requested two-state status display.
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

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct output_status_state {
    int active_profile_index;
    bool active_profile_connected;
};

struct layer_status_state {
    zmk_keymap_layer_index_t index;
    const char *label;
};

/* -------------------------------------------------------------------------- */
/* General drawing helpers                                                     */
/* -------------------------------------------------------------------------- */

static void clear_canvas(lv_obj_t *canvas) {
    lv_draw_rect_dsc_t background;
    init_rect_dsc(&background, LVGL_BACKGROUND);
    lv_canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &background);
}

/*
 * Layer 2 (Work2) deliberately reports WORK.
 */
static const char *mode_name(const struct status_state *state) {
    switch (state->layer_index) {
    case 0:
        return "PLAY";
    case 1:
    case 2:
        return "WORK";
    default:
        return "WORK";
    }
}

/* -------------------------------------------------------------------------- */
/* Tiny 3x5 battery percentage font                                            */
/* -------------------------------------------------------------------------- */

/*
 * Each glyph is 3 pixels wide by 5 pixels tall.
 * Bits 2..0 correspond to the left, middle, and right pixel.
 */
static const uint8_t tiny_0[5] = {0x7, 0x5, 0x5, 0x5, 0x7};
static const uint8_t tiny_1[5] = {0x2, 0x6, 0x2, 0x2, 0x7};
static const uint8_t tiny_2[5] = {0x7, 0x1, 0x7, 0x4, 0x7};
static const uint8_t tiny_3[5] = {0x7, 0x1, 0x7, 0x1, 0x7};
static const uint8_t tiny_4[5] = {0x5, 0x5, 0x7, 0x1, 0x1};
static const uint8_t tiny_5[5] = {0x7, 0x4, 0x7, 0x1, 0x7};
static const uint8_t tiny_6[5] = {0x7, 0x4, 0x7, 0x5, 0x7};
static const uint8_t tiny_7[5] = {0x7, 0x1, 0x1, 0x1, 0x1};
static const uint8_t tiny_8[5] = {0x7, 0x5, 0x7, 0x5, 0x7};
static const uint8_t tiny_9[5] = {0x7, 0x5, 0x7, 0x1, 0x7};
static const uint8_t tiny_pct[5] = {0x5, 0x1, 0x2, 0x4, 0x5};

static const uint8_t *tiny_glyph(char ch) {
    switch (ch) {
    case '0': return tiny_0;
    case '1': return tiny_1;
    case '2': return tiny_2;
    case '3': return tiny_3;
    case '4': return tiny_4;
    case '5': return tiny_5;
    case '6': return tiny_6;
    case '7': return tiny_7;
    case '8': return tiny_8;
    case '9': return tiny_9;
    case '%': return tiny_pct;
    default:  return NULL;
    }
}

static void draw_tiny_text(lv_obj_t *canvas, int x, int y, const char *text) {
    lv_draw_rect_dsc_t pixel;
    init_rect_dsc(&pixel, LVGL_FOREGROUND);

    int cursor_x = x;

    for (size_t i = 0; text[i] != '\0'; i++) {
        const uint8_t *glyph = tiny_glyph(text[i]);

        if (glyph != NULL) {
            for (int row = 0; row < 5; row++) {
                for (int col = 0; col < 3; col++) {
                    if (glyph[row] & (1 << (2 - col))) {
                        lv_canvas_draw_rect(canvas, cursor_x + col, y + row, 1, 1, &pixel);
                    }
                }
            }
        }

        cursor_x += 4;
    }
}

/* -------------------------------------------------------------------------- */
/* Battery                                                                     */
/* -------------------------------------------------------------------------- */

static void draw_small_battery(lv_obj_t *canvas, uint8_t battery) {
    lv_draw_rect_dsc_t fg;
    init_rect_dsc(&fg, LVGL_FOREGROUND);

    lv_draw_rect_dsc_t bg;
    init_rect_dsc(&bg, LVGL_BACKGROUND);

    /*
     * 12x8 battery body in the upper-left corner.
     */
    lv_canvas_draw_rect(canvas, 2, 5, 12, 8, &fg);
    lv_canvas_draw_rect(canvas, 3, 6, 10, 6, &bg);
    lv_canvas_draw_rect(canvas, 14, 7, 2, 4, &fg);

    /*
     * Seven-pixel fill bar.
     */
    uint8_t fill = (battery * 7 + 99) / 100;
    if (fill > 7) {
        fill = 7;
    }

    if (fill > 0) {
        lv_canvas_draw_rect(canvas, 4, 8, fill, 2, &fg);
    }
}

/* -------------------------------------------------------------------------- */
/* Ghost from the supplied pixel-art reference                                 */
/* -------------------------------------------------------------------------- */

/*
 * 15 x 29 logical pixels derived from the supplied ghost.
 *
 * '#' = solid black pixel block
 * 'g' = light-gray area represented with a 25% monochrome dither
 * space = white / transparent
 *
 * Rendered at 2x scale => 30 x 58 pixels.
 */
static const char ghost_rows[][16] = {
    "     #####     ",
    "    #     #    ",
    "   #       #   ",
    "   #       #   ",
    "  #         #  ",
    "  #  ## ##  #  ",
    "  #  ## ##  #  ",
    "  #  ## ##  #  ",
    "  #  ## ##  #  ",
    "  #         #   ",
    " #          #   ",
    " #          #   ",
    " #           #  ",
    " #           #  ",
    " #  #     #  #  ",
    " #  #     #  #  ",
    " #  #     #  #  ",
    " #  #g    #g  # ",
    "#g  #g    #g  # ",
    "#g  #g    #g  # ",
    "#g  #g    #g  # ",
    "#g  #g    #g  # ",
    "#g  #g    #g  # ",
    "#g  #g    #g  # ",
    "#gg #g    #g  # ",
    "#gg #g    #g  # ",
    "#ggg#g g  #ggg# ",
    " ####gg#gg####  ",
    "     ## ##      ",
};

static void draw_ghost(lv_obj_t *canvas) {
    const int scale = 2;
    const int ghost_width = 15 * scale;
    const int ghost_height = 29 * scale;
    const int origin_x = (CANVAS_SIZE - ghost_width) / 2;
    const int origin_y = (CANVAS_SIZE - ghost_height) / 2;

    lv_draw_rect_dsc_t fg;
    init_rect_dsc(&fg, LVGL_FOREGROUND);

    for (int row = 0; row < 29; row++) {
        for (int col = 0; col < 15; col++) {
            char px = ghost_rows[row][col];

            if (px == '#') {
                lv_canvas_draw_rect(canvas,
                                    origin_x + col * scale,
                                    origin_y + row * scale,
                                    scale,
                                    scale,
                                    &fg);
            } else if (px == 'g') {
                /*
                 * One black pixel out of each 2x2 logical cell produces
                 * a subtle monochrome approximation of the gray shading.
                 */
                int dx = (row + col) & 1;
                int dy = (row + col + 1) & 1;

                lv_canvas_draw_rect(canvas,
                                    origin_x + col * scale + dx,
                                    origin_y + row * scale + dy,
                                    1,
                                    1,
                                    &fg);
            }
        }
    }
}

/* -------------------------------------------------------------------------- */
/* TOP CANVAS                                                                  */
/* -------------------------------------------------------------------------- */

/*
 * Header:
 *
 * [battery] 87%                           1
 *
 *                 PLAY
 *
 * The standalone 1/2 is the currently selected Bluetooth profile.
 */
static void draw_top(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 0);
    clear_canvas(canvas);

    draw_small_battery(canvas, state->battery);

    char battery_text[8] = {};
    snprintf(battery_text, sizeof(battery_text), "%u%%", state->battery);
    draw_tiny_text(canvas, 18, 7, battery_text);

    lv_draw_label_dsc_t profile_dsc;
    init_label_dsc(&profile_dsc, LVGL_FOREGROUND, &lv_font_montserrat_18,
                   LV_TEXT_ALIGN_CENTER);

    char profile_text[2] = {};
    snprintf(profile_text, sizeof(profile_text), "%d", state->active_profile_index + 1);
    lv_canvas_draw_text(canvas, 49, 0, 18, &profile_dsc, profile_text);

    lv_draw_label_dsc_t mode_dsc;
    init_label_dsc(&mode_dsc, LVGL_FOREGROUND, &lv_font_montserrat_18,
                   LV_TEXT_ALIGN_CENTER);

    lv_canvas_draw_text(canvas, 0, 35, CANVAS_SIZE, &mode_dsc, mode_name(state));

    rotate_canvas(canvas, cbuf);
}

/* -------------------------------------------------------------------------- */
/* MIDDLE CANVAS — static ghost                                                */
/* -------------------------------------------------------------------------- */

static void draw_middle(lv_obj_t *widget, lv_color_t cbuf[]) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 1);
    clear_canvas(canvas);

    draw_ghost(canvas);

    rotate_canvas(canvas, cbuf);
}

/* -------------------------------------------------------------------------- */
/* BOTTOM CANVAS — Connected / Pairing                                         */
/* -------------------------------------------------------------------------- */

static void draw_bottom(lv_obj_t *widget, lv_color_t cbuf[],
                        const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 2);
    clear_canvas(canvas);

    lv_draw_label_dsc_t status_dsc;
    init_label_dsc(&status_dsc, LVGL_FOREGROUND, &lv_font_montserrat_10,
                   LV_TEXT_ALIGN_CENTER);

    const char *status_text =
        state->active_profile_connected ? "Connected" : "Pairing";

    /*
     * Keep this near the visible edge because only ~24 pixels of the
     * third stock nice!view canvas appear on the physical screen.
     */
    lv_canvas_draw_text(canvas, 0, 5, CANVAS_SIZE, &status_dsc, status_text);

    rotate_canvas(canvas, cbuf);
}

/* -------------------------------------------------------------------------- */
/* Battery listener                                                            */
/* -------------------------------------------------------------------------- */

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

static struct battery_status_state battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);

    return (struct battery_status_state){
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct battery_status_state,
                            battery_status_update_cb, battery_status_get_state)

ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);

#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif

/* -------------------------------------------------------------------------- */
/* Bluetooth listener                                                          */
/* -------------------------------------------------------------------------- */

static void set_output_status(struct zmk_widget_status *widget,
                              const struct output_status_state *state) {
    widget->state.active_profile_index = state->active_profile_index;
    widget->state.active_profile_connected = state->active_profile_connected;

    /*
     * Bluetooth changes affect the profile number at the top and the
     * Connected/Pairing label at the bottom.
     */
    draw_top(widget->obj, widget->cbuf, &widget->state);
    draw_bottom(widget->obj, widget->cbuf3, &widget->state);
}

static void output_status_update_cb(struct output_status_state state) {
    struct zmk_widget_status *widget;

    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_output_status(widget, &state);
    }
}

static struct output_status_state output_status_get_state(const zmk_event_t *_eh) {
    return (struct output_status_state){
        .active_profile_index = zmk_ble_active_profile_index(),
        .active_profile_connected = zmk_ble_active_profile_is_connected(),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct output_status_state,
                            output_status_update_cb, output_status_get_state)

ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);

#if defined(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
#endif

/* -------------------------------------------------------------------------- */
/* Layer listener                                                              */
/* -------------------------------------------------------------------------- */

static void set_layer_status(struct zmk_widget_status *widget,
                             struct layer_status_state state) {
    widget->state.layer_index = state.index;
    widget->state.layer_label = state.label;

    draw_top(widget->obj, widget->cbuf, &widget->state);
}

static void layer_status_update_cb(struct layer_status_state state) {
    struct zmk_widget_status *widget;

    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_layer_status(widget, state);
    }
}

static struct layer_status_state layer_status_get_state(const zmk_event_t *eh) {
    zmk_keymap_layer_index_t index = zmk_keymap_highest_layer_active();

    return (struct layer_status_state){
        .index = index,
        .label = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(index)),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state,
                            layer_status_update_cb, layer_status_get_state)

ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

/* -------------------------------------------------------------------------- */
/* Widget initialization                                                       */
/* -------------------------------------------------------------------------- */

int zmk_widget_status_init(struct zmk_widget_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 160, 68);

    /*
     * These three 68x68 canvases are the same placement strategy used by
     * the working custom nice!view widget. rotate_canvas() handles the
     * display's physical 90-degree orientation.
     */
    lv_obj_t *top = lv_canvas_create(widget->obj);
    lv_obj_align(top, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_canvas_set_buffer(top, widget->cbuf, CANVAS_SIZE, CANVAS_SIZE,
                         LV_IMG_CF_TRUE_COLOR);

    lv_obj_t *middle = lv_canvas_create(widget->obj);
    lv_obj_align(middle, LV_ALIGN_TOP_LEFT, 24, 0);
    lv_canvas_set_buffer(middle, widget->cbuf2, CANVAS_SIZE, CANVAS_SIZE,
                         LV_IMG_CF_TRUE_COLOR);

    lv_obj_t *bottom = lv_canvas_create(widget->obj);
    lv_obj_align(bottom, LV_ALIGN_TOP_LEFT, -44, 0);
    lv_canvas_set_buffer(bottom, widget->cbuf3, CANVAS_SIZE, CANVAS_SIZE,
                         LV_IMG_CF_TRUE_COLOR);

    sys_slist_append(&widgets, &widget->node);

    /*
     * The ghost never changes, so draw it once.
     */
    draw_middle(widget->obj, widget->cbuf2);

    widget_battery_status_init();
    widget_output_status_init();
    widget_layer_status_init();

    return 0;
}

lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget) {
    return widget->obj;
}
