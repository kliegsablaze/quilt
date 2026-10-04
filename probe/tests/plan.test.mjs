// Plans the probe's pages with the host's own planner, once per instrument,
// and runs the host's contract validator. Usage: node plan.test.mjs <dir> <schwung>
import fs from "node:fs";
import path from "node:path";
import { pathToFileURL } from "node:url";

const [dir, schwung] = process.argv.slice(2);
const { planPages } = await import(pathToFileURL(path.join(schwung, "src/shared/param_pages/page_plan.mjs")));
const { validateContract } = await import(pathToFileURL(path.join(schwung, "src/shared/param_pages/validate_contract.mjs")));

const hierarchy = JSON.parse(fs.readFileSync(path.join(dir, "ui_hierarchy.json"), "utf8"));
const chainParams = JSON.parse(fs.readFileSync(path.join(dir, "chain_params.json"), "utf8"));

let fails = 0;
const check = (ok, msg) => { if (!ok) { fails++; console.log("FAIL " + msg); } };

const expect = {
    "Felt Upright": { char: "c_felt", page: ["p_unison", "p_strike"], hidden: ["b_bow", "h_ding"] },
    "Solo Cello": { char: "c_tasto", page: ["b_bow", "b_vib"], hidden: ["p_unison", "h_ding"] },
    "Handpan": { char: "c_cavity", page: ["h_ding", "h_ring"], hidden: ["p_unison", "b_bow"] },
};

for (const [type, want] of Object.entries(expect)) {
    // The same comparison the shadow UI makes: an exact string match on `type`.
    const visible = (cond) => cond.param !== "type" || String(type) === String(cond.equals);
    const plan = planPages({ hierarchy, chainParams, visible });
    const knobPages = plan.pages.filter((p) => Array.isArray(p.keys));
    const allKeys = knobPages.flatMap((p) => p.keys);
    const main = knobPages[0] ? knobPages[0].keys : [];

    check(main.length === 6, `${type}: Main has 6 cells, got ${JSON.stringify(main)}`);
    check(main[1] === want.char, `${type}: Main's second cell is ${want.char}, got ${main[1]}`);
    for (const c of ["c_felt", "c_tasto", "c_cavity"])
        if (c !== want.char) check(!allKeys.includes(c), `${type}: ${c} is hidden`);
    for (const k of want.page) check(allKeys.includes(k), `${type}: ${k} is on a page`);
    for (const k of want.hidden) check(!allKeys.includes(k), `${type}: ${k} is hidden`);
    check([...(plan.conditionKeys || [])].join() === "type", `${type}: the only gate key is type, got ${[...(plan.conditionKeys || [])]}`);
    console.log(`${type}: ` + knobPages.map((p) => `[${p.keys.join(" ")}]`).join(" "));
}

const { findings } = validateContract({ id: "quiltprobe", hierarchy, chainParams, capabilities: {} });
for (const f of findings) console.log(`  validate ${f.level}: ${f.rule} — ${f.message}`);
check(!findings.some((f) => f.level === "error"), "validator reports no errors");

console.log(fails ? `FAIL: plan.test ${fails} failed` : "ok: plan.test");
process.exit(fails ? 1 : 0);
