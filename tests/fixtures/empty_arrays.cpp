#include <bits/stdc++.h>
using namespace std;
struct Immobile {
    Immobile() = delete;
    Immobile(const Immobile&) = delete;
    Immobile(Immobile&&) = delete;
    Immobile& operator=(const Immobile&) = delete;
    Immobile& operator=(Immobile&&) = delete;
};
int visits;
array<Immobile, 0> a, b;
array<Immobile, 0>& destination() {
    visits = visits * 10 + 1;
    return a;
}
array<Immobile, 0>&& source() {
    visits = visits * 10 + 2;
    return std::move(b);
}
int main() {
    auto copy = a;
    auto moved = std::move(copy);
    auto* result = &(destination() = source());
    cout << a.size() << ' ' << moved.size() << ' ' << (result == &a) << ' ' << visits << '\n';
}
