#include "src/display/eh_keycode_str.h"

#include "keycodes.h"
#include "ergohaven.h"
#include "eh_symbols.h"
#include "src/eh_ruen.h"
#include "src/eh_pointing.h"

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
            return EH_SYMBOL_RET;
        case KC_ESCAPE:
            return "Esc";
        case KC_BACKSPACE:
            return EH_SYMBOL_BCKSPC;
        case KC_TAB:
            return EH_SYMBOL_TAB;
        case KC_SPACE:
            return EH_SYMBOL_SPC;
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
            return "CpL";
        case KC_PRINT_SCREEN:
            return EH_SYMBOL_IMAGE;
        case KC_SCROLL_LOCK:
            return "ScL";
        case KC_PAUSE:
            return "Pau";
        case KC_INSERT:
            return "Ins";
        case KC_HOME:
            return "Home";
        case KC_PAGE_UP:
            return "Pg" EH_SYMBOL_UP;
        case KC_DELETE:
            return "Del";
        case KC_END:
            return "End";
        case KC_PAGE_DOWN:
            return "Pg" EH_SYMBOL_DOWN;
        case KC_RIGHT:
            return EH_SYMBOL_RIGHT;
        case KC_LEFT:
            return EH_SYMBOL_LEFT;
        case KC_DOWN:
            return EH_SYMBOL_DOWN;
        case KC_UP:
            return EH_SYMBOL_UP;
        case KC_NUM_LOCK:
            return "NmL";
        case KC_KP_SLASH:
            return EH_SYMBOL_DIVIDE;
        case KC_KP_ASTERISK:
            return EH_SYMBOL_MULTIPLY;
        case KC_KP_MINUS:
            return EH_SYMBOL_MINUS;
        case KC_KP_PLUS:
            return EH_SYMBOL_PLUS;
        case KC_KP_ENTER:
            return EH_SYMBOL_RET;
        case KC_KP_1 ... KC_KP_0:
            sprintf(buf, "%d", (keycode - KC_KP_1 + 1) % 10);
            return buf;
        case KC_KP_DOT:
            return ".";
        case KC_NONUS_BACKSLASH:
            return "\\";
        case KC_APPLICATION:
            return EH_SYMBOL_SETTINGS;
        case KC_KB_POWER:
            return EH_SYMBOL_POWER;
        case KC_KP_EQUAL:
            return "="; //
        case KC_F1 ... KC_F12:
            sprintf(buf, "F%d", keycode - KC_F1 + 1);
            return buf;
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
            return EH_SYMBOL_OK;
        case KC_STOP:
            return "Stop"; //
        case KC_AGAIN:
            return EH_SYMBOL_ROTATE_RIGHT;
        case KC_UNDO:
            return EH_SYMBOL_ROTATE_LEFT;
        case KC_CUT:
            return EH_SYMBOL_CUT;
        case KC_COPY:
            return EH_SYMBOL_COPY;
        case KC_PASTE:
            return EH_SYMBOL_PASTE;
        case KC_FIND:
            return EH_SYMBOL_SEARCH;
        case KC_KB_MUTE:
            return EH_SYMBOL_VOLUME_MUTE;
        case KC_KB_VOLUME_UP:
            return EH_SYMBOL_VOLUME_UP;
        case KC_KB_VOLUME_DOWN:
            return EH_SYMBOL_VOLUME_DOWN;
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
            return EH_SYMBOL_POWER;
        case KC_SYSTEM_SLEEP:
            return EH_SYMBOL_SLEEP;
        case KC_SYSTEM_WAKE:
            return EH_SYMBOL_WAKE;
        case KC_AUDIO_MUTE:
            return EH_SYMBOL_VOLUME_MUTE;
        case KC_AUDIO_VOL_UP:
            return EH_SYMBOL_VOLUME_UP;
        case KC_AUDIO_VOL_DOWN:
            return EH_SYMBOL_VOLUME_DOWN;
        case KC_MEDIA_NEXT_TRACK:
            return EH_SYMBOL_NEXT;
        case KC_MEDIA_PREV_TRACK:
            return EH_SYMBOL_PREV;
        case KC_MEDIA_STOP:
            return EH_SYMBOL_STOP;
        case KC_MEDIA_PLAY_PAUSE:
            return EH_SYMBOL_PAUSE;
        case KC_MEDIA_SELECT:
            return EH_SYMBOL_AUDIO;
        case KC_MEDIA_EJECT:
            return EH_SYMBOL_EJECT;
        case KC_MAIL:
            return EH_SYMBOL_MAIL;
        case KC_CALCULATOR:
            return EH_SYMBOL_CALC;
        case KC_MY_COMPUTER:
            return EH_SYMBOL_COMPUTER;
        case KC_WWW_SEARCH:
            return EH_SYMBOL_GLOBE EH_SYMBOL_SEARCH;
        case KC_WWW_HOME:
            return EH_SYMBOL_GLOBE EH_SYMBOL_HOME;
        case KC_WWW_BACK:
            return EH_SYMBOL_GLOBE EH_SYMBOL_ROTATE_LEFT;
        case KC_WWW_FORWARD:
            return EH_SYMBOL_GLOBE EH_SYMBOL_ROTATE_RIGHT;
        case KC_WWW_STOP:
            return EH_SYMBOL_GLOBE EH_SYMBOL_CLOSE;
        case KC_WWW_REFRESH:
            return EH_SYMBOL_GLOBE EH_SYMBOL_REFRESH;
        case KC_WWW_FAVORITES:
            return EH_SYMBOL_GLOBE EH_SYMBOL_HEART;
        case KC_MEDIA_FAST_FORWARD:
            return EH_SYMBOL_NEXT;
        case KC_MEDIA_REWIND:
            return EH_SYMBOL_PREV;
        case KC_BRIGHTNESS_UP:
            return EH_SYMBOL_SUN;
        case KC_BRIGHTNESS_DOWN:
            return EH_SYMBOL_MOON;
        case KC_CONTROL_PANEL:
            return EH_SYMBOL_SETTINGS;
        case KC_ASSISTANT:
            return "Astn";
        case KC_MISSION_CONTROL:
            return "MsCtr";
        case KC_LAUNCHPAD:
            return "Launchpad";
        case KC_MS_UP:
            return EH_SYMBOL_MOUSE EH_SYMBOL_UP;
        case KC_MS_DOWN:
            return EH_SYMBOL_MOUSE EH_SYMBOL_DOWN;
        case KC_MS_LEFT:
            return EH_SYMBOL_MOUSE EH_SYMBOL_LEFT;
        case KC_MS_RIGHT:
            return EH_SYMBOL_MOUSE EH_SYMBOL_RIGHT;
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
            return EH_SYMBOL_MOUSE " " EH_SYMBOL_ANGLES_UP;
        case KC_MS_WH_DOWN:
            return EH_SYMBOL_MOUSE " " EH_SYMBOL_ANGLES_DOWN;
        case KC_MS_WH_LEFT:
            return EH_SYMBOL_MOUSE " " EH_SYMBOL_ANGLES_LEFT;
        case KC_MS_WH_RIGHT:
            return EH_SYMBOL_MOUSE " " EH_SYMBOL_ANGLES_RIGHT;
        case KC_MS_ACCEL0:
            return EH_SYMBOL_MOUSE "A0";
        case KC_MS_ACCEL1:
            return EH_SYMBOL_MOUSE "A1";
        case KC_MS_ACCEL2:
            return EH_SYMBOL_MOUSE "A2";
        case KC_LEFT_CTRL:
        case KC_RIGHT_CTRL:
            return EH_SYMBOL_CTL;
        case KC_LEFT_SHIFT:
        case KC_RIGHT_SHIFT:
            return EH_SYMBOL_SFT;
        case KC_LEFT_ALT:
        case KC_RIGHT_ALT:
            return EH_SYMBOL_ALT;
        case KC_LEFT_GUI:
        case KC_RIGHT_GUI:
            return EH_SYMBOL_GUI;
        default:
            return "Unkn";
    }
}

bool special_keycode_str(char *buf, uint16_t keycode) {
    switch (keycode) {
        case KC_TILD:
            sprintf(buf, "~");
            return true;
        case KC_EXLM:
            sprintf(buf, "!");
            return true;
        case KC_AT:
            sprintf(buf, "@");
            return true;
        case KC_HASH:
            sprintf(buf, "#");
            return true;
        case KC_DLR:
            sprintf(buf, "$");
            return true;
        case KC_PERC:
            sprintf(buf, "%%");
            return true;
        case KC_CIRC:
            sprintf(buf, "^");
            return true;
        case KC_AMPR:
            sprintf(buf, "&");
            return true;
        case KC_ASTR:
            sprintf(buf, "*");
            return true;
        case KC_LPRN:
            sprintf(buf, "(");
            return true;
        case KC_RPRN:
            sprintf(buf, ")");
            return true;
        case KC_UNDS:
            sprintf(buf, "_");
            return true;
        case KC_PLUS:
            sprintf(buf, "+");
            return true;
        case KC_LCBR:
            sprintf(buf, "{");
            return true;
        case KC_RCBR:
            sprintf(buf, "}");
            return true;
        case KC_PIPE:
            sprintf(buf, "|");
            return true;
        case KC_COLN:
            sprintf(buf, ":");
            return true;
        case KC_DQUO:
            sprintf(buf, "\"");
            return true;
        case KC_LABK:
            sprintf(buf, "<");
            return true;
        case KC_RABK:
            sprintf(buf, ">");
            return true;
        case KC_QUES:
            sprintf(buf, "?");
            return true;
        case QK_BOOT:
            sprintf(buf, EH_SYMBOL_KEYBOARD "\nFlsh");
            return true;
        case QK_RBT:
            sprintf(buf, EH_SYMBOL_KEYBOARD "\nRbt");
            return true;
        case EE_CLR:
            sprintf(buf, EH_SYMBOL_KEYBOARD "\nClr");
            return true;
        case QK_LAYER_LOCK:
            sprintf(buf, EH_SYMBOL_LAYER "\nLock");
            return true;
        case QK_TRI_LAYER_LOWER:
            sprintf(buf, EH_SYMBOL_LAYER "Tr\nLow");
            return true;
        case QK_TRI_LAYER_UPPER:
            sprintf(buf, EH_SYMBOL_LAYER "Tr\nUp");
            return true;
        case QK_TO ... QK_TO_MAX:
            sprintf(buf, "TO\n" EH_SYMBOL_LAYER "%d", keycode - QK_TO);
            return true;
        case QK_MOMENTARY ... QK_MOMENTARY_MAX:
            sprintf(buf, EH_SYMBOL_LAYER "%d", keycode - QK_MOMENTARY);
            return true;
        case QK_PERSISTENT_DEF_LAYER ... QK_PERSISTENT_DEF_LAYER_MAX:
            sprintf(buf, "PDF\n" EH_SYMBOL_LAYER "%d", keycode - QK_PERSISTENT_DEF_LAYER);
            return true;
        case QK_DEF_LAYER ... QK_DEF_LAYER_MAX:
            sprintf(buf, "DF\n" EH_SYMBOL_LAYER "%d", keycode - QK_DEF_LAYER);
            return true;
        case QK_TOGGLE_LAYER ... QK_TOGGLE_LAYER_MAX:
            sprintf(buf, "TG\n" EH_SYMBOL_LAYER "%d", keycode - QK_TOGGLE_LAYER);
            return true;
        case QK_LAYER_TAP_TOGGLE ... QK_LAYER_TAP_TOGGLE_MAX:
            sprintf(buf, "TT\n" EH_SYMBOL_LAYER "%d", keycode - QK_LAYER_TAP_TOGGLE);
            return true;
        case QK_ONE_SHOT_LAYER ... QK_ONE_SHOT_LAYER_MAX:
            sprintf(buf, "OSL\n" EH_SYMBOL_LAYER "%d", keycode - QK_ONE_SHOT_LAYER);
            return true;
        case QK_LAYER_TAP ... QK_LAYER_TAP_MAX: {
            int layer = QK_LAYER_TAP_GET_LAYER(keycode);
            int kc    = QK_LAYER_TAP_GET_TAP_KEYCODE(keycode);
            sprintf(buf, EH_SYMBOL_LAYER "%d\n%s", layer, basic_keycode_to_str(kc));
            return true;
        }

        case BL_ON:
            sprintf(buf, EH_SYMBOL_BL EH_SYMBOL_ON);
            return true;
        case BL_OFF:
            sprintf(buf, EH_SYMBOL_BL EH_SYMBOL_OFF);
            return true;
        case BL_TOGG:
            sprintf(buf, EH_SYMBOL_BL);
            return true;
        case BL_STEP:
            sprintf(buf, EH_SYMBOL_BL EH_SYMBOL_ANGLES_RIGHT);
            return true;
        case BL_DOWN:
            sprintf(buf, EH_SYMBOL_BL EH_SYMBOL_DOWN);
            return true;
        case BL_UP:
            sprintf(buf, EH_SYMBOL_BL EH_SYMBOL_UP);
            return true;
        case BL_BRTG:
            sprintf(buf, EH_SYMBOL_BL "\nBrTg");
            return true;

        case RM_ON:
            sprintf(buf, EH_SYMBOL_RGB EH_SYMBOL_ON);
            return true;
        case RM_OFF:
            sprintf(buf, EH_SYMBOL_RGB EH_SYMBOL_OFF);
            return true;
        case RM_TOGG:
        case UG_TOGG:
            sprintf(buf, EH_SYMBOL_RGB);
            return true;
        case RM_NEXT:
        case UG_NEXT:
            sprintf(buf, EH_SYMBOL_RGB EH_SYMBOL_ANGLES_RIGHT);
            return true;
        case RM_PREV:
        case UG_PREV:
            sprintf(buf, EH_SYMBOL_RGB EH_SYMBOL_ANGLES_LEFT);
            return true;
        case RM_VALD:
        case UG_VALD:
            sprintf(buf, EH_SYMBOL_RGB "\nVal-");
            return true;
        case RM_VALU:
        case UG_VALU:
            sprintf(buf, EH_SYMBOL_RGB "\nVal+");
            return true;
        case RM_SPDU:
        case UG_SPDU:
            sprintf(buf, EH_SYMBOL_RGB "\nSpd-");
            return true;
        case RM_SPDD:
        case UG_SPDD:
            sprintf(buf, EH_SYMBOL_RGB "\nSpd+");
            return true;
        case RM_SATU:
        case UG_SATU:
            sprintf(buf, EH_SYMBOL_RGB "\nSat+");
            return true;
        case RM_SATD:
        case UG_SATD:
            sprintf(buf, EH_SYMBOL_RGB "\nSat-");
            return true;
        case RM_HUEU:
        case UG_HUEU:
            sprintf(buf, EH_SYMBOL_RGB "\nHue+");
            return true;
        case RM_HUED:
        case UG_HUED:
            sprintf(buf, EH_SYMBOL_RGB "\nHue-");
            return true;
        case RGB_M_P ... RGB_M_TW:
            sprintf(buf, "Undf");
            return true;

        case QK_MACRO ... QK_MACRO_MAX:
            sprintf(buf, "M%d", keycode - QK_MACRO);
            return true;
        case QK_TAP_DANCE ... QK_TAP_DANCE_MAX:
            sprintf(buf, "TD%d", keycode - QK_TAP_DANCE);
            return true;

        case C(KC_Z):
            sprintf(buf, EH_SYMBOL_ROTATE_LEFT "\n" EH_SYMBOL_CTL "Z");
            return true;
        case C(KC_X):
            sprintf(buf, EH_SYMBOL_CUT "\n" EH_SYMBOL_CTL "X");
            return true;
        case C(KC_C):
            sprintf(buf, EH_SYMBOL_COPY "\n" EH_SYMBOL_CTL "C");
            return true;
        case C(KC_V):
            sprintf(buf, EH_SYMBOL_PASTE "\n" EH_SYMBOL_CTL "V");
            return true;

        case EH_PRINFO:
            sprintf(buf, "Prnt\nInfo");
            return true;
        case WRD_PRV:
            sprintf(buf, "Word\n" EH_SYMBOL_ANGLES_LEFT);
            return true;
        case WRD_NXT:
            sprintf(buf, "Word\n" EH_SYMBOL_ANGLES_RIGHT);
            return true;
        case WPREV:
            sprintf(buf, "Win\n" EH_SYMBOL_ANGLES_LEFT);
            return true;
        case WNEXT:
            sprintf(buf, "Win\n" EH_SYMBOL_ANGLES_RIGHT);
            return true;
        case LAYER_NEXT:
            sprintf(buf, EH_SYMBOL_LAYER EH_SYMBOL_ANGLES_RIGHT);
            return true;
        case LAYER_PREV:
            sprintf(buf, EH_SYMBOL_LAYER EH_SYMBOL_ANGLES_LEFT);
            return true;

        case EH_LED_BL:
            sprintf(buf, "Led\nBlnk");
            return true;
        case EH_SNP:
            sprintf(buf, "Snpr\nMode");
            return true;
        case EH_SCR:
            sprintf(buf, "Scrl\nMode");
            return true;
        case EH_TXT:
            sprintf(buf, "Text\nMode");
            return true;
        case EH_USR1:
            sprintf(buf, "Mode\nUsr1");
            return true;
        case EH_USR2:
            sprintf(buf, "Mode\nUsr2");
            return true;
        case EH_USR3:
            sprintf(buf, "Mode\nUsr3");
            return true;
        case EH_RSRV1:
        case EH_RSRV2 ... EH_RSRV7:
            sprintf(buf, "Rsrv");
            return true;

        case LG_TOGGLE:
            sprintf(buf, "RuEn\nTg");
            return true;
        case LG_SYNC:
            sprintf(buf, "RuEn\nSync");
            return true;
        case LG_SET_EN:
            sprintf(buf, "RuEn\nEn");
            return true;
        case LG_SET_RU:
            sprintf(buf, "RuEn\nRu");
            return true;
        case LG_SET_M0:
            sprintf(buf, "RuEn\nM0");
            return true;
        case LG_SET_M1M2:
            sprintf(buf, "RuEn\nM1M2");
            return true;
        case LG_SET_DFLT:
            sprintf(buf, "RuEn\nDflt");
            return true;
        case LG_DOT:
            sprintf(buf, "RuEn\n.");
            return true;
        case LG_COMMA:
            sprintf(buf, "RuEn\n,");
            return true;
        case LG_SCLN:
            sprintf(buf, "RuEn\n;");
            return true;
        case LG_COLON:
            sprintf(buf, "RuEn\n:");
            return true;
        case LG_DQUO:
            sprintf(buf, "RuEn\n\"");
            return true;
        case LG_QUES:
            sprintf(buf, "RuEn\n?");
            return true;
        case LG_SLASH:
            sprintf(buf, "RuEn\n/");
            return true;
        case LG_LBR:
            sprintf(buf, "RuEn\n[");
            return true;
        case LG_RBR:
            sprintf(buf, "RuEn\n]");
            return true;
        case LG_LCBR:
            sprintf(buf, "RuEn\n{");
            return true;
        case LG_RCBR:
            sprintf(buf, "RuEn\n}");
            return true;
        case LG_LT:
            sprintf(buf, "RuEn\n<");
            return true;
        case LG_GT:
            sprintf(buf, "RuEn\n>");
            return true;
        case LG_GRAVE:
            sprintf(buf, "RuEn\n`");
            return true;
        case LG_TILD:
            sprintf(buf, "RuEn\n~");
            return true;
        case LG_AT:
            sprintf(buf, "RuEn\n@");
            return true;
        case LG_HASH:
            sprintf(buf, "RuEn\n#");
            return true;
        case LG_DLR:
            sprintf(buf, "RuEn\n$");
            return true;
        case LG_CIRC:
            sprintf(buf, "RuEn\n^");
            return true;
        case LG_AMPR:
            sprintf(buf, "RuEn\n&");
            return true;
        case LG_PIPE:
            sprintf(buf, "RuEn\n|");
            return true;
        case LG_QUOTE:
            sprintf(buf, "RuEn\n'");
            return true;
        case LG_NUM:
            sprintf(buf, "RuEn\n№");
            return true;
        case LG_WORD:
            sprintf(buf, "RuEn\nword");
            return true;
        case LG_MOD:
            sprintf(buf, "RuEn\nMod");
            return true;
        case LG_STORE:
            sprintf(buf, "RuEn\nStor");
            return true;
        case LG_REVERT:
            sprintf(buf, "RuEn\nRvrt");
            return true;
        case LG_PERC:
            sprintf(buf, "RuEn\n%%");
            return true;
        case LG_TG_MAC:
            sprintf(buf, "RuEn\nMac");
            return true;
        case LG_RU_BE:
            sprintf(buf, "RuEn\nБ");
            return true;
        case LG_RU_YU:
            sprintf(buf, "RuEn\nЮ");
            return true;
        case LG_RU_ZHE:
            sprintf(buf, "RuEn\nЖ");
            return true;
        case LG_RU_E:
            sprintf(buf, "RuEn\nЭ");
            return true;
        case LG_RU_KHA:
            sprintf(buf, "RuEn\nХ");
            return true;
        case LG_RU_HRD_SGN:
            sprintf(buf, "RuEn\nЪ");
            return true;
        case LG_RU_YO:
            sprintf(buf, "RuEn\nЁ");
            return true;

        default:
            return false;
    }
}

const char *mods_to_str(uint8_t mods) {
    bool ctrl  = mods & MOD_MASK_CTRL;
    bool shift = mods & MOD_MASK_SHIFT;
    bool alt   = mods & MOD_MASK_ALT;
    bool gui   = mods & MOD_MASK_GUI;

    const char *mod_str;
    if (ctrl && shift && alt && gui)
        mod_str = "All";
    else if (shift && alt && gui)
        mod_str = "󰘶󰘵";
    else if (ctrl && alt && gui)
        mod_str = "󰘴󰘵";
    else if (ctrl && shift && gui)
        mod_str = "󰘴󰘶";
    else if (ctrl && shift && alt)
        mod_str = "󰘴󰘶󰘵";
    else if (alt && gui)
        mod_str = "󰘵";
    else if (shift && gui)
        mod_str = "󰘶";
    else if (shift && alt)
        mod_str = "󰘶󰘵";
    else if (ctrl && gui)
        mod_str = "󰘴";
    else if (ctrl && shift)
        mod_str = "󰘴󰘶";
    else if (ctrl && alt)
        mod_str = "󰘴󰘵";
    else if (ctrl)
        mod_str = "󰘴";
    else if (shift)
        mod_str = "󰘶";
    else if (alt)
        mod_str = "󰘵";
    else if (gui)
        mod_str = "";
    else
        mod_str = "";
    return mod_str;
}

bool mods_keycode_to_str(char *str, uint16_t keycode) {
    uint8_t     mods      = QK_MODS_GET_MODS(keycode);
    const char *basic_str = basic_keycode_to_str(QK_MODS_GET_BASIC_KEYCODE(keycode));
    if (mods == 0) {
        sprintf(str, "%s", basic_str);
    } else {
        const char *mod_str = mods_to_str(mods);
        sprintf(str, "%s\n%s", mod_str, basic_str);
    }
    return true;
}

bool modtap_keycode_to_str(char *str, uint16_t keycode) {
    const char *mod_str   = mods_to_str(QK_MOD_TAP_GET_MODS(keycode));
    const char *basic_str = basic_keycode_to_str(QK_MOD_TAP_GET_TAP_KEYCODE(keycode));
    sprintf(str, "%s\\\n%s", mod_str, basic_str);
    return true;
}

bool osm_keycode_to_str(char *str, uint16_t keycode) {
    const char *mod_str = mods_to_str(QK_ONE_SHOT_MOD_GET_MODS(keycode));
    sprintf(str, "OSM\n%s", mod_str);
    return true;
}

bool midi_keycode_to_str(char *str, uint16_t keycode) {
    switch (keycode) {
        case QK_MIDI_ON:
            sprintf(str, "MIDI\non");
            return true;
        case QK_MIDI_OFF:
            sprintf(str, "MIDI\noff");
            return true;
        case QK_MIDI_TOGGLE:
            sprintf(str, "MIDI\ntoggle");
            return true;

        case QK_MIDI_NOTE_C_0 ... QK_MIDI_NOTE_B_5: {
            const char *notes[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            int         note      = (keycode - QK_MIDI_NOTE_C_0) % 12;
            int         octave    = (keycode - QK_MIDI_NOTE_C_0) / 12;
            sprintf(str, "%s%d", notes[note], octave);
            return true;
        }

        case QK_MIDI_OCTAVE_N2 ... QK_MIDI_OCTAVE_7: {
            int octave = (int)keycode - QK_MIDI_OCTAVE_0;
            sprintf(str, "Oct %+d", octave);
            return true;
        }
        case QK_MIDI_OCTAVE_DOWN:
            sprintf(str, "Oct");
            return true;
        case QK_MIDI_OCTAVE_UP:
            sprintf(str, "Oct");
            return true;

        case QK_MIDI_TRANSPOSE_N6 ... QK_MIDI_TRANSPOSE_6: {
            int tran = (int)keycode - QK_MIDI_TRANSPOSE_0;
            sprintf(str, "Trans\n%+d", tran);
            return true;
        }
        case QK_MIDI_TRANSPOSE_DOWN:
            sprintf(str, "Trans\n");
            return true;
        case QK_MIDI_TRANSPOSE_UP:
            sprintf(str, "Trans\n");
            return true;

        case QK_MIDI_VELOCITY_0 ... QK_MIDI_VELOCITY_10: {
            int vel = (int)keycode - QK_MIDI_VELOCITY_0;
            sprintf(str, "Vel %d", vel);
            return true;
        }
        case QK_MIDI_VELOCITY_DOWN:
            sprintf(str, "Vel");
            return true;
        case QK_MIDI_VELOCITY_UP:
            sprintf(str, "Vel");
            return true;

        case QK_MIDI_CHANNEL_1 ... QK_MIDI_CHANNEL_16: {
            int chan = (int)keycode - QK_MIDI_CHANNEL_1 + 1;
            sprintf(str, "Ch %d", chan);
            return true;
        }
        case QK_MIDI_CHANNEL_DOWN:
            sprintf(str, "Ch");
            return true;
        case QK_MIDI_CHANNEL_UP:
            sprintf(str, "Ch");
            return true;

        case QK_MIDI_ALL_NOTES_OFF:
            sprintf(str, "Notes\nOff");
            return true;
        case QK_MIDI_SUSTAIN:
            sprintf(str, "Sust");
            return true;
        case QK_MIDI_PORTAMENTO:
            sprintf(str, "Port");
            return true;
        case QK_MIDI_SOSTENUTO:
            sprintf(str, "Sost");
            return true;
        case QK_MIDI_SOFT:
            sprintf(str, "Soft\nPedal");
            return true;
        case QK_MIDI_LEGATO:
            sprintf(str, "Legat");
            return true;
        case QK_MIDI_MODULATION:
            sprintf(str, "Modul");
            return true;
        case QK_MIDI_MODULATION_SPEED_DOWN:
            sprintf(str, "Modul\n");
            return true;
        case QK_MIDI_MODULATION_SPEED_UP:
            sprintf(str, "Modul\n");
            return true;
        case QK_MIDI_PITCH_BEND_DOWN:
            sprintf(str, "Bend");
            return true;
        case QK_MIDI_PITCH_BEND_UP:
            sprintf(str, "Bend");
            return true;
    }
    return false;
}

void get_keycode_str(char *str, uint16_t keycode) {
    if (special_keycode_str(str, keycode))
        return;
    else if (IS_QK_BASIC(keycode) || IS_QK_MODS(keycode))
        mods_keycode_to_str(str, keycode);
    else if (IS_QK_MOD_TAP(keycode))
        modtap_keycode_to_str(str, keycode);
    else if (IS_QK_ONE_SHOT_MOD(keycode))
        osm_keycode_to_str(str, keycode);
    else if (IS_QK_MIDI(keycode))
        midi_keycode_to_str(str, keycode);
    else
        sprintf(str, "Undf");
}

const char *keycode_to_str(uint16_t keycode) {
    static char buf[32];
    get_keycode_str(buf, keycode);
    return buf;
}
