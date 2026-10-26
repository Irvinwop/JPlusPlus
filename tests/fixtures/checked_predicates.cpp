#include <iostream>

constexpr bool constantEffects() {
    int effects = 0;
    bool overflow = __builtin_add_overflow_p(2, 3, (++effects, static_cast<short>(0)));
    return !overflow && effects == 1;
}
static_assert(constantEffects());
static_assert(__builtin_add_overflow_p(127, 1, static_cast<signed char>(0)));
static_assert(!__builtin_sub_overflow_p(-127, 1, static_cast<signed char>(0)));
static_assert(__builtin_sub_overflow_p(0, 1, static_cast<unsigned char>(0)));
static_assert(__builtin_mul_overflow_p(65536, 65536, 0U));

template <class Result, class Left, class Right> void checked(Left first, Right second) {
    std::cout << __builtin_add_overflow_p(first, second, Result{}) << ':'
              << __builtin_sub_overflow_p(first, second, Result{}) << ':'
              << __builtin_mul_overflow_p(first, second, Result{}) << '\n';
}

void wide() {
    unsigned __int128 maximum = -static_cast<unsigned __int128>(1);
    __int128 minimum = -static_cast<__int128>((static_cast<unsigned __int128>(1) << 127) - 1) - 1;
    checked<unsigned __int128>(maximum, maximum);
    checked<__int128>(minimum, -1);
    checked<__int128>(maximum, minimum);
    checked<unsigned __int128>(minimum, minimum);
    checked<__int128>(maximum, false);
    checked<unsigned __int128>(maximum, true);
}

int main() {
    std::cout << constantEffects() << '\n';
    wide();
    int count;
    std::cin >> count;
    while (count--) {
        long long first;
        unsigned long long second;
        std::cin >> first >> second;
        checked<signed char>(first, second);
        checked<unsigned char>(first, second);
        checked<short>(first, second);
        checked<unsigned short>(first, second);
        checked<int>(first, second);
        checked<unsigned int>(first, second);
        checked<long long>(first, second);
        checked<unsigned long long>(first, second);
        checked<char>(first, second);
        checked<char8_t>(first, second);
        checked<char16_t>(first, second);
        checked<char32_t>(first, second);
        checked<wchar_t>(first, second);
        checked<__int128>(first, second);
        checked<unsigned __int128>(first, second);
    }
}
