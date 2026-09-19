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
#pragma once
#undef min
#undef max
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <string>
#include <iomanip>
#include <algorithm>

// ─── Возрастная группа ─────────────────────────────────────────────
struct AgeGroup {
    int age_min;
    int age_max;
    long long count;
};

// ─── Результат расчёта ζ ────────────────────────────────────────────
struct ZetaResult {
    double zeta_eff = 0.0;
    double fill_percent = 0.0;
    double reserve = 0.0;
    long long total_pop = 0;
    double median_age = 0.0;
    double period = 0.0;
    std::string status;
    std::string description;
    std::vector<std::pair<std::string, double>> group_details;
};

// ═══ Каскадная модель ═════════════════════════════════════════════
class CascadeModel {
public:
    static constexpr double B_STAR = 4.0 / 9.0;
    static constexpr double K_CASCADE = 0.13518;   // -ln|f'(b*)|
    static constexpr int    MAX_CONNECT = 49;       // 7^2 = (43+55)/2
    static constexpr double PERIOD_MIN = 57.0;
    static constexpr double PERIOD_99 = 62.6;

    // k(t) = 49 / (1 + 48 * e^(-0.1352 * t))
    static double connections_at_age(double age) {
        double exponent = -K_CASCADE * age;
        if (exponent < -700.0) return 49.0;
        double k = 49.0 / (1.0 + 48.0 * std::exp(exponent));
        return std::min(k, 49.0);
    }
};

// ═══ Монитор ζ-эфф ════════════════════════════════════════════════
class ZetaMonitor {
public:
    ZetaMonitor() = default;

    // ─── Установка данных ──────────────────────────────────────────

    void set_groups(std::vector<AgeGroup> g) {
        groups_ = std::move(g);
    }

    void set_opzh(double opzh) { opzh_ = opzh; }
    void set_year(int year) { year_ = year; }
    void set_source(std::string s) { source_ = std::move(s); }

    // ─── Встроенные данные ─────────────────────────────────────────

    void load_default_belarus() {
        groups_ = {
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
        source_ = "Белстат (оценка 2025)";
    }

    void load_default_world() {
        groups_ = {
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
        source_ = "ООН WPP 2024 (оценка 2025)";
    }

    // ─── Загрузка CSV ──────────────────────────────────────────────

    bool load_csv(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Ошибка: не удалось открыть файл " << filename << std::endl;
            return false;
        }
        groups_.clear();
        std::string line;
        std::getline(file, line);   // пропуск заголовка
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            AgeGroup g;
            char sep;
            std::istringstream ss(line);
            ss >> g.age_min >> sep >> g.age_max >> sep >> g.count;
            if (ss) groups_.push_back(g);
        }
        source_ = "CSV: " + filename;
        return !groups_.empty();
    }

    // ─── Ручной ввод групп ─────────────────────────────────────────

    void input_groups_manually() {
        groups_.clear();
        source_ = "Пользовательские данные";
        std::cout << "Введите количество групп: ";
        int n;
        std::cin >> n;
        for (int i = 0; i < n; ++i) {
            AgeGroup g;
            std::cout << "Группа " << (i + 1) << " (age_min age_max count): ";
            std::cin >> g.age_min >> g.age_max >> g.count;
            groups_.push_back(g);
        }
    }

    // ─── Расчёт ζ_эфф ──────────────────────────────────────────────

    ZetaResult calculate() const {
        ZetaResult r;
        r.period = opzh_;

        long long total = 0;
        double weighted_sum = 0.0;
        double age_sum = 0.0;

        for (const auto& g : groups_) {
            double age_mid = (g.age_min + g.age_max) / 2.0;
            double k = CascadeModel::connections_at_age(age_mid);
            weighted_sum += k * g.count;
            total += g.count;
            age_sum += age_mid * g.count;

            std::ostringstream label;
            label << g.age_min << "-" << g.age_max << " лет";
            r.group_details.push_back({ label.str(), k });
        }

        r.total_pop = total;
        r.zeta_eff = weighted_sum / total;
        r.median_age = age_sum / total;
        r.fill_percent = (r.zeta_eff / 49.0) * 100.0;
        r.reserve = 49.0 - r.zeta_eff;

        determine_status(r);
        determine_period(r);

        return r;
    }

    // ─── Вывод отчёта ──────────────────────────────────────────────

    void print_report(const ZetaResult& r) const {
        std::cout << std::string(72, '=') << std::endl;
        std::cout << "  ОТЧЁТ: Эффективный ζ-параметр популяции" << std::endl;
        std::cout << "  Источник: " << source_ << "  |  Год: " << year_ << std::endl;
        std::cout << std::string(72, '=') << std::endl;
        std::cout << std::endl;

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  ζ_эфф          = " << r.zeta_eff << std::endl;
        std::cout << "  Заполнение    = " << r.fill_percent << "% от 49" << std::endl;
        std::cout << "  Запас связей  = " << r.reserve << std::endl;
        std::cout << "  ОПЖ          = " << r.period << " лет" << std::endl;
        std::cout << "  Медиана      = " << r.median_age << " лет" << std::endl;
        std::cout << "  Население    = " << fmt_pop(r.total_pop) << std::endl;
        std::cout << std::endl;

        std::cout << "  Статус: " << r.status << std::endl;
        std::cout << std::endl;
        std::cout << "  Динамика:" << std::endl;
        std::cout << "  " << r.description << std::endl;
        std::cout << std::endl;

        print_group_details(r);

        std::cout << std::endl;
        std::cout << "  Рекомендация для ГПСЧ perfect_random:" << std::endl;
        std::cout << "    period = " << r.period << std::endl;
        if (r.period >= CascadeModel::PERIOD_99) {
            std::cout << "    Каскад насыщен — генератор стабилен." << std::endl;
        }
        else {
            std::cout << "    ВНИМАНИЕ: period ниже 99%-го порога (62.6)." << std::endl;
            std::cout << "    Генератор может не пройти статистические тесты." << std::endl;
        }

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

    // ─── Интерактивный режим ───────────────────────────────────────

    void run_interactive() {
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

        if (choice == 1) {
            load_default_belarus();
        }
        else if (choice == 2) {
            load_default_world();
        }
        else if (choice == 3) {
            std::cout << "Путь к CSV: ";
            std::string path;
            std::cin >> path;
            load_csv(path);
        }
        else if (choice == 4) {
            input_groups_manually();
        }

        std::cout << "ОПЖ (лет, по умолчанию 73.8): ";
        std::string opzh_str;
        std::cin.ignore();
        std::getline(std::cin, opzh_str);
        opzh_ = opzh_str.empty() ? 73.8 : std::stod(opzh_str);

        std::cout << "Год данных (по умолчанию 2025): ";
        std::string year_str;
        std::getline(std::cin, year_str);
        year_ = year_str.empty() ? 2025 : std::stoi(year_str);

        if (groups_.empty()) {
            std::cerr << "Ошибка: нет данных для расчёта." << std::endl;
            return;
        }

        ZetaResult result = calculate();
        print_report(result);
    }

    bool has_data() const { return !groups_.empty(); }

private:
    std::vector<AgeGroup> groups_;
    double opzh_ = 73.8;
    int    year_ = 2025;
    std::string source_ = "Белстат (оценка 2025)";

    // ─── Определение статуса ───────────────────────────────────────

    void determine_status(ZetaResult& r) const {
        if (r.zeta_eff < 20.0) {
            r.status = "КРИТИЧНО";
            r.description = "Каскад не компенсирует возмущения. Структура связей "
                "недостаточна для устойчивости. Молодая популяция с малой долей "
                "зрелых возрастных групп. Требуется срочное вмешательство — "
                "поддержка демографии, здравоохранения, удержание населения.";
        }
        else if (r.zeta_eff < 25.0) {
            r.status = "СРОЧНО";
            r.description = "Каскад частично компенсирует, но запас мал. "
                "Доля зрелых возрастных групп недостаточна. Необходимы "
                "целенаправленные меры поддержки: миграционная политика, "
                "стимулирование рождаемости, снижение смертности в трудоспособных "
                "возрастах.";
        }
        else if (r.zeta_eff < 30.0) {
            r.status = "ВНИМАНИЕ";
            r.description = "Каскад работает, но устойчивость снижается. "
                "Возрастная структура смещена в сторону молодых групп. "
                "Рекомендуется усилить меры поддержки, мониторировать "
                "демографические тренды ежегодно после публикации отчётов "
                "Белстата и ООН.";
        }
        else if (r.zeta_eff < 35.0) {
            r.status = "НОРМА";
            r.description = "Каскад устойчив. Возрастная структура "
                "обеспечивает достаточное заполнение связей. Регулярный "
                "мониторинг — раз в год. Резерв позволяет компенсировать "
                "внешние возмущения без экстренных мер.";
        }
        else if (r.zeta_eff < 40.0) {
            r.status = "ХОРОШО";
            r.description = "Каскад устойчив с запасом. Сбалансированная "
                "возрастная структура. Меры поддержки можно поддерживать "
                "в штатном режиме. Дополнительных вмешательств не требуется.";
        }
        else {
            r.status = "ОПТИМУМ";
            r.description = "Каскад в зоне полного насыщения. "
                "Большая часть популяции достигла структурного предела 49 "
                "связей. Система максимально устойчива. Риск — только от "
                "панических сценариев (усиление возмущений вместо затухания).";
        }
    }

    // ─── Рекомендация по period ───────────────────────────────────

    void determine_period(ZetaResult& r) const {
        if (opzh_ < CascadeModel::PERIOD_MIN) {
            r.period = CascadeModel::PERIOD_MIN;
            r.description += " ВНИМАНИЕ: ОПЖ ниже минимального порога "
                "каскада (57 лет). Period установлен на минимум.";
        }
        else {
            r.period = opzh_;
        }
    }

    // ─── Детали по группам ────────────────────────────────────────

    void print_group_details(const ZetaResult& r) const {
        std::cout << "  Возрастные группы:" << std::endl;
        std::cout << std::string(72, '-') << std::endl;
        std::cout << "  " << std::left << std::setw(18) << "Группа"
            << std::setw(12) << "Связей k(t)" << std::setw(12) << "% от 49"
            << "  Бар" << std::endl;
        std::cout << std::string(72, '-') << std::endl;
        for (const auto& d : r.group_details) {
            double pct = (d.second / 49.0) * 100.0;
            int bar_len = static_cast<int>(pct / 2);
            std::string bar(bar_len, '#');
            std::cout << "  " << std::left << std::setw(18) << d.first
                << std::setw(12) << std::fixed << std::setprecision(1) << d.second
                << std::setw(12) << std::fixed << std::setprecision(1) << pct << "%"
                << "  " << bar << std::endl;
        }
        std::cout << std::string(72, '-') << std::endl;
    }

    // ─── Форматирование числа с разделителями ───────────────────

    static std::string fmt_pop(long long n) {
        std::string s = std::to_string(n);
        std::string result;
        int count = 0;
        for (int i = static_cast<int>(s.length()) - 1; i >= 0; --i) {
            if (count > 0 && count % 3 == 0) result = "'" + result;
            result = s[i] + result;
            ++count;
        }
        return result;
    }
};

// ═══ main ═════════════════════════════════════════════════════════

static void print_help() {
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
}

int _main(int argc, char* argv[]) {
    ZetaMonitor monitor;

    double opzh = 73.8;
    std::string csv_path;
    int year = 2025;
    bool interactive = false;
    bool use_world = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--interactive" || arg == "-i") {
            interactive = true;
        }
        else if (arg == "--opzh" && i + 1 < argc) {
            opzh = std::stod(argv[++i]);
        }
        else if (arg == "--groups" && i + 1 < argc) {
            csv_path = argv[++i];
        }
        else if (arg == "--year" && i + 1 < argc) {
            year = std::stoi(argv[++i]);
        }
        else if (arg == "--source" && i + 1 < argc) {
            monitor.set_source(argv[++i]);
        }
        else if (arg == "--world") {
            use_world = true;
        }
        else if (arg == "--help" || arg == "-h") {
            print_help();
            return 0;
        }
    }

    if (interactive) {
        monitor.run_interactive();
        return 0;
    }

    monitor.set_opzh(opzh);
    monitor.set_year(year);

    if (!csv_path.empty()) {
        if (!monitor.load_csv(csv_path)) {
            std::cerr << "Ошибка: нет данных для расчёта." << std::endl;
            return 1;
        }
    }
    else if (use_world) {
        monitor.load_default_world();
    }
    else {
        monitor.load_default_belarus();
    }

    if (!monitor.has_data()) {
        std::cerr << "Ошибка: нет данных для расчёта." << std::endl;
        return 1;
    }

    ZetaResult result = monitor.calculate();
    monitor.print_report(result);

    return 0;
}
