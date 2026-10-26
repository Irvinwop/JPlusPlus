import jpp_practice;
#include <iostream>

int main() {
    int count;
    std::cin >> count;
    std::cout << practice::checkpoint() << '\n';
    while (count--) {
        int first, second;
        std::cin >> first >> second;
        std::cout << practice::evaluate(first, second) << ':' << practice::checkpoint() << '\n';
    }
}
