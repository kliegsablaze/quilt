/*
 * bench — what a block costs, instrument by instrument, at its worst: sixteen
 * notes held low, with the plate on. Runs on the Mac and on the Move
 * (scripts/bench.sh). DESIGN.md, Will it fit: the target is a quarter of the
 * 2902 us block. "onset us" is the worst block in which a chord of four
 * starts while twelve notes ring, stealing as it must: the whole block,
 * starting the notes and rendering, must fit in one block or audio drops.
 * Names given on the command line bench only those instruments.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "host/plugin_api_v1.h"
#include "quilt.h"

plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);

static double now_us(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e6 + (double)t.tv_nsec / 1e3;
}

int main(int argc, char **argv) {
    plugin_api_v2_t *a = move_plugin_init_v2(NULL);
    int16_t out[256];
    printf("%-18s %6s %8s %8s %7s %9s\n", "instrument", "osc", "mean us", "worst us", "block", "onset us");
    for (int i = -1; i < QUILT_NINST; i++) {
        if (i < 0) {
            /* The shared effects alone: no notes, the plate's tank kept running. */
            void *p = a->create_instance(".", "");
            a->set_param(p, "space", "0.5");
            a->set_param(p, "volume", "-60");
            uint8_t on[3] = { 0x90, 60, 100 };
            a->on_midi(p, on, 3, 0);
            for (int b = 0; b < 20; b++) a->render_block(p, out, 128);
            double total = 0;
            for (int b = 0; b < 3000; b++) {
                double t0 = now_us();
                a->render_block(p, out, 128);
                total += now_us() - t0;
            }
            printf("%-18s %6s %8.1f %8s %6.1f%%\n", "(effects alone)", "-", total / 3000, "",
                   total / 3000 / 2902.0 * 100.0);
            a->destroy_instance(p);
            continue;
        }
        if (i >= QUILT_NTYPES) break;
        if (argc > 1) {
            int want = 0;
            for (int k = 1; k < argc; k++) want |= !strcmp(argv[k], QUILT_TYPE_NAMES[i]);
            if (!want) continue;
        }
        void *p = a->create_instance(".", "");
        char idx[8];
        snprintf(idx, sizeof(idx), "%d", quilt_type_first_preset(i));
        a->set_param(p, "preset", idx);
        a->set_param(p, "space", "0.5");
        a->set_param(p, "m_pedal", "1");
        for (int n = 0; n < QUILT_VOICES; n++) {
            uint8_t on[3] = { 0x90, (uint8_t)(28 + n * 2), 100 };
            a->on_midi(p, on, 3, 0);
            a->render_block(p, out, 128);
        }
        double total = 0, worst = 0;
        const int blocks = 3000;     /* about 9 s, while the notes ring */
        for (int b = 0; b < blocks; b++) {
            double t0 = now_us();
            a->render_block(p, out, 128);
            double us = now_us() - t0;
            total += us;
            if (us > worst) worst = us;
        }
        const int osc = quilt_modal_in_use((quilt_t *)p);
        a->destroy_instance(p);

        p = a->create_instance(".", "");
        a->set_param(p, "preset", idx);
        a->set_param(p, "space", "0.5");
        double onset = 0;
        for (int r = 0; r < 40; r++) {
            for (int c = 0; c < 4; c++) {
                uint8_t on[3] = { 0x90, (uint8_t)(36 + (r * 4 + c) * 5 % 48), (uint8_t)(60 + (r * 13 + c * 7) % 67) };
                if (c == 0) {
                    double t0 = now_us();
                    for (int k = 0; k < 4; k++) {
                        on[1] = (uint8_t)(36 + (r * 4 + k) * 5 % 48);
                        a->on_midi(p, on, 3, 0);
                    }
                    a->render_block(p, out, 128);
                    double us = now_us() - t0;
                    if (r >= 3 && us > onset) onset = us;
                }
            }
            for (int b = 0; b < 40; b++) a->render_block(p, out, 128);
        }
        a->destroy_instance(p);
        printf("%-18s %6d %8.1f %8.1f %6.1f%% %9.1f\n", QUILT_TYPE_NAMES[i], osc,
               total / blocks, worst, total / blocks / 2902.0 * 100.0, onset);
    }
    return 0;
}
