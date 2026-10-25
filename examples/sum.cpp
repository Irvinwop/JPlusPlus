#include <bits/stdc++.h>
using namespace std;

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    int n;
    cin >> n;
    long long answer = 0;
    for (int i = 0; i < n; ++i) {
        long long value;
        cin >> value;
        answer += value;
    }
    cout << answer << '\n';
}
