#ifndef VSA_INTERPRETER_H
#define VSA_INTERPRETER_H

#include "AbstractInterpreter.h"
#include "AbsRegion.h"
#include "AbsState.h"
#include "cfg.h"
#include <string>
#include <sstream>

#include "dataflow.h"

namespace absinter {

template <typename T>
class VSAInterpreter : public AbstractInterpreter<VSAInterpreter<T>,
    typename absdomain::ValueSet<T>, typename absdomain::AbsState<T> > {
    typedef VSAInterpreter _Self;

    absdomain::reg::RegEnumTy getRegisterId(const std::string&);

public:
    typedef typename absdomain::ValueSet<T> ValSetTy;
    typedef typename absdomain::AbsState<T> AbsState;
    typedef AbstractInterpreter<_Self,ValSetTy,AbsState> _Base;
    typedef typename boost::intrusive_ptr<ValSetTy> VSetPtr;
    typedef typename _Base::RegionPtr RegionPtr;
    typedef typename boost::intrusive_ptr<AbsState> AbsStatePtr;
    typedef absdomain::Region<T> AbsRegion;
    typedef typename ValSetTy::AlocPair AlocPair;

    void visitMoveInstr(MoveInstr&);
    void visitVarDeclInstr(VarDeclInstr&);
    void visitCallInstr(CallInstr&);
    void visitReturnInstr(ReturnInstr&);

    void visitXallocInstr(Instruction&, const char*);
    void visitStringInstr(Instruction&, const char*);

    VSetPtr visitBinopExpr(BinopExpr&, VSetPtr, VSetPtr);
    VSetPtr visitUnopExpr(UnopExpr&, VSetPtr);
    VSetPtr visitMemExpr(MemExpr&, VSetPtr);
    VSetPtr visitCastExpr(CastExpr&, VSetPtr);
    VSetPtr visitTempExpr(TempExpr&);
    VSetPtr visitConstExpr(ConstExpr&);

    typedef std::pair<addr_t, AlocPair> alocinfo;
    struct alocinfocmp {
        bool operator()(const alocinfo &a1, const alocinfo &a2) const {
            // Compare the address
            if (a1.first != a2.first)
                return a1.first < a2.first;

            AlocPair ap1 = a1.second, ap2 = a2.second;

            // Compare the regions
            if (ap1.first->getId() != ap2.first->getId())
                return ap1.first->getId() < ap2.first->getId();
            // Compare the SI
            if (ap1.second->getStride() != ap2.second->getStride())
                return ap1.second->getStride() < ap2.second->getStride();
            if (ap1.second->getLo() != ap2.second->getLo())
                return ap1.second->getLo() < ap2.second->getLo();
            if (ap1.second->getHi() != ap2.second->getHi())
                return ap1.second->getHi() < ap2.second->getHi();

            return 0;
        }
    };
    

    typedef std::map<std::string, std::set<Instruction *> >  defmap_t;

private:
    defuse_map_t defines;
    defuse_map_t uses;
    defmap_t    definitions;

    bool visiting_lhs;

    // _Base::TempMapTy temp_exps;

    // #define aloc2str(x) "R" << (x).first->getId() << ":" << *((x).second)
    // #define alocinfo2str(x) std::hex << (x.first) << std::dec << "|R" << (x.second).first->getId() << ":" << *((x.second).second)

    std::string aloc2str(AlocPair a) {
        if (a.first->isRegister()) {
            return absdomain::getRegNameAtAddress(a.second->getLo(), 
                                                  a.second->getHi());
        } else {
            std::stringstream s;
            s << "R" << a.first->getId() << ":" << *(a.second);
            return s.str();
        }
    }

#define TRACK_USE_DEF
#ifdef TRACK_USE_DEF

    void define(absdomain::reg::RegEnumTy reg) {
        if (reg == absdomain::reg::ESP_REG || reg == absdomain::reg::EBP_REG ||
            reg == absdomain::reg::DF_REG || reg == absdomain::reg::AF_REG ||
            reg == absdomain::reg::PF_REG || reg == absdomain::reg::ZF_REG ||
            reg == absdomain::reg::CF_REG || reg == absdomain::reg::SF_REG ||
            reg == absdomain::reg::OF_REG)
            return;

        std::string aloc = absdomain::getRegNameAtAddress(
              AbsState::getIntervalFor(reg)->getLo(), 
              AbsState::getIntervalFor(reg)->getHi());

        debug3("Instruction %.8x defines %s\n", _Base::cur_instr->getAddress(),
               aloc.c_str());
        definitions[aloc].insert(_Base::cur_instr);
        defines[_Base::cur_instr].insert(aloc);
    }

    void define(VSetPtr vset) {
        for (typename ValSetTy::const_iterator it = vset->begin(); 
             it != vset->end(); ++it) {
            std::string aloc = aloc2str(it->get());
            debug3("Instruction %.8x defines %s\n", 
                   _Base::cur_instr->getAddress(),
                   aloc.c_str());
            definitions[aloc].insert(_Base::cur_instr);
            defines[_Base::cur_instr].insert(aloc);
        }
    }

    void use(VSetPtr vset) {
        for (typename ValSetTy::const_iterator it = vset->begin(); it !=
                vset->end(); ++it) {
            std::string aloc = aloc2str(it->get());
	    
	    if (!visiting_lhs) {
                debug3("Instruction %.8x uses %s\n", 
                       _Base::cur_instr->getAddress(),
                       aloc.c_str());
		uses[_Base::cur_instr].insert(aloc);
		assert(uses.find(_Base::cur_instr) != uses.end());
	    }
        }
    }

    void use(absdomain::reg::RegEnumTy reg) {
        if (reg == absdomain::reg::ESP_REG || reg == absdomain::reg::EBP_REG ||
            reg == absdomain::reg::DF_REG || reg == absdomain::reg::AF_REG ||
            reg == absdomain::reg::PF_REG || reg == absdomain::reg::ZF_REG ||
            reg == absdomain::reg::CF_REG || reg == absdomain::reg::SF_REG ||
            reg == absdomain::reg::OF_REG)
            return;

        std::string aloc = absdomain::getRegNameAtAddress(
              AbsState::getIntervalFor(reg)->getLo(), 
              AbsState::getIntervalFor(reg)->getHi());

	if (!visiting_lhs) {
            debug3("Instruction %.8x uses %s\n", _Base::cur_instr->getAddress(),
                   aloc.c_str());
	    uses[_Base::cur_instr].insert(aloc);
	    assert(uses.find(_Base::cur_instr) != uses.end());
	}
    }
#else
#define use(x) 
#define define(x)
#endif

public:
    void dumpUseDef(Function *func) {
        Cfg *cfg = func->getCfg();
        char tmp[16];
        std::set<Instruction *> ii;

        for (Cfg::const_bb_iterator bit = cfg->bb_begin(); 
             bit != cfg->bb_end(); ++bit) {
            for (instructions_t::const_iterator iit = (*bit)->inst_begin();
                 iit != (*bit)->inst_end(); ++iit) {
                ii.insert(*iit);
            }
        }

        debug3("@@@@@@@@@@@@@@@@@@@ %.8x @@@@@@@@@@@@@@@@@@\n", func->getAddress());

        debug3("==================== USES ===================\n");

        for (defuse_map_t::const_iterator it = uses.begin(); it != uses.end(); 
             ++it) {
            if (ii.find(it->first) == ii.end())
                continue;

            debug3("%.8x|%.8x\t", it->first->getAddress(), 
                   it->first->getBasicBlock()->getAddress());

            for (var_set_t::const_iterator it2 = it->second.begin();
                 it2 != it->second.end(); ++it2) {
                debug3("%s ", it2->c_str());
            }
            debug3("\n");
        }

        debug3("\n");
        debug3("\n");

        debug3("==================== DEFS ===================\n");

        for (defuse_map_t::const_iterator it = defines.begin(); 
             it != defines.end(); ++it) {
            if (ii.find(it->first) == ii.end())
                continue;

            debug3("%.8x|%.8x\t", it->first->getAddress(),
                   it->first->getBasicBlock()->getAddress());

            for (var_set_t::const_iterator it2 = it->second.begin();
                 it2 != it->second.end(); ++it2) {
                debug3("%s ", it2->c_str());
            }
            debug3("\n");
        }

        debug3("\n");
        debug3("\n");

#if 0
        std::cerr << "==================== DEFINITIONS ===================" << std::endl;
        for (typename defmap_t::const_iterator it = definitions.begin(); 
             it != definitions.end(); 
             ++it) {   
            std::cerr << aloc2str(it->first) << "\t";

            for (std::set<Instruction *>::const_iterator it2 = 
                     it->second.begin(); it2 != it->second.end(); ++it2) {
                std::cerr << " " << std::hex << (*it2)->getAddress() << std::dec;
            }
            std::cerr << std::endl;
        }

        std::cerr << "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" << std::endl;
#endif
    }


    void slice(Instruction *start, std::set<Instruction *> &instrs) {
        std::list<std::string> wlist;
        std::set<std::string> done;

        wlist.insert(wlist.begin(), uses[start].begin(), uses[start].end());
        instrs.insert(start);

        while (!wlist.empty()) {
            std::string aloc = wlist.front();
            wlist.pop_front();

            debug3("Searching for definitions of %s\n", aloc.c_str());

            typename std::set<Instruction *>::const_iterator it;
            if (definitions.find(aloc) != definitions.end()) {
                for (it = definitions[aloc].begin(); it != 
                         definitions[aloc].end(); ++it) {
                    debug3("\tdefined by %.8x\n\t\t", (*it)->getAddress());

                    // Put in the slice the instruction
                    instrs.insert(*it);

                    typename std::set<std::string>::const_iterator it2;
                    if (uses.find(*it) != uses.end()) {
                        for (it2 = uses[*it].begin(); it2 != uses[*it].end(); 
                             ++it2) {
                            debug3(" uses %s", it2->c_str());
                            if (done.find(*it2) == done.end()) {
                                wlist.push_back(*it2);
                                done.insert(*it2);
                                debug3(" (added to the worklist)");
                            } 
                        }
                    }

                    debug3("\n");
                }
            }
        }
    }

    void slice2(Instruction *start, std::set<Instruction *> &instrs) {
        computeSlice(start, defines, uses, instrs);
    }

#if 0
    struct expcmp {
        bool operator()(const Expression *e1, const Expression *e2) const {
            return e1->tostring() < e2->tostring();
        };
    };

    typedef std::set<Expression *, expcmp> expset;
    typedef std::map<std::string, expset> tmpexpmap;

    void getUse(const Expression *e, expset &use, tmpexpmap tmp) {
        switch (e->exp_type) {
        case vine::TEMP: {
            absdomain::reg::RegEnumTy rty;
            rty = getRegisterId(static_cast<TempExpr *>(e)->name);
            if (rty != absdomain::reg::TERM_REG) {
                use.insert(e);
            } else {
                use.insert(tmp[e->tostring()].begin(), 
                           tmp[e->tostring()].end());
            }
            break;
        }

        case vine::MEM: {
            MemExpr *m = static_cast<MemExpr *>(e);
            expset u;

            getUse(m->addr, u, tmp);
            use.insert(u.begin(), u.end());
            break;
        }

        case vine::UNOP: {
            UnopExpr *un = static_cast<UnopExpr *>(e);
            expset u;

            getUse(un->exp, u, tmp);
            use.insert(u.begin(), u.end());

            break;
        }

        case vine::BINOP: {
            BinopExpr *b = static_cast<BinopExpr *>(e);
            expset u;

            getUse(b->rhs, u, tmp);
            use.insert(u.begin(), u.end());
            u.clear();
            getUse(b->lhs, u, tmp);
            use.insert(u.begin(), u.end());

            break;
        }

        case vine::CAST: {
            CastExpr *c = static_cast<CastExpr *>(e);
            expset u;

            getUse(c->exp, u, tmp);
            use.insert(u.begin(), u.end());

            break;
        }

        case vine::CONSTANT:
            break;

        default:
            assert(0);
        }
    }

    
    void getUse(vine::Stmt *i, tmpexpmap &tmp) {
        if (i->stmt_type == vine::MOVE) {
            Expression *rhs = static_cast<MoveInstr *>(i)->rhs,
                *lhs = static_cast<MoveInstr *>(i)->lhs;
            expset use;

            debug3("Getting 'use' for %s\n", i->tostring().c_str());

            getUse(rhs, use, tmp);
            if (lhs->exp_type == vine::TEMP) {
                debug3("Storing use for '%s'\n", lhs->tostring().c_str());
                tmp[lhs->tostring()].insert(use.begin(), use.end());
            }
        }
    }

    // Given a store instruction (e.g., mov %ebx,(%eax)) try to guess the aloc
    // representing the pointer
    void guessIndex(Instruction *i, std::set<AlocPair, aloccmp> &alocs) {
        MoveInstr *store = NULL;
        TempExpr *addr = NULL;
        tmpexpmap tmp;

        for(statements_t::const_reverse_iterator rit = i->stmt_rbegin();
            rit != i->stmt_rend(); rit++) {
            if ((*rit)->stmt_type == vine::MOVE) {
                store = static_cast<MoveInstr *>(*rit);
                assert(store->lhs->exp_type == vine::MEM);
                assert(static_cast<MemExpr *>(store->lhs)->addr->exp_type == vine::TEMP);
                addr = static_cast<TempExpr *>(static_cast<MemExpr *>(store->lhs)->addr);
                break;
            }
        }
        assert(store);

        for (statements_t::const_iterator it = i->stmt_begin(); 
             it != i->stmt_end(); ++it) {
            if (*it != store) {
                getUse(*it, tmp);
            }
        }

        for (typename tmpexpmap::const_iterator it = tmp.begin(); 
             it != tmp.end(); ++it) {
            debug3(" %s -->", it->first.c_str());
            for (typename expset::const_iterator it2 = it->second.begin(); 
                 it2 != it->second.end(); ++it2) {
                debug3(" %s", (*it2)->tostring().c_str());
            }
            debug3("\n");
        }

        debug3("Looking for addr %s...\n", addr->tostring().c_str());
        assert(tmp.find(addr->tostring()) != tmp.end());
        for (typename expset::const_iterator it = tmp[addr->tostring()].begin();
             it != tmp[addr->tostring()].end(); ++it) {
            assert((*it)->exp_type == vine::TEMP);
            absdomain::reg::RegEnumTy rty;
            rty = getRegisterId(static_cast<TempExpr *>(*it)->name);
            assert(rty != absdomain::reg::TERM_REG);
            AlocPair aloc = std::make_pair(_Base::cur_state->getRegister(),
                                           AbsState::getIntervalFor(rty)); 
            if (DEBUG_LEVEL >= 3) {
                std::cerr << "ALOC: " << aloc2str(aloc) << std::endl;
            }
            alocs.insert(aloc);
        }
    }

    void sliceWithBounds(Instruction *start, std::set<Instruction *> &bounds, 
                         std::set<Instruction *> &inslice) {
        std::list<AlocPair> wlist;
        std::set<AlocPair, aloccmp> done, ptrs;

	debug3("Bounds for slice (%d):", bounds.size());
	for (std::set<Instruction *>::const_iterator iit = bounds.begin(); 
	     iit != bounds.end(); ++iit) {
	    debug3(" %.8x", (*iit)->getAddress());
	}
	debug3("\n");

	for (std::set<Instruction *>::const_iterator it = bounds.begin(); 
	     it != bounds.end(); ++it) {
	    debug3("%.8x defines", (*it)->getAddress());
            typename std::set<AlocPair, aloccmp>::const_iterator it2;
            for (it2 = defines[*it].begin(); it2 != defines[*it].end(); ++it2) {
                if (DEBUG_LEVEL >= 3) {
                    std::cerr << " " << aloc2str(*it2);
                }
            }

            if (DEBUG_LEVEL >= 3) {
                std::cerr << std::endl;
            }
	}

        guessIndex(start, ptrs);
        wlist.insert(wlist.begin(), ptrs.begin(), ptrs.end());
        inslice.insert(start);

        while (!wlist.empty()) {
            AlocPair aloc = wlist.front();
            wlist.pop_front();

	    if (DEBUG_LEVEL >= 3) {
		std::cerr << "Processing aloc: " << aloc2str(aloc) << std::endl;
	    }

            // Process all instructions that define the current aloc
            typename std::set<Instruction *, instrcmp>::const_iterator it;
            if (definitions.find(aloc) != definitions.end()) {
                for (it = definitions[aloc].begin(); it != 
                         definitions[aloc].end(); ++it) {
                    // Consider only a subset of the instructions
                    if (bounds.find(*it) != bounds.end()) {
                        inslice.insert(*it);
         
                        typename std::set<AlocPair, aloccmp>::const_iterator it2;
                        if (uses.find(*it) != uses.end()) {
                            for (it2 = uses[*it].begin(); 
                                 it2 != uses[*it].end(); ++it2) {
                                // Put in the worklist all the alocs used by
                                // the current instruction and not yet
                                // processed
                                if (done.find(*it2) == done.end()) {

                                    wlist.push_back(*it2);
                                    done.insert(*it2);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    void dirtySlice(Instruction *start, std::set<Instruction *> &instrs) {
        std::list<alocinfo> wlist;
        std::set<alocinfo, alocinfocmp> done;
        typename std::set<AlocPair, aloccmp>::const_iterator uit;
        typename std::set<Instruction *, instrcmp>::const_iterator dit;

#define FUNCADDR(x) ((x)->getBasicBlock()->getCfg()->getFunction()->getAddress())
#define ISREGISTER(x) ((x).first->isRegister())

        
        if (DEBUG_LEVEL >= 3)
            std::cerr << "Warning uses";

        for (uit = uses[start].begin(); uit != uses[start].end(); ++uit) {
            alocinfo ai;
            if (ISREGISTER(*uit)) {
                ai = alocinfo(FUNCADDR(start), *uit);
            } else {
                ai = alocinfo(0, *uit);
            }

            if (DEBUG_LEVEL >= 3)
                std::cerr << " " << alocinfo2str(ai);
            wlist.push_back(ai);            
        }
        if (DEBUG_LEVEL >= 3)
            std::cerr << std::endl;

        instrs.insert(start);

        while (!wlist.empty()) {
            alocinfo ai = wlist.front();
            wlist.pop_front();

            if (DEBUG_LEVEL >= 3)
                std::cerr << "Searching for definitions of " << 
                    alocinfo2str(ai)
                          << std::endl;

            if (definitions.find(ai.second) != definitions.end()) {
                for (dit = definitions[ai.second].begin(); dit != 
                         definitions[ai.second].end(); ++dit) {

                    if (ISREGISTER(ai.second) && ai.first != FUNCADDR(*dit)) {
                        if (DEBUG_LEVEL >= 3) {
                            std::cerr << "\t [IGNORED] defined by " << 
                                std::hex << (*dit)->getAddress() << 
                                                      std::dec << std::endl;
                        }
                        continue;
                    }

                    if (DEBUG_LEVEL >= 3) {
                        std::cerr << "\tdefined by " << std::hex << 
                            (*dit)->getAddress() << std::dec << std::endl;
                        std::cerr << "\t\tuses";
                    }

                    // Put in the slice the instruction
                    instrs.insert(*dit);

                    typename std::set<AlocPair, aloccmp>::const_iterator it2;
                    if (uses.find(*dit) != uses.end()) {
                        for (uit = uses[*dit].begin(); uit != uses[*dit].end(); 
                             ++uit) {
                            alocinfo n;
                            if (ISREGISTER(*uit)) {
                                n = alocinfo(FUNCADDR(*dit), *uit);
                            } else {
                                n = alocinfo(0, *uit);
                            }
                                
                            if (DEBUG_LEVEL >= 3)
                                std::cerr << " " << alocinfo2str(n);

                            if (done.find(n) == done.end()) {
                                wlist.push_back(n);
                                done.insert(n);

                                if (DEBUG_LEVEL >= 3)
                                    std::cerr << " (added to the worklist)";
                            } 
                        }
                    }

                    if (DEBUG_LEVEL >= 3)
                        std::cerr << std::endl;
                }
            }
        }
    }
#undef FUNCADDR
#endif
};

    
template <class T>
absdomain::reg::RegEnumTy VSAInterpreter<T>::getRegisterId(const
        std::string& s) {

    if (s.length() > 2 && s.substr(0,2) == "R_") {
        const std::string::size_type idx = s.find(':');
        const struct absdomain::regs* r = 0;
        if (idx == std::string::npos) {
            const std::string nm = s.substr(2);
            if (nm == "DFLAG") {
                r = absdomain::regLookup("DF");
            } else {
                r = absdomain::regLookup(s.substr(2).c_str());
            }
        } else {
            // This case processes the vine "name:type" result of
            // vine::Exp::tostring(). tostring is fairly inefficient,
            // use Exp::name instead.
            r = absdomain::regLookup(s.substr(2, s.length() - 2 -
                        idx).c_str());
        }
        if (r) {
            return (absdomain::reg::RegEnumTy)r->id;
        }
    }
    return absdomain::reg::TERM_REG;
}

template <class T>
void VSAInterpreter<T>::visitMoveInstr(MoveInstr &m) {
    Expression *rhs = m.rhs, *lhs = m.lhs;

    visiting_lhs = false;
    // Interpret rhs
    VSetPtr abs_rhs = _Base::visit(*rhs);
    assert(abs_rhs.get() && "Right hand side is undefined.");
    
    switch(lhs->exp_type) {
    case vine::TEMP:
        {
        TempExpr &temp = static_cast<TempExpr &>(*lhs);
        // Map the string representing into the corresponding register, if the
        // lookup fails expression represents a temporary variable
    const absdomain::reg::RegEnumTy rty = getRegisterId(temp.name);

        if (rty == absdomain::reg::TERM_REG) {
            // Temporary variable

            assert_msg((_Base::temps.find(temp.name) != _Base::temps.end() ||
                        temp.name == "R_CC_OP" ||
                        temp.name == "R_CC_DEP1" ||
                        temp.name == "R_CC_DEP2" ||
                        temp.name == "R_CC_NDEP" ||
                        temp.name == "R_IDFLAG" ||
                        temp.name == "R_ACFLAG" ||
                        temp.name == "EFLAGSREST" ||
                        temp.name == "R_EMWARN"),
                       "Found an undeclared temporary '%s'.", 
                       temp.name.c_str());

        // Don't update the temporaries for Vine's garbage (above)
        if (_Base::temps.find(temp.name) != _Base::temps.end()) {
            _Base::temps[temp.name] = abs_rhs;
	    // temp_exps[temp.name] = 
        }
        } else {
            // Real register
        assert(!_Base::cur_state->empty() && "Uninitialized state?");
        //std::cerr << "MOV:: " << rty << " = " << *abs_rhs << std::endl;
        define(rty);
        _Base::cur_state = _Base::cur_state->write(rty, abs_rhs);
        assert(!_Base::cur_state->empty() && "Uninitialized state?");
        }
        }
        break;
    case vine::MEM: {
        MemExpr &mem = static_cast<MemExpr &>(*lhs);
        visiting_lhs = true;
        VSetPtr abs_addr = _Base::visit(mem.addr);
        visiting_lhs = false;
        assert(!_Base::cur_state->empty() && "Uninitialized state?");
        //std::cerr << "MOV:: " << *abs_addr << " = " << *abs_rhs << std::endl;
        define(abs_addr);
        _Base::cur_state = _Base::cur_state->write(abs_addr, abs_rhs);
        assert(!_Base::cur_state->empty() && "Uninitialized state?");
    } break;
    default:
        assert(0 && "Invalid lhs.");
    }

}

template <class T>
void VSAInterpreter<T>::visitVarDeclInstr(VarDeclInstr &vd) {
    // Just flag the declared variable as temporary
    _Base::temps[vd.name] = VSetPtr();
    // temp_exps[vd.name] = NULL;
}

template <class T>
void VSAInterpreter<T>::visitCallInstr(CallInstr &c) {
    ;
}

template <class T>
void VSAInterpreter<T>::visitReturnInstr(ReturnInstr &r) {
    (void)r;
    // define(absdomain::reg::EAX_REG);
}

template <class T>
void VSAInterpreter<T>::visitXallocInstr(Instruction &i, const char *a) {
    VSetPtr vs_size;
    VSetPtr esp = _Base::cur_state->read(absdomain::reg::ESP_REG);

    debug2("Call to '%s' detected @ %.8x\n", a, i.getAddress());
    if (strcmp(a, "realloc") == 0) {
        // 2nd argument is the size
        vs_size = _Base::cur_state->read(*esp + *ValSetTy::get(4,4));
    } else if (strcmp(a, "calloc") == 0) {
        // 1st * 2nd argument is the size
        VSetPtr size1, size2;
        size1 = _Base::cur_state->read(esp);
        size2 = _Base::cur_state->read(*esp + *ValSetTy::get(4,4));
        vs_size = *size1 * *size2;
    } else {
        // 1st argument is the size
        vs_size = _Base::cur_state->read(esp);
    }

    bool e = false;
    BasicBlock *bb = _Base::cur_bb;
    for (typename ValSetTy::const_iterator I = vs_size->begin(), E =
            vs_size->end(); I !=E; ++I){
        assert(!e && "Size is not a singleton valueset");

        debug2("@@@@ Heap object (%.8x) size is... ", i.getAddress());
        if (DEBUG_LEVEL >= 2) {
            std::cerr << *(I->get().second) << std::endl;
        }

        if (I->get().second->isTop()) {
            WARNING("*** Unbounded malloc");
        }

    RegionPtr obj;

        if (_Base::heap_objects.find(bb) == _Base::heap_objects.end()) {

            // This is the first time we see this alloc-site
        const int id = AbsRegion::getNextId();
            _Base::cur_state = _Base::cur_state->addNewRegion(
                                      absdomain::rg::STRONG_HEAP, 
                                      I->get().second);
            obj = _Base::cur_state->find(id);
        assert(obj.get() && "Invalid region.");
            debug2("Heap (strong) region succesfully created\n");
            _Base::heap_objects[bb] = obj;

        } else {
            // We have already allocated this heap region. We have to make it
            // weakly updatable
            RegionPtr obj0 = _Base::heap_objects[bb];
        assert(obj0.get() && "Invalid region found.");
            if (obj0->isStronglyUpdatable()) {
                const int id = AbsRegion::getNextId();
                _Base::cur_state = _Base::cur_state->addNewRegion(
                        absdomain::rg::STRONG_HEAP, I->get().second);
                obj = _Base::cur_state->find(id);
                assert(obj.get() && "Invalid region.");
                obj = obj->getWeaklyUpdatable();
                assert(obj.get() && "Invalid region.");
                debug2("Heap region succesfully transformed from "
                    "strongly to weakly updatable\n");
                _Base::heap_objects[bb] = obj;
            } else {
                obj = obj0;
                debug2("Heap object is already weakly updatable");
            }
        }
        
        _Base::cur_state = _Base::cur_state->write(absdomain::reg::EAX_REG,
                                                   ValSetTy::get(std::make_pair(obj, T::get(0,4))));
        e = true;
    }
}

template <class T>
void VSAInterpreter<T>::visitStringInstr(Instruction &i, const char *a) {
    // TODO: implement warnings, at perhaps summarization.
    debug2("Call to '%s' detected @ %.8x\n", a, i.getAddress());
    if (strcmp(a, "strcat") == 0) {
    } else if (strcmp(a, "memcpy") == 0) {
    } else if (strcmp(a, "strcpy") == 0) {
    } else if (strcmp(a, "sprintf") == 0) {
    } else if (strcmp(a, "strncpy") == 0) {
    } else {
        assert(false && "isStringFun recongizes more than visitStrI?");
    }
}

template <class T>
typename VSAInterpreter<T>::VSetPtr
VSAInterpreter<T>::visitBinopExpr(BinopExpr& E, VSetPtr L, VSetPtr R) {
    VSetPtr res;
#ifndef NDEBUG
    std::stringstream ss;
    ss << *L << ' ';
#endif
    switch (E.binop_type) {
        case vine::PLUS:
#ifndef NDEBUG
            ss << '+';
#endif
            res =  *L + *R;
            break;
        case vine::MINUS:
#ifndef NDEBUG
            ss << '-';
#endif
            res = *L - *R;
            break;
        case vine::TIMES:
#ifndef NDEBUG
            ss << '*';
#endif
            res =  *L * *R;
            break;
        case vine::BITAND:
#ifndef NDEBUG
            ss << '&';
#endif
            res =  *L & *R;
            break;
        case vine::BITOR:
#ifndef NDEBUG
            ss << '|';
#endif
            res =  *L | *R;
            break;
        case vine::XOR:
#ifndef NDEBUG
            ss << '^';
#endif
            res =  *L ^ *R;
            break;
        case vine::LROTATE:
#ifndef NDEBUG
            ss << "lrot";
#endif
            res =  L->lrotate(*R);
            break;
        case vine::RROTATE:
#ifndef NDEBUG
            ss << "rrot";
#endif
            res =  L->rrotate(*R);
            break;
        case vine::DIVIDE:
#ifndef NDEBUG
            ss << "/u";
#endif
            res =  L->udivide(*R);
            break;
        case vine::SDIVIDE:
#ifndef NDEBUG
            ss << "/";
#endif
            res =  L->sdivide(*R);
            break;
        case vine::MOD:
#ifndef NDEBUG
            ss << "%u";
#endif
            res =  L->umod(*R);
            break;
        case vine::SMOD:
#ifndef NDEBUG
            ss << '%';
#endif
            res =  L->smod(*R);
            break;
        case vine::LSHIFT:
#ifndef NDEBUG
            ss << "<<";
#endif
            res =  L->lshift(*R);
            break;
        case vine::RSHIFT:
#ifndef NDEBUG
            ss << ">>l";
#endif
            res =  L->rshift(*R);
            break;
        case vine::ARSHIFT:
#ifndef NDEBUG
            ss << ">>a";
#endif
            res =  L->arshift(*R);
            break;
        case vine::EQ:
#ifndef NDEBUG
            ss << "==";
#endif
            res =  ValSetTy::get((int)L->ai_equal(*R));
            break;
        case vine::NEQ:
#ifndef NDEBUG
            ss << "!=";
#endif
            res =  ValSetTy::get((int)L->ai_distinct(*R));
            break;
        case vine::LT:
#ifndef NDEBUG
            ss << "<";
#endif
            {
                // Comparison on sets is not a total order
                const bool lt = *L < *R;
                const bool gte = *L >= *R;
                const int val = (int)(lt && !gte);
                res =  ValSetTy::get(val);
            }
            break;
        case vine::LE:
#ifndef NDEBUG
            ss << "<=";
#endif
            {
                // Comparison on sets is not a total order
                const bool lte = *L <= *R;
                const bool gt = *L > *R;
                const int val = (int)(lte && !gt);
                res =  ValSetTy::get(val);
            }
            break;
        default:
            assert_msg(false, "Missing binary op (%d).", E.binop_type);
            break;
    }
#ifndef NDEBUG
    ss << ' ' << *R << " = " << *res << std::endl;
    debug3("%s", ss.str().c_str());
#endif
    char toContinue;
    /*
    std::cin >> toContinue;
    if (toContinue == 'y') {
        return res;
    } else {
        exit(EXIT_SUCCESS);
    }// */
    return res;
}

template <class T>
typename VSAInterpreter<T>::VSetPtr
VSAInterpreter<T>::visitUnopExpr(UnopExpr& E, VSetPtr O) {
    switch (E.unop_type) {
        case vine::NOT:
            return ~*O;
        default:
            assert(false && "Missing unary op.");
            return VSetPtr();
    }
}

template <class T>
typename VSAInterpreter<T>::VSetPtr
VSAInterpreter<T>::visitMemExpr(MemExpr& E, VSetPtr adr) {
    assert(!_Base::cur_state->empty() && "Uninitialized state?");
    (void)E;
    if (DEBUG_LEVEL >= 2) {
        std::cerr << "mem addr " << *adr << std::endl;
    }
    VSetPtr r = _Base::cur_state->read(adr);
    use(adr);
    use(r);
    return r;
}

inline int getSize(vine::reg_t ty) {
    switch (ty) {
        case vine::REG_1:   return  1;
        case vine::REG_8:   return  8;
        case vine::REG_16:  return 16;
        case vine::REG_32:  return 32;
        case vine::REG_64:  return 64;
        default:
            assert_msg(false, "Unsupported type %d.", ty);
            return 0;
    }
}

template <class T>
typename VSAInterpreter<T>::VSetPtr
VSAInterpreter<T>::visitCastExpr(CastExpr& E, VSetPtr v) {
    // The oldSize is approximate, as vine doesn't provide size of the
    // casted expression (like Binop).
    const unsigned oldSize = sizeof(int)*CHAR_BIT;
    const unsigned newSize = getSize(E.typ);
    const unsigned expectedStride = newSize >= 8 ? newSize / CHAR_BIT : 1;

    if (oldSize <= newSize) {
        return v;
    }

    const unsigned mask = (1 << newSize) - 1;
    VSetPtr res;

    switch (E.cast_type) {
        case vine::CAST_HIGH:
            if (newSize < oldSize) {
                res = (*v & *ValSetTy::get(~mask))->rshift(
                    *ValSetTy::get(oldSize - newSize, expectedStride));
                break;
            }
        case vine::CAST_LOW:
            if (newSize < oldSize) {
                res = *v & *ValSetTy::get(mask, expectedStride);
            }
            break;
        case vine::CAST_UNSIGNED:
        case vine::CAST_SIGNED: 
            assert(newSize < oldSize && "Downcast expected.");
            res = *v & *ValSetTy::get(mask, expectedStride);
            break;
        default:
            assert_msg(false, "Unimplemented cast %d\n",
                    E.cast_type);
            break;
    }

    //std::cout << "Casting " << *v << " to size " << newSize << " = " <<
    //    *res << std::endl;
    return res;
}

template <class T>
typename VSAInterpreter<T>::VSetPtr
VSAInterpreter<T>::visitTempExpr(TempExpr& E) {
    const absdomain::reg::RegEnumTy rty = getRegisterId(E.name);

    if (rty != absdomain::reg::TERM_REG) {
        assert(!_Base::cur_state->empty() && "Uninitialized state?");
        use(rty);
        return _Base::cur_state->read(rty);
    } 
    
    typename _Base::TempMapTy::iterator FI = _Base::temps.find(E.name);
    
    if (FI != _Base::temps.end()) {
        VSetPtr vsp = FI->second;
        assert_msg(vsp.get(), "Undefined temporary %s", E.name.c_str());
	// use(temp_exps[E.name]);
        return vsp;
    } else {
        assert_msg((E.name == "R_CC_OP" ||
                    E.name == "R_CC_DEP1" ||
                    E.name == "R_CC_DEP2" ||
                    E.name == "R_CC_NDEP" ||
                    E.name == "R_IDFLAG" ||
                    E.name == "R_ACFLAG" ||
                    E.name == "EFLAGSREST" ||
                    E.name == "R_EMWARN"),
                    "Found an undeclared temporary '%s'.", E.name.c_str());
        return ValSetTy::getTop();
    }
}

template <class T>
typename VSAInterpreter<T>::VSetPtr
VSAInterpreter<T>::visitConstExpr(ConstExpr& E) {
    switch (E.typ) {
        case vine::REG_1:
        case vine::REG_8:
            return ValSetTy::get(E.val);
        case vine::REG_16:
            return ValSetTy::get(E.val, 2);
        case vine::REG_32:
            return ValSetTy::get(E.val, 4);
        case vine::REG_64:
            assert(sizeof(int)*CHAR_BIT == 64 &&
            "Recompile on a 64-bit machine.");
            break;
        default:
            assert(false && "Unsupported type.");
            break;
    }
    return ValSetTy::getTop();
}

} // End of the absinter namespace

#endif // VSA_INTERPRETER_H


// Local Variables: 
// mode: c++
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
