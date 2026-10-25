#include <bits/stdc++.h>
using namespace std;

int measure(int& calls, int length) {
    ++calls;
    return length;
}
int order;
int ordered(int length) {
    order = order * 10 + length;
    return length;
}

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        int n = 1 + (unsigned(x) % 5), m = 2 + (unsigned(y) % 6), depth = 1 + (unsigned(x + y) % 3);
        int nc = 0, mc = 0, dc = 0;
        long long cube[measure(nc, n)][measure(mc, m)][measure(dc, depth)];
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < m; ++j)
                for (int k = 0; k < depth; ++k)
                    cube[i][j][k] = x * (i + 1LL) + y * (j + 2LL) + k;
        cout << nc << ' ' << mc << ' ' << dc << ' ' << sizeof cube << '\n';
        n += 9;
        m += 11;
        depth += 7;
        cout << sizeof cube << ' ' << sizeof cube[0] << ' ' << sizeof cube[0][0] << ' ' << nc << ' '
             << mc << ' ' << dc << '\n';
        int index = 0;
        cout << sizeof cube[index++] << ' ' << index << ' ' << cube[0][0][0] << '\n';
        typedef short Line[measure(mc, m)];
        typedef Line Matrix[measure(nc, n)];
        auto originalSize = sizeof(Matrix);
        n += 2;
        m += 3;
        Matrix matrix;
        Matrix second;
        matrix[0][0] = short(x);
        second[0][0] = short(y);
        cout << originalSize << ' ' << sizeof matrix << ' ' << sizeof second << ' ' << sizeof(Line)
             << ' ' << nc << ' ' << mc << '\n';
        cout << alignof(int[measure(dc, depth)]) << ' ' << dc << '\n';
        bool selected = (x & 1) != 0;
        auto size = selected ? sizeof(int[measure(nc, n)][measure(mc, m)]) : 13;
        cout << size << ' ' << nc << ' ' << mc << '\n';
        if (selected && sizeof(int[measure(dc, depth)]))
            cout << "taken ";
        cout << dc << '\n';
        int allocationCalls = 0;
        auto lazy = selected ? new int[measure(allocationCalls, 4)] : nullptr;
        cout << allocationCalls << ' ' << (lazy != nullptr) << '\n';
        delete[] lazy;
        void* raw = calloc(256, sizeof(int));
        int castCalls = 0;
        auto rows = selected ? (int(*)[measure(castCalls, 8)])raw : nullptr;
        cout << castCalls << ' ' << (rows != nullptr) << '\n';
        if (rows) {
            rows[1][3] = x - y;
            cout << sizeof *rows << ' ' << rows[1][3] << ' ' << castCalls << '\n';
            auto before = rows;
            cout << sizeof *rows++ << ' ' << (rows - before) << ' ' << castCalls << '\n';
        }
        free(raw);
    }
    int orderedArray[ordered(2)][ordered(3)][ordered(4)];
    cout << order << ' ' << sizeof orderedArray[0] << '\n';
}
