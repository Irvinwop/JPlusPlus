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

int rangeJumps(int value) {
    int total = 0;
    for (int index : {0, 1, 2, 3}) {
        if (index == 2)
            goto skip;
        swap(value, index);
        total += index;
    skip:
        total += value;
    }
    vector<int> numbers{value, value - 3, value + 5};
    for (int& element : numbers) {
        if (element < 0)
            goto next;
        element += 5;
    next:
        total += element;
    }
    vector<pair<int, int>> pairs{{value, 3}, {3, value}};
    for (auto [first, second] : pairs) {
        if (first < 0)
            goto bindingDone;
        second += first;
    bindingDone:
        total += second;
    }
    for (auto copied : pairs) {
        if (copied.first > 0)
            goto copyDone;
        copied.second += value;
    copyDone:
        total += copied.second;
    }
    return total;
}

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        cout << nestedBreaks(x) << ' ' << directBreak(y) << ' ' << constantWhile(x) << ' '
             << constantDo(y) << ' ' << constexprBreak(x) << ' ' << jumpOut(y) << ' '
             << zeroIterations(x) << ' ' << rangeJumps(x) << '\n';
    }
}
