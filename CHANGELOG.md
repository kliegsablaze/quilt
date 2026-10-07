# Changelog

## 1.1.0

### Modulation page

A new page after Effects, with four modulators. MOD picks which one you are
editing.

- **KIND**: what the modulator follows.
  - Velocity: how hard each note was played.
  - MPE: pressure, slide (CC74) or pitch bend, per note. Has LAG for smoothing.
  - LFO: seven shapes, from Sine to Random and Drift. Turn RATE right of
    centre for tempo-synced divisions (8 bars to 1/64), left for free
    speeds (one cycle in 50 s up to 20 Hz).
  - Envelope: RISE while a note is held, FALL after it is let go.
- **AIM / DEPTH**, twice: two destinations per modulator. Depth goes either
  way. The destinations are Tone, Soft, Character, Roll, Decay, Sway, Space,
  Volume, Type, and the next modulator's rate or depth (1 into 2, 2 into 3,
  3 into 4, 4 into 1).
  - **Roll** is the mallets' roll speed on Marimba and Xylophone, from a
    quarter of the speed to four times it.
  - **Type** picks a different instrument for each new note, so velocity can
    put hard notes on another instrument.
- Velocity, MPE and Envelope act on each note separately. The LFO is shared.
- Modulation never moves the knobs themselves: every page keeps showing what
  you set.
- Factory presets and turning TYPE leave this page alone. Saved sounds keep it.

### Knob pictures

Every knob draws a small picture of what it does: a hammer, a mallet, a
reed, a room. The pictures change with the instrument. Holding a knob shows
its number. Needs Schwung 1.7.3 or later; older versions show the usual dials.

### Presets and sounds

- Each instrument's three presets are now clearly different from each other.
  Each variation changes four to eight knobs instead of one.
- Every default sound starts with Space at 0 (no room).
- Felt Lullaby and other soft pianos: less hammer noise, more string.
- Breathy flutes and ocarina: the breath now moves with the note instead of
  sitting on top of it.
- Clarinet Bright: less hiss. Kalimba: gentler buzz, off by default.
- Harps ring for less time.
- Presets within each instrument are in alphabetical order.
- All 114 presets re-levelled to the same loudness (-26.5 LUFS).

### Fixes

- Pad pressure no longer clicks when it takes over a held note (worst on
  Clarinet and Choir).
- Fading notes no longer end in a gritty whistle (quantization distortion).
  The output is now dithered, and still ends in silence.
- Instrument page titles no longer get cut off.

### Other

- Module Help covers the Modulation page and explains the organ's drawbar
  order.
- License changed to PolyForm Strict 1.0.0.

## 1.0.0

- First release: 38 instruments, 114 presets.
