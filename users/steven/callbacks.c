
#include "callbacks.h"
#include "deferred_exec.h"


os_variant_t os_type;

uint32_t startup_exec(uint32_t trigger_time, void *cb_arg) {
    /* do something */

    if (is_keyboard_master()) {
        os_type = detected_host_os();
        if (os_type) {
            bool is_mac                  = (os_type == OS_MACOS) || (os_type == OS_IOS);
            keymap_config.swap_lctl_lgui = keymap_config.swap_rctl_rgui = is_mac;
            switch (os_type) {
                case OS_UNSURE:
                    xprintf("unknown OS Detected\n");
                    break;
                case OS_LINUX:
                    xprintf("Linux Detected\n");
                    break;
                case OS_WINDOWS:
                    xprintf("Windows Detected\n");
                    break;
#    if 0
                case OS_WINDOWS_UNSURE:
                    xprintf("Windows? Detected\n");
                    break;
#    endif
                case OS_MACOS:
                    xprintf("MacOS Detected\n");
                    break;
                case OS_IOS:
                    xprintf("iOS Detected\n");
                    break;
#    if 0
                case OS_PS5:
                    xprintf("PlayStation 5 Detected\n");
                    break;
                case OS_HANDHELD:
                    xprintf("Nintend Switch/Quest 2 Detected\n");
                    break;
#    endif
            }
        }
    }
    return os_type ? 0 : 500;
}



void                       keyboard_post_init_user(void) {


    defer_exec(100, startup_exec, NULL);

}