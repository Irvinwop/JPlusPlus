#include <bits/stdc++.h>
using namespace std;

int System = 3;
int Math = System + 4;

struct Integer {
    int Long;
    Integer(int value) : Long(value) {}
    int sum(int Math) {
        return Long + Math + ::System;
    }
};
struct Main {
    int value;
};
enum class Left { Integer = 2 };
enum class Right { Integer = 5 };

int cppMain() {
    return -901;
}
namespace custom {
bool is_constant_evaluated(int value) {
    return value < 0;
}
} // namespace custom

int main() {
    int input;
    cin >> input;
    Integer value(input);
    Main main{value.sum(::Math)};
    vector<int> ArrayList{input, main.value, ::System};
    long long Long = 0;
    for (auto System : ArrayList)
        Long += 1LL * System * (::Math + (int)Left::Integer - (int)Right::Integer);
    cout << main.value << ' ' << Long << ' ' << ::System << '\n';
    cout << custom::is_constant_evaluated(input) << '\n';
    vector<int> repeated{1000 + input, 1000 + input};
    int& left = repeated[0];
    int& right = repeated[1];
    vector<double> floating{0.0, -0.0, numeric_limits<double>::quiet_NaN()};
    cout << (repeated[0] == repeated[1]) << ' ' << (left == right) << ' '
         << (floating[0] == floating[1]) << ' ' << (floating[2] == floating[2]) << ' '
         << (floating[2] != floating[2]) << '\n';
}
