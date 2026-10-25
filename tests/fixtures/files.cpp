#include <bits/stdc++.h>
using namespace std;

int readHeader() {
    int value;
    cin >> value;
    return value;
}

int header = readHeader();
ofstream globalOutput("jpp-io-global.txt");

long long writeValues(int n) {
    ofstream output("jpp-io-values.txt");
    long long sum = 0;
    for (int i = 0; i < n; ++i) {
        long long value;
        cin >> value;
        output << value << '\n';
        sum += value;
    }
    return sum;
}

long long readValues(istream& input, int n) {
    long long sum = 0;
    for (int i = 0; i < n; ++i) {
        long long value;
        input >> value;
        sum += value * (i + 1);
    }
    return sum;
}

int main() {
    cout << writeValues(header) << '\n';
    ifstream source;
    cout << source.is_open() << ' ' << source.good() << '\n';
    source.open("jpp-io-values.txt");
    cout << source.is_open() << ' ' << readValues(source, header) << '\n';
    source.close();
    cout << source.is_open() << '\n';
    source.open("jpp-io-values.txt");
    long long value, repeated = 0;
    while (source >> value)
        repeated += value;
    cout << repeated << ' ' << source.eof() << ' ' << source.fail() << '\n';
    {
        ofstream cout("jpp-io-words.txt");
        cout << "first line\nsecond line\n";
    }
    {
        ofstream appended;
        appended.open("jpp-io-words.txt", ios::app);
        appended << header << '\n';
    }
    {
        ifstream cin("jpp-io-words.txt");
        string line;
        getline(cin, line);
        cout << line << '\n';
        cin.ignore(1000, '\n');
        int value;
        cin >> value;
        globalOutput << value * 7LL << '\n';
    }
    ifstream missing("jpp-does-not-exist.txt");
    cout << bool(missing) << ' ' << missing.fail() << '\n';
    missing.clear();
    missing.open("jpp-io-values.txt");
    cout << bool(missing) << ' ' << missing.is_open() << '\n';
}
