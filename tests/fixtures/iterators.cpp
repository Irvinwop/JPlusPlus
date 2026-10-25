#include <bits/stdc++.h>
using namespace std;

int main() {
    int t;
    cin >> t;
    while (t--) {
        int n;
        cin >> n;
        long long values[n];
        for (auto& value : values)
            cin >> value;
        sort(values, n + values);
        vector<long long> copied(values, n + values);
        auto iterator = copied.end();
        iterator -= n;
        iterator += min(n - 1, 2);
        auto other = min(n - 1, 2) + copied.begin();
        cout << *iterator << ' ' << iterator - copied.begin() << ' ' << (iterator == other)
             << '\n';
        for (auto key : copied)
            cout << lower_bound(values, values + n, key) - values << ' '
                 << upper_bound(values, n + values, key) - lower_bound(values, n + values, key)
                 << '\n';
        auto reverse = copied.rbegin();
        reverse += n - 1;
        cout << *reverse << ' ' << distance(copied.rbegin(), reverse) << '\n';
        vector<long long>::iterator delayed;
        delayed = copied.begin();
        cout << *delayed << '\n';
    }
}
