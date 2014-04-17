#include "Utilities.h"
#include <cassert>
#include <climits>

namespace utils {

// Greatest common divisor
int gcd(int a, int b) {
    assert(b != 0 && "Attempted modulo by zero.");
    int c;
    while(1) {
        c = a % b;
        if (c == 0) return b;
        a = b;
        b = c;
    }   
}

int gcdu(unsigned a, unsigned b) {
    assert(b != 0 && "Attempted modulo by zero.");
    unsigned c;
    while(1) {
        c = a % b;
        if (c == 0) return b;
        a = b;
        b = c;
    }   
}

unsigned gcdSafe(unsigned x, unsigned y) {
    if (y == 0) {
        return x == 0 ? 0 : 1;
    } else {
        return gcdu(x, y);
    }
}

int lcm(int a, int b) {
    if (a == 0 || b == 0) return 0;
    return a*b/gcd(a,b);
}

// Number of trailing zeros, e.g. if x is 1101000 (base 2), returns 3
int tlz(unsigned x) {
    if (x == 0) return CHAR_BIT * sizeof(x);
    int r = 0;
    x = (x ^ (x - 1)) >> 1; // Set x's trailing 0s to 1s and zero rest
    for (; x; r++) x >>= 1;
    return r;
}

int max(int x, int y) {
    return x ^ ((x ^ y) & -(x < y));
}

unsigned umin(unsigned x, unsigned y) {
    return x < y ? x : y;
}

unsigned umax(unsigned x, unsigned y) {
    return x > y ? x : y;
}

int min(int x, int y) {
    return y ^ ((x ^ y) & -(x < y));
}

} // End of the utils namespace
