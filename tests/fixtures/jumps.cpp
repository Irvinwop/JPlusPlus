#include <bits/stdc++.h>
using namespace std;

int crossing(int value) {
    if (value < 0)
        goto first;
    int saved;
    saved = value * value;
    if (value % 2)
        goto second;
first:
    saved = abs(value) + 1;
    if (value % 3)
        goto third;
second:
    saved += 4;
third:
    return saved;
}

int nested(int value) {
    int sum = 0;
    for (int i = 0; i < 7; ++i) {
        for (int j = 0; j < 8; ++j) {
            if ((i + j + value) % 11 == 0)
                goto done;
            if ((i + j) % 3 == 0)
                continue;
            if (j == 6)
                break;
            sum += i * 8 + j;
        }
    }
done:
    return sum;
}

int early(int value) {
    {
        ofstream output("jpp-jump.txt");
        output << value * 3 << '\n';
        if (value % 2)
            goto readBack;
        output << value + 1 << '\n';
    }
readBack:
    ifstream input("jpp-jump.txt");
    int sum = 0, item;
    while (input >> item)
        sum += item;
    return sum;
}

int dead(int value) {
    goto result;
    cout << "unreachable";
result:
    return value + 9;
}

int main() {
    int n;
    cin >> n;
    while (n--) {
        int value;
        cin >> value;
        int choice;
        switch (value % 3) {
        case 0:
            choice = value + 1;
            break;
        case 1:
            choice = value - 1;
            break;
        default:
            choice = value * 2;
        }
        cout << crossing(value) << ' ' << nested(value) << ' ' << early(value) << ' '
             << dead(value) << ' ' << choice << '\n';
    }
}
