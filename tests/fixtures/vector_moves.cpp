#include <bits/stdc++.h>
using namespace std;

struct Traced {
    static int copies, moves;
    int value;
    Traced(int value) : value(value) {}
    Traced(const Traced& other) : value(other.value) {
        ++copies;
    }
    Traced(Traced&& other) : value(other.value) {
        ++moves;
        other.value = -77;
    }
};
int Traced::copies, Traced::moves;

vector<int> returned(int x, int y, int*& allocation) {
    vector<int> values{x, y, x + y};
    allocation = values.data();
    return values;
}

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        vector<Traced> first;
        first.reserve(4);
        first.emplace_back(x);
        first.emplace_back(y);
        auto& outerAlias = first;
        Traced& element = first[0];
        Traced* location = &element;
        auto iterator = first.begin();
        Traced::copies = Traced::moves = 0;
        vector<Traced> moved(std::move(first));
        element.value += 5;
        cout << outerAlias.empty() << ' ' << moved[0].value << ' ' << (location == &moved[0]) << ' '
             << (iterator == moved.begin()) << ' ' << Traced::copies << ' ' << Traced::moves
             << '\n';

        vector<Traced> target;
        target.emplace_back(y - 3);
        auto& targetAlias = target;
        Traced::copies = Traced::moves = 0;
        vector<Traced>* assignment = &(target = std::move(moved));
        cout << (assignment == &target) << ' ' << moved.empty() << ' ' << targetAlias[0].value
             << ' ' << (location == &target[0]) << ' ' << Traced::copies << ' ' << Traced::moves
             << '\n';
        auto copied = target;
        target[0].value += 7;
        cout << copied[0].value << ' ' << target[0].value << ' ' << Traced::copies << '\n';

        Traced::copies = Traced::moves = 0;
        auto wrapped = make_tuple(std::move(target));
        cout << target.empty() << ' ' << (location == &get<0>(wrapped)[0]) << '\n';
        vector<Traced> restored(std::move(get<0>(wrapped)));
        cout << get<0>(wrapped).empty() << ' ' << restored[0].value << ' '
             << (location == &restored[0]) << ' ' << Traced::copies << ' ' << Traced::moves << '\n';
        int* returnedAllocation = nullptr;
        auto result = returned(x, y, returnedAllocation);
        cout << result[0] << ' ' << (returnedAllocation == result.data()) << '\n';
    }
}
