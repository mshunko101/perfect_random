/* perfect_random.h
// MSHUNKO 2026 © Шунько Михаил Геннадьевич.
 * Полностью автономный ГПСЧ на CascadePRNG — никаких внешних зависимостей
 * C-версия
 */
#ifndef PERFECT_RANDOM_H
#define PERFECT_RANDOM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ═══════════════════════════════════════════════════════════════
 *  CascadePRNG — каскадный ГПСЧ на простом числе Мерсенна M31
 * ═══════════════════════════════════════════════════════════════ */

#define CPRNG_M     2147483647ULL   /* 2^31 - 1 */
#define CPRNG_MUL_A  954437177ULL    /* 5/9 mod M */
#define CPRNG_MUL_B  715827884ULL    /* 5/3 mod M */

typedef struct {
    uint64_t a, b, c;
    int      counter;
    int      out_idx;
    size_t   N;
} CascadePRNG;

static void cprng_seed(CascadePRNG *r, uint64_t s) {
    if (s == 0) s = 1;
    r->a = (s * CPRNG_MUL_A) % CPRNG_M;
    if (r->a == 0) r->a = 1;
    r->b = (s * CPRNG_MUL_B) % CPRNG_M;
    if (r->b == 0) r->b = 1;
    r->c = (s * s + 1) % CPRNG_M;
    if (r->c == 0) r->c = 1;
    r->counter = 0;
    r->out_idx = 0;
}

static void cprng_init(CascadePRNG *r, uint64_t s, size_t n) {
    r->N = (n < 1) ? 1 : n;
    r->counter = 0;
    r->out_idx = 0;
    cprng_seed(r, s);
}

static void cprng_step(CascadePRNG *r) {
    r->a = (r->a * CPRNG_MUL_A) % CPRNG_M;
    r->b = (r->b * CPRNG_MUL_B) % CPRNG_M;
    r->c = (r->c * r->c + r->a + r->b) % CPRNG_M;
    if (++r->counter >= (int)r->N) {
        uint64_t tmp = r->a;
        r->a = r->c;
        r->c = r->b;
        r->b = tmp;
        r->counter = 0;
        r->out_idx = (r->out_idx + 1) % 3;
    }
}

static uint32_t cprng_current_value(const CascadePRNG *r) {
    return (uint32_t)((r->a * r->b + r->c) % CPRNG_M);
}

static double cprng_generate(CascadePRNG *r) {
    cprng_step(r);
    return (double)cprng_current_value(r) / (double)CPRNG_M;
}

static uint32_t cprng_generate_raw(CascadePRNG *r) {
    cprng_step(r);
    return cprng_current_value(r);
}

static uint64_t cprng_get_a(const CascadePRNG *r) { return r->a; }
static uint64_t cprng_get_b(const CascadePRNG *r) { return r->b; }
static uint64_t cprng_get_c(const CascadePRNG *r) { return r->c; }
static int      cprng_get_counter(const CascadePRNG *r) { return r->counter; }
static int      cprng_get_out_idx(const CascadePRNG *r) { return r->out_idx; }

static size_t cprng_get_period(CascadePRNG *r) {
    CascadePRNG probe;
    cprng_init(&probe, 7, r->N);
    uint64_t init_a = probe.a;
    uint64_t init_b = probe.b;
    uint64_t init_c = probe.c;
    int      init_counter = probe.counter;
    int      init_out = probe.out_idx;

    const size_t LIMIT = 10000000;
    for (size_t i = 0; i < LIMIT; i++) {
        cprng_step(&probe);
        if (probe.a == init_a &&
            probe.b == init_b &&
            probe.c == init_c &&
            probe.counter == init_counter &&
            probe.out_idx == init_out)
        {
            return i + 1;
        }
    }
    return SIZE_MAX;
}

/* ═══════════════════════════════════════════════════════════════
 *  Геометрия — расчёт периода вращения
 * ═══════════════════════════════════════════════════════════════ */

typedef struct {
    double x, y, z;
} Point;

static Point point_make(double x, double y, double z) {
    Point p = { x, y, z };
    return p;
}

typedef struct {
    Point A, B, O_okr;
    double time_AB;
} RotationCalculator;

static Point vec_between(Point p1, Point p2) {
    return point_make(p1.x - p2.x, p1.y - p2.y, p1.z - p2.z);
}

static double dot_product(Point v1, Point v2) {
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

static double vec_length(Point v) {
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

static Point calc_OA(const RotationCalculator *rc) { return vec_between(rc->A, rc->O_okr); }
static Point calc_OB(const RotationCalculator *rc) { return vec_between(rc->B, rc->O_okr); }

static double calc_angle(const RotationCalculator *rc) {
    Point OA = calc_OA(rc);
    Point OB = calc_OB(rc);
    double cos_angle = dot_product(OA, OB) / (vec_length(OA) * vec_length(OB));
    return acos(cos_angle);
}

static double calc_omega(const RotationCalculator *rc) {
    return calc_angle(rc) / rc->time_AB;
}

static void rc_init_full(RotationCalculator *rc, Point a, Point b, Point o, double t) {
    rc->A = a;
    rc->B = b;
    rc->O_okr = o;
    rc->time_AB = t;
}

static void rc_init_default(RotationCalculator *rc, double time) {
    rc->A = point_make(-3.0, -0.5, -2.5);
    rc->B = point_make(-5.0 / 3.0, -5.0 / 6.0, 25.0 / 4.0);
    rc->O_okr = point_make(-2.64, -7.91, 1.65);
    rc->time_AB = time;
}

static double rc_get_period(const RotationCalculator *rc) {
    return 2.0 * M_PI / calc_omega(rc);
}

static void rc_show_results(const RotationCalculator *rc) {
    Point OA = calc_OA(rc);
    Point OB = calc_OB(rc);
    printf("Результаты расчёта:\n");
    printf("Длина вектора OA: %.2f\n", vec_length(OA));
    printf("Длина вектора OB: %.2f\n", vec_length(OB));
    printf("Угол между векторами (рад): %.2f\n", calc_angle(rc));
    printf("Угловая скорость (рад/год): %.2f\n", calc_omega(rc));
    printf("Период вращения (лет): %.2f\n", rc_get_period(rc));
}

/* ═══════════════════════════════════════════════════════════════
 *  Каскадное порождение под-зёрен
 * ═══════════════════════════════════════════════════════════════ */

#define SEED_DELTA 0x9E3779B97F4A7C15ULL

static uint64_t seed_cascade_derive(uint64_t master_seed, int index) {
    CascadePRNG factory;
    cprng_init(&factory, master_seed + (uint64_t)index * SEED_DELTA, 49);
    return ((uint64_t)cprng_generate_raw(&factory) << 32) | cprng_generate_raw(&factory);
}

/* ═══════════════════════════════════════════════════════════════
 *  Ядро 1 — ассоциативность
 * ═══════════════════════════════════════════════════════════════ */

typedef struct {
    CascadePRNG rng;
} AssociativityCore;

static void ac_init(AssociativityCore *ac, uint64_t s) {
    cprng_init(&ac->rng, s, 48);
}

static double ac_generate(AssociativityCore *ac) {
    return cprng_generate(&ac->rng);
}

static uint32_t ac_generate_raw(AssociativityCore *ac) {
    return cprng_generate_raw(&ac->rng);
}

static uint64_t ac_state_seed(AssociativityCore *ac) {
    uint64_t a = cprng_get_a(&ac->rng);
    uint64_t b = cprng_get_b(&ac->rng);
    uint64_t c = cprng_get_c(&ac->rng);
    return (a << 31) ^ (b << 13) ^ c ^
           ((uint64_t)cprng_get_counter(&ac->rng) * 0x100000001B3ULL);
}

/* ═══════════════════════════════════════════════════════════════
 *  Ядро 2 — среднее значение
 * ═══════════════════════════════════════════════════════════════ */

typedef struct {
    double mean;
    CascadePRNG rng;
} MeanCore;

static void mc_init(MeanCore *mc, double m, uint64_t s) {
    mc->mean = m;
    cprng_init(&mc->rng, s, 49);
}

static double mc_adjust(MeanCore *mc, double base) {
    return (base / (double)CPRNG_M) * mc->mean + cprng_generate(&mc->rng);
}

/* ═══════════════════════════════════════════════════════════════
 *  Ядро 3 — фантазия
 * ═══════════════════════════════════════════════════════════════ */

typedef struct {
    int vals[2];  /* [0] и [1] */
} Dimension;

typedef struct {
    CascadePRNG rng;
    Dimension *dims;      /* динамический массив */
    size_t     dim_count;
    size_t     dim_cap;
} FantasyCore;

static void fc_init(FantasyCore *fc, uint64_t s) {
    cprng_init(&fc->rng, s, 50);
    fc->dim_count = 0;
    fc->dim_cap   = 16;
    fc->dims      = (Dimension *)malloc(fc->dim_cap * sizeof(Dimension));
}

static void fc_free(FantasyCore *fc) {
    free(fc->dims);
    fc->dims = NULL;
    fc->dim_count = 0;
    fc->dim_cap = 0;
}

static void fc_clear_dims(FantasyCore *fc) {
    fc->dim_count = 0;
}

static int fc_dim_equals(const Dimension *d, int v0, int v1) {
    return d->vals[0] == v0 && d->vals[1] == v1;
}

static void fc_push_dim(FantasyCore *fc, int v0, int v1) {
    /* проверка на дубликат */
    for (size_t i = 0; i < fc->dim_count; i++) {
        if (fc_dim_equals(&fc->dims[i], v0, v1))
            return;
    }
    if (fc->dim_count >= fc->dim_cap) {
        fc->dim_cap *= 2;
        fc->dims = (Dimension *)realloc(fc->dims, fc->dim_cap * sizeof(Dimension));
    }
    fc->dims[fc->dim_count].vals[0] = v0;
    fc->dims[fc->dim_count].vals[1] = v1;
    fc->dim_count++;
}

static void fc_add_collision(FantasyCore *fc, uint32_t a, uint32_t b) {
    if (a == b) return;
    for (int i = 0; i < 32; i++) {
        int bit_a = (a >> i) & 1;
        int bit_b = (b >> i) & 1;
        if (bit_a != bit_b) {
            int v0 = bit_a ? 1 : 0;
            int v1 = bit_b ? 0 : 1;
            fc_push_dim(fc, v0, v1);
        }
    }
}

static double fc_apply_fantasy(FantasyCore *fc, double base) {
    for (size_t i = 0; i < fc->dim_count; i++) {
        if (cprng_generate(&fc->rng) == 0.0) {
            base += fc->dims[i].vals[0] * 0.01;
        } else {
            base += fc->dims[i].vals[1] * 0.01;
        }
    }
    return base;
}

/* ═══════════════════════════════════════════════════════════════
 *  Простая хеш-таблица для history (замена unordered_set<uint32_t>)
 * ═══════════════════════════════════════════════════════════════ */

#define HASH_INITIAL_CAP 128

typedef struct {
    uint32_t *keys;
    uint8_t  *used;     /* 0 — свободен, 1 — занят */
    size_t    count;
    size_t    cap;
} HashSet32;

static void hs_init(HashSet32 *hs) {
    hs->cap   = HASH_INITIAL_CAP;
    hs->count = 0;
    hs->keys  = (uint32_t *)calloc(hs->cap, sizeof(uint32_t));
    hs->used  = (uint8_t  *)calloc(hs->cap, sizeof(uint8_t));
}

static void hs_free(HashSet32 *hs) {
    free(hs->keys);
    free(hs->used);
    hs->keys = NULL;
    hs->used = NULL;
    hs->count = 0;
    hs->cap = 0;
}

static void hs_clear(HashSet32 *hs) {
    memset(hs->used, 0, hs->cap * sizeof(uint8_t));
    hs->count = 0;
}

static size_t hs_hash(uint32_t key, size_t cap) {
    /* простое мультипликативное хеширование */
    return (size_t)((key * 2654435761u) % cap);
}

static void hs_resize(HashSet32 *hs) {
    size_t new_cap = hs->cap * 2;
    uint32_t *new_keys = (uint32_t *)calloc(new_cap, sizeof(uint32_t));
    uint8_t  *new_used = (uint8_t  *)calloc(new_cap, sizeof(uint8_t));
    for (size_t i = 0; i < hs->cap; i++) {
        if (hs->used[i]) {
            size_t idx = hs_hash(hs->keys[i], new_cap);
            while (new_used[idx]) idx = (idx + 1) % new_cap;
            new_keys[idx] = hs->keys[i];
            new_used[idx] = 1;
        }
    }
    free(hs->keys);
    free(hs->used);
    hs->keys = new_keys;
    hs->used = new_used;
    hs->cap  = new_cap;
}

/* Возвращает 1 если ключ уже есть, 0 если вставлен новый */
static int hs_insert(HashSet32 *hs, uint32_t key) {
    if ((hs->count + 1) * 2 > hs->cap)
        hs_resize(hs);
    size_t idx = hs_hash(key, hs->cap);
    while (hs->used[idx]) {
        if (hs->keys[idx] == key)
            return 1;  /* уже есть */
        idx = (idx + 1) % hs->cap;
    }
    hs->keys[idx] = key;
    hs->used[idx] = 1;
    hs->count++;
    return 0;  /* новый */
}

/* ═══════════════════════════════════════════════════════════════
 *  Главный класс RNG
 * ═══════════════════════════════════════════════════════════════ */

#define MAX_RETRIES     49
#define MAX_HISTORY_SZ  49

typedef struct {
    uint64_t          master_seed;
    AssociativityCore assoc_core;
    MeanCore          mean_core;
    FantasyCore       fantasy_core;
    HashSet32         history;
    size_t            inc_counter;
    size_t            inc_max;
} RNG;

static void rng_reseed(RNG *rng, uint64_t new_seed) {
    rng->master_seed = new_seed;
    uint64_t s1 = seed_cascade_derive(new_seed, 0);
    uint64_t s2 = seed_cascade_derive(new_seed, 1);
    uint64_t s3 = seed_cascade_derive(new_seed, 2);
    ac_init(&rng->assoc_core, s1);
    mc_init(&rng->mean_core, 0.5, s2);
    fc_clear_dims(&rng->fantasy_core);
    fc_init(&rng->fantasy_core, s3);
}

static void rng_init(RNG *rng, uint32_t seed, double period) {
    rng->master_seed = seed;
    uint64_t s1 = seed_cascade_derive(seed, 0);
    uint64_t s2 = seed_cascade_derive(seed, 1);
    uint64_t s3 = seed_cascade_derive(seed, 2);
    ac_init(&rng->assoc_core, s1);
    mc_init(&rng->mean_core, 0.5, s2);
    fc_init(&rng->fantasy_core, s3);
    hs_init(&rng->history);
    rng->inc_counter = 0;

    RotationCalculator rc;
    rc_init_default(&rc, period);
    rng->inc_max = (size_t)round((rc_get_period(&rc) * 365.25 * 24.0 * 3600.0) / 8.0);
}

static void rng_free(RNG *rng) {
    fc_free(&rng->fantasy_core);
    hs_free(&rng->history);
}

static size_t rng_get_period(RNG *rng) {
    return rng->inc_max;
}

static int rng_is_collision(RNG *rng, uint32_t value) {
    return hs_insert(&rng->history, value);
}

static void rng_handle_collision(RNG *rng, uint32_t a, uint32_t b) {
    fc_add_collision(&rng->fantasy_core, a, b);
}

static void rng_clear_history(RNG *rng) {
    hs_clear(&rng->history);
}

static size_t rng_get_history_size(const RNG *rng) {
    return rng->history.count;
}

static double rng_generate(RNG *rng) {
    if (rng->inc_counter >= rng->inc_max) {
        uint64_t fresh = ac_state_seed(&rng->assoc_core);
        rng_reseed(rng, fresh ^ rng->master_seed);
        rng->inc_counter = 0;
    }

    uint32_t base_raw = ac_generate_raw(&rng->assoc_core);
    double base = (double)base_raw / (double)CPRNG_M;

    int retries = 0;
    uint32_t prev_raw = base_raw;

    while (rng_is_collision(rng, base_raw) && retries < MAX_RETRIES) {
        prev_raw = base_raw;
        base_raw = ac_generate_raw(&rng->assoc_core);
        if (base_raw == prev_raw) {
            rng_handle_collision(rng, prev_raw, base_raw);
        }
        retries++;
    }

    if (retries >= MAX_RETRIES) {
        uint64_t fresh = ac_state_seed(&rng->assoc_core);
        rng_reseed(rng, fresh);
        base_raw = ac_generate_raw(&rng->assoc_core);
        base = (double)base_raw / (double)CPRNG_M;
        rng_clear_history(rng);
        fc_clear_dims(&rng->fantasy_core);
    }

    if (rng_get_history_size(rng) >= MAX_HISTORY_SZ) {
        rng_clear_history(rng);
        fc_clear_dims(&rng->fantasy_core);
    }

    double mean_adjusted = mc_adjust(&rng->mean_core, base);
    double current = fc_apply_fantasy(&rng->fantasy_core, mean_adjusted);
    rng->inc_counter += 8;
    return current;
}

static void rng_reset(RNG *rng) {
    rng_reseed(rng, rng->master_seed + 1);
    rng_clear_history(rng);
    fc_clear_dims(&rng->fantasy_core);
    rng->inc_counter = 0;
}

#endif /* PERFECT_RANDOM_H */
