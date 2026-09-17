// ============================================================
// zeta_monitor — утилита мониторинга демографической нагрузки
// для ГПСЧ perfect_random
//
// Принимает демографические данные, вычисляет ζ_эфф,
// определяет, выдаст ли генератор верную последовательность,
// и описывает динамику ситуации.
//
// Использование:
//   ./zeta_monitor              — Беларусь (по умолчанию)
//   ./zeta_monitor --world      — Мир (ООН)
//   ./zeta_monitor --opzh 75.0  — своя ОПЖ
//   ./zeta_monitor --file data.csv — свой CSV
//   ./zeta_monitor --interactive — интерактивный режим
//
// CSV формат: start_age,end_age,population (заголовок необязателен)
// ============================================================

#include "perfect_random.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>

using namespace perfect_random;

// ============================================================
// Парсинг CSV с демографическими данными
// ============================================================

Demographics parse_csv(const std::string& filename, double opzh) {
    Demographics d;
    d.name = "Пользовательские данные";
    d.year = 2025;
    d.source = filename;
    d.total_population = 0;
    d.life_expectancy = opzh;

    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Ошибка: не удалось открыть " << filename << "\n";
        return d;
    }

    std::string line;
    bool header = true;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        // Пропуск заголовка
        if (header) {
            header = false;
            if (line.find("start") != std::string::npos ||
                line.find("age")  != std::string::npos) {
                continue;
            }
        }

        std::istringstream ss(line);
        std::string token;
        int start, end;
        double pop;

        if (std::getline(ss, token, ',')) start = std::stoi(token);
        if (std::getline(ss, token, ',')) end   = std::stoi(token);
        if (std::getline(ss, token, ',')) pop   = std::stod(token);

        d.groups.push_back({start, end, pop});
        d.total_population += pop;
    }
    return d;
}

// ============================================================
// Описание динамики естественным языком
// ============================================================

std::string describe_dynamics(const Demographics& demo,
                               const QualityReport& q) {
    std::ostringstream os;
    double fill = q.filling_percent;

    os << "Динамика: ";

    if (fill >= 70) {
        os << "каскад устойчив и хорошо заполнен. "
           << "Возрастная структура обеспечивает "
           << "достаточное число связей для компенсации возмущений. "
           << "Нагрузка на систему минимальна — генератор "
           << "работает в оптимальном режиме.";
    } else if (fill >= 60) {
        os << "каскад в норме, но резерв ограничен. "
           << "Возрастная структура сбалансирована, "
           << "однако требует ежегодного мониторинга. "
           << "Генератор стабилен, но запас прочности "
           << "составляет " << q.capacity_left << " связей.";
    } else if (fill >= 50) {
        os << "каскад заполнен наполовину. "
           << "Возрастная структура молодая — "
           << "связи нарабатываются, но недостаточно быстро. "
           << "Генератор может выдавать последовательности "
           << "с пониженным качеством. "
           << "Рекомендуется проверить результаты BigCrush.";
    } else if (fill >= 40) {
        os << "каскад существенно недозаполнен. "
           << "Молодая возрастная структура не обеспечивает "
           << "достаточного числа связей. "
           << "Часть статистических тестов может не пройти. "
           << "Требуются меры по стабилизации демографии.";
    } else {
        os << "каскад критически незаполнен. "
           << "Возрастная структура не позволяет каскаду "
           << "сформировать достаточное число связей. "
           << "Генератор не может гарантировать качество "
           << "выходной последовательности.";
    }

    return os.str();
}

// ============================================================
// Детальный отчёт по возрастным группам
// ============================================================

void print_group_details(const Demographics& demo, double zeta_eff) {
    std::cout << "\n  Возрастные группы:\n";
    std::cout << "  " << std::left
              << std::setw(12) << "Группа"
              << std::right
              << std::setw(14) << "Население"
              << std::setw(8)  << "Доля"
              << std::setw(8)  << "k(t)"
              << std::setw(10) << "Вклад"
              << "    График\n";
    std::cout << "  " << std::string(70, '-') << "\n";

    for (const auto& g : demo.groups) {
        double mid = (g.start_age + g.end_age) / 2.0;
        if (mid > 100) mid = 85.0;
        double weight = g.population / demo.total_population;
        double k = connections_at_age(mid);
        double contrib = weight * k;

        std::string label = std::to_string(g.start_age) + "-" +
                           (g.end_age >= 100 ? std::string("120+") :
                            std::to_string(g.end_age));

        int bar_len = static_cast<int>(contrib / zeta_eff * 50);
        std::string bar(bar_len, '#');

        std::cout << "  " << std::left
                  << std::setw(12) << label
                  << std::right
                  << std::setw(14) << static_cast<long long>(g.population)
                  << std::setw(7)  << std::fixed << std::setprecision(1)
                  << (weight * 100) << "%"
                  << std::setw(8)  << std::setprecision(2) << k
                  << std::setw(10) << contrib
                  << "    " << bar << "\n";
    }
}

// ============================================================
// Главная функция
// ============================================================

int main(int argc, char* argv[]) {
    double opzh = 0;
    bool use_world = false;
    bool interactive = false;
    std::string csv_file;

    // Разбор аргументов
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--world") {
            use_world = true;
        } else if (arg == "--opzh" && i + 1 < argc) {
            opzh = std::stod(argv[++i]);
        } else if (arg == "--file" && i + 1 < argc) {
            csv_file = argv[++i];
        } else if (arg == "--interactive") {
            interactive = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Использование:\n"
                      << "  zeta_monitor              — Беларусь (по умолчанию)\n"
                      << "  zeta_monitor --world      — Мир (ООН)\n"
                      << "  zeta_monitor --opzh 75.0  — своя ОПЖ\n"
                      << "  zeta_monitor --file d.csv — свой CSV\n"
                      << "  zeta_monitor --interactive\n";
            return 0;
        }
    }

    // Выбор демографических данных
    Demographics demo;
    if (!csv_file.empty()) {
        demo = parse_csv(csv_file, opzh > 0 ? opzh : 73.8);
    } else if (use_world) {
        demo = world_demographics_2025();
    } else {
        demo = belarus_demographics_2025();
    }

    if (opzh > 0) {
        demo.life_expectancy = opzh;
    }

    // Вычисление ζ_эфф
    double zeta = compute_zeta_eff(demo);
    QualityReport q = assess_quality(zeta, demo.life_expectancy);

    // Вывод отчёта
    std::cout << format_report(demo, q);
    std::cout << "\n  " << describe_dynamics(demo, q) << "\n";

    print_group_details(demo, zeta);

    // Проверка генератора
    std::cout << "\n  ───────────────────────────────────────────────────\n";
    std::cout << "  ПРОВЕРКА ГЕНЕРАТОРА:\n";
    std::cout << "  ОПЖ = " << std::fixed << std::setprecision(1)
              << demo.life_expectancy
              << " → period " << (q.period_ok ? "≥ 57 ✓" : "< 57 ✗")
              << "\n";
    std::cout << "  ζ_эфф = " << std::setprecision(2) << zeta
              << " → окно " << q.effective_window << " из " << MAX_CONN
              << "\n";
    std::cout << "  Вердикт: "
              << (q.grade <= QualityGrade::D_LIMITED
                  ? "генератор может выдавать НЕВЕРНУЮ последовательность"
                  : "генератор выдаёт ВЕРНУЮ последовательность")
              << "\n";

    // Демонстрация генерации
    std::cout << "\n  ───────────────────────────────────────────────────\n";
    std::cout << "  ДЕМОНСТРАЦИЯ:\n";
    CascadePRNG prng(42, demo.life_expectancy);
    prng.set_effective_window(q.effective_window);
    std::cout << "  Первые 10 значений (seed=42, period="
              << std::setprecision(1) << demo.life_expectancy
              << ", window=" << q.effective_window << "):\n  ";
    for (int i = 0; i < 10; ++i) {
        std::cout << prng.next_u64() << " ";
    }
    std::cout << "\n";

    // Интерактивный режим
    if (interactive) {
        std::cout << "\n  ───────────────────────────────────────────────────\n";
        std::cout << "  Интерактивный режим. Введите ОПЖ или 'quit':\n";
        std::string input;
        while (std::getline(std::cin, input)) {
            if (input == "quit" || input == "q") break;
            try {
                double v = std::stod(input);
                demo.life_expectancy = v;
                double z = compute_zeta_eff(demo);
                QualityReport qr = assess_quality(z, v);
                std::cout << "  ОПЖ=" << v
                          << " → period " << (qr.period_ok ? "OK" : "LOW")
                          << ", ζ_эфф=" << std::setprecision(2) << z
                          << ", статус: " << qr.status << "\n";
            } catch (...) {
                std::cout << "  Некорректный ввод\n";
            }
        }
    }

    // Код возврата: 0 — нормально, 1 — внимание, 2 — критично
    if (q.grade <= QualityGrade::E_CRITICAL) return 2;
    if (q.grade <= QualityGrade::D_LIMITED) return 1;
    return 0;
}
