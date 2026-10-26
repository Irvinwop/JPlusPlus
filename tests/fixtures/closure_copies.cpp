#include <functional>
#include <iostream>
#include <utility>

int copy(int value) {
    return value * 3 + 1;
}
int plus(int value) {
    return value + 8;
}

auto savedFactory(int seed) {
    static auto saved = [seed](int value) mutable {
        seed += value;
        return seed;
    };
    return saved;
}

int main() {
    int cases;
    std::cin >> cases;
    while (cases--) {
        int seed, change;
        std::cin >> seed >> change;
        int shared = seed + 4;
        int& alias = seed;
        int (*operation)(int) = plus;
        auto original = [seed, alias, operation, &shared](int value) mutable {
            seed += value;
            alias -= value;
            shared += 2;
            return copy(seed) + alias + operation(shared);
        };
        std::cout << original(change) << ' ';
        auto first = original;
        auto second = std::move(original);
        std::cout << first(2) << ' ' << second(-3) << ' ' << original(0) << ' ' << seed << ' '
                  << shared << '\n';

        int state = seed;
        auto closure = [state](int value) mutable {
            state += value;
            return state;
        };
        std::function<int(int)> owned = closure;
        std::function<int(int)> borrowed = std::ref(closure);
        auto ownedCopy = owned;
        auto borrowedCopy = borrowed;
        std::cout << owned(3) << ' ' << ownedCopy(5) << ' ' << closure(0) << ' ' << borrowed(7)
                  << ' ' << borrowedCopy(11) << ' ' << closure(0) << '\n';

        auto capturedAddress = [state]() mutable {
            int* pointer = &state;
            ++*pointer;
            return state;
        };
        capturedAddress();
        auto addressCopy = capturedAddress;
        std::cout << capturedAddress() << ' ' << addressCopy() << ' ' << capturedAddress() << '\n';

        int* stored = nullptr;
        auto self = [state, stored](int value) mutable {
            if (!stored)
                stored = &state;
            *stored += value;
            return state;
        };
        std::cout << self(1) << ' ';
        auto selfCopy = self;
        std::cout << selfCopy(2) << ' ' << self(0) << ' ' << selfCopy(1) << ' ' << self(0) << '\n';

        auto persistentCopy = savedFactory(seed);
        std::cout << persistentCopy(change) << ' ' << savedFactory(seed)(0) << '\n';

        auto nested = [state]() mutable {
            auto inner = [state](int value) mutable {
                state += value;
                return state;
            };
            inner(3);
            auto innerCopy = inner;
            return inner(2) + innerCopy(9) + ++state;
        };
        nested();
        auto nestedCopy = nested;
        std::cout << nested() << ' ' << nestedCopy() << '\n';

        int (*f)(int) = plus;
        int (*g)(int) = plus;
        std::cout << (f == g) << ' ' << (&f == &g) << '\n';
    }
}
