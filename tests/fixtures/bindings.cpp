#include <bits/stdc++.h>
using namespace std;

struct Bounds {
    int left, right;
};
struct Self {
    int value;
    int* pointer;
    Self(int initial) : value(initial), pointer(&value) {}
    Self(const Self& other) : value(other.value), pointer(&value) {}
};

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        Bounds bounds{x, y};
        auto& [left, right] = bounds;
        left += y;
        right -= x;
        cout << bounds.left << ' ' << bounds.right << ' ' << (&left == &bounds.left) << '\n';
        auto [copiedLeft, copiedRight] = bounds;
        copiedLeft += copiedRight;
        cout << copiedLeft << ' ' << copiedRight << ' ' << bounds.left << '\n';
        pair<int, int> pair{x, y};
        auto& [first, second] = pair;
        second += first;
        cout << pair.second << ' ' << (&second == &pair.second) << '\n';
        int raw[2]{x, y};
        auto& [rawLeft, rawRight] = raw;
        rawLeft += rawRight;
        cout << raw[0] << ' ' << (&rawLeft == raw) << '\n';
        array<int, 2> fixed{x, y};
        auto& [fixedLeft, fixedRight] = fixed;
        fixedRight -= fixedLeft;
        cout << fixed[1] << ' ' << (&fixedRight == &fixed[1]) << '\n';
        Self self{x};
        auto [value, pointer] = self;
        value += y;
        cout << value << ' ' << *pointer << ' ' << self.value << '\n';
        vector<array<int, 2>> rows{{x, y}, {y, x}};
        for (auto& [a, b] : rows)
            a += b;
        for (auto [a, b] : rows)
            cout << a << ':' << b << ' ';
        cout << '\n';
    }
}
