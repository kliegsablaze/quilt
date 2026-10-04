#!/usr/bin/env bash
# Builds quiltprobe.so for aarch64 (Ableton Move) into dist/quiltprobe/.
# Runs the cross compiler from the fleet's builder image unless it is on PATH.
set -euo pipefail

cd "$(dirname "$0")/.."

IMAGE="${QUILT_BUILD_IMAGE:-forgetful-builder}"
CC_CMD="aarch64-linux-gnu-gcc -g -O2 -shared -fPIC -Wall -Wextra -Werror \
  -Isrc/dsp src/dsp/probe.c -o dist/quiltprobe/dsp.so -lm"

rm -rf dist
mkdir -p dist/quiltprobe

if command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then
    sh -c "$CC_CMD"
else
    docker run --rm -v "$PWD:/build" -w /build "$IMAGE" sh -c "$CC_CMD"
fi

cp src/module.json dist/quiltprobe/module.json
echo "Built dist/quiltprobe/"
