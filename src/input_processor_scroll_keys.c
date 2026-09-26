/* SPDX-License-Identifier: MIT */

#define DT_DRV_COMPAT zmk_input_processor_scroll_keys

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include <drivers/input_processor.h>
#include <zmk/behavior.h>
#include <zmk/input_listeners.h>
#include <zmk/keymap.h>
#include <zmk/virtual_key_position.h>

struct scroll_keys_config {
    uint8_t index;
    const struct zmk_behavior_binding *bindings;
};

static int scroll_keys_handle_event(const struct device *dev, struct input_event *event,
                                    uint32_t param1, uint32_t param2,
                                    struct zmk_input_processor_state *state) {
    const struct scroll_keys_config *cfg = dev->config;

    if (event->type != INPUT_EV_REL || event->code != INPUT_REL_WHEEL) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    const struct zmk_behavior_binding *binding = &cfg->bindings[event->value < 0];
    struct zmk_behavior_binding_event behavior_event = {
        .position = ZMK_VIRTUAL_KEY_POSITION_BEHAVIOR_INPUT_PROCESSOR(
            state->input_device_index, cfg->index),
        .timestamp = k_uptime_get(),
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
        .source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
#endif
    };
    int32_t steps = event->value < 0 ? -(int32_t)event->value : event->value;

    /* Limit a large wheel report so key events cannot saturate the input thread. */
    for (int32_t i = 0; i < MIN(steps, 4); i++) {
        int press_ret = zmk_behavior_invoke_binding(binding, behavior_event, true);
        int release_ret = zmk_behavior_invoke_binding(binding, behavior_event, false);
        if (press_ret < 0 || release_ret < 0) {
            return press_ret < 0 ? press_ret : release_ret;
        }
    }

    return ZMK_INPUT_PROC_STOP;
}

static const struct zmk_input_processor_driver_api scroll_keys_driver_api = {
    .handle_event = scroll_keys_handle_event,
};

#define SCROLL_KEYS_INST(n)                                                                        \
    BUILD_ASSERT(DT_INST_PROP_LEN(n, bindings) == 2, "scroll keys need two bindings");           \
    static const struct zmk_behavior_binding scroll_keys_bindings_##n[] = {                       \
        LISTIFY(2, ZMK_KEYMAP_EXTRACT_BINDING, (, ), DT_DRV_INST(n))};                             \
    static const struct scroll_keys_config scroll_keys_config_##n = {                             \
        .index = n, .bindings = scroll_keys_bindings_##n,                                          \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, &scroll_keys_config_##n, POST_KERNEL,              \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &scroll_keys_driver_api);

DT_INST_FOREACH_STATUS_OKAY(SCROLL_KEYS_INST)
