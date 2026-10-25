#include <bits/stdc++.h>
using namespace std;

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        tuple<unsigned, int> a{unsigned(x), y}, b{unsigned(y), x};
        auto order = a <=> b;
        cout << (a < b) << ' ' << (a > b) << ' ' << (a <= b) << ' ' << (a >= b) << ' ' << (a == b)
             << ' ' << (a != b) << '\n';
        cout << (order < 0) << ' ' << (0 < order) << ' ' << (order == 0) << ' ' << (0 != order)
             << ' ' << is_lt(order) << ' ' << is_gteq(order) << '\n';
        partial_ordering partial = order;
        weak_ordering weak = order;
        cout << is_eq(partial) << ' ' << (weak == weak_ordering::equivalent) << '\n';
        auto preserved = order;
        auto& alias = order;
        order = strong_ordering::greater;
        cout << is_gt(alias) << ' ' << is_eq(preserved) << ' ' << is_lt(partial) << ' '
             << is_lt(0 <=> order) << ' ' << is_gt(order <=> 0) << '\n';
        for (double number :
             {0.0, -0.0, double(x), numeric_limits<double>::infinity(),
              -numeric_limits<double>::infinity(), numeric_limits<double>::quiet_NaN()}) {
            tuple<double, int> left{number, x}, right{0.0, y};
            auto result = left <=> right;
            cout << (left < right) << ' ' << (left <= right) << ' ' << (left == right) << ' '
                 << (left != right) << ' ' << (left >= right) << ' ' << (left > right) << ' '
                 << (result == partial_ordering::unordered) << ' ' << is_neq(result) << ' '
                 << (0 <= result) << ' ' << (result >= 0) << '\n';
            cout << is_lt(0 <=> result) << ' ' << is_neq(result <=> 0) << '\n';
        }
    }
}
