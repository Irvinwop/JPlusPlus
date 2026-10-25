#include <bits/stdc++.h>
using namespace std;

struct Edge {
    int to;
    long long cost;
};

int main() {
    int n, m;
    cin >> n >> m;
    vector<vector<Edge>> graph(n);
    for (int i = 0; i < m; ++i) {
        int from, to;
        long long cost;
        cin >> from >> to >> cost;
        graph[from].push_back({to, cost});
    }
    const long long infinity = 4000000000000000000LL;
    vector<long long> distance(n, infinity);
    priority_queue<pair<long long, int>, vector<pair<long long, int>>, greater<>> pending;
    distance[0] = 0;
    pending.push({0, 0});
    while (!pending.empty()) {
        auto [cost, node] = pending.top();
        pending.pop();
        if (cost != distance[node])
            continue;
        for (auto edge : graph[node]) {
            if (cost + edge.cost < distance[edge.to]) {
                distance[edge.to] = cost + edge.cost;
                pending.push({distance[edge.to], edge.to});
            }
        }
    }
    for (auto cost : distance)
        cout << cost << ' ';
    cout << '\n';
}
