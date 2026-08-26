/*
 * STM32Cube integration template. This file is intentionally excluded from the
 * portable CMake target until CubeMX has generated main.h, i2c.h and USB CDC.
 * Rename/copy it into the generated Core/Src tree and replace the GPIO aliases
 * after the final schematic pin map is frozen.
 */

#include "tester_app.h"

#include "i2c.h"
#include "main.h"
#include "usbd_cdc_if.h"

static tester_app_t tester_app;

#define TESTER_USB_TX_RING_SIZE 8192u
#define TESTER_USB_TX_PACKET_SIZE 64u

static uint8_t usb_tx_ring[TESTER_USB_TX_RING_SIZE];
static volatile uint16_t usb_tx_head;
static volatile uint16_t usb_tx_tail;
static volatile uint16_t usb_tx_count;
static volatile uint16_t usb_tx_inflight_length;
static volatile bool usb_tx_inflight;

static bool board_i2c_write(
    void *context,
    uint8_t address_7bit,
    uint8_t register_address,
    const uint8_t *data,
    size_t data_length)
{
    (void)context;
    return HAL_I2C_Mem_Write(
               &hi2c1,
               (uint16_t)(address_7bit << 1u),
               register_address,
               I2C_MEMADD_SIZE_8BIT,
               (uint8_t *)data,
               (uint16_t)data_length,
               20u) == HAL_OK;
}

static bool board_i2c_read(
    void *context,
    uint8_t address_7bit,
    uint8_t register_address,
    uint8_t *data,
    size_t data_length)
{
    (void)context;
    return HAL_I2C_Mem_Read(
               &hi2c1,
               (uint16_t)(address_7bit << 1u),
               register_address,
               I2C_MEMADD_SIZE_8BIT,
               data,
               (uint16_t)data_length,
               20u) == HAL_OK;
}

static bool board_usb_write(void *context, const uint8_t *data, size_t data_length)
{
    uint32_t primask;
    size_t index;

    (void)context;
    if ((data == NULL) || (data_length > TESTER_USB_TX_RING_SIZE)) {
        return false;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    if (data_length > (size_t)(TESTER_USB_TX_RING_SIZE - usb_tx_count)) {
        if (primask == 0u) {
            __enable_irq();
        }
        return false;
    }
    for (index = 0u; index < data_length; ++index) {
        usb_tx_ring[usb_tx_head] = data[index];
        usb_tx_head = (uint16_t)((usb_tx_head + 1u) % TESTER_USB_TX_RING_SIZE);
    }
    usb_tx_count = (uint16_t)(usb_tx_count + data_length);
    if (primask == 0u) {
        __enable_irq();
    }
    return true;
}

static void board_usb_pump(void)
{
    uint16_t contiguous;

    if (usb_tx_inflight || (usb_tx_count == 0u)) {
        return;
    }
    contiguous = (uint16_t)(TESTER_USB_TX_RING_SIZE - usb_tx_tail);
    if (contiguous > usb_tx_count) {
        contiguous = usb_tx_count;
    }
    if (contiguous > TESTER_USB_TX_PACKET_SIZE) {
        contiguous = TESTER_USB_TX_PACKET_SIZE;
    }
    if (CDC_Transmit_FS(&usb_tx_ring[usb_tx_tail], contiguous) == USBD_OK) {
        usb_tx_inflight_length = contiguous;
        usb_tx_inflight = true;
    }
}

static void board_set_status_leds(void *context, bool pass_on, bool short_on, bool open_on)
{
    (void)context;
    HAL_GPIO_WritePin(LED_STATUS_GREEN_GPIO_Port, LED_STATUS_GREEN_Pin, pass_on ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_STATUS_RED_GPIO_Port, LED_STATUS_RED_Pin, short_on ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_STATUS_YELLOW_GPIO_Port, LED_STATUS_YELLOW_Pin, open_on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static void board_set_buzzer(void *context, bool enabled)
{
    (void)context;
    HAL_GPIO_WritePin(BUZZER_EN_GPIO_Port, BUZZER_EN_Pin, enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static const tester_platform_t tester_platform = {
    .context = NULL,
    .i2c_write = board_i2c_write,
    .i2c_read = board_i2c_read,
    .usb_write = board_usb_write,
    .set_status_leds = board_set_status_leds,
    .set_buzzer = board_set_buzzer,
};

void TypecTester_Init(void)
{
    tester_scan_config_t scan_config = tester_scan_default_config();

    /* BUZZER_EN must already be configured low before this call. */
    /* Replace scan_config.contact_to_pcal_pin after the final pin/net audit. */
    (void)tester_app_init_with_scan_config(&tester_app, &tester_platform, HAL_GetTick(), &scan_config);
}

void TypecTester_Tick(void)
{
    bool start_pressed = HAL_GPIO_ReadPin(START_KEY_GPIO_Port, START_KEY_Pin) == GPIO_PIN_RESET;
    board_usb_pump();
    tester_app_tick(&tester_app, HAL_GetTick(), start_pressed);
    board_usb_pump();
}

void TypecTester_CdcLineReceived(const char *line)
{
    tester_app_handle_command(&tester_app, line, HAL_GetTick());
}

/* Call this from the generated CDC_TransmitCplt_FS callback. */
void TypecTester_CdcTransmitComplete(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    if (usb_tx_inflight) {
        usb_tx_tail = (uint16_t)((usb_tx_tail + usb_tx_inflight_length) % TESTER_USB_TX_RING_SIZE);
        usb_tx_count = (uint16_t)(usb_tx_count - usb_tx_inflight_length);
        usb_tx_inflight_length = 0u;
        usb_tx_inflight = false;
    }
    if (primask == 0u) {
        __enable_irq();
    }
}
