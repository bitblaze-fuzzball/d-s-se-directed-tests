#include "instr.h"
#include "debug.h"
#include "cfg.h"


Instruction::Instruction() {
    decoded = false;
    executed = false;
}

Instruction::Instruction(addr_t a, byte_t *b, size_t s) {
    address = a;
    memcpy(rawbytes, b, s);
    size = s;
    decoded = false;
    executed = false;
    cksum = computeCksum();
}

Instruction::~Instruction() {
    if (decoded) {
	for (statements_t::iterator it = statements.begin(); 
	     it != statements.end(); it++) {
	    delete *it;
	}
	statements.clear();
    }

    decoded = false;
}

addr_t Instruction::getAddress() const {
    return address;
}

size_t Instruction::getSize() {
    return size;
}

byte_t *Instruction::getRawBytes() {
    return rawbytes;
}

void Instruction::setRawBytes(const byte_t *b, size_t s) {
    assert(s == size);
    memcpy(rawbytes, b, s);
    cksum = computeCksum();
}

BasicBlock *Instruction::getBasicBlock() {
    return basicblock;
}

void Instruction::setExecuted() {
    executed = true;
}

bool Instruction::isExecuted() {
    return executed;
}

void Instruction::decode() {
    vine::asm_program_t *prog;
    vine::vine_block_t *block;
    extern bool translate_calls_and_returns;

    if (decoded)
        return;

    assert_msg(cksum == computeCksum(), "%.8x %.8x \\x%.2x\\x%.2x", cksum, 
	       computeCksum(), rawbytes[0], rawbytes[1]);

    bool old_tcar = translate_calls_and_returns;
    translate_calls_and_returns = true;

    prog = vine::byte_insn_to_asmp(vine::asmir_arch_x86, address, rawbytes, size);
    block = vine::asm_addr_to_ir(prog, address);

    statements.insert(statements.begin(), block->vine_ir->begin(), block->vine_ir->end());

    delete block;
    vine::free_asm_program(prog);

    translate_calls_and_returns = old_tcar;

    decoded = true;
}

#define FNV_PRIME_32 16777619
#define FNV_OFFSET_32 2166136261U
uint32_t Instruction::computeCksum() {
  uint32_t hash = FNV_OFFSET_32, i;
  for(i = 0; i < size; i++) {
    hash = hash ^ (rawbytes[i]);      // xor next byte into the bottom of the hash
    hash = hash * FNV_PRIME_32;  // Multiply by prime number found to work well
  }
  return hash;
}

uint32_t Instruction::getCksum() {
    return cksum;
}

bool Instruction::isCall() {
    assert(decoded);

    for (statements_t::iterator it = statements.begin(); 
	 it != statements.end(); it++) {
	if ((*it)->stmt_type == vine::CALL) {
	    return true;
	}
    }

    return false;
}

bool Instruction::isReturn() {
    assert(decoded);

    for (statements_t::iterator it = statements.begin(); 
	 it != statements.end(); it++) {
	if ((*it)->stmt_type == vine::RETURN) {
	    return true;
	}
    }

    return false;
}

functions_t::const_iterator Instruction::call_targets_begin() const {
    return basicblock->getCfg()->call_targets_begin(address);
}

functions_t::const_iterator Instruction::call_targets_end() const {
    return basicblock->getCfg()->call_targets_end(address);
}

statements_t::const_iterator Instruction::stmt_begin() const {
    assert(decoded);
    return statements.begin();
}

statements_t::const_iterator Instruction::stmt_end() const {
    assert(decoded);
    return statements.end();
}

statements_t::const_reverse_iterator Instruction::stmt_rbegin() const {
    assert(decoded);
    return statements.rbegin();
}

statements_t::const_reverse_iterator Instruction::stmt_rend() const {
    assert(decoded);
    return statements.rend();
}

std::ostream& operator<<(std::ostream& os, const Instruction& I){
    for (statements_t::const_iterator II = I.stmt_begin(), IE =
            I.stmt_end(); II != IE; ++II) {
        os << (*II)->tostring() << std::endl;
    }
    return os;
}

// Local Variables: 
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
