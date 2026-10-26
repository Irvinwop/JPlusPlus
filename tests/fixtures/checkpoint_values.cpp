#include <climits>
#include <csetjmp>
#include <iostream>
using namespace std;
jmp_buf first, second, third;
volatile int phase = 0;
[[noreturn]] void jump(jmp_buf destination, int value) {
    longjmp(destination, value);
}
int main() {
    int limit;
    cin >> limit;
    if (setjmp(first) == 0) {
        cout << "initial\n";
        jump(first, 0);
    } else {
        cout << "resumed " << phase << '\n';
        if (phase < limit) {
            phase = phase + 1;
            jump(first, -17);
        }
    }
    if (-17 == setjmp(second))
        cout << "negative\n";
    else
        jump(second, -17);
    switch (setjmp(third)) {
    case 0:
        cout << "zero\n";
        jump(third, INT_MIN);
    case INT_MIN:
        cout << "minimum\n";
        jump(third, 0);
    case 1:
        cout << "one\n";
        break;
    default:
        cout << "unexpected\n";
    }
    jmp_buf unsignedValue;
    if (setjmp(unsignedValue) == UINT_MAX)
        cout << "unsigned\n";
    else
        jump(unsignedValue, -1);
    jmp_buf direct;
    if (setjmp(direct))
        cout << "direct\n";
    else
        jump(direct, 6);
    jmp_buf negated;
    if (!setjmp(negated))
        jump(negated, 0);
    else
        cout << "negated\n";
    jmp_buf compared;
    if (6 > setjmp(compared))
        jump(compared, 7);
    else
        cout << "compared\n";
    jmp_buf table[2];
    volatile int selected = 0;
    if (setjmp(table[selected++]) == 0)
        jump(table[0], 15);
    else
        cout << "selected " << selected << '\n';
    volatile int innerPhase = 0;
    jmp_buf nested;
    if (setjmp(first)) {
        cout << "outer branch\n";
    } else {
        cout << "outer initial\n";
        if (setjmp(nested) == 0)
            jump(nested, 19);
        else {
            cout << "inner branch " << innerPhase << '\n';
            innerPhase = innerPhase + 1;
            if (innerPhase < limit)
                jump(nested, -1);
            jump(first, -2);
        }
    }
    cout << "done\n";
}
