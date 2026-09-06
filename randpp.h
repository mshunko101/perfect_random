#pragma once
// MSHUNKO 2026 — усиленная версия (физическая энтропия + каскады)
#define _USE_MATH_DEFINES
#include <cmath>
#include <math.h>
#include <bitset>
#include <string>
#include <vector>
#include <algorithm>
#include <random>
#include <stdexcept>
#include <unordered_set>
#include <cstdint>
#include <set>
#include <fstream>
#include <iostream>
#include <iomanip>

#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/random.h>
#endif

// ============================================================================
//                          ПРИНЦИП РАБОТЫ ГСЧ (усиленная версия)
// ============================================================================
//
// Архитектура — трёхкаскадный генератор с динамическим сбросом.
// УСИЛЕНИЕ: первый каскад (источник сырого потока) заменён с детерминированного
// LCG на кроссплатформенное чтение системной энтропии (физический шум ядра ОС).
// Остальные каскады и вся логика коллизий/сброса — без изменений.
//
// КАСКАД 1 — EntropySource: физическая энтропия ОС (BCryptGenRandom / getrandom)
// КАСКАД 2 — MeanCore: нормализация в [0,1] + шум MT19937 (амплитуда 0.1)
// КАСКАД 3 — FantasyCore: коллизионное перемешивание ±0.01 по размерностям
// СБРОС: после inc_max байтов — пересоздание всех ядер из свежей энтропии
//
// Криптостойкость: высокая. Состояние не восстанавливается, т.к. корень
// (EntropySource) — физический, а не детерминированный.
// ============================================================================

// ============================================================================
// Утилита чтения системной энтропии (единая для всех каскадов)
// ============================================================================
inline bool readEntropy(unsigned char* buf, size_t len) {
#if defined(_WIN32)
    return BCryptGenRandom(nullptr, buf, (ULONG)len,
        BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
#elif defined(__linux__) || defined(__APPLE__)
    size_t got = 0;
    while (got < len) {
        ssize_t r = getrandom(buf + got, len - got, 0);
        if (r <= 0) break;
        got += (size_t)r;
    }
    if (got == len) return true;
    // фолбэк на /dev/urandom
    std::ifstream ur("/dev/urandom", std::ios::binary);
    ur.read(reinterpret_cast<char*>(buf), static_cast<std::streamsize>(len));
    return ur.gcount() == static_cast<std::streamsize>(len);
#else
    std::ifstream ur("/dev/urandom", std::ios::binary);
    ur.read(reinterpret_cast<char*>(buf), static_cast<std::streamsize>(len));
    return ur.gcount() == static_cast<std::streamsize>(len);
#endif
}

inline uint32_t entropyUint32() {
    unsigned char buf[4];
    if (!readEntropy(buf, 4))
        throw std::runtime_error("entropy source unavailable");
    uint32_t v = 0;
    for (int i = 0; i < 4; i++)
        v = (v << 8) | buf[i];
    return v;
}

// ============================================================================
// Структура координат точки
// ============================================================================
struct Point {
    double x, y, z;
    Point(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}
};

// ============================================================================
// RotationCalculator — расчёт периода сброса
// ============================================================================
class RotationCalculator {
private:
    Point A, B, O_okr;
    double time_AB;

    Point vectorBetween(const Point& p1, const Point& p2) const {
        return Point(p1.x - p2.x, p1.y - p2.y, p1.z - p2.z);
    }

    double dotProduct(const Point& v1, const Point& v2) const {
        return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
    }

    double vectorLength(const Point& v) const {
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }

    Point calculateOA() const { return vectorBetween(A, O_okr); }
    Point calculateOB() const { return vectorBetween(B, O_okr); }

    double calculateAngle() const {
        Point OA = calculateOA();
        Point OB = calculateOB();
        double cos_angle = dotProduct(OA, OB) / (vectorLength(OA) * vectorLength(OB));
        return std::acos(cos_angle);
    }

    double calculateOmega() const { return calculateAngle() / time_AB; }

public:
    RotationCalculator(const Point& a, const Point& b, const Point& o, double t)
        : A(a), B(b), O_okr(o), time_AB(t) {}

    RotationCalculator(double time) {
        A = Point(-3, -0.5, -2.5);
        B = Point(-5.0 / 3.0, -5.0 / 6.0, 25.0 / 4.0);
        O_okr = Point(-2.64, -7.91, 1.65);
        time_AB = time;
    }

    double getPeriod() const {
        double omega = calculateOmega();
        return 2 * M_PI / omega;
    }

    void showResults() const {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Результаты расчёта:\n";
        Point OA = calculateOA();
        Point OB = calculateOB();
        std::cout << "Длина вектора OA: " << vectorLength(OA) << "\n";
        std::cout << "Длина вектора OB: " << vectorLength(OB) << "\n";
        std::cout << "Угол между векторами (рад): " << calculateAngle() << "\n";
        std::cout << "Угловая скорость (рад/год): " << calculateOmega() << "\n";
        std::cout << "Период вращения (лет): " << getPeriod() << "\n";
    }
};

// ============================================================================
// КАСКАД 1 — EntropySource (физический поток из ОС)
// ============================================================================
class EntropySource {
private:
    static uint32_t aval32(uint32_t x) {
        x ^= x >> 16; x *= 0x45d9f3bULL;
        x ^= x >> 16; x *= 0x45d9f3bULL;
        x ^= x >> 16; return x;
    }

public:
    EntropySource() {}
    EntropySource(unsigned int /*seed*/) {}  // совместимость

    unsigned int generate() {
        uint32_t raw = entropyUint32();
        return aval32(raw);
    }

    void reseed() {}  // каждый generate() читает свежую энтропию
};

// ============================================================================
// КАСКАД 2 — MeanCore (размывание)
// ============================================================================
class MeanCore {
private:
    double mean;
    std::uniform_real_distribution<> dist;
    std::mt19937 gen;
public:
    MeanCore(double m = 1.0)
        : mean(m), dist(0.0, 1.0), gen(entropyUint32()) {}

    double adjust(unsigned int base) {
        return (base / static_cast<double>(UINT_MAX)) * mean + dist(gen) * 0.1;
    }
};

// ============================================================================
// КАСКАД 3 — FantasyCore (коллизионное перемешивание)
// ============================================================================
class FantasyCore {
private:
    std::mt19937 gen;
public:
    std::set<std::pair<int,int>> dimensions;

    FantasyCore() : gen(entropyUint32()) {}

    void add_collision(unsigned int a, unsigned int b) {
        if (a == b) return;
        std::bitset<32> bits_a(a), bits_b(b);
        for (int i = 0; i < 32; i++) {
            if (bits_a[i] != bits_b[i]) {
                dimensions.insert({
                    bits_a[i] ? 1 : 0,
                    bits_b[i] ? 0 : 1
                });
            }
        }
    }

    double apply_fantasy(double base) {
        std::uniform_int_distribution<> coin(0, 1);
        for (const auto& dim : dimensions) {
            base += (coin(gen) == 0 ? dim.first : dim.second) * 0.01;
        }
        return base;
    }
};

// ============================================================================
// ОСНОВНОЙ КЛАСС ГЕНЕРАТОРА
// ============================================================================
#ifdef APPLICATION
class RNG : public RNGAbstract {
#else
class RNG {
#endif
private:
    EntropySource entropyCore;
    MeanCore meanCore;
    FantasyCore fantasyCore;
    std::unordered_set<unsigned int> history;
    static constexpr int MAX_RETRIES = 49;
    static constexpr int MAX_HISTORY_SIZE = 49;
    size_t inc_counter;
    size_t inc_max;

public:
    RNG(unsigned int /*seed*/, double period)
        : entropyCore(), meanCore(), fantasyCore() {
        RotationCalculator rc(period);
        inc_max = static_cast<size_t>(std::round(
            rc.getPeriod() * 365.25 * 24 * 3600 / 8.0));
        inc_counter = 0;
    }

#ifdef APPLICATION
    size_t get_period() override {
#else
    size_t get_period() {
#endif
        return inc_max;
    }

    bool isCollision(unsigned int value) {
        if (history.count(value) > 0) return true;
        history.insert(value);
        return false;
    }

    void handle_collision(unsigned int a, unsigned int b) {
        fantasyCore.add_collision(a, b);
    }

#ifdef APPLICATION
    double generate(size_t size) override {
#else
    double generate(size_t size) {
#endif
        if (inc_counter >= inc_max) {
            entropyCore = EntropySource();
            meanCore = MeanCore();
            fantasyCore = FantasyCore();
            inc_counter = 0;
        }
        inc_counter += size;

        unsigned int base = entropyCore.generate();
        int retries = 0;
        unsigned int previous_base = base;

        while (isCollision(base) && retries < MAX_RETRIES) {
            previous_base = base;
            base = entropyCore.generate();
            if (base == previous_base) {
                handle_collision(previous_base, base);
            }
            retries++;
        }

        if (retries >= MAX_RETRIES) {
            entropyCore = EntropySource();
            base = entropyCore.generate();
            clearHistory();
            fantasyCore.dimensions.clear();
        }

        if (getHistorySize() >= MAX_HISTORY_SIZE) {
            clearHistory();
            fantasyCore.dimensions.clear();
        }

        double mean_adjusted = meanCore.adjust(base);
        double current = fantasyCore.apply_fantasy(mean_adjusted);

        // Клампинг в [0, 1) — убирает переполнение при касте в uint32_t
        if (current < 0.0) current = 0.0;
        if (current >= 1.0) current = 0.9999999999;

        return current;
    }

    void clearHistory() { history.clear(); }

    size_t getHistorySize() const { return history.size(); }

    void reset() {
        entropyCore = EntropySource();
        clearHistory();
        fantasyCore.dimensions.clear();
    }
};
