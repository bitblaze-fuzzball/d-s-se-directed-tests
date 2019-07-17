#ifndef __INSTR_H__
#define __INSTR_H__

#include <cassert>
#include <vector>
#include <ostream>

#include <boost/serialization/vector.hpp>

#include "debug.h"
#include "types.h"

/* There's something a bit ugly going on with the "namespace vine"
   below. Putting all of the Vine/LibASMIR stuff in its own namespace
   is a reasonable complexity management idea, since there isn't any
   other naming convention used to make the LibASMIR code distinct,
   and it defines lots of common-sounding types. However the LibASMIR
   code was not designed to live in its own namespace, and putting a
   namespace declaration around a random include file doesn't
   necessarily do the right thing. In particular the C++ parts of
   LibASMIR use a lot of STL include files, but if those inclusions
   are inside a "namespace vine" the compiler will get confused, for
   instance thinking that std::cout is really vine::std::cout, so that
   then other uses will be wrong. The ugly thing that makes it work is
   include guards: each STL header file is designed to be empty if it
   has already been included. So as long as all of the STL headers
   that LibASMIR uses are actually included here before the "namespace
   vine" code tries to include them, everything will get into the
   right namespace. This is what the extensive list of STL includes
   below is for.  -- SMcC */

#include <iostream>
#include <set>
#include <map>
#include <string>
#include <sstream>
#include <algorithm>

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
	(void)version;
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
