#include "cfgs_for_ocaml.h"
#include "warning.h"

int DEBUG_LEVEL = 0;
FILE *DEBUG_FILE = stderr;

program_info *program_info_from_file(const char *fname) {
    program_info *pi = new program_info();
    unserialize(fname, pi->prog, pi->callgraph, pi->functions);
    pi->fname = strdup(fname);
    return pi;
}

void free_program_info(program_info *pi) {
    delete pi;
}

InterProcCFG *construct_interproc_cfg(program_info *pi) {
    // return new InterProcCFG(pi->fname);
    return build_ipcfg(pi->fname, pi->functions);
}

void free_interproc_cfg(InterProcCFG *ipcfg) {
    delete ipcfg;
}

void interproc_cfg_set_target_addr(InterProcCFG *ipcfg, addr_t addr,
				   program_info *pi) {
    ipcfg->set_target_addr(addr, pi->functions);
}

void interproc_cfg_compute_shortest_paths(InterProcCFG *ipcfg) {
    ipcfg->compute_shortest_paths();
}

void interproc_cfg_add_target_addr(InterProcCFG *ipcfg, addr_t addr,
				   program_info *pi) {
    ipcfg->add_target_addr(addr, pi->functions);
}

long long interproc_cfg_lookup_distance(InterProcCFG *ipcfg, addr_t addr,
					program_info *pi) {
    return ipcfg->lookup_distance(addr, pi->functions);
}

long long interproc_cfg_lookup_multi_distance(InterProcCFG *ipcfg,
					      addr_t from_addr, addr_t to_addr,
					      program_info *pi) {
    return ipcfg->lookup_multi_distance(from_addr, to_addr, pi->functions);
}

int interproc_cfg_lookup_influence(InterProcCFG *ipcfg, addr_t addr,
				   program_info *pi) {
    return ipcfg->lookup_influence(addr, pi->functions, &pi->influence);
}

long interproc_cfg_lookup_pth(InterProcCFG *ipcfg, addr_t addr,
			      program_info *pi) {
    return ipcfg->lookup_pth(addr, pi->functions);
}

bool_for_camlidl
interproc_cfg_is_loop_exit_cond(InterProcCFG *ipcfg, addr_t addr,
				program_info *pi) {
    return ipcfg->is_loop_exit_cond(addr, pi->functions);
}

bool_for_camlidl
interproc_cfg_edge_is_loop_exit(InterProcCFG *ipcfg,
				addr_t from_addr, addr_t to_addr,
				program_info *pi) {
    return ipcfg->edge_is_loop_exit(from_addr, to_addr, pi->functions);
}

void interproc_cfg_load_warnings(InterProcCFG *ipcfg, program_info *pi,
				 const char *fname) {
    warnings_t ww;
    unserialize(fname, ww);
    assert(ww.size() >= 1);
    Warning *w = 0;
    for (warnings_t::const_iterator it = ww.begin();
	 it != ww.end(); ++it) {
	pi->warnings[(*it)->getAddress()] = *it;
    }
    assert(w);
    ipcfg->set_target_warning(w, pi->functions); 
}

void interproc_cfg_set_target_warning(InterProcCFG *ipcfg, addr_t addr,
				      program_info *pi) {
    Warning *w = pi->warnings[addr];
    assert(w);
    ipcfg->set_target_warning(w, pi->functions);
}

bool_for_camlidl
interproc_cfg_is_interesting_loop(InterProcCFG *ipcfg,
				  addr_t addr,
				  program_info *pi) {
    return ipcfg->is_interesting_loop(addr, pi->functions);
}

unsigned
interproc_cfg_get_component(InterProcCFG *ipcfg,
			    addr_t addr,
			    program_info *pi) {
    return (unsigned)ipcfg->get_component(addr, pi->functions);
}

bool_for_camlidl
interproc_cfg_same_bb(InterProcCFG *ipcfg,
		      addr_t addr1, addr_t addr2,
		      program_info *pi) {
    return ipcfg->same_bb(addr1, addr2, pi->functions);
}


void compute_influence_for_warning(program_info *pi, addr_t addr) {
    instr_set_t slice;
    std::map<addr_t, Instruction *> instructions;

    // Map addresses to Instructions (to handle warnings)
    for (std::map<size_t, Function *>::iterator fit = pi->functions.begin(); 
         fit != pi->functions.end(); fit++) {
        Function *func = fit->second;

        for (Cfg::const_bb_iterator bbit = func->getCfg()->bb_begin(); 
             bbit != func->getCfg()->bb_end(); bbit++) {
            BasicBlock *bb = *bbit;

            for (instructions_t::const_iterator iit = bb->inst_begin(); 
                 iit != bb->inst_end(); iit++) {
                Instruction *i = *iit;

                i->decode();
                instructions[i->getAddress()] = i;
            }
        }
    }

    assert(pi->warnings.find(addr) != pi->warnings.end());
    for (Warning::slice_iterator it = pi->warnings[addr]->slice_begin(); 
       it != pi->warnings[addr]->slice_end(); ++it) {
        slice.insert(instructions[*it]);
    }

    // Compute the influence starting from the leaf functions and going
    // backward. 
    // for (...)
    std::vector<Function *> ordering;
    computeDFNumbering(pi->callgraph, ordering);
    debug("Functions ordered by DFT (finish time)\n");
    for (std::vector<Function *>::const_iterator it = ordering.begin();
         it != ordering.end(); ++it) {
        Function *func = *it;
        debug("   %.8x %s\n", func->getAddress(), func->getName());

        compute_influence(func->getCfg(), slice, pi->influence);
    }

}

// To generate the file with warnings you need to invoke 'static' with 
// the extra arguments '--warns FILE'
void warnings_from_file(const char *fname, program_info *pi) {
    warnings_t ww;
    unserialize(fname, ww);

    for(warnings_t::const_iterator it = ww.begin(); 
        it != ww.end(); ++it) {
      pi->warnings[(*it)->getAddress()] = *it;

      debug1("Warning %.8x (%d)\n", (*it)->getAddress(), 
             (*it)->getSliceSize());

      for(Warning::slice_iterator sit = (*it)->slice_begin(); 
          sit != (*it)->slice_end(); ++sit) {
        debug2(" %.8x", *sit);
      }

      debug2("\n");
    }
}

// Local Variables: 
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
