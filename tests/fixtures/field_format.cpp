#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
using namespace std;
namespace own {
int calls;
int setw(int value) {
    ++calls;
    return value + 100;
}
int setprecision(int value) {
    ++calls;
    return value + 200;
}
} // namespace own
int main() {
    cout << setfill('_') << internal << hexfloat;
    cout << '[' << setw(18) << -1.5 << "]\n";
    cout << '[' << setw(18) << 1.5 << "]\n";
    cout << '[' << setw(18) << "-0x1.8p+0" << "]\n";
    cout << defaultfloat;
    cout << '[' << setw(12) << -42 << "]\n";
    cout << '[' << setw(12) << '-' << "]\n";
    cout << left << '[' << setw(8) << -23 << "]\n";
    cout << right << '[' << setw(8) << -23 << "]\n";
    auto oldWidth = cout.width(12), width = cout.width();
    cout.width(0);
    auto oldFill = cout.fill('0'), fill = cout.fill();
    cout << oldWidth << ':' << width << ':' << oldFill << ':' << fill << '\n';
    cout.width(7);
    cout << endl;
    cout << 42 << '|' << cout.width() << '\n';
    cout << setw(6) << flush << 3 << '\n';
    cout.width(6);
    printf("C");
    cout << 7 << '|' << cout.width() << '\n';
    cout.width(4);
    puts("S");
    cout << 8 << '|' << cout.width() << '\n';
    cout.width(5);
    fwrite("abc", 1, 3, stdout);
    cout << 9 << '|' << cout.width() << '\n';
    cout.width(4);
    putchar('P');
    cout << 6 << '|' << cout.width() << '\n';
    cout.width(5);
    cout.put('Q');
    cout << 4 << '|' << cout.width() << '\n';
    cout.width(5);
    cout.write("x\0y", 3);
    cout << 1 << '|' << cout.width() << '\n';
    cout << setw(-3) << 7 << setw(3) << 'A' << '\n';
    cout.width(numeric_limits<streamsize>::min());
    cout << 'X' << '|' << cout.width() << '\n';
    auto previous = cout.width(numeric_limits<streamsize>::max());
    auto largest = cout.width();
    cout.width(0);
    cout << previous << ':' << largest << '\n';
    int right = 77, endl = 19, fixed = 23;
    cout << right << ':' << endl << ':' << fixed << '\n';
    cout << own::setw(3) << ':' << own::setprecision(4) << ':' << own::calls << '\n';
    cout << std::fixed << setprecision(2) << internal << setfill('.') << '[' << setw(10) << -0.0
         << "]\n";
    cout << std::right << std::setfill('_');
    cout.width(5);
    int octet = putchar(-1);
    cout << octet << '|' << cout.width() << '\n';
    cout.width(4);
    int narrowed = putchar(0x141);
    cout << narrowed << '|' << cout.width() << '\n';
    cout << std::setw(5) << "é" << '|' << std::setw(4) << '\xff' << '\n';
    ofstream bad("jpp-format-missing-dir/output.txt");
    bad.width(7);
    bad.fill('#');
    bad << "ignored";
    cout << bad.width() << ':' << bad.fail() << ':' << bad.fill() << '\n';
}
