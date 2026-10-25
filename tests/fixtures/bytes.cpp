#include <bits/stdc++.h>
using namespace std;

void dump(const string& value) {
    cout << value.size() << ':';
    for (unsigned char byte : value)
        cout << ' ' << unsigned(byte);
    cout << '\n' << value << '\n';
}

int main() {
    string literal = "café 日本語 ☃";
    char payload[]{0, 'a', (char)0xff, 0, 'b', (char)0x80};
    string binary(payload, sizeof payload);
    string range(payload, payload + sizeof payload);
    string list{'a', 0, (char)0xff, 'b'};
    dump(literal);
    dump(binary);
    dump(range);
    dump(list);
    dump(string("left\0right"));
    dump(string("left\0right", 10));
    string modified;
    modified.append(3, (char)0xff).append(payload, sizeof payload);
    modified.append(literal, 1, string::npos);
    dump(modified);
    modified.assign(modified.data() + 1, modified.size() - 2);
    dump(modified);
    modified.append(modified.begin(), modified.end());
    dump(modified);
    modified.assign({'x', 0, (char)0x80});
    dump(modified);
    modified.append({0, (char)0xff});
    dump(modified);
    modified.assign("stop\0hidden");
    dump(modified);
    cout << "nul\0suffix" << '\n';
    string joined = literal + binary;
    auto copy = joined;
    copy.back() ^= 31;
    dump(joined);
    dump(copy);
    dump(string(joined, 1, 1ULL << 32));
    dump(joined.substr(1, string::npos));
    cout << joined.find(string(payload + 1, 3)) << ' ' << joined.find("", 1ULL << 32) << ' '
         << joined.find(payload + 1, 0, 3) << '\n';
    int count = -1;
    int written = printf("<%8.5s>%n\n", literal.c_str(), &count);
    cout << count << ' ' << written << '\n';
    literal.reserve(200);
    cout << (literal.capacity() >= 200) << '\n';
    literal.resize(literal.size() + 3, (char)0xff);
    dump(literal);
    literal.resize(3);
    dump(literal);
    string repeated(4, (char)0xe9);
    dump(repeated);

    int cases;
    cin >> cases;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    string line;
    const string& retained = line;
    while (cases--) {
        getline(cin, line);
        dump(retained);
    }
    cin >> line;
    dump(retained);
    char scanned[16]{};
    int consumed = -1;
    cout << scanf(" %15s%n", scanned, &consumed) << ' ' << consumed << '\n';
    dump(string(scanned));
    cout << printf("é:%s:%c\n", scanned, 0xff) << '\n';
    const char path[] = "jpp-byte-é.txt\0ignored";
    string filename(path, sizeof(path) - 1);
    {
        ofstream output(filename, ios::binary);
        output << joined << '\n' << binary << '\n';
        cout << output.is_open() << '\n';
    }
    {
        ifstream input(filename, ios::binary);
        getline(input, line);
        dump(line);
        getline(input, line);
        dump(line);
    }
}
