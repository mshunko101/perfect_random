// bench_speed.cpp — Benchmark: perfect_random (C++ & C) vs std::mt19937_64
// MSHUNKO 2026
//
// g++ -std=c++17 -O3 -march=native -o bench_speed bench_speed.cpp
//   cl /std:c++17 /O2 bench_speed.cpp
//
//   ./bench_speed                      — 1 млрд чисел
//   ./bench_speed 5000000000           — 5 млрд чисел

#include "../../perfect_random.hpp"

// ── C-версия с переименованием типов ────────────────────────────
extern "C" {
#define RNG             CRNG
#define CascadePRNG     CCascadePRNG
#define Point            CPoint
#define RotationCalculator CRotationCalculator
#define AssociativityCore  CAssociativityCore
#define MeanCore         CMeanCore
#define FantasyCore      CFantasyCore
#define Dimension        CDimension
#define HashSet32        CHashSet32
#include "../../perfect_random_с.h"
#undef RNG
#undef CascadePRNG
#undef Point
#undef RotationCalculator
#undef AssociativityCore
#undef MeanCore
#undef FantasyCore
#undef Dimension
#undef HashSet32
#undef SEED_DELTA
#undef HASH_INITIAL_CAP
#undef MAX_RETRIES
#undef MAX_HISTORY_SZ
}

#include <chrono>
#include <cstdio>
#include <cstdint>
#include <random>
#include <string>
#include <vector>
#include <iomanip>
#include <iostream>
#include <algorithm>

// ── Конфигурация ────────────────────────────────────────────────
static constexpr double PERIOD = 73.8;
static constexpr uint32_t SEED = 12345;

// ── Результат ──────────────────────────────────────────────────
struct BenchResult {
    std::string name;
    size_t      count;
    double      seconds;
    double      ns_per_call;
    double      nums_per_sec;   // млн/сек
    double      gb_per_sec;     // ГБ/сек (double = 8 байт)
};

// ── Замер: прогрев + best of 3 ─────────────────────────────────
template<typename GenFn>
static BenchResult run_bench(const std::string& name, size_t N, GenFn fn) {
    // Прогрев — 1% от N
    size_t warmup = std::max(N / 100, (size_t)100000);
    volatile double warmup_sink = 0.0;
    for (size_t i = 0; i < warmup; ++i)
        warmup_sink += fn();

    double best_time = 1e18;

    for (int run = 0; run < 3; ++run) {
        volatile double sink = 0.0;
        auto t0 = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < N; ++i) {
            sink += fn();
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> dt = t1 - t0;
        if (dt.count() < best_time)
            best_time = dt.count();
    }

    return {
        name, N, best_time,
        best_time / N * 1e9,           // ns/call
        N / best_time / 1e6,           // млн/сек
        N * 8.0 / best_time / 1e9      // ГБ/сек
    };
}

// ── Компилятор ──────────────────────────────────────────────────
static const char* compiler_name() {
#if defined(__clang__)
    return "Clang";
#elif defined(__GNUC__)
    return "GCC";
#elif defined(_MSC_VER)
    return "MSVC";
#else
    return "Unknown";
#endif
}

// ── Main ────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    size_t N = 1'000'000'000;
    if (argc > 1) N = std::stoull(argv[1]);

    std::cout << "=============================================\n";
    std::cout << "  Benchmark: perfect_random vs std::mt19937_64\n";
    std::cout << "  Compiler: " << compiler_name();
#if defined(__VERSION__)
    std::cout << " (" << __VERSION__ << ")";
#endif
    std::cout << "\n  Samples: " << N << "\n";
    std::cout << "  Runs: 3 (best of), warmup: "
        << std::max(N / 100, (size_t)100000) << "\n";
    std::cout << "=============================================\n\n";

    // ── Генераторы ──
    RNG cpp_rng(SEED, PERIOD);
    auto cpp_fn = [&cpp_rng]() { return cpp_rng.generate(); };

    CRNG c_rng;
    rng_init(&c_rng, SEED, PERIOD);
    auto c_fn = [&c_rng]() { return rng_generate(&c_rng); };

    std::mt19937_64 mt(SEED);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    auto mt_fn = [&mt, &dist]() { return dist(mt); };

    auto mt_raw_fn = [&mt]() {
        return (double)(mt() >> 11) / (double)(1ULL << 53);
        };

    // ── Прогоны ──
    std::vector<BenchResult> results;

    std::cout << "Running...\n";
    std::cout << "  [1/4] perfect_random C++ ...  " << std::flush;
    results.push_back(run_bench("perfect_random (C++)", N, cpp_fn));
    std::cout << "done\n";

    std::cout << "  [2/4] perfect_random C   ...  " << std::flush;
    results.push_back(run_bench("perfect_random (C)", N, c_fn));
    std::cout << "done\n";

    std::cout << "  [3/4] mt19937_64 + dist  ...  " << std::flush;
    results.push_back(run_bench("mt19937_64 + dist", N, mt_fn));
    std::cout << "done\n";

    std::cout << "  [4/4] mt19937_64 raw     ...  " << std::flush;
    results.push_back(run_bench("mt19937_64 raw", N, mt_raw_fn));
    std::cout << "done\n\n";

    rng_free(&c_rng);

    // ── Таблица ──
    std::cout << "=============================================\n";
    std::cout << "  Results\n";
    std::cout << "=============================================\n\n";

    std::cout << "  " << std::left << std::setw(28) << "Generator"
        << std::right
        << std::setw(12) << "ns/call"
        << std::setw(14) << "M nums/sec"
        << std::setw(12) << "GB/sec"
        << "\n";
    std::cout << "  " << std::string(66, '-') << "\n";

    for (const auto& r : results) {
        std::cout << "  " << std::left << std::setw(28) << r.name
            << std::right
            << std::setw(12) << std::fixed << std::setprecision(2) << r.ns_per_call
            << std::setw(14) << std::fixed << std::setprecision(1) << r.nums_per_sec
            << std::setw(12) << std::fixed << std::setprecision(2) << r.gb_per_sec
            << "\n";
    }

    // ── Сравнение ──
    double baseline = results[2].nums_per_sec;

    std::cout << "\n=============================================\n";
    std::cout << "  Relative to mt19937_64 + dist\n";
    std::cout << "=============================================\n\n";

    for (const auto& r : results) {
        double ratio = r.nums_per_sec / baseline;
        std::cout << "  " << std::left << std::setw(28) << r.name
            << std::right << std::setw(8)
            << std::fixed << std::setprecision(2) << ratio << "x\n";
    }

    // ── Markdown ──
    std::cout << "\n=============================================\n";
    std::cout << "  Markdown (paste into README)\n";
    std::cout << "=============================================\n\n";

    std::cout << "| Generator | ns/call | M nums/sec | GB/sec | vs mt19937 |\n";
    std::cout << "|-----------|---------|-------------|--------|------------|\n";
    for (const auto& r : results) {
        double ratio = r.nums_per_sec / baseline;
        std::cout << "| " << r.name
            << " | " << std::fixed << std::setprecision(2) << r.ns_per_call
            << " | " << std::fixed << std::setprecision(1) << r.nums_per_sec
            << " | " << std::fixed << std::setprecision(2) << r.gb_per_sec
            << " | " << std::fixed << std::setprecision(2) << ratio << "x |"
            << "\n";
    }
    std::cout << "\n";

    return 0;
}
