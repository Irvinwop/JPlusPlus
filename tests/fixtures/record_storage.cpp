#include <bits/stdc++.h>
using namespace std;

struct Node {
    unsigned left, right;
    long long total;
    char tag;
    short small;
    float ratio;
    double weight;
};

void check(int seed) {
    Node* nodes = static_cast<Node*>(calloc(6, sizeof(Node)));
    if (!nodes)
        exit(2);
    auto& saved = nodes[2];
    unsigned* savedLeft = &saved.left;
    char* origin = reinterpret_cast<char*>(nodes);
    cout << saved.left << ' ' << saved.total << ' ' << saved.ratio << '\n';
    nodes[0].left = 0x87654321u + seed;
    nodes[0].right = 111 + seed;
    nodes[0].total = (1LL << 54) + seed;
    nodes[0].tag = 'a' + (seed & 7);
    nodes[0].small = -912 + seed;
    nodes[0].ratio = 1.25f;
    nodes[0].weight = -3.5;
    memcpy(nodes + 2, nodes, sizeof(Node));
    cout << saved.left << ' ' << saved.total << ' ' << saved.tag << ' ' << saved.small << ' '
         << saved.ratio << ' ' << saved.weight << '\n';
    *savedLeft += 17;
    unsigned check;
    memcpy(&check, origin + 2 * sizeof(Node) + offsetof(Node, left), sizeof(check));
    cout << check << ' ' << nodes[0].left << ' '
         << (reinterpret_cast<char*>(savedLeft) - origin) << ' '
         << (savedLeft == reinterpret_cast<unsigned*>(nodes + 2)) << ' '
         << (&saved == nodes + 2) << '\n';
    memset(origin + 2 * sizeof(Node) + offsetof(Node, total) + 1, 0x5a, 2);
    cout << saved.total << '\n';
    memmove(nodes + 1, nodes, 4 * sizeof(Node));
    cout << nodes[1].left << ' ' << nodes[3].total << ' ' << saved.total << '\n';
    Node copied = nodes[3];
    copied.left += 333;
    cout << copied.left << ' ' << nodes[3].left << '\n';
    nodes = static_cast<Node*>(realloc(nodes, 9 * sizeof(Node)));
    if (!nodes)
        exit(3);
    cout << nodes[1].left << ' ' << nodes[3].total << '\n';
    free(nodes);

    Node local{};
    auto* localMember = &local.total;
    *localMember = 0x1122334455667788LL;
    auto* localBytes = reinterpret_cast<unsigned char*>(&local);
    localBytes[offsetof(Node, total) + 3] = 0x77;
    cout << local.total << ' '
         << (reinterpret_cast<unsigned char*>(localMember) - localBytes) << '\n';
    memset(&local, 0, sizeof(local));
    cout << local.total << ' ' << *localMember << '\n';

    Node array[3]{};
    auto* firstAddress = &array[1].right;
    auto* arrayAddress = reinterpret_cast<unsigned char*>(array);
    array[1].right = 71;
    auto* laterAddress = &array[1].right;
    cout << (firstAddress == laterAddress) << ' '
         << (reinterpret_cast<unsigned char*>(firstAddress) - arrayAddress) << ' '
         << *firstAddress << '\n';

    Node padding[2]{};
    auto* earlier = reinterpret_cast<unsigned char*>(&padding[1]);
    memset(earlier, 0x5a, sizeof(Node));
    auto* allBytes = reinterpret_cast<unsigned char*>(padding);
    padding[1].left = static_cast<unsigned>(seed);
    memcpy(padding, padding + 1, sizeof(Node));
    cout << (memcmp(allBytes, allBytes + sizeof(Node), sizeof(Node)) == 0) << ' '
         << (earlier == allBytes + sizeof(Node)) << ' ' << padding[0].left << '\n';
}

void largeMove(int seed) {
    auto* nodes = static_cast<Node*>(calloc(1203, sizeof(Node)));
    if (!nodes)
        exit(2);
    for (int i = 0; i < 1100; ++i) {
        nodes[i].left = seed + i;
        nodes[i].total = (1LL << 54) + i * 1009LL - seed;
    }
    auto& retained = nodes[612];
    auto* field = &retained.total;
    memmove(nodes + 1, nodes, 1100 * sizeof(Node));
    cout << retained.left << ' ' << *field << ' ' << nodes[1100].total << ' '
         << (field == &nodes[612].total) << '\n';
    free(nodes);
}

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        int seed = x * 17 + y;
        check(seed);
        largeMove(seed);
    }
}
