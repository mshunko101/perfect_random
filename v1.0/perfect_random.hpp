// perfect_random.hpp
// Каскадный генератор псевдослучайных чисел
// Автор: Шунько Михаил Геннадьевич
// Проект: «Каскадная модель развёртки» (2021–2026)
//
// Математическая основа:
//   Каскадное отображение с фикс-поинтом b* = 4/9
//   a(n+1) = a(n) * 5/9   — сила (убывание)
//   b(n+1) = b(n) * 5/3   — форма (рост)
//   c(n+1) = c(n)^2       — энергия (двойная экспонента)
//   K = (5/3)/(5/9) = 3   — структурная константа
//   |f'(b*)| ≈ 0.874      — коэффициент затухания
//   k_cascade = -ln|f'(b*)| ≈ 0.1352
//
// Структурный предел: 49 = (43 + 55) / 2 = 7^2
//   43 — шаг организма, 55 — шаг сознания
//   49 — размер окна памяти каскада
//
// Period = ОПЖ (ожидаемая продолжительность жизни при рождении)
//   Связан с ζ-порогом каскада
//   Логистический рост связей: k(t) = 49 / (1 + 48 * e^(-0.1352*t))
//   Насыщение при t ≥ ~62.6 (99% заполнения)
//   При ОПЖ 73.8 — заполнение 99.8%
//
// Тестирование: BigCrush (TestU01), PractRand (16 ГБ), NIST SP 800-22

#ifndef PERFECT_RANDOM_HPP
#define PERFECT_RANDOM_HPP

#include <cstdint>
#include <cmath>
#include <array>
#include <limits>

namespace perfect_random {

// ─── Каскадные константы ───────────────────────────────────────────
constexpr double B_STAR       = 4.0 / 9.0;      // фикс-поинт
constexpr double MUL_FORCE    = 5.0 / 9.0;      // множитель силы
constexpr double MUL_FORM     = 5.0 / 3.0;      // множитель формы
constexpr double K_STRUCT     = 3.0;            // структурная константа
constexpr double A0           = 4.660964;       // начальная сила
constexpr double DELTA_B      = 1.211;          // масштаб наблюдателя (м)
constexpr double K_CASCADE    = 0.13518;        // -ln|f'(b*)| — скорость роста связей

// ─── Структурный предел ────────────────────────────────────────────
constexpr int    MAX_HISTORY  = 49;             // 7^2 = (43+55)/2
constexpr int    MAX_RETRIES  = 49;
constexpr int    N_CASCADE    = 49;

// ─── Коридор устойчивости ─────────────────────────────────────────
constexpr double PERIOD_MIN   = 57.0;          // минимум для 98% заполнения
constexpr double PERIOD_99    = 62.6;          // 99% заполнения
constexpr double PERIOD_FULL  = 73.8;          // текущая ОПЖ (2026, ООН)

// ─── Состояние генератора ─────────────────────────────────────────
struct PRNGState {
    double a;                          // сила
    double b;                          // форма
    double c;                          // энергия
    double period;                     // ОПЖ — параметр period
    uint64_t step;                     // номер шага
    uint64_t seed;                     // начальное зерно
    std::array<uint64_t, MAX_HISTORY> history;  // окно памяти каскада
    int history_idx;
};

// ─── Вспомогательные функции ──────────────────────────────────────

// Логистический рост связей с возрастом
// k(t) = 49 / (1 + 48 * e^(-0.1352 * t))
inline double connections_at_age(double age) {
    double exponent = -K_CASCADE * age;
    if (exponent < -700.0) return 49.0;  // защита от переполнения
    return 49.0 / (1.0 + 48.0 * std::exp(exponent));
}

// Эффективный ζ для возрастной группы
// age_mid — средний возраст группы, weight — доля в популяции
inline double zeta_component(double age_mid, double weight) {
    double k = connections_at_age(age_mid);
    if (k > 49.0) k = 49.0;  // насыщение
    return weight * k;
}

// ─── Каскадный ГПСЧ ────────────────────────────────────────────────
class CascadePRNG {
private:
    PRNGState state_;

    // Смешивание состояния каскада → 64-битное число
    uint64_t mix() const {
        // Берём три компоненты и окно памяти
        uint64_t h = state_.seed;

        // Компонента силы (a) — затухание
        uint64_t bits_a;
        std::memcpy(&bits_a, &state_.a, sizeof(bits_a));

        // Компонента формы (b) — рост
        uint64_t bits_b;
        std::memcpy(&bits_b, &state_.b, sizeof(bits_b));

        // Компонента энергии (c) — двойная экспонента
        uint64_t bits_c;
        std::memcpy(&bits_c, &state_.c, sizeof(bits_c));

        //_xor64 mixing
        h ^= bits_a + 0x9e3779b97f4a7c15ULL;
        h ^= bits_b + 0x6a09e667bb67ae85ULL;
        h ^= bits_c + 0xbb67ae8584caa73bULL;

        // История каскада — окно 49
        h ^= state_.history[state_.history_idx] + 0x3c6ef372bb20e437ULL;
        h ^= (h >> 33);
        h ^= state_.step * 0xff51afd7ed558ccdULL;
        h ^= (h >> 31);
        h *= 0xc4ceb9fe1a85ec53ULL;
        h ^= (h >> 33);

        return h;
    }

    // Один шаг каскада
    void cascade_step() {
        // a(n+1) = a(n) * 5/9 — сила убывает
        state_.a *= MUL_FORCE;

        // b(n+1) = b(n) * 5/3 — форма растёт
        state_.b *= MUL_FORM;

        // c(n+1) = c(n)^2 — энергия (двойная экспонента)
        // Защита от переполнения: нормализация
        if (state_.c > 1e18) {
            state_.c = 1.0 + std::fmod(state_.c, 1.0);
        }
        state_.c *= state_.c;

        // Нормализация b к фикс-поинту
        // При больших значениях возвращаем к b*
        if (state_.b > 1e15) {
            state_.b = B_STAR * std::fmod(state_.b / B_STAR, 1.0);
            if (state_.b < 0) state_.b = -state_.b;
        }

        // Нормализация a
        if (state_.a < 1e-300) {
            state_.a = A0;
        }

        // Запись в окно памяти
        uint64_t mixed = mix();
        state_.history[state_.history_idx] = mixed;
        state_.history_idx = (state_.history_idx + 1) % MAX_HISTORY;

        state_.step++;
    }

    // Инициализация окна памяти из зерна
    void init_history() {
        // Заполняем окно 49 значениями, производными от зерна
        uint64_t s = state_.seed;
        for (int i = 0; i < MAX_HISTORY; i++) {
            s ^= s << 13;
            s ^= s >> 7;
            s ^= s << 17;
            state_.history[i] = s;
        }
        state_.history_idx = 0;
    }

public:
    // Конструктор с зерном и period
    explicit CascadePRNG(uint64_t seed, double period = PERIOD_FULL)
        : state_{} {
        state_.seed = seed;
        state_.step = 0;
        state_.period = (period < PERIOD_MIN) ? PERIOD_MIN : period;
        state_.a = A0;
        state_.b = DELTA_B * 1e-6;  // старт с малого масштаба
        state_.c = 1.0 + (seed % 1000) * 1e-15;  // 1 + ε
        init_history();
    }

    // Конструктор по умолчанию
    CascadePRNG() : CascadePRNG(0x853a769c4d2b1e8fULL, PERIOD_FULL) {}

    // Сгенерировать 64-битное число
    uint64_t next_u64() {
        cascade_step();
        return mix();
    }

    // Сгенерировать число в диапазоне [0, max)
    uint64_t next_u64(uint64_t max) {
        if (max == 0) return 0;
        return next_u64() % max;
    }

    // Сгенерировать double в [0, 1)
    double next_double() {
        // 53 бита для максимальной точности double
        return (next_u64() >> 11) * (1.0 / 9007199254740992.0);
    }

    // Сгенерировать float в [0, 1)
    float next_float() {
        return static_cast<float>(next_double());
    }

    // Сгенерировать int в [min, max]
    int next_int(int min_val, int max_val) {
        if (max_val <= min_val) return min_val;
        return min_val + static_cast<int>(next_u64(static_cast<uint64_t>(max_val - min_val + 1)));
    }

    // Сгенерировать массив чисел
    template <size_t N>
    void next_array(std::array<uint64_t, N>& arr) {
        for (size_t i = 0; i < N; i++) {
            arr[i] = next_u64();
        }
    }

    // ─── Сохранение/восстановление состояния ───────────────────────
    PRNGState save_state() const {
        return state_;
    }

    void restore_state(const PRNGState& saved) {
        state_ = saved;
    }

    // ─── Геттеры ──────────────────────────────────────────────────
    double get_period() const { return state_.period; }
    uint64_t get_step() const { return state_.step; }
    uint64_t get_seed() const { return state_.seed; }

    // Текущее число связей (заполнение окна)
    double current_connections() const {
        double age = static_cast<double>(state_.step);
        return connections_at_age(age);
    }

    // Процент заполнения каскада
    double fill_percentage() const {
        return (current_connections() / 49.0) * 100.0;
    }

    // Динамика каскада — текстовое описание
    const char* cascade_phase() const {
        double conn = current_connections();
        if (conn < 5.0)   return "Зерно — каскад инициализируется";
        if (conn < 19.0)  return "Рост — формирование базовых связей";
        if (conn < 40.0)  return "Ядро — активное нарабатывание связей";
        if (conn < 48.0)  return "Насыщение — структура почти заполнена";
        return "Зрелость — каскад в режиме насыщения";
    }
};

// ─── Удобные функции без создания объекта ─────────────────────────
inline uint64_t quick_random(uint64_t seed, double period = PERIOD_FULL) {
    CascadePRNG prng(seed, period);
    return prng.next_u64();
}

} // namespace perfect_random

#endif // PERFECT_RANDOM_HPP
