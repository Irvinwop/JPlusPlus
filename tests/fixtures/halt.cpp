#include <cstdlib>
#include <fstream>
#include <iostream>

std::ofstream output("jpp-halt.txt");
int main() {
    std::cout << "buffered console output";
    output << "buffered file output";
    std::_Exit(0);
}
