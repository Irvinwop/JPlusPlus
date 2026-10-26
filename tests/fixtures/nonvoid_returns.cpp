#include <iostream>
#include <vector>
using namespace std;
struct Result {
    long long value;
    int digit;
};
long long selected(int n) {
    if (n > 0) {
        long long value = n;
        for (int i = 0; i < 9; ++i)
            value = (value * 17 + i) % 1000000007;
        return value;
    }
}
int& slot(vector<int>& values, int index) {
    if (index >= 0 && index < values.size())
        return values[index];
}
Result result(int n) {
    if (n > 0)
        return {selected(n), n % 10};
}
int branch(int n, int selector) {
    switch (selector) {
    case 0:
        return n * 3 + 1;
    case 1:
        return n / 7 - 2;
    case 2:
        return n ^ 0x1357;
    }
}
int exhaustive(int n, int selector) {
    switch (selector) {
    case 0:
        return n + 11;
    case 1:
        return n - 19;
    default:
        return n * 5;
    }
}
int whileValue(int n) {
    while (n > 0) {
        return n ^ 0x2468;
    }
}
int doValue(int n) {
    do {
        return n + 31;
    } while (n < 0);
}
int caught(int n) {
    try {
        if (n & 1)
            throw n;
        return n * 7;
    } catch (int value) {
        if (value > 0)
            return value * 11;
    }
}
struct Calculator {
    int scale;
    int evaluate(int n) {
        if (n > 0)
            return n * scale + 3;
    }
};
template <class T> T wide(T value) {
    if (value > 0)
        return value * 1000003 + 17;
}
int unused(int n) {
    if (n > 0)
        return n;
}
int main() {
    int count;
    cin >> count;
    vector<int> values{10, 20, 30};
    Calculator calculator{13};
    auto conditional = [](int n) -> long long {
        if (n > 0)
            return (long long)n * n + 29;
    };
    for (int i = 0; i < count; ++i) {
        int n;
        cin >> n;
        Result answer = result(n);
        slot(values, i % 3) += n;
        cout << selected(n) << ' ' << answer.value << ':' << answer.digit << ' ' << branch(n, i % 3)
             << ' ' << exhaustive(n, i % 5) << ' ' << whileValue(n) << ' ' << doValue(n) << ' '
             << caught(n) << ' ' << calculator.evaluate(n) << ' ' << wide((long long)n) << ' '
             << conditional(n) << ' ' << values[i % 3] << '\n';
    }
}
