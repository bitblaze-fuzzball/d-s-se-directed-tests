// TODO:
// 1) std::deque is more efficient than std::list for worklist purposes,
// but for efficient searching, we'd need either a map or a hash table.
// If map is used, the ordering must be deterministic and total!

#ifndef ABSTRACT_INTERPRETER_H
#define ABSTRACT_INTERPRETER_H

#include "Utilities.h"
#include "Rand.h"
#include "cfg.h"
#include "Counted.h"
#include "HashFunctions.h"
#include "Context.h"
#include "Registers.h"
#include <map>
#include <vector>
#include <set>
#include <string>
#ifndef NDEBUG
#include <sstream>
#endif

#include "warning.h"

namespace absinter {

typedef const vine::Exp Expression;
typedef const vine::BinOp BinopExpr;
typedef const vine::UnOp UnopExpr;
typedef const vine::Mem MemExpr;
typedef const vine::Constant ConstExpr;
typedef const vine::Temp TempExpr;
typedef const vine::Cast CastExpr;
typedef const vine::Stmt Statement;
typedef const vine::Move MoveInstr;
typedef const vine::VarDecl VarDeclInstr;
typedef const vine::Call CallInstr;
typedef const vine::Return ReturnInstr;

typedef std::list<BasicBlock *> bb_list_t;
typedef std::set<const BasicBlock *> bb_set_t;
typedef std::list<const Function *> func_list_t;

// ****************************************************************************

// Template parameters:
// SubClass - subclass of the AbstractInterpreter, implementing the
// chosen functions
// ValSetTy - a pointer type representing the abstract domain over which
// the expressions operate
// StateTy - a pointer type representing the abstract domain over which
// the statements operate

template<class SubClass, class ValSetTy, class StateTy>
class AbstractInterpreter {
public:
    typedef typename StateTy::StatePtr StatePtr;
    typedef typename StateTy::RegionPtr RegionPtr;
    typedef typename ValSetTy::VSetPtr VSetPtr;
    typedef typename std::vector<StatePtr> StateVectorTy;
    typedef typename StateTy::RegionVecTy RegionVecTy;
    typedef typename StateVectorTy::iterator state_iterator;
    typedef typename ValSetTy::AlocPair AlocPair;

protected:
    typedef utils::Map<CallInstr*, int> StackDepthMapTy;
    typedef utils::Map<std::string, VSetPtr> TempMapTy;
    typedef std::map<ContextPtr, BasicBlock *> CtxBBTy;
    typedef std::map<BasicBlock *, StatePtr> bb2state_t;
    typedef std::map<BasicBlock *, RegionPtr> bb2heapobj_t;

    TempMapTy temps;
    StackDepthMapTy depth;

    // State after the execution of the basic block in a given context
    std::map<ContextPtr, bb2state_t *, ctxcmp> ctx_post_states;
    // State before the execution of the basic block in a given context
    std::map<ContextPtr, bb2state_t *, ctxcmp> ctx_pre_states;
    // Heap object allocated by the basic block
    bb2heapobj_t heap_objects;

    // State after the execution of the basic block in the current context
    bb2state_t *cur_ctx_post_states;
    // State before the execution of the basic block in the current context
    // (join of all incoming states)
    bb2state_t *cur_ctx_pre_states;

    // Current context
    ContextPtr cur_context;
    // Current state
    StatePtr cur_state;
    // Current basic block (hack to avoid to pass to call statements the bb)
    BasicBlock *cur_bb;
    // Current function
    Function *cur_func;
    Instruction *cur_instr;

    // Callstack
    func_list_t callstack;

public:

    AbstractInterpreter() {
        cur_state = StateTy::getInitForMain();
	cur_func = NULL;
	interproc = true;
    }

    void setInterproc(bool b) {
	interproc = b;
    }

    // Usage: The expected usage model is that subclasses will not touch
    // visit(...) functions and will subclass visitX(...) functions for
    // which the default implementation is not implementing the right
    // functionality.

    void visit(Cfg&, addr_t = 0);

    void visit(Function& F) {
        if (!static_cast<SubClass*>(this)->visitFunction(F)) {
            visit(*F.getCfg());
        }
    }

    void visit(BasicBlock&);

    // These two are implemented below
    void visit(Instruction&);
    void visit(Statement&);
    VSetPtr visit(Expression&);
    
    // Pointer versions
    void visit(Cfg* C, addr_t a = 0)           {        visit(*C, a); }
    void visit(Function* F)      {        visit(*F); }
    void visit(BasicBlock* B)    {        visit(*B); }
    void visit(Instruction* I)   {        visit(*I); }
    void visit(Statement* S)     {        visit(*S); }
    VSetPtr visit(Expression* E){ return visit(*E); }

    // Functions that overwrite these functions must return true, if
    // they want to surpress the currently implemented visit()
    // functionality.
    bool visitCFG(Cfg&) { return false; }
    bool visitFunction(Function&) { return false; }
    bool visitBasicBlock(BasicBlock&) { return false; }
    bool visitInstruction(Instruction&) { return false; }

    // Support for special functions
    void visitXallocInstr(Instruction&, const char*) {}
    void visitStringInstr(Instruction&, const char*) {}

#define DELEGATE(TO_VISIT) \
    (void)static_cast<SubClass*>(this)->visit##TO_VISIT(I);

    void visitMoveInstr(MoveInstr& I)      { DELEGATE(Statement) }
    void visitVarDeclInstr(VarDeclInstr& I){ DELEGATE(Statement) }
    void visitCallInstr(CallInstr& I);
    void visitReturnInstr(ReturnInstr& I)  { DELEGATE(Statement) }

    // Silently ignore unhandled statements
    bool visitStatement(Statement&) { return false; }

#undef DELEGATE
#define DELEGATE(TO_VISIT) \
    return static_cast<SubClass*>(this)->visit##TO_VISIT(E);

    // Binary operators
    VSetPtr visitBinopExpr(BinopExpr& E, VSetPtr, VSetPtr) { 
        DELEGATE(Expression) 
    }
    // Unary operators
    VSetPtr visitUnopExpr(UnopExpr& E, VSetPtr) {
        DELEGATE(Expression)
    }
    VSetPtr visitMemExpr(MemExpr& E, VSetPtr) {
        DELEGATE(Expression)
    }
    VSetPtr visitCastExpr(CastExpr& E, VSetPtr) {
        DELEGATE(Expression);
    }
    // Zero-operand operators
    VSetPtr visitTempExpr(TempExpr& E) {
        DELEGATE(Expression)
    }
    VSetPtr visitConstExpr(ConstExpr& E) {
        DELEGATE(Expression)
    }
#undef DELEGATE

    // Silently ignore unhandled expressions
    VSetPtr visitExpression(Expression&) { 
        return VSetPtr();
    }
    
    // Warning: None of these can operate on the internal cur_state, as
    // cur_state might need to be compared to the result of these
    // functions.
    StatePtr join(StateVectorTy&);
    StatePtr meet(StateVectorTy&);
    StatePtr widen(StateTy&, StateTy&);
    StatePtr rwiden(StatePtr, StatePtr) {
        assert(false && "Not yet implemented.");
        return StatePtr::get();
    }

private:
    bool preVisit(BasicBlock &, BasicBlock &, StatePtr &, bb_list_t &);
    void putComponentInWorklist(BasicBlock &, bb_list_t &, addr_t addr, bool = true);
    void postVisit(BasicBlock &, bb_list_t &);

    addr_t enterFunction(Function &, addr_t);
    void leaveFunction(const Function &, ContextPtr);

    bool interproc;
};

// *** Implementation ***

template <class SubClass, class ValSetTy, class StateTy>
inline typename AbstractInterpreter<SubClass,ValSetTy,StateTy>::VSetPtr
AbstractInterpreter<SubClass, ValSetTy, StateTy>::visit(Expression& E) {

#define DELEGATE(TO_VISIT, ARGS...) \
    return static_cast<SubClass*>(this)-> \
    visit##TO_VISIT(static_cast<const TO_VISIT&>(E), ## ARGS);

    switch (E.exp_type) {
        case vine::BINOP: {
            BinopExpr& boe = static_cast<BinopExpr&>(E);
            DELEGATE(BinopExpr, visit(boe.lhs), visit(boe.rhs))
        } break;
        case vine::UNOP: {
            UnopExpr& uoe = static_cast<UnopExpr&>(E);
            DELEGATE(UnopExpr, visit(uoe.exp))
        } break;
        case vine::CONSTANT:
            DELEGATE(ConstExpr)
            break;
        case vine::MEM: {
            MemExpr& me = static_cast<MemExpr&>(E);
            DELEGATE(MemExpr, visit(me.addr))
        } break;
        case vine::TEMP:
            DELEGATE(TempExpr)
            break;
        case vine::CAST: {
            CastExpr& ce = static_cast<CastExpr&>(E);
            DELEGATE(CastExpr, visit(ce.exp))
        } break;
        case vine::ITE: // XXX should have a proper delegation
        case vine::UNKNOWN:
            return ValSetTy::getTop();
            break;
        default:
            assert(false && "Unhandled expression type.");
            return VSetPtr();
    }
#undef DELEGATE
}

template <class SubClass, class ValSetTy, class StateTy>
inline void
AbstractInterpreter<SubClass,ValSetTy,StateTy>::visit(Statement& S) {
    if (static_cast<SubClass*>(this)->visitStatement(S)) {
        return;
    }

#define DELEGATE(TO_VISIT) \
    static_cast<SubClass*>(this)-> \
    visit##TO_VISIT(static_cast<const TO_VISIT&>(S));

    debug3("\t\t\t[STATEMENT] %s\n",
            const_cast<vine::Stmt&>(S).tostring().c_str());

    switch (S.stmt_type) {
        case vine::MOVE:
            DELEGATE(MoveInstr)
            break;
        case vine::VARDECL: // Create a temporary
            DELEGATE(VarDeclInstr)
            break;
        case vine::CALL:
            visitCallInstr(static_cast<const CallInstr&>(S));
            break;
        case vine::RETURN:
            DELEGATE(ReturnInstr)
            break;
        case vine::JMP:     // Just skip, nothing to do
        case vine::CJMP:
            // CJMP has to be handled by the abstract interpreter's
            // client. The client has to do copy-const propagation and
            // construct a predicate for restricting widening.
        case vine::SPECIAL:
            // Special instructions not translated by Vine, like
            // interrupts and similar. Silently ignore them.
        case vine::COMMENT:
        case vine::LABEL:
        case vine::ASSERT:
            break;
        default:
            assert(false && "Undefined statement.");
            break;
    }
#undef DELEGATE
}

//#define TEMPLATE template<class SubClass, class ValSetTy, class StateTy>
//#define ABSTRACTINTEPRETER AbstractIntepreter<SubClass,ValSetTy,StateTy>

template<class SubClass, class ValSetTy, class StateTy>
inline void
AbstractInterpreter<SubClass,ValSetTy,StateTy>::visitCallInstr(CallInstr
        &i) {
    (void)i;
    BasicBlock *bb = cur_bb;
    Cfg *cfg = cur_bb->getCfg();

#define INTERPROCEDURAL_CONTEXT_SENSITIVE_ANALYSIS
#ifdef INTERPROCEDURAL_CONTEXT_SENSITIVE_ANALYSIS
    assert(!cur_state->empty() && "Uninitialized state.");
    StateVectorTy states_after_call;
	
    // Backup the state before the call
    StatePtr state_before_call = cur_state;
    ContextPtr ctx_before_call = cur_context;
    bb2state_t *ctx_post_states_before_call = cur_ctx_post_states;
    RegionPtr caller_regs = cur_state->getRegister();

    // At this point, stack already contains the return address, and ESP
    // points past that address. The return instruction, on return,
    // increments ESP by 4, so we need to compensate for that.
    caller_regs = caller_regs->write(absdomain::reg::ESP_REG,
            *caller_regs->read(absdomain::reg::ESP_REG) +
            *ValSetTy::get(4,4));

    VSetPtr stack_frame_size = *caller_regs->read(absdomain::reg::EBP_REG) - 
        *caller_regs->read(absdomain::reg::ESP_REG);

    debug2("@@@@ %.8x estimated stack frame size is... ", 
           cfg->getFunction()->getAddress());

    if (DEBUG_LEVEL >= 2)
        std::cerr << *stack_frame_size << std::endl;

    for (functions_t::const_iterator ctit = cfg->call_targets_begin(*bb); 
	 ctit != cfg->call_targets_end(*bb); ++ctit) {
	assert(!cur_state->empty() && "Uninitialized state.");

	const int cutOff =  
	    cur_state->read(absdomain::reg::ESP_REG)->isConstant() ?
	    cur_state->read(absdomain::reg::ESP_REG)->
            begin()->get().second->getLo() :
            0;
	assert(cutOff <= 0 && "Invalid cutoff.");

	// Interpret all functions starting from the same initial state since
	// only one function is effectively called
	visit((*ctit)->getCfg(), cur_instr->getAddress());

	// Save the state after the call
	if (cutOff < 0) {
	    states_after_call.push_back(cur_state->discardFrame(cutOff));
	} else {
	    states_after_call.push_back(cur_state);
	}

	// Restore the state before the call to be able to reinterpret
	// the call starting from the same original state
	cur_context = ctx_before_call;
	cur_state = state_before_call;
	cur_ctx_post_states = ctx_post_states_before_call;
    }

    if (!states_after_call.empty()) {
	// Join callees' output state
	cur_state = join(states_after_call);
	assert(!cur_state->empty() && "Uninitialized state.");
    }

    // Restore ESP, EBP, this is a quick hack
    cur_state = cur_state->write(absdomain::reg::ESP_REG,
            caller_regs->read(absdomain::reg::ESP_REG));
    cur_state = cur_state->write(absdomain::reg::EBP_REG,
            caller_regs->read(absdomain::reg::EBP_REG));

    // Update the state for the current bb
    assert(!cur_state->empty() && "Uninitialized state.");
    (*cur_ctx_post_states)[bb] = cur_state;
#else
    // This is currently not used, but the idea is to simulate the effect of
    // return.
    RegionPtr caller_regs;

    caller_regs = caller_regs->write(absdomain::reg::ESP_REG,
            *caller_regs->read(absdomain::reg::ESP_REG) +
            *ValSetTy::get(4,4));
    cur_state = cur_state->write(absdomain::reg::ESP_REG,
            caller_regs->read(absdomain::reg::ESP_REG));

    // Update the state for the current bb
    assert(!cur_state->empty() && "Uninitialized state.");
    (*cur_ctx_post_states)[bb] = cur_state;
#endif // INTERPROCEDURAL_CONTEXT_SENSITIVE_ANALYSIS
}

inline const char *isAlloc(Instruction &i) {
    Cfg *cfg = i.getBasicBlock()->getCfg();

    functions_t::const_iterator it;
    for(it = cfg->call_targets_begin(i.getAddress());
	    it != cfg->call_targets_end(i.getAddress()); ++it) {
	Function *f = *it;
#define func_name(x, y) (strcmp(x->getName(), y) == 0)
	if (func_name(f, "malloc") || func_name(f, "calloc") ||
	    func_name(f, "realloc") || func_name(f, "__libc_malloc")) {
	    return f->getName();
	}
#undef func_name
    }	    

    return NULL;
}

inline const char *isStringFun(Instruction &i) {
    Cfg *cfg = i.getBasicBlock()->getCfg();

    for (functions_t::const_iterator I =
            cfg->call_targets_begin(i.getAddress()), E =
            cfg->call_targets_end(i.getAddress()); I != E; ++I) {
        const Function* f = *I;
#define FCMP(y) (strcmp(f->getName(), y) == 0)
        if (FCMP("strcat") || FCMP("memcpy") || FCMP("strcpy") ||
            FCMP("sprintf") || FCMP("strncpy")) {
            return f->getName();
        }
#undef FCMP
    }
    return NULL;
}

inline bool isBlacklisted(const Function &f) {
#define CMPSTR(x,y) (strcmp(x,y) == 0)
    // We don't know how to handle it 
    if (CMPSTR(f.getName(), "free")) {
	return true;
    }

    // We don't know how to handle it (in dietlibc the sysenter wrapper has no
    // symbol)
    if (CMPSTR(f.getName(), "__unified_syscall") ||
	CMPSTR(f.getName(), "unknown")) {
	return true;
    }

    // Recursive
    if (CMPSTR(f.getName(), "fflush") || 
	CMPSTR(f.getName(), "fflush_unlocked") ||
        CMPSTR(f.getName(), "syslog") ||
        CMPSTR(f.getName(), "__libc_vsyslog") ||
        CMPSTR(f.getName(), "__v_printf")) {
	return true;
    }

    // Does not return
    if (CMPSTR(f.getName(), "__assert_fail") ||
	CMPSTR(f.getName(), "__libc_exit") ||
	CMPSTR(f.getName(), "__stack_chk_fail") ||
	CMPSTR(f.getName(), "ftrylockfile")) {
	return true;
    }

    // Exit 
    if (CMPSTR(f.getName(), "_exit")) {
	return true;
    }

    // Does not exist in the origianl program
    if (CMPSTR(f.getName(), "taint_data")) {
	return true;
    }

    // Output only, doesn't change state
    if (CMPSTR(f.getName(), "printf")) {
        return true;
    }

#undef CMPSTR

    return false;
}

template<class SubClass, class ValSetTy, class StateTy>
inline void
AbstractInterpreter<SubClass,ValSetTy,StateTy>::visit(Instruction &i) {
    BasicBlock *bb = cur_bb;
    Cfg *cfg = bb->getCfg();

    cur_instr = &i;
    current_instruction = i.getAddress();

#ifndef NDEBUG
    {
        debug3("\t\t[INSTRUCTION] %.8x %s\n", i.getAddress(), 
           disasm(i.getRawBytes()));
        std::stringstream ststring;
        ststring << *cur_state;
        debug3("\t\t[STATE]\n%s", ststring.str().c_str());
    }
#endif

    temps.clear();
    
    if (static_cast<SubClass*>(this)->visitInstruction(i)) {
        return;
    }

    if (i.isCall()) {
        const char *a = isAlloc(i);
        if (a) {
            static_cast<SubClass*>(this)->visitXallocInstr(i, a);
            return;
        }

        a = isStringFun(i);
        if (a) {
            static_cast<SubClass*>(this)->visitStringInstr(i, a);
            // TODO: decide whether to summarize or to interpret these
            // functions. Currently, they don't do anything really
            // special.
            
        }

        if (!interproc) {
            debug2("\t\tSkipping call because the analysis is "
               "intraprocedural\n");
            return;
        }

        for (functions_t::const_iterator ctit =
             cfg->call_targets_begin(*bb); ctit !=
             cfg->call_targets_end(*bb); ++ctit) {

            // DB: Why are we skipping all call targets even if only one
            // is blacklisted? 
	    // LM: These functions are typically called directly. Anyway, the
	    // new stack handling should allow to postpone the visitCallInstr
            if (isBlacklisted(**ctit)) {
                debug2("\t\tSkipping call to %.8x %s@%s because "
                        "blacklisted\n", (*ctit)->getAddress(),
                        (*ctit)->getName(), (*ctit)->getModule());
                return;
            }
        }

        if (i.getBasicBlock()->getCfg()->call_targets_begin(
             i.getAddress()) == i.getBasicBlock()->getCfg()->
             call_targets_end(i.getAddress())) { 
                debug3("!!! Cannot interpret the call @ %.8x since "
                    "no call target is known\n", i.getAddress());
            return;
        }
    }

    for (statements_t::const_iterator sit = i.stmt_begin(); sit !=
            i.stmt_end(); ++sit) {
        visit(**sit);
    }
}

template<class SubClass, class ValSetTy, class StateTy>
inline void
AbstractInterpreter<SubClass,ValSetTy,StateTy>::visit(BasicBlock &bb) {
    if (static_cast<SubClass*>(this)->visitBasicBlock(bb)) {
        return;
    }

    assert(!cur_state->empty() && "Uninitialized state.");
    for (instructions_t::const_iterator iit = bb.inst_begin(); iit !=
            bb.inst_end(); ++iit) {
        visit(**iit);
    }
    assert(!cur_state->empty() && "Uninitialized state.");
}

// Return true if the BB is in the worklist 
// XXX: this is extremely slow. In the future bb_list_t will be replaced with
// a more suited data structure
inline bool inWorklist(const BasicBlock &bb, const bb_list_t &wl) {
    for (bb_list_t::const_iterator it = wl.begin(); it != wl.end(); ++it) {
	if (*it == &bb) {
	    return true;
	}
    }

    return false;
}

inline void printWorklist(const bb_list_t &wl) {
    for (bb_list_t::const_iterator it = wl.begin(); it != wl.end(); ++it) {
	debug2(" %.8x", (*it)->getAddress());
    }
}

// Traverse all incoming edges, decide which ones are join, and which ones are
// widening edges, and perform the appropriate actions
template<class S, class V, class St>
inline bool AbstractInterpreter<S, V, St>::preVisit(BasicBlock &bb,
					     BasicBlock &pbb,
					     StatePtr &new_state,
					     bb_list_t &wl) {
    Cfg *cfg = bb.getCfg();
    StateVectorTy states_to_join;
    StatePtr state_to_widen;
    BasicBlock *bb_to_widen = NULL;
    bool widened = false;

    debug2("\t\tIncoming states:\n");	
    
    for (Cfg::const_pred_iterator pit = cfg->pred_begin(&bb); pit !=
	     cfg->pred_end(&bb); ++pit) {
	char msg[64];

	if (cfg->isSubComponentOf(*pit, &bb)) {
	    // This is a widening edge (the current bb is the entry of the
	    // component and the predecessor is the source of the backedge)
	    if (inWorklist(**pit, wl)) {
		// The source of the backedge hasn't been interpreted yet.
		sprintf(msg, "backedge: will NOT apply widening");
	    } else {
		if (*pit == &pbb) {
		    widened = true;
		    assert(!bb_to_widen);
		    bb_to_widen = *pit;
		    state_to_widen = (*cur_ctx_post_states)[*pit];
		    strcpy(msg, "backedge: will apply widening");
		} /* else {
		    // XXX: this is useless since we don't do any join when we
		    // apply widening
		    assert(cur_ctx_post_states->find(*pit) != 
		       cur_ctx_post_states->end());
		    states_to_join.push_back((*cur_ctx_post_states)[*pit]);
		    strcpy(msg, "backedge: will be joined");
		} */
	    }
	} else {
	    // This is a regular edge. Note that in case of widening no join
	    // will be performed
	    if (cur_ctx_post_states->find(*pit) != cur_ctx_post_states->end()) {
		assert(!(*cur_ctx_post_states)[*pit]->empty());

		states_to_join.push_back((*cur_ctx_post_states)[*pit]);
		strcpy(msg, "regular edge: will be joined");
	    }
	}

	debug2("\t\t\t%.8x-%.8x --> %.8x-%.8x [%s]\n",
	       (*pit)->getAddress(), 
	       (*pit)->getAddress() + (*pit)->getSize() - 1,
	       bb.getAddress(), 
	       bb.getAddress() + bb.getSize() - 1, msg);
    }

    if (widened) {
	debug2("\t\tWidening %.8x-%.8x wrt %.8x-%.8x\n",
	       bb.getAddress(), 
	       bb.getAddress() + bb.getSize() - 1,
	       bb_to_widen->getAddress(), 
	       bb_to_widen->getAddress() + bb_to_widen->getSize() - 1);


	// This is the loop carried state (the state before the last execution
	// of the loop)
	new_state = (*cur_ctx_pre_states)[&bb];
	assert(cur_ctx_pre_states->find(&bb) != 
	       cur_ctx_pre_states->end());

	assert(new_state.get() && "Undefined state.");
	assert(!new_state->empty() && "Undefined state.");
	assert(state_to_widen.get() && "Undefined state.");
	assert(!state_to_widen->empty() && "Undefined state.");

	new_state = widen(*new_state, *state_to_widen);
    } else if (!states_to_join.empty()) {
	if (states_to_join.size() > 1)
	    debug2("\t\tJoining multiple incoming states\n");
	new_state = join(states_to_join);
	assert(!new_state->empty());
    }

    return widened;
}

template<class S, class V, class St>
inline void AbstractInterpreter<S, V, St>::postVisit(BasicBlock &bb, 
					      bb_list_t &wrklist) {
    Cfg *cfg = bb.getCfg();

    for (Cfg::const_succ_iterator sit = cfg->succ_begin(&bb); sit !=
	     cfg->succ_end(&bb); ++sit) {
	BasicBlockEdge *edge = cfg->getEdge(&bb, *sit);

	if (*sit == cfg->getComponent(&bb)) {
	    // This is a widening edge (the target of the edge is the entry of
	    // the component)
	    assert(cur_ctx_post_states->find(*sit) != 
		   cur_ctx_post_states->end());

	    debug2("\t\tReached the source of a widening edge "
		   "[%.8x-%.8x -> %.8x-%.8x]\n", 
		   bb.getAddress(), bb.getAddress() + bb.getSize() - 1,
		   (*sit)->getAddress(), 
		   (*sit)->getAddress() + (*sit)->getSize() - 1);
	    
	    // Put the entry of the component in the worklist (subsequent
	    // nodes will be added only if widening produces a new state)
	    wrklist.push_front(*sit);
	    
	    debug2("\t\tPutting %.8x-%.8x %s in the worklist for "
		   "widening at the next iteration\n", (*sit)->getAddress(), 
		   (*sit)->getAddress() + (*sit)->getSize() - 1,
		   "again");
	}
    }

    assert(cur_ctx_post_states->find(&bb) != cur_ctx_post_states->end());
    assert((*cur_ctx_post_states)[&bb].get() && "Uninitialized state.");
}

// Put the basic block of a component in the worklist (the entry point of the
// component can be excluded if needed).
template<class S, class V, class St>
inline void AbstractInterpreter<S, V, St>::putComponentInWorklist(BasicBlock &bb, 
								  bb_list_t &wlist, addr_t rec_addr,
								  bool putentry) {
    Cfg *cfg = bb.getCfg();
    bool k = false;
    bb_list_t::iterator wlit = wlist.begin();

    for (Cfg::const_wto_iterator wit = cfg->wto_begin(); wit !=
	     cfg->wto_end(); ++wit) {
	// TODO: Temporary hack to avoid an extra layer of iterators
	BasicBlock *bb2 = cfg->getVertex(*wit); 
	if (bb2->getAddress() == rec_addr) {
	    //fprintf(stderr, "Skipping recursive function block: %.8x\n", rec_addr);
	    continue;
	}

	if (&bb == cfg->getComponent(bb2))
	    // Mark the component as entered
	    k = true;

	if (k) {
	    // Put basic block in the component in the worklist
	    if (cfg->getComponentNo(bb2) < cfg->getComponentNo(&bb))
		// Stop if we are outside the component
		break;

	    if (((&bb == bb2) && putentry) || (&bb != bb2)) {
		// Put vertex in the worklist if not already in (a vertex could
		// be already in the worklist in case we have a loop with
		// multiple backages)
		if (!inWorklist(*bb2, wlist)) {
		    wlit = wlist.insert(wlit, bb2);
		    ++wlit;
		} else {
		    debug2("\t\tSkipping %.8x-%.8x because already in the "
			   "worklist\n", bb2->getAddress(), 
			   bb2->getAddress() + bb2->getSize() - 1);
		}
	    }
	}
    }
}


template<class S, class V, class St>
inline void AbstractInterpreter<S, V, St>::visit(Cfg &cfg, addr_t
        callsite) {
    addr_t rec_addr = 0;
    assert(!cur_state->empty() && "Uninitialized state.");
    if (static_cast<S*>(this)->visitCFG(cfg)) {
        return;
    }

    assert(!cur_state->empty() && "Uninitialized state.");

    debug2("[FUNCTION] %s@%s %.8x %d\n", cfg.getFunction()->getName(), 
	   cfg.getFunction()->getModule(), cfg.getFunction()->getAddress(),
	   callstack.size());

    Function *prev_func = cur_func;
    ContextPtr prev_context = cur_context;
    rec_addr = enterFunction(*cfg.getFunction(), callsite);

    cur_func = cfg.getFunction();
    assert(!cur_state->empty() && "Uninitialized state.");

    debug2("Weak topological ordering: %s\n", cfg.wto2string().c_str());

    // Worklist (used for tracking the block that requires interpretation)
    bb_list_t worklist;
    // Put all the basic blocks in the worklist
    putComponentInWorklist(*cfg.getEntry(), worklist, rec_addr);

    BasicBlock *bb = 0, *pbb = 0;
    while (!worklist.empty()) {
        StatePtr prev_state;
	bool widened;

	debug2("\tWorklist:");
	printWorklist(worklist);
	debug2("\n");
	
        bb = worklist.front();
        worklist.pop_front();
	current_instruction = bb->getAddress();

        debug2("\t[BASICBLOCK] %.8x-%.8x (reached from %.8x-%.8x) %s\n", 
	       bb->getAddress(),
	       bb->getAddress() + bb->getSize() - 1, 
	       pbb ? pbb->getAddress() : 0,
	       pbb ? pbb->getAddress() + pbb->getSize() - 1 : 0, 
	       cur_ctx_post_states->find(bb) != cur_ctx_post_states->end() ? 
	       "***" : "");

	// Ensure that the graph has no self loops (they should have been
	// removed before the visit)
	assert(!cfg.hasEdge(bb, bb) && "Graph has a self loop.");
	
        // Pre-interpretation: join multiple incoming states and apply
        // widening if needed. Returns true if widening has been applied
	assert(cur_state.get() && "Uninitialized state.");
        assert(!cur_state->empty() && "Uninitialized state.");
	widened = preVisit(*bb, *pbb, cur_state, worklist);
	assert(cur_state.get() && "Uninitialized state.");
	assert(!cur_state->empty() && "Uninitialized state.");

        if (widened) {
	    assert(cur_ctx_pre_states->find(bb) != cur_ctx_pre_states->end());
	    prev_state = (*cur_ctx_pre_states)[bb];
	    assert(prev_state.get() && "Uninitialized state.");
	    assert(!prev_state->empty() && "Uninitialized state.");

            if (!cur_state->subsumedBy(*prev_state)) {
                // Widening was performed and we obtainted a new state
                // (we need to reinterpret the component because we
                // haven't reached the fixpoint yet)
                debug2("\t\tFixed point not yet reached, need to "
                    "reinterpret the rest of the component\n");
                putComponentInWorklist(*bb, worklist, rec_addr, false);
            } else {
		// Fixed point reached, we don't have to re-interprtet the
		// component
                debug2("\t\tFixed point reached!\n");
		debug2("\t\tStoring state for %.8x\n", bb->getAddress());
		(*cur_ctx_post_states)[bb] = cur_state;
                continue;
            }
        } 
	
	// Store the state right before the execution of the basic block
	debug2("\t\tStoring pre-state for %.8x\n", bb->getAddress());
	(*cur_ctx_pre_states)[bb] = cur_state;

        // Interpret the current basic block
        debug2("\t\tInterpreting basic block\n");
	BasicBlock *local_bb = cur_bb = bb;
        visit(*bb);
	cur_bb = local_bb;

	debug2("\t\tStoring post-state for %.8x\n", bb->getAddress());
	assert(cur_state.get() && "Uninitialized state.");
	(*cur_ctx_post_states)[bb] = cur_state;

        // Post-interpretation: check if we reached the source of a widening
        // edge and put the entry of the loop back in the worklist
        postVisit(*bb, worklist);

	pbb = bb;
    }

    // The final state is the join of all exits points
    debug2("\tJoining exit states:");
    StateVectorTy exit_states;
    for (Cfg::const_exit_iterator eit = cfg.exits_begin();
	 eit != cfg.exits_end(); ++eit) {
	assert(cur_ctx_post_states->find(*eit) != cur_ctx_post_states->end());
	debug2(" %.8x-%.8x", (*eit)->getAddress(),
	       (*eit)->getAddress() + (*eit)->getSize() - 1);
	exit_states.push_back((*cur_ctx_post_states)[*eit]);
    }
    debug2("\n");

    cur_state = join(exit_states);
    assert(!cur_state->empty() && "Uninitialized state.");
    
    debug2("[FUNCTION] %s@%s %.8x DONE!!!!\n\n", cfg.getFunction()->getName(), 
	   cfg.getFunction()->getModule(), cfg.getFunction()->getAddress());

    if (prev_context.get())
	leaveFunction(*cfg.getFunction(), prev_context);
    
    cur_func = prev_func;

    if (cur_func) {
	debug2("Resuming function %s@%s %.8x\n\n",  cur_func->getName(), 
	       cur_func->getModule(), cur_func->getAddress());
    }
}

template<class SubClass, class ValSetTy, class StateTy>
inline typename AbstractInterpreter<SubClass,ValSetTy,StateTy>::StatePtr
AbstractInterpreter<SubClass, ValSetTy,
        StateTy>::join(StateVectorTy& v) {

    assert(!v.empty() && "Can't join an empty vector of states.");

    state_iterator I = v.begin(), E = v.end();
    StatePtr s = *I;
    ++I;
    assert(s && "Unexpected NULL ptr.");

    for (; I != E; ++I) {
	assert(*I && "Unexpected NULL ptr.");
        s = s->join(**I);
    }

    return s;
}

template<class SubClass, class ValSetTy, class StateTy>
inline typename AbstractInterpreter<SubClass,ValSetTy,StateTy>::StatePtr
AbstractInterpreter<SubClass, ValSetTy,
        StateTy>::meet(StateVectorTy& v) {
    
    assert(!v.empty() && "Can't join an empty vector of states.");

    state_iterator I = v.begin(), E = v.end();
    StatePtr s = *I;
    ++I;

    for (; I != E; ++I) {
        assert(s && "Unexpected NULL ptr.");
        s = s->meet(**I);
    }

    return s;
}

template<class SubClass, class ValSetTy, class StateTy>
inline typename AbstractInterpreter<SubClass,ValSetTy,StateTy>::StatePtr
AbstractInterpreter<SubClass, ValSetTy,
		    StateTy>::widen(StateTy& l, StateTy& w) {
    return l.widen(w);
}


template<class S, class V, class St>
inline addr_t AbstractInterpreter<S,V,St>::enterFunction(Function &f, 
						addr_t callsite) {
    // *************************************************************************
    // Ensure that the function has at least one exit point
    assert(!f.getCfg()->exits.empty() && "Function has no exit points.");

    // *************************************************************************
    // Put the function the in callstack (slow!)
    debug2("\tCurrent callstack:");
    addr_t rec_addr;
    bool rec = false;
    for (func_list_t::const_iterator cit = callstack.begin(); 
	 cit != callstack.end(); ++cit) {
	debug2(" %.8x (%s@%s)", (*cit)->getAddress(), (*cit)->getName(), 
	       (*cit)->getModule());
	if (*cit == &f) {
	    rec = true;
	}
    }
    debug2(" %.8x (%s@%s)\n", f.getAddress(), f.getName(), f.getModule());
    if (rec) {
	rec_addr = f.getAddress();
	fprintf(stderr, "WARNING: Recursive function call (%.8x), ignoring.\n", rec_addr);
    } else {
	callstack.push_back(&f);
    }
    //assert_msg(!rec, "Recursive function call (%.8x)", f.getAddress());

    // *************************************************************************
    // Create the context for the current function
#define CONTEXT_SENSITIVE_ANALYSIS
#if defined CONTEXT_SENSITIVE_ANALYSIS
#define CONTEXT(...) ContextSensitive(__VA_ARGS__)
#elif defined CONTEXT_INSENSITIVE_ANALYSIS
#define CONTEXT(...) ContextInsensitive(__VA_ARGS__)
#elif defined KCONTEXT_INSENSITIVE_ANALYSIS
#define CONTEXT(...) KContextSensitive(__VA_ARGS__, 3)
#else
#error "Invalid context type"
#endif

    if (!cur_context.get()) {
	assert(callsite == 0);
	cur_context = ContextPtr(new CONTEXT(0));
    } else {
	cur_context = ContextPtr(new CONTEXT(*cur_context, callsite));
    }

    // Create the new context
    cur_ctx_post_states = new bb2state_t();
    cur_ctx_pre_states = new bb2state_t();
    ctx_post_states[cur_context] = cur_ctx_post_states;
    ctx_pre_states[cur_context] = cur_ctx_pre_states;

    debug2("\tCurrent context: %s\n", cur_context->tostring().c_str());

    // Remove self loops to simplify the analysis
    f.getCfg()->removeSelfLoops();
    f.getCfg()->sanityCheck(true);

    // Compute WTO
    f.getCfg()->computeWeakTopologicalOrdering();

    // Decode the function if not decoded yet
    f.getCfg()->decode();

    __enter_function(callsite, f.getName());

    return rec_addr;
}

template<class S, class V, class St>
inline void AbstractInterpreter<S,V,St>::leaveFunction(const Function
        &f, ContextPtr prev_context) {
    (void)f;
    callstack.pop_back();

    // This is necessary for ctx-sensitive analysis, otherwise we
    // quickly run out of memory. For ctx-insensitive and k-sensitive
    // analysis, this memory shouldn't be such a big issue.
    ctx_pre_states.erase(cur_context);
    ctx_post_states.erase(cur_context);

    cur_context = prev_context;
    assert(cur_context.get());
    cur_ctx_pre_states = ctx_pre_states[cur_context];
    cur_ctx_post_states = ctx_post_states[cur_context];
    debug2("\tResumed context: %s\n", cur_context->tostring().c_str());

    __exit_function();
}

} // End of the absinter namespace

#endif // ABSTRACT_INTERPRETER_H

// Local Variables: 
// mode: c++
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
