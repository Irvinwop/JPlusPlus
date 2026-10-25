#include <bits/stdc++.h>
using namespace std;

bool descending;
struct Direction {
    bool reversed = descending;
    bool operator()(int first, int second) const {
        return reversed ? first > second : first < second;
    }
};

template <class Map> void dump(const Map& values) {
    for (auto entry : values)
        cout << entry.first << ':' << entry.second << ' ';
    cout << '\n';
}

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        map<int, int> first{{1, x}, {4, y}, {7, x + y}}, second{{2, x - y}, {5, y - x}};
        auto position = first.find(4);
        auto& entry = *position;
        int& retained = position->second;
        int* address = &retained;
        auto& mapAlias = first;
        swap(first, second);
        retained += 9;
        entry.second ^= x;
        cout << mapAlias.count(2) << ' ' << mapAlias.count(4) << ' '
             << (address == &second.at(4)) << ' ' << (position == second.find(4)) << '\n';
        ++position;
        cout << position->first << ' ' << position->second << '\n';
        --position;
        second.erase(position);
        dump(first);
        dump(second);
        first.swap(second);
        first.swap(first);
        dump(mapAlias);
        dump(second);

        descending = false;
        map<int, int, Direction> ascending{{1, x}, {4, y}, {7, 3}};
        descending = true;
        map<int, int, Direction> reversed{{2, y}, {5, x}, {8, 2}};
        auto moved = ascending.find(4);
        auto reverseMoved = reversed.find(5);
        auto& orderingAlias = ascending;
        swap(ascending, reversed);
        ascending[6] = x + y;
        reversed[6] = x - y;
        ++moved;
        ++reverseMoved;
        cout << moved->first << ' ' << reverseMoved->first << '\n';
        dump(orderingAlias);
        dump(reversed);
        auto copied = ascending;
        ascending = reversed;
        dump(ascending);
        dump(copied);
        Direction supplied;
        map<int, int, Direction> explicitOrder(supplied);
        supplied.reversed = false;
        explicitOrder[1] = x;
        explicitOrder[4] = y;
        dump(explicitOrder);
        pair<int, short> raw[]{{x, short(y)}, {y, short(x)}, {-1, 17}, {-1, 19}};
        map<unsigned, long long> converted(raw, raw + 4);
        raw[0].second = 99;
        dump(converted);
        map<int, int, greater<>> range(raw, raw + 4, greater<>());
        dump(range);
        pair<int, int> nested[2][3]{{{x, y}, {y, x}}, {{-1, 7}}};
        map<int, int> partial(nested[1], nested[1] + 3);
        dump(partial);

        unordered_map<int, int> hashed{{11, x}, {17, y}}, other{{23, x + y}};
        auto hashedPosition = hashed.find(17);
        int& hashedValue = hashedPosition->second;
        auto* hashedAddress = &hashedValue;
        auto& hashedAlias = hashed;
        swap(hashed, other);
        hashedValue -= x;
        cout << hashedAlias.count(23) << ' ' << (hashedPosition == other.find(17)) << ' '
             << (hashedAddress == &other.at(17)) << ' ' << other.at(17) << '\n';
        other.erase(hashedPosition);
        cout << hashed.size() << ' ' << other.size() << ' ' << other.count(17) << '\n';
        unordered_map<int, int> unorderedRange(raw, raw + 4);
        cout << unorderedRange.size() << ' ' << unorderedRange.at(-1) << '\n';
    }
}
