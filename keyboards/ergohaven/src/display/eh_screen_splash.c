#include "eh_display.h"
#include <lvgl.h>

#include "ergohaven.h"
#include "lvgl_helpers.h"
#ifdef EH_STARTUP_IMAGE_ENABLE
#    include "eh_startup_image.h"
#endif

static lv_obj_t *screen_splash;
static lv_obj_t *label_version;
#ifdef EH_STARTUP_IMAGE_ENABLE
static lv_img_dsc_t startup_image;
#endif

LV_IMG_DECLARE(eh_logo);

void splash_screen_init(void) {
    screen_splash = lv_obj_create(NULL);
    // Startup artwork is independent of the user's main/standby background.
    lv_obj_set_style_bg_color(screen_splash, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen_splash, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen_splash, 0, 0);
    lv_obj_set_style_border_width(screen_splash, 0, 0);
    lv_obj_clear_flag(screen_splash, LV_OBJ_FLAG_SCROLLABLE);

#ifdef EH_STARTUP_IMAGE_ENABLE
    eh_startup_image_init();
    if (eh_startup_image_is_valid()) {
        startup_image = (lv_img_dsc_t){
            .header.always_zero = 0,
            .header.w = EH_STARTUP_IMAGE_WIDTH,
            .header.h = EH_STARTUP_IMAGE_HEIGHT,
            .header.cf = LV_IMG_CF_INDEXED_8BIT,
            .data_size = EH_STARTUP_IMAGE_DATA_SIZE,
            .data = eh_startup_image_data(),
        };
        lv_obj_t *img = lv_img_create(screen_splash);
        lv_img_set_src(img, &startup_image);
        lv_obj_set_pos(img, 0, 0);
        return;
    }
#endif


    lv_obj_t *img = lv_img_create(screen_splash);
    lv_img_set_src(img, &eh_logo);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);

    label_version = lv_label_create(screen_splash);
    lv_label_set_text(label_version, "v" EH_VERSION_STR);
    lv_obj_align(label_version, LV_ALIGN_BOTTOM_MID, 0, -16);
}

void splash_screen_load(void) {
    lv_scr_load(screen_splash);
}

void splash_screen_housekeep(void) {
}

const eh_screen_t eh_screen_splash = {
    .init      = splash_screen_init,
    .load      = splash_screen_load,
    .housekeep = splash_screen_housekeep,
};
