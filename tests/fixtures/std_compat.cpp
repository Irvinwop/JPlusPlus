import std.compat;
import std;

#if defined(EOF) || defined(RAND_MAX) || defined(errno) || defined(offsetof) || defined(va_arg)
#error Library macros must not be exported by modules
#endif

static_assert(__is_same(size_t, decltype(sizeof(0))));
static_assert(__is_same(ptrdiff_t,
                        decltype(static_cast<int*>(nullptr) - static_cast<int*>(nullptr))));
static_assert(__is_same(nullptr_t, decltype(nullptr)));

int main() {
    unsigned long long sizes[] = {
        sizeof(size_t),   sizeof(ptrdiff_t), sizeof(nullptr_t),   sizeof(max_align_t),
        sizeof(FILE),     sizeof(fpos_t),    sizeof(tm),          sizeof(clock_t),
        sizeof(time_t),   sizeof(int64_t),   sizeof(uint64_t),    sizeof(div_t),
        sizeof(ldiv_t),   sizeof(lldiv_t),   sizeof(mbstate_t),   sizeof(wint_t),
        sizeof(wctype_t), sizeof(wctrans_t), sizeof(fenv_t),      sizeof(fexcept_t),
        sizeof(va_list),  sizeof(jmp_buf),   sizeof(sig_atomic_t)};
    for (auto size : sizes)
        std::cout << size << ',';
    std::cout << '\n';

    int count;
    std::cin >> count;
    for (int i = 0; i < count; ++i) {
        int64_t value;
        int code;
        std::cin >> value >> code;
        int* values = static_cast<int*>(malloc(4 * sizeof(int)));
        for (int j = 0; j < 4; ++j)
            values[j] = static_cast<int>((value + j * 17) % 1000003);
        long long total = 0;
        for (int j = 0; j < 4; ++j)
            total = total * 7 + values[j];
        free(values);

        char bytes[] = "ab\0cd";
        memmove(bytes + 1, bytes, 5);
        printf("%lld:%llu:%zu:%d:%d:", total, static_cast<unsigned long long>(value) * 1000003ULL,
               strlen(bytes), toupper(static_cast<unsigned char>(code)),
               tolower(static_cast<unsigned char>(code)));
        for (auto byte : bytes)
            printf("%u,", static_cast<unsigned char>(byte));
        printf(":%.9f\n", sqrt(fabs(static_cast<double>(value))));

        std::vector<int> ordered{3, -1, static_cast<int>(value % 71)};
        std::sort(ordered.begin(), ordered.end());
        for (int item : ordered)
            std::cout << item << ',';
        std::cout << '\n';
    }
}
