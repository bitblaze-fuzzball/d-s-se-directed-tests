#include "debug.h"
#include "func.h"
#include "cfg.h"
#include "callgraph.h"
#include "serialize.h"
#include "warning.h"
#include "InterProcCFG.h"

#include <boost/program_options.hpp>

#include <stdio.h>
#include <fstream>
#include <vector>
#include <string>
#include <map>

using namespace std;
using namespace boost::program_options;
using namespace boost;

int DEBUG_LEVEL = 0;
FILE *DEBUG_FILE = stderr;

int main(int argc, char **argv) {
    options_description opts;
    options_description mandatory("Mandatory parameters");
    options_description optional("Allowed options");
    FILE *f;
    char tmp[PATH_MAX];

    variables_map vm;
    mandatory.add_options()
        ("cfg", value<string>(), "Graph to analyze")
	("target-addr", value<addr_t>(), "Code address to target")
       ;
    optional.add_options()
	("warn-file", value<string>(), "Serialized warnings")
	("warn-addr", value<addr_t>(), "Warning to focus on")
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

    if (vm.count("dlev")) {
        DEBUG_LEVEL = vm["dlev"].as<unsigned>();
        if (DEBUG_LEVEL > 2) {
            cerr << "Debug level must be between 0 and 2." << endl;
            cout << opts << endl;
            return EXIT_FAILURE;
        }
    }

    if (vm.count("cfg") == 0) {
        cerr << "Mandatory parameter --cfg missing." << endl;
        cout << opts << endl;
        return EXIT_FAILURE;
    }

    addr_t target_addr = 0;
    if (vm.count("target-addr") == 0) {
	target_addr = 0;
    } else {
	target_addr = vm["target-addr"].as<unsigned>();
    }   
 
    // Map addresses to functions
    map<addr_t, Function *> functions;
    InterProcCFG *ipcfg = build_ipcfg(vm["cfg"].as<string>().c_str(), functions);

    if (vm.count("warn-file")) {	
	warnings_t ww;
	unserialize(vm["warn-file"].as<string>().c_str(), ww);
	assert(ww.size() >= 1);
	addr_t warn_addr;
	if (vm.count("warn-addr")) {
	    warn_addr = vm["warn-addr"].as<addr_t>();
	} else {
	    warn_addr = 0;
	}
	Warning *w = 0;
	for (warnings_t::const_iterator it = ww.begin();
	     it != ww.end(); ++it) {
	    if (!warn_addr ||
		(warn_addr && warn_addr == (*it)->getAddress())) {
		w = *it;
	    }
	}
	assert(w);
	ipcfg->set_target_warning(w, functions); 
	ipcfg->compute_shortest_paths();
   } else if (target_addr) {
	ipcfg->set_target_addr(target_addr, functions);
	ipcfg->compute_shortest_paths();
    }
    ipcfg->to_vcg(cout);

    return EXIT_SUCCESS;
}

// Local Variables: 
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
