#include <bits/stdc++.h>
using namespace std;

struct Tracked {
    static int defaults, copies, moves, copyAssignments, moveAssignments;
    int value;
    Tracked() : value(17) {
        ++defaults;
    }
    Tracked(int x) : value(x) {}
    Tracked(const Tracked& other) : value(other.value) {
        ++copies;
    }
    Tracked(Tracked&& other) : value(other.value) {
        ++moves;
        other.value = -71;
    }
    Tracked& operator=(const Tracked& other) {
        value = other.value;
        ++copyAssignments;
        return *this;
    }
    Tracked& operator=(Tracked&& other) {
        value = other.value;
        other.value = -83;
        ++moveAssignments;
        return *this;
    }
};
int Tracked::defaults, Tracked::copies, Tracked::moves;
int Tracked::copyAssignments, Tracked::moveAssignments;

void check(int x, int y) {
    array<vector<int>, 2> source{vector<int>{x, y}, vector<int>{x + y}};
    auto* allocation = source[0].data();
    auto retained = source[0].begin();
    auto copy = source;
    copy[0][0] += y;
    cout << source[0][0] << ' ' << copy[0][0] << ' ' << (allocation == copy[0].data()) << '\n';
    auto moved = std::move(source);
    cout << source[0].empty() << ' ' << (allocation == moved[0].data()) << ' ' << *retained << '\n';
    array<vector<int>, 2> target{vector<int>{y}, vector<int>{x}};
    auto& inlineVector = target[0];
    auto* inlineAddress = &target[0];
    auto* assignment = &(target = std::move(moved));
    cout << (assignment == &target) << ' ' << (&target[0] == inlineAddress) << ' '
         << moved[0].empty() << ' ' << (allocation == target[0].data()) << ' ' << inlineVector[0]
         << '\n';
    const array<vector<int>, 2> constant{vector<int>{x, y}, vector<int>{y, x}};
    auto constCopy = std::move(constant);
    constCopy[0][0] += y;
    cout << constant[0][0] << ' ' << constCopy[0][0] << '\n';

    array<Tracked, 2> objects{Tracked{x}, Tracked{y}};
    Tracked::defaults = Tracked::copies = Tracked::moves = 0;
    auto copiedObjects = objects;
    auto movedObjects = std::move(objects);
    cout << objects[0].value << ' ' << copiedObjects[0].value << ' ' << movedObjects[0].value << ' '
         << Tracked::defaults << ' ' << Tracked::copies << ' ' << Tracked::moves << '\n';
    auto& objectAlias = copiedObjects[1];
    Tracked::copyAssignments = Tracked::moveAssignments = 0;
    copiedObjects = movedObjects;
    movedObjects = std::move(copiedObjects);
    cout << objectAlias.value << ' ' << movedObjects[1].value << ' ' << Tracked::copyAssignments
         << ' ' << Tracked::moveAssignments << '\n';

    array<vector<int>[2], 2> rows{};
    rows[0][1] = {x, y};
    rows[1][0] = {y, x};
    auto* rowAllocation = rows[0][1].data();
    auto rowsCopy = rows;
    rowsCopy[0][1][0] += y;
    cout << rows[0][1][0] << ' ' << rowsCopy[0][1][0] << '\n';
    auto rowsMoved = std::move(rows);
    cout << rows[0][1].empty() << ' ' << (rowAllocation == rowsMoved[0][1].data()) << '\n';
    array<vector<int>[2], 2> rowsTarget{};
    rowsTarget[0][1] = {y};
    auto* rowAddress = &rowsTarget[0];
    auto& rawInline = rowsTarget[0][1];
    rowsTarget = std::move(rowsMoved);
    cout << (&rowsTarget[0] == rowAddress) << ' ' << rawInline[0] << ' ' << rowsMoved[0][1].empty()
         << ' ' << (rowAllocation == rowsTarget[0][1].data()) << '\n';

    array<array<Tracked[2], 2>, 2> nested{};
    nested[1][0][1].value = x - y;
    Tracked::defaults = Tracked::copies = Tracked::moves = 0;
    auto nestedMoved = std::move(nested);
    cout << nested[1][0][1].value << ' ' << nestedMoved[1][0][1].value << ' ' << Tracked::defaults
         << ' ' << Tracked::copies << ' ' << Tracked::moves << '\n';
    array<array<Tracked[2], 2>, 2> nestedTarget{};
    auto& nestedAlias = nestedTarget[1][0][1];
    Tracked::copyAssignments = Tracked::moveAssignments = 0;
    nestedTarget = std::move(nestedMoved);
    cout << nestedAlias.value << ' ' << nestedMoved[1][0][1].value << ' '
         << Tracked::copyAssignments << ' ' << Tracked::moveAssignments << '\n';

    array<int[2][3], 2> primitive{};
    primitive[1][1][2] = x + y;
    auto primitiveCopy = primitive;
    primitiveCopy[1][1][2] += y;
    auto primitiveMoved = std::move(primitive);
    primitiveMoved.front()[0][0] = x - y;
    array<int[2][3], 2> primitiveTarget{};
    auto& plane = primitiveTarget.at(1);
    int* scalar = plane[1];
    auto* planeAddress = &primitiveTarget[1];
    auto* container = &primitiveTarget;
    primitiveTarget = std::move(primitiveMoved);
    cout << scalar[2] << ' ' << primitive[1][1][2] << ' ' << primitiveCopy[1][1][2] << ' '
         << primitiveTarget.front()[0][0] << ' ' << (scalar == primitiveTarget.back()[1]) << ' '
         << (&primitiveTarget.at(1) == planeAddress) << '\n';
    cout << container->at(1)[1][2] << ' ' << container->front()[0][0] << ' '
         << (container->back()[1] == scalar) << '\n';

    array<Tracked, 50000> large{};
    large[49999].value = x + y;
    Tracked::defaults = Tracked::copies = Tracked::moves = 0;
    auto largeMoved = std::move(large);
    cout << large[49999].value << ' ' << largeMoved[49999].value << ' ' << Tracked::defaults << ' '
         << Tracked::copies << ' ' << Tracked::moves << '\n';
    auto& largeAlias = large[49999];
    Tracked::copyAssignments = Tracked::moveAssignments = 0;
    large = std::move(largeMoved);
    cout << largeAlias.value << ' ' << largeMoved[49999].value << ' ' << Tracked::copyAssignments
         << ' ' << Tracked::moveAssignments << '\n';
    array<Tracked, 0> empty{};
    auto emptyMoved = std::move(empty);
    empty = std::move(emptyMoved);
    cout << empty.size() << ' ' << emptyMoved.size() << '\n';
}

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        check(x, y);
    }
}
