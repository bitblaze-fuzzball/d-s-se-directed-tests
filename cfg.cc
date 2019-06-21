#include "cfg.h"
#include "instr.h"
#include "PinDisasm.h"
#include "prog.h"
//#include "json_spirit_reader_template.h"
#include "debug.h"
#define NDEBUG // disable asserts
#include <cassert>

extern "C" {
#include "xed-interface.h"
}

uint32_t total_inst;

BasicBlock *Cfg::addBasicBlock(addr_t addr) {
    BasicBlock *b;

    b = new BasicBlock(addr);
    assert(b);

    cfg_t::addVertex(b);
    b->cfg = this;

    if (!cfg_t::hasVertex(b)) {
        fprintf(stderr, "ERROR: Incorrect vertex addition.\n");
        return 0;
    }
    assert(cfg_t::hasVertex(b));

    debug3("Created new bb %.8x %p\n", addr, b);

    can_have_self_loops = true;

    return b;
}

void Cfg::delBasicBlock(BasicBlock *bb) {
    debug3("Deleting BB %p %.8x\n", bb, bb->getAddress());

    cfg_t::delVertex(bb);

    if (entry == bb)
	entry = NULL;

    exits.erase(bb);
    delete bb;
}

void Cfg::linkBasicBlocks(BasicBlock *sbb, BasicBlock *dbb) {
    BasicBlockEdge *e;
    bool r;

    e = new BasicBlockEdge(sbb, dbb);
    assert(e);

    debug3("Linking BBS %.8x-%.8x %.8x-%.8x %p %p %p\n", sbb->getAddress(), 
	   sbb->getAddress() + sbb->getSize(), 
	   dbb->getAddress(), dbb->getAddress() + dbb->getSize(), 
	   sbb, dbb, e);

    r = cfg_t::addEdge(sbb, dbb, e);
    if (!r) {
	delete e;
    }
    assert(cfg_t::hasEdge(sbb, dbb));

    sanityCheck();

    can_have_self_loops = true;
}

void Cfg::unlinkBasicBlocks(BasicBlock *sbb, BasicBlock *dbb) {
    BasicBlockEdge *e;

    e = cfg_t::delEdge(sbb, dbb);
    debug3("Unlinking BBS %.8x-%.8x %.8x-%.8x %p %p %p\n", sbb->getAddress(), 
	   sbb->getAddress() + sbb->getSize(), 
	   dbb->getAddress(), dbb->getAddress() + dbb->getSize(), 
	   sbb, dbb, e);

    if (e)
	delete e;
}

void Cfg::clear() {
    std::set<BasicBlockEdge *> edges;
    std::set<BasicBlock *> bbs;

    for (Cfg::const_edge_iterator it = edges_begin(); it != edges_end(); 
	 it++) {
	edges.insert(*it);
    }

    for (std::set<BasicBlockEdge *>::iterator it = edges.begin(); 
	 it != edges.end(); it++) {
	unlinkBasicBlocks((*it)->getSource(), (*it)->getTarget());
    }

    for (Cfg::const_bb_iterator it = bb_begin(); it != bb_end(); it++) {
	bbs.insert(*it);
    }

    for (std::set<BasicBlock *>::iterator it = bbs.begin(); 
	 it != bbs.end(); it++) {
	delBasicBlock(*it);
    }


    addr2bb.clear();
    calls.clear();
    exits.clear();
    entry = NULL;

    assert(getNumVertex() == 0);
    assert(getNumEdges() == 0);
}

BasicBlock *Cfg::splitBasicBlock(BasicBlock *oldbb, addr_t before) {
    BasicBlock *newbb1, *newbb2;
    cfg_t::vertex_descriptor oldbb_vd, newbb1_vd, newbb2_vd;
    cfg_t::out_edge_iterator oei, oee;
    cfg_t::in_edge_iterator iei, iee;
    std::list< std::pair<BasicBlock *, BasicBlock *> > todel;
    std::list< std::pair<BasicBlock *, BasicBlock *> > toadd;

    assert(addr2bb.find(before) != addr2bb.end());
    assert(oldbb->address <= before && oldbb->address + oldbb->size > before);

    debug3("Splitting BB %.8x-%.8x (%p) @ %.8x (%d instructions)\n", 
	   oldbb->address, oldbb->address + oldbb->size, oldbb, before, 
	   oldbb->instructions.size());

    newbb1 = addBasicBlock(oldbb->address);
    newbb2 = addBasicBlock(before);

    // Scan the list of instructions and assign those before the split point to
    // newbb1 and the remaining ones to newbb2
    for (size_t i = 0; i < oldbb->instructions.size(); i++) {
	Instruction *inst;

	inst = oldbb->instructions[i];
	debug3("  Instruction %.8x-%.8x -- %.8x\n", inst->address, 
	       inst->address + inst->size, before);
	assert((inst->address < before && 
		inst->address + inst->size - 1 < before) ||
	       inst->address >= before);
    
	if (inst->address < before) {
	    debug3("     adding to bb1 %.8x\n", newbb1->address);
	    newbb1->addInstruction(inst);
	    addr2bb[inst->address] = newbb1;
	} else {
	    debug3("     adding to bb2 %.8x\n", newbb2->address);
	    newbb2->addInstruction(inst);
	    addr2bb[inst->address] = newbb2;
	}
    }

    // Update the entry point of the cfg if needed
    if (entry == oldbb) {
	setEntry(newbb1);
    }

    // Update the exit points of the cfg if needed
    if (exits.find(oldbb) != exits.end()) {
	exits.erase(oldbb);
	exits.insert(newbb2);
    }

    oldbb_vd = cfg_t::getVertex(oldbb);
    newbb1_vd = cfg_t::getVertex(newbb1);
    newbb2_vd = cfg_t::getVertex(newbb2);

    // Link the predecessors of the oldbb with newbb1
    for (boost::tie(iei, iee) = boost::in_edges(oldbb_vd, cfg_t::graph); 
	 iei != iee; ++iei) {
	BasicBlock *pred = cfg_t::getVertex(boost::source(*iei, cfg_t::graph));
	
	debug3("  Processing incoming edge %.8x-%.8x -- %.8x-%.8x\n", 
	       pred->address, pred->address + pred->size, oldbb->address, 
	       oldbb->address + oldbb->size);
	
	todel.push_back(std::pair<BasicBlock *, BasicBlock *>(pred, oldbb));
	toadd.push_back(std::pair<BasicBlock *, BasicBlock *>(pred, newbb1));
	// linkBasicBlocks(pred, newbb1);
    }

    // Link the successors of the oldbb with newbb2
    for (tie(oei, oee) = boost::out_edges(oldbb_vd, cfg_t::graph); 
	 oei != oee; ++oei) {
	BasicBlock *succ = cfg_t::getVertex(boost::target(*oei, cfg_t::graph));
	
	debug3("  Processing outgoing edge %.8x-%.8x -- %.8x-%.8x\n", 
	       oldbb->address, oldbb->address + oldbb->size, succ->address, 
	       succ->address + succ->size);
	
	todel.push_back(std::pair<BasicBlock *, BasicBlock *>(oldbb, succ));
	toadd.push_back(std::pair<BasicBlock *, BasicBlock *>(newbb2, succ));
	// linkBasicBlocks(newbb2, succ);
    }
    
    // Remove old links
    while (!todel.empty()) {
	unlinkBasicBlocks(todel.front().first, todel.front().second);
	todel.pop_front();
    }

    while (!toadd.empty()) {
	linkBasicBlocks(toadd.front().first, toadd.front().second);
	toadd.pop_front();
    }
    
    // XXX: hack to delete instructions when the oldbb is destroyed
    oldbb->instructions.clear();
    delBasicBlock(oldbb);

    linkBasicBlocks(newbb1, newbb2);

    debug3("Splitted\n");

    return newbb2;
}

byte_t *addr2bytes(Cfg *cfg, addr_t addr) {
    Prog *prog = cfg->getFunction()->getProg();
    Section *sec = prog->getSection(addr);
    if (sec)
	return sec->addrToPtr(addr);
    else
	return (byte_t *)addr;
}

bool Cfg::addInstruction(addr_t addr, byte_t *bytes, size_t len, int pos, 
			 addr_t prev, bool isret) {
    bool changed = false;
    BasicBlock *curbb, *prevbb = NULL;
    Instruction *inst = NULL;
    bool isrep = false, prev_isrep = false;

    // Dirty hack to detect whether previous instruction was a rep
    isrep = len == 2 && (*bytes == 0xf3 || *bytes == 0xf2);
    byte_t prev_byte = prev ? *(addr2bytes(this, prev)) : 0;
    prev_isrep = prev && ( prev_byte == 0xf3 || prev_byte == 0xf2);

    debug3("Processing instruction %.8x-%.8x %d %.8x\n", addr, addr + len - 1,
	   pos, prev);

    // Check if instruction has been processed already and if any BB must be
    // split
    if (addr2bb.find(addr) != addr2bb.end()) {
	// Instruction already processed
	prevbb = curbb = addr2bb[addr];

	// Sanity checks
	assert(prevbb->instructions.size() > 0);
	if (pos == BASICBLOCK_TAIL) {
	    // Temporary disabled
	    // assert_msg(addr + len == prevbb->address + prevbb->size, 
	    // "eip:%.8x len:%d prevbb:%.8x size:%.8x", addr, len, 
	    // prevbb->address, prevbb->size);
	    ;
	} else if (pos == BASICBLOCK_HEAD || pos == BASICBLOCK_MIDDLE) {
	    assert_msg(addr >= prevbb->address, "%.8x >= %.8x (%.8x)", addr, prevbb->address, prev);
	    assert(addr + len <= prevbb->address + prevbb->size);
	}

	// Check whether we need to split the BB. Need to split if pos = head,
	// but the instruction is not currently the first of the BB
	if ((pos == BASICBLOCK_HEAD && prevbb->address < addr)) {
	    // Split
	    curbb = splitBasicBlock(prevbb, addr);

	    // Sanity checks
	    assert(prev);
	    assert(addr2bb.find(prev) != addr2bb.end());

	    prevbb = addr2bb[prev];
	    debug3("    linking %.8x-%.8x with %.8x-%.8x\n", 
		   prevbb->address, prevbb->address + prevbb->size,
		   curbb->address, curbb->address + curbb->size);

	    linkBasicBlocks(prevbb, curbb);
	    changed = true;
	} else { 
	    assert(prev == 0 || addr2bb.find(prev) != addr2bb.end());
	    if (prev) {
		prevbb = addr2bb[prev];

		// Check if we have to add and edge or if it already exists
		if (prevbb != curbb && !hasEdge(prevbb, curbb)) {
		    debug3("    linking %.8x-%.8x with %.8x-%.8x "
			   "(already executed but edge was missing)\n", 
			   prevbb->address, prevbb->address + prevbb->size,
			   curbb->address, curbb->address + curbb->size);
		    linkBasicBlocks(prevbb, curbb);
		}
	    }
	}
    } else {
	debug3("Instruction seen for the first time\n");
	// First time we see the instruction
	assert(addr);
        if (addr == 0) {
            fprintf(stderr, "ERROR: Address somehow set to 0, though we searched for it.\n");
        }
	inst = new Instruction(addr, bytes, len); 
	assert(inst);

	if (!prev) {
	    // First instruction of the function
	    if (addr2bb.size()) {
		for (std::map<addr_t, BasicBlock *>::const_iterator it = addr2bb.begin();
		     it != addr2bb.end(); it++) {
		    debug2("   %.8x %.8x\n", it->first, it->second->getAddress());
		}
	    }
	    assert_msg(addr2bb.size() == 0, "eip:%.8x prev:%.8x bbs:%d", addr, 
		       prev, addr2bb.size());
	    assert(!entry);

	    // This assertion claims that the first instruction in
	    // a function should not be a return.
	    // This initially sounded reasonable, but it failed for
	    // instance in a binary for g++ (cc1plus) we tested with.
	    //assert(!isret);

	    curbb = addBasicBlock(addr);
	    setEntry(curbb);
	    curbb->addInstruction(inst);
	    changed = true;

	    debug3("Creating new BB @%.8x (%p) to hold the instruction\n", 
		   addr, curbb);
	} else {
	    assert(addr2bb.find(prev) != addr2bb.end());
	    prevbb = curbb = addr2bb[prev];

	    if (pos != BASICBLOCK_HEAD && !isrep && !prev_isrep) {
		curbb->addInstruction(inst);
		changed = true;

		debug3("Appending instruction %.8x to BB %.8x (%p)\n", 
		       addr, curbb->address, curbb);
	    } else {
		if (isrep || prev_isrep) {
		    debug2("Creating a new BB because %s%s\n", 
			   isrep ? "isrep" : "", 
			   prev_isrep ? "prev_isrep" : "");
		}
		curbb = addBasicBlock(addr);
		curbb->addInstruction(inst);
		linkBasicBlocks(prevbb, curbb);
		changed = true;

		debug3("Creating new BB @%.8x (%p) to hold the instruction\n", 
		       addr, curbb);
		debug3("Linking BB %.8x with BB %.8x\n", prevbb->address, 
		       curbb->address);
	    }

	    if (isret) {
		exits.insert(curbb);
	    }

	    if (isrep) {
		debug2("Adding self-loop for rep\n");
		linkBasicBlocks(curbb, curbb);
	    }
	}

	addr2bb[addr] = curbb;
    }
    // debug3("\n", "");

    return changed;
}

size_t Cfg::getBasicBlocksNo() {
    return cfg_t::getNumVertex();
}

void Cfg::addCall(addr_t caller, Function *callee) {
    // Mark the node as an exitpoint if it is calling exit
    if (strcmp(callee->getName(), "exit") == 0) {
	assert(addr2bb.find(caller) != addr2bb.end());
	exits.insert(addr2bb[caller]);
    }

    calls[caller].insert(callee);
}

void Cfg::check() {
    ;
}

void Cfg::decode() {
    if (!decoded) {
	for(Cfg::const_bb_iterator it = bb_begin(); it != bb_end(); it++) {
	    BasicBlock *bb = *it;
	    bb->decode();
	}
    }
    decoded = true;
}

json_spirit::Object Cfg::json() {
    std::set<Function *> functions;
    char tmp[1024];
    int j = 0;

    if (getNumVertex() > 0) {
	computeWeakTopologicalOrdering();
	debug2("Weak topological ordering: %s\n", wto2string().c_str());
    }

    json_spirit::Object cfgobj;

    json_spirit::Array blocksarr;
    for (Cfg::const_bb_iterator bbit = bb_begin(); 
	 bbit != bb_end(); bbit++) {
	BasicBlock *bb = *bbit;
	json_spirit::Object blockobj;
	stringstream addrstr;
	addrstr << "0x" << std::hex << bb->getAddress();
	blockobj.push_back(json_spirit::Pair("blockaddress", addrstr.str()));
	blockobj.push_back(json_spirit::Pair("blocksize", (int)(bb->getSize())));
	blockobj.push_back(json_spirit::Pair("functionblocknumberid", j));
	//blockobj.push_back(Pair("executed", bb->isExecuted()));
	json_spirit::Array instrarray;
	for (instructions_t::iterator it = bb->instructions.begin(); 
	     it != bb->instructions.end(); it++) {
	    json_spirit::Object instructionobj;
	    stringstream instraddr;
	    instraddr << "0x" << std::hex << (*it)->getAddress();
	    instructionobj.push_back(json_spirit::Pair("address", instraddr.str()));
	    instructionobj.push_back(json_spirit::Pair("instruction", disasm((*it)->getRawBytes())));
	    stringstream bytes;
	    bytes << "0x" << std::hex << int((*it)->getRawBytes()[0]);
	    for (unsigned int i=1;i<(*it)->getSize();i++) {
		bytes << " 0x" << std::hex << int((*it)->getRawBytes()[i]);
	    }
	    instructionobj.push_back(json_spirit::Pair("bytes", bytes.str()));
	    
	    instrarray.push_back(instructionobj);
	}
	blockobj.push_back(json_spirit::Pair("instructions", json_spirit::Value(instrarray)));
	
	blocksarr.push_back(blockobj);
	//	r += "   " + std::string(tmp);
	j++;
    }

     for (std::map<addr_t, functions_t>::iterator it = calls.begin(); 
 	 it != calls.end(); it++) {
 	functions.insert(it->second.begin(), it->second.end());
     }

    json_spirit::Array edgesarr;

    // inter-function edges
    for (std::map<addr_t, functions_t>::iterator it1 = calls.begin();
	 it1 != calls.end(); it1++) {
	
	for (functions_t::iterator it2 = it1->second.begin(); 
	     it2 != it1->second.end(); it2++) {
	    json_spirit::Object edgeobj;
	    stringstream sourceaddr, targetaddr;
	    sourceaddr << "0x" << std::hex << addr2bb[it1->first]->getAddress();
	    targetaddr << "0x" << std::hex << (*it2)->getAddress();

	    edgeobj.push_back(json_spirit::Pair("source", sourceaddr.str()));
	    edgeobj.push_back(json_spirit::Pair("target", targetaddr.str()));

	    edgesarr.push_back(edgeobj);
	}
    }

    // intra-function edges
    for (Cfg::const_edge_iterator eit = edge_begin();
 	 eit != edge_end(); eit++) {
	BasicBlockEdge *e = *eit;
	BasicBlock *source = e->getSource(), *target = e->getTarget();

	json_spirit::Object edgeobj;
	stringstream sourceaddr, targetaddr;
	sourceaddr << "0x" << std::hex << source->getAddress();
	targetaddr << "0x" << std::hex << target->getAddress();

	edgeobj.push_back(json_spirit::Pair("source", sourceaddr.str()));
	edgeobj.push_back(json_spirit::Pair("target", targetaddr.str()));
	
	edgesarr.push_back(edgeobj);
    }

    // put together the block list and edge list
    json_spirit::Object finalcfg;
    
    finalcfg.push_back(json_spirit::Pair("blocks", blocksarr));
    finalcfg.push_back(json_spirit::Pair("edges",edgesarr));

    stringstream funcaddr;
    funcaddr << "0x" << std::hex << function->getAddress();
    cfgobj.push_back(json_spirit::Pair("function_addr", funcaddr.str()));
    cfgobj.push_back(json_spirit::Pair("cfg", finalcfg));

    //    string jsonstr = json_spirit::write_string(json_spirit::Value(finaljson), json_spirit::pretty_print);

    return cfgobj;
}

std::string Cfg::dot() {
    std::set<Function *> functions;
    std::string r = "";
    char tmp[1024];
    int j = 0;

    if (getNumVertex() > 0) {
	computeWeakTopologicalOrdering();
	debug2("Weak topological ordering: %s\n", wto2string().c_str());
    }

    r = "digraph G {\n";

    for (Cfg::const_bb_iterator bbit = bb_begin(); 
	 bbit != bb_end(); bbit++) {
	BasicBlock *bb = *bbit;
	if (entry == bb) {
	    sprintf(tmp, "bb_%.8x [label=\"%.8x-%.8x (%d)\", "
		    "color=\"green\" %s];\n", 
		    bb->getAddress(), bb->getAddress(), 
		    bb->getAddress() + bb->getSize(), j,
		    bb->isExecuted() ? "style=filled" : "");
	} else if (exits.find(bb) != exits.end()) {
	    sprintf(tmp, "bb_%.8x [label=\"%.8x-%.8x (%d)\", color=\"red\" %s];"
		    "\n", bb->getAddress(), bb->getAddress(), 
		    bb->getAddress() + bb->getSize(), j,
		    bb->isExecuted() ? "style=filled" : "");
	} else {
	    sprintf(tmp, "bb_%.8x [label=\"%.8x-%.8x (%d)\" %s];\n", 
		    bb->getAddress(), bb->getAddress(), 
		    bb->getAddress() + bb->getSize(), j,
		    bb->isExecuted() ? "style=filled" : "");
	}
	r += "   " + std::string(tmp);
	j++;
    }

    for (std::map<addr_t, functions_t>::iterator it = calls.begin(); 
	 it != calls.end(); it++) {
	functions.insert(it->second.begin(), it->second.end());
    }

    for (std::set<Function *>::iterator it = functions.begin();
	 it != functions.end(); it++) {
	sprintf(tmp, "func_%.8x [label=\"%s@%.8x [%s]\", color=blue, "
		"shape=rectangle,URL=\"%.8x.svg\"];\n", 
		(*it)->getAddress(), (*it)->getName(), 
		(*it)->getAddress(), (*it)->getModule(),
		(*it)->getAddress());
	r += "   " + std::string(tmp);
    }

    for (std::map<addr_t, functions_t>::iterator it1 = calls.begin(); 
	 it1 != calls.end(); it1++) {
	for (functions_t::iterator it2 = it1->second.begin(); 
	     it2 != it1->second.end(); it2++) {
	    sprintf(tmp, "bb_%.8x -> func_%.8x [color=blue];\n", 
		    addr2bb[it1->first]->getAddress(), 
		    (*it2)->getAddress());
	    r += "   " + std::string(tmp);
	}
    }    

    for (Cfg::const_edge_iterator eit = edge_begin();
	 eit != edge_end(); eit++) {
	BasicBlockEdge *e = *eit;
	BasicBlock *source = e->getSource(), *target = e->getTarget();

	if (isSubComponentOf(source, target)) {
	    sprintf(tmp, "bb_%.8x -> bb_%.8x [color=purple];\n", 
		    e->getSource()->getAddress(), 
		    e->getTarget()->getAddress());
	    r += "   " + std::string(tmp);
	} else {
	    sprintf(tmp, "bb_%.8x -> bb_%.8x;\n", e->getSource()->getAddress(), 
		    e->getTarget()->getAddress());
	    r += "   " + std::string(tmp);
	}

    }

/*
    if (idoms[bb2vertex[bb1]] != cfg_traits::null_vertex()) {
	sprintf(tmp, "bb_%.8x -> bb_%.8x [color=\"cyan\"];\n", 
		basicblocks[idoms[bb2vertex[bb1]]]->getAddress(), 
		bb1->getAddress());
	r += "   " + std::string(tmp);
    }
// */

#if 0
    for (Cfg::const_bb_iterator bbit = bb_begin(); 
	 bbit != bb_end(); bbit++) {
	BasicBlock *c = getComponent(*bbit);
	if (c)
	    sprintf(tmp, "bb_%.8x -> bb_%.8x[color=pink,style=dashed];\n", 
		    (*bbit)->getAddress(), c->getAddress());
	r += "   " + std::string(tmp);
    }
#endif

    r += "}";

    return r;
}

std::string Cfg::vcg() {
    std::set<Function *> functions;
    std::string r = "";
    char tmp[1024];
    int j = 0;

    r = "graph: {\n";

    for (Cfg::const_bb_iterator bbit = bb_begin(); 
	 bbit != bb_end(); bbit++) {
	BasicBlock *bb = *bbit;
	if (entry == bb) {
	    sprintf(tmp, "node: { title: \"bb_%.8x\" "
		    "label: \"%.8x-%.8x (%d)\" color: green}\n", 
		    bb->getAddress(), bb->getAddress(), 
		    bb->getAddress() + bb->getSize(), j);
	} else if (exits.find(bb) != exits.end()) {
	    sprintf(tmp, "node: { title: \"bb_%.8x\" "
		    "label: \"%.8x-%.8x (%d)\" color: red}\n", 
		    bb->getAddress(), bb->getAddress(), 
		    bb->getAddress() + bb->getSize(), j);
	} else {
	    sprintf(tmp, "node: { title: \"bb_%.8x\" "
		    "label: \"%.8x-%.8x (%d)\"}\n", 
		    bb->getAddress(), bb->getAddress(), 
		    bb->getAddress() + bb->getSize(), j);
	}
	r += "   " + std::string(tmp);
	j++;
    }

    for (std::map<addr_t, functions_t>::iterator it = calls.begin(); 
	 it != calls.end(); it++) {
	functions.insert(it->second.begin(), it->second.end());
    }

    for (std::set<Function *>::iterator it = functions.begin();
	 it != functions.end(); it++) {
	sprintf(tmp, "node: { title: \"func_%.8x\" "
		"label: \"%s@%.8x [%s]\" color: blue}\n", 
		(*it)->getAddress(), (*it)->getName(), 
		(*it)->getAddress(), (*it)->getModule());
	r += "   " + std::string(tmp);
    }

    for (std::map<addr_t, functions_t>::iterator it1 = calls.begin(); 
	 it1 != calls.end(); it1++) {
	for (functions_t::iterator it2 = it1->second.begin(); 
	     it2 != it1->second.end(); it2++) {
	    sprintf(tmp, "edge: { sourcename: \"bb_%.8x\" "
		    "targetname: \"func_%.8x\" color: blue}\n", 
		    addr2bb[it1->first]->getAddress(), 
		    (*it2)->getAddress());
	    r += "   " + std::string(tmp);
	}
    }    

    for (Cfg::const_edge_iterator eit = edge_begin();
	 eit != edge_end(); eit++) {
	BasicBlockEdge *e = *eit;
	sprintf(tmp, "edge: { sourcename: \"bb_%.8x\" "
		"targetname: \"bb_%.8x\"}\n", e->getSource()->getAddress(), 
		e->getTarget()->getAddress());
	r += "   " + std::string(tmp);
    }

/*
    if (idoms[bb2vertex[bb1]] != cfg_traits::null_vertex()) {
	sprintf(tmp, "edge: { sourcename: \"bb_%.8x\" "
		"targetname: \"bb_%.8x\" color: cyan}\n", 
		basicblocks[idoms[bb2vertex[bb1]]]->getAddress(), 
		bb1->getAddress());
	r += "   " + std::string(tmp);
    }
// */

    r += "}";

    return r;
}

void Cfg::sanityCheck(bool aggressive) {
    cfg_t::vertex_iterator vi, ve;
    cfg_t::edge_iterator ei, ee;
    cfg_t::out_edge_iterator oei, oee;
    cfg_t::in_edge_iterator iei, iee;

    assert(boost::num_vertices(graph) == vertex_rev_map.size());
    assert(boost::num_edges(graph) == edge_rev_map.size());

    for (boost::tie(vi, ve) = boost::vertices(graph); vi != ve; vi++) {
	assert(vertex_rev_map.find(vertex_map[*vi]) != vertex_rev_map.end());
	assert(hasVertex(vertex_map[*vi]));
	assert(hasVertex(vertex_rev_map[vertex_map[*vi]]));
    }

    for (boost::tie(ei, ee) = boost::edges(graph); ei != ee; ei++) {
	assert(edge_rev_map.find(edge_map[*ei]) != edge_rev_map.end());
	assert(hasEdge(edge_map[*ei]->getSource(), edge_map[*ei]->getTarget()));
    }

    if (!aggressive)
	return;

    if (exits.empty())
	debug("Function %.8x has no exit nodes\n", 
	      function->getAddress());
#if 0
    assert_msg(!exits.empty(), "Function %.8x has no exit nodes", 
	       function->getAddress());
    for (const_bb_iterator it = bb_begin(); it != bb_end(); it++) {
	assert_msg(getNumSuccessors(*it) > 0 || 
		   exits.find(*it) != exits.end(),
		   "BasicBlock %.8x in function %.8x has not successor and is "
		   "not an exit node", (*it)->getAddress(), 
		   function->getAddress());
    }
#endif
}

int
disassemble(addr_t addr, byte_t *code, addr_t &next1, addr_t &next2,
	    xed_category_enum_t &category, char *buf = NULL,
	    size_t bufsize = 0) {
    xed_state_t dstate;
    xed_decoded_inst_t xedd;
    xed_error_enum_t xed_error;
    int len;

    xed_tables_init();

    xed_state_zero(&dstate);
    xed_state_init(&dstate,
                   XED_MACHINE_MODE_LEGACY_32, 
                   XED_ADDRESS_WIDTH_32b, 
                   XED_ADDRESS_WIDTH_32b);

    xed_decoded_inst_zero_set_mode(&xedd, &dstate);
    xed_error = xed_decode(&xedd, code, 16);
    if (xed_error == XED_ERROR_GENERAL_ERROR) {
	fprintf(stdout, "WARNING: Could not decode instruction at %.8x, length ignored is 1. Inserting (bad) in buffer.\n", addr);
	snprintf(buf, bufsize, "(bad)");
	return 1;
    }
    // assert(xed_error == XED_ERROR_NONE);
    if (xed_error != XED_ERROR_NONE) {
	fprintf(stdout, "WARNING: Error code from XED at %.8x: %d. Inserting (bad) in buffer.\n", addr, xed_error);
	snprintf(buf, bufsize, "(bad)");
	return 1;
    }

    const xed_inst_t *inst= xed_decoded_inst_inst(&xedd);
    category = xed_decoded_inst_get_category(&xedd);
    len = xed_decoded_inst_get_length(&xedd);
    if (buf)
	xed_decoded_inst_dump_att_format(&xedd, buf, bufsize, 0);

    next1 = next2 = 0xFFFFFFFF;

    switch (category) {
    case XED_CATEGORY_COND_BR:
	next1 = addr + len;
	if (xed_operand_name(xed_inst_operand(inst, 0)) == XED_OPERAND_RELBR)
	    next2 = addr + len + 
		xed_decoded_inst_get_branch_displacement(&xedd);
	else 
	    debug("!! Instruction %.8x uses an indirect jump target\n", addr);
	break;
    case XED_CATEGORY_UNCOND_BR:
	if (xed_operand_name(xed_inst_operand(inst, 0)) == XED_OPERAND_RELBR)
	    next1 = addr + len + 
		xed_decoded_inst_get_branch_displacement(&xedd);
	else if (xed_operand_name(xed_inst_operand(inst, 0)) == 
		 XED_OPERAND_IMM0) 
	    next1 =  xed_decoded_inst_get_unsigned_immediate(&xedd);
	else 
	    debug("!! Instruction %.8x uses an indirect jump target\n", addr);
	break;
    case XED_CATEGORY_RET:
	break;
    case XED_CATEGORY_CALL:
	next1 = addr + len;
	if (xed_operand_name(xed_inst_operand(inst, 0)) == XED_OPERAND_RELBR)
	    next2 = addr + len + 
		xed_decoded_inst_get_branch_displacement(&xedd);
	else if (xed_operand_name(xed_inst_operand(inst, 0)) ==
		 XED_OPERAND_IMM0) 
	    next2 =  xed_decoded_inst_get_unsigned_immediate(&xedd);
	else 
	    debug("!! Instruction %.8x uses an indirect call target\n", addr);
	break;
    default:
	next1 = addr + len;
	break;
    }

    return len; 
}

void Cfg::augmentCfg(std::list<std::pair<addr_t, addr_t> > &wlist, 
		     std::set<addr_t> &done, 
		     std::map<addr_t, Function *> &funcs, std::vector<std::pair<addr_t, addr_t>> &indirects) {
    addr_t curr, prev, next1, next2;
    int len = 0, pos = 0;
    bool isret;
    xed_category_enum_t category, prev_category;
    BasicBlock *prevbb = NULL;
    static char assembly[128];

    curr = wlist.front().first;
    prev = wlist.front().second;
    wlist.pop_front();

    if (prev)
	disassemble(prev, addr2bytes(this, prev), next1, next2,
		    prev_category);

    len = disassemble(curr, addr2bytes(this, curr), next1, next2,
		      category, assembly, sizeof(assembly));

    if (!prev || prev_category == XED_CATEGORY_RET 
	|| prev_category == XED_CATEGORY_CALL 
	|| prev_category == XED_CATEGORY_COND_BR 
	|| prev_category == XED_CATEGORY_UNCOND_BR)
	// Previous instruction is a tail
	pos = BASICBLOCK_HEAD;
    else if (category == XED_CATEGORY_RET 
	     || category == XED_CATEGORY_CALL 
	     || category == XED_CATEGORY_COND_BR 
	     || category == XED_CATEGORY_UNCOND_BR)
	// Current instruction is a tail
	pos = BASICBLOCK_TAIL;
    else
	// Anything else
	pos = BASICBLOCK_MIDDLE;

    debug2(" Statically processing instruction %.8x "
    	   "(%d bytes long, successor of %.8x, pos %d)\n", 
    	   curr, len, prev, pos);
    //debug2(" Statically processing instruction %.8x\n", 
    //   curr);

    // Add instruction to the CFG if not in there already
    addInstruction(curr, addr2bytes(this, curr), len, pos, prev, 
		   category == XED_CATEGORY_RET);

    // Add a call target if necessary
    if (category == XED_CATEGORY_CALL && next2 != 0xFFFFFFFF) {
	if (function->getProg()->isPlt(next2)) {
	    next2 = derefplt(curr, next2, 
			     function->getProg()->getBase(curr, ".got.plt"));
	}

	if (next2) {
	    if (funcs.find(next2) == funcs.end()) {
		// Function already seen
		funcs[next2] = new Function(next2);
		funcs[next2]->setPending(true);
	    }
	    addCall(curr, funcs.find(next2)->second);
	} else {
	    // This should not happen, but it happens and I don't know why!
	    debug("Invalid NULL call target\n");
	}
    }

    // Update the worklist
    if (done.find(curr) == done.end()) {
	if (next1 != 0xFFFFFFFF) {
	    debug2("\t adding %.8x to the worklist\n", next1);
	    wlist.push_back(std::pair<addr_t, addr_t>(next1, curr));
	}
	
	if (next2 != 0xFFFFFFFF && category != XED_CATEGORY_CALL) {
	    debug2("\t adding %.8x to the worklist\n", next2);
	    wlist.push_back(std::pair<addr_t, addr_t>(next2, curr));
	}
    }

    // Mask instruction as processed
    done.insert(curr);
}

// Statically augment the CFG. The process consists of two passes: (1)
// recursive traversal disassembly starting from the entry point, (2) recursive
// traversal starting from indirect control transfer instrutions
void Cfg::augmentCfg(addr_t start, Elf32_Addr *lbphr, Elf32_Addr *ubphr, int numsegs, uint32_t max_func_inst, uint32_t max_inst,
		     std::map<addr_t, Function *> &funcs, std::vector<std::pair<addr_t, addr_t>> &indirects) {
    std::list<std::pair<addr_t, addr_t> > wlist;
    std::list<std::pair<addr_t, addr_t> > indirect_wlist;
    std::set<addr_t> done;

    uint32_t inst_count = 0;
    int i;
    addr_t prev = 0;

    //Prog *prog = this->getFunction()->getProg();
    //Section *sec = prog->getSection(start);
    //if (!sec) {
    //    debug3("attempting to disassemble at a non-executable code segment.");
    //    return;
    //}
    
    debug2("Augmenting CFG of %.8x\n", start);

    if (strcmp(function->getName(), "exit") == 0 || 
	strcmp(function->getName(), "pthread_exit") == 0) {
	debug2("Skipping exit because we do not want to know what happens "
	       "after\n");
	clear();
	addInstruction(function->getAddress(), (byte_t *) "\xc3", 1, 
		       BASICBLOCK_HEAD, 0, true);
	entry = addr2bb[function->getAddress()];
	exits.insert(entry);
	return;
    }

    // First pass, recursive traversal disassembly
    wlist.push_back(std::pair<addr_t, addr_t>(start, prev));
    while (!wlist.empty()) {
        //Prog *prog = this->getFunction()->getProg();
        //Section *sec = prog->getSection(wlist.front().first);
	addr_t addr_n = wlist.front().first;
	bool ok = false;
	total_inst++;
	if (inst_count++ > max_func_inst || total_inst > max_inst) {
	    debug2("WARNING: Maximum instructions reached!\n");
	    break;
	} 
	if (ubphr != NULL && lbphr != NULL) {
	    for (i = 0; i < numsegs; i++) {
		if ((addr_n >= lbphr[i]) && (addr_n <= ubphr[i])) {
		    ok = true; 
		    break;
		}
	    }
	} else ok = true;

        if (!ok) {
            debug2("attempting to disassemble at a non-executable code segment at %.8x.\n", wlist.front().first);
            wlist.pop_front();
            continue;
        }
	augmentCfg(wlist, done, funcs, indirects);
    }

    debug2("First pass completed\n");

    // Second pass, disassembly targets of indirect calls and jumps that have
    // been reached dynamically but couldn't be reached during the first pass
    // for obvious reasons
    for (Cfg::const_bb_iterator bbit = bb_begin();
	 bbit != bb_end(); bbit++) {
	// The basic block hasn't been processed yet
	if (done.find((*bbit)->getAddress()) == done.end()) {
	    // Schedule the block for disassemly (one entry in the worklist for
	    // each predecessor)
	    for (Cfg::const_pred_iterator pit = pred_begin(*bbit);
		 pit != pred_end(*bbit); pit++) {
		addr_t tmp0;
		xed_category_enum_t tmp1;
		static char buf[128];

		prev = (*((*pit)->inst_end() - 1))->getAddress();
		disassemble(prev, addr2bytes(this, prev),
				 tmp0, tmp0, tmp1, buf, sizeof(buf));
		debug2("Found unprocessed basic block %.8x-%.8x "
		       "(reached from %.8x %s)\n", 
		       (*bbit)->getAddress(), 
		       (*bbit)->getAddress() + (*bbit)->getSize(), prev, buf);
		wlist.push_back(std::pair<addr_t, addr_t>
 				((*bbit)->getAddress(), prev));
	    }
	}
    }

    while (!wlist.empty()) {
	total_inst++;
	if (inst_count++ > max_func_inst || total_inst > max_inst) {
	    debug2("WARNING: Maximum instructions reached!\n");
	    break;
	}
	augmentCfg(indirect_wlist, done, funcs, indirects);
    }
    debug2("Second pass completed\n");

    // handle indirects    
    indirect_wlist.insert(indirect_wlist.end(), indirects.begin(), indirects.end());

    while (!indirect_wlist.empty()) {
        Prog *prog = this->getFunction()->getProg();
        Section *sec = prog->getSection(indirect_wlist.front().first);
	addr_t curr = indirect_wlist.front().first;
	addr_t prev = indirect_wlist.front().second;

	if (done.find(prev) == done.end()) {
	    wlist.pop_front();
	    continue;
	}
	
        if (!sec) {
            debug2("attempting to disassemble at a non-executable code segment at %.8x.\n", indirect_wlist.front().second);
            indirect_wlist.pop_front();
            continue;
        }
	augmentCfg(indirect_wlist, done, funcs, indirects);
    }

    debug2("Third pass (indirects) complete\n");

}

void Cfg::setExecuted(addr_t i) {
    assert(addr2bb.find(i) != addr2bb.end());
    BasicBlock *bb = addr2bb[i];
    assert(bb->getAddress() <= i && bb->getAddress() + bb->getSize() > i);

    for (instructions_t::const_iterator iit = bb->instructions.begin();
	 iit != bb->instructions.end(); iit++) {
	if ((*iit)->getAddress() == i) {
	    (*iit)->setExecuted();
	    return;
	}
    }

    assert(0);
}

bool Cfg::isExecuted(addr_t i) {
    assert_msg(addr2bb.find(i) != addr2bb.end(), "%.8x not in %.8x", i, entry->getAddress());
    BasicBlock *bb = addr2bb[i];
    assert(bb->getAddress() <= i && bb->getAddress() + bb->getSize() > i);

    for (instructions_t::const_iterator iit = bb->instructions.begin();
	 iit != bb->instructions.end(); iit++) {
	if ((*iit)->getAddress() == i) {
	    return (*iit)->isExecuted();
	}
    }

    assert(0);
    return false;
}

// Remove self loops in the graph to simplify abstract interpretation
void Cfg::removeSelfLoops() {
    bool done = !can_have_self_loops;

    while (!done) {
	done = true;

	for (Cfg::const_bb_iterator bbit = bb_begin(); 
	     bbit != bb_end(); bbit++) {
	    if (hasEdge(*bbit, *bbit)) {
		// Found a self loop
		debug2("Detected self loop in %.8x (%.8x -> %.8x)\n", 
		       function->getAddress(), (*bbit)->getAddress(), 
		       (*bbit)->getAddress());

		// Create a new empty basic block
		BasicBlock *dummybb = addBasicBlock(0);

		// Remove the self loop
		unlinkBasicBlocks(*bbit, *bbit);

		// Build a list of predecessors to process (can't modify edges
		// during the iteration)
		std::list<BasicBlock *> preds;
		for (Cfg::const_pred_iterator pbbit = pred_begin(*bbit); 
		     pbbit != pred_end(*bbit); pbbit++) {
		    preds.push_back(*pbbit);
		}

		// Remove old links and add new ones
		while (!preds.empty()) {
		    unlinkBasicBlocks(preds.front(), *bbit);
		    linkBasicBlocks(preds.front(), dummybb);
		    preds.pop_front();
		}

		// Link the dummy bb with the one with the self loop and vice
		// versa
		linkBasicBlocks(dummybb, *bbit);
		linkBasicBlocks(*bbit, dummybb);

		// Update the entry point of the cfg if needed
		if (entry == *bbit) {
		    setEntry(dummybb);
		}

		done = false;
		break;
	    }
	}
    }

    can_have_self_loops = false;
}

functions_t::const_iterator Cfg::call_targets_begin(const BasicBlock &bb) { 
    assert(!bb.instructions.empty());

    Instruction *i = bb.instructions.back();
    assert(i->isCall());
    return calls.find(i->getAddress() & ~0x80000000)->second.begin();
}

functions_t::const_iterator Cfg::call_targets_end(const BasicBlock &bb) {
    assert(!bb.instructions.empty());

    Instruction *i = bb.instructions.back();
    assert(i->isCall());
    return calls.find(i->getAddress() & ~0x80000000)->second.end();
}

std::string Cfg::wto2string() {
    int j = 0, k;
    char buf[32];
    std::string r = "";

    assert(wto_computed);

    for (const_wto_iterator bbit = wto_begin(); bbit !=
	     wto_end(); bbit++) {
	BasicBlock *bb = getVertex(*bbit); 
	
	k = j;
	while (k > 0 && getComponentNo(bb) < k--) {
	    r += ")";
	}

	sprintf(buf, " %s", (j < getComponentNo(bb)) ? "(" : "");
	r += buf;
	sprintf(buf, "%.8x|%d", bb->getAddress(), getComponentNo(bb));
	r += buf;
	
	j = getComponentNo(bb);
    }

    k = j;
    while (k-- > 0) {
	r += ")";
    }

    return r;
}

// ****************************************************************************

BasicBlock::~BasicBlock() {
    for (instructions_t::iterator it = instructions.begin();
	 it != instructions.end(); it++) {
	delete *it;
    }
}

void BasicBlock::addInstruction(Instruction *i) {
    instructions.push_back(i);
    i->basicblock = this;
    size += i->getSize();
}

size_t BasicBlock::getInstructionsNo() {
    return instructions.size();
}

addr_t BasicBlock::getAddress() {
    return address;
}

size_t BasicBlock::getSize() {
    return size;
}

Cfg *BasicBlock::getCfg() {
    return cfg;
}

void BasicBlock::decode() {
    if (!decoded) {
	for (instructions_t::iterator it = instructions.begin(); 
	     it != instructions.end(); it++) {
	    (*it)->decode();
	}
    }

    decoded = true;
}

int BasicBlock::getNumPredecessors() {
    return cfg->getNumPredecessors(this);
}

bool BasicBlock::isCall() {
    decode();
    return !instructions.empty() && instructions.back()->isCall();
}

bool BasicBlock::isReturn() {
    decode();
    return !instructions.empty() && instructions.back()->isCall();
}

bool BasicBlock::isExecuted() {
    for (instructions_t::iterator it = instructions.begin(); 
	 it != instructions.end(); it++) {
	if ((*it)->isExecuted())
	    return true;
    }

    return false;
}

Instruction *BasicBlock::getInstruction(addr_t i) {
    assert(i >= address && i < address + size);
    for (instructions_t::iterator it = instructions.begin();
	 it != instructions.end(); it++) {
	if ((*it)->getAddress() == i)
	    return *it;
    }

    assert(0);
    fprintf(stderr, "ERROR: Could not find instruction %.8x", i);
    return 0;
}

// ****************************************************************************

BasicBlockEdge::BasicBlockEdge(BasicBlock *s, BasicBlock *t) {
    source = s;
    target = t;
}

BasicBlockEdge::~BasicBlockEdge() {
    ;
}

BasicBlock *BasicBlockEdge::getSource() {
    return source;
}

BasicBlock *BasicBlockEdge::getTarget() {
    return target;
}

std::ostream& operator<<(std::ostream& os, const BasicBlock& B) {
    for (instructions_t::const_iterator I = B.inst_begin(), E =
            B.inst_end(); I != E; ++I) {
        os << **I << "----- next BB -----" << std::endl;
    }
    return os;
}

// Local Variables: 
// mode: c++
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
