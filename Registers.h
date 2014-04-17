#ifndef ABSDOM_REGISTERS_H
#define ABSDOM_REGISTERS_H

namespace absdomain {

const static struct regs {
    const char *name;
    const int id;
    const int begin; // Inclusive
    const int end;   // Exclusive
    const int size;
} regs[] = {
    // Invariant: This array has to be sorted by name.
    // i=0; while read a b c d e ; do echo "$a $i, $c $d $e,"; 
    // i=$((i+1)); done
    {"AF",   0, 49, 50, 1},
    {"AH",   1,  2,  3, 1},
    {"AL",   2,  3,  4, 1},
    {"AX",   3,  2,  4, 2},
    {"BH",   4,  6,  7, 1},
    {"BL",   5,  7,  8, 1},
    {"BP",   6, 26, 28, 2},
    {"BX",   7,  6,  8, 2},
    {"CF",   8, 44, 45, 1},
    {"CH",   9, 10, 11, 1},
    {"CL",  10, 11, 12, 1},
    {"CS",  11, 34, 36, 2},
    {"CX",  12, 10, 12, 2},
    {"DF",  13, 50, 51, 1},
    {"DH",  14, 14, 15, 1},
    {"DI",  15, 22, 24, 2},
    {"DL",  16, 15, 16, 1},
    {"DS",  17, 38, 40, 2},
    {"DX",  18, 14, 16, 2},
    {"EAX", 19,  0,  4, 4},
    {"EBP", 20, 24, 28, 4},
    {"EBX", 21,  4,  8, 4},
    {"ECX", 22,  8, 12, 4},
    {"EDI", 23, 20, 24, 4},
    {"EDX", 24, 12, 16, 4},
    {"ES",  25, 32, 34, 2},
    {"ESI", 26, 16, 20, 4},
    {"ESP", 27, 28, 32, 4},
    {"FS",  28, 40, 42, 2},
    {"GDT", 29, 56, 60, 4},
    {"GS",  30, 42, 44, 2},
    {"LDT", 31, 52, 56, 4},
    {"OF",  32, 48, 49, 1},
    {"PF",  33, 46, 47, 1},
    {"SF",  34, 47, 48, 1},
    {"SI",  35, 18, 20, 2},
    {"SP",  36, 30, 32, 2},
    {"SS",  37, 36, 38, 2},
    {"ZF",  38, 45, 46, 1},
    // Terminator, determines the size of the register region.
    {"TERM",39,  0, 60,  1 },
//  { <++>, <++>, <++>, <++> },
};

namespace reg {

enum RegEnumTy {
    AF_REG = 0, AH_REG, AL_REG, AX_REG, BH_REG, BL_REG, BP_REG, BX_REG,
    CF_REG, CH_REG, CL_REG, CS_REG, CX_REG, DF_REG, DH_REG, DI_REG,
    DL_REG, DS_REG, DX_REG, EAX_REG, EBP_REG, EBX_REG, ECX_REG, EDI_REG,
    EDX_REG, ES_REG, ESI_REG, ESP_REG, FS_REG, GDT_REG, GS_REG, LDT_REG,
    OF_REG, PF_REG, SF_REG, SI_REG, SP_REG, SS_REG, ZF_REG, TERM_REG
};

} // End of the reg namespace

// Binary search based on the register name
const struct regs* regLookup(const char*);

// Linear search, but this is needed only in debugging anyways
const char* getRegNameAtAddress(int, int);

} // End of the absdomain namespace

#endif // ABSDOM_REGISTERS_H

// Local Variables: 
// mode: c++
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
