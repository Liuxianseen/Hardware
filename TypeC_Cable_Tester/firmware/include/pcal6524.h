#ifndef PCAL6524_H
#define PCAL6524_H

#include "tester_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PCAL6524_PORT_COUNT 3u
#define PCAL6524_PIN_COUNT 24u

#define PCAL6524_REG_INPUT_PORT_0 0x00u
#define PCAL6524_REG_OUTPUT_PORT_0 0x04u
#define PCAL6524_REG_CONFIGURATION_PORT_0 0x0Cu
#define PCAL6524_REG_PULL_ENABLE_PORT_0 0x4Cu
#define PCAL6524_REG_PULL_SELECT_PORT_0 0x50u

typedef struct {
    const tester_platform_t *platform;
    uint8_t address_7bit;
} pcal6524_t;

void pcal6524_bind(pcal6524_t *device, const tester_platform_t *platform, uint8_t address_7bit);
bool pcal6524_probe(const pcal6524_t *device);
bool pcal6524_set_all_inputs(const pcal6524_t *device);
bool pcal6524_init_test_inputs(const pcal6524_t *device);
bool pcal6524_verify_test_inputs(const pcal6524_t *device);
bool pcal6524_read_configuration(
    const pcal6524_t *device,
    uint8_t configuration[PCAL6524_PORT_COUNT]);
bool pcal6524_verify_all_inputs(const pcal6524_t *device);
bool pcal6524_drive_one_low(const pcal6524_t *device, uint8_t pin_index);
bool pcal6524_read_inputs(const pcal6524_t *device, uint8_t input_ports[PCAL6524_PORT_COUNT]);
bool pcal6524_init_led_outputs(const pcal6524_t *device);
bool pcal6524_write_led_bitmap(const pcal6524_t *device, const uint8_t led_on_bitmap[PCAL6524_PORT_COUNT]);

#ifdef __cplusplus
}
#endif

#endif
