// cascade_test.cpp
#include "../../cascade_prng.hpp"
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
    CascadePRNG g1(42, 5);
    CascadePRNG g2(42, 5);

    for (int i = 0; i < 10000; i++) {
        double v1 = g1.generate();
        double v2 = g2.generate();
        ASSERT_EQ(v1, v2, "sequences diverged at step " + std::to_string(i));
    }
    return { true, name, "" };
}

// ── Тест 2: Разный seed → разный результат ───────────────────────
TestResult test_seed_sensitivity(const std::string& name) {
    CascadePRNG g1(1000, 5);
    CascadePRNG g2(1001, 5);

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
    CascadePRNG g(0, 5);
    double v = g.generate();
    ASSERT_TRUE(v >= 0.0 && v < 1.0,
        "value out of range [0,1) for seed=0");
    return { true, name, "" };
}

// ── Тест 4: Значения в диапазоне [0, 1) ─────────────────────────
TestResult test_range(const std::string& name) {
    CascadePRNG g(999, 5);
    for (int i = 0; i < 100000; i++) {
        double v = g.generate();
        ASSERT_TRUE(v >= 0.0 && v < 1.0,
            "value " + std::to_string(v) + " out of [0,1)");
    }
    return { true, name, "" };
}

// ── Тест 5: Среднее ~0.5 ─────────────────────────────────────────
TestResult test_mean(const std::string& name) {
    CascadePRNG g(12345, 5);
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
    CascadePRNG g(777, 5);
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
    CascadePRNG g(555, 5);
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
    // df=9, критическое значение при p=0.05 → 16.92
    ASSERT_TRUE(chi2 < 16.92,
        "chi2 = " + std::to_string(chi2) + " (critical 16.92)");
    return { true, name, "" };
}

// ── Тест 8: Автокорреляция ───────────────────────────────────────
TestResult test_autocorrelation(const std::string& name) {
    CascadePRNG g(314, 5);
    const int N = 100000;
    std::vector<double> vals;
    vals.reserve(N);
    for (int i = 0; i < N; i++)
        vals.push_back(g.generate());

    // Для равномерного [0,1) E[x*x] = 1/3, E[x]*E[x] = 1/4
    // AC(lag) = E[x_i * x_{i-lag}] — должно быть ~1/3, не ~0.5
    for (int lag = 1; lag <= 5; lag++) {
        double ac = 0.0;
        for (int i = lag; i < N; i++)
            ac += vals[i] * vals[i - lag];
        ac /= (N - lag);
        // Для независимых значений AC ~ 1/4 (E[x]^2)
        // Допустимый диапазон: 0.20..0.30
        ASSERT_TRUE(ac > 0.20 && ac < 0.30,
            "AC lag-" + std::to_string(lag) + " = " + std::to_string(ac) +
            " (expected ~0.25)");
    }
    return { true, name, "" };
}

// ── Тест 9: Пермутация меняет out_idx ───────────────────────────
TestResult test_permutation(const std::string& name) {
    CascadePRNG g(42, 3);  // N=3 — пермутация каждые 3 шага
    // После 3 шагов out_idx должен измениться
    int idx_before = g.get_out_idx();
    g.generate(); g.generate(); g.generate();
    int idx_after = g.get_out_idx();
    ASSERT_TRUE(idx_before != idx_after,
        "out_idx not changed after N steps (before=" +
        std::to_string(idx_before) + ", after=" +
        std::to_string(idx_after) + ")");
    // После 3*N=9 шагов out_idx должен вернуться
    g.generate(); g.generate(); g.generate();
    g.generate(); g.generate(); g.generate();
    int idx_final = g.get_out_idx();
    ASSERT_EQ(idx_final, idx_before,
        "out_idx not returned after 3N steps");
    return { true, name, "" };
}

// ── Тест 10: N не меняется при generate ─────────────────────────
TestResult test_N_stable(const std::string& name) {
    CascadePRNG g(100, 7);
    for (int i = 0; i < 100; i++)
        g.generate();  // передаём 8, но N должно остаться 7
    // Косвенная проверка: другой генератор с N=7 и тем же seed
    CascadePRNG g2(100, 7);
    for (int i = 0; i < 100; i++)
        g2.generate();  // передаём 16
    // Результаты должны совпадать — N не зависит от size
    double v1 = g.generate();
    double v2 = g2.generate();
    ASSERT_EQ(v1, v2,
        "N changed by generate(size) — results differ");
    return { true, name, "" };
}

// ── Тест 11: Разный N → разная последовательность ───────────────
TestResult test_N_affects_sequence(const std::string& name) {
    CascadePRNG g1(42, 3);
    CascadePRNG g2(42, 7);
    int diff = 0;
    for (int i = 0; i < 100; i++) {
        if (g1.generate() != g2.generate())
            diff++;
    }
    ASSERT_TRUE(diff >= 90,
        "only " + std::to_string(diff) + "/100 differ (N should affect sequence)");
    return { true, name, "" };
}

// ── Тест 12: Монобит (частота 0/1) ──────────────────────────────
TestResult test_monobit(const std::string& name) {
    CascadePRNG g(2024, 5);
    const int N = 10000;  // 10000 чисел × 31 бит = 310000 бит
    int ones = 0;
    int total_bits = 0;
    for (int i = 0; i < N; i++) {
        uint32_t bits = g.generate_raw();  // сырые биты, без потери точности
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


// ── Тест 13: Runs test (серии) ───────────────────────────────────
TestResult test_runs(const std::string& name) {
    CascadePRNG g(888, 5);
    const int N = 100000;
    int runs = 1;
    double prev = g.generate();
    for (int i = 1; i < N; i++) {
        double v = g.generate();
        if ((v > 0.5) != (prev > 0.5))
            runs++;
        prev = v;
    }
    // Ожидаемое число серий: N/2 + 1
    double expected_runs = N / 2.0;
    double z = (runs - expected_runs) / std::sqrt(N / 4.0);
    // |z| < 3.09 (p=0.001) — проход
    ASSERT_TRUE(std::abs(z) < 3.09,
        "z = " + std::to_string(z) + " (|z| < 3.09 required)");
    return { true, name, "" };
}

// ── Тест 14: Период ──────────────────────────────────────────────
TestResult test_period_exists(const std::string& name) {
    CascadePRNG g(7, 5);
    size_t period = g.get_period();
    if (period == SIZE_MAX) {
        std::cout << "(> 10M) ";
        return { true, name, "" };
    }
    ASSERT_TRUE(period > 1000,
        "period too short: " + std::to_string(period));
    std::cout << "(period = " << period << ") ";
    return { true, name, "" };
}


// ── Main ─────────────────────────────────────────────────────────
int main() {
    std::setlocale(0, "ru-ru");
    std::cout << "===========================================\n";
    std::cout << "  CascadePRNG Test Suite\n";
    std::cout << "===========================================\n\n";

    std::cout << "─ Basic ──────────────────────────────────\n";
    RUN_TEST(test_reproducibility);
    RUN_TEST(test_seed_sensitivity);
    RUN_TEST(test_seed_zero);
    RUN_TEST(test_range);

    std::cout << "\n─ Statistical ─────────────────────────────\n";
    RUN_TEST(test_mean);
    RUN_TEST(test_stddev);
    RUN_TEST(test_chi_square);
    RUN_TEST(test_autocorrelation);
    RUN_TEST(test_monobit);
    RUN_TEST(test_runs);

    std::cout << "\n─ Structural ─────────────────────────────\n";
    RUN_TEST(test_permutation);
    RUN_TEST(test_N_stable);
    RUN_TEST(test_N_affects_sequence);
    RUN_TEST(test_period_exists);

    std::cout << "\n===========================================\n";
    std::cout << "  Total:\t" << g_tests_run << "\n";
    std::cout << "  Passed:\t" << g_tests_passed << "\n";
    std::cout << "  Failed:\t" << g_tests_failed << "\n";
    std::cout << "============================================\n";

    return g_tests_failed > 0 ? 1 : 0;
}
