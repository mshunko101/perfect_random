// perfect_random.hpp
// MSHUNKO 2026 © Шунько Михаил Геннадьевич.
// Unified CascadePRNG — Standard, Crypto, and Monte Carlo modes in one file.
// Fully autonomous — no external dependencies.
//
// Three modes of operation:
//   1. Standard  — 31-bit, simple seed, a*b+c output      → simulations, games
//   2. Crypto    — 31-bit, SplitMix64 seed, role-permuted   → keys, nonce, RTK
//   3. MonteCarlo — 61-bit, __int128, high-precision        → Monte Carlo
//
// Usage:
//   perfect_random::CascadePRNG  rng(seed, 49, CascadePRNG::Mode::Standard);
//   perfect_random::CascadePRNG  rng(seed, 49, CascadePRNG::Mode::Crypto);
//   perfect_random::CascadePRNG64 rng(seed, 49);
//
// C++ UniformRandomBitGenerator interface:
//   CascadePRNG rng(...);
//   std::uniform_int_distribution<uint32_t> dist(0, 100);
//   dist(rng);
//
#ifndef PERFECT_RANDOM_HPP
#define PERFECT_RANDOM_HPP
#undef min
#undef max
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
#include <fstream>
#include <ctime>
#include <stdexcept>
#include <unordered_set>
#include <cstdint>

// ─── 128-bit arithmetic detection ──────────────────────────────
#if defined(__SIZEOF_INT128__) || defined(__int128)
    #define HAS_INT128 1
#else
    #define HAS_INT128 0
#endif

namespace perfect_random
{
	using namespace std;

	// ═══════════════════════════════════════════════════════════════
	//  SplitMix64 — independent hashing for Crypto-mode initialization
	// ═══════════════════════════════════════════════════════════════

	static inline uint64_t splitmix64(uint64_t& state) {
		uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
		z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
		return z ^ (z >> 31);
	}

	// ═══════════════════════════════════════════════════════════════
	//  Geometry — rotation period calculation (shared)
	// ═══════════════════════════════════════════════════════════════

	struct Point {
		double x, y, z;
		Point(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}
	};

	class RotationCalculator {
	private:
		Point A, B, O_okr;
		double time_AB;

		Point vectorBetween(const Point& p1, const Point& p2) const {
			return Point(p1.x - p2.x, p1.y - p2.y, p1.z - p2.z);
		}
		double dotProduct(const Point& v1, const Point& v2) const {
			return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
		}
		double vectorLength(const Point& v) const {
			return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
		}
		Point calculateOA() const { return vectorBetween(A, O_okr); }
		Point calculateOB() const { return vectorBetween(B, O_okr); }
		double calculateAngle() const {
			Point OA = calculateOA();
			Point OB = calculateOB();
			double cos_angle = dotProduct(OA, OB) / (vectorLength(OA) * vectorLength(OB));
			return acos(cos_angle);
		}
		double calculateOmega() const { return calculateAngle() / time_AB; }

	public:
		RotationCalculator(const Point& a, const Point& b, const Point& o, double t)
			: A(a), B(b), O_okr(o), time_AB(t) {}
		RotationCalculator(double time) {
			A = Point(-3, -0.5, -2.5);
			B = Point(-5.0 / 3.00, -5.0 / 6.00, 25.0 / 4.00);
			O_okr = Point(-2.64, -7.91, 1.65);
			time_AB = time;
		}
		double getPeriod() const { return 2 * M_PI / calculateOmega(); }
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

	// ═══════════════════════════════════════════════════════════════════════
	//  CascadePRNG — 31-bit cascade PRNG on Mersenne prime M_31
	//  Modes: Standard (simple seed, a*b+c) | Crypto (SplitMix64, role-permuted)
	// ═══════════════════════════════════════════════════════════════════════

	class CascadePRNG
	{
	public:
		enum class Mode { Standard, Crypto };
		static constexpr uint64_t M = 2147483647ULL;        // 2^31 - 1

		// C++ UniformRandomBitGenerator interface
		typedef uint32_t result_type;
		static constexpr result_type min() { return 1; }
		static constexpr result_type max() { return (result_type)(M - 1); }

	private:
		static constexpr uint64_t MUL_A = 954437177ULL;      // 5/9 mod M
		static constexpr uint64_t MUL_B = 715827884ULL;      // 5/3 mod M

		Mode     mode;
		uint64_t a, b, c;
		int      counter;
		int      out_idx;
		size_t   N;

		void step() {
			a = (a * MUL_A) % M;
			b = (b * MUL_B) % M;
			c = (c * c + a + b) % M;
			if (++counter >= (int)N) {
				uint64_t tmp = a;
				a = c;
				c = b;
				b = tmp;
				counter = 0;
				out_idx = (out_idx + 1) % 3;
			}
		}

		uint32_t current_value() const {
			if (mode == Mode::Crypto) {
				// Role-permuted output: 3 structurally identical formulas
				switch (out_idx) {
					case 0:  return (uint32_t)((a * b + c) % M);
					case 1:  return (uint32_t)((b * c + a) % M);
					default: return (uint32_t)((c * a + b) % M);
				}
			}
			return (uint32_t)((a * b + c) % M);
		}

		void seed_standard(uint64_t s) {
			if (s == 0) s = 1;
			a = (s * MUL_A) % M;
			if (a == 0) a = 1;
			b = (s * MUL_B) % M;
			if (b == 0) b = 1;
			c = (s * s + 1) % M;
			if (c == 0) c = 1;
		}

		void seed_crypto(uint64_t s) {
			if (s == 0) s = 1;
			uint64_t sm_state = s;
			a = splitmix64(sm_state) % M;
			if (a == 0) a = 1;
			b = splitmix64(sm_state) % M;
			if (b == 0) b = 1;
			c = splitmix64(sm_state) % M;
			if (c == 0) c = 1;
		}

	public:
		CascadePRNG(uint64_t s = 1, size_t n = 49, Mode m = Mode::Standard)
			: mode(m), counter(0), out_idx(0), N(n < 1 ? 1 : n)
		{
			seed(s);
		}

		void seed(uint64_t s) {
			if (mode == Mode::Crypto)
				seed_crypto(s);
			else
				seed_standard(s);
			counter = 0;
			out_idx = 0;
		}

		// Switch mode (will re-seed on next seed() call)
		void set_mode(Mode m) { mode = m; }
		Mode get_mode() const { return mode; }

		// Standard RNG methods
		double generate() {
			step();
			return (double)current_value() / (double)M;
		}

		uint32_t generate_raw() {
			step();
			return current_value();
		}

		// C++ UniformRandomBitGenerator
		result_type operator()() { return generate_raw(); }

		// State accessors
		uint64_t get_a() const { return a; }
		uint64_t get_b() const { return b; }
		uint64_t get_c() const { return c; }
		int      get_counter() const { return counter; }
		int      get_out_idx() const { return out_idx; }

		size_t get_period() {
			CascadePRNG probe(7, N);
			uint64_t init_a = probe.a;
			uint64_t init_b = probe.b;
			uint64_t init_c = probe.c;
			int      init_counter = probe.counter;
			int      init_out = probe.out_idx;

			const size_t LIMIT = 10000000;
			for (size_t i = 0; i < LIMIT; i++) {
				probe.step();
				if (probe.a == init_a &&
					probe.b == init_b &&
					probe.c == init_c &&
					probe.counter == init_counter &&
					probe.out_idx == init_out)
				{
					return i + 1;
				}
			}
			return SIZE_MAX;
		}
	};

	// ═══════════════════════════════════════════════════════════════════════
	//  CascadePRNG64 — 61-bit cascade PRNG on Mersenne prime M_61
	//  For high-precision Monte Carlo. 183 bits of state, tail resolution ~1e-18.
	//  Modes: Standard (simple seed) | Crypto (SplitMix64, role-permuted)
	// ═══════════════════════════════════════════════════════════════════════

	class CascadePRNG64
	{
	public:
		enum class Mode { Standard, Crypto };
		static constexpr uint64_t M = 2305843009213693951ULL;  // 2^61 - 1

		// C++ UniformRandomBitGenerator interface
		typedef uint64_t result_type;
		static constexpr result_type min() { return 0; }
		static constexpr result_type max() { return UINT64_MAX; }

	private:
		static constexpr uint64_t MUL_A = 0x0E38E38E38E38E39ULL;  // 5/9 mod M
		static constexpr uint64_t MUL_B = 0x0AAAAAAAAAAAAAACULL;  // 5/3 mod M

		Mode     mode;
		uint64_t a, b, c;
		int      counter;
		int      out_idx;
		size_t   N;

		static inline uint64_t mulmod(uint64_t x, uint64_t y) {
#if HAS_INT128
			return (uint64_t)((__uint128_t)x * y % M);
#else
			// MSVC fallback
			uint64_t x_lo = x & 0xFFFFFFFF, x_hi = x >> 32;
			uint64_t y_lo = y & 0xFFFFFFFF, y_hi = y >> 32;
			uint64_t lo = x_lo * y_lo;
			uint64_t mid = x_lo * y_hi + x_hi * y_lo;
			uint64_t hi = x_hi * y_hi;
			uint64_t mid_lo = mid << 32;
			uint64_t mid_hi = mid >> 32;
			uint64_t r_lo = lo + mid_lo;
			uint64_t carry = (r_lo < lo) ? 1 : 0;
			uint64_t r_hi = hi + mid_hi + carry;
			// M = 2^61-1, 2^64 mod M = 8
			uint64_t t = (r_hi * 8 + r_lo) % M;
			return t;
#endif
		}

		void step() {
			a = mulmod(a, MUL_A);
			b = mulmod(b, MUL_B);
			c = (mulmod(c, c) + a + b) % M;
			if (++counter >= (int)N) {
				uint64_t tmp = a;
				a = c;
				c = b;
				b = tmp;
				counter = 0;
				out_idx = (out_idx + 1) % 3;
			}
		}

		uint64_t current_value_mod() const {
			uint64_t v;
			if (mode == Mode::Crypto) {
				switch (out_idx) {
					case 0:  v = mulmod(a, b) + c; break;
					case 1:  v = mulmod(b, c) + a; break;
					default: v = mulmod(c, a) + b; break;
				}
			} else {
				v = mulmod(a, b) + c;
			}
			return v % M;
		}

		void seed_standard(uint64_t s) {
			if (s == 0) s = 1;
			a = mulmod(s, MUL_A);
			if (a == 0) a = 1;
			b = mulmod(s, MUL_B);
			if (b == 0) b = 1;
			c = (mulmod(s, s) + 1) % M;
			if (c == 0) c = 1;
		}

		void seed_crypto(uint64_t s) {
			if (s == 0) s = 1;
			uint64_t sm_state = s;
			a = splitmix64(sm_state) % M;
			if (a == 0) a = 1;
			b = splitmix64(sm_state) % M;
			if (b == 0) b = 1;
			c = splitmix64(sm_state) % M;
			if (c == 0) c = 1;
		}

	public:
		CascadePRNG64(uint64_t s = 1, size_t n = 49, Mode m = Mode::Standard)
			: mode(m), counter(0), out_idx(0), N(n < 1 ? 1 : n)
		{
			seed(s);
		}

		void seed(uint64_t s) {
			if (mode == Mode::Crypto)
				seed_crypto(s);
			else
				seed_standard(s);
			counter = 0;
			out_idx = 0;
		}

		void set_mode(Mode m) { mode = m; }
		Mode get_mode() const { return mode; }

		// double [0,1) — 53 bits (standard Monte Carlo)
		double generate() {
			step();
			return (double)current_value_mod() / (double)M;
		}

		// long double [0,1) — 61 bits (high-precision Monte Carlo)
		long double generate_ld() {
			step();
			return (long double)current_value_mod() / (long double)M;
		}

		// Raw 61-bit output
		uint64_t generate_raw() {
			step();
			return current_value_mod();
		}

		// Full 64-bit output (for PractRand/BigCrush)
		uint64_t generate_u64() {
			step();
			uint64_t v = current_value_mod();
			uint64_t extra = (v >> 58) & 0x7;
			return (v << 3) | extra;
		}

		// Two independent doubles per two cascade steps (2D sampling)
		void generate2(double& x, double& y) {
			step();
			uint64_t v1 = current_value_mod();
			step();
			uint64_t v2 = current_value_mod();
			x = (double)v1 / (double)M;
			y = (double)v2 / (double)M;
		}

		// C++ UniformRandomBitGenerator
		result_type operator()() { return generate_u64(); }

		uint64_t get_a() const { return a; }
		uint64_t get_b() const { return b; }
		uint64_t get_c() const { return c; }
		int      get_counter() const { return counter; }
		int      get_out_idx() const { return out_idx; }

		size_t get_period() {
			CascadePRNG64 probe(7, N);
			uint64_t init_a = probe.a;
			uint64_t init_b = probe.b;
			uint64_t init_c = probe.c;
			int      init_counter = probe.counter;
			int      init_out = probe.out_idx;

			const size_t LIMIT = 10000000;
			for (size_t i = 0; i < LIMIT; i++) {
				probe.step();
				if (probe.a == init_a &&
					probe.b == init_b &&
					probe.c == init_c &&
					probe.counter == init_counter &&
					probe.out_idx == init_out)
				{
					return i + 1;
				}
			}
			return SIZE_MAX;
		}
	};

	// ═══════════════════════════════════════════════════════════════
	//  31-bit high-level classes
	// ═══════════════════════════════════════════════════════════════

	struct SeedCascade {
		static constexpr uint64_t SEED_DELTA = 0x9E3779B97F4A7C15ULL;

		static uint64_t derive(uint64_t master_seed, int index,
			CascadePRNG::Mode mode = CascadePRNG::Mode::Standard) {
			CascadePRNG factory(master_seed + index * SEED_DELTA, 49, mode);
			return (uint64_t)factory.generate_raw() << 32 | factory.generate_raw();
		}
	};

	class AssociativityCore {
	private:
		CascadePRNG rng;
	public:
		AssociativityCore(uint64_t s, CascadePRNG::Mode mode = CascadePRNG::Mode::Standard)
			: rng(s, 48, mode) {}

		double generate() { return rng.generate(); }
		uint32_t generate_raw() { return rng.generate_raw(); }

		uint64_t state_seed() {
			uint64_t a = rng.get_a();
			uint64_t b = rng.get_b();
			uint64_t c = rng.get_c();
			return (a << 31) ^ (b << 13) ^ c ^ (rng.get_counter() * 0x100000001B3ULL);
		}
	};

	class MeanCore {
	private:
		double mean;
		CascadePRNG rng;
	public:
		MeanCore(double m, uint64_t s, CascadePRNG::Mode mode = CascadePRNG::Mode::Standard)
			: mean(m), rng(s, 49, mode) {}

		double adjust(double base) {
			return (base / static_cast<double>(CascadePRNG::M)) * mean + rng.generate();
		}
	};

	class FantasyCore {
	private:
		CascadePRNG rng;
	public:
		std::vector<std::vector<int>> dimensions;

		FantasyCore(uint64_t s, CascadePRNG::Mode mode = CascadePRNG::Mode::Standard)
			: rng(s, 50, mode) {}

		void add_collision(uint32_t a, uint32_t b) {
			if (a == b) return;
			std::bitset<32> bits_a(a);
			std::bitset<32> bits_b(b);
			for (int i = 0; i < 32; i++) {
				if (bits_a[i] != bits_b[i]) {
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
				if (rng.generate() < 0.5) {
					base += dim[0] * 0.01;
				} else {
					base += dim[1] * 0.01;
				}
			}
			return base;
		}
	};

	class RNG {
	private:
		uint64_t           master_seed;
		CascadePRNG::Mode  mode;
		AssociativityCore  assocCore;
		MeanCore           meanCore;
		FantasyCore        fantasyCore;
		std::unordered_set<uint32_t> history;
		static constexpr int MAX_RETRIES = 49;
		static constexpr int MAX_HISTORY_SIZE = 49;
		size_t             inc_counter;
		size_t             inc_max;

		void reseed(uint64_t new_seed) {
			master_seed = new_seed;
			uint64_t s1 = SeedCascade::derive(new_seed, 0, mode);
			uint64_t s2 = SeedCascade::derive(new_seed, 1, mode);
			uint64_t s3 = SeedCascade::derive(new_seed, 2, mode);
			assocCore = AssociativityCore(s1, mode);
			meanCore = MeanCore(0.5, s2, mode);
			fantasyCore = FantasyCore(s3, mode);
		}

	public:
		RNG(unsigned int seed, double period,
			CascadePRNG::Mode m = CascadePRNG::Mode::Standard)
			: master_seed(seed), mode(m),
			assocCore(SeedCascade::derive(seed, 0, m), m),
			meanCore(0.5, SeedCascade::derive(seed, 1, m), m),
			fantasyCore(SeedCascade::derive(seed, 2, m), m),
			inc_counter(0)
		{
			RotationCalculator rc(period);
			inc_max = (size_t)round((rc.getPeriod() * 365.25 * 24 * 3600) / 8.0);
		}

		size_t get_period() { return inc_max; }

		bool isCollision(uint32_t value) {
			if (history.count(value) > 0) return true;
			history.insert(value);
			return false;
		}

		void handle_collision(uint32_t a, uint32_t b) {
			fantasyCore.add_collision(a, b);
		}

		double generate() {
			if (inc_counter >= inc_max) {
				uint64_t fresh_seed = assocCore.state_seed();
				reseed(fresh_seed ^ master_seed);
				inc_counter = 0;
			}

			uint32_t base_raw = assocCore.generate_raw();
			double base = (double)base_raw / (double)CascadePRNG::M;

			int retries = 0;
			uint32_t previous_raw = base_raw;

			while (isCollision(base_raw) && retries < MAX_RETRIES) {
				previous_raw = base_raw;
				base_raw = assocCore.generate_raw();
				if (base_raw == previous_raw) {
					handle_collision(previous_raw, base_raw);
				}
				retries++;
			}

			if (retries >= MAX_RETRIES) {
				uint64_t fresh_seed = assocCore.state_seed();
				reseed(fresh_seed);
				base_raw = assocCore.generate_raw();
				base = (double)base_raw / (double)CascadePRNG::M;
				clearHistory();
				fantasyCore.dimensions.clear();
			}

			if (getHistorySize() >= MAX_HISTORY_SIZE) {
				clearHistory();
				fantasyCore.dimensions.clear();
			}

			double mean_adjusted = meanCore.adjust(base);
			double current = fantasyCore.apply_fantasy(mean_adjusted);
			inc_counter += 8;
			return current;
		}

		void clearHistory() { history.clear(); }
		size_t getHistorySize() const { return history.size(); }

		void reset() {
			reseed(master_seed + 1);
			clearHistory();
			fantasyCore.dimensions.clear();
			inc_counter = 0;
		}
	};

	// ═══════════════════════════════════════════════════════════════
	//  64-bit high-level classes (Monte Carlo)
	// ═══════════════════════════════════════════════════════════════

	struct SeedCascade64 {
		static constexpr uint64_t SEED_DELTA = 0x9E3779B97F4A7C15ULL;

		static uint64_t derive(uint64_t master_seed, int index,
			CascadePRNG64::Mode mode = CascadePRNG64::Mode::Standard) {
			CascadePRNG64 factory(master_seed + index * SEED_DELTA, 49, mode);
			uint64_t hi = factory.generate_raw();
			uint64_t lo = factory.generate_raw();
			return hi ^ lo ^ ((hi >> 3) << 3);
		}
	};

	class AssociativityCore64 {
	private:
		CascadePRNG64 rng;
	public:
		AssociativityCore64(uint64_t s, CascadePRNG64::Mode mode = CascadePRNG64::Mode::Standard)
			: rng(s, 48, mode) {}

		double generate() { return rng.generate(); }
		long double generate_ld() { return rng.generate_ld(); }
		uint64_t generate_raw() { return rng.generate_raw(); }

		uint64_t state_seed() {
			uint64_t a = rng.get_a();
			uint64_t b = rng.get_b();
			uint64_t c = rng.get_c();
			return (a << 3) ^ (b >> 7) ^ c ^ (rng.get_counter() * 0x100000001B3ULL);
		}
	};

	class MeanCore64 {
	private:
		double mean;
		CascadePRNG64 rng;
	public:
		MeanCore64(double m, uint64_t s, CascadePRNG64::Mode mode = CascadePRNG64::Mode::Standard)
			: mean(m), rng(s, 49, mode) {}

		double adjust(double base) {
			return (base / static_cast<double>(CascadePRNG64::M)) * mean + rng.generate();
		}

		long double adjust_ld(long double base) {
			return (base / static_cast<long double>(CascadePRNG64::M)) * (long double)mean + rng.generate_ld();
		}
	};

	class FantasyCore64 {
	private:
		CascadePRNG64 rng;
	public:
		std::vector<std::vector<int>> dimensions;

		FantasyCore64(uint64_t s, CascadePRNG64::Mode mode = CascadePRNG64::Mode::Standard)
			: rng(s, 50, mode) {}

		void add_collision(uint64_t a, uint64_t b) {
			if (a == b) return;
			std::bitset<64> bits_a(a);
			std::bitset<64> bits_b(b);
			for (int i = 0; i < 64; i++) {
				if (bits_a[i] != bits_b[i]) {
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
				if (rng.generate() < 0.5) {
					base += dim[0] * 0.01;
				} else {
					base += dim[1] * 0.01;
				}
			}
			return base;
		}

		long double apply_fantasy_ld(long double base) {
			for (const auto& dim : dimensions) {
				if (rng.generate_ld() < 0.5L) {
					base += dim[0] * 0.01L;
				} else {
					base += dim[1] * 0.01L;
				}
			}
			return base;
		}
	};

	class RNG64 {
	private:
		uint64_t            master_seed;
		CascadePRNG64::Mode mode;
		AssociativityCore64 assocCore;
		MeanCore64          meanCore;
		FantasyCore64       fantasyCore;
		std::unordered_set<uint64_t> history;
		static constexpr int MAX_RETRIES = 49;
		static constexpr int MAX_HISTORY_SIZE = 49;
		size_t              inc_counter;
		size_t              inc_max;

		void reseed(uint64_t new_seed) {
			master_seed = new_seed;
			uint64_t s1 = SeedCascade64::derive(new_seed, 0, mode);
			uint64_t s2 = SeedCascade64::derive(new_seed, 1, mode);
			uint64_t s3 = SeedCascade64::derive(new_seed, 2, mode);
			assocCore = AssociativityCore64(s1, mode);
			meanCore = MeanCore64(0.5, s2, mode);
			fantasyCore = FantasyCore64(s3, mode);
		}

	public:
		RNG64(unsigned int seed, double period,
			CascadePRNG64::Mode m = CascadePRNG64::Mode::Standard)
			: master_seed(seed), mode(m),
			assocCore(SeedCascade64::derive(seed, 0, m), m),
			meanCore(0.5, SeedCascade64::derive(seed, 1, m), m),
			fantasyCore(SeedCascade64::derive(seed, 2, m), m),
			inc_counter(0)
		{
			RotationCalculator rc(period);
			inc_max = (size_t)round((rc.getPeriod() * 365.25 * 24 * 3600) / 8.0);
		}

		size_t get_period() { return inc_max; }

		bool isCollision(uint64_t value) {
			if (history.count(value) > 0) return true;
			history.insert(value);
			return false;
		}

		void handle_collision(uint64_t a, uint64_t b) {
			fantasyCore.add_collision(a, b);
		}

		// double [0,1) — 53 bits
		double generate() {
			if (inc_counter >= inc_max) {
				uint64_t fresh_seed = assocCore.state_seed();
				reseed(fresh_seed ^ master_seed);
				inc_counter = 0;
			}

			uint64_t base_raw = assocCore.generate_raw();
			double base = (double)base_raw / (double)CascadePRNG64::M;

			int retries = 0;
			uint64_t previous_raw = base_raw;

			while (isCollision(base_raw) && retries < MAX_RETRIES) {
				previous_raw = base_raw;
				base_raw = assocCore.generate_raw();
				if (base_raw == previous_raw) {
					handle_collision(previous_raw, base_raw);
				}
				retries++;
			}

			if (retries >= MAX_RETRIES) {
				uint64_t fresh_seed = assocCore.state_seed();
				reseed(fresh_seed);
				base_raw = assocCore.generate_raw();
				base = (double)base_raw / (double)CascadePRNG64::M;
				clearHistory();
				fantasyCore.dimensions.clear();
			}

			if (getHistorySize() >= MAX_HISTORY_SIZE) {
				clearHistory();
				fantasyCore.dimensions.clear();
			}

			double mean_adjusted = meanCore.adjust(base);
			double current = fantasyCore.apply_fantasy(mean_adjusted);
			inc_counter += 8;
			return current;
		}

		// long double [0,1) — 61 bits (high-precision Monte Carlo)
		long double generate_ld() {
			if (inc_counter >= inc_max) {
				uint64_t fresh_seed = assocCore.state_seed();
				reseed(fresh_seed ^ master_seed);
				inc_counter = 0;
			}

			uint64_t base_raw = assocCore.generate_raw();
			long double base = (long double)base_raw / (long double)CascadePRNG64::M;

			int retries = 0;
			uint64_t previous_raw = base_raw;

			while (isCollision(base_raw) && retries < MAX_RETRIES) {
				previous_raw = base_raw;
				base_raw = assocCore.generate_raw();
				if (base_raw == previous_raw) {
					handle_collision(previous_raw, base_raw);
				}
				retries++;
			}

			if (retries >= MAX_RETRIES) {
				uint64_t fresh_seed = assocCore.state_seed();
				reseed(fresh_seed);
				base_raw = assocCore.generate_raw();
				base = (long double)base_raw / (long double)CascadePRNG64::M;
				clearHistory();
				fantasyCore.dimensions.clear();
			}

			if (getHistorySize() >= MAX_HISTORY_SIZE) {
				clearHistory();
				fantasyCore.dimensions.clear();
			}

			long double mean_adjusted = meanCore.adjust_ld(base);
			long double current = fantasyCore.apply_fantasy_ld(mean_adjusted);
			inc_counter += 8;
			return current;
		}

		void clearHistory() { history.clear(); }
		size_t getHistorySize() const { return history.size(); }

		void reset() {
			reseed(master_seed + 1);
			clearHistory();
			fantasyCore.dimensions.clear();
			inc_counter = 0;
		}
	};

} // namespace perfect_random

#endif // PERFECT_RANDOM_HPP
