#include <bits/stdc++.h>
using namespace std;

int length(int& calls, int n) {
    ++calls;
    return n;
}

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        int n = 1 + unsigned(x) % 7, m = 1 + unsigned(y) % 5;
        int calls = 0;
        cout << sizeof(sizeof(int[length(calls, n)])) << ' ' << calls << '\n';
        cout << noexcept(sizeof(int[length(calls, n)])) << ' ' << calls << '\n';
        cout << __is_trivial(int[length(calls, n)]) << ' ' << calls << '\n';
        decltype(sizeof(int[length(calls, n)])) value = unsigned(x);
        cout << value << ' ' << calls << '\n';
        using Size = decltype(sizeof(int[length(calls, m)]));
        Size first = unsigned(y), second = first + 1;
        cout << first << ' ' << second << ' ' << calls << '\n';
        cout << (false && noexcept(sizeof(int[length(calls, n)]))) << ' ' << calls << '\n';
        int outer = 0, inner = 0;
        cout << __is_trivial(int[length(outer, n)][length(inner, m)]) << ' ' << outer << ' '
             << inner << '\n';
        int a = 1, b = 2;
        bool trivial = __is_trivial(int[++a][++b]);
        cout << trivial << ' ' << a << ' ' << b << '\n';
    }
}
