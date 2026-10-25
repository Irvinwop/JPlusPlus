#include <bits/stdc++.h>
using namespace std;

int initialized, first = 10, second = 100, shared = 4;
int prepare(int seed) {
    ++initialized;
    return seed + 11;
}
int counter(int seed) {
    static int value = prepare(seed);
    value += seed;
    return value;
}
int selfValue(int delta) {
    static int value = value + 1;
    value += delta;
    return value;
}
long long history(int seed) {
    if (seed >= 0)
        return initialized;
    static vector<int> values;
    values.push_back(seed);
    return accumulate(values.begin(), values.end(), 0LL);
}
struct Stamp {
    int value;
    Stamp() : value(++initialized) {}
};
int object(int seed) {
    if (seed == 0)
        return initialized;
    static Stamp stamp;
    stamp.value += seed;
    return stamp.value;
}
int arrays(int seed) {
    static int values[4]{seed, seed + 1, seed + 2};
    static int* saved = values + 1;
    *saved += seed;
    return values[0] + values[1] + values[2] + values[3];
}
int bind(int& argument, int delta) {
    static int& destination = argument;
    destination += delta;
    return destination;
}
int node(int seed) {
    struct Node {
        Node* next;
        int value;
    };
    static Node self{&self, seed};
    static Node links[1]{{links, seed + 1}};
    self.next->value += seed;
    links[0].next->value += seed;
    return self.value + links[0].value;
}
template <int Start> int numbered(int seed) {
    static int count = Start;
    return count += seed;
}
int recurse(int depth) {
    static int count;
    ++count;
    if (depth)
        recurse(depth - 1);
    return count;
}
int lambdaCount(int seed) {
    auto callback = [seed] {
        static int count = seed;
        return ++count;
    };
    int firstCall = callback();
    return firstCall * 1000 + callback();
}
int external(int seed) {
    int shared = seed;
    {
        extern int shared;
        shared += seed;
    }
    return shared + ::shared;
}
void write(int seed) {
    static ofstream log("jpp-static.txt", ios::app);
    static ofstream&& delayed = ofstream("jpp-static-ref.txt");
    log << seed << '\n';
    delayed << seed * 2 << '\n';
}

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        cout << counter(x) << ' ' << counter(y) << ' ' << initialized << '\n';
        cout << selfValue(y) << ' ' << history(x) << ' ' << object(x) << '\n';
        cout << arrays(y) << ' ' << node(x) << '\n';
        cout << bind(x < 0 ? second : first, y) << ' ' << first << ' ' << second << '\n';
        cout << numbered<3>(x) << ' ' << numbered<7>(y) << ' ' << recurse(abs(y)) << '\n';
        cout << lambdaCount(x) << ' ' << external(y) << '\n';
        write(x + y);
    }
    exit(0);
}
