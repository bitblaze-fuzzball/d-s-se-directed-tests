#ifndef __CFG_H__
#define __CFG_H__

#include "types.h"
#include "graph.h"
#include "func.h"
#include "instr.h"
#include "json_spirit_writer_template.h"
#include <map>

class Function;
class Cfg;
class Instruction;

class BasicBlock {
    friend class Cfg;

private:
    instructions_t    instructions;
    addr_t            address;
    size_t            size;
    Cfg               *cfg;
    bool              decoded;

    friend class boost::serialization::access;
    template<class Archive>
	void serialize(Archive & ar, const unsigned int version) {
	ar & size;
	ar & address;
	ar & instructions;
	ar & cfg;
	(void)version;
    }

public:
    // for serialization
    BasicBlock() : address(0), size(0), cfg(0), decoded(false) {} 

    BasicBlock(addr_t a) : address(a), size(0), cfg(0), decoded(false){}

    ~BasicBlock();

    addr_t getAddress();
    size_t getSize();
    Cfg *getCfg();

    void addInstruction(Instruction *);
    size_t getInstructionsNo();
    void decode();
    int getNumPredecessors();

    void setAddress(addr_t a) { address = a; }

    Instruction *getInstruction(addr_t i);

    bool isCall();
    bool isReturn();
    bool isExecuted();

    instructions_t::const_iterator inst_begin() const { 
        return instructions.begin(); 
    }
    instructions_t::const_iterator inst_end() const {
        return instructions.end(); 
    }
};

std::ostream& operator<<(std::ostream&, const BasicBlock&);

class BasicBlockEdge {
    friend class Cfg;

private:
    BasicBlock *source;
    BasicBlock *target;

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version) {
	ar & source;
	ar & target;
	(void)version;
    }

public:
    BasicBlockEdge() {;}
    BasicBlockEdge(BasicBlock *s, BasicBlock *t);
    ~BasicBlockEdge();
    
    BasicBlock *getSource();
    BasicBlock *getTarget();    
};


class Cfg : public Graph<BasicBlock *, BasicBlockEdge *> {
public: // For testing
    typedef Graph<BasicBlock *, BasicBlockEdge *> cfg_t;
    typedef std::map<addr_t,functions_t> Adr2FunMapTy;
    typedef Adr2FunMapTy::const_iterator adr2fun_const_iterator;
    
    std::map<addr_t, BasicBlock *> addr2bb;
    // Map instructions to called functions
    Adr2FunMapTy calls;

    BasicBlock *entry;
    basicblocks_t exits;
    Function *function;
    bool decoded;
    bool can_have_self_loops;

    BasicBlock *addBasicBlock(addr_t addr);
    void delBasicBlock(BasicBlock *);
    void unlinkBasicBlocks(BasicBlock *sbb, BasicBlock *dbb);
    void linkBasicBlocks(BasicBlock *sbb, BasicBlock *dbb);
    BasicBlock *splitBasicBlock(BasicBlock *bb, addr_t before);
    void clear();

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version) {
	if (Archive::is_saving::value) {
	    sanityCheck();
	}

	cfg_t::serialize(ar, version);

	sanityCheck();

	ar & addr2bb;
	ar & function;
	ar & entry;
	ar & exits;
	ar & calls;

    }

    void sanityCheck(bool = false);

    void setEntry(BasicBlock *b) {
        entry = b;
        cfg_t::setEntry(b);
    }

    void augmentCfg(std::list<std::pair<addr_t, addr_t> > &, 
		    std::set<addr_t> &, std::map<addr_t, Function *> &, std::map<addr_t, Function *> &);

public:
    Cfg(Function *f = NULL) : entry(NULL), function(f), decoded(false),
    can_have_self_loops(true) {}

    ~Cfg() {}

    bool addInstruction(addr_t addr, byte_t *bytes, size_t len, 
			int pos, addr_t prev, bool isret = false);

    void addCall(addr_t caller, Function *callee);

    void augmentCfg(addr_t, std::map<addr_t, Function *> &funcs, std::map<addr_t, Function *> &indirect);
    
    BasicBlock *getEntry() const {
	return entry;
    }

    size_t getBasicBlocksNo();

    json_spirit::Object json();
    std::string dot();
    std::string vcg();
    std::string wto2string();

    void check();
    void decode();
    void removeSelfLoops();

    bool isEntry(BasicBlock *bb) {
        return bb == entry;
    }

    bool isExit(BasicBlock *bb) {
        return exits.find(bb) != exits.end();
    }

    Function *getFunction() { return function; }

    Instruction *getInstruction(addr_t i) { 
	assert(addr2bb.find(i) != addr2bb.end());
	return addr2bb[i]->getInstruction(i);
    }

    void setExecuted(addr_t i);
    bool isExecuted(addr_t i);

    typedef cfg_t::const_vertex_iterator const_bb_iterator;
    typedef cfg_t::const_edge_iterator const_edge_iterator;
    typedef basicblocks_t::iterator const_exit_iterator;

    const_bb_iterator bb_begin() const { return vertices_begin(); }
    const_bb_iterator bb_end() const { return vertices_end(); }
    const_edge_iterator edge_begin() const { return edges_begin(); }
    const_edge_iterator edge_end() const { return edges_end(); }
    const_exit_iterator exits_begin() const { return exits.begin(); }
    const_exit_iterator exits_end() const { return exits.end(); }

    // Note: Older version accessed the calls map by using the [] operator,
    // which is dangerous. If the element doesn't exist, a dumb element will be
    // created. With the current implementation, the function can become const.

    // XXX: cannot be const at the moment because we might statically reach an
    // indirect call and we might not be able to guess the target and add the
    // proper entry in the map --lm
    functions_t::const_iterator call_targets_begin(addr_t i) const {
	// adr2fun_const_iterator I = calls.find(i); assert_msg(I !=
	// calls.end(), "XXX: %.8x %.8x\n", function->getAddress(), i); return
	// I->second.begin();
        adr2fun_const_iterator I = calls.find(i);
        assert(I != calls.end() && "Address not found.");
        return I->second.begin();
    }

    functions_t::const_iterator call_targets_end(addr_t i) const {
	// adr2fun_const_iterator I = calls.find(i);
	// assert(I != calls.end() && "Index out of bounds.");
	// return I->second.end();
        adr2fun_const_iterator I = calls.find(i);
        assert(I != calls.end() && "Address not found.");
        return I->second.end();
    }

    functions_t::iterator call_targets_begin(addr_t i) {
        return calls[i].begin();
    }
    
    functions_t::iterator call_targets_end(addr_t i) {
        return calls[i].end();
    }


    functions_t::const_iterator call_targets_begin(const BasicBlock &bb);
    functions_t::const_iterator call_targets_end(const BasicBlock &bb);
};

#endif

// Local Variables: 
// mode: c++
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
