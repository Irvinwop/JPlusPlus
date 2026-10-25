#include "library.cpp"
#include <iostream>

int main() {
    int n;
    std::cin >> n;
    std::vector<int> values(n);
    for (int& value : values)
        std::cin >> value;
    std::cout << score(values) << '\n';
}
