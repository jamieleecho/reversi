/*
 * Reversi engine -- the rules and the computer player from Flipper09.b09,
 * with the screen and keyboard stripped out. Basic09 line numbers from the
 * original listing are given so the two can be compared.
 *
 * Two things carried over deliberately, because they define how this program
 * plays and "the same game" means keeping them:
 *
 *   - va[][] is the original's positional weight table: corners 2000, the
 *     squares diagonally inside them -250, and so on, mirrored into all four
 *     quadrants. It is mutated during play (lines 184-208) -- once 60 discs are
 *     down every square is worth a flat 10, and playing an edge square re-values
 *     its neighbours -- so it is per-game state, not a constant.
 *   - The search (line 70) is one ply deep with a reply term: for each candidate
 *     it sums the weights it would flip, then subtracts the best reply the
 *     opponent could make, and takes the largest difference.
 */
#include "game.h"

unsigned char game_board[10][10];
int game_player;
int game_moves;
GameCell game_changed[GAME_MAX_CHANGES];
int game_changed_count;

static int va[10][10];

/* The original's `nogo` (line 27/144): set when a player passes, cleared by any
   played disc. A pass while it is already set ends the game (line 142). */
static bool nogo;
static bool ended_by_pass;

/* Weight seeds, DATA lines 92-95, read in x-major order (lines 83-91) and
   mirrored into the other three quadrants. */
static const int weight_seed[16] = {
    2000, -100,  50,  40,
    -100, -250, -21, -15,
      50,  -21,   5,   2,
      40,  -15,   2,   1
};

/* Scratch for the search's undo list (xa/ya in the original, lines 280-281). */
static unsigned char sx[GAME_MAX_CHANGES], sy[GAME_MAX_CHANGES];

static void record_change(int x, int y)
{
    if (game_changed_count < GAME_MAX_CHANGES) {
        game_changed[game_changed_count].x = (unsigned char)x;
        game_changed[game_changed_count].y = (unsigned char)y;
        game_changed_count++;
    }
}

void game_new(void)                                     /* 73-95 */
{
    int x, y, n;

    for (x = 0; x <= 9; x++)
        for (y = 0; y <= 9; y++)
            game_board[x][y] = GAME_EMPTY;

    game_board[4][4] = GAME_P1;                         /* 79-82 */
    game_board[5][5] = GAME_P1;
    game_board[4][5] = GAME_P2;
    game_board[5][4] = GAME_P2;

    n = 0;
    for (x = 1; x <= 4; x++) {
        for (y = 1; y <= 4; y++) {
            int w = weight_seed[n++];
            va[x][y] = w;
            va[x][9 - y] = w;
            va[9 - x][y] = w;
            va[9 - x][9 - y] = w;
        }
    }

    game_player = GAME_P1;                              /* 125 */
    game_moves = 4;                                     /* 126 */
    game_changed_count = 0;
    nogo = false;                                       /* 27 */
    ended_by_pass = false;
}

/*
 * Would playing (x, y) flip anything? Walks each of the eight rays over a run
 * of enemy discs and checks it ends on one of ours (lines 152-173, without the
 * flipping).
 */
static bool would_flip(int player, int x, int y)
{
    int dx, dy, cx, cy;

    if (x < 1 || x > 8 || y < 1 || y > 8)
        return false;
    if (game_board[x][y] != GAME_EMPTY)
        return false;

    for (dx = -1; dx <= 1; dx++) {
        for (dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0)
                continue;
            if (game_board[x + dx][y + dy] != 3 - player)
                continue;
            cx = x + dx;
            cy = y + dy;
            do {
                cx += dx;
                cy += dy;
            } while (game_board[cx][cy] == 3 - player);
            if (game_board[cx][cy] == player)
                return true;
        }
    }
    return false;
}

bool game_has_move(int player)
{
    int x, y;
    for (x = 1; x <= 8; x++)
        for (y = 1; y <= 8; y++)
            if (would_flip(player, x, y))
                return true;
    return false;
}

/* The weight-table updates that follow a played disc (lines 184-208). */
static void reweight(int x, int y)
{
    int i, j;

    if (game_moves == 60) {                             /* 184 */
        for (i = 1; i <= 8; i++)
            for (j = 1; j <= 8; j++)
                va[i][j] = 10;
    }

    if (x == 1 || x == 8) {                             /* 191 */
        if (y == 1 || y == 8) {
            va[x][y - 1] = 500;
            va[x][y + 1] = 500;
        } else {
            va[x][y - 1] += 100;
            va[x][y + 1] += 200;
        }
    }
    if (y == 1 || y == 8) {                             /* 200 */
        if (x == 1 || x == 8) {
            va[x - 1][y] = 500;
            va[x + 1][y] = 500;
        } else {
            va[x - 1][y] += 200;
            va[x + 1][y] += 100;
        }
    }
}

bool game_play(int x, int y)                            /* 141-209 */
{
    int dx, dy, cx, cy, ll;
    int player = game_player;
    bool point = false;

    game_changed_count = 0;

    if (x < 1 || x > 8 || y < 1 || y > 8)
        return false;
    if (game_board[x][y] != GAME_EMPTY)                  /* 148 "POSITION TAKEN" */
        return false;
    if (!would_flip(player, x, y))                       /* 174 "NO POINTS!" */
        return false;

    for (dx = -1; dx <= 1; dx++) {                       /* 152 */
        for (dy = -1; dy <= 1; dy++) {                   /* 153 */
            if (game_board[x + dx][y + dy] != 3 - player)    /* 154 */
                continue;
            cx = x + dx;
            cy = y + dy;
            ll = 0;
            do {                                         /* 158 */
                cx += dx;
                cy += dy;
                ll++;
            } while (game_board[cx][cy] == 3 - player);   /* 162 */

            if (game_board[cx][cy] == player) {           /* 163 */
                point = true;
                do {                                      /* 164 */
                    cx -= dx;
                    cy -= dy;
                    ll--;
                    game_board[cx][cy] = (unsigned char)player;   /* GOSUB 50 */
                    record_change(cx, cy);
                } while (ll != 0);
            }
        }
    }

    if (!point)
        return false;

    game_board[x][y] = (unsigned char)player;             /* 177-179 */
    record_change(x, y);
    nogo = false;                                         /* 181 */
    game_moves++;                                         /* 182 */
    game_player = 3 - player;                             /* 183 */
    reweight(x, y);
    return true;
}

bool game_pass(void)                                      /* 141-147 */
{
    game_changed_count = 0;

    if (nogo) {                                           /* 142 IF nogo THEN 40 */
        ended_by_pass = true;
        return true;
    }

    nogo = true;                                          /* 144 */
    game_player = 3 - game_player;                        /* 145 */
    return false;
}

bool game_computer_move(int *x, int *y)                   /* 255-337 */
{
    int player = game_player;
    int total, best_x, best_y;
    int x1, y1, x2, y2, x3, y3, dx, dy, ll;
    int sum1, sum2, sum3, sum4, cnter;
    bool good;

    total = -10000;                                       /* 256 */
    best_x = 0;
    best_y = 0;

    for (x1 = 1; x1 <= 8; x1++) {                         /* 259 */
        for (y1 = 1; y1 <= 8; y1++) {                     /* 260 */
            if (game_board[x1][y1] != GAME_EMPTY)          /* 261 */
                continue;

            cnter = 0;
            sum1 = va[x1][y1];                             /* 262 */

            /* Play the candidate for real, remembering what to undo. */
            for (dx = -1; dx <= 1; dx++) {                 /* 263 */
                for (dy = -1; dy <= 1; dy++) {             /* 264 */
                    if (game_board[x1 + dx][y1 + dy] != 3 - player)   /* 265 */
                        continue;
                    x2 = x1 + dx;
                    y2 = y1 + dy;
                    ll = 0;
                    do {                                   /* 269 */
                        x2 += dx;
                        y2 += dy;
                        ll++;
                    } while (game_board[x2][y2] == 3 - player);       /* 273 */

                    if (game_board[x2][y2] == player) {    /* 274 */
                        do {                               /* 275 */
                            x2 -= dx;
                            y2 -= dy;
                            sum1 += va[x2][y2] * 2;
                            if (cnter < GAME_MAX_CHANGES) {
                                sx[cnter] = (unsigned char)x2;
                                sy[cnter] = (unsigned char)y2;
                            }
                            cnter++;
                            ll--;
                            game_board[x2][y2] = (unsigned char)player;
                        } while (ll != 0);                 /* 284 */
                    }
                }
            }

            if (cnter > 0) {                               /* 289 */
                game_board[x1][y1] = (unsigned char)player;    /* 290 */
                sum2 = -10000;                             /* 291 */

                /* Best reply the opponent could then make. */
                for (x2 = 1; x2 <= 8; x2++) {              /* 292 */
                    for (y2 = 1; y2 <= 8; y2++) {          /* 293 */
                        if (game_board[x2][y2] != GAME_EMPTY)   /* 294 */
                            continue;
                        sum4 = va[x2][y2];
                        good = false;                      /* 295 */

                        for (dx = -1; dx <= 1; dx++) {     /* 296 */
                            for (dy = -1; dy <= 1; dy++) { /* 297 */
                                if (game_board[x2 + dx][y2 + dy] != player)   /* 298 */
                                    continue;
                                x3 = x2 + dx;
                                y3 = y2 + dy;
                                sum3 = 0;                  /* 301 */
                                do {                       /* 302 */
                                    sum3 += 2 * va[x3][y3];
                                    x3 += dx;
                                    y3 += dy;
                                } while (game_board[x3][y3] == player);   /* 306 */

                                if (game_board[x3][y3] == 3 - player) {   /* 307 */
                                    sum4 += sum3;
                                    good = true;
                                }
                            }
                        }

                        if (good && sum4 > sum2)           /* 314-315 */
                            sum2 = sum4;
                    }
                }

                if (sum1 - sum2 > total) {                 /* 322 */
                    total = sum1 - sum2;
                    best_x = x1;
                    best_y = y1;
                }

                /* Undo the candidate. */
                while (cnter > 0) {                        /* 327 */
                    cnter--;
                    if (cnter < GAME_MAX_CHANGES)
                        game_board[sx[cnter]][sy[cnter]] = (unsigned char)(3 - player);
                }
                game_board[x1][y1] = GAME_EMPTY;           /* 331 */
            }
        }
    }

    *x = best_x;
    *y = best_y;
    return best_x != 0 || best_y != 0;
}

void game_score(int *s1, int *s2)                          /* 211-218 */
{
    int x, y;
    *s1 = 0;
    *s2 = 0;
    for (x = 1; x <= 8; x++) {
        for (y = 1; y <= 8; y++) {
            if (game_board[x][y] == GAME_P1)
                (*s1)++;
            else if (game_board[x][y] == GAME_P2)
                (*s2)++;
        }
    }
}

bool game_over(void)
{
    /* The original has exactly two exits: the board fills (line 127), or a
       player passes when the other already had (line 142). */
    return game_moves >= 64 || ended_by_pass;
}
