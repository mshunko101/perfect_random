#pragma once
// MSHUNKO 2026
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// Глобальные переменные для хранения состояния
static unsigned int assoc_seed;
static double mean_value = 1.0;
static int** dimensions;
static size_t dimensions_count;
static size_t dimensions_capacity;
static unsigned int* history;
static size_t history_count;
static const size_t MAX_HISTORY = 1024;  // Увеличенный размер истории
static size_t collision_count = 0;  // Счетчик коллизий



static void clear_history();
// Инициализация ассоциативного ядра
static void init_associativity(unsigned int seed) {
    assoc_seed = seed;
}

// Генерация числа через ассоциативное ядро (линейный конгруэнтный генератор)
static unsigned int generate_associative() {
    assoc_seed = (assoc_seed * 1103515245 + 12345) % UINT_MAX;
    return assoc_seed;
}

// Инициализация хранилища измерений для «ядра фантазии»
static void init_dimensions() {
    dimensions_count = 0;
    dimensions_capacity = 100;
    dimensions = (int**)malloc(dimensions_capacity * sizeof(int*));
    if (!dimensions) {
        fprintf(stderr, "Ошибка выделения памяти для dimensions\n");
        exit(EXIT_FAILURE);
    }
}

static void init_history();
static void cleanup_dimensions();
// Инициализация всего генератора
static void init_rng(unsigned int seed) {
    srand(seed); // Инициализация rand()
    init_associativity(seed);
    init_dimensions();
    init_history();
}

// Очистка всех ресурсов
static void cleanup_rng() {
    cleanup_dimensions();
    free(history);
}


// Освобождение памяти измерений
static void cleanup_dimensions() {
    for (size_t i = 0; i < dimensions_count; i++) {
        free(dimensions[i]);
    }
    free(dimensions);
}

// Проверка, существует ли уже такое измерение
static bool dimension_exists(int val1, int val2) {
    for (size_t i = 0; i < dimensions_count; i++) {
        if (dimensions[i][0] == val1 && dimensions[i][1] == val2) {
            return true;
        }
    }
    return false;
}

// Добавление нового паттерна
static void add_pattern(unsigned int a, unsigned int b) {
    if (a == b) return;

    for (int i = 0; i < 32; i++) {
        bool bit_a = (a >> i) & 1;
        bool bit_b = (b >> i) & 1;

        if (bit_a != bit_b) {  // Анализируем различающиеся биты
            int val1 = bit_a ? 1 : 0;
            int val2 = bit_b ? 1 : 0;

            if (!dimension_exists(val1, val2)) {
                // Проверяем необходимость расширения массива
                if (dimensions_count >= dimensions_capacity) {
                    size_t new_capacity = dimensions_capacity * 2;
                    int** new_dimensions = (int**)realloc(dimensions, new_capacity * sizeof(int*));
                    if (!new_dimensions) {
                        fprintf(stderr, "Ошибка перераспределения памяти для dimensions\n");
                        cleanup_rng();
                        exit(EXIT_FAILURE);
                    }
                    dimensions = new_dimensions;
                    dimensions_capacity = new_capacity;
                }

                dimensions[dimensions_count] = (int*)malloc(2 * sizeof(int));
                if (!dimensions[dimensions_count]) {
                    fprintf(stderr, "Ошибка выделения памяти для измерения\n");
                    cleanup_rng();
                    exit(EXIT_FAILURE);
                }
                dimensions[dimensions_count][0] = val1;
                dimensions[dimensions_count][1] = val2;
                dimensions_count++;
            }
        }
    }
}

// Регистрация коллизии
static void register_collision(unsigned int value) {
    collision_count++;

    if (history_count >= MAX_HISTORY) {
        clear_history();  // Очищаем историю при заполнении
    }

    // Добавляем в историю, если места хватает
    if (history_count < MAX_HISTORY) {
        history[history_count++] = value;
    }
    // Добавляем паттерн на основе коллизии
    add_pattern(value, generate_associative());
}

// Применение «фантазии» к базовому значению
static double apply_fantasy(double base) {
    for (size_t i = 0; i < dimensions_count; i++) {
        if ((rand() % 2) == 0) {
            base += dimensions[i][0] * 0.01;
        }
        else {
            base += dimensions[i][1] * 0.01;
        }
    }
    return base;
}

// Инициализация истории коллизий
static void init_history() {
    history = (unsigned int*)malloc(MAX_HISTORY * sizeof(unsigned int));
    if (!history) {
        fprintf(stderr, "Ошибка выделения памяти для history\n");
        exit(EXIT_FAILURE);
    }
    history_count = 0;
}

// Очистка истории коллизий
static void clear_history() {
    history_count = 0;
    memset(history, 0, MAX_HISTORY * sizeof(unsigned int));
    collision_count = 0;  // Сброс счетчика коллизий
}

// Получение размера истории
static size_t get_history_size() {
    return history_count;
}

// Проверка на коллизию
static bool is_collision(unsigned int value) {
    // Ищем значение в истории
    for (size_t i = 0; i < history_count; i++) {
        if (history[i] == value) {
            return true;
        }
    }
    return false;
}

// Основная функция генерации
static double generate_random() {
    unsigned int base = generate_associative();
    int retries = 0;
    const int MAX_RETRIES = 1024;
    const unsigned int PRIME = 1664525;

    // Обработка коллизий
    while (is_collision(base) && retries < MAX_RETRIES) {
        register_collision(base);
        base = generate_associative() ^ (retries * PRIME);
        retries++;
    }

    if (retries >= MAX_RETRIES) {
        // Переинициализируем seed на основе текущего времени
        init_associativity((unsigned int)time(NULL) ^ generate_associative());
        base = generate_associative();
    }

    // Корректировка среднего значения с шумом
    double mean_adjusted = (base / (double)UINT_MAX) * mean_value +
        (rand() / (double)RAND_MAX) * 0.1;

    // Применение фантазии
    double current = apply_fantasy(mean_adjusted);

    // Очистка истории при достижении порога коллизий
    if (collision_count > MAX_HISTORY / 2) {
        clear_history();
    }

    return current;
}
