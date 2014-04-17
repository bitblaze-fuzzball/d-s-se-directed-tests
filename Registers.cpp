#include "Registers.h"
#include <cstring>

namespace absdomain {

const struct regs* regLookup(const char* regName) {
    int low = 0;
    int mid;
    int high = sizeof(regs) / sizeof(regs[0]) - 1;
    while (low <= high) {
        int c;
        mid = low + (high - low) / 2;
        c = strcasecmp(regs[mid].name, regName);
        if (c == 0) {
            return &regs[mid];
        } else if (c < 0) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }    
    }   
    return NULL;
}

const char* getRegNameAtAddress(int lo, int hi) {
    const int limit = sizeof(regs) / sizeof(regs[0]) - 1;
    for (int i = 0; i <= limit; i++) {
        if (regs[i].begin == lo && regs[i].end == hi) {
            return regs[i].name;
        }
    }
    return 0;
}

} // End of the absdomain namespace
