# CLAUDE.md — reversi (Reversi for Multi-Vue)

`Flipper09.b09`, a Basic09 Reversi program from *The Rainbow*, converted into a
Multi-Vue application. Three stages, all landed:

1. **Dev environment** — Makefile, toolchain, disk build, MAME run + test.
2. **`Flipper09.b09` → `game.c`**, a literal port of the rules and the computer
   player, keeping the original's quirks.
3. **Multi-Vue app** — `reversi.c` (menus, dispatch) over `board_view.c`
   (palette, layout, drawing, hit-testing).

Platform knowledge started from `../mvdraw/CLAUDE.md` and `../cmoc_os9/CLAUDE.md`.
Everything below marked as a lesson cost real time to find; most failures on this
platform are **silent**, which is what makes them expensive.

## Toolchain

- `cmoc`, ToolShed and the PNG converters exist **only** inside the
  `jamieleecho/coco-dev` image. `os9` and `lwasm` happen to be on the host too,
  which is why `os9 dir` / `os9 ident` work outside the container.
- `./coco-dev` mounts **only this project** at `/work`, unlike mvdraw/xmastree's
  wrapper which mounts all of `/Users`. Everything the build needs is in-project.
- The image tag is pinned in `./coco-dev` and in `.github/workflows/build.yml` —
  **keep the two in step**. There is no `latest` tag; don't reintroduce one.
- **Every `docker run` is a fresh container.** `make -C mvkit install` writes
  MVKit's headers and `libmvkit.a` into `/usr/local/share/cmoc`, and that does
  not persist — the bootstrap runs every build. `mvkit/mv_defs.h: No such file`
  means the install step didn't run.
- Neither `cmoc_os9` nor `mvkit` is checked in; both are cloned by the
  bootstrap. MVKit has no repo of its own — it is lifted out of a clone of
  `xmastree`.
- **cmoc language limits:** 16-bit `int`; 32-bit `long`; **no floating point**;
  **no `<stddef.h>`** (use `0`/`(char *)0`, not `NULL`); strict `?:`
  pointer-vs-int typing. `exit()` is in `<unistd.h>`, not `<stdlib.h>`.
- Host `clang`/LSP flags every `#include <mvkit/mvkit.h>` as missing. Expected —
  those headers only exist in the container. Ignore.
- Verify a build: `os9 ident build/reversi.os9,CMDS/reversi` → CRC **Good**.

## Build system — lessons

- **`-include mvkit/app.mk` needs a rule for the file itself.** On a clean tree
  app.mk doesn't exist, and `-include` silently skips it, so every target it
  defines vanishes. The `$(MVKIT_DIR)/app.mk` rule is what makes GNU make clone
  it, restart itself and build in one shot. A rule for the *directory* is not
  enough — make only bootstraps a missing include if there is a rule for that
  file.
- **`ifdef` tests whether a variable has non-empty text, without expanding it.**
  `INSIDE_COCO_DEV ?= $(if $(shell ...),1,)` is therefore *always* "defined", and
  the host silently took the container branch. Use `:=` so the variable holds
  the result.
- **GNU make does not apply pattern rules to phony targets.** A `test-%:` rule
  listed in `.PHONY` resolves to "Nothing to be done". Generate explicit rules
  with `$(eval)` instead.
- **Variables used by `$(eval)`-generated rules must be defined before the
  branch that generates them.** `SCENARIOS` sat after the `ifdef`, so the
  container branch generated zero rules and `make test-about-r05` reported "No
  rule to make target".
- **Don't name a phony target `build`** — app.mk already has a rule for the
  `build/` directory, and the two collide with `Circular build/reversi <- build
  dependency dropped`. Use a variable for the dependency instead.
- **`os9 copy` fails with `error 218` if the file exists.** Anything that adds
  files to an already-built disk needs `os9 copy -r`, or a second `make` errors
  out. Running `make && make` in CI is what catches this.

## Boot and launch

The base image `disks/NOS9_6809_L2_v030300_coco3_80d.os9` **boots straight into
Multi-Vue (gshell)** — `sysgo` brings the desktop up ~55–60 s in, on its own.

- There is **no interactive text shell** on this disk. Typing a program name via
  MAME's natural keyboard does nothing, at any point. (`cmoc_os9`'s recipe disk
  deliberately leaves you at a shell prompt; ours does not.)
- A **startup-driven launch runs the program but stays on `/term`** —
  `_cgfx_select()` does not switch the GIME to the program's `/w` window. Fine
  for a text-mode smoke test, useless for a GUI.
- So the only way in is the way a person does it: **double-click the icon on the
  desktop.** That is automated now — see *Graphics tests*.
- **An AIF asking for fewer columns than the screen triggers interactive window
  placement**: Multi-Vue waits for you to click two corners, and until you do the
  screen is blank with the "illegal" pointer. It looks exactly like a hung app.
  Each launcher therefore asks for its mode's full width — 80 columns for the
  640-pixel types, 40 for the 320-pixel ones.

## Multi-Vue / cowin behaviours

- **Menus are click-to-open, then click the item.** *Not* press-drag-release.
  A press does open the menu, but dragging onto an item and releasing selects
  nothing, so the app looks wedged.
- **`mv_app_show_message_box` splits lines on CR/LF and needs `\r\n`.** With a
  bare `\r` the lines overprint. It **centres each line itself** within a
  26-column interior, so don't pad manually.
  - Known defect: it places lines one row apart but the rows are ~4 px while the
    font is 8 px, so three lines overlap vertically. Single-line boxes are fine.
    Lives in `mvkit/src/mv_app_message_box.c`, which is cloned at build time —
    fix it upstream in xmastree, not here.
- **Menu item titles are at most 14 characters** (`MIDSCR._mittl` is `char[15]`
  and needs the NUL). cowin draws an item at its own length rather than clipping
  to the menu width, so a menu narrower than its widest item renders ragged —
  give each menu a dash string at least as long as its longest title.
- **cowin draws menu-bar text in the window's `WIN_BG` register.** Setting
  `WIN_BG` to a content colour (green, for the 16-colour board) makes the menu
  text that colour — green on grey, i.e. invisible. Keep the AIF's background
  dark and paint the app's own background yourself.
- **Palette registers 0–3 are the chrome ramp**, darkest → lightest, and cowin
  uses them for the menu bar, dropdowns, shadows, 3D edges *and the screen
  background around the window*. In the 4-colour modes those are the only
  registers, so an app cannot have its own hues there without recolouring the
  chrome — which is why types 6/7 keep the standard greys and distinguish players
  by tone. 16-colour mode has 4–15 free.
- **Icons are 24×24 and 2 bpp** — four colours regardless of the screen type the
  AIF launches — and the desktop draws them from *its* palette, not the app's.
  Design them against the chrome ramp.

## cgfx drawing

- **Text must go through `cwrite()`.** `printf()` and `write()` reach the same
  path but bypass cgfx's buffered writer, and *nothing appears at all*. This cost
  hours: the graphics beneath the text drew perfectly, so it looked like a
  positioning bug. `_cgfx_curxy` addresses **character cells**, not pixels.
- A graphics window also needs a font selected (`_cgfx_font(path, GRP_FONT,
  FNT_S8X8)`) before it will draw text.
- **Get/Put buffers**: draw the prototype, `Flush()`, *then* `_cgfx_getblk`.
  Anything still queued when the capture happens is baked into the saved cell and
  reappears in every cell drawn from it. Clear and flush the background before
  rendering prototypes. Group = the pid, buffers numbered from 1.
- **Pixel aspect**: the 640-wide modes have pixels about half as wide as they are
  tall, so cells there are made twice as wide as they are tall to keep the board
  square and the discs round. Screenshots are misleading — a capture's raster
  pixels aren't square either, so a disc that looks like a flat pill in a PNG is
  round on screen. Divide raster x by 2 in the 320-wide modes before judging.
- `bar`/`box`/`circle` fill with the **foreground** colour, not the background.
- A primitive extending beyond the active working area may be **rejected
  entirely** rather than clipped — a "too big" fill that vanishes means the
  geometry left the area.
- Each cell only rules its own right and bottom edge, so a grid drawn cell-by-cell
  has **no top or left border** until something draws it.

## Graphics tests

Ported from `../cmoc_os9/graphictest`; the scenario API is unchanged (see its
README). What is specific here:

- **`graphictest/shared/mvdesk.lua` drives the Multi-Vue desktop with the mouse**
  — MAME exposes the CoCo joystick as absolute AD-stick axes (0..1023). It boots,
  opens the disk, double-clicks a named launcher, and picks menu items.
- **The pointer mapping and the landmark coordinates are a matched pair.**
  "Improving" the mapping by measuring it accurately moves every landmark and
  breaks everything; the landmarks are what is actually verified. Don't re-derive
  one without the other.
- **Clear the results directory between runs.** The runner pairs MAME's
  auto-numbered PNGs to snapshot names *by index*, so leftovers in `mame-snap/`
  shift every name and produce phantom failures.
- **`BUDGET` must exceed the whole scripted timeline.** Otherwise MAME hits
  `-seconds_to_run` while the Lua coroutine is suspended and **segfaults**; you
  get "scenario produced no snapshot manifest (Lua didn't run?)" plus a stray
  `0000.png` that is MAME's own end-of-run snapshot, not the scenario's.
- A desktop scenario owns all of its own timing — with no `PROGRAM` to type there
  is no `BOOT_WAIT` padding the front. Clicking before the desktop exists looks
  exactly like a wrong coordinate.
- **Goldens are pinned to the MAME build that blessed them.** The host and the
  container ship different MAME versions, so `make test` runs the tests *inside
  the container* even from the host, staging the ROM set into `roms/`. Bless from
  a container run or CI will disagree.
- `jvc_format: track count of 160 unsupported` in `mame.log` is a harmless probe
  message, not a failure.

## CI

- `.github/workflows/build.yml` compiles (`make && make`, twice on purpose) and
  runs the graphics tests, both inside the coco-dev image.
- **The image ships no CoCo 3 ROMs** — they're copyrighted. CI fetches
  `coco3.zip` from archive.org at run time and passes `MAME_ROMPATH`, the same
  way cmoc_os9's CI does.
- The Makefile detects whether `cmoc` is on `PATH` and only hops into Docker when
  it isn't, so a plain `make` works both on the host and inside the CI container.

## Working method

- **You cannot render the Multi-Vue GUI headlessly by reasoning about it.** Build
  it, run a scenario, look at the PNG. State the assumption explicitly, test it,
  and don't flip-flop between guesses.
- When something doesn't appear, find out *whether the code ran at all* before
  theorising about coordinates. Drawing a deliberately obvious marker (an
  inverted band, a coloured rectangle) settles in one run what an afternoon of
  hypotheses will not.
- Prefer reading how MVKit itself does a thing over inferring it from headers —
  `cwrite` and the menu interaction model were both sitting in
  `mvkit/src/mv_file_dialog.c`.
- The base disk ships `basic09` in `CMDS`, so the original `Flipper09.b09` could
  in principle be run in the emulator to diff against the port. It is a text
  listing, not a packed procedure, so this has never been done.
