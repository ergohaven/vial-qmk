#include "display.h"
#include "qp.h"
#include "lvgl_helpers.h"
#include "ergohaven_ruen.h"
#include "hid.h"
#include "ergohaven.h"

LV_FONT_DECLARE(ergohaven_symbols);
LV_IMG_DECLARE(Infostart);

#define EH_SYMBOL_VOLUME_MUTE "\xEF\x9A\xA9"
#define EH_SYMBOL_DIVIDE "\xEF\x94\xA9"
#define EH_SYMBOL_MULTIPLY "\xEF\x80\x8D"
#define EH_SYMBOL_MOUSE "\xEF\xA3\x8C"
#define EH_SYMBOL_CALC "\xEF\x87\xAC"
#define EH_SYMBOL_MAIL "\xEF\x83\xA0"
#define EH_SYMBOL_SEARCH "\xEF\x80\x82"
#define EH_SYMBOL_HEART "\xEF\x80\x84"
#define EH_SYMBOL_COMPUTER "\xEF\x84\x89"
#define EH_SYMBOL_SUN "\xEF\x86\x85"
#define EH_SYMBOL_MOON "\xEF\x86\x86"
#define EH_SYMBOL_DOWN "\xEF\x8C\x89"
#define EH_SYMBOL_LEFT "\xEF\x8C\x8A"
#define EH_SYMBOL_RIGHT "\xEF\x8C\x8B"
#define EH_SYMBOL_UP "\xEF\x8C\x8C"
#define EH_SYMBOL_ANGLES_LEFT "\xEF\x84\x80"
#define EH_SYMBOL_ANGLES_RIGHT "\xEF\x84\x81"
#define EH_SYMBOL_ANGLES_UP "\xEF\x84\x82"
#define EH_SYMBOL_ANGLES_DOWN "\xEF\x84\x83"
#define EH_SYMBOL_INFO "\xEF\x81\x9A"
#define EH_SYMBOL_ROTATE_RIGHT "\xEF\x8B\xB9"
#define EH_SYMBOL_ROTATE_LEFT "\xEF\x8B\xAA"
#define EH_SYMBOL_GLOBE "\xEF\x82\xAC"
#define EH_SYMBOL_LAYER "\xEF\x97\xBD"

static uint32_t screen_timer = 0;

static bool display_enabled = false;
static bool is_display_on   = false;

typedef enum {
    SCREEN_OFF = -1,
    SCREEN_SPLASH,
    SCREEN_LAYOUT,
    SCREEN_VOLUME,
    SCREEN_HID,
} screen_t;

static screen_t screen_state        = SCREEN_OFF;
static screen_t change_screen_state = SCREEN_OFF;

static painter_device_t display;

/* shared styles */
lv_style_t style_screen;
lv_style_t style_container;
lv_style_t style_button;
lv_style_t style_button_active;

/* screens */
static lv_obj_t *screen_splash;
static lv_obj_t *screen_hid;
static lv_obj_t *screen_layout;
static lv_obj_t *screen_volume;

/* hid screen content */
static lv_obj_t *label_time;
static lv_obj_t *label_layer;
static lv_obj_t *label_media_artist;
static lv_obj_t *label_media_title;
static lv_obj_t *label_layer;
static lv_obj_t *label_layout;

/* layout screen content */
static lv_obj_t *key_labels[15];
static lv_obj_t *label_layer_small;

/* volume screen content */
static lv_obj_t *arc_volume;
static lv_obj_t *label_volume_arc;

/* media screen content */

/* public function to be used in keymaps */
bool is_display_enabled(void) {
    return display_enabled;
}

void init_styles(void) {
    lv_style_init(&style_screen);
    lv_style_set_bg_color(&style_screen, lv_color_black());

    lv_style_init(&style_container);
    lv_style_set_pad_top(&style_container, 0);
    lv_style_set_pad_bottom(&style_container, 0);
    lv_style_set_pad_left(&style_container, 0);
    lv_style_set_pad_right(&style_container, 0);
    lv_style_set_bg_opa(&style_container, 0);
    lv_style_set_border_width(&style_container, 0);
    lv_style_set_width(&style_container, lv_pct(100));
    lv_style_set_height(&style_container, LV_SIZE_CONTENT);

    lv_style_init(&style_button);
    lv_style_set_pad_top(&style_button, 4);
    lv_style_set_pad_bottom(&style_button, 4);
    lv_style_set_pad_left(&style_button, 4);
    lv_style_set_pad_right(&style_button, 4);
    lv_style_set_radius(&style_button, 6);
    lv_style_set_text_color(&style_button, lv_palette_main(LV_PALETTE_TEAL));

    lv_style_init(&style_button_active);
    lv_style_set_bg_color(&style_button_active, lv_palette_main(LV_PALETTE_TEAL));
    lv_style_set_bg_opa(&style_button_active, LV_OPA_100);
    lv_style_set_text_color(&style_button_active, lv_color_black());
}

void init_screen_layout(void) {
    screen_layout = lv_obj_create(NULL);
    lv_obj_add_style(screen_layout, &style_screen, 0);
    use_flex_column(screen_layout);
    lv_obj_set_scrollbar_mode(screen_layout, LV_SCROLLBAR_MODE_OFF);

    label_layer_small = lv_label_create(screen_layout);
    lv_label_set_text(label_layer_small, "layer");
    lv_obj_set_style_pad_top(label_layer_small, 25, 0);
    lv_obj_set_style_pad_bottom(label_layer_small, 25, 0);
    lv_obj_set_style_text_color(label_layer_small, lv_palette_main(LV_PALETTE_TEAL), 0);
    lv_obj_set_style_text_font(label_layer_small, &lv_font_montserrat_28, LV_PART_MAIN);

    lv_obj_t *cont = lv_obj_create(screen_layout);
    lv_obj_set_size(cont, 232, 250);
    // lv_obj_center(cont);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW_WRAP);
    int32_t v = 0;
    lv_obj_set_style_pad_row(cont, v, 0);
    lv_obj_set_style_pad_column(cont, v, 0);
    lv_obj_add_style(cont, &style_container, 0);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_OFF);

    uint32_t i;
    for (i = 0; i < 15; i++) {
        if (i == 12) {
            lv_obj_t *obj = lv_obj_create(cont);
            lv_obj_set_size(obj, 231, 10);
            lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
            lv_obj_add_style(obj, &style_screen, 0);
            lv_obj_set_style_border_opa(obj, 0, 0);
        }
        lv_obj_t *obj = lv_obj_create(cont);
        lv_obj_set_size(obj, 77, 45);
        lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_style(obj, &style_screen, 0);
        if (i >= 12) lv_obj_set_style_border_opa(obj, 0, 0);

        key_labels[i] = lv_label_create(obj);
        lv_obj_center(key_labels[i]);
        lv_obj_set_style_text_font(key_labels[i], &ergohaven_symbols, LV_PART_MAIN);
        lv_obj_set_style_text_align(key_labels[i], LV_TEXT_ALIGN_CENTER, 0);
    }
}

void init_screen_splash(void) {
    screen_splash = lv_obj_create(NULL);
    lv_obj_add_style(screen_splash, &style_screen, 0);
    use_flex_column(screen_splash);

    lv_obj_t *img = lv_img_create(screen_splash);
    lv_img_set_src(img, &Infostart);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
}

void init_screen_hid(void) {
    screen_hid = lv_obj_create(NULL);
    lv_obj_add_style(screen_hid, &style_screen, 0);
    use_flex_column(screen_hid);

    label_time = lv_label_create(screen_hid);
    lv_label_set_text(label_time, "HH:MM");
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_pad_top(label_time, 40, 0);
    lv_obj_set_style_pad_bottom(label_time, 20, 0);

    lv_obj_t *bottom_row = lv_obj_create(screen_hid);
    lv_obj_add_style(bottom_row, &style_container, 0);
    use_flex_row(bottom_row);

    label_layer = lv_label_create(bottom_row);
    lv_label_set_text(label_layer, "");
    lv_obj_set_size(label_layer, 110, 20);
    lv_obj_set_style_text_align(label_layer, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label_layer, &ergohaven_symbols, LV_PART_MAIN);

    label_layout = lv_label_create(bottom_row);
    lv_label_set_text(label_layout, "");
    lv_obj_set_size(label_layout, 110, 20);
    lv_obj_set_style_text_align(label_layout, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label_layout, &ergohaven_symbols, LV_PART_MAIN);

    label_media_title = lv_label_create(screen_hid);
    lv_label_set_text(label_media_title, "");
    lv_label_set_long_mode(label_media_title, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label_media_title, lv_pct(90));
    lv_obj_set_style_text_align(label_media_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label_media_title, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_pad_top(label_media_title, 20, 0);
    lv_obj_set_style_pad_bottom(label_media_title, 0, 0);

    label_media_artist = lv_label_create(screen_hid);
    lv_label_set_text(label_media_artist, "");
    lv_label_set_long_mode(label_media_artist, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label_media_artist, lv_pct(90));
    lv_obj_set_style_text_align(label_media_artist, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_set_style_pad_top(label_media_artist, 0, 0);
    lv_obj_set_style_pad_bottom(label_media_artist, 40, 0);
    lv_obj_set_style_text_color(label_media_artist, lv_palette_main(LV_PALETTE_TEAL), 0);
}

void init_screen_volume(void) {
    screen_volume = lv_obj_create(NULL);
    lv_obj_add_style(screen_volume, &style_screen, 0);

    arc_volume = lv_arc_create(screen_volume);
    lv_obj_set_size(arc_volume, 200, 200);
    lv_obj_center(arc_volume);

    label_volume_arc = lv_label_create(screen_volume);
    lv_label_set_text(label_volume_arc, "00");
    lv_obj_set_style_text_font(label_volume_arc, &lv_font_montserrat_40, LV_PART_MAIN);
    lv_obj_center(label_volume_arc);

    lv_obj_t *volume_text_label = lv_label_create(screen_volume);
    lv_label_set_text(volume_text_label, "Volume");
    lv_obj_align(volume_text_label, LV_ALIGN_BOTTOM_MID, 0, -50);
}

bool display_init_kb(void) {
    display_enabled = false;
    dprint("display_init_kb - start\n");

    gpio_set_pin_output(GP17);

    display = qp_st7789_make_spi_device(240, 280, LCD_CS_PIN, LCD_DC_PIN, LCD_RST_PIN, 16, 3);
    qp_set_viewport_offsets(display, 0, 20);

    if (!qp_init(display, QP_ROTATION_180) || !qp_lvgl_attach(display)) return display_enabled;

    dprint("display_init_kb - initialised\n");

    lv_disp_t  *lv_display = lv_disp_get_default();
    lv_theme_t *lv_theme   = lv_theme_default_init(lv_display, lv_palette_main(LV_PALETTE_TEAL), lv_palette_main(LV_PALETTE_BLUE), true, LV_FONT_DEFAULT);
    lv_disp_set_theme(lv_display, lv_theme);
    init_styles();

    init_screen_splash();
    init_screen_layout();
    init_screen_hid();
    init_screen_volume();
    display_enabled = true;

    display_process_layer_state(layer_state);
    change_screen_state = SCREEN_SPLASH;

    return display_enabled;
}

bool display_process_hid_data(struct hid_data_t *hid_data) {
    static uint32_t hid_sync_time = 0;
    bool            new_hid_data  = false;

    dprintf("display_process_hid_data");
    if (hid_data->time_changed) {
        lv_label_set_text_fmt(label_time, "%02d:%02d", hid_data->hours, hid_data->minutes);
        hid_data->time_changed = false;
        new_hid_data           = true;
    }
    if (hid_data->volume_changed) {
        lv_label_set_text_fmt(label_volume_arc, "%02d", hid_data->volume);
        lv_arc_set_value(arc_volume, hid_data->volume);
        change_screen_state      = SCREEN_VOLUME;
        hid_data->volume_changed = false;
        new_hid_data             = true;
    }
    if (hid_data->media_artist_changed && hid_data->media_title_changed) {
        lv_label_set_text(label_media_artist, hid_data->media_artist);
        lv_label_set_text(label_media_title, hid_data->media_title);
        hid_data->media_artist_changed = false;
        hid_data->media_title_changed  = false;
        change_screen_state            = SCREEN_HID;
        new_hid_data                   = true;
    }
    if (new_hid_data) hid_sync_time = timer_read32();
    return (hid_sync_time != 0) && timer_elapsed32(hid_sync_time) < 61 * 1000;
}

void set_layout_label(uint8_t layout) {
    switch (layout) {
        case LANG_EN:
            lv_label_set_text(label_layout, EH_SYMBOL_GLOBE " EN");
            break;

        case LANG_RU:
            lv_label_set_text(label_layout, EH_SYMBOL_GLOBE " RU");
            break;
    }
}

static const char *PROGMEM LAYER_NAME[] = {
    "ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "TEN", "ELEVEN", "TWELVE", "THIRTEEN", "FOURTEEN", "FIFTEEN",
};

const char *get_layer_name(uint8_t layer) {
    if (layer < 16) {
        return LAYER_NAME[layer];
    } else {
        return "UNDEFINED";
    }
}

const char *basic_keycode_to_str(uint16_t keycode) {
    static char buf[16];
    switch (keycode) {
        case KC_NO:
            return "";

        case KC_1 ... KC_0:
            sprintf(buf, "%d", (keycode - KC_1 + 1) % 10);
            return buf;
        case KC_A ... KC_Z:
            sprintf(buf, "%c", keycode - KC_A + 'A');
            return buf;

        case KC_ENTER:
            return LV_SYMBOL_NEW_LINE;
        case KC_ESCAPE:
            return "Esc";
        case KC_BACKSPACE:
            return LV_SYMBOL_BACKSPACE;
        case KC_TAB:
            return "Tab";
        case KC_SPACE:
            return "Space";
        case KC_MINUS:
            return "-";
        case KC_EQUAL:
            return "=";
        case KC_LEFT_BRACKET:
            return "[";
        case KC_RIGHT_BRACKET:
            return "]";
        case KC_BACKSLASH:
            return "\\";
        case KC_NONUS_HASH:
            return "#";
        case KC_SEMICOLON:
            return ";";
        case KC_QUOTE:
            return "\"";
        case KC_GRAVE:
            return "`";
        case KC_COMMA:
            return ",";
        case KC_DOT:
            return ".";
        case KC_SLASH:
            return "/";
        case KC_CAPS_LOCK:
            return "CpsLk";
        case KC_F1 ... KC_F12:
            sprintf(buf, "F%d", keycode - KC_F1 + 1);
            return buf;
        case KC_PRINT_SCREEN:
            return LV_SYMBOL_IMAGE;
        case KC_SCROLL_LOCK:
            return "ScrLk";
        case KC_PAUSE:
            return "Pause";
        case KC_INSERT:
            return "Ins";
        case KC_HOME:
            return "Home";
        case KC_PAGE_UP:
            return "Pg" LV_SYMBOL_UP;
        case KC_DELETE:
            return "Del";
        case KC_END:
            return "End";
        case KC_PAGE_DOWN:
            return "Pg" LV_SYMBOL_DOWN;
        case KC_RIGHT:
            return EH_SYMBOL_RIGHT;
        case KC_LEFT:
            return EH_SYMBOL_LEFT;
        case KC_DOWN:
            return EH_SYMBOL_DOWN;
        case KC_UP:
            return EH_SYMBOL_UP;
        case KC_NUM_LOCK:
            return "NumLk";
        case KC_KP_SLASH:
            return EH_SYMBOL_DIVIDE;
        case KC_KP_ASTERISK:
            return EH_SYMBOL_MULTIPLY;
        case KC_KP_MINUS:
            return LV_SYMBOL_MINUS;
        case KC_KP_PLUS:
            return LV_SYMBOL_PLUS;
        case KC_KP_ENTER:
            return LV_SYMBOL_NEW_LINE;
        case KC_KP_1 ... KC_KP_0:
            sprintf(buf, "%d", (keycode - KC_KP_1 + 1) % 10);
            return buf;
        case KC_KP_DOT:
            return ".";
        case KC_NONUS_BACKSLASH:
            return "\\";
        case KC_APPLICATION:
            return LV_SYMBOL_SETTINGS;
        case KC_KB_POWER:
            return LV_SYMBOL_POWER;
        case KC_KP_EQUAL:
            return "="; //
        case KC_F13 ... KC_F24:
            sprintf(buf, "F%d", keycode - KC_F13 + 13);
            return buf;
        case KC_EXECUTE:
            return "Exec"; //
        case KC_HELP:
            return EH_SYMBOL_INFO;
        case KC_MENU:
            return "Menu"; //
        case KC_SELECT:
            return LV_SYMBOL_OK;
        case KC_STOP:
            return "Stop"; //
        case KC_AGAIN:
            return EH_SYMBOL_ROTATE_RIGHT;
        case KC_UNDO:
            return EH_SYMBOL_ROTATE_LEFT;
        case KC_CUT:
            return LV_SYMBOL_CUT;
        case KC_COPY:
            return LV_SYMBOL_COPY;
        case KC_PASTE:
            return LV_SYMBOL_PASTE;
        case KC_FIND:
            return EH_SYMBOL_SEARCH;
        case KC_KB_MUTE:
            return EH_SYMBOL_VOLUME_MUTE;
        case KC_KB_VOLUME_UP:
            return LV_SYMBOL_VOLUME_MAX;
        case KC_KB_VOLUME_DOWN:
            return LV_SYMBOL_VOLUME_MID;
        case KC_KP_COMMA:
            return ",";
        case KC_ALTERNATE_ERASE:
            return "Alternate_Erase";
        case KC_SYSTEM_REQUEST:
            return "System_Request";
        case KC_CANCEL:
            return "Cancel";
        case KC_CLEAR:
            return "Clear";
        case KC_PRIOR:
            return "Prior";
        case KC_RETURN:
            return "Return";
        case KC_SEPARATOR:
            return "Separator";
        case KC_OUT:
            return "Out";
        case KC_OPER:
            return "Oper";
        case KC_CLEAR_AGAIN:
            return "Clear_Again";
        case KC_CRSEL:
            return "Crsel";
        case KC_EXSEL:
            return "Exsel";
        case KC_SYSTEM_POWER:
            return LV_SYMBOL_POWER;
        case KC_SYSTEM_SLEEP:
            return "System_Sleep";
        case KC_SYSTEM_WAKE:
            return "System_Wake";
        case KC_AUDIO_MUTE:
            return EH_SYMBOL_VOLUME_MUTE;
        case KC_AUDIO_VOL_UP:
            return LV_SYMBOL_VOLUME_MAX;
        case KC_AUDIO_VOL_DOWN:
            return LV_SYMBOL_VOLUME_MID;
        case KC_MEDIA_NEXT_TRACK:
            return LV_SYMBOL_NEXT;
        case KC_MEDIA_PREV_TRACK:
            return LV_SYMBOL_PREV;
        case KC_MEDIA_STOP:
            return LV_SYMBOL_STOP;
        case KC_MEDIA_PLAY_PAUSE:
            return LV_SYMBOL_PAUSE;
        case KC_MEDIA_SELECT:
            return LV_SYMBOL_AUDIO;
        case KC_MEDIA_EJECT:
            return LV_SYMBOL_EJECT;
        case KC_MAIL:
            return EH_SYMBOL_MAIL;
        case KC_CALCULATOR:
            return EH_SYMBOL_CALC;
        case KC_MY_COMPUTER:
            return EH_SYMBOL_COMPUTER;
        case KC_WWW_SEARCH:
            return EH_SYMBOL_GLOBE " " EH_SYMBOL_SEARCH;
        case KC_WWW_HOME:
            return EH_SYMBOL_GLOBE " " LV_SYMBOL_HOME;
        case KC_WWW_BACK:
            return EH_SYMBOL_GLOBE " " EH_SYMBOL_ROTATE_LEFT;
        case KC_WWW_FORWARD:
            return EH_SYMBOL_GLOBE " " EH_SYMBOL_ROTATE_RIGHT;
        case KC_WWW_STOP:
            return EH_SYMBOL_GLOBE " " LV_SYMBOL_CLOSE;
        case KC_WWW_REFRESH:
            return EH_SYMBOL_GLOBE " " LV_SYMBOL_REFRESH;
        case KC_WWW_FAVORITES:
            return EH_SYMBOL_GLOBE " " EH_SYMBOL_HEART;
        case KC_MEDIA_FAST_FORWARD:
            return LV_SYMBOL_NEXT;
        case KC_MEDIA_REWIND:
            return LV_SYMBOL_PREV;
        case KC_BRIGHTNESS_UP:
            return EH_SYMBOL_SUN;
        case KC_BRIGHTNESS_DOWN:
            return EH_SYMBOL_MOON;
        case KC_CONTROL_PANEL:
            return LV_SYMBOL_SETTINGS;
        case KC_ASSISTANT:
            return "Astn";
        case KC_MISSION_CONTROL:
            return "MsCtr";
        case KC_LAUNCHPAD:
            return "Launchpad";
        case KC_MS_UP:
            return EH_SYMBOL_MOUSE " " EH_SYMBOL_UP;
        case KC_MS_DOWN:
            return EH_SYMBOL_MOUSE " " EH_SYMBOL_DOWN;
        case KC_MS_LEFT:
            return EH_SYMBOL_MOUSE " " EH_SYMBOL_LEFT;
        case KC_MS_RIGHT:
            return EH_SYMBOL_MOUSE " " EH_SYMBOL_RIGHT;
        case KC_MS_BTN1:
            return EH_SYMBOL_MOUSE " 1";
        case KC_MS_BTN2:
            return EH_SYMBOL_MOUSE " 2";
        case KC_MS_BTN3:
            return EH_SYMBOL_MOUSE " 3";
        case KC_MS_BTN4:
            return EH_SYMBOL_MOUSE " 4";
        case KC_MS_BTN5:
            return EH_SYMBOL_MOUSE " 5";
        case KC_MS_BTN6:
            return EH_SYMBOL_MOUSE " 6";
        case KC_MS_BTN7:
            return EH_SYMBOL_MOUSE " 7";
        case KC_MS_BTN8:
            return EH_SYMBOL_MOUSE " 8";
        case KC_MS_WH_UP:
            return EH_SYMBOL_MOUSE " " LV_SYMBOL_UP;
        case KC_MS_WH_DOWN:
            return EH_SYMBOL_MOUSE " " LV_SYMBOL_DOWN;
        case KC_MS_WH_LEFT:
            return EH_SYMBOL_MOUSE " " LV_SYMBOL_LEFT;
        case KC_MS_WH_RIGHT:
            return EH_SYMBOL_MOUSE " " LV_SYMBOL_RIGHT;
        case KC_MS_ACCEL0:
            return EH_SYMBOL_MOUSE " Acc0";
        case KC_MS_ACCEL1:
            return EH_SYMBOL_MOUSE " Acc1";
        case KC_MS_ACCEL2:
            return EH_SYMBOL_MOUSE " Acc2";
        case KC_LEFT_CTRL:
            return "Ctrl";
        case KC_LEFT_SHIFT:
            return "Shift";
        case KC_LEFT_ALT:
            return "Alt";
        case KC_LEFT_GUI:
            return "Gui";
        case KC_RIGHT_CTRL:
            return "Ctrl";
        case KC_RIGHT_SHIFT:
            return "Shift";
        case KC_RIGHT_ALT:
            return "Alt";
        case KC_RIGHT_GUI:
            return "Gui";

        default:
            return "Unkn";
    }
}

const char *special_keycode_to_str(uint16_t keycode) {
    static char buf[32];
    switch (keycode) {
        case KC_TILD:
            return "~";
        case KC_EXLM:
            return "!";
        case KC_AT:
            return "@";
        case KC_HASH:
            return "#";
        case KC_DLR:
            return "$";
        case KC_PERC:
            return "%";
        case KC_CIRC:
            return "^";
        case KC_AMPR:
            return "&";
        case KC_ASTR:
            return "*";
        case KC_LPRN:
            return "(";
        case KC_RPRN:
            return ")";
        case KC_UNDS:
            return "_";
        case KC_PLUS:
            return "+";
        case KC_LCBR:
            return "{";
        case KC_RCBR:
            return "}";
        case KC_PIPE:
            return "|";
        case KC_COLN:
            return ":";
        case KC_DQUO:
            return "\"";
        case KC_LABK:
            return "<";
        case KC_RABK:
            return ">";
        case KC_QUES:
            return "?";
        case QK_BOOT:
            return LV_SYMBOL_KEYBOARD "Rst";
        case QK_TO ... QK_TO_MAX:
            sprintf(buf, "TO\n" EH_SYMBOL_LAYER "%d", keycode - QK_TO);
            return buf;
        case QK_MOMENTARY ... QK_MOMENTARY_MAX:
            sprintf(buf, "MO\n" EH_SYMBOL_LAYER "%d", keycode - QK_MOMENTARY);
            return buf;
        case QK_DEF_LAYER ... QK_DEF_LAYER_MAX:
            sprintf(buf, "DF\n" EH_SYMBOL_LAYER "%d", keycode - QK_DEF_LAYER);
            return buf;
        case QK_TOGGLE_LAYER ... QK_TOGGLE_LAYER_MAX:
            sprintf(buf, "TG\n" EH_SYMBOL_LAYER "%d", keycode - QK_TOGGLE_LAYER);
            return buf;
        case QK_LAYER_TAP_TOGGLE ... QK_LAYER_TAP_TOGGLE_MAX:
            sprintf(buf, "TT\n" EH_SYMBOL_LAYER "%d", keycode - QK_LAYER_TAP_TOGGLE);
            return buf;
        case QK_ONE_SHOT_LAYER ... QK_ONE_SHOT_LAYER_MAX:
            sprintf(buf, "OSL\n" EH_SYMBOL_LAYER "%d", keycode - QK_ONE_SHOT_LAYER);
            return buf;
        case QK_LAYER_TAP ... QK_LAYER_TAP_MAX: {
            int layer = QK_LAYER_TAP_GET_LAYER(keycode);
            int kc    = QK_LAYER_TAP_GET_TAP_KEYCODE(keycode);
            sprintf(buf, EH_SYMBOL_LAYER "%d\n%s", layer, basic_keycode_to_str(kc));
            return buf;
        }
        case WPREV:
            return "Win\n" EH_SYMBOL_ANGLES_LEFT;
        case WNEXT:
            return "Win\n" EH_SYMBOL_ANGLES_RIGHT;
        case LAYER_NEXT:
            return EH_SYMBOL_LAYER EH_SYMBOL_ANGLES_RIGHT;
        case LAYER_PREV:
            return EH_SYMBOL_LAYER EH_SYMBOL_ANGLES_LEFT;
        case QK_MACRO ... QK_MACRO_MAX:
            sprintf(buf, "M%d", keycode - QK_MACRO);
            return buf;
        case QK_TAP_DANCE ... QK_TAP_DANCE_MAX:
            sprintf(buf, "TD%d", keycode - QK_TAP_DANCE);
            return buf;
        default:
            return "";
    }
}

const char *keycode_to_str(uint16_t keycode) {
    const char *special_str = special_keycode_to_str(keycode);
    if (strlen(special_str) > 0) return special_str;

    uint8_t     mods              = QK_MODS_GET_MODS(keycode);
    uint8_t     basic_keycode     = QK_MODS_GET_BASIC_KEYCODE(keycode);
    const char *basic_keycode_str = basic_keycode_to_str(basic_keycode);
    bool        ctrl              = mods & MOD_MASK_CTRL;
    bool        shift             = mods & MOD_MASK_SHIFT;
    bool        alt               = mods & MOD_MASK_ALT;
    bool        gui               = mods & MOD_MASK_GUI;
    char       *mod_str;
    if (ctrl && shift && alt && gui)
        mod_str = "CSAG\n";
    else if (shift && alt && gui)
        mod_str = "SAG\n";
    else if (ctrl && alt && gui)
        mod_str = "CAG\n";
    else if (ctrl && shift && gui)
        mod_str = "CSG\n";
    else if (ctrl && shift && alt)
        mod_str = "CSA\n";
    else if (alt && gui)
        mod_str = "Alt Gui\n";
    else if (shift && gui)
        mod_str = "Sft Gui\n";
    else if (shift && alt)
        mod_str = "Sft Alt\n";
    else if (ctrl && gui)
        mod_str = "Ctl Gui\n";
    else if (ctrl && shift)
        mod_str = "Ctl Sft\n";
    else if (ctrl && alt)
        mod_str = "Ctl Alt\n";
    else if (ctrl)
        mod_str = "Ctl\n";
    else if (shift)
        mod_str = "Sft\n";
    else if (alt)
        mod_str = "Alt\n";
    else if (gui)
        mod_str = "Gui\n";
    else
        mod_str = "";
    static char buf[32];
    sprintf(buf, "%s%s", mod_str, basic_keycode_str);
    return buf;
}

uint16_t get_keycode(int layer, int row, int col) {
    uint16_t keycode = dynamic_keymap_get_keycode(layer, row, col);
    if (keycode == KC_TRANSPARENT) keycode = dynamic_keymap_get_keycode(0, row, col);
    return keycode;
}

uint16_t get_encoder_keycode(int layer, int encoder, bool clockwise) {
    uint16_t keycode = dynamic_keymap_get_encoder(layer, encoder, clockwise);
    if (keycode == KC_TRANSPARENT) keycode = dynamic_keymap_get_encoder(0, encoder, clockwise);
    return keycode;
}

void display_process_layer_state(uint8_t layer) {
    if (!display_enabled) return;

    change_screen_state = SCREEN_LAYOUT;

    const char *layer_name = get_layer_name(layer);
    lv_label_set_text(label_layer_small, layer_name);
    char buf[32];
    sprintf(buf, EH_SYMBOL_LAYER " %s", layer_name);
    lv_label_set_text(label_layer, buf);

    for (int i = 0; i < 15; i++) {
        uint16_t keycode = KC_TRANSPARENT;
        if (i < 12)
            keycode = get_keycode(layer, i / 3 + 1, i % 3);
        else if (i == 13)
            keycode = get_keycode(layer, 0, 2);
        else if (i == 12)
            keycode = get_encoder_keycode(layer, 0, false);
        else if (i == 14)
            keycode = get_encoder_keycode(layer, 0, true);
        lv_label_set_text(key_labels[i], keycode_to_str(keycode));
    }
}

void update_screen_state(void) {
    screen_timer = timer_read32();
    screen_state = change_screen_state;
    switch (screen_state) {
        case SCREEN_SPLASH:
            lv_scr_load(screen_splash);
            display_turn_on();
            break;
        case SCREEN_HID:
            lv_scr_load(screen_hid);
            display_turn_on();
            break;
        case SCREEN_LAYOUT:
            lv_scr_load(screen_layout);
            display_turn_on();
            break;
        case SCREEN_VOLUME:
            lv_scr_load(screen_volume);
            display_turn_on();
            break;
        case SCREEN_OFF:
            display_turn_off();
            break;
    }
}

void display_housekeeping_task(void) {
    if (!display_enabled) return;

    struct hid_data_t *hid_data   = get_hid_data();
    bool               hid_active = display_process_hid_data(hid_data);
    static uint8_t     prev_lang  = 0;
    uint8_t            cur_lang   = get_cur_lang();
    set_layout_label(cur_lang);
    if (prev_lang != cur_lang) {
        change_screen_state = SCREEN_HID;
        prev_lang           = cur_lang;
    }

    if (screen_state == change_screen_state) {
        uint32_t screen_elapsed   = timer_elapsed32(screen_timer);
        uint32_t activity_elapsed = last_input_activity_elapsed();

        switch (screen_state) {
            case SCREEN_SPLASH:
                if (screen_elapsed > 2 * 1000) {
                    change_screen_state = SCREEN_LAYOUT;
                }
                break;

            case SCREEN_LAYOUT:
                if (hid_active && activity_elapsed > 10 * 1000) {
                    change_screen_state = SCREEN_HID;
                } else if (activity_elapsed > EH_TIMEOUT) {
                    change_screen_state = SCREEN_OFF;
                }
                break;

            case SCREEN_HID:
                if (!hid_active) {
                    change_screen_state = SCREEN_LAYOUT;
                } else if (activity_elapsed > EH_TIMEOUT && screen_elapsed > 10 * 1000) {
                    change_screen_state = SCREEN_OFF;
                }
                break;

            case SCREEN_VOLUME:
                if (screen_elapsed > 2 * 1000) {
                    change_screen_state = SCREEN_HID;
                }
                break;

            case SCREEN_OFF:
                if (activity_elapsed < EH_TIMEOUT) {
                    change_screen_state = SCREEN_LAYOUT;
                }
                break;
        }
    }
    if (change_screen_state != screen_state) {
        update_screen_state();
        return;
    }
}

void display_turn_on(void) {
    if (!is_display_on) {
        gpio_write_pin_high(GP17);
        qp_power(display, true);
        is_display_on = true;
    }
}

void display_turn_off(void) {
    if (is_display_on) {
        is_display_on = false;
        qp_power(display, false);
        gpio_write_pin_low(GP17);
    }
}

void suspend_power_down_kb(void) {
    display_turn_off();
    suspend_power_down_user();
}

void suspend_wakeup_init_kb(void) {
    display_turn_on();
    suspend_wakeup_init_user();
}
