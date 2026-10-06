/*
 * Quilt's knob pictures (DESIGN.md, Knob pictures). Schwung loads this file
 * for any module whose chain_params name a `custom:` viz kind, and calls
 * drawCell for each cell with a frame whose (0,0) is the cell.
 *
 * One rule set for every cell, so a page reads as one hand and stays quiet:
 *
 *   SMALL AND CENTRED. Each picture is drawn in a 32x12 design space and
 *     scaled to 80% about the cell's centre before it becomes pixels, so it
 *     sits in the middle of its cell with room around it.
 *   ONE PEN. Everything is a one-pixel line. What rings (strings, bars,
 *     reeds, air) is a plain line, what plays it (hammer, mallet, finger, bow,
 *     breath) is an outline, and sound itself is dotted.
 *   MOTION MEANS TIME. Only controls that are about time or movement move:
 *     sway, speed, the Leslie, the vibes motor, beating, a bow, a breath. They
 *     move while their knob is touched (when the host says so) or turned, and
 *     hold still otherwise. Every value eases to a new one in 140 ms.
 *
 * A still picture is drawn once and replayed; see pen().
 * The frame is a 32x15 Movy cell.
 */
(function () {
const W = 32, PIC = 11;
/* How long a time control keeps moving after its knob was last turned. */
const AWAKE_MS = 1500;
const clamp01 = (v) => (v < 0 ? 0 : v > 1 ? 1 : v);
const TAU = Math.PI * 2;

/* ---------------------------------------------------------------- memory -- */
/* The renderer is stateless; these remember just enough to ease a value and
 * to keep a running phase continuous when its rate changes. */
const tweens = new Map();
function eased(key, v, now) {
    let t = tweens.get(key);
    if (!t || typeof now !== "number") { t = { from: v, to: v, t0: -1e9 }; tweens.set(key, t); return v; }
    if (t.to !== v) { t.from = shown(t, now); t.to = v; t.t0 = now; }
    return shown(t, now);
}
function shown(t, now) {
    const k = clamp01((now - t.t0) / 140);
    const e = 1 - (1 - k) * (1 - k) * (1 - k);
    return t.from + (t.to - t.from) * e;
}
/* A time control moves only while it is AWAKE: its knob touched (when the
 * host says so) or turned within AWAKE_MS. Asleep, its phase holds still. */
const lastTurn = new Map();
let awake = true;
function noteValue(key, v, now) {
    const l = lastTurn.get(key);
    if (!l) { lastTurn.set(key, { v, t: -1e9 }); return; }
    if (l.v !== v) { l.v = v; l.t = now; }
}
const phases = new Map();
function phase(key, hz, now) {
    let p = phases.get(key);
    if (!p || typeof now !== "number") { p = { ph: 0, t: now || 0 }; phases.set(key, p); }
    const dt = awake ? Math.min(0.25, Math.max(0, (now - p.t) / 1000)) : 0;
    p.ph = (p.ph + hz * dt) % 1; p.t = now;
    return p.ph;
}
/* A small fixed hash, so noise is the same picture every frame. */
function hash(i) { let x = (i * 2654435761) >>> 0; x ^= x >>> 15; x = Math.imul(x, 2246822519) >>> 0; x ^= x >>> 13; return (x >>> 0) / 4294967296; }

/* ------------------------------------------------------------- the pen -- */
function px(c, x, y) { c.fillRect(Math.round(x), Math.round(y), 1, 1, 1); }
function hline(c, x0, x1, y) { c.fillRect(Math.min(x0, x1), y, Math.abs(x1 - x0) + 1, 1, 1); }
function vline(c, x, y0, y1) { c.fillRect(x, Math.min(y0, y1), 1, Math.abs(y1 - y0) + 1, 1); }
function dots(c, x0, x1, y, every) { for (let x = x0; x <= x1; x += every) px(c, x, y); }
function curve(c, x0, x1, f) {            /* y = f(x), joined so steep runs stay solid */
    let py = null;
    for (let x = x0; x <= x1; x++) {
        const y = Math.round(f(x));
        if (py !== null && Math.abs(y - py) > 1) c.line(x - 1, py, x, y, 1); else px(c, x, y);
        py = y;
    }
}
function dotted(c, x0, x1, f, every) { for (let x = x0; x <= x1; x += every || 2) px(c, x, f(x)); }

function spark(c, x, y) { px(c, x, y); px(c, x - 1, y - 1); px(c, x + 1, y - 1); px(c, x - 1, y + 1); px(c, x + 1, y + 1); }

/* ------------------------------------------------------------- motifs -- */
/* The things that play. Each is the ONLY solid object in its picture. */
function hammer(c, x, y, soft) {          /* wood core on a shank, felt under it */
    const fw = 5 + Math.round(soft * 4), fh = 1 + Math.round(soft * 3);
    vline(c, x, 0, y - fh - 3);
    hline(c, x - 2, x + 2, y - fh - 2);                           /* the wooden moulding */
    const l = x - (fw >> 1), r = l + fw - 1, top = y - fh;       /* the felt: an outline that grows */
    hline(c, l + 1, r - 1, top); hline(c, l + 1, r - 1, y);
    if (fh > 1) { vline(c, l, top + 1, y - 1); vline(c, r, top + 1, y - 1); }
    else { px(c, l, y); px(c, r, y); }
    if (soft > 0.5) for (let i = l + 2; i < r - 1; i += 2) px(c, i, top + 2);
}
function mallet(c, x, y, soft) {          /* ball resting on row y */
    const r = 1 + Math.round(soft * 2);
    c.line(x + r, y - r, x + 9, 0, 1);
    c.drawCircle(x, y - r, r, 1); if (soft < 0.5) px(c, x, y - r);
    {
           if (soft > 0.8) { px(c, x - r - 1, y - r); px(c, x + r + 1, y - r - 1); px(c, x, y - 2 * r - 1); } }
}
function bowStroke(c, y, x0, x1) {        /* stick above, hair below, frog at the heel */
    hline(c, x0 + 2, x1, y); hline(c, x0 + 2, x1 - 1, y + 3); vline(c, x1, y + 1, y + 2);
    vline(c, x0 + 1, y, y + 3);
}
/* A string between two bridges, vibrating with amplitude a in mode shape. */
function string(c, y, a, ph, x0, x1, node) {
    x0 = x0 ?? 3; x1 = x1 ?? 28;
    const L = x1 - x0;
    curve(c, x0, x1, (x) => {
        const u = (x - x0) / L;
        let s = Math.sin(Math.PI * u);
        if (node) s = 0.7 * s + 0.3 * Math.sin(Math.PI * u * node);
        return y + a * s * Math.cos(ph * TAU);
    });
    vline(c, x0, y - 1, y + 1); vline(c, x1, y - 1, y + 1);
}
/* Partials as a dotted ladder of stems, heights h(i). */
function ladder(c, xs, hs, base) {
    xs.forEach((x, i) => { const h = Math.round(hs[i]); if (h > 0) vline(c, x, base - h + 1, base); });
}
/* An amplitude band: the outline of a sound's loudness along time, hatched
 * inside. It pinches where the sound dips. */
function band(c, x0, x1, mid, env) {
    for (let x = x0; x <= x1; x++) {
        const a = Math.max(0, Math.round(env(x)));
        px(c, x, mid - a); px(c, x, mid + a);
        if (x % 4 === 0) for (let y = mid - a + 2; y <= mid + a - 2; y += 2) px(c, x, y);
    }
}
function waveTone(c, x0, x1, mid, amp, cyc, shape, ph) {
    curve(c, x0, x1, (x) => {
        const u = ((x - x0) / (x1 - x0)) * cyc + (ph || 0);
        const s = Math.sin(u * TAU);
        return mid - amp * shape(s, u);
    });
}
function envDecay(c, len, x0) {          /* strike then exponential tail */
    x0 = x0 ?? 4; const top = 1, base = 10;
    vline(c, x0, top, base);
    const L = 2 + len * 23;
    curve(c, x0, Math.min(28, x0 + Math.round(L * 1.1)), (x) => base - (base - top) * Math.exp(-3 * (x - x0) / L));
    hline(c, x0, 28, base + 1);
}
function speckle(c, density, seed, x0, x1, y0, y1, twinkle) {
    let i = 0;
    for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++, i++) {
        const r = hash(i + seed * 977 + (twinkle | 0) * 31);
        if (r < density * 0.33) px(c, x, y);
    }
}
function spin(c, cx, cy, r, ang, blades) {
    for (let b = 0; b < blades; b++) {
        const a = ang + b * TAU / blades;
        c.line(cx, cy, cx + Math.round(Math.cos(a) * r), cy + Math.round(Math.sin(a) * r * 0.55), 1);
    }
}

/* -------------------------------------------------------- instruments -- */
const TYPES = ["Felt Upright", "Una Corda Grand", "Electric Grand", "Clavichord", "Tine Piano", "Reed Piano",
    "Celesta", "Toy Piano", "Tonewheel Organ", "Flute Organ", "Harmonium", "Glass E.Piano", "String Ensemble",
    "Vibraphone", "Marimba", "Xylophone", "Glockenspiel", "Tubular Bells", "Handbells", "Handpan", "Tongue Drum",
    "Kalimba / Music Box", "Harp", "Nylon Guitar", "Hammered Dulcimer", "Pizzicato", "Solo Cello", "Solo Violin",
    "String Section", "Bowed Vibes", "Glass Harmonica", "Singing Bowl", "Flute", "Pan Flute", "Ocarina",
    "Recorder", "Clarinet", "Choir"];
function typeName(raw) {
    if (raw == null) return null;
    const n = Number(raw);
    if (Number.isFinite(n) && String(raw).trim() !== "" && TYPES[Math.round(n)]) return TYPES[Math.round(n)];
    return String(raw);
}
/* What PLAYS each instrument — the solid thing in its pictures. */
function player(t) {
    switch (t) {
    case "Vibraphone": case "Marimba": case "Xylophone": case "Glockenspiel": case "Tubular Bells":
    case "Handbells": case "Tongue Drum": case "Hammered Dulcimer": return "mallet";
    case "Handpan": case "Kalimba / Music Box": case "Harp": case "Nylon Guitar": case "Pizzicato": case "Clavichord": return "finger";
    case "Solo Cello": case "Solo Violin": case "String Section": case "Bowed Vibes": case "Glass Harmonica": case "Singing Bowl": return "bow";
    case "Flute": case "Pan Flute": case "Ocarina": case "Recorder": case "Clarinet": case "Harmonium": case "Flute Organ": return "breath";
    case "Tonewheel Organ": return "organ";
    case "String Ensemble": return "ensemble";
    case "Choir": return "voice";
    default: return "hammer";
    }
}
/* What RINGS: how a sustained instrument's sway behaves. */
function swayKind(t) {
    const p = player(t);
    if (p === "organ") return "leslie";
    if (p === "bow" || p === "breath" || p === "voice" || p === "ensemble") return "vibrato";
    return "tremolo";
}
const sustained = (t) => { const p = player(t); return p === "bow" || p === "breath" || p === "voice" || p === "organ" || p === "ensemble"; };

/* ------------------------------------------------------------ icons -- */
/* TYPE draws the instrument. 22x11 sketches, one pen. */
const ICON = {
    upright(c, x) { c.drawArc(x + 3, 3, 3, 270, 90, 1); hline(c, x + 3, x + 19, 0); vline(c, x, 3, 10); vline(c, x + 19, 0, 10);
        hline(c, x, x + 19, 6); for (let i = 0; i < 10; i++) vline(c, x + 1 + i * 2, 7, i % 3 === 1 ? 8 : 10); hline(c, x, x + 19, 10); },
    grand(c, x) { c.line(x, 8, x + 4, 1, 1); hline(c, x + 4, x + 12, 1); c.drawArc(x + 15, 6, 5, 300, 160, 1); hline(c, x, x + 20, 8);
        for (let i = 0; i < 10; i++) px(c, x + 1 + i * 2, 9); hline(c, x, x + 20, 10); c.line(x + 4, 1, x + 10, 6, 1); },
    tine(c, x) { for (let i = 0; i < 4; i++) { hline(c, x + 2, x + 13 + i * 2, 1 + i * 3); vline(c, x + 14 + i * 2, i * 3, i * 3 + 2); } vline(c, x + 1, 0, 10); },
    organ(c, x) { [5, 3, 6, 2].forEach((n, i) => { const xx = x + 3 + i * 5; vline(c, xx, 0, n); hline(c, xx - 1, xx + 1, n); });
        hline(c, x, x + 20, 8); for (let i = 2; i < 20; i += 3) px(c, x + i, 9); hline(c, x, x + 20, 11); vline(c, x, 8, 11); vline(c, x + 20, 8, 11); },
    pipes(c, x) { [10, 8, 6, 5, 6, 8, 10].forEach((n, i) => { const xx = x + 1 + i * 3; vline(c, xx, 10 - n, 10); vline(c, xx + 2, 10 - n, 10); px(c, xx + 1, 10 - n); px(c, xx + 1, 8); }); },
    bars(c, x) { [10, 9, 8, 7, 6, 5, 4].forEach((n, i) => { const xx = x + i * 3; vline(c, xx, 10 - n, 10); }); },
    tubes(c, x) { [10, 9, 8, 7].forEach((n, i) => { const xx = x + 3 + i * 4; hline(c, xx, xx + 2, 0); vline(c, xx, 0, n); vline(c, xx + 2, 0, n); hline(c, xx, xx + 2, n); }); hline(c, x + 1, x + 19, 0); },
    bell(c, x) { c.drawArc(x + 10, 5, 4, 270, 180, 1); vline(c, x + 6, 5, 8); vline(c, x + 14, 5, 8); c.line(x + 6, 8, x + 4, 10, 1); c.line(x + 14, 8, x + 16, 10, 1); hline(c, x + 4, x + 16, 10); vline(c, x + 10, 0, 1); },
    pan(c, x) { c.drawArc(x + 10, 9, 9, 270, 180, 1); c.drawArc(x + 10, 6, 2, 0, 360, 1); hline(c, x + 1, x + 19, 9); for (const d of [-6, 6]) c.drawArc(x + 10 + d, 7, 1, 0, 360, 1); },
    kalimba(c, x) { c.drawArc(x + 10, 6, 5, 90, 180, 1); hline(c, x + 2, x + 18, 5); hline(c, x + 2, x + 18, 10); vline(c, x + 2, 5, 10); vline(c, x + 18, 5, 10);
        [3, 5, 7, 8, 7, 5, 3].forEach((n, i) => vline(c, x + 4 + i * 2, 9 - n, 8)); },
    harp(c, x) { c.line(x + 2, 10, x + 17, 0, 1); vline(c, x + 2, 1, 10); c.line(x + 2, 1, x + 17, 0, 1); for (let i = 1; i < 7; i++) vline(c, x + 2 + i * 2, 1, 10 - Math.round(i * 1.4)); hline(c, x, x + 6, 10); },
    guitar(c, x) { c.drawCircle(x + 5, 6, 4, 1); c.drawCircle(x + 5, 6, 1, 1); hline(c, x + 9, x + 20, 5); hline(c, x + 9, x + 20, 7); vline(c, x + 20, 4, 8); },
    cello(c, x) { c.drawArc(x + 10, 3, 3, 270, 180, 1); c.drawArc(x + 10, 8, 4, 90, 180, 1); vline(c, x + 6, 3, 8); vline(c, x + 14, 3, 8);
        hline(c, x + 6, x + 14, 10); px(c, x + 8, 6); px(c, x + 12, 6); vline(c, x + 10, 0, 9); c.line(x + 1, 9, x + 19, 2, 1); },
    glass(c, x) { c.drawArc(x + 10, 2, 5, 90, 180, 1); hline(c, x + 5, x + 15, 2); vline(c, x + 10, 7, 9); hline(c, x + 7, x + 13, 10); },
    bowl(c, x) { c.drawArc(x + 10, 3, 7, 90, 180, 1); hline(c, x + 3, x + 17, 3); c.line(x + 18, 0, x + 13, 5, 1); hline(c, x + 7, x + 13, 10); },
    flute(c, x) { hline(c, x, x + 21, 4); hline(c, x, x + 21, 7); vline(c, x, 4, 7); vline(c, x + 21, 4, 7); px(c, x + 3, 5); px(c, x + 4, 6); for (let i = 0; i < 4; i++) px(c, x + 9 + i * 3, 5); },
    panflute(c, x) { [10, 9, 8, 7, 6, 5].forEach((n, i) => { const xx = x + 2 + i * 3; vline(c, xx, 0, n); vline(c, xx + 2, 0, n); hline(c, xx, xx + 2, n); }); hline(c, x + 1, x + 19, 3); },
    ocarina(c, x) { c.drawArc(x + 9, 6, 6, 0, 360, 1); hline(c, x + 15, x + 20, 5); hline(c, x + 15, x + 20, 7); px(c, x + 6, 4); px(c, x + 9, 3); px(c, x + 12, 4); px(c, x + 8, 8); },
    clarinet(c, x) { c.line(x, 2, x + 15, 7, 1); c.line(x, 4, x + 15, 9, 1); c.drawArc(x + 18, 8, 3, 0, 360, 1); for (let i = 0; i < 4; i++) px(c, x + 4 + i * 3, 3 + i); },
    choir(c, x) { for (let i = 0; i < 3; i++) { const xx = x + 3 + i * 7, yy = i === 1 ? 2 : 4; c.drawCircle(xx, yy, 2, 1); c.drawArc(xx, yy + 7, 3, 270, 180, 1); px(c, xx, yy + 1); } },
    clavi(c, x) { hline(c, x, x + 20, 3); hline(c, x, x + 20, 10); vline(c, x, 3, 10); vline(c, x + 20, 3, 10); for (let i = 0; i < 8; i++) vline(c, x + 2 + i * 2, 7, i % 2 ? 8 : 10); for (let i = 0; i < 3; i++) hline(c, x + 2, x + 18, 4 + i); },
};
const TYPE_ICON = {
    "Felt Upright": "upright", "Una Corda Grand": "grand", "Electric Grand": "grand", "Clavichord": "clavi",
    "Tine Piano": "tine", "Reed Piano": "tine", "Celesta": "upright", "Toy Piano": "upright",
    "Tonewheel Organ": "organ", "Flute Organ": "pipes", "Harmonium": "pipes", "Glass E.Piano": "tine",
    "String Ensemble": "cello", "Vibraphone": "bars", "Marimba": "bars", "Xylophone": "bars", "Glockenspiel": "bars",
    "Tubular Bells": "tubes", "Handbells": "bell", "Handpan": "pan", "Tongue Drum": "pan", "Kalimba / Music Box": "kalimba",
    "Harp": "harp", "Nylon Guitar": "guitar", "Hammered Dulcimer": "clavi", "Pizzicato": "cello", "Solo Cello": "cello",
    "Solo Violin": "cello", "String Section": "cello", "Bowed Vibes": "bars", "Glass Harmonica": "glass",
    "Singing Bowl": "bowl", "Flute": "flute", "Pan Flute": "panflute", "Ocarina": "ocarina", "Recorder": "flute",
    "Clarinet": "clarinet", "Choir": "choir",
};

/* ------------------------------------------------------------ drawers -- */
/* Each takes (c, v, s): v is the eased value in 0..1 and s carries the
 * instrument, the time and the raw value. Each draws its picture only; the
 * pen scales it into the cell. */
const D = {};

/* MAIN ------------------------------------------------------------------ */
D.type = (c, v, s) => {
    /* The instrument, sliding in from the side you turned towards. */
    const t = s.type || "Felt Upright";
    const tw = s.typeTween;
    const draw = (name, dx) => { const ic = ICON[TYPE_ICON[name] || "upright"]; const sub = offset(c, dx); ic(sub, 5); };
    if (tw && tw.k < 1) {
        const dir = tw.dir, d = Math.round((1 - tw.k) * 26) * dir;
        draw(tw.from, d - 26 * dir); draw(t, d);
    } else draw(t, 0);
};
function offset(c, dx) {                 /* the same pen, shifted and clipped to the picture */
    return { fillRect: (x, y, w, h, col) => {
            let a = x + dx, b = a + w; if (a < 0) a = 0; if (b > W) b = W; if (b <= a) return;
            if (y > PIC) return; c.fillRect(a, y, b - a, Math.min(h, PIC + 1 - y), col); },
        line(x0, y0, x1, y1) { lineVia(this, x0, y0, x1, y1); },
        fillCircle(x, y, r) { for (let dy = -r; dy <= r; dy++) { const h = Math.floor(Math.sqrt(r * r - dy * dy)); this.fillRect(x - h, y + dy, 2 * h + 1, 1, 1); } },
        drawCircle(x, y, r) { arcVia(this, x, y, r, 0, 360); },
        drawArc(x, y, r, a, sw) { arcVia(this, x, y, r, a, sw); } };
}
function lineVia(c, x0, y0, x1, y1) {
    let dx = Math.abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -Math.abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (let g = 0; g < 512; g++) { c.fillRect(x0, y0, 1, 1, 1); if (x0 === x1 && y0 === y1) break; const e2 = 2 * err; if (e2 >= dy) { err += dy; x0 += sx; } if (e2 <= dx) { err += dx; y0 += sy; } }
}
function arcVia(c, x, y, r, start, sweep) {
    const plot = (dx, dy) => { if (sweep < 360) { let a = Math.atan2(dx, -dy) * 180 / Math.PI; if (a < 0) a += 360; let d = a - start; if (d < 0) d += 360; if (d > sweep) return; } c.fillRect(x + dx, y + dy, 1, 1, 1); };
    for (let dy = -r; dy <= r; dy++) { const dx = Math.round(Math.sqrt(r * r - dy * dy)); plot(dx, dy); if (dx) plot(-dx, dy); }
    for (let dx = -r; dx <= r; dx++) { const dy = Math.round(Math.sqrt(r * r - dx * dx)); plot(dx, dy); if (dy) plot(dx, -dy); }
}

D.soft = (c, v, s) => {
    /* What plays the note, from hard to soft. The dent it leaves in the
     * string widens as it softens: a soft touch stays longer. */
    const p = player(s.type);
    if (p === "organ") {                  /* fewer bright drawbars */
        for (let i = 0; i < 9; i++) { const keep = clamp01((1 - v) * 9 - i + 2.5); const h = Math.round([6, 5, 9, 8, 6, 7, 5, 5, 4][i] * keep);
            if (h > 0) vline(c, 4 + i * 3, 10 - h, 10); }
        hline(c, 2, 29, 11); return;
    }
    if (p === "breath" || p === "voice") { /* the air jet: one hard line, or a wide gentle fan */
        c.drawArc(2, 6, 3, 30, 120, 1);
        hline(c, 6, 27, 6);
        const spread = 1 + v * 4;
        if (v > 0.1) for (const k of [-1, 1]) dotted(c, 10, 27, (x) => 6 + k * spread * (x - 9) / 18, 2);
        if (v > 0.55) for (const k of [-1, 1]) dotted(c, 15, 27, (x) => 6 + k * spread * 1.9 * (x - 14) / 13, 3);
        return;
    }
    if (p === "bow" || p === "ensemble") { /* hair pressure: the string bends under a heavy bow */
        const bend = (1 - v) * 3;
        curve(c, 3, 28, (x) => 9 - bend * Math.max(0, 1 - Math.abs(x - 16) / 9));
        bowStroke(c, Math.round(4 - bend), 5, 27);
        return;
    }
    const dent = 0.4 + v * 1.2, w = 2 + v * 6;
    curve(c, 3, 28, (x) => 10 + dent * Math.max(0, 1 - Math.abs(x - 16) / w));
    vline(c, 3, 9, 11); vline(c, 28, 9, 11);
    if (p === "mallet") mallet(c, 16, 9, v);
    else if (p === "finger") {
        if (v < 0.4) { c.line(13, 3, 16, 9, 1); c.line(19, 3, 16, 9, 1); hline(c, 13, 19, 3); }   /* a pick */
        else { const r = 2 + Math.round(v * 1.5); c.drawCircle(16, 9 - r, r, 1); vline(c, 16, 0, 9 - 2 * r - 1); }
    } else hammer(c, 16, 9, v);
};

D.decay = (c, v, s) => {
    /* How long a note lasts: a strike and its tail, or for a sustained
     * instrument the hold and the release after you let go. A spark rides
     * the curve at the speed it really falls. */
    const L = 3 + v * 22;
    let f, x0 = 4;
    if (sustained(s.type)) { hline(c, 3, 9, 2); vline(c, 3, 2, 10); x0 = 9; f = (x) => 2 + 8 * (1 - Math.exp(-2.5 * (x - x0) / L)); px(c, 9, 0); px(c, 9, 1); }
    else { vline(c, 4, 1, 10); f = (x) => 10 - 9 * Math.exp(-3 * (x - x0) / L); }
    curve(c, x0, 28, f);
    const sec = 0.4 + v * 3.2;
    const ph = phase(s.key, 1 / (sec + 0.6), s.now);
    const u = ph * (sec + 0.6) / sec;
    if (awake && u < 1) { const x = Math.round(x0 + u * (28 - x0)); spark(c, x, Math.round(f(x))); }
};

D.sway = (c, v, s) => {
    /* The instrument's own movement, at the speed set on the Effects page:
     * a tremolo's swell, a player's vibrato, or the organ's Leslie turning. */
    const k = swayKind(s.type), sp = s.speed;
    if (k === "leslie") {
        const hz = 0.15 + sp * 1.6 * (0.3 + v);
        const a = phase(s.key, hz, s.now) * TAU;
        c.drawArc(16, 6, 6, 0, 360, 1);
        const hx = Math.round(Math.cos(a) * 5), hy = Math.round(Math.sin(a) * 3);
        c.line(16 - hx, 6 - hy, 16 + hx, 6 + hy, 1); c.drawCircle(16 + hx, 6 + hy, 1, 1);
        if (v > 0.1) dotted(c, 25, 29, (x) => 6 + Math.sin(x + a * 3) * v * 3, 2);
        return;
    }
    const hz = 0.4 + sp * 3;
    const ph = phase(s.key, hz, s.now);
    if (k === "vibrato") {
        waveTone(c, 2, 29, 6, 1 + v * 4, 1.5, (x) => x, -ph * 1.5);
    } else {                              /* tremolo: the swell travels along the note */
        band(c, 2, 29, 6, (x) => 5 * (1 - v * 0.85 * (0.5 + 0.5 * Math.cos(((x - 2) / 27 * 2 - ph) * TAU))));
    }
};

D.tone = (c, v) => {
    /* A tilt across the spectrum, pivoting in the middle: dark falls to the right, bright rises. */
    const tilt = (v - 0.5) * 2;
    const f = (x) => 6 - tilt * (x - 16) * 0.32;
    curve(c, 3, 28, f);
    for (let x = 4; x <= 28; x += 4) for (let y = Math.round(f(x)) + 3; y <= 11; y += 3) px(c, x, y);
    c.drawCircle(16, 6, 1, 1);
};

D.space = (c, v, s) => {
    /* A sound in a room: the more space, the more echoes roll outward. */
    vline(c, 3, 5, 7); for (let y = 0; y <= 11; y += 3) px(c, 28, y);      /* the source, and the far wall */
    const n = Math.ceil(v * 6 - 0.05), ph = phase(s.key, 0.6, s.now), o = offset(c, 0);
    for (let i = 0; i < n; i++) {
        const r = Math.round(3 + (i + ph) * 4);
        if (r > 26) continue;
        if (i < 3) o.drawArc(4, 6, r, 35, 110); else arcDots(o, 4, 6, r);
    }
};
function arcDots(o, x, y, r) { for (let a = 35; a <= 145; a += 90 / r) { const t = a * Math.PI / 180; if (Math.round(a * r / 60) % 2) o.fillRect(Math.round(x + Math.sin(t) * r), Math.round(y - Math.cos(t) * r), 1, 1, 1); } }

D.volume = (c, v, s) => {
    /* A wedge, filled to the level, with a mark at 0 dB. */
    const n = clamp01(v);
    for (let x = 3; x <= 28; x++) { const h = Math.round(1 + (x - 3) * 0.4); const y0 = 11 - h;
        if ((x - 3) / 25 <= n) { px(c, x, y0); px(c, x, 11); if (x % 3 === 0) vline(c, x, y0, 11); } else if (x % 2 === 0) px(c, x, y0); }
    const z = 3 + Math.round(25 * 60 / 66); vline(c, z, 0, 1);
};

/* CHAR, one per instrument -------------------------------------------- */
const C = {};
D.char = (c, v, s) => { const f = C[s.inst] || C.default; f(c, v, s); };
C.default = (c, v) => { waveTone(c, 3, 28, 6, 4, 2, (x) => x); };
C.felt_upright = (c, v) => {                 /* the felt strip slides in under the hammer */
    string(c, 10, 0, 0); hammer(c, 16, 6, 0.2);
    const w = Math.round(v * 22); if (w > 0) for (let x = 5; x < 5 + w; x++) { px(c, x, 8); if (x % 2) px(c, x, 7); }
};
C.una_corda_grand = (c, v) => {              /* seen from above: the hammer slides off one string, then two */
    const hit = 3 - Math.min(2, Math.floor(v * 3));
    for (let i = 0; i < 3; i++) { const y = 4 + i * 3, rings = i >= 3 - hit;
        if (rings) curve(c, 3, 28, (x) => y + 0.8 * Math.sin(Math.PI * (x - 3) / 25)); else hline(c, 3, 28, y); }
    const top = 4 + (3 - hit) * 3 - 1;
    vline(c, 15, top, 11); hline(c, 14, 16, top);
};
C.electric_grand = (c, v) => {               /* a pickup under the string; the wave sharpens */
    hline(c, 3, 28, 4); hline(c, 8, 11, 6); px(c, 8, 7); px(c, 11, 7);
    waveTone(c, 3, 28, 9, 2, 2, (x) => Math.tanh(x * (1 + v * 5)) / Math.tanh(1 + v * 5));
};
C.clavichord = (c, v, s) => {                /* bebung: pressure bends the pitch */
    const ph = phase(s.key, 1.2, s.now);
    waveTone(c, 3, 28, 6, 1 + v * 3, 1, (x) => x, ph);
    hline(c, 15, 17, 0); px(c, 16, 1);
};
C.tine_piano = (c, v) => {                   /* the tine moves towards the pickup: bark */
    hline(c, 3, 20, 3); vline(c, 21, 2, 4); vline(c, 25 - Math.round(v * 2), 1, 5); vline(c, 27 - Math.round(v * 2), 1, 5);
    waveTone(c, 3, 28, 9, 2, 2, (x) => (x > 0 ? x * (1 + v) : x * (1 - v * 0.5)) / (1 + v * 0.4));
};
C.reed_piano = (c, v) => {                   /* the reed and its plate close in: bite */
    const gap = 4 - Math.round(v * 3);
    hline(c, 3, 22, 3); vline(c, 3, 2, 4); hline(c, 10, 28, 3 + gap); px(c, 10, 4 + gap); px(c, 28, 4 + gap);
    waveTone(c, 3, 28, 9, 2, 2, (x) => Math.sign(x) * Math.pow(Math.abs(x), 1 - v * 0.7));
};
const bellLadder = (c, v, extra) => {
    const xs = [5, 9, 13, 16, 20, 23, 26], hs = [9, 6, 5, 4, 3, 3, 2];
    ladder(c, xs, hs.map((h, i) => (i < 2 ? h : h * (0.25 + v * 1.1))), 11);
    if (extra) extra();
};
C.celesta = C.toy_piano = C.glockenspiel = (c, v) => bellLadder(c, v);
C.glass_e_piano = (c, v) => bellLadder(c, v, () => { if (v > 0.5) c.drawCircle(26, 2, 1, 1); });
const chorus = (c, v, s, n) => {             /* three voices, drifting against each other */
    const ph = phase(s.key, 0.2 + v * 0.4, s.now);
    for (let i = 0; i < 3; i++) {
        const d = Math.sin((ph + i / 3) * TAU) * v * 0.5, y = 2 + i * 4;
        curve(c, 3, 28, (x) => y - 1.5 * Math.sin((((x - 3) / 25) * 2.5 + d) * TAU));
    }
};
C.tonewheel_organ = (c, v, s) => chorus(c, v, s, 3);
C.string_ensemble = (c, v, s) => chorus(c, v, s, 3);
const beats = (c, v, s) => {                  /* two pitches beating: the loudness pinches, and the pinch travels */
    const ph = phase(s.key, 0.25 + v * 1.2, s.now), k = 0.5 + v * 2.5;
    band(c, 3, 28, 6, (x) => 5 * (1 - (0.15 + v * 0.8) * (0.5 + 0.5 * Math.cos(((x - 3) / 25 * k - ph) * TAU))));
};
C.harmonium = C.singing_bowl = beats;
const chiff = (c, v) => {                    /* a puff of air bursts ahead of the note */
    const r = 1 + v * 4, n = Math.round(4 + v * 14);
    for (let i = 0; i < n; i++) { const a = i * 2.4, rr = r * (0.4 + 0.6 * hash(i)); px(c, 6 + Math.cos(a) * rr, 6 + Math.sin(a) * rr * 0.8); }
    waveTone(c, 12, 28, 6, 3 + v, 2, (x) => x);
};
C.flute_organ = C.pan_flute = C.recorder = chiff;
C.vibraphone = (c, v, s) => {                /* the fan turns at the motor's speed */
    hline(c, 2, 29, 9); hline(c, 2, 29, 11); vline(c, 2, 9, 11); vline(c, 29, 9, 11);
    const a = phase(s.key, v * 4, s.now) * TAU;
    c.drawArc(16, 4, 4, 0, 360, 1); spin(c, 16, 4, 4, a, 2);
};
C.marimba = C.xylophone = (c, v, s) => {     /* a roll: strikes come faster */
    hline(c, 3, 28, 10); hline(c, 3, 28, 11);
    const n = 1 + Math.round(v * 6), ph = phase(s.key, 1 + v * 6, s.now);
    for (let i = 0; i < n; i++) { const x = 4 + Math.round(i * 23 / Math.max(1, n - 1)); vline(c, x, 8 - (i === Math.floor(ph * n) ? 2 : 0), 8); }
    mallet(c, 4 + Math.round(Math.floor(ph * n) * 23 / Math.max(1, n - 1)), 9 - 3, 0.3);
};
C.tubular_bells = (c, v) => {                 /* a hand closes on the tube: the ring shortens */
    vline(c, 6, 0, 11); vline(c, 9, 0, 11); hline(c, 6, 9, 0);
    const L = 3 + (1 - v) * 16; curve(c, 12, 28, (x) => 6 - 4 * Math.exp(-3 * (x - 12) / L) * Math.sin(x * 1.4));
    if (v > 0.05) { vline(c, 4, 6 - Math.round(v * 3), 8); vline(c, 11, 6 - Math.round(v * 3), 8); }
};
C.handbells = (c, v) => {                     /* the minor third partial rises */
    const xs = [5, 10, 13, 17, 22, 26], hs = [9, 6, 3 + v * 6, 5, 3, 2];
    ladder(c, xs, hs, 11); if (v > 0.3) c.drawArc(13, 11 - Math.round(hs[2]) - 1, 1, 0, 360, 1);
};
C.handpan = C.tongue_drum = (c, v) => {       /* rings bloom across the shell */
    c.drawArc(16, 12, 13, 290, 140, 1);
    const n = 1 + Math.round(v * 3);
    for (let i = 0; i < n; i++) c.drawArc(16, 8, 1 + i * 2, 0, 360, 1);
};
C.kalimba_music_box = (c, v) => {             /* the buzzers rattle on the tine */
    vline(c, 6, 1, 11); hline(c, 6, 26, 6);
    const a = v * 3; for (let x = 8; x <= 26; x++) px(c, x, 6 + ((x % 2) ? -a : a) * (x > 14 ? 1 : 0.3));
};
const bloom = (c, v) => {                     /* the played string, and its neighbours ringing along */
    curve(c, 3, 28, (x) => 2 + 1.5 * Math.sin(Math.PI * (x - 3) / 25));
    for (let i = 1; i < 4; i++) { const y = 2 + i * 3, a = clamp01(v * 3 - (i - 1)) * 1.1;
        if (a > 0.15) curve(c, 3, 28, (x) => y + a * Math.sin(Math.PI * (x - 3) / 25)); else hline(c, 3, 28, y); }
    vline(c, 3, 1, 11); vline(c, 28, 1, 11);
};
C.harp = C.nylon_guitar = C.hammered_dulcimer = bloom;
C.pizzicato = (c, v) => {                     /* viola to cello: the wave grows longer and deeper */
    waveTone(c, 3, 28, 6, 2.5 + v * 2.5, 3 - v * 1.5, (x) => x);
};
const veil = (c, v) => {                      /* the bow moves from the bridge up over the fingerboard */
    hline(c, 3, 28, 6); vline(c, 28, 4, 8);
    hline(c, 3, 11, 3); hline(c, 3, 11, 9); vline(c, 11, 3, 9);
    const x = 23 - Math.round(v * 13);
    vline(c, x, 0, 11); vline(c, x + 3, 1, 11); hline(c, x, x + 3, 0);
};
C.solo_cello = C.solo_violin = veil;
C.string_section = (c, v) => {                /* the players spread across the room */
    for (let i = 0; i < 7; i++) { const u = (i - 3) / 3, x = 16 + Math.round(u * (3 + v * 10)), y = 3 + (i % 2) * 3;
        px(c, x, y); px(c, x - 1, y + 2); px(c, x + 1, y + 2); }
    hline(c, 3, 28, 11);
};
C.bowed_vibes = (c, v) => {                   /* the bow bites into the bar's edge */
    hline(c, 3, 28, 9); hline(c, 3, 28, 11);
    c.line(12, 0, 20, 9 - Math.round(v * 2), 1); c.line(13, 0, 21, 9 - Math.round(v * 2), 1);
};
C.glass_harmonica = (c, v) => {               /* a wet finger on the rim; the wetter, the more drops */
    c.drawArc(16, -3, 11, 120, 120, 1); hline(c, 6, 26, 2);
    c.drawCircle(10, 1, 2, 1);
    const n = Math.round(v * 5); for (let i = 0; i < n; i++) px(c, 13 + i * 3, 4 + (i % 2) * 2 + (i > 2 ? 1 : 0));
};
C.flute = (c, v) => {                         /* the lip covers the embouchure hole */
    c.drawCircle(16, 7, 3, 1); const cover = Math.round(v * 5);
    for (let y = 4; y < 4 + cover; y++) hline(c, 12, 20, y);
    dotted(c, 3, 11, () => 5, 2);
};
C.ocarina = (c, v) => {                       /* breath becomes a pure tone */
    for (let x = 3; x <= 28; x++) { const n = (hash(x) - 0.5) * 4 * (1 - v); px(c, x, Math.round(6 - 4 * Math.sin((x - 3) / 25 * TAU * 2) + n)); }
};
C.clarinet = (c, v) => {                      /* a stiffer reed squares the wave */
    c.line(3, 2, 10, 4, 1); c.line(3, 2 + Math.round((1 - v) * 2), 10, 5, 1);
    waveTone(c, 12, 28, 6, 4, 1.5, (x) => Math.tanh(x * (1 + v * 6)) / Math.tanh(1 + v * 6));
};
C.choir = (c, v) => {                         /* oo, oh, ah: the mouth opens */
    const rx = 2 + Math.round(v * 5), ry = 4 - Math.round(v * 1) + Math.round(v * 2);
    for (let a = 0; a < 360; a += 8) px(c, 16 + Math.cos(a * Math.PI / 180) * rx, 6 + Math.sin(a * Math.PI / 180) * ry);
};

/* INSTRUMENT PAGES ------------------------------------------------------ */
D.split = (c, v, s) => {                      /* copies drift apart and beat */
    const ph = phase(s.key, v * 1.5, s.now);
    waveTone(c, 3, 28, 6, 4, 2, (x) => x);
    if (v > 0.01) dotted(c, 3, 28, (x) => 6 - 4 * Math.sin(((x - 3) / 25 * 2 * (1 + v * 0.15) + ph) * TAU), 2);
};
D.stiff = (c, v) => {                         /* overtones stretch sharp */
    const xs = [], hs = [];
    for (let k = 1; k <= 8; k++) { const f = k * Math.sqrt(1 + v * 0.03 * k * k); xs.push(Math.round(2 + f * 3.1)); hs.push(10 - k); }
    ladder(c, xs.filter((x) => x <= 29), hs, 11);
    for (let k = 1; k <= 8; k++) px(c, 2 + k * 3.1, 11);
};
D.spot = (c, v, s) => {                       /* where the string is struck, and the shape it rings in */
    const x = 4 + Math.round(v * 11);
    string(c, 8, 2, 0, 3, 28, 1 + v * 4);
    const p = player(s.type);
    if (p === "finger") { c.line(x - 1, 2, x, 6, 1); c.line(x + 1, 2, x, 6, 1); }
    else if (p === "mallet") c.drawCircle(x, 3, 2, 1);
    else { hline(c, x - 1, x + 1, 4); vline(c, x, 0, 3); }
};
D.body = (c, v, s) => {                        /* how much the box under the strings sings */
    const p = player(s.type);
    hline(c, 3, 28, 2);
    const h = 2 + Math.round(v * 7);
    if (p === "finger" || p === "bow") { c.drawArc(16, 3 + h, h, 270, 180, 1); hline(c, 16 - h, 16 + h, 3 + h); c.drawCircle(16, 3 + Math.round(h / 2), Math.max(1, h >> 2), 1); }
    else { hline(c, 4, 27, 3 + h); vline(c, 4, 3, 3 + h); vline(c, 27, 3, 3 + h); for (let x = 6; x < 27; x += 3) dotted(c, 4, 4, () => 0, 1); dots(c, 6, 25, 3 + (h >> 1) + 1, 3); }
};
D.noise = (c, v, s) => {                      /* the scratches and thumps around the note */
    const tw = Math.floor(phase(s.key, 5.5, s.now) * 7);
    waveTone(c, 3, 28, 6, 2, 2, (x) => x);
    speckle(c, v * 0.8, 3, 3, 28, 0, 2, tw); speckle(c, v * 0.8, 5, 3, 28, 10, 11, tw);
};
D.damp = (c, v) => {                          /* the damper settles onto the string */
    const y = 1 + Math.round(v * 4);
    hline(c, 12, 19, y); hline(c, 12, 19, y + 2); px(c, 12, y + 1); px(c, 19, y + 1);
    string(c, 9, 2 * (1 - v), 0);
};
D.pedal = (c, v) => {                         /* the pedal goes down; the dampers lift */
    const lift = Math.round(v * 3);
    for (let i = 0; i < 4; i++) { hline(c, 5 + i * 6, 8 + i * 6, 3 - lift); vline(c, 6 + i * 6, 0, 3 - lift); }
    hline(c, 3, 28, 6);
    c.line(8, 9, 24, 9 + Math.round(v * 2), 1); hline(c, 22, 27, 9 + Math.round(v * 2)); px(c, 27, 10 + Math.round(v * 2)); px(c, 8, 10); px(c, 8, 11);
};
D.edge = (c, v) => {                          /* brightness: a sine grows corners */
    waveTone(c, 3, 28, 6, 4, 2, (x, u) => { const saw = 2 * (u % 1) - 1; return x * (1 - v) + saw * v; });
};
/* organ */
D.ping = (c, v) => {                          /* percussion: a bright blip over the note */
    ladder(c, [6, 10, 14, 18, 22, 26], [6, 5, 3 + v * 7, 3, 2, 2], 11);
    if (v > 0.05) { px(c, 13, 0); px(c, 15, 0); px(c, 14, 0 - 1); }
};
D.tail = (c, v) => { envDecay(c, v * 0.6); };
D.click = (c, v) => {                          /* the key contacts close with a spark */
    hline(c, 3, 13, 9); c.line(13, 9, 20, 5, 1); hline(c, 21, 28, 9);
    const n = Math.round(v * 6); for (let i = 0; i < n; i++) { const a = (i / 6) * Math.PI - Math.PI; c.line(21, 8, 21 + Math.round(Math.cos(a) * 4), 8 + Math.round(Math.sin(a) * 4), 1); }
};
D.leak = (c, v) => {                           /* neighbouring wheels bleed in */
    c.drawCircle(16, 6, 4, 1); px(c, 16, 6);
    for (const dx of [-11, 11]) { const r = 3; if (v > 0.05) for (let a = 0; a < 360; a += 360 / (4 + Math.round(v * 14))) px(c, 16 + dx + Math.cos(a * Math.PI / 180) * r, 6 + Math.sin(a * Math.PI / 180) * r); }
};
const leslie = (rate) => (c, v, s) => {        /* the horn turns at this speed */
    const hz = rate * (0.2 + v * 1.5);
    const a = phase(s.key, hz, s.now) * TAU;
    c.drawArc(16, 6, 6, 0, 360, 1);
    const hx = Math.round(Math.cos(a) * 5), hy = Math.round(Math.sin(a) * 3);
    c.line(16 - hx, 6 - hy, 16 + hx, 6 + hy, 1); c.drawCircle(16 + hx, 6 + hy, 1, 1);
};
D.slow = leslie(0.5); D.fast = leslie(4);
/* DRAWBARS: the real thing, pulled out. Brown subs are hatched, white
 * octaves hollow, black non-octaves solid, as on a Hammond. */
const BAR_KIND = { o_16: "sub", o_513: "sub", o_8: "white", o_4: "white", o_2: "white", o_1: "white", o_223: "black", o_135: "black", o_113: "black" };
D.drawbar = (c, v, s) => {
    const n = Math.round(clamp01(v) * 8), kind = BAR_KIND[s.param] || "white";
    const end = Math.round(1 + n * 1.2), x = 13;
    vline(c, x, 0, end); vline(c, x + 6, 0, end);
    for (let y = 0; y <= end; y++) {
        if (kind === "black") px(c, x + 3, y);
        else if (kind === "sub" && y % 3 === 0) px(c, x + 3, y);
    }
    hline(c, x - 1, x + 7, end + 1);
    for (let i = 0; i <= 8; i += 4) px(c, 9, Math.round(1 + i * 1.2));
};
/* pipe organ / ensemble */
D.swell = (c, v, s) => {                       /* how slowly the note blooms; a spark climbs it */
    const L = 2 + v * 22;
    const f = (x) => 10 - 9 * (1 - Math.exp(-3 * (x - 3) / L));
    curve(c, 3, 28, f); hline(c, 2, 3, 10);
    const sec = 0.2 + v * 2.5, ph = phase(s.key, 1 / (sec + 0.8), s.now), u = ph * (sec + 0.8) / sec;
    if (awake && u < 1) { const x = Math.round(3 + u * 25 * Math.min(1, L / 12 + 0.2)); spark(c, x, Math.round(f(x))); }
};
const pipe = (len) => (c, v) => {              /* a pipe of this length, its voice filled in */
    const h = Math.round(len), x = 13;
    vline(c, x, 11 - h, 11); vline(c, x + 5, 11 - h, 11); hline(c, x, x + 5, 11 - h);
    c.line(x, 11 - Math.round(h * 0.25), x + 2, 11 - Math.round(h * 0.25) + 1, 1);
    const fill = Math.round(v * (h - 1));
    for (let y = 11; y > 11 - fill; y -= 2) for (let xx = x + 2; xx < x + 5; xx += 2) px(c, xx, y);
};
D.reg8 = pipe(11); D.reg4 = pipe(8); D.reg2 = pipe(5);
/* breath */
D.onset = (c, v) => {                          /* tonguing: a 't' before the tone */
    const h = Math.round(v * 9);
    if (h > 0) { vline(c, 4, 11 - h, 11); hline(c, 3, 5, 11 - h + 1); }
    waveTone(c, 8, 28, 6, 4, 2, (x) => x);
};
D.press = (c, v, s) => {                       /* who controls the breath: your pad, the instrument, or both */
    const i = Math.round(Number(s.raw)), m = Number.isFinite(i) ? i : ({ Pad: 0, Auto: 1, Blend: 2 }[s.raw] ?? 0);
    const pad = (x) => { hline(c, x, x + 7, 4); hline(c, x, x + 7, 11); vline(c, x, 4, 11); vline(c, x + 7, 4, 11); c.drawCircle(x + 4, 7, 2, 1); };
    const auto = (x) => { curve(c, x, x + 9, (xx) => 11 - 7 * Math.sin(Math.PI * (xx - x) / 9)); };
    if (m === 0) pad(12); else if (m === 1) auto(11); else { pad(5); auto(18); }
};
D.air = (c, v, s) => {                         /* breath, drifting through */
    const ph = phase(s.key, 0.8, s.now), n = Math.round(v * 14);
    waveTone(c, 3, 28, 6, 3, 2, (x) => x);
    for (let i = 0; i < n; i++) { const x = 3 + ((hash(i) * 25 + ph * 25) % 25); px(c, x, 1 + hash(i + 9) * 10); }
};
/* glass e.piano */
D.tune = (c, v) => { const xs = [5, 9 + v * 4, 14 + v * 7, 19 + v * 9].map(Math.round), hs = [9, 6, 5, 3]; ladder(c, xs.filter((x) => x <= 29), hs, 11); };
D.tine = (c, v) => { hline(c, 3, 18, 6); vline(c, 19, 5, 7); const a = v * 3; curve(c, 3, 18, (x) => 6 - a * Math.sin((x - 3) / 15 * Math.PI) * 0.8); vline(c, 25, 2, 10); };
/* bowed */
D.bite = (c, v) => {                           /* the bow catches before it sings */
    const n = Math.round(v * 8);
    for (let i = 0; i < n; i++) vline(c, 3 + i, 6 - Math.round(hash(i) * 5), 6 + Math.round(hash(i + 7) * 5));
    waveTone(c, 3 + n, 28, 6, 4, 2, (x) => x);
};
D.bow = (c, v, s) => {                         /* the bow draws back and forth at its speed */
    const ph = phase(s.key, 0.2 + v * 1.4, s.now);
    vline(c, 16, 0, 11);
    const off = Math.round(Math.sin(ph * TAU) * 7);
    bowStroke(c, 5, 5 + off, 27 + off);
};
D.blur = (c, v) => {                           /* how wide each ringing band is */
    const w = 1 + v * 6;
    curve(c, 3, 28, (x) => 11 - 9 * Math.exp(-((x - 16) * (x - 16)) / (2 * w * w)));
};
D.hit = (c, v) => {                            /* strike against bow */
    hline(c, 3, 28, 10);
    mallet(c, 8, 9, 0.2);
    const n = Math.round(v * 6); for (let i = 0; i < n; i++) px(c, 12 + i, 3 + (i % 2));
    bowStroke(c, 6, 18, 28);
};
/* choir */
D.crowd = (c, v, s) => {                        /* how many singers per note */
    const n = Math.max(1, Math.min(3, Math.round(Number(s.raw)) || 1));
    const xs = n === 1 ? [16] : n === 2 ? [12, 20] : [9, 16, 23];
    xs.forEach((x, i) => { const y = (n === 3 && i === 1) ? 2 : 3; c.drawCircle(x, y, 2, 1); c.drawArc(x, y + 8, 3, 270, 180, 1); hline(c, x - 3, x + 3, y + 8); });
};
/* effects */
D.size = (c, v) => {                           /* the room, in perspective */
    const d = 2 + Math.round(v * 9);
    hline(c, 16 - d - 2, 16 + d + 2, 11); c.line(16 - d - 2, 11, 16 - d, 11 - d, 1); c.line(16 + d + 2, 11, 16 + d, 11 - d, 1);
    hline(c, 16 - d, 16 + d, 11 - d); px(c, 16, 10);
};
D.dark = (c, v) => {                           /* the reverb's top end falls away */
    const fc = 28 - v * 20;
    curve(c, 3, 28, (x) => 3 + (x > fc ? Math.min(8, (x - fc) * 0.9) : 0));
    for (let x = 4; x <= 28; x += 4) { const y = 3 + (x > fc ? Math.min(8, (x - fc) * 0.9) : 0); for (let yy = Math.round(y) + 3; yy <= 11; yy += 3) px(c, x, yy); }
};
D.delay = (c, v) => {                          /* a gap before the room answers */
    vline(c, 3, 1, 11);
    const g = 5 + Math.round(v * 14);
    for (let i = 0; i < 6; i++) { const x = g + i * 2; if (x > 28) break; const h = Math.round(7 * Math.exp(-i * 0.35)); for (let y = 11; y > 11 - h; y -= 2) px(c, x, y); }
    dots(c, 5, g - 2, 11, 3);
};
D.drive = (c, v) => {                          /* warmth: the wave's shoulders round off */
    const k = 1 + v * 6;
    waveTone(c, 3, 28, 6, 4, 2, (x) => Math.tanh(x * k) / Math.tanh(k));
};
D.speed = (c, v, s) => {                       /* how fast the sway goes, moving at that speed */
    const ph = phase(s.key, 0.4 + v * 3, s.now);
    waveTone(c, 3, 28, 6, 4, 1 + v * 2, (x) => x, -ph * (1 + v * 2));
};

/* MODULATION ------------------------------------------------------------ */
/* Which modulator: four boxes, the chosen one filled. */
D.modsel = (c, v) => {
    const k = Math.round(v * 3);
    for (let i = 0; i < 4; i++) {
        const x = 3 + i * 7;
        if (i === k) c.fillRect(x, 3, 5, 6, 1);
        else { hline(c, x, x + 4, 3); hline(c, x, x + 4, 8); vline(c, x, 3, 8); vline(c, x + 4, 3, 8); }
    }
};
const lfoWave = (shape, u) => {              /* -1..1 at u cycles, as mod.c draws it */
    const p = u - Math.floor(u), n = Math.floor(u);
    switch (shape) {
    case 1: return 1 - 4 * Math.abs(p - 0.5);
    case 2: return 1 - 2 * p;
    case 3: return 2 * p - 1;
    case 4: return p < 0.5 ? 1 : -1;
    case 5: return hash(n + 7) * 2 - 1;
    case 6: { const a = hash(n + 6) * 2 - 1, b = hash(n + 7) * 2 - 1; return a + (b - a) * (0.5 - 0.5 * Math.cos(Math.PI * p)); }
    default: return Math.sin(u * TAU);
    }
};
const lfoCurve = (c, shape, x0, x1, mid, amp, cyc, ph) => curve(c, x0, x1, (x) => mid - amp * lfoWave(shape, ((x - x0) / (x1 - x0)) * cyc + (ph || 0)));
D.mod_kind = (c, v) => {
    const k = Math.round(v * 3);
    if (k === 0) {                             /* Velocity: harder and harder strikes */
        for (let i = 0; i < 5; i++) { const x = 6 + i * 5, h = 2 + i * 2; vline(c, x, 11 - h, 11); hline(c, x - 1, x + 1, 11 - h); }
    } else if (k === 1) {                      /* MPE: a fingertip pressing a pad */
        hline(c, 5, 26, 10); hline(c, 5, 26, 11); c.drawCircle(16, 5, 3, 1);
        dotted(c, 7, 12, () => 8, 2); dotted(c, 20, 25, () => 8, 2);
    } else if (k === 2) lfoCurve(c, 0, 4, 27, 6, 4, 2);   /* LFO */
    else {                                     /* Envelope: up, held, down */
        c.line(4, 10, 10, 2, 1); hline(c, 10, 20, 2); curve(c, 20, 28, (x) => 10 - 8 * Math.exp(-(x - 20) / 2.5)); hline(c, 4, 28, 11);
    }
};
D.mod_shape = (c, v) => lfoCurve(c, Math.round(v * 6), 4, 27, 6, 4, 2);
/* RATE: the wave at its speed, and the speed in plain figures. */
const SYNC = ["8BAR", "4BAR", "2BAR", "1BAR", "1/2.", "1/2", "1/2T", "1/4.", "1/4", "1/4T", "1/8.", "1/8", "1/8T",
    "1/16.", "1/16", "1/16T", "1/32", "1/64"];
function rateText(rate) {
    if (rate > 0) return SYNC[Math.min(SYNC.length - 1, Math.floor(rate * SYNC.length))];
    const hz = 0.02 * Math.pow(1000, Math.min(1, -rate));
    return (hz < 1 ? hz.toFixed(2) : hz < 10 ? hz.toFixed(1) : String(Math.round(hz))) + "HZ";
}
D.mod_rate = (c, v, s) => {
    const rate = v * 2 - 1, a = Math.abs(rate);
    const ph = phase(s.key, 0.3 + a * 2.5, s.now);
    lfoCurve(c, 0, 4, 27, 1.5, 1.5, 1 + a * 3, -ph * (1 + a * 3));
    c.text(rateText(rate), 9);
};
D.mod_rise = (c, v) => { const x1 = 4 + Math.round(v * 16); c.line(4, 10, x1, 2, 1); hline(c, x1, 27, 2); hline(c, 4, 28, 11); };
D.mod_fall = (c, v) => { const L = 1 + v * 9; vline(c, 4, 2, 10); hline(c, 4, 8, 2); curve(c, 8, 28, (x) => 10 - 8 * Math.exp(-(x - 8) / L)); hline(c, 4, 28, 11); };
D.mod_axis = (c, v) => {
    const k = Math.round(v * 2);
    if (k === 0) { vline(c, 16, 1, 7); c.line(13, 4, 16, 7, 1); c.line(19, 4, 16, 7, 1); hline(c, 8, 24, 10); hline(c, 8, 24, 11); }
    else if (k === 1) { vline(c, 16, 1, 10); c.line(13, 4, 16, 1, 1); c.line(19, 4, 16, 1, 1); c.line(13, 7, 16, 10, 1); c.line(19, 7, 16, 10, 1); }
    else { hline(c, 5, 26, 6); c.line(8, 3, 5, 6, 1); c.line(8, 9, 5, 6, 1); c.line(23, 3, 26, 6, 1); c.line(23, 9, 26, 6, 1); }
};
D.mod_lag = (c, v) => {                       /* a jump, smoothed over */
    const L = 0.3 + v * 7;
    hline(c, 4, 10, 10); dotted(c, 10, 10, () => 6, 1);
    curve(c, 10, 28, (x) => 10 - 8 * (1 - Math.exp(-(x - 10) / L)));
};
D.mod_aim = (c, v) => {                       /* an arrow into a target, or a target with nothing aimed at it */
    c.drawCircle(22, 6, 4, 1); px(c, 22, 6);
    if (v > 0) { hline(c, 4, 16, 6); c.line(13, 3, 16, 6, 1); c.line(13, 9, 16, 6, 1); }
    else c.line(18, 10, 26, 2, 1);
};
D.mod_depth = (c, v) => {                     /* how far, and which way, from the middle */
    const d = Math.round((v * 2 - 1) * 12);
    vline(c, 16, 2, 10);
    if (d) { const x1 = 16 + d, sg = Math.sign(d); hline(c, 16, x1, 5); hline(c, 16, x1, 7); vline(c, x1, 4, 8); px(c, x1 + sg, 6); }
    dots(c, 4, 28, 11, 3);
};
D.gap = () => {};                              /* a blank cell: nothing to see */

/* ------------------------------------------------------------ routing -- */
function roleOf(key) {
    if (key.startsWith("c_")) return "char";
    if (key === "mod") return "modsel";
    const mm = /^mod\d_([a-z]+?)\d?$/.exec(key);
    if (mm) return mm[1] === "gap" ? "gap" : "mod_" + mm[1];
    if (/^o_(16|513|8|4|223|2|135|113|1)$/.test(key)) return "drawbar";
    if (key === "k_8") return "reg8"; if (key === "k_4") return "reg4"; if (key === "k_2") return "reg2";
    const m = /^[a-z]_(.*)$/.exec(key);
    return m ? m[1] : key;
}
/* Every key in Quilt's contract that has a picture, and what each needs. */
const RANGE = { volume: [-60, 6], v_crowd: [1, 3], o_16: [0, 8], o_513: [0, 8], o_8: [0, 8], o_4: [0, 8], o_223: [0, 8], o_2: [0, 8], o_135: [0, 8], o_113: [0, 8], o_1: [0, 8] };
const ENUMS = { a_press: 3, b_press: 3, d_press: 3, v_press: 3, type: 38 };
/* The Modulation page's enums, by role, as mod.c names them. */
const OPTS = { modsel: ["1", "2", "3", "4"], mod_kind: ["Velocity", "MPE", "LFO", "Envelope"],
    mod_shape: ["Sine", "Triangle", "Saw", "Ramp", "Square", "Random", "Drift"], mod_axis: ["Press", "Slide", "Bend"] };
const BIPOLAR = { mod_rate: 1, mod_depth: 1 };

function norm(key, raw) {
    const role = roleOf(key);
    if (role === "gap") return 0;
    if (role === "mod_aim") { const t = String(raw).trim(); return t === "" ? NaN : t === "None" || t === "0" ? 0 : 1; }
    if (OPTS[role]) {
        const o = OPTS[role]; let i = o.indexOf(String(raw));
        if (i < 0 && String(raw).trim() !== "" && Number.isFinite(Number(raw)) && role !== "modsel") i = Number(raw);
        return i < 0 ? NaN : i / (o.length - 1);
    }
    if (BIPOLAR[role]) { const n = Number(raw); return Number.isFinite(n) && String(raw).trim() !== "" ? (n + 1) / 2 : NaN; }
    if (ENUMS[key]) {
        let i = Number(raw);
        if (!Number.isFinite(i) || String(raw).trim() === "") i = key === "type" ? TYPES.indexOf(String(raw)) : ["Pad", "Auto", "Blend"].indexOf(String(raw));
        return i < 0 ? NaN : i / (ENUMS[key] - 1);
    }
    const n = Number(raw); if (!Number.isFinite(n)) return NaN;
    const r = RANGE[key]; return r ? (n - r[0]) / (r[1] - r[0]) : n;
}
const typeTws = new Map();

function drawCell(ctx, { values, group, nowMs, touched }) {
    const key = group && group.keys && group.keys[0];
    if (!key || !values) return;
    const sid = (group.tile != null ? group.tile + "|" : "") + key;
    const raw = values[key];
    const v0 = norm(key, raw);
    if (!Number.isFinite(v0)) return;   /* no answer yet: no picture of a made-up value */
    const now = typeof nowMs === "number" ? nowMs : 0;
    const type = typeName(values.type);
    const role = roleOf(key);
    const fn = D[role]; if (!fn) return false;
    const stepped = role === "drawbar" || role === "press" || role === "crowd" || key === "type" || !!OPTS[role] || role === "mod_aim";
    const v = stepped ? v0 : eased(sid, v0, now);
    noteValue(sid, v0, now);
    awake = !!(touched || (group && group.touched)) || now - lastTurn.get(sid).t < AWAKE_MS;
    const s = { key: sid, param: key, raw, type, now, inst: key.startsWith("c_") ? key.slice(2) : null,
        speed: Number.isFinite(Number(values.speed)) ? Number(values.speed) : 0.3 };
    if (key === "type") {
        const nm = typeName(raw);
        let typeTw = typeTws.get(sid); if (!typeTw) { typeTw = { name: null, from: null, t0: -1e9, dir: 1 }; typeTws.set(sid, typeTw); }
        if (typeTw.name !== nm) { if (typeTw.name) { typeTw.from = typeTw.name; typeTw.t0 = now; typeTw.dir = TYPES.indexOf(nm) > TYPES.indexOf(typeTw.name) ? 1 : -1; } typeTw.name = nm; }
        s.type = nm; s.typeTween = { from: typeTw.from, k: clamp01((now - typeTw.t0) / 160), dir: typeTw.dir };
    }
    /* A still picture is drawn once and replayed as runs; only a moving one
     * (awake, easing, or TYPE sliding) is drawn again each frame. */
    const moving = awake || v !== v0 || (s.typeTween && s.typeTween.k < 1);
    const sig = String(raw) + "|" + type + "|" + s.speed + "|" + ctx.width + "x" + ctx.height;
    const hit = stills.get(sid);
    if (!moving && hit && hit.sig === sig) { replay(ctx, hit.runs); return; }
    const p = pen(ctx.width, ctx.height, SCALE);
    fn(p, v, s);
    const runs = p.runs();
    replay(ctx, runs);
    if (moving) stills.delete(sid); else stills.set(sid, { sig, runs });
}

/* The pen draws into a bitmap the size of the frame, so every shape is clipped
 * once, overlapping strokes cost nothing, and the result goes to the host as a
 * few horizontal runs rather than a call per pixel. */
const stills = new Map();
/* Pictures are drawn in a 32x12 design space and scaled down by SCALE about
 * its centre BEFORE they become pixels: a line stays one pixel, a circle stays
 * round, and the picture sits small in the middle of its cell. */
const SCALE = 0.8;
const GLYPH = {
    0: "111101101101111", 1: "010110010010111", 2: "111001111100111", 3: "111001111001111", 4: "101101111001001",
    5: "111100111001111", 6: "111100111101111", 7: "111001001001001", 8: "111101111101111", 9: "111101111001111",
    "/": "001001010100100", ".": "00001", T: "111010010010010", B: "110101110101110", A: "010101111101101",
    R: "110101110101101", H: "101101111101101", Z: "111001010100111",
};
function pen(w, h, k) {
    const w0 = w;
    const bm = new Uint8Array(w * h);
    const cx = w / 2 - 0.5, cy = h / 2 - 0.5;
    const tx = (x) => Math.round(cx + (x - 15.5) * k), ty = (y) => Math.round(cy + (y - 5.5) * k);
    const plot = (x, y) => { if (x >= 0 && y >= 0 && x < w && y < h) bm[y * w + x] = 1; };
    const raw = { fillRect(x, y, rw, rh) { for (let j = y; j < y + rh; j++) for (let i = x; i < x + rw; i++) plot(i, j); } };
    const p = {
        width: W, height: PIC + 1,
        fillRect(x, y, rw, rh) {
            const x0 = tx(x), x1 = tx(x + Math.max(1, rw) - 1), y0 = ty(y), y1 = ty(y + Math.max(1, rh) - 1);
            for (let j = y0; j <= y1; j++) for (let i = x0; i <= x1; i++) plot(i, j);
        },
        line(x0, y0, x1, y1) { lineVia(raw, tx(x0), ty(y0), tx(x1), ty(y1)); },
        drawArc(x, y, r, a, sw) { arcVia(raw, tx(x), ty(y), Math.max(1, Math.round(r * k)), a, sw); },
        drawCircle(x, y, r) { arcVia(raw, tx(x), ty(y), Math.max(1, Math.round(r * k)), 0, 360); },
        fillCircle(x, y, r) { const X = tx(x), Y = ty(y), R = Math.max(1, Math.round(r * k));
            for (let dy = -R; dy <= R; dy++) { const hf = Math.floor(Math.sqrt(R * R - dy * dy)); raw.fillRect(X - hf, Y + dy, 2 * hf + 1, 1); } },
        /* Words are not scaled: GLYPH's 3x5 figures, centred, from raw row y. */
        text(str, y) {
            const w = [...str].reduce((a, ch) => a + (ch === "." ? 2 : 4), 0) - 1;
            let x = Math.round(w > 0 ? (w0 - w) / 2 : 0);
            for (const ch of str) {
                const g = GLYPH[ch]; if (!g) { x += 4; continue; }
                const gw = ch === "." ? 1 : 3;
                for (let r = 0; r < 5; r++) for (let i = 0; i < gw; i++) if (g[r * gw + i] === "1") plot(x + i, y + r);
                x += gw + 1;
            }
        },
        runs() {
            const out = [];
            for (let j = 0; j < h; j++) for (let i = 0; i < w; i++) {
                if (!bm[j * w + i]) continue;
                const a = i; while (i < w && bm[j * w + i]) i++;
                out.push(a, j, i - a);
            }
            return out;
        },
    };
    return p;
}
function replay(ctx, runs) { for (let i = 0; i < runs.length; i += 3) ctx.fillRect(runs[i], runs[i + 1], runs[i + 2], 1, 1); }

const overlay = { /* for tests */ _reset() { tweens.clear(); phases.clear(); typeTws.clear(); lastTurn.clear(); stills.clear(); }, widgetKinds: ["custom:quilt"], drawCell, _D: D, _C: C, _roleOf: roleOf, _TYPES: TYPES };
globalThis.canvas_overlay = overlay;
})();
