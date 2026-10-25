#include <bits/stdc++.h>
using namespace std;

int shared = 13, evaluations, receivers;
int next() {
    return ++evaluations + shared;
}
int freeDefault(int amount = next(), int& target = shared) {
    target += amount;
    return target;
}
struct Methods {
    static const int scale = 7;
    int value;
    Methods(int amount = next(), int& target = shared) : value(amount) {
        target -= amount;
    }
    int size() {
        return value;
    }
    int size(int amount) {
        return value + amount;
    }
    int length(int amount = shared) {
        return value - amount;
    }
    int capacity(int amount) {
        return value * amount + scale;
    }
    int sync_with_stdio(int amount) {
        value += amount;
        return value;
    }
    int adjust(int amount = next(), int& target = shared) {
        target += amount;
        return value + target;
    }
    static int selected(int amount = next()) {
        return amount + receivers;
    }
    static void change(int& target = shared) {
        target += receivers;
    }
};
struct Other {
    int value;
    int size() {
        return -value;
    }
    int size(int amount) {
        return value ^ amount;
    }
};
Methods* receiver() {
    ++receivers;
    return nullptr;
}
struct ExtraCopy {
    int value;
    ExtraCopy(int amount) : value(amount) {}
    ExtraCopy(const ExtraCopy& other, int amount = next()) : value(other.value + amount) {}
};

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        int shared = x; // Defaults bind to the file variable, including in an inlined call.
        Methods first, second(y, shared);
        Other other{x};
        cout << first.size() << ' ' << first.size(y) << ' ' << other.size() << ' ' << other.size(y)
             << '\n';
        cout << first.length() << ' ' << first.capacity(y) << ' ' << first.sync_with_stdio(x)
             << '\n';
        cout << first.adjust() << ' ' << second.adjust(y, shared) << '\n';
        cout << freeDefault() << ' ' << shared << ' ' << ::shared << '\n';
        cout << receiver()->selected() << ' ' << receivers << '\n';
        receiver()->change();
        ExtraCopy original{x};
        ExtraCopy copied(original);
        cout << copied.value << ' ' << evaluations << ' ' << ::shared << '\n';
        vector<Methods> defaults(2);
        cout << defaults[0].size() << ' ' << defaults[1].size() << ' ' << ::shared << '\n';
    }
}
