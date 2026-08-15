/*
 * reversi -- a straight C port of Flipper09.b09 (OTHELL09).
 *
 * Original Basic09 program by Stephen J. Page, Ottawa, Canada, published in
 * the December 1989 issue of The Rainbow (Copyright 1989 Falsoft, Inc.).
 *
 * This is deliberately a literal port, not a cleanup: the control flow, the
 * quirks, and the screen output are meant to match the original statement for
 * statement. Comments give the Basic09 line numbers. Things that look like
 * bugs are preserved and flagged rather than fixed -- stage 3 is where this
 * becomes a real Multi-Vue application.
 *
 * Notes on the mapping:
 *
 *   BASE 0        Basic09 DIM aa(10,10) is indices 0..9. The 0 and 9 rings are
 *                 never played on; they are sentinel borders holding 0, which
 *                 is what terminates the ray-walking REPEAT loops.
 *   GET #0,str    one raw byte, no line editing -> read(0,...)
 *   INPUT         line-edited -> readln(0,...)
 *   PUT #0,s      raw bytes -> written to stdout, so ordering with PRINT holds
 *   PRINT TAB(n)  1-based column, so tabto(n) emits n-1 spaces
 *   CHR$(2),c,r   the OS-9 /term cursor-position sequence: column+32, row+32
 *   INTEGER       16-bit, same as cmoc's int
 *
 * The board is drawn with aa(x0,y0) on screen row x0 (line 99-108), but the
 * incremental update in turn_on() (line 227) writes at row y1, column derived
 * from x1 -- transposed. This is invisible in play: the opening position
 * {(4,4),(5,5)} and {(4,5),(5,4)} and the whole va weight table are symmetric
 * under transpose, and every piece placed after the initial draw goes through
 * turn_on(), so the screen is self-consistent with x horizontal throughout.
 * Both are reproduced exactly as written.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* ---- DIM (lines 3-14) ---------------------------------------------------- */
static int sum4, sum2, sum1, sum3, col1, col2, total;
static int mv, ln, ll, player, xx, yy, cnter;   /* move, line */
static int x0, y0, dx, dy, x1, y1, x2, y2, x3, y3, tb;

static unsigned char scor[3];
static unsigned char xa[20], ya[20];            /* xa(20),ya(20): 0..19 */
static unsigned char aa[10][10];
static int va[10][10];

static char nameb[3][33];       /* name(3):STRING -- Basic09 default is 32 */
static char edge;               /* edge:STRING[1] */
static char nm[3];              /* nm(3):STRING[1] -- one character each */
static char left_[2];           /* left:STRING[2] */
static char note[37];           /* note:STRING[37] */
static char bottom[17];         /* bottom:STRING[17] */
static char blank[6];           /* blank:STRING[6] */
static char nout[6];            /* nout:STRING[6] */

static int nogo, good, point;   /* BOOLEAN */

/* va seed values, DATA lines 92-95, READ in x0-major order (lines 83-91). */
static const int dat[16] = {
    2000, -100,  50,  40,
    -100, -250, -21, -15,
      50,  -21,   5,   2,
      40,  -15,   2,   1
};

/* ---- output helpers ------------------------------------------------------ */

static void outs(const char *s) { fputs(s, stdout); }
static void outn(const char *s, int n) { fwrite(s, 1, n, stdout); }
static void outc(int c) { putchar(c); }
static void nl(void) { putchar('\n'); }

/* PRINT TAB(n): Basic09 columns are 1-based, and every TAB here is at the
   start of a line, so this is simply n-1 spaces. */
static void tabto(int n)
{
    int i;
    for (i = 1; i < n; i++)
        putchar(' ');
}

/* PRINT of an INTEGER. Basic09 emits no leading sign space -- that is what
   makes the board columns line up with turn_on()'s cursor arithmetic. */
static void outi(int v) { printf("%d", v); }

/* ---- input helpers ------------------------------------------------------- */

/* GET #0,str -- one raw byte, bypassing line editing. */
static char getb(void)
{
    char c;
    fflush(stdout);
    if (read(0, &c, 1) != 1)
        return 0;
    return c;
}

/* INPUT "prompt",var */
static void inputs(const char *prompt, char *dst, int size)
{
    int n;
    outs(prompt);
    fflush(stdout);
    n = readln(0, dst, size - 1);
    if (n < 0)
        n = 0;
    while (n > 0 && (dst[n - 1] == '\r' || dst[n - 1] == '\n'))
        n--;
    dst[n] = '\0';
}

/* VAL(str) where str is one character. Basic09 raises error 67 on a
   non-numeric string; -1 lets the caller take the error-67 path. */
static int val1(char c)
{
    return (c >= '0' && c <= '9') ? (c - '0') : -1;
}

/* ---- 50 REM TURN ON BOARD POSITION X1,Y1 (lines 226-231) ----------------- */
static void turn_on(void)
{
    outc(2);
    outc(x1 * 2 + tb + 31);
    outc(y1 + 32);
    outc((unsigned char)nm[player]);
    aa[x1][y1] = (unsigned char)player;
}

/* ---- 70 REM COMPUTER MOVES (lines 255-337) ------------------------------- */
static void computer_move(void)
{
    total = -10000;                                     /* 256 */
    xx = 0;                                             /* 257 */
    yy = 0;                                             /* 258 */

    for (x1 = 1; x1 <= 8; x1++) {                       /* 259 */
        for (y1 = 1; y1 <= 8; y1++) {                   /* 260 */
            if (aa[x1][y1] == 0) {                      /* 261 */
                cnter = 0;
                sum1 = va[x1][y1];                      /* 262 */

                for (dx = -1; dx <= 1; dx++) {          /* 263 */
                    for (dy = -1; dy <= 1; dy++) {      /* 264 */
                        if (aa[x1 + dx][y1 + dy] == 3 - player) {   /* 265 */
                            x2 = x1 + dx;               /* 266 */
                            y2 = y1 + dy;               /* 267 */
                            ll = 0;                     /* 268 */
                            do {                        /* 269 REPEAT */
                                x2 = x2 + dx;
                                y2 = y2 + dy;
                                ll = ll + 1;
                            } while (aa[x2][y2] == 3 - player);     /* 273 */

                            if (aa[x2][y2] == player) { /* 274 */
                                do {                    /* 275 REPEAT */
                                    x2 = x2 - dx;
                                    y2 = y2 - dy;
                                    sum1 = sum1 + va[x2][y2] * 2;
                                    cnter = cnter + 1;
                                    /* xa/ya are xa(20)/ya(20); Basic09 would
                                       raise error 55 past index 19. The most
                                       a single move can flip is 18. */
                                    xa[cnter] = (unsigned char)x2;
                                    ya[cnter] = (unsigned char)y2;
                                    ll = ll - 1;
                                    aa[x2][y2] = (unsigned char)player;
                                } while (ll != 0);      /* 284 */
                            }
                        }
                    }
                }

                if (cnter > 0) {                        /* 289 */
                    aa[x1][y1] = (unsigned char)player; /* 290 */
                    sum2 = -10000;                      /* 291 */

                    for (x2 = 1; x2 <= 8; x2++) {       /* 292 */
                        for (y2 = 1; y2 <= 8; y2++) {   /* 293 */
                            if (aa[x2][y2] == 0) {      /* 294 */
                                sum4 = va[x2][y2];
                                good = 0;               /* 295 */

                                for (dx = -1; dx <= 1; dx++) {      /* 296 */
                                    for (dy = -1; dy <= 1; dy++) {  /* 297 */
                                        if (aa[x2 + dx][y2 + dy] == player) {   /* 298 */
                                            x3 = x2 + dx;           /* 299 */
                                            y3 = y2 + dy;           /* 300 */
                                            sum3 = 0;               /* 301 */
                                            do {                    /* 302 REPEAT */
                                                sum3 = sum3 + 2 * va[x3][y3];
                                                x3 = x3 + dx;
                                                y3 = y3 + dy;
                                            } while (aa[x3][y3] == player);     /* 306 */

                                            if (aa[x3][y3] == 3 - player) {     /* 307 */
                                                sum4 = sum4 + sum3;
                                                good = 1;
                                            }
                                        }
                                    }
                                }

                                if (good) {             /* 314 */
                                    if (sum4 > sum2)    /* 315 */
                                        sum2 = sum4;
                                }
                            }
                        }
                    }

                    /* 321 REM COMPARE SCORE TO TOTAL */
                    if (sum1 - sum2 > total) {          /* 322 */
                        total = sum1 - sum2;
                        xx = x1;
                        yy = y1;
                    }

                    while (cnter > 0) {                 /* 327 */
                        aa[xa[cnter]][ya[cnter]] = (unsigned char)(3 - player);
                        cnter = cnter - 1;
                    }
                    aa[x1][y1] = 0;                     /* 331 */
                }
            }
        }
    }

    outi(xx);                                           /* 336 */
    outs(",");
    outi(yy);
    nl();
}

/* ---- main ---------------------------------------------------------------- */
int main(void)
{
    char str;
    char board[2];
    int i, n, nn;

    /* 5 */
    ln = 5;
    outc(12); nl();                                     /* 16 */
    tabto(12); outs("OTHELL09"); nl();                  /* 17 */
    tabto(9);  outs("COPYRIGHT 1985"); nl();            /* 18 */
    tabto(7);  outs("BY STEPHEN J. PAGE"); nl();        /* 19 */
    tabto(9);  outs("OTTAWA, CANADA"); nl();            /* 20 */
    nl();                                               /* 21 */

    left_[0] = 2; left_[1] = 32;                        /* 22 */

    /* note (23): position to column 0 row 13, blank 31 columns, position back.
       Every "PRINT note; ..." therefore clears line 13 and writes there. */
    note[0] = 2; note[1] = 32; note[2] = 45;
    for (i = 0; i < 31; i++)
        note[3 + i] = ' ';
    note[34] = 2; note[35] = 32; note[36] = 45;

    blank[0] = ' '; blank[1] = ' '; blank[2] = ' ';     /* 25 */
    blank[3] = 8;   blank[4] = 8;   blank[5] = 8;

    /* 26 ON ERROR GOTO 60 -- see the inline error checks at the move prompt. */
    nogo = 0;                                           /* 27 */
    /* 28 BASE 0 */

    inputs("FIRST PLAYERS NAME: ", nameb[1], sizeof(nameb[1]));    /* 29 */
    inputs("SECOND PLAYERS NAME: ", nameb[2], sizeof(nameb[2]));   /* 30 */
    if (nameb[1][0] == '\0')                            /* 31 */
        strcpy(nameb[1], "COCO");
    if (nameb[2][0] == '\0')                            /* 33 */
        strcpy(nameb[2], "coco");

    outs("COLOR DISPLAY? (Y/N) ");                      /* 35 */
    str = getb();                                       /* 36 */
    nl();                                               /* 37 */

    if (str == 'Y' || str == 'y') {                     /* 38 */
        for (x0 = 1; x0 <= 4; x0++) {                   /* 39 */
            outs("  "); outi(x0); outs(":");            /* 40 */
            outc(127 + 16 * x0);                        /* 41-42 */
        }
        nl();                                           /* 44 */

        /* 45 */
        ln = 10;
        do {                                            /* 46 REPEAT */
            outn(left_, 2); outc(43);                   /* 47 */
            outs("FIRST COLOR:");
            /* 48: the original reads GET #3 here, not GET #0. Path 3 is not
               open in a Basic09 program run from the shell, so as printed this
               raises an error every time and the handler loops back to 10.
               Treated as a listing typo for #0; flagged rather than silently
               "fixed" elsewhere. */
            str = getb();
            col1 = val1(str);                           /* 49 */
            if (col1 < 0) {                             /* error 67 */
                outn(note, 37);
                outs("INPUT NUMBERS ONLY!");
                col1 = 0;
            }
        } while (!(col1 < 5 && col1 > 0));              /* 50 */

        /* 51 */
        ln = 20;
        do {                                            /* 52 REPEAT */
            outn(left_, 2); outc(44);                   /* 53 */
            outs("SECOND COLOR:");
            str = getb();                               /* 54 */
            col2 = val1(str);                           /* 55 */
            if (col2 < 0) {                             /* error 67 */
                outn(note, 37);
                outs("INPUT NUMBERS ONLY!");
                col2 = 0;
            }
        } while (!(col2 < 5 && col2 > 0 && col1 != col2));   /* 56 */
        nl();                                           /* 57 */

        edge = (char)128;                               /* 58 */
        for (i = 0; i < 17; i++)                        /* 59-62 */
            bottom[i] = (char)195;
        nm[0] = (char)195;                              /* 63 */
        nm[1] = (char)(115 + 16 * col1);                /* 64 */
        nm[2] = (char)(115 + 16 * col2);                /* 65 */
    } else {                                            /* 66 */
        edge = ' ';                                     /* 67 */
        nm[0] = '+';                                    /* 68 */
        /* nm is STRING[1], so these assignments truncate to the first
           character of each name -- that is the playing piece in mono. */
        nm[1] = nameb[1][0];                            /* 69 */
        nm[2] = nameb[2][0];                            /* 70 */
    }

    do {                                                /* 72 REPEAT */
        /* 73 REM INITIALIZE ARRAY */
        for (x0 = 0; x0 <= 9; x0++)                     /* 74 */
            for (y0 = 0; y0 <= 9; y0++)                 /* 75 */
                aa[x0][y0] = 0;
        aa[4][4] = 1;                                   /* 79 */
        aa[5][5] = 1;                                   /* 80 */
        aa[4][5] = 2;                                   /* 81 */
        aa[5][4] = 2;                                   /* 82 */

        n = 0;
        for (x0 = 1; x0 <= 4; x0++) {                   /* 83 */
            for (y0 = 1; y0 <= 4; y0++) {               /* 84 */
                nn = dat[n++];                          /* 85 READ nn */
                va[x0][y0] = nn;                        /* 86 */
                va[x0][9 - y0] = nn;                    /* 87 */
                va[9 - x0][y0] = nn;                    /* 88 */
                va[9 - x0][9 - y0] = nn;                /* 89 */
            }
        }

        /* 96 REM PRINT OUT BOARD */
        tb = 8;                                         /* 97 */
        /* 98: Basic09's TAB(n) pads to column n counting what this PRINT
           statement has already emitted, and the CHR$(12) counts as one
           character even though it consumes no screen column. So the header
           starts one column left of a naive TAB(tb) -- which is exactly what
           lines it up with the board cells (column 2*y0+7) and with the bottom
           header on line 113. Without this it sits one column right. */
        outc(12); tabto(tb - 1);
        outs("   1 2 3 4 5 6 7 8"); nl();
        for (x0 = 1; x0 <= 8; x0++) {                   /* 99 */
            tabto(tb); outi(x0);                        /* 100 */
            outc((unsigned char)edge);                  /* 101-102 */
            for (y0 = 1; y0 <= 8; y0++) {               /* 103 */
                board[0] = nm[aa[x0][y0]];              /* 104 */
                board[1] = edge;
                outn(board, 2);                         /* 105 */
            }
            outi(x0); nl();                             /* 107 */
        }
        /* 109: only true in colour mode, where line was left at 20. From the
           second game onward line is 30 (set at the move prompt), so the
           bottom border is not redrawn -- an original quirk, preserved. */
        if (ln == 20) {
            tabto(tb + 1);                              /* 109 */
            outn(bottom, 17);                           /* 110 */
            nl();                                       /* 111 */
        }
        tabto(tb); outs("  1 2 3 4 5 6 7 8"); nl();     /* 113 */

        outn(left_, 2); outc(47);                       /* 114 */
        outs("ENTER 0,0 IF UNABLE TO MOVE");

        /* 115-119 / 120-124: each player's name written vertically down the
           side, one character per row, flanked by that player's piece. */
        for (x0 = 1; x0 <= (int)strlen(nameb[1]); x0++) {
            nout[0] = 2; nout[1] = 34; nout[2] = (char)(x0 + 31);
            nout[3] = nm[1]; nout[4] = nameb[1][x0 - 1]; nout[5] = nm[1];
            outn(nout, 6);
        }
        for (x0 = 1; x0 <= (int)strlen(nameb[2]); x0++) {
            nout[0] = 2; nout[1] = 60; nout[2] = (char)(x0 + 31);
            nout[3] = nm[2]; nout[4] = nameb[2][x0 - 1]; nout[5] = nm[2];
            outn(nout, 6);
        }

        player = 1;                                     /* 125 */
        mv = 4;                                         /* 126 */

        while (mv < 64) {                               /* 127 WHILE */
L30:                                                    /* 129 */
            ln = 30;                                    /* 128 */
            outn(left_, 2); outc(42 + player);          /* 130 */
            outs(nameb[player]);
            outs("'S MOVE (X,Y):");
            outn(blank, 6);

            if (strcmp(nameb[player], "COCO") == 0 ||   /* 132 */
                strcmp(nameb[player], "coco") == 0) {
                computer_move();                        /* 133 GOSUB 70 */
            } else {
                str = getb();                           /* 135 */
                xx = val1(str);                         /* 136 */
                if (xx < 0) {                           /* error 67 */
                    outn(note, 37);
                    outs("INPUT NUMBERS ONLY!");
                    goto L30;                           /* 65 -> line 30 */
                }
                outs(",");                              /* 137 */
                str = getb();                           /* 138 */
                yy = val1(str);                         /* 139 */
                if (yy < 0) {                           /* error 67 */
                    outn(note, 37);
                    outs("INPUT NUMBERS ONLY!");
                    goto L30;
                }
            }

            if (xx == 0 && yy == 0) {                   /* 141 */
                if (nogo)                               /* 142 */
                    goto L40;
                outn(note, 37);                         /* 143 */
                outs(nameb[player]);
                outs(" CAN'T GO"); nl();
                nogo = 1;                               /* 144 */
                player = 3 - player;                    /* 145 */
                goto L30;                               /* 146 */
            }

            if (aa[xx][yy] == 0) {                      /* 148 */
                point = 0;
            } else {
                outn(note, 37);                         /* 149 */
                outs("POSITION TAKEN");
                goto L30;                               /* 150 */
            }

            /* Basic09 error 55 (subscript out of range): a single digit can
               name the sentinel border (0 or 9), where aa(xx,yy) is 0 so the
               "position taken" test passes, and the neighbour scan below then
               indexes -1 or 10. The handler prints this and returns to 30. */
            if (xx < 1 || xx > 8 || yy < 1 || yy > 8) {
                outn(note, 37);
                outs("X OR Y IS OUT OF RANGE");
                goto L30;
            }

            for (dx = -1; dx <= 1; dx++) {              /* 152 */
                for (dy = -1; dy <= 1; dy++) {          /* 153 */
                    if (aa[xx + dx][yy + dy] == 3 - player) {   /* 154 */
                        x1 = xx + dx;                   /* 155 */
                        y1 = yy + dy;                   /* 156 */
                        ll = 0;                         /* 157 */
                        do {                            /* 158 REPEAT */
                            x1 = x1 + dx;
                            y1 = y1 + dy;
                            ll = ll + 1;
                        } while (aa[x1][y1] == 3 - player);     /* 162 */

                        if (aa[x1][y1] == player) {     /* 163 */
                            point = 1;
                            do {                        /* 164 REPEAT */
                                x1 = x1 - dx;
                                y1 = y1 - dy;
                                ll = ll - 1;
                                turn_on();              /* 168 GOSUB 50 */
                            } while (ll != 0);          /* 169 */
                        }
                    }
                }
            }

            if (!point) {                               /* 174 */
                outn(note, 37);
                outs("MOVE GETS NO POINTS!");
                goto L30;                               /* 175 */
            } else {                                    /* 176 */
                x1 = xx;                                /* 177 */
                y1 = yy;                                /* 178 */
                turn_on();                              /* 179 GOSUB 50 */
                outn(note, 37); outs(" ");              /* 180 */
                nogo = 0;                               /* 181 */
                mv = mv + 1;                            /* 182 */
                player = 3 - player;                    /* 183 */

                if (mv == 60) {                         /* 184 */
                    for (x0 = 1; x0 <= 8; x0++)
                        for (y0 = 1; y0 <= 8; y0++)
                            va[x0][y0] = 10;
                }

                if (x1 == 1 || x1 == 8) {               /* 191 */
                    if (y1 == 1 || y1 == 8) {           /* 192 */
                        va[x1][y1 - 1] = 500;
                        va[x1][y1 + 1] = 500;
                    } else {                            /* 195 */
                        va[x1][y1 - 1] = va[x1][y1 - 1] + 100;
                        va[x1][y1 + 1] = va[x1][y1 + 1] + 200;
                    }
                }
                if (y1 == 1 || y1 == 8) {               /* 200 */
                    if (x1 == 1 || x1 == 8) {           /* 201 */
                        va[x1 - 1][y1] = 500;
                        va[x1 + 1][y1] = 500;
                    } else {                            /* 204 */
                        va[x1 - 1][y1] = va[x1 - 1][y1] + 200;
                        va[x1 + 1][y1] = va[x1 + 1][y1] + 100;
                    }
                }
            }
        }                                               /* 210 ENDWHILE */

L40:                                                    /* 211 REM CALCULATE SCORE */
        scor[1] = 0;                                    /* 212 */
        scor[2] = 0;                                    /* 213 */
        for (x0 = 1; x0 <= 8; x0++)                     /* 214 */
            for (y0 = 1; y0 <= 8; y0++)                 /* 215 */
                scor[aa[x0][y0]]++;                     /* 216 */

        outn(note, 37);                                 /* 219 */
        outs(nameb[1]); outs(" WON "); outi(scor[1]);
        outs(", ");
        outs(nameb[2]); outs(" WON "); outi(scor[2]);
        nl();

        outs("DO YOU WANT ANOTHER GAME? ");             /* 221 */
        str = getb();                                   /* 222 */
        nl();                                           /* 223 */
    } while (!(str == 'n' || str == 'N'));              /* 224 UNTIL */

    return 0;                                           /* 225 END */
}
