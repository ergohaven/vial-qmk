#pragma once

#ifdef OLED_ENABLE
#    define OLED_DISPLAY_128X64
#define I2C1_SCL_PIN        GP11
#define I2C1_SDA_PIN        GP10
#define OLED_BRIGHTNESS 128
#define I2C_DRIVER I2CD1
#endif
