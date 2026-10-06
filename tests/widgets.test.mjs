// Draws Quilt's knob pictures (src/canvas.js) through the host's own widget
// registry, frame context and viz resolver, on every page of every
// instrument. Usage: node widgets.test.mjs <dump dir> <schwung checkout>
import fs from "node:fs";
import path from "node:path";
import vm from "node:vm";
import { pathToFileURL, fileURLToPath } from "node:url";

const [dir, schwung] = process.argv.slice(2);
const here = path.dirname(fileURLToPath(import.meta.url));
const load = (rel) => import(pathToFileURL(path.join(schwung, "src/shared/param_pages", rel)));
const { planPages } = await load("page_plan.mjs");
const { buildMetaIndex } = await load("param_meta.mjs");
const { resolveViz } = await load("viz.mjs");
const { frameCtx } = await load("frame_ctx.mjs");
const { registerOverlayWidgets, getWidget, clearWidgets } = await load("widget_registry.mjs");

const hierarchy = JSON.parse(fs.readFileSync(path.join(dir, "ui_hierarchy.json"), "utf8"));
const chainParams = JSON.parse(fs.readFileSync(path.join(dir, "chain_params.json"), "utf8"));
const byKey = Object.fromEntries(chainParams.map((p) => [p.key, p]));
const metaIndex = buildMetaIndex({ hierarchy, chainParams });

let fails = 0;
const check = (ok, msg) => { if (!ok) { fails++; console.log("FAIL " + msg); } };

// Loaded the way the host loads it: a script over a bare global.
const g = {};
g.globalThis = g;
vm.runInNewContext(fs.readFileSync(path.join(here, "../src/canvas.js"), "utf8"), g);
const ov = g.canvas_overlay;
clearWidgets();
const { registered } = registerOverlayWidgets(ov);
check(registered.includes("custom:quilt"), `custom:quilt registers, got ${registered}`);
const impl = getWidget("custom:quilt");

// Every control but the preset index names the picture.
for (const p of chainParams) {
    if (p.key === "preset") continue;
    check(p.viz && p.viz.kind === "custom:quilt", `${p.key} declares the Quilt picture`);
}
check(byKey.sway.viz.extra_keys.join() === "type,speed", "SWAY reads TYPE and SPEED");
check(byKey.soft.viz.extra_keys.join() === "type", "SOFT reads TYPE");

/* A host ctx that paints a 128x64 screen, so a picture can be compared. */
function screen() {
    const px = new Uint8Array(128 * 64);
    let calls = 0;
    return { px, calls: () => calls, textWidth: (s) => String(s).length * 5,
        fillRect(x, y, w, h, c) { calls++; for (let j = y; j < y + h; j++) for (let i = x; i < x + w; i++) px[j * 128 + i] = c ? 1 : 0; },
        print() {} };
}
function draw(key, values, nowMs, touched) {
    const s = screen();
    const f = frameCtx(s, { x: 0, y: 0, w: 32, h: 15 });
    impl.draw(f, { group: { kind: "custom:quilt", keys: [key] }, values, nowMs, touched });
    return { lit: s.px.reduce((a, b) => a + b, 0), clipped: f.clipped(), calls: s.calls(), px: s.px };
}
function value(key, u) {
    const m = byKey[key];
    if (m.type === "enum") return m.options[Math.round(u * (m.options.length - 1))];
    const lo = m.min ?? 0, hi = m.max ?? 1, v = lo + u * (hi - lo);
    return m.type === "int" ? Math.round(v) : v;
}

let worstCalls = 0, worstKey = "";
for (const type of byKey.type.options) {
    const visible = (c) => c.param !== "type" || (c.equals === undefined || String(type) === String(c.equals));
    const pages = planPages({ hierarchy, chainParams, visible }).pages.filter((p) => Array.isArray(p.keys));
    for (const page of pages) {
        // The host's resolver gives every cell its own one-cell picture.
        const { groups } = resolveViz({ keys: page.keys, metaIndex });
        for (const k of page.keys) {
            const grp = groups.find((x) => x.keys.includes(k));
            check(grp && grp.kind === "custom:quilt" && grp.slotSpan === 1, `${type}: ${k} draws as its own Quilt cell`);
        }
        for (const k of page.keys) for (const u of [0, 0.37, 1]) {
            ov._reset();
            const vals = { type, speed: 0.4, [k]: value(k, u) };
            let r;
            try { r = draw(k, vals, 1000); } catch (e) { check(false, `${type}: ${k}@${u} throws ${e}`); continue; }
            check(r.clipped === 0, `${type}: ${k}@${u} draws outside its frame (${r.clipped})`);
            check(r.lit >= 5, `${type}: ${k}@${u} draws a picture (${r.lit} px)`);
            if (r.calls > worstCalls) { worstCalls = r.calls; worstKey = `${type} ${k}@${u}`; }
        }
    }
}

// No answer yet: nothing at all, never a picture of a made-up value.
ov._reset();
const empty = draw("soft", { type: "Felt Upright" }, 1000);
check(empty.lit === 0, `an unanswered value draws nothing (${empty.lit} px)`);

// Small and centred: every picture keeps a clear margin inside its cell.
for (const k of ["c_handpan", "space", "volume", "o_16", "type", "sway"]) {
    ov._reset();
    const r = draw(k, { type: k === "o_16" ? "Tonewheel Organ" : "Handpan", speed: 0.4, [k]: value(k, 1) }, 1000);
    let x0 = 99, x1 = -1;
    for (let y = 0; y < 15; y++) for (let x = 0; x < 32; x++) if (r.px[y * 128 + x]) { x0 = Math.min(x0, x); x1 = Math.max(x1, x); }
    check(x0 >= 3 && x1 <= 28, `${k} stays clear of its cell's edges (${x0}..${x1})`);
    check(Math.abs((x0 + x1) / 2 - 15.5) <= 4, `${k} sits near the middle of its cell (${x0}..${x1})`);
}

// A still picture replays exactly what it drew.
ov._reset();
const a = draw("c_handpan", { type: "Handpan", c_handpan: 0.6 }, 1000);
const b = draw("c_handpan", { type: "Handpan", c_handpan: 0.6 }, 1040);
check(a.px.every((x, i) => x === b.px[i]), "a still picture replays the same pixels");

// Time controls sleep until turned or touched, then move.
ov._reset();
const t0 = draw("o_fast", { type: "Tonewheel Organ", o_fast: 0.6 }, 1000);
const t1 = draw("o_fast", { type: "Tonewheel Organ", o_fast: 0.6 }, 1200);
check(t0.px.every((x, i) => x === t1.px[i]), "the Leslie holds still while nobody touches it");
const t2 = draw("o_fast", { type: "Tonewheel Organ", o_fast: 0.6 }, 1400, true);
const t3 = draw("o_fast", { type: "Tonewheel Organ", o_fast: 0.6 }, 1550, true);
check(t2.px.some((x, i) => x !== t3.px[i]), "the Leslie turns while its knob is touched");
draw("o_fast", { type: "Tonewheel Organ", o_fast: 0.7 }, 5000);
const t4 = draw("o_fast", { type: "Tonewheel Organ", o_fast: 0.7 }, 5300);
const t5 = draw("o_fast", { type: "Tonewheel Organ", o_fast: 0.7 }, 5450);
check(t4.px.some((x, i) => x !== t5.px[i]), "the Leslie turns just after its knob is turned");

console.log(`  widest cell: ${worstCalls} host calls (${worstKey})`);
check(worstCalls <= 100, `every cell reaches the host in at most 100 runs, worst ${worstCalls}`);
console.log(fails ? `FAIL: widgets.test ${fails} failed` : "ok: widgets.test");
process.exit(fails ? 1 : 0);
