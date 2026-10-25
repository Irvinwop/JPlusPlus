#include <bits/stdc++.h>
using namespace std;

int finiteOnly(double value) {
    if (isfinite(value))
        return 1;
    unreachable();
}

int main() {
    int n;
    if (scanf("%d", &n) != 1)
        return 1;
    unordered_map<int, long long> saved;
    saved.reserve(n * 2);
    for (int i = 0; i < n; ++i) {
        double value;
        if (scanf("%lf", &value) != 1)
            return 2;
        cout << value << ' ' << isnan(value) << ' ' << isinf(value) << ' ' << isfinite(value) << ' '
             << signbit(value) << '\n';
        cout << bit_cast<unsigned long long>(round(value)) << ' '
             << bit_cast<unsigned long long>(trunc(value)) << ' '
             << bit_cast<unsigned long long>(nearbyint(value)) << ' '
             << bit_cast<unsigned>(round((float)value)) << ' '
             << bit_cast<unsigned long long>(copysign(1.0, value)) << '\n';
        printf("[%+9f][% 9E][%9g]\n", value, value, value);
        printf("[%08.0f][%#.0a][%#12.3g][%.17g]\n", value, value, value, value);
        cout << fixed << setprecision(0) << value << ' ' << scientific << setprecision(2) << value
             << ' ' << hexfloat << value << defaultfloat << setprecision(6) << '\n';
        if (isfinite(value) && abs(value) < 1000000) {
            cout << lround(value) << ' ' << llround(value) << ' ' << floor((float)value) << ' '
                 << ceil((float)value) << ' ' << finiteOnly(value) << '\n';
            saved.emplace(i, llround(value));
        }
    }
    long long sum = 0;
    for (int exponent = -1074; exponent <= 1023; ++exponent) {
        double value = ldexp(1.0, exponent);
        sum += (long long)log2(value) * (exponent + 1075);
    }
    cout << sum << ' ' << saved.size() << '\n';
    double negative = bit_cast<double>(0x8000000000000000ULL);
    float negativeFloat = __builtin_bit_cast(float, 0x80000000U);
    cout << signbit(negative) << ' ' << signbit(negativeFloat) << '\n';
}
