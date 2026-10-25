#include <bits/stdc++.h>
using namespace std;

void dump(const vector<int>& values) {
    for (int value : values)
        cout << value << ' ';
    cout << '\n';
}

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        vector<int> first{x, y, x + y}, second{x - y, y - x};
        first.reserve(32);
        second.reserve(32);
        auto& containerAlias = first;
        int& retained = first[1];
        int* address = first.data();
        auto iterator = first.begin() + 1;
        auto reverse = first.rbegin();
        swap(first, second);
        retained += 9;
        cout << containerAlias.front() << ' ' << (address == second.data()) << ' '
             << (iterator == second.begin() + 1) << ' ' << *reverse << '\n';
        ++iterator;
        second.erase(iterator);
        dump(first);
        dump(second);
        first.swap(second);
        first.swap(first);
        cout << retained << ' ' << (address == first.data()) << ' ' << (&containerAlias == &first)
             << '\n';
        auto copied = first;
        first[0] += 7;
        dump(copied);
        dump(containerAlias);

        vector<vector<int>> nested{{x, y}, {y + 3, x - 2}};
        auto& inner = nested[0];
        int& element = inner[0];
        auto allocation = inner.data();
        swap(nested[0], nested[1]);
        element += 13;
        cout << inner[0] << ' ' << nested[1][0] << ' ' << (allocation == nested[1].data()) << '\n';
        auto left = tie(first), right = tie(second);
        swap(left, right);
        dump(containerAlias);
        dump(second);
        vector<int> empty;
        vector<int>* pointer = &second;
        pointer->swap(empty);
        cout << second.empty() << ' ' << empty.size() << '\n';
        dump(empty);

        array<vector<int>, 2> table{vector<int>{x, y}, vector<int>{y, x}};
        array<vector<int>, 2> replacement{vector<int>{x + 3}, vector<int>{y - 2}};
        auto& vectorAlias = table[0];
        int& oldElement = table[0][0];
        auto oldAllocation = table[0].data();
        swap(table, replacement);
        oldElement += 19;
        cout << vectorAlias[0] << ' ' << replacement[0][0] << ' '
             << (oldAllocation == replacement[0].data()) << '\n';
        vector<int> raw[2]{{x, y}, {y, x}};
        vector<int> other[2]{{x + 11}, {y - 7}};
        auto& rawAlias = raw[0];
        auto allocationBefore = raw[0].data();
        swap(raw, other);
        cout << rawAlias[0] << ' ' << (allocationBefore == other[0].data()) << '\n';

        array<int, 3> fixed{x, y, x + y}, fixedOther{x + 1, y + 2, x - y};
        int* inlineStorage = fixed.data();
        int& inlineElement = fixed[1];
        fixed.swap(fixedOther);
        cout << inlineStorage[0] << ' ' << inlineElement << ' ' << (inlineStorage == fixed.data())
             << '\n';
    }
}
