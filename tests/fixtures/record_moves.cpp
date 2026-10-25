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

struct Holder {
    vector<int> items;
    Holder* next = nullptr;
    int value = 0;
};
struct Group {
    Holder first;
    array<Holder, 2> more;
    vector<int> raw[2];
    Tracked grid[2][2];
    int coordinates[2][3];
};
struct ConstMember {
    const Tracked fixed;
    Tracked movable;
    const vector<int> fixedItems;
    vector<int> items;
};
struct CopyOnly {
    vector<int> items;
    CopyOnly(int x) : items{x, x + 1} {}
    CopyOnly(const CopyOnly&) = default;
};
class Secret {
    friend struct Vault;
    Secret(const Secret& other) : value(other.value + 3) {}
    Secret(Secret&& other) : value(other.value + 7) {
        other.value = -19;
    }
    Secret& operator=(const Secret& other) {
        value = other.value + 11;
        return *this;
    }
    Secret& operator=(Secret&& other) {
        value = other.value + 13;
        other.value = -29;
        return *this;
    }

  public:
    int value;
    Secret(int x) : value(x) {}
};
struct Vault {
    Secret key;
    vector<int> items;
    Vault(int x) : key(x), items{x, x + 1} {}
    Vault(const Vault&) = default;
    Vault(Vault&&) = default;
    Vault& operator=(const Vault&) = default;
    Vault& operator=(Vault&&) = default;
};
struct Marker {
    int value;
    Marker(int x) : value(x) {}
    Marker(Marker& other, bool increment) : value(other.value + (increment ? 37 : 0)) {}
    Marker(Marker&& other) : value(other.value) {
        other.value = -59;
    }
};
int trace;
Holder& target(Holder& value) {
    trace = trace * 10 + 1;
    return value;
}
Holder* targetPointer(Holder& value) {
    trace = trace * 10 + 3;
    return &value;
}
Holder&& source(Holder& value) {
    trace = trace * 10 + 2;
    return std::move(value);
}

void check(int x, int y) {
    Holder a{{x, y}, nullptr, x};
    auto allocation = a.items.data();
    auto retained = a.items.begin();
    Holder b = std::move(a);
    cout << a.items.empty() << ' ' << (allocation == b.items.data()) << ' ' << *retained << '\n';
    Holder c{{y}, nullptr, y};
    auto& inlineItems = c.items;
    auto& object = c;
    trace = 0;
    Holder* assignment = &(target(c) = source(b));
    cout << trace << ' ' << (assignment == &c) << ' ' << (&object == &c) << ' ' << b.items.empty()
         << ' ' << inlineItems[0] << ' ' << (allocation == c.items.data()) << '\n';

    Holder nextPayload{{y, x}, nullptr, y};
    trace = 0;
    Holder* explicitAssignment = &target(c).operator=(source(nextPayload));
    cout << trace << ' ' << (explicitAssignment == &c) << ' ' << nextPayload.items.empty() << ' '
         << inlineItems[0] << '\n';
    Holder finalPayload{{x - y}, nullptr, x};
    trace = 0;
    Holder* pointerAssignment = &targetPointer(c)->operator=(source(finalPayload));
    cout << trace << ' ' << (pointerAssignment == &c) << ' ' << finalPayload.items.empty() << ' '
         << inlineItems[0] << '\n';

    Group group;
    group.first.items = {x};
    group.more[0].items = {y};
    group.raw[1] = {x + y};
    group.grid[1][0].value = x - y;
    group.coordinates[1][2] = x + y;
    int* firstAllocation = group.first.items.data();
    int* moreAllocation = group.more[0].items.data();
    int* rawAllocation = group.raw[1].data();
    Tracked::defaults = Tracked::copies = Tracked::moves = 0;
    Group transferred = std::move(group);
    cout << group.first.items.empty() << ' ' << group.more[0].items.empty() << ' '
         << group.raw[1].empty() << ' ' << (firstAllocation == transferred.first.items.data())
         << ' ' << (moreAllocation == transferred.more[0].items.data()) << ' '
         << (rawAllocation == transferred.raw[1].data()) << ' ' << Tracked::defaults << ' '
         << Tracked::copies << ' ' << Tracked::moves << ' ' << transferred.grid[1][0].value << '\n';
    Group destination;
    auto& vectorAlias = destination.raw[1];
    auto& recordAlias = destination.more[0];
    auto& trackedAlias = destination.grid[1][0];
    auto* row = destination.grid[1];
    auto* vectorAddress = &destination.raw[1];
    int* coordinateAlias = destination.coordinates[1];
    Tracked::copyAssignments = Tracked::moveAssignments = 0;
    Group* groupAssignment = &(destination = std::move(transferred));
    cout << (groupAssignment == &destination) << ' ' << (&destination.raw[1] == vectorAddress)
         << ' ' << (destination.grid[1] == row) << ' ' << vectorAlias[0] << ' '
         << recordAlias.items[0] << ' ' << trackedAlias.value << ' ' << Tracked::copyAssignments
         << ' ' << Tracked::moveAssignments << '\n';

    cout << coordinateAlias[2] << ' ' << (coordinateAlias == destination.coordinates[1]) << '\n';

    ConstMember fixed{Tracked{x}, Tracked{y}, {x}, {y}};
    Tracked::defaults = Tracked::copies = Tracked::moves = 0;
    ConstMember mixed = std::move(fixed);
    cout << fixed.fixed.value << ' ' << fixed.movable.value << ' ' << mixed.fixed.value << ' '
         << mixed.movable.value << ' ' << fixed.fixedItems[0] << ' ' << fixed.items.empty() << ' '
         << Tracked::defaults << ' ' << Tracked::copies << ' ' << Tracked::moves << '\n';
    CopyOnly original(x);
    CopyOnly copy = std::move(original);
    copy.items[0] += y;
    cout << original.items[0] << ' ' << copy.items[0] << '\n';
    Vault locked(x);
    auto lockAllocation = locked.items.data();
    Vault unlocked = std::move(locked);
    cout << locked.key.value << ' ' << unlocked.key.value << ' ' << locked.items.empty() << ' '
         << (lockAllocation == unlocked.items.data()) << '\n';
    Vault assigned(y);
    auto& keyAlias = assigned.key;
    assigned = std::move(unlocked);
    cout << unlocked.key.value << ' ' << keyAlias.value << ' ' << assigned.items[0] << '\n';
    Marker originalMarker(x);
    Marker selected(originalMarker, true);
    Marker movedMarker = std::move(originalMarker);
    cout << selected.value << ' ' << movedMarker.value << ' ' << originalMarker.value << '\n';

    Holder payload{{x, y}, nullptr, x + y};
    auto* payloadAllocation = payload.items.data();
    vector<Holder> stored;
    stored.reserve(3);
    stored.push_back(std::move(payload));
    stored.push_back(stored[0]);
    stored[1].items[0] += y;
    cout << payload.items.empty() << ' ' << (payloadAllocation == stored[0].items.data()) << ' '
         << stored[0].items[0] << ' ' << stored[1].items[0] << '\n';

    const Vault sealed(x);
    Vault fromConst = std::move(sealed);
    fromConst.items[0] += y;
    cout << sealed.key.value << ' ' << fromConst.key.value << ' ' << sealed.items[0] << ' '
         << fromConst.items[0] << '\n';
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
