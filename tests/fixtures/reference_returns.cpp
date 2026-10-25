#include <bits/stdc++.h>
using namespace std;

int evaluations;
int& slot(int* values, int index) {
    ++evaluations;
    return values[index];
}
int& forwardSlot(int* values, int index, int remaining) {
    if (remaining)
        return forwardSlot(values, index, remaining - 1);
    return slot(values, index);
}
int*& cursor(int*& value) {
    return value;
}
int& choose(bool condition, int& first, int& second) {
    return condition ? first : second;
}
int& accumulate(int* values, int& index, int change) {
    return values[index++] += change;
}
int& persistent(int seed) {
    static int value = seed;
    return ++value;
}

struct ReferenceBox {
    int values[4];
    int& operator[](int index) {
        ++evaluations;
        return values[index];
    }
    int& operator*() {
        return values[0];
    }
    const int& operator()(int index) const {
        return values[index];
    }
};

const int& smaller(const int& first, const int& second) {
    return min(first, second);
}

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        int numbers[]{x, y, x + y, x - y};
        int& alias = slot(numbers, 1);
        alias += x;
        int* address = &slot(numbers, 1);
        address[1] += y;
        forwardSlot(numbers, 3, 4) ^= x;
        int& (*functionPointer)(int*, int) = &slot;
        functionPointer(numbers, 0) += 9;
        function<int&(int*, int)> callable = functionPointer;
        callable(numbers, 1) -= 3;
        choose(y & 1, numbers[0], numbers[1]) += x;
        int updated = 0;
        int& accumulated = accumulate(numbers, updated, y);
        accumulated ^= x;
        cout << updated << ' ' << (&accumulated == numbers) << '\n';
        int index = 0;
        slot(numbers, index++) = (index = 3);
        cout << index << ' ' << alias << ' ' << evaluations << '\n';
        int* pointer = numbers;
        cursor(pointer)++;
        *pointer += x;
        cout << pointer - numbers << ' ' << (&alias == pointer) << '\n';

        std::function<int&(int)> accessor = [&numbers](int position) -> int& {
            return numbers[position];
        };
        std::function<long long(int)> read = accessor;
        long long before = read(2);
        accessor(2) += 11;
        int& returned = accessor(2);
        returned += x;
        cout << before << ' ' << read(2) << ' ' << (&returned == numbers + 2) << '\n';

        ReferenceBox box{{x, y, x + y, 17}};
        const int& retained = box(1);
        box[1] += 7;
        *box += 5;
        int* member = &box[0];
        member[2] -= y;
        cout << retained << ' ' << *box << ' ' << box[2] << '\n';
        const int& selected = smaller(box[0], box[1]);
        const int& tied = max(box[0], box[0]);
        box[0] += 23;
        box[1] -= 11;
        cout << selected << ' ' << tied << '\n';
        int& state = persistent(x);
        state += y;
        cout << persistent(y) << ' ' << state << '\n';
        for (int value : numbers)
            cout << value << ' ';
        cout << evaluations << '\n';
    }
}
