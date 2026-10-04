/*
 * render — plays a note script through Quilt and writes a WAV, so it can be
 * heard on the Mac before it goes near the Move. It drives the module through
 * the same v2 API the chain host uses.
 *
 *   build/render script.txt out.wav
 *
 * A script is one event per line, in time order; '#' starts a comment.
 *
 *   0.0  type   Solo Cello        choose an instrument (or: preset 26)
 *   0.0  set    soft 0.8          any parameter
 *   0.0  on     48 90             note on: key, velocity
 *   0.5  press  48 100            pad pressure on one key
 *   2.0  off    48                note off
 *   2.0  cc     64 127            any controller
 *   4.0  end                      stop here (otherwise 2 s after the last event)
 *
 * It prints the peak, and what a block cost on this machine.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "host/plugin_api_v1.h"

plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);

#define SR 44100
#define BLOCK 128
#define MAX_EVENTS 4096

typedef struct {
    double t;
    char cmd[16];
    char args[160];
} event_t;

static event_t ev[MAX_EVENTS];

static void put32(FILE *f, uint32_t v) { uint8_t b[4] = { v, v >> 8, v >> 16, v >> 24 }; fwrite(b, 1, 4, f); }
static void put16(FILE *f, uint16_t v) { uint8_t b[2] = { v, v >> 8 }; fwrite(b, 1, 2, f); }

static void wav_header(FILE *f, uint32_t frames) {
    fwrite("RIFF", 1, 4, f); put32(f, 36 + frames * 4);
    fwrite("WAVEfmt ", 1, 8, f); put32(f, 16); put16(f, 1); put16(f, 2);
    put32(f, SR); put32(f, SR * 4); put16(f, 4); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, frames * 4);
}

static void run(plugin_api_v2_t *a, void *p, const event_t *e) {
    int x = 0, y = 0;
    if (!strcmp(e->cmd, "type")) a->set_param(p, "type", e->args);
    else if (!strcmp(e->cmd, "preset")) a->set_param(p, "preset", e->args);
    else if (!strcmp(e->cmd, "set")) {
        char key[64];
        int used = 0;
        if (sscanf(e->args, "%63s %n", key, &used) == 1) a->set_param(p, key, e->args + used);
    } else if (sscanf(e->args, "%d %d", &x, &y) >= 1) {
        uint8_t m[3] = { 0, (uint8_t)x, (uint8_t)y };
        if (!strcmp(e->cmd, "on")) m[0] = 0x90;
        else if (!strcmp(e->cmd, "off")) m[0] = 0x80;
        else if (!strcmp(e->cmd, "press")) m[0] = 0xA0;
        else if (!strcmp(e->cmd, "cc")) m[0] = 0xB0;
        else { fprintf(stderr, "render: unknown command '%s'\n", e->cmd); return; }
        a->on_midi(p, m, 3, 0);
    }
}

int main(int argc, char **argv) {
    if (argc != 3) { fprintf(stderr, "usage: render script.txt out.wav\n"); return 2; }
    FILE *in = fopen(argv[1], "r");
    if (!in) { perror(argv[1]); return 1; }
    int n = 0;
    double end = -1, last = 0;
    char line[256];
    while (fgets(line, sizeof(line), in) && n < MAX_EVENTS) {
        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';
        event_t *e = &ev[n];
        int used = 0;
        if (sscanf(line, "%lf %15s %n", &e->t, e->cmd, &used) < 2) continue;
        snprintf(e->args, sizeof(e->args), "%s", line + used);
        e->args[strcspn(e->args, "\r\n")] = '\0';
        for (int k = (int)strlen(e->args) - 1; k >= 0 && e->args[k] == ' '; k--) e->args[k] = '\0';
        if (!strcmp(e->cmd, "end")) { end = e->t; break; }
        if (e->t > last) last = e->t;
        n++;
    }
    fclose(in);
    if (end < 0) end = last + 2.0;

    plugin_api_v2_t *a = move_plugin_init_v2(NULL);
    void *p = a->create_instance(".", "");
    if (!p) { fprintf(stderr, "render: create_instance failed\n"); return 1; }

    FILE *out = fopen(argv[2], "wb");
    if (!out) { perror(argv[2]); return 1; }
    uint32_t frames = (uint32_t)(end * SR) / BLOCK * BLOCK;
    wav_header(out, frames);

    int16_t blk[BLOCK * 2];
    int next = 0, peak = 0;
    double worst = 0, total = 0;
    for (uint32_t f = 0; f < frames; f += BLOCK) {
        while (next < n && ev[next].t * SR <= f) run(a, p, &ev[next++]);
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        a->render_block(p, blk, BLOCK);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double us = (double)(t1.tv_sec - t0.tv_sec) * 1e6 + (double)(t1.tv_nsec - t0.tv_nsec) / 1e3;
        total += us;
        if (us > worst) worst = us;
        for (int i = 0; i < BLOCK * 2; i++) {
            if (abs(blk[i]) > peak) peak = abs(blk[i]);
            put16(out, (uint16_t)blk[i]);
        }
    }
    fclose(out);
    a->destroy_instance(p);
    printf("render: %s, %.1f s, peak %.1f dBFS, %.1f us per block (worst %.1f)\n", argv[2],
           (double)frames / SR, peak ? 20.0 * __builtin_log10(peak / 32768.0) : -999.0,
           total / (frames / BLOCK), worst);
    return 0;
}
