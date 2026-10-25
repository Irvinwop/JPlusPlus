#include <bits/stdc++.h>
using namespace std;

struct Counts {
    static constexpr int WIDTH = 11;
    static const long long OFFSET = 10000000000LL;
    static int calls;
    static int add(int value) {
        calls += value * WIDTH;
        return calls;
    }
};
int Counts::calls;
enum class Wide : unsigned long long { High = 1ULL << 63, All = ~0ULL };
enum class Toggle : bool { Off = false, On = true };
enum class Byte : signed char { Low = -128, High = 127 };

constexpr int phase(int value) {
    if consteval {
        return value + 100;
    } else {
        return value - 100;
    }
}
constexpr double realPhase(double value) {
    return is_constant_evaluated() ? value + 1 : value - 1;
}
consteval int immediate(int value) {
    return is_constant_evaluated() ? value * 3 : -value;
}
constinit int prepared = phase(5);

int main() {
    int input;
    cin >> input;
    constexpr int atCompileTime = phase(7);
    constexpr double realConstant = realPhase(0.5);
    int immediateValue = immediate(9);
    cout << atCompileTime << ' ' << prepared << ' ' << phase(input) << ' ' << realConstant << ' '
         << realPhase(input) << ' ' << immediateValue << ' ' << is_constant_evaluated() << '\n';
    Counts counter;
    cout << Counts::OFFSET + input * Counts::WIDTH << ' ' << Counts::add(input) << ' '
         << counter.add(3) << ' ' << Counts::calls << '\n';
    cout << numeric_limits<int>::max() << ' ' << numeric_limits<int>::lowest() << ' '
         << numeric_limits<long long>::max() << ' ' << numeric_limits<long long>::min() << '\n';
    cout << numeric_limits<unsigned>::max() << ' ' << numeric_limits<unsigned long long>::max()
         << ' ' << numeric_limits<signed char>::lowest() * input << '\n';
    cout << numeric_limits<double>::digits << ' ' << numeric_limits<unsigned>::is_signed << ' '
         << is_integral_v<int> << ' ' << is_same_v<int, long> << '\n';
    cout << bit_cast<unsigned long long>(numeric_limits<double>::max()) << ' '
         << bit_cast<unsigned long long>(numeric_limits<double>::lowest()) << ' '
         << bit_cast<unsigned long long>(numeric_limits<double>::min()) << ' '
         << bit_cast<unsigned long long>(numeric_limits<double>::denorm_min()) << ' '
         << bit_cast<unsigned long long>(numeric_limits<double>::epsilon()) << ' '
         << bit_cast<unsigned long long>(numeric_limits<double>::infinity()) << ' '
         << bit_cast<unsigned long long>(numeric_limits<double>::quiet_NaN()) << '\n';
    cout << bit_cast<unsigned long long>(numbers::pi) << ' '
         << bit_cast<unsigned>(numbers::pi_v<float>) << '\n';
    cout << (numeric_limits<int>::max() - abs(input)) << '\n';
    cout << (int)numeric_limits<char>::lowest() << ' ' << (int)L'\U0001F600' << ' '
         << ('RIFF' ^ input) << '\n';
    constexpr unsigned long long ceiling = ~0ULL;
    constexpr auto high = Wide::High;
    Toggle toggle = input < 0 ? Toggle::On : Toggle::Off;
    cout << ceiling - abs(input) << ' ' << to_underlying(high) << ' '
         << (to_underlying(Wide::All) == ceiling) << ' '
         << bit_cast<unsigned long long>((double)high) << '\n';
    cout << (bool)toggle << ' ' << (Toggle::Off < toggle) << ' ' << (Byte::Low < Byte::High) << ' '
         << (int)to_underlying(Byte::Low) * input << '\n';
    __int128 wideValues[4]{};
    wideValues[abs(input) % 4] = (__int128)to_underlying(high) + input;
    __int128 total = 0;
    for (auto value : wideValues)
        total += value;
    cout << (long long)(total % 1000000007) << '\n';
}
