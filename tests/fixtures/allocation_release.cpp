#include <bits/stdc++.h>
using namespace std;

vector<unsigned char*> retired;

int main() {
    int rounds, seed;
    cin >> rounds >> seed;
    unsigned long long checksum = 0;
    for (int i = 0; i < rounds; ++i) {
        size_t bytes = (1024 * 1024) + static_cast<size_t>(i % 3) * 65536;
        auto* block = static_cast<unsigned char*>(i & 1 ? malloc(bytes) : calloc(bytes, 1));
        if (!block) {
            cout << "allocation failed " << i << '\n';
            return 2;
        }
        block[0] = static_cast<unsigned char>(seed + i);
        block[bytes - 1] = static_cast<unsigned char>(seed - i);
        retired.push_back(block);
        if (i % 4 == 0) {
            auto* resized = static_cast<unsigned char*>(realloc(block, bytes + 8192));
            if (!resized) {
                free(block);
                return 3;
            }
            block = resized;
            retired.push_back(block);
        }
        checksum += block[0] ^ block[bytes - 1];
        free(block);
    }
    cout << retired.size() << ' ' << checksum << '\n';
}
