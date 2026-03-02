#pragma once

/*
	
	rand++ 	 	perfect random
	
	Автор:		mshunko 2026
	
	
	This is free and unencumbered software released into the public domain.

	Anyone is free to copy, modify, publish, use, compile, sell, or
	distribute this software, either in source code form or as a compiled
	binary, for any purpose, commercial or non-commercial, and by any
	means.

	In jurisdictions that recognize copyright laws, the author or authors
	of this software dedicate any and all copyright interest in the
	software to the public domain. We make this dedication for the benefit
	of the public at large and to the detriment of our heirs and
	successors. We intend this dedication to be an overt act of
	relinquishment in perpetuity of all present and future rights to this
	software under copyright law.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
	IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
	OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
	ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
	OTHER DEALINGS IN THE SOFTWARE.

	For more information, please refer to <https://unlicense.org>
*/


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
    MeanCore(double m = 1.0) : mean(m), dist(0.0, 1.0), gen(std::time(0)) {}

    double adjust(unsigned int base) {
        return (base / static_cast<double>(UINT_MAX)) * mean + dist(gen) * 0.1;
    }
};

// Третье ядро - фантазия (содержит несколько под-ядер)
class FantasyCore {
private:
    std::vector<std::vector<int>> dimensions;
    std::mt19937 gen;

public:
    FantasyCore() : gen(std::time(0)) {}

    void add_collision(unsigned int a, unsigned int b) {
        if (a == b) return;  // Пропускаем идентичные значения

        std::bitset<32> bits_a(a);
        std::bitset<32> bits_b(b);

        for (int i = 0; i < 32; i++) {
            if (bits_a[i] == bits_b[i]) {
                // Проверяем, существует ли уже такой паттерн
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

// Основной класс генератора
class RNG : public RNGAbstract {
private:
    AssociativityCore assocCore;
    MeanCore meanCore;
    FantasyCore fantasyCore;
    double m_prev;
    std::unordered_set<unsigned int> history;  // Буфер истории
    const int MAX_RETRIES = 49;  // Максимальное число попыток перегенерации

public:
    RNG(unsigned int seed) : assocCore(seed), meanCore(), fantasyCore(), m_prev(0.00) {}

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

    double generate() override {
        unsigned int base = assocCore.generate();
        int retries = 0;

        // Обработка коллизий
        while (isCollision(base) && retries < MAX_RETRIES) {
            base = assocCore.generate();  // Перегенерация при коллизии
            retries++;
        }

        if (retries >= MAX_RETRIES) {
            // Если превышено максимальное число попыток, 
            // можно применить дополнительные меры:
            // 1. Обновить seed
            // 2. Изменить параметры генератора
            // 3. Сгенерировать новое начальное состояние

            // Пример: обновление seed на основе текущего времени
            assocCore = AssociativityCore(static_cast<unsigned int>(std::time(0)));
            base = assocCore.generate();
        }

        double mean_adjusted = meanCore.adjust(base);
        double current = fantasyCore.apply_fantasy(mean_adjusted);

        if (getHistorySize() == 49)
        {
            clearHistory();
        }

        return current;
    }

    // Метод для очистки истории (может понадобиться при длительной работе)
    void clearHistory() {
        history.clear();
    }

    // Метод для получения размера истории
    size_t getHistorySize() const {
        return history.size();
    }
};

