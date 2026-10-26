#include <cstdlib>
#include <iostream>
#include <vector>

#define CHECK(NAME, TYPE)                                                                          \
    {                                                                                              \
        TYPE result = 37;                                                                          \
        bool overflow = __builtin_##NAME(first, static_cast<TYPE>(second), &result);               \
        std::cout << #NAME << ':' << overflow << ':' << static_cast<unsigned long long>(result)    \
                  << '\n';                                                                         \
    }

struct Record {
    int before, value, after;
};

int selections;
int* select(int* pointer) {
    ++selections;
    return pointer;
}

int main() {
    int count;
    std::cin >> count;
    while (count--) {
        long long first;
        unsigned long long second;
        std::cin >> first >> second;
        CHECK(sadd_overflow, int);
        CHECK(ssub_overflow, int);
        CHECK(smul_overflow, int);
        CHECK(uadd_overflow, unsigned int);
        CHECK(usub_overflow, unsigned int);
        CHECK(umul_overflow, unsigned int);
        CHECK(saddl_overflow, long);
        CHECK(ssubl_overflow, long);
        CHECK(smull_overflow, long);
        CHECK(uaddl_overflow, unsigned long);
        CHECK(usubl_overflow, unsigned long);
        CHECK(umull_overflow, unsigned long);
        CHECK(saddll_overflow, long long);
        CHECK(ssubll_overflow, long long);
        CHECK(smulll_overflow, long long);
        CHECK(uaddll_overflow, unsigned long long);
        CHECK(usubll_overflow, unsigned long long);
        CHECK(umulll_overflow, unsigned long long);

        int array[] = {11, 13, 17};
        selections = 0;
        bool overflow = __builtin_add_overflow(first, second, select(array + 1));
        std::cout << overflow << ':' << array[0] << ':' << array[1] << ':' << array[2] << ':'
                  << selections << '\n';
        Record record{19, 23, 29};
        overflow = __builtin_sub_overflow(first, second, &record.value);
        std::cout << overflow << ':' << record.before << ':' << record.value << ':' << record.after
                  << '\n';
        std::vector<int> values{31, 37, 41};
        int& alias = values[1];
        overflow = __builtin_mul_overflow(first, second, values.data() + 1);
        std::cout << overflow << ':' << values[0] << ':' << alias << ':' << values[2] << '\n';
        int* memory = static_cast<int*>(std::malloc(3 * sizeof(int)));
        memory[0] = 43;
        memory[1] = 47;
        memory[2] = 53;
        overflow = __builtin_add_overflow(first, second, memory + 1);
        std::cout << overflow << ':' << memory[0] << ':' << memory[1] << ':' << memory[2] << '\n';
        std::free(memory);
    }
}
