#include <bits/stdc++.h>
using namespace std;

int main() {
    bool first = ios::sync_with_stdio(false);
    bool second = ios::sync_with_stdio(false);
    bool third = ios::sync_with_stdio(true);
    bool fourth = ios::sync_with_stdio(false);
    auto old = cin.tie(nullptr);
    bool wasCout = old == &cout;
    bool detached = cin.tie() == nullptr;
    auto previous = cin.tie(&cout);
    bool wasNull = previous == nullptr;
    cout << first << second << third << fourth << ' ' << wasCout << detached << wasNull << ' '
         << (cin.tie() == &cout) << ' ' << (cout.tie() == nullptr) << ' ' << (cerr.tie() == &cout)
         << ' ' << (clog.tie() == nullptr) << '\n';
    int cases;
    (cin >> cases).tie(nullptr)->sync_with_stdio(false);
    while (cases--) {
        int x, y;
        (cin >> x >> y).tie(&cout)->sync_with_stdio(false);
        cout << x + y << ' ' << x - y << '\n';
    }
    // cerr is tied to cout, so this makes the preceding buffered output observable.
    clog << "shared buffer ";
    cerr << "flush tied output";
    cout << unitbuf << "unit buffered " << numeric_limits<double>::infinity();
    cout << nounitbuf << "discarded by immediate termination";
    std::_Exit(0);
}
