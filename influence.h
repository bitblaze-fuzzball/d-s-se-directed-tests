#ifndef __DF_H__
#define __DF_H__

#include "cfg.h"
#include "instr.h"

typedef std::set<Instruction *> instr_set_t;
typedef std::set<BasicBlock *> bb_set_t;
typedef std::map<BasicBlock *, instr_set_t> influence_map_t;

static void depthFirstTraversal(CallGraph *cg, Function *f, 
                         std::set<Function *> &done,
                         std::vector<Function *> &ordering) {
    done.insert(f);

    for (CallGraph::const_succ_iterator it = cg->succ_begin(f);
         it != cg->succ_end(f); ++it) {
        if (done.find(*it) == done.end()) {
            depthFirstTraversal(cg, *it, done, ordering);
        }
    }

    ordering.push_back(f);
}

// Order nodes by finish time
static void computeDFNumbering(CallGraph *cg, std::vector<Function *> &ordering) {
    std::set<Function *> done;

    depthFirstTraversal(cg, cg->getMain(), done, ordering);
}

static void compute_influence(Cfg *cfg, const instr_set_t &slice, 
                       influence_map_t &influence) {
    influence_map_t local_influence;
    instr_set_t total_influence;
    bb_set_t done_once;
    std::list<BasicBlock *> worklist;
    std::map<BasicBlock *, bb_set_t> preds, succs;
    BasicBlock entry;
    Function *func = cfg->getFunction();

    debug("Computing local influence for %.8x %s@%s...\n", func->getAddress(),
          func->getName(), func->getModule());

    for (instr_set_t::const_iterator it = slice.begin(); it != slice.end();
         ++it) {
        local_influence[(*it)->getBasicBlock()].insert(*it);
    }

    debug2("Computing reverse graph...\n");
    for (Cfg::const_bb_iterator it = cfg->bb_begin(); it != cfg->bb_end(); 
         ++it) {
        for (Cfg::const_pred_iterator pit = cfg->pred_begin(*it);
             pit != cfg->pred_end(*it); ++pit) {
            succs[*it].insert(*pit);
        }
        for (Cfg::const_succ_iterator pit = cfg->succ_begin(*it);
             pit != cfg->succ_end(*it); ++pit) {
            preds[*it].insert(*pit);
        }
    }

    debug2("Adding fake entry node...\n");
    for (Cfg::const_exit_iterator eit = cfg->exits_begin(); 
         eit != cfg->exits_end(); ++eit) {
        succs[&entry].insert(*eit);
        preds[*eit].insert(&entry);
    }

    worklist.push_back(&entry);
    while (!worklist.empty()) {
        BasicBlock *bb = worklist.front();
        worklist.pop_front();

        debug2("Processing %.8x\n", bb->getAddress());

        total_influence.clear();

        for (bb_set_t::const_iterator pit = preds[bb].begin();
             pit != preds[bb].end(); ++pit) {
            total_influence.insert(influence[*pit].begin(), 
                                   influence[*pit].end());            
        }

        // the second condition of the if is used because bbs with no
        // instructions in the slice do not modify anything.
        if ((influence[bb].size() != total_influence.size()) || 
            done_once.find(bb) == done_once.end()) {
            influence[bb].insert(total_influence.begin(), 
                                 total_influence.end());
            influence[bb].insert(local_influence[bb].begin(),
                                 local_influence[bb].end());
            if (bb->isCall()) {
                // Add the influence of the callee
                debug2("Processing call targets of %.8x\n", bb->getAddress());
                functions_t::const_iterator fit;
                for (fit = cfg->call_targets_begin(*bb); 
                     fit != cfg->call_targets_end(*bb); ++fit) {
                    Function *f = *fit;
                    debug2("\t%.8x (%s@%s) %d\n", f->getAddress(), 
                          f->getName(), f->getModule(), 
                          influence[f->getCfg()->getEntry()].size());

                    // There are recursive functions!!! Ignore them since all
                    // of them are blacklisted

                    // assert(influence.find(f->getCfg()->getEntry()) !=
                    // influence.end());
                    influence[bb].insert(
                         influence[f->getCfg()->getEntry()].begin(),
                         influence[f->getCfg()->getEntry()].end());
                }
            }
            worklist.insert(worklist.end(), succs[bb].begin(), 
                            succs[bb].end());
            done_once.insert(bb);
        }
    }
}

#endif


// Local Variables: 
// mode: c++
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
