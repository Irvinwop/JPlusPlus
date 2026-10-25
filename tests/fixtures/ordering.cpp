#include <bits/stdc++.h>
using namespace std;

struct Order {
    static int created;
    unsigned mask = 0x80000000U;
    Order() {
        ++created;
    }
    bool operator()(unsigned a, unsigned b) const {
        return (a ^ mask) < (b ^ mask);
    }
};
int Order::created;

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        vector<unsigned> values{0, ~0U, 0x80000000U, unsigned(x), unsigned(y), 1};
        sort(values.begin(), values.end());
        for (auto value : values)
            cout << value << ' ';
        cout << '\n';
        unsigned raw[]{unsigned(x), ~0U, 0, 0x80000000U, unsigned(y)};
        sort(raw, raw + 5, greater<>());
        for (auto value : raw)
            cout << value << ' ';
        cout << '\n';
        vector<pair<unsigned, unsigned>> pairs{{~0U, 0}, {0, ~0U}, {0, 1}, {unsigned(x), 2}};
        sort(pairs.begin(), pairs.end());
        for (auto value : pairs)
            cout << value.first << ':' << value.second << ' ';
        cout << '\n';
        set<unsigned> keys(raw, raw + 5);
        set<unsigned, greater<>> descending{~0U, unsigned(x), 0, 0x80000000U};
        for (auto value : keys)
            cout << value << ' ';
        for (auto value : descending)
            cout << value << ' ';
        cout << '\n';
        map<unsigned, int> indexed;
        for (unsigned i = 0; i < 5; ++i)
            indexed.insert({raw[i], int(i) + x});
        for (auto entry : indexed)
            cout << entry.first << ':' << entry.second << ' ';
        cout << '\n';
        priority_queue<unsigned, vector<unsigned>, greater<>> heap;
        for (auto value : values)
            heap.push(value);
        while (!heap.empty()) {
            cout << heap.top() << ' ';
            heap.pop();
        }
        cout << '\n';
        set<unsigned, Order> custom;
        priority_queue<unsigned, vector<unsigned>, Order> customHeap;
        for (auto value : values) {
            custom.insert(value);
            customHeap.push(value);
        }
        cout << Order::created << '\n';
        for (auto value : custom)
            cout << value << ' ';
        while (!customHeap.empty()) {
            cout << customHeap.top() << ' ';
            customHeap.pop();
        }
        cout << '\n';
        vector<int> signedValues{-100, -5, 0, 1, 7, 99};
        long long key = y;
        cout << lower_bound(signedValues.begin(), signedValues.end(), key) - signedValues.begin()
             << ' '
             << upper_bound(signedValues.begin(), signedValues.end(), key) - signedValues.begin()
             << ' ' << binary_search(signedValues.begin(), signedValues.end(), key) << '\n';
        cout << lower_bound(values.begin(), values.end(), key) - values.begin() << ' '
             << upper_bound(values.begin(), values.end(), key) - values.begin() << '\n';
        sort(signedValues.begin(), signedValues.end(), greater<>());
        cout << lower_bound(signedValues.begin(), signedValues.end(), key, greater<>()) -
                    signedValues.begin()
             << ' '
             << upper_bound(signedValues.begin(), signedValues.end(), key, greater<>()) -
                    signedValues.begin()
             << ' ' << binary_search(signedValues.begin(), signedValues.end(), key, greater<>())
             << '\n';
        cout << min(unsigned(x), 0U) << ' ' << max(unsigned(x), 0x80000000U) << ' '
             << min({unsigned(x), ~0U, 1U}) << ' ' << clamp(unsigned(y), 1U, 0xfffffffeU) << '\n';
        cout << less<>()(x, ~0U) << ' ' << greater<>()(~0U, y) << '\n';
    }
}
