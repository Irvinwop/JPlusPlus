#include <bits/stdc++.h>
using namespace std;

struct Direction {
    static bool descending;
    bool reversed;
    Direction() : reversed(descending) {}
    bool operator()(int left, int right) const {
        return reversed ? left > right : left < right;
    }
};
bool Direction::descending;

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        map<int, int> values;
        for (int i = 0; i <= 8; i += 2)
            values[i] = x + i;
        auto bound = values.lower_bound(y);
        if (bound != values.end())
            bound->second += y;
        auto found = values.find(4);
        int& retained = values[4];
        int* saved = &found->second;
        auto& entry = *found;
        entry.second += x;
        auto snapshot = *found;
        snapshot.second += 999;
        values[7] = x - y;
        values[4] += y;
        cout << retained << ' ' << *saved << ' ' << snapshot.second << ' ' << (&values[4] == saved)
             << ' ' << (&*values.find(4) == &entry) << '\n';
        auto& [key, mapped] = *found;
        mapped += x;
        int& checked = values.at(4);
        checked += y;
        cout << key << ' ' << values[4] << ' ' << (&mapped == saved) << '\n';
        for (auto it = values.begin(); it != values.end(); ++it)
            if (it->first < 7 && it->first % 2 == 0)
                values[it->first + 1] = it->second + y;
        for (auto& value : values)
            value.second += y;
        for (auto value : values)
            cout << value.first << ':' << value.second << ' ';
        cout << '\n';
        for (auto it = values.rbegin(); it != values.rend(); ++it)
            cout << it->first << ':' << it->second << ' ';
        cout << '\n';
        auto last = values.end();
        --last;
        cout << last->first << ' ' << distance(values.begin(), values.end()) << ' '
             << distance(values.rbegin(), values.rend()) << '\n';
        auto range = values.equal_range(4);
        cout << range.first->first << ' ' << range.second->first << '\n';
        auto successor = values.erase(values.find(1));
        cout << successor->first << ' ' << values.count(1) << '\n';
        successor = values.erase(values.lower_bound(5), values.end());
        cout << (successor == values.end()) << ' ' << values.size() << '\n';

        map<int, int> source;
        source[2] = 700;
        source[12] = y;
        auto moved = source.find(12);
        int& alias = moved->second;
        int* address = &alias;
        values.merge(source);
        values[12] += x;
        alias += y;
        cout << values[12] << ' ' << (address == &values[12]) << ' ' << (moved == values.find(12))
             << ' ' << source.count(12) << ' ' << source[2] << '\n';
        ++moved;
        cout << (moved == values.end()) << '\n';
        map<int, int*> nullable;
        nullable[1] = nullptr;
        cout << nullable.erase(1) << ' ' << nullable.size() << '\n';

        multiset<int> repeated;
        for (int value : {x, y, x, 7, y})
            repeated.insert(value);
        cout << distance(repeated.begin(), repeated.end()) << ' '
             << distance(repeated.rbegin(), repeated.rend()) << '\n';
        auto position = next(repeated.begin(), 2);
        cout << *position << ' ' << distance(repeated.begin(), position) << '\n';
        auto matches = repeated.equal_range(x);
        cout << distance(matches.first, matches.second) << '\n';
        Direction::descending = false;
        map<int, int, Direction> ascendingMap;
        set<int, Direction> ascendingSet;
        for (int key : {3, 1, 2}) {
            ascendingMap[key] = key + x;
            ascendingSet.insert(key);
        }
        Direction::descending = true;
        map<int, int, Direction> assignedMap;
        set<int, Direction> assignedSet;
        assignedMap = ascendingMap;
        assignedSet = ascendingSet;
        for (auto entry : assignedMap)
            cout << entry.first << ':' << entry.second << ' ';
        for (int key : assignedSet)
            cout << key << ' ';
        cout << '\n';
    }
}
