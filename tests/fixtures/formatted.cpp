#include <bits/stdc++.h>
using namespace std;

int main() {
    int n;
    if (scanf("%d", &n) != 1)
        return 1;
    for (int i = 0; i < n; ++i) {
        int a, consumed;
        unsigned b;
        long long c;
        double d;
        char word[8], mark, letters[8];
        int assigned = scanf(" %d,%u %lld %lf %3s %c %3[a-z]%n", &a, &b, &c, &d,
                             word, &mark, letters, &consumed);
        printf("%d %d %u %lld %.2f [%s] %c %s %d\n", assigned, a, b, c, d, word,
               mark, letters, consumed);
        int written;
        int count = printf("[%+08d][%#012x][%.0u][%*.*s]%n", a, b, 0u,
                           i % 2 ? -8 : 8, 2, word, &written);
        cout << ' ' << count << ' ' << written << ' ' << word << '\n';
    }
    int automatic, consumed;
    unsigned hexadecimal, octal;
    int assigned = scanf("%i %x %o %*d%n", &automatic, &hexadecimal, &octal, &consumed);
    cout << assigned << ' ' << automatic << ' ' << hexadecimal << ' ' << octal << ' '
         << consumed << '\n';
    int unchanged = 193;
    cout << scanf("%d", &unchanged) << ' ' << unchanged << ' ' << getchar() << '\n';
    cout << scanf("%d", &unchanged) << ' ' << unchanged << '\n';
    char greeting[12] = "ready";
    greeting[2] = '\0';
    printf("%s\n", greeting);
    cout << greeting << '\n';
}
