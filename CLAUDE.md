# CLAUDE.md — flipper (Reversi for Multi-Vue)

Converting `Flipper09.b09`, a Basic09 Reversi program, into a Multi-Vue app that
looks like Windows Reversi. Three stages:

1. **Dev environment** — Makefile, toolchain, disk build, MAME run + test. *(done)*
2. **`Flipper09.b09` → `reversi.c`**, producing identical results.
3. **Turn that into a Multi-Vue app.**

Much of the platform knowledge here is lifted from `../mvdraw/CLAUDE.md` and
`../cmoc_os9/CLAUDE.md`; this file keeps what applies plus what was learned here.

## Toolchain

- `cmoc`, ToolShed, and the PNG converters exist **only** inside the
  `jamieleecho/coco-dev` image. `os9` and `lwasm` also happen to be installed on
  the host, which is why disk inspection (`os9 dir`, `os9 ident`) works outside
  the container.
- `./coco-dev` mounts **only this project** at `/work`, unlike the wrapper in
  mvdraw/xmastree which mounts all of `/Users`. Everything the build needs lives
  inside the project, so the wider mount buys nothing.
- Image tag is pinned in `./coco-dev` (`COCO_DEV_IMAGE`, currently `0.85`).
  There is no `latest` tag locally — don't reintroduce one.
- `docker run -t` fails in a non-TTY shell; the wrapper adds `-t` only when
  stdin/stdout really are terminals.
- **Every `docker run` is a fresh container.** `make -C mvkit install` puts
  MVKit's headers and `libmvkit.a` into `/usr/local/share/cmoc`, and that does
  **not** persist — the bootstrap runs on every build. `mvkit/mv_defs.h: No such
  file or directory` means the install step didn't run.
- Neither `cmoc_os9` nor `mvkit` is checked in; both are cloned by the bootstrap.
  MVKit has no repo of its own — it is lifted out of a clone of `xmastree`.
  `app.mk` comes *from* the MVKit checkout, so it is `-include`d: on a clean tree
  it doesn't exist yet, and the `$(MVKIT_DIR)/app.mk` rule is what makes GNU make
  clone it, restart itself, and build in one shot. Without a rule for the
  included file specifically, `-include` would silently skip it and every target
  app.mk defines would vanish.
- **cmoc language limits:** 16-bit `int`; 32-bit `long` is available; **no
  floating point**; **no `<stddef.h>`** (use `0`/`(char *)0`, not `NULL`); strict
  `?:` pointer-vs-int typing (cast the null branch:
  `(argc==2)?argv[1]:(char*)0`). Basic09 uses floats freely — stage 2 has to
  render the engine in integers.
- Host `clang`/LSP flags every `#include <mvkit/mvkit.h>` as missing. Expected:
  those headers only exist inside the container. Ignore.
- Verify a build produced a valid module: `os9 ident build/flipper.os9,CMDS/flipper`
  → CRC must read **Good**.

## Make targets

Everything is driven from the host `Makefile`; build targets re-enter it inside
the container with `INSIDE_COCO_DEV=1`.

    make            # build build/flipper.os9
    make run        # boot it in MAME on the host (needs a display)
    make test       # every graphictest scenario, headless
    make bless SCENARIO=flipper CONFIRM=1
    make shell      # interactive container shell
    make clean / real-clean / help

## Boot and launch — the expensive lesson

The base image `disks/NOS9_6809_L2_v030300_coco3_80d.os9` **boots straight into
Multi-Vue (gshell)**. Its `startup` only links shell and merges
`SYS/std{fonts,ptrs,pats_*}`; `sysgo` brings up the desktop ~55–60s in, on its
own. Consequences:

- There is **no interactive text shell** on this disk. Typing a program name via
  MAME's natural keyboard does nothing at any point during boot — the keystrokes
  aren't echoed and nothing launches. This is unlike `cmoc_os9`'s recipe disk,
  whose startup deliberately leaves you at an OS-9 shell prompt.
- Normal use is therefore: `make run`, wait for the desktop, **double-click the
  app icon**. That is what the AIF (`aif.flp`) and icon (`CMDS/ICONS/icon.flp`)
  exist for, and it's why mvdraw/xmastree ship no automated graphics tests.
- For automated testing, `graphictest/startup` replaces the disk's startup and
  runs `flipper` directly. `startup` never returns, so `sysgo` never reaches
  gshell. The program runs and its stdout is captured.
- **But a startup-driven launch stays on `/term`** — `_cgfx_select()` does not
  switch the GIME to the program's `/w` graphics window. So this smoke test
  proves the module loads and executes; it does **not** capture Multi-Vue window
  chrome. Capturing a real Multi-Vue window will need either mouse automation to
  double-click the icon from the desktop, or a different launch path. Unsolved —
  it will matter in stage 3.

## Graphics tests

Ported from `../cmoc_os9/graphictest` (scenario API unchanged — see its README).
Local changes:

- `runner.sh` no longer force-overwrites the disk's `startup`; it does so only
  when `STARTUP_SRC` is set. Our base image's own startup already does the
  grfdrv merges a Multi-Vue app needs.
- The shim's post-`DOS` boot wait is `BOOT_WAIT` (was hard-coded 55s for
  cmoc_os9's longer recipe startup).
- `BUDGET` must exceed the **whole** scripted timeline — autoboot delay +
  `BOOT_WAIT` + the scenario's own waits. If it doesn't, MAME hits
  `-seconds_to_run` while the Lua coroutine is suspended and **segfaults**, and
  you get "scenario produced no snapshot manifest (Lua didn't run?)" plus a
  stray `0000.png` that is MAME's own end-of-run snapshot, not the scenario's.
  That combination is confusing enough to be worth recognising on sight.
- `jvc_format: track count of 160 unsupported` in `mame.log` is a harmless probe
  message — MAME tries JVC before settling on the right format. Not a failure.
- Goldens are pinned to the MAME build that blessed them (0.286 here). Re-bless
  after a MAME upgrade rather than chasing pixel diffs.

## cgfx / Multi-Vue display model

Carried over from mvdraw, still to be confirmed for this app's screen type:

- Screen type is set in the `Makefile` (`SCREEN_TYPE`); app.mk derives image BPP
  from it. **5** = 640×192 1bpp, **6/7** = 2bpp, **8** = 320×192 4bpp. We use 8
  for a colour board. Image assets must match the depth or colours come out wrong.
- **Pixel aspect is ~2:1** — vertical pixels are ~2× taller. A pixel-square
  circle renders as a tall oval; draw discs ~2:1 wide to look round. This matters
  for Reversi pieces.
- `bar`/`box`/`rbar`/`circle` fill with the **foreground** colour
  (`_cgfx_fcolor`), not the background.
- `_cgfx_cwarea` takes **character cells**, not pixels; it clips *and* shifts the
  origin, and re-bases the mouse. Set it only around drawing, never while
  reading the mouse.
- A primitive extending beyond the active working area may be **rejected
  entirely** rather than clipped — a "too big" fill that vanishes means the
  geometry left the working area.
- `mv_app_run` owns the loop: `pre_init`/`init`/`menu_actions`/
  `refresh_menus_action`/`application_action`. Menu enabled-state is read only on
  `mv_app_refresh_menubar()`.

## Working method

- **You cannot render the Multi-Vue GUI headlessly** (see above). Coordinate and
  API-semantics assumptions must be confirmed by a real `make run`. State the
  assumption, build, and have the user verify; don't flip-flop between guesses.
- The base disk ships `basic09` in `CMDS`, so stage 2's "identical results"
  check can run the original `Flipper09.b09` in the emulator and compare against
  the C port rather than reasoning about equivalence on paper.
