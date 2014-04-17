#include "Rand.h"
#include "CompilerPortability.h"
#include <tr1/random>
#include <climits>

namespace utils {
namespace rnd {

namespace {

typedef std::tr1::variate_generator<
    std::tr1::mt19937,
    std::tr1::uniform_int<unsigned> > Int32RGenTy;

// Note that both of these functions are thread-safe
static THREADLOCAL Int32RGenTy     *Rnd = 0;

} // End of anonymous namespace

void init(unsigned seed) {
    std::tr1::mt19937 gen;
    gen.seed(seed);
    Rnd = new Int32RGenTy(gen,
        std::tr1::uniform_int<unsigned>(0,UINT_MAX));
}

int irand() {
    return sizeof(int) == 4 ? i32rand() : i64rand();
}

int32_t i32rand() {
    if (Rnd == 0) {
        init();
    }
    return (*Rnd)();
}

int64_t i64rand() {
    if (Rnd == 0) {
        init();
    }
    return (static_cast<int64_t>((*Rnd)()) << 32) + (*Rnd)();
}

double drand_closed() {
    if (Rnd == 0) {
        init();
    }
    return static_cast<double>((*Rnd)()) * (1. / 4294967295.);
}

double drand_open() {
    if (Rnd == 0) {
        init();
    }
    return (static_cast<double>((*Rnd)()) + .5) *
        (1. / 4294967296.);
}

// Generates 53-bit resolution doubles in the half-open interval [0,1)
double drand_hopen() {
    if (Rnd == 0) {
        init();
    }
    return (static_cast<double>((*Rnd)() >> 5) * 67108864. +
        static_cast<double>((*Rnd)() >> 6)) *
        (1. / 9007199254740992.);
}

void shutdown() {
    delete Rnd;
    Rnd = 0;
}

} // End of the rnd namespace
} // End of utils namespace
