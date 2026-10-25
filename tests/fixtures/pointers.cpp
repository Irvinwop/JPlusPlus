#include <bits/stdc++.h>
using namespace std;

int globalLeft, globalRight;
void rebase(int*& at, int shift) {
    at += shift;
}
bool sameAddress(int& value, int* expected) {
    return &value == expected;
}
int* following(int& value, int count) {
    return &value + count;
}
long long change(int* values, int n, int increment) {
    long long result = 0;
    for (int* at = values; at < values + n; ++at) {
        *at += increment;
        result += *at;
    }
    return result;
}
struct Node {
    Node* next;
    int value;
    static int twice(int value) {
        return value * 2;
    }
    int add(int increment) {
        value += increment;
        return value;
    }
    Node* self() {
        return this;
    }
};

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        int (*operation)(int) = &Node::twice;
        cout << operation(x + y) << '\n';
        cout << (operation == &Node::twice) << ' ' << (*operation)(x - y) << ' '
             << ((&*operation) == operation) << '\n';
        int data[8]{x, y, x - y, y - x, 2 * x, 2 * y, 7, 11};
        int* at = data + 1;
        int* alias = at;
        *at++ = x + y;
        rebase(at, 2);
        int*& reference = at;
        --reference;
        int position = y & 1;
        int old = alias[position++]++;
        int incremented = ++*at;
        cout << old << ' ' << position << ' ' << incremented << '\n';
        cout << (at - data) << ' ' << (alias - data) << ' ' << *alias << ' ' << *at << '\n';
        cout << change(data + 2, 4, y) << ' ' << (at == data + 2) << ' ' << (alias != nullptr)
             << ' ' << (at - alias) << '\n';
        int another[2]{};
        int* separate = another;
        cout << (data == &data[0]) << ' ' << (data == separate) << ' '
             << (&globalLeft == &globalLeft) << ' ' << (&globalLeft == &globalRight) << '\n';
        int& element = data[2];
        cout << sameAddress(element, data + 2) << ' ' << *following(element, 3) << ' '
             << sameAddress(globalLeft, &globalLeft) << '\n';
        int* bound = lower_bound(data + 6, data + 8, abs(y));
        cout << (bound - data) << '\n';
        int local = x;
        int* localPointer = &local;
        *localPointer += y;
        cout << local << ' ' << (&local == localPointer) << '\n';
        Node first{nullptr, x}, second{&first, y};
        Node* head = &second;
        cout << head->add(x) << ' ' << head->next->value << ' ' << (head->self() == head) << '\n';
        int* field = &head->value;
        *field += y;
        cout << second.value << ' ' << (field == &second.value) << ' ' << (field != &first.value)
             << '\n';
        cout << sameAddress(second.value, field) << '\n';
        Node* allocated = new Node{head, x - y};
        allocated->value += allocated->next->value;
        cout << allocated->value << '\n';
        delete allocated;
        int* heap = new int[6]{x, y, 3};
        cout << change(heap, 6, y) << ' ' << heap[5] << '\n';
        int* one = new int(x + y);
        *one *= 2;
        cout << *one << '\n';
        delete one;
        delete[] heap;
        int rows[2][3]{{x, y, 1}, {2, 3, 4}};
        int(*row)[3] = rows + 1;
        (*row)[y & 1] += x;
        cout << rows[1][0] << ' ' << rows[1][1] << ' ' << (row - rows) << '\n';
        void* opaque = data;
        int* restored = static_cast<int*>(opaque);
        cout << restored[1] << ' ' << (restored == data) << '\n';
        const char* literal = "pointers";
        cout << (literal + 2) << ' ' << literal[0] << ' ' << ((literal + 4) - literal) << '\n';
        string word = "mutable";
        const char* characters = word.c_str();
        word[0] = 'M';
        cout << (word == "Mutable") << ' ' << ("Mutable" == word) << ' ' << (word != "mutable")
             << ' ' << ("mutable" != word) << '\n';
        cout << characters << ' ' << (characters == word.c_str()) << ' ' << (characters == &word[0])
             << '\n';
        vector<int> sequence{x, y, x - y};
        int* contiguous = sequence.data();
        contiguous[1] += x;
        cout << sequence[1] << ' ' << (contiguous == &sequence[0]) << '\n';
        struct Local {
            Local* previous;
            int value;
        };
        Local last{nullptr, x};
        Local* next = new Local{&last, y};
        cout << next->value + next->previous->value << '\n';
        delete next;
        map<int, int> mapping;
        int& entry = mapping[x + 1000];
        int* mapped = &entry;
        *mapped += y;
        cout << mapping[x + 1000] << ' ' << sameAddress(mapping[x + 1000], mapped) << '\n';
    }
}
