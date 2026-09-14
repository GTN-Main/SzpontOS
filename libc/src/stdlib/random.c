/*
 * SzpontOS Libc - Standard BSD/POSIX random, srandom, initstate, setstate
 * Based on 4.3BSD linear feedback shift register random number generation.
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#define TYPE_0      0       /* linear congruential */
#define BREAK_0     8
#define DEG_0       0
#define SEP_0       0

#define TYPE_1      1       /* x**7 + x**3 + 1 */
#define BREAK_1     32
#define DEG_1       7
#define SEP_1       3

#define TYPE_2      2       /* x**15 + x + 1 */
#define BREAK_2     64
#define DEG_2       15
#define SEP_2       1

#define TYPE_3      3       /* x**31 + x**3 + 1 */
#define BREAK_3     128
#define DEG_3       31
#define SEP_3       3

#define TYPE_4      4       /* x**63 + x + 1 */
#define BREAK_4     256
#define DEG_4       63
#define SEP_4       1

#define MAX_TYPES   5
#define NSHUFF      50

static const int degrees[MAX_TYPES] = { DEG_0, DEG_1, DEG_2, DEG_3, DEG_4 };
static const int seps[MAX_TYPES]    = { SEP_0, SEP_1, SEP_2, SEP_3, SEP_4 };

struct __random_state {
    uint32_t *rst_fptr;
    uint32_t *rst_rptr;
    uint32_t *rst_state;
    int       rst_type;
    int       rst_deg;
    int       rst_sep;
    uint32_t *rst_end_ptr;
    uint32_t  rst_randtbl[DEG_3 + 1];
};

static struct __random_state g_implicit = {
    .rst_randtbl = {
        TYPE_3,
        0x2cf41758, 0x27bb3711, 0x4916d4d1, 0x7b02f59f, 0x9b8e28eb, 0xc0e80269,
        0x696f5c16, 0x878f1ff5, 0x52d9c07f, 0x916a06cd, 0xb50b3a20, 0x2776970a,
        0xee4eb2a6, 0xe94640ec, 0xb1d65612, 0x9d1ed968, 0x1043f6b7, 0xa3432a76,
        0x17eacbb9, 0x3c09e2eb, 0x4f8c2b3,  0x708a1f57, 0xee341814, 0x95d0e4d2,
        0xb06f216c, 0x8bd2e72e, 0x8f7c38d7, 0xcfc6a8fc, 0x2a59495,  0xa20d2a69,
        0xe29d12d1
    },
    .rst_fptr = &g_implicit.rst_randtbl[SEP_3 + 1],
    .rst_rptr = &g_implicit.rst_randtbl[1],
    .rst_state = &g_implicit.rst_randtbl[1],
    .rst_type = TYPE_3,
    .rst_deg = DEG_3,
    .rst_sep = SEP_3,
    .rst_end_ptr = &g_implicit.rst_randtbl[DEG_3 + 1],
};

static inline uint32_t parkmiller32(uint32_t ctx) {
    int32_t hi, lo, x;
    x = (ctx % 0x7ffffffe) + 1;
    hi = x / 127773;
    lo = x % 127773;
    x = 16807 * lo - 2836 * hi;
    if (x < 0)
        x += 0x7fffffff;
    return (x - 1);
}

long random_r_impl(struct __random_state *estate) {
    uint32_t i;
    uint32_t *f, *r;

    if (estate->rst_type == TYPE_0) {
        i = estate->rst_state[0];
        i = parkmiller32(i);
        estate->rst_state[0] = i;
    } else {
        f = estate->rst_fptr;
        r = estate->rst_rptr;
        *f += *r;
        i = *f >> 1;
        if (++f >= estate->rst_end_ptr) {
            f = estate->rst_state;
            ++r;
        } else if (++r >= estate->rst_end_ptr) {
            r = estate->rst_state;
        }
        estate->rst_fptr = f;
        estate->rst_rptr = r;
    }
    return (long)i;
}

void srandom_r_impl(struct __random_state *estate, unsigned int x) {
    int i, lim;

    estate->rst_state[0] = (uint32_t)x;
    if (estate->rst_type == TYPE_0) {
        lim = NSHUFF;
    } else {
        for (i = 1; i < estate->rst_deg; i++)
            estate->rst_state[i] = parkmiller32(estate->rst_state[i - 1]);
        estate->rst_fptr = &estate->rst_state[estate->rst_sep];
        estate->rst_rptr = &estate->rst_state[0];
        lim = 10 * estate->rst_deg;
    }
    for (i = 0; i < lim; i++)
        (void)random_r_impl(estate);
}

static int initstate_r_impl(struct __random_state *estate, unsigned int seed, uint32_t *arg_state, size_t sz) {
    if (sz < BREAK_0)
        return EINVAL;

    if (sz < BREAK_1) {
        estate->rst_type = TYPE_0;
        estate->rst_deg = DEG_0;
        estate->rst_sep = SEP_0;
    } else if (sz < BREAK_2) {
        estate->rst_type = TYPE_1;
        estate->rst_deg = DEG_1;
        estate->rst_sep = SEP_1;
    } else if (sz < BREAK_3) {
        estate->rst_type = TYPE_2;
        estate->rst_deg = DEG_2;
        estate->rst_sep = SEP_2;
    } else if (sz < BREAK_4) {
        estate->rst_type = TYPE_3;
        estate->rst_deg = DEG_3;
        estate->rst_sep = SEP_3;
    } else {
        estate->rst_type = TYPE_4;
        estate->rst_deg = DEG_4;
        estate->rst_sep = SEP_4;
    }
    estate->rst_state = arg_state + 1;
    estate->rst_end_ptr = &estate->rst_state[estate->rst_deg];
    srandom_r_impl(estate, seed);
    return 0;
}

char *initstate(unsigned int seed, char *arg_state, size_t n) {
    char *ostate = (char *)(&g_implicit.rst_state[-1]);
    uint32_t *int_arg_state = (uint32_t *)arg_state;
    int error;

    if (g_implicit.rst_type == TYPE_0)
        g_implicit.rst_state[-1] = g_implicit.rst_type;
    else
        g_implicit.rst_state[-1] = MAX_TYPES * (g_implicit.rst_rptr - g_implicit.rst_state) + g_implicit.rst_type;

    error = initstate_r_impl(&g_implicit, seed, int_arg_state, n);
    if (error != 0)
        return NULL;

    if (g_implicit.rst_type == TYPE_0)
        int_arg_state[0] = g_implicit.rst_type;
    else
        int_arg_state[0] = MAX_TYPES * (g_implicit.rst_rptr - g_implicit.rst_state) + g_implicit.rst_type;

    return ostate;
}

char *setstate(char *arg_state) {
    uint32_t *new_state = (uint32_t *)arg_state;
    uint32_t type = new_state[0] % MAX_TYPES;
    uint32_t rear = new_state[0] / MAX_TYPES;
    char *ostate = (char *)(&g_implicit.rst_state[-1]);

    if (type != TYPE_0 && rear >= (uint32_t)degrees[type])
        return NULL;

    if (g_implicit.rst_type == TYPE_0)
        g_implicit.rst_state[-1] = g_implicit.rst_type;
    else
        g_implicit.rst_state[-1] = MAX_TYPES * (g_implicit.rst_rptr - g_implicit.rst_state) + g_implicit.rst_type;

    g_implicit.rst_type = type;
    g_implicit.rst_deg = degrees[type];
    g_implicit.rst_sep = seps[type];
    g_implicit.rst_state = new_state + 1;
    if (g_implicit.rst_type != TYPE_0) {
        g_implicit.rst_rptr = &g_implicit.rst_state[rear];
        g_implicit.rst_fptr = &g_implicit.rst_state[(rear + g_implicit.rst_sep) % g_implicit.rst_deg];
    }
    g_implicit.rst_end_ptr = &g_implicit.rst_state[g_implicit.rst_deg];
    return ostate;
}

long random(void) {
    return random_r_impl(&g_implicit);
}

void srandom(unsigned int seed) {
    srandom_r_impl(&g_implicit, seed);
}
