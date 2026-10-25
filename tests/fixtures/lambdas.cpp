#include <bits/stdc++.h>
using namespace std;

template <class T> T twice(T value) {
    return value + value;
}
void add(int& destination, int value) {
    destination += value;
}

int main() {
    int n;
    cin >> n;
    int visits = 0;
    function<long long(int)> fibonacci = [&](int x) -> long long {
        ++visits;
        if (x < 2)
            return x;
        return fibonacci(x - 1) + fibonacci(x - 2);
    };
    cout << fibonacci(n) << ' ' << visits << '\n';
    int sum = 4;
    add(sum, n);
    int a[2] = {3, 8};
    add(a[1], sum);
    int& alias = a[0];
    ++alias;
    cout << sum << ' ' << a[0] << ' ' << a[1] << ' ' << twice(n) << ' ' << twice(5LL) << '\n';
    vector<int> order{1, 9, 2, 4};
    sort(order.begin(), order.end(), [](int a, int b) { return a > b; });
    for (auto x : order)
        cout << x << ' ';
    cout << '\n';
    int snapshot = 5;
    auto saved = [snapshot](int value) mutable {
        snapshot += value;
        return snapshot;
    };
    snapshot = 99;
    cout << saved(2) << ' ' << saved(3) << ' ' << snapshot << '\n';
    for (auto& value : order)
        value *= 3;
    for (const auto& value : order)
        cout << value << ' ';
    cout << '\n';
    swap(a[0], a[1]);
    swap(sum, snapshot);
    cout << a[0] << ' ' << a[1] << ' ' << sum << ' ' << snapshot << '\n';
}
