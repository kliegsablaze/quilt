#!/usr/bin/env bash
# Builds dsp.so for aarch64 (Ableton Move) into dist/quilt/.
# Runs the cross compiler from the fleet's builder image unless it is on PATH.
set -euo pipefail

cd "$(dirname "$0")/.."

IMAGE="${QUILT_BUILD_IMAGE:-forgetful-builder}"
CC_CMD="aarch64-linux-gnu-gcc -std=c11 -g -O2 -shared -fPIC -Wall -Wextra -Werror \
  -Isrc/dsp src/dsp/*.c -o dist/quilt/dsp.so -lm"

rm -rf dist
mkdir -p dist/quilt

if command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then
    sh -c "$CC_CMD"
else
    docker run --rm -v "$PWD:/build" -w /build "$IMAGE" sh -c "$CC_CMD"
fi

# help.json is what puts "Module Help" one jog from the controls.
cp src/module.json src/help.json dist/quilt/
echo "Built dist/quilt/"

cd dist
tar -czf quilt-module.tar.gz quilt/
cd ..
echo "Tarball: dist/quilt-module.tar.gz"
