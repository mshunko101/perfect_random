// cascade_test.cpp
#include "../../perfect_random.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <string>
#include <functional>

// ── Тестовый фреймворк ────────────────────────────────────────────
static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

// Эмпирическая константа периода на 2026 год
static constexpr double PERIOD = 73.8;

struct TestResult {
    bool        passed;
    std::string name;
    std::string detail;
};

#define ASSERT_TRUE(cond, msg) \
    if (!(cond)) return {false, name, msg}

#define ASSERT_EQ(a, b, msg) \
    if ((a) != (b)) return {false, name, msg}

#define ASSERT_NEAR(a, b, eps, msg) \
    if (std::abs((a) - (b)) > (eps)) return {false, name, msg}

#define RUN_TEST(test_func)                                          \
    do {                                                             \
        g_tests_run++;                                               \
        TestResult r = test_func(#test_func);                        \
        if (r.passed) {                                              \
            g_tests_passed++;                                        \
            std::cout << "  [PASS] " << r.name << "\n";              \
        } else {                                                     \
            g_tests_failed++;                                        \
            std::cout << "  [FAIL] " << r.name                       \
                      << " — " << r.detail << "\n";                  \
        }                                                            \
    } while (0)

// ── Тест 1: Воспроизводимость ────────────────────────────────────
TestResult test_reproducibility(const std::string& name) {
    RNG g1(42, PERIOD);
    RNG g2(42, PERIOD);

    for (int i = 0; i < 10000; i++) {
        double v1 = g1.generate();
        double v2 = g2.generate();
        ASSERT_EQ(v1, v2, "sequences diverged at step " + std::to_string(i));
    }
    return { true, name, "" };
}

// ── Тест 2: Разный seed → разный результат ───────────────────────
TestResult test_seed_sensitivity(const std::string& name) {
    RNG g1(1000, PERIOD);
    RNG g2(1001, PERIOD);

    int diff = 0;
    for (int i = 0; i < 100; i++) {
        if (g1.generate() != g2.generate())
            diff++;
    }
    ASSERT_TRUE(diff >= 95,
        "only " + std::to_string(diff) + "/100 differ (expected >= 95)");
    return { true, name, "" };
}

// ── Тест 3: Seed=0 не должен крашить ────────────────────────────
TestResult test_seed_zero(const std::string& name) {
    RNG g(0, PERIOD);
    double v = g.generate();
    ASSERT_TRUE(v >= 0.0 && v < 1.0,
        "value out of range [0,1) for seed=0");
    return { true, name, "" };
}

// ── Тест 4: Значения в диапазоне [0, 1) ─────────────────────────
TestResult test_range(const std::string& name) {
    RNG g(999, PERIOD);
    for (int i = 0; i < 100000; i++) {
        double v = g.generate();
        ASSERT_TRUE(v >= 0.0 && v < 1.0,
            "value " + std::to_string(v) + " out of [0,1)");
    }
    return { true, name, "" };
}

// ── Тест 5: Среднее ~0.5 ─────────────────────────────────────────
TestResult test_mean(const std::string& name) {
    RNG g(12345, PERIOD);
    const int N = 100000;
    double sum = 0.0;
    for (int i = 0; i < N; i++)
        sum += g.generate();
    double mean = sum / N;
    ASSERT_NEAR(mean, 0.5, 0.01,
        "mean = " + std::to_string(mean) + " (expected ~0.5)");
    return { true, name, "" };
}

// ── Тест 6: Стандартное отклонение ~0.2887 ───────────────────────
TestResult test_stddev(const std::string& name) {
    RNG g(777, PERIOD);
    const int N = 100000;
    std::vector<double> vals;
    vals.reserve(N);
    double sum = 0.0;
    for (int i = 0; i < N; i++) {
        double v = g.generate();
        vals.push_back(v);
        sum += v;
    }
    double mean = sum / N;
    double sq = 0.0;
    for (double v : vals)
        sq += (v - mean) * (v - mean);
    double stddev = std::sqrt(sq / N);
    ASSERT_NEAR(stddev, 0.2887, 0.01,
        "stddev = " + std::to_string(stddev) + " (expected ~0.2887)");
    return { true, name, "" };
}

// ── Тест 7: Chi-square (10 корзин) ──────────────────────────────
TestResult test_chi_square(const std::string& name) {
    RNG g(555, PERIOD);
    const int N = 100000;
    int bins[10] = { 0 };
    for (int i = 0; i < N; i++) {
        double v = g.generate();
        int b = (int)(v * 10.0);
        if (b > 9) b = 9;
        bins[b]++;
    }
    double expected = N / 10.0;
    double chi2 = 0.0;
    for (int i = 0; i < 10; i++) {
        double diff = bins[i] - expected;
        chi2 += diff * diff / expected;
    }
    ASSERT_TRUE(chi2 < 16.92,
        "chi2 = " + std::to_string(chi2) + " (critical 16.92)");
    return { true, name, "" };
}

// ── Тест 8: Автокорреляция ───────────────────────────────────────
TestResult test_autocorrelation(const std::string& name) {
    RNG g(314, PERIOD);
    const int N = 100000;
    std::vector<double> vals;
    vals.reserve(N);
    for (int i = 0; i < N; i++)
        vals.push_back(g.generate());

    for (int lag = 1; lag <= 5; lag++) {
        double ac = 0.0;
        for (int i = lag; i < N; i++)
            ac += vals[i] * vals[i - lag];
        ac /= (N - lag);
        ASSERT_TRUE(ac > 0.20 && ac < 0.30,
            "AC lag-" + std::to_string(lag) + " = " + std::to_string(ac) +
            " (expected ~0.25)");
    }
    return { true, name, "" };
}

// ── Тест 9: Монобит (частота 0/1) ───────────────────────────────
TestResult test_monobit(const std::string& name) {
    RNG g(2024, PERIOD);
    const int N = 10000;
    int ones = 0;
    int total_bits = 0;
    for (int i = 0; i < N; i++) {
        double v = g.generate();
        uint32_t bits = (uint32_t)(v * 2147483647.0);
        for (int b = 0; b < 31; b++) {
            if (bits & (1u << b)) ones++;
            total_bits++;
        }
    }
    double ratio = (double)ones / total_bits;
    ASSERT_NEAR(ratio, 0.5, 0.01,
        "bit ratio = " + std::to_string(ratio) + " (expected ~0.5)");
    return { true, name, "" };
}

// ── Тест 10: Runs test (серии) ───────────────────────────────────
TestResult test_runs(const std::string& name) {
    RNG g(888, PERIOD);
    const int N = 100000;
    int runs = 1;
    double prev = g.generate();
    for (int i = 1; i < N; i++) {
        double v = g.generate();
        if ((v > 0.5) != (prev > 0.5))
            runs++;
        prev = v;
    }
    double expected_runs = N / 2.0;
    double z = (runs - expected_runs) / std::sqrt(N / 4.0);
    ASSERT_TRUE(std::abs(z) < 3.09,
        "z = " + std::to_string(z) + " (|z| < 3.09 required)");
    return { true, name, "" };
}

// ── Тест 11: inc_max (период пересева) ──────────────────────────
TestResult test_period_value(const std::string& name) {
    RNG g(42, PERIOD);
    size_t period = g.get_period();
    ASSERT_TRUE(period > 0,
        "inc_max = 0 (expected > 0)");
    std::cout << "(inc_max = " << period << ") ";
    return { true, name, "" };
}

// ── Тест 12: Разный period → разный inc_max ─────────────────────
TestResult test_period_sensitivity(const std::string& name) {
    RNG g1(42, PERIOD);
    RNG g2(42, 42.0);

    size_t p1 = g1.get_period();
    size_t p2 = g2.get_period();
    ASSERT_TRUE(p1 != p2,
        "same inc_max for different periods (" +
        std::to_string(p1) + " vs " + std::to_string(p2) + ")");
    return { true, name, "" };
}

// ── Тест 13: Пересев → расхождение ─────────────────────────────
TestResult test_period_affects_sequence(const std::string& name) {
    // Крошечный период → inc_max ≈ 2 → пересев на 3-м вызове
    RNG g1(42, 0.0000001);
    RNG g2(42, 0.0000001);

    g1.generate();
    g1.generate();
    g1.generate();  // пересев

    g2.generate();  // без пересева

    int diff = 0;
    for (int i = 0; i < 100; i++) {
        if (g1.generate() != g2.generate())
            diff++;
    }
    ASSERT_TRUE(diff >= 50,
        "only " + std::to_string(diff) + "/100 differ (reseed should cause divergence)");
    return { true, name, "" };
}

// ── Тест 14: Первый элемент уникален для разных seed ───────────
TestResult test_seed_uniqueness(const std::string& name) {
    int diff = 0;
    for (unsigned int s = 1; s <= 100; s++) {
        RNG g1(s, PERIOD);
        RNG g2(s + 1, PERIOD);
        if (g1.generate() != g2.generate())
            diff++;
    }
    ASSERT_TRUE(diff >= 95,
        "only " + std::to_string(diff) + "/100 first values differ");
    return { true, name, "" };
}

// ── Тест 15: Большая серия в диапазоне [0,1) ───────────────────
TestResult test_large_batch(const std::string& name) {
    RNG g(31337, PERIOD);
    double min_v = 1.0, max_v = 0.0;
    for (int i = 0; i < 1000000; i++) {
        double v = g.generate();
        ASSERT_TRUE(v >= 0.0 && v < 1.0,
            "value " + std::to_string(v) + " out of [0,1) at step " + std::to_string(i));
        if (v < min_v) min_v = v;
        if (v > max_v) max_v = v;
    }
    ASSERT_TRUE(max_v - min_v > 0.9,
        "range too narrow: [" + std::to_string(min_v) + ", " + std::to_string(max_v) + "]");
    return { true, name, "" };
}

// ── Тест 16: reset() меняет последовательность ─────────────────
TestResult test_reset(const std::string& name) {
    RNG g(42, PERIOD);

    std::vector<double> before;
    for (int i = 0; i < 50; i++)
        before.push_back(g.generate());

    g.reset();

    int diff = 0;
    for (int i = 0; i < 50; i++) {
        double v = g.generate();
        if (v != before[i]) diff++;
    }
    ASSERT_TRUE(diff >= 40,
        "only " + std::to_string(diff) + "/50 differ after reset");
    return { true, name, "" };
}



// ── Main ─────────────────────────────────────────────────────────
int main() {
    std::setlocale(0, "ru-ru");
    std::cout << "===========================================\n";
    std::cout << "  RNG Test Suite (CascadePRNG)\n";
    std::cout << "  period = " << PERIOD << " (2026)\n";
    std::cout << "===========================================\n\n";

    std::cout << "--- Basic ---\n";
    RUN_TEST(test_reproducibility);
    RUN_TEST(test_seed_sensitivity);
    RUN_TEST(test_seed_zero);
    RUN_TEST(test_range);

    std::cout << "\n--- Statistical ---\n";
    RUN_TEST(test_mean);
    RUN_TEST(test_stddev);
    RUN_TEST(test_chi_square);
    RUN_TEST(test_autocorrelation);
    RUN_TEST(test_monobit);
    RUN_TEST(test_runs);

    std::cout << "\n--- Structural ---\n";
    RUN_TEST(test_period_value);
    RUN_TEST(test_period_sensitivity);
    RUN_TEST(test_period_affects_sequence);
    RUN_TEST(test_seed_uniqueness);
    RUN_TEST(test_large_batch);
    RUN_TEST(test_reset);

    std::cout << "\n===========================================\n";
    std::cout << "  Total:  " << g_tests_run << "\n";
    std::cout << "  Passed: " << g_tests_passed << "\n";
    std::cout << "  Failed: " << g_tests_failed << "\n";
    std::cout << "===========================================\n";

    return g_tests_failed > 0 ? 1 : 0;
}
