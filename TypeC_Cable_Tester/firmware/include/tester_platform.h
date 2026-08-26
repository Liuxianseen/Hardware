#ifndef TESTER_PLATFORM_H
#define TESTER_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*tester_i2c_write_fn)(
    void *context,
    uint8_t address_7bit,
    uint8_t register_address,
    const uint8_t *data,
    size_t data_length);

typedef bool (*tester_i2c_read_fn)(
    void *context,
    uint8_t address_7bit,
    uint8_t register_address,
    uint8_t *data,
    size_t data_length);

typedef bool (*tester_usb_write_fn)(void *context, const uint8_t *data, size_t data_length);

typedef struct {
    void *context;
    tester_i2c_write_fn i2c_write;
    tester_i2c_read_fn i2c_read;
    tester_usb_write_fn usb_write;
    void (*set_status_leds)(void *context, bool pass_on, bool short_on, bool open_on);
    void (*set_buzzer)(void *context, bool enabled);
} tester_platform_t;

#ifdef __cplusplus
}
#endif

#endif
