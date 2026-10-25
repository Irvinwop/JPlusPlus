#include <bits/stdc++.h>
using namespace std;

void change(bool& flag, int mask) {
    flag ^= mask;
    flag |= mask - 2;
    flag &= mask + 1;
}

struct Counter {
    int value;
    void add(int o, int v) {
        value += v;
        this->value ^= o;
    }
    int replace(int jppLeft1) {
        return value += (value = jppLeft1);
    }
};

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;

        int a = x, b = y;
        int sum = 3 + (a = b = x - y);
        cout << sum << ' ' << a << ' ' << b << ' ' << ((a = x) < y) << ' ' << ((b = y) == x)
             << '\n';
        if ((a = x - y) != 0)
            cout << a;
        else
            cout << b;
        cout << ' ' << (y ? (a = x + 1) : (b = x - 1)) << '\n';

        bool flag = x;
        flag ^= y;
        cout << flag << ' ';
        flag += y;
        cout << flag << ' ';
        flag -= y;
        cout << flag << ' ';
        flag *= y;
        cout << flag << ' ';
        change(flag, y);
        cout << flag << ' ' << int(flag) << ' ' << static_cast<double>(flag) << '\n';
        bool parity = x;
        parity ^= y;
        int promoted = (parity ^= y);
        bool active = y;
        active |= x;
        active &= y - 1;
        cout << parity << ' ' << promoted << ' ' << active << '\n';
        vector<long long> truth;
        truth.push_back(flag);
        truth.push_back(static_cast<long long>(!flag));
        __int128 wide = flag;
        cout << truth[0] << ' ' << truth[1] << ' ' << (long long)wide << '\n';

        // C++17 evaluates the RHS first, then evaluates the destination exactly once.
        int slots[8] = {};
        int index = 1;
        int stored = (slots[index++] = (index = 3));
        cout << stored << ' ' << index << ' ' << slots[1] << ' ' << slots[3] << '\n';
        index = 1;
        slots[index++] += (index = 4);
        cout << index << ' ' << slots[1] << ' ' << slots[4] << '\n';
        int total = x;
        total += (total = y);
        cout << total << '\n';
        long long running = (long long)x * 1000000007;
        int narrowed[3] = {};
        narrowed[0] = running = running + y;
        cout << narrowed[0] << ' ';
        running += (running = (long long)y * 2000000011);
        narrowed[1] = running;
        narrowed[2] = (narrowed[0] = running);
        cout << narrowed[0] << ' ' << narrowed[1] << ' ' << narrowed[2] << '\n';
        long long offsets[3] = {};
        running = 0;
        narrowed[offsets[0] = ++running] = y;
        cout << narrowed[1] << ' ' << offsets[0] << ' ' << running << '\n';
        Counter counter{x};
        counter.add(x, y);
        cout << counter.value << ' ' << counter.replace(y) << '\n';
        Counter counters[4] = {};
        index = 0;
        int o = x, v = y;
        counters[index++].value += (index = 2);
        counters[1].value += o + v;
        cout << index << ' ' << counters[1].value << ' ' << counters[2].value << '\n';

        vector<int> counts(8, x);
        index = 1;
        counts[index++] ^= (index = 3);
        index = 1;
        counts[index++] = (index = 4);
        cout << index << ' ' << counts[1] << ' ' << counts[3] << ' ' << counts[4] << '\n';
        bool flags[3] = {false, true, false};
        index = 0;
        flags[index++] ^= y;
        flags[index++] |= x;
        cout << flags[0] << ' ' << flags[1] << ' ' << index << '\n';
        float fractions[2] = {0.5f, 1.25f};
        fractions[index = 0] += 0.25f;
        cout << fractions[0] << ' ' << index << '\n';

        string letters = "AZaz";
        index = 0;
        letters[index++] -= y;
        cout << letters << ' ' << index << '\n';
        unsigned long long bits = ~0ULL;
        bits >>= 1;
        bits /= 3;
        bits %= 97;
        cout << bits << '\n';
        int iterations = 0;
        for (int i = 0; i < 4; i += 1)
            iterations += i;
        int i = 0;
        for (; i < 4; i = i + 1)
            iterations += i;
        cout << iterations << '\n';
    }
}
