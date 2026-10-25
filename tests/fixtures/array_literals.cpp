#include <bits/stdc++.h>
using namespace std;

using Label = const char[4];
Label& selectLabel(bool first) {
    return first ? "red" : "blu";
}
template <size_t N> unsigned long long digest(const char (&text)[N]) {
    unsigned long long result = N;
    for (unsigned char value : text)
        result = result * 131 + value;
    return result;
}

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        auto& label = (x & 1) ? "red" : "blu";
        cout << label << ' ' << digest(label) << ' ' << digest((y & 1) ? "one" : "two") << '\n';
        Label& returned = selectLabel(y & 1);
        const char* first = returned;
        cout << returned << ' ' << (data(returned) == first) << ' '
             << (end(returned) - begin(returned)) << ' ' << (first + 3 - first) << '\n';
        cout << (static_cast<const void*>(&returned) == static_cast<const void*>(first)) << ' '
             << ((x & 1) ? "yes" : "no ")[y & 1] << '\n';
        Label& (*function)(bool) = selectLabel;
        std::function<Label&()> selected = [&]() -> Label& { return function(x & 1); };
        cout << digest(selected()) << ' ' << selected()[2] << '\n';
        cout << digest("a\0b") << ' ' << digest("élève") << ' ' << digest("\xff\x80X") << '\n';
        printf((x & 1) ? "%d\n" : "%d ", y);
        cout << ((y & 1) ? "equal" : "sizes") << '\n';
    }
}
