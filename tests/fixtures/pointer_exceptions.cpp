#include <iostream>
using namespace std;
void rethrow() {
    throw;
}
int increment(int value) {
    return value + 1;
}
int main() {
    int data[3] = {41, 73, 29};
    int* pointer = data;
    try {
        try {
            throw pointer;
        } catch (int*& selected) {
            int** address = &selected;
            ++selected;
            try {
                rethrow();
            } catch (int*& again) {
                cout << "same " << (&again == address) << ' ' << *again << '\n';
                ++again;
            }
            rethrow();
        }
    } catch (const int* const& selected) {
        cout << "pointer " << *selected << ' ' << *pointer << '\n';
    }
    try {
        throw pointer;
    } catch (void* selected) {
        auto cast = static_cast<int*>(selected);
        cout << "void " << *cast << '\n';
    }
    try {
        throw nullptr;
    } catch (int* selected) {
        cout << "null " << (selected == nullptr) << '\n';
    }
    try {
        throw 0;
    } catch (int*) {
        cout << "wrong null\n";
    } catch (int selected) {
        cout << "integer zero " << selected << '\n';
    }
    try {
        throw pointer;
    } catch (float*) {
        cout << "wrong type\n";
    } catch (const int* selected) {
        cout << "qualified " << *selected << '\n';
    }
    try {
        throw &pointer;
    } catch (const int* const* selected) {
        cout << "nested " << **selected << '\n';
    }
    try {
        throw &increment;
    } catch (int (*selected)(int)) {
        cout << selected(19) << '\n';
    }
}
