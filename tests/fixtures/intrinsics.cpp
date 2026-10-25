#include <bits/stdc++.h>
using namespace std;

int main() {
    int t;
    cin >> t;
    while (t--) {
        unsigned long long value;
        long long a, b, mod;
        cin >> value >> a >> b >> mod;
        cout << __builtin_popcountll(value) << ' ' << __builtin_parityll(value) << ' ';
        if (value)
            cout << __builtin_clzll(value) << ' ' << __builtin_ctzll(value);
        cout << '\n';
        cout << (long long)((__int128)a * b % mod) << '\n';
        cout << value << ' ' << value / 3ULL << ' ' << (value >> 17) << ' ' << (value < 10ULL)
             << '\n';
    }
}
