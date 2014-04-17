#include "func.h"
#include "cfg.h"
#include "callgraph.h"
#include "serialize.h"
#include "InterProcCFG.h"
#include "influence.h"

extern "C" {
    /* CamlIDL assumes that "boolean" types in the IDL will translate into
       "int" types on the C side. But this wouldn't be compatible with
       a C++ "bool". */
    typedef int bool_for_camlidl;
    
    struct program_info {
	Prog prog;
	CallGraph *callgraph;
	char *fname;
	map<addr_t, Function *> functions;
        std::map<addr_t, Warning *> warnings;
        influence_map_t influence;
    };

    program_info *program_info_from_file(const char *fname);

    void free_program_info(program_info *pi);

    InterProcCFG *construct_interproc_cfg(program_info *pi);

    void free_interproc_cfg(InterProcCFG *ipcfg);

    void interproc_cfg_set_target_addr(InterProcCFG *ipcfg, addr_t addr,
				       program_info *pi);

    void interproc_cfg_compute_shortest_paths(InterProcCFG *ipcfg);

    long long interproc_cfg_lookup_distance(InterProcCFG *ipcfg, addr_t addr,
					    program_info *pi);

    int interproc_cfg_lookup_influence(InterProcCFG *ipcfg, addr_t addr,
				       program_info *pi);

    long interproc_cfg_lookup_pth(InterProcCFG *ipcfg, addr_t addr,
				  program_info *pi);

    bool_for_camlidl
    interproc_cfg_is_loop_exit_cond(InterProcCFG *ipcfg, addr_t addr,
				    program_info *pi);

    bool_for_camlidl
    interproc_cfg_edge_is_loop_exit(InterProcCFG *ipcfg,
				    addr_t from_addr, addr_t to_addr,
				    program_info *pi);

    void interproc_cfg_set_target_warning(InterProcCFG *ipcfg, addr_t addr,
					  program_info *pi,
					  const char *fname);

    bool_for_camlidl
    interproc_cfg_is_interesting_loop(InterProcCFG *ipcfg,
				      addr_t addr,
				      program_info *pi);

    unsigned
    interproc_cfg_get_component(InterProcCFG *ipcfg,
				addr_t addr,
				program_info *pi);

    bool_for_camlidl
    interproc_cfg_same_bb(InterProcCFG *ipcfg,
			  addr_t addr1, addr_t addr2,
			  program_info *pi);

    void compute_influence_for_warning(program_info *pi, addr_t addr);

    void warnings_from_file(const char *fname, program_info *pi);
}

// Local Variables: 
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
