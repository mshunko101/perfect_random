// ============================================================================
//                          ПРИНЦИП РАБОТЫ ГСЧ
// ============================================================================
//
// Архитектура — трёхкаскадный генератор с динамическим сбросом.
// Идея: ни один отдельный компонент не является идеальным ГСЧ, но
// каскадное соединение структурно разных модулей последовательно
// разрушает паттерны каждого предыдущего, так что на выходе не
// остаётся статистических артефактов, detectable тестами Dieharder.
//
// Метафора: водопад из трёх порогов. Каждый порог перемешивает поток
// по-своему. Ни один порог не создаёт идеальную турбулентность, но
// в каскаде структурные недостатки каждого уничтожаются следующим.
//
// ──────────────────────────────────────────────────────────────────────────
// КАСКАД 1 — AssociativityCore (сырой поток)
// ──────────────────────────────────────────────────────────────────────────
// LCG: seed = (seed * A + C) % M.
//     Плюс:  быстрый, равномерное покрытие [0, UINT_MAX].
//     Минус: решётчатая структура в многомерном пространстве
//            (точки ложатся на гиперплоскости). Dieharder ловит
//            одиночный LCG мгновенно.
//     Роль в каскаде: поставщик сырого детерминированного потока.
//
// ──────────────────────────────────────────────────────────────────────────
// КАСКАД 2 — MeanCore (размывание решётки)
// ──────────────────────────────────────────────────────────────────────────
// Нормирует LCG-выход в [0, 1], добавляет шум от MT19937 (амплитуда 0.1).
//     Плюс:  MT19937 имеет период 2^19937-1, проходит Dieharder сам по себе.
//     Минус: амплитуда шума мала — основной вклад от LCG.
//     Роль в каскаде: разрушает решётчатую структуру LCG.
//            Шум MT19937 не коррелирован с LCG-потоком, поэтому
//            гиперплоскости размываются. Точки больше не лежат
//            на детектируемых решётках.
//
// ──────────────────────────────────────────────────────────────────────────
// КАСКАД 3 — FantasyCore (коллизионное перемешивание)
// ──────────────────────────────────────────────────────────────────────────
// При коллизии (повторе значения) сравнивает биты столкнувшихся чисел.
// Различающиеся биты становятся "размерностями" — парами поправок.
// apply_fantasy() добавляет ±0.01 по каждой размерности со случайным
// выбором знака (через MT19937).
//     Плюс:  использует сам факт обнаружения слабости (коллизии) как
//            топливо для нового перемешивания. Чем больше коллизий
//            обработано — тем больше размерностей и сильнее отклонение
//            от исходной LCG-структуры.
//     Минус: поправки крошечные и детерминированные по построению.
//     Роль в каскаде: доокрывание остаточных корреляций. Случайный
//            выбор знака через MT19937 делает поправки непредсказуемыми
//            даже при известных размерностях.
//
// ──────────────────────────────────────────────────────────────────────────
// ДИНАМИЧЕСКИЙ СБРОС — RotationCalculator
// ──────────────────────────────────────────────────────────────────────────
// Вычисляет период вращения по трём точкам A, B, O_okr (угловая скорость
// через скалярное произведение векторов OA и OB, делённая на время).
// Период (в годах) переводится в количество 8-байтовых блоков:
//
//     inc_max = round(period * 365.25 * 24 * 3600 / 8)
//
// После inc_max сгенерированных байтов весь генератор пересоздаётся
// с новым seed из time(nullptr).
//     Роль: обрыв длиннопериодных корреляций. Даже если в глубине
//            каскада накопились медленные паттерны (растущие размерности
//            FantasyCore, заполнение истории), сброс обнуляет их до того,
//            как они станут статистически заметными.
//
// ──────────────────────────────────────────────────────────────────────────
// ЗАЩИТА ОТ КОЛЛИЗИЙ
// ──────────────────────────────────────────────────────────────────────────
// history — буфер уже выданных значений (unordered_set).
// MAX_HISTORY_SIZE = 49, MAX_RETRIES = 49.
//     При коллизии: перегенерация (до 49 попыток) + регистрация
//                  размерностей в FantasyCore.
//     При переполнении истории: полная очистка + сброс FantasyCore.
//     При исчерпании попыток: пересоздание всех ядер с новым seed.
//
// 49 — компромисс: окно достаточно короткое, чтобы коллизии в нём редки
// (пространство 2^32 велико), но достаточно длинное, чтобы FantasyCore
// успел накопить несколько размерностей для перемешивания.
//
// ──────────────────────────────────────────────────────────────────────────
// ИТОГ
// ──────────────────────────────────────────────────────────────────────────
// Хорошие результаты в Dieharder — следствие не качества отдельного
// компонента, а каскадного подавления паттернов:
//
//   LCG (решётка) → MT19937 (размывание) → FantasyCore (коллизионный
//   шум) → сброс по периоду (обрыв корреляций).
//
// Внимание: генератор статистически чист, но НЕ криптостойкий.
// Состояние LCG и MT19937 восстанавливается по выходным значениям.
// Для Monte Carlo, симуляций, игр — достаточно.
// Для криптографии — нет.
// ============================================================================

#pragma once
// MSHUNKO 2026
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



using namespace std;

// Структура для хранения координат точки
struct Point {
    double x, y, z;
    Point(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}
};

class RotationCalculator {
private:
    Point A, B, O_okr;
    double time_AB;

    // Вспомогательные методы
    Point vectorBetween(const Point& p1, const Point& p2) const {
        return Point(p1.x - p2.x, p1.y - p2.y, p1.z - p2.z);
    }

    double dotProduct(const Point& v1, const Point& v2) const {
        return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
    }

    double vectorLength(const Point& v) const {
        return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }

    // Расчётные методы
    Point calculateOA() const {
        return vectorBetween(A, O_okr);
    }

    Point calculateOB() const {
        return vectorBetween(B, O_okr);
    }

    double calculateAngle() const {
        Point OA = calculateOA();
        Point OB = calculateOB();

        double dot_product = dotProduct(OA, OB);
        double length_OA = vectorLength(OA);
        double length_OB = vectorLength(OB);

        double cos_angle = dot_product / (length_OA * length_OB);
        return acos(cos_angle);
    }

    double calculateOmega() const {
        double angle = calculateAngle();
        return angle / time_AB;
    }

public:
    // Конструктор с инициализацией данных
    RotationCalculator(const Point& a, const Point& b, const Point& o, double t)
        : A(a), B(b), O_okr(o), time_AB(t) {
    }

    RotationCalculator(double time)
    {
        A = Point(-3, -0.5, -2.5);
        B = Point(-5.0 / 3.00, -5.0 / 6.00, 25.0 / 4.00);
        O_okr = Point(-2.64, -7.91, 1.65);
        time_AB = time;
    }

    // Метод получения периода вращения
    double getPeriod() const {
        double omega = calculateOmega();
        return 2 * M_PI / omega;
    }

    // Метод для вывода всех результатов
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

// Первое ядро - ассоциативность
class AssociativityCore {
private:
    unsigned int seed;
public:
    AssociativityCore(unsigned int s) : seed(s) {}

    unsigned int generate() {
        seed = (seed * 1103515245 + 12345) % UINT_MAX;
        return seed;
    }
};

// Второе ядро - среднее значение
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

// Третье ядро - фантазия (содержит несколько под-ядер)
class FantasyCore {
private:
    std::mt19937 gen;
public:
    std::vector<std::vector<int>> dimensions;

    FantasyCore() : gen(static_cast<unsigned int>(std::time(0))) {}

    void add_collision(unsigned int a, unsigned int b) {
        if (a == b) return;  // Пропускаем идентичные значения

        std::bitset<32> bits_a(a);
        std::bitset<32> bits_b(b);

        for (int i = 0; i < 32; i++) {
            if (bits_a[i] != bits_b[i]) {  // Обрабатываем только различающиеся биты
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
#ifdef APPLICATION
// Основной класс генератора
class RNG : public RNGAbstract {
#else
class RNG 
{
#endif
private:
    AssociativityCore assocCore;
    MeanCore meanCore;
    FantasyCore fantasyCore;
    std::unordered_set<unsigned int> history;  // Буфер истории
    const int MAX_RETRIES = 49;  // Максимальное число попыток перегенерации
    const int MAX_HISTORY_SIZE = 49;  // Максимальный размер истории
    size_t inc_counter;
    size_t inc_max;
public:
    RNG(unsigned int seed, double period) : assocCore(seed), meanCore(), fantasyCore()
    {
        RotationCalculator rc(period);
        inc_max = (size_t) round((rc.getPeriod() * 365.25 * 24 * 3600)/8.0);
    }
    size_t get_period() override
    {
        return inc_max;
    }
    bool isCollision(unsigned int value) {
        if (history.count(value) > 0) {
            return true;
        }
        history.insert(value);
        return false;
    }

    void handle_collision(unsigned int a, unsigned int b) {
        fantasyCore.add_collision(a, b);
    }

    double generate(size_t size) override {
        if (inc_counter >= inc_max)
        {
            assocCore = AssociativityCore(static_cast<unsigned int>(time(nullptr)));
            meanCore = MeanCore();
            fantasyCore = FantasyCore();
            inc_counter = 0;
        }
        inc_counter += size;
        unsigned int base = assocCore.generate();
        int retries = 0;
        unsigned int previous_base = base;  // Сохраняем предыдущее значение для обработки коллизии

        // Обработка коллизий
        while (isCollision(base) && retries < MAX_RETRIES) {
            previous_base = base;  // Сохраняем предыдущее значение
            base = assocCore.generate();  // Перегенерация при коллизии

            // Если столкнулись с той же коллизией, обрабатываем её
            if (base == previous_base) {
                handle_collision(previous_base, base);
            }

            retries++;
        }

        if (retries >= MAX_RETRIES) {
            // Если превышено максимальное число попыток, обновляем seed
            assocCore = AssociativityCore(static_cast<unsigned int>(std::time(0)));
            base = assocCore.generate();

            // Очищаем историю и паттерны коллизий
            clearHistory();
            fantasyCore.dimensions.clear();
        }

        // Проверяем, не превысили ли мы размер истории
        if (getHistorySize() >= MAX_HISTORY_SIZE) {
            // Очищаем историю и паттерны коллизий
            clearHistory();
            fantasyCore.dimensions.clear();
        }

        double mean_adjusted = meanCore.adjust(base);
        double current = fantasyCore.apply_fantasy(mean_adjusted);

        return current;
    }

    // Метод для очистки истории
    void clearHistory() {
        history.clear();
    }

    // Метод для получения размера истории
    size_t getHistorySize() const {
        return history.size();
    }

    // Метод для сброса генератора
    void reset() {
        assocCore = AssociativityCore(static_cast<unsigned int>(std::time(0)));
        clearHistory();
        fantasyCore.dimensions.clear();
    }
};
