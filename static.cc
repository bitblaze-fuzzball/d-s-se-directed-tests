// TODOs:
// 1) Check whether /tmp/yyy directory exists, issue an error if it
// doesn't. Even better, make the path/file a cmnd line option.

#include "cfg.h"
#include "callgraph.h"
#include "serialize.h"
#include "VSAInterpreter.h"
#include "MemMap.h"

#include <boost/program_options.hpp>
#include <stdio.h>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <signal.h>

using namespace std;
using namespace boost::program_options;

int DEBUG_LEVEL = 0;
int ASSERT_LEVEL = 2;
FILE *DEBUG_FILE = stderr;

typedef absdomain::StridedInterval Domain;
typedef absdomain::AbsState<Domain> State;
typedef absinter::VSAInterpreter<Domain> Interpreter;
typedef absdomain::Region<Domain> Region;
typedef Region::RegionPtr RegionPtr;
typedef State::StatePtr StatePtr;

// Map addresses to functions
static map <size_t, Function *> functions;
static Prog prog;
static CallGraph *callgraph;
Interpreter *absi = 0;

addr_t current_instruction = 0;

std::set<std::string> dietlibcfuncs;
std::list<std::pair<addr_t, std::string> > callcontext;
warnings_t warnings;
std::map<addr_t, Instruction *> instructions;
bool dirtyslice = false;

addr_t last_program_instruction() {
    int i = 0;
    std::list<std::pair<addr_t, std::string> >::const_iterator it;
    for (it = callcontext.begin(); it != callcontext.end(); ++it) {
	i++;
	debug3("CHECKING %s %.8x (%d)\n", it->second.c_str(), it->first, i);
	if (dietlibcfuncs.find(it->second) != dietlibcfuncs.end()) {
	    debug3("\t FOUND LIBC FUNCTION\n");
	    return(it->first);
	}
    }

    return current_instruction;
}

void __enter_function(addr_t caller, const std::string name) {
    callcontext.push_back(std::pair<addr_t, const std::string>(caller, name));
    debug3("ENTERING %s from %.8x (%d)\n", name.c_str(), caller, callcontext.size());
}

void __exit_function() {
    debug3("LEAVING %s\n", callcontext.back().second.c_str());
    callcontext.pop_back();
}

void __warning(addr_t last, addr_t last_not_lib) {
    debug2("Recording warning (write out-of-bound) %.8x %.8x\n", last, 
           last_not_lib);
    Warning *w = new Warning(last_not_lib);

    std::set<Instruction *> instrs;
#ifndef NEW_SLICING
    debug3("Slicing warning %.8x\n", last);
    absi->slice(instructions[last], instrs);	
    
    for (std::set<Instruction *>::const_iterator iit = instrs.begin(); 
         iit != instrs.end(); iit++) {
        w->addToSlice((*iit)->getAddress());
    }

    debug3("%d instructions in the slice\n", instrs.size());
    warnings.insert(w);
#else
    absi->dumpUseDef(instructions[last]->getBasicBlock()->getCfg()->getFunction());
    absi->slice2(instructions[last], instrs);
#endif

#if 0
    // Heuristic #3
    BasicBlock *bb = instructions[last]->getBasicBlock();
    Cfg *cfg = bb->getCfg();
    if (last == last_not_lib && cfg->getComponentNo(bb) >= 1) {
        // This is a loop 
        std::set<Instruction *> slice, bounds;

        debug2("## Warning inside a loop %.8x %.8x\n", last, cfg->getComponent(bb)->getAddress());

        // Get the basic block in the component
        for (Cfg::const_bb_iterator bbit = cfg->bb_begin();
             bbit != cfg->bb_end(); ++bbit) {
            if (cfg->isSubComponentOf(*bbit, cfg->getComponent(bb))) {
                // This node in the loop
                for (instructions_t::const_iterator iit = 
                         (*bbit)->inst_begin();
                     iit != (*bbit)->inst_end(); ++iit) {
                    bounds.insert(*iit);
                }
            }
        }

        absi->sliceWithBounds(instructions[last], bounds, slice);
        for (std::set<Instruction *>::const_iterator iit = 
                 slice.begin();
             iit != slice.end(); iit++) {
            debug2("## Instruction %.8x ## SCC %.8x\n", (*iit)->getAddress(),
                   cfg->getComponent(bb)->getAddress());
        }
    }
#endif
}

// Stupid heuristic to detect the main: the main is the successor of
// __libc_start_main with the largest number of successors.
Function *guess_main() {
    Function *main_func = NULL;
    int main_succ = -1;

    for (map<size_t, Function *>::iterator fit = functions.begin(); 
	 fit != functions.end(); fit++) {
	Function *func = fit->second;

	if (strcmp(func->getName(), "main") == 0) {
	    return func;
	}

	if (strcmp(func->getName(), "__libc_start_main") == 0) {
	    debug("Found '__libc_start_main@%s' at %.8x\n", func->getModule(), 
		  func->getAddress());
	    for (CallGraph::const_succ_iterator sit =
		     callgraph->succ_begin(func);
		 sit != callgraph->succ_end(func); sit++) {
		debug("    %s@%s %.8x - %d successors\n", (*sit)->getName(), 
		      (*sit)->getModule(), (*sit)->getAddress(),
		      callgraph->getNumSuccessors(*sit));
		if (main_succ < callgraph->getNumSuccessors(*sit)) {
		    main_func = (*sit);
		    main_succ = callgraph->getNumSuccessors(*sit);
		}
	    }

	    debug("Guessed main function: %s@%s at %.8x\n", 
		  main_func->getName(), main_func->getModule(),
		  main_func->getAddress());
	    return main_func;
	}
    }

    assert(0);
    return 0;
}

void timeout(int a) {
    debug("Timeout expired!\nBye bye.\n");
    
    void(*boom)() = NULL; boom(); // :-)
}

int main(int argc, char **argv) {
    options_description opts;
    options_description mandatory("Mandatory parameters");
    options_description optional("Allowed options");
    FILE *f;
    char tmp[PATH_MAX];
    memmap::MemMap *mmap;
    addr_t main_addr;

    variables_map vm;
    mandatory.add_options()
        ("cfg", value<string>(), "Graph to analyze.")
        ;
    optional.add_options()
        ("interpret", "Enable abstract interpretation")
        ("intraproc", "Perform intraprocedural analysis only")
        ("function", value<string>(), "Interpret a specific function")
        ("dlev", value<unsigned>(), "Debug level [0-4]")
        ("alev", value<unsigned>(), "Assert level [0-4]")
        ("warns", value<string>(), "Output file for warnings")
        ("heurslice", "Enable heuristic slicing")
        ("timeout", value<unsigned>(), "Stop the analysis when the timeout "
	 "expires")
        ;
    opts.add(mandatory).add(optional);

    try {
        store(parse_command_line(argc, argv, opts), vm);
    } catch (...) {
        cout << opts << endl;
        return EXIT_FAILURE;
    }

    if (vm.count("cfg") == 0) {
        cerr << "Mandatory parameters missing." << endl;
        cout << opts << endl;
        return EXIT_FAILURE;
    }

    if (vm.count("dlev")) {
        DEBUG_LEVEL = vm["dlev"].as<unsigned>();
        if (DEBUG_LEVEL > 4) {
            cerr << "Debug level must be between 0 and 4." << endl;
            cout << opts << endl;
            return EXIT_FAILURE;
        }
    }

    if (vm.count("alev")) {
        ASSERT_LEVEL = vm["alev"].as<unsigned>();
        if (ASSERT_LEVEL > 4) {
            cerr << "Assert level must be between 0 and 4." << endl;
            cout << opts << endl;
            return EXIT_FAILURE;
        }
    }

    if (vm.count("heurslice")) {
        dirtyslice = true;
    }

    unserialize(vm["cfg"].as<string>().c_str(), prog, callgraph, functions);

    prog.createSection(Prog::getDefaultArgvAddress(), ".argv");
    mmap = new memmap::MemMap(prog);
    mmap->check();
    Region::setMemMap(mmap);

    f = fopen("/tmp/yyy/callgraph.dot", "w");
    assert(f);
    fprintf(f, "%s", callgraph->dot().c_str());
    fclose(f);

    // Load the list of dietlibc funcs
#define MAGICFOO(x) dietlibcfuncs.insert(#x);
#include "dietlibc_funcs.h"
#undef MAGICFOO

    Function *main_func;
    // Map addresses to Instructions (to handle warnings)
    for (map<size_t, Function *>::iterator fit = functions.begin(); 
	 fit != functions.end(); fit++) {
	Function *func = fit->second;

	for (Cfg::const_bb_iterator bbit = func->getCfg()->bb_begin(); 
	     bbit != func->getCfg()->bb_end(); bbit++) {
	    BasicBlock *bb = *bbit;

	    for (instructions_t::const_iterator iit = bb->inst_begin(); 
		 iit != bb->inst_end(); iit++) {
		Instruction *i = *iit;

		instructions[i->getAddress()] = i;
	    }
	}
    }		


    debug("[*] Program loaded from disk (%d functions, %d instructions)\n", functions.size(), instructions.size());
    debug2("%s\n", prog.tostring().c_str());

    if (vm.count("timeout")) {
        time_t t = vm["timeout"].as<unsigned>();
	signal(SIGALRM, timeout);
	alarm(t);
    }

    if (vm.count("interpret")) {
        absi = new Interpreter();
	absi->setInterproc(!((bool) vm.count("intraproc")));
        // currentState = State::get();

        debug2("@@@@ Overall '.data' size is %.u\n", prog.guessGlobalSize());

        // Decode the function
	if (vm.count("function")) {
	    addr_t f = strtoul(vm["function"].as<string>().c_str(), 0, 16);
	    if (functions.find(f) != functions.end()) {
		main_func = functions[f];
	    } else {
		fprintf(stderr, "Invalid function %.8x\n", f);
		exit(1);
	    }
	} else {
	    main_func = guess_main();
	}

	absi->visit(*(main_func->getCfg()));

	if (vm.count("warns")) {
#if 0
	    if (DEBUG_LEVEL >= 3) {
		absi->dumpUseDef();
	    }

	    for (warnings_t::iterator wit = warnings.begin();
		 wit != warnings.end(); ++wit) {
		std::set<Instruction *> instrs;
		debug3("Slicing warning %.8x\n", (*wit)->getAddress());
		assert(instructions.find((*wit)->getAddress()) != 
		       instructions.end());
		absi->slice(instructions[(*wit)->getAddress()], instrs);
		debug3("%d instructions in the slice\n", instrs.size());
		for (std::set<Instruction *>::const_iterator iit = 
			 instrs.begin();
		     iit != instrs.end(); iit++) {
		    (*wit)->addToSlice((*iit)->getBasicBlock()->getAddress());
		}
	    }
#endif
	    debug2("Serializing warnings to '%s'...\n", 
		   vm["warns"].as<string>().c_str())
	    serialize(vm["warns"].as<string>().c_str(), warnings);
	    debug2("Serialization done!\n");
	}

        exit(0);
    }


    // Just print
    for (map<size_t, Function *>::iterator fit = functions.begin(); 
	 fit != functions.end(); fit++) {
	Function *func = fit->second;

	if (vm.count("function")) {
	    addr_t f = strtoul(vm["function"].as<string>().c_str(), 0, 16);
	    if (func->getAddress() != f) {
		continue;
	    }
	}

	debug("[FUNC] %.8x %s@%s (%d args, %d bbs)\n", func->getAddress(), 
	      func->getName(), func->getModule(), func->getArgumentsNo(),
	      func->getCfg()->getBasicBlocksNo());
	fflush(DEBUG_FILE);

	if (func->getCfg()->getBasicBlocksNo() == 0)
	    continue;

	// Remove self loops 
	func->getCfg()->removeSelfLoops();

	snprintf(tmp, sizeof(tmp) - 1, "%s/%.8x.dot", "/tmp/yyy", 
		 func->getAddress());
	tmp[sizeof(tmp) - 1] = '\0';
	f = fopen(tmp, "w");
	assert(f);
	fprintf(f, "%s", func->getCfg()->dot().c_str());
	fclose(f);

	// Sanity checks (aggressive)
	func->getCfg()->sanityCheck(true);

	// Decode the function
	func->getCfg()->decode();

	for (Cfg::const_bb_iterator bbit = func->getCfg()->bb_begin(); 
	     bbit != func->getCfg()->bb_end(); bbit++) {
	    BasicBlock *bb = *bbit;
	    
	    // Test that the pointer to the Cfg stored in the BB is consistent
	    assert(bb->getCfg() == func->getCfg());
	    assert(!bb->getCfg()->hasEdge(bb, bb));

	    
	    debug2("\t[BB] %.8x-%.8x\n", bb->getAddress(), 
		   bb->getAddress() + bb->getSize());
	    
	    fflush(DEBUG_FILE);
	    
	    for (instructions_t::const_iterator iit = bb->inst_begin(); 
		 iit != bb->inst_end(); iit++) {
		Instruction *i = *iit;
		
		// Test the the pointer to the BB stored in the instruction is
		// consistent
		assert(i->getBasicBlock() == bb);
		// Test the checksum of the instruction
		assert(i->getCksum() == i->computeCksum());
		
		debug3("\t\t[INST] %.8x-%.8x%s [%s]\n", i->getAddress(), 
		       i->getAddress() + i->getSize(), 
		       i->isCall() ? " (CALL)" : "",
		       disasm(i->getRawBytes()));
		
		/*
		  if (i->isCall()) {
		  debug2("\t\t\t[CALL TARGETs]");
		  for (functions_t::const_iterator fit = i->call_targets_begin();
		  fit != i->call_targets_end(); fit++) {
		  debug2(" %.8x (%s@%s)", 
		  (*fit)->getAddress(), (*fit)->getName(), (*fit)->getModule());
			}
			debug2("\n");
			}
			// */
		
		for (statements_t::const_iterator sit = i->stmt_begin();
		     sit != i->stmt_end(); sit++) {
		    vine::Stmt *s = *sit;
		    
		    debug4("\t\t\t[STMT] %s\n", s->tostring().c_str());
		    
		}
	    }
	}
    }
    
    /* if (vm.count("interpret")) {
        VSAInterp::shutdown();
    }// */

    return EXIT_SUCCESS;
}

// Local Variables: 
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
