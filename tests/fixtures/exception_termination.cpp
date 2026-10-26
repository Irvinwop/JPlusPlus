#include <iostream>
using namespace std;
void fail(int value) {
    throw value;
}
void boundary(int value) noexcept {
    fail(value);
}
void rethrow() {
    throw;
}
struct Bomb {
    ~Bomb() {
        fail(32);
    }
};
struct Construction {
    Construction(int value) noexcept {
        fail(value);
    }
};
int main() {
    int mode;
    cin >> mode;
    cout << "before " << mode << endl;
    if (mode == 0)
        throw 13;
    try {
        if (mode == 1)
            boundary(29);
        if (mode == 2) {
            auto call = []() noexcept { fail(41); };
            call();
        }
        if (mode == 3)
            rethrow();
        if (mode == 4) {
            Bomb bomb;
        }
        if (mode == 5) {
            Bomb bomb;
            fail(53);
        }
        if (mode == 6) {
            Construction object(67);
        }
        if (mode == 7)
            fail(79);
    } catch (...) {
        cout << "caught " << mode << endl;
    }
    cout << "after " << mode << endl;
}
