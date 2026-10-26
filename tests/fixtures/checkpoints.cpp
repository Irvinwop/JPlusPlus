#include <csetjmp>
#include <iostream>
using namespace std;
jmp_buf outer, inner;
volatile int roundNumber = 0, innerNumber = 0;

void recurse(jmp_buf destination, int depth, int value) {
    if (depth)
        recurse(destination, depth - 1, value);
    else
        longjmp(destination, value);
}
int returnFromCheckpoint(int limit) {
    jmp_buf saved;
    volatile int count = 0;
    setjmp(saved);
    if (count < limit) {
        count = count + 1;
        recurse(saved, 4, count);
    }
    return count + 19;
}
void finalCheckpoint() {
    jmp_buf saved;
    setjmp(saved);
}
int main() {
    int limit;
    cin >> limit;
    setjmp(outer);
    cout << "outer " << roundNumber << '\n';
    if (roundNumber < limit) {
        roundNumber = roundNumber + 1;
        innerNumber = 0;
        setjmp(inner);
        cout << "inner " << innerNumber << '\n';
        if (innerNumber < 3) {
            innerNumber = innerNumber + 1;
            recurse(inner, 7, 0);
        }
        recurse(outer, 11, -17);
    }
    jmp_buf buffers[2];
    volatile int index = 0, repeats = 0;
    setjmp(buffers[index++]);
    int initializedAgain = 100 + repeats;
    cout << index << ' ' << initializedAgain << '\n';
    if (repeats < limit) {
        repeats = repeats + 1;
        recurse(buffers[0], 5, 23);
    }
    for (int iteration = 0; iteration < 4; ++iteration) {
        jmp_buf loop;
        volatile int trips = 0;
        setjmp(loop);
        for (int i = 0; i < 6; ++i) {
            if (i == 1)
                continue;
            if (i == 3)
                break;
            switch (i) {
            case 0:
                cout << 'A';
                break;
            default:
                cout << 'B';
                break;
            }
        }
        if (trips < iteration) {
            trips = trips + 1;
            recurse(loop, 3, 1);
        }
        cout << ':' << iteration << '\n';
    }
    cout << returnFromCheckpoint(limit) << '\n';
    finalCheckpoint();
}
