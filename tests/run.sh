#!/usr/bin/env bash
# Builds Quilt natively, drives it black-box through the v2 API, plans its
# pages with the host's own planner, and renders the example scripts.
#   SCHWUNG=<checkout> bash tests/run.sh
set -euo pipefail

cd "$(dirname "$0")/.."

SCHWUNG="${SCHWUNG:-../schwung}"
out="build/tests"
mkdir -p "$out"
CFLAGS="-std=c11 -O2 -Wall -Wextra -Werror -Isrc/dsp"

cc $CFLAGS tests/test_quilt.c src/dsp/*.c -lm -o "$out/test_quilt"
cc $CFLAGS tools/render.c src/dsp/*.c -lm -o build/render

rc=0
"$out/test_quilt" "$out" || rc=$?

if command -v node >/dev/null 2>&1; then
  node tests/plan.test.mjs "$out" "$SCHWUNG" || rc=$?
  node tests/help_lint.mjs || rc=$?
else
  echo "FAIL: node is not on PATH (tests/plan.test.mjs, tests/help_lint.mjs)"
  rc=1
fi

for script in tools/examples/*.txt; do
  build/render "$script" "$out/$(basename "$script" .txt).wav" || rc=$?
done

# A synth slot always dlopens <module>/dsp.so, whatever module.json says.
if ! grep -q '"dsp": "dsp.so"' src/module.json; then
  echo "FAIL: module.json dsp must be dsp.so (the chain host loads that name)"; rc=1
fi

exit $rc
