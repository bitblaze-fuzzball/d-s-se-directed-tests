#ifndef __PIN_DISASM_H__
#define __PIN_DISASM_H__

#include "instr.h"
#include <pin.H>
#include <string.h>

class Instruction;

size_t inslen(ADDRINT addr);
ADDRINT derefplt(ADDRINT instptr, ADDRINT funcaddr, ADDRINT ebx);
byte_t ispicthunk(ADDRINT instptr);
bool patchpicthunk(ADDRINT instrptr, ADDRINT funcaddr, Instruction *I);

#endif // !__PIN_DISASM_H__

// Local Variables: 
// mode: c++
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
