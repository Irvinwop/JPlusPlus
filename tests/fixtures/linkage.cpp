#include <iostream>

extern "C" {
int shared;
int seed() {
    shared = 7;
    return 31;
}
int startup = seed();
int update(int first, int second) {
    shared += first;
    return shared * 3 + second;
}
}

extern "C++" {
namespace support {
template <class T> T twice(T value) {
    return value + value;
}
struct Offset {
    int amount;
    int apply(int value) const {
        return value + amount;
    }
};
} // namespace support
}

int main() {
    int count;
    std::cin >> count;
    std::cout << startup << ':' << shared << '\n';
    while (count--) {
        int first, second;
        std::cin >> first >> second;
        support::Offset offset{second};
        std::cout << offset.apply(support::twice(update(first, second))) << ':' << shared << '\n';
    }
}
