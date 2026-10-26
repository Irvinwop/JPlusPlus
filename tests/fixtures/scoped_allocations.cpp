#include <bits/stdc++.h>
using namespace std;

vector<unsigned char*> retired;
unsigned long long checksum;
int destroyed;

struct Owner {
    unsigned char* bytes;
    Owner(size_t size, int seed) : bytes(static_cast<unsigned char*>(malloc(size))) {
        if (!bytes)
            exit(2);
        bytes[0] = static_cast<unsigned char>(seed);
    }
    ~Owner() {
        checksum += bytes[0];
        free(bytes);
        ++destroyed;
    }
};

void work(int iteration, int seed) {
    Owner outer(1024 * 1024, seed + iteration);
    retired.push_back(outer.bytes);
    if (iteration % 3 == 0)
        return;
    Owner inner(512 * 1024, seed - iteration);
    retired.push_back(inner.bytes);
}

int main() {
    int rounds, seed;
    cin >> rounds >> seed;
    for (int i = 0; i < rounds; ++i)
        work(i, seed);
    cout << retired.size() << ' ' << destroyed << ' ' << checksum << '\n';
}
