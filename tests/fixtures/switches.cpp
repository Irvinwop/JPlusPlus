#include <bits/stdc++.h>
using namespace std;

enum class Toggle : bool { Off, On };
enum class Byte : unsigned char { Low = 0, High = 255 };
constexpr long long tag() {
    if consteval {
        return 0x100000001LL;
    } else {
        return 99;
    }
}
int calls;
long long select(long long value) {
    ++calls;
    return value;
}

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        unsigned u;
        unsigned long long bits;
        cin >> u >> bits;
        long long value = (long long)bits;
        int result = 0;
        switch (select(value)) {
        case 1:
            result = 3;
            break;
        case tag():
            result = 7;
            break;
        case LLONG_MIN:
            result = 11;
            break;
        case -1:
            result = 13;
            break;
        default:
            result = 17;
        }
        cout << result << ' ' << calls << '\n';
        switch (u) {
        case 0:
            result += 19;
        case 0xffffffffU:
            result += 23;
            break;
        case 0x80000000U:
            result += 29;
            break;
        default:
            result += u & 15;
        }
        cout << result << '\n';
        switch (bits) {
        case 0:
            result -= 1;
            break;
        case 1ULL << 63:
            result -= 3;
            break;
        case ~0ULL:
            result -= 5;
            break;
        default:
            result -= 7;
        }
        cout << result << '\n';
        switch (bits) {
        case 0xfffffffffffffff0ULL ... ~0ULL:
            result += 31;
            break;
        case 1 ... 7:
            result += 37;
            break;
        default:
            result += 41;
        }
        __int128 wide = (__int128)value * 4294967296LL + (u & 1);
        switch (wide) {
        case -((__int128)1 << 95)... - ((__int128)1 << 95) + 1:
            result += 43;
            break;
        case (__int128)1 << 32:
            result += 47;
        case ((__int128)1 << 32) + 1:
            result += 53;
            break;
        default:
            result += 59;
        }
        cout << result << '\n';
        Toggle active = u & 1 ? Toggle::On : Toggle::Off;
        switch (active) {
        case Toggle::On:
            result -= 61;
            break;
        case Toggle::Off:
            result -= 67;
            break;
        }
        Byte byte = u & 1 ? Byte::High : Byte::Low;
        switch (byte) {
        case Byte::High:
            result += 71;
            break;
        case Byte::Low:
            result += 73;
            break;
        }
        switch (int index = u & 3; value ^ index) {
        case 2:
            result += index;
        default:
            result -= index;
        }
        switch (long long selected = select(value)) {
        case 0:
            result ^= 7;
            break;
        default:
            result ^= 11;
        }
        cout << result << ' ' << calls << '\n';
    }
}
