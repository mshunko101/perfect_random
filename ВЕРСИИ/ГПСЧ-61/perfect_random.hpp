// perfect_random_mc.hpp
// MSHUNKO 2026
// CascadePRNG64 — каскадный ГПСЧ на простом числе Мерсенна M61
// Версия для высокоточного Монте-Карло: 61-битный выход, ~183 бита состояния
// Разрешение хвостов: до P ~ 10^-18 (vs 10^-9 у 31-битной версии)
//
// Зависимости: __uint128_t (GCC/Clang на 64-битных платформах)
#ifndef PERFECT_RANDOM_MC_HPP
#define PERFECT_RANDOM_MC_HPP

#include <cstdint>
#include <cmath>

// ═══════════════════════════════════════════════════════════════
//  SplitMix64 — независимое хеширование для инициализации
// ═══════════════════════════════════════════════════════════════

static inline uint64_t splitmix64(uint64_t& state) {
    uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

// ═══════════════════════════════════════════════════════════════
//  CascadePRNG64 — каскадный ГПСЧ на Mersenne M61
//  M = 2^61 - 1, выход 61 бит, состояние ~183 бита
// ═══════════════════════════════════════════════════════════════

class CascadePRNG64 {
public:
    static constexpr uint64_t M = 2305843009213693951ULL;   // 2^61 - 1
    static constexpr int      OUT_BITS = 61;

private:
    // Множители: 5/9 mod M и 5/3 mod M (те же отношения, другой модуль)
    static constexpr uint64_t MUL_A = 0x0E38E38E38E38E39ULL; // 5/9 mod M
    static constexpr uint64_t MUL_B = 0x0AAAAAAAAAAAAAACULL; // 5/3 mod M

    uint64_t a, b, c;
    int      counter;
    int      out_idx;
    size_t   N;

    // Модульное умножение через __uint128_t (M < 2^61, произведение < 2^122)
    static inline uint64_t mulmod(uint64_t x, uint64_t y) {
        return (uint64_t)((__uint128_t)x * y % M);
    }

    void step() {
        a = mulmod(a, MUL_A);
        b = mulmod(b, MUL_B);
        c = (uint64_t)((__uint128_t)c * c + a + b) % M;
        if (++counter >= (int)N) {
            uint64_t tmp = a;
            a = c;
            c = b;
            b = tmp;
            counter = 0;
            out_idx = (out_idx + 1) % 3;
        }
    }

    uint64_t current_value() const {
        return (uint64_t)((__uint128_t)a * b % M + c) % M;
    }

public:
    CascadePRNG64(uint64_t s = 1, size_t n = 49)
        : counter(0), out_idx(0), N(n < 1 ? 1 : n)
    {
        seed(s);
    }

    // Независимая инициализация через SplitMix64 — три независимых хеша
    void seed(uint64_t s) {
        if (s == 0) s = 1;
        uint64_t sm = s;
        a = splitmix64(sm) % M;
        if (a == 0) a = 1;
        b = splitmix64(sm) % M;
        if (b == 0) b = 1;
        c = splitmix64(sm) % M;
        if (c == 0) c = 1;
        counter = 0;
        out_idx = 0;
    }

    // double в [0, 1) с 53-битной точностью (стандартный IEEE 754 mantissa)
    double generate() {
        step();
        uint64_t raw = current_value();
        // Старшие 53 бита 61-битного значения -> полная мантисса double
        return (double)(raw >> 8) * (1.0 / 9007199254740992.0); // / 2^53
    }

    // long double с 61-битной точностью (x87 80-bit, mantissa 64 бит)
    long double generate_ld() {
        step();
        uint64_t raw = current_value();
        return (long double)raw / (long double)M;
    }

    // 61-битное сырое значение
    uint64_t generate_raw() {
        step();
        return current_value();
    }

    // Полный 64-битный выход (два 61-битных вызова, склейка)
    uint64_t generate_u64() {
        uint64_t hi = generate_raw();   // 61 бит
        uint64_t lo = generate_raw();   // 61 бит
        return (hi << 3) | (lo & 0x7ULL); // 61 + 3 = 64 бита
    }

    // Два независимых double для 2D-выборки (без лишних вызовов step)
    void generate2(double& x1, double& x2) {
        step();
        uint64_t raw1 = current_value();
        step();
        uint64_t raw2 = current_value();
        x1 = (double)(raw1 >> 8) * (1.0 / 9007199254740992.0);
        x2 = (double)(raw2 >> 8) * (1.0 / 9007199254740992.0);
    }

    // Состояние
    uint64_t get_a() const { return a; }
    uint64_t get_b() const { return b; }
    uint64_t get_c() const { return c; }
    int      get_counter() const { return counter; }
    int      get_out_idx() const { return out_idx; }

    // Период (зонд, до LIMIT шагов)
    size_t get_period(size_t limit = 10000000) {
        CascadePRNG64 probe(7, N);
        uint64_t ia = probe.a, ib = probe.b, ic = probe.c;
        int icn = probe.counter, iout = probe.out_idx;
        for (size_t i = 0; i < limit; i++) {
            probe.step();
            if (probe.a == ia && probe.b == ib && probe.c == ic &&
                probe.counter == icn && probe.out_idx == iout)
                return i + 1;
        }
        return SIZE_MAX;
    }
};

// ═══════════════════════════════════════════════════════════════
//  SeedCascade64 — каскадное порождение под-зёрен для MC
// ═══════════════════════════════════════════════════════════════

struct SeedCascade64 {
    static constexpr uint64_t SEED_DELTA = 0x9E3779B97F4A7C15ULL;

    static uint64_t derive(uint64_t master_seed, int index) {
        CascadePRNG64 factory(master_seed + index * SEED_DELTA, 49);
        return factory.generate_u64();
    }
};

// ═══════════════════════════════════════════════════════════════
//  RNG64 — обёртка для высокоточного Монте-Карло
//  Простой интерфейс: seed + generate / generate_ld / generate_raw
// ═══════════════════════════════════════════════════════════════

class RNG64 {
private:
    CascadePRNG64 rng;

public:
    explicit RNG64(uint64_t seed = 1, size_t n = 49)
        : rng(seed, n) {}

    void seed(uint64_t s) { rng.seed(s); }

    // 53-битный double в [0, 1)
    double generate()       { return rng.generate(); }

    // 61-битный long double в [0, 1)
    long double generate_ld() { return rng.generate_ld(); }

    // 61-битное сырое значение
    uint64_t generate_raw()  { return rng.generate_raw(); }

    // Полный 64-битный выход
    uint64_t generate_u64()  { return rng.generate_u64(); }

    // Пара double для 2D-выборки
    void generate2(double& x1, double& x2) { rng.generate2(x1, x2); }

    // Прямой доступ к ядру
    const CascadePRNG64& core() const { return rng; }
};

#endif // PERFECT_RANDOM_MC_HPP
