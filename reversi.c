/*
 * Reversi for Multi-Vue.
 *
 * A point-and-click front end over the engine from Flipper09.b09 (OTHELL09) by
 * Stephen J. Page, published in the December 1989 issue of The Rainbow.
 * Player 1 is you; player 2 is the computer, playing the original's search.
 *
 * The presentation lives in board_view.c and the rules in game.c; this file is
 * the application: menus, dispatch, and whose turn it is.
 */
#include <stdio.h>
#include <unistd.h>          /* exit */

#include <mvkit/mvkit.h>

#include "game.h"
#include "board_view.h"
#include "version.h"

/* App-chosen menu ids; cgfx reserves the low numbers for its own. */
#define MN_GAME 30
#define MN_HELP 31

/* MIDSCR's title field is char[15] and needs room for the NUL, so a menu item
   menu narrower than its widest item renders ragged. Each menu therefore gets a
   dash string at least as long as its longest title. */
#define GAME_DASHES "--------"
#define HELP_DASHES "--------------"

static MIDSCR game_items[] = {
    MV_MENU_ITEM("New"),
    MV_MENU_ITEM("Pass"),
    MV_MENU_SEPARATOR_S(GAME_DASHES),
    MV_MENU_ITEM("Quit"),
};

static MIDSCR help_items[] = {
    MV_MENU_ITEM("About..."),
};

static MNDSCR menus[] = {
    MV_MENU_SIZED("Game", MN_GAME, MV_MENU_WIDTH_OF(GAME_DASHES), game_items),
    MV_MENU_SIZED("Help", MN_HELP, MV_MENU_WIDTH_OF(HELP_DASHES), help_items),
};

mv_set_menus_sized(reversi_window, "Reversi", menus, 20, 18);

static bool game_finished;

/* ------------------------------------------------------------------------- */

static void announce_result(void)
{
    char msg[40];
    int s1, s2;

    game_score(&s1, &s2);
    if (s1 > s2)
        sprintf(msg, "You win  %d - %d", s1, s2);
    else if (s2 > s1)
        sprintf(msg, "Computer wins  %d - %d", s2, s1);
    else
        sprintf(msg, "A draw  %d - %d", s1, s2);

    game_finished = true;
    bv_blink_result(msg);
}

/*
 * Hand play back to the user. Runs the computer's turns -- there may be several
 * in a row if the user has to pass -- and leaves a status line saying what is
 * expected next.
 */
static void advance(void)
{
    int x, y;

    for (;;) {
        if (game_over()) {
            announce_result();
            return;
        }

        if (game_player == GAME_P1) {
            /* Refresh the menu bar before writing the status line: the redraw
               paints over the row the status sits on. */
            mv_app_refresh_menubar();
            if (game_has_move(GAME_P1))
                bv_status("Your move");
            else
                bv_status("No move - choose Game/Pass");
            return;
        }

        /* Computer's turn. The search is not fast on a 6809, so say so. */
        bv_status("Thinking...");
        if (game_computer_move(&x, &y) && game_play(x, y)) {
            bv_draw_changed();
        } else if (game_pass()) {
            /* Both sides passed in a row -- the original's 0,0 twice. */
            announce_result();
            return;
        }
    }
}

/* ------------------------------------------------------------------------- */

static void new_action(MSRET *msinfo, int menuid, int itemno)
{
    game_new();
    game_finished = false;
    bv_draw_all();
    advance();
}

/*
 * Pass is what entering 0,0 did in the original: hand the turn over, and if the
 * other side had already passed, the game is over.
 */
static void pass_action(MSRET *msinfo, int menuid, int itemno)
{
    if (game_finished || game_player != GAME_P1)
        return;
    if (game_pass()) {
        announce_result();
        return;
    }
    advance();
}

/*
 * Quitting is confirmed, and the same handler serves the Game/Quit item and the
 * window close box (MN_CLOS), so both routes ask. Answering No repaints, since
 * the dialog covered the window.
 */
static void quit_action(MSRET *msinfo, int menuid, int itemno)
{
    if (mv_app_show_message_box("   Really quit Reversi?",
                                MVMessageBoxType_YesNo) != MVMessageBoxResult_Yes) {
        bv_draw_all();
        bv_refresh_status();
        return;
    }
    bv_free_buffers();
    exit(0);
}

static void about_action(MSRET *msinfo, int menuid, int itemno)
{
    /* Lines are split on CR/LF and each one is centred by the message box
       itself within its 26-column interior, so they carry no padding here.
       MVKit's own callers separate with \r\n. */
    mv_app_show_message_box(
        "Reversi " APP_VERSION " - based on\r\n"
        "Flipper09 by S. Page\r\n"
        "S. Page Rainbow Dec 1989",
        MVMessageBoxType_Info);
    /* The dialog painted over the window; put the board and status back. */
    bv_draw_all();
    bv_refresh_status();
}

static const MVMenuItemAction menu_actions[] = {
    {MN_GAME, 1, new_action},
    {MN_GAME, 2, pass_action},
    {MN_GAME, 4, quit_action},     /* item 3 is the separator */
    {MN_CLOS, 1, quit_action},     /* the window close box asks too */
    {MN_HELP, 1, about_action},
    MV_MENU_ACTION_END
};

/*
 * Pass is only meaningful when it is your turn and you have nothing legal to
 * play -- otherwise passing would be a way to skip a turn you could use.
 */
static void refresh_menus(void)
{
    bool can_pass = !game_finished &&
                    game_player == GAME_P1 &&
                    !game_has_move(GAME_P1);
    mv_menu_item_set_enabled(game_items, 1, can_pass);
}

/* ------------------------------------------------------------------------- */

static void handle_event(MVUiEvent *event)
{
    int x, y;

    /* A resize changes the layout, which invalidates the cell buffers: they
       hold pixels at the old cell size. Rebuild and repaint. */
    if (bv_layout()) {
        bv_build_buffers();
        bv_draw_all();
        advance();
        return;
    }

    if (event->event_type != MVUiEventType_MouseClick)
        return;
    if (game_finished || game_player != GAME_P1)
        return;

    if (!bv_hit_test(event->info.mouse.pt_wrx, event->info.mouse.pt_wry, &x, &y))
        return;

    if (!game_play(x, y))
        return;                    /* illegal square: ignore the click */

    bv_draw_changed();
    advance();
}

/* ------------------------------------------------------------------------- */

static void pre_init(int argc, char **argv)
{
    bv_set_palette();
}

static void app_init(void)
{
    bv_layout();
    bv_build_buffers();
    game_new();
    game_finished = false;
    bv_draw_all();
    advance();
}

int main(int argc, char **argv)
{
    return mv_app_run(argc, argv, &reversi_window,
        pre_init, app_init, menu_actions, refresh_menus, handle_event);
}
