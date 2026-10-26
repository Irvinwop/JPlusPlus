#include <iostream>
int positive(int value) {
    if (true)
        return value * 7 + 1;
}
int constexprValue(int value) {
    if constexpr ((true && true) || false)
        return value * 3 + 2;
    value += 100;
}
int constantElse(int value) {
    if (false)
        return -1;
    else
        return value + 9;
}
int selectedElse(int value) {
    if (true)
        return value * 19;
    else
        value += 7;
}
int afterInfinite(int value) {
    while (true || false) {
        if (value > 0)
            return value * 23;
    }
    value += 100;
}
int switchWhile(int value) {
    switch (value % 3) {
    default:
        while (false || true) {
            if (value > 0)
                return value * 29;
        }
    }
}
int switchDo(int value) {
    switch (value % 3) {
    default:
        do {
            if (value > 0)
                return value * 31;
        } while (true && true);
    }
}
int switchFor(int value) {
    switch (value % 3) {
    default:
        for (;;) {
            if (value > 0)
                return value * 37;
        }
    }
}
int switchForCondition(int value) {
    switch (value % 3) {
    default:
        for (; true && true;) {
            if (value > 0)
                return value * 41;
        }
    }
}
int closed(int value) {
    if (false)
        value += 1;
    switch (value % 3) {
    case 0:
        return value * 43;
    case 1:
        return value * 47;
    default:
        return value * 53;
    }
}
constexpr int dual(int value) {
    if consteval {
        return value * 2;
    } else {
        return value * 3;
    }
    value += 100;
}
int main() {
    int count;
    std::cin >> count;
    for (int i = 0; i < count; ++i) {
        int value;
        std::cin >> value;
        constexpr int compileTime = dual(7);
        std::cout << positive(value) << ' ' << constexprValue(value) << ' ' << constantElse(value)
                  << ' ' << selectedElse(value) << ' ' << afterInfinite(value) << ' '
                  << switchWhile(value) << ' ' << switchDo(value) << ' ' << switchFor(value) << ' '
                  << switchForCondition(value) << ' ' << closed(value) << ' ' << dual(value) << ' '
                  << compileTime << '\n';
    }
}
