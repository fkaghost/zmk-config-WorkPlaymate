/*
 * Simplified nice!view status screen for a single left-hand Corne.
 *
 * Screen blocks:
 *   TOP:    battery percentage only
 *   MIDDLE: PLAY / WORK / WORK 2
 *   BOTTOM: Bluetooth profiles 1 and 2
 *
 * This file intentionally follows the ZMK v0.3 nice_view widget API.
 */

#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/keymap.h>
#include <zmk/usb.h>

#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>

#include "status.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct output_status_state {
    struct zmk_endpoint_instance selected_endpoint;
    int active_profile_index;
    bool active_profile_connected;
    bool active_profile_bonded;
    bool profiles_connected[NICEVIEW_PROFILE_COUNT];
    bool profiles_bonded[NICEVIEW_PROFILE_COUNT];
};

struct layer_status_state {
    zmk_keymap_layer_index_t index;
    const char *label;
};

/* -------------------------------------------------------------------------- */
/* Drawing helpers                                                             */
/* -------------------------------------------------------------------------- */

static void clear_canvas(lv_obj_t *canvas) {
    lv_draw_rect_dsc_t background;
    init_rect_dsc(&background, LVGL_BACKGROUND);
    lv_canvas_draw_rect(canvas, 0, 0, CANVAS_SIZE, CANVAS_SIZE, &background);
}

static const char *mode_name(const struct status_state *state) {
    /*
     * These three names deliberately match this keyboard's three layers.
     * For any future extra layer, fall back to its ZMK display-name.
     */
    switch (state->layer_index) {
    case 0:
        return "PLAY";
    case 1:
        return "WORK";
    case 2:
        return "WORK 2";
    default:
        if (state->layer_label != NULL && strlen(state->layer_label) > 0) {
            return state->layer_label;
        }
        return "LAYER";
    }
}

static void draw_small_battery(lv_obj_t *canvas, uint8_t battery) {
    lv_draw_rect_dsc_t fg;
    init_rect_dsc(&fg, LVGL_FOREGROUND);

    lv_draw_rect_dsc_t bg;
    init_rect_dsc(&bg, LVGL_BACKGROUND);

    lv_canvas_draw_rect(canvas, 1, 7, 11, 7, &fg);
    lv_canvas_draw_rect(canvas, 2, 8, 9, 5, &bg);
    lv_canvas_draw_rect(canvas, 12, 9, 2, 3, &fg);

    uint8_t fill = (battery * 7 + 99) / 100;
    if (fill > 7) {
        fill = 7;
    }
    if (fill > 0) {
        lv_canvas_draw_rect(canvas, 3, 9, fill, 3, &fg);
    }
}

static void draw_profile_circle(lv_obj_t *canvas, const struct status_state *state,
                                int profile, int x, int y) {
    bool selected = profile == state->active_profile_index;
    bool connected = state->profiles_connected[profile];
    bool bonded = state->profiles_bonded[profile];

    lv_draw_arc_dsc_t outline;
    init_arc_dsc(&outline, LVGL_FOREGROUND, 2);

    lv_draw_arc_dsc_t selected_fill;
    init_arc_dsc(&selected_fill, LVGL_FOREGROUND, 8);

    lv_draw_label_dsc_t normal_text;
    init_label_dsc(&normal_text, LVGL_FOREGROUND, &lv_font_montserrat_14,
                   LV_TEXT_ALIGN_CENTER);

    lv_draw_label_dsc_t selected_text;
    init_label_dsc(&selected_text, LVGL_BACKGROUND, &lv_font_montserrat_14,
                   LV_TEXT_ALIGN_CENTER);

    if (connected) {
        lv_canvas_draw_arc(canvas, x, y, 11, 0, 360, &outline);
    } else if (bonded) {
        const int segments = 8;
        const int gap = 20;

        for (int i = 0; i < segments; i++) {
            lv_canvas_draw_arc(canvas, x, y, 11,
                               (360 / segments) * i + gap / 2,
                               (360 / segments) * (i + 1) - gap / 2,
                               &outline);
        }
    }

    if (selected) {
        lv_canvas_draw_arc(canvas, x, y, 7, 0, 359, &selected_fill);
    }

    char label[2] = {};
    snprintf(label, sizeof(label), "%d", profile + 1);

    lv_canvas_draw_text(canvas, x - 7, y - 9, 14,
                        selected ? &selected_text : &normal_text, label);
}

/*
 * TOP:
 * [battery] 87%
 *
 * Intentionally battery-only. Bluetooth profile/USB state is not shown here.
 */
static void draw_top(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 0);
    clear_canvas(canvas);

    lv_draw_label_dsc_t battery_label;
    init_label_dsc(&battery_label, LVGL_FOREGROUND, &lv_font_montserrat_14,
                   LV_TEXT_ALIGN_LEFT);

    draw_small_battery(canvas, state->battery);

    char battery_text[8] = {};
    snprintf(battery_text, sizeof(battery_text), "%u%%", state->battery);
    lv_canvas_draw_text(canvas, 16, 3, 32, &battery_label, battery_text);

    rotate_canvas(canvas, cbuf);
}

/*
 * MIDDLE:
 *                 PLAY
 *
 *            (1)          (2)
 */
static void draw_middle(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 1);
    clear_canvas(canvas);

    lv_draw_label_dsc_t mode;
    init_label_dsc(&mode, LVGL_FOREGROUND, &lv_font_montserrat_18, LV_TEXT_ALIGN_CENTER);

    lv_canvas_draw_text(canvas, 0, 3, CANVAS_SIZE, &mode, mode_name(state));

    draw_profile_circle(canvas, state, 0, 19, 44);
    draw_profile_circle(canvas, state, 1, 49, 44);

    rotate_canvas(canvas, cbuf);
}

/*
 * BOTTOM:
 *              Connected
 *
 * The bottom canvas is partially clipped by the stock nice!view layout,
 * so only a short status label is placed here.
 */
static void draw_bottom(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 2);
    clear_canvas(canvas);

    lv_draw_label_dsc_t status;
    init_label_dsc(&status, LVGL_FOREGROUND, &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);

    const char *status_text = "Ready";

    if (state->selected_endpoint.transport == ZMK_TRANSPORT_USB) {
        status_text = "USB";
    } else if (state->selected_endpoint.transport == ZMK_TRANSPORT_BLE) {
        if (!state->active_profile_bonded) {
            status_text = "Pairing";
        } else if (state->active_profile_connected) {
            status_text = "Connected";
        } else {
            status_text = "Offline";
        }
    }

    lv_canvas_draw_text(canvas, 0, 4, CANVAS_SIZE, &status, status_text);

    rotate_canvas(canvas, cbuf);
}

/* -------------------------------------------------------------------------- */
/* Battery                                                                     */
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
/* Bluetooth / output                                                          */
/* -------------------------------------------------------------------------- */

static void set_output_status(struct zmk_widget_status *widget,
                              const struct output_status_state *state) {
    widget->state.selected_endpoint = state->selected_endpoint;
    widget->state.active_profile_index = state->active_profile_index;
    widget->state.active_profile_connected = state->active_profile_connected;
    widget->state.active_profile_bonded = state->active_profile_bonded;

    for (int i = 0; i < NICEVIEW_PROFILE_COUNT; i++) {
        widget->state.profiles_connected[i] = state->profiles_connected[i];
        widget->state.profiles_bonded[i] = state->profiles_bonded[i];
    }

    draw_middle(widget->obj, widget->cbuf2, &widget->state);
    draw_bottom(widget->obj, widget->cbuf3, &widget->state);
}

static void output_status_update_cb(struct output_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_output_status(widget, &state);
    }
}

static struct output_status_state output_status_get_state(const zmk_event_t *_eh) {
    struct output_status_state state = {
        .selected_endpoint = zmk_endpoints_selected(),
        .active_profile_index = zmk_ble_active_profile_index(),
        .active_profile_connected = zmk_ble_active_profile_is_connected(),
        .active_profile_bonded = !zmk_ble_active_profile_is_open(),
    };

    for (int i = 0; i < MIN(NICEVIEW_PROFILE_COUNT, ZMK_BLE_PROFILE_COUNT); i++) {
        state.profiles_connected[i] = zmk_ble_profile_is_connected(i);
        state.profiles_bonded[i] = !zmk_ble_profile_is_open(i);
    }

    return state;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct output_status_state,
                            output_status_update_cb, output_status_get_state)

ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);

#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);
#endif

#if defined(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
#endif

/* -------------------------------------------------------------------------- */
/* Layer / mode                                                                */
/* -------------------------------------------------------------------------- */

static void set_layer_status(struct zmk_widget_status *widget,
                             struct layer_status_state state) {
    widget->state.layer_index = state.index;
    widget->state.layer_label = state.label;

    draw_middle(widget->obj, widget->cbuf2, &widget->state);
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

    widget_battery_status_init();
    widget_output_status_init();
    widget_layer_status_init();

    return 0;
}

lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget) {
    return widget->obj;
}
