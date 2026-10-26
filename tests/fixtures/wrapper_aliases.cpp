#include <bits/stdc++.h>
using namespace std;

void rebind(reference_wrapper<int>& handle, int& target) {
    handle = ref(target);
}
void byValue(reference_wrapper<int> handle, int& target) {
    handle = ref(target);
    handle.get() += 31;
}
reference_wrapper<int>& first(array<reference_wrapper<int>, 2>& values) {
    return values[0];
}
struct Box {
    reference_wrapper<int> binding;
};
struct Required {
    int value;
    Required(int x) : value(x * 3 + 17) {}
};
struct Nested {
    Required member;
};

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        int z = x - y;
        reference_wrapper<int> handle(x);
        auto& alias = handle;
        auto saved = handle;
        rebind(alias, y);
        alias.get() += 5;
        saved.get() += 7;
        cout << handle.get() << ' ' << x << ' ' << y << '\n';
        byValue(handle, z);
        cout << handle.get() << ' ' << y << ' ' << z << '\n';

        array<reference_wrapper<int>, 2> a{ref(x), ref(y)}, b{ref(y), ref(z)};
        auto& element = a[0];
        a = b;
        element.get() += 11;
        cout << a[0].get() << ' ' << x << ' ' << y << ' ' << z << '\n';
        first(a) = ref(z);
        element.get() += 17;
        cout << a[0].get() << ' ' << x << ' ' << y << ' ' << z << '\n';
        auto* assignment = &(handle = ref(x));
        cout << (assignment == &handle) << ' ' << handle.get() << '\n';

        Box box{ref(x)};
        auto& member = box.binding;
        member = ref(y);
        member.get() += 19;
        cout << box.binding.get() << ' ' << x << ' ' << y << '\n';
        reference_wrapper<reference_wrapper<int>> outer(handle);
        auto& nested = outer.get();
        nested = ref(z);
        cout << handle.get() << ' ' << (&outer.get() == &handle) << '\n';

        function<void(reference_wrapper<int>&, int&)> setter = rebind;
        setter(alias, x);
        function<reference_wrapper<int>&()> getter = [&]() -> reference_wrapper<int>& {
            return a[0];
        };
        getter() = ref(y);
        element.get() += 23;
        cout << handle.get() << ' ' << a[0].get() << ' ' << x << ' ' << y << ' ' << z << '\n';

        array<Required, 2> required{Required{x}, Required{y}};
        auto copied = required;
        copied[0].value += z;
        Nested nestedRequired{Required{z}};
        cout << required[0].value << ' ' << copied[0].value << ' ' << nestedRequired.member.value
             << '\n';
    }
}
