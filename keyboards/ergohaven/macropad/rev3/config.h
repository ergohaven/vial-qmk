#pragma once

// W25Q32JV: keep the historical EEPROM slot while declaring the full 4 MiB chip.
#define WEAR_LEVELING_RP2040_FLASH_BASE 0x001FC000u

// Render a complete 240x280 frame before flushing it, then drive the ST7789
// at its practical RP2040 SPI ceiling. This removes the ten visible LVGL bands.
#define QP_LVGL_BUFFER_DIVISOR 1
#define EH_DISPLAY_SPI_DIVISOR 2

// SPI config for display
#define SPI_DRIVER SPID0
#define SPI_SCK_PIN GP18
#define SPI_MOSI_PIN GP19
#define SPI_MISO_PIN GP20

// LCD config
#define LCD_DC_PIN GP21
#define LCD_CS_PIN GP22
#define LCD_RST_PIN GP20
#define BACKLIGHT_PWM_DRIVER PWMD3
#define BACKLIGHT_PWM_CHANNEL RP2040_PWM_CHANNEL_B
// The ChibiOS backlight driver applies a CIE lightness curve after this limit.
// 245 produces an actual maximum PWM duty of approximately 89.8%.
#define BACKLIGHT_LIMIT_VAL 245
#define QUANTUM_PAINTER_LVGL_USE_CUSTOM_CONF
#define QUANTUM_PAINTER_DISPLAY_TIMEOUT 0

#define TAP_CODE_DELAY 1

#define EH_SHORT_PRODUCT_NAME "M4CR0Pad"
#define EH_HAS_DISPLAY
#define EH_HOME_HIDE_MODIFIERS
#define EH_STANDBY_BACKGROUND_ENABLE
// Use built-in startup artwork; obsolete custom splash flash is ignored.
#define EH_SEQUENTIAL_LAYER_FALLBACK
#define EH_CLOCK_FONT_CHOICES_ENABLE
#define EH_ENCODER_WAKE_INTERRUPTS
#define EH_RGB_MATRIX_RUNTIME_TIMEOUT

#define RGB_MATRIX_DEFAULT_VAL 100
#define RGB_MATRIX_DEFAULT_MODE RGB_MATRIX_CYCLE_LEFT_RIGHT
