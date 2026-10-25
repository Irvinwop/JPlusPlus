#include <bits/stdc++.h>
using namespace std;

int calls;
istream& nextStream() {
    ++calls;
    return cin;
}

int main() {
    int values[4]{11, 22, 33, 44}, index = 0;
    nextStream() >> values[index++] >> values[index++];
    cin >> values[index++];
    cout << index << ' ' << calls << ' ' << values[0] << ' ' << values[1] << ' ' << values[2]
         << '\n';
    int n;
    cin >> n;
    while (n--) {
        int kind;
        cin >> kind;
        if (kind == 0) {
            int value = 193;
            cin >> value;
            cout << value;
        } else if (kind == 1) {
            long long value = 193;
            cin >> value;
            cout << value;
        } else if (kind == 2) {
            unsigned value = 193;
            cin >> value;
            cout << value;
        } else if (kind == 3) {
            unsigned long long value = 193;
            cin >> value;
            cout << value;
        } else if (kind == 4) {
            short value = 193;
            cin >> value;
            cout << value;
        } else if (kind == 5) {
            unsigned short value = 193;
            cin >> value;
            cout << value;
        } else if (kind == 6) {
            double value = 193;
            cin >> value;
            cout << value;
        } else if (kind == 7) {
            float value = 193;
            cin >> value;
            cout << value;
        } else {
            bool value = false;
            cin >> value;
            cout << value;
        }
        cout << ' ' << cin.good() << ' ' << cin.fail() << ' ';
        cin.clear();
        char trailing;
        cin >> trailing;
        cout << trailing << '\n';
        cin.ignore(1000, '\n');
    }
    string line = "initial";
    string& alias = line;
    while (getline(nextStream(), line)) {
        cout << line.size() << ':' << line << '\n';
        cout << alias.size() << ':' << alias << '\n';
    }
    cout << '[' << line << "] " << calls << ' ' << cin.eof() << ' ' << cin.fail() << '\n';
    cout << '[' << alias << "]\n";
    int unchanged = 289;
    char letter = '?';
    cin >> unchanged >> letter;
    cout << unchanged << ' ' << letter << '\n';
}
