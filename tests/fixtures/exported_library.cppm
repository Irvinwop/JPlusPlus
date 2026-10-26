export module jpp_practice;

namespace practice {
int effects;
int initialize() {
    effects += 5;
    return 23;
}
int startup = initialize();
} // namespace practice

export namespace practice {
enum Mode { Subtract, Add };
struct Calculator {
    int bias;
    int apply(int value) const {
        return value + bias;
    }
};
template <class T> T combine(T first, T second) {
    return first * 3 + second;
}
int evaluate(int first, int second) {
    ++effects;
    Calculator calculator{startup + first};
    Mode mode = (first & 1) ? Subtract : Add;
    return combine(calculator.apply(second), mode == Add ? first : -first);
}
int checkpoint() {
    return effects * 100 + startup;
}
} // namespace practice
