#include <algorithm>
#include <array>
#include <iostream>
#include <vector>
struct Entry {
    int key;
    unsigned value;
};
struct ListEntry {
    int key;
    unsigned value;
};
template <class Range> void show(const Range& values) {
    for (const auto& value : values)
        std::cout << value.key << ':' << value.value << ',';
    std::cout << '\n';
}
int main() {
    Entry entries[] = {{3, 30}, {2, 20}, {1, 10}, {4, 40}};
    Entry& saved = entries[0];
    int* key = &entries[0].key;
    auto less = [](const Entry& a, const Entry& b) { return a.key < b.key; };
    std::sort(entries, entries + 4, less);
    show(entries);
    std::cout << saved.key << ' ' << *key << ' ' << (&saved == entries) << '\n';
    std::reverse(entries, entries + 4);
    show(entries);
    std::cout << saved.key << ' ' << *key << '\n';
    auto listLess = [](const ListEntry& a, const ListEntry& b) { return a.key < b.key; };
    std::vector<ListEntry> vector = {{9, 90}, {1, 10}, {7, 70}, {4, 40}};
    ListEntry& slot = vector[0];
    std::stable_sort(vector.begin(), vector.end(), listLess);
    show(vector);
    std::cout << slot.key << '\n';
    std::reverse(vector.begin(), vector.end());
    show(vector);
    std::cout << slot.key << '\n';
    std::array<ListEntry, 3> array = {{{6, 60}, {5, 50}, {8, 80}}};
    ListEntry& arraySlot = array[0];
    std::sort(array.begin(), array.end(), listLess);
    show(array);
    std::cout << arraySlot.key << '\n';
    std::reverse(array.begin(), array.end());
    show(array);
    std::cout << arraySlot.key << '\n';
    double sources[] = {
        0, -0.75, 2147483648.75, 4294967295.0, 9223372036854775808.0, 1e19, 18446744073709549568.0};
    for (volatile double source : sources) {
        if (source < 4294967296.0)
            std::cout << static_cast<unsigned>(source) << ' ';
        std::cout << static_cast<unsigned long long>(source) << '\n';
    }
}
