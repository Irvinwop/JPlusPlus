#include <bits/stdc++.h>
using namespace std;
struct Tracking {
    static int constructed;
    int value = 999;
    Tracking() { ++constructed; }
    int jppLayout() const { return value + 3; }
};
int Tracking::constructed = 0;
struct __attribute__((packed)) Packed {
    char tag;
    unsigned long long payload;
    short delta;
};
struct Forward;
Forward* makeForward();
struct Forward { int value; };
Forward* makeForward() { return static_cast<Forward*>(calloc(3, sizeof(Forward))); }
int main() {
    Tracking* values = static_cast<Tracking*>(calloc(2, sizeof(Tracking)));
    cout << Tracking::constructed << ' ' << values[0].value << '\n';
    values[1].value = 73;
    cout << values[1].jppLayout() << ' ' << Tracking::constructed << '\n';
    Tracking ordinary;
    cout << Tracking::constructed << ' ' << ordinary.jppLayout() << '\n';
    memcpy(&ordinary, values + 1, sizeof(Tracking));
    cout << ordinary.jppLayout() << ' ' << Tracking::constructed << '\n';
    free(values);
    auto* packed = static_cast<Packed*>(calloc(3, sizeof(Packed)));
    packed[1].tag = 'z';
    packed[1].payload = 0x8765432112345678ULL;
    packed[1].delta = -333;
    auto* bytes = reinterpret_cast<unsigned char*>(packed);
    bytes[sizeof(Packed) + offsetof(Packed, payload) + 2] = 0x81;
    cout << sizeof(Packed) << ' ' << packed[1].tag << ' ' << packed[1].payload << ' '
         << packed[1].delta << '\n';
    free(packed);
    auto* forward = makeForward();
    forward[2].value = -931;
    cout << forward[2].value << ' ' << (forward + 2 - forward) << '\n';
    free(forward);
}
