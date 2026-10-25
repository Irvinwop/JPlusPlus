#include <vector>

struct Accumulator {
    long long total;
    void push(int value) {
        total += (long long)value * value;
    }
};

long long score(std::vector<int> values) {
    Accumulator sum{};
    for (int value : values)
        sum.push(value);
    return sum.total;
}
