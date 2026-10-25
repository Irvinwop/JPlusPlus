#include <bits/stdc++.h>
using namespace std;

using Small = mersenne_twister_engine<unsigned, 13, 17, 7, 6, 0x11a3, 3, 0x1fff, 4,
                                     0x1300, 5, 0x1b00, 2, 1237>;
using Word32 = mersenne_twister_engine<unsigned, 32, 624, 397, 31, 0x9908b0dfU, 11,
                                      0xffffffffU, 7, 0x9d2c5680U, 15, 0xefc60000U, 18,
                                      1812433253U>;
using Tiny = mersenne_twister_engine<unsigned long long, 64, 1, 1, 0,
                                    0xb5026f5aa96619e9ULL, 29, 0x5555555555555555ULL,
                                    17, 0x71d67fffeda60000ULL, 37, 0xfff7eee000000000ULL,
                                    43, 6364136223846793005ULL>;

template <class Engine> Engine advanced(Engine value, unsigned long long count) {
    value.discard(count);
    return value;
}

template <class Engine> void inspect(unsigned long long seed) {
    Engine generator(seed), copy;
    cout << Engine::min() << ' ' << Engine::max() << ' ' << generator.min() << ' '
         << generator.max() << '\n';
    unsigned long long hash = 0;
    for (int i = 0; i < 2050; ++i) {
        auto value = generator();
        hash = (hash ^ value) * 0x9e3779b97f4a7c15ULL;
        if (i < 2 || i == 310 || i == 311 || i == 312 || i == 623 || i == 624 || i == 1247 ||
            i == 2048)
            cout << value << ' ';
    }
    cout << hash << '\n';
    auto retained = generator;
    cout << (retained == generator) << ' ';
    auto& alias = copy;
    copy = generator;
    cout << copy() << ' ' << generator() << ' ' << (retained != generator) << ' ' << alias()
         << '\n';
    for (unsigned long long count : {0ULL, 1ULL, 16ULL, 17ULL, 311ULL, 312ULL, 623ULL, 624ULL,
                                     625ULL, 1248ULL}) {
        Engine source(seed);
        auto skipped = advanced(source, count);
        for (unsigned long long i = 0; i < count; ++i)
            source();
        cout << (source == skipped) << ' ' << source() << ' ' << skipped() << '\n';
    }
    generator.seed(seed);
    cout << generator() << ' ';
    generator.seed();
    Engine defaulted;
    cout << generator() << ' ' << defaulted() << ' ' << (generator == defaulted) << '\n';
    defaulted.operator=(retained);
    cout << defaulted.operator()() << ' ' << retained() << '\n';
    Engine engines[]{generator, retained};
    cout << engines[0]() << ' ' << engines[1]() << ' ' << generator() << '\n';
    auto captured = [generator]() mutable { return generator(); };
    cout << captured() << ' ' << captured() << ' ' << generator() << '\n';
}

mt19937_64 global(12345);
int constructed;
mt19937_64 makeEngine(unsigned long long seed) {
    ++constructed;
    return mt19937_64(seed);
}
int main() {
    cout << global() << ' ' << global() << '\n';
    cout << makeEngine(17).min() << ' ' << makeEngine(31).max() << ' ' << constructed << '\n';
    int cases;
    cin >> cases;
    while (cases--) {
        unsigned long long seed;
        cin >> seed;
        inspect<mt19937>(seed);
        inspect<mt19937_64>(seed);
        inspect<Small>(seed);
        inspect<Word32>(seed);
        inspect<Tiny>(seed);
    }
}
