/* SPDX-License-Identifier: MIT */

#define DT_DRV_COMPAT zmk_input_processor_trackpad_transition

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

#include <drivers/input_processor.h>
#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/input_listeners.h>
#include <zmk/keymap.h>
#include <zmk/virtual_key_position.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

enum trackpad_mode { TRACKPAD_BASE, TRACKPAD_ZOOM, TRACKPAD_VOLUME };

struct trackpad_transition_config {
    uint8_t index;
    uint8_t zoom_layer;
    uint8_t volume_layer;
    uint8_t zoom_key_position;
    uint8_t volume_key_position;
    uint32_t guard_ms;
    const struct zmk_behavior_binding *tap_bindings;
};

struct trackpad_transition_data {
    bool tap_pending;
    enum trackpad_mode tap_mode;
};

/* Uptime is 32-bit here so the read/write is atomic on the MCU. */
static atomic_t last_relevant_layer_change_ms;
static atomic_t zoom_key_down;
static atomic_t volume_key_down;

static int trackpad_layer_changed(const zmk_event_t *event) {
    const struct zmk_layer_state_changed *change = as_zmk_layer_state_changed(event);
    const struct zmk_position_state_changed *position = as_zmk_position_state_changed(event);

    if (change && (change->layer == DT_INST_PROP(0, zoom_layer) ||
                   change->layer == DT_INST_PROP(0, volume_layer))) {
        atomic_set(&last_relevant_layer_change_ms, (atomic_val_t)k_uptime_get_32());
    }
    if (position) {
        if (position->position == DT_INST_PROP(0, zoom_key_position)) {
            atomic_set(&zoom_key_down,
                       position->state && !zmk_keymap_layer_active(DT_INST_PROP(0, zoom_layer)) &&
                           !zmk_keymap_layer_active(DT_INST_PROP(0, volume_layer)));
        }
        if (position->position == DT_INST_PROP(0, volume_key_position)) {
            atomic_set(&volume_key_down,
                       position->state && !zmk_keymap_layer_active(DT_INST_PROP(0, zoom_layer)) &&
                           !zmk_keymap_layer_active(DT_INST_PROP(0, volume_layer)));
        }
    }
    return 0;
}

ZMK_LISTENER(trackpad_transition, trackpad_layer_changed);
ZMK_SUBSCRIPTION(trackpad_transition, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(trackpad_transition, zmk_position_state_changed);

static bool in_transition_guard(const struct trackpad_transition_config *cfg) {
    if ((atomic_get(&zoom_key_down) && !zmk_keymap_layer_active(cfg->zoom_layer)) ||
        (atomic_get(&volume_key_down) && !zmk_keymap_layer_active(cfg->volume_layer))) {
        return true;
    }
    uint32_t changed_at = (uint32_t)atomic_get(&last_relevant_layer_change_ms);
    return changed_at != 0 && k_uptime_get_32() - changed_at < cfg->guard_ms;
}

static enum trackpad_mode current_mode(const struct trackpad_transition_config *cfg) {
    if (zmk_keymap_layer_active(cfg->zoom_layer)) {
        return TRACKPAD_ZOOM;
    }
    if (zmk_keymap_layer_active(cfg->volume_layer)) {
        return TRACKPAD_VOLUME;
    }
    return TRACKPAD_BASE;
}

static int invoke_tap(const struct trackpad_transition_config *cfg,
                      struct zmk_input_processor_state *state, enum trackpad_mode mode,
                      bool pressed) {
    struct zmk_behavior_binding_event behavior_event = {
        .position = ZMK_VIRTUAL_KEY_POSITION_BEHAVIOR_INPUT_PROCESSOR(
            state->input_device_index, cfg->index),
        .timestamp = k_uptime_get(),
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
        .source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
#endif
    };
    return zmk_behavior_invoke_binding(&cfg->tap_bindings[mode], behavior_event, pressed);
}

static int trackpad_transition_handle_event(const struct device *dev, struct input_event *event,
                                            uint32_t param1, uint32_t param2,
                                            struct zmk_input_processor_state *state) {
    const struct trackpad_transition_config *cfg = dev->config;
    struct trackpad_transition_data *data = dev->data;
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);

    if (event->type == INPUT_EV_REL) {
        if (event->code == INPUT_REL_HWHEEL ||
            (event->code == INPUT_REL_WHEEL && in_transition_guard(cfg))) {
            return ZMK_INPUT_PROC_STOP;
        }
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (event->type != INPUT_EV_KEY || event->code != INPUT_BTN_3) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (event->value != 0) {
        data->tap_pending = !in_transition_guard(cfg);
        if (data->tap_pending) {
            data->tap_mode = current_mode(cfg);
        }
    } else {
        /* The marker releases after 50 ms. Commit only if its layer stayed stable. */
        if (data->tap_pending && !in_transition_guard(cfg) &&
            data->tap_mode == current_mode(cfg)) {
            int press_ret = invoke_tap(cfg, state, data->tap_mode, true);
            int release_ret = invoke_tap(cfg, state, data->tap_mode, false);
            if (press_ret < 0 || release_ret < 0) {
                data->tap_pending = false;
                return press_ret < 0 ? press_ret : release_ret;
            }
        }
        data->tap_pending = false;
    }

    return ZMK_INPUT_PROC_STOP;
}

static const struct zmk_input_processor_driver_api trackpad_transition_driver_api = {
    .handle_event = trackpad_transition_handle_event,
};

#define TRACKPAD_TRANSITION_INST(n)                                                                \
    BUILD_ASSERT(DT_INST_PROP_LEN(n, bindings) == 3, "trackpad needs three tap bindings");       \
    static const struct zmk_behavior_binding tap_bindings_##n[] = {                               \
        LISTIFY(3, ZMK_KEYMAP_EXTRACT_BINDING, (, ), DT_DRV_INST(n))};                             \
    static struct trackpad_transition_data trackpad_transition_data_##n;                          \
    static const struct trackpad_transition_config trackpad_transition_config_##n = {             \
        .index = n,                                                                                 \
        .zoom_layer = DT_INST_PROP(n, zoom_layer),                                                  \
        .volume_layer = DT_INST_PROP(n, volume_layer),                                              \
        .zoom_key_position = DT_INST_PROP(n, zoom_key_position),                                   \
        .volume_key_position = DT_INST_PROP(n, volume_key_position),                               \
        .guard_ms = DT_INST_PROP(n, transition_guard_ms),                                           \
        .tap_bindings = tap_bindings_##n,                                                            \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &trackpad_transition_data_##n,                            \
                          &trackpad_transition_config_##n, POST_KERNEL,                           \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &trackpad_transition_driver_api);

DT_INST_FOREACH_STATUS_OKAY(TRACKPAD_TRANSITION_INST)

#endif
