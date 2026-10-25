#include <bits/stdc++.h>
using namespace std;

void increase(long long& value) {
    value += 3;
}

struct Total {
    long long sum;
    void operator()(long long value) {
        sum += value;
    }
};

int main() {
    int t;
    cin >> t;
    while (t--) {
        int n;
        cin >> n;
        vector<long long> values(n);
        for (auto& value : values)
            cin >> value;
        vector<long long> saved(n);
        auto end = copy(values.begin(), values.end(), saved.begin());
        cout << end - saved.begin() << ' ';
        for_each(values.begin(), values.end(), [](long long& value) { value = value * 2 + 1; });
        auto callback = for_each(values.begin(), values.end(), increase);
        long long sample = values[0];
        callback(sample);
        cout << sample << ' ';
        sort(values.begin(), values.end(),
             [](const auto& left, const auto& right) { return left > right ? 1 : 0; });
        sort(saved.begin(), saved.end(),
             [](long long left, long long right) { return left > right; });
        vector<long long> merged(2 * n);
        merge(values.begin(), values.end(), saved.begin(), saved.end(), merged.begin(),
              [](long long left, long long right) { return left > right; });
        auto total = for_each(merged.begin(), merged.end(), Total{0});
        cout << total.sum << '\n';
        vector<int> narrowed;
        copy(merged.begin(), merged.end(), back_inserter(narrowed));
        for (auto value : narrowed)
            cout << value << ' ';
        cout << '\n';
        copy_backward(merged.begin(), merged.end() - 1, merged.end());
        copy_n(saved.begin(), min(2, n), merged.begin());
        for (auto value : merged)
            cout << value << ' ';
        cout << '\n';
    }
}
