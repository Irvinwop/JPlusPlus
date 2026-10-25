#include <bits/stdc++.h>
using namespace std;

void advance(void*& pointer, int amount) {
    pointer += amount;
}
long long byteDistance(const void* left, const void* right) {
    return static_cast<const char*>(left) - static_cast<const char*>(right);
}
int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        unsigned int values[8]{unsigned(x), unsigned(y), 3, 4, 5, 6, 7, 8};
        void* first = values;
        void* at = first + 7;
        cout << sizeof(void) << ' ' << __alignof__(void) << ' ' << byteDistance(at, first) << '\n';
        void* previous = at++;
        cout << byteDistance(previous, first) << ' ' << byteDistance(at, first) << '\n';
        advance(at, 12);
        --at;
        void*& alias = at;
        alias -= 3;
        void* shifted = 4 + at;
        cout << byteDistance(alias, first) << ' ' << byteDistance(shifted, first) << ' '
             << (at > first) << '\n';
        unsigned int* restored = static_cast<unsigned int*>(at);
        *restored += unsigned(x + 100);
        memset(first + 1, y, 2);
        memcpy(first + 24, first + 16, 4);
        for (unsigned int value : values)
            cout << value << ' ';
        cout << '\n';
        const void* read = first;
        cout << byteDistance(read + 11, read) << ' ' << (shifted == static_cast<char*>(first) + 20)
             << '\n';
        void* slots[2]{first, first + 3};
        int index = 0;
        void* changed = (slots[index++] += 5);
        cout << index << ' ' << byteDistance(changed, first) << ' ' << byteDistance(slots[0], first)
             << ' ' << byteDistance(slots[1], first) << '\n';
        alignas(64) int aligned;
        cout << __alignof__(aligned) << ' ' << __alignof__(slots[index++]) << ' ' << index << '\n';
        void* empty = nullptr;
        empty += 0;
        cout << (empty + 0 == nullptr) << ' ' << (0 + empty == nullptr) << ' '
             << byteDistance(empty, empty) << '\n';
        int* typedEmpty = nullptr;
        cout << (typedEmpty - typedEmpty) << '\n';
    }
}
