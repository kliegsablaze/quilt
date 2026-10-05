# Quilt

A Schwung instrument for Ableton Move: everything that can be played gently.
Thirty-eight soft instruments, all synthesised, no samples: felt and grand
pianos, electric pianos, organs, vibraphone, marimba, bells, harp, guitar,
bowed cello and violin, glass harmonica, singing bowl, flutes, clarinet and
choir. Velocity is how hard the hammer or mallet lands; pad pressure is the
bow, the breath and the swell.

Turn `TYPE` to pick an instrument; each starts on its own default sound, and
has two more factory presets (114 in all). The Main page is the same eight
knobs for everything; the Instrument page is relabelled for each one. Module
Help on the device explains every page and every instrument.

Needs Schwung 1.6.3 or later. See [DESIGN.md](DESIGN.md) for how it works and
why.

## Build and install

```bash
bash tests/run.sh
```

```bash
docker build -t quilt-builder -f scripts/Dockerfile .
```

```bash
QUILT_BUILD_IMAGE=quilt-builder scripts/build.sh
```

```bash
scripts/install.sh
```

`install.sh` copies the build to `ableton@move.local` (or `$MOVE_HOST`).
The tests plan the pages with the host's own planner, so they want a Schwung
checkout at `../schwung` (or `$SCHWUNG`).
