#include <bits/stdc++.h>
using namespace std;

struct Item {
    int value;
};

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        deque<int> values;
        for (int i = 0; i < 80; ++i)
            values.push_back(x + i);
        int& retained = values[40];
        int* address = &values[40];
        for (int i = 0; i < 700; ++i) {
            values.push_front(y - i);
            values.push_back(x + i);
        }
        retained += y;
        *address ^= x;
        cout << (address == &values[740]) << ' ' << values[740] << ' ' << values.size() << '\n';
        auto observer = values.begin() + 740;
        int& throughIterator = *observer;
        int* iteratorAddress = &*observer;
        for (int i = 0; i < 17; ++i) {
            values.pop_front();
            values.pop_back();
        }
        cout << observer - values.begin() << ' ' << *observer << ' ' << (iteratorAddress == address)
             << '\n';
        throughIterator -= 11;
        deque<int> other{y, x, y + x};
        auto& containerAlias = values;
        swap(values, other);
        cout << containerAlias.size() << ' ' << (observer == other.begin() + 723) << ' '
             << (address == &other[723]) << ' ' << *observer << '\n';
        other.swap(values);
        values.swap(values);
        cout << containerAlias[723] << ' ' << throughIterator << '\n';

        deque<int> small{y, x, x + y, -1, 0, x};
        int assignment = (small[2] = y);
        int previous = small[1]++;
        for (int& value : small)
            value += 3;
        sort(small.begin(), small.end());
        auto finish = unique(small.begin(), small.end());
        small.erase(finish, small.end());
        auto inserted = small.insert(small.begin() + 1, x - y);
        cout << assignment << ' ' << previous << ' ' << *inserted << '\n';
        auto erased = small.erase(inserted);
        cout << *erased << ' ' << small.size() << '\n';
        for (auto at = small.rbegin(); at != small.rend(); ++at)
            cout << *at << ' ';
        cout << '\n';
        auto copy = small;
        copy[0] += 71;
        small.resize(small.size() + 3, x);
        small.resize(2);
        copy.assign(4, y);
        cout << small[0] << ' ' << small[1] << ' ' << copy.front() << ' ' << copy.back() << '\n';

        long long raw[]{x * 1000000007LL, y * 1000000007LL, 1, 2};
        deque<long long> range(raw, raw + 4);
        raw[0] = 99;
        deque<long long> zeros(3), filled(5, x * 1000000007LL);
        cout << range[0] << ' ' << range[1] << ' ' << zeros[1] << ' ' << filled[4] << '\n';
        deque<int> trimmed(80);
        iota(trimmed.begin(), trimmed.end(), x);
        auto survivor = trimmed.begin() + 20;
        trimmed.erase(trimmed.begin(), trimmed.begin() + 7);
        cout << *survivor << ' ' << survivor - trimmed.begin() << '\n';
        trimmed.erase(trimmed.begin() + 3, trimmed.begin() + 10);
        trimmed.erase(trimmed.end() - 10, trimmed.end() - 3);
        cout << trimmed[2] << ' ' << trimmed[3] << ' ' << trimmed.back() << ' ' << trimmed.size()
             << '\n';
        deque<unsigned> unsignedValues{~0U, 0x80000000U}, low{0, 1};
        cout << (unsignedValues > low) << ' ' << (low < unsignedValues) << '\n';
        deque<Item> items;
        Item item{x};
        items.push_back(item);
        item.value = y;
        items.push_front(item);
        int& member = items.back().value;
        auto* memberAddress = &member;
        for (int i = 0; i < 70; ++i)
            items.push_front({i});
        *memberAddress += y;
        cout << items.back().value << ' ' << member << ' ' << item.value << '\n';
    }
}
