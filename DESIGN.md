# Quilt — design

Everything that can be played gently.

Reflects **the proposal approved on 2026-10-04**, widened twice the same day on
request. The first request was "a wider palette, more soft sounds". The second was
"not just keys: strings, xylophones, whatever; the mother of all modules for soft
sounds". It has grown from four keyboards to **thirty-eight instruments in five
families**, built on six engines. **The modal engine is built** (build steps 3
and 4), and so are the banks and the FM voice (step 5) and the plucked half of
the waveguide engine (step 6): twenty-five instruments play, with the shared
effects, measured on the Move. They are Felt Upright, Una Corda Grand, Electric
Grand, Clavichord, Tine Piano, Reed Piano, Celesta, Toy Piano, Tonewheel Organ,
Flute Organ, Glass E.Piano, String Ensemble, Vibraphone, Marimba, Xylophone,
Glockenspiel, Tubular Bells, Handbells, Handpan, Tongue Drum, Kalimba / Music
Box, Harp, Nylon Guitar, Hammered Dulcimer and Pizzicato. The other
thirteen instruments have their
pages and labels in place, but **`TYPE` does not offer them until their engine is
built**, so nothing on the Move is a stand-in. Each instrument arrives with its own
default sound on every page (see *Every instrument starts beautiful*).

**Module ID:** `quilt` — slot abbreviation `QLT`
**Component type:** `sound_generator`. It lives in a Signal Chain synth slot. It is
played from the pads or external MIDI, with knobs and the jog wheel only.
**Host:** designed against **Schwung v1.7.2**, and checked against **v1.7.3**, the
latest release (2026-10-04), which changes nothing Quilt uses. The device still runs
1.6.3. Everything Quilt depends on is in both versions (see *Host
check*), so the catalog `min_host_version` can be 1.6.3.

Forgetful, Wayward and Ragtag are effects. Quilt is the fleet's first instrument.
It makes sound from a key number, how hard the key was struck, and how hard the pad
is still being pressed.

**The name is the material, as with Ragtag.** A quilt is two things at once. It is
soft: padding you sink into, the felt hammers, yarn mallets, fingertips, horsehair
and breath that every instrument here is played with. It is also made of many
unmatched pieces sewn into one: thirty-eight instruments, six engines and forty
years of papers, stitched under one set of eight knobs. Ragtag is a kit made of
whatever it caught. Quilt is the same idea made cosy: odds and ends, chosen with
care and sewn together.

It is the fleet's first noun. The others are adjectives describing what a module
does or is made of; *quilt* names the object outright, because the object is the
point. Each instrument is a patch, the families are the blocks, and the shared
engines are the stitching. (Chosen 2026-10-04 over *Tender*, which the user
disliked, and over *Plush*, *Dulcet*, *Sheepish*, *Patchwork*, *Quilted* and
*Woolgathering*. *Hushed* was ruled out because the catalog already has `hush1`.)

Every instrument here grows rounder, darker and longer the more gently it is
played, because that is what soft physical contact does.

## The idea

Every sound in this module is made by three things. **Something excites, something
rings, something listens.**

- **Excite:** strike, pluck, bow, rub or blow.
- **Ring:** a string, a bar, a plate or shell, an air column, a reed, a spinning
  wheel.
- **Listen:** a soundboard or body, a resonator tube, a pickup, a rotating speaker,
  a room.

A felt piano is *strike → string → soundboard*. A vibraphone is *strike → bar →
tube, with a fan*. A cello is *bow → string → body*. A glass harmonica is *rub →
glass shell → air*. An ocarina is *blow → a single cavity → air*.

So Quilt is **a small set of exciters, resonators and listeners, and thirty-eight
curated recipes that combine them.** It is not thirty-eight synths. That is what
makes it buildable, and what keeps it affordable on the device.

**Softness comes from the contact, not from a filter.** A felt hammer, a yarn mallet
and a fingertip are springs that stiffen as they are squeezed (Stulov; Chaigne &
Askenfelt; Avanzini & Rocchesso). Struck gently, they stay in contact longer and
cannot excite the high modes, so the tone is round. Struck hard, they are brief and
stiff, so the tone is bright.

The same holds for the self-sustaining instruments. A bow with light pressure, far
from the bridge, gives the veiled *flautando* tone (McIntyre, Schumacher & Woodhouse;
Woodhouse 2014). A gentle breath across a flute's edge gives a fundamental-heavy,
breathy tone (Verge; Cook).

Quilt computes these contacts, so velocity and pressure change the timbre the way
they do on the real instruments.

**Nothing is sampled.** There are no sample files, impulse responses or ROMs. It
loads instantly, it is small, and it needs no loader thread. (Rejected: *sampling*.
The SF2, SFZ and JV modules already do it, and the files are too large for the
brief.)

## What already exists, and why this is still worth building

The catalog has **Wurl**, a physically modelled Wurlitzer 200A ported from
OpenWurli (GPL-3.0). It has **Fizzik**, a general physical-modelling synth with
coupled resonators. It has **Chonk**, a waveguide electric bass.

It has **no acoustic piano, no tonewheel organ, no Rhodes, no mallets, no bowed or
plucked acoustic strings, no flutes, no bells or handpan, and no choir.** Fizzik
gives you the parts. Quilt gives you the finished, curated instruments, all soft,
under one set of eight knobs.

Quilt's reed piano overlaps Wurl on purpose; the user confirmed keeping it on
2026-10-04. It is written from the papers, and
**no OpenWurli code is read into it or copied**, because that code is GPL. It leans
quiet rather than reproducing the 200A amplifier circuit.

## Playing it with the Move

The Move's pads sense **velocity and continuous pressure** (polyphonic aftertouch).
Pressure is what makes the bowed and blown instruments playable at all, because a
cello or a flute needs a hand on it for the whole note. Quilt uses the physical
meaning of each input, never a gimmick.

| Input | Struck and plucked | Bowed and rubbed | Blown and voice | Organs and machines |
|---|---|---|---|---|
| Velocity | Hammer, mallet or finger speed | Bow attack (the first bite) | Tonguing and onset | Key click and percussion |
| Pad pressure | Nothing, except clavichord *Bebung* pitch | **Bow pressure**, so you swell by pressing | **Breath pressure** | Swell; harmonium bellows |
| CC64 sustain (external) | Dampers up | Nothing | Nothing | Nothing |
| CC1 mod wheel | Nothing | Vibrato | Vibrato | Leslie slow and fast |

Move's clips record pad pressure, so a sequenced part keeps its swells. With no
pressure at all (external MIDI without aftertouch, or a part recorded without it), the
sustained instruments follow a **velocity-shaped automatic swell**, so they still
sound right. `PRESS` on the instrument page chooses pad pressure, auto swell, or a
blend of the two.

## The thirty-eight instruments

Each family's table names, per instrument, the engine it uses, its **CHAR** (the one
signature control on the Main page: the word the cell shows, then what it does), and
the papers it rests on. **No CHAR repeats a knob already in reach.** Where an
instrument's obvious signature was really `SOFT` (a harp's nail, a xylophone's
mallet) or a knob on its own page (`BODY`, `SPOT`), it gets a different one. The engines are
described after the tables.

### Keys and organs (13)

| Instrument | Engine | The soft character | CHAR | Built on |
|---|---|---|---|---|
| **Felt Upright** | Modal | Moderator felt strip, close-miked (Frahm, *Felt*) | FELT, felt strip thickness | Bank–Zambon–Fontana; Stulov; Weinreich |
| **Una Corda Grand** | Modal | Soft pedal: hammer hits 2 of 3 strings, then 1; the unstruck string sings through the bridge | HUSH, soft pedal: 3 → 2 → 1 strings | Weinreich; Bank–Zambon–Fontana |
| **Electric Grand** | Modal | CP-70-style: strings heard through bridge pickups, no soundboard | TWANG, bridge pickups | Bank–Zambon–Fontana |
| **Clavichord** | Waveguide | The tangent stays on the string, so pad pressure bends pitch (*Bebung*, C.P.E. Bach) | BEND, *Bebung*: pressure bends the pitch | Välimäki–Laurson–Erkut |
| **Tine Piano** | Modal + pickup | Rhodes-style: glassy when played soft, barking when played hard | BARK, tine against pickup | Gabrielli et al.; Falaize–Hélie; Pfeifle |
| **Reed Piano** | Modal + pickup | Wurlitzer-style, turned down in a small room | BITE, pickup gap | Pfeifle |
| **Celesta** | Modal | Steel plates over wooden boxes (*Sugar Plum Fairy*) | BELL, upper plate modes | Doutaut–Matignon–Chaigne |
| **Toy Piano** | Modal | Struck clamped rods (Cage, *Suite for Toy Piano*) | BELL, upper rod modes | Beam theory |
| **Tonewheel Organ** | Banks | 91 shared wheels, scanner chorus, Leslie, flute drawbars | LUSH, scanner chorus, off → C3 | Pekonen et al.; Werner et al.; Smith et al.; Herrera et al. |
| **Flute Organ** | Banks | Chamber organ flue stops with chiff, plus tremulant | PUFF, the chiff starting each note | Castellengo |
| **Harmonium** | Air | Free reeds and bellows (Nico, *The Marble Index*) | BEAT, a sharp second rank (*céleste*) | Puranik–Scavone |
| **Glass E.Piano** | FM | The 1980s FM electric piano, nearly a sine when soft | GLASS, bell-pair index | Chowning |
| **String Ensemble** | Banks | Solina/Eminent string machine (Jarre, *Oxygène*) | LUSH, ensemble chorus depth | Raffel–Smith |

### Mallets, bells and metal (9)

| Instrument | Engine | The soft character | CHAR | Built on |
|---|---|---|---|---|
| **Vibraphone** | Modal | Bars tuned 1 : 4 over tubes opened and closed by a motor-driven fan | MOTOR, fan speed | Bar tuning; Doutaut et al. |
| **Marimba** | Modal | Rosewood bars, 1 : 4 : 10, soft yarn mallets (Reich, *Music for 18 Musicians*) | ROLL, held notes are rolled; press to roll faster | Bar tuning; Avanzini–Rocchesso |
| **Xylophone** | Modal | Tuned 1 : 3, played with rubber or yarn instead of hard plastic | ROLL, as Marimba | Bar tuning |
| **Glockenspiel** | Modal | Steel bars, free-bar ratios 1 : 2.76 : 5.40, brass mallets wrapped soft | BELL, upper bar modes | Beam theory |
| **Tubular Bells** | Modal | The strike note is a virtual pitch heard from modes 4–6 | MUTE, hand damping | Rossing |
| **Handbells** | Modal | Hum, prime, tierce, quint, nominal (0.5 : 1 : 1.2 : 1.5 : 2) | MINOR, the minor-third partial (tierce) | Bell partial tables |
| **Handpan** | Modal + cavity | Notes tuned to octave and twelfth, with the deep *ding* coupled to the body's air (Helmholtz) resonance | RING, the tuned octave and twelfth | Morrison & Rossing |
| **Tongue Drum** | Modal + cavity | Steel tongues, clamped cantilevers, over a cavity, played with mallets or fingers | RING, the tuned octave and twelfth | Beam theory; Rossing |
| **Kalimba / Music Box** | Modal | Plucked tines; `BUZZ` adds the mbira's rattling buzzers, and zero gives a music box | BUZZ, the mbira's buzzers | Beam theory |

### Strings, plucked, struck and bowed (7)

| Instrument | Engine | The soft character | CHAR | Built on |
|---|---|---|---|---|
| **Harp** | Waveguide | Gut and nylon, plucked with the fingertip, with a long ring | BLOOM, the other strings ringing in sympathy | Karjalainen–Välimäki–Tolonen; commuted body |
| **Nylon Guitar** | Waveguide | Fingerstyle, thumb near the soundhole | BLOOM, as Harp | Karjalainen–Välimäki–Tolonen; Jaffe–Smith |
| **Hammered Dulcimer** | Modal | Felt-covered beaters on courses of 2–4 strings, no dampers | BLOOM, the undamped courses ringing in sympathy | Bank–Zambon–Fontana; Weinreich |
| **Pizzicato** | Waveguide | Plucked cello and viola, finger pad, warm body | DEEP, viola → cello | Karjalainen–Välimäki–Tolonen |
| **Solo Cello** | Waveguide + bow | *Sul tasto*, light bow; pressure is the pad | VEIL, bow toward the fingerboard (*sul tasto*) | Smith; Serafin–Smith–Woodhouse; McIntyre et al. |
| **Solo Violin** | Waveguide + bow | *Flautando*: fast, light, over the fingerboard | VEIL, as Solo Cello | Same as Solo Cello; Woodhouse 2014 |
| **String Section** | Waveguide + bow | Three bowed players per note, each slightly different, through one shared hall body | WIDTH, spread between players | Same as Solo Cello |

### Glass and bowls, rubbed and bowed (3)

| Instrument | Engine | The soft character | CHAR | Built on |
|---|---|---|---|---|
| **Bowed Vibes** | Banded | A bow on a vibraphone bar: pure, endless | GRIP, the bow's friction | Essl & Cook |
| **Glass Harmonica** | Banded | A wet finger on spinning glass (Franklin's instrument) | WET, slip ↔ stick | Essl & Cook |
| **Singing Bowl** | Banded + modal | Rubbed or struck; the two near-degenerate modes beat slowly | BEAT, mode split | Essl & Cook |

### Breath and voice (6)

| Instrument | Engine | The soft character | CHAR | Built on |
|---|---|---|---|---|
| **Flute** | Air | Jet against an edge, open tube; breathy when soft | COVER, the lip covering more of the hole | Verge; Cook (STK) |
| **Pan Flute** | Air | Stopped pipes, so odd harmonics, with a soft chiff | PUFF, the chiff starting each note | Verge; Cook |
| **Ocarina** | Air | A jet driving a single cavity, so almost a pure tone | PURE, nearer a pure tone | Verge; Cook |
| **Recorder** | Air | Fipple flute: steady, sweet, low wind | PUFF, the chiff starting each note | Verge; Cook |
| **Clarinet** | Air | Low *chalumeau*, soft reed; cylindrical bore, so odd harmonics | REED, reed stiffness | McIntyre et al. |
| **Choir** | Formant | "Ooh" and "aah", three singers per note | VOWEL, oo → oh → ah | Klatt |

### Considered and set aside

- **Non-pitched percussion** (brushes, shakers). Quilt is for played, pitched
  sounds. Drums belong in a drum module.
- **Clavinet** and **combo organs** (Farfisa, Vox). They are well researched
  (Gabrielli, Välimäki et al. 2012–13 for the Clavinet) but bright and spiky, not
  soft.
- **Hohner Pianet.** No paper covers it, and the reed piano plus kalimba presets
  get close.
- **Mellotron.** It plays back tape, so it means samples.
- **Sung words.** Consonants need a full speech synthesiser and phoneme control the
  Move can't give. The choir sings vowels only.
- **Neural and DDSP models** (Renault, Mignot & Roebel 2022; Simionato & Fasciani
  2025). They need trained weights, sound like one recording, and are heavy for the
  device. We borrow their inharmonicity and detuning formulas, not their networks.
- **Full finite-difference simulation** (Chaigne & Askenfelt; Chabassier;
  Desvages and the NESS project for bowed strings). This is the reference standard,
  and many times too expensive at 16 voices. Modal synthesis, waveguides and banded
  waveguides are each its real-time stand-in.
- **A free "patch any exciter into any resonator" mode** like Chromaphone or
  Fizzik. It would be possible, because the engine is built that way, but it would
  mean eight knobs controlling an open-ended matrix. Fizzik already offers that.
  Quilt's value is curation; the user confirmed curated on 2026-10-04. Recorded so
  it is a deliberate choice, and revisitable after 1.0.

## The engines

There are six engines. Each is one C file with one paper trail. Instruments are
tables and defaults on top of them.

### 1. Modal: struck and plucked solids, and struck strings

This is the workhorse. It covers seventeen instruments: pianos, mallets, bells, handpan
and kalimba.

- **What it is.** A ringing object is a set of decaying sine tones (modes). Each
  is a Mathews–Smith complex one-pole, y ← r·e^{jω}·y + x: four multiplies per
  mode per sample. Frequency and decay can change every sample without clicks,
  which the vibraphone motor, *Bebung*-like bends and stretch tuning all need.
  Modes are stored as structure-of-arrays and run four at a time with NEON.
- **Where the modes sit.** This depends on the object:
  - **Strings:** f_k = k·f₀·√(1+B·k²). This is Bank, Zambon & Fontana's
    real-time modal piano.
  - **Clamped bars:** toy-piano rods, kalimba and tongue drum, at
    1 : 6.27 : 17.55.
  - **Tines and reeds:** cantilevers too, pulled higher. The tine is one prong
    of an asymmetric fork, 1 : 7.1 : 18.6; its second mode is Gabrielli et al.'s
    quick, bright *tink* and dies within a tenth of the note. The tonebar, its
    other prong, is the resonator on f₀ and carries the sustain. The reed, with
    the solder blob that tunes it, is 1 : 7.3 : 21 and shorter-lived. Its
    `BODY` is the same resonator on f₀: the reed bar's share.
  - **Free bars:** glockenspiel and celesta, at 1 : 2.76 : 5.40.
  - **Undercut tuned bars:** vibraphone 1 : 4, marimba 1 : 4 : 10,
    xylophone 1 : 3.
  - **Bells:** from bell-partial tables. Handbells: prime, nominal, hum,
    tierce, quint, 1 : 2 : 0.5 : 1.2 : 1.5, the hum ringing longest.
  - **Tubes:** tubular bells, whose modes rise as (2n+1)², 9 : 25 : 49 : 81 :
    121 : 169. Modes 4, 5 and 6 stand nearly 2 : 3 : 4, so the ear hears a
    strike note an octave below mode 4 that no mode sounds (Rossing). That
    strike note is the played pitch.
  - **Handpan and tongue drum:** the fundamental, octave and twelfth tuned,
    weaker untuned modes between, and the body's air as one Helmholtz mode
    (90 Hz in the handpan, 140 Hz in the tongue drum), driven most by the
    notes nearest it.
  - **Each table is the instrument's own** (`modal.c`): its mode ratios,
    levels and relative decays, its mallet (mass, felt stiffness K and
    exponent p), and the bar's local stiffness. The glockenspiel sounds two
    octaves above the key, and the celesta, xylophone and toy piano one, as
    their parts are written, so each sits where it should under the pads.
  - **Handpan:** octave and twelfth tuning, after Morrison & Rossing.
- **Losses.** Each mode loses energy at b₁ + b₃·ω² (Chaigne & Askenfelt's loss
  model). `DECAY` scales both terms, and dampers raise them.
- **Contact.** One moving mass hits a felt (or yarn, rubber or fingertip) spring
  with hysteresis, F = K·cᵖ, after Stulov and Avanzini & Rocchesso. It is
  computed sample by sample only during the 1–5 ms of contact. Its force pulse
  drives the modes, weighted by the mode shape at the strike point. **`SOFT` sets
  K.** What the felt pushes against:
  - **A string** first gives way like a resistance, 2Z (Z its wave impedance),
    because the push leaves as a wave both ways. The half sent toward the near
    end (agraffe or capo) **returns inverted after x₀/f₀ seconds and throws the
    hammer off.**
  - **A bar** is a local spring with damping, stiffer for higher bars.
- **Checked against Chaigne & Askenfelt, as this document required** (build
  step 3). The resistive load alone failed: the string only absorbed, the
  hammer never rebounded, and contact ran 5–10 ms, *longer* for harder blows.
  With the near-end return, their C4 hammer (2.97 g, K 4.5·10⁹, p 2.5) gives
  contact of 2.7 ms at 0.5 m/s falling to 2.3 ms at 4 m/s, and peak forces of
  2–17 N. C2 gives 3.5–5 ms and C7 gives 0.9–1.6 ms. That is their trend and
  close to their scale. `tests/run.sh` keeps these as checks.
- **Stable at any setting** (fixed 2026-10-04, while adding the tine and reed).
  The struck point was stepped explicitly. With the hardest felt on the thinnest
  treble strings or the stiffest bars, the felt's stiffness (including its
  hysteresis, which at this step is about five times the plain stiffness)
  outran the step, and the note rang up to full scale: 162 notes did, among them
  Una Corda, Electric Grand and Dulcimer above C5 at `SOFT` 0 and a hard touch.
  The struck point is now solved implicitly against the felt's local stiffness,
  so it cannot ring up, whatever the stiffness. The hammer is still stepped
  explicitly; it is far from its limit. Every other note sounds as before,
  within 0.4 dB (the Toy Piano 1.2 dB) and a few hertz of brightness, and the
  gains were re-matched. Some of what had passed for brightness was this
  ringing: the Electric Grand at `SOFT` 0 came out 6 dB less bright, so its hammers
  are now lighter (×0.7) and softer (×0.3) to keep `SOFT`'s full range. A test
  strikes every key of every instrument at both ends of `SOFT`.
- **Graduated mallets.** One mallet across the whole bar range kept contact
  near 2 ms everywhere. That dulls the top octaves, whose period is shorter
  than the pulse: they came out 13 dB per octave quieter. So the yarn hardens up
  the instrument, as players' graduated mallet sets do. Default contact at
  middle C is about 1 ms on the vibraphone and 1.3 ms on the marimba.
- **Each mallet set against its overtone.** The first Celesta, Toy Piano and
  Kalimba mallets were so soft that the second mode came out 40–55 dB under the
  fundamental: a pure tone, and velocity, `SOFT` and `SPOT` barely changed
  anything. Each mallet is now set so that at medium velocity the second mode
  sits 18–23 dB down, with `SOFT` sweeping it over more than 20 dB. The
  celesta's felt has a steeper curve (p 2.7, as hammer felt does), so a harder
  touch brightens it by about 8 dB.
- **Mallets for the low-lying modes.** The first tubular-bell, handbell,
  handpan and tongue-drum mallets were so hard and brief that every mode was
  struck alike, and neither velocity nor `SOFT` changed the colour. Their modes
  that matter sit at 500–800 Hz, so the contact must last about as long as one
  of their periods. The mallets are softer and their felt curves steeper
  (p 2.7–2.9). The tubes and bells also give back part of the level a soft
  mallet loses, as the strings do, so `SOFT` is heard as tone.
- **`BODY` where there is no resonator.** Tubular bells and handbells have no
  tube or box under them. There `BODY` sets their deep, long-ringing modes: a
  tube's lowest, a bell's hum.
- **The other strings.** The Electric Grand (Yamaha CP-70) has short, stiff
  strings (inharmonicity ×1.5, ×4 in the bass) and no soundboard. The
  Hammered Dulcimer has thin wire (×0.7) and no dampers, so `DAMP` is the
  player's hand. Its light beaters keep its bright, wide velocity range. Its
  2–4 strings per course are struck together and sound nearly equally, so the
  second string's share is 40 % rather than a piano's 18 %, and the courses beat
  clearly. Its first, fast fall is slower, and `SPLIT` reaches twice as far,
  because dulcimer courses are tuned less closely than piano unisons.
- **`BELL`** (Celesta, Toy Piano, Glockenspiel) scales the upper modes from 8 dB
  down to 8 dB up.
- **`MUTE`** (Tubular Bells) is the player's hand on the tube: added loss,
  greatest on the upper modes.
- **`MINOR`** (Handbells) sets the tierce, from none to twice its table level.
- **`RING`** (Handpan, Tongue Drum) sets the tuned octave and twelfth, from
  12 dB down to 12 dB up.
- **`BLOOM`** (Hammered Dulcimer) is the undamped courses taking up the
  partials they share and ringing on, slightly out of tune with the struck one.
  They are more strings sounding, so they add a longer, wider aftersound rather
  than taking from the strike. That ring dies within about 20 s, longer only as
  `DECAY` asks.
- **`TWANG`** (Electric Grand) is the piezo pickups, which hear the bridge's
  force; it rises with the partial's number, so turning up brings the overtones
  forward (partial k × k^1.1 at full).
- **`BARK`** (Tine Piano) brings the tine nearer the pickup's pole. A gentle
  note stays round, nearly a sine with its *tink*; a firm one swings far
  enough that the pickup folds the wave, and the second harmonic comes up:
  at a firm touch it is 10 dB under the fundamental with `BARK` at zero, and
  level with it or above at full.
- **`BITE`** (Reed Piano) narrows the gap between reed and pickup plate. The
  pickup hears the reed swinging in much more than out, so a harder touch
  adds a reedy edge, again mostly the second harmonic. By default, at a firm
  touch, the second harmonic is about 10 dB down and the third 20 dB.
- **`BUZZ`** (Kalimba) adds the mbira's buzzers, the bottle caps or rings that
  rattle against the box when a tine swings hard. A band of noise, about
  4–10 kHz, rides the note's own swing (|y|), so the rattle chatters with the
  tine and dies with it. At zero it is a music box. By default there is a
  touch, about −31 dB against the fundamental; full up it is about −20 dB.
- **Coupled strings.** These follow Weinreich, in JOS's coupled-strings form. Two
  or three detuned copies of each mode, with a small bridge coupling, give the
  **double decay**: a fast in-phase fall, then a slow anti-phase aftersound. Una
  Corda changes which strings receive the force. The dulcimer uses courses of 2–4.
- **Listeners for this engine:**
  - **Soundboard.** Commuted synthesis (Smith & Van Duyne), with a generated
    excitation, so there is no impulse-response file.
  - **Resonator tubes and boxes.** One or two coupled modes per bar, after
    Doutaut, Matignon & Chaigne. The vibraphone's fan is a periodic change in
    bar–tube coupling.
  - **Cavity.** Handpan and tongue drum: one Helmholtz mode coupled to the low
    notes.
  - **Pickups** (Tine Piano and Reed Piano). The modes' sum is the tine's or
    reed's swing, scaled so that full velocity swings it a set number of
    pickup gaps anywhere on the keyboard (from the momentum a blow passes).
    The pickup is a static curve on that swing, and what is heard is its rate of
    change, as a coil or a charged plate gives. Each curve's slope at rest is
    1, so `BARK` and `BITE` change the tone and not the quiet level.
    - **Magnetic, for the tine:** flux 1/(1+s²) of the tine's offset s from
      the pole, resting a little to one side (s = −0.4 gaps). Swung hard, past
      the pole, the flux folds back. That is the bark.
    - **Electrostatic, for the reed:** 1/(1−x/d) as the reed swings toward
      the plate (Pfeifle), kept from touching by a smooth limit.
    - **Dividing by the note's frequency** keeps the fundamental level across
      the keys, and leaves the overtones the edge the rate of change gives them.
    - **No oversampling.** The plan was to oversample ×2 when hard enough to
      alias. Instead, above C6 the swing falls as (f/1047 Hz)^−1.5, as short
      treble tines and reeds swing less on the instruments. At the top, struck as
      hard as possible with `BARK` or `BITE` full, what lies off the harmonic
      series measures about −30 dB, and that includes the tines' own
      inharmonic partials. CPU is 6.9 % and 5.7 % on the Move.

### 2. Waveguide: plucked and bowed strings, and the clavichord

The plucked half is built (2026-10-04, `waveguide.c`): Harp, Nylon Guitar,
Pizzicato and Clavichord. The bowed half is next.

- **What it is.** A string is a delay line closed on itself through its losses
  (Jaffe & Smith's extended Karplus–Strong; Karjalainen, Välimäki & Tolonen).
  One string per note, inline in the voice, up to C1 (1,349 samples).
  - **Loss.** A one-pole low-pass and a gain. The fundamental falls 60 dB in
    its T60 exactly; a partial at f in T60 / (1 + (f/f_b)²), where f_b is
    `EDGE`'s brightness, 300 Hz to 8 kHz. Both are worked out per note, so the
    decay does not depend on the line's length. The pole may spend at most
    90 % of the loss one period allows at f₀, so a dark `EDGE` on a short note
    never takes the fundamental with it. (The first version let it: a dark
    Pizzicato's high notes died in a tenth of their ring time.)
  - **Stiffness.** A first-order allpass (Van Duyne & Smith; Rauhala &
    Välimäki): more delay low than high, so the upper partials stretch sharp.
    `STIFF` moves where it turns over, from the top of the band down to the
    note's fourth partial, relative to the note, so it stretches alike in every
    register.
  - **Tuning.** A third-order Lagrange read at the period less both filters'
    phase delay at f₀. Every key is within 3 cents at any setting, and the read
    can move each sample, which the clavichord's *Bebung* uses.
- **The pluck is commuted** (Smith; Välimäki, Laurson & Erkut): the string is
  linear, so the body can come before it, folded into what drives it.
  1. One impulse, as wide as the fingertip (`SOFT`: a pad at 250 Hz to a nail
     at 7 kHz) and gentler for a soft touch (×(0.5 + velocity)²).
  2. Tilted to fall as 1/k from the fundamental, as a plucked string's
     partials do.
  3. The body: the direct path plus four band-passes in phase with it at their
     peaks, each up to about 12 dB at `BODY` full.
  4. A comb at the pluck point (`SPOT`, by the bridge to the middle), which
     takes out the partials with a node there: at the middle the second
     partial falls by 50 dB or more.
  5. Into the loop ahead of the loss filter, so the first period is rounded
     like every one after it.
- **The finger's own sound** (`NOISE`): a short band of noise beside the
  string: the fingertip, the nail's click, the tangent's knock.
- **Fingers alternate.** Each pluck starts the other way from the last. Four
  plucks of a C major chord meet again in phase after 15 ms (two, three, four
  and five periods), and plucked all the same way they stacked into one spike
  2 to 3 dB above the rest. A real hand pulls strings this way and that.
- **Released,** a harp's or guitar's string rings on unless `DAMP` is up (the
  hand); a pizzicato's or clavichord's is always damped, `DAMP` setting how
  fast (the finger, the listing cloth).

| Instrument | The strings | Body | `CHAR` |
|---|---|---|---|
| Harp | Gut and nylon, a long ring (7 s at middle C) | Soundbox, 140–900 Hz | `BLOOM` |
| Nylon Guitar | 4 s at middle C, keys below E2 an octave up | Air mode at 100 Hz, the top above it, broad | `BLOOM` |
| Pizzicato | Gut, stopped by the other hand: 1.6 s at C3, dark | The viola's, 230–1,100 Hz | `DEEP` |
| Clavichord | Brass, bright and stiff, 3.5 s at middle C | A small board, 250–2,400 Hz | `BEND` |

- **`BLOOM`** is a bank of open strings shared by every note, driven from the
  bridge (the sum of the plucked voices): the guitar's six (E2 A2 D3 G3 B3 E4)
  and a chromatic harp's octave below middle C. The strings whose partials meet
  the played note's take them up and ring on, about 6 s at C3; the rest stay
  quiet. At A3 it adds 13 to 15 dB at 1.5 s. Changing to another instrument or
  moving `DECAY` retunes it once it has faded out.
- **`DEEP`** moves the Pizzicato's body from the viola's to the cello's
  (100–600 Hz) and its strings a little longer and darker.
- **`BEND`** is the clavichord's *Bebung*: pad pressure pushes the tangent and
  the string goes sharp, up to 60 cents. The lightest fifth of the pad is a
  resting finger and stays in tune, because the Move's pads report some
  pressure as soon as a key is held.
- **Cost on the Move:** Harp 8.3 % with its bank, Nylon Guitar 6.9 %,
  Clavichord 4.6 %, Pizzicato 3.4 %.

The bowed half, still to be built:
- **Bowed instruments** (cello, violin, section) use Smith's bowed-string
  waveguide. The bow is a nonlinear friction junction where the string sticks and
  slips, after McIntyre, Schumacher & Woodhouse. The friction curve is chosen
  after Serafin, Smith & Woodhouse's playability study. **Bow pressure is pad
  pressure. Bow speed follows pressure and velocity. Bow position is `VEIL`.**
  At low pressure and far from the bridge the model gives the soft *flautando*
  tone naturally. The body is a shared filter after the voice sum, which is valid
  because the body is linear.
- **Section.** Three bowed strings per note, each with its own small detune,
  vibrato phase and bow noise, through one shared body and hall.

### 3. Banded waveguide: bowed and rubbed glass and metal

- **What it is.** Essl & Cook's banded waveguides keep a travelling-wave loop for
  each strong mode, so a friction exciter (bow or wet finger) can drive an
  inharmonic object such as a bar, glass or bowl. Modal synthesis can't do that,
  because modes have no loop to sustain.
- **Exciter.** The same friction junction as the bowed strings.
- **Singing bowl.** Two near-degenerate modes per band give its slow beating.

### 4. Air: flutes, clarinet and harmonium

- **Flute family.** A jet driving a tube. This is Cook's STK flute structure: a
  jet delay, a polynomial jet nonlinearity and a bore delay, grounded in Verge's
  jet-drive analysis.
  - **Breath pressure is pad pressure.** Breath noise is mixed into the jet, and
    that is the softness.
  - **Stopped pipes** (pan flute) reflect with the opposite sign, so they have
    odd harmonics.
  - **Ocarina** replaces the tube with a single cavity mode.
- **Clarinet.** A reed valve on a cylindrical bore, after McIntyre, Schumacher &
  Woodhouse. Soft reed and low pressure give the *chalumeau*.
- **Harmonium.** Puranik & Scavone's real-time model (DAFx 2023): a physically
  derived free-reed source driving a reed-chamber filter, designed for real-time
  use with timbre control. Bellows pressure is pad pressure. The voix céleste is
  a second rank tuned slightly sharp.

### 5. Banks: always-running generators

Built 2026-10-04 (`banks.c`). The generators run once and are shared; keys only
open gates on them. None of these has touch: an organ key is a switch, so
velocity changes nothing.

- **Tonewheel Organ.**
  - **Wheels.** 91 sine wheels on Hammond's twelve gear ratios (85/104 for C,
    88/64 for A, …), wheel frequency 20 rev/s × ratio × teeth, the teeth
    doubling each octave. So the tuning is close to equal temperament, not on
    it, as on the instrument. Each wheel starts at its own phase. The lowest
    twelve carry a little third harmonic. They are Mathews–Smith phasors,
    renormalised each block, run four at a time.
  - **Busbars.** A key connects nine wheels, one per drawbar (16′ an octave
    down, 5⅓′ a fifth up, … 1′ three octaves up), folding back an octave past
    either end of the 91. A drawbar step is 3 dB. **A settled key costs only
    nine gains on the busbars**, ramped per block, so the organ's cost hardly
    grows with keys held: 30 keys cost 36 % more than one, on the Mac.
  - **Key click.** Each of a key's nine contacts closes at its own moment in its
    first 1.2 ms. `CLICK` sets how abruptly: a 6 ms ramp at zero, 0.06 ms at
    full, and a short burst of 2–4 kHz noise for the contacts' bounce on key
    down and, softer, key up. A key in its first milliseconds is mixed sample by
    sample so the contacts close when they should.
  - **Percussion** (`PING`) is on the third harmonic (the 2⅔′ wheel) and
    single-triggered: it fires only when no key is down, and then for every
    key struck with it, so legato lines get none. `TAIL` sets its fall, 0.25 to
    2.5 s.
  - **Leakage** (`LEAK`): each sounding wheel bleeds into the wheels a
    semitone either side and its partner four octaves away on the same shaft,
    up to −24 dB (after Pekonen, Pihlajamäki & Välimäki).
  - **`SOFT`** is drawbar brightness: the drawbars above 8′ fall away, 13 dB at
    the 4′ end to end and twice that at the 2′, at the same overall level. At
    the middle the registration is literal.
  - **Scanner vibrato and chorus** (`LUSH`): a delay swept by a 6.9 Hz triangle,
    up to 0.5 ms either way, mixed half and half with the dry sound at full,
    which is the C3 setting. Turned down it is gentler; at zero it is off.
    (Werner, Dunkel & Germain model the scanner itself, a line box of LC
    sections; a swept delay is its audible half.)
  - **Leslie** (`fx.c`), after Smith, Serafin, Abel & Berners:
    - **Split** at 800 Hz: the horn above, the drum below. Each is a Doppler
      delay, the horn's swinging about ±0.5 ms and the drum's ±0.14 ms.
    - **The horn** is louder and brighter facing the microphone (about 8 dB
      and 2.5 → 12 kHz around a turn).
    - **Stereo:** two microphones a third of a turn apart.
    - **Speed:** `SWAY` moves both rotors continuously from `SLOW` (0.4–1.6 Hz
      for the horn) to `FAST` (5–8 Hz), the drum at 0.84 of the horn. Each
      rotor chases the setting with its own inertia (Herrera, Hanson & Abel):
      the horn in about 0.7 s up and 0.9 s down, the drum in about 4.5 s up and
      5.5 s down. So a quick turn of `SWAY` gives the familiar slow wind-up.
  - **Default** is flute drawbars `00 8800 000` and a slow Leslie, in the
    lineage of *A Whiter Shade of Pale*.
  - Rejected: SWAY as a two-position switch, which is how a half-moon works. A
    knob that only did something at its middle would read as broken, and
    intermediate speeds are useful.
- **Flute Organ.** Pipe ranks at 8′, 4′ and 2′, one phase per pitch shared by
  every rank that sounds it, each pipe a fraction of a cent off true. The 8′
  is stopped (odd partials); `EDGE` opens it toward an open flute and brightens
  the others. `SOFT` is the wind: each partial above the first up to 3.5 dB
  softer per step, and a gentler wind speaks more slowly. A pipe speaks with its
  upper partials first, then its fundamental (`SWELL` sets how long, 15 ms to
  0.4 s), as Castellengo measured. `PUFF` is the chiff: for an instant the
  pipe overblows (a stopped pipe to its twelfth, an open one to its octave),
  and a breath of noise rings about its fourth partial. `SWAY` is the
  tremulant, a tremolo in one channel, as it is in the wind.
- **String Ensemble.** Twelve top-octave sawtooth oscillators (C8–B8, PolyBLEP)
  divided down, so every octave of every note is locked in phase, as a string
  machine's are. 8′, 4′ and 2′ registers. `SOFT` is a two-pole low-pass on the
  sum, 12 kHz down to 400 Hz; `EDGE` a presence peak about 2.2 kHz, up to
  +9.5 dB. `SWELL` is the attack, 20 ms to 1.2 s. **`LUSH` is the ensemble:**
  three taps on one delay line (5.5 ms), each swept by a 0.63 Hz and a 6.3 Hz
  LFO at its own third of the cycle, after Raffel & Smith. The left hears the
  first two taps and the right the last two. **`SWAY` is a vibrato**, up to
  20 cents at `SPEED`. The plan had `SWAY` as the ensemble's depth, but that is
  `LUSH`, and one meaning per knob is the rule.
- **The flute organ and string machine** cost a little more per key (about
  2.4 µs and 0.7 µs per key per block on the Mac). Unlike the organ, each
  key reads its generators itself.

### 6. Synthetic: FM and formant

- **Glass E.Piano** (built 2026-10-04, `fm.c`). Chowning FM in two operator
  pairs, after the 1980s FM electric pianos:
  - **Tine pair:** carrier at f₀, modulator at 14 f₀. Its index jumps with the
    blow (up to about 2.5, past which the tink's sideband would fall back
    toward its first null) and falls within about 0.25 s to a small floor. So a
    firm note opens with the bright, inharmonic *tink* (`TINE`) and settles to
    nearly a sine.
  - **Bell pair:** carrier at f₀, a few cents apart (`SPLIT`, 1–15 cents, so
    the pairs beat), modulator at `TUNE`'s ratio, 1 to 5. Whole numbers ring
    as bells and the ratios between them as glass. Its index is `GLASS`; it
    rings shorter than the tine pair, so the note mellows as it sounds.
  - **Velocity** drives both indices, so soft playing is nearly a sine.
    `SOFT` scales them a decade either way of the middle and gives the attack
    up to 8 ms. It rings 5 s at middle C, shorter up the keys, `DECAY` a
    factor of 4 either way; released, 0.3 s.
- **Choir.** Each singer is a soft glottal pulse with jitter and breath noise
  through a Klatt-style cascade of five formant resonators. `VOWEL` morphs oo →
  oh → ah. Three singers per note, each detuned and drifting on its own, make a
  section, not a chorus effect.

### Shared effects

The signal path is: instrument, then tremolo or auto-pan (on the Reed Piano a
tremolo in one channel, as its amplifier has; on the others an auto-pan, the
Tine Piano's like the Suitcase's), then gentle drive, then
Leslie (organ only), then plate reverb (Dattorro), then tilt EQ, then `VOL`, then
8 dB of headroom, then the limiter, then `int16`.

- **Headroom.** The sum is scaled by 0.4 (−8 dB) before the limiter, so four loud
  notes at once stay under it.
- **The limiter** is straight (linear) up to half of full scale (−6 dBFS). Above
  that it bends smoothly, 0.5 + 0.5·tanh(2(a − 0.5)), so it never clips. At
  −3 dBFS it takes off 0.14 dB. Everything quieter than −6 dBFS passes untouched.
  (Rejected: the first draft's tanh over the whole range. It coloured every
  note a little and flattened chords, which was part of the clicks the user heard
  on 2026-10-04.)

## Lineage

**Music** (what each family is for)

- Nils Frahm, *Felt* (2011): the felt upright.
- C.P.E. Bach: *Bebung* on the clavichord.
- Tchaikovsky, *The Nutcracker* (1892): the celesta.
- Cage, *Suite for Toy Piano* (1948): the toy piano.
- Reich, *Music for 18 Musicians* (1976): marimbas and vibraphone.
- Procol Harum, *A Whiter Shade of Pale* (1967): the soft Hammond.
- Nico, *The Marble Index* (1968): the harmonium.
- Supertramp, *Dreamer* (1974): the Wurlitzer.
- Jarre, *Oxygène* (1976): the string machine.
- Benjamin Franklin's glass armonica (1761).
- Earth, Wind & Fire's kalimba.
- PANArt's Hang (2000): the handpan.

**Papers.** Grouped by engine. The bold entries are the ones an engine is built on.

*Modal engine and struck strings*

- **Bank, Zambon & Fontana, *A Modal-Based Real-Time Piano Synthesizer*, IEEE TASLP
  18(4), 2010.** [PDF](https://iris.univr.it/retrieve/e14ff6e2-e533-0209-e053-6605fe0ad24c/allegatooa_6366.pdf),
  [sounds](https://home.mit.bme.hu/~bank/publist/taslp-piano/index.html)
- **Mathews & Smith, *Methods for Synthesizing Very High Q Parametrically Well
  Behaved Two Pole Filters*, SMAC 2003.** [PDF](https://ccrma.stanford.edu/~jos/smac03maxjos/smac03maxjos.pdf)
- Bank & Chabassier, *Model-Based Digital Pianos: From Physics to Sound
  Synthesis*, IEEE Signal Processing Magazine 36(1), 2019. [list](https://home.mit.bme.hu/~bank/publist)
- **Smith & Van Duyne, *Commuted Piano Synthesis*, ICMC 1995.** [PDF](https://ccrma.stanford.edu/~jos/pdf/vds95.pdf)
- **Weinreich, *Coupled Piano Strings*, JASA.** [summary](https://www.speech.kth.se/music/5_lectures/weinreic/weinreic.html),
  [JOS](https://ccrma.stanford.edu/~jos/pasp/Coupled_Strings.html)
- **Stulov, hysteretic felt models** ([author](https://independent.academia.edu/AStulov)),
  and Russell, *The Piano Hammer as a Nonlinear Spring*
  ([page](https://www.acs.psu.edu/drussell/piano/nonlinearhammer.html)).
- Chaigne & Askenfelt, *Numerical Simulations of Piano Strings I*, JASA 1994.
  [PDF](https://web.phys.ntnu.no/~stovneng/TFY4160_2007/oving8/numerical_simulations_of_pianostrings_I.pdf)
- Bank & Sujbert, *Generation of Longitudinal Vibrations in Piano Strings*, JASA
  117(4), 2005. [PDF](https://web.phys.ntnu.no/~stovneng/TFY4160_2007/oving8/longitudinal_vibrations_in_piano_strings.pdf)
- Askenfelt, on hammer voicing. [KTH](https://www.speech.kth.se/music/5_lectures/askenflt/voicing.html)
- **Avanzini & Rocchesso, modal impact and contact-force model, DAFx 2001.**
  [PDF](https://avanzini.di.unimi.it/downloads/publications/avanzini_dafx01_revised.pdf)
- Doutaut, Matignon & Chaigne, *Numerical Simulations of Xylophones II*, JASA
  1998. [summary](https://labs.sonicfield.org/library/numerical-simulations-of-xylophones-ii-time-domain-modeling-of-the-resonator-and)
- Bar tuning (1 : 4 : 10, undercuts). [RUN](https://run.unl.pt/entities/publication/1e4adbba-3dbd-49ea-8652-43bf54a97cb0),
  [CCRMA notes](https://ccrma.stanford.edu/CCRMA/Courses/150/percussion.html)
- Bell partials and bell modelling. [BME notes](https://dsp.mit.bme.hu/eng/_sgt/m5m2s4_1.htm),
  [DAFx, *Efficient Modeling and Synthesis of Bell-like Sounds*](https://dafx.de/paper-archive/details/g31AEhtQ6ww5tzEZDddvkg)
- **Morrison & Rossing, *The Extraordinary Sound of the Hang*, Physics Today,
  2009**, and Rossing et al., *Acoustics of the Hang*.
  [PDF](https://aip.brightspotcdn.com/PTO.v62.i3.66_1.online.pdf),
  [CNMAT](https://www.cnmat.berkeley.edu/sites/default/files/attachments/2008_Sound_of_the_Hang.pdf)
- **Gabrielli, Cantarini, Castellini & Squartini, *The Rhodes Electric Piano:
  Analysis and Simulation of the Inharmonic Overtones*, JASA 148(5), 2020.**
  [record](https://iris.univpm.it/handle/11566/286030)
- **Pfeifle, *Real-time Physical Model of a Wurlitzer and Rhodes Electronic
  Piano*, DAFx 2017.** [PDF](http://www.dafx17.eca.ed.ac.uk/papers/DAFx17_paper_79.pdf)
- Falaize & Hélie, *Passive Simulation of the Nonlinear Port-Hamiltonian Modeling
  of a Rhodes Piano*, J. Sound & Vibration, 2017. [HAL](https://hal.archives-ouvertes.fr/hal-01470949)

*Waveguide engine: plucked and bowed strings*

- **Jaffe & Smith, *Extensions of the Karplus–Strong Plucked-String Algorithm*,
  Computer Music Journal, 1983.** [overview](https://en.wikipedia.org/wiki/Karplus%E2%80%93Strong_string_synthesis)
- **Karjalainen, Välimäki & Tolonen, *Plucked-String Models: From the
  Karplus–Strong Algorithm to Digital Waveguides and Beyond*, Computer Music
  Journal 22(3), 1998.**
- Rauhala & Välimäki, *Dispersion Modeling in Waveguide Piano Synthesis Using
  Tunable Allpass Filters*, DAFx 2006. [DAFx](https://dafx.de/paper-archive/details/n4Qv1ScD_VbmqShsq0ZVQQ)
- **Välimäki, Laurson & Erkut, *Commuted Waveguide Synthesis of the Clavichord*,
  Computer Music Journal 27(1), 2003.** [record](https://vbn.aau.dk/en/publications/commuted-waveguide-synthesis-of-the-clavichord/)
- Välimäki et al., harpsichord synthesis, EURASIP JASP 2004 (commuted body
  design). [PDF](https://www.ee.columbia.edu/~dpwe/e6820/papers/ValPK04-harpsi.pdf)
- **McIntyre, Schumacher & Woodhouse, *On the Oscillations of Musical
  Instruments*, JASA 74, 1983.** Bow, reed and jet in one framework.
  [notes](https://www.damtp.cam.ac.uk/user/mem2/papers/MUS-ACOUST/)
- **Smith, bowed-string waveguide.** [JOS](https://ccrma.stanford.edu/~jos/jnmr/Bowed_Strings.html)
- **Serafin, Smith & Woodhouse, bowed-string playability and friction models.**
  [PDF](https://ccrma.stanford.edu/~serafin/SerafinEtAl.pdf)
- Woodhouse, *The Acoustics of the Violin: A Review*, Reports on Progress in
  Physics 77, 2014. [Cambridge](https://www.repository.cam.ac.uk/handle/1810/245817)
- Willemsen et al., real-time bowed-string simulation, DAFx 2019. A cost
  reference. [PDF](https://vbn.aau.dk/files/310482246/DAFx2019Willemsen.pdf)

*Banded waveguide engine*

- **Essl & Cook, banded waveguides**: bowed bars, rubbed glasses, bowls.
  [Princeton report](https://www.cs.princeton.edu/techreports/2002/659.pdf),
  [CMJ](https://muse.jhu.edu/article/53790)

*Air engine*

- **Cook & Scavone, STK flute.** [STK](https://ccrma.stanford.edu/software/stk/classstk_1_1Flute.html)
- **Verge, jet-drive and aeroacoustic sources in flutelike instruments.**
  [author](https://independent.academia.edu/VergeMarcPierre)
- **Puranik & Scavone, *Physically Inspired Signal Model for Harmonium Sound
  Synthesis*, DAFx 2023.** [PDF](https://www.dafx.de/paper-archive/2023/DAFx23_paper_47.pdf)
- Castellengo, flute attack transients, 1999. [PDF](https://www.lam.jussieu.fr/Membres/Castellengo/publications/1999a1-Flute%20transients%20Analysis.pdf)

*Banks engine*

- **Pekonen, Pihlajamäki & Välimäki, *Computationally Efficient Hammond Organ
  Synthesis*, DAFx 2011.** [DAFx](https://dafx.de/paper-archive/details/Va-Hsjj4g396c-1d8ofv2w)
- Werner, Dunkel & Germain, *A Computational Model of the Hammond Organ
  Vibrato/Chorus using Wave Digital Filters*, DAFx 2016. [DAFx](https://dafx.de/paper-archive/details/nd_rFZKONvryB6uA-7xKrg)
- Smith, Serafin, Abel & Berners, *Doppler Simulation and the Leslie*, DAFx 2002.
  [PDF](https://ccrma.stanford.edu/~jos/doppler/dafx02.pdf)
- Herrera, Hanson & Abel, *Discrete Time Emulation of the Leslie Speaker*, AES
  2009. [CCRMA](https://ccrma.stanford.edu/papers/discrete-time-emulation-of-leslie-speaker)
- **Raffel & Smith, *Practical Modeling of Bucket-Brigade Device Circuits*, DAFx
  2010.** [DAFx](https://dafx.de/paper-archive/details/JhVfAOFXD1lAtctkMTUODg)

*Synthetic engine and effects*

- **Chowning, *The Synthesis of Complex Audio Spectra by Means of Frequency
  Modulation*, JAES 21(7), 1973.** [PDF](https://web.eecs.umich.edu/~fessler/course/100/misc/chowning-73-tso.pdf)
- **Klatt, *Software for a Cascade/Parallel Formant Synthesizer*, JASA, 1980.**
  [PDF](https://www.fon.hum.uva.nl/david/ba_shs/2009/klatt-1980.pdf)
- **Dattorro, *Effect Design Part 1: Reverberator and Other Filters*, JAES 45(9),
  1997.** [PDF](https://ccrma.stanford.edu/~dattorro/EffectDesignPart1.pdf)

*Set aside, recorded so they are not re-researched*

- Gabrielli et al., Clavinet, EURASIP JASP 2013. [open access](https://asp-eurasipjournals.springeropen.com/articles/10.1186/1687-6180-2013-103)
- Renault, Mignot & Roebel, DDSP piano, DAFx 2022. [DAFx](https://dafx.de/paper-archive/details/_5jvqdya3I0yV4ZgRQWTPw)
- Simionato & Fasciani, *Sines, Transient, Noise*, 2025. [arXiv](https://arxiv.org/abs/2409.06513)
- Desvages, finite-difference bowed strings (NESS).
  [PDF](https://www.ness.music.ed.ac.uk/wp-content/uploads/2014/05/desvages.pdf)
- OpenWurli, for listening comparison only. GPL; no code is read or reused.
  [GitHub](https://github.com/Ferglerz/openwurli)

## Control surface

**This is a first draft. It will be cut after the instrument list is agreed and again
after the first look on the device**, as Ragtag went from ten pages to four.

**Every label is a plain word.** The user asked on 2026-10-04 for each control
label to make sense at a glance, after the probe's `MSGS` readout did not. The cell
is five characters wide (`LABEL_CHARS`), which tempts squeezed jargon such as
`TRATE` or `PDECAY`. So:

- **The cell (`short_name`) is a real word of five letters or fewer.** It names what
  you hear as the knob turns up: `SWAY`, `SPACE`, `HUSH`, `BARK`, `PUFF`. `VOL` is
  the one standard abbreviation.
- **The header (`name`) says it in full** when the knob is held, and it is where an
  instrument's own term goes: *Bebung*, *Sul Tasto*, *Céleste*.
- **No label is `MOVE`**, on a device called Move.
- **No debug readouts on knobs.**
- **The test enforces it.** `tests/run.sh` runs every `short_name` through the
  host's own `labelVerbatim`, which must return it unchanged, so the device draws
  every word exactly as written.

### One place for everything

The user found on 2026-10-04 that some controls were doubled between pages, which
was confusing, and asked for the controls to be simple and clear without losing
any of the range of sound. So:

- **Every knob is on exactly one page.** The instrument's signature (`FELT`,
  `HUSH`, `MOTOR`, `ROLL`) had been on both Main and the Instrument page. It is
  now on Main only.
- **Each page has one job.** Main is the instrument and how it is played. The
  Instrument page, titled with the instrument's name, is how it is built.
  Effects is the room and the extras.
- **No two knobs do the same job.** `CLOSE` was removed. It raised the mechanism
  knock, added presence and changed the reverb send, which made it overlap
  `NOISE`, `TONE` and `SPACE`, so it was never clear which knob to reach for. No
  sound was lost. Each instrument's closeness now lives in its voicing, set with
  those three knobs, and measures the same as before within 0.1 dB. `TONE` moved to
  Main in its place, because it shapes the whole sound. That leaves `DARK` on
  Effects among the reverb's knobs, where it plainly means the reverb.
- **No knob is shown where it does nothing.** `SPEED` is hidden on the
  Vibraphone, whose fan speed is its `MOTOR`. The gate is
  `visible_if: {"param": "type", "not_equals": "Vibraphone"}`. A gate is one
  comparison, so when a second instrument's `SWAY` stops being a tremolo (the
  organ's Leslie), `SPEED` will be split per instrument, as `CHAR` is.
- **`SPACE` stays on Main** (the user's choice, 2026-10-04), although it is
  only the reverb's amount. The reverb is the effect turned most often, so its
  amount sits in reach and its character (`SIZE`, `DARK`, `DELAY`) on Effects.
  (Considered: moving it to Effects and bringing `NOISE` up to Main.)
- **No word means two things.** The shapes still to be built had their own `TONE`
  for an instrument's brightness. With `TONE` on Main, those became `EDGE`
  (*Edge (Brightness)*).

`tests/plan.test.mjs` plans every instrument's pages with the host's own planner
and fails if any knob appears twice, if `CLOSE` returns, or if `SPEED` is shown on
the Vibraphone.

**Picking an instrument.** `TYPE` steps through the instruments built so far, in
family order: today the twenty-five named at the top of this document. The jog
wheel browses **presets** on the Main page; for now there is one per instrument, its
default sound. At 1.0 the presets become the catalogue: 38 instruments × 3 = 114,
named *Family · Instrument · Variation*, for example *Mallets · Vibraphone · Motor
Off*.

### Every instrument starts beautiful

**Turning `TYPE` loads that instrument's own default sound on every page**: Main,
`CHAR`, the Instrument page and Effects. Only `VOL` stays where you left it. The
user asked for this on 2026-10-04: each engine, when chosen, should sound
gorgeous out of the box, before any knob is touched. The default sound is the same
as the instrument's factory preset.

It replaces an earlier rule, *each instrument keeps its own settings*, under which
leaving the cello and coming back found the cello as you left it. That rule meant
that switching instruments kept whatever Effects the last one had: the Felt
Upright's small dark room under the vibraphone, say. A saved sound is still
recalled exactly, because `state` sets every value directly and never goes
through `TYPE` (see *Implementation notes*).

Each default sound, the *voicing*, is one line in `instruments.c`: Main from the
instrument's row, then `key=value` pairs for `CHAR`, its own page and any Effects
key. Any Effects key not named takes its shared default.

| Instrument | The idea | Main | Own page | Room |
|---|---|---|---|---|
| Felt Upright | Close and intimate (Frahm, *Felt*): a thick strip, the action loud and near | SOFT 55, FELT 35, DECAY 50, TONE 55, SPACE 18 | NOISE 60, SPLIT 30, BODY 60, DAMP 30 | Small and warm: SIZE 35 (1.7 s), DARK 55, DELAY 8, DRIVE 10 |
| Una Corda Grand | Further off, on two strings, a touch darker, in a hall | SOFT 50, HUSH 45, DECAY 65, TONE 45, SPACE 28 | SPOT 25, SPLIT 25, BODY 55, NOISE 21, DAMP 30 | A hall: SIZE 65 (3.1 s), DARK 45, DELAY 30 |
| Vibraphone | A slow motor, released bars left to ring a little | SOFT 50, MOTOR 22 (2.9 Hz), DECAY 60, SWAY 45, SPACE 30 | BODY 70, SPLIT 5, NOISE 15, DAMP 15 | Shimmering: SIZE 60 (2.7 s), DARK 35, DELAY 25 |
| Marimba | Rosewood and yarn, nearly dry (Reich) | SOFT 50, ROLL 0, DECAY 58, TONE 52, SPACE 21 | BODY 75, SPLIT 5, NOISE 32, DAMP 0 | SIZE 45 (2.1 s), DARK 50, DELAY 15 |
| Celesta | Felt hammers and a little bell (*Sugar Plum Fairy*) | SOFT 55, BELL 40, DECAY 50, SPACE 30 | BODY 60, SPLIT 5, NOISE 30, DAMP 30 | SIZE 55 (2.5 s), DARK 40, DELAY 20 |
| Toy Piano | Slightly out of tune and clangy (Cage) | SOFT 45, BELL 50, DECAY 50, SPACE 22 | BODY 60, SPLIT 15, NOISE 40, DAMP 0 | Small: SIZE 30 (1.5 s), DARK 50, DELAY 5 |
| Xylophone | Yarn mallets on rosewood | SOFT 60, ROLL 0, DECAY 55, SPACE 22 | BODY 70, SPLIT 5, NOISE 30, DAMP 0 | SIZE 45 (2.2 s), DARK 50, DELAY 15 |
| Glockenspiel | Soft-wrapped mallets, long-ringing steel | SOFT 70, BELL 35, DECAY 50, TONE 45, SPACE 28 | BODY 30, SPLIT 5, NOISE 15, DAMP 0 | SIZE 60 (2.3 s), DARK 45, DELAY 25 |
| Kalimba / Music Box | A thumb piano with a touch of its buzzers | SOFT 60, BUZZ 30, DECAY 55, SPACE 25 | BODY 70, SPLIT 10, NOISE 30, DAMP 0 | SIZE 40 (1.8 s), DARK 50, DELAY 10 |
| Electric Grand | A CP-70 in a ballad | SOFT 50, TWANG 35, DECAY 50, SWAY 20, SPACE 20 | BODY 50, SPLIT 20, NOISE 30, DAMP 30 | SIZE 45 (2.0 s), DARK 50, DELAY 12, SPEED 25 (a slow auto-pan) |
| Tine Piano | A suitcase tine piano, voiced a little toward bark | SOFT 60, BARK 40, DECAY 55, SWAY 30, SPACE 20 | BODY 60, SPLIT 10, NOISE 25, DAMP 30 | SIZE 45 (2.0 s), DARK 50, DELAY 12, SPEED 30 (the slow stereo vibrato) |
| Tonewheel Organ | *A Whiter Shade of Pale*: flutes `00 8800 000`, the Leslie slow | SOFT 50, LUSH 60, DECAY 30, SWAY 0, SPACE 20 | CLICK 35, LEAK 25, SLOW 30, FAST 60 | SIZE 40 (1.9 s), DARK 50, DELAY 10, DRIVE 20 (the preamp a little warm) |
| Flute Organ | A chamber organ in a stone church: stopped 8′, open 4′ | SOFT 60, PUFF 45, DECAY 30, SWAY 10, SPACE 35 | EDGE 25, SWELL 35, 8′ 80, 4′ 50, 2′ 0 | SIZE 70 (3.2 s), DARK 45, DELAY 25, SPEED 55 (a 4.6 Hz tremulant) |
| Glass E.Piano | An FM electric piano with its chorus | SOFT 60, GLASS 35, DECAY 55, SWAY 20, SPACE 30 | TUNE 50 (the bell pair on 3 : 1), TINE 45, SPLIT 25 | SIZE 50 (2.2 s), DARK 40, DELAY 15, SPEED 30 |
| String Ensemble | *Oxygène*: viola and violin registers, a slow swell | SOFT 60, LUSH 70, DECAY 50, SWAY 0, SPACE 35 | EDGE 30, SWELL 45, 8′ 80, 4′ 60, 2′ 0 | SIZE 65 (3.0 s), DARK 45, DELAY 20 |
| Reed Piano | A reed piano turned down in a small room | SOFT 60, BITE 45, DECAY 50, SWAY 25, SPACE 20 | BODY 50, SPLIT 8, NOISE 25, DAMP 30 | SIZE 35 (1.7 s), DARK 55, DELAY 8, SPEED 45 (its own tremolo), DRIVE 15 (the amplifier just warm) |
| Tubular Bells | Chimes in a church, the pedal half down | SOFT 50, MUTE 20, DECAY 55, SPACE 32 | BODY 15, SPLIT 10, NOISE 25, DAMP 10 | SIZE 70 (3.2 s), DARK 45, DELAY 30 |
| Handbells | The tierce half in, the bells beating gently | SOFT 60, MINOR 50, DECAY 55, SPACE 32 | BODY 30, SPLIT 15, NOISE 20, DAMP 0 | SIZE 65 (3.0 s), DARK 40, DELAY 25 |
| Handpan | Fingertips, the shell's air under the low notes | SOFT 65, RING 55, DECAY 55, SPACE 30 | BODY 60, SPLIT 15, NOISE 30, DAMP 0 | SIZE 60 (2.7 s), DARK 45, DELAY 20 |
| Tongue Drum | Rubber mallets on a wooden box | SOFT 60, RING 50, DECAY 50, SPACE 25 | BODY 65, SPLIT 10, NOISE 30, DAMP 0 | SIZE 45 (2.1 s), DARK 50, DELAY 12 |
| Hammered Dulcimer | Courses a little apart, struck near the bridge, blooming | SOFT 60, BLOOM 55, DECAY 55, SPACE 25 | SPOT 20, SPLIT 35, BODY 80, NOISE 30, DAMP 0 | SIZE 50 (2.2 s), DARK 45, DELAY 15 |
| Clavichord | In a small room: the tangent toward the end, stiff brass, the *Bebung* half in | SOFT 50, BEND 50, DECAY 40, SPACE 15 | SPOT 25, BODY 50, EDGE 60, NOISE 35, DAMP 50, STIFF 50 | Small: SIZE 30 (1.5 s), DARK 50, DELAY 5 |
| Harp | A concert harp in a hall, its lower strings answering | SOFT 60, BLOOM 40, DECAY 70, SPACE 35 | SPOT 35, BODY 60, EDGE 45, NOISE 20, DAMP 5, STIFF 20 | A hall: SIZE 65 (3.1 s), DARK 45, DELAY 25 |
| Nylon Guitar | Fingerstyle near the soundhole, the open strings ringing a little | SOFT 60, BLOOM 35, DECAY 50, SPACE 20 | SPOT 25, BODY 65, EDGE 40, NOISE 30, DAMP 20, STIFF 25 | SIZE 45 (2.1 s), DARK 50, DELAY 12 |
| Pizzicato | Cellos and violas plucked with the pad of the finger, nearer the cello | SOFT 70, DEEP 60, DECAY 50, SPACE 30 | SPOT 45, BODY 70, EDGE 30, NOISE 35, DAMP 30, STIFF 15 | SIZE 60 (2.7 s), DARK 45, DELAY 20 |

Values are percentages; the times are the plate's measured ring (RT60).

**They are equally loud.** Switching instruments must not jump in level. Each
instrument's own gain is set so the same phrase (a gentle chord, a line over it, a
fuller chord, velocities 55–76) measures within half a decibel of
**−26.5 LUFS** on all twenty-five (measured −26.4 to −26.6), with the voicing's
room included. Before this the first four were 12 dB apart, the Felt Upright
quietest. Two engine changes made the
match possible without pushing loud chords into the limiter:

- **Velocity is a speed.** A key's or pad's velocity sets hammer and mallet speed
  in proportion, 0.3 to 4.5 m/s for the hammers (Chaigne & Askenfelt's range).
  The first draft raised velocity to a power (1.6 for strings), which buried the
  middle velocities, where most playing happens, about 10 dB under the loudest.
- **The treble carries.** Above middle C the string gain rises with pitch (f₀ to
  the power 0.35, about +5 dB at C6), as the soundboard projects the treble. A
  melody up there had been 10 dB under middle C.

**Rule for every instrument still to come:** it joins `TYPE` with a voicing, and
its phrase loudness within a decibel of the others.

**What the screen shows.** The grid caches knob values, and nothing tells it that
`TYPE` has just changed the others. It re-reads the knobs on screen in rotation,
one per tick, so the new values appear within about 0.2 s (`page_controller.mjs`,
the value rotation). A knob turned in that moment steps from its old value. This
was judged acceptable; the host has no call for a module to ask for a re-read.

### Every knob is heard

The user found on 2026-10-04 that the instruments sounded alike and the knobs
made hardly any difference. The tests had checked only which way each knob moved
the sound. Now **each knob, turned end to end on every instrument, must change
the sound by at least a set amount**, measured on a middle C:

| Knob | The minimum, end to end |
|---|---|
| SOFT, FELT | The overtones fall at least 12 dB, while the level changes less than 10 dB, so it is the tone that changes |
| HUSH | At least 8 dB off the overtones |
| DECAY | Rings at least 12 dB longer at 1.5 s |
| DAMP | Stops a released note at least 20 dB sooner (from about 2 s down to about 50 ms) |
| TONE | A tilt of at least 4 dB per octave between the two modes measured, on every instrument (the reason it has a second shelf at 3 kHz) |
| BODY | At least 3 dB more body |
| SPLIT | The note beats: a wobble of more than 3 dB |
| SPOT | Overtones up at least 12 dB toward the end; on a string, the middle hollows the second partial by 15 dB |
| STIFF | Overtones move at least 20 cents |
| NOISE | The knock heard at no less than −14 dB against the note, in the first 50 ms |
| MOTOR, SWAY | A wobble of at least 6 dB, and none at zero |
| BELL | The upper modes at least 12 dB up |
| BUZZ | The sizzle above 6 kHz at least 10 dB up, to −30 dB or more against the note |
| ROLL | A held note re-struck at least five times a second; none at zero |
| MUTE | Rings at least 12 dB shorter at 1.5 s |
| MINOR | The tierce at least 12 dB up |
| RING | The tuned overtone at least 12 dB up |
| BLOOM | Rings at least 6 dB more at 1.5 s |
| TWANG | The fourth partial at least 8 dB forward |
| BARK, BITE | At a firm touch, the second harmonic at least 10 dB up, while the level changes less than 6 dB |
| DRIVE | At least 15 dB more harmonics |
| **Organ:** SOFT | The 4′ at least 12 dB down against the 8′, the level within 10 dB |
| LUSH (organ), LUSH (strings) | The scanner chorus, the ensemble: the note moves at least 3 dB more |
| SWAY (organ), SLOW, FAST | The Leslie's swing under 2 Hz at the bottom, over 5 Hz at the top; SLOW at least doubles the slow speed, FAST raises the fast one 40 % |
| PING, TAIL | The third harmonic at least 12 dB up; at 0.6 s, at least 12 dB longer |
| CLICK | The click heard at no less than −14 dB against the note, in the first 50 ms |
| LEAK | The next wheel at least 12 dB more |
| Each drawbar, 8′ and the registers | Its own harmonic at least 12 dB up |
| DECAY on a sustained instrument | A released note at least 20 dB longer at 0.3 s |
| **Flute Organ:** PUFF | The chiff heard at no less than −14 dB, in the first 50 ms |
| EDGE (flute), EDGE (strings) | The stopped pipe's octave at least 12 dB up; the presence near 2 kHz at least 6 dB up |
| SWELL | At least 12 dB quieter at 30 ms against the held note |
| **Strings:** SWAY | The vibrato spreads the fourth harmonic at least 6 dB |
| **Glass E.Piano:** GLASS, TINE | The bell pair's sideband at 4 f₀, the tink at 13 f₀: each at least 12 dB up |
| TUNE | Off the whole numbers the bell goes inharmonic: 1.5 f₀ at least 12 dB up |
| **Plucked:** SOFT, DECAY, DAMP, TONE, BODY, SPOT, STIFF, NOISE | As for the struck strings above (STIFF on the fourth partial at C3) |
| EDGE (plucked) | The fourth partial at least 12 dB up against the fundamental at 0.25 s: the overtones ring on |
| BLOOM (harp, guitar) | Rings at least 6 dB more at 1.5 s, at A3 |
| DEEP | The third partial at least 8 dB down against the fundamental at C3 |
| BEND | Full pressure sharpens the note at least 20 cents; none with BEND at zero |
| Tuning (plucked) | Every C from C3 to C6 within 3 cents |
| SIZE | The room rings at least 20 dB longer at 2 s |
| DARK | The room's tail at least 6 dB darker, heard on the Electric Grand, hard, pickups full up (once on a hard Felt Upright, whose brightness there was partly the contact ringing up; see *Stable at any setting*) |
| DELAY | The room answers at least 80 ms later |

Getting there took real changes, not relabelling:

- **SOFT and FELT** change brightness, not mostly loudness: a softer contact gives
  back most of the level it loses (level × (K/K₀)^−0.15).
- **NOISE** had been 20–35 dB too quiet to hear. It is now a clear knock.
- **DARK** acted only above 1.4 kHz, where these instruments have almost nothing.
  It now sets the plate's damping from 9 kHz down to 400 Hz.
- **SPOT** reaches the middle of the string, where the even partials vanish.
- **DAMP, STIFF, DECAY and TONE** have wider ranges; the vibraphone's fan
  reaches 6 dB.
- **Brighter beginnings.** The Felt Upright had started nearly as a pure tone at a
  medium touch, and the mallets lacked the bright overtone two octaves up that
  gives them their character (vibraphone 1 : 4 now at −8 dB, marimba at −17 dB).

### No pops

The user heard a click at the start of some notes. There were three causes, each
fixed and each tested:

- **Knobs glide.** What you set (`q->g`) and what is heard (`q->gs`) are separate.
  The heard value follows over about 20 ms, and every effect ramps sample by sample
  across the block. `DELAY` glides over 300 ms with a fractional read, so the tail
  bends rather than jumps. A test jumps every Main and Effects knob end to end
  while a note sounds and requires no spike above the note's own.
- **Chords stay out of the limiter.** See *Shared effects*. Four notes struck hard
  together peak no higher than −3 dBFS, where the limiter barely acts.
- **Stolen notes fade.** Eight ghost slots carry a stolen voice out over a few
  milliseconds. When all eight are busy, the most-faded ghost is reused, so no
  note is ever cut off.

### Main (`root`): the same eight knobs for everything

```
TYPE   SOFT   CHAR   DECAY
SWAY   TONE   SPACE  VOL
```

`CHAR` is never drawn. The third cell always shows the instrument's own word, for
example `FELT`, `VEIL` or `PUFF`.

| Cell | Key | Range | Behaviour |
|---|---|---|---|
| `TYPE` | `type` | the instruments built so far | Selects the instrument, and loads its default sound on every page but `VOL` (see *Every instrument starts beautiful*). |
| `SOFT` | `soft` | 0–100 % | **The contact.** Hammer, mallet or fingertip hardness for struck and plucked instruments. Bow hair tension and friction for bowed. Breath softness (more noise, less edge) for blown. Drawbar brightness for the organ. Attack softness for the synthetic voices. |
| *(CHAR)* | `c_felt_upright`, `c_una_corda_grand`, … | 0–100 % | The active instrument's signature, from the CHAR column above. The cell carries that instrument's own word (`FELT`, `VEIL`, `AIR`…); see below for how. It is on Main only. |
| `DECAY` | `decay` | 0–100 % | Losses and ring time for struck and plucked. Release for sustained. |
| `SWAY` | `sway` | 0–100 % | Motion. Auto-pan for keys and mallets (a tremolo in one channel on the Reed Piano, the tremulant on the Flute Organ), the vibraphone fan's depth, vibrato for the string machine, bowed and blown, the Leslie from slow to fast for the organ, ensemble depth for the choir. |
| `TONE` | `tone` | 0–100 % | Tilts the whole sound: a shelf about 600 Hz (±10 dB at the ends) and a gentler one about 3 kHz (±8 dB), so it is heard on a glockenspiel as well as a piano. The middle is flat. |
| `SPACE` | `space` | 0–100 % | How much goes to the plate reverb: full up sends 1.75 times the dry sound. |
| `VOL` | `volume` | −60 to +6 dB | Output level, before the headroom and the limiter. The one knob `TYPE` leaves alone. |

### Instrument page: one page, relabelled by the instrument

You see **one** Instrument page, and its title and labels always belong to the
instrument you are playing. Underneath, it is **one level per instrument** (38).
Each level is gated with `visible_if: {"param": "type", "equals": "<instrument>"}`,
so exactly one of them is ever shown. Levels that share an engine list the same
keys, so the cost is only hierarchy text, about 12 KB.

**Every gate is on `type` itself**, never on a derived key. The grid re-plans
only when the key that changed is itself a condition key
(`page_controller.mjs`, `replanIfCondition`). A derived `ui_page` would have
moved when `TYPE` turned without the grid noticing, until the next pad press.
(Rejected: derived `ui_page` / `ui_char` gates, the DR32 pattern. It suits a
mode that moves with a pad press, not one that moves with a knob.) The probe
proved the plan offline with the host's own planner; see *Host check*.

Every knob meaning is its own key, prefixed by its page shape (`m_split`,
`b_body`, `a_onset`…). That way each label is correct, and a p-lock or automation
lane recorded on one knob never lands on a different meaning. Instruments of the
same shape share keys, but **each instrument has its own values**, which `TYPE`
sets to its voicing (see *Every instrument starts beautiful*).

There are nine page shapes. These are drafts, to be cut when each engine is built:

| Shape (prefix) | Instruments | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| Modal (`m_`) | pianos, mallets, bells, handpan, kalimba, dulcimer | SPLIT | STIFF | SPOT | BODY | NOISE | DAMP | PEDAL |
| Plucked (`p_`) | harp, guitar, pizzicato, clavichord | SPOT | BODY | EDGE | NOISE | DAMP | STIFF | |
| Bowed (`b_`) | cello, violin, section | BODY | EDGE | PRESS | SWELL | NOISE | BITE | |
| Banded (`d_`) | bowed vibes, glass harmonica, singing bowl | BOW | BLUR | HIT | PRESS | SWELL | NOISE | |
| Air (`a_`) | flutes, clarinet, harmonium | ONSET | EDGE | PRESS | SWELL | AIR | | |
| Organ (`o_`) | tonewheel organ | PING | TAIL | CLICK | LEAK | SLOW | FAST | 1' |
| Machine (`k_`) | flute organ, string ensemble | EDGE | SWELL | 8' | 4' | 2' | | |
| FM (`f_`) | glass e.piano | TUNE | TINE | SPLIT | | | | |
| Choir (`v_`) | choir | CROWD | AIR | SWELL | PRESS | SPLIT | | |

What each word means, as the held-knob header spells it out:

| Cell | Header | Turning it up |
|---|---|---|
| SPLIT | Unison Detune | The strings of a note (or the operator pairs, or the singers) drift further apart. |
| STIFF | String Stiffness | Overtones stretch sharp, as in a short, stiff piano string. |
| SPOT | Strike Point / Pluck Point | The hammer or finger moves from the middle toward the end. |
| BODY | Body | More soundboard, box or body resonance. |
| NOISE | Mechanism Noise | Key thump, damper, bow hair, breath, fingers. |
| DAMP | Damper | Notes stop faster when released. |
| PEDAL | Sustain Pedal | Dampers lift; the same as CC64. |
| EDGE | Edge (Brightness) | A brighter string, bore or rank. (Not `TONE`, which is Main's and tilts everything.) |
| PRESS | Pressure Source | Chooses **Pad**, **Auto** (the velocity swell) or **Blend**. |
| SWELL | Swell Time | The automatic swell, and the attack, take longer. |
| BITE | Bow Bite | The first grip of the bow is stronger. |
| BOW | Bow Speed | A faster bow or rim stroke. |
| BLUR | Band Width | Each band is wider, so the tone is less pure. |
| HIT | Strike Mix | A mallet strike is mixed in under the bow. |
| ONSET | Tonguing | A firmer start to each note. |
| PING | Percussion | The Hammond's percussion ping on the attack. |
| TAIL | Percussion Tail | The ping lasts longer. |
| CLICK | Key Click | Louder key contacts. |
| LEAK | Wheel Leakage | More of the neighbouring wheels bleeds in. |
| SLOW, FAST | Leslie Slow Speed, Leslie Fast Speed | The two rotor speeds that `SWAY` moves between. |
| 1' … 16' | Drawbar 1′ … 16′ | That drawbar pulled out. Players know drawbars by their foot lengths. |
| TUNE | Bell Tuning | The bell pair's frequency ratio. |
| TINE | Tine Depth | The tine pair's modulation: more *tink* when struck hard. |
| CROWD | Singers | One, two or three singers per note. |
| AIR | Breath | Breath noise in the voice or jet. |

**`CHAR` on Main is relabelled the same way.** Main's `knobs` list holds one CHAR
key per instrument (`c_felt_upright`, `c_marimba`, `c_solo_cello`, … 38 in all),
each gated on `type`. Instruments with the same meaning share a word (Harp,
Nylon Guitar and Hammered Dulcimer all read `BLOOM`), but each keeps its own key,
and so its own value. The
planner drops hidden knobs before cutting pages of eight, so Main always shows
exactly eight cells, with the right CHAR label in the third place.

The tonewheel organ also has a **Drawbars** page (`drawbars`), gated on `type`
itself, with the 1′ bar on the Instrument page:

```
16'    5.3'   8'     4'
2.7'   2'     1.6'   1.3'
```

The cells use decimals because the cell font has no fractions; the header says
*Drawbar 5⅓′*.

### Host check (2026-10-04, against Schwung v1.6.3 and upstream v1.7.2)

Done before any code, in the host source. The question was: can the knob labels
follow the instrument? **Yes. No fallback is needed.**

- **Hidden pages and knobs, updated live.** `visible_if` on a level, or on a
  level's `params` entry, hides it. When a gate's value changes, the grid
  re-plans at once:
  - the re-plan commit is `de80771d`;
  - "gated pages: let a module's own mode choose the page set" is `f8b40919`
    (#533);
  - "read a module-wide gate bare" is `6fd32f65` (#551).
  All three are in 1.6.3, which is what the device runs.
- **Rechecked after updating the local copy to v1.7.2.** The 14 commits since
  1.6.3 leave `src/shared/param_pages/`, the 128 KB buffers and the
  aftertouch route unchanged. What did change: module loading moved off the
  callback (#605; see *Implementation notes*), set switches confirm each slot
  (#609), and Move 2.1.1 note records are read correctly (#611). The pressure
  lane survives #611.
- **What triggers a re-plan.** A write from the grid re-plans only if the key it
  wrote is itself a condition key (`replanIfCondition`). Gates on other keys
  are re-read only after a pad press or a module focus change. So every Quilt
  gate is on `type`, the key the `TYPE` knob writes. The host documents derived
  gates (*The gate does not need a cell*, DR32's `ui_engine`), but they suit
  modes that follow a pad, not a knob.
- **Limits we must respect:**
  - **At most four distinct gate keys.** Quilt uses one: `type`.
  - **`visible_if` must sit in the hierarchy, not in `chain_params`.** In
    `chain_params` it is silently ignored. `validate.mjs` reports this as
    `visible-if-not-on-level`.
  - **Conditions are single comparisons** (`equals`, `not_equals`, `gt`, `lt`,
    `truthy`, `falsey`), and `equals` is an exact string match on the raw value
    (`compareConditionValue`). That is why there is one level, and one CHAR key,
    per instrument, and why `type` is served and accepted as option **names**
    (`options_as_string: true`).
- **Proved offline by the probe** (`probe/`, 2026-10-04). `tests/run.sh` drives
  it through the v2 API (22 checks), then plans its pages with the host's own
  `planPages` for each instrument. Main's CHAR cell and the instrument page
  change with `type`, the only gate key is `type`, and `validate_contract`
  reports no errors.
- **Hidden knobs compact.** In `page_plan.mjs`, hidden authored knobs are
  filtered out before the 8-per-page chunking, so the cells close up.
- **Size.** `chain_params` and `ui_hierarchy` each get a 128 KB buffer
  (`shadow_constants.h`). The chain host rejects a module whose answer is
  larger. Quilt's estimate is about 30 KB for both (38-label enum and about 200
  params), so it fits comfortably.
- **Pad pressure reaches the instrument.** Poly aftertouch (`0xA0`) is routed to
  synth slots and follows the slot's transpose to the right note
  (`shadow_midi.c`, `shadow_chain_apply_transpose`). Move's clips also store a
  pressure lane (`move_model.h`), so **sequenced parts can carry pressure** too.
  The user's Schwung setting *Aftertouch* (on/off, with a deadzone) applies
  first.
- **Confirmed on the device** with the probe (2026-10-04, host 1.6.3, played by
  the user):
  - Turning `TYPE` relabels Main's CHAR cell (the probe's FELT → TASTO → CAVITY) and swaps
    the instrument page.
  - **Jogging presets does the same**, so the preset path re-plans too.
  - Pad pressure arrives with Move's settings as they are. PRES followed the
    pad, and the cello voice swelled with pressure.
  - One display lesson: a pressure-message counter declared 0–99999 drew as an
    arc knob (`shouldDrawBigNumber` caps a plain number's span at 24), so a few
    hundred messages did not visibly move it. Readouts that are counts need a
    small range and `display: "big"`. The user also found its label, `MSGS`,
    meaningless, which led to the plain-word rule under *Control surface*.
  - The probe was removed from the Move the same day.

### Effects (`fx`)

```
SIZE   DARK   DELAY  DRIVE
SPEED
```

`SPEED` is hidden on the Vibraphone, whose fan has `MOTOR`, and on the Tonewheel
Organ, whose Leslie has `SLOW` and `FAST` (see *One place for everything*). A
gate is one comparison, so the organ has its own Effects level without `SPEED`
(`fx_organ`, gated `equals`), the other is gated `not_equals` the organ, and
within it `SPEED` is gated `not_equals` the Vibraphone.

| Cell | Key | Header | Turning it up |
|---|---|---|---|
| SIZE | `size` | Reverb Size | A larger plate. |
| DARK | `dark` | Reverb Darkness | The plate's damping, from 9 kHz open down to 400 Hz muffled, so it is heard on soft instruments. |
| DELAY | `delay` | Reverb Pre-delay | A longer gap before the reverb starts. |
| DRIVE | `drive` | Drive | Gentle saturation before the reverb. |
| SPEED | `speed` | Sway Speed | `SWAY`'s tremolo, auto-pan or vibrato goes faster. Hidden where it would do nothing. (The Leslie has its own speeds on the organ page.) |

The rotor's spin-up and spin-down follow Herrera, Hanson & Abel and are not a
knob.

## Will it fit

Each block is 128 frames at 44.1 kHz, about 2.9 ms, on a CM4 (Cortex-A72 with
NEON). Up to four synth slots share it. **Target: at most a quarter of the block
with 16 notes on the heaviest instrument.**

| Engine | Cost driver | Estimate |
|---|---|---|
| Modal, pianos | modes × strings × voices | Heaviest: 16 voices × about 80 modes, NEON ×4 |
| Modal, mallets and bells | 4–16 modes per note | Light |
| Waveguide, plucked | one delay line + 2–3 small filters per string | Light |
| Waveguide, bowed | the same + friction junction; the section is ×3 | Moderate |
| Banded | 4–8 bands per note | Moderate |
| Air | two delays + a nonlinearity per voice | Light |
| Banks | fixed generators + Leslie | Flat, whatever is held |
| Synthetic | 4 operators, or 3 singers × 5 resonators | Light to moderate |

**A global budget, not a voice count, is the guard.** Each note costs units:
modes, bands, or waveguide-equivalents. A note takes what its register needs, and
when the pool is full the quietest note is stolen with a short fade. The cost can
never exceed the budget, whatever is played. The budget is **1,536 oscillators**
(`MODAL_BUDGET`).

**Measured on the Move** (2026-10-04, host 1.6.3, `scripts/bench.sh`), as a share
of the 2,902 µs block. Each instrument is at its worst: sixteen notes held low,
with the pedal down and the plate on.

| Instrument | Mean | Mean, µs |
|---|---|---|
| Felt Upright | **14.5 %** | 420 |
| Electric Grand | 14.4 % | 417 |
| Hammered Dulcimer | 13.9 % | 403 |
| Una Corda Grand | 13.7 % | 399 |
| Vibraphone | 7.1 % | 207 |
| Tine Piano | 6.9 % | 201 |
| Kalimba / Music Box | 6.1 % | 177 |
| Reed Piano | 5.7 % | 166 |
| Handbells | 5.0 % | 145 |
| Marimba | 4.9 % | 141 |
| Xylophone | 4.8 % | 138 |
| Glockenspiel | 4.8 % | 140 |
| Handpan | 4.7 % | 137 |
| Tubular Bells | 4.6 % | 133 |
| Celesta | 4.0 % | 116 |
| Tongue Drum | 3.8 % | 111 |
| Toy Piano | 2.9 % | 83 |
| Glass E.Piano | 12.2 % | 354 |
| Flute Organ | 11.6 % | 338 |
| String Ensemble | 7.0 % | 203 |
| Tonewheel Organ | 6.0 % | 174 |
| Harp | 8.3 % | 241 |
| Nylon Guitar | 6.9 % | 200 |
| Clavichord | 4.6 % | 135 |
| Pizzicato | 3.4 % | 98 |
| Effects alone | 2.7 % | 79 |

Remeasured with seventeen instruments (2026-10-04); the four of build step 5
and the four plucked strings of step 6 measured the same day, sixteen keys held. The Flute Organ first measured
21.8 %: its inner loop worked out each pitch (`powf`) and the chiff's
resonator (`cosf`) every sample, and walked all 128 keys. Those now happen once
per block, for the keys that are sounding. The Glass E.Piano's four sines per
voice per sample could be vectorised if room is needed. The four string instruments
use nearly the whole budget of 1,536 oscillators; the rest are light. The tine
and reed pianos' pickups cost about 1 % over a plain bar.

This is inside the quarter-block target, with room for the heavier engines. Two
changes took the pianos from 18.7 % to 14 %:
- Eight modes per pass instead of four, which halves the traffic through the
  accumulator.
- Phasors in place of per-sample `sinf` for the fan and the plate's LFO.

Worst single blocks run to about 1 ms, but they appear on the Mac too and come
from the benchmark sharing the CPU, not from the engine.

## Implementation notes

- **Plugin API v2.** It uses `move_plugin_init_v2` and renders stereo interleaved
  `int16`. Inside it is float, then a soft limiter. `render_block`, `on_midi`
  and every live `set_param` and `get_param` run on the audio callback: no
  `malloc`, file I/O, locks or logging there.
- **Loading.** Since 1.7.0 (#605), `create_instance`, `destroy_instance` and the
  restoring `set_param` run on a background loader thread, and the new instance
  is warmed with two render blocks before it is swapped in. On 1.6.3 they still
  run on the callback. Quilt keeps `create_instance` cheap on both: all
  allocation happens there, there is no file I/O, and the larger tables are
  built per instance.
- **Thread-safe shared tables** (new rule in 1.7.0). One Quilt instance can be
  constructed on the loader while another renders in a different slot. So every
  shared table is `static const`, compiled in: the bell, bar and mode-ratio
  tables, the Hammond gear ratios, and the factory presets. Nothing is
  lazily-initialised global state. Any table that has to be computed lives in
  the instance.
- **No threads.** Quilt creates none. (Since 1.7.0 a thread created in
  `create_instance` would inherit `SCHED_OTHER`, so an audio-producing worker
  would need an explicit policy. Quilt has no use for one.)
- **`ui_hierarchy` and `chain_params` come from `get_param`.** A sound generator's
  hierarchy in `module.json` is ignored by the host. Both fit the 128 KB
  ceiling; see *Host check*.
- **The binary is named `dsp.so`.** A synth slot loads
  `modules/sound_generators/<id>/dsp.so` whatever `module.json`'s `dsp` field
  says (MODULES.md, *module.json*). The probe's first install was named
  `quiltprobe.so` and failed to load on the device, with `dlopen failed` in
  `debug.log`. `tests/run.sh` now checks the name.
- **`type` is the one gate.** It is served and accepted as option names, with
  indices also accepted on write and anything else rejected with a log line.
- **Polyphonic aftertouch** arrives as MIDI `0xA0` per key. Channel pressure
  (`0xD0`) from external controllers is applied to all sounding notes.
  `capabilities.aftertouch: true` is declared in `module.json`.
- **Code layout.** Built so far:
  - `quilt.c`: the entry points, and flush-to-zero while rendering.
  - `instruments.c`: the recipes, shapes and labels, the `TYPE` list, and each
    instrument's voicing.
  - `contract.c`: `chain_params` and `ui_hierarchy`, built in
    `create_instance`.
  - `state.c`.
  - `voice.c`: engine dispatch, pressure routing, the budget, fading ghosts
    for stolen notes, and the 20 ms knob glide.
  - `modal.c` and `contact.c`.
  - `banks.c` (the organs and the string machine) and `fm.c` (the Glass
    E.Piano).
  - `waveguide.c`: the plucked strings, their commuted bodies, and `BLOOM`'s
    open strings.
  - `fx.c`: soundboard, tremolo, drive, the Leslie, plate and tilt.
  - `tools/render` and `tools/bench`.

  Still to come:
  - The bow in `waveguide.c`, then `banded.c`, `air.c`, and the choir's
    formants.
  - `friction.c`, the bow and finger friction junction, shared by the bowed
    strings and the banded engine.
  - The recipes: `instruments.c`, a table of 38 entries, each naming its engine,
    its tables, its defaults and its CHAR key.
- **`state`** is one JSON blob holding the global keys and, for each instrument
  `TYPE` offers, its `CHAR` and own page. Reading it sets every value directly and
  never resets to a voicing, so a saved sound comes back exactly. The chain host
  restores a patch by sending `preset` and then `state` (`chain_patch.c`), and a
  test does the same. Unknown keys are skipped, so older and newer blobs both
  load.
- **Writing `TYPE` again changes nothing.** Only a different instrument loads a
  voicing, so a repeated write cannot wipe what was turned.

## Build order

1. ~~Proposal and this design document.~~ **Done**, 2026-10-04.
2. ~~**Host checks in source:** whether labels can follow `type`, the buffer size,
   and the poly aftertouch path.~~ **Done**, 2026-10-04: all three are favourable
   (see *Host check*). ~~Probe module on the device.~~ **Done**, 2026-10-04:
   labels follow `type` from the knob and from presets, and pad pressure
   arrives. ~~The **scaffold.**~~ **Done**, 2026-10-04, and installed on the
   Move. The probe was removed.
   - `module.json`, the v2 entry points, `state`, the 38 recipes and the nine
     page shapes, with every label in place.
   - Voices, pressure routing (`PRESS`), sustain pedal and stealing. The
     **sound is a placeholder**: three sine partials.
   - `tests/run.sh`: 336 checks through the v2 API, then the host's own
     planner for all 38 instruments, then the label fitter.
   - `tools/render`, which renders a note script to WAV on the Mac
     (`build/render tools/examples/felt.txt out.wav`).
3. ~~**Modal engine, contact and budget.** Felt Upright, Una Corda Grand,
   Vibraphone, Marimba. Then the effects. Then **measure on the Move**.~~
   **Done**, 2026-10-04, and installed on the Move. Demos are in
   `tools/examples/`.
   - **First listening on the device** (2026-10-04): the instruments sounded
     alike, the knobs barely mattered, and some notes clicked. Fixed the same
     day: see *Every knob is heard* and *No pops*. `TYPE` now offers only built
     instruments; the placeholder sound is gone.
   - **Default sounds** (2026-10-04, on request): each instrument arrives with
     its own voicing on every page, all matched in loudness. See *Every
     instrument starts beautiful*.
   - **Controls simplified** (2026-10-04, on request): every knob on one page,
     `CLOSE` removed, `TONE` on Main. See *One place for everything*.
   - The user's second listening found the sounds good.
4. **The rest of the modal engine** (tables only). Each instrument joins `TYPE`
   only when it meets the knob minimums (*Every knob is heard*), has its voicing,
   and matches the others' loudness.
   - ~~Celesta, Toy Piano, Xylophone, Glockenspiel, Kalimba / Music Box.~~
     **Done**, 2026-10-04, installed on the Move, awaiting the user's listening.
     The knob tests caught each of them first: mallets too soft, `TONE`
     inaudible above 600 Hz, buzzers far too loud. See *The engines*.
   - ~~Tubular Bells, Handbells, Handpan, Tongue Drum, Hammered Dulcimer,
     Electric Grand.~~ **Done**, 2026-10-04, installed on the Move, awaiting the
     user's listening. Again the knob tests caught each first. Four mallets were
     too hard for their low-lying modes; `BODY` did nothing without a
     resonator; and the dulcimer was modelled as a piano unison, which hid its
     beating. See *The engines*. 524 checks pass.
   - ~~Tine Piano and Reed Piano, with the pickups.~~ **Done**, 2026-10-04,
     installed on the Move; the user found them good. Building them found
     that the contact could ring up to full scale on hard treble notes; fixed
     for every instrument (see *Stable at any setting*). 575 checks pass.
     **Release 0.2** was not cut; the work went on to step 5.
5. ~~**Banks and synthetic engines:** Tonewheel Organ (with Leslie), Flute Organ,
   String Ensemble, Glass E.Piano.~~ **Done**, 2026-10-04, on request after the
   user found the tine and reed pianos good. Installed on the Move, awaiting the
   user's listening. 701 checks pass. **Release 0.3** waits on the user's
   go-ahead.
6. **Waveguide engine:** ~~Harp, Nylon Guitar, Pizzicato, Clavichord.~~
   **Done**, 2026-10-04, on request after the organ group. Installed on the
   Move, awaiting the user's listening. 807 checks pass. The knob tests caught
   a loss filter that could take a short note's fundamental with it, a body
   some 40 dB louder than its string, and finger noise that loudness matching
   moved. Then the bow and friction: Solo Cello, Solo Violin, String Section.
   **Release 0.4.**
7. **Air engine and banded waveguide engine:** Flute, Pan Flute, Ocarina,
   Recorder, Clarinet, Harmonium; Bowed Vibes, Glass Harmonica, Singing Bowl.
   **Release 0.5.**
8. **Choir.** Then the 114 factory presets, `help.json`, and the catalog entry.
   **Release 1.0: all thirty-eight.**
9. Device revision: cut the pages after the first look.

Unbuilt, in rough order of what they would add:

- Sympathetic string resonance with the pedal down.
- Phantom partials (Bank & Sujbert).
- Per-note "age" detune.
- An open exciter × resonator mode (see *set aside*).
- Chimes triggered by a breeze (non-keyed).

## Testing

`bash tests/run.sh` builds natively with `-Wall -Wextra -Werror` and drives the
module black-box through the v2 API, as `chain_host` would.

- **Contracts.** Every declared key is served. Both JSON blobs parse and fit the
  host's limits. Every enum accepts its label, its index and a float index.
  `state` round-trips exactly. `TYPE` offers exactly the instruments whose
  engine is built.
- **Sound where expected.** Each instrument at three velocities, three registers,
  and with pressure on and off. After release each falls silent within 25 s,
  or, with no dampers to stop it (glockenspiel, dulcimer), is still falling and
  60 dB down, so no note is left hanging. And:
  - It sounds within 10 ms, or within its declared swell time.
  - No NaNs, no denormal stalls, nothing beyond full scale.
  - Bowed and blown notes never self-oscillate after the note ends.
- **Physics checks against the formulas,** not against the module's arithmetic:
  - String partials sit at f₀·k·√(1+Bk²).
  - Bar and bell partials sit at their declared ratios.
  - A soft strike, a light bow and a gentle breath each have a lower spectral
    centroid than their hard versions, for every instrument.
  - Detuned unisons give a two-stage decay.
  - A bowed string reaches Helmholtz motion (a periodic stick–slip) within the
    expected number of periods.
  - (Planned, not a test: the organ's cost with 1 and with 30 keys held. It
    was measured instead; see *Banks*.)
- **Every knob is heard.** Each knob, end to end on every instrument `TYPE`
  offers, meets its minimum in *Every knob is heard*; `BELL` brings the upper
  modes up 12 dB, and `BUZZ` raises the sizzle above 6 kHz by 10 dB, to at least
  −30 dB against the note. These run against the shared defaults, not
  the voicings, so they test the engine.
- **Default sounds.** Turning `TYPE` onto each instrument sets every page to the
  same values as its factory preset, leaves `VOL` alone, and a repeated write
  changes nothing. `state` recalled after `preset`, as the host does it, keeps
  the saved sound.
- **No pops.** Every Main and Effects knob jumped end to end under a sounding
  note adds no spike beyond the note's own. Four notes struck hard together peak
  below −3 dBFS on every instrument.
- **Robustness.** Budget exhaustion steals cleanly. Every key of every
  instrument, struck at full velocity with the hardest and then the softest
  felt, sounds and stays below −2.5 dBFS on its own: none rings up.
- **Cost.** Block time on the Mac in the test, then on the Move with 16 notes on
  each instrument. This is recorded in this document.
- **Listening.** You play it.
