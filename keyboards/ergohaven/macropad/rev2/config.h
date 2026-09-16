#pragma once

// RP2040-Zero has 2 MiB flash; preserve the EEPROM slot at its end.
#define WEAR_LEVELING_RP2040_FLASH_BASE 0x001FC000u

// Render a complete 240x280 frame before flushing it, then drive the ST7789
// at its practical RP2040 SPI ceiling. This removes the ten visible LVGL bands.
#define QP_LVGL_BUFFER_DIVISOR 1
#define EH_DISPLAY_SPI_DIVISOR 2

// SPI config for display
#define SPI_DRIVER SPID1
#define SPI_SCK_PIN GP10
#define SPI_MOSI_PIN GP11
#define SPI_MISO_PIN GP29

// LCD config
#define LCD_DC_PIN GP26
#define LCD_CS_PIN GP15
#define LCD_RST_PIN GP29
#define BACKLIGHT_PWM_DRIVER PWMD0
#define BACKLIGHT_PWM_CHANNEL RP2040_PWM_CHANNEL_B
#define QUANTUM_PAINTER_LVGL_USE_CUSTOM_CONF
#define QUANTUM_PAINTER_DISPLAY_TIMEOUT 0

#define TAP_CODE_DELAY 1

#define EH_SHORT_PRODUCT_NAME "M4CR0Pad"
#define EH_HAS_DISPLAY
#define EH_HOME_HIDE_MODIFIERS
#define EH_STANDBY_BACKGROUND_ENABLE
// Use built-in startup artwork; obsolete custom splash flash is ignored.
#define EH_SEQUENTIAL_LAYER_FALLBACK
#define EH_ENCODER_WAKE_INTERRUPTS

#define EH_CLOCK_FONT_CHOICES_ENABLE
