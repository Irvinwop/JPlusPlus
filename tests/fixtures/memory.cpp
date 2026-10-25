#include <bits/stdc++.h>
using namespace std;

void dump(const void* storage, size_t count) {
    const unsigned char* bytes = static_cast<const unsigned char*>(storage);
    for (size_t i = 0; i < count; ++i)
        cout << unsigned(bytes[i]) << ' ';
    cout << '\n';
}

struct State {
    unsigned int word;
};

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        unsigned long long source[3]{0x0123456789abcdefULL + unsigned(x),
                                     0xfedcba9876543210ULL - unsigned(y), ~0ULL};
        unsigned int destination[8]{1, 2, 3, 4, 5, 6, 7, 8};
        unsigned char* bytes = reinterpret_cast<unsigned char*>(destination);
        size_t count = 1 + unsigned(x + 200) % 22;
        void* returned = memcpy(bytes + 3, source, count);
        cout << (returned == bytes + 3) << '\n';
        dump(destination, sizeof destination);
        cout << destination[0] << ' ' << destination[3] << ' ' << destination[6] << '\n';
        count = 1 + unsigned(y + 200) % 13;
        cout << (memset(bytes + 1, x - 1000, count) == bytes + 1) << '\n';
        dump(destination, sizeof destination);
        memmove(bytes + 5, bytes + 1, 20);
        dump(destination, sizeof destination);
        __builtin_memmove(bytes + 1, bytes + 7, 19);
        dump(destination, sizeof destination);
        unsigned char* found = static_cast<unsigned char*>(memchr(bytes, bytes[13] + 256, 32));
        cout << (found - bytes) << ' ' << (memchr(bytes, bytes[0], 0) == nullptr) << '\n';
        unsigned char other[32]{};
        __builtin_memcpy(other, destination, sizeof destination);
        cout << (memcmp(other, destination, sizeof destination) == 0) << ' ';
        other[11] ^= 128;
        int ordering = __builtin_memcmp(other, destination, sizeof destination);
        cout << ((ordering > 0) - (ordering < 0)) << '\n';

        short small[5]{short(x), short(y), -1, 32767, -32768};
        unsigned short unsignedSmall[5]{};
        memcpy(unsignedSmall, small, sizeof small);
        dump(small, sizeof small);
        cout << unsignedSmall[0] << ' ' << unsignedSmall[2] << ' ' << unsignedSmall[4] << '\n';
        unsigned char* smallBytes = reinterpret_cast<unsigned char*>(small);
        smallBytes[1] = 0x80;
        cout << small[0] << ' ' << (smallBytes + 3 - smallBytes) << '\n';

        State state{unsigned(x + 1000)};
        unsigned int& alias = state.word;
        unsigned char* member = reinterpret_cast<unsigned char*>(&alias);
        member[2] = unsigned(y) & 255;
        cout << state.word << ' ' << (member == reinterpret_cast<unsigned char*>(&state.word))
             << '\n';
        state.word = 0x7f123456U;
        dump(member, sizeof state.word);
        unsigned int local = 0;
        unsigned char* localBytes = reinterpret_cast<unsigned char*>(&local);
        __builtin_memset(&local, x, 3);
        cout << local << ' ' << unsigned(localBytes[3]) << '\n';
        local = 0x879abcdeU;
        cout << unsigned(localBytes[1]) << ' ' << (static_cast<void*>(localBytes) == &local)
             << '\n';

        int* allocated = static_cast<int*>(calloc(9, sizeof(int)));
        cout << allocated[0] << ' ' << allocated[8] << '\n';
        allocated[3] = x;
        allocated[8] = y;
        unsigned char* allocationBytes = reinterpret_cast<unsigned char*>(allocated);
        allocationBytes[1] = 0x34;
        cout << allocated[0] << ' ' << allocated[3] << ' ' << allocated[8] << '\n';
        allocated = static_cast<int*>(realloc(allocated, 15 * sizeof(int)));
        allocated[14] = x - y;
        cout << allocated[0] << ' ' << allocated[3] << ' ' << allocated[8] << ' ' << allocated[14]
             << '\n';
        allocated = static_cast<int*>(realloc(allocated, 4 * sizeof(int)));
        cout << allocated[0] << ' ' << allocated[3] << '\n';
        free(allocated);
        char* dynamic = static_cast<char*>(malloc(5));
        memcpy(dynamic, "hello", 5);
        dump(dynamic, 5);
        free(dynamic);
        free(nullptr);

        string text = "mutable";
        memcpy(text.data() + 1, "BCD", 3);
        char characters[8]{};
        memcpy(characters, text.c_str(), text.size() + 1);
        cout << text << ' ' << characters << '\n';
        bool truth[4]{true, false, true, y > 0}, restored[4]{};
        memcpy(restored, truth, sizeof truth);
        for (bool value : restored)
            cout << value << ' ';
        cout << '\n';
        unsigned __int128 wide = (static_cast<unsigned __int128>(source[0]) << 64) | source[1];
        unsigned __int128 wideCopy = 0;
        memcpy(&wideCopy, &wide, sizeof wide);
        cout << static_cast<unsigned long long>(wideCopy >> 64) << ' '
             << static_cast<unsigned long long>(wideCopy) << '\n';
    }

    unsigned int floatBits[5]{0, 0x80000000U, 0x7f800000U, 0x7fc12345U, 0xffc76543U};
    float values[5]{};
    memcpy(values, floatBits, sizeof values);
    unsigned int roundTrip[5]{};
    memcpy(roundTrip, values, sizeof roundTrip);
    dump(roundTrip, sizeof roundTrip);
    unsigned long long doubleBits[4]{0, 0x8000000000000000ULL, 0x7ff8000012345678ULL,
                                     0xfff0000000000000ULL};
    double doubles[4]{};
    memcpy(doubles, doubleBits, sizeof doubles);
    dump(doubles, sizeof doubles);
    char16_t characters16[3]{0x12ab, 0x8000, 0xffff};
    unsigned short units[3]{};
    memcpy(units, characters16, sizeof units);
    for (unsigned short value : units)
        cout << value << ' ';
    cout << '\n';
    __int128 signedWide = -static_cast<__int128>(0x123456789abcdefULL);
    __int128 signedCopy = 0;
    memcpy(&signedCopy, &signedWide, sizeof signedWide);
    cout << static_cast<long long>(signedCopy) << '\n';
    enum class Status : unsigned char { idle = 0, busy = 0xab, done = 0xfe };
    Status statuses[3]{Status::idle, Status::busy, Status::done};
    dump(statuses, sizeof statuses);
    unsigned char replacement = 0x7f;
    memcpy(statuses + 1, &replacement, 1);
    cout << unsigned(statuses[1]) << '\n';
    byte octets[3]{byte{0x12}, byte{0xab}, byte{0xfe}};
    dump(octets, sizeof octets);

    vector<unsigned char> large(40017);
    for (int i = 0; i < int(large.size()); ++i)
        large[i] = (i * 37 + i / 251) & 255;
    memmove(large.data() + 3, large.data(), 38001);
    memmove(large.data(), large.data() + 9, 37003);
    unsigned long long checksum = 0;
    for (unsigned char value : large)
        checksum = checksum * 131 + value;
    cout << checksum << '\n';
    size_t overflow = size_t(1) << 63;
    cout << (calloc(overflow, 2) == nullptr) << '\n';
}
