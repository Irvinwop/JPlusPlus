#include <bits/stdc++.h>
using namespace std;

template <class Sequence> void inspect(Sequence& sequence) {
    vector<unsigned long long> parameters;
    sequence.param(back_inserter(parameters));
    cout << sequence.size() << ':';
    for (auto parameter : parameters)
        cout << ' ' << parameter;
    cout << '\n';
    for (int size : {0, 1, 2, 3, 6, 7, 38, 39, 67, 68, 622, 623, 624, 1248}) {
        vector<unsigned> output(size + 4, 0xdeadbeefU);
        sequence.generate(output.begin() + 2, output.begin() + 2 + size);
        unsigned long long hash = 0;
        for (unsigned value : output)
            hash = (hash ^ value) * 0x9e3779b97f4a7c15ULL;
        vector<unsigned long long> wide(size);
        sequence.generate(wide.begin(), wide.end());
        cout << size << ':' << hash;
        for (int i = 0; i < size; ++i)
            if (i < 2 || i + 2 >= size)
                cout << ' ' << wide[i];
        cout << '\n';
    }
    mt19937 first(sequence), second(sequence);
    mt19937_64 wide(sequence);
    unsigned long long hash = 0;
    for (int i = 0; i < 1300; ++i)
        hash = (hash ^ first()) * 0x9e3779b97f4a7c15ULL + wide();
    cout << hash << ' ' << (first == second) << '\n';
    first.seed(sequence);
    cout << (first == second) << ' ' << first() << ' ' << second() << '\n';
    using Tiny = mersenne_twister_engine<unsigned, 2, 1, 1, 1, 3, 1, 3, 1, 2, 1, 3, 1, 3>;
    Tiny small(sequence);
    for (int i = 0; i < 5; ++i)
        cout << small() << ' ';
    cout << '\n';
}

int main() {
    seed_seq empty;
    inspect(empty);
    int cases;
    cin >> cases;
    while (cases--) {
        unsigned long long seed;
        cin >> seed;
        long long signedSeed = (long long)(seed >> 1);
        cout << llabs(-signedSeed) << ' ' << labs(-(long)signedSeed) << '\n';
        seed_seq direct{seed, seed >> 32, ~seed, 0ULL, 0xffffffffULL, 0x100000000ULL};
        inspect(direct);
        long long source[]{(long long)seed, -1, (long long)(seed >> 32), 0, -123456789};
        seed_seq range(source, source + 5);
        source[0] = 0;
        inspect(range);
        vector<char> chars{char(seed), (char)0xff, 'A', 0};
        seed_seq characters(chars.begin(), chars.end());
        chars[0] = 0;
        inspect(characters);
    }
}
