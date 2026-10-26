#include <csetjmp>
#include <iostream>
using namespace std;
struct Token {
    int code;
    long long weight;
};
struct Guard {
    int id;
    ~Guard() {
        cout << "destroy " << id << '\n';
    }
};
struct ThreadLocal {
    int id;
};
enum Code { orange = 17, blue = 31 };
void firstLocal(int value) {
    struct Error {
        int code;
    };
    try {
        throw Error{value};
    } catch (Error& error) {
        ++error.code;
        cout << "first " << error.code << '\n';
        throw;
    }
}
void otherLocal(int value) {
    struct Error {
        int code;
    };
    try {
        firstLocal(value);
    } catch (Error& error) {
        cout << "wrong local " << error.code << '\n';
    } catch (...) {
        cout << "distinct local\n";
    }
}
void firstEnum() {
    enum Local { selected = 37 };
    try {
        throw selected;
    } catch (Local& value) {
        cout << "enum " << static_cast<int>(value) << '\n';
        throw;
    }
}
void otherEnum() {
    enum Local { selected = 41 };
    try {
        firstEnum();
    } catch (Local& value) {
        cout << "wrong enum " << static_cast<int>(value) << '\n';
    } catch (...) {
        cout << "distinct enum\n";
    }
}
int calls;
void both(bool select) {
    select ? throw ++calls : throw calls += 3;
    cout << "unreachable conditional\n";
}
int commaValue() {
    return (throw ++calls, 99);
}
void commaStatement() {
    (throw ++calls, cout << "unreachable comma\n");
}
void castThrow() {
    static_cast<void>(throw ++calls);
    cout << "unreachable cast\n";
}
void rethrow() {
    throw;
}
void descend(int depth, int value) {
    Guard guard{depth};
    if (depth)
        descend(depth - 1, value);
    else
        throw value;
}
int choose(int value) {
    return value < 0 ? throw value * 3 : value + 1;
}
int functionTry(int value) try {
    if (value % 2)
        throw value;
    return value * 11;
} catch (int error) {
    return error * 7;
}
void select(int kind, int value) {
    try {
        Guard guard{100 + kind};
        if (kind == 0)
            throw static_cast<short>(value);
        if (kind == 1)
            throw static_cast<unsigned>(value);
        if (kind == 2)
            throw static_cast<long long>(value) * 1000000007;
        if (kind == 3)
            throw static_cast<float>(value) + 0.5f;
        if (kind == 4)
            throw Token{value, static_cast<long long>(value) * 1000000007};
        if (kind == 5)
            throw "hello";
        if (kind == 6)
            throw orange;
        if (kind == 7)
            throw static_cast<double>(value) + 0.25;
        if (kind == 8)
            throw static_cast<long double>(value) + 0.5L;
        throw static_cast<char>('Q');
    } catch (int) {
        cout << "wrong promotion\n";
    } catch (short error) {
        cout << "short " << error << '\n';
    } catch (unsigned& error) {
        unsigned* address = &error;
        error += 9;
        auto alter = [&]() { error += 5; };
        alter();
        try {
            rethrow();
        } catch (unsigned copy) {
            copy += 20;
            cout << "copy " << copy << '\n';
        }
        cout << "original " << error << '\n';
        try {
            rethrow();
        } catch (const unsigned& again) {
            cout << "again " << again << ' ' << (&again == address) << '\n';
        }
    } catch (const long long& error) {
        cout << "wide " << error << '\n';
    } catch (float error) {
        cout << "float " << error << '\n';
    } catch (Token& error) {
        error.code += 3;
        try {
            throw;
        } catch (Token copy) {
            copy.code += 100;
            cout << copy.code << '\n';
        }
        cout << "token " << error.code << ' ' << error.weight << '\n';
    } catch (const char* text) {
        cout << "text " << text << '\n';
    } catch (Code& code) {
        code = blue;
        try {
            rethrow();
        } catch (Code copy) {
            cout << "code " << static_cast<int>(copy) << '\n';
        }
    } catch (double error) {
        cout << "double " << error << '\n';
    } catch (long double& error) {
        error += 2.0L;
        try {
            rethrow();
        } catch (long double copy) {
            cout << "extended " << static_cast<long long>(copy * 2) << '\n';
        }
    } catch (...) {
        cout << "fallback\n";
    }
}
int main() {
    int count;
    cin >> count;
    for (int i = 0; i < count; ++i) {
        int value;
        cin >> value;
        select(i % 10, value);
        cout << functionTry(value) << '\n';
        try {
            cout << choose(value) << '\n';
        } catch (int error) {
            cout << "choose " << error << '\n';
        }
    }
    try {
        descend(4, 919);
    } catch (int& error) {
        cout << "deep " << error << '\n';
    }
    try {
        throw 39;
    } catch (int& outer) {
        try {
            throw 6L;
        } catch (long& inner) {
            inner += 2;
            cout << "inner " << inner << '\n';
        }
        outer += 2;
        try {
            rethrow();
        } catch (int value) {
            cout << "outer " << value << '\n';
        }
    }
    for (int i = 0; i < 5; ++i) {
        try {
            throw i;
        } catch (int value) {
            if (value == 1)
                continue;
            if (value == 3)
                break;
            cout << "loop " << value << '\n';
        }
    }
    try {
        throw ThreadLocal{67};
    } catch (ThreadLocal object) {
        cout << object.id << '\n';
    }
    for (int mode = 0; mode < 5; ++mode) {
        try {
            if (mode < 2)
                both(mode);
            if (mode == 2)
                cout << commaValue() << '\n';
            if (mode == 3)
                commaStatement();
            if (mode == 4)
                castThrow();
        } catch (int value) {
            cout << mode << ':' << value << ':' << calls << '\n';
        }
    }
    otherLocal(43);
    otherEnum();
    jmp_buf checkpoint;
    if (setjmp(checkpoint) == 0) {
        try {
            longjmp(checkpoint, 7);
        } catch (...) {
            cout << "wrong jump\n";
        }
    } else {
        cout << "jump resumed\n";
    }
}
