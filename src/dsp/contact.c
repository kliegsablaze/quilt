/*
 * Soft contact, after Stulov's hysteretic felt and Avanzini & Rocchesso's
 * contact force: F = K (c^p + alpha d(c^p)/dt) while the felt is compressed
 * by c, and nothing once it lets go.
 *
 * What the felt pushes against:
 *
 *   String  The push leaves as a wave in both directions, so the string
 *           first gives way like a resistance, 2Z (Z its wave impedance).
 *           The half that ran toward the near end (agraffe or capo) comes
 *           back inverted after x0/f0 seconds and throws the hammer off.
 *           That return is what ends the contact. Without it the string
 *           only absorbs, the hammer never rebounds, and a harder blow
 *           wrongly stays on longer. (Checked first; see DESIGN.md, Contact.)
 *           The far end's return is later than any soft contact and is left
 *           out.
 *   Bar     The struck point is a spring kb with damping r: the bar's local
 *           stiffness, which pushes the mallet back.
 *
 * The consequence everything soft rests on: a slow or soft hammer stays on
 * longer, so its force pulse is longer and smoother and cannot excite the
 * high modes. A fast or hard one is brief, stiff and bright.
 *
 * Integrated at four substeps per output sample (semi-implicit Euler).
 */
#include <math.h>
#include <string.h>

#include "contact.h"
#include "quilt.h"

#define SUB 4

static void strike(contact_t *c, float mass, float K, float p, float alpha, float v0) {
    c->active = 1;
    c->touched = 0;
    c->m = mass;
    c->K = K;
    c->p = p;
    c->alpha = alpha;
    c->xh = 0.0f;
    c->vh = v0;
    c->ys = 0.0f;
    c->f_prev = 0.0f;
    c->cp_prev = 0.0f;
    c->t = 0.0f;
    c->peak = 0.0f;
}

void contact_strike_string(contact_t *c, float mass, float K, float p, float alpha,
                           float z, float x0, float f0, float v0) {
    strike(c, mass, K, p, alpha, v0);
    c->kind = CONTACT_STRING;
    c->z2 = 2.0f * z;
    int tau = (int)(x0 / f0 * QUILT_SR * SUB + 0.5f);
    c->tau = tau < 1 ? 1 : (tau > CONTACT_DELAY - 1 ? CONTACT_DELAY - 1 : tau);
    c->w = 0;
    memset(c->dl, 0, sizeof(c->dl));
}

void contact_strike_bar(contact_t *c, float mass, float K, float p, float alpha,
                        float kb, float r, float v0) {
    strike(c, mass, K, p, alpha, v0);
    c->kind = CONTACT_BAR;
    c->kb = kb;
    c->z2 = r;
}

float contact_step(contact_t *c) {
    if (!c->active) return 0.0f;
    const float dt = 1.0f / (QUILT_SR * SUB);
    float sum = 0.0f;
    for (int i = 0; i < SUB; i++) {
        float comp = c->xh - c->ys;
        float f = 0.0f;
        if (comp > 0.0f) {
            float cp = powf(comp, c->p);
            f = c->K * (cp + c->alpha * (cp - c->cp_prev) / dt);
            if (f < 0.0f) f = 0.0f;
            c->cp_prev = cp;
            c->touched = 1;
        } else {
            c->cp_prev = 0.0f;
        }
        c->vh -= f / c->m * dt;
        c->xh += c->vh * dt;

        float vs;
        if (c->kind == CONTACT_STRING) {
            /* Velocity at the hammer: its own push, plus the inverted return
             * of what it sent toward the near end tau substeps ago. */
            int mask = CONTACT_DELAY - 1;
            float back = -c->dl[(c->w - c->tau) & mask];
            float out = f / c->z2;
            c->dl[c->w & mask] = out;
            c->w++;
            vs = out + back;
        } else {
            vs = (f - c->kb * c->ys) / c->z2;
        }
        c->ys += vs * dt;
        sum += f;
        if (f > c->peak) c->peak = f;
    }
    c->t += 1.0f / QUILT_SR;
    /* Done once the hammer has let go and is moving away, or after 30 ms. */
    if ((c->touched && c->xh < c->ys && c->vh < 0.0f) || c->t > 0.03f) c->active = 0;
    return sum / SUB;
}
