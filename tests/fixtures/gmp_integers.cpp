#include <climits>
#include <gmp.h>
#include <iostream>
using namespace std;
int lookups;
mpz_ptr selected(mpz_t value) {
    ++lookups;
    return value;
}
void dump(const mpz_t value) {
    cout << mpz_sgn(value) << ':';
    mpz_t magnitude, divisor;
    mpz_init_set(magnitude, value);
    mpz_abs(magnitude, magnitude);
    mpz_init_set_ui(divisor, 1);
    mpz_mul_2exp(divisor, divisor, 64);
    while (mpz_cmp_ui(magnitude, 0) != 0) {
        cout << mpz_get_ui(magnitude) << ',';
        mpz_fdiv_q(magnitude, magnitude, divisor);
    }
    cout << '\n';
    mpz_clear(magnitude);
    mpz_clear(divisor);
}
int sign(int value) {
    return value < 0 ? -1 : value > 0;
}
int main() {
    int count;
    cin >> count;
    for (int i = 0; i < count; ++i) {
        long left, right;
        unsigned long extra;
        unsigned leftShift, rightShift;
        cin >> left >> right >> extra >> leftShift >> rightShift;
        mpz_t a, b, values[4];
        mpz_init_set_si(a, left);
        mpz_init_set_si(b, right);
        mpz_mul_2exp(a, a, leftShift);
        mpz_mul_2exp(b, b, rightShift);
        mpz_add_ui(a, a, extra);
        for (int j = 0; j < 4; ++j)
            mpz_init(values[j]);
        dump(a);
        dump(b);
        mpz_add(values[0], a, b);
        dump(values[0]);
        mpz_sub(values[0], a, b);
        dump(values[0]);
        mpz_mul(values[0], a, b);
        dump(values[0]);
        mpz_fdiv_qr(values[0], values[1], a, b);
        dump(values[0]);
        dump(values[1]);
        mpz_tdiv_qr(values[0], values[1], a, b);
        dump(values[0]);
        dump(values[1]);
        mpz_cdiv_qr(values[0], values[1], a, b);
        dump(values[0]);
        dump(values[1]);
        mpz_mod(values[0], a, b);
        dump(values[0]);
        mpz_set(values[0], a);
        mpz_set(values[1], b);
        mpz_fdiv_qr(values[0], values[1], values[0], values[1]);
        dump(values[0]);
        dump(values[1]);
        mpz_neg(values[0], a);
        dump(values[0]);
        cout << mpz_get_ui(values[0]) << '\n';
        mpz_and(values[0], a, b);
        dump(values[0]);
        mpz_ior(values[0], a, b);
        dump(values[0]);
        mpz_xor(values[0], a, b);
        dump(values[0]);
        mpz_com(values[0], a);
        dump(values[0]);
        mpz_mul_ui(values[0], a, extra);
        dump(values[0]);
        mpz_sub_ui(values[0], a, extra);
        dump(values[0]);
        mpz_ui_sub(values[0], extra, a);
        dump(values[0]);
        mpz_mul_si(values[0], a, -7);
        dump(values[0]);
        mpz_set(values[2], a);
        mpz_set(values[3], b);
        mpz_swap(values[2], values[3]);
        dump(values[2]);
        dump(values[3]);
        cout << sign(mpz_cmp(a, b)) << ' ' << sign(mpz_cmp_ui(a, extra)) << ' '
             << sign(mpz_cmp_si(a, -7)) << ' ' << sign(mpz_cmpabs(a, b)) << ' '
             << sign(mpz_cmpabs_ui(a, extra)) << '\n';
        unsigned long bits[] = {0, 1, 31, 63, 64, 65, 127, 255, 511, 2147483648UL, ULONG_MAX};
        for (auto bit : bits)
            cout << mpz_tstbit(a, bit);
        cout << '\n';
        lookups = 0;
        mpz_add(selected(values[2]), selected(values[2]), selected(values[3]));
        cout << lookups << '\n';
        dump(values[2]);
        lookups = 0;
        cout << mpz_sgn(selected(a)) << ' ' << lookups << '\n';
        for (int j = 0; j < 4; ++j)
            mpz_clear(values[j]);
        mpz_clear(a);
        mpz_clear(b);
    }
    mpz_t zero, destination;
    mpz_init(zero);
    mpz_init_set_ui(destination, 9);
    mpz_mul_2exp(destination, zero, ULONG_MAX);
    dump(destination);
    mpz_mul_2exp(zero, zero, ULONG_MAX);
    dump(zero);
    mpz_clear(zero);
    mpz_clear(destination);
}
