#include <cstdlib>
#include <iostream>
using namespace std;
int selections;
unsigned long long chosen(unsigned long long value) {
    ++selections;
    return value;
}
namespace own {
int random(int value) {
    return value * 7 + 3;
}
int srand(int value, int extra) {
    return value * 3 + extra;
}
} // namespace own
int main() {
    cout << RAND_MAX << '\n';
    for (int i = 0; i < 40; ++i)
        cout << (i % 3 ? ::random() : std::rand()) << '\n';
    int count;
    cin >> count;
    for (int i = 0; i < count; ++i) {
        unsigned long long seed;
        int length;
        cin >> seed >> length;
        selections = 0;
        srandom(chosen(seed));
        for (int j = 0; j < length; ++j) {
            if (j == length / 2)
                std::srand(chosen(~seed));
            if (j % 4 == 0)
                cout << std::rand();
            else
                cout << ::random();
            cout << '\n';
        }
        cout << selections << '\n';
        std::srand(chosen(seed));
        for (int j = 0; j < 31; ++j)
            cout << std::rand() << '\n';
        srandom(chosen(seed));
        for (int j = 0; j < 31; ++j)
            cout << ::random() << '\n';
        cout << selections << ':' << own::random(i) << ':' << own::srand(i, 11) << '\n';
    }
}
