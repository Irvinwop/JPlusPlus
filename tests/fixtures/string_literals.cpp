#include <chrono>
#include <iostream>
#include <string>
#include <vector>
using namespace std;
using namespace std::literals;
namespace own {
int calls;
string operator""_text(const char* source, size_t length) {
    ++calls;
    string result(source, length);
    result[0] = '#';
    return result;
}
} // namespace own
using own::operator""_text;
int main() {
    int count;
    cin >> count;
    for (int i = 0; i < count; ++i) {
        int value;
        cin >> value;
        auto text = "A\0Bé\xff"s;
        auto copy = text;
        copy[0] = static_cast<char>(value);
        vector<string> values{text, ""s, "x\0y"s};
        text[2] = 'Z';
        auto custom = "abc\0xyz"_text;
        cout << text.size() << ':' << copy.size() << ':' << values[1].size() << ':'
             << values[2].size() << ':' << custom.size() << ':' << own::calls << '\n';
        cout << text << '|' << copy << '|' << values[0] << '|' << values[2] << '|' << custom
             << '\n';
        auto period = 2s + 17ms;
        cout << chrono::duration_cast<chrono::milliseconds>(period).count() << '\n';
    }
}
