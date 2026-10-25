#include <bits/stdc++.h>
using namespace std;

long long sum(int x, long long y) {
    return x + y;
}

int& chooseValue(int& x, int& y, bool first) {
    return first ? x : y;
}

struct Score {
    int value;
    long long total;
    Score(int x, long long y) : value(x * 3), total(y + 17) {}
};

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        auto values = make_tuple(x, 5000000000LL + y);
        cout << apply(sum, values) << '\n';
        auto score = make_from_tuple<Score>(values);
        cout << score.value << ' ' << score.total << '\n';
        auto lambda = [](int& a, int& b) {
            a += b;
            b -= 2;
        };
        apply(lambda, tie(x, y));
        apply(chooseValue, tuple_cat(tie(x, y), make_tuple(x < 0))) += 11;
        cout << x << ' ' << y << '\n';
        auto concatenated =
            tuple_cat(tuple<>(), values, tie(x), make_pair(y, string("tail")), array<int, 2>{7, 9});
        get<2>(concatenated) += 3;
        get<4>(concatenated)[0] = 'T';
        cout << x << ' ' << get<0>(concatenated) << ' ' << get<1>(concatenated) << ' '
             << get<3>(concatenated) << ' ' << get<4>(concatenated) << ' '
             << get<5>(concatenated) + get<6>(concatenated) << '\n';
        auto generic = [](auto a, auto b) { return a * 2 + b; };
        cout << apply(generic, make_pair(x, y)) << ' ' << apply([] { return 13; }, tuple_cat())
             << '\n';
        int& alias = make_from_tuple<int&>(tie(x));
        alias += 5;
        cout << x << '\n';
    }
}
