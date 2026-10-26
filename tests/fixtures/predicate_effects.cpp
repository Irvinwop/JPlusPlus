#include <iostream>

int leftEffects, rightEffects, destroyed;
long long left(long long value) {
    ++leftEffects;
    return value;
}
unsigned long long right(unsigned long long value) {
    ++rightEffects;
    return value;
}
int* selectedNull(int& effects) {
    ++effects;
    return nullptr;
}
int* selectedPointer(int& effects) {
    ++effects;
    static int value = 29;
    return &value;
}
int& selectedReference(int& effects) {
    ++effects;
    static int value = 19;
    return value;
}
int mayThrow(int& effects, bool fail) {
    ++effects;
    if (fail)
        throw 37;
    return 99;
}
struct Scope {
    ~Scope() {
        ++destroyed;
    }
};

int main() {
    int count;
    std::cin >> count;
    while (count--) {
        long long first;
        unsigned long long second;
        std::cin >> first >> second;
        int effects = 0;
        leftEffects = rightEffects = destroyed = 0;
        bool add = __builtin_add_overflow_p(left(first), right(second),
                                            (++effects, static_cast<signed char>(0)));
        bool plain = __builtin_add_overflow_p(first, second, *selectedNull(effects));
        bool reference = __builtin_sub_overflow_p(first, second, selectedReference(effects));
        bool statement = __builtin_mul_overflow_p(first, second, ({
                                                      Scope guard;
                                                      ++effects;
                                                      *selectedPointer(effects);
                                                  }));
        std::cout << add << ':' << plain << ':' << reference << ':' << statement << ':' << effects
                  << ':' << leftEffects << ':' << rightEffects << ':' << destroyed << '\n';
        bool gate = (first & 1) != 0;
        int conditionalEffects = 0;
        bool logicalAnd =
            __builtin_add_overflow_p(first, second, static_cast<int>(gate && ++conditionalEffects));
        bool logicalOr =
            __builtin_sub_overflow_p(first, second, static_cast<int>(gate || ++conditionalEffects));
        bool conditional = __builtin_mul_overflow_p(first, second,
                                                    gate ? selectedReference(conditionalEffects)
                                                         : *selectedPointer(conditionalEffects));
        bool omitted = __builtin_add_overflow_p(first, second,
                                                (++conditionalEffects, static_cast<int>(first & 2))
                                                    ?: (++conditionalEffects, 0));
        std::cout << logicalAnd << ':' << logicalOr << ':' << conditional << ':' << omitted << ':'
                  << conditionalEffects << '\n';
        int throwEffects = 0;
        try {
            bool result = __builtin_mul_overflow_p(first, second, mayThrow(throwEffects, gate));
            std::cout << result << ':';
        } catch (int code) {
            std::cout << code << ':';
        }
        std::cout << throwEffects << '\n';
        try {
            __builtin_add_overflow_p(first, second, (throw 41, 0));
        } catch (int code) {
            std::cout << code << '\n';
        }
        int ignored;
        const int constant = 23;
        std::cout << __builtin_add_overflow_p(first, second, ignored) << ':'
                  << __builtin_sub_overflow_p(first, second, constant) << ':'
                  << __builtin_mul_overflow_p(first, second, *static_cast<int*>(nullptr)) << '\n';
    }
}
