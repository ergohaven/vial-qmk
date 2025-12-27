#include "ergohaven_keycode_str.h"

#include "keycodes.h"
#include "ergohaven.h"
#include "ergohaven_symbols.h"
#include "ergohaven_ruen.h"
#include "ergohaven_pointing.h"

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
        case QK_LAYER_LOCK:
            return EH_SYMBOL_LAYER "Lock";
        case QK_TRI_LAYER_LOWER:
            return EH_SYMBOL_LAYER "Tri\nLower";
        case QK_TRI_LAYER_UPPER:
            return EH_SYMBOL_LAYER "Tri\nUpper";
        case QK_TO ... QK_TO_MAX:
            sprintf(buf, "TO\n" EH_SYMBOL_LAYER "%d", keycode - QK_TO);
            return buf;
        case QK_MOMENTARY ... QK_MOMENTARY_MAX:
            sprintf(buf, "MO\n" EH_SYMBOL_LAYER "%d", keycode - QK_MOMENTARY);
            return buf;
        case QK_PERSISTENT_DEF_LAYER ... QK_PERSISTENT_DEF_LAYER_MAX:
            sprintf(buf, "PDF\n" EH_SYMBOL_LAYER "%d", keycode - QK_PERSISTENT_DEF_LAYER);
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

        case QK_MACRO ... QK_MACRO_MAX:
            sprintf(buf, "M%d", keycode - QK_MACRO);
            return buf;
        case QK_TAP_DANCE ... QK_TAP_DANCE_MAX:
            sprintf(buf, "TD%d", keycode - QK_TAP_DANCE);
            return buf;

        case C(KC_Z):
            return EH_SYMBOL_ROTATE_LEFT "\nCtl Z";
        case C(KC_X):
            return LV_SYMBOL_CUT "\nCtl X";
        case C(KC_C):
            return LV_SYMBOL_COPY "\nCtl C";
        case C(KC_V):
            return LV_SYMBOL_PASTE "\nCtl V";

        case EH_PRINFO:
            return "Print\nInfo";
        case WRD_PRV:
            return "Word\n" EH_SYMBOL_ANGLES_LEFT;
        case WRD_NXT:
            return "Word\n" EH_SYMBOL_ANGLES_RIGHT;
        case WPREV:
            return "Win\n" EH_SYMBOL_ANGLES_LEFT;
        case WNEXT:
            return "Win\n" EH_SYMBOL_ANGLES_RIGHT;
        case LAYER_NEXT:
            return EH_SYMBOL_LAYER EH_SYMBOL_ANGLES_RIGHT;
        case LAYER_PREV:
            return EH_SYMBOL_LAYER EH_SYMBOL_ANGLES_LEFT;

        case EH_LED_BL:
            return "Tg Led\nBlinks";
        case EH_SNP:
            return "Sniper\nMode";
        case EH_SCR:
            return "Scroll\nMode";
        case EH_TXT:
            return "Text\nMode";
        case EH_USR1:
            return "Mode\nUser 1";
        case EH_USR2:
            return "Mode\nUser 2";
        case EH_USR3:
            return "Mode\nUser 3";

        case LG_TOGGLE:
            return "RuEn\nToggle";
        case LG_SYNC:
            return "RuEn\nSync";
        case LG_SET_EN:
            return "RuEn\nEn";
        case LG_SET_RU:
            return "RuEn\nRu";
        case LG_SET_M0:
            return "RuEn\nM0";
        case LG_SET_M1M2:
            return "RuEn\nM1M2";
        case LG_SET_DFLT:
            return "RuEn\nDflt";
        case LG_DOT:
            return "RuEn\n.";
        case LG_COMMA:
            return "RuEn\n,";
        case LG_SCLN:
            return "RuEn\n;";
        case LG_COLON:
            return "RuEn\n:";
        case LG_DQUO:
            return "RuEn\n\"";
        case LG_QUES:
            return "RuEn\n?";
        case LG_SLASH:
            return "RuEn\n/";
        case LG_LBR:
            return "RuEn\n[";
        case LG_RBR:
            return "RuEn\n]";
        case LG_LCBR:
            return "RuEn\n{";
        case LG_RCBR:
            return "RuEn\n}";
        case LG_LT:
            return "RuEn\n<";
        case LG_GT:
            return "RuEn\n>";
        case LG_GRAVE:
            return "RuEn\n`";
        case LG_TILD:
            return "RuEn\n~";
        case LG_AT:
            return "RuEn\n@";
        case LG_HASH:
            return "RuEn\n#";
        case LG_DLR:
            return "RuEn\n$";
        case LG_CIRC:
            return "RuEn\n^";
        case LG_AMPR:
            return "RuEn\n&";
        case LG_PIPE:
            return "RuEn\n|";
        case LG_QUOTE:
            return "RuEn\n'";
        case LG_NUM:
            return "RuEn\n№";
        case LG_WORD:
            return "RuEn\nword";
        case LG_MOD:
            return "RuEn\nMod";
        case LG_STORE:
            return "RuEn\nStore";
        case LG_REVERT:
            return "RuEn\nRevert";
        case LG_PERC:
            return "RuEn\n%";
        case LG_TG_MAC:
            return "RuEn\nMac Tg";
        case LG_RU_BE:
            return "RuEn\nБ";
        case LG_RU_YU:
            return "RuEn\nЮ";
        case LG_RU_ZHE:
            return "RuEn\nЖ";
        case LG_RU_E:
            return "RuEn\nЭ";
        case LG_RU_KHA:
            return "RuEn\nХ";
        case LG_RU_HRD_SGN:
            return "RuEn\nЪ";
        case LG_RU_YO:
            return "RuEn\nЁ";

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
