/*
 * Board presentation: palette, layout, drawing, hit-testing.
 *
 * Screen types supported, chosen at run time from _cgfx_gs_styp():
 *
 *   5  640x200, 2 colours (1 bpp)
 *   6  320x200, 4 colours (2 bpp)
 *   7  640x200, 4 colours (2 bpp)
 *   8  320x200, 16 colours (4 bpp)  <- what the launcher AIF asks for
 *
 * Palette note: cowin draws every piece of window chrome -- menu bar, dropdowns,
 * shadows, 3D edges, and the screen background around the window -- from
 * palette registers 0..3, and expects them ordered darkest to lightest. The
 * 4-colour modes have only those four registers, so an app cannot have its own
 * hues there without recolouring the chrome. Those modes therefore keep the
 * standard black / dark grey / light grey / white ramp and distinguish the
 * players by tone. 16-colour mode has registers 4..15 free and gets the green
 * board with blue and red discs.
 *
 * Pixel aspect: the 640-wide modes have pixels roughly half as wide as they are
 * tall, so a square looks like a tall rectangle. Cells there are made twice as
 * wide as they are tall, which makes the board look square and the discs round.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <cgfx.h>
#include <mvkit/mvkit.h>

#include "game.h"
#include "board_view.h"

/* cgfx colour numbers, %R1 G1 B1 R0 G0 B0 (see mv_theme_default). */
#define C_BLACK   0x00
#define C_DKGREY  0x07
#define C_LTGREY  0x38
#define C_WHITE   0x3f
#define C_BLUE    0x09
#define C_RED     0x24
#define C_GREEN   0x12

/* Ticks per half second; NitrOS-9 on the CoCo 3 runs a 60 Hz clock. */
#define HALF_SECOND 30

/* Get/Put buffer numbers within our group. The convention is to use the
   process id as the group so buffers cannot collide with another app's. */
#define BUF_EMPTY 1
#define BUF_P1    2
#define BUF_P2    3

static int screen_type;
static bool wide;          /* 640-pixel modes (types 5 and 7) */
static int bpp;

/* Palette registers this app draws with. */
static int reg_bg, reg_fg, reg_p1, reg_p2;
static int reg_board_base, reg_board_ink;   /* board fill, and the dither ink */
static int reg_shadow;     /* the board's drop shadow */
static bool board_dither;  /* dither the board, vs. fill it solid */

/* Layout, in pixels, window-relative. */
static int cell_w, cell_h;
static int board_x, board_y;
static int status_row;     /* character row for the status/result line */
static int win_cols, win_rows;

static int grp;            /* Get/Put group = our pid */
static bool buffers_ready;

/* ------------------------------------------------------------------------- */

void bv_set_palette(void)
{
    _cgfx_gs_styp(MV_OUTPATH, &screen_type);

    wide = (screen_type == 5 || screen_type == 7);
    bpp = (screen_type == 5) ? 1 : (screen_type == 8) ? 4 : 2;

    if (screen_type == 5) {
        /* Two colours: white paper, black ink. Player 1 is a white disc with a
           black rim, player 2 a solid black one; the board between them is a
           50% dither, so both read clearly against it. */
        _cgfx_palette(MV_OUTPATH, 0, C_BLACK);
        _cgfx_palette(MV_OUTPATH, 1, C_WHITE);
        reg_fg = 0; reg_bg = 1;
        reg_p1 = 1; reg_p2 = 0;
        reg_board_base = 1; reg_board_ink = 0;
        reg_shadow = 0;
        board_dither = true;
    } else if (screen_type == 8) {
        /* Sixteen colours: chrome keeps its standard grey ramp in 0..3, and the
           app's colours live in 4..6 where they cannot disturb it. */
        _cgfx_palette(MV_OUTPATH, 0, C_BLACK);
        _cgfx_palette(MV_OUTPATH, 1, C_DKGREY);
        _cgfx_palette(MV_OUTPATH, 2, C_LTGREY);
        _cgfx_palette(MV_OUTPATH, 3, C_WHITE);
        _cgfx_palette(MV_OUTPATH, 4, C_GREEN);
        _cgfx_palette(MV_OUTPATH, 5, C_BLUE);
        _cgfx_palette(MV_OUTPATH, 6, C_RED);
        reg_fg = 0; reg_bg = 4;
        reg_p1 = 5; reg_p2 = 6;
        reg_board_base = 1; reg_board_ink = 1;   /* solid dark grey */
        reg_shadow = 0;
        board_dither = false;
    } else {
        /* Four colours: the standard Multi-Vue chrome ramp, left intact so the
           menu bar and the screen background around the window look native.
           There is no room for blue and red here without taking over registers
           cowin draws its chrome from, so the players are told apart by tone
           instead -- white for player 1, black for player 2, over a solid light
           grey board. Dark grey was the obvious first choice for player 2 and
           turned out to be nearly invisible against a dithered board, which
           reads as mid grey; black and white separate cleanly from light grey
           and from each other. */
        _cgfx_palette(MV_OUTPATH, 0, C_BLACK);
        _cgfx_palette(MV_OUTPATH, 1, C_DKGREY);
        _cgfx_palette(MV_OUTPATH, 2, C_LTGREY);
        _cgfx_palette(MV_OUTPATH, 3, C_WHITE);
        reg_fg = 0; reg_bg = 3;
        reg_p1 = 3; reg_p2 = 0;
        reg_board_base = 2; reg_board_ink = 2;   /* solid light grey */
        reg_shadow = 0;
        board_dither = false;
    }
}

/* ------------------------------------------------------------------------- */

bool bv_layout(void)
{
    int cols, rows, avail_w, avail_h, cw, ch, ox, oy;

    if (_cgfx_gs_scsz(MV_OUTPATH, &cols, &rows) != 0)
        return false;

    /* A character cell of margin all round, plus rows at the top for the
       status / result banner, which sits on row 0 directly under the menu
       bar. */
    avail_w = (cols - 2) * 8;
    avail_h = (rows - 4) * 8;
    if (avail_w < 64) avail_w = 64;
    if (avail_h < 64) avail_h = 64;

    /* Cell height: a multiple of 8 so squares line up with the 8x8 grid the
       mouse moves on in its default resolution -- a click always lands
       unambiguously inside one square. */
    ch = (avail_h / 8) & ~7;
    if (ch > 24) ch = 24;
    if (ch < 8)  ch = 8;

    cw = wide ? ch * 2 : ch;
    while (cw > 8 && cw * 8 > avail_w)
        cw -= 8;

    ox = 8 + (avail_w - cw * 8) / 2;
    oy = 24 + (avail_h - ch * 8) / 2;
    if (ox < 8)  ox = 8;
    if (oy < 24) oy = 24;

    if (cw == cell_w && ch == cell_h && ox == board_x && oy == board_y &&
        cols == win_cols && rows == win_rows)
        return false;

    cell_w = cw;
    cell_h = ch;
    board_x = ox;
    board_y = oy;
    win_cols = cols;
    win_rows = rows;
    status_row = 0;
    return true;
}

/* ------------------------------------------------------------------------- */

/* Integer square root, on a long because the ellipse term overflows 16 bits
   for the larger cell sizes. */
static unsigned int isqrt32(unsigned long v)
{
    unsigned long rem = 0, root = 0, i;
    for (i = 0; i < 16; i++) {
        root <<= 1;
        rem = (rem << 2) | (v >> 30);
        v <<= 2;
        if (root < rem) {
            root++;
            rem -= root;
            root++;
        }
    }
    return (unsigned int)(root >> 1);
}

static void clear_content(void);

static void plot(int x, int y)
{
    _cgfx_setdptr(MV_OUTPATH, x, y);
    _cgfx_bar(MV_OUTPATH, x, y);
}

static void fill_rect(int x, int y, int w, int h, int colour)
{
    _cgfx_fcolor(MV_OUTPATH, colour);
    _cgfx_setdptr(MV_OUTPATH, x, y);
    _cgfx_bar(MV_OUTPATH, x + w - 1, y + h - 1);
}

/*
 * Paint one cell's background at (px, py): either a 50% black/white checker --
 * which at this pixel size reads as a flat grey, the way Windows Reversi's
 * board does -- or a solid fill where the palette has a real grey to use.
 * A black rule along the right and bottom edges makes the grid.
 */
static void draw_cell_background(int px, int py)
{
    int i, j;

    fill_rect(px, py, cell_w, cell_h, reg_board_base);
    if (board_dither) {
        _cgfx_fcolor(MV_OUTPATH, reg_board_ink);
        for (j = 0; j < cell_h; j++)
            for (i = (j & 1); i < cell_w; i += 2)
                plot(px + i, py + j);
    }

    /* Grid lines. */
    _cgfx_fcolor(MV_OUTPATH, reg_fg);
    _cgfx_setdptr(MV_OUTPATH, px + cell_w - 1, py);
    _cgfx_bar(MV_OUTPATH, px + cell_w - 1, py + cell_h - 1);
    _cgfx_setdptr(MV_OUTPATH, px, py + cell_h - 1);
    _cgfx_bar(MV_OUTPATH, px + cell_w - 1, py + cell_h - 1);
}

/* Half-width of an ellipse of radii (ra, rb) at row j. */
static int ellipse_hw(int ra, int rb, int j)
{
    unsigned long t;
    if (rb < 1)
        return 0;
    t = (unsigned long)((long)rb * rb - (long)j * j);
    return (int)(isqrt32(t * (unsigned long)ra * ra) / (unsigned)rb);
}

/*
 * A disc inscribed in the cell, drawn as one horizontal run per scan line from
 * the ellipse equation. Because the cell is already proportioned to the mode's
 * pixel aspect, the result looks round in both 320- and 640-wide modes.
 *
 * Two passes: a dark rim, then the body colour one pixel inside it.
 */
static void draw_disc(int px, int py, int colour)
{
    int a, b, cx, cy, j, hw;

    a = (cell_w - 4) / 2;
    b = (cell_h - 4) / 2;
    if (a < 2) a = 2;
    if (b < 2) b = 2;
    cx = px + cell_w / 2;
    cy = py + cell_h / 2;

    _cgfx_fcolor(MV_OUTPATH, reg_fg);
    for (j = -b; j <= b; j++) {
        hw = ellipse_hw(a, b, j);
        _cgfx_setdptr(MV_OUTPATH, cx - hw, cy + j);
        _cgfx_bar(MV_OUTPATH, cx + hw, cy + j);
    }

    _cgfx_fcolor(MV_OUTPATH, colour);
    for (j = -(b - 1); j <= b - 1; j++) {
        hw = ellipse_hw(a - 1, b - 1, j);
        _cgfx_setdptr(MV_OUTPATH, cx - hw, cy + j);
        _cgfx_bar(MV_OUTPATH, cx + hw, cy + j);
    }
}

/* ------------------------------------------------------------------------- */

void bv_build_buffers(void)
{
    int len, i;

    bv_free_buffers();

    grp = getpid();

    /* A graphics window draws no text until a font is selected -- without this
       the status line is silently a no-op, band and all. The standard 8x8 font
       comes from SYS/stdfonts, which the boot startup merges into grfdrv. */
    _cgfx_font(MV_OUTPATH, GRP_FONT, FNT_S8X8);

    /* Bytes for one cell at this depth, plus room for the buffer header. */
    len = (cell_w * bpp / 8) * cell_h + 64;

    for (i = BUF_EMPTY; i <= BUF_P2; i++)
        _cgfx_dfngpbuf(MV_OUTPATH, grp, i, len);

    /* Clear and flush FIRST. The prototypes are drawn on the real window and
       read back with getblk, so anything still queued when the capture happens
       lands inside the saved cell images -- which is what put a large box
       through the board and pieces. bv_draw_all() repaints the scratch after. */
    clear_content();
    Flush();

    draw_cell_background(board_x, board_y);
    Flush();
    _cgfx_getblk(MV_OUTPATH, grp, BUF_EMPTY, board_x, board_y, cell_w, cell_h);

    draw_cell_background(board_x, board_y);
    draw_disc(board_x, board_y, reg_p1);
    Flush();
    _cgfx_getblk(MV_OUTPATH, grp, BUF_P1, board_x, board_y, cell_w, cell_h);

    draw_cell_background(board_x, board_y);
    draw_disc(board_x, board_y, reg_p2);
    Flush();
    _cgfx_getblk(MV_OUTPATH, grp, BUF_P2, board_x, board_y, cell_w, cell_h);

    buffers_ready = true;
}

void bv_free_buffers(void)
{
    int i;
    if (!buffers_ready)
        return;
    for (i = BUF_EMPTY; i <= BUF_P2; i++)
        _cgfx_kilbuf(MV_OUTPATH, grp, i);
    buffers_ready = false;
}

/* ------------------------------------------------------------------------- */

static void put_cell(int x, int y)
{
    int buf = (game_board[x][y] == GAME_P1) ? BUF_P1 :
              (game_board[x][y] == GAME_P2) ? BUF_P2 : BUF_EMPTY;
    _cgfx_putblk(MV_OUTPATH, grp, buf,
                 board_x + (x - 1) * cell_w,
                 board_y + (y - 1) * cell_h);
}

/* Paint the whole content area. Row 0 is ours -- the menu bar is chrome drawn
   above it, not an overlay on it -- so filling from 0 avoids a dark strip
   between the menu bar and the board. */
static void clear_content(void)
{
    fill_rect(0, 0, win_cols * 8, win_rows * 8, reg_bg);
}

static void draw_line_h(int x0, int x1, int y, int colour)
{
    _cgfx_fcolor(MV_OUTPATH, colour);
    _cgfx_setdptr(MV_OUTPATH, x0, y);
    _cgfx_bar(MV_OUTPATH, x1, y);
}

static void draw_line_v(int x, int y0, int y1, int colour)
{
    _cgfx_fcolor(MV_OUTPATH, colour);
    _cgfx_setdptr(MV_OUTPATH, x, y0);
    _cgfx_bar(MV_OUTPATH, x, y1);
}

/*
 * The board's outer edge and its drop shadow.
 *
 * Each cell rules only its own right and bottom edge, so the grid's top and
 * left edges do not exist until they are drawn here. A solid shadow is then
 * offset down and to the right, so the board reads as sitting above the
 * background rather than being outlined twice.
 */
static void draw_board_frame(void)
{
    int l = board_x, t = board_y;
    int r = board_x + cell_w * 8 - 1;
    int b = board_y + cell_h * 8 - 1;
    int i;

    draw_line_h(l - 1, r + 1, t - 1, reg_fg);
    draw_line_v(l - 1, t - 1, b + 1, reg_fg);
    draw_line_h(l - 1, r + 1, b + 1, reg_fg);
    draw_line_v(r + 1, t - 1, b + 1, reg_fg);

    for (i = 1; i <= 2; i++) {
        draw_line_v(r + 1 + i, t - 1 + i, b + 1 + i, reg_shadow);
        draw_line_h(l - 1 + i, r + 1 + i, b + 1 + i, reg_shadow);
    }
}

void bv_draw_all(void)
{
    int x, y;

    clear_content();
    draw_board_frame();

    for (x = 1; x <= 8; x++)
        for (y = 1; y <= 8; y++)
            put_cell(x, y);

    Flush();
}

void bv_draw_changed(void)
{
    int i;
    for (i = 0; i < game_changed_count; i++)
        put_cell(game_changed[i].x, game_changed[i].y);
    Flush();
}

/* ------------------------------------------------------------------------- */

bool bv_hit_test(int px, int py, int *x, int *y)
{
    int cx, cy;

    if (px < board_x || py < board_y)
        return false;
    cx = (px - board_x) / cell_w + 1;
    cy = (py - board_y) / cell_h + 1;
    if (cx < 1 || cx > 8 || cy < 1 || cy > 8)
        return false;

    *x = cx;
    *y = cy;
    return true;
}

/* ------------------------------------------------------------------------- */

/* The last status message, kept so it can be reasserted after anything that
   repaints over it -- the menu-bar redraw lands on top of this row. */
static char status_text[48];

static void set_status_text(const char *msg)
{
    int i;
    for (i = 0; i < (int)sizeof(status_text) - 1 && msg[i]; i++)
        status_text[i] = msg[i];
    status_text[i] = '\0';
}

static void write_status(const char *msg, int fg, int bg)
{
    int len = (int)strlen(msg);
    int col = (win_cols - len) / 2;
    if (col < 0) col = 0;

    fill_rect(0, status_row * 8, win_cols * 8, 8, bg);
    _cgfx_fcolor(MV_OUTPATH, fg);
    _cgfx_bcolor(MV_OUTPATH, bg);
    /* Text has to go out through cgfx's own buffered writer. printf() and a raw
       write() both reach the same path but bypass that buffer, and nothing ever
       appears -- which is what made this look like a positioning bug. curxy
       addresses character cells, not pixels. */
    _cgfx_curxy(MV_OUTPATH, col, status_row);
    cwrite(MV_OUTPATH, msg, len);
    Flush();
}

void bv_status(const char *msg)
{
    set_status_text(msg);
    write_status(status_text, reg_fg, reg_bg);
}

void bv_refresh_status(void)
{
    if (status_text[0])
        write_status(status_text, reg_fg, reg_bg);
}

/*
 * End-of-game banner: five blinks at half-second intervals, alternating normal
 * and reverse video. The reversal swaps the app's own foreground and background
 * rather than forcing black on white, so the flash reads against whatever
 * background the current screen type uses -- green in 16 colours, white in the
 * others. The message is left on screen afterwards.
 */
void bv_blink_result(const char *msg)
{
    int i;

    set_status_text(msg);

    for (i = 0; i < 5; i++) {
        write_status(status_text, reg_bg, reg_fg);   /* reversed */
        tsleep(HALF_SECOND);
        write_status(status_text, reg_fg, reg_bg);   /* normal */
        tsleep(HALF_SECOND);
    }
    write_status(status_text, reg_fg, reg_bg);
}
