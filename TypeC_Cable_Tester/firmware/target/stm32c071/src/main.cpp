#include <Arduino.h>
#include <Wire.h>

extern "C" {
#include "PeripheralPins.h"
#include "stm32c0xx_hal.h"
#include "stm32c0xx_hal_rcc_ex.h"
#include "stm32yyxx_ll_utils.h"
#include "tester_app.h"
}

/*
 * The generic STM32C071GU Arduino variant also lists PA4, PA13 and PA15 as
 * optional USB_NOE pins.  The framework configures every entry in that weak
 * table, which changes PA13 away from its reset-default SWDIO function after
 * USB starts.  This board only routes USB D-/D+ on PA11/PA12, so provide a
 * board-specific strong definition and keep SWD available while firmware runs.
 */
extern const PinMap PinMap_USB_DRD_FS[] = {
    {PA_11, USB_DRD_FS, STM_PIN_DATA(STM_MODE_INPUT, GPIO_NOPULL, GPIO_AF_NONE)},
    {PA_12, USB_DRD_FS, STM_PIN_DATA(STM_MODE_INPUT, GPIO_NOPULL, GPIO_AF_NONE)},
    {NC, NP, 0},
};

namespace {

constexpr uint32_t kLedPassPin = PA2;
constexpr uint32_t kLedShortPin = PA3;
constexpr uint32_t kLedOpenPin = PA4;
constexpr uint32_t kBuzzerPin = PA5;
constexpr uint32_t kStartKeyPin = PA0;
constexpr uint32_t kPcalInterruptPin = PA1;
constexpr uint32_t kPcalResetPin = PB1;
constexpr uint32_t kI2cSdaPin = PB7;
constexpr uint32_t kI2cSclPin = PB6;

constexpr uint32_t kI2cClockHz = 400000u;
constexpr uint32_t kI2cTimeoutMs = 20u;
constexpr size_t kCommandCapacity = 96u;
constexpr uint8_t kUsbBytesPerLoop = 32u;
#if defined(TYPEC_TESTER_OFFICE_SILENT) && TYPEC_TESTER_OFFICE_SILENT
constexpr bool kBuzzerMuted = true;
#else
constexpr bool kBuzzerMuted = false;
#endif

TwoWire g_pcal_wire(kI2cSdaPin, kI2cSclPin);
tester_app_t g_app;
char g_command[kCommandCapacity];
size_t g_command_length = 0u;
bool g_command_overflow = false;
bool g_usb_announced = false;
bool g_pcal_fail_safe_asserted = false;

bool pcal_i2c_write(
    void *,
    uint8_t address_7bit,
    uint8_t register_address,
    const uint8_t *data,
    size_t data_length)
{
    if (((data == nullptr) && (data_length != 0u)) || (data_length > UINT16_MAX)) {
        return false;
    }

    return HAL_I2C_Mem_Write(
               g_pcal_wire.getHandle(),
               static_cast<uint16_t>(address_7bit << 1u),
               register_address,
               I2C_MEMADD_SIZE_8BIT,
               const_cast<uint8_t *>(data),
               static_cast<uint16_t>(data_length),
               kI2cTimeoutMs) == HAL_OK;
}

bool pcal_i2c_read(
    void *,
    uint8_t address_7bit,
    uint8_t register_address,
    uint8_t *data,
    size_t data_length)
{
    if (((data == nullptr) && (data_length != 0u)) || (data_length > UINT16_MAX)) {
        return false;
    }

    return HAL_I2C_Mem_Read(
               g_pcal_wire.getHandle(),
               static_cast<uint16_t>(address_7bit << 1u),
               register_address,
               I2C_MEMADD_SIZE_8BIT,
               data,
               static_cast<uint16_t>(data_length),
               kI2cTimeoutMs) == HAL_OK;
}

bool usb_write(void *, const uint8_t *data, size_t data_length)
{
    if ((data == nullptr) && (data_length != 0u)) {
        return false;
    }
    /* The framework fragments long reports and stops promptly if CDC disconnects. */
    return SerialUSB.write(data, data_length) == data_length;
}

void set_status_leds(void *, bool pass_on, bool short_on, bool open_on)
{
    digitalWrite(kLedPassPin, pass_on ? LOW : HIGH);
    digitalWrite(kLedShortPin, short_on ? LOW : HIGH);
    digitalWrite(kLedOpenPin, open_on ? LOW : HIGH);
}

void set_buzzer(void *, bool enabled)
{
    digitalWrite(kBuzzerPin, (!kBuzzerMuted && enabled) ? HIGH : LOW);
}

const tester_platform_t kPlatform = {
    nullptr,
    pcal_i2c_write,
    pcal_i2c_read,
    usb_write,
    set_status_leds,
    set_buzzer,
};

void configure_safe_gpio()
{
    /* Preload safe output levels before changing the GPIO direction. */
    digitalWrite(kBuzzerPin, LOW);
    pinMode(kBuzzerPin, OUTPUT);

    digitalWrite(kLedPassPin, HIGH);
    digitalWrite(kLedShortPin, HIGH);
    digitalWrite(kLedOpenPin, HIGH);
    pinMode(kLedPassPin, OUTPUT);
    pinMode(kLedShortPin, OUTPUT);
    pinMode(kLedOpenPin, OUTPUT);

    pinMode(kStartKeyPin, INPUT);       /* External 10 kOhm pull-up; active low. */
    pinMode(kPcalInterruptPin, INPUT);  /* External 10 kOhm pull-up; polling is used. */

    digitalWrite(kPcalResetPin, LOW);
    pinMode(kPcalResetPin, OUTPUT);
}

void reset_pcal_devices()
{
    /* Shared RESET# for U2/U3/U5. Long margins simplify first-board bring-up. */
    delay(10u);
    digitalWrite(kPcalResetPin, HIGH);
    delay(2u);
}

void configure_usb_clock_recovery()
{
    RCC_CRSInitTypeDef crs = {};

    __HAL_RCC_CRS_CLK_ENABLE();
    crs.Prescaler = RCC_CRS_SYNC_DIV1;
    crs.Source = RCC_CRS_SYNC_SOURCE_USB;
    crs.Polarity = RCC_CRS_SYNC_POLARITY_RISING;
    crs.ReloadValue = RCC_CRS_RELOADVALUE_DEFAULT;
    crs.ErrorLimitValue = RCC_CRS_ERRORLIMIT_DEFAULT;
    crs.HSI48CalibrationValue = RCC_CRS_HSI48CALIBRATION_DEFAULT;
    HAL_RCCEx_CRSConfig(&crs);
}

void send_usb_text(const char *text)
{
    if (text != nullptr) {
        (void)usb_write(nullptr, reinterpret_cast<const uint8_t *>(text), strlen(text));
    }
}

void submit_command(uint32_t now_ms)
{
    if (g_command_overflow) {
        send_usb_text("ERR COMMAND_TOO_LONG\r\n");
    } else if (g_command_length != 0u) {
        g_command[g_command_length] = '\0';
        tester_app_handle_command(&g_app, g_command, now_ms);
    }

    g_command_length = 0u;
    g_command_overflow = false;
}

void service_usb_commands(uint32_t now_ms)
{
    uint8_t processed = 0u;

    while ((processed < kUsbBytesPerLoop) && (SerialUSB.available() > 0)) {
        int incoming = SerialUSB.read();
        ++processed;
        if (incoming < 0) {
            break;
        }

        char character = static_cast<char>(incoming);
        if (character == '\r') {
            continue;
        }
        if (character == '\n') {
            submit_command(now_ms);
            continue;
        }
        if ((character == '\b') || (character == 0x7f)) {
            if (g_command_length != 0u) {
                --g_command_length;
            }
            continue;
        }
        if ((character >= 'a') && (character <= 'z')) {
            character = static_cast<char>(character - ('a' - 'A'));
        }

        if (g_command_length < (kCommandCapacity - 1u)) {
            g_command[g_command_length++] = character;
        } else {
            g_command_overflow = true;
        }
    }
}

}  // namespace

extern "C" void HAL_MspInit(void)
{
    /* This runs inside HAL_Init(), before USB and before Arduino setup(). */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* PA2/3/4 high = LEDs off, PA5 low = buzzer off, PB1 low = PCAL reset held. */
    GPIOA->BSRR = GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 |
                  (static_cast<uint32_t>(GPIO_PIN_5) << 16u);
    GPIOB->BSRR = static_cast<uint32_t>(GPIO_PIN_1) << 16u;

    MODIFY_REG(
        GPIOA->MODER,
        GPIO_MODER_MODE2 | GPIO_MODER_MODE3 | GPIO_MODER_MODE4 | GPIO_MODER_MODE5,
        GPIO_MODER_MODE2_0 | GPIO_MODER_MODE3_0 | GPIO_MODER_MODE4_0 | GPIO_MODER_MODE5_0);
    MODIFY_REG(GPIOB->MODER, GPIO_MODER_MODE1, GPIO_MODER_MODE1_0);
}

extern "C" void SystemClock_Config(void)
{
    RCC_PeriphCLKInitTypeDef peripheral_clock = {};

    LL_FLASH_SetLatency(LL_FLASH_LATENCY_1);

    LL_RCC_HSI_Enable();
    while (LL_RCC_HSI_IsReady() != 1u) {
    }
    LL_RCC_HSI_SetCalibTrimming(64u);
    LL_RCC_SetHSIDiv(LL_RCC_HSI_DIV_1);

    LL_RCC_HSI48_Enable();
    while (LL_RCC_HSI48_IsReady() != 1u) {
    }

    LL_RCC_SetAHBPrescaler(LL_RCC_HCLK_DIV_1);
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI) {
    }
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
    LL_SetSystemCoreClock(48000000u);

    peripheral_clock.PeriphClockSelection = RCC_PERIPHCLK_USB;
    peripheral_clock.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
    if (HAL_RCCEx_PeriphCLKConfig(&peripheral_clock) != HAL_OK) {
        Error_Handler();
    }

    /* USB SOF trims HSI48 before the framework initializes CDC. */
    configure_usb_clock_recovery();

    if (HAL_InitTick(TICK_INT_PRIORITY) != HAL_OK) {
        Error_Handler();
    }
}

void setup()
{
    configure_safe_gpio();
    reset_pcal_devices();

    g_pcal_wire.begin();
    g_pcal_wire.setClock(kI2cClockHz);

    tester_scan_config_t scan_config = tester_scan_default_config();
    (void)tester_app_init_with_scan_config(&g_app, &kPlatform, millis(), &scan_config);
}

void loop()
{
    uint32_t now_ms = millis();

    if (!g_usb_announced && SerialUSB.dtr()) {
        send_usb_text("TYPEC_TESTER READY FW=" TESTER_FIRMWARE_VERSION " PROFILE=AUTO\r\n");
        tester_app_handle_command(&g_app, "STATUS", now_ms);
        g_usb_announced = true;
    }

    service_usb_commands(now_ms);
    tester_app_tick(&g_app, now_ms, digitalRead(kStartKeyPin) == LOW);
    if ((g_app.state == TESTER_APP_HARDWARE_ERROR) && !g_pcal_fail_safe_asserted) {
        /* A shared hardware reset guarantees U2/U3 return to high impedance. */
        digitalWrite(kPcalResetPin, LOW);
        g_pcal_fail_safe_asserted = true;
    }
    yield();
}
