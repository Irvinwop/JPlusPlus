#include <cstdlib>
void seed_rng(unsigned seed) {
    srandom(seed);
}
long read_rng() {
    return random();
}
