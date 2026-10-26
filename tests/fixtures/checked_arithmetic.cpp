#include <iostream>

constexpr bool constant_overflow() {
    signed char result = 37;
    return __builtin_add_overflow(127, 1, &result) && result == -128;
}
static_assert(constant_overflow());

int leftEffects, rightEffects, destinationEffects;
long long left(long long value) {
    ++leftEffects;
    return value;
}
unsigned long long right(unsigned long long value) {
    ++rightEffects;
    return value;
}
int* destination(int* value) {
    ++destinationEffects;
    return value;
}

template <class Result, class Left, class Right> void checked(Left first, Right second) {
    Result result = 37;
    bool overflow = __builtin_add_overflow(first, second, &result);
    std::cout << overflow << ':' << static_cast<long long>(result) << ',';
    overflow = __builtin_sub_overflow(first, second, &result);
    std::cout << overflow << ':' << static_cast<long long>(result) << ',';
    overflow = __builtin_mul_overflow(first, second, &result);
    std::cout << overflow << ':' << static_cast<long long>(result) << '\n';
}

void report(bool overflow, unsigned __int128 result) {
    std::cout << overflow << ':' << static_cast<unsigned long long>(result >> 64) << ':'
              << static_cast<unsigned long long>(result) << '\n';
}

void wide() {
    unsigned __int128 maximum = -static_cast<unsigned __int128>(1);
    __int128 minimum = -static_cast<__int128>((static_cast<unsigned __int128>(1) << 127) - 1) - 1;
    unsigned __int128 unsignedResult = 37;
    __int128 signedResult = 37;
    bool overflow = __builtin_mul_overflow(maximum, maximum, &unsignedResult);
    report(overflow, unsignedResult);
    overflow = __builtin_add_overflow(maximum, minimum, &signedResult);
    report(overflow, static_cast<unsigned __int128>(signedResult));
    overflow = __builtin_sub_overflow(minimum, maximum, &unsignedResult);
    report(overflow, unsignedResult);
    overflow = __builtin_mul_overflow(minimum, -1, &signedResult);
    report(overflow, static_cast<unsigned __int128>(signedResult));
    overflow = __builtin_add_overflow(minimum, minimum, &signedResult);
    report(overflow, static_cast<unsigned __int128>(signedResult));
    overflow = __builtin_mul_overflow(maximum, false, &signedResult);
    report(overflow, static_cast<unsigned __int128>(signedResult));
    overflow = __builtin_add_overflow(maximum, true, &unsignedResult);
    report(overflow, unsignedResult);
}

int main() {
    std::cout << constant_overflow() << '\n';
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
        int result = 0;
        leftEffects = rightEffects = destinationEffects = 0;
        bool overflow = __builtin_add_overflow(left(first), right(second), destination(&result));
        std::cout << overflow << ':' << result << ':' << leftEffects << ':' << rightEffects << ':'
                  << destinationEffects << '\n';
        int alias = static_cast<int>(first);
        overflow = __builtin_mul_overflow(alias, second, &alias);
        std::cout << overflow << ':' << alias << '\n';
        long long signedResult;
        unsigned long long unsignedResult;
        overflow = __builtin_saddll_overflow(first, static_cast<long long>(second), &signedResult);
        std::cout << overflow << ':' << signedResult << ',';
        overflow = __builtin_usubll_overflow(static_cast<unsigned long long>(first), second,
                                             &unsignedResult);
        std::cout << overflow << ':' << unsignedResult << ',';
        overflow = __builtin_umulll_overflow(static_cast<unsigned long long>(first), second,
                                             &unsignedResult);
        std::cout << overflow << ':' << unsignedResult << '\n';
    }
}
