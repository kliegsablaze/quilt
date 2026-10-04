#!/usr/bin/env bash
# Copies Quilt to a connected Move. No restart: the synth picker rescans the
# modules folder each time it opens. Files are uploaded beside the target and
# renamed over it, never written in place (see Ragtag's install.sh for the
# SIGSEGV that writing over a dlopen()'d .so causes).
#
#   scripts/install.sh            install
#   scripts/install.sh --remove   delete Quilt from the Move
set -euo pipefail

cd "$(dirname "$0")/.."

HOST="${MOVE_HOST:-ableton@move.local}"
REMOTE_DIR="/data/UserData/schwung/modules/sound_generators/quilt"

if [ "${1:-}" = "--remove" ]; then
    ssh "$HOST" "rm -rf '$REMOTE_DIR'"
    echo "Removed $REMOTE_DIR"
    exit 0
fi

[ -f dist/quilt/dsp.so ] || ./scripts/build.sh
if ! head -c 20 dist/quilt/dsp.so | od -An -tx1 |
     tr -d ' \n' | grep -Eq '^7f454c46.{28}b700'; then
    echo "dist/quilt/dsp.so is not an aarch64 ELF" >&2
    exit 1
fi

ssh "$HOST" "mkdir -p '$REMOTE_DIR'"
scp -q dist/quilt/dsp.so "$HOST:$REMOTE_DIR/.dsp.so.incoming"
scp -q dist/quilt/module.json "$HOST:$REMOTE_DIR/.module.json.incoming"
ssh "$HOST" "cd '$REMOTE_DIR' && chmod 755 .dsp.so.incoming && \
    mv -f .dsp.so.incoming dsp.so && \
    mv -f .module.json.incoming module.json && ls -l"
echo "Installed to $HOST:$REMOTE_DIR"
