#include <bits/stdc++.h>
using namespace std;

int main() {
    int n;
    cin >> n;
    map<int, int> frequency;
    set<int> distinct;
    multiset<int> repeated;
    for (int i = 0; i < n; ++i) {
        int value;
        cin >> value;
        ++frequency[value];
        distinct.insert(value);
        repeated.insert(value);
    }
    for (auto [key, count] : frequency)
        cout << key << ':' << count << ' ';
    cout << '\n';
    for (auto& [key, count] : frequency)
        ++count;
    for (auto [key, count] : frequency)
        cout << key << ':' << count << ' ';
    cout << '\n';
    for (auto key : distinct) {
        cout << key << ' ' << repeated.count(key) << ' ';
        auto it = distinct.lower_bound(key);
        cout << *it << '\n';
    }
    auto copy = frequency;
    copy[123456] = 8;
    cout << frequency.count(123456) << ' ' << copy.count(123456) << '\n';
    queue<int> q;
    stack<int> s;
    for (auto x : distinct) {
        q.push(x);
        s.push(x);
    }
    while (!q.empty()) {
        cout << q.front() << ' ' << s.top() << '\n';
        q.pop();
        s.pop();
    }
}
