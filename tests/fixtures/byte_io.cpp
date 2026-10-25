#include <bits/stdc++.h>
using namespace std;

int main() {
    uint32_t words[5];
    memset(words, 0xa5, sizeof(words));
    printf("%zu %zu %zu %zu\n", fread(words, 0, SIZE_MAX, stdin), fread(words, SIZE_MAX, 0, stdin),
           fwrite(words, 0, SIZE_MAX, stdout), fwrite(words, SIZE_MAX, 0, stderr));
    auto items = fread(reinterpret_cast<unsigned char*>(words) + 3, 3, 4, stdin);
    printf("%zu:", items);
    for (auto word : words)
        printf("%08x ", word);
    printf("\n");
    fwrite(reinterpret_cast<unsigned char*>(words) + 2, 2, 8, stdout);
    array<uint32_t, 4> cells{0x12345678, 0x87654321, 0x13579bdf, 0x2468ace0};
    auto& alias = cells[1];
    auto amount = fread(cells.data() + 1, sizeof(uint32_t), 2, stdin);
    printf("\n%zu %08x %08x %08x %08x\n", amount, cells[0], alias, cells[2], cells[3]);
    fwrite(cells.data(), sizeof(uint32_t), cells.size(), stderr);

    size_t sizes[] = {1, 2, 3, 5, 7, 11};
    for (int batch = 0; batch < 25; ++batch) {
        unsigned char buffer[9008];
        memset(buffer, 0xa5, sizeof(buffer));
        size_t size = sizes[batch % 6], count = 700, requested = size * count;
        size_t received = fread(buffer + 1, size, count, stdin);
        unsigned sum = 0;
        for (size_t i = 0; i < received * size; ++i)
            sum += buffer[1 + i];
        printf("%zu %u %u %u %u\n", received, sum, buffer[0], buffer[requested + 1], buffer[9007]);
        size_t written = fwrite(buffer + 1, size, received, stdout);
        size_t errors = fwrite(buffer + 1, size, received, stderr);
        printf("%zu %zu\n", written, errors);
    }
    unsigned char tail[8] = {11, 13, 17, 19, 23, 29, 31, 37};
    auto end = fread(tail + 1, 1, 6, stdin);
    printf("%zu %u %u %u\n", end, tail[0], tail[1], tail[7]);
}
