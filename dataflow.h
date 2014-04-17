#ifndef __DATAFLOW_H__
#define __DATAFLOW_H__

#include "debug.h"
#include "cfg.h"
#include "instr.h"

#include <string>
#include <set>
#include <map>

typedef std::string var_t;
typedef std::set<var_t> var_set_t;
typedef std::pair<var_t, Instruction *> def_t;

std::string def_to_string(def_t vd);

struct defcmp {
  bool operator()(const def_t &d1, const def_t &d2) const {
    debug("%s %s %d\n", def_to_string(d1).c_str(), def_to_string(d2).c_str(),
          strcmp(def_to_string(d1).c_str(), def_to_string(d2).c_str()) < 0);
    return strcmp(def_to_string(d1).c_str(), def_to_string(d2).c_str()) < 0;
  }
};

typedef std::set<def_t> def_set_t;

typedef std::map<BasicBlock *, def_set_t> bb2def_set_t;
typedef std::map<Instruction *, var_set_t> defuse_map_t;
typedef std::map<Instruction *, def_set_t> ud_chain_t;


std::string def_to_string(def_t vd);

std::string def_to_string(def_set_t &vds);

// Compute the set of generated variables (by an instruction)
void computeGeneratedVariables(Instruction *i, defuse_map_t &defmap,
                               def_set_t &defs);

// Compute the set of generated variables (by a basic block)
void computeGeneratedVariables(BasicBlock *bb, defuse_map_t &defmap, 
                               def_set_t &defs);

// Compute the set of generated variables (by the function)
void computeDefinedVariables(Cfg *cfg, defuse_map_t &defmap, 
                             def_set_t &defs);

// Compute the set of killed definitions
void computeKilledVariables(BasicBlock *bb, defuse_map_t &defmap,
                            def_set_t &defs, def_set_t &kills);

// Compute reaching definitions
void computeReachingDef(Cfg *cfg, defuse_map_t &defmap, 
                        bb2def_set_t &reachdefs);

// Compute ud-chain for each instruction of the function
void computeUseDefChain(Cfg *cfg, defuse_map_t &defmap, defuse_map_t &usemap,
                        ud_chain_t &ud_chain);

void computeSlice(Instruction *i, defuse_map_t &defmap, defuse_map_t &usemap,
                  std::set<Instruction *> &slice);
#endif

// Local Variables: 
// mode: c++
// End:
