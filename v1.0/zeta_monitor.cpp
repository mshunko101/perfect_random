// zeta_monitor.cpp
// Вспомогательная утилита для расчёта ζ_эфф популяции
// и определения period для ГПСЧ perfect_random
//
// Автор: Шунько Михаил Геннадьевич
// Проект: «Каскадная модель развёртки» (2021–2026)
//
// Использование:
//   zeta_monitor [опции]
//
// Опции:
//   --opzh <значение>      Ожидаемая продолжительность жизни (лет)
//   --groups <файл>        Файл с возрастными группами (CSV)
//   --year <год>           Год данных (для отчёта)
//   --source <название>    Источник данных (ОН/Белстат/...)
//   --interactive          Интерактивный режим
//
// Формат CSV (groups):
//   age_min,age_max,count
//   0,4,350000
//   5,9,380000
//   ...
//
// Если файл не указан — используются встроенные данные Беларуси (оценка 2025)

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <string>
#include <iomanip>
#include <algorithm>

// ─── Каскадные константы (из perfect_random.hpp) ──────────────────
constexpr double B_STAR      = 4.0 / 9.0;
constexpr double K_CASCADE    = 0.13518;   // -ln|f'(b*)|
constexpr int    MAX_CONNECT  = 49;        // 7^2 = (43+55)/2
constexpr double PERIOD_MIN   = 57.0;
constexpr double PERIOD_99    = 62.6;

// ─── Возрастная группа ─────────────────────────────────────────────
struct AgeGroup {
    int age_min;
    int age_max;
    long long count;    // численность группы
};

// ─── Логистическая модель связей ───────────────────────────────────
// k(t) = 49 / (1 + 48 * e^(-0.1352 * t))
double connections_at_age(double age) {
    double exponent = -K_CASCADE * age;
    if (exponent < -700.0) return 49.0;
    double k = 49.0 / (1.0 + 48.0 * std::exp(exponent));
    return std::min(k, 49.0);
}

// ─── Расчёт ζ_эфф для популяции ────────────────────────────────────
struct ZetaResult {
    double zeta_eff;           // эффективный ζ
    double fill_percent;      // процент заполнения от 49
    double reserve;           // запас (49 - ζ_эфф)
    long long total_pop;      // общая численность
    double median_age;        // медианный возраст (оценка)
    double period;            // рекомендуемый period для ГПСЧ
    std::string status;       // статус: КРИТИЧНО / СРОЧНО / ВНИМАНИЕ / НОРМА / ХОРОШО
    std::string description;  // текстовое описание динамики
    std::vector<std::pair<std::string, double>> group_details;  // по группам
};

ZetaResult calculate_zeta(const std::vector<AgeGroup>& groups, double opzh) {
    ZetaResult result;
    result.total_pop = 0;
    result.zeta_eff = 0.0;
    result.period = opzh;

    long long total = 0;
    double weighted_sum = 0.0;
    double age_sum = 0.0;

    // Средние возраста для стандартных 5-летних групп
    for (const auto& g : groups) {
        double age_mid = (g.age_min + g.age_max) / 2.0;
        double k = connections_at_age(age_mid);
        double contribution = k * g.count;
        weighted_sum += contribution;
        total += g.count;
        age_sum += age_mid * g.count;

        // Детали по группе
        std::ostringstream label;
        label << g.age_min << "-" << g.age_max << " лет";
        result.group_details.push_back({label.str(), k});
    }

    result.total_pop = total;
    result.zeta_eff = weighted_sum / total;
    result.median_age = age_sum / total;
    result.fill_percent = (result.zeta_eff / 49.0) * 100.0;
    result.reserve = 49.0 - result.zeta_eff;

    // Определение статуса и описания
    if (result.zeta_eff < 20.0) {
        result.status = "КРИТИЧНО";
        result.description = "Каскад не компенсирует возмущения. Структура связей "
            "недостаточна для устойчивости. Молодая популяция с малой долей "
            "зрелых возрастных групп. Требуется срочное вмешательство — "
            "поддержка демографии, здравоохранения, удержание населения.";
    } else if (result.zeta_eff < 25.0) {
        result.status = "СРОЧНО";
        result.description = "Каскад частично компенсирует, но запас мал. "
            "Доля зрелых возрастных групп недостаточна. Необходимы "
            "целенаправленные меры поддержки: миграционная политика, "
            "стимулирование рождаемости, снижение смертности в трудоспособных "
            "возрастах.";
    } else if (result.zeta_eff < 30.0) {
        result.status = "ВНИМАНИЕ";
        result.description = "Каскад работает, но устойчивость снижается. "
            "Возрастная структура смещена в сторону молодых групп. "
            "Рекомендуется усилить меры поддержки, мониторировать "
            "демографические тренды ежегодно после публикации отчётов "
            "Белстата и ООН.";
    } else if (result.zeta_eff < 35.0) {
        result.status = "НОРМА";
        result.description = "Каскад устойчив. Возрастная структура "
            "обеспечивает достаточное заполнение связей. Регулярный "
            "мониторинг — раз в год. Резерв позволяет компенсировать "
            "внешние возмущения без экстренных мер.";
    } else if (result.zeta_eff < 40.0) {
        result.status = "ХОРОШО";
        result.description = "Каскад устойчив с запасом. Сбалансированная "
            "возрастная структура. Меры поддержки можно维持 в штатном "
            "режиме. Дополнительных вмешательств не требуется.";
    } else {
        result.status = "ОПТИМУМ";
        result.description = "Каскад в зоне полного насыщения. "
            "Большая часть популяции достигла структурного предела 49 "
            "связей. Система максимально устойчива. Риск — только от "
            "панических сценариев (усиление возмущений вместо затухания).";
    }

    // Рекомендация по period
    if (opzh < PERIOD_MIN) {
        result.period = PERIOD_MIN;
        result.description += " ВНИМАНИЕ: ОПЖ ниже минимального порога "
            "каскада (57 лет). Period установлен на минимум.";
    } else {
        result.period = opzh;
    }

    return result;
}

// ─── Встроенные данные Беларуси (оценка 2025, Белстат) ─────────────
std::vector<AgeGroup> default_belarus() {
    return {
        {0, 4,   350000},
        {5, 9,   380000},
        {10, 14, 390000},
        {15, 19, 360000},
        {20, 24, 340000},
        {25, 29, 360000},
        {30, 34, 400000},
        {35, 39, 440000},
        {40, 44, 460000},
        {45, 49, 480000},
        {50, 54, 470000},
        {55, 59, 440000},
        {60, 64, 400000},
        {65, 69, 340000},
        {70, 74, 260000},
        {75, 79, 180000},
        {80, 120, 160000},
    };
}

// ─── Встроенные данные мира (оценка 2025, ООН) ─────────────────────
std::vector<AgeGroup> default_world() {
    return {
        {0, 4,   680000000},
        {5, 9,   640000000},
        {10, 14, 620000000},
        {15, 19, 600000000},
        {20, 24, 590000000},
        {25, 29, 600000000},
        {30, 34, 620000000},
        {35, 39, 610000000},
        {40, 44, 580000000},
        {45, 49, 560000000},
        {50, 54, 530000000},
        {55, 59, 480000000},
        {60, 64, 420000000},
        {65, 69, 350000000},
        {70, 74, 270000000},
        {75, 79, 190000000},
        {80, 120, 150000000},
    };
}

// ─── Загрузка CSV ──────────────────────────────────────────────────
std::vector<AgeGroup> load_csv(const std::string& filename) {
    std::vector<AgeGroup> groups;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Ошибка: не удалось открыть файл " << filename << std::endl;
        return groups;
    }
    std::string line;
    // Пропуск заголовка
    std::getline(file, line);
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        AgeGroup g;
        char sep;
        std::istringstream ss(line);
        ss >> g.age_min >> sep >> g.age_max >> sep >> g.count;
        if (ss) groups.push_back(g);
    }
    return groups;
}

// ─── Форматирование числа с разделителями ──────────────────────────
std::string fmt_pop(long long n) {
    std::string s = std::to_string(n);
    std::string result;
    int count = 0;
    for (int i = s.length() - 1; i >= 0; i--) {
        if (count > 0 && count % 3 == 0) result = "'" + result;
        result = s[i] + result;
        count++;
    }
    return result;
}

// ─── Вывод отчёта ──────────────────────────────────────────────────
void print_report(const ZetaResult& result, const std::string& source, int year) {
    std::cout << std::string(72, '=') << std::endl;
    std::cout << "  ОТЧЁТ: Эффективный ζ-параметр популяции" << std::endl;
    std::cout << "  Источник: " << source << "  |  Год: " << year << std::endl;
    std::cout << std::string(72, '=') << std::endl;
    std::cout << std::endl;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  ζ_эфф          = " << result.zeta_eff << std::endl;
    std::cout << "  Заполнение    = " << result.fill_percent << "% от 49" << std::endl;
    std::cout << "  Запас связей  = " << result.reserve << std::endl;
    std::cout << "  ОПЖ          = " << result.period << " лет" << std::endl;
    std::cout << "  Медиана      = " << result.median_age << " лет" << std::endl;
    std::cout << "  Население    = " << fmt_pop(result.total_pop) << std::endl;
    std::cout << std::endl;

    std::cout << "  Статус: " << result.status << std::endl;
    std::cout << std::endl;
    std::cout << "  Динамика:" << std::endl;
    // Разбиваем описание по предложениям
    std::string desc = result.description;
    std::cout << "  " << desc << std::endl;
    std::cout << std::endl;

    // Детали по группам
    std::cout << "  Возрастные группы:" << std::endl;
    std::cout << std::string(72, '-') << std::endl;
    std::cout << "  " << std::left << std::setw(18) << "Группа"
              << std::setw(12) << "Связей k(t)" << std::setw(12) << "% от 49"
              << "  Бар" << std::endl;
    std::cout << std::string(72, '-') << std::endl;
    for (const auto& d : result.group_details) {
        double pct = (d.second / 49.0) * 100.0;
        int bar_len = static_cast<int>(pct / 2);
        std::string bar(bar_len, '#');
        std::cout << "  " << std::left << std::setw(18) << d.first
                  << std::setw(12) << std::fixed << std::setprecision(1) << d.second
                  << std::setw(12) << std::fixed << std::setprecision(1) << pct << "%"
                  << "  " << bar << std::endl;
    }
    std::cout << std::string(72, '-') << std::endl;

    // Рекомендация по period для ГПСЧ
    std::cout << std::endl;
    std::cout << "  Рекомендация для ГПСЧ perfect_random:" << std::endl;
    std::cout << "    period = " << result.period << std::endl;
    if (result.period >= PERIOD_99) {
        std::cout << "    Каскад насыщен — генератор стабилен." << std::endl;
    } else {
        std::cout << "    ВНИМАНИЕ: period ниже 99%-го порога (62.6)." << std::endl;
        std::cout << "    Генератор может не пройти статистические тесты." << std::endl;
    }

    // Пороги
    std::cout << std::endl;
    std::cout << "  Пороги ζ_эфф:" << std::endl;
    std::cout << "    < 20  — Критическая зона" << std::endl;
    std::cout << "    20-25 — Срочные меры" << std::endl;
    std::cout << "    25-30 — Усилить меры поддержки" << std::endl;
    std::cout << "    30-35 — Регулярный мониторинг (НОРМА)" << std::endl;
    std::cout << "    35-40 — Устойчивое состояние" << std::endl;
    std::cout << "    > 40  — Полный запас прочности" << std::endl;

    std::cout << std::endl;
    std::cout << std::string(72, '=') << std::endl;
}

// ─── Интерактивный режим ───────────────────────────────────────────
void interactive_mode() {
    std::cout << "=== Утилита расчёта ζ_эфф (интерактивный режим) ===" << std::endl;
    std::cout << std::endl;

    std::cout << "Выберите источник данных:" << std::endl;
    std::cout << "  1. Беларусь (встроенные данные, оценка 2025)" << std::endl;
    std::cout << "  2. Мир (встроенные данные, оценка 2025)" << std::endl;
    std::cout << "  3. Загрузить CSV файл" << std::endl;
    std::cout << "  4. Ввести вручную" << std::endl;
    std::cout << "Выбор: ";

    int choice;
    std::cin >> choice;

    std::vector<AgeGroup> groups;
    std::string source = "Пользовательские данные";

    if (choice == 1) {
        groups = default_belarus();
        source = "Белстат (оценка 2025)";
    } else if (choice == 2) {
        groups = default_world();
        source = "ООН WPP 2024 (оценка 2025)";
    } else if (choice == 3) {
        std::cout << "Путь к CSV: ";
        std::string path;
        std::cin >> path;
        groups = load_csv(path);
        source = "CSV: " + path;
    } else if (choice == 4) {
        std::cout << "Введите количество групп: ";
        int n;
        std::cin >> n;
        for (int i = 0; i < n; i++) {
            AgeGroup g;
            std::cout << "Группа " << (i+1) << " (age_min age_max count): ";
            std::cin >> g.age_min >> g.age_max >> g.count;
            groups.push_back(g);
        }
    }

    std::cout << "ОПЖ (лет, по умолчанию 73.8): ";
    double opzh;
    std::string opzh_str;
    std::cin.ignore();
    std::getline(std::cin, opzh_str);
    if (opzh_str.empty()) {
        opzh = 73.8;
    } else {
        opzh = std::stod(opzh_str);
    }

    std::cout << "Год данных (по умолчанию 2025): ";
    std::string year_str;
    std::getline(std::cin, year_str);
    int year = year_str.empty() ? 2025 : std::stoi(year_str);

    ZetaResult result = calculate_zeta(groups, opzh);
    print_report(result, source, year);
}

// ─── main ──────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    // Разбор аргументов
    double opzh = 73.8;
    std::string csv_path;
    std::string source = "Белстат (оценка 2025)";
    int year = 2025;
    bool interactive = false;
    bool use_world = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--interactive" || arg == "-i") {
            interactive = true;
        } else if (arg == "--opzh" && i+1 < argc) {
            opzh = std::stod(argv[++i]);
        } else if (arg == "--groups" && i+1 < argc) {
            csv_path = argv[++i];
        } else if (arg == "--year" && i+1 < argc) {
            year = std::stoi(argv[++i]);
        } else if (arg == "--source" && i+1 < argc) {
            source = argv[++i];
        } else if (arg == "--world") {
            use_world = true;
            source = "ООН WPP 2024";
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Использование: zeta_monitor [опции]" << std::endl;
            std::cout << "  --opzh <значение>      ОПЖ (лет), по умолчанию 73.8" << std::endl;
            std::cout << "  --groups <файл>        CSV с возрастными группами" << std::endl;
            std::cout << "  --year <год>           Год данных" << std::endl;
            std::cout << "  --source <название>     Источник данных" << std::endl;
            std::cout << "  --world                Использовать данные мира (ООН)" << std::endl;
            std::cout << "  --interactive          Интерактивный режим" << std::endl;
            std::cout << std::endl;
            std::cout << "Формат CSV:" << std::endl;
            std::cout << "  age_min,age_max,count" << std::endl;
            std::cout << "  0,4,350000" << std::endl;
            std::cout << "  5,9,380000" << std::endl;
            std::cout << "  ..." << std::endl;
            return 0;
        }
    }

    if (interactive) {
        interactive_mode();
        return 0;
    }

    // Загрузка данных
    std::vector<AgeGroup> groups;
    if (!csv_path.empty()) {
        groups = load_csv(csv_path);
        source = "CSV: " + csv_path;
    } else if (use_world) {
        groups = default_world();
        source = "ООН WPP 2024 (оценка 2025)";
    } else {
        groups = default_belarus();
        source = "Белстат (оценка 2025)";
    }

    if (groups.empty()) {
        std::cerr << "Ошибка: нет данных для расчёта." << std::endl;
        return 1;
    }

    ZetaResult result = calculate_zeta(groups, opzh);
    print_report(result, source, year);

    return 0;
}
