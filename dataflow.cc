#include "dataflow.h"

std::string def_to_string(def_t vd) {
  char tmp[24];
  sprintf(tmp, "@%.8x", vd.second->getAddress());
  return vd.first + tmp;
}

std::string def_to_string(def_set_t &vds) {
  std::string r = "";

  for (def_set_t::const_iterator it = vds.begin(); it != vds.end(); ++it)
    r += " " + def_to_string(*it);

  return r;
}


// Compute the set of generated variables (by an instruction)
void computeGeneratedVariables(Instruction *i, defuse_map_t &defmap,
                               def_set_t &defs) {
  for (var_set_t::const_iterator it2 = defmap[i].begin();
       it2 != defmap[i].end(); ++it2) {
    def_t d = def_t(*it2, i);
    debug4("Instruction %.8x/%.8x defines %s\n", i->getAddress(), 
           i->getBasicBlock()->getAddress(), def_to_string(d).c_str());
    defs.insert(d);
  }
}


// Compute the set of generated variables (by a basic block)
void computeGeneratedVariables(BasicBlock *bb, defuse_map_t &defmap, 
                             def_set_t &defs) {
  for (instructions_t::const_iterator it = bb->inst_begin();
       it != bb->inst_end(); ++it) {
    computeGeneratedVariables(*it, defmap, defs);
  }
}


// Compute the set of generated variables (by the function)
void computeGeneratedVariables(Cfg *cfg, defuse_map_t &defmap, 
                             def_set_t &defs) {
  for (Cfg::const_bb_iterator it = cfg->bb_begin(); it != cfg->bb_end(); ++it) {
    computeGeneratedVariables(*it, defmap, defs);
  }
}


// Compute the set of killed definitions
void computeKilledVariables(BasicBlock *bb, defuse_map_t &defmap,
                            def_set_t &defs, def_set_t &kills) {
  def_set_t tmp;
  computeGeneratedVariables(bb, defmap, tmp);

  for (def_set_t::const_iterator it = defs.begin(); it != defs.end();
       ++it) {
    for (def_set_t::const_iterator it2 = tmp.begin(); 
         it2 != tmp.end(); ++it2) {
      // Mark as killed all the variables defined elsewhere
      if ((it2->first == it->first) && 
          (it2->second != it->second)) {
        debug4("BasicBlock %.8x kills %s %d\n", bb->getAddress(), 
               def_to_string(*it).c_str(), kills.find(*it) == kills.end());
        kills.insert(*it);
      }
    }
  }
}


// Compute reaching definitions
void computeReachingDef(Cfg *cfg, defuse_map_t &defmap, 
                        bb2def_set_t &reachdefs) {
  bb2def_set_t gen, kill, rcin, rcout;
  def_set_t defs, tmp, live;
  BasicBlock *bb;
  bool stable;
  unsigned int i, j = 0;

  // Compute the set of variables defined by the function
  computeGeneratedVariables(cfg, defmap, defs);

  for (Cfg::const_bb_iterator it = cfg->bb_begin(); it != cfg->bb_end(); ++it) {
    bb = *it;
    
    // Compute the set of variables defined by the bb (this is reduntant)
    tmp.clear();
    computeGeneratedVariables(bb, defmap, tmp);
    gen[bb].insert(tmp.begin(), tmp.end());

    // Compute the set of variables defined by the bb (this is 
    tmp.clear();
    computeKilledVariables(bb, defmap, defs, tmp);
    kill[bb].insert(tmp.begin(), tmp.end());
  }

  stable = false;
  debug3("Computing WTO...\n");
  cfg->computeWeakTopologicalOrdering();
  // Repeat the computation of the reaching definitions until the result 
  // stabilizes
  while (!stable) {
    stable = true;

    debug3("Starting round (%d)...\n", j++);
    // Traverse the basic blocks (in WTO) and compute reaching definitions
    for (Cfg::const_wto_iterator wit = cfg->wto_begin(); wit != cfg->wto_end();
         ++wit) {
      bb = cfg->getVertex(*wit);

      // in = pred[1] + pred[2] + ...
      rcin[bb].clear();
      for (Cfg::const_pred_iterator it = cfg->pred_begin(bb); 
           it != cfg->pred_end(bb); ++it) {
        rcin[bb].insert(rcout[*it].begin(), rcout[*it].end());
      }

      debug3("RCIN[%.8x] %s\n", bb->getAddress(), 
             def_to_string(rcin[bb]).c_str());
      debug3("KILL[%.8x] %s\n", bb->getAddress(), 
             def_to_string(kill[bb]).c_str());

      // live = in - kill
      live.clear();
      for (def_set_t::const_iterator it = rcin[bb].begin(); 
           it != rcin[bb].end(); ++it) {
        if (kill[bb].find(*it) == kill[bb].end()) {
          live.insert(*it);
        } else {
          debug4("Killing %s\n", def_to_string(*it).c_str());
        }
      }

      tmp.insert(rcout[bb].begin(), rcout[bb].end());

      // out = gen | live
      rcout[bb].clear();
      rcout[bb].insert(gen[bb].begin(), gen[bb].end());
      rcout[bb].insert(live.begin(), live.end());
      
      debug3("RCOUT[%.8x] %s\n", bb->getAddress(), 
             def_to_string(rcout[bb]).c_str());
             
      // Mark as unstable if something has changed
      i = tmp.size();
      tmp.insert(rcout[bb].begin(), rcout[bb].end());
      stable &= i == tmp.size();
    }
  }

  debug2("Results stabilized after %d rounds\n", j);

  for (bb2def_set_t::const_iterator it = rcin.begin(); it != rcin.end(); ++it) {
    reachdefs[it->first].insert(it->second.begin(), it->second.end());
  }
}


// Compute ud-chain for each instruction of the function
void computeUseDefChain(Cfg *cfg, defuse_map_t &defmap, defuse_map_t &usemap,
                        ud_chain_t &ud_chain) {
  bb2def_set_t reachdefs;
  std::map<var_t, Instruction *> loc_defs;

  // Compute the reaching definitions
  computeReachingDef(cfg, defmap, reachdefs);
  debug2("Reaching definitions computed for %.8x\n", 
         cfg->getFunction()->getAddress());


  for (bb2def_set_t::const_iterator it = reachdefs.begin(); 
       it != reachdefs.end();++it) {
    def_set_t d = it->second;
    debug3("     %.8x %s\n", it->first->getAddress(), def_to_string(d).c_str());
  }

  debug2("Computing use-def chain\n");

  // Link each instruction with the reaching definitions for the used variables
  for (bb2def_set_t::const_iterator bit = reachdefs.begin(); 
       bit != reachdefs.end(); ++bit) {
    BasicBlock *bb = bit->first;

    loc_defs.clear();
    for (instructions_t::const_iterator iit = bb->inst_begin();
         iit != bb->inst_end(); ++iit) {
      Instruction *i = *iit;

      debug4("Instruction %.8x\n", i->getAddress());
      for (var_set_t::const_iterator uit = usemap[i].begin();
           uit != usemap[i].end(); ++uit) {
        if (loc_defs.find(*uit) != loc_defs.end()) {
          // The variable has been defined locally (this definition kills the
          // reaching one)
          debug4("Using local definition for %s '%s'\n", uit->c_str(), def_to_string(def_t(*uit, loc_defs[*uit])).c_str());
          ud_chain[i].insert(def_t(*uit, loc_defs[*uit]));
        } else {
          // The variables hasn't been defined locally
          for (def_set_t::const_iterator rit = reachdefs[bb].begin();
               rit != reachdefs[bb].end(); ++rit) {
            debug4("Variable %s not killed locally \n", uit->c_str());
            ud_chain[i].insert(*rit);
          }
        }
      }

      // Build a map of locally defined variables (to kill current reaching
      // definitions)
      for (var_set_t::const_iterator dit = defmap[i].begin(); 
           dit != defmap[i].end(); ++dit) {
        
        debug4("Found local definition for %s @ %.8x\n", dit->c_str(), 
               i->getAddress());
        loc_defs[*dit] = i;
      }
    }
  }
}

void computeSlice(Instruction *i, defuse_map_t &defmap, defuse_map_t &usemap,
                  std::set<Instruction *> &slice) {
  Cfg *cfg = i->getBasicBlock()->getCfg();
  Function *func = cfg->getFunction();
  ud_chain_t ud_chain;
  std::list<Instruction *> instrs;
  var_set_t notdone;

  debug2("Slicing %.8x [%.8x %s]\n", i->getAddress(), func->getAddress(), 
         func->getName());

  computeUseDefChain(cfg, defmap, usemap, ud_chain);

  debug2("Use def chain computed\n");

  for (Cfg::const_bb_iterator bit = cfg->bb_begin(); bit != cfg->bb_end();
       ++bit) {
    for (instructions_t::const_iterator iit = (*bit)->inst_begin();
         iit != (*bit)->inst_end(); ++iit) {
      debug3("%.8x: %s\n", (*iit)->getAddress(), 
             def_to_string(ud_chain[*iit]).c_str());
    }
  }

  instrs.push_back(i);

  debug3("Slicing...\n");

  while(!instrs.empty()) {
    Instruction *i = instrs.front();
    instrs.pop_front();

    slice.insert(i);
    
    debug3("  Processing instruction %.8x\n", i->getAddress());

    for (var_set_t::const_iterator it = usemap[i].begin(); 
         it != usemap[i].end(); ++it) {
      notdone.insert(*it);
      debug3("     Looking for definition of %s\n", it->c_str());
      for (def_set_t::const_iterator it2 = ud_chain[i].begin();
           it2 != ud_chain[i].end(); ++it2) {
        if (*it == it2->first) {
          debug3("     Defined by %.8x\n", it2->second->getAddress());
          if (slice.find(it2->second) == slice.end()) {
            debug3("         Adding instruction to the worklist\n");
            instrs.push_back(it2->second);
          } else {
            debug3("         Skipping because already in the slice\n");
          }
          notdone.erase(*it);
        }
      }
    }
  }

  debug2("Slice:");
  for (std::set<Instruction *>::const_iterator it = slice.begin(); 
       it != slice.end(); ++it) {
    debug2(" %.8x", (*it)->getAddress());
  }
  debug2("\n");
  debug2("Pending definitions:");
  for (var_set_t::const_iterator it = notdone.begin(); it != notdone.end();
       ++it) {
    debug2(" %s", it->c_str());
  }
  debug2("\n");
}

// Local Variables: 
// mode: c++
// End:
