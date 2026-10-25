#include <bits/stdc++.h>
using namespace std;

struct Item {
    int value = 7;
};
struct Scale {
    int seed;
    int operator()(long long value) {
        return int(value * 3 + seed);
    }
};
struct RefConstructor {
    int value;
    RefConstructor(int& source) : value(++source) {}
};

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        int constructorInput = x;
        RefConstructor constructed(constructorInput);
        cout << constructed.value << ' ' << constructorInput << '\n';
        array<long long, 4> values{x, y};
        array<long long, 4> empty{};
        array<int, 0> zero{};
        auto saved = values;
        values = {y, x, x + y};
        long long& alias = values.front();
        values.fill(x - y);
        alias += y;
        for (auto value : values)
            cout << value << ' ';
        cout << saved[0] << ' ' << empty[3] << ' ' << zero.size() << '\n';
        array<array<int, 3>, 2> nested{{{x, y}, {y}}};
        auto copied = nested;
        nested[0][1] += x;
        cout << nested[0][1] << ' ' << copied[0][1] << ' ' << nested[1][2] << '\n';
        array<Item, 3> records{{{x}, {y}}};
        Item& retained = records[1];
        records.fill(Item{x + y});
        retained.value += x;
        cout << records[0].value << ' ' << records[1].value << ' ' << records[2].value << '\n';

        function<bool(int, int)> differs = [](int a, int b) { return a - b; };
        function<long long(int)> widened = [x](long long value) { return value * 2 + x; };
        function<long long(int)> scaled = Scale{y};
        function<void(int&)> discard = [x](int& value) {
            value += x;
            return (long long)value * 9;
        };
        function<bool(int)> recursive;
        recursive = [&](int depth) -> int {
            if (depth)
                return recursive(depth - 1);
            return y;
        };
        int modified = y;
        discard(modified);
        cout << differs(x, y) << ' ' << widened(y) << ' ' << scaled(x) << ' ' << modified << ' '
             << recursive(4) << '\n';

        double fraction = x * 0.5 + y * 0.125;
        __int128 truncated = fraction;
        double magnitude = ldexp(double(x), 100) + ldexp(double(y), 60);
        __int128 wide = magnitude;
        __int128 low = -4e18, high = 4e18;
        cout << (long long)truncated << ' ' << (long long)(wide >> 64) << ' '
             << (unsigned long long)wide << ' ' << (long long)low << ' ' << (long long)high << '\n';
    }
}
