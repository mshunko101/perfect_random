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
    RNG g1(42, 5.0);
    RNG g2(42, 5.0);

    for (int i = 0; i < 10000; i++) {
        double v1 = g1.generate(1);
        double v2 = g2.generate(1);
        ASSERT_EQ(v1, v2, "sequences diverged at step " + std::to_string(i));
    }
    return { true, name, "" };
}

// ── Тест 2: Разный seed → разный результат ───────────────────────
TestResult test_seed_sensitivity(const std::string& name) {
    RNG g1(1000, 5.0);
    RNG g2(1001, 5.0);

    int diff = 0;
    for (int i = 0; i < 100; i++) {
        if (g1.generate(1) != g2.generate(1))
            diff++;
    }
    ASSERT_TRUE(diff >= 95,
        "only " + std::to_string(diff) + "/100 differ (expected >= 95)");
    return { true, name, "" };
}

// ── Тест 3: Seed=0 не должен крашить ────────────────────────────
TestResult test_seed_zero(const std::string& name) {
    RNG g(0, 5.0);
    double v = g.generate(1);
    ASSERT_TRUE(v >= 0.0 && v < 1.0,
        "value out of range [0,1) for seed=0");
    return { true, name, "" };
}

// ── Тест 4: Значения в диапазоне [0, 1) ─────────────────────────
TestResult test_range(const std::string& name) {
    RNG g(999, 5.0);
    for (int i = 0; i < 100000; i++) {
        double v = g.generate(1);
        ASSERT_TRUE(v >= 0.0 && v < 1.0,
            "value " + std::to_string(v) + " out of [0,1)");
    }
    return { true, name, "" };
}

// ── Тест 5: Среднее ~0.5 ─────────────────────────────────────────
TestResult test_mean(const std::string& name) {
    RNG g(12345, 5.0);
    const int N = 100000;
    double sum = 0.0;
    for (int i = 0; i < N; i++)
        sum += g.generate(1);
    double mean = sum / N;
    ASSERT_NEAR(mean, 0.5, 0.01,
        "mean = " + std::to_string(mean) + " (expected ~0.5)");
    return { true, name, "" };
}

// ── Тест 6: Стандартное отклонение ~0.2887 ───────────────────────
TestResult test_stddev(const std::string& name) {
    RNG g(777, 5.0);
    const int N = 100000;
    std::vector<double> vals;
    vals.reserve(N);
    double sum = 0.0;
    for (int i = 0; i < N; i++) {
        double v = g.generate(1);
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
    RNG g(555, 5.0);
    const int N = 100000;
    int bins[10] = { 0 };
    for (int i = 0; i < N; i++) {
        double v = g.generate(1);
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
    RNG g(314, 5.0);
    const int N = 100000;
    std::vector<double> vals;
    vals.reserve(N);
    for (int i = 0; i < N; i++)
        vals.push_back(g.generate(1));

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
    RNG g(2024, 5.0);
    const int N = 10000;
    int ones = 0;
    int total_bits = 0;
    for (int i = 0; i < N; i++) {
        double v = g.generate(1);
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
    RNG g(888, 5.0);
    const int N = 100000;
    int runs = 1;
    double prev = g.generate(1);
    for (int i = 1; i < N; i++) {
        double v = g.generate(1);
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
    RNG g(42, 5.0);
    size_t period = g.get_period();
    ASSERT_TRUE(period > 0,
        "inc_max = 0 (expected > 0)");
    std::cout << "(inc_max = " << period << ") ";
    return { true, name, "" };
}

// ── Тест 12: Разный period → разный inc_max ─────────────────────
TestResult test_period_sensitivity(const std::string& name) {
    RNG g1(42, 5.0);
    RNG g2(42, 42.0);

    size_t p1 = g1.get_period();
    size_t p2 = g2.get_period();
    ASSERT_TRUE(p1 != p2,
        "same inc_max for different periods (" +
        std::to_string(p1) + " vs " + std::to_string(p2) + ")");
    return { true, name, "" };
}

// ── Тест 13: Пересев по достижении inc_max → расхождение ────────
TestResult test_period_affects_sequence(const std::string& name) {
    RNG g1(42, 5.0);
    RNG g2(42, 5.0);

    // inc_max — это get_period(). Вызываем generate(inc_max),
    // что продвигает inc_counter до inc_max.
    // Следующий generate(1) вызовет пересев.
    size_t inc_max = g1.get_period();
    g1.generate(inc_max);   // inc_counter = inc_max
    g1.generate(1);         // пересев → новая последовательность

    // g2 не доходил до inc_max
    g2.generate(1);

    int diff = 0;
    for (int i = 0; i < 100; i++) {
        if (g1.generate(1) != g2.generate(1))
            diff++;
    }
    ASSERT_TRUE(diff >= 50,
        "only " + std::to_string(diff) + "/100 differ (reseed should cause divergence)");
    return { true, name, "" };
}

// ── Тест 14: size не влияет на последовательность (без пересева) ─
TestResult test_size_consistency(const std::string& name) {
    RNG g1(42, 5.0);
    RNG g2(42, 5.0);

    // size влияет только на inc_counter, а не на ядро.
    // Пока нет пересева — последовательности одинаковые.
    int diff = 0;
    for (int i = 0; i < 100; i++) {
        if (g1.generate(1) != g2.generate(4))
            diff++;
    }
    ASSERT_EQ(diff, 0,
        "size should not affect sequence when no reseed occurs (got " +
        std::to_string(diff) + "/100 differences)");
    return { true, name, "" };
}

// ── Тест 15: Все допустимые size в диапазоне [0,1) ──────────────
TestResult test_size_validity(const std::string& name) {
    RNG g(555, 5.0);
    size_t sizes[] = { 1, 2, 4, 8, 16 };
    for (size_t s : sizes) {
        for (int i = 0; i < 1000; i++) {
            double v = g.generate(s);
            ASSERT_TRUE(v >= 0.0 && v < 1.0,
                "value " + std::to_string(v) + " out of [0,1) for size=" + std::to_string(s));
        }
    }
    return { true, name, "" };
}

// ── Тест 16: reset() меняет последовательность ─────────────────
TestResult test_reset(const std::string& name) {
    RNG g(42, 5.0);

    std::vector<double> before;
    for (int i = 0; i < 50; i++)
        before.push_back(g.generate(1));

    g.reset();

    int diff = 0;
    for (int i = 0; i < 50; i++) {
        double v = g.generate(1);
        if (v != before[i]) diff++;
    }
    ASSERT_TRUE(diff >= 40,
        "only " + std::to_string(diff) + "/50 differ after reset");
    return { true, name, "" };
}

// ── Тест 17: История коллизий очищается ─────────────────────────
TestResult test_history_clear(const std::string& name) {
    RNG g(42, 5.0);

    for (int i = 0; i < 100; i++)
        g.generate(1);

    ASSERT_TRUE(g.getHistorySize() > 0,
        "history empty after 100 generates");
    g.clearHistory();
    ASSERT_EQ(g.getHistorySize(), (size_t)0,
        "history not cleared");
    return { true, name, "" };
}

// ── Main ─────────────────────────────────────────────────────────
int main() {
    std::setlocale(0, "ru-ru");
    std::cout << "===========================================\n";
    std::cout << "  RNG Test Suite (CascadePRNG)\n";
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
    RUN_TEST(test_size_consistency);
    RUN_TEST(test_size_validity);
    RUN_TEST(test_reset);
    RUN_TEST(test_history_clear);

    std::cout << "\n===========================================\n";
    std::cout << "  Total:  " << g_tests_run << "\n";
    std::cout << "  Passed: " << g_tests_passed << "\n";
    std::cout << "  Failed: " << g_tests_failed << "\n";
    std::cout << "===========================================\n";

    return g_tests_failed > 0 ? 1 : 0;
}
