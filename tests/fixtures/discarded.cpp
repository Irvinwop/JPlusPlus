#include <bits/stdc++.h>
using namespace std;

int unavailable(int);

int main() {
    cout << &unavailable << ' ' << bool(unavailable) << '\n';
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        int mask = unsigned(x) & 65535;
        int count = 0, sum = 0;
        for (int position = 0; mask;
             mask ? (position += __builtin_ctz(mask), mask >>= __builtin_ctz(mask)) : 71) {
            if (mask & 1) {
                sum += position;
                mask &= ~1;
                ++count;
            }
        }
        int position = 0;
        for (position < y ? position++ : ++position; position < 8;
             position < 3 ? ++position : (position += 2)) {
            if (position % 3 == 0)
                continue;
            sum += position;
        }
        for (int i = 0; i < 4; (sum += i, ++i) * 7) {
            sum += x;
            if (i == 2 && y < 0)
                break;
        }
        static_cast<long long>(++sum);
        (x ? ++sum : --sum) + 23;
        !(++count);
        42;
        nullptr;
        "unused literal";
        vector<int> values{sum, count};
        values.size();
        values.empty();
        values[sum &= 1];
        cout.good();
        cout << sum << ' ' << count << ' ' << position << '\n';
    }
}
