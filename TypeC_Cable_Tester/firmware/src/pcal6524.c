#include "pcal6524.h"

#include <string.h>

static bool write_ports(const pcal6524_t *device, uint8_t register_address, const uint8_t values[PCAL6524_PORT_COUNT])
{
    if ((device == NULL) || (device->platform == NULL) || (device->platform->i2c_write == NULL)) {
        return false;
    }
    return device->platform->i2c_write(
        device->platform->context,
        device->address_7bit,
        register_address,
        values,
        PCAL6524_PORT_COUNT);
}

static bool read_ports(const pcal6524_t *device, uint8_t register_address, uint8_t values[PCAL6524_PORT_COUNT])
{
    if ((device == NULL) || (values == NULL) || (device->platform == NULL) ||
        (device->platform->i2c_read == NULL)) {
        return false;
    }
    return device->platform->i2c_read(
        device->platform->context,
        device->address_7bit,
        register_address,
        values,
        PCAL6524_PORT_COUNT);
}

void pcal6524_bind(pcal6524_t *device, const tester_platform_t *platform, uint8_t address_7bit)
{
    if (device != NULL) {
        device->platform = platform;
        device->address_7bit = address_7bit;
    }
}

bool pcal6524_probe(const pcal6524_t *device)
{
    uint8_t configuration[PCAL6524_PORT_COUNT];
    return read_ports(device, PCAL6524_REG_CONFIGURATION_PORT_0, configuration);
}

bool pcal6524_set_all_inputs(const pcal6524_t *device)
{
    static const uint8_t all_inputs[PCAL6524_PORT_COUNT] = {0xFFu, 0xFFu, 0xFFu};
    return write_ports(device, PCAL6524_REG_CONFIGURATION_PORT_0, all_inputs);
}

bool pcal6524_init_test_inputs(const pcal6524_t *device)
{
    static const uint8_t all_enabled[PCAL6524_PORT_COUNT] = {0xFFu, 0xFFu, 0xFFu};

    if (!pcal6524_set_all_inputs(device)) {
        return false;
    }
    if (!write_ports(device, PCAL6524_REG_PULL_SELECT_PORT_0, all_enabled)) {
        return false;
    }
    return write_ports(device, PCAL6524_REG_PULL_ENABLE_PORT_0, all_enabled);
}

bool pcal6524_verify_test_inputs(const pcal6524_t *device)
{
    static const uint8_t expected[PCAL6524_PORT_COUNT] = {0xFFu, 0xFFu, 0xFFu};
    uint8_t actual[PCAL6524_PORT_COUNT];

    if (!pcal6524_read_configuration(device, actual) ||
        (memcmp(actual, expected, sizeof(actual)) != 0)) {
        return false;
    }
    if (!read_ports(device, PCAL6524_REG_PULL_SELECT_PORT_0, actual) ||
        (memcmp(actual, expected, sizeof(actual)) != 0)) {
        return false;
    }
    return read_ports(device, PCAL6524_REG_PULL_ENABLE_PORT_0, actual) &&
           (memcmp(actual, expected, sizeof(actual)) == 0);
}

bool pcal6524_read_configuration(
    const pcal6524_t *device,
    uint8_t configuration[PCAL6524_PORT_COUNT])
{
    return read_ports(device, PCAL6524_REG_CONFIGURATION_PORT_0, configuration);
}

bool pcal6524_verify_all_inputs(const pcal6524_t *device)
{
    static const uint8_t all_inputs[PCAL6524_PORT_COUNT] = {0xFFu, 0xFFu, 0xFFu};
    uint8_t configuration[PCAL6524_PORT_COUNT];

    return pcal6524_read_configuration(device, configuration) &&
           (memcmp(configuration, all_inputs, sizeof(configuration)) == 0);
}

bool pcal6524_drive_one_low(const pcal6524_t *device, uint8_t pin_index)
{
    uint8_t output[PCAL6524_PORT_COUNT] = {0xFFu, 0xFFu, 0xFFu};
    uint8_t configuration[PCAL6524_PORT_COUNT] = {0xFFu, 0xFFu, 0xFFu};
    uint8_t port;
    uint8_t bit;

    if (pin_index >= PCAL6524_PIN_COUNT) {
        return false;
    }

    port = (uint8_t)(pin_index >> 3u);
    bit = (uint8_t)(pin_index & 7u);
    output[port] &= (uint8_t)~(uint8_t)(1u << bit);
    configuration[port] &= (uint8_t)~(uint8_t)(1u << bit);

    if (!pcal6524_set_all_inputs(device)) {
        return false;
    }
    if (!write_ports(device, PCAL6524_REG_OUTPUT_PORT_0, output)) {
        return false;
    }
    return write_ports(device, PCAL6524_REG_CONFIGURATION_PORT_0, configuration);
}

bool pcal6524_read_inputs(const pcal6524_t *device, uint8_t input_ports[PCAL6524_PORT_COUNT])
{
    return read_ports(device, PCAL6524_REG_INPUT_PORT_0, input_ports);
}

bool pcal6524_init_led_outputs(const pcal6524_t *device)
{
    static const uint8_t all_off[PCAL6524_PORT_COUNT] = {0xFFu, 0xFFu, 0xFFu};
    static const uint8_t all_outputs[PCAL6524_PORT_COUNT] = {0x00u, 0x00u, 0x00u};

    if (!write_ports(device, PCAL6524_REG_OUTPUT_PORT_0, all_off)) {
        return false;
    }
    return write_ports(device, PCAL6524_REG_CONFIGURATION_PORT_0, all_outputs);
}

bool pcal6524_write_led_bitmap(const pcal6524_t *device, const uint8_t led_on_bitmap[PCAL6524_PORT_COUNT])
{
    uint8_t active_low[PCAL6524_PORT_COUNT];
    uint8_t index;

    if (led_on_bitmap == NULL) {
        return false;
    }
    for (index = 0u; index < PCAL6524_PORT_COUNT; ++index) {
        active_low[index] = (uint8_t)~led_on_bitmap[index];
    }
    return write_ports(device, PCAL6524_REG_OUTPUT_PORT_0, active_low);
}
