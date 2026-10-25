#include <bits/stdc++.h>
using namespace std;

int initialized, constructed, copied, moved, assigned;
struct Record {
    int value = ++initialized;
    int data[2]{value, value + 1};
};
struct Owned {
    int value;
    Owned(int number) : value(number) {
        ++constructed;
    }
    Owned(const Owned& source) : value(source.value + 1) {
        ++copied;
    }
    Owned(Owned&& source) : value(source.value) {
        source.value = -1;
        ++moved;
    }
    Owned& operator=(const Owned& source) {
        value = source.value + 2;
        ++assigned;
        return *this;
    }
};
struct Envelope {
    Owned item;
    Envelope(int value) : item(value) {}
};
void send(vector<int>& output, int value) {
    return output.push_back(value);
}

int main() {
    int cases;
    cin >> cases;
    while (cases--) {
        int x, y;
        cin >> x >> y;
        Record first;
        Record copy = first;
        Record source{};
        Record& alias = first;
        int& member = first.value;
        int* address = &first.value;
        int* element = &first.data[1];
        source.value += x;
        source.data[1] += y;
        first = source;
        cout << initialized << ' ' << alias.value << ' ' << member << ' ' << *address << ' '
             << *element << ' ' << copy.value << '\n';
        alias.value += y;
        cout << first.value << ' ' << source.value << '\n';
        vector<Record> records;
        records.push_back({copy});
        records[0].value += x;
        cout << records[0].value << ' ' << copy.value << ' ' << initialized << '\n';
        Owned original(x);
        Owned duplicate = original;
        Owned transferred = std::move(duplicate);
        cout << original.value << ' ' << duplicate.value << ' ' << transferred.value << ' '
             << constructed << ' ' << copied << ' ' << moved << '\n';
        Owned destination(y);
        int& saved = destination.value;
        destination = original;
        cout << destination.value << ' ' << saved << ' ' << assigned << '\n';
        vector<int> values{x};
        vector<int>& valuesAlias = values;
        vector<int> replacement{y, x - y};
        values = replacement;
        send(valuesAlias, x + y);
        cout << values.size() << ' ' << values[0] << ' ' << values[2] << '\n';
        Record recordsArray[2];
        Record& selected = recordsArray[1];
        int* selectedMember = &selected.value;
        recordsArray[1] = source;
        cout << selected.value << ' ' << *selectedMember << ' ' << initialized << '\n';
        Envelope envelope(x);
        envelope = envelope;
        cout << envelope.item.value << ' ' << assigned << '\n';
        struct Node {
            Node* self;
            int value;
        };
        Node self{&self, x + y};
        cout << self.self->value << ' ' << (self.self == &self) << '\n';
    }
}
