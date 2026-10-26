#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>
using namespace std;
void dump(long double value) {
    if (isnan(value)) {
        cout << "nan\n";
        return;
    }
    cout << signbit(value) << ':';
    if (isinf(value)) {
        cout << "inf\n";
        return;
    }
    int exponent;
    long double fraction = frexp(fabs(value), &exponent);
    cout << static_cast<unsigned long long>(ldexp(fraction, 64)) << ':' << exponent << '\n';
}
long double calculate(long double a, long double b) {
    return a / b + a * b - (a - b);
}
int main() {
    volatile long double one = 1.0L;
    dump(one + 0x1p-63L);
    dump(one + 0x1p-64L);
    dump(one + 0x3p-64L);
    long double base = one, greater = one + 0x1p-63L;
    dump(min(base, greater));
    dump(max(base, greater));
    dump(clamp(greater, base, base + 0x1p-61L));
    dump(static_cast<long double>(~0ULL));
    long double values[4] = {2.5L, -2.5L};
    dump(values[2]);
    dump(values[0]++);
    dump(++values[1]);
    values[3] = values[0] + values[1];
    values[3] *= 0.1L;
    dump(values[3]);
    vector<long double> ordered = {0.0L, -0.0L, one + 0x1p-63L, one, -2.0L};
    sort(ordered.begin(), ordered.end());
    for (auto value : ordered)
        dump(value);
    dump(numeric_limits<long double>::min());
    dump(numeric_limits<long double>::denorm_min());
    dump(numeric_limits<long double>::max());
    dump(numeric_limits<long double>::infinity());
    dump(numeric_limits<long double>::quiet_NaN());
    long double narrow = 0x1.0000010000000002p0L;
    cout << bit_cast<unsigned>(static_cast<float>(narrow)) << ' '
         << bit_cast<unsigned long long>(static_cast<double>(narrow)) << '\n';
    int count;
    cin >> count;
    for (int i = 0; i < count; ++i) {
        long long a, b;
        int leftPower, rightPower;
        cin >> a >> b >> leftPower >> rightPower;
        long double x = ldexp(static_cast<long double>(a), leftPower);
        long double y = ldexp(static_cast<long double>(b), rightPower);
        dump(x);
        dump(y);
        dump(x + y);
        dump(x - y);
        dump(x * y);
        dump(x / y);
        dump(sqrt(fabs(x)));
        dump(floor(x));
        dump(ceil(x));
        dump(round(x));
        dump(trunc(x));
        dump(rint(x));
        dump(calculate(x, y));
        cout << (x < y) << (x <= y) << (x == y) << (x != y) << (x >= y) << (x > y) << '\n';
        if (isfinite(x) && fabs(x) < 0x1p63L)
            cout << static_cast<long long>(x) << '\n';
    }
    for (int i = 0; i < 10; ++i) {
        long double value = 17.0L;
        cin >> value;
        dump(value);
        cout << cin.fail() << ' ' << cin.eof() << '\n';
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}
