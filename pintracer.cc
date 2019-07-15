#include <iostream>
#include <sstream>
#include <string>
#include <typeinfo>
#include <map>
#include <stdexcept>
#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <pin.H>

#include <fstream>
#include <vector>
#include <map>

#include "callstack.h"
#include "argv_readparam.h"
#include "trace.h"
#include "cfg.h"
#include "callgraph.h"
#include "prog.h"
#include "serialize.h"
#include "PinDisasm.h"


extern "C" {
#include "xed-interface.h"
}

#include "debug.h"
#define DEBUG_FILE_NAME "/tmp/pintrace.log"
int DEBUG_LEVEL = 0;
FILE *DEBUG_FILE = NULL;
// #define REPLACE_PIC_THUNK
#ifdef REPLACE_PIC_THUNK
std::set<ADDRINT> pic_thunks;
#endif

using namespace std;

// Thread id 
static ADDRINT threadid = 0;
// Last instruction executed
static ADDRINT last_instruction = 0;
// Function currently executed
static ADDRINT current_function = 0;
// Map addresses to functions
static map <ADDRINT, Function *> functions;
// Stack pointer at the time main is entered
static ADDRINT main_stackptr = 0;
static CallGraph *callgraph = NULL;
static Prog prog;
static Trace trace;

// Cmd line args
static bool skiplibs = true;
static const char *dot = NULL;
static const char *vcg = NULL;
static const char *tracefile = NULL;
static const char *outprog = NULL;
static bool augmentcfg = false;
static bool augmented_cfg = false;

// ****************************************************************************
bool isplt(ADDRINT addr) {
    RTN rtn;

    PIN_LockClient();
    rtn = RTN_FindByAddress(addr);
    PIN_UnlockClient();

    // All .plt thunks have a valid RTN
    if (!RTN_Valid(rtn))
	return false;

    return (".plt" == SEC_Name(RTN_Sec(rtn)));
}

string funcname(ADDRINT addr) {
    RTN rtn;

    // TODO handle thunks
    PIN_LockClient();
    rtn = RTN_FindByAddress(addr);
    PIN_UnlockClient();
    if (RTN_Valid(rtn)) {
	return RTN_Name(rtn);
    } else {
	return "unknown";
    }
}

string modulename(ADDRINT addr) {
    IMG img;
  
    PIN_LockClient();
    img = IMG_FindByAddress(addr);
    PIN_UnlockClient();
    if (IMG_Valid(img)) {
	return IMG_Name(img);
    } else {
	return "unknown";
    }
}

bool islib(ADDRINT addr) {
    IMG img;
  
    PIN_LockClient();
    img = IMG_FindByAddress(addr);
    PIN_UnlockClient();

    return (IMG_Valid(img) && !IMG_IsMainExecutable(img));
}

static void augment_cfg() {
    if (augmentcfg) {
	// Augment the CFGs of the called functions
	std::set<Function *> worklist;

	// Put all functions in the worklist
	for (map<ADDRINT, Function *>::iterator fit = functions.begin(); 
	     fit != functions.end(); fit++) {
	    worklist.insert(fit->second);
	}

	while (!worklist.empty()) {
	    debug("\n\n----------------------------------------\n\n");
	    for (std::set<Function *>::iterator fit = worklist.begin();
		 fit != worklist.end(); fit++) {
		debug("Statically processing function %.8x %s@%s %d\n", 
		      (*fit)->getAddress(), 
		      funcname((*fit)->getAddress()).c_str(), 
		      modulename((*fit)->getAddress()).c_str(), 
		      (*fit)->isPending());
		assert(functions.find((*fit)->getAddress()) != functions.end());
		if ((*fit)->isPending()) {
		    (*fit)->setName(funcname((*fit)->getAddress()).c_str());
		    (*fit)->setModule(modulename((*fit)->getAddress()).c_str());
		    (*fit)->setProg(&prog);
		}
		(*fit)->getCfg()->augmentCfg((*fit)->getAddress(), functions);
		if ((*fit)->isPending()) {
		    (*fit)->setPending(false);
		    functions[(*fit)->getAddress()] = *fit;
		}
	    }

	    worklist.clear();
	    debug2("Looking for new functions...\n");

	    // Put in the worklist all the called functions that have not been
	    // visited yet
	    for (map<ADDRINT, Function *>::iterator fit = functions.begin(); 
		 fit != functions.end(); fit++) {
		Cfg *cfg = fit->second->getCfg();

		for (Cfg::const_bb_iterator bbit = cfg->bb_begin(); 
		     bbit != cfg->bb_end(); bbit++) {
		    instructions_t::const_iterator iit;
		    for (iit = (*bbit)->inst_begin();
			 iit != (*bbit)->inst_end(); iit++) {
			functions_t::const_iterator ctit;
			for (ctit = (*iit)->call_targets_begin();
			     ctit != (*iit)->call_targets_end(); ctit++) {

			    // The function has not been processed yet
			    assert(functions.find((*ctit)->getAddress()) != 
				   functions.end());
			    callgraph->addCall(fit->second, *ctit);
			    if ((*ctit)->isPending()) {
				worklist.insert(*ctit);
			    }
			}
		    }
		}
	    }
	}
	augmented_cfg = true;
    }
}
// ****************************************************************************

static void hook_call(THREADID tid, ADDRINT instrptr,
		      ADDRINT stackptr, ADDRINT funcaddr, CONTEXT *ctx) {
    bool aug = false;

    // Skip code executed before the main
    if (!main_stackptr)
	return;
	
    // Skip library code
    if (skiplibs && islib(instrptr))
	return;

    if (isplt(funcaddr)) {
	ADDRINT ebx = PIN_GetContextReg(ctx, LEVEL_BASE::REG_EBX);
	funcaddr = derefplt(instrptr, funcaddr, ebx);
    }

    // Stupid heuristics to detect when the program terminates
    if (funcname(funcaddr) == "exit") {
	debug("'exit' called (caller %.8x); assuming the program is "
	      "terminated\n", instrptr);
	main_stackptr = 0;
	aug = true;
    }

#ifdef REPLACE_PIC_THUNK
    Cfg *cfg = functions[current_function]->getCfg()
    Instruction *i = cfg->getInstruction(instrptr);
    if (!isplt(funcaddr) && patchpicthunk(instrptr, funcaddr, i)) {
	return;
    }
#endif

    // First time we see the function
    if (functions.find(funcaddr) == functions.end()) {
	Function *f = new Function(funcname(funcaddr), funcaddr, 0, 
				   modulename(funcaddr));
	functions[funcaddr] = f;
	f->setProg(&prog);
    }

    // Push function in the callstack
    callstack_push(tid, (void *) (stackptr - sizeof(void*)), 
		   (void *) funcaddr, (void *) instrptr);
    debug2(">> %.8x entering %.8x %d %s@%s\n", instrptr, 
	   (unsigned int) funcaddr, 
	   callstack_depth(tid) - 1, funcname(funcaddr).c_str(), 
	   modulename(funcaddr).c_str());

    // Mark the function as the current one
    assert(functions.find(funcaddr) != functions.end());
    assert(functions[funcaddr]);
    assert(functions.find(current_function) != functions.end());
    assert(functions[current_function]);
    functions[current_function]->getCfg()->addCall(instrptr, 
						   functions[funcaddr]);
    callgraph->addCall(functions[current_function], functions[funcaddr]);
    current_function = funcaddr;
    last_instruction = 0;

    if (aug) {
	augment_cfg();
    }
}

static void hook_return(THREADID tid, ADDRINT instrptr,
                            ADDRINT stackptr, ADDRINT retval) {
    ADDRINT funcaddr;
    ADDRINT caller;
    ADDRINT retaddr = *((ADDRINT *) stackptr);
    (void)retval;

    // Skip code executed before the main
    if (!main_stackptr)
	return;
		
    // Skip library code
    if (skiplibs && islib(retaddr) && stackptr != main_stackptr)
	return;

#ifdef REPLACE_PIC_THUNK
    if (ispicthunk(instrptr - 3) {
	debug2("Detected & ignored return from PIC thunk @ %.8x, "
	       "returning to %.8x\n", 
	       instrptr, retaddr);
	return;
    }
#endif

    // Pop function from the callstack
    callstack_pop(tid, (void *) stackptr, (void **) &funcaddr, 
		  (void **) &caller);
    debug2("<< %.8x leaving  %.8x %d %s@%s (called from %.8x)\n", instrptr, 
	   (unsigned int) funcaddr, callstack_depth(tid), 
	   funcname(funcaddr).c_str(), modulename(funcaddr).c_str(), caller);

    // Stupid heuristics to detect when main returns
    if (!caller) {
	main_stackptr = 0;
	augment_cfg();
	return;
    }

    // Mark the function we are returning to as the current one
    last_instruction = caller;
    callstack_top(tid, (void **) &funcaddr, (void **) &caller);
    assert(functions.find(funcaddr) != functions.end());
    assert(functions[funcaddr]);
    current_function = funcaddr;
}

static void hook_cpuid(THREADID tid, ADDRINT instrptr, CONTEXT *ctx, 
		       UINT32 pre) {
    unsigned char *bytes;
    // Dirty hack to remember the value of EAX before the execution of the
    // instruction
    static ADDRINT eax;
    ADDRINT ecx, edx;
    (void)tid;

    bytes = (unsigned char *) instrptr;
    assert(bytes[0] == 0x0f && bytes[1] == 0xa2);

    if (pre) {
	eax = PIN_GetContextReg(ctx, LEVEL_BASE::REG_EAX);
	debug("%.8x CPUID EAX:%.8x --> ", instrptr, eax);
    } else {
	ecx = PIN_GetContextReg(ctx, LEVEL_BASE::REG_ECX);
	edx = PIN_GetContextReg(ctx, LEVEL_BASE::REG_EDX);

	debug("EAX:%.8x EBX:%.8x ECX:%.8x EDX:%.8x\n", 
	      PIN_GetContextReg(ctx, LEVEL_BASE::REG_EAX),
	      PIN_GetContextReg(ctx, LEVEL_BASE::REG_EBX),
	      ecx, edx);

	// Feature flags
	if (eax == 1) {
	    // Clear bit 15 & 25 & 26 (CMOV & SSE & SSE2)
	    edx = edx & ~0x6000000;
	    // Clear bit 0 & 1 & 9 & 19 & 20 (SSE3 & PCLMULDQ & SSSE3 & SSE4.1
	    // & SSE4.2)
	    ecx = ecx & ~0x180203;

	    debug("%.8x Resuming executing with synthetic CPUID output: "
		  "ECX:%.8x EDX:%.8x\n", 
		  PIN_GetContextReg(ctx, LEVEL_BASE::REG_EIP), ecx, edx);
	    PIN_SetContextReg(ctx, LEVEL_BASE::REG_ECX, ecx);
	    PIN_SetContextReg(ctx, LEVEL_BASE::REG_EDX, edx);

	    PIN_ExecuteAt(ctx);
	}
    }
}

static void hook_instruction(THREADID tid, ADDRINT instrptr, USIZE len, 
			     ADDRINT stackptr, UINT32 pos, UINT32 isret) {
    Function *func;
    Cfg *cfg;
    (void)stackptr;

    // Skip code executed before the main
    if (!main_stackptr)
	return;
		
    // Skip library code
    if (skiplibs && islib(instrptr))
	return;

    assert(current_function);
    assert(current_function == (ADDRINT) callstack_top_funcaddr(tid));
    assert(functions.find(current_function) != functions.end());
    func = functions[current_function];
    cfg = func->getCfg();
    cfg->addInstruction(instrptr, (unsigned char *) instrptr, len, pos, 
			last_instruction, isret);
    cfg->setExecuted(instrptr);
    last_instruction = instrptr;

    // Execution trace
    if (tracefile) {
	trace.append(instrptr, len);
    }
}

static void hook_main(THREADID tid, ADDRINT ip, ADDRINT target, ADDRINT sp) {
    Cfg *cfg;

    main_stackptr = sp;
    callstack_push(tid, (void *) sp, (void *) ip, (void *) NULL);
    debug2(">> %.8x entering %.8x/%.8x %.8x %d %s@%s\n", ip, ip, target, 
	   sp, callstack_depth(tid) - 1, 
	   funcname(ip).c_str(), modulename(ip).c_str());

    // Mark the function as the current one
    current_function = ip;

    assert (functions.find(ip) == functions.end());
    functions[ip] = new Function(funcname(ip), ip, 0, modulename(ip));
    cfg = functions[ip]->getCfg();
    functions[ip]->setProg(&prog);
    // Add the first instruction of the main since instruction tracing is not
    // yet enabled at this point
    cfg->addInstruction(ip, (unsigned char *) ip, inslen(ip), 
			BASICBLOCK_HEAD, 0, 0);
    cfg->setExecuted(ip);
    last_instruction = ip;
}



static void hook_fini(INT32 c, void *v) {
    FILE *f;
    char tmp[PATH_MAX];
    // Graph<BasicBlock *, BasicBlockEdge *>::vertex_const_iterator bbit;
    (void)c; (void)v;

    stderr = DEBUG_FILE;

    assert(!augmentcfg || (augmentcfg && augmented_cfg));
    
    for (map<ADDRINT, Function *>::iterator fit = functions.begin(); 
	 fit != functions.end(); fit++) {
	Cfg *cfg = fit->second->getCfg();
	cfg->sanityCheck();
    }

    if (tracefile) {
	ofstream ofs(tracefile);
	boost::archive::binary_oarchive oa(ofs);

	debug2("Trace size: %d output file: %s\n", trace.size(), tracefile);
	oa << trace;
    }

    if (dot || vcg) {
	for (map<ADDRINT, Function *>::iterator it = functions.begin(); 
	     it != functions.end(); it++) {
	    Function *func = it->second;
	    if (dot) {
		snprintf(tmp, sizeof(tmp) - 1, "%s/%.8x.dot", dot, 
			 func->getAddress());
		tmp[sizeof(tmp) - 1] = '\0';
		f = fopen(tmp, "w");
		assert(f);
		fprintf(f, "%s", func->getCfg()->dot().c_str());
		fclose(f);
	    }
	    if (vcg) {
		snprintf(tmp, sizeof(tmp) - 1, "%s/%.8x.vcg", vcg, 
			 func->getAddress());
		tmp[sizeof(tmp) - 1] = '\0';
		f = fopen(tmp, "w");
		if (!f) {
		    fprintf(stderr, "Failed to open %s for writing: %s\n",
			    tmp, strerror(errno));
		    assert(f);
		}
		fprintf(f, "%s", func->getCfg()->vcg().c_str());
		fclose(f);
	    }

	    // Test decoding
	    // func->getCfg()->decode();
	    // Test dominators
	    // func->getCfg()->computeDominators();
	    // Test iterator
	    // for (Cfg::bb_const_iterator bbit = func->getCfg()->bb_begin(); 
	    //      bbit != func->getCfg()->bb_end(); bbit++) {
	    //	debug2("BBIT: %.8x\n", (*bbit)->getAddress());
	    // }
	}

	if (dot) {
	    snprintf(tmp, sizeof(tmp) - 1, "%s/callgraph.dot", dot);
	    tmp[sizeof(tmp) - 1] = '\0';
	    f = fopen(tmp, "w");
	    assert(f);
	    fprintf(f, "%s", callgraph->dot().c_str());
	    fclose(f);
	}
	if (vcg) {
	    snprintf(tmp, sizeof(tmp) - 1, "%s/callgraph.vcg", vcg);
	    tmp[sizeof(tmp) - 1] = '\0';
	    f = fopen(tmp, "w");
	    assert(f);
	    fprintf(f, "%s", callgraph->vcg().c_str());
	    fclose(f);
	}
    }

    // Serialize the callgraph 
    if (outprog) {
	serialize(outprog, prog);
    }

    debug3("Serialization done\n");
}

// ****************************************************************************

static void instrument_trace(TRACE trace, void *v) {
    (void)v;
    for(BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
	for(INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {  
	    ADDRINT addr = INS_Address(ins);
	    unsigned char *bytes = (unsigned char *) addr;

	    if (!isplt(addr)) {
		UINT32 pos;

		// Identify the position of the instruction in the BB
		if (BBL_InsHead(bbl) == ins) {
		    pos = BASICBLOCK_HEAD;
		} else if (BBL_InsTail(bbl) == ins) {
		    pos = BASICBLOCK_TAIL;
		} else {
		    pos = BASICBLOCK_MIDDLE;
		}

		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) hook_instruction,
			       IARG_THREAD_ID, IARG_INST_PTR,
			       IARG_UINT32, INS_Size(ins),
			       IARG_REG_VALUE, REG_STACK_PTR,
			       IARG_UINT32, pos,
			       IARG_UINT32, INS_IsRet(ins),
			       IARG_END);
	    }

	    // CPUID
	    if (bytes[0] == 0x0f && bytes[1] == 0xa2) {
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) hook_cpuid,
			       IARG_THREAD_ID, IARG_INST_PTR,
			       IARG_CONTEXT, IARG_UINT32, 1, IARG_END);
		INS_InsertCall(ins, IPOINT_AFTER, (AFUNPTR) hook_cpuid,
			       IARG_THREAD_ID, IARG_INST_PTR,
			       IARG_CONTEXT, IARG_UINT32, 0, IARG_END);
	    }

	    // Instrument function calls and returns to build the call stack
	    // and to be able to detect to which function an instruction
	    // belongs to
	    if (INS_IsCall(ins)) {
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) hook_call,
			       IARG_THREAD_ID, IARG_INST_PTR,
			       IARG_REG_VALUE, REG_STACK_PTR,
			       IARG_BRANCH_TARGET_ADDR, 
			       IARG_CONTEXT, IARG_END);
	    } else if (INS_IsRet(ins)) {
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) hook_return,
			       IARG_THREAD_ID, IARG_INST_PTR,
			       IARG_REG_VALUE, REG_STACK_PTR,
			       IARG_FUNCRET_EXITPOINT_VALUE,
			       IARG_END);
	    }
	}
    }
}

// ****************************************************************************

static void instrument_image(IMG img, void *v) {
    (void)v;
    static bool main_rtn_instrumented = false;

    if(!main_rtn_instrumented) {
	RTN rtn = RTN_FindByName(img, "main");
	if(rtn == RTN_Invalid()) {
	    rtn = RTN_FindByName(img, "__libc_start_main");
	}

	// Instrument main
	if(rtn != RTN_Invalid()) {
	    main_rtn_instrumented = true;
	    RTN_Open(rtn);
	    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)hook_main, 
			   IARG_THREAD_ID, 
			   IARG_INST_PTR, 
			   IARG_ADDRINT, RTN_Address(rtn),
			   IARG_REG_VALUE, REG_STACK_PTR,
			   IARG_END);
	    RTN_Close(rtn);
	}
    }

    printf("## %s loaded at %.8x-%.8x (%s)\n", IMG_Name(img).c_str(), 
	   IMG_LowAddress(img), IMG_HighAddress(img), 
	   IMG_IsMainExecutable(img) ? "exe" : "lib");

    prog.addModule(IMG_LowAddress(img), 
		   IMG_HighAddress(img) - IMG_LowAddress(img),
		   IMG_Name(img).c_str(), IMG_IsMainExecutable(img));

    for (SEC sec = IMG_SecHead(img); sec != IMG_SecTail(img); sec =
        SEC_Next(sec)) {

        if (SEC_Valid(sec)) {
            string secname = SEC_Name(sec);
            byte_t* mem = 0;
            bool deallocate = false;
            /*
            std::cout << "Processing: " << secname << " @adr: " <<
                std::hex << 
                SEC_Address(sec) << ", size: " << SEC_Size(sec) << std::endl;
                // */

            if (SEC_Mapped(sec) && SEC_Type(sec) == SEC_TYPE_DATA) {
                if (secname == ".data" || secname == ".rodata") {
                    mem = (byte_t*)SEC_Data(sec);
                }
            } else if (secname == ".bss") {
                // .bss is initialized later, so PIN doesn't know about
                // it. Fake the zero-initialized chunk of memory.
                mem = new byte_t[SEC_Size(sec)];
                //std::cout << "Initializing .bss" << std::endl;
                for (size_t i = 0; i < SEC_Size(sec); i++) {
                    mem[i] = 0;
                }
                deallocate = true;
            } 


	    secname = secname + "@" + IMG_Name(img);
	    debug("** processing data section %s@%s\n",
		  SEC_Name(sec).c_str(), IMG_Name(img).c_str());
	    prog.addSection(SEC_Address(sec), mem, SEC_Size(sec),
			    SEC_IsReadable(sec) | (SEC_IsWriteable(sec) << 1)
			    | (SEC_IsExecutable(sec) << 2), secname.c_str());
	    if (deallocate) {
		delete mem;
	    }
        }
    }
}


// This is used to ensure that the program runs a single thread. We do not want
// to deal with multithreaded programs.
VOID instrument_thread(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *v) {
    (void)ctxt; (void)flags; (void)v;
    if (threadid == 0) {
	threadid = tid;
    } else {
	if (threadid != tid) {
	    fprintf(stderr, "One thread is enough!!!\n");
	    abort();
	}
    }
}

// ****************************************************************************

int main(int argc, char **argv) {
    char *tmpstr, *inprog, *wdir;
    int tmpint = 1;

    int debug_fd = open(DEBUG_FILE_NAME, O_WRONLY|O_CREAT|O_TRUNC|O_LARGEFILE,
			0666);
    assert(debug_fd != -1);
    DEBUG_FILE = fdopen(debug_fd, "w");

    argv_getInt(argc, argv, (char *) "--skiplibs=", &tmpint);
    skiplibs = (bool) tmpint;
    argv_getInt(argc, argv, (char *) "--debug=", &DEBUG_LEVEL);
    if((tmpstr = argv_getString(argc, argv, "--dot=", NULL)) != NULL ) {
	dot = tmpstr;
    } else {
	dot = NULL;
    }
    if((tmpstr = argv_getString(argc, argv, "--vcg=", NULL)) != NULL ) {
	vcg = tmpstr;
    } else {
	vcg = NULL;
    }
    if((tmpstr = argv_getString(argc, argv, "--trace=", NULL)) != NULL ) {
	tracefile = tmpstr;
    } else {
	tracefile = NULL;
    }
    if((tmpstr = argv_getString(argc, argv, "--outprog=", NULL)) != NULL ) {
	outprog = tmpstr;
    } else {
	outprog = NULL;
    }
    if((tmpstr = argv_getString(argc, argv, "--inprog=", NULL)) != NULL ) {
	inprog = tmpstr;
	FILE *aslr;

	aslr = fopen("/proc/sys/kernel/randomize_va_space", "r");
	assert(aslr);
	if (fgetc(aslr) != 0x30) {
	    fprintf(stderr, "This feature is incompatible with ASLR!\n");
	    abort();
	}
	fclose(aslr);

	// Unserialize the CFGs
	unserialize(inprog, prog, callgraph, functions);
    } else {
	inprog = NULL;
    }
    if(argv_hasLongFlag(argc, argv, (char *) "--augment-cfg") ) {
	augmentcfg = true;
    }

    if((tmpstr = argv_getString(argc, argv, "--chdir=", NULL)) != NULL ) {
	wdir = tmpstr;
    int r = chdir(wdir);
    assert(r == 0 && "chdir failed, pintool doesn't know where to go?");
    } else {
	wdir = NULL;
    }

    callgraph = prog.getCallGraph();

    fprintf(stderr, "Debug level:                      %d\n", DEBUG_LEVEL);
    fprintf(stderr, "Log file:                         %s\n", DEBUG_FILE_NAME);
    fprintf(stderr, "Execution trace:                  %s\n", 
	    tracefile ? tracefile : "disabled");
    fprintf(stderr, "Library tracing:                  %s\n", 
	    skiplibs ? "disabled" : "enabled");
    fprintf(stderr, "Working directory:                %s\n", 
	    wdir ? wdir: "current");
    fprintf(stderr, "Input program:                    %s\n", 
	    inprog ? inprog: "disabled");
    fprintf(stderr, "Ouput program:                    %s\n", 
	    outprog ? outprog: "disabled");
    fprintf(stderr, "CFGs and callgraph (dot):         %s\n", 
	    dot ? dot: "disabled");
    fprintf(stderr, "CFGs and callgraph (VCG):         %s\n", 
	    vcg ? vcg: "disabled");
    fprintf(stderr, "Static CFGs augmentation:         %s\n", 
	    augmentcfg ? "enabled": "disabled");
    fprintf(stderr, "======================================================"
	    "=========================\n");

    PIN_InitSymbols();
    PIN_Init(argc, argv);
    PIN_SetSyntaxATT();
    PIN_AddThreadStartFunction(instrument_thread, 0);
    IMG_AddInstrumentFunction(instrument_image, 0);
    TRACE_AddInstrumentFunction(instrument_trace, 0);
    PIN_AddFiniFunction(hook_fini, 0);

    PIN_StartProgram();
    return 0;
}

// Local Variables: 
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
