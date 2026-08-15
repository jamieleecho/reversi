# flipper — Reversi for Multi-Vue

A Reversi (Othello) game for the Tandy Color Computer 3, running under NitrOS-9
Level 2 as a [Multi-Vue](https://en.wikipedia.org/wiki/OS-9) application built
with MVKit — styled after the Reversi that shipped with early Windows.

It is a port of `Flipper09.b09`, a Basic09 program, to C.

## Origin and credits

`Flipper09.b09` is from the **December 1989 issue of *The Rainbow*** magazine,
carrying `Copyright 1989 Falsoft, Inc.` (Falsoft published *The Rainbow*).

The program was written by **Stephen J. Page**. Its own title screen reads:

```
        OTHELL09
     COPYRIGHT 1985
   BY STEPHEN J. PAGE
     OTTAWA, CANADA
```

The original Basic09 source is kept in this repo unmodified as `Flipper09.b09`,
both as the reference for the conversion and as a credit to its author.

This port also builds on:

- [MVKit](https://github.com/jamieleecho/xmastree) — the Multi-Vue application
  framework, which lives inside the `xmastree` repo
- [cmoc_os9](https://github.com/nitros9project/cmoc_os9) — libc and the `cgfx`
  graphics library for the cmoc 6809 C compiler
- [`jamieleecho/coco-dev`](https://hub.docker.com/r/jamieleecho/coco-dev) — the
  toolchain image
- The screenshot test harness, adapted from `cmoc_os9`'s `graphictest`

## Prerequisites

- **Docker**, running. The C toolchain (`cmoc`, ToolShed, the PNG converters)
  lives only in the `jamieleecho/coco-dev` image — nothing needs to be installed
  on the host to build.
- **MAME** at `~/Applications/mame`, with a CoCo 3 ROM set in
  `~/Applications/mame/roms`. Goldens here were blessed against MAME 0.286.
- **Python 3 with Pillow and NumPy**, for the screenshot comparator:
  `python3 -m pip install --user Pillow numpy`

Override the image tag with `COCO_DEV_IMAGE`, and MAME's location with
`MAME_DIR=...`.

## Build

```sh
make            # build build/flipper.os9
make run        # boot it in MAME (needs a display)
make test       # screenshot regression tests, headless
make help       # list every target
```

The first build clones both dependencies — `cmoc_os9` at a pinned commit, and
MVKit (lifted out of a clone of `xmastree`) — then builds libc, libcgfx and
MVKit. Neither is checked in, so this happens automatically on a clean checkout;
later builds are incremental. Everything host-side is driven from the
`Makefile` — build targets re-enter it inside the container, while MAME runs on
the host, where the display and ROMs are.

| Target | What it does |
| --- | --- |
| `make` / `make build` | Compile and produce the bootable disk image |
| `make run` | Boot `build/flipper.os9` in MAME |
| `make test` | Run every scenario under `graphictest/scenarios/` |
| `make test-<scenario>` | Run one scenario |
| `make bless SCENARIO=<name> CONFIRM=1` | Promote captures to goldens |
| `make shell` | Interactive shell in the toolchain container |
| `make clean` / `make real-clean` | Remove build output / also the `cmoc_os9` checkout |

### Running it

`make run` boots to the Multi-Vue desktop. **Launch the app by double-clicking
its icon** — this disk has no interactive text shell, so there is nothing to type
a command at. The launcher metadata (`aif.flp`) and icon are built onto the disk
for exactly this purpose.

### Testing

`make test` boots the disk in headless MAME, captures screenshots at points a
scenario chooses, and compares them against goldens checked into
`graphictest/scenarios/<name>/goldens/`. Failures drop `actual`/`golden`/`diff`
PNGs plus a single `<scenario>-failure.tar.gz` into `build/graphictest/<name>/`.

Note this is currently a **smoke test**: it launches the program from a
test-only `startup`, which runs it on `/term` rather than in a Multi-Vue window,
so it proves the module loads and runs but does not capture window chrome. See
`CLAUDE.md` for the details and why.

## Layout

```
flipper.c                 the application
Flipper09.b09             the original Basic09 program (unmodified)
Makefile                  host + container build
coco-dev                  toolchain container wrapper (mounts only this project)
assets/                   icon and palettes
disks/                    NitrOS-9 base disk image
mvkit/                    MVKit framework (cloned, not checked in)
cmoc_os9/                 libc + cgfx (cloned, not checked in)
graphictest/              screenshot test harness, scenarios and goldens
CLAUDE.md                 platform notes: cmoc limits, cgfx/Multi-Vue gotchas
```

## Status

Stage 2 of three. `reversi.c` is a literal C port of `Flipper09.b09` — same
control flow, same screen output, same quirks — running as a text-mode program
under NitrOS-9. Stage 3 turns it into a full Multi-Vue application.
