#include <bits/stdc++.h>
using namespace std;

#define TEN_INTS int, int, int, int, int, int, int, int, int, int
#define FIFTY_INTS TEN_INTS, TEN_INTS, TEN_INTS, TEN_INTS, TEN_INTS
using Large = tuple<FIFTY_INTS, FIFTY_INTS, FIFTY_INTS, FIFTY_INTS, int>;

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        Large values{};
        get<0>(values) = x;
        get<199>(values) = x + y;
        get<200>(values) = y;
        auto copied = values;
        get<0>(copied) += 3;
        get<200>(values) -= 7;
        cout << get<0>(values) << ' ' << get<0>(copied) << ' ' << get<199>(copied) << ' '
             << get<200>(values) << ' ' << get<200>(copied) << '\n';
        values = copied;
        cout << (values == copied) << ' ' << (values < Large{}) << '\n';
    }
}
