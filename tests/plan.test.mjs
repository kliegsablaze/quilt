// Plans Quilt's pages with the host's own planner, once per instrument, runs
// the host's contract validator, and fits every cell label with the host's own
// fitter. Usage: node plan.test.mjs <dump dir> <schwung checkout>
import fs from "node:fs";
import path from "node:path";
import { pathToFileURL } from "node:url";

const [dir, schwung] = process.argv.slice(2);
const load = (rel) => import(pathToFileURL(path.join(schwung, "src/shared/param_pages", rel)));
const { planPages } = await load("page_plan.mjs");
const { validateContract } = await load("validate_contract.mjs");
const { labelVerbatim, HEADER_MIN_LEFT, HEADER_GAP } = await load("render_page_movy.mjs");
const { fontWidth4x5 } = await load("font4x5.mjs");

const hierarchy = JSON.parse(fs.readFileSync(path.join(dir, "ui_hierarchy.json"), "utf8"));
const chainParams = JSON.parse(fs.readFileSync(path.join(dir, "chain_params.json"), "utf8"));
const byKey = Object.fromEntries(chainParams.map((p) => [p.key, p]));

let fails = 0;
const check = (ok, msg) => { if (!ok) { fails++; console.log("FAIL " + msg); } };

// DESIGN.md, Control surface: every cell is a plain word that draws as typed.
for (const p of chainParams) {
    if (p.key === "preset") continue;
    check(typeof p.short_name === "string" && p.short_name.length > 0, `${p.key} declares a cell word`);
    check(typeof p.name === "string" && p.name.length > 0, `${p.key} declares a header name`);
    const drawn = labelVerbatim(p.short_name);
    check(drawn === p.short_name.toUpperCase(), `${p.key}: cell "${p.short_name}" draws as "${drawn}"`);
    check(p.short_name.toUpperCase() !== "MOVE", `${p.key}: no label is MOVE`);
}

const types = byKey.type.options;
check(types.length >= 4 && types.length <= 38, `4 to 38 instruments offered, got ${types.length}`);
const main = ["type", "soft", null, "decay", "sway", "tone", "space", "volume"];
const charKeys = chainParams.filter((p) => p.key.startsWith("c_")).map((p) => p.key);

for (const type of types) {
    // The comparison the shadow UI makes (compareConditionValue): an exact
    // string match on `type`, for equals and not_equals.
    const visible = (cond) => {
        if (cond.param !== "type") return true;
        if (cond.equals !== undefined) return String(type) === String(cond.equals);
        if (cond.not_equals !== undefined) return String(type) !== String(cond.not_equals);
        return true;
    };
    const plan = planPages({ hierarchy, chainParams, visible });
    const pages = plan.pages.filter((p) => Array.isArray(p.keys));
    const all = pages.flatMap((p) => p.keys);
    const first = pages[0] ? pages[0].keys : [];
    const char = first[2];

    check(first.length === 8, `${type}: Main has 8 cells, got ${JSON.stringify(first)}`);
    main.forEach((k, i) => { if (k) check(first[i] === k, `${type}: Main cell ${i + 1} is ${k}, got ${first[i]}`); });
    check(charKeys.includes(char), `${type}: Main's third cell is a CHAR key, got ${char}`);
    check(all.filter((k) => charKeys.includes(k)).every((k) => k === char), `${type}: only its own CHAR is shown`);

    const level = Object.entries(hierarchy.levels).find(([k, l]) => k.startsWith("i_") && l.visible_if && l.visible_if.equals === type);
    check(level, `${type}: has its own Instrument level`);
    if (level) {
        const want = level[1].knobs;
        check(!want.includes(char), `${type}: its CHAR is on Main only, not on the Instrument page`);
        check(want.every((k) => all.includes(k)), `${type}: every Instrument knob is on a page`);
        check(want.length <= 8, `${type}: the Instrument page fits one page`);
    }
    // DESIGN.md, One place for everything: no key is on two pages.
    check(new Set(all).size === all.length, `${type}: no knob appears twice (${all.filter((k, i) => all.indexOf(k) !== i)})`);
    check(!all.includes("close"), `${type}: CLOSE is gone`);
    check(all.includes("size") && all.includes("drive"), `${type}: Effects is reachable`);
    check(all.includes("speed") === (type !== "Vibraphone" && type !== "Tonewheel Organ"), `${type}: SPEED is shown only where it does something`);
    check(all.includes("o_16") === (type === "Tonewheel Organ"), `${type}: Drawbars only for the organ`);
    check([...(plan.conditionKeys || [])].join() === "type", `${type}: the only gate key is type`);

    const cells = pages.map((p) => `[${p.keys.map((k) => (byKey[k] ? byKey[k].short_name : k).toUpperCase()).join(" ")}]`);
    console.log(`${type.padEnd(20)} ${cells.join(" ")}`);
}

// The header gives the page name what the slot's title leaves: at least the
// title's floor (HEADER_MIN_LEFT) and the gap, from 128 px less 2 px a side.
// Every page title must fit that whole, never cut.
const pageRoom = 128 - 4 - HEADER_MIN_LEFT - HEADER_GAP;
for (const [k, l] of Object.entries(hierarchy.levels)) {
    if (k === "root") continue;
    const w = fontWidth4x5(String(l.label).toUpperCase());
    check(w <= pageRoom, `page "${l.label}" (${k}) is ${w} px, the header has ${pageRoom}`);
}

const { findings } = validateContract({ id: "quilt", hierarchy, chainParams, capabilities: {} });
for (const f of findings) if (f.level !== "info") console.log(`  validate ${f.level}: ${f.rule} — ${f.message}`);
check(!findings.some((f) => f.level === "error"), "the host's validator reports no errors");

console.log(fails ? `FAIL: plan.test ${fails} failed` : "ok: plan.test");
process.exit(fails ? 1 : 0);
