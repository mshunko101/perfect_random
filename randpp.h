// cascade_prng.hpp
#ifndef CASCADE_PRNG_HPP
#define CASCADE_PRNG_HPP

#include <cstdint>

class CascadePRNG
{
private:
    static constexpr uint64_t M = 2147483647ULL;       // 2^31 - 1
    static constexpr uint64_t MUL_A = 954437177ULL;     // 5/9 mod M
    static constexpr uint64_t MUL_B = 715827884ULL;    // 5/3 mod M

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

    // Смешивание всех трёх компонент для вывода
    uint32_t current_value() const {
        return (uint32_t)((a * b + c) % M);
    }

public:
    CascadePRNG(uint64_t s = 1, size_t n = 5)
        : counter(0), out_idx(0), N(n < 1 ? 1 : n)
    {
        seed(s);
    }

    void seed(uint64_t s) {
        if (s == 0) s = 1;
        a = (s * MUL_A) % M;
        if (a == 0) a = 1;
        b = (s * MUL_B) % M;
        if (b == 0) b = 1;
        c = (s * s + 1) % M;
        if (c == 0) c = 1;
        counter = 0;
        out_idx = 0;
    }

    double generate() {
        step();
        return (double)current_value() / (double)M;
    }

    // Сырое значение для битовых тестов (без потери точности double)
    uint32_t generate_raw() {
        step();
        return current_value();
    }

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

#endif // CASCADE_PRNG_HPP
