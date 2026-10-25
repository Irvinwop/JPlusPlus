#include <bits/stdc++.h>
using namespace std;

struct Value {
    int number;
};

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        int a = x, b = y;
        auto references = tie(a, b);
        auto copiedReferences = references;
        copiedReferences = make_tuple(y + 3, x - 2);
        tie(a, b) = tie(b, a);
        cout << a << ' ' << b << '\n';
        tie(ignore, b) = make_tuple(x, y * 2);
        get<0>(references) += get<1>(references);
        cout << a << ' ' << b << '\n';

        Value value{x};
        pair<int&, Value&> pairReferences(a, value);
        auto pairCopy = pairReferences;
        pairCopy.first += 5;
        pairCopy.second.number += y;
        cout << a << ' ' << value.number << '\n';
        pairReferences = pair<int, Value>{y, Value{x - y}};
        cout << a << ' ' << value.number << '\n';
        tuple<int&, int&> other(b, a);
        references.swap(other);
        cout << a << ' ' << b << '\n';

        int raw[2][3]{{x, y, 1}, {2, 3, 4}};
        auto arrays = tie(raw[0], raw[1]);
        int(&row)[3] = get<0>(arrays);
        row[1] += 17;
        int* element = get<0>(arrays) + 2;
        *element += get<1>(arrays)[1];
        cout << raw[0][1] << ' ' << raw[0][2] << ' ' << (&row == &raw[0]) << '\n';
        auto reverseArrays = tie(raw[1], raw[0]);
        swap(arrays, reverseArrays);
        cout << raw[0][1] << ' ' << raw[1][1] << ' ' << (&row == &raw[0]) << '\n';
        auto pointer = &references;
        pointer->swap(other);
        cout << a << ' ' << b << '\n';
        auto memory = static_cast<int(*)[3]>(malloc(sizeof(int[2][3])));
        memory[0][0] = x;
        memory[0][1] = y;
        memory[0][2] = 13;
        auto allocation = forward_as_tuple(memory[0]);
        int* address = get<0>(allocation);
        address[1] += 5;
        cout << memory[0][1] << ' ' << (address == &memory[0][0]) << '\n';
        auto sameRow = tie(memory[0]);
        auto nextRow = tie(memory[1]);
        cout << (allocation == sameRow) << ' ' << (allocation != nextRow) << ' '
             << (allocation < nextRow) << '\n';
        free(memory);
    }
}
