# flipper -- Reversi for Multi-Vue (CoCo 3 / NitrOS-9), built with MVKit.
#
# Everything is driven from this Makefile on the host:
#
#     make            # build build/flipper.os9
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

APP   := flipper
SHORT := flp

ifdef INSIDE_COCO_DEV
# ============================== container side ===============================
# Runs inside coco-dev, where cmoc and friends are on PATH.

SRCS         := flipper.c

# cgfx screen type 8 = 4 bpp (320x192, 16 colours) -- a colour board. app.mk
# derives the image bit depth from this, so assets must match.
SCREEN_TYPE  := 8
WIN_W        := 40
WIN_H        := 25
WIN_BG       := 0
WIN_FG       := 3
MEM_SIZE     := 96

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

MAME_DIR   := $(HOME)/Applications/mame
MAME       := $(MAME_DIR)/mame
ROMPATH    := $(MAME_DIR)/roms
MAME_FLAGS := -speed 4 -window -skip_gameinfo -rompath $(ROMPATH) \
              -ext:fdc:wd17xx:0 525qd -autoboot_delay 1 -autoboot_command 'dos\n'

# Scenarios live one directory each under graphictest/scenarios/.
SCENARIOS := $(notdir $(wildcard graphictest/scenarios/*))

# Emulated-second budget per scenario, and how long to wait after typing DOS
# before typing the program name. The budget must exceed the whole scripted
# timeline (autoboot delay + boot wait + the scenario's own waits) or MAME hits
# -seconds_to_run mid-scenario and dies. Our base disk's startup is shorter than
# cmoc_os9's recipe startup, so the boot wait is well under its 55s default.
GFX_BUDGET    ?= 140
GFX_BOOT_WAIT ?= 45

.PHONY: all build run test bless shell clean real-clean help

## Build the bootable OS-9 disk image (default target)
all: build

## Compile and build the disk image inside the coco-dev container
build:
	$(CONTAINER) make INSIDE_COCO_DEV=1 all

## Boot the disk image in MAME on the host (needs a display)
run: build
	$(MAME) coco3 $(MAME_FLAGS) -flop1 $(DSK)

## Run every screenshot regression scenario in headless MAME
test: $(addprefix test-,$(SCENARIOS))

# One explicit rule per scenario, generated. Deliberately NOT a `test-%:` pattern
# rule: GNU make does not apply pattern rules to phony targets, so `test-flipper`
# would silently resolve to "Nothing to be done".
#
# The runner invokes `mame` and `os9` by name; MAME lives outside PATH here.
# RESULTS_DIR is pinned (runner.sh would otherwise mktemp) so captures survive
# the run and `make bless` can find them.
define SCENARIO_rule
.PHONY: test-$(1)
test-$(1): build
	@mkdir -p $$(BUILD)/graphictest/$(1)
	@PATH="$$(MAME_DIR):$$$$PATH" \
	  DISK_SRC=$$(DSK) ROMPATH=$$(ROMPATH) \
	  SCENARIO_DIR=graphictest/scenarios/$(1) \
	  RESULTS_DIR=$$(BUILD)/graphictest/$(1) \
	  BUDGET=$$(GFX_BUDGET) BOOT_WAIT=$$(GFX_BOOT_WAIT) \
	  STARTUP_SRC=graphictest/startup \
	  graphictest/shared/runner.sh
endef
$(foreach s,$(SCENARIOS),$(eval $(call SCENARIO_rule,$(s))))

## Bless the current captures as goldens: make bless SCENARIO=<name> CONFIRM=1
bless:
	@test -n "$(SCENARIO)" || { echo "usage: make bless SCENARIO=<name> CONFIRM=1"; exit 1; }
	@test "$(CONFIRM)" = "1" || { echo "refusing without CONFIRM=1"; exit 1; }
	@graphictest/shared/bless.sh graphictest/scenarios/$(SCENARIO)

## Open an interactive shell in the toolchain container
shell:
	$(CONTAINER)

## Remove build artifacts
clean:
	$(CONTAINER) make INSIDE_COCO_DEV=1 clean

## Remove build artifacts and the cloned dependencies (cmoc_os9, mvkit)
real-clean: clean
	rm -rf cmoc_os9 mvkit xmastree

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

endif
