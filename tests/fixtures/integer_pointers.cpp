#include <cstdint>
#include <functional>
#include <iostream>
#include <vector>

int evaluations;
int next(int value) {
    ++evaluations;
    return value;
}
int* make(int value) {
    return reinterpret_cast<int*>(next(value));
}
struct Node {
    Node* parent;
    int value;
};

int main() {
    int cases;
    std::cin >> cases;
    while (cases--) {
        int number, extra;
        std::cin >> number >> extra;
        evaluations = 0;
        int* first = reinterpret_cast<int*>(next(number));
        int* second = reinterpret_cast<int*>(number);
        void* erased = first;
        int* restored = static_cast<int*>(erased);
        std::cout << static_cast<bool>(first) << ' ' << (first == second) << ' '
                  << (first == restored) << ' ' << evaluations << '\n';
        auto copy = first;
        first = make(extra);
        std::cout << (copy == second) << ' ' << static_cast<bool>(first) << ' ' << (first == copy)
                  << ' ' << evaluations << '\n';
        std::vector<int*> pointers{copy, second, first};
        auto saved = pointers;
        pointers[0] = make(extra + 1);
        std::cout << (saved[0] == copy) << ' ' << (pointers[0] == saved[0]) << ' ' << evaluations
                  << '\n';
        int* allBits = reinterpret_cast<int*>(static_cast<std::uintptr_t>(-1));
        int* signedBits = reinterpret_cast<int*>(-1);
        int* narrowBits = reinterpret_cast<int*>(~0U);
        void* otherType = reinterpret_cast<double*>(-1);
        std::cout << (allBits == signedBits) << ' ' << (signedBits == narrowBits) << ' '
                  << (otherType == signedBits) << '\n';
        unsigned __int128 wide =
            (static_cast<unsigned __int128>(1) << 100) + static_cast<std::uintptr_t>(-1);
        std::cout << (reinterpret_cast<int*>(wide) == allBits) << '\n';
        int object = 9;
        std::cout << (&object == signedBits) << ' ' << (signedBits == &object) << ' '
                  << (signedBits == nullptr) << '\n';
        std::function<int*(int)> function = make;
        std::cout << (function(number) == second) << ' ' << evaluations << '\n';
        auto captured = [copy](int* replacement) mutable {
            int* old = copy;
            copy = replacement;
            return old;
        };
        auto cloned = captured;
        std::cout << (captured(first) == second) << ' ' << (cloned(nullptr) == second) << ' '
                  << (captured(second) == first) << '\n';
        Node nodes[3]{};
        nodes[0].parent = reinterpret_cast<Node*>(-1);
        nodes[1].parent = nodes;
        nodes[2].parent = nodes + 1;
        int count = 0;
        for (auto current = nodes + 2; current != reinterpret_cast<Node*>(-1);
             current = current->parent)
            ++count;
        std::cout << count << '\n';
    }
}
