#include "debug.h"
#include "types.h"
#include "PinDisasm.h"
#include <pin.H>
#include <string.h>
#include <xed-interface.h>

size_t inslen(ADDRINT addr) {
    xed_state_t dstate;
    xed_decoded_inst_t xedd;

    xed_tables_init();

    xed_state_zero(&dstate);
    xed_state_init(&dstate,
                   XED_MACHINE_MODE_LEGACY_32, 
                   XED_ADDRESS_WIDTH_32b, 
                   XED_ADDRESS_WIDTH_32b);

    xed_decoded_inst_zero_set_mode(&xedd, &dstate);
    xed_decode(&xedd, (const xed_uint8_t*) addr, 16);
    return xed_decoded_inst_get_length(&xedd);
}


// Resolve thunks (only for main executable) In order for this to work lazy
// binding must be disabled (i.e., LD_BIND_NOW must be set)
// if (isplt(funcaddr) && !islib(instrptr)) {
ADDRINT derefplt(ADDRINT instrptr, ADDRINT funcaddr, ADDRINT ebx) {
  unsigned char *plt = (unsigned char *) funcaddr;

  debug2("Try to dereference PLT entry @ %.8x with base %.8x\n", funcaddr, ebx);

  // The entry is of the form 'jmp *0x804a014'
  if (plt[0] == 0xFF && plt[1] == 0x25) {
    funcaddr += 2;
    funcaddr = *((ADDRINT *) (*((ADDRINT *) funcaddr)));
    debug2("Resolved function address %.8x (PLT) -> %.8x\n", (ADDRINT) plt,
	   funcaddr);
  } else if (plt[0] == 0xFF && plt[1] == 0xa3) {
    // The entry is of the form 'jmp *0xc(%ebx)' 
    funcaddr += 2;
    funcaddr = *((ADDRINT *) funcaddr);
    funcaddr = *((ADDRINT *) (ebx + funcaddr));
    debug2("Resolved PIC function address %.8x (PLT) -> %.8x\n", (ADDRINT) plt,
	   funcaddr);
  } else {
    assert_msg(0, "Unknown PLT entry type eip:%.8x funcaddr:%.8x "
	       "plt[0]:%.2x plt[1]:%.2x", instrptr, funcaddr, plt[0], 
	       plt[1]);
  }
 
  return funcaddr;
}

byte_t ispicthunk(ADDRINT instptr) {
  if (memcmp((byte_t *) instptr, "\x8b\x1c\x24\xc3", 4) == 0) {
    return '\x1c';
  } else if (memcmp((byte_t *) instptr, "\x8b\x0c\x24\xc3", 4) == 0) {
    return '\x0c';
  } else if (memcmp((byte_t *) instptr, "\x8b\x14\x24\xc3", 4) == 0) {
    return '\x14';
  }
  
  return 0;
}

bool patchpicthunk(ADDRINT instrptr, ADDRINT funcaddr, Instruction *I) {
  byte_t r = ispicthunk(funcaddr);
  // Is the target 'mov (%esp),%ebx; ret'?
  if (r) {
    // Simulate a 'mov retaddr, %ebx;'
    debug2("Detected PIC thunk @ %.8x, called from %.8x\n", funcaddr, 
	   instrptr);
    ADDRINT addr = instrptr + 5;
    byte_t fake[5];

    // Generate fake 'mov $nexteip,%ebx'
    fake[0] = r;
    memcpy(fake + 1, &addr, sizeof(addr));
    I->setRawBytes(fake, 5);
    
    return true;
  }

  return false;
}

// Local Variables: 
// mode: c++
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
