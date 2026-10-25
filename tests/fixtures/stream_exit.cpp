#include <cstdlib>
#include <iostream>

int main() {
    std::ios::sync_with_stdio(false);
    std::cout << "buffered output";
    std::clog << "buffered log";
    std::exit(0);
}
