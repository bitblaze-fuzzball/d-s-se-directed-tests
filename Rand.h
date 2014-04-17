#ifndef UTILS_RAND_H
#define UTILS_RAND_H

#include <tr1/cstdint>

namespace utils {
namespace rnd {

int             irand();
int32_t         i32rand();
int64_t         i64rand();
double          drand_closed(); // In closed    interval [0,1]
double          drand_open();   // In open      interval (0,1)
double          drand_hopen();  // In half-open interval [0,1)

// Call to set the seed
void init(unsigned seed = 0x8944C407); // Arbitrary number
// Call at the end of the program
void shutdown();

} // End of the rnd namespace
} // End of the utils namespace

#endif // UTILS_RAND_H
