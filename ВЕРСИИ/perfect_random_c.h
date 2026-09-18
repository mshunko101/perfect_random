// perfect_random.h
// MSHUNKO 2026 © Шунько Михаил Геннадьевич.
// Unified CascadePRNG — Standard, Crypto, and Monte Carlo modes in one file.
// C version — fully autonomous, no external dependencies.
//
// Three modes of operation:
//   1. Standard  — 31-bit, simple seed, a*b+c output      -> simulations, games
//   2. Crypto    — 31-bit, SplitMix64 seed, role-permuted   -> keys, nonce, RTK
//   3. MonteCarlo — 61-bit, __int128, high-precision        -> Monte Carlo
//
// Usage (C):
//   cascade_prng rng;
//   cascade_prng_init(&rng, seed, 49, CASCADE_MODE_STANDARD);
//   double x = cascade_prng_generate(&rng);
//   uint32_t r = cascade_prng_generate_raw(&rng);
//
//   cascade_prng64 rng64;
//   cascade_prng64_init(&rng64, seed, 49, CASCADE_MODE_STANDARD);
//   double x = cascade_prng64_generate(&rng64);
//   long double ld = cascade_prng64_generate_ld(&rng64);
//
#ifndef PERFECT_RANDOM_H_C
#define PERFECT_RANDOM_H_C

#include <stdint.h>
#include <stddef.h>
#include <math.h>

// ─── 128-bit arithmetic detection ──────────────────────────────
#if defined(__SIZEOF_INT128__) || defined(__int128)
    #define HAS_INT128 1
#else
    #define HAS_INT128 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ═══════════════════════════════════════════════════════════════
//  Mode enum
// ═══════════════════════════════════════════════════════════════

typedef enum {
    CASCADE_MODE_STANDARD = 0,
    CASCADE_MODE_CRYPTO   = 1
} cascade_mode_t;

// ═══════════════════════════════════════════════════════════════
//  SplitMix64 — independent hashing for Crypto-mode initialization
// ═══════════════════════════════════════════════════════════════

static inline uint64_t splitmix64(uint64_t *state) {
    uint64_t z = (*state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

// ═══════════════════════════════════════════════════════════════
//  Geometry — rotation period calculation
// ═══════════════════════════════════════════════════════════════

typedef struct {
    double x, y, z;
} pr_point_t;

typedef struct {
    pr_point_t A, B, O_okr;
    double time_AB;
} rotation_calculator_t;

static inline pr_point_t pr_vector_between(pr_point_t p1, pr_point_t p2) {
    pr_point_t r;
    r.x = p1.x - p2.x;
    r.y = p1.y - p2.y;
    r.z = p1.z - p2.z;
    return r;
}

static inline double pr_dot(pr_point_t v1, pr_point_t v2) {
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

static inline double pr_length(pr_point_t v) {
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

static inline double pr_angle(const rotation_calculator_t *rc) {
    pr_point_t OA = pr_vector_between(rc->A, rc->O_okr);
    pr_point_t OB = pr_vector_between(rc->B, rc->O_okr);
    double cos_a = pr_dot(OA, OB) / (pr_length(OA) * pr_length(OB));
    return acos(cos_a);
}

static inline void rotation_calculator_init(rotation_calculator_t *rc, double time) {
    rc->A.x = -3.0;    rc->A.y = -0.5;     rc->A.z = -2.5;
    rc->B.x = -5.0/3.0; rc->B.y = -5.0/6.0;  rc->B.z = 25.0/4.0;
    rc->O_okr.x = -2.64; rc->O_okr.y = -7.91; rc->O_okr.z = 1.65;
    rc->time_AB = time;
}

static inline double rotation_calculator_get_period(const rotation_calculator_t *rc) {
    double omega = pr_angle(rc) / rc->time_AB;
    return 2.0 * M_PI / omega;
}

// ═══════════════════════════════════════════════════════════════
//  CascadePRNG — 31-bit cascade PRNG on Mersenne prime M_31
//  Modes: Standard (simple seed, a*b+c) | Crypto (SplitMix64, role-permuted)
// ═══════════════════════════════════════════════════════════════

#define CASCADE_PRNG_M     2147483647ULL      // 2^31 - 1
#define CASCADE_PRNG_MUL_A 954437177ULL       // 5/9 mod M
#define CASCADE_PRNG_MUL_B 715827884ULL       // 5/3 mod M

typedef struct {
    cascade_mode_t mode;
    uint64_t a, b, c;
    int      counter;
    int      out_idx;
    size_t   N;
} cascade_prng;

static inline void cascade_prng_seed_standard(cascade_prng *rng, uint64_t s) {
    if (s == 0) s = 1;
    rng->a = (s * CASCADE_PRNG_MUL_A) % CASCADE_PRNG_M;
    if (rng->a == 0) rng->a = 1;
    rng->b = (s * CASCADE_PRNG_MUL_B) % CASCADE_PRNG_M;
    if (rng->b == 0) rng->b = 1;
    rng->c = (s * s + 1) % CASCADE_PRNG_M;
    if (rng->c == 0) rng->c = 1;
}

static inline void cascade_prng_seed_crypto(cascade_prng *rng, uint64_t s) {
    if (s == 0) s = 1;
    uint64_t sm_state = s;
    rng->a = splitmix64(&sm_state) % CASCADE_PRNG_M;
    if (rng->a == 0) rng->a = 1;
    rng->b = splitmix64(&sm_state) % CASCADE_PRNG_M;
    if (rng->b == 0) rng->b = 1;
    rng->c = splitmix64(&sm_state) % CASCADE_PRNG_M;
    if (rng->c == 0) rng->c = 1;
}

static inline void cascade_prng_seed(cascade_prng *rng, uint64_t s) {
    if (rng->mode == CASCADE_MODE_CRYPTO)
        cascade_prng_seed_crypto(rng, s);
    else
        cascade_prng_seed_standard(rng, s);
    rng->counter = 0;
    rng->out_idx = 0;
}

static inline void cascade_prng_init(cascade_prng *rng, uint64_t s,
                                     size_t n, cascade_mode_t mode) {
    rng->mode = mode;
    rng->counter = 0;
    rng->out_idx = 0;
    rng->N = (n < 1) ? 1 : n;
    cascade_prng_seed(rng, s);
}

static inline void cascade_prng_step(cascade_prng *rng) {
    rng->a = (rng->a * CASCADE_PRNG_MUL_A) % CASCADE_PRNG_M;
    rng->b = (rng->b * CASCADE_PRNG_MUL_B) % CASCADE_PRNG_M;
    rng->c = (rng->c * rng->c + rng->a + rng->b) % CASCADE_PRNG_M;
    if (++rng->counter >= (int)rng->N) {
        uint64_t tmp = rng->a;
        rng->a = rng->c;
        rng->c = rng->b;
        rng->b = tmp;
        rng->counter = 0;
        rng->out_idx = (rng->out_idx + 1) % 3;
    }
}

static inline uint32_t cascade_prng_current_value(const cascade_prng *rng) {
    if (rng->mode == CASCADE_MODE_CRYPTO) {
        switch (rng->out_idx) {
            case 0:  return (uint32_t)((rng->a * rng->b + rng->c) % CASCADE_PRNG_M);
            case 1:  return (uint32_t)((rng->b * rng->c + rng->a) % CASCADE_PRNG_M);
            default: return (uint32_t)((rng->c * rng->a + rng->b) % CASCADE_PRNG_M);
        }
    }
    return (uint32_t)((rng->a * rng->b + rng->c) % CASCADE_PRNG_M);
}

static inline double cascade_prng_generate(cascade_prng *rng) {
    cascade_prng_step(rng);
    return (double)cascade_prng_current_value(rng) / (double)CASCADE_PRNG_M;
}

static inline uint32_t cascade_prng_generate_raw(cascade_prng *rng) {
    cascade_prng_step(rng);
    return cascade_prng_current_value(rng);
}

// ═══════════════════════════════════════════════════════════════
//  CascadePRNG64 — 61-bit cascade PRNG on Mersenne prime M_61
//  For high-precision Monte Carlo. 183 bits of state, tail resolution ~1e-18.
// ═══════════════════════════════════════════════════════════════

#define CASCADE_PRNG64_M     2305843009213693951ULL  // 2^61 - 1
#define CASCADE_PRNG64_MUL_A 0x0E38E38E38E38E39ULL   // 5/9 mod M
#define CASCADE_PRNG64_MUL_B 0x0AAAAAAAAAAAAAACULL    // 5/3 mod M

typedef struct {
    cascade_mode_t mode;
    uint64_t a, b, c;
    int      counter;
    int      out_idx;
    size_t   N;
} cascade_prng64;

static inline uint64_t cascade_prng64_mulmod(uint64_t x, uint64_t y) {
#if HAS_INT128
    return (uint64_t)((__uint128_t)x * y % CASCADE_PRNG64_M);
#else
    // MSVC fallback: multiplication via decomposition
    uint64_t x_lo = x & 0xFFFFFFFF, x_hi = x >> 32;
    uint64_t y_lo = y & 0xFFFFFFFF, y_hi = y >> 32;
    uint64_t lo = x_lo * y_lo;
    uint64_t mid = x_lo * y_hi + x_hi * y_lo;
    uint64_t hi = x_hi * y_hi;
    uint64_t mid_lo = mid << 32;
    uint64_t mid_hi = mid >> 32;
    uint64_t r_lo = lo + mid_lo;
    uint64_t carry = (r_lo < lo) ? 1 : 0;
    uint64_t r_hi = hi + mid_hi + carry;
    // M = 2^61-1, 2^64 mod M = 8
    uint64_t t = (r_hi * 8 + r_lo) % CASCADE_PRNG64_M;
    return t;
#endif
}

static inline void cascade_prng64_seed_standard(cascade_prng64 *rng, uint64_t s) {
    if (s == 0) s = 1;
    rng->a = cascade_prng64_mulmod(s, CASCADE_PRNG64_MUL_A);
    if (rng->a == 0) rng->a = 1;
    rng->b = cascade_prng64_mulmod(s, CASCADE_PRNG64_MUL_B);
    if (rng->b == 0) rng->b = 1;
    rng->c = (cascade_prng64_mulmod(s, s) + 1) % CASCADE_PRNG64_M;
    if (rng->c == 0) rng->c = 1;
}

static inline void cascade_prng64_seed_crypto(cascade_prng64 *rng, uint64_t s) {
    if (s == 0) s = 1;
    uint64_t sm_state = s;
    rng->a = splitmix64(&sm_state) % CASCADE_PRNG64_M;
    if (rng->a == 0) rng->a = 1;
    rng->b = splitmix64(&sm_state) % CASCADE_PRNG64_M;
    if (rng->b == 0) rng->b = 1;
    rng->c = splitmix64(&sm_state) % CASCADE_PRNG64_M;
    if (rng->c == 0) rng->c = 1;
}

static inline void cascade_prng64_seed(cascade_prng64 *rng, uint64_t s) {
    if (rng->mode == CASCADE_MODE_CRYPTO)
        cascade_prng64_seed_crypto(rng, s);
    else
        cascade_prng64_seed_standard(rng, s);
    rng->counter = 0;
    rng->out_idx = 0;
}

static inline void cascade_prng64_init(cascade_prng64 *rng, uint64_t s,
                                        size_t n, cascade_mode_t mode) {
    rng->mode = mode;
    rng->counter = 0;
    rng->out_idx = 0;
    rng->N = (n < 1) ? 1 : n;
    cascade_prng64_seed(rng, s);
}

static inline void cascade_prng64_step(cascade_prng64 *rng) {
    rng->a = cascade_prng64_mulmod(rng->a, CASCADE_PRNG64_MUL_A);
    rng->b = cascade_prng64_mulmod(rng->b, CASCADE_PRNG64_MUL_B);
    rng->c = (cascade_prng64_mulmod(rng->c, rng->c) + rng->a + rng->b) % CASCADE_PRNG64_M;
    if (++rng->counter >= (int)rng->N) {
        uint64_t tmp = rng->a;
        rng->a = rng->c;
        rng->c = rng->b;
        rng->b = tmp;
        rng->counter = 0;
        rng->out_idx = (rng->out_idx + 1) % 3;
    }
}

static inline uint64_t cascade_prng64_current_value(const cascade_prng64 *rng) {
    uint64_t v;
    if (rng->mode == CASCADE_MODE_CRYPTO) {
        switch (rng->out_idx) {
            case 0:  v = cascade_prng64_mulmod(rng->a, rng->b) + rng->c; break;
            case 1:  v = cascade_prng64_mulmod(rng->b, rng->c) + rng->a; break;
            default: v = cascade_prng64_mulmod(rng->c, rng->a) + rng->b; break;
        }
    } else {
        v = cascade_prng64_mulmod(rng->a, rng->b) + rng->c;
    }
    return v % CASCADE_PRNG64_M;
}

static inline double cascade_prng64_generate(cascade_prng64 *rng) {
    cascade_prng64_step(rng);
    return (double)cascade_prng64_current_value(rng) / (double)CASCADE_PRNG64_M;
}

static inline long double cascade_prng64_generate_ld(cascade_prng64 *rng) {
    cascade_prng64_step(rng);
    return (long double)cascade_prng64_current_value(rng) / (long double)CASCADE_PRNG64_M;
}

static inline uint64_t cascade_prng64_generate_raw(cascade_prng64 *rng) {
    cascade_prng64_step(rng);
    return cascade_prng64_current_value(rng);
}

static inline uint64_t cascade_prng64_generate_u64(cascade_prng64 *rng) {
    cascade_prng64_step(rng);
    uint64_t v = cascade_prng64_current_value(rng);
    uint64_t extra = (v >> 58) & 0x7;
    return (v << 3) | extra;
}

static inline void cascade_prng64_generate2(cascade_prng64 *rng,
                                             double *x, double *y) {
    cascade_prng64_step(rng);
    uint64_t v1 = cascade_prng64_current_value(rng);
    cascade_prng64_step(rng);
    uint64_t v2 = cascade_prng64_current_value(rng);
    *x = (double)v1 / (double)CASCADE_PRNG64_M;
    *y = (double)v2 / (double)CASCADE_PRNG64_M;
}

// ═══════════════════════════════════════════════════════════════
//  Seed derivation
// ═══════════════════════════════════════════════════════════════

#define SEED_DELTA 0x9E3779B97F4A7C15ULL

static inline uint64_t seed_cascade_derive(uint64_t master_seed, int index,
                                             cascade_mode_t mode) {
    cascade_prng factory;
    cascade_prng_init(&factory, master_seed + (uint64_t)index * SEED_DELTA, 49, mode);
    uint64_t hi = (uint64_t)cascade_prng_generate_raw(&factory);
    uint64_t lo = (uint64_t)cascade_prng_generate_raw(&factory);
    return (hi << 32) | lo;
}

static inline uint64_t seed_cascade64_derive(uint64_t master_seed, int index,
                                               cascade_mode_t mode) {
    cascade_prng64 factory;
    cascade_prng64_init(&factory, master_seed + (uint64_t)index * SEED_DELTA, 49, mode);
    uint64_t hi = cascade_prng64_generate_raw(&factory);
    uint64_t lo = cascade_prng64_generate_raw(&factory);
    return hi ^ lo ^ ((hi >> 3) << 3);
}

// ═══════════════════════════════════════════════════════════════
//  AssociativityCore (31-bit)
// ═══════════════════════════════════════════════════════════════

typedef struct {
    cascade_prng rng;
} assoc_core_t;

static inline void assoc_core_init(assoc_core_t *core, uint64_t s, cascade_mode_t mode) {
    cascade_prng_init(&core->rng, s, 48, mode);
}

static inline double assoc_core_generate(assoc_core_t *core) {
    return cascade_prng_generate(&core->rng);
}

static inline uint32_t assoc_core_generate_raw(assoc_core_t *core) {
    return cascade_prng_generate_raw(&core->rng);
}

static inline uint64_t assoc_core_state_seed(assoc_core_t *core) {
    uint64_t a = core->rng.a;
    uint64_t b = core->rng.b;
    uint64_t c = core->rng.c;
    return (a << 31) ^ (b << 13) ^ c ^ ((uint64_t)core->rng.counter * 0x100000001B3ULL);
}

// ═══════════════════════════════════════════════════════════════
//  MeanCore (31-bit)
// ═══════════════════════════════════════════════════════════════

typedef struct {
    double mean;
    cascade_prng rng;
} mean_core_t;

static inline void mean_core_init(mean_core_t *core, double m,
                                   uint64_t s, cascade_mode_t mode) {
    core->mean = m;
    cascade_prng_init(&core->rng, s, 49, mode);
}

static inline double mean_core_adjust(mean_core_t *core, double base) {
    return (base / (double)CASCADE_PRNG_M) * core->mean + cascade_prng_generate(&core->rng);
}

// ═══════════════════════════════════════════════════════════════
//  FantasyCore (31-bit)
// ═══════════════════════════════════════════════════════════════

#define FANTASY_MAX_DIMS 64

typedef struct {
    cascade_prng rng;
    int dims[FANTASY_MAX_DIMS][2];  // [0]=val_a, [1]=val_b
    int num_dims;
} fantasy_core_t;

static inline void fantasy_core_init(fantasy_core_t *core, uint64_t s, cascade_mode_t mode) {
    cascade_prng_init(&core->rng, s, 50, mode);
    core->num_dims = 0;
}

static inline void fantasy_core_add_collision(fantasy_core_t *core, uint32_t a, uint32_t b) {
    if (a == b) return;
    for (int i = 0; i < 32; i++) {
        int bit_a = (a >> i) & 1;
        int bit_b = (b >> i) & 1;
        if (bit_a != bit_b) {
            int va = bit_a ? 1 : 0;
            int vb = bit_b ? 0 : 1;
            // Check for duplicates
            int found = 0;
            for (int j = 0; j < core->num_dims; j++) {
                if (core->dims[j][0] == va && core->dims[j][1] == vb) {
                    found = 1;
                    break;
                }
            }
            if (!found && core->num_dims < FANTASY_MAX_DIMS) {
                core->dims[core->num_dims][0] = va;
                core->dims[core->num_dims][1] = vb;
                core->num_dims++;
            }
        }
    }
}

static inline double fantasy_core_apply(fantasy_core_t *core, double base) {
    for (int i = 0; i < core->num_dims; i++) {
        if (cascade_prng_generate(&core->rng) < 0.5)
            base += core->dims[i][0] * 0.01;
        else
            base += core->dims[i][1] * 0.01;
    }
    return base;
}

static inline void fantasy_core_clear(fantasy_core_t *core) {
    core->num_dims = 0;
}

// ═══════════════════════════════════════════════════════════════
//  Simple hash set for collision history (31-bit)
//  Open-addressing, fixed size
// ═══════════════════════════════════════════════════════════════

#define HASH_SET_CAPACITY 128
#define HASH_SET_EMPTY 0xFFFFFFFF

typedef struct {
    uint32_t slots[HASH_SET_CAPACITY];
    size_t size;
} hash_set32_t;

static inline void hash_set32_init(hash_set32_t *hs) {
    for (int i = 0; i < HASH_SET_CAPACITY; i++) hs->slots[i] = HASH_SET_EMPTY;
    hs->size = 0;
}

static inline int hash_set32_contains(hash_set32_t *hs, uint32_t val) {
    uint32_t idx = val % HASH_SET_CAPACITY;
    for (int i = 0; i < HASH_SET_CAPACITY; i++) {
        uint32_t pos = (idx + i) % HASH_SET_CAPACITY;
        if (hs->slots[pos] == HASH_SET_EMPTY) return 0;
        if (hs->slots[pos] == val) return 1;
    }
    return 0;
}

static inline void hash_set32_insert(hash_set32_t *hs, uint32_t val) {
    if (hs->size >= HASH_SET_CAPACITY) return;
    uint32_t idx = val % HASH_SET_CAPACITY;
    for (int i = 0; i < HASH_SET_CAPACITY; i++) {
        uint32_t pos = (idx + i) % HASH_SET_CAPACITY;
        if (hs->slots[pos] == HASH_SET_EMPTY) {
            hs->slots[pos] = val;
            hs->size++;
            return;
        }
        if (hs->slots[pos] == val) return; // already present
    }
}

static inline void hash_set32_clear(hash_set32_t *hs) {
    for (int i = 0; i < HASH_SET_CAPACITY; i++) hs->slots[i] = HASH_SET_EMPTY;
    hs->size = 0;
}

// ═══════════════════════════════════════════════════════════════
//  RNG — 31-bit high-level generator with collision tracking
// ═══════════════════════════════════════════════════════════════

#define RNG_MAX_RETRIES 49
#define RNG_MAX_HISTORY 49

typedef struct {
    uint64_t master_seed;
    cascade_mode_t mode;
    assoc_core_t assoc_core;
    mean_core_t mean_core;
    fantasy_core_t fantasy_core;
    hash_set32_t history;
    size_t inc_counter;
    size_t inc_max;
} prng_t;

static inline void prng_reseed(prng_t *prng, uint64_t new_seed) {
    prng->master_seed = new_seed;
    uint64_t s1 = seed_cascade_derive(new_seed, 0, prng->mode);
    uint64_t s2 = seed_cascade_derive(new_seed, 1, prng->mode);
    uint64_t s3 = seed_cascade_derive(new_seed, 2, prng->mode);
    assoc_core_init(&prng->assoc_core, s1, prng->mode);
    mean_core_init(&prng->mean_core, 0.5, s2, prng->mode);
    fantasy_core_init(&prng->fantasy_core, s3, prng->mode);
}

static inline void prng_init(prng_t *prng, unsigned int seed, double period,
                              cascade_mode_t mode) {
    prng->master_seed = seed;
    prng->mode = mode;
    uint64_t s1 = seed_cascade_derive(seed, 0, mode);
    uint64_t s2 = seed_cascade_derive(seed, 1, mode);
    uint64_t s3 = seed_cascade_derive(seed, 2, mode);
    assoc_core_init(&prng->assoc_core, s1, mode);
    mean_core_init(&prng->mean_core, 0.5, s2, mode);
    fantasy_core_init(&prng->fantasy_core, s3, mode);
    hash_set32_init(&prng->history);
    prng->inc_counter = 0;

    rotation_calculator_t rc;
    rotation_calculator_init(&rc, period);
    prng->inc_max = (size_t)round((rotation_calculator_get_period(&rc) * 365.25 * 24 * 3600) / 8.0);
}

static inline int prng_is_collision(prng_t *prng, uint32_t value) {
    if (hash_set32_contains(&prng->history, value)) return 1;
    hash_set32_insert(&prng->history, value);
    return 0;
}

static inline double prng_generate(prng_t *prng) {
    if (prng->inc_counter >= prng->inc_max) {
        uint64_t fresh_seed = assoc_core_state_seed(&prng->assoc_core);
        prng_reseed(prng, fresh_seed ^ prng->master_seed);
        prng->inc_counter = 0;
    }

    uint32_t base_raw = assoc_core_generate_raw(&prng->assoc_core);
    double base = (double)base_raw / (double)CASCADE_PRNG_M;

    int retries = 0;
    uint32_t previous_raw = base_raw;

    while (prng_is_collision(prng, base_raw) && retries < RNG_MAX_RETRIES) {
        previous_raw = base_raw;
        base_raw = assoc_core_generate_raw(&prng->assoc_core);
        if (base_raw == previous_raw) {
            fantasy_core_add_collision(&prng->fantasy_core, previous_raw, base_raw);
        }
        retries++;
    }

    if (retries >= RNG_MAX_RETRIES) {
        uint64_t fresh_seed = assoc_core_state_seed(&prng->assoc_core);
        prng_reseed(prng, fresh_seed);
        base_raw = assoc_core_generate_raw(&prng->assoc_core);
        base = (double)base_raw / (double)CASCADE_PRNG_M;
        hash_set32_clear(&prng->history);
        fantasy_core_clear(&prng->fantasy_core);
    }

    if (prng->history.size >= RNG_MAX_HISTORY) {
        hash_set32_clear(&prng->history);
        fantasy_core_clear(&prng->fantasy_core);
    }

    double mean_adjusted = mean_core_adjust(&prng->mean_core, base);
    double current = fantasy_core_apply(&prng->fantasy_core, mean_adjusted);
    prng->inc_counter += 8;
    return current;
}

static inline void prng_reset(prng_t *prng) {
    prng_reseed(prng, prng->master_seed + 1);
    hash_set32_clear(&prng->history);
    fantasy_core_clear(&prng->fantasy_core);
    prng->inc_counter = 0;
}

// ═══════════════════════════════════════════════════════════════
//  AssociativityCore64 (61-bit)
// ═══════════════════════════════════════════════════════════════

typedef struct {
    cascade_prng64 rng;
} assoc_core64_t;

static inline void assoc_core64_init(assoc_core64_t *core, uint64_t s, cascade_mode_t mode) {
    cascade_prng64_init(&core->rng, s, 48, mode);
}

static inline double assoc_core64_generate(assoc_core64_t *core) {
    return cascade_prng64_generate(&core->rng);
}

static inline long double assoc_core64_generate_ld(assoc_core64_t *core) {
    return cascade_prng64_generate_ld(&core->rng);
}

static inline uint64_t assoc_core64_generate_raw(assoc_core64_t *core) {
    return cascade_prng64_generate_raw(&core->rng);
}

static inline uint64_t assoc_core64_state_seed(assoc_core64_t *core) {
    uint64_t a = core->rng.a;
    uint64_t b = core->rng.b;
    uint64_t c = core->rng.c;
    return (a << 3) ^ (b >> 7) ^ c ^ ((uint64_t)core->rng.counter * 0x100000001B3ULL);
}

// ═══════════════════════════════════════════════════════════════
//  MeanCore64 (61-bit)
// ═══════════════════════════════════════════════════════════════

typedef struct {
    double mean;
    cascade_prng64 rng;
} mean_core64_t;

static inline void mean_core64_init(mean_core64_t *core, double m,
                                     uint64_t s, cascade_mode_t mode) {
    core->mean = m;
    cascade_prng64_init(&core->rng, s, 49, mode);
}

static inline double mean_core64_adjust(mean_core64_t *core, double base) {
    return (base / (double)CASCADE_PRNG64_M) * core->mean + cascade_prng64_generate(&core->rng);
}

static inline long double mean_core64_adjust_ld(mean_core64_t *core, long double base) {
    return (base / (long double)CASCADE_PRNG64_M) * (long double)core->mean
           + cascade_prng64_generate_ld(&core->rng);
}

// ═══════════════════════════════════════════════════════════════
//  FantasyCore64 (61-bit)
// ═══════════════════════════════════════════════════════════════

typedef struct {
    cascade_prng64 rng;
    int dims[FANTASY_MAX_DIMS][2];
    int num_dims;
} fantasy_core64_t;

static inline void fantasy_core64_init(fantasy_core64_t *core, uint64_t s, cascade_mode_t mode) {
    cascade_prng64_init(&core->rng, s, 50, mode);
    core->num_dims = 0;
}

static inline void fantasy_core64_add_collision(fantasy_core64_t *core, uint64_t a, uint64_t b) {
    if (a == b) return;
    for (int i = 0; i < 64; i++) {
        int bit_a = (int)((a >> i) & 1);
        int bit_b = (int)((b >> i) & 1);
        if (bit_a != bit_b) {
            int va = bit_a ? 1 : 0;
            int vb = bit_b ? 0 : 1;
            int found = 0;
            for (int j = 0; j < core->num_dims; j++) {
                if (core->dims[j][0] == va && core->dims[j][1] == vb) {
                    found = 1;
                    break;
                }
            }
            if (!found && core->num_dims < FANTASY_MAX_DIMS) {
                core->dims[core->num_dims][0] = va;
                core->dims[core->num_dims][1] = vb;
                core->num_dims++;
            }
        }
    }
}

static inline double fantasy_core64_apply(fantasy_core64_t *core, double base) {
    for (int i = 0; i < core->num_dims; i++) {
        if (cascade_prng64_generate(&core->rng) < 0.5)
            base += core->dims[i][0] * 0.01;
        else
            base += core->dims[i][1] * 0.01;
    }
    return base;
}

static inline long double fantasy_core64_apply_ld(fantasy_core64_t *core, long double base) {
    for (int i = 0; i < core->num_dims; i++) {
        if (cascade_prng64_generate_ld(&core->rng) < 0.5L)
            base += core->dims[i][0] * 0.01L;
        else
            base += core->dims[i][1] * 0.01L;
    }
    return base;
}

static inline void fantasy_core64_clear(fantasy_core64_t *core) {
    core->num_dims = 0;
}

// ═══════════════════════════════════════════════════════════════
//  Simple hash set for collision history (64-bit)
// ═══════════════════════════════════════════════════════════════

#define HASH_SET64_CAPACITY 128
#define HASH_SET64_EMPTY 0xFFFFFFFFFFFFFFFFULL

typedef struct {
    uint64_t slots[HASH_SET64_CAPACITY];
    size_t size;
} hash_set64_t;

static inline void hash_set64_init(hash_set64_t *hs) {
    for (int i = 0; i < HASH_SET64_CAPACITY; i++) hs->slots[i] = HASH_SET64_EMPTY;
    hs->size = 0;
}

static inline int hash_set64_contains(hash_set64_t *hs, uint64_t val) {
    uint32_t idx = (uint32_t)(val % HASH_SET64_CAPACITY);
    for (int i = 0; i < HASH_SET64_CAPACITY; i++) {
        uint32_t pos = (idx + i) % HASH_SET64_CAPACITY;
        if (hs->slots[pos] == HASH_SET64_EMPTY) return 0;
        if (hs->slots[pos] == val) return 1;
    }
    return 0;
}

static inline void hash_set64_insert(hash_set64_t *hs, uint64_t val) {
    if (hs->size >= HASH_SET64_CAPACITY) return;
    uint32_t idx = (uint32_t)(val % HASH_SET64_CAPACITY);
    for (int i = 0; i < HASH_SET64_CAPACITY; i++) {
        uint32_t pos = (idx + i) % HASH_SET64_CAPACITY;
        if (hs->slots[pos] == HASH_SET64_EMPTY) {
            hs->slots[pos] = val;
            hs->size++;
            return;
        }
        if (hs->slots[pos] == val) return;
    }
}

static inline void hash_set64_clear(hash_set64_t *hs) {
    for (int i = 0; i < HASH_SET64_CAPACITY; i++) hs->slots[i] = HASH_SET64_EMPTY;
    hs->size = 0;
}

// ═══════════════════════════════════════════════════════════════
//  RNG64 — 61-bit high-level generator with collision tracking
// ═══════════════════════════════════════════════════════════════

typedef struct {
    uint64_t master_seed;
    cascade_mode_t mode;
    assoc_core64_t assoc_core;
    mean_core64_t mean_core;
    fantasy_core64_t fantasy_core;
    hash_set64_t history;
    size_t inc_counter;
    size_t inc_max;
} prng64_t;

static inline void prng64_reseed(prng64_t *prng, uint64_t new_seed) {
    prng->master_seed = new_seed;
    uint64_t s1 = seed_cascade64_derive(new_seed, 0, prng->mode);
    uint64_t s2 = seed_cascade64_derive(new_seed, 1, prng->mode);
    uint64_t s3 = seed_cascade64_derive(new_seed, 2, prng->mode);
    assoc_core64_init(&prng->assoc_core, s1, prng->mode);
    mean_core64_init(&prng->mean_core, 0.5, s2, prng->mode);
    fantasy_core64_init(&prng->fantasy_core, s3, prng->mode);
}

static inline void prng64_init(prng64_t *prng, unsigned int seed, double period,
                                 cascade_mode_t mode) {
    prng->master_seed = seed;
    prng->mode = mode;
    uint64_t s1 = seed_cascade64_derive(seed, 0, mode);
    uint64_t s2 = seed_cascade64_derive(seed, 1, mode);
    uint64_t s3 = seed_cascade64_derive(seed, 2, mode);
    assoc_core64_init(&prng->assoc_core, s1, mode);
    mean_core64_init(&prng->mean_core, 0.5, s2, mode);
    fantasy_core64_init(&prng->fantasy_core, s3, mode);
    hash_set64_init(&prng->history);
    prng->inc_counter = 0;

    rotation_calculator_t rc;
    rotation_calculator_init(&rc, period);
    prng->inc_max = (size_t)round((rotation_calculator_get_period(&rc) * 365.25 * 24 * 3600) / 8.0);
}

static inline int prng64_is_collision(prng64_t *prng, uint64_t value) {
    if (hash_set64_contains(&prng->history, value)) return 1;
    hash_set64_insert(&prng->history, value);
    return 0;
}

static inline double prng64_generate(prng64_t *prng) {
    if (prng->inc_counter >= prng->inc_max) {
        uint64_t fresh_seed = assoc_core64_state_seed(&prng->assoc_core);
        prng64_reseed(prng, fresh_seed ^ prng->master_seed);
        prng->inc_counter = 0;
    }

    uint64_t base_raw = assoc_core64_generate_raw(&prng->assoc_core);
    double base = (double)base_raw / (double)CASCADE_PRNG64_M;

    int retries = 0;
    uint64_t previous_raw = base_raw;

    while (prng64_is_collision(prng, base_raw) && retries < RNG_MAX_RETRIES) {
        previous_raw = base_raw;
        base_raw = assoc_core64_generate_raw(&prng->assoc_core);
        if (base_raw == previous_raw) {
            fantasy_core64_add_collision(&prng->fantasy_core, previous_raw, base_raw);
        }
        retries++;
    }

    if (retries >= RNG_MAX_RETRIES) {
        uint64_t fresh_seed = assoc_core64_state_seed(&prng->assoc_core);
        prng64_reseed(prng, fresh_seed);
        base_raw = assoc_core64_generate_raw(&prng->assoc_core);
        base = (double)base_raw / (double)CASCADE_PRNG64_M;
        hash_set64_clear(&prng->history);
        fantasy_core64_clear(&prng->fantasy_core);
    }

    if (prng->history.size >= RNG_MAX_HISTORY) {
        hash_set64_clear(&prng->history);
        fantasy_core64_clear(&prng->fantasy_core);
    }

    double mean_adjusted = mean_core64_adjust(&prng->mean_core, base);
    double current = fantasy_core64_apply(&prng->fantasy_core, mean_adjusted);
    prng->inc_counter += 8;
    return current;
}

static inline long double prng64_generate_ld(prng64_t *prng) {
    if (prng->inc_counter >= prng->inc_max) {
        uint64_t fresh_seed = assoc_core64_state_seed(&prng->assoc_core);
        prng64_reseed(prng, fresh_seed ^ prng->master_seed);
        prng->inc_counter = 0;
    }

    uint64_t base_raw = assoc_core64_generate_raw(&prng->assoc_core);
    long double base = (long double)base_raw / (long double)CASCADE_PRNG64_M;

    int retries = 0;
    uint64_t previous_raw = base_raw;

    while (prng64_is_collision(prng, base_raw) && retries < RNG_MAX_RETRIES) {
        previous_raw = base_raw;
        base_raw = assoc_core64_generate_raw(&prng->assoc_core);
        if (base_raw == previous_raw) {
            fantasy_core64_add_collision(&prng->fantasy_core, previous_raw, base_raw);
        }
        retries++;
    }

    if (retries >= RNG_MAX_RETRIES) {
        uint64_t fresh_seed = assoc_core64_state_seed(&prng->assoc_core);
        prng64_reseed(prng, fresh_seed);
        base_raw = assoc_core64_generate_raw(&prng->assoc_core);
        base = (long double)base_raw / (long double)CASCADE_PRNG64_M;
        hash_set64_clear(&prng->history);
        fantasy_core64_clear(&prng->fantasy_core);
    }

    if (prng->history.size >= RNG_MAX_HISTORY) {
        hash_set64_clear(&prng->history);
        fantasy_core64_clear(&prng->fantasy_core);
    }

    long double mean_adjusted = mean_core64_adjust_ld(&prng->mean_core, base);
    long double current = fantasy_core64_apply_ld(&prng->fantasy_core, mean_adjusted);
    prng->inc_counter += 8;
    return current;
}

static inline void prng64_reset(prng64_t *prng) {
    prng64_reseed(prng, prng->master_seed + 1);
    hash_set64_clear(&prng->history);
    fantasy_core64_clear(&prng->fantasy_core);
    prng->inc_counter = 0;
}

// ═══════════════════════════════════════════════════════════════
//  Period detection (optional, slow)
// ═══════════════════════════════════════════════════════════════

static inline size_t cascade_prng_get_period(cascade_prng *probe) {
    uint64_t init_a = probe->a, init_b = probe->b, init_c = probe->c;
    int init_counter = probe->counter, init_out = probe->out_idx;
    const size_t LIMIT = 10000000;
    for (size_t i = 0; i < LIMIT; i++) {
        cascade_prng_step(probe);
        if (probe->a == init_a && probe->b == init_b && probe->c == init_c &&
            probe->counter == init_counter && probe->out_idx == init_out)
            return i + 1;
    }
    return (size_t)-1;
}

static inline size_t cascade_prng64_get_period(cascade_prng64 *probe) {
    uint64_t init_a = probe->a, init_b = probe->b, init_c = probe->c;
    int init_counter = probe->counter, init_out = probe->out_idx;
    const size_t LIMIT = 10000000;
    for (size_t i = 0; i < LIMIT; i++) {
        cascade_prng64_step(probe);
        if (probe->a == init_a && probe->b == init_b && probe->c == init_c &&
            probe->counter == init_counter && probe->out_idx == init_out)
            return i + 1;
    }
    return (size_t)-1;
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // PERFECT_RANDOM_H
