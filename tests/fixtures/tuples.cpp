#include <bits/stdc++.h>
using namespace std;

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        tuple<int, long long, string> value{x, 5000000000LL + y, "word"};
        auto copy = value;
        get<0>(copy) += 9;
        get<string>(copy)[1] = 'X';
        auto [a, b, text] = copy;
        auto& [c, d, original] = value;
        c += y;
        original += '!';
        cout << a << ' ' << b << ' ' << text << ' ' << get<0>(value) << ' ' << get<string>(value)
             << '\n';

        tuple<int, int, int> integers{x * 100000 + 70000, -200 + y, -1 + x};
        tuple<short, signed char, unsigned short> narrow(integers);
        cout << int(get<0>(narrow)) << ' ' << int(get<1>(narrow)) << ' ' << get<2>(narrow) << '\n';
        tuple<long long, double, bool> converted(integers);
        cout << get<0>(converted) << ' ' << get<1>(converted) << ' ' << get<2>(converted) << '\n';
        tuple<short> single(tuple<int>{x * 100000 + 70000});
        tuple<tuple<int>> nested(tuple<int>{x});
        tuple<tuple<long long>> widened(nested);
        auto nestedCopy = nested;
        get<0>(get<0>(nestedCopy)) += y;
        cout << get<0>(single) << ' ' << get<0>(get<0>(widened)) << ' '
             << get<0>(get<0>(nestedCopy)) << '\n';

        vector<tuple<int, int, int>> edges;
        edges.emplace_back(x, y, 7);
        edges.push_back(make_tuple(y, x, 9));
        edges.emplace_back(x + y, x - y, 11);
        sort(edges.begin(), edges.end());
        for (auto [from, to, weight] : edges)
            cout << from << ':' << to << ':' << weight << ' ';
        cout << '\n';
        tuple<> empty;
        cout << (empty == make_tuple()) << ' ' << (empty < tuple<>()) << ' ' << (empty <= tuple<>())
             << '\n';
    }
}
