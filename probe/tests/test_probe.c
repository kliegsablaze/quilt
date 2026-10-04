/* Black-box checks of the probe through the v2 API, and a dump of its two
 * contracts for tests/plan.test.mjs to plan with the host's own planner. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host/plugin_api_v1.h"

plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);

static int fails = 0, checks = 0;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; \
    printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static char buf[131072];

static const char *get(plugin_api_v2_t *a, void *p, const char *k) {
    int n = a->get_param(p, k, buf, sizeof(buf));
    return n < 0 ? NULL : buf;
}

static double rms(plugin_api_v2_t *a, void *p, int blocks) {
    int16_t out[256];
    double acc = 0;
    long n = 0;
    for (int b = 0; b < blocks; b++) {
        a->render_block(p, out, 128);
        for (int i = 0; i < 256; i++) { acc += (double)out[i] * out[i]; n++; }
    }
    return sqrt(acc / n);
}

static void dump(plugin_api_v2_t *a, void *p, const char *key, const char *path) {
    const char *s = get(a, p, key);
    FILE *f = fopen(path, "w");
    if (!f || !s) { printf("FAIL: cannot dump %s\n", key); fails++; if (f) fclose(f); return; }
    fputs(s, f);
    fclose(f);
}

int main(int argc, char **argv) {
    const char *outdir = argc > 1 ? argv[1] : ".";
    plugin_api_v2_t *a = move_plugin_init_v2(NULL);
    CHECK(a && a->api_version == 2, "v2 api");
    void *p = a->create_instance(".", "");
    CHECK(p != NULL, "create");

    /* type speaks names, accepts names and indices, rejects junk */
    CHECK(!strcmp(get(a, p, "type"), "Felt Upright"), "default type");
    a->set_param(p, "type", "Solo Cello");
    CHECK(!strcmp(get(a, p, "type"), "Solo Cello"), "type by name");
    a->set_param(p, "type", "2");
    CHECK(!strcmp(get(a, p, "type"), "Handpan"), "type by index");
    a->set_param(p, "type", "Banjo");
    CHECK(!strcmp(get(a, p, "type"), "Handpan"), "junk type ignored");

    /* a preset load moves type: the path the device check is about */
    CHECK(!strcmp(get(a, p, "preset_count"), "3"), "preset count");
    a->set_param(p, "preset", "0");
    CHECK(!strcmp(get(a, p, "type"), "Felt Upright"), "preset 0 -> piano");
    CHECK(!strcmp(get(a, p, "preset_name"), "Probe Piano"), "preset name");
    a->set_param(p, "preset", "1");
    CHECK(!strcmp(get(a, p, "type"), "Solo Cello"), "preset 1 -> cello");

    /* state round-trips */
    a->set_param(p, "c_tasto", "0.25");
    char state[1024];
    snprintf(state, sizeof(state), "%s", get(a, p, "state"));
    void *q = a->create_instance(".", "");
    a->set_param(q, "state", state);
    CHECK(!strcmp(get(a, q, "type"), "Solo Cello"), "state type");
    CHECK(fabs(atof(get(a, q, "c_tasto")) - 0.25) < 1e-3, "state c_tasto");
    a->destroy_instance(q);

    /* pressure: poly and channel aftertouch are counted and published */
    CHECK(!strcmp(get(a, p, "at_count"), "0"), "no pressure yet");
    uint8_t on[3] = { 0x90, 60, 100 }, at[3] = { 0xA0, 60, 90 }, cp[2] = { 0xD0, 30 };
    a->on_midi(p, on, 3, 0);
    a->on_midi(p, at, 3, 0);
    CHECK(!strcmp(get(a, p, "pressure"), "90"), "poly pressure value");
    a->on_midi(p, cp, 2, 0);
    CHECK(!strcmp(get(a, p, "pressure"), "30"), "channel pressure value");
    CHECK(!strcmp(get(a, p, "at_count"), "2"), "pressure count");
    a->set_param(p, "pressure", "5");
    CHECK(!strcmp(get(a, p, "pressure"), "30"), "readout ignores writes");

    /* the cello swells with pressure */
    rms(a, p, 40);
    uint8_t lo[3] = { 0xA0, 60, 10 }, hi[3] = { 0xA0, 60, 127 };
    a->on_midi(p, lo, 3, 0);
    rms(a, p, 20);
    double quiet = rms(a, p, 20);
    a->on_midi(p, hi, 3, 0);
    rms(a, p, 20);
    double loud = rms(a, p, 20);
    CHECK(loud > 3.0 * quiet && quiet > 0, "pressure swells the cello (%.0f -> %.0f)", quiet, loud);

    /* struck types sound, then decay to silence after release */
    a->set_param(p, "type", "Felt Upright");
    a->set_param(p, "decay", "0");
    uint8_t off[3] = { 0x80, 60, 0 }, on2[3] = { 0x90, 64, 100 }, off2[3] = { 0x80, 64, 0 };
    a->on_midi(p, off, 3, 0);
    a->on_midi(p, on2, 3, 0);
    CHECK(rms(a, p, 4) > 100, "piano sounds");
    a->on_midi(p, off2, 3, 0);
    rms(a, p, 400);
    CHECK(rms(a, p, 4) < 1, "piano decays to silence");

    /* contracts fit the host's 128 KB buffers, with room */
    int nh = a->get_param(p, "ui_hierarchy", buf, sizeof(buf));
    int nc = a->get_param(p, "chain_params", buf, sizeof(buf));
    CHECK(nh > 0 && nh < 16384 && nc > 0 && nc < 16384, "contract sizes %d / %d", nh, nc);
    char small[64];
    CHECK(a->get_param(p, "ui_hierarchy", small, sizeof(small)) == -1, "short buffer refused");

    char path[512];
    snprintf(path, sizeof(path), "%s/ui_hierarchy.json", outdir);
    dump(a, p, "ui_hierarchy", path);
    snprintf(path, sizeof(path), "%s/chain_params.json", outdir);
    dump(a, p, "chain_params", path);

    a->destroy_instance(p);
    printf("%s: %d checks, %d failed\n", fails ? "FAIL" : "ok", checks, fails);
    return fails ? 1 : 0;
}
