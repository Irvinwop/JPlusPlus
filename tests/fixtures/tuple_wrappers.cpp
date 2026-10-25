#include <bits/stdc++.h>
using namespace std;

struct Adder {
    int offset;
    int operator()(int& value) {
        value += offset;
        return value;
    }
};

int& increment(int& value) {
    ++value;
    return value;
}

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        int a = x, b = y;
        tuple<reference_wrapper<int>> wrappers(ref(a));
        auto oldWrapper = wrappers;
        wrappers = tuple<reference_wrapper<int>>(ref(b));
        get<0>(wrappers).get() += 7;
        get<0>(oldWrapper).get() -= 3;
        cout << a << ' ' << b << '\n';

        auto unwrapped = make_tuple(ref(a), ref(b));
        get<0>(unwrapped) += 4;
        tuple<int, int> values(unwrapped);
        get<1>(values) += 11;
        cout << a << ' ' << b << ' ' << get<1>(values) << '\n';
        pair<reference_wrapper<int>, int> pairWrapper(ref(a), x);
        auto oldPair = pairWrapper;
        pairWrapper = make_pair(ref(b), y);
        pairWrapper.first.get() += 2;
        oldPair.first.get() += 9;
        cout << a << ' ' << b << ' ' << pairWrapper.second << '\n';
        auto wrapper = ref(a);
        auto referenceToWrapper = tie(wrapper);
        referenceToWrapper = make_tuple(ref(b));
        wrapper.get() += 13;
        cout << a << ' ' << b << '\n';
        Adder adder{x};
        auto callable = ref(adder);
        cout << callable(a) << ' ' << adder.offset << '\n';
        auto function = ref(increment);
        function(b) += 3;
        apply(callable, tie(b));
        cout << a << ' ' << b << '\n';
    }
}
