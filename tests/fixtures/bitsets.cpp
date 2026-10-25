#include <bits/stdc++.h>
using namespace std;

struct BitSet {
    unsigned seed;
    unsigned count(unsigned value) {
        return seed ^= value;
    }
};

template <size_t N>
void exercise(unsigned long long value, unsigned long long amount, unsigned pos) {
    bitset<N> bits(value), other(value ^ (value >> 1));
    auto original = bits;
    cout << bits << ' ' << bits.size() << ' ' << bits.count() << ' ' << bits.any() << bits.all()
         << bits.none() << '\n';
    cout << bits._Find_first() << ' ' << bits._Find_next(pos) << ' '
         << bits._Find_next(numeric_limits<size_t>::max()) << '\n';
    cout << (bits << amount) << ' ' << (bits >> amount) << ' ' << ((~bits) >> amount) << ' '
         << ((~bits) << amount) << '\n';
    bits <<= amount;
    bits >>= amount;
    cout << bits << ' ' << original << '\n';
    bits = original;
    auto combined = (bits | other) ^ (bits & other);
    combined &= other;
    combined |= original;
    combined ^= other;
    cout << combined << ' ' << (~bits) << ' ' << (bits == original) << (bits != other) << '\n';
    auto aliasCopy = bits;
    aliasCopy.flip();
    cout << bits << ' ' << aliasCopy << '\n';
    bitset<N> text(string("xOIOIOOIIy"), 1, 8, 'O', 'I');
    bitset<N> pointerText("1110001110001010");
    bitset<N> equalChars(string("aaaa"), 0, 4, 'a', 'a');
    cout << text << ' ' << pointerText << ' ' << equalChars.count() << ' '
         << text.template to_string<char>('-', '+') << '\n';
    if constexpr (N > 0) {
        size_t i = pos % N, j = (i + 17) % N;
        auto proxy = bits[i];
        auto copiedProxy = proxy;
        proxy = !bits[j];
        copiedProxy.flip();
        cout << bits << ' ' << bool(proxy) << bool(copiedProxy) << (~proxy) << '\n';
        bits = other;
        bool before = proxy;
        bits[i] = bits[j];
        bits[(bits[j] = false, i)] = bits[j];
        cout << before << bool(proxy) << ' ' << bits.test(i) << ' ' << bits << '\n';
        bits.set(i).flip(j).reset(i);
        cout << bits << ' ' << bool(copiedProxy) << '\n';
        swap(bits, other);
        cout << bits << ' ' << other << ' ' << bool(proxy) << '\n';
    }
    if constexpr (N >= 4) {
        unsigned counter = 0;
        bits.reset();
        bits[counter++] = (counter += 2) == 2;
        cout << counter << ' ' << bits << '\n';
    }
    bits.set();
    cout << bits.all() << ' ' << bits.count() << ' ';
    bits.reset();
    cout << bits.none() << ' ' << bits.count() << '\n';
}

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        unsigned shift;
        unsigned long long value;
        cin >> shift >> value;
        unsigned long long amount = (shift & 256) ? (1ULL << 32) + shift % 140 : shift % 140;
        BitSet named{shift};
        cout << named.count((unsigned)value) << '\n';
        cout << bitset<16>(value).to_string().substr(3) << '\n';
        int numbers[3]{(int)shift, (int)value, 7};
        void* raw = numbers;
        static_cast<int*>(raw)[1] ^= numbers[0];
        ((int*)raw)[2] = numbers[1] & 0x7fff;
        cout << numbers[1] << ' ' << numbers[2] << ' ' << ((int*)raw)[1] << '\n';
        exercise<0>(value, amount, shift);
        exercise<1>(value, amount, shift);
        exercise<16>(value, amount, shift);
        exercise<63>(value, amount, shift);
        exercise<64>(value, amount, shift);
        exercise<65>(value, amount, shift);
        exercise<129>(value, amount, shift);
        if (cases < 4)
            for (unsigned long long boundary : {0ULL, 1ULL, 63ULL, 64ULL, 65ULL, 128ULL, 129ULL})
                exercise<129>(value, boundary, shift);
        bitset<64> low(value);
        cout << low.to_ulong() << ' ' << low.to_ullong() << '\n';
        unordered_map<bitset<65>, int> indexed;
        bitset<65> key(value);
        indexed[key] = 1;
        key.flip();
        indexed[key] = 2;
        cout << indexed[bitset<65>(value)] << ' ' << indexed[key] << ' ' << indexed.size() << '\n';
    }
}
