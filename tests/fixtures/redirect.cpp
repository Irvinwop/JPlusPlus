#include <bits/stdc++.h>
using namespace std;

int main() {
    if (!freopen("jpp-redirect.in", "r", stdin))
        return 2;
    if (!freopen("jpp-redirect.out", "w", stdout))
        return 3;
    int n;
    cin >> n;
    long long sum = 0;
    for (int i = 0; i < n; ++i) {
        long long value;
        cin >> value;
        sum += value * (i + 1);
    }
    cout << sum << '\n';
    printf("%d\n", n);
}
