#include <algorithm>
#include <functional>
#include <iostream>
#include <limits>
#include <ranges>

int copies, moves, visits;
struct Handler {
    int state;
    Handler(int state) : state(state) {}
    Handler(const Handler& other) : state(other.state + 10) {
        ++copies;
    }
    Handler(Handler&& other) : state(other.state + 100) {
        other.state = -7;
        ++moves;
    }
    void operator()(int value) {
        state += value;
    }
};
void count(int value) {
    visits += value;
}

int main() {
    int cases;
    std::cin >> cases;
    while (cases--) {
        int first, offset;
        std::cin >> first >> offset;
        int last = first + 9 + (offset < 0 ? -offset : offset);
        auto build = std::views::iota;
        auto each = std::ranges::for_each;
        auto values = build(first, last);
        Handler original(offset);
        copies = moves = visits = 0;
        auto result = each(values, original);
        std::cout << result.fun.state << ' ' << original.state << ' ' << copies << ' ' << moves
                  << ' ' << (result.in == values.end()) << '\n';

        int state = offset;
        auto closure = [state](int value) mutable {
            state += value;
            return state;
        };
        auto copied = each(values, closure);
        std::cout << closure(0) << ' ' << copied.fun(0) << '\n';
        auto borrowed = each(build(1, 4), std::ref(closure));
        std::cout << borrowed.fun(2) << ' ' << closure(0) << '\n';
        auto generic = [state](auto value) mutable {
            state += value;
            return state;
        };
        auto genericResult = each(build(1, 4), generic);
        std::cout << generic(0) << ' ' << genericResult.fun(0) << '\n';
        each(build(1, 6), count);
        std::function<void(int)> function = count;
        each(build(3, 7), std::ref(function));
        std::cout << visits << '\n';

        long long sum = 0;
        for (int value : values)
            sum += value;
        auto position = values.begin();
        auto old = position++;
        position += 2;
        auto shifted = 2 + position;
        auto& alias = position;
        alias = shifted - 1;
        std::cout << sum << ' ' << *old << ' ' << *position << ' ' << position[3] << ' '
                  << (position - values.begin()) << ' ' << (position > old) << '\n';
        std::cout << values[7] << ' ' << values.front() << ' ' << values.back() << ' '
                  << values.size() << ' ' << static_cast<bool>(values) << '\n';
        decltype(values) selected(values.begin() + 2, values.end() - 3);
        std::cout << selected.front() << ' ' << selected.back() << ' ' << selected.size() << '\n';
        std::ranges::iota_view<int, int> empty;
        decltype(empty.begin()) zero;
        std::cout << empty.empty() << ' ' << empty.size() << ' ' << *zero << ' '
                  << static_cast<bool>(empty) << '\n';
        std::ranges::iota_view<short, long long> heterogeneous(-7, 19);
        sum = 0;
        for (short value : heterogeneous)
            sum += value;
        std::cout << heterogeneous.size() << ' ' << sum << '\n';

        auto changing = build(1, 5);
        sum = 0;
        for (int value : changing) {
            sum += value;
            if (value == 1)
                changing = build(10, 12);
        }
        std::cout << sum << ' ' << changing.front() << '\n';
        auto unsignedValues = build(0U, 3U);
        auto unsignedPosition = unsignedValues.begin();
        --unsignedPosition;
        std::cout << *unsignedPosition << ' ' << (unsignedPosition > unsignedValues.begin()) << ' '
                  << (unsignedPosition - unsignedValues.begin()) << '\n';
        ++unsignedPosition;
        std::cout << (unsignedPosition == unsignedValues.begin()) << ' ' << *unsignedPosition
                  << '\n';
        unsignedPosition -= 3;
        unsignedPosition += 5;
        auto previous = unsignedPosition--;
        std::cout << *previous << ' ' << *unsignedPosition << ' ' << unsignedPosition[3] << '\n';
        auto unsignedLongs = build(0ULL, 3ULL);
        auto longPosition = unsignedLongs.begin() - 1;
        std::cout << *longPosition << ' ' << (longPosition > unsignedLongs.begin()) << ' '
                  << static_cast<unsigned long long>(longPosition - unsignedLongs.begin()) << '\n';
        auto narrow = build(static_cast<unsigned char>(0), static_cast<unsigned char>(7));
        auto narrowPosition = narrow.begin();
        --narrowPosition;
        std::cout << static_cast<unsigned>(*narrowPosition) << ' '
                  << (narrowPosition > narrow.begin()) << '\n';

        auto enormous =
            build(std::numeric_limits<long long>::min(), std::numeric_limits<long long>::max());
        auto distant = enormous.begin();
        distant += std::numeric_limits<long long>::max();
        std::cout << static_cast<unsigned long long>(enormous.size()) << ' ' << *distant << ' '
                  << static_cast<unsigned long long>(enormous.end() - enormous.begin()) << '\n';
        auto upper = build(std::numeric_limits<unsigned long long>::max() - 3,
                           std::numeric_limits<unsigned long long>::max());
        sum = 0;
        for (unsigned long long value : upper)
            sum += value & 15;
        std::cout << upper.size() << ' ' << sum << ' ' << upper[2] << '\n';
    }
}
