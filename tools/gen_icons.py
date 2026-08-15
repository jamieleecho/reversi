#!/usr/bin/env python3
"""Generate the four Multi-Vue launcher icons.

A Multi-Vue icon is 24x24 and 2 bits per pixel -- four colours, whatever screen
type the app itself runs in. The desktop draws it from ITS palette registers
0..3, which are the standard chrome ramp, so the icons are designed in black,
dark grey, light grey and white rather than in colour. assets/icon-palette.txt
maps those four RGB values onto indices 0..3 for png-to-mvicon.

Each icon shows the opening position: a 4x4 corner of the board with the four
starting discs in the middle. The four differ in how they render it, matching
the screen type each AIF launches:

    icon-r05  two colours only, board dithered   (screen type 5, 1 bpp)
    icon-r06  four colours, deliberately chunky  (screen type 6, 320 wide)
    icon-r07  four colours, smooth               (screen type 7, 640 wide)
    icon-r08  four colours with shadow and bevel (screen type 8, 16 colours)
"""
from PIL import Image

SIZE = 24
CELL = 6                      # 4 cells of 6 px fills the icon exactly

BLACK = (0, 0, 0)
DKGREY = (85, 85, 85)         # palette component 1,1,1
LTGREY = (170, 170, 170)      # palette component 2,2,2
WHITE = (255, 255, 255)

# The opening position over a 4x4 excerpt: player 1 on one diagonal, player 2
# on the other. (col, row) -> player.
OPENING = {(1, 1): 1, (2, 2): 1, (1, 2): 2, (2, 1): 2}


def new_image(bg):
    return Image.new("RGB", (SIZE, SIZE), bg)


def dither_cells(px, a, b):
    """Fill the whole icon with a 50% checker of two colours."""
    for y in range(SIZE):
        for x in range(SIZE):
            px[x, y] = a if (x + y) & 1 else b


def grid(px, colour):
    """Rule the 4x4 grid, including the outer border."""
    for i in range(0, SIZE, CELL):
        for k in range(SIZE):
            px[i, k] = colour
            px[k, i] = colour
    for k in range(SIZE):
        px[SIZE - 1, k] = colour
        px[k, SIZE - 1] = colour


def disc_pixels(radius):
    """Offsets of a filled disc of the given radius, as a set."""
    out = set()
    for dy in range(-radius, radius + 1):
        for dx in range(-radius, radius + 1):
            if dx * dx + dy * dy <= radius * radius + 1:
                out.add((dx, dy))
    return out


def blocky_pixels():
    """A deliberately chunky disc: a 4x4 square with the corners knocked off."""
    out = set()
    for dy in range(-2, 2):
        for dx in range(-2, 2):
            if abs(dx + 0.5) + abs(dy + 0.5) <= 2.5:
                out.add((dx, dy))
    return out


def draw_discs(px, shape, fill1, fill2, shadow=None):
    """Draw the opening discs, each clipped to the inside of its own cell.

    A 5-pixel disc exactly fills a 6-pixel cell's interior, so there is no room
    for an outline: without the clip the discs bled over the grid lines and the
    two dark ones merged into a single blob. They are told apart by tone against
    the board instead.
    """
    for (c, r), player in OPENING.items():
        cx, cy = c * CELL + CELL // 2, r * CELL + CELL // 2
        x0, y0 = c * CELL + 1, r * CELL + 1
        x1, y1 = c * CELL + CELL, r * CELL + CELL
        body = fill1 if player == 1 else fill2

        def put(x, y, colour):
            if x0 <= x < x1 and y0 <= y < y1:
                px[x, y] = colour

        if shadow is not None:
            for dx, dy in shape:
                put(cx + dx + 1, cy + dy + 1, shadow)
        for dx, dy in shape:
            put(cx + dx, cy + dy, body)


def icon_r05():
    """Two colours: a dithered board, a white disc and a black one."""
    im = new_image(WHITE)
    px = im.load()
    dither_cells(px, BLACK, WHITE)
    grid(px, BLACK)
    draw_discs(px, disc_pixels(2), WHITE, BLACK)
    return im


def icon_r06():
    """Four colours, chunky -- the 320-wide mode has visibly fat pixels."""
    im = new_image(LTGREY)
    px = im.load()
    grid(px, BLACK)
    draw_discs(px, blocky_pixels(), WHITE, BLACK)
    return im


def icon_r07():
    """Four colours, smooth round discs -- the 640-wide mode is finer."""
    im = new_image(LTGREY)
    px = im.load()
    grid(px, BLACK)
    draw_discs(px, disc_pixels(2), WHITE, BLACK)
    return im


def icon_r08():
    """Four colours with a drop shadow and a bevelled edge."""
    im = new_image(DKGREY)
    px = im.load()
    grid(px, LTGREY)
    draw_discs(px, disc_pixels(2), WHITE, BLACK, shadow=BLACK)
    for k in range(SIZE):                     # bevel: light top/left, dark below
        px[k, 0] = LTGREY
        px[0, k] = LTGREY
        px[k, SIZE - 1] = BLACK
        px[SIZE - 1, k] = BLACK
    return im


def main():
    for name, fn in (("r05", icon_r05), ("r06", icon_r06),
                     ("r07", icon_r07), ("r08", icon_r08)):
        path = "assets/icon-%s.png" % name
        fn().save(path)
        print("wrote", path)


if __name__ == "__main__":
    main()
