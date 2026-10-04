#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

SCHWUNG="${SCHWUNG:-../../schwung}"
out="build/tests"
mkdir -p "$out"

cc -std=c11 -Wall -Wextra -Werror \
  -Isrc/dsp tests/test_probe.c src/dsp/probe.c -lm -o "$out/test_probe"

rc=0
"$out/test_probe" "$out" || rc=$?

if command -v node >/dev/null 2>&1; then
  node tests/plan.test.mjs "$out" "$SCHWUNG" || rc=$?
else
  echo "FAIL: node is not on PATH (tests/plan.test.mjs)"
  rc=1
fi

# A synth slot always dlopens <module>/dsp.so, whatever module.json says.
if ! grep -q '"dsp": "dsp.so"' src/module.json; then
  echo "FAIL: module.json dsp must be dsp.so (the chain host loads that name)"; rc=1
fi

exit $rc
