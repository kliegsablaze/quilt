/* Black-box checks of Quilt through the v2 API, as chain_host drives it, and a
 * dump of its two contracts for tests/plan.test.mjs to plan with the host's
 * own planner. DESIGN.md, Testing. */
#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "contact.h"
#include "host/plugin_api_v1.h"
#include "quilt.h"

plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);

static int fails = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; \
    printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* The host's contract buffers are 128 KB (shadow_constants.h). */
static char buf[131072];
static plugin_api_v2_t *A;

static const char *get(void *p, const char *k) {
    int n = A->get_param(p, k, buf, sizeof(buf));
    return n < 0 ? NULL : buf;
}
static int is(void *p, const char *k, const char *want) {
    const char *got = get(p, k);
    return got && !strcmp(got, want);
}
static double num(void *p, const char *k) {
    const char *got = get(p, k);
    return got ? atof(got) : NAN;
}
static void midi3(void *p, int a, int b, int c) {
    uint8_t m[3] = { (uint8_t)a, (uint8_t)b, (uint8_t)c };
    A->on_midi(p, m, 3, 0);
}

/* RMS over some blocks, and the largest sample seen. */
static double rms(void *p, int blocks, int *peak) {
    int16_t out[256];
    double acc = 0;
    long n = 0;
    for (int b = 0; b < blocks; b++) {
        A->render_block(p, out, 128);
        for (int i = 0; i < 256; i++) {
            acc += (double)out[i] * out[i];
            n++;
            if (peak && abs(out[i]) > *peak) *peak = abs(out[i]);
        }
    }
    return sqrt(acc / (double)n);
}

static void dump(void *p, const char *key, const char *dir) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.json", dir, key);
    const char *s = get(p, key);
    FILE *f = fopen(path, "w");
    CHECK(f && s, "dump %s", key);
    if (f && s) fputs(s, f);
    if (f) fclose(f);
}

static void contracts(void *p, const char *dir) {
    int nh = A->get_param(p, "ui_hierarchy", buf, sizeof(buf));
    CHECK(nh > 0 && nh < 131072, "ui_hierarchy fits 128 KB (%d bytes)", nh);
    int nc = A->get_param(p, "chain_params", buf, sizeof(buf));
    CHECK(nc > 0 && nc < 131072, "chain_params fits 128 KB (%d bytes)", nc);
    printf("contracts: ui_hierarchy %d bytes, chain_params %d bytes\n", nh, nc);
    CHECK(A->get_param(p, "chain_params", buf, 100) < 0, "a short buffer is refused, not overrun");

    /* Every key chain_params declares is served. */
    A->get_param(p, "chain_params", buf, sizeof(buf));
    char *cp = strdup(buf);
    int keys = 0;
    for (char *s = strstr(cp, "{\"key\":\""); s; s = strstr(s + 1, "{\"key\":\"")) {
        char key[64];
        const char *k = s + 8;
        int n = (int)(strchr(k, '"') - k);
        if (n <= 0 || n >= (int)sizeof(key)) continue;
        memcpy(key, k, (size_t)n);
        key[n] = '\0';
        keys++;
        CHECK(get(p, key) != NULL, "declared key %s is served", key);
    }
    free(cp);
    printf("contracts: %d keys declared\n", keys);
    dump(p, "ui_hierarchy", dir);
    dump(p, "chain_params", dir);
}

static void types(void *p) {
    CHECK(is(p, "type", "Felt Upright"), "default type is Felt Upright");
    /* TYPE offers exactly the instruments whose engine is built. */
    int offered = 0;
    for (int i = 0; i < QUILT_NINST; i++) {
        int t = quilt_type_index(i);
        CHECK((t >= 0) == quilt_built(i), "%s is offered if and only if its engine is built",
              QUILT_INST[i].name);
        if (t < 0) continue;
        offered++;
        CHECK(quilt_type_inst(t) == i, "type option %d is %s", t, QUILT_INST[i].name);
        A->set_param(p, "type", QUILT_INST[i].name);
        CHECK(is(p, "type", QUILT_INST[i].name), "type by name: %s", QUILT_INST[i].name);
    }
    CHECK(offered == QUILT_NTYPES, "%d instruments offered", offered);
    A->set_param(p, "type", "1");
    CHECK(is(p, "type", "Una Corda Grand"), "type by index");
    A->set_param(p, "type", "2.0");
    CHECK(is(p, "type", QUILT_TYPE_NAMES[2]), "type by float index");
    A->set_param(p, "type", "Banjo");
    CHECK(is(p, "type", QUILT_TYPE_NAMES[2]), "an unknown type is ignored");
    A->set_param(p, "type", "Flute");
    CHECK(is(p, "type", QUILT_TYPE_NAMES[2]), "an instrument not built yet is not offered");
    A->set_param(p, "type", "99");
    CHECK(is(p, "type", QUILT_TYPE_NAMES[QUILT_NTYPES - 1]), "an index past the end clamps to the last");
}

static void presets(void *p) {
    CHECK(num(p, "preset_count") == QUILT_NTYPES, "one factory preset per instrument for now");
    char idx[8];
    snprintf(idx, sizeof(idx), "%d", quilt_type_index(quilt_instrument_by_name("Marimba")));
    A->set_param(p, "preset", idx);
    CHECK(is(p, "type", "Marimba") && is(p, "preset_name", "Marimba"), "a preset sets the instrument");
    CHECK(fabs(num(p, "decay") - 0.58) < 1e-3, "and Main to suit it");
    A->set_param(p, "preset", "38");
    CHECK(is(p, "type", "Marimba"), "a preset past the end is ignored");
}

/* Turning TYPE brings the instrument's whole default sound, on every page,
 * the same as its factory preset; only VOL is left as it was. */
static shape_t shape_of(const char *slug) { return QUILT_INST[quilt_instrument_by_slug(slug, (int)strlen(slug))].shape; }
static void dirty(void *p, const char *slug) {
    char k[64];
    const shape_t sh = shape_of(slug);
    for (int g = 0; g < G_COUNT; g++)
        if (g != G_VOLUME) A->set_param(p, QUILT_GLOBALS[g].key, "0.01");
    snprintf(k, sizeof(k), "c_%s", slug);
    A->set_param(p, k, "0.99");
    for (int i = 0; i < quilt_shape_key_count(sh); i++) A->set_param(p, quilt_shape_param(sh, i)->key, "0.97");
    A->set_param(p, "volume", "-12");
}
static int same_sound(void *p, void *ref, const char *slug) {
    char k[64];
    int ok = 1;
    for (int g = 0; g < G_COUNT; g++)
        if (g != G_VOLUME) ok &= fabs(num(p, QUILT_GLOBALS[g].key) - num(ref, QUILT_GLOBALS[g].key)) < 1e-4;
    snprintf(k, sizeof(k), "c_%s", slug);
    ok &= fabs(num(p, k) - num(ref, k)) < 1e-4;
    const shape_t sh = shape_of(slug);
    for (int i = 0; i < quilt_shape_key_count(sh); i++) {
        const char *key = quilt_shape_param(sh, i)->key;
        ok &= fabs(num(p, key) - num(ref, key)) < 1e-4;
    }
    return ok;
}
static void voicings(void *p) {
    for (int t = 0; t < QUILT_NTYPES; t++) {
        const int inst = quilt_type_inst(t);
        const char *slug = QUILT_INST[inst].slug;
        void *ref = A->create_instance(".", "");
        char idx[8];
        snprintf(idx, sizeof(idx), "%d", t);
        A->set_param(ref, "preset", idx);
        A->set_param(p, "type", QUILT_TYPE_NAMES[(t + 1) % QUILT_NTYPES]);
        dirty(p, QUILT_INST[quilt_type_inst((t + 1) % QUILT_NTYPES)].slug);
        A->set_param(p, "type", QUILT_TYPE_NAMES[t]);
        CHECK(same_sound(p, ref, slug), "turning TYPE to %s sets every page to its default sound",
              QUILT_TYPE_NAMES[t]);
        CHECK(fabs(num(p, "volume") + 12) < 1e-3, "and leaves VOL alone");
        CHECK(is(p, "preset_name", QUILT_TYPE_NAMES[t]), "and names its preset");
        dirty(p, slug);
        A->set_param(p, "type", QUILT_TYPE_NAMES[t]);
        const char *k0 = quilt_shape_param(QUILT_INST[inst].shape, 0)->key;
        CHECK(fabs(num(p, k0) - 0.97) < 1e-4 && fabs(num(p, "soft") - 0.01) < 1e-4,
              "writing %s again keeps what was turned", QUILT_TYPE_NAMES[t]);
        A->destroy_instance(ref);
    }
    A->set_param(p, "type", "Felt Upright");
    A->set_param(p, "b_edge", "0.99");
    CHECK(fabs(num(p, "b_edge") - 0.4) < 1e-3, "a bowed key written to a piano went nowhere");
    A->set_param(p, "volume", "-120");
    CHECK(fabs(num(p, "volume") + 60) < 1e-3, "volume clamps at -60 dB");
    A->set_param(p, "volume", "0");
}

/* A saved sound comes back as it was saved, not as the instrument's default. */
static void state(void *p) {
    A->set_param(p, "type", "Felt Upright");
    A->set_param(p, "type", "Una Corda Grand");
    A->set_param(p, "soft", "0.33");
    A->set_param(p, "size", "0.81");
    A->set_param(p, "m_split", "0.9");
    A->set_param(p, "c_una_corda_grand", "0.12");
    char *blob = strdup(get(p, "state"));
    printf("state: %d bytes\n", (int)strlen(blob));

    void *q = A->create_instance(".", "");
    A->set_param(q, "state", blob);
    CHECK(is(q, "type", "Una Corda Grand"), "state: type");
    CHECK(fabs(num(q, "soft") - 0.33) < 1e-3 && fabs(num(q, "size") - 0.81) < 1e-3, "state: Main and Effects");
    CHECK(fabs(num(q, "m_split") - 0.9) < 1e-3 && fabs(num(q, "c_una_corda_grand") - 0.12) < 1e-3,
          "state: the instrument's own values");
    char *again = strdup(get(q, "state"));
    CHECK(!strcmp(blob, again), "state round-trips exactly");
    /* As the chain host restores a patch: the preset first, then the state. */
    void *r = A->create_instance(".", "");
    A->set_param(r, "preset", "1");
    A->set_param(r, "state", blob);
    CHECK(fabs(num(r, "soft") - 0.33) < 1e-3 && fabs(num(r, "m_split") - 0.9) < 1e-3,
          "state: preset then state, as the host restores, keeps the saved sound");
    A->destroy_instance(r);
    A->set_param(q, "state", "{\"v\":7,\"future\":{\"x\":1},\"type\":\"Vibraphone\",\"soft\":\"x\"}");
    CHECK(is(q, "type", "Vibraphone"), "state: unknown keys and values are skipped");
    A->set_param(q, "state", "{\"type\":\"Choir\"}");
    CHECK(is(q, "type", "Vibraphone"), "state: an instrument not offered is skipped");
    A->set_param(q, "state", "not json");
    CHECK(is(q, "type", "Vibraphone"), "state: junk changes nothing");
    free(again);
    free(blob);
    A->destroy_instance(q);
}

/* Every instrument sounds, stays in range, and goes quiet after release. */
static void sound(void) {
    for (int t = 0; t < QUILT_NTYPES; t++) {
        void *p = A->create_instance(".", "");
        char idx[8];
        snprintf(idx, sizeof(idx), "%d", t);
        A->set_param(p, "preset", idx);
        const char *name = QUILT_TYPE_NAMES[t];
        int peak = 0;
        for (int v = 0; v < 3; v++) midi3(p, 0x90, 48 + 12 * v, 40 + 40 * v);
        double on = rms(p, 60, &peak);
        CHECK(on > 300, "%s sounds (rms %.0f)", name, on);
        CHECK(peak < 32767, "%s stays inside full scale (peak %d)", name, peak);
        for (int v = 0; v < 3; v++) midi3(p, 0x80, 48 + 12 * v, 0);
        /* Within 25 s it is silent, or, with no dampers to stop it (a
         * glockenspiel, a dulcimer), still falling and 60 dB down: no note
         * is ever left hanging. */
        double after = 1e9, start = rms(p, 10, NULL);
        for (int w = 0; w < 86 && after >= 1.0; w++) { rms(p, 90, NULL); after = rms(p, 10, NULL); }
        CHECK(after < 1.0 || after < start * 1e-3, "%s goes quiet after release (rms %.0f -> %.2f)", name, start, after);
        A->destroy_instance(p);
    }
}

static void pedal(void) {
    /* Struck notes ring on under the sustain pedal, and stop without it. */
    void *p = A->create_instance(".", "");
    A->set_param(p, "space", "0");
    midi3(p, 0xB0, 64, 127);
    midi3(p, 0x90, 60, 100);
    midi3(p, 0x80, 60, 0);
    rms(p, 100, NULL);
    double held = rms(p, 20, NULL);
    midi3(p, 0xB0, 64, 0);
    rms(p, 200, NULL);
    double dropped = rms(p, 20, NULL);
    CHECK(held > 300 && dropped < held * 0.05, "the pedal holds a struck note (%.0f -> %.0f)", held, dropped);

    /* More notes than voices: the quietest is stolen, with no overflow. */
    for (int n = 0; n < 40; n++) midi3(p, 0x90, 30 + n, 127);
    int peak = 0;
    rms(p, 50, &peak);
    CHECK(peak < 32767, "forty notes at once stay inside full scale");
    midi3(p, 0xB0, 123, 0);
    A->destroy_instance(p);
}

/* ---- physics, against the formulas and the papers (DESIGN.md, Testing) ---- */

#define PI_D 3.14159265358979
static float wave[44100 * 6];

/* A fresh instance playing one note, dry, with the knock and body off unless
 * asked, rendered into wave[] (left channel) once the knobs have settled. */
static void *note(const char *type, int key, int vel, int frames, int release_at, const char *extra) {
    void *p = A->create_instance(".", "");
    A->set_param(p, "type", type);
    /* Measured against the shared defaults, not the instrument's voicing, so
     * these check the engine; the voicings are listened to. */
    for (int g = 0; g < G_COUNT; g++) {
        char v[16];
        snprintf(v, sizeof(v), "%g", (double)QUILT_GLOBALS[g].def);
        A->set_param(p, QUILT_GLOBALS[g].key, v);
    }
    A->set_param(p, "space", "0");
    A->set_param(p, "volume", "-24");
    A->set_param(p, "m_noise", "0");
    A->set_param(p, "m_body", "0");
    A->set_param(p, "p_noise", "0");
    A->set_param(p, "p_body", "0");
    A->set_param(p, "b_noise", "0");
    A->set_param(p, "b_body", "0");
    for (const char *e = extra; e && *e;) {           /* "key=value;key=value" */
        char k[32], v[32];
        int n = 0;
        if (sscanf(e, "%31[^=]=%31[^;]%n", k, v, &n) < 2) break;
        A->set_param(p, k, v);
        e += n;
        if (*e == ';') e++;
    }
    int16_t out[256];
    for (int b = 0; b < 520; b++) A->render_block(p, out, 128);   /* let the knobs settle, 1.5 s */
    midi3(p, 0x90, key, vel);
    for (int f = 0; f < frames; f += 128) {
        if (release_at >= 0 && f <= release_at && release_at < f + 128) midi3(p, 0x80, key, 0);
        A->render_block(p, out, 128);
        for (int i = 0; i < 128 && f + i < (int)(sizeof(wave) / sizeof(wave[0])); i++)
            wave[f + i] = out[2 * i] / 32768.0f;
    }
    return p;
}

/* Hann-windowed magnitude at one frequency, over n samples from a. */
static double mag(int a, int n, double f) {
    double re = 0, im = 0;
    for (int i = 0; i < n; i++) {
        double w = (0.5 - 0.5 * cos(2 * PI_D * i / n)) * wave[a + i], ph = 2 * PI_D * f * i / QUILT_SR;
        re += w * cos(ph);
        im += w * sin(ph);
    }
    return sqrt(re * re + im * im);
}

/* The strongest frequency within +-span of f, in 0.1 Hz steps after a coarse pass. */
static double peak_near(int a, int n, double f, double span) {
    double best = f, bm = -1;
    for (double x = f - span; x <= f + span; x += 0.5) { double m = mag(a, n, x); if (m > bm) { bm = m; best = x; } }
    for (double x = best - 0.5; x <= best + 0.5; x += 0.05) { double m = mag(a, n, x); if (m > bm) { bm = m; best = x; } }
    return best;
}

static double rms_at(int a, int n) {
    double acc = 0;
    for (int i = 0; i < n; i++) acc += (double)wave[a + i] * wave[a + i];
    return sqrt(acc / n);
}

/* Spectral centroid, 50 Hz to 8 kHz, of 4096 samples from 10 ms in. */
static double centroid(void) {
    double num = 0, den = 0;
    for (double f = 50; f < 8000; f += 25) { double m = mag(441, 4096, f); num += m * f; den += m; }
    return num / den;
}

static void contact(void) {
    /* Chaigne & Askenfelt's C4 hammer and string: as velocity rises, contact
     * shortens and force rises, within the ranges they report. */
    static contact_t c;
    float last_ms = 1e9f, last_peak = 0;
    const float vs[] = { 0.5f, 1.0f, 2.0f, 4.0f };
    for (int i = 0; i < 4; i++) {
        contact_strike_string(&c, 2.97e-3f, 4.5e9f, 2.5f, 0, 1.62f, 0.12f, 261.6f, vs[i]);
        int n = 0, first = -1, lastn = 0;
        while (c.active) { if (contact_step(&c) > 0) { if (first < 0) first = n; lastn = n; } n++; }
        float ms = (float)(lastn - first + 1) / 44.1f;
        CHECK(ms > 1.0f && ms < 4.0f, "C4 hammer at %.1f m/s: contact %.2f ms, 1-4 ms", vs[i], ms);
        CHECK(c.peak > 1.0f && c.peak < 40.0f, "C4 hammer at %.1f m/s: peak %.1f N, 1-40 N", vs[i], c.peak);
        CHECK(ms < last_ms && c.peak > last_peak, "a faster hammer is briefer and harder");
        CHECK(c.vh < 0, "the hammer rebounds");
        last_ms = ms;
        last_peak = c.peak;
    }
}

static void strings(void) {
    /* Felt Upright partials sit at k f0 sqrt(1 + B k^2), not at k f0. */
    const int key = 48, inst = quilt_instrument_by_name("Felt Upright");
    void *p = note("Felt Upright", key, 100, 16384 + 2048, -1, "m_split=0;c_felt_upright=0;soft=0.2");
    double f0 = quilt_note_freq(inst, key), B = quilt_string_B(inst, key, 0.5f);
    for (int k = 1; k <= 8; k++) {
        double want = k * f0 * sqrt(1 + B * k * k), got = peak_near(1024, 16384, want, want * 0.012);
        CHECK(fabs(got - want) / want < 0.0015, "string partial %d at %.2f Hz, formula %.2f Hz", k, got, want);
        if (k >= 6) CHECK(fabs(got - want) < fabs(got - k * f0), "partial %d is stretched, not harmonic", k);
    }
    A->destroy_instance(p);
}

static void bars(void) {
    /* The tunings DESIGN.md declares: undercut bars 1 : 4 : 10 and 1 : 3, free
     * bars 1 : 2.76 : 5.40, clamped rods and tines 1 : 6.27 : 17.55. */
    /* Tubular bells: modes 4, 5 and 6 of a tube, as 81 : 121 : 169, the
     * strike note an octave below mode 4. Bells: prime, nominal, tierce. The
     * handpan and tongue drum: fundamental, octave or twelfth. `ref` is where
     * the reference mode sits against the played pitch. */
    const struct { const char *type; double ref, r[3]; int n; } b[] = {
        { "Vibraphone", 1, { 1, 4, 10 }, 2 }, { "Marimba", 1, { 1, 4, 10 }, 3 }, { "Xylophone", 1, { 1, 3, 0 }, 2 },
        { "Glockenspiel", 1, { 1, 2.76, 5.40 }, 3 }, { "Celesta", 1, { 1, 2.76, 5.40 }, 2 },
        { "Toy Piano", 1, { 1, 6.27, 17.55 }, 2 }, { "Kalimba / Music Box", 1, { 1, 6.27, 0 }, 2 },
        { "Tubular Bells", 2, { 1, 121.0 / 81, 169.0 / 81 }, 3 }, { "Handbells", 1, { 1, 2, 1.2 }, 3 },
        { "Handpan", 1, { 1, 2, 3 }, 3 }, { "Tongue Drum", 1, { 1, 3, 2 }, 3 },
    };
    for (int i = 0; i < (int)(sizeof(b) / sizeof(b[0])); i++) {
        const int inst = quilt_instrument_by_name(b[i].type);
        /* Two octaves down for the glockenspiel, so its top mode stays in range. */
        const int key = !strcmp(b[i].type, "Glockenspiel") ? 48 : 60;
        void *p = note(b[i].type, key, 110, 16384 + 512, -1, "m_split=0;soft=0.2;c_celesta=1;c_toy_piano=1;c_glockenspiel=1");
        const double fn = quilt_note_freq(inst, key) * b[i].ref;
        double f0 = peak_near(256, 16384, fn, fn * 0.03);
        for (int k = 1; k < b[i].n; k++) {
            double got = peak_near(256, 8192, f0 * b[i].r[k], f0 * b[i].r[k] * 0.03);
            CHECK(fabs(got / f0 - b[i].r[k]) / b[i].r[k] < 0.015, "%s mode %d at %.2f x f0, declared %.2f",
                  b[i].type, k + 1, got / f0, b[i].r[k]);
        }
        A->destroy_instance(p);
    }
}

static void softness(void) {
    /* Gentler is darker, from the contact alone: velocity, SOFT, and each CHAR
     * that softens (FELT, HUSH). */
    const char *const *types = QUILT_TYPE_NAMES;
    for (int i = 0; i < QUILT_NTYPES; i++) {
        /* At full volume: a soft note at -24 dB would sit on the 16-bit floor. */
        void *p = note(types[i], 60, 30, 8192, -1, "volume=0;c_felt_upright=0;c_kalimba_music_box=0;c_hammered_dulcimer=0");
        double soft = centroid();
        A->destroy_instance(p);
        p = note(types[i], 60, 120, 8192, -1, "volume=0;c_felt_upright=0;c_kalimba_music_box=0;c_hammered_dulcimer=0");
        double hard = centroid();
        A->destroy_instance(p);
        if (banks_supports(quilt_type_inst(i)))
            CHECK(fabs(soft - hard) < 1, "%s: has no touch, so velocity leaves the tone alone (%.0f Hz, %.0f Hz)",
                  types[i], soft, hard);
        else
            CHECK(soft < hard * 0.9, "%s: soft strike darker (%.0f Hz) than hard (%.0f Hz)", types[i], soft, hard);
        p = note(types[i], 60, 90, 8192, -1, "soft=0.95;volume=0;c_kalimba_music_box=0");
        double felt = centroid();
        A->destroy_instance(p);
        p = note(types[i], 60, 90, 8192, -1, "soft=0.05;volume=0;c_kalimba_music_box=0");
        double bare = centroid();
        A->destroy_instance(p);
        CHECK(felt < bare * 0.9, "%s: SOFT darkens (%.0f Hz vs %.0f Hz)", types[i], felt, bare);
    }
    void *p = note("Felt Upright", 60, 90, 8192, -1, "c_felt_upright=1;volume=0");
    double strip = centroid();
    A->destroy_instance(p);
    p = note("Felt Upright", 60, 90, 8192, -1, "c_felt_upright=0;volume=0");
    double none = centroid();
    A->destroy_instance(p);
    CHECK(strip < none * 0.9, "FELT darkens (%.0f Hz vs %.0f Hz)", strip, none);
}

static void double_decay(void) {
    /* Weinreich: the fundamental falls fast at first, then sings on. HUSH
     * (fewer strings struck) raises the aftersound. */
    double ratio[2];
    for (int h = 0; h < 2; h++) {
        void *p = note("Una Corda Grand", 48, 100, 44100 * 5, -1, h ? "c_una_corda_grand=1" : "c_una_corda_grand=0");
        double f0 = quilt_note_freq(quilt_instrument_by_name("Una Corda Grand"), 48);
        double a = 20 * log10(mag(4410, 4096, f0)), b = 20 * log10(mag(44100, 4096, f0));
        double c = 20 * log10(mag(44100 * 3, 4096, f0)), d = 20 * log10(mag(44100 * 4, 4096, f0));
        double early = (a - b) / 0.9, late = (c - d) / 1.0;
        if (!h) CHECK(early > late * 1.5, "two-stage decay: %.1f dB/s, then %.1f dB/s", early, late);
        ratio[h] = c - a;
        A->destroy_instance(p);
    }
    CHECK(ratio[1] > ratio[0] + 3, "HUSH raises the aftersound (%.1f dB vs %.1f dB)", ratio[1], ratio[0]);
}

static void dampers(void) {
    void *p = note("Felt Upright", 60, 100, 44100, 22050, NULL);
    double before = rms_at(20000, 2000), after = rms_at(22050 + 13230, 2000);
    CHECK(after < before * 0.03, "released, the damper stops a piano note (%.1f dB)", 20 * log10(after / before));
    A->destroy_instance(p);
    p = note("Marimba", 48, 100, 44100, 4410, NULL);
    double ring = rms_at(8820, 2000);
    A->destroy_instance(p);
    p = note("Marimba", 48, 100, 44100, -1, NULL);
    double held = rms_at(8820, 2000);
    A->destroy_instance(p);
    CHECK(fabs(20 * log10(ring / held)) < 0.5, "a marimba has no dampers: release changes nothing");
    p = note("Vibraphone", 60, 100, 44100, 4410, "m_damp=0.5");
    CHECK(rms_at(4410 + 13230, 2000) < rms_at(2000, 2000) * 0.03, "the vibraphone's damper bar stops a released note");
    A->destroy_instance(p);
}

/* The strongest modulation of a note's level, 1-12 Hz (or at `only`), from 5 ms frames. */
static double wobble(int a, int seconds, double *strength, double only) {
    int frames = seconds * 200;
    static double env[2400];
    double mean = 0;
    for (int i = 0; i < frames; i++) { env[i] = log(rms_at(a + i * 220, 220) + 1e-9); mean += env[i]; }
    mean /= frames;
    double best = 0, bm = -1, total = 0;
    for (double f = only > 0 ? only : 1; f <= (only > 0 ? only : 12); f += 0.05) {
        double re = 0, im = 0;
        for (int i = 0; i < frames; i++) { double x = env[i] - mean - (env[frames - 1] - env[0]) * ((double)i / frames - 0.5);
            re += x * cos(2 * PI_D * f * i / 200); im += x * sin(2 * PI_D * f * i / 200); }
        double m = sqrt(re * re + im * im);
        total += m;
        if (m > bm) { bm = m; best = f; }
    }
    (void)total;
    *strength = bm;
    return best;
}

static void motor(void) {
    double s_on, s_off;
    void *p = note("Vibraphone", 60, 100, 44100 * 3, -1, "c_vibraphone=0.5;sway=1;m_body=0.9");
    double rate = wobble(4410, 2, &s_on, 0);
    A->destroy_instance(p);
    double want = 0.7 + 10.0 * 0.5;
    CHECK(fabs(rate - want) < 0.4, "the vibraphone fan turns at %.2f Hz (MOTOR 50%% is %.1f Hz)", rate, want);
    p = note("Vibraphone", 60, 100, 44100 * 3, -1, "c_vibraphone=0;sway=1;m_body=0.9");
    wobble(4410, 2, &s_off, rate);
    A->destroy_instance(p);
    CHECK(s_on > 3 * s_off, "with the motor off there is no fan (depth %.1f vs %.1f)", s_on, s_off);
}

static void roll(void) {
    const char *rollers[] = { "Marimba", "Xylophone" }, *keys[] = { "c_marimba", "c_xylophone" };
    for (int m = 0; m < 2; m++) {
        int onsets[2];
        for (int r = 0; r < 2; r++) {
            char e[64];
            snprintf(e, sizeof(e), "%s=%s", keys[m], r ? "0.8" : "0");
            void *p = note(rollers[m], 60, 90, 44100, -1, e);
            int n = 0;
            for (int i = 2; i < 200; i++)           /* rises in 5 ms level */
                if (rms_at(i * 220, 220) > 1.3 * rms_at((i - 1) * 220, 220) + 1e-5) n++;
            onsets[r] = n;
            A->destroy_instance(p);
        }
        CHECK(onsets[0] == 0, "%s ROLL 0: one strike (%d re-strikes)", rollers[m], onsets[0]);
        CHECK(onsets[1] >= 5, "%s ROLL 80%%: a held note rolls (%d re-strikes in 1 s)", rollers[m], onsets[1]);
    }
}

static void budget(void) {
    void *p = A->create_instance(".", "");
    A->set_param(p, "type", "Felt Upright");
    int over = 0, peak_use = 0;
    for (int k = 0; k < 40; k++) {
        midi3(p, 0x90, 24 + k, 100);
        int use = quilt_modal_in_use((quilt_t *)p);
        if (use > peak_use) peak_use = use;
        if (use > MODAL_BUDGET) over++;
        rms(p, 2, NULL);
    }
    CHECK(!over, "40 low notes never exceed the budget (peak %d of %d oscillators)", peak_use, MODAL_BUDGET);
    int peak = 0;
    rms(p, 200, &peak);
    CHECK(peak < 32767, "and stay inside full scale");
    A->destroy_instance(p);
}

static void reverb(void) {
    double tail[2];
    for (int s = 0; s < 2; s++) {
        void *p = A->create_instance(".", "");
        A->set_param(p, "type", "Marimba");
        A->set_param(p, "space", s ? "0.8" : "0");
        A->set_param(p, "size", "1");
        midi3(p, 0x90, 60, 110);
        rms(p, 345, NULL);                 /* 1 s */
        tail[s] = rms(p, 345, NULL);       /* the second after */
        if (s) {
            rms(p, 345 * 20, NULL);
            double gone = rms(p, 100, NULL);
            CHECK(gone < 2.0, "the plate dies away at full SIZE (rms %.2f after 20 s)", gone);
        }
        A->destroy_instance(p);
    }
    CHECK(tail[1] > tail[0] * 10, "SPACE leaves a tail (%.0f vs %.0f)", tail[1], tail[0]);
}

/* ---- every knob is clearly audible (DESIGN.md, Every knob is heard) ----
 *
 * Each knob, turned from one end to the other, must change what it names by
 * at least a set amount, measured the way it is heard: a partial against the
 * fundamental for brightness, the level some time after release for decay and
 * damping, the depth of the level's wobble for SWAY, MOTOR and SPLIT, and so
 * on. "Moves the right way" is not enough; it has to be heard on the Move. */

static double db(double x) { return 20 * log10(x + 1e-12); }

/* A partial's level, in dB, at the strongest frequency within 2% of f. */
static double part_db(int a, int n, double f) { return db(mag(a, n, peak_near(a, n, f, f * 0.02))); }

/* The depth of the level's wobble over `n` samples from a, in dB peak to
 * peak, after taking out the steady decay (a straight line in dB). */
static double am_depth(int a, int n) {
    int frames = n / 441;
    static double env[2000];
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (int i = 0; i < frames; i++) {
        env[i] = db(rms_at(a + i * 441, 441));
        sx += i; sy += env[i]; sxx += (double)i * i; sxy += i * env[i];
    }
    double slope = (frames * sxy - sx * sy) / (frames * sxx - sx * sx), icpt = (sy - slope * sx) / frames;
    double lo = 1e9, hi = -1e9;
    for (int i = 0; i < frames; i++) {
        double r = env[i] - (icpt + slope * i);
        if (r < lo) lo = r;
        if (r > hi) hi = r;
    }
    return hi - lo;
}

/* Renders the same note with `key` at 0 and at 1; the caller measures each. */
static double turn(const char *type, int key, int vel, int frames, int release_at, const char *extra,
                   const char *knob, double (*m)(void), double *at0, double *at1) {
    char e[160];
    for (int end = 0; end < 2; end++) {
        snprintf(e, sizeof(e), "%s%s%s=%d", extra ? extra : "", extra ? ";" : "", knob, end);
        void *p = note(type, key, vel, frames, release_at, e);
        *(end ? at1 : at0) = m();
        A->destroy_instance(p);
    }
    return *at1 - *at0;
}

#define TYPES QUILT_TYPE_NAMES
static int is_bar(int i) { return quilt_bar_ratio(quilt_type_inst(i), 0) > 0; }
static double cur_f0;
static double cur_ratio;
static double cur_window = 0.04;   /* where m_stretch looks: a bar's modes move further */
static double m_partial(void) { return part_db(441, 8192, cur_f0 * cur_ratio) - part_db(441, 8192, cur_f0); }
static double m_level(void) { return db(rms_at(0, 22050)); }
static double m_tail(void) { return db(rms_at(44100 * 3 / 2, 4410)); }
static double m_release(void) { return db(rms_at(22050 + 13230, 4410)) - db(rms_at(18000, 4000)); }
static double m_wobble(void) { return am_depth(22050, 88200); }
static double m_stretch(void) {
    double f1 = peak_near(1024, 16384, cur_f0, cur_f0 * 0.02);
    double f8 = peak_near(1024, 16384, cur_f0 * cur_ratio, cur_f0 * cur_ratio * cur_window);
    return 1200 * log2(f8 / (f1 * cur_ratio));
}

static void audible(void) {
    double a0, a1, d;
    for (int i = 0; i < QUILT_NTYPES; i++) {
        const char *t = TYPES[i];
        const int inst = quilt_instrument_by_name(t);
        if (QUILT_INST[inst].shape != SH_MODAL) continue;   /* see machines() */
        /* A bar is measured from its first mode (on a tubular bell, mode 4,
         * not the strike note), against its second; a string, its fourth
         * partial against its first. */
        const double bar0 = is_bar(i) ? quilt_bar_ratio(inst, 0) : 1.0;
        cur_f0 = quilt_note_freq(inst, 60) * bar0;
        cur_ratio = is_bar(i) ? quilt_bar_ratio(inst, 1) / bar0 : 4 * sqrt(1 + quilt_string_B(inst, 60, 0.5f) * 16);

        d = turn(t, 60, 90, 8192 + 441, -1, "volume=0", "soft", m_partial, &a0, &a1);
        CHECK(d < -12, "%s SOFT: the overtones fall at least 12 dB (%.1f -> %.1f dB)", t, a0, a1);
        d = turn(t, 60, 90, 22050, -1, "volume=0", "soft", m_level, &a0, &a1);
        CHECK(fabs(d) < 10, "%s SOFT: changes the tone more than the level (%+.1f dB)", t, d);

        d = turn(t, 60, 90, 44100 * 2, -1, "volume=0", "decay", m_tail, &a0, &a1);
        CHECK(d > 12, "%s DECAY: rings at least 12 dB longer (%.1f -> %.1f dB at 1.5 s)", t, a0, a1);

        d = turn(t, 60, 100, 44100, 22050, "volume=0;m_damp=0.5", "m_damp", m_release, &a0, &a1);
        CHECK(d < -20, "%s DAMP: stops a released note at least 20 dB sooner (%.1f -> %.1f dB)", t, a0, a1);

        /* TONE is a tilt, so it is measured per octave between the two
         * modes: on a bell they may be half an octave apart, on a piano two. */
        d = turn(t, 60, 90, 8192 + 441, -1, "volume=0", "tone", m_partial, &a0, &a1);
        CHECK(d / log2(cur_ratio) > 4, "%s TONE: tilts at least 4 dB per octave (%.1f -> %.1f dB over %.2f octaves)",
              t, a0, a1, log2(cur_ratio));

        d = turn(t, 60, 90, 22050, -1, "volume=0", "m_body", m_level, &a0, &a1);
        CHECK(d > 3, "%s BODY: at least 3 dB more body (%+.1f dB)", t, d);

        d = turn(t, 60, 90, 44100 * 3, -1, "volume=0;m_split=0;decay=1;c_hammered_dulcimer=0", "m_split", m_wobble, &a0, &a1);
        CHECK(a1 > 3 && a1 > a0 + 2, "%s SPLIT: the note beats (wobble %.1f -> %.1f dB)", t, a0, a1);

        if (is_bar(i)) {
            cur_ratio = quilt_bar_ratio(inst, 1) / bar0;
            d = turn(t, 60, 90, 8192 + 441, -1, "volume=0", "m_spot", m_partial, &a0, &a1);
            CHECK(d > 12, "%s SPOT: the overtones rise at least 12 dB toward the end (%.1f -> %.1f dB)", t, a0, a1);
            cur_window = 0.12;
            d = turn(t, 60, 90, 16384 + 1024, -1, "volume=0;soft=0.2", "m_stiff", m_stretch, &a0, &a1);
            cur_window = 0.04;
            CHECK(d > 20, "%s STIFF: moves the overtones at least 20 cents (%.0f -> %.0f cents)", t, a0, a1);
        } else {
            /* Struck at the middle, the second partial vanishes and it goes hollow. */
            cur_ratio = 2 * sqrt(1 + quilt_string_B(inst, 60, 0.5f) * 4);
            d = turn(t, 60, 90, 8192 + 441, -1, "volume=0", "m_spot", m_partial, &a0, &a1);
            CHECK(d < -15, "%s SPOT: the middle of the string hollows the second partial (%.1f -> %.1f dB)", t, a0, a1);
            cur_f0 = quilt_note_freq(inst, 48);
            cur_ratio = 4;
            d = turn(t, 48, 100, 16384 + 1024, -1, "volume=0;soft=0.2;c_felt_upright=0;c_una_corda_grand=0;"
                     "c_hammered_dulcimer=0;m_split=0", "m_stiff", m_stretch, &a0, &a1);
            CHECK(d > 20, "%s STIFF: stretches the fourth partial at least 20 cents (%.0f -> %.0f cents)", t, a0, a1);
        }
    }

    /* NOISE: the knock of the action against the note, in the first 50 ms. */
    for (int i = 0; i < QUILT_NTYPES; i++) {
        static float quiet[2205];
        if (QUILT_INST[quilt_type_inst(i)].shape != SH_MODAL) continue;
        void *p = note(TYPES[i], 60, 90, 2304, -1, "volume=0;m_noise=0");
        memcpy(quiet, wave, sizeof(quiet));
        double sig = rms_at(0, 2205);
        A->destroy_instance(p);
        p = note(TYPES[i], 60, 90, 2304, -1, "volume=0;m_noise=1");
        double acc = 0;
        for (int k = 0; k < 2205; k++) acc += (wave[k] - quiet[k]) * (wave[k] - quiet[k]);
        double knock = db(sqrt(acc / 2205)) - db(sig);
        CHECK(knock > -14, "%s NOISE: the knock is heard, %.1f dB against the note", TYPES[i], knock);
        A->destroy_instance(p);
    }

    /* The CHAR cells. */
    cur_f0 = quilt_note_freq(0, 60);
    cur_ratio = 4 * sqrt(1 + quilt_string_B(0, 60, 0.5f) * 16);
    d = turn("Felt Upright", 60, 90, 8192 + 441, -1, "volume=0", "c_felt_upright", m_partial, &a0, &a1);
    CHECK(d < -12, "FELT: the strip takes at least 12 dB off the overtones (%.1f -> %.1f dB)", a0, a1);
    d = turn("Felt Upright", 60, 90, 22050, -1, "volume=0", "c_felt_upright", m_level, &a0, &a1);
    CHECK(fabs(d) < 10, "FELT: changes the tone more than the level (%+.1f dB)", d);
    int uc = quilt_instrument_by_name("Una Corda Grand");
    cur_f0 = quilt_note_freq(uc, 60);
    cur_ratio = 4 * sqrt(1 + quilt_string_B(uc, 60, 0.5f) * 16);
    d = turn("Una Corda Grand", 60, 90, 8192 + 441, -1, "volume=0", "c_una_corda_grand", m_partial, &a0, &a1);
    CHECK(d < -8, "HUSH: at least 8 dB off the overtones (%.1f -> %.1f dB)", a0, a1);

    /* MUTE: the hand on the tube. */
    {
        const int tb = quilt_instrument_by_name("Tubular Bells");
        cur_f0 = quilt_note_freq(tb, 60) * 2;
        d = turn("Tubular Bells", 60, 90, 44100 * 2, -1, "volume=0", "c_tubular_bells", m_tail, &a0, &a1);
        CHECK(d < -12, "Tubular Bells MUTE: rings at least 12 dB shorter (%.1f -> %.1f dB at 1.5 s)", a0, a1);
    }
    /* MINOR: the tierce; RING: the tuned octave. */
    {
        const int hb = quilt_instrument_by_name("Handbells");
        cur_f0 = quilt_note_freq(hb, 60);
        cur_ratio = 1.189;
        d = turn("Handbells", 60, 90, 8192 + 441, -1, "volume=0;m_split=0", "c_handbells", m_partial, &a0, &a1);
        CHECK(d > 12, "Handbells MINOR: the tierce at least 12 dB up (%.1f -> %.1f dB)", a0, a1);
        const char *rings[] = { "Handpan", "Tongue Drum" }, *rkeys[] = { "c_handpan", "c_tongue_drum" };
        const double rr[] = { 2, 3 };
        for (int k = 0; k < 2; k++) {
            cur_f0 = quilt_note_freq(quilt_instrument_by_name(rings[k]), 60);
            cur_ratio = rr[k];
            d = turn(rings[k], 60, 90, 8192 + 441, -1, "volume=0;m_split=0", rkeys[k], m_partial, &a0, &a1);
            CHECK(d > 12, "%s RING: the tuned overtone at least 12 dB up (%.1f -> %.1f dB)", rings[k], a0, a1);
        }
    }
    /* BLOOM: the undamped courses ring on; TWANG: the pickups bring the overtones forward. */
    {
        const int hd = quilt_instrument_by_name("Hammered Dulcimer");
        cur_f0 = quilt_note_freq(hd, 60);
        d = turn("Hammered Dulcimer", 60, 90, 44100 * 2, -1, "volume=0", "c_hammered_dulcimer", m_tail, &a0, &a1);
        CHECK(d > 6, "Hammered Dulcimer BLOOM: rings at least 6 dB more at 1.5 s (%.1f -> %.1f dB)", a0, a1);
        const int eg = quilt_instrument_by_name("Electric Grand");
        cur_f0 = quilt_note_freq(eg, 60);
        cur_ratio = 4 * sqrt(1 + quilt_string_B(eg, 60, 0.5f) * 16);
        d = turn("Electric Grand", 60, 90, 8192 + 441, -1, "volume=0", "c_electric_grand", m_partial, &a0, &a1);
        CHECK(d > 8, "Electric Grand TWANG: the overtones at least 8 dB forward (%.1f -> %.1f dB)", a0, a1);
    }
    /* BARK and BITE: the tine or reed nearer its pickup, so a firm note
     * folds over into its second harmonic, at about the same level. */
    {
        const char *pus[] = { "Tine Piano", "Reed Piano" }, *pkeys[] = { "c_tine_piano", "c_reed_piano" };
        const char *what[] = { "BARK", "BITE" };
        for (int k = 0; k < 2; k++) {
            cur_f0 = quilt_note_freq(quilt_instrument_by_name(pus[k]), 60);
            cur_ratio = 2;
            d = turn(pus[k], 60, 100, 8192 + 441, -1, "volume=0;m_split=0", pkeys[k], m_partial, &a0, &a1);
            CHECK(d > 10, "%s %s: the second harmonic at least 10 dB up (%.1f -> %.1f dB)", pus[k], what[k], a0, a1);
            d = turn(pus[k], 60, 100, 22050, -1, "volume=0", pkeys[k], m_level, &a0, &a1);
            CHECK(fabs(d) < 6, "%s %s: changes the tone more than the level (%+.1f dB)", pus[k], what[k], d);
        }
    }

    /* BELL: the upper modes come up. */
    const char *bells[] = { "Celesta", "Toy Piano", "Glockenspiel" };
    for (int b = 0; b < 3; b++) {
        const int inst = quilt_instrument_by_name(bells[b]);
        char key[48];
        snprintf(key, sizeof(key), "c_%s", QUILT_INST[inst].slug);
        cur_f0 = quilt_note_freq(inst, 60);
        cur_ratio = quilt_bar_ratio(inst, 1);
        d = turn(bells[b], 60, 90, 8192 + 441, -1, "volume=0", key, m_partial, &a0, &a1);
        CHECK(d > 12, "%s BELL: the upper modes at least 12 dB up (%.1f -> %.1f dB)", bells[b], a0, a1);
    }
    /* BUZZ: the buzzers' sizzle, 6 to 12 kHz, where a kalimba's tines have
     * nothing, against the note's fundamental. */
    {
        double hi[2];
        const int kal = quilt_instrument_by_name("Kalimba / Music Box");
        for (int z = 0; z < 2; z++) {
            void *p = note("Kalimba / Music Box", 60, 100, 8192 + 2205, -1, z ? "volume=0;c_kalimba_music_box=1;m_noise=0"
                                                                           : "volume=0;c_kalimba_music_box=0;m_noise=0");
            double e = 0;
            for (double f = 6000; f < 12000; f += 100) e += pow(mag(2205, 8192, f), 2);
            hi[z] = 10 * log10(e + 1e-30) - 20 * log10(mag(2205, 8192, quilt_note_freq(kal, 60)));
            A->destroy_instance(p);
        }
        CHECK(hi[1] > hi[0] + 10 && hi[1] > -30, "BUZZ: the buzzers' sizzle comes up at least 10 dB, to -30 dB or more "
              "(%.1f -> %.1f dB)", hi[0], hi[1]);
    }

    d = turn("Vibraphone", 60, 100, 44100 * 3, -1, "volume=0;sway=1;decay=1;c_vibraphone=0", "c_vibraphone", m_wobble, &a0, &a1);
    CHECK(a1 > 6 && a0 < 1.5, "MOTOR: the fan wobbles the note at least 6 dB (%.1f -> %.1f dB)", a0, a1);
    d = turn("Felt Upright", 60, 100, 44100 * 3, -1, "volume=0;decay=1;m_split=0;speed=0.4", "sway", m_wobble, &a0, &a1);
    CHECK(a1 > 6 && a0 < 1.5, "SWAY: the tremolo wobbles the note at least 6 dB (%.1f -> %.1f dB)", a0, a1);
    d = turn("Vibraphone", 60, 100, 44100 * 3, -1, "volume=0;decay=1;sway=0.6;c_vibraphone=0.5;m_split=0", "sway", m_wobble, &a0, &a1);
    CHECK(a1 > 6 && a0 < 1.5, "SWAY: on the vibraphone it is the fan's depth (%.1f -> %.1f dB)", a0, a1);

    /* DRIVE: the vibraphone's bars have no 3 f0, so all of it is the drive's. */
    cur_f0 = quilt_note_freq(quilt_instrument_by_name("Vibraphone"), 60);
    cur_ratio = 3;
    d = turn("Vibraphone", 60, 110, 8192 + 441, -1, "volume=0;soft=0.9", "drive", m_partial, &a0, &a1);
    CHECK(d > 15, "DRIVE: adds at least 15 dB of harmonics (%.1f -> %.1f dB)", a0, a1);
}

/* ---- the organs, the string machine and the FM piano: every knob heard ---- */

static double m_onset(void) { return db(rms_at(441, 1323)) - db(rms_at(17640, 4410)); }
static double m_late(void) { return part_db(26460, 8192, cur_f0 * cur_ratio); }
static double m_leak(void) { return part_db(2000, 16384, cur_f0 * cur_ratio) - part_db(2000, 16384, cur_f0); }
static double m_abs(void) { return part_db(441, 8192, cur_f0 * cur_ratio); }
static double m_line(void) { return db(mag(22050, 44100, cur_f0 * cur_ratio)) - db(rms_at(22050, 44100)); }

/* The rate of the strongest swing in a note's level, 0.3 to 12 Hz. */
static double rate(int a, int seconds) {
    int frames = seconds * 200;
    static double env[2400];
    double mean = 0;
    for (int i = 0; i < frames; i++) { env[i] = log(rms_at(a + i * 220, 220) + 1e-9); mean += env[i]; }
    mean /= frames;
    double best = 0, bm = -1;
    for (double f = 0.3; f <= 12; f += 0.05) {
        double re = 0, im = 0;
        for (int i = 0; i < frames; i++) { re += (env[i] - mean) * cos(2 * PI_D * f * i / 200); im += (env[i] - mean) * sin(2 * PI_D * f * i / 200); }
        if (re * re + im * im > bm) { bm = re * re + im * im; best = f; }
    }
    return best;
}
static double m_rate(void) { return rate(44100, 4); }

/* What `knob` adds in the first 50 ms, against the note: as NOISE is measured. */
static double burst(const char *type, const char *extra, const char *knob) {
    static float quiet[2205];
    char e[200];
    snprintf(e, sizeof(e), "%s;%s=0", extra, knob);
    void *p = note(type, 60, 90, 2304, -1, e);
    memcpy(quiet, wave, sizeof(quiet));
    double sig = rms_at(0, 2205);
    A->destroy_instance(p);
    snprintf(e, sizeof(e), "%s;%s=1", extra, knob);
    p = note(type, 60, 90, 2304, -1, e);
    double acc = 0;
    for (int k = 0; k < 2205; k++) acc += (wave[k] - quiet[k]) * (wave[k] - quiet[k]);
    A->destroy_instance(p);
    return db(sqrt(acc / 2205)) - db(sig);
}

/* One harmonic of a held note, with a drawbar in and out. */
static double drawbar(const char *key, double harm) {
    double at[2];
    for (int e = 0; e < 2; e++) {
        char x[200];
        snprintf(x, sizeof(x), "volume=0;o_8=0;o_4=0;c_tonewheel_organ=0;o_leak=0;%s=%d", key, e ? 8 : 0);
        void *p = note("Tonewheel Organ", 60, 100, 8192 + 441, -1, x);
        at[e] = part_db(441, 8192, 261.6 * harm);
        A->destroy_instance(p);
    }
    return at[1] - at[0];
}

static void machines(void) {
    double a0, a1, d;
    const double f0 = 261.63;
    cur_f0 = f0;

    /* Tonewheel Organ, 00 8800 000 unless said. */
    {
        const char *t = "Tonewheel Organ", *base = "volume=0;c_tonewheel_organ=0;o_leak=0";
        char x[200];
        cur_ratio = 2;
        d = turn(t, 60, 100, 8192 + 441, -1, base, "soft", m_partial, &a0, &a1);
        CHECK(d < -12, "%s SOFT: the 4' falls at least 12 dB against the 8' (%.1f -> %.1f dB)", t, a0, a1);
        d = turn(t, 60, 100, 22050, -1, base, "soft", m_level, &a0, &a1);
        CHECK(fabs(d) < 10, "%s SOFT: changes the tone more than the level (%+.1f dB)", t, d);
        d = turn(t, 60, 100, 44100, 22050, base, "decay", m_release, &a0, &a1);
        CHECK(d > 20, "%s DECAY: a released note sounds at least 20 dB longer (%.1f -> %.1f dB)", t, a0, a1);
        d = turn(t, 60, 100, 8192 + 441, -1, base, "tone", m_partial, &a0, &a1);
        CHECK(d > 4, "%s TONE: tilts at least 4 dB per octave (%.1f -> %.1f dB)", t, a0, a1);
        d = turn(t, 60, 100, 44100 * 5, -1, base, "sway", m_rate, &a0, &a1);
        CHECK(a0 < 2 && a1 > 5, "%s SWAY: the Leslie from slow to fast (%.1f -> %.1f Hz)", t, a0, a1);
        snprintf(x, sizeof(x), "volume=0;o_leak=0;sway=0;o_slow=1");
        d = turn(t, 60, 100, 44100 * 3, -1, x, "c_tonewheel_organ", m_wobble, &a0, &a1);
        CHECK(a1 > a0 + 3, "%s LUSH: the scanner chorus moves the note (%.1f -> %.1f dB)", t, a0, a1);
        cur_ratio = 3;
        d = turn(t, 60, 100, 8192 + 441, -1, base, "o_ping", m_partial, &a0, &a1);
        CHECK(d > 12, "%s PING: the percussion's third harmonic at least 12 dB up (%.1f -> %.1f dB)", t, a0, a1);
        d = turn(t, 60, 100, 44100, -1, "volume=0;c_tonewheel_organ=0;o_leak=0;o_ping=1", "o_tail", m_late, &a0, &a1);
        CHECK(d > 12, "%s TAIL: the percussion rings at least 12 dB longer (%.1f -> %.1f dB at 0.6 s)", t, a0, a1);
        d = burst(t, base, "o_click");
        CHECK(d > -14, "%s CLICK: the key click is heard, %.1f dB against the note", t, d);
        cur_ratio = pow(2, 1 / 12.0);
        d = turn(t, 60, 100, 16384 + 2048, -1, "volume=0;c_tonewheel_organ=0;o_4=0", "o_leak", m_leak, &a0, &a1);
        CHECK(d > 12, "%s LEAK: the next wheel bleeds in at least 12 dB more (%.1f -> %.1f dB)", t, a0, a1);
        d = turn(t, 60, 100, 44100 * 5, -1, "volume=0;c_tonewheel_organ=0;o_leak=0;sway=0", "o_slow", m_rate, &a0, &a1);
        CHECK(a1 > a0 * 2, "%s SLOW: the slow rotor speed at least doubles (%.2f -> %.2f Hz)", t, a0, a1);
        d = turn(t, 60, 100, 44100 * 5, -1, "volume=0;c_tonewheel_organ=0;o_leak=0;sway=1", "o_fast", m_rate, &a0, &a1);
        CHECK(a1 > a0 * 1.4, "%s FAST: the fast rotor speed at least 40 %% up (%.2f -> %.2f Hz)", t, a0, a1);
        static const char *bars[9] = { "o_16", "o_513", "o_8", "o_4", "o_223", "o_2", "o_135", "o_113", "o_1" };
        static const double harm[9] = { 0.5, 1.5, 1, 2, 3, 4, 5, 6, 8 };
        for (int b = 0; b < 9; b++) {
            d = drawbar(bars[b], harm[b]);
            CHECK(d > 12, "%s %s: its harmonic at least 12 dB up (%+.1f dB)", t, bars[b], d);
        }
    }

    /* Flute Organ. */
    {
        const char *t = "Flute Organ", *base = "volume=0;c_flute_organ=0;k_4=0";
        cur_ratio = 3;
        d = turn(t, 60, 100, 8192 + 441, -1, base, "soft", m_partial, &a0, &a1);
        CHECK(d < -12, "%s SOFT: the stopped pipe's twelfth falls at least 12 dB (%.1f -> %.1f dB)", t, a0, a1);
        d = turn(t, 60, 100, 22050, -1, base, "soft", m_level, &a0, &a1);
        CHECK(fabs(d) < 10, "%s SOFT: changes the tone more than the level (%+.1f dB)", t, d);
        d = turn(t, 60, 100, 44100, 22050, base, "decay", m_release, &a0, &a1);
        CHECK(d > 20, "%s DECAY: a released note sounds at least 20 dB longer (%.1f -> %.1f dB)", t, a0, a1);
        cur_ratio = 2;
        d = turn(t, 60, 100, 8192 + 441, -1, base, "tone", m_partial, &a0, &a1);
        CHECK(d > 4, "%s TONE: tilts at least 4 dB per octave (%.1f -> %.1f dB)", t, a0, a1);
        d = turn(t, 60, 100, 44100 * 3, -1, base, "sway", m_wobble, &a0, &a1);
        CHECK(a1 > 6 && a0 < 1.5, "%s SWAY: the tremulant wobbles the note (%.1f -> %.1f dB)", t, a0, a1);
        d = burst(t, "volume=0;k_4=0", "c_flute_organ");
        CHECK(d > -14, "%s PUFF: the chiff is heard, %.1f dB against the note", t, d);
        d = turn(t, 60, 100, 8192 + 441, -1, base, "k_edge", m_partial, &a0, &a1);
        CHECK(d > 12, "%s EDGE: opens the stopped pipe, its octave at least 12 dB up (%.1f -> %.1f dB)", t, a0, a1);
        d = turn(t, 60, 100, 22050, -1, base, "k_swell", m_onset, &a0, &a1);
        CHECK(d < -12, "%s SWELL: the pipe speaks at least 12 dB more slowly (%.1f -> %.1f dB at 30 ms)", t, a0, a1);
    }

    /* String Ensemble. */
    {
        const char *t = "String Ensemble", *base = "volume=0;c_string_ensemble=0";
        cur_ratio = 4;
        d = turn(t, 60, 100, 8192 + 441, -1, base, "soft", m_partial, &a0, &a1);
        CHECK(d < -12, "%s SOFT: the overtones fall at least 12 dB (%.1f -> %.1f dB)", t, a0, a1);
        d = turn(t, 60, 100, 22050, -1, base, "soft", m_level, &a0, &a1);
        CHECK(fabs(d) < 10, "%s SOFT: changes the tone more than the level (%+.1f dB)", t, d);
        d = turn(t, 60, 100, 44100, 22050, base, "decay", m_release, &a0, &a1);
        CHECK(d > 20, "%s DECAY: a released note sounds at least 20 dB longer (%.1f -> %.1f dB)", t, a0, a1);
        d = turn(t, 60, 100, 8192 + 441, -1, base, "tone", m_partial, &a0, &a1);
        CHECK(d / 2 > 4, "%s TONE: tilts at least 4 dB per octave (%.1f -> %.1f dB over 2 octaves)", t, a0, a1);
        cur_ratio = 4;
        d = turn(t, 60, 100, 44100 * 2, -1, base, "sway", m_line, &a0, &a1);
        CHECK(d < -6, "%s SWAY: the vibrato spreads the fourth harmonic at least 6 dB (%.1f -> %.1f dB)", t, a0, a1);
        d = turn(t, 60, 100, 44100 * 3, -1, "volume=0", "c_string_ensemble", m_wobble, &a0, &a1);
        CHECK(a1 > a0 + 3, "%s LUSH: the ensemble moves the note (%.1f -> %.1f dB)", t, a0, a1);
        cur_ratio = 8;
        d = turn(t, 60, 100, 8192 + 441, -1, base, "k_edge", m_partial, &a0, &a1);
        CHECK(d > 6, "%s EDGE: the presence at 2 kHz at least 6 dB up (%.1f -> %.1f dB)", t, a0, a1);
        d = turn(t, 60, 100, 22050, -1, base, "k_swell", m_onset, &a0, &a1);
        CHECK(d < -12, "%s SWELL: the strings swell in at least 12 dB more slowly (%.1f -> %.1f dB at 30 ms)", t, a0, a1);
    }

    /* The registers, each with the others out: its own fundamental comes in. */
    {
        const char *mt[2] = { "Flute Organ", "String Ensemble" };
        const char *rk[3] = { "k_8", "k_4", "k_2" };
        for (int m = 0; m < 2; m++)
            for (int r = 0; r < 3; r++) {
                char x[200];
                snprintf(x, sizeof(x), "volume=0;k_8=0;k_4=0;k_2=0;c_flute_organ=0;c_string_ensemble=0;k_swell=0");
                cur_ratio = 1 << r;
                d = turn(mt[m], 60, 100, 8192 + 441, -1, x, rk[r], m_abs, &a0, &a1);
                CHECK(d > 12, "%s %s: its register comes in at least 12 dB (%.1f -> %.1f dB)", mt[m], rk[r], a0, a1);
            }
    }

    /* Glass E.Piano. */
    {
        const char *t = "Glass E.Piano", *base = "volume=0";
        cur_ratio = 2;
        d = turn(t, 60, 100, 8192 + 441, -1, base, "soft", m_partial, &a0, &a1);
        CHECK(d < -12, "%s SOFT: the overtones fall at least 12 dB (%.1f -> %.1f dB)", t, a0, a1);
        d = turn(t, 60, 100, 22050, -1, base, "soft", m_level, &a0, &a1);
        CHECK(fabs(d) < 10, "%s SOFT: changes the tone more than the level (%+.1f dB)", t, d);
        d = turn(t, 60, 90, 44100 * 2, -1, base, "decay", m_tail, &a0, &a1);
        CHECK(d > 12, "%s DECAY: rings at least 12 dB longer (%.1f -> %.1f dB at 1.5 s)", t, a0, a1);
        d = turn(t, 60, 90, 8192 + 441, -1, base, "tone", m_partial, &a0, &a1);
        CHECK(d > 4, "%s TONE: tilts at least 4 dB per octave (%.1f -> %.1f dB)", t, a0, a1);
        cur_ratio = 4;
        d = turn(t, 60, 100, 8192 + 441, -1, base, "c_glass_e_piano", m_partial, &a0, &a1);
        CHECK(d > 12, "%s GLASS: the bell pair's sideband at least 12 dB up (%.1f -> %.1f dB)", t, a0, a1);
        cur_ratio = 13;
        d = turn(t, 60, 110, 8192 + 441, -1, base, "f_tine", m_partial, &a0, &a1);
        CHECK(d > 12, "%s TINE: the tink at least 12 dB up (%.1f -> %.1f dB)", t, a0, a1);
        d = turn(t, 60, 90, 44100 * 3, -1, "volume=0;decay=1", "f_split", m_wobble, &a0, &a1);
        CHECK(a1 > 3 && a1 > a0 + 2, "%s SPLIT: the pairs beat (wobble %.1f -> %.1f dB)", t, a0, a1);
        /* TUNE: a whole-number ratio rings as a bell, one between as glass. */
        double at[2];
        for (int e = 0; e < 2; e++) {
            void *p = note(t, 60, 100, 8192 + 441, -1, e ? "volume=0;c_glass_e_piano=1;f_tune=0.375" : "volume=0;c_glass_e_piano=1;f_tune=0");
            at[e] = part_db(441, 8192, f0 * 1.5) - part_db(441, 8192, f0);
            A->destroy_instance(p);
        }
        CHECK(at[1] > at[0] + 12, "%s TUNE: off the whole numbers the bell goes inharmonic, 1.5 f0 at least 12 dB up "
              "(%.1f -> %.1f dB)", t, at[0], at[1]);
    }
}

/* ---- the plucked strings: in tune, and every knob heard ---- */

static double m_edge(void) { return part_db(11025, 8192, cur_f0 * cur_ratio) - part_db(11025, 8192, cur_f0); }
static double m_pitch(void) { return 1200 * log2(peak_near(22050, 16384, cur_f0, cur_f0 * 0.03) / cur_f0); }

/* A held note with the pad pressed from 0.2 s; the pitch from 0.5 s. */
static double pressed(const char *type, int key, const char *extra) {
    void *p = note(type, key, 100, 4096, -1, extra);
    midi3(p, 0xA0, key, 127);
    int16_t out[256];
    for (int f = 0; f < 44100; f += 128) {
        A->render_block(p, out, 128);
        for (int i = 0; i < 128 && f + i < 44100; i++) wave[f + i] = out[2 * i] / 32768.0f;
    }
    A->destroy_instance(p);
    return 1200 * log2(peak_near(13230, 16384, cur_f0, cur_f0 * 0.06) / cur_f0);
}

static void plucked(void) {
    static const char *const pl[4] = { "Harp", "Nylon Guitar", "Pizzicato", "Clavichord" };
    double a0, a1, d;
    for (int i = 0; i < 4; i++) {
        const char *t = pl[i];
        const int inst = quilt_instrument_by_name(t);
        /* In tune across the keys, at the shared defaults (a little stiffness). */
        double worst = 0;
        for (int key = 48; key <= 84; key += 12) {
            cur_f0 = waveguide_freq(inst, key);
            void *p = note(t, key, 90, 22050 + 16384, -1, "volume=0;decay=1");
            double c = m_pitch();
            if (fabs(c) > fabs(worst)) worst = c;
            A->destroy_instance(p);
        }
        CHECK(fabs(worst) < 3, "%s: in tune from C3 to C6 (worst %+.1f cents)", t, worst);

        cur_f0 = waveguide_freq(inst, 60);
        cur_ratio = 4;
        d = turn(t, 60, 90, 8192 + 441, -1, "volume=0", "soft", m_partial, &a0, &a1);
        CHECK(d < -12, "%s SOFT: the overtones fall at least 12 dB (%.1f -> %.1f dB)", t, a0, a1);
        d = turn(t, 60, 90, 22050, -1, "volume=0", "soft", m_level, &a0, &a1);
        CHECK(fabs(d) < 10, "%s SOFT: changes the tone more than the level (%+.1f dB)", t, d);
        d = turn(t, 60, 90, 44100 * 2, -1, "volume=0", "decay", m_tail, &a0, &a1);
        CHECK(d > 12, "%s DECAY: rings at least 12 dB longer (%.1f -> %.1f dB at 1.5 s)", t, a0, a1);
        d = turn(t, 60, 100, 44100, 22050, "volume=0;decay=1;c_harp=0;c_nylon_guitar=0", "p_damp", m_release, &a0, &a1);
        CHECK(d < -20, "%s DAMP: stops a released note at least 20 dB sooner (%.1f -> %.1f dB)", t, a0, a1);
        d = turn(t, 60, 90, 8192 + 441, -1, "volume=0", "tone", m_partial, &a0, &a1);
        CHECK(d / 2 > 4, "%s TONE: tilts at least 4 dB per octave (%.1f -> %.1f dB over 2 octaves)", t, a0, a1);
        d = turn(t, 60, 90, 22050, -1, "volume=0", "p_body", m_level, &a0, &a1);
        CHECK(d > 3, "%s BODY: at least 3 dB more body (%+.1f dB)", t, d);
        d = turn(t, 60, 90, 44100, -1, "volume=0", "p_edge", m_edge, &a0, &a1);
        CHECK(d > 12, "%s EDGE: the overtones ring on, at least 12 dB up at 0.25 s (%.1f -> %.1f dB)", t, a0, a1);
        cur_ratio = 2;
        d = turn(t, 60, 90, 8192 + 441, -1, "volume=0", "p_spot", m_partial, &a0, &a1);
        CHECK(d < -15, "%s SPOT: the middle of the string hollows the second partial (%.1f -> %.1f dB)", t, a0, a1);
        cur_f0 = waveguide_freq(inst, 48);
        cur_ratio = 4;
        d = turn(t, 48, 100, 16384 + 1024, -1, "volume=0;soft=0.2;decay=1;c_harp=0;c_nylon_guitar=0",
                 "p_stiff", m_stretch, &a0, &a1);
        CHECK(d > 20, "%s STIFF: stretches the fourth partial at least 20 cents (%.0f -> %.0f cents)", t, a0, a1);
        d = burst(t, "volume=0", "p_noise");
        CHECK(d > -14, "%s NOISE: the finger is heard, %.1f dB against the note", t, d);
    }

    /* BLOOM: the open strings take up the note and ring on. */
    const char *bl[2] = { "Harp", "Nylon Guitar" }, *bk[2] = { "c_harp", "c_nylon_guitar" };
    for (int i = 0; i < 2; i++) {
        d = turn(bl[i], 57, 90, 44100 * 2, -1, "volume=0", bk[i], m_tail, &a0, &a1);
        CHECK(d > 6, "%s BLOOM: rings at least 6 dB more at 1.5 s (%.1f -> %.1f dB)", bl[i], a0, a1);
    }
    /* DEEP: from the viola's body to the cello's, so the low end comes up. */
    cur_f0 = waveguide_freq(quilt_instrument_by_name("Pizzicato"), 48);
    cur_ratio = 3;
    d = turn("Pizzicato", 48, 90, 8192 + 441, -1, "volume=0;p_body=1", "c_pizzicato", m_partial, &a0, &a1);
    CHECK(d < -8, "Pizzicato DEEP: the third partial falls at least 8 dB against the fundamental (%.1f -> %.1f dB)",
          a0, a1);
    /* BEND: pressing the pad sharpens the note, and only with BEND up. */
    cur_f0 = waveguide_freq(quilt_instrument_by_name("Clavichord"), 60);
    a0 = pressed("Clavichord", 60, "volume=0;c_clavichord=0");
    a1 = pressed("Clavichord", 60, "volume=0;c_clavichord=1");
    CHECK(fabs(a0) < 2 && a1 > 20, "Clavichord BEND: full pressure bends the note up at least 20 cents "
          "(%+.1f -> %+.1f cents)", a0, a1);
}

/* ---- the bowed strings ---- */

/* How alike one period is to the next, 0.1 to 0.2 s in: 1 is Helmholtz motion. */
static double periodic(void) {
    int lag = (int)(QUILT_SR / cur_f0 + 0.5);
    double s = 0, e0 = 0, e1 = 0;
    for (int i = 4410; i < 8820; i++) {
        s += wave[i] * wave[i + lag]; e0 += wave[i] * wave[i]; e1 += wave[i + lag] * wave[i + lag];
    }
    return s / sqrt(e0 * e1 + 1e-30);
}
/* Harmonics 5 to 12 together, against the fundamental, in the held note:
 * as bands, so a section's players a few cents apart are all counted. */
static double band(double lo, double hi) {
    double e = 0;
    for (double f = lo; f < hi; f += 5) e += pow(mag(22050, 8192, f), 2);
    return e;
}
static double m_upper(void) {
    return 10 * log10(band(cur_f0 * 4.5, cur_f0 * 12.5) / band(cur_f0 * 0.5, cur_f0 * 1.5) + 1e-30);
}
static double m_held(void) { return db(rms_at(22050, 22050)); }
/* The held note's spectral centroid, 50 Hz to 8 kHz: how bright it is. */
static double m_bright(void) {
    double num = 0, den = 0;
    for (double f = 50; f < 8000; f += 25) { double m = mag(22050, 4096, f); num += m * f; den += m; }
    return num / den;
}
/* What the knob adds over the held note, 0.25 to 0.75 s, against it. */
static double hiss(const char *type, const char *extra, const char *knob) {
    static float quiet[22050];
    char e[200];
    snprintf(e, sizeof(e), "%s;%s=0", extra, knob);
    void *p = note(type, 60, 90, 33075, -1, e);
    memcpy(quiet, wave + 11025, sizeof(quiet));
    double sig = rms_at(11025, 22050);
    A->destroy_instance(p);
    snprintf(e, sizeof(e), "%s;%s=1", extra, knob);
    p = note(type, 60, 90, 33075, -1, e);
    double acc = 0;
    for (int k = 0; k < 22050; k++) acc += (wave[11025 + k] - quiet[k]) * (wave[11025 + k] - quiet[k]);
    A->destroy_instance(p);
    return db(sqrt(acc / 22050)) - db(sig);
}
/* A held note with the pad at `press` from the start; its level from 0.5 s. */
static double bowed_at(const char *type, int press, const char *extra) {
    void *p = note(type, 60, 90, 256, -1, extra);
    midi3(p, 0xA0, 60, press);
    int16_t out[256];
    for (int f = 0; f < 44100; f += 128) {
        A->render_block(p, out, 128);
        for (int i = 0; i < 128 && f + i < 44100; i++) wave[f + i] = out[2 * i] / 32768.0f;
    }
    A->destroy_instance(p);
    return db(rms_at(22050, 22050));
}

/* A bowed string has two steady motions, one slip a period (Helmholtz) and
 * two (the note an octave up), and the bow must hold the first. Every key,
 * SOFT, bow place and touch: by 1 s, never more alike after half a period
 * than after a whole one, less 0.1. */
static void octaves(void) {
    static const char *const bw[3] = { "Solo Cello", "Solo Violin", "String Section" };
    static const char *const ck[3] = { "c_solo_cello", "c_solo_violin", "c_string_section" };
    int bad = 0, n = 0;
    char what[120] = "";
    for (int i = 0; i < 3; i++)
        for (int key = 36; key <= 96; key += 6)
            for (int sf = 0; sf <= 2; sf++)
                for (int c = 0; c <= 2; c++)
                    for (int vel = 30; vel <= 127; vel += 97) {
                        char x[120];
                        snprintf(x, sizeof(x), "volume=0;soft=%g;%s=%g", sf * 0.5, ck[i], c * 0.5);
                        cur_f0 = bowed_freq(quilt_instrument_by_name(bw[i]), key);
                        void *p = note(bw[i], key, vel, 44100 + 4410 + 2048, -1, x);
                        double r1 = 0, r2 = 0, e0 = 0, e1 = 0, e2 = 0;
                        int l1 = (int)(QUILT_SR / cur_f0 + 0.5), l2 = (int)(QUILT_SR / cur_f0 / 2 + 0.5);
                        for (int k = 44100; k < 44100 + 4410; k++) {
                            r1 += wave[k] * wave[k + l1]; r2 += wave[k] * wave[k + l2];
                            e0 += wave[k] * wave[k]; e1 += wave[k + l1] * wave[k + l1]; e2 += wave[k + l2] * wave[k + l2];
                        }
                        r1 /= sqrt(e0 * e1 + 1e-30);
                        r2 /= sqrt(e0 * e2 + 1e-30);
                        n++;
                        if (r2 > r1 - 0.1 && !bad++)
                            snprintf(what, sizeof(what), "%s key %d soft %.1f char %.1f vel %d", bw[i], key, sf * 0.5, c * 0.5, vel);
                        A->destroy_instance(p);
                    }
    CHECK(!bad, "the bow holds one slip a period on all %d settings, never the octave (%d bad; first %s)", n, bad, what);
}

/* How alike the two channels of a held note are, 0.5 to 1.5 s: 1 is a
 * point in the middle, lower is wider. */
static double stereo(const char *type, const char *extra) {
    void *p = note(type, 60, 90, 256, -1, extra);
    int16_t out[256];
    double lr = 0, ll = 0, rr = 0;
    for (int f = 0; f < 66150; f += 128) {
        A->render_block(p, out, 128);
        if (f < 22050) continue;
        for (int i = 0; i < 128; i++) {
            double l = out[2 * i], r = out[2 * i + 1];
            lr += l * r; ll += l * l; rr += r * r;
        }
    }
    A->destroy_instance(p);
    return lr / sqrt(ll * rr + 1e-30);
}

static void bowed(void) {
    static const char *const bw[3] = { "Solo Cello", "Solo Violin", "String Section" };
    static const char *const ck[3] = { "c_solo_cello", "c_solo_violin", "c_string_section" };
    double a0, a1, d;
    for (int i = 0; i < 3; i++) {
        const char *t = bw[i];
        const int inst = quilt_instrument_by_name(t);
        double worst = 0, still = 1;
        for (int key = 48; key <= 84; key += 12) {
            cur_f0 = bowed_freq(inst, key);
            void *p = note(t, key, 90, 22050 + 16384, -1, "volume=0;c_string_section=0");
            double c = m_pitch();
            if (fabs(c) > fabs(worst)) worst = c;
            if (periodic() < still) still = periodic();
            A->destroy_instance(p);
        }
        /* A bowed string's pitch moves a few cents with the bow's force, as
         * a real one's does (the player's ear takes it up), so 5, not 3. */
        CHECK(fabs(worst) < 5, "%s: in tune from C3 to C6 (worst %+.1f cents)", t, worst);
        CHECK(still > 0.95, "%s: the bow settles into Helmholtz motion within 0.1 s (periodicity %.3f)", t, still);

        cur_f0 = bowed_freq(inst, 60);
        cur_ratio = 4;
        d = turn(t, 60, 90, 22050 + 8192, -1, "volume=0;c_string_section=0", "soft", m_upper, &a0, &a1);
        CHECK(d < -12, "%s SOFT: the overtones from the fifth up fall at least 12 dB (%.1f -> %.1f dB)", t, a0, a1);
        d = turn(t, 60, 90, 44100, -1, "volume=0", "soft", m_held, &a0, &a1);
        CHECK(fabs(d) < 10, "%s SOFT: changes the tone more than the level (%+.1f dB)", t, d);
        d = turn(t, 60, 90, 44100, 22050, "volume=0", "decay", m_release, &a0, &a1);
        CHECK(d > 20, "%s DECAY: a released note sounds at least 20 dB longer (%.1f -> %.1f dB)", t, a0, a1);
        {
            double h[2];
            for (int e = 0; e < 2; e++) {
                void *p = note(t, 60, 90, 22050 + 8192, -1, e ? "volume=0;tone=1" : "volume=0;tone=0");
                h[e] = part_db(22050, 8192, cur_f0 * 4) - part_db(22050, 8192, cur_f0);
                A->destroy_instance(p);
            }
            CHECK((h[1] - h[0]) / 2 > 4, "%s TONE: tilts at least 4 dB per octave (%.1f -> %.1f dB over 2 octaves)", t, h[0], h[1]);
            for (int e = 0; e < 2; e++) {
                void *p = note(t, 60, 90, 22050 + 8192, -1, e ? "volume=0;c_string_section=0;b_edge=1"
                                                               : "volume=0;c_string_section=0;b_edge=0");
                h[e] = m_upper();
                A->destroy_instance(p);
            }
            CHECK(h[1] - h[0] > 6, "%s EDGE: the upper harmonics at least 6 dB up (%.1f -> %.1f dB)", t, h[0], h[1]);
        }
        d = turn(t, 60, 90, 44100, -1, "volume=0", "b_body", m_held, &a0, &a1);
        CHECK(d > 3, "%s BODY: at least 3 dB more body (%+.1f dB)", t, d);
        d = turn(t, 60, 90, 22050, -1, "volume=0;b_bite=0", "b_swell", m_onset, &a0, &a1);
        CHECK(d < -12, "%s SWELL: the bow swells in at least 12 dB more slowly (%.1f -> %.1f dB at 30 ms)", t, a0, a1);
        d = hiss(t, "volume=0", "b_noise");
        CHECK(d > -20, "%s NOISE: the bow's hair is heard, %.1f dB against the note", t, d);
        d = burst(t, "volume=0", "b_bite");
        CHECK(d > -14, "%s BITE: the bow's first grip is heard, %.1f dB against the note", t, d);
        a0 = bowed_at(t, 15, "volume=0;b_press=0");
        a1 = bowed_at(t, 127, "volume=0;b_press=0");
        CHECK(a1 - a0 > 10, "%s PRESS Pad: the pad is the bow, at least 10 dB from light to full (%.1f -> %.1f dB)", t, a0, a1);
        a0 = bowed_at(t, 15, "volume=0;b_press=1");
        a1 = bowed_at(t, 127, "volume=0;b_press=1");
        CHECK(fabs(a1 - a0) < 1, "%s PRESS Auto: the pad changes nothing (%.1f -> %.1f dB)", t, a0, a1);
        d = turn(t, 60, 100, 44100 * 2, -1, "volume=0;c_string_section=0", "sway", m_line, &a0, &a1);
        CHECK(d < -6, "%s SWAY: the vibrato spreads the fourth harmonic at least 6 dB (%.1f -> %.1f dB)", t, a0, a1);
        if (i < 2) {
            turn(t, 60, 90, 22050 + 8192, -1, "volume=0", ck[i], m_bright, &a0, &a1);
            CHECK(a1 < a0 * 0.75, "%s VEIL: toward the fingerboard the note darkens by at least a quarter "
                  "(centroid %.0f -> %.0f Hz)", t, a0, a1);
        } else {
            a0 = stereo(t, "volume=0;c_string_section=0");
            a1 = stereo(t, "volume=0;c_string_section=1");
            CHECK(a1 < a0 - 0.2, "%s WIDTH: the players spread, the channels at least 0.2 less alike (%.2f -> %.2f)", t, a0, a1);
        }
    }
}

/* The reverb alone: the same note with and without the plate, subtracted.
 * Everything else is deterministic, so what is left is the plate. */
static float wet[44100 * 4];
static void plate_only(const char *type, const char *extra) {
    char e[200];
    snprintf(e, sizeof(e), "volume=0;space=0;%s", extra);
    void *p = note(type, 72, 110, 44100 * 4, 4410, e);
    memcpy(wet, wave, sizeof(wet));
    A->destroy_instance(p);
    snprintf(e, sizeof(e), "volume=0;space=1;%s", extra);
    p = note(type, 72, 110, 44100 * 4, 4410, e);
    for (int i = 0; i < 44100 * 4; i++) wet[i] = wave[i] - wet[i];
    A->destroy_instance(p);
}
static double wet_rms(int a, int n) {
    double acc = 0;
    for (int i = 0; i < n; i++) acc += (double)wet[a + i] * wet[a + i];
    return db(sqrt(acc / n));
}
static double wet_bright(int a, int n) {
    double d = 0, x = 0;
    for (int i = 1; i < n; i++) { double e = wet[a + i] - wet[a + i - 1]; d += e * e; x += (double)wet[a + i] * wet[a + i]; }
    return 10 * log10(d / (x + 1e-20));
}
static int wet_arrives(void) {
    double peak = 0;
    for (int i = 0; i < 44100; i++) if (fabs(wet[i]) > peak) peak = fabs(wet[i]);
    for (int i = 0; i < 44100; i++) if (fabs(wet[i]) > peak * 0.05) return i;
    return 44100;
}

static void effects(void) {
    plate_only("Marimba", "size=0");
    double small = wet_rms(44100 * 2, 4410);
    plate_only("Marimba", "size=1");
    double big = wet_rms(44100 * 2, 4410);
    CHECK(big > small + 20, "SIZE: the room rings at least 20 dB longer (%.1f -> %.1f dB at 2 s)", small, big);

    /* Heard on a bright source: the electric grand, hard, pickups full up. */
    plate_only("Electric Grand", "dark=0;size=0.7;soft=0;c_electric_grand=1");
    double open = wet_bright(44100, 22050);
    plate_only("Electric Grand", "dark=1;size=0.7;soft=0;c_electric_grand=1");
    double dark = wet_bright(44100, 22050);
    CHECK(dark < open - 6, "DARK: the tail at least 6 dB darker (%.1f -> %.1f dB)", open, dark);

    plate_only("Marimba", "delay=0");
    int soon = wet_arrives();
    plate_only("Marimba", "delay=1");
    int late = wet_arrives();
    CHECK(late - soon > 3528, "DELAY: the room answers at least 80 ms later (%.0f -> %.0f ms)",
          soon / 44.1, late / 44.1);
}

/* ---- no pops ----
 * A knob jumping end to end, and loud chords, must not step the signal. The
 * measure is the largest second difference against the note's own, without
 * the jumps. */
static double spikiness(int a, int n) {
    double worst = 0;
    for (int i = a + 2; i < a + n; i++) {
        double d = fabs(wave[i] - 2 * wave[i - 1] + wave[i - 2]);
        if (d > worst) worst = d;
    }
    return worst;
}

static void clicks(void) {
    const char *knobs[] = { "soft", "decay", "sway", "space", "volume", "speed", "size",
                            "dark", "delay", "tone", "drive" };
    for (int k = 0; k < 11; k++) {
        double worst[2];
        for (int jumps = 0; jumps < 2; jumps++) {
            void *p = A->create_instance(".", "");
            A->set_param(p, "type", "Vibraphone");
            midi3(p, 0x90, 60, 90);
            int16_t out[256];
            for (int b = 0; b < 300; b++) {
                if (jumps && b % 40 == 20) {
                    /* VOL tops out at 0 dB, the level the quiet run is measured at. */
                    const char *hi = !strcmp(knobs[k], "volume") ? "0" : "1";
                    const char *lo = !strcmp(knobs[k], "volume") ? "-30" : "0";
                    A->set_param(p, knobs[k], (b / 40) % 2 ? lo : hi);
                }
                A->render_block(p, out, 128);
                for (int i = 0; i < 128; i++) wave[b * 128 + i] = out[2 * i] / 32768.0f;
            }
            worst[jumps] = spikiness(4410, 300 * 128 - 4410);
            A->destroy_instance(p);
        }
        CHECK(worst[1] < worst[0] * 2.0, "%s: jumping end to end does not pop (%.4f vs %.4f)", knobs[k],
              worst[1], worst[0]);
    }

    /* Four loud notes at once at full volume. The limiter is straight up to
     * -6 dBFS and takes off 0.14 dB at -3 dBFS, so their attacks may reach
     * that far and no further. */
    for (int i = 0; i < QUILT_NTYPES; i++) {
        void *p = A->create_instance(".", "");
        A->set_param(p, "type", TYPES[i]);
        const int chord[4] = { 48, 55, 60, 64 };
        for (int n = 0; n < 4; n++) midi3(p, 0x90, chord[n], 110);
        int peak = 0;
        rms(p, 100, &peak);
        CHECK(peak < 23200, "%s: four loud notes peak at %.1f dBFS, where the limiter barely acts", TYPES[i],
              db(peak / 32768.0));
        A->destroy_instance(p);
    }
}

/* What one block costs here. The Move figure is measured in build step 3. */
static void cost(void) {
    void *p = A->create_instance(".", "");
    A->set_param(p, "type", "Felt Upright");
    A->set_param(p, "space", "0.5");
    for (int n = 0; n < QUILT_VOICES; n++) midi3(p, 0x90, 28 + n, 100);
    int16_t out[256];
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int b = 0; b < 2000; b++) A->render_block(p, out, 128);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double us = ((double)(t1.tv_sec - t0.tv_sec) * 1e9 + (double)(t1.tv_nsec - t0.tv_nsec)) / 2000 / 1e3;
    printf("cost: %.1f us per block, 16 low Felt Upright notes and the plate, %d oscillators "
           "(%.1f%% of a 2902 us block, on this machine)\n",
           us, quilt_modal_in_use((quilt_t *)p), us / 2902.0 * 100.0);
    A->destroy_instance(p);
}

/* Every key of every instrument, struck as hard as it goes with the hardest
 * felt, then the softest: each sounds, and none rings up toward full scale.
 * The felt's stiffness once outran the contact's explicit step on the
 * thinnest strings and stiffest bars and blew up (DESIGN.md, Contact). */
static void stable(void) {
    int bad = 0;
    char what[96] = "";
    for (int t = 0; t < QUILT_NTYPES; t++)
        for (int key = 21; key <= 108; key++)
            for (int s = 0; s < 2; s++) {
                void *p = A->create_instance(".", "");
                A->set_param(p, "type", TYPES[t]);
                A->set_param(p, "soft", s ? "1" : "0");
                A->set_param(p, "m_noise", "0");
                midi3(p, 0x90, key, 127);
                int peak = 0;
                double r = rms(p, 40, &peak);
                if (peak > 24000 || !(r > 0.0)) {
                    if (!bad++) snprintf(what, sizeof(what), "%s key %d soft %d: peak %d", TYPES[t], key, s, peak);
                }
                A->destroy_instance(p);
            }
    CHECK(!bad, "every key sounds and none rings up toward full scale (%d bad; first %s)", bad, what);
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : ".";
    A = move_plugin_init_v2(NULL);
    CHECK(A && A->api_version == 2, "v2 api");
    void *p = A->create_instance(".", "");
    CHECK(p != NULL, "create");

    contracts(p, dir);
    types(p);
    presets(p);
    voicings(p);
    state(p);
    sound();
    pedal();
    contact();
    strings();
    bars();
    softness();
    double_decay();
    dampers();
    motor();
    roll();
    budget();
    stable();
    reverb();
    audible();
    machines();
    plucked();
    bowed();
    octaves();
    effects();
    clicks();
    cost();
    A->destroy_instance(p);

    printf("%s: %d checks, %d failed\n", fails ? "FAIL" : "ok", checks, fails);
    return fails ? 1 : 0;
}
