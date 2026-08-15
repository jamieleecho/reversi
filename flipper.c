/*
 * flipper -- Reversi for Multi-Vue.
 *
 * Stage 1: the smallest app that exercises the whole pipeline -- cmoc compiles
 * it inside coco-dev, app.mk lands it on a bootable NitrOS-9 disk with a
 * Multi-Vue launcher, and MAME boots it into a Multi-Vue window. Stage 2
 * replaces this body with the Reversi engine converted from Flipper09.b09.
 *
 * mv_menu_none() declares a framed window with no menu bar; the window's close
 * box quits. Text printed to stdout lands in the window, and Flush() forces it
 * out (cgfx buffers writes).
 */
#include <stdio.h>

#include <mvkit/mvkit.h>

mv_menu_none(flipper_window, "flipper");

static void flipper_init(void) {
    printf("Flipper -- Reversi for Multi-Vue\n");
    printf("\n");
    printf("Stage 1 scaffold: toolchain OK.\n");
    Flush();
}

int main(int argc, char **argv) {
    return mv_app_run(argc, argv, &flipper_window,
        mv_app_pre_init_nop, flipper_init, mv_app_menu_actions_nop,
        mv_app_refresh_menus_action_nop, mv_app_event_nop);
}
