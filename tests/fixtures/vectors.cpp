#include <bits/stdc++.h>
using namespace std;

int main() {
    int t;
    cin >> t;
    while (t--) {
        int n;
        cin >> n;
        vector<long long> values(n);
        for (int i = 0; i < n; ++i)
            cin >> values[i];
        auto original = values;
        sort(values.begin(), values.end());
        values.erase(unique(values.begin(), values.end()), values.end());
        cout << accumulate(values.begin(), values.end(), 0LL) << ' ' << values.size() << '\n';
        for (auto x : original) {
            cout << lower_bound(values.begin(), values.end(), x) - values.begin() << ' ';
        }
        cout << '\n';
        auto first = values.begin();
        auto second = first;
        ++second;
        cout << (first == values.begin()) << ' ' << second - first << '\n';
        vector<vector<int>> grid(3, vector<int>(n + 1, 7));
        grid[0][0] = 99;
        grid[1].push_back(4);
        cout << grid[2][0] << ' ' << grid[0][0] << ' ' << grid[1].size() << '\n';
        vector<int> p{1, 2, 3};
        int permutations = 0;
        do {
            ++permutations;
        } while (next_permutation(p.begin(), p.end()));
        cout << permutations << '\n';
        pair<int, int> slots[4];
        slots[2] = {n, 7};
        swap(slots[0], slots[2]);
        cout << slots[0].first << ' ' << slots[2].second << '\n';
        vector<long long> grown;
        grown.resize(n + 2, -4LL);
        grown.resize(2);
        grown.assign(n, 6LL);
        cout << grown.size() << ' ' << grown.back() << '\n';
    }
}
