#pragma once

// Revision-specific display wiring and hardware capabilities.

// Limit maximum backlight PWM to the existing production value.
#define SPI_DRIVER SPID0
#define SPI_SCK_PIN GP18
#define SPI_MOSI_PIN GP19
#define SPI_MISO_PIN GP20
#define LCD_DC_PIN GP21
#define LCD_CS_PIN GP22
#define LCD_RST_PIN GP20
#define BACKLIGHT_PWM_DRIVER PWMD3
#define BACKLIGHT_LIMIT_VAL 245
#define EH_RGB_MATRIX_RUNTIME_TIMEOUT
#define RGB_MATRIX_DEFAULT_VAL 100
#define RGB_MATRIX_DEFAULT_MODE RGB_MATRIX_CYCLE_LEFT_RIGHT
