// TODO:
// 1) there's a circular dependency bteween cfg.h and instr.h. It's
// actually needed, because the compiler won't otherwise swallow the
// vine-enclosed STL libraries. This has to be fixed, as soon as there's
// time for that.

#ifndef __INSTR_H__
#define __INSTR_H__

#include <cassert>
#include <vector>
#include <ostream>
#include "debug.h"
#include "cfg.h"

namespace vine {
#include "irtoir.h"
}

#define BASICBLOCK_MIDDLE 0
#define BASICBLOCK_HEAD   1
#define BASICBLOCK_TAIL   2

class Instruction;
class BasicBlock;

typedef std::vector<vine::Stmt *> statements_t;

#define MAX_INSTR_LEN 16

class Instruction {
    friend class Cfg;
    friend class BasicBlock;

private:
    addr_t   address;
    byte_t   rawbytes[MAX_INSTR_LEN];
    size_t   size;
    statements_t statements;
    bool     decoded;
    BasicBlock *basicblock;
    uint32_t cksum;
    bool executed;

    friend class boost::serialization::access;
    template<class Archive>
	void serialize(Archive & ar, const unsigned int version) {
	ar & address;
	ar & size;
	ar & rawbytes;
	ar & cksum;
	ar & basicblock;
	ar & executed;
    }

public:
    Instruction();  // for serialization
    Instruction(addr_t, byte_t *b, size_t l);
    ~Instruction();

    void decode();
    size_t getSize();
    addr_t getAddress() const;
    byte_t *getRawBytes();
    BasicBlock *getBasicBlock();
    uint32_t computeCksum();
    uint32_t getCksum();

    void setAddress(addr_t a) { address = a; }

    bool isCall();
    bool isReturn();

    void setExecuted();
    bool isExecuted();

    void setRawBytes(const byte_t *, size_t);

    functions_t::const_iterator call_targets_begin() const;
    functions_t::const_iterator call_targets_end() const;

    statements_t::const_iterator stmt_begin() const;
    statements_t::const_iterator stmt_end() const;
    statements_t::const_reverse_iterator stmt_rbegin() const;
    statements_t::const_reverse_iterator stmt_rend() const;
};

std::ostream& operator<<(std::ostream&, const Instruction&);

#endif

// Local Variables: 
// mode: c++
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
