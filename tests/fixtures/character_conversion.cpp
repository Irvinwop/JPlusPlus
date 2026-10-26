#include <cctype>
#include <iostream>

namespace own {
int toupper(int value) {
    return value + 1000;
}
} // namespace own

int selections;
int selected(int value) {
    ++selections;
    return value;
}

int main() {
    for (int value = -1; value < 256; ++value) {
        selections = 0;
        auto upper = std::toupper(selected(value));
        auto lower = ::tolower(selected(value));
        std::cout << upper << ':' << lower << ':' << selections << ':' << own::toupper(value)
                  << '\n';
        std::cout << static_cast<char>(upper) << static_cast<char>(lower) << '\n';
    }
}
