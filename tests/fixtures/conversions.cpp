#include <bits/stdc++.h>
using namespace std;

int adjust(int value) {
    return value * 3 + 1;
}

void add(int& value, int amount) {
    value += amount;
}

int main() {
    int x, y, flag;
    cin >> x >> y >> flag;
    (flag ? x : y) = 7;
    cout << x << ' ' << y << ' ' << (flag ? x : y)++ << '\n';
    add(flag ? x : y, 9);
    int& selected = flag ? x : y;
    selected += 5;
    cout << x << ' ' << y << '\n';
    vector<pair<int, long long>> values{{x, 5000000000LL + y}, {y, 7000000000LL + x}};
    for (pair<long long, int> value : values)
        cout << value.first << ' ' << value.second << '\n';
    pair<bool, int> converted = pair<int, long long>{x, 5000000000LL};
    cout << converted.first << ' ' << converted.second << '\n';
    vector<pair<long long, pair<bool, int>>> nested;
    nested.emplace_back(x, pair<int, int>{flag, y});
    cout << nested[0].first << ' ' << nested[0].second.first << ' ' << nested[0].second.second
         << '\n';
    priority_queue<pair<long long, int>> heap;
    heap.emplace(x, y);
    heap.emplace(y, x);
    cout << heap.top().first << ' ' << heap.top().second << '\n';
    set<pair<long long, int>> unique;
    auto inserted = unique.emplace(x, y);
    cout << inserted.second << ' ' << unique.emplace(x, y).second << '\n';
    multiset<int> multiple;
    multiple.emplace(x);
    multiple.emplace(x);
    multiset<int> others;
    others.emplace(y);
    multiple.merge(others);
    cout << multiple.size() << ' ' << others.size() << '\n';
    map<int, long long> mapping;
    cout << mapping.emplace(x, 1234567890123LL).second << ' ' << mapping.emplace(x, 4).second << ' '
         << mapping[x] << '\n';
    for (int value : vector<long long>{x, y, 4000000000LL}) {
        value += adjust(value);
        cout << value << ' ';
    }
    cout << '\n';
}
