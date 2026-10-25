#include <bits/stdc++.h>
using namespace std;

template <integral T> constexpr T cube(T x) {
    if consteval {
        return x * x * x;
    } else {
        return x * x * x;
    }
}
enum class Mode { Small = 3, Large = 7 };

int main() {
    constexpr int compileTime = cube(4);
    int n;
    cin >> n;
    cout << compileTime << ' ' << cube(n) << ' ' << (int)Mode::Large << '\n';
    auto size = 3uz;
    int values[3] = {2, 4, 8};
    for (auto value : values)
        cout << value * (long long)size << ' ';
    cout << '\n';
    vector<int> large(50000 + abs(n));
    cout << large.size() * large.size() << ' '
         << (large.end() - large.begin()) * (large.end() - large.begin()) << '\n';
    pair<long long, long long> dimensions(large.size(), distance(large.begin(), large.end()));
    cout << dimensions.first << ' ' << dimensions.second << '\n';
}
