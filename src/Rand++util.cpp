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
class RNG {
private:
	AssociativityCore assocCore;
	MeanCore meanCore;
	FantasyCore fantasyCore;

public:
	RNG(unsigned int seed) : assocCore(seed), meanCore(), fantasyCore() {}

	void handle_collision(unsigned int a, unsigned int b) {
		fantasyCore.add_collision(a, b);
	}

	double generate() {
		unsigned int base = assocCore.generate();
		double mean_adjusted = meanCore.adjust(base);
		return fantasyCore.apply_fantasy(mean_adjusted);
	}
};


int main(int argc, char* argv[])
{
    std::setlocale(LC_ALL, "Russian");

    // Проверка количества аргументов
    if (argc != 7) {
        std::cerr << "Использование: " << argv[0]
            << " <количество_чисел> <тип:double|int> <формат:txt|bin> "
            << "<min> <max> <имя_файла>\n";
        std::cerr << "Пример 1 (бинарный, double, диапазон 0–1): " << argv[0]
            << " 2000000000 double bin 0.0 1.0 random_data.bin\n";
        std::cerr << "Пример 2 (текстовый, int, диапазон 100–1000): " << argv[0]
            << " 1000000 int txt 100 1000 random_data.txt\n";
        return 1;
    }

    try {
        // Парсинг первого аргумента — количество чисел
        size_t count = std::stoull(argv[1]);
        if (count == 0) {
            throw std::invalid_argument("Количество чисел должно быть больше 0");
        }

        // Получение второго аргумента — тип данных
        std::string type_arg = argv[2];
        bool output_double = false;
        if (type_arg == "double") {
            output_double = true;
        }
        else if (type_arg != "int") {
            throw std::invalid_argument("Тип должен быть 'double' или 'int'");
        }

        // Получение третьего аргумента — формат вывода
        std::string format_arg = argv[3];
        bool binary_format = false;
        if (format_arg == "bin") {
            binary_format = true;
        }
        else if (format_arg != "txt") {
            throw std::invalid_argument("Формат должен быть 'txt' или 'bin'");
        }

        // Парсинг min и max
        double min_val = std::stod(argv[4]);
        double max_val = std::stod(argv[5]);

        if (min_val >= max_val) {
            throw std::invalid_argument("min должен быть меньше max");
        }

        // Получение четвёртого аргумента — имя файла
        std::string filename = argv[6];

        RNG rng_perfect(static_cast<unsigned int>(time(nullptr)));

        if (binary_format) {
            // Открываем файл в бинарном режиме
            std::ofstream file(filename, std::ios::binary);
            if (!file.is_open()) {
                std::cerr << "Ошибка: не удалось открыть файл '" << filename << "' для записи!\n";
                return 1;
            }

            if (output_double) {
                // Запись double значений в заданном диапазоне
                for (size_t i = 0; i < count; ++i) {
                    double random_double = rng_perfect.generate();
                    // Масштабируем от [0,1) до [min_val, max_val)
                    random_double = min_val + random_double * (max_val - min_val);
                    file.write(reinterpret_cast<const char*>(&random_double), sizeof(random_double));
                }
                std::cout << "Успешно записано " << count
                    << " чисел типа double в диапазоне [" << min_val
                    << ", " << max_val << ") в бинарный файл '" << filename << "'\n";
            }
            else {
                // Запись uint32_t значений в заданном диапазоне
                for (size_t i = 0; i < count; ++i) {
                    double random_double = rng_perfect.generate();
                    // Масштабируем от [0,1) до [min_val, max_val), затем преобразуем в int
                    random_double = min_val + random_double * (max_val - min_val);
                    uint32_t random_uint = static_cast<uint32_t>(random_double);
                    file.write(reinterpret_cast<const char*>(&random_uint), sizeof(random_uint));
                }
                std::cout << "Успешно записано " << count
                    << " чисел типа uint32_t в диапазоне [" << min_val
                    << ", " << max_val << ") в бинарный файл '" << filename << "'\n";
            }
            file.close();
        }
        else {
            // Открываем файл в текстовом режиме
            std::ofstream file(filename);
            if (!file.is_open()) {
                std::cerr << "Ошибка: не удалось открыть файл '" << filename << "' для записи!\n";
                return 1;
            }

            // Устанавливаем точность для double
            if (output_double) {
                file << std::fixed << std::setprecision(15);
            }

            for (size_t i = 0; i < count; ++i) {
                double random_double = rng_perfect.generate();
                // Масштабируем от [0,1) до [min_val, max_val)
                random_double = min_val + random_double * (max_val - min_val);

                if (output_double) {
                    file << random_double;
                }
                else {
                    uint32_t random_uint = static_cast<uint32_t>(random_double);
                    file << random_uint;
                }

                // Добавляем разделитель (пробел) для всех чисел, кроме последнего
                if (i < count - 1) {
                    file << "\n";
                }
            }
            file.close();
            std::cout << "Успешно записано " << count
                << " чисел в диапазоне [" << min_val << ", " << max_val
                << ") в текстовый файл '" << filename << "'\n";
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Ошибка: " << e.what() << "\n";
        return 1;
    }

    return 0;
}












