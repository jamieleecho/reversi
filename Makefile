# reversi -- Reversi for Multi-Vue (CoCo 3 / NitrOS-9), built with MVKit.
#
# Everything is driven from this Makefile on the host:
#
#     make            # build build/reversi.os9
#     make run        # boot the disk image in MAME (needs a display)
#     make test       # screenshot regression tests in headless MAME
#     make shell      # interactive shell in the toolchain container
#     make clean
#     make help
#
# The C toolchain (cmoc, ToolShed, the PNG converters) exists only inside the
# jamieleecho/coco-dev image, so the build targets re-enter this same Makefile
# inside the container with INSIDE_COCO_DEV=1. MAME runs on the host, which is
# where the display and the CoCo 3 ROM set live.

APP   := reversi
# app.mk builds the type-8 launcher from SHORT; 5/6/7 are added alongside it.
SHORT := r08

# A workstation keeps MAME and its ROM set together under ~/Applications/mame.
# In CI mame is already on PATH and the ROMs are fetched into the workspace, so
# both are overridable. MAME_ROMPATH is spelled the way cmoc_os9's CI spells it.
# Prepending a MAME_DIR that does not exist is harmless -- the container's own
# mame is then found on PATH.
MAME_DIR     ?= $(HOME)/Applications/mame
MAME         ?= $(MAME_DIR)/mame
MAME_ROMPATH ?= $(MAME_DIR)/roms
MAME_FLAGS   ?= -speed 4 -window -skip_gameinfo -rompath $(MAME_ROMPATH) \
                -ext:fdc:wd17xx:0 525qd -autoboot_delay 1 -autoboot_command 'dos\n'

# Scenarios live one directory each under graphictest/scenarios/.
SCENARIOS := $(notdir $(wildcard graphictest/scenarios/*))

# Emulated-second budget per scenario. It must exceed the scenario's whole
# timeline or MAME hits -seconds_to_run part-way through and the suspended Lua
# dies with it. A desktop scenario spends roughly 45s booting, 35s waiting for
# the file window to enumerate the disk, and 35s launching, before it does
# anything -- so the default is generous.
GFX_BUDGET ?= 240

# Are we already inside the toolchain? On the host cmoc does not exist and the
# build re-enters the coco-dev container; in CI, make runs *inside* that image,
# where there is no docker to hop into. Detect the toolchain rather than making
# the caller pass a flag, so a plain `make` does the right thing in both places.
# Assigned with := so it holds the RESULT, not the expression: ifdef tests
# whether a variable has non-empty text without expanding it, so a recursive
# assignment here would read as "defined" on the host too and take the wrong
# branch. A command-line INSIDE_COCO_DEV=1 still overrides.
INSIDE_COCO_DEV := $(if $(shell command -v cmoc 2>/dev/null),1,)

ifdef INSIDE_COCO_DEV
# ============================== container side ===============================
# Runs where cmoc and friends are on PATH.

SRCS         := game.c board_view.c reversi.c

# cgfx screen type 8 = 4 bpp (320x192, 16 colours) -- a colour board. app.mk
# derives the image bit depth from this, so assets must match.
SCREEN_TYPE  := 8
WIN_W        := 40
WIN_H        := 25
WIN_BG       := 0
WIN_FG       := 3
MEM_SIZE     := 96

APP_ICON     := assets/icon-r08.png
ICON_PALETTE := assets/icon-palette.txt

CMOC_OS9_DIR := cmoc_os9
MVKIT_DIR    := mvkit
BASEIMAGE    := disks/NOS9_6809_L2_v030300_coco3_80d.os9

# Neither dependency is checked in (see .gitignore); both are cloned by the
# bootstrap below. app.mk comes from the MVKit checkout, so on a clean tree it
# does not exist yet -- hence `-include`. GNU make remakes missing included
# makefiles and then restarts itself, so the $(MVKIT_DIR)/app.mk rule below is
# what makes `make` work from a fresh clone in one shot.
-include $(MVKIT_DIR)/app.mk

# ---- bootstrap that app.mk leaves to the project ----------------------------
# Pinned to the commit mvdraw builds against, so a fresh checkout reproduces a
# known-good libc/libcgfx rather than whatever HEAD happens to be.
CMOC_OS9_COMMIT := 14b8f6bc983a1c694d36e3890f34b16c06a2af20

# The app binary needs cmoc_os9's libc/libcgfx and an installed MVKit. These are
# order-only so they don't force a relink on every rebuild; the sub-makes are
# themselves incremental.
$(BUILD)/$(APP): | libc libcgfx mvkit-install

# app.mk's AIF rule has no prerequisites, so edits to WIN_*/SCREEN_TYPE/MEM_SIZE
# would not regenerate it. Depend on this Makefile so those edits take effect.
$(AIF): Makefile

# ---- launcher variants ------------------------------------------------------
# One AIF per screen type, all naming the same executable: Multi-Vue fixes the
# screen type at launch, so a separate launcher is how the app is offered in
# each mode. The program itself adapts at run time from _cgfx_gs_styp().
#
# The width matters. An AIF asking for fewer columns than the screen has puts
# Multi-Vue into interactive window placement -- it waits for you to click the
# two corners before the app appears -- so each variant asks for its mode's full
# width: 80 columns for the 640-pixel modes, 40 for the 320-pixel ones.
#
# Type 8 is built by app.mk itself (SHORT := r08), so only 5, 6 and 7 are here.
VARIANTS := 5 6 7
win_w_5  := 80
win_w_6  := 40
win_w_7  := 80

VAIFS  := $(foreach t,$(VARIANTS),$(BUILD)/aif.r0$(t))
VICONS := $(foreach t,$(VARIANTS),$(BUILD)/icon.r0$(t))

$(BUILD)/aif.r0%: Makefile | $(BUILD)
	@printf '%s\n\nICONS/icon.r0%s\n%s\n%s\n%s\n%s\n%s\n%s\n' \
		'$(APP)' '$*' '$(MEM_SIZE)' '$*' '$(win_w_$*)' '$(WIN_H)' '$(WIN_BG)' '$(WIN_FG)' > $@.tmp
	@unix2mac -q -n $@.tmp $@
	@rm -f $@.tmp

$(BUILD)/icon.r0%: assets/icon-r0%.png $(ICON_PALETTE) | $(BUILD)
	png-to-mvicon $< $(ICON_PALETTE) $@

## Add the screen-type 5/6/7 launchers to the disk image
.PHONY: variants
variants: $(DSK) $(VAIFS) $(VICONS)
	@# -r replaces an existing file. Without it a rebuild that leaves the disk
	@# image up to date still re-runs this and dies with "error 218 file
	@# already exists", because the copies are not conditional on the disk
	@# having just been recreated.
	@for t in $(VARIANTS); do \
		os9 copy -r $(BUILD)/aif.r0$$t $(DSK),aif.r0$$t; \
		$(ATTR_DATA) $(DSK),aif.r0$$t; \
		os9 copy -r $(BUILD)/icon.r0$$t $(DSK),CMDS/ICONS/icon.r0$$t; \
		$(ATTR_EXEC) $(DSK),CMDS/ICONS/icon.r0$$t; \
	done
	@echo "Added launchers for screen types $(VARIANTS)"

all: variants

$(CMOC_OS9_DIR):
	git clone https://github.com/nitros9project/cmoc_os9.git $@
	cd $@ && git checkout $(CMOC_OS9_COMMIT)

# MVKit has no standalone repo -- it lives inside xmastree, so clone that and
# lift mvkit/ out of it, the same way mvdraw bootstraps.
$(MVKIT_DIR):
	rm -rf xmastree
	git clone https://github.com/jamieleecho/xmastree.git
	mv xmastree/$@ .
	rm -rf xmastree

# Target for the included makefile itself, so a missing app.mk triggers the
# clone and make restarts with it available.
$(MVKIT_DIR)/app.mk: | $(MVKIT_DIR)
	@test -f $@ || { echo "$(MVKIT_DIR) checkout has no app.mk"; exit 1; }

# What the graphics-test targets depend on. Inside the container that is the
# normal build; `build` cannot be used as an alias here because app.mk already
# has a rule for the build/ directory of that name, and the two collide with
# "Circular build/reversi <- build dependency dropped".
# Runs them all and reports at the end. Depending on the per-scenario targets
# would stop at the first failure, which is the wrong shape for a suite: one
# broken mode would hide the state of the other three.
# Run every scenario. The host-side `test` below is the documented entry point;
# this is the one it delegates to, so it carries no ## doc comment -- help is
# scanned from the file text and would otherwise list `test` twice.
test: all
	@fail=; \
	for s in $(SCENARIOS); do \
		rm -rf $(BUILD)/graphictest/$$s; mkdir -p $(BUILD)/graphictest/$$s; \
		PATH="$(MAME_DIR):$$PATH" \
		  DISK_SRC=$(DSK) ROMPATH=$(MAME_ROMPATH) \
		  SCENARIO_DIR=graphictest/scenarios/$$s \
		  RESULTS_DIR=$(BUILD)/graphictest/$$s \
		  BUDGET=$(GFX_BUDGET) \
		  graphictest/shared/runner.sh || fail="$$fail $$s"; \
	done; \
	if [ -n "$$fail" ]; then echo; echo "FAILED:$$fail"; exit 1; fi; \
	echo; echo "All scenarios passed."

# One explicit rule per scenario, generated. Deliberately NOT a `test-%:` pattern
# rule: GNU make does not apply pattern rules to phony targets, so `test-about-r05`
# would silently resolve to "Nothing to be done".
define SCENARIO_rule
.PHONY: test-$(1)
test-$(1): all
	@# Cleared first: the runner pairs MAME's auto-numbered PNGs to snapshot
	@# names by index, so leftovers from a previous run shift every name.
	@rm -rf $$(BUILD)/graphictest/$(1)
	@mkdir -p $$(BUILD)/graphictest/$(1)
	@PATH="$$(MAME_DIR):$$$$PATH" \
	  DISK_SRC=$$(DSK) ROMPATH=$$(MAME_ROMPATH) \
	  SCENARIO_DIR=graphictest/scenarios/$(1) \
	  RESULTS_DIR=$$(BUILD)/graphictest/$(1) \
	  BUDGET=$$(GFX_BUDGET) \
	  graphictest/shared/runner.sh
endef
$(foreach s,$(SCENARIOS),$(eval $(call SCENARIO_rule,$(s))))

.PHONY: libc libcgfx mvkit-install

## Build cmoc_os9's C library (libc)
libc: | $(CMOC_OS9_DIR)
	$(MAKE) -C $(CMOC_OS9_DIR)/lib all

## Build cmoc_os9's CoCo graphics library (libcgfx)
libcgfx: | $(CMOC_OS9_DIR)
	$(MAKE) -C $(CMOC_OS9_DIR)/cgfx all

# `install` copies MVKit's headers and libmvkit.a into cmoc's shared dir so
# `#include <mvkit/mvkit.h>` and -lmvkit resolve. Every `docker run` is a fresh
# container, so this must run on every build -- the image does not retain it.
mvkit-install: | $(MVKIT_DIR)
	$(MAKE) -C $(MVKIT_DIR) install

else
# ================================ host side ==================================

CONTAINER := ./coco-dev
BUILD     := build
DSK       := $(BUILD)/$(APP).os9

# Tests run INSIDE the container even when invoked from the host, so that local
# results match CI exactly. Goldens are pinned to the MAME build that blessed
# them, and the image's MAME (0.287) is not the same as a typical workstation's.
# The container only mounts this project, so the ROM set is staged into ./roms
# (gitignored) first -- CI fetches its own copy there instead.
.PHONY: stage-roms
stage-roms:
	@mkdir -p roms
	@test -f roms/coco3.zip || cp $(MAME_ROMPATH)/coco3.zip roms/ 2>/dev/null || { \
		echo "No CoCo 3 ROM set found at $(MAME_ROMPATH)/coco3.zip"; exit 1; }

## Run every screenshot regression scenario in headless MAME (in the container)
test: build stage-roms
	$(CONTAINER) make INSIDE_COCO_DEV=1 test MAME_ROMPATH=/work/roms

# Not declared .PHONY on purpose: make does not apply pattern rules to phony
# targets, and this needs to match test-<scenario>.
test-%: build stage-roms
	$(CONTAINER) make INSIDE_COCO_DEV=1 test-$* MAME_ROMPATH=/work/roms

.PHONY: all build run shell clean real-clean

## Build the bootable OS-9 disk image (default target)
all: build

## Compile and build the disk image inside the coco-dev container
build:
	$(CONTAINER) make INSIDE_COCO_DEV=1 all

## Boot the disk image in MAME on the host (needs a display)
run: build
	$(MAME) coco3 $(MAME_FLAGS) -flop1 $(DSK)

## Open an interactive shell in the toolchain container
shell:
	$(CONTAINER)

## Remove build artifacts
clean:
	$(CONTAINER) make INSIDE_COCO_DEV=1 clean

## Remove build artifacts and the cloned dependencies (cmoc_os9, mvkit)
real-clean: clean
	rm -rf cmoc_os9 mvkit xmastree

endif

# ========================= graphics tests (both sides) ========================
# Deliberately outside the host/container split: CI runs these INSIDE the
# coco-dev image, which already has mame, os9 and python with Pillow and numpy.
# While they lived in the host-only branch, `make test` in CI failed outright
# with "No rule to make target 'test'".

.PHONY: test bless help

## Bless the current captures as goldens: make bless SCENARIO=<name> CONFIRM=1
bless:
	@test -n "$(SCENARIO)" || { echo "usage: make bless SCENARIO=<name> CONFIRM=1"; exit 1; }
	@test "$(CONFIRM)" = "1" || { echo "refusing without CONFIRM=1"; exit 1; }
	@graphictest/shared/bless.sh graphictest/scenarios/$(SCENARIO)

## Show this help message
help:
	@awk 'BEGIN { \
		FS = ":"; \
		printf "Usage: make \033[36m<target>\033[0m\n\nTargets:\n"; \
	} \
	/^## / { doc = substr($$0, 4); next } \
	/^[a-zA-Z_][a-zA-Z0-9_%-]*:/ { \
		if (doc) printf "  \033[36m%-16s\033[0m %s\n", $$1, doc; \
		doc = ""; next; \
	} \
	{ doc = "" }' $(MAKEFILE_LIST)
