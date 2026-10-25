#include <bits/stdc++.h>
using namespace std;

struct Tracked {
    static int copies, moves, copyAssignments, moveAssignments;
    int value;
    Tracked(int x) : value(x) {}
    Tracked(const Tracked& other) : value(other.value) {
        ++copies;
    }
    Tracked(Tracked&& other) : value(other.value) {
        ++moves;
        other.value = -77;
    }
    Tracked& operator=(const Tracked& other) {
        value = other.value;
        ++copyAssignments;
        return *this;
    }
    Tracked& operator=(Tracked&& other) {
        value = other.value;
        other.value = -88;
        ++moveAssignments;
        return *this;
    }
};
int Tracked::copies, Tracked::moves, Tracked::copyAssignments, Tracked::moveAssignments;

namespace custom {
struct Token {
    int value;
};
void swap(Token& a, Token& b) {
    int previous = a.value;
    a.value = b.value + 1;
    b.value = previous - 1;
}
} // namespace custom

struct Converted {
    int value;
    Converted(int x) : value(x + 3) {}
    Converted(const Tracked& other) : value(other.value * 2) {}
    Converted& operator=(int x) {
        value = x * 3;
        return *this;
    }
    Converted& operator=(Tracked&& other) {
        value = other.value + 17;
        other.value = -99;
        return *this;
    }
};

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        Tracked::copies = Tracked::moves = Tracked::copyAssignments = Tracked::moveAssignments = 0;
        tuple<Tracked> first{Tracked{x}};
        auto copied = first;
        auto moved = std::move(first);
        cout << get<0>(first).value << ' ' << get<0>(copied).value << ' ' << get<0>(moved).value
             << ' ' << Tracked::copies << ' ' << Tracked::moves << '\n';
        copied = moved;
        moved = std::move(copied);
        cout << get<0>(copied).value << ' ' << get<0>(moved).value << ' '
             << Tracked::copyAssignments << ' ' << Tracked::moveAssignments << '\n';
        tuple<Converted> converted(moved);
        cout << get<0>(converted).value << '\n';
        converted = tuple<int>{y};
        cout << get<0>(converted).value << '\n';
        converted = std::move(moved);
        cout << get<0>(converted).value << ' ' << get<0>(moved).value << '\n';
        auto concatenated = tuple_cat(std::move(first), copied);
        cout << get<0>(concatenated).value << ' ' << get<1>(concatenated).value << ' '
             << Tracked::copies << ' ' << Tracked::moves << '\n';
        first.swap(copied);
        cout << get<0>(first).value << ' ' << get<0>(copied).value << ' ' << Tracked::moves << ' '
             << Tracked::moveAssignments << '\n';
        tuple<custom::Token> left{custom::Token{x}}, right{custom::Token{y}};
        swap(left, right);
        cout << get<0>(left).value << ' ' << get<0>(right).value << '\n';
        string text = "before";
        tie(text) = tuple<char>{'Z'};
        cout << text << '\n';

        tuple<Tracked> payload{Tracked{x}};
        Tracked::copies = Tracked::moves = 0;
        vector<tuple<Tracked>> rows;
        rows.reserve(3);
        rows.push_back(std::move(payload));
        rows.emplace_back(std::move(rows[0]));
        rows.emplace_back(y);
        cout << get<0>(payload).value << ' ' << get<0>(rows[0]).value << ' '
             << get<0>(rows[1]).value << ' ' << get<0>(rows[2]).value << ' ' << Tracked::copies
             << ' ' << Tracked::moves << '\n';

        pair<Tracked, int> entry{Tracked{x}, y};
        Tracked::copies = Tracked::moves = 0;
        queue<pair<Tracked, int>> pending;
        pending.push(std::move(entry));
        pending.emplace(Tracked{y}, x);
        cout << entry.first.value << ' ' << pending.front().first.value << ' ' << Tracked::copies
             << ' ' << Tracked::moves << '\n';
        pending.pop();
        cout << pending.front().first.value << '\n';

        Tracked::copies = Tracked::moves = 0;
        deque<tuple<Tracked>> endpoints;
        endpoints.push_front(std::move(rows[1]));
        endpoints.emplace_back(y);
        endpoints.push_back(rows[2]);
        get<0>(rows[2]).value += 17;
        cout << get<0>(rows[1]).value << ' ' << get<0>(endpoints.front()).value << ' '
             << get<0>(endpoints.back()).value << ' ' << Tracked::copies << ' ' << Tracked::moves
             << '\n';
    }
}
