#include <bits/stdc++.h>
using namespace std;

struct MaskOrder {
    unsigned mask;
    bool operator()(unsigned left, unsigned right) const {
        return (left ^ mask) < (right ^ mask);
    }
};

int comparisonsCreated;
greater<long long> makeDescending() {
    ++comparisonsCreated;
    return {};
}

template <class Heap> void drain(Heap values) {
    while (!values.empty()) {
        cout << values.top() << ' ';
        values.pop();
    }
    cout << '\n';
}

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        int raw[]{x, y, -1, 0, 1, x + y};
        auto ascending = [](long long left, long long right) { return left < right; };
        auto descending = [](long long left, long long right) { return left > right; };
        priority_queue<long long, vector<long long>, decltype(ascending)> first(raw, raw + 6,
                                                                             ascending);
        priority_queue<long long, vector<long long>, decltype(descending)> second(descending);
        for (int value : raw)
            second.push(value);
        drain(first);
        drain(second);
        vector<long long> seed{x * 1000000007LL, y * 1000000007LL};
        priority_queue<long long, vector<long long>, greater<long long>> combined(raw, raw + 6,
                                                                                greater<long long>(),
                                                                                seed);
        seed[0] = 123456789;
        drain(combined);
        auto source = first;
        source.push(987654321);
        drain(first);
        drain(source);
        vector<unsigned> keys{unsigned(x), unsigned(y), ~0U, 0, 0x80000000U};
        MaskOrder comparator{0x80000000U};
        priority_queue<unsigned, vector<unsigned>, MaskOrder> custom(comparator, keys);
        comparator.mask = 0;
        keys[0] = 0;
        custom.push(2);
        drain(custom);
        priority_queue<int> defaultRange(raw, raw + 6);
        drain(defaultRange);
        priority_queue<long long, vector<long long>, greater<long long>> seeded(greater<long long>(),
                                                                              seed);
        drain(seeded);
        priority_queue<long long, vector<long long>, greater<long long>> observed(makeDescending());
        observed.push(x);
        observed.push(y);
        drain(observed);
        cout << comparisonsCreated << '\n';
    }
}
