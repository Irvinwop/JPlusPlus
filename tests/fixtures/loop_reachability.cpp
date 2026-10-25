#include <bits/stdc++.h>
using namespace std;

int nestedBreaks(int value) {
    for (;;) {
        for (int i = 0; i < 3; ++i) {
            if (i == 1)
                break;
            value += i;
        }
        switch (value & 1) {
        case 0:
            ++value;
            break;
        default:
            value += 2;
            break;
        }
        if constexpr (false) {
            break;
        }
        return value;
    }
    return -999;
}

int directBreak(int value) {
    for (;;) {
        if (value > 0)
            break;
        value += 7;
    }
    return value * 2;
}

int constantWhile(int value) {
    while (2 + 2 == 4) {
        if (value & 1)
            return value + 5;
        return value - 3;
    }
    return -999;
}

int constantDo(int value) {
    do {
        if (value & 1)
            return value * 2;
        return value + 9;
    } while (true);
    return -999;
}

int constexprBreak(int value) {
    for (;;) {
        if constexpr (sizeof(int) == 4)
            break;
        else
            return -999;
    }
    return value + 12;
}

int jumpOut(int value) {
    for (;;) {
        if (value > 0)
            goto done;
        value += 3;
    }
done:
    return value + 21;
}

int zeroIterations(int value) {
    while (false) {
        ++value;
        if (value & 1)
            continue;
        break;
    }
    for (int i = ++value; false; ++i)
        value += i;
    do {
        value += 3;
    } while (false);
    return value;
}

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        cout << nestedBreaks(x) << ' ' << directBreak(y) << ' ' << constantWhile(x) << ' '
             << constantDo(y) << ' ' << constexprBreak(x) << ' ' << jumpOut(y) << ' '
             << zeroIterations(x) << '\n';
    }
}
