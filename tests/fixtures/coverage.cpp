#include <bits/stdc++.h>
using namespace std;

int main() {
    int a, b, v;
    cin >> a >> b >> v;
    priority_queue<int> largest;
    priority_queue<int, vector<int>, greater<int>> smallest;
    for (int x : {a, b, v, -a, b + 1}) {
        largest.push(x);
        smallest.push(x);
    }
    while (!largest.empty()) {
        cout << largest.top() << ' ';
        largest.pop();
    }
    cout << '\n';
    while (!smallest.empty()) {
        cout << smallest.top() << ' ';
        smallest.pop();
    }
    cout << '\n';
    vector<pair<int, int>> values{{a, b}, {v, a}};
    for (auto [a, v] : values)
        cout << a + v << ' ';
    cout << a << ' ' << b << ' ' << v << '\n';
    for (unsigned long long x : {0ULL, 1ULL, 3ULL, 17ULL, (1ULL << 40) + 1, 1ULL << 63})
        cout << bit_ceil(x) << ' ' << bit_floor(x) << ' ' << bit_width(x) << ' '
             << has_single_bit(x) << '\n';
    for (unsigned x : {0U, 1U, 3U, 31U, (1U << 30) + 1, 1U << 31})
        cout << bit_ceil(x) << ' ' << bit_floor(x) << ' ' << bit_width(x) << ' '
             << has_single_bit(x) << '\n';
}
