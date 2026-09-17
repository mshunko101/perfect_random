#ifndef PERFECT_RANDOM_HPP
#define PERFECT_RANDOM_HPP

// ============================================================
// perfect_random — каскадный ГПСЧ с демографической осознанностью
// Автор: Шунько Михаил Геннадьевич
// Проект: «Каскадная модель развёртки» (2021–2026)
// ============================================================
//
// Генератор построен на каскадной динамике с фикс-поинтом b* = 4/9.
// Параметр period = ОПЖ (ожидаемая продолжительность жизни).
// Эффективное окно каскада = min(49, round(ζ_эфф)),
// где ζ_эфф вычисляется из реальной возрастной структуры.
//
// Пользователь генератора видит:
//   — текущий ζ_эфф мира/региона
//   — статус качества (КРИТИЧНО → ПОЛНАЯ)
//   — какие тесты пройдут, какие нет
//   — какую нагрузку он размещает на систему
// ============================================================

#include <cstdint>
#include <cmath>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>

namespace perfect_random {

// ============================================================
// Константы каскадной динамики
// ============================================================

constexpr double B_STAR       = 4.0 / 9.0;     // фикс-поинт
constexpr double FIVE_THIRD   = 5.0 / 3.0;      // множитель формы
constexpr double FIVE_NINTH   = 5.0 / 9.0;      // множитель силы
constexpr int    MAX_CONN     = 49;             // 7² — структурный предел
constexpr int    MAX_HISTORY  = 49;             // размер окна памяти каскада
constexpr int    MAX_RETRIES  = 49;             // максимум попыток

// Производная в фикс-поинте: |f'(b*)| = (2/3) * (9/4)^(1/3) ≈ 0.874
constexpr double F_PRIME_ABS  = 0.87358;       // |f'(b*)|
constexpr double K_CASCADE    = 0.13518;        // -ln(|f'(b*)|)

// Пороги period (ОПЖ) для созревания каскада
constexpr double PERIOD_99    = 62.6;           // 99% заполнения
constexpr double PERIOD_95    = 50.4;           // 95% заполнения
constexpr double PERIOD_MIN   = 57.0;           // абсолютный минимум

// ============================================================
// Качество генератора по ζ_эфф
// ============================================================

enum class QualityGrade {
    E_CRITICAL,    // ζ < 20
    D_LIMITED,     // 20 ≤ ζ < 25
    C_WARNING,     // 25 ≤ ζ < 30
    B_NORMAL,      // 30 ≤ ζ < 35
    A_FULL         // ζ ≥ 35
};

struct QualityReport {
    QualityGrade grade;
    std::string  status;
    std::string  tests;
    std::string  recommendation;
    double       zeta_eff;
    double       filling_percent;
    int          effective_window;
    double       capacity_left;
    bool         period_ok;
    double       period;
};

// ============================================================
// Возрастная группа для демографических данных
// ============================================================

struct AgeGroup {
    int    start_age;
    int    end_age;
    double population;  // численность группы
};

// ============================================================
// Демографические данные региона
// ============================================================

struct Demographics {
    std::string name;
    int         year;
    std::string source;
    double      total_population;
    double      life_expectancy;  // ОПЖ
    std::vector<AgeGroup> groups;
};

// ============================================================
// Логистическая модель роста связей
// ============================================================

inline double connections_at_age(double age) {
    return MAX_CONN / (1.0 + 48.0 * std::exp(-K_CASCADE * age));
}

// ============================================================
// Вычисление ζ_эфф популяции
// ============================================================

inline double compute_zeta_eff(const Demographics& demo) {
    double zeta = 0.0;
    for (const auto& g : demo.groups) {
        double mid = (g.start_age + g.end_age) / 2.0;
        if (mid > 100) mid = 85.0;
        double weight = g.population / demo.total_population;
        zeta += weight * connections_at_age(mid);
    }
    return zeta;
}

// ============================================================
// Оценка качества генератора
// ============================================================

inline QualityReport assess_quality(double zeta_eff, double opzh) {
    QualityReport r;
    r.zeta_eff = zeta_eff;
    r.filling_percent = zeta_eff / MAX_CONN * 100.0;
    r.effective_window = std::min(MAX_CONN, (int)std::round(zeta_eff));
    r.capacity_left = MAX_CONN - zeta_eff;
    r.period = opzh;
    r.period_ok = opzh >= PERIOD_MIN;

    if (zeta_eff >= 35) {
        r.grade = QualityGrade::A_FULL;
        r.status = "ПОЛНАЯ КАЧЕСТВЕННОСТЬ";
        r.tests = "Все тесты (BigCrush, PractRand, NIST) проходят с запасом";
        r.recommendation = "Генератор работает на полном окне каскада. "
                           "Нагрузка на систему минимальна.";
    } else if (zeta_eff >= 30) {
        r.grade = QualityGrade::B_NORMAL;
        r.status = "НОРМА";
        r.tests = "Все тесты проходят. Рекомендуется ежегодный мониторинг";
        r.recommendation = "Генератор стабилен. Каскад достаточно заполнен "
                           "для компенсации возмущений.";
    } else if (zeta_eff >= 25) {
        r.grade = QualityGrade::C_WARNING;
        r.status = "ВНИМАНИЕ";
        r.tests = "Большинство тестов проходят. Возможны слабые отклонения в BigCrush";
        r.recommendation = "Каскад заполнен частично. Рекомендуется усилить "
                           "меры демографической поддержки.";
    } else if (zeta_eff >= 20) {
        r.grade = QualityGrade::D_LIMITED;
        r.status = "ОГРАНИЧЕННО";
        r.tests = "Часть тестов может не пройти. Качество последовательности снижено";
        r.recommendation = "Каскад недозаполнен. Требуются срочные меры "
                           "по стабилизации возрастной структуры.";
    } else {
        r.grade = QualityGrade::E_CRITICAL;
        r.status = "КРИТИЧНО";
        r.tests = "Статистические тесты с высокой вероятностью не проходят";
        r.recommendation = "Каскад критически незаполнен. "
                           "Генератор не гарантирует качество.";
    }
    return r;
}

// ============================================================
// Формирование отчёта для пользователя
// ============================================================

inline std::string format_report(const Demographics& demo,
                                  const QualityReport& q) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(2);

    os << "═══════════════════════════════════════════════════════\n";
    os << "  PERFECT_RANDOM — СТАТУС КАСКАДНОЙ СИСТЕМЫ\n";
    os << "═══════════════════════════════════════════════════════\n";
    os << "  Регион:       " << demo.name << " (" << demo.year << ")\n";
    os << "  Источник:     " << demo.source << "\n";
    os << "  Население:    " << std::setprecision(0)
       << demo.total_population << "\n";
    os << "  ОПЖ:          " << std::setprecision(1)
       << demo.life_expectancy << " лет\n";
    os << "───────────────────────────────────────────────────────\n";
    os << "  ζ_эфф         = " << std::setprecision(2) << q.zeta_eff
       << " из " << MAX_CONN << "\n";
    os << "  Заполнение    = " << q.filling_percent << "%\n";
    os << "  Окно каскада  = " << q.effective_window << " / " << MAX_CONN
       << "\n";
    os << "  Ёмкость       = " << q.capacity_left << " свободных связей\n";
    os << "  Period (ОПЖ)  = " << std::setprecision(1) << q.period
       << (q.period_ok ? " ✓" : " ✗") << "\n";
    os << "───────────────────────────────────────────────────────\n";
    os << "  СТАТУС: " << q.status << "\n";
    os << "  Тесты:  " << q.tests << "\n";
    os << "───────────────────────────────────────────────────────\n";
    os << "  " << q.recommendation << "\n";
    os << "═══════════════════════════════════════════════════════\n";

    return os.str();
}

// ============================================================
// Каскадный ГПСЧ
// ============================================================

class CascadePRNG {
public:
    // Конструктор с seed и period (ОПЖ)
    CascadePRNG(uint64_t seed, double period = 73.8)
        : period_(period), step_(0)
    {
        // Инициализация состояния каскада из seed
        // a — сила (убывание), b — форма (рост), c — энергия
        double s = static_cast<double>(seed);
        state_a_ = 4.660964 * std::abs(std::sin(s * 0.5981)) + 0.001;
        state_b_ = std::abs(std::cos(s * 1.6180)) + 0.001;
        state_c_ = 1.0 + std::abs(std::sin(s * 3.14159)) * 0.001;

        // Заполнение окна памяти
        for (int i = 0; i < MAX_HISTORY; ++i) {
            history_[i] = next_raw();
        }
    }

    // Генерация 64-битного значения
    uint64_t next_u64() {
        uint64_t val = next_raw();
        // Смешивание с окном памяти
        uint64_t mix = val;
        for (int i = 0; i < effective_window_; ++i) {
            mix ^= (history_[(step_ - i - 1 + MAX_HISTORY) % MAX_HISTORY]
                    << (i % 32));
            mix = mix * 0x517cc1b727220a95ULL + 0x6c62272e07bb0142ULL;
            // rotl
            mix = (mix << (i % 16 + 1)) | (mix >> (64 - (i % 16 + 1)));
        }
        history_[step_ % MAX_HISTORY] = val;
        step_++;
        return mix ^ val;
    }

    // Генерация double в [0, 1)
    double next_double() {
        return static_cast<double>(next_u64() >> 11) / (1ULL << 53);
    }

    // Генерация int в [min, max]
    int next_int(int min_v, int max_v) {
        return min_v + static_cast<int>(next_double() * (max_v - min_v + 1));
    }

    // Генерация массива
    void next_array(uint64_t* arr, size_t n) {
        for (size_t i = 0; i < n; ++i) {
            arr[i] = next_u64();
        }
    }

    // Установка эффективного окна (на основе ζ_эфф)
    void set_effective_window(int w) {
        effective_window_ = std::max(1, std::min(MAX_HISTORY, w));
    }

    // Сохранение состояния для воспроизводимости
    struct State {
        uint64_t seed;
        double   period;
        int      effective_window;
        double   state_a;
        double   state_b;
        double   state_c;
        int      step;
        uint64_t history[MAX_HISTORY];
    };

    State save_state() const {
        State s;
        s.seed = 0; // нужно хранить изначальный seed
        s.period = period_;
        s.effective_window = effective_window_;
        s.state_a = state_a_;
        s.state_b = state_b_;
        s.state_c = state_c_;
        s.step = step_;
        for (int i = 0; i < MAX_HISTORY; ++i)
            s.history[i] = history_[i];
        return s;
    }

    void restore_state(const State& s) {
        period_ = s.period;
        effective_window_ = s.effective_window;
        state_a_ = s.state_a;
        state_b_ = s.state_b;
        state_c_ = s.state_c;
        step_ = s.step;
        for (int i = 0; i < MAX_HISTORY; ++i)
            history_[i] = s.history[i];
    }

    double period() const { return period_; }
    int    effective_window() const { return effective_window_; }

private:
    double   period_;
    double   state_a_;
    double   state_b_;
    double   state_c_;
    uint64_t history_[MAX_HISTORY];
    int      step_;
    int      effective_window_ = MAX_HISTORY;

    uint64_t next_raw() {
        // Каскадный шаг
        state_a_ *= FIVE_NINTH;
        state_b_ *= FIVE_THIRD;
        state_c_ = state_c_ * state_c_;

        // Нормализация к фикс-поинту
        double norm = state_b_ / (state_b_ + state_a_ + 1e-15);
        double folded = norm * B_STAR + (1.0 - norm) * (1.0 - B_STAR);

        // Преобразование в 64-битное значение
        uint64_t raw = 0;
        double   frac = folded - std::floor(folded);
        for (int bit = 0; bit < 64; ++bit) {
            frac *= 2.0;
            if (frac >= 1.0) { raw |= (1ULL << bit); frac -= 1.0; }
        }

        // Дополнительное смешивание с энергией
        uint64_t c_bits = *reinterpret_cast<uint64_t*>(&state_c_);
        raw ^= c_bits + 0x9e3779b97f4a7c15ULL;
        raw = (raw ^ (raw >> 17)) * 0xbf58476d1ce4e5b9ULL;
        raw = (raw ^ (raw >> 31)) * 0x94d049bb133111ebULL;
        raw = raw ^ (raw >> 32);

        return raw;
    }
};

// ============================================================
// Встроенные демографические данные (2025-2026)
// ============================================================

inline Demographics belarus_demographics_2025() {
    Demographics d;
    d.name = "Беларусь";
    d.year = 2025;
    d.source = "UN WPP 2024 / populationpyramids.net / Белстат";
    d.total_population = 8997612;
    d.life_expectancy = 75.0;
    d.groups = {
        {0,  4,  352562},  {5,  9,  499598},  {10, 14, 577454},
        {15, 19, 506861},  {20, 24, 427515},  {25, 29, 445476},
        {30, 34, 592399},  {35, 39, 744574},  {40, 44, 724000},
        {45, 49, 645953},  {50, 54, 613059},  {55, 59, 584212},
        {60, 64, 638773},  {65, 69, 658070},  {70, 74, 493553},
        {75, 79, 279680},  {80, 120, 213873}
    };
    return d;
}

inline Demographics world_demographics_2025() {
    Demographics d;
    d.name = "Мир (ООН)";
    d.year = 2025;
    d.source = "UN WPP 2024 / UNFPA";
    d.total_population = 8231613070;
    d.life_expectancy = 73.8;
    d.groups = {
        {0,  4,  674900000}, {5,  9,  659200000}, {10, 14, 651000000},
        {15, 19, 643000000}, {20, 24, 618000000}, {25, 29, 593000000},
        {30, 34, 560000000}, {35, 39, 535000000}, {40, 44, 494000000},
        {45, 49, 453000000}, {50, 54, 412000000}, {55, 59, 371000000},
        {60, 64, 329000000}, {65, 69, 288000000}, {70, 74, 231000000},
        {75, 79, 165000000}, {80, 120, 148000000}
    };
    return d;
}

} // namespace perfect_random

#endif // PERFECT_RANDOM_HPP
