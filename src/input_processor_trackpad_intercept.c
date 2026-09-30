/* SPDX-License-Identifier: MIT */

#define DT_DRV_COMPAT zmk_input_processor_trackpad_intercept

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/kernel.h>

#include <drivers/input_processor.h>
#include <zmk/keymap.h>

struct trackpad_intercept_config {
    uint8_t zoom_layer;
    uint8_t volume_layer;
    uint32_t idle_ms;
};

struct trackpad_intercept_data {
    bool has_wheel;
    uint8_t wheel_mode;
    uint32_t last_wheel_ms;
};

static uint8_t current_mode(const struct trackpad_intercept_config *cfg) {
    if (zmk_keymap_layer_active(cfg->zoom_layer)) {
        return 1;
    }
    if (zmk_keymap_layer_active(cfg->volume_layer)) {
        return 2;
    }
    return 0;
}

static int trackpad_intercept_handle_event(const struct device *dev, struct input_event *event,
                                           uint32_t param1, uint32_t param2,
                                           struct zmk_input_processor_state *state) {
    const struct trackpad_intercept_config *cfg = dev->config;
    struct trackpad_intercept_data *data = dev->data;
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);

    /* The original driver uses BTN_4 for double taps, which are disabled here. */
    if (event->type == INPUT_EV_KEY && event->code == INPUT_BTN_4) {
        return ZMK_INPUT_PROC_STOP;
    }
    if (event->type != INPUT_EV_REL) {
        return ZMK_INPUT_PROC_CONTINUE;
    }
    if (event->code == INPUT_REL_HWHEEL) {
        return ZMK_INPUT_PROC_STOP;
    }
    if (event->code != INPUT_REL_WHEEL || event->value == 0) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    uint32_t now = k_uptime_get_32();
    uint8_t mode = current_mode(cfg);
    if (!data->has_wheel || now - data->last_wheel_ms > cfg->idle_ms) {
        data->wheel_mode = mode;
        data->has_wheel = true;
    }
    data->last_wheel_ms = now;

    /* Inertia and layer changes can overlap; discard the rest of that burst. */
    return mode == data->wheel_mode ? ZMK_INPUT_PROC_CONTINUE : ZMK_INPUT_PROC_STOP;
}

static const struct zmk_input_processor_driver_api trackpad_intercept_driver_api = {
    .handle_event = trackpad_intercept_handle_event,
};

#define TRACKPAD_INTERCEPT_INST(n)                                                                 \
    static struct trackpad_intercept_data trackpad_intercept_data_##n;                             \
    static const struct trackpad_intercept_config trackpad_intercept_config_##n = {                \
        .zoom_layer = DT_INST_PROP(n, zoom_layer),                                                 \
        .volume_layer = DT_INST_PROP(n, volume_layer),                                             \
        .idle_ms = DT_INST_PROP(n, idle_ms),                                                         \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &trackpad_intercept_data_##n,                             \
                          &trackpad_intercept_config_##n, POST_KERNEL,                            \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &trackpad_intercept_driver_api);

DT_INST_FOREACH_STATUS_OKAY(TRACKPAD_INTERCEPT_INST)
