#include <bits/stdc++.h>
#include <ext/pb_ds/assoc_container.hpp>
#include <ext/pb_ds/tree_policy.hpp>
using namespace std;
using namespace __gnu_pbds;
template <class T, class Compare = less<T>>
using ordered_set = tree<T, null_type, Compare, rb_tree_tag, tree_order_statistics_node_update>;

template <class Tree> void show(const Tree& values) {
    for (auto value : values)
        cout << value << ',';
    cout << ':' << values.size() << '\n';
}
int main() {
    ordered_set<int> values;
    int count;
    cin >> count;
    for (int i = 0; i < count; ++i) {
        int operation, key;
        cin >> operation >> key;
        if (operation == 0) {
            auto result = values.insert(key);
            cout << result.second << ' ' << *result.first;
        } else if (operation == 1)
            cout << values.erase(key);
        else if (operation == 2) {
            auto position = values.lower_bound(key);
            if (position != values.end())
                cout << *position;
            else
                cout << 'E';
        } else if (operation == 3) {
            auto position = values.upper_bound(key);
            if (position != values.end())
                cout << *position;
            else
                cout << 'E';
        } else if (operation == 4)
            cout << values.order_of_key(key);
        else if (operation == 5) {
            auto position = values.find_by_order(static_cast<size_t>(key));
            if (position != values.end())
                cout << *position;
            else
                cout << 'E';
        } else if (operation == 6) {
            auto position = values.find(key);
            if (position != values.end()) {
                auto next = values.erase(position);
                if (next != values.end())
                    cout << *next;
                else
                    cout << 'E';
            } else
                cout << '-';
        } else {
            ordered_set<int> copied(values);
            copied.insert(key);
            ordered_set<int> assigned;
            assigned = copied;
            copied.clear();
            show(assigned);
        }
        cout << ':' << values.size() << '\n';
    }
    show(values);
    for (auto position = values.rbegin(); position != values.rend(); ++position)
        cout << *position << ',';
    cout << '\n';
    if (!values.empty()) {
        auto end = values.end();
        auto reverseEnd = values.rend();
        cout << *--end << ' ' << *--reverseEnd << '\n';
    }
    ordered_set<int> other;
    bool heldElement = !values.empty();
    auto position = values.begin();
    values.swap(other);
    if (heldElement)
        cout << (position == other.begin());
    else
        cout << 'E';
    cout << ' ' << values.empty() << '\n';
    ordered_set<int>& alias = values;
    swap(values, other);
    show(alias);

    int range[] = {9, 1, 3, 3, -7, 42};
    ordered_set<int, greater<>> descending(range, range + 6, greater<>{});
    show(descending);
    cout << descending.order_of_key(3) << ' ' << *descending.find_by_order(2) << '\n';
    ordered_set<int, greater<>>::iterator forward = descending.rbegin();
    cout << *forward << ' ' << *--forward << '\n';
    forward = descending.rbegin();
    cout << *--forward << '\n';
    ordered_set<int, greater<>>::reverse_iterator reverse = descending.begin();
    cout << *reverse << ' ' << *--reverse << '\n';
    reverse = descending.begin();
    cout << *--reverse << '\n';
    ordered_set<int, greater<>>::iterator singular;
    singular = descending.begin();
    cout << *singular << '\n';
    ordered_set<pair<unsigned long long, int>, less<>> pairs;
    pair<unsigned long long, int> key = {~0ULL, 2};
    pairs.insert(key);
    key.first = 0;
    pairs.insert(key);
    pairs.insert({1ULL << 63, -1});
    pairs.insert({~0ULL, -1});
    auto selected = pairs.find_by_order(2);
    auto saved = pairs.find_by_order(1);
    const auto* address = &*saved;
    pairs.erase(pairs.begin());
    cout << (address == &*saved) << ' ' << selected->first << ' ' << selected->second << '\n';
    cout << pairs.order_of_key({~0ULL, 0}) << '\n';
    for (auto pair : pairs) {
        pair.first = 0;
        cout << pair.second << ',';
    }
    cout << '\n' << pairs.begin()->first << '\n';

    ordered_set<int> large;
    for (int i = 0; i < 30000; ++i)
        large.insert(i);
    auto held = large.find(17000);
    for (int i = 0; i < 30000; i += 3)
        large.erase(i);
    long long checksum = 0;
    for (int i = 0; i < 30000; ++i)
        checksum += large.order_of_key(i);
    for (size_t i = 0; i < large.size(); i += 97)
        checksum += *large.find_by_order(i);
    cout << large.size() << ' ' << checksum << ' ' << *held << ' ' << *++held << '\n';
}
