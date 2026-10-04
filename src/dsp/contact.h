/*
 * Soft contact: a felt hammer, yarn mallet or fingertip meeting a string or
 * bar. DESIGN.md, Modal engine, Contact.
 */
#ifndef QUILT_CONTACT_H
#define QUILT_CONTACT_H

#define CONTACT_DELAY 1024   /* substeps of string between hammer and near end */

typedef enum { CONTACT_STRING, CONTACT_BAR } contact_kind_t;

typedef struct {
    int active, touched;
    contact_kind_t kind;
    float m;        /* hammer or mallet mass, kg */
    float K, p;     /* felt law F = K (c^p + alpha d(c^p)/dt), c in metres */
    float alpha;    /* hysteresis, s (Stulov) */
    float z2;       /* string: twice its wave impedance; bar: its point damping, kg/s */
    float kb;       /* bar: local stiffness, N/m */
    float xh, vh;   /* hammer position and velocity, m and m/s */
    float ys;       /* the object's displacement at the contact point, m */
    float f_prev, cp_prev;
    float t;        /* time since the strike, s */
    float peak;     /* largest force so far, N (for tests) */
    int tau, w;     /* string: round trip to the near end, in substeps */
    float dl[CONTACT_DELAY];
} contact_t;

/* A string of impedance z struck at a fraction x0 of its length, fundamental f0. */
void contact_strike_string(contact_t *c, float mass, float K, float p, float alpha,
                           float z, float x0, float f0, float v0);
/* A bar point of local stiffness kb and damping r. */
void contact_strike_bar(contact_t *c, float mass, float K, float p, float alpha,
                        float kb, float r, float v0);
/* Advances one output sample and returns the mean force over it, in newtons. */
float contact_step(contact_t *c);

#endif
