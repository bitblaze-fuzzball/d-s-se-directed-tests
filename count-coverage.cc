#include "debug.h"
#include "func.h"
#include "cfg.h"
#include "callgraph.h"
#include "serialize.h"

#include <boost/program_options.hpp>
#include <stdio.h>
#include <fstream>
#include <vector>
#include <string>
#include <map>

using namespace std;
using namespace boost::program_options;

int DEBUG_LEVEL = 0;
FILE *DEBUG_FILE = stderr;

// Map addresses to functions
static map <addr_t, Function *> functions;
CallGraph* callgraph;
Prog prog;

map<addr_t, const char *> seen_insns;

int main(int argc, char **argv) {
    options_description opts;
    options_description mandatory("Mandatory parameters");
    options_description optional("Allowed options");
    FILE *f;
    char tmp[PATH_MAX];

    unsigned start_addr, end_addr;
    int insn_count = 0;

    variables_map vm;
    mandatory.add_options()
        ("cfg", value<string>(), "Graph to analyze.")
	("start-addr", value<unsigned>(), "Start point of range (decimal)")
	("end-addr", value<unsigned>(), "End point of range (decimal)")
       ;
    optional.add_options()
        ("dlev", value<unsigned>(), "Debug level [0-2]");
        ;
    opts.add(mandatory).add(optional);

    try {
        store(parse_command_line(argc, argv, opts), vm);
    } catch (...) {
        cerr << "Command-line options parsing failure." << endl;
        cout << opts << endl;
        return EXIT_FAILURE;
    }

    if (vm.count("cfg") == 0) {
        cerr << "Mandatory parameter --cfg missing." << endl;
        cout << opts << endl;
        return EXIT_FAILURE;
    }
    if (vm.count("start-addr") == 0) {
        cerr << "Mandatory parameter --start-addr missing." << endl;
        cout << opts << endl;
        return EXIT_FAILURE;
    } else {
	start_addr = vm["start-addr"].as<unsigned>();
    }
    if (vm.count("end-addr") == 0) {
        cerr << "Mandatory parameter --end-addr missing." << endl;
        cout << opts << endl;
        return EXIT_FAILURE;
    } else {
	end_addr = vm["end-addr"].as<unsigned>();
    }

    if (vm.count("dlev")) {
        DEBUG_LEVEL = vm["dlev"].as<unsigned>();
        if (DEBUG_LEVEL > 2) {
            cerr << "Debug level must be between 0 and 2." << endl;
            cout << opts << endl;
            return EXIT_FAILURE;
        }
    }

    debug("Address range 0x%08x to 0x%08x\n", start_addr, end_addr);
    
    unserialize(vm["cfg"].as<string>().c_str(), prog, callgraph, functions);

    debug("Program loaded from disk (%d functions)!\n", functions.size());

    for (map<addr_t, Function *>::iterator it = functions.begin(); 
	 it != functions.end(); it++) {
	Function *func = it->second;
	const char *func_name = func->getName();

	if (func->getCfg()->getBasicBlocksNo() == 0)
	    continue;

	if (func->getAddress() < start_addr || 
	    func->getAddress() > end_addr)
	    continue;

	// Decode the function
	debug("[FUNC] %.8x %s@%s\n", func->getAddress(), 
	       func_name, func->getModule());
	fflush(DEBUG_FILE);

	func->getCfg()->decode();

	for (Cfg::const_bb_iterator bbit = func->getCfg()->bb_begin(); 
	     bbit != func->getCfg()->bb_end(); bbit++) {
	    BasicBlock *bb = *bbit;

	    debug2("\t[BB] %.8x-%.8x\n", bb->getAddress(), 
		   bb->getAddress() + bb->getSize());

	    for (instructions_t::const_iterator iit = bb->inst_begin(); 
		 iit != bb->inst_end(); iit++) {
		Instruction *i = *iit;
		addr_t insn_addr = i->getAddress();

		assert(seen_insns.find(insn_addr) == seen_insns.end());
		seen_insns[insn_addr] = func_name;

		insn_count++;
	    }

	}

    }
    
    printf("Saw %d instructions in the range 0x%08x-0x%08x\n",
	   insn_count, start_addr, end_addr);

    return EXIT_SUCCESS;
}

// Local Variables: 
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
