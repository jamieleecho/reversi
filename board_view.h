#ifndef REVERSI_BOARD_VIEW_H
#define REVERSI_BOARD_VIEW_H

#include <stdbool.h>

/*
 * The board's on-screen presentation: palette, layout, drawing and hit-testing.
 *
 * Drawing on this hardware is slow, so a cell is never drawn twice. The three
 * possible cell appearances (empty, player 1, player 2) are rendered once at
 * startup and captured into Get/Put buffers; everything after that is a putblk.
 * Moves repaint only the squares the engine reports as changed.
 */

/** Install the palette for the current screen type. Call from pre_init. */
void bv_set_palette(void);

/**
 * Measure the window and lay out the board.
 * Returns true if the geometry changed (so the caller should redraw).
 */
bool bv_layout(void);

/** Render the three cell prototypes and capture them into Get/Put buffers. */
void bv_build_buffers(void);

/** Release the Get/Put buffers. */
void bv_free_buffers(void);

/** Repaint the whole content area: background, board, every cell. */
void bv_draw_all(void);

/** Repaint only the cells the last move changed. */
void bv_draw_changed(void);

/** Map a window-relative pixel to a board square; false if outside the board. */
bool bv_hit_test(int px, int py, int *x, int *y);

/** Draw a one-line message above the board, clearing whatever was there. */
void bv_status(const char *msg);

/** Redraw the last status message, after something has painted over it. */
void bv_refresh_status(void);

/** Flash a message above the board in reverse video: 5 blinks at 0.5s. */
void bv_blink_result(const char *msg);

#endif /* REVERSI_BOARD_VIEW_H */
