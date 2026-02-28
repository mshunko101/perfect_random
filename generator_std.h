#pragma once
#include <iostream>
#include <vector>
#include <bitset>
#include <cstdlib>
#include <ctime>
#include <random>
#include <string>
#include <chrono>



// Базовый класс генератора
class RandomEngine :public RNGAbstract {
private:
    std::mt19937_64 engine;
    std::uniform_real_distribution<double> distribution;

public:
    RandomEngine(unsigned long long seed = std::chrono::system_clock::now().time_since_epoch().count())
        : engine(seed), distribution(0.0, 1.0) {
    }

    double generate() override {
        return distribution(engine);
    }

    // Дополнительные методы генерации
    int generateInt(int min, int max) {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(engine);
    }

    double generateRange(double min, double max) {
        std::uniform_real_distribution<double> dist(min, max);
        return dist(engine);
    }
};

// Улучшенный генератор с дополнительными функциями
class StdGeneralRNG : public RNGAbstract {
private:
    RandomEngine baseEngine;
    std::vector<double> entropyBuffer;
    size_t bufferSize = 1024;

public:
    StdGeneralRNG(unsigned long long seed = std::chrono::system_clock::now().time_since_epoch().count())
        : baseEngine(seed) {
        initializeEntropyBuffer();
    }

    void initializeEntropyBuffer() {
        entropyBuffer.resize(bufferSize);
        for (size_t i = 0; i < bufferSize; ++i) {
            entropyBuffer[i] = baseEngine.generate();
        }
    }

    double generate() override {
        // Использование нескольких источников энтропии
        double result = baseEngine.generate();
        size_t index = baseEngine.generateInt(0, bufferSize - 1);
        result = (result + entropyBuffer[index]) / 2.0;

        // Обновление буфера энтропии
        entropyBuffer[index] = baseEngine.generate();

        return result;
    }

    // Метод для заполнения буфера энтропией
    void addEntropy(double value) {
        size_t index = baseEngine.generateInt(0, bufferSize - 1);
        entropyBuffer[index] = value;
    }
};