#pragma once
// MSHUNKO 2026
#include <bitset>
#include <cstdlib>
#include <string>
#include <chrono>
#include <vector>
#include <numeric>
#include <cmath>
#include <map>
#include <algorithm>
#include <random>
#include <iostream>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <stdexcept>
#include <unordered_set>
#include <cstdint>

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
    static size_t inc_counter;
    const size_t inc_max = 1646594460;
public:
    RNG(unsigned int seed) : assocCore(seed), meanCore(), fantasyCore() {}

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
