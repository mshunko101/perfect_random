#include <iostream>
#include <vector>
#include <bitset>
#include <cstdlib>
#include <ctime>
#include <random>
#include <string>
#include <chrono>

class RNGAbstract {
public:
    virtual double generate() = 0;
};


// Базовый класс генератора
class RandomEngine :public RNGAbstract {
private:
    std::mt19937_64 engine;
    std::uniform_real_distribution<double> distribution;

public:
    RandomEngine(unsigned long long seed = std::chrono::system_clock::now().time_since_epoch().count())
        : engine(seed), distribution(0.0, 1.0) {
    }

    double generate() override{
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
        std::bitset<32> bits_a(a);
        std::bitset<32> bits_b(b);

        for (int i = 0; i < 32; i++) {
            if (bits_a[i] == bits_b[i]) {
                // Создаем новое под-ядро фантазии
                dimensions.push_back({
                    bits_a[i] ? 1 : 0,
                    bits_b[i] ? 0 : 1
                    });
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

public:
    RNG(unsigned int seed) : assocCore(seed), meanCore(), fantasyCore() {}

    void handle_collision(unsigned int a, unsigned int b) {
        fantasyCore.add_collision(a, b);
    }

    double generate() override{
        unsigned int base = assocCore.generate();
        double mean_adjusted = meanCore.adjust(base);
        return fantasyCore.apply_fantasy(mean_adjusted);
    }
};
 







#include <vector>
#include <iostream>
#include <numeric>
#include <cmath>
#include <map>
#include <algorithm>
#include <random>
#include <ctime>

// Генерация тестовой последовательности
std::vector<double> generateTestSequence(RNGAbstract& rng, int size) {
    std::vector<double> sequence;
    for (int i = 0; i < size; i++) {
        sequence.push_back(rng.generate());
    }
    return sequence;
}

// Частотный тест
void frequencyTest(const std::vector<double>& sequence) {
    std::map<int, int> buckets;
    for (double val : sequence) {
        int bucket = static_cast<int>(val * 10);
        buckets[bucket]++;
    }

    std::cout << "frequ disperse:\n";
    for (int i = 0; i < 10; i++) {
        std::cout << "[" << i / 10.0 << "-" << (i + 1) / 10.0 << "]: "
            << buckets[i] << " (" << buckets[i] * 100.0 / sequence.size() << "%)\n";
    }
}

// Тест на серийность
void runsTest(const std::vector<double>& sequence) {
    int up = 0, down = 0, equal = 0;
    for (size_t i = 1; i < sequence.size(); i++) {
        if (sequence[i] > sequence[i - 1]) up++;
        else if (sequence[i] < sequence[i - 1]) down++;
        else equal++;
    }
    std::cout << "Test series:\n";
    std::cout << "Upper: " << up << "\n";
    std::cout << "Lowwer: " << down << "\n";
    std::cout << "Equ: " << equal << "\n";
}

// Тест на автокорреляцию
void autocorrelationTest(const std::vector<double>& sequence, int lag) {
    double mean = std::accumulate(sequence.begin(), sequence.end(), 0.0) / sequence.size();
    double var = 0.0;
    for (double val : sequence) var += (val - mean) * (val - mean);
    var /= sequence.size();

    double corr = 0.0;
    for (size_t i = 0; i < sequence.size() - lag; i++) {
        corr += (sequence[i] - mean) * (sequence[i + lag] - mean);
    }
    corr /= (sequence.size() - lag) * var;

    std::cout << "Autocorelation  " << lag << "): " << corr << "\n";
}

// Тест на равномерность
void uniformityTest(const std::vector<double>& sequence) {
    double mean = std::accumulate(sequence.begin(), sequence.end(), 0.0) / sequence.size();
    double var = 0.0;
    for (double val : sequence) var += (val - mean) * (val - mean);
    var /= sequence.size();

    std::cout << "RAvnomernost:\n";
    std::cout << "Average: " << mean << " (awaitable: 0.5)\n";
    std::cout << "Disperse: " << var << " (awaitable: ~0.0833)\n";
}

// Продолжение pokerTest
void pokerTest(const std::vector<double>& sequence) {
    std::map<std::string, int> combinations;
    for (size_t i = 0; i < sequence.size() - 3; i += 4) {
        std::string combo = "";
        for (int j = 0; j < 4; j++) {
            combo += std::to_string(static_cast<int>(sequence[i + j] * 10));
        }
        combinations[combo]++;
    }

    std::cout << "Poker test:\n";
    for (const auto& pair : combinations) {
        std::cout << "Combination " << pair.first << ": " << pair.second << " times\n";
    }
}

// Тест на спектральный анализ
void spectralTest(const std::vector<double>& sequence) {
    std::vector<double> freq;
    // Простой спектральный анализ (можно использовать FFT для более точного)
    for (size_t i = 1; i < sequence.size() / 2; i++) {
        double sum = 0.0;
        for (size_t j = 0; j < sequence.size(); j++) {
            sum += sequence[j] * std::cos(2 * 3.14 * i * j / sequence.size());
        }
        freq.push_back(std::abs(sum));
    }

    std::cout << "Spectra:\n";
    std::cout << "Max freq: " << *std::max_element(freq.begin(), freq.end()) << "\n";
}

// Тест на сжимаемость (грубая оценка)
void compressibilityTest(const std::vector<double>& sequence) {
    // Простая оценка через энтропию
    std::map<int, int> counts;
    for (double val : sequence) {
        counts[static_cast<int>(val * 10)]++;
    }

    double entropy = 0.0;
    for (const auto& pair : counts) {
        double p = pair.second / static_cast<double>(sequence.size());
        entropy -= p * std::log2(p);
    }

    std::cout << "Entropy: " << entropy << " бит\n";
    std::cout << "Awaitable entropy: ~3.32 бит\n";
}



// Функция для построения гистограммы
void plotHistogram(const std::vector<double>& sequence) {
    std::map<int, int> buckets;
    for (double val : sequence) {
        buckets[static_cast<int>(val * 10)]++;
    }

    std::cout << "Gistogram:\n";
    for (int i = 0; i < 10; i++) {
        std::cout << i / 10.0 << " - " << (i + 1) / 10.0 << ": ";
        for (int j = 0; j < buckets[i] / 100; j++) {
            std::cout << "*";
        }
        std::cout << " (" << buckets[i] << ")\n";
    }
}

// Тест на визуальную проверку пар
void pairPlotTest(const std::vector<double>& sequence) {
    // Проверяем пары последовательных чисел
    for (size_t i = 0; i < sequence.size() - 1; i++) {
        // Здесь можно добавить визуализацию или анализ пар
    }
}





// Тест на периодичность
void periodicityTest(const std::vector<double>& sequence) {
    std::map<double, int> occurrences;
    for (double val : sequence) {
        occurrences[val]++;
    }

    int max_period = 0;
    for (const auto& pair : occurrences) {
        if (pair.second > max_period) {
            max_period = pair.second;
        }
    }

    std::cout << "Periodic test:\n";
    std::cout << "Max repeatable value: " << max_period << " times\n";
}

// Тест на последовательности одинаковых битов
void sameBitsSequenceTest(const std::vector<double>& sequence) {
    std::vector<std::string> binaryStrings;
    for (double val : sequence) {
        binaryStrings.push_back(std::bitset<32>(static_cast<unsigned int>(val * UINT_MAX)).to_string());
    }

    int max_ones = 0, max_zeros = 0;
    for (const auto& str : binaryStrings) {
        int current_ones = 0, current_zeros = 0;
        for (char c : str) {
            if (c == '1') {
                current_ones++;
                current_zeros = 0;
            }
            else {
                current_zeros++;
                current_ones = 0;
            }
            max_ones = std::max(max_ones, current_ones);
            max_zeros = std::max(max_zeros, current_zeros);
        }
    }

    std::cout << "Sequnce bit test:\n";
    std::cout << "maxs 1: " << max_ones << "\n";
    std::cout << "maxs 0: " << max_zeros << "\n";
}






void runAllTests(RNGAbstract & rng, int sampleSize) {
    std::vector<double> testData = generateTestSequence(rng, sampleSize);

    std::cout << "\n--- Main test ---\n";
    uniformityTest(testData);
    frequencyTest(testData);
    runsTest(testData);

    std::cout << "\n--- Corelation tests ---\n";
    autocorrelationTest(testData, 1);
    autocorrelationTest(testData, 5);
    autocorrelationTest(testData, 10);

    std::cout << "\n--- Exended tests ---\n";
    //pokerTest(testData);
    spectralTest(testData);
    compressibilityTest(testData);

    std::cout << "\n--- Special tests ---\n";
    periodicityTest(testData);
    sameBitsSequenceTest(testData);

    std::cout << "\n--- Visual ---\n";
    plotHistogram(testData);
}



int main() {
    RNG rng_perfect(time(0));
    std::cout << "START FOR perfect_random\n";
    runAllTests(rng_perfect, 10000000);
    std::cout << "==================================================";
    StdGeneralRNG rng_general(time(0));
    std::cout << "START FOR StdGeneralRNG\n";
    runAllTests(rng_general, 10000000);
    std::cout << "==================================================";
    std::cout << "MSHUNKO (C) prod 2013-2026\n";
    return 0;
}









