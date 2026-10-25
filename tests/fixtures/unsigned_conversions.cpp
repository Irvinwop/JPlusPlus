#include <bits/stdc++.h>
using namespace std;

int main() {
    int n;
    cin >> n;
    while (n--) {
        unsigned narrow;
        unsigned long long wide;
        cin >> narrow >> wide;
        long long extended = narrow;
        unsigned long long positive = narrow;
        cout << extended << ' ' << positive << '\n';
        cout << bit_cast<unsigned long long>((double)narrow) << ' '
             << bit_cast<unsigned>((float)narrow) << ' '
             << bit_cast<unsigned long long>((double)wide) << ' ' << bit_cast<unsigned>((float)wide)
             << '\n';
        cout << bit_cast<unsigned long long>(round(narrow)) << ' '
             << bit_cast<unsigned long long>(round(wide)) << ' '
             << bit_cast<unsigned long long>(sqrt(wide)) << '\n';
        __int128 total = (__int128)wide + narrow;
        cout << (long long)(total % 1000000007) << ' ' << signbit(wide) << '\n';
    }
}
