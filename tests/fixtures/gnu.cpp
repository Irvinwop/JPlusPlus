#include <bits/stdc++.h>
using namespace std;

int calls;
int choose(int value) {
    ++calls;
    return value;
}
void consumeReference(int&) {}
struct Box {
    int value;
};

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        const int original = x;
        int fallback = 0;
        calls = 0;
        int selected = choose(x) ?: (++fallback, y);
        cout << selected << ' ' << calls << ' ' << fallback << '\n';
        (x ?: y) = 9;
        int previous = (x ?: y)++;
        cout << x << ' ' << y << ' ' << previous << '\n';
        map<int, int> seen;
        int condition = original;
        consumeReference(condition ?: seen[42]);
        cout << seen.size() << '\n';
        int value = original;
        int result = ({
            int value = y - original;
            for (int i = 0; i < 6; ++i) {
                if (i == 2)
                    continue;
                value += i;
                if (value > 30)
                    break;
            }
            value + original;
        });
        cout << value << ' ' << result << '\n';
        Box box{original};
        Box copy = ({ box; });
        copy.value += y;
        cout << box.value << ' ' << copy.value << '\n';
        int&& moved = std::move(y);
        moved += 3;
        const long long& temporary = (long long)(choose(original) ?: y);
        cout << y << ' ' << temporary << ' ' << calls << '\n';
        ({
            int nested = y;
            ({ nested += original; });
            cout << nested << '\n';
            (void)0;
        });
        int written = ({
            ofstream output("gnu.txt");
            output << original << '\n';
            y + original;
        });
        ifstream input("gnu.txt");
        int restored;
        input >> restored;
        cout << written << ' ' << restored << '\n';
    }
}
