#!/usr/bin/env bash
#
# Promote the captures from the last `make test-<scenario>` run to goldens.
#
#     graphictest/shared/bless.sh graphictest/scenarios/<name>
#
# Reads from the run's results dir (build/graphictest/<name>/actual), which the
# Makefile pins to a stable path precisely so this is possible -- runner.sh on
# its own would mktemp a fresh dir every run.
#
# Goldens are pinned to the MAME build that produced them (see graphictest
# README), so re-bless after a MAME upgrade rather than chasing pixel diffs.
set -euo pipefail

SCENARIO_DIR=${1:?usage: bless.sh graphictest/scenarios/<name>}
SCENARIO_NAME=$(basename "$SCENARIO_DIR")
ACTUAL_DIR="build/graphictest/$SCENARIO_NAME/actual"
GOLDEN_DIR="$SCENARIO_DIR/goldens"

if [ ! -d "$ACTUAL_DIR" ]; then
  echo "ERROR: no captures at $ACTUAL_DIR -- run 'make test-$SCENARIO_NAME' first" >&2
  exit 1
fi

shopt -s nullglob
captures=("$ACTUAL_DIR"/*.png)
if [ ${#captures[@]} -eq 0 ]; then
  echo "ERROR: $ACTUAL_DIR contains no PNGs -- did the scenario snapshot anything?" >&2
  exit 1
fi

mkdir -p "$GOLDEN_DIR"
for png in "${captures[@]}"; do
  cp "$png" "$GOLDEN_DIR/$(basename "$png")"
  echo "  blessed $(basename "$png")"
done
echo "[$SCENARIO_NAME] ${#captures[@]} golden(s) updated in $GOLDEN_DIR"
