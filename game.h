#ifndef REVERSI_GAME_H
#define REVERSI_GAME_H

#include <stdbool.h>

/*
 * The Reversi engine, lifted from the Basic09 original (Flipper09.b09 by
 * Stephen J. Page, The Rainbow, December 1989) and kept headless: no I/O, no
 * screen. The rules, the positional weight table and the computer's search are
 * the original's, so play is unchanged from the text version.
 *
 * The board keeps the original's 10x10 shape. Ranks and files 1..8 are the
 * playing area; the 0 and 9 rings are sentinel borders that always hold
 * GAME_EMPTY, which is what terminates the ray walks without a bounds test.
 */

#define GAME_EMPTY 0
#define GAME_P1    1        /* the human */
#define GAME_P2    2        /* the computer */

/* A move flips at most 18 discs, plus the disc played. */
#define GAME_MAX_CHANGES 20

typedef struct {
    unsigned char x, y;
} GameCell;

extern unsigned char game_board[10][10];
extern int game_player;         /* whose turn it is: GAME_P1 or GAME_P2 */
extern int game_moves;          /* discs placed; the game ends at 64 */

/* Cells whose contents changed during the last game_play(), so the view can
   repaint just those instead of the whole board. */
extern GameCell game_changed[GAME_MAX_CHANGES];
extern int game_changed_count;

/** Reset to the opening position and rebuild the weight table. */
void game_new(void);

/** True if @p player has at least one legal move. */
bool game_has_move(int player);

/**
 * Play at (x, y) for the player to move.
 *
 * Returns false and changes nothing if the square is occupied, off the board,
 * or would flip nothing. On success the discs are flipped, game_changed lists
 * every square that changed, game_moves advances and the turn passes.
 */
bool game_play(int x, int y);

/**
 * Choose a move for the player to move, using the original's search.
 * Returns false (and leaves *x, *y as 0) when no move is available.
 */
bool game_computer_move(int *x, int *y);

/**
 * Pass the turn -- what entering 0,0 did in the original (line 141).
 *
 * The first pass just hands the turn over. A second one with no move played in
 * between means neither side can go, and the game is over; returns true in that
 * case. Playing a disc clears the state again.
 */
bool game_pass(void);

/** Count the discs. */
void game_score(int *s1, int *s2);

/** True once the board is full or neither side can move. */
bool game_over(void);

#endif /* REVERSI_GAME_H */
