/* SPDX-License-Identifier: MIT */

#define DT_DRV_COMPAT zmk_input_processor_responsive_scroll

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/kernel.h>

#include <drivers/input_processor.h>

struct responsive_scroll_config {
    uint32_t reset_after_ms;
};

struct responsive_scroll_data {
    int64_t last_wheel_time;
    int8_t last_direction;
};

static int responsive_scroll_handle_event(const struct device *dev, struct input_event *event,
                                          uint32_t multiplier, uint32_t divisor,
                                          struct zmk_input_processor_state *state) {
    if (event->type != INPUT_EV_REL || event->code != INPUT_REL_WHEEL || event->value == 0) {
        return ZMK_INPUT_PROC_CONTINUE;
    }
    if (divisor == 0 || state->remainder == NULL) {
        return -EINVAL;
    }

    const struct responsive_scroll_config *cfg = dev->config;
    struct responsive_scroll_data *data = dev->data;
    int64_t now = k_uptime_get();
    int8_t direction = event->value > 0 ? 1 : -1;
    bool new_gesture = data->last_wheel_time == 0 ||
                       now - data->last_wheel_time > cfg->reset_after_ms ||
                       direction != data->last_direction;

    if (new_gesture) {
        *state->remainder = 0;
    }

    int64_t total = (int64_t)event->value * multiplier + *state->remainder;
    int64_t scaled = total / divisor;

    if (new_gesture && scaled == 0 && multiplier != 0) {
        /* Give a small first movement one immediate wheel step. */
        scaled = direction;
        *state->remainder = 0;
    } else {
        *state->remainder = total - scaled * divisor;
    }

    if (scaled > INT32_MAX) {
        scaled = INT32_MAX;
        *state->remainder = 0;
    } else if (scaled < INT32_MIN) {
        scaled = INT32_MIN;
        *state->remainder = 0;
    }

    event->value = (int32_t)scaled;
    data->last_wheel_time = now;
    data->last_direction = direction;
    return ZMK_INPUT_PROC_CONTINUE;
}

static const struct zmk_input_processor_driver_api responsive_scroll_driver_api = {
    .handle_event = responsive_scroll_handle_event,
};

#define RESPONSIVE_SCROLL_INST(n)                                                                  \
    static struct responsive_scroll_data responsive_scroll_data_##n;                              \
    static const struct responsive_scroll_config responsive_scroll_config_##n = {                  \
        .reset_after_ms = DT_INST_PROP(n, reset_after_ms),                                         \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &responsive_scroll_data_##n,                              \
                          &responsive_scroll_config_##n, POST_KERNEL,                            \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &responsive_scroll_driver_api);

DT_INST_FOREACH_STATUS_OKAY(RESPONSIVE_SCROLL_INST)
