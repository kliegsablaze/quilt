/*
 * bench — what a block costs, instrument by instrument, at its worst: sixteen
 * notes held low, with the plate on. Runs on the Mac and on the Move
 * (scripts/bench.sh). DESIGN.md, Will it fit: the target is a quarter of the
 * 2902 us block.
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

int main(void) {
    plugin_api_v2_t *a = move_plugin_init_v2(NULL);
    int16_t out[256];
    printf("%-18s %6s %8s %8s %7s\n", "instrument", "osc", "mean us", "worst us", "block");
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
        void *p = a->create_instance(".", "");
        char idx[8];
        snprintf(idx, sizeof(idx), "%d", i);
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
        printf("%-18s %6d %8.1f %8.1f %6.1f%%\n", QUILT_TYPE_NAMES[i], quilt_modal_in_use((quilt_t *)p),
               total / blocks, worst, total / blocks / 2902.0 * 100.0);
        a->destroy_instance(p);
    }
    return 0;
}
