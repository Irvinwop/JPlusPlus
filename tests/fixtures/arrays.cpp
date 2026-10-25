#include <bits/stdc++.h>
using namespace std;

int memo[1000000] = {};

long long query(const long long* prefix, int l, int r) {
    return prefix[r] - prefix[l];
}

int main() {
    memo[999999] = 7;
    cout << memo[17] << ' ' << memo[999999] << '\n';
    int t;
    cin >> t;
    while (t--) {
        int n;
        cin >> n;
        long long a[n], prefix[n + 1];
        prefix[0] = 0;
        for (int i = 0; i < n; ++i) {
            cin >> a[i];
            prefix[i + 1] = prefix[i] + a[i];
        }
        sort(a, a + n);
        cout << query(prefix, 0, n) << ' ' << a[0] << ' ' << a[n - 1] << '\n';
        reverse(a, a + n);
        for (auto x : a)
            cout << x << ' ';
        cout << '\n';
        int flag;
        if ((flag = n & 1))
            cout << flag << '\n';
        else
            cout << 0 << '\n';
        cout << " \n"[n > 4];
        cout << - --n << ' ' << n << '\n';
    }
}
