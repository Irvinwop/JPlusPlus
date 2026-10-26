#include <bits/stdc++.h>
using namespace std;

struct Cell {
    int key;
    long long value;
};

vector<Cell*> retired;
long long checksum;

int main() {
    int rounds, seed;
    cin >> rounds >> seed;
    for (int round = 0; round < rounds; ++round) {
        auto* cells = static_cast<Cell*>(calloc(20000, sizeof(Cell)));
        if (!cells)
            return 2;
        for (int i = 0; i < 20000; ++i) {
            cells[i].key = i + round;
            cells[i].value = (1LL << 40) + seed * 1009LL - i;
        }
        checksum += cells[round % 20000].key + cells[19999 - round % 20000].value;
        retired.push_back(cells);
        free(cells);
    }
    cout << retired.size() << ' ' << checksum << '\n';
}
