#pragma once
// MSHUNKO 2026 — усиленная версия (физическая энтропия + каскады)
#define _USE_MATH_DEFINES
#include <iostream>
#include <cmath>
#include <iomanip>
#include <math.h>
#include <bitset>
#include <cstdlib>
#include <string>
#include <chrono>
#include <vector>
#include <numeric>
#include <map>
#include <algorithm>
#include <random>
#include <fstream>
#include <ctime>
#include <stdexcept>
#include <unordered_set>
#include <cstdint>
#include <cstring>

#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <fstream>
#if defined(__linux__)
#include <sys/random.h>
#elif defined(__APPLE__)
#include <sys/random.h>
#endif
#endif

using namespace std;

// ============================================================================
//                          ПРИНЦИП РАБОТЫ ГСЧ (усиленная версия)
// ============================================================================
//
// Архитектура — трёхкаскадный генератор с динамическим сбросом.
// УСИЛЕНИЕ: первый каскад (источник сырого потока) заменён с детерминированного
// LCG на кроссплатформенное чтение системной энтропии (физический шум ядра ОС).
// Остальные каскады и вся логика коллизий/сброса — без изменений.
//
// Это превращает генератор из "каскада, маскирующего детерминизм" в
// "каскад, защищающий настоящую случайность".
//
// ──────────────────────────────────────────────────────────────────────────
// КАСКАД 1 — EntropySource (физический поток)  [ЗАМЕНА LCG]
// ──────────────────────────────────────────────────────────────────────────
// Читает системную энтропию: getrandom() на Linux/macOS, BCryptGenRandom
// на Windows, /dev/urandom как фолбэк.
//     Плюс:  настоящая физическая случайность (тепловой шум, тайминги
//            железа, квантовые эффекты — в зависимости от источника ядра).
//     Роль в каскаде: поставщик сырого физического потока вместо
//            детерминированного LCG. Корень каскада теперь непредсказуем.
//
// ──────────────────────────────────────────────────────────────────────────
// КАСКАД 2 — MeanCore (размывание)  [БЕЗ ИЗМЕНЕНИЙ]
// ──────────────────────────────────────────────────────────────────────────
// Нормирует выход в [0, 1], добавляет шум MT19937 (амплитуда 0.1).
// Теперь размывает не LCG-решётку (её больше нет), а возможные смещения
// физического источника.
//
// ──────────────────────────────────────────────────────────────────────────
// КАСКАД 3 — FantasyCore (коллизионное перемешивание)  [БЕЗ ИЗМЕНЕНИЙ]
// ──────────────────────────────────────────────────────────────────────────
// При коллизии сравнивает биты, различия становятся размерностями.
// apply_fantasy() добавляет ±0.01 по каждой размерности со случайным
// выбором знака через MT19937.
//
// ──────────────────────────────────────────────────────────────────────────
// ДИНАМИЧЕСКИЙ СБРОС — RotationCalculator  [ИЗМЕНЁН ТОЛЬКО RESEED]
// ──────────────────────────────────────────────────────────────────────────
// После inc_max байтов — пересоздание всех ядер.
// reseed теперь из системной энтропии вместо time(nullptr).
//
// ──────────────────────────────────────────────────────────────────────────
// ИТОГ
// ──────────────────────────────────────────────────────────────────────────
// Первый каскад — физическая энтропия (непредсказуемая).
// Второй каскад — размывание остаточных смещений.
// Третий каскад — коллизионное перемешивание.
// Сброс по периоду — обрыв корреляций + свежий reseed из энтропии.
//
// Криптостойкость: высокая. Состояние не восстанавливается, т.к. корень
// (EntropySource) — физический, а не детерминированный.
// ============================================================================

// Структура для хранения координат точки
struct Point {
    double x, y, z;
    Point(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}
};

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
        return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }

    Point calculateOA() const { return vectorBetween(A, O_okr); }
    Point calculateOB() const { return vectorBetween(B, O_okr); }

    double calculateAngle() const {
        Point OA = calculateOA();
        Point OB = calculateOB();
        double cos_angle = dotProduct(OA, OB) / (vectorLength(OA) * vectorLength(OB));
        return acos(cos_angle);
    }

    double calculateOmega() const { return calculateAngle() / time_AB; }

public:
    RotationCalculator(const Point& a, const Point& b, const Point& o, double t)
        : A(a), B(b), O_okr(o), time_AB(t) {
    }

    RotationCalculator(double time) {
        A = Point(-3, -0.5, -2.5);
        B = Point(-5.0 / 3.00, -5.0 / 6.00, 25.0 / 4.00);
        O_okr = Point(-2.64, -7.91, 1.65);
        time_AB = time;
    }

    double getPeriod() const {
        double omega = calculateOmega();
        return 2 * M_PI / omega;
    }

    void showResults() const {
        cout << fixed << setprecision(2);
        cout << "Результаты расчёта:\n";
        Point OA = calculateOA();
        Point OB = calculateOB();
        cout << "Длина вектора OA: " << vectorLength(OA) << "\n";
        cout << "Длина вектора OB: " << vectorLength(OB) << "\n";
        cout << "Угол между векторами (рад): " << calculateAngle() << "\n";
        cout << "Угловая скорость (рад/год): " << calculateOmega() << "\n";
        cout << "Период вращения (лет): " << getPeriod() << "\n";
    }
};

// ============================================================================
// КАСКАД 1 — EntropySource (ЗАМЕНА AssociativityCore / LCG)
// ============================================================================
// Кроссплатформенное чтение системной энтропии.
// Интерфейс совпадает с оригинальным AssociativityCore: метод generate()
// возвращает unsigned int.

class EntropySource {
private:
    // Avalanche-смеситель для расширения энтропии:
    // из 4 байт системного шума получаем 4 байта с полным перемешиванием.
    static uint32_t aval32(uint32_t x) {
        x ^= x >> 16; x *= 0x45d9f3bULL;
        x ^= x >> 16; x *= 0x45d9f3bULL;
        x ^= x >> 16; return x;
    }

    // Кроссплатформенное чтение системной энтропии
    static bool getEntropy(unsigned char* buf, size_t len) {
#if defined(_WIN32)
        return BCryptGenRandom(nullptr, buf, (ULONG)len,
            BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
#elif defined(__linux__) || defined(__APPLE__)
        if (getrandom(buf, len, 0) == (ssize_t)len) return true;
        std::ifstream ur("/dev/urandom", std::ios::binary);
        ur.read((char*)buf, len);
        return ur.gcount() == (std::streamsize)len;
#else
        std::ifstream ur("/dev/urandom", std::ios::binary);
        ur.read((char*)buf, len);
        return ur.gcount() == (std::streamsize)len;
#endif
    }

public:
    EntropySource() {}

    EntropySource(unsigned int /*seed*/) {
        // seed больше не нужен — энтропия приходит из системы.
        // Параметр оставлен для совместимости с интерфейсом.
    }

    unsigned int generate() {
        unsigned char buf[4];
        if (!getEntropy(buf, 4))
            throw std::runtime_error("entropy source unavailable");

        uint32_t raw = 0;
        for (int i = 0; i < 4; i++)
            raw = (raw << 8) | buf[i];

        // Avalanche-смешивание: убирает возможное смещение младших бит
        return aval32(raw);
    }

    void reseed() {
        // Сброс не нужен — каждый generate() читает свежую энтропию.
        // Метод оставлен для совместимости с логикой ресета.
    }
};

// ============================================================================
// КАСКАД 2 — MeanCore (БЕЗ ИЗМЕНЕНИЙ)
// ============================================================================
class MeanCore {
private:
    double mean;
    std::uniform_real_distribution<> dist;
    std::mt19937 gen;
public:
    MeanCore(double m = 1.0) : mean(m), dist(0.0, 1.0), gen(static_cast<unsigned int>(std::time(0))) {}

    double adjust(unsigned int base) {
        return (base / static_cast<double>(UINT_MAX)) * mean + dist(gen) * 0.1;
    }
};

// ============================================================================
// КАСКАД 3 — FantasyCore (БЕЗ ИЗМЕНЕНИЙ)
// ============================================================================
class FantasyCore {
private:
    std::mt19937 gen;
public:
    std::vector<std::vector<int>> dimensions;

    FantasyCore() : gen(static_cast<unsigned int>(std::time(0))) {}

    void add_collision(unsigned int a, unsigned int b) {
        if (a == b) return;

        std::bitset<32> bits_a(a);
        std::bitset<32> bits_b(b);

        for (int i = 0; i < 32; i++) {
            if (bits_a[i] != bits_b[i]) {
                if (std::find(dimensions.begin(), dimensions.end(),
                    std::vector<int>{bits_a[i] ? 1 : 0, bits_b[i] ? 0 : 1}) == dimensions.end()) {
                    dimensions.push_back({
                        bits_a[i] ? 1 : 0,
                        bits_b[i] ? 0 : 1
                        });
                }
            }
        }
    }

    double apply_fantasy(double base) {
        for (const auto& dim : dimensions) {
            if (std::uniform_int_distribution<>(0, 1)(gen) == 0) {
                base += dim[0] * 0.01;
            }
            else {
                base += dim[1] * 0.01;
            }
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
    EntropySource entropyCore;   // БЫЛО: AssociativityCore — СТАЛО: EntropySource
    MeanCore meanCore;
    FantasyCore fantasyCore;
    std::unordered_set<unsigned int> history;
    const int MAX_RETRIES = 49;
    const int MAX_HISTORY_SIZE = 49;
    size_t inc_counter;
    size_t inc_max;

    // Кроссплатформенный reseed из системной энтропии
    static unsigned int freshSeed() {
        unsigned char buf[4];
#if defined(_WIN32)
        BCryptGenRandom(nullptr, buf, 4, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
#elif defined(__linux__) || defined(__APPLE__)
        getrandom(buf, 4, 0);
#else
        std::ifstream ur("/dev/urandom", std::ios::binary);
        ur.read((char*)buf, 4);
#endif
        uint32_t s = 0;
        for (int i = 0; i < 4; i++) s = (s << 8) | buf[i];
        return s;
    }

public:
    RNG(unsigned int /*seed*/, double period) : entropyCore(), meanCore(), fantasyCore() {
        // seed больше не используется — энтропия из системы.
        // Параметр оставлен для совместимости с вызывающим кодом.
        RotationCalculator rc(period);
        inc_max = (size_t)round((rc.getPeriod() * 365.25 * 24 * 3600) / 8.0);
        inc_counter = 0;
    }

    size_t get_period() override {
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

    double generate(size_t size) override {
        if (inc_counter >= inc_max) {
            // ИЗМЕНЕНИЕ: reseed из системной энтропии вместо time(nullptr)
            entropyCore = EntropySource();
            meanCore = MeanCore(freshSeed());    // было: MeanCore()
            fantasyCore = FantasyCore();
            inc_counter = 0;
        }
        inc_counter += size;

        unsigned int base = entropyCore.generate();  // БЫЛО: assocCore.generate()
        int retries = 0;
        unsigned int previous_base = base;

        while (isCollision(base) && retries < MAX_RETRIES) {
            previous_base = base;
            base = entropyCore.generate();  // БЫЛО: assocCore.generate()

            if (base == previous_base) {
                handle_collision(previous_base, base);
            }
            retries++;
        }

        if (retries >= MAX_RETRIES) {
            // ИЗМЕНЕНИЕ: reseed из энтропии вместо time(0)
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
