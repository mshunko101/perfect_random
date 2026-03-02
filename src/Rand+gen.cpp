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

int main() 
{

	std::setlocale(LC_ALL, "Russian");
	RNG rng_perfect(time(0));

	const size_t count = 5000000000; // 10 млрд. чисел

	// Открываем файл в бинарном режиме
	std::ofstream file("random_data.bin", std::ios::binary);
	if (!file.is_open()) 
	{
		std::cerr << "Ошибка: не удалось открыть файл для записи!\n";
		return 1;
	}

	for (size_t i = 0; i < count; ++i) 
	{
		double random_double = rng_perfect.generate();
		uint32_t random_uint = static_cast<uint32_t>(random_double * 4294967295.0);
		file.write(reinterpret_cast<const char*>(&random_uint), sizeof(random_uint));
	}

	file.close();
	std::cout << "Успешно записано " << count << " чисел в файл random_data.bin\n";
	return 0;
}









