#include <bits/stdc++.h>
using namespace std;
long long trace;
struct Watch {
    int id;
    Watch(int value) : id(value) {
        trace = trace * 17 + id;
    }
    Watch(const Watch& other) : id(other.id + 1) {
        trace = trace * 17 + id;
    }
    ~Watch() {
        trace = trace * 17 - id;
    }
    void close() {
        id += 3;
    }
};
class BufferedScope {
    char bytes[32];
    size_t used = 0;

  public:
    void put(char value) {
        bytes[used++] = value;
    }
    ~BufferedScope() {
        fwrite(bytes, 1, used, stdout);
    }
};
void borrow(Watch& value) {
    value.id += 5;
}
long long early(int x) {
    Watch first(x);
    {
        Watch second(x + 1);
        if (x & 1)
            return trace;
        borrow(second);
    }
    return trace;
}
void check(int x, int y) {
    if constexpr (false) {
        Watch(999);
    }
    if consteval {
        Watch(998);
    }
    {
        BufferedScope output;
        output.put(static_cast<char>('a' + (x & 7)));
        output.put(static_cast<char>('a' + (y & 7)));
    }
    cout << '\n';
    trace = 0;
    {
        Watch first(x);
        auto& alias = first;
        {
            Watch second(y);
            alias.close();
            borrow(second);
        }
        Watch copied(first);
    }
    cout << trace << '\n';
    trace = 0;
    auto result = early(x);
    cout << result << ' ' << trace << '\n';
    trace = 0;
    for (int i = 0; i < 4; ++i) {
        Watch local(x + i);
        if (i == 1)
            continue;
        if (i == 2)
            break;
    }
    cout << trace << '\n';
    trace = 0;
    {
        Watch leaving(y);
        if (x & 1)
            goto done;
        borrow(leaving);
    }
done:
    cout << trace << '\n';
    trace = 0;
    {
        Watch elided = Watch(x);
        Watch selected = x & 1 ? Watch(y) : Watch(x + y);
        cout << elided.id << ' ' << selected.id << '\n';
    }
    cout << trace << '\n';
}

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        check(x, y);
    }
}
