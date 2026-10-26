import std;
using namespace std;
using namespace std::literals;
static_assert(std::is_same_v<decltype(std::views::iota(0, 3)), std::ranges::iota_view<int, int>>);
static_assert(std::is_base_of_v<std::exception, std::bad_alloc>);
static_assert(requires {
    static_cast<void* (*)(std::size_t, const std::nothrow_t&) noexcept>(&::operator new);
    static_cast<void (*)(void*, const std::nothrow_t&) noexcept>(&::operator delete);
});
struct Item {
    int key;
    long long score;
};
int main() {
    int count;
    cin >> count;
    auto text = "module"s;
    auto duration = 17ms + 3ms;
    cout << text << ':' << duration.count() << '\n';
    for (int test = 0; test < count; ++test) {
        int n;
        cin >> n;
        vector<pair<int, long long>> values;
        map<int, int> frequencies;
        for (int i = 0; i < n; ++i) {
            int key;
            long long score;
            cin >> key >> score;
            values.emplace_back(key, score);
            ++frequencies[key];
        }
        sort(values.begin(), values.end());
        long long total = 0;
        for (auto i : views::iota(0, n))
            total += values[i].second * (i + 1);
        function<long long(int)> evaluate = [total](int value) { return total + value * 7; };
        bitset<65> bits(static_cast<unsigned long long>(total));
        bits.set(64);
        auto result = make_tuple(values.front().first, values.back().first, evaluate(n));
        auto [smallest, largest, score] = result;
        std::printf("%d:", test);
        cout << setfill('_') << right << setw(7) << smallest << ':' << largest << ':' << score
             << ':' << frequencies.size() << ':' << bits.count() << '\n';
        Item a{smallest, score}, b{largest, total};
        Item items[] = {b, a};
        sort(items, items + 2,
             [](const Item& left, const Item& right) { return left.key < right.key; });
        cout << items[0].key << ':' << items[1].key << '\n';
    }
}
