#include <bits/stdc++.h>
using namespace std;

void add(int& value, int delta) {
    value += delta;
}

int main() {
    int n;
    cin >> n;
    auto compute = [](int value) { return (add(value, 3), value * 2); };
    cout << compute(n) << '\n';
    long long bound = n % 7 + 2;
    auto increaseBound = [&] { return ++bound; };
    increaseBound();
    int scratch[bound];
    fill(scratch, scratch + bound, n);
    cout << scratch[bound - 1] << ' ' << bound << '\n';
    int a = 3, b = -7;
    vector<int> values;
    for (int i = 0; i < n; ++i, add(b, 2)) {
        int first = (add(a, i), a * 2);
        int second = ((void)add(b, i % 3), b + 3);
        int selected = (i % 2 ? add(a, 1) : add(b, -1), a + b);
        values.push_back((values.resize(i), first + second + selected));
        if ((add(a, -1), a % 7 == 0))
            cout << a << ' ';
        i % 2 ? add(a, 2) : add(b, 3);
    }
    for (auto value : values)
        cout << value << ' ';
    cout << '\n' << a << ' ' << b << '\n';
}
