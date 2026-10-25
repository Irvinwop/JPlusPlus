#include <bits/stdc++.h>
using namespace std;

using Row = int[5];
using Plane = Row[3];

int calls;
Row& rowAt(Plane& plane, int index) {
    ++calls;
    return plane[index];
}
long long total(const Plane& plane) {
    long long sum = 0;
    for (const auto& row : plane)
        for (int value : row)
            sum = sum * 3 + value;
    return sum;
}
int* neighbor(int& element) {
    return &element + 1;
}
void update(Plane& plane, int increment) {
    for (auto& row : plane)
        for (auto& value : row)
            value += increment;
}
struct Grid {
    Plane data;
};

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        Grid grid{};
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 5; ++j)
                grid.data[i][j] = x * (i + 1) + y * (j + 2);
        calls = 0;
        Row& alias = rowAt(grid.data, 1);
        alias[2] += y;
        *neighbor(alias[2]) += x;
        cout << grid.data[1][2] << ' ' << grid.data[1][3] << ' ' << (&alias == grid.data + 1) << ' '
             << (alias == &grid.data[1][0]) << '\n';
        Row& (*get)(Plane&, int) = rowAt;
        get(grid.data, 2)[3] = x - y;
        function<Row&()> selected = [&]() -> Row& { return rowAt(grid.data, 0); };
        selected()[4] += 17;
        cout << calls << ' ' << total(grid.data) << '\n';
        update(grid.data, y);
        auto& [first, second, third] = grid.data;
        second[1] += first[4];
        cout << (&second == grid.data + 1) << ' ' << total(grid.data) << '\n';
        auto [copyFirst, copySecond, copyThird] = grid.data;
        copyFirst[0] += 91;
        cout << copyFirst[0] << ' ' << grid.data[0][0] << ' ' << (&copySecond == &second) << '\n';
        cout << (end(alias) - begin(alias)) << ' ' << size(alias) << ' ' << ssize(alias) << ' '
             << (data(alias) == &grid.data[1][0]) << '\n';

        Plane* allocation = static_cast<Plane*>(calloc(1, sizeof(Plane)));
        Plane& heap = *allocation;
        memcpy(&heap, &grid.data, sizeof heap);
        Row* rows = heap;
        rows[2][4] -= x;
        update(heap, 3);
        auto& [left, middle, right] = heap;
        middle[2] += y;
        *neighbor(middle[2]) += x;
        cout << total(heap) << ' ' << (+middle == +rows[1]) << ' '
             << (static_cast<void*>(&middle) == static_cast<void*>(rows + 1)) << '\n';
        for (auto row : heap)
            row[0] -= y;
        for (auto& [a, b, c, d, e] : heap)
            c += a + e;
        for (auto [a, b, c, d, e] : heap)
            a += b + c + d + e;
        cout << total(heap) << ' ' << total(grid.data) << '\n';
        rowAt(heap, 1)[2]++;
        ++rowAt(heap, 1)[3];
        cout << calls << ' ' << total(heap) << '\n';
        auto [heapFirst, heapSecond, heapThird] = heap;
        heapSecond[0] += 31;
        cout << heapSecond[0] << ' ' << heap[1][0] << '\n';
        Row& selectedRow = heap[1];
        auto snapshot = [selectedRow]() mutable {
            selectedRow[0] += 19;
            return selectedRow[0];
        };
        selectedRow[0] += 7;
        cout << snapshot() << ' ' << snapshot() << ' ' << heap[1][0] << '\n';
        Row&& moved = std::move(heap[2]);
        moved[1] -= y;
        cout << (std::as_const(moved) == data(heap[2])) << ' ' << total(heap) << '\n';
        free(allocation);

        Row* dynamic = new Row[3]{};
        dynamic[1][2] = x;
        dynamic[2][4] = y;
        unsigned char* dynamicBytes = reinterpret_cast<unsigned char*>(dynamic);
        memset(dynamicBytes + 17, 0x5a, 7);
        cout << dynamic[0][4] << ' ' << dynamic[1][0] << ' ' << dynamic[1][2] << ' '
             << dynamic[2][4] << ' ' << (dynamic + 3 - dynamic) << '\n';
        delete[] dynamic;

        unsigned int cube[2][3][4]{};
        for (int i = 0; i < 2; ++i)
            for (int j = 0; j < 3; ++j)
                for (int k = 0; k < 4; ++k)
                    cube[i][j][k] = unsigned(x) * 1234567U + unsigned(y) * (i * 12 + j * 4 + k);
        unsigned char* bytes = reinterpret_cast<unsigned char*>(&cube);
        unsigned short source[4]{0x1234, 0xabcd, 0x5678, 0xef01};
        memcpy(bytes + 45, source, sizeof source);
        memset(bytes + 15, y, 5);
        memmove(bytes + 11, bytes + 3, 63);
        unsigned int restored[2][3][4]{};
        memcpy(&restored, &cube, sizeof cube);
        unsigned long long checksum = 0;
        for (auto& plane : restored)
            for (auto& row : plane)
                for (auto value : row)
                    checksum = checksum * 131 + value;
        cout << checksum << ' ' << cube[1][0][0] << ' ' << restored[0][2][3] << '\n';
        unsigned int* element = &cube[1][2][1];
        cout << (element - cube[1][2]) << ' ' << (reinterpret_cast<unsigned char*>(element) - bytes)
             << ' ' << (static_cast<void*>(cube[1]) == static_cast<void*>(&cube[1][0][0])) << '\n';
    }
}
