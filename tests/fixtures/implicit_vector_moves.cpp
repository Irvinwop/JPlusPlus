#include <iostream>
#include <vector>
using namespace std;
struct Node {
    vector<int> path;
    int stamp;
};
struct Envelope {
    Node child;
};
Envelope nodes[4];
void show(const vector<int>& values) {
    for (int value : values)
        cout << value << ',';
    cout << '\n';
}
int main() {
    Node value{{1, 3, 5}, 8};
    vector<int>& outer = value.path;
    value = Node{{7, 9}, 10};
    show(outer);
    cout << value.stamp << ' ' << (&outer == &value.path) << '\n';
    vector<int>& nested = nodes[2].child.path;
    for (int i = 0; i < 16; ++i) {
        nodes[2] = Envelope{Node{{i + 10, i + 20, i + 30}, i + 40}};
        nested.push_back(i + 50);
        show(nodes[2].child.path);
        cout << nodes[2].child.stamp << ' ' << (&nested == &nodes[2].child.path) << '\n';
    }
}
