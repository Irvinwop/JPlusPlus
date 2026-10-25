#include <iostream>
struct Cell {
    int first, second;
    long long weight;
};
consteval long long pattern(int input) {
    Cell* cells = new Cell[257]();
    cells[0].first = input;
    cells[128].second = input + 17;
    cells[256].weight = 1LL * input * input - 3;
    long long result = 0;
    for (int i = 0; i < 257; ++i)
        result += (i + 1LL) * cells[i].first + (i + 3LL) * cells[i].second + cells[i].weight;
    delete[] cells;
    return result;
}
struct Block {
    Cell cells[33];
};
consteval long long copied() {
    Block original{};
    original.cells[32].weight = 100;
    Block copy = original;
    copy.cells[32].weight += 7;
    copy.cells[0].first = 5;
    long long result = 0;
    for (int i = 0; i < 33; ++i)
        result += original.cells[i].weight - copy.cells[i].weight + original.cells[i].first;
    return result;
}
struct Self {
    const Self* owner = this;
};
consteval int addresses() {
    Self values[71]{};
    int matches = 0;
    for (int i = 0; i < 71; ++i)
        matches += values[i].owner == &values[i];
    return matches;
}
Cell globalCells[100003];
int main() {
    std::cout << pattern(-5) << ' ' << pattern(0) << ' ' << pattern(7) << ' ' << pattern(1000000000)
              << ' ' << copied() << ' ' << addresses() << '\n';
    globalCells[0].first = 19;
    globalCells[100002].weight = -71;
    std::cout << globalCells[1].first << ' ' << globalCells[100001].weight << ' '
              << globalCells[0].first + globalCells[100002].weight << '\n';
}
