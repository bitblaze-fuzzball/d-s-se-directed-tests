#include "func.h"
#include "cfg.h"
#include "callgraph.h"
#include "warning.h"
#include "prog.h"
#include "InterProcCFG.h"
#include "serialize.h"

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>

#include <stdio.h>
#include <vector>
#include <string>
#include <map>

using namespace std;
using namespace boost;

static const char *func_unique_name(Function *func) {
    static char buf[256];
    const char *name = func->getName();
    if (!strcmp(name, "unknown")) {
	snprintf(buf, sizeof(buf), "unknown_0x%08x", func->getAddress());
	return buf;
    } else {
	return name;
    }
}

#define DISTANCE_INFINITY 1000000L
#define G2_ADDR(x) ((x) | 0x80000000)
#define G1_ADDR(x) ((x) & ~0x80000000)
#define IS_G1(x) (!((x)->getAddress() & 0x80000000))
#define IS_G2(x) ((x)->getAddress() & 0x80000000)

void
InterProcCFG::to_vcg(ostream &out) {
    char tmp[1024];
    bool flat = false;
    property_map<IPCFGraph, vertex_index_t>::type
 	index = get(vertex_index_t(), ipcfg);
    property_map<IPCFGraph, bb_ptr_t>::type bb_ptr = get(bb_ptr_t(), ipcfg);
    property_map<IPCFGraph, edge_type_t>::type edge_type
	= get(edge_type_t(), ipcfg);
    property_map<IPCFGraph, vertex_distance_t>::type
 	distance = get(vertex_distance_t(), ipcfg);
    property_map<IPCFGraph, paths_to_header_t>::type
 	pth = get(paths_to_header_t(), ipcfg);
    out << "graph: {" << endl;
    if (flat) {
	graph_traits<IPCFGraph>::vertex_iterator vi, vi_end;
	for (tie(vi, vi_end) = vertices(ipcfg); vi != vi_end; ++vi) {
	    int bb_num = index[*vi];
	    BasicBlock *bb = bb_ptr[*vi];
	    sprintf(tmp, "node: { title: \"bb%d\" "
		    "label: \"%.8x-%.8x [%lld]\"}",
		    bb_num, bb->getAddress(), 
		    bb->getAddress() + bb->getSize() - 1,
		    distance[*vi]);
	    out << tmp << endl;
	}
    } else {
	for (subgraph_map::iterator fi = subgraphs.begin();
	     fi != subgraphs.end(); ++fi) {
	    string func_name = (*fi).first;
	    vector<IPCFGNode> &nodes = (*fi).second;
	    out << "graph: { title: \"" << func_name << "\"" << endl;
	    out << "    status: black" << endl;
	    for (vector<IPCFGNode>::iterator ni = nodes.begin();
		 ni != nodes.end(); ++ni) {
		int bb_num = index[*ni];
		BasicBlock *bb = bb_ptr[*ni];
		const char *options;
		if (interesting_loops.find(bb->getAddress()) !=
		    interesting_loops.end()) {
		    options = " color: khaki";
		} else if (warning && warning->slice_find(bb->getAddress())) {
		    options = " color: orchid";
		} else if (is_loop_exit_cond(bb)) {
		    options = " color: lightgreen";
		} else {
		    options = "";
		}
		sprintf(tmp, "    node: { title: \"bb%d\" "
			"label: \"%.8x-%.8x [%lld] (%ld)\"%s}",
			bb_num, bb->getAddress(), 
			bb->getAddress() + bb->getSize() - 1,
			distance[*ni], pth[*ni], options);
		out << tmp << endl;
	    }
	    out << "}" << endl;
	}
    }
    graph_traits<IPCFGraph>::edge_iterator ei, ei_end;
    for (tie(ei, ei_end) = edges(ipcfg); ei != ei_end; ++ei) {
	int bb_num1 = index[target(*ei, ipcfg)];
	int bb_num2 = index[source(*ei, ipcfg)];
	const char *options;
	switch (edge_type[*ei]) {
	case INTRA_EDGE:
	    options = "color: black priority: 10"; break;
	case CALL_EDGE:
	    options = "color: blue priority: 10"; break;
	case RETURN_EDGE:
	    options = "color: red priority: 1"; break;
	case CALL_TO_RET_EDGE:
	    options = "color: lightgray priority: 10"; break;
	case BACK_EDGE:
	    options = "color: darkgreen priority: 1"; break;
	default: 
        assert(0);
        options = "color: black priority: 5"; break;
	}
 	sprintf(tmp, "edge: { sourcename: \"bb%d\" targetname: \"bb%d\" %s}",
		bb_num1, bb_num2, options);
 	out << tmp << endl;
    }
    if (0 /* show post-dominator edges */) {
	graph_traits<IPCFGraph>::vertex_iterator vi, vi_end;
	for (tie(vi, vi_end) = vertices(ipcfg); vi != vi_end; ++vi) {
	    int bb_num1 = index[*vi];
	    BasicBlock *bb = bb_ptr[*vi];
	    BasicBlock *ipd = bb->getCfg()->getPostDominator(bb);
	    if (ipd) {
		int bb_num2 = index[bb_to_node[ipd]];
		sprintf(tmp, "edge: { sourcename: \"bb%d\" "
			"targetname: \"bb%d\""
			" color: purple}", bb_num1, bb_num2);	
		out << tmp << endl;
	    }
	}
    }
    if (0 /* show component edges */) {
	graph_traits<IPCFGraph>::vertex_iterator vi, vi_end;
	for (tie(vi, vi_end) = vertices(ipcfg); vi != vi_end; ++vi) {
	    int bb_num1 = index[*vi];
	    BasicBlock *bb = bb_ptr[*vi];
	    BasicBlock *leader = bb->getCfg()->getComponent(bb);
	    if (bb != leader) {
		/* Pink edge from node to component leader */
		int bb_num2 = index[bb_to_node[leader]];
		sprintf(tmp, "edge: { sourcename: \"bb%d\" "
			"targetname: \"bb%d\""
			" color: pink}", bb_num1, bb_num2);	
		out << tmp << endl;
	    } else {
		/* Orange edge from leader to enclosing leader */
		BasicBlock *leader2 =
		    bb->getCfg()->getEnclosingComponent(leader);
		bb_num1 = index[bb_to_node[leader]];
		int bb_num2 = index[bb_to_node[leader2]];
		sprintf(tmp, "edge: { sourcename: \"bb%d\" "
			"targetname: \"bb%d\""
			" color: orange}", bb_num1, bb_num2);	
		out << tmp << endl;
	    }
	}
    }
    out << "}" << endl;
}

void
InterProcCFG::copy_nodes_to_ipcfg(Function *func) {
    property_map<IPCFGraph, bb_ptr_t>::type bb_ptr = get(bb_ptr_t(), ipcfg);
    property_map<IPCFGraph, paths_to_header_t>::type
 	pth = get(paths_to_header_t(), ipcfg);
    
    string func_name = func_unique_name(func);
    Cfg *cfg = func->getCfg();

    cfg->decode();
    basicblocks_t exits(cfg->exits_begin(), cfg->exits_end());
    if (0)
	cfg->computePostDominators(&exits);
    if (0 && exits.size() != 1) {
	fprintf(stderr, "Function %s (%08x) has %d exits\n",
		func_name.c_str(), func->getAddress(), exits.size());
    }

    for (Cfg::const_bb_iterator bbit = cfg->bb_begin();
	 bbit != cfg->bb_end(); bbit++) {
	BasicBlock *bb = *bbit;

	IPCFGNode n = bb_to_node[bb] = add_vertex(ipcfg);
	bb_ptr[n] = bb;
	pth[n] = -1;
	subgraphs[func_name].push_back(n);
    }
}

void
InterProcCFG::make_call_ret_edges(Instruction *i, bool include_returns) {
    property_map<IPCFGraph, edge_type_t>::type edge_type
	= get(edge_type_t(), ipcfg);
    property_map<IPCFGraph, edge_weight_t>::type edge_weight
	= get(edge_weight_t(), ipcfg);
    BasicBlock *bb = i->getBasicBlock();
    Cfg *cfg = bb->getCfg();
    // The call should be the last insn in the block
    assert(i->getAddress() + i->getSize() ==
	   bb->getAddress() + bb->getSize());
    // The block should fall through to a single successor
    BasicBlock *bb_next = 0;
    for (Cfg::const_succ_iterator si = cfg->succ_begin(bb);
	 si != cfg->succ_end(bb); ++si) {
	assert_msg(bb_next == 0, "%.8x\n", bb_next->getAddress()); 
	// Loop should not repeat
	bb_next = *si;
    }
    int seen_callees = 0;
    IPCFGEdge e; bool okay;
    bool multiple_callees = false;
    {
	functions_t::const_iterator fit = cfg->call_targets_begin(G1_ADDR(i->getAddress()));
	if (fit != cfg->call_targets_end(G1_ADDR(i->getAddress()))) {
	    ++fit;
	    if (fit != cfg->call_targets_end(G1_ADDR(i->getAddress())))
		multiple_callees = true;
	}
    }
    for (functions_t::const_iterator fit = cfg->call_targets_begin(G1_ADDR(i->getAddress()));
	 fit != cfg->call_targets_end(G1_ADDR(i->getAddress())); fit++) {
	Function *callee = *fit;
	Cfg *callee_cfg = callee->getCfg();
	BasicBlock *callee_entry = callee_cfg->getEntry();
	if (!callee_entry)
	    continue;
	seen_callees++;
	tie(e, okay) =
	    add_edge(bb_to_node[callee_entry], bb_to_node[bb], ipcfg);
	assert(okay);
	edge_type[e] = CALL_EDGE;
	edge_weight[e] = (multiple_callees ? 1 /* 100 */ : 1);
	
	if (include_returns) {
	    for (Cfg::const_exit_iterator xit = callee_cfg->exits_begin();
		 xit != callee_cfg->exits_end(); ++xit) {
		BasicBlock *callee_exit = *xit;
		tie(e, okay) = add_edge(bb_to_node[bb_next],
					bb_to_node[callee_exit], ipcfg);
		assert(okay);
		edge_type[e] = RETURN_EDGE;
		edge_weight[e] = 1;
	    }
	}
    }
    if (!seen_callees) {
	// None of the callees exist in our representation, so add
	// a call-to-ret edge unconditionally.
	debug2("Adding call-to-ret %.8x --> %.8x\n", bb->getAddress(), bb_next->getAddress());
	tie(e, okay) = add_edge(bb_to_node[bb_next], bb_to_node[bb], ipcfg);
	assert(okay);
	edge_type[e] = CALL_TO_RET_EDGE;
	edge_weight[e] = 1;
    }
}

void
InterProcCFG::copy_edges_to_ipcfg(Function *func,
				  bool include_returns,
				  bool include_call_to_rets,
				  bool include_calls) {
    property_map<IPCFGraph, edge_type_t>::type edge_type
	= get(edge_type_t(), ipcfg);
    property_map<IPCFGraph, edge_weight_t>::type edge_weight
	= get(edge_weight_t(), ipcfg);

    const char *func_name = func_unique_name(func);
    Cfg *cfg = func->getCfg();

    for (Cfg::const_edge_iterator eit = cfg->edge_begin(); 
	 eit != cfg->edge_end(); eit++) {
	BasicBlockEdge *bbe = *eit;
	BasicBlock *bb1 = bbe->getSource();
	BasicBlock *bb2 = bbe->getTarget();
	bool is_call = bb1->isCall();
	IPCFGEdge e;
	bool okay;
	if (is_call && include_call_to_rets) {
	    tie(e, okay) = add_edge(bb_to_node[bb2], bb_to_node[bb1], ipcfg);
	    assert(okay);
	    edge_type[e] = CALL_TO_RET_EDGE;
	    edge_weight[e] = DISTANCE_INFINITY;
	} else if (!is_call) {
	    tie(e, okay) = add_edge(bb_to_node[bb2], bb_to_node[bb1], ipcfg);
	    assert(okay);
	    int weight;
	    if (cfg->getComponent(bb1) == bb2) {
		edge_type[e] = BACK_EDGE;
		weight = DISTANCE_INFINITY;
	    } else {
		edge_type[e] = INTRA_EDGE;
		if (cfg->getNumSuccessors(bb1) > 1) {
		    // branch
		    weight = 1 /* 100 */;
		} else {
		    weight = 1;
		}
	    }
	    edge_weight[e] = weight;
	}
    }

    for (Cfg::const_bb_iterator bbit = cfg->bb_begin(); 
	 bbit != cfg->bb_end(); bbit++) {
	BasicBlock *bb = *bbit;
	for (instructions_t::const_iterator iit = bb->inst_begin(); 
	     iit != bb->inst_end(); iit++) {
	    Instruction *i = *iit;
	    if (i->isCall()) {
		make_call_ret_edges(i, include_returns);
	    }
	}
    }
}

long InterProcCFG::compute_pth(bool first_call, BasicBlock *bb) {
    property_map<IPCFGraph, paths_to_header_t>::type
 	pth = get(paths_to_header_t(), ipcfg);
    Cfg *cfg = bb->getCfg();
    BasicBlock *leader = cfg->getComponent(bb);
    IPCFGNode node = bb_to_node[bb];
    if (bb == leader && !first_call)
	return 1;
    else if (cfg->isExit(bb)) {
	pth[node] = 1;
	return 1;
    }
    if (pth[node] != -1)
	return pth[node];
    long total = 0;
    for (Cfg::const_succ_iterator si = cfg->succ_begin(bb);
	 si != cfg->succ_end(bb); ++si) {
	BasicBlock *succ_bb = *si;
	if (cfg->getComponent(succ_bb) == leader) {
	    IPCFGNode succ_node = bb_to_node[succ_bb];
	    total += compute_pth(false, succ_bb);
	} else {
	    total++;
	}
    }
    pth[node] = total;
    return total;
}

void InterProcCFG::set_target_addr(addr_t target_addr,
				   map<addr_t, Function *> &functions) {
    BasicBlock *target_bb = lookup_bb(G2_ADDR(target_addr), functions);
    assert(target_bb);
    target_node = bb_to_node[target_bb];
    return;
}

void InterProcCFG::set_target_warning(Warning *w,
				      map<addr_t, Function *> &functions) {
    set_target_addr(w->getAddress(), functions);
    warning = w;
    for (Warning::slice_iterator sit = w->slice_begin(); 
	 sit != w->slice_end(); ++sit) {
	BasicBlock *slice_bb = lookup_bb(*sit, functions);
	assert(slice_bb);
	Cfg *cfg = slice_bb->getCfg();
	BasicBlock *leader = cfg->getComponent(slice_bb);
	if (leader == cfg->getEntry()) {
	    //fprintf(stderr, "BB 0x%08x is not in an (intra-proc) loop\n",
	    //    slice_bb->getAddress());
	} else {
	    //fprintf(stderr, "BB 0x%08x is in a loop with head 0x%08x\n",
	    //	    slice_bb->getAddress(), leader->getAddress());
	    interesting_loops.insert(leader->getAddress());
	}
    }
    return;
}

void
InterProcCFG::compute_shortest_paths() {
    property_map<IPCFGraph, vertex_distance_t>::type
 	distance = get(vertex_distance_t(), ipcfg);
    dijkstra_shortest_paths(ipcfg, target_node,
			    distance_map(distance));

}

BasicBlock*
InterProcCFG::lookup_bb(addr_t addr,
			map<addr_t, Function *> &functions) {
    BasicBlock *the_bb = 0;

    tr1::unordered_map<addr_t, BasicBlock*>::const_iterator cache_it =
	bb_lookup_cache.find(addr);
    if (cache_it != bb_lookup_cache.end())
	return (*cache_it).second;

    for (map<addr_t, Function *>::iterator it = functions.begin(); 
	 it != functions.end(); it++) {
	Function *func = (*it).second;

	for (Cfg::const_bb_iterator bbit = func->getCfg()->bb_begin(); 
	     bbit != func->getCfg()->bb_end(); bbit++) {
	    BasicBlock *bb = *bbit;

	    if (addr >= bb->getAddress() &&
		addr < bb->getAddress() + bb->getSize()) {
		// XXX check and re-enable this assertion.
		// assert(the_bb == 0); // should only appear once
		the_bb = bb;
	    }
	}
    }
    bb_lookup_cache[addr] = the_bb;
    return the_bb;
}

long long
InterProcCFG::lookup_distance(addr_t source_addr,
			      map<addr_t, Function *> &functions) {
    property_map<IPCFGraph, vertex_distance_t>::type
 	distance = get(vertex_distance_t(), ipcfg);
    BasicBlock *source_bb = lookup_bb(source_addr, functions);
    if (source_bb)
	return distance[bb_to_node[source_bb]];
    else
	return -1;
}

int
InterProcCFG::lookup_influence(addr_t addr,
			       map<addr_t, Function *> &functions,
			       influence_map_t *influence) {
    BasicBlock *bb = lookup_bb(addr, functions);
    if (bb)
	return (*influence)[bb].size();
    else
	return 0;
}

long
InterProcCFG::lookup_pth(addr_t addr,
			 map<addr_t, Function *> &functions) {
    property_map<IPCFGraph, paths_to_header_t>::type
 	pth = get(paths_to_header_t(), ipcfg);
    BasicBlock *bb = lookup_bb(addr, functions);
    if (bb)
	return pth[bb_to_node[bb]];
    else
	return -1;
}


bool
InterProcCFG::is_loop_exit_cond(addr_t branch_addr,
				map<addr_t, Function *> &functions) {
    BasicBlock *bb = lookup_bb(branch_addr, functions);
    if (!bb)
	return false; // Hmm, should we signal this separately?
    else
	return is_loop_exit_cond(bb);
}

/* A basic block is a loop exit condition if it has at least one
   successor that remains within the loop (its target is in the same
   SCC, perhaps a sub-SCC for a nested loop) and at least one that
   isn't (an exit edge). */
bool
InterProcCFG::is_loop_exit_cond(BasicBlock *bb) {
    Cfg *cfg = bb->getCfg();
    BasicBlock *leader = cfg->getComponent(bb);
    bool seen_inside = false;
    bool seen_outside = false;
    for (Cfg::const_succ_iterator si = cfg->succ_begin(bb);
	 si != cfg->succ_end(bb); ++si) {
	BasicBlock *succ_bb = *si;
	if (cfg->withinComponent(leader, succ_bb))
	    seen_inside = true;
	else
	    seen_outside = true;
	if (seen_inside && seen_outside)
	    break;
    }
    return seen_inside && seen_outside;
}

bool
InterProcCFG::edge_is_loop_exit(addr_t from_addr, addr_t to_addr,
				map<addr_t, Function *> &functions) {
    BasicBlock *from_bb = lookup_bb(from_addr, functions);
    BasicBlock *to_bb = lookup_bb(to_addr, functions);
    if (!from_bb || !to_bb)
	return false;
    else
	return edge_is_loop_exit(from_bb, to_bb);
}

bool
InterProcCFG::edge_is_loop_exit(BasicBlock *from_bb, BasicBlock *to_bb) {
    assert(is_loop_exit_cond(from_bb));
    Cfg *cfg = from_bb->getCfg();
    assert(to_bb->getCfg() == cfg);
    bool seen = false;
    for (Cfg::const_succ_iterator si = cfg->succ_begin(from_bb);
	 si != cfg->succ_end(from_bb); ++si) {
	BasicBlock *succ_bb = *si;
	if (succ_bb->getAddress() == to_bb->getAddress()) {
	    seen = true;
	    break;
	}
    }
    assert(seen);
    BasicBlock *leader = cfg->getComponent(from_bb);
    return !cfg->withinComponent(leader, to_bb);
}

bool
InterProcCFG::is_interesting_loop(addr_t addr,
				  map<addr_t, Function *> &functions) {
    BasicBlock *bb = lookup_bb(addr, functions);
    Cfg *cfg = bb->getCfg();
    BasicBlock *leader = cfg->getComponent(bb);
    bool is_interesting =
	(interesting_loops.find(leader->getAddress()) !=
	 interesting_loops.end());
    return is_interesting;
}

addr_t
InterProcCFG::get_component(addr_t addr,
			    map<addr_t, Function *> &functions) {
    BasicBlock *bb = lookup_bb(addr, functions);
    if (!bb)
	return 0;
    Cfg *cfg = bb->getCfg();
    BasicBlock *leader = cfg->getComponent(bb);
    return leader->getAddress();
}

bool
InterProcCFG::same_bb(addr_t addr1, addr_t addr2,
		      map<addr_t, Function *> &functions) {
    BasicBlock *bb1 = lookup_bb(addr1, functions);
    BasicBlock *bb2 = lookup_bb(addr2, functions);
    if (!bb1 || !bb2)
	return false;
    return bb1 == bb2;
}

InterProcCFG::InterProcCFG(map<addr_t, Function *> &functions)
{
    warning = 0;
    for (map<addr_t, Function *>::iterator it = functions.begin(); 
	 it != functions.end(); it++) {
	Function *func = it->second;
	const char *func_name = func_unique_name(func);

	// Decode the function
	debug("[FUNC] %.8x %s@%s\n", func->getAddress(), 
	       func_name, func->getModule());

	copy_nodes_to_ipcfg(func);

	func->getCfg()->computeWeakTopologicalOrdering();
    }
    
    for (map<addr_t, Function *>::iterator it = functions.begin(); 
	 it != functions.end(); it++) {
	Function *func = it->second;

	copy_edges_to_ipcfg(func, true, true, true);

	Cfg *cfg = func->getCfg();
	for (Cfg::const_bb_iterator bbit = cfg->bb_begin();
	     bbit != cfg->bb_end(); bbit++) {
	    BasicBlock *bb = *bbit;

	    compute_pth(true, bb);
	}
    }
}

void InterProcCFG::remove_call_edges_from_g1() {
    property_map<IPCFGraph, bb_ptr_t>::type bb_ptr = get(bb_ptr_t(), ipcfg);
    property_map<IPCFGraph, edge_type_t>::type edge_type
	= get(edge_type_t(), ipcfg);
    property_map<IPCFGraph, edge_weight_t>::type edge_weight
	= get(edge_weight_t(), ipcfg);
    list<IPCFGEdge> toremove;

    graph_traits<IPCFGraph>::edge_iterator ei, ei_end;
    for (tie(ei, ei_end) = edges(ipcfg); ei != ei_end; ++ei) {
	BasicBlock *caller = bb_ptr[target(*ei, ipcfg)];
	BasicBlock *callee = bb_ptr[source(*ei, ipcfg)];

	if (edge_type[*ei] == CALL_EDGE && IS_G1(caller) && IS_G1(callee)) {
	    debug2("Removing call edge %.8x --> %.8x\n", caller->getAddress(),
		   callee->getAddress());
	    toremove.push_back(*ei);
	}
    }

    while (!toremove.empty()) {
	IPCFGEdge e = toremove.front();
	toremove.pop_front();
	remove_edge(e, ipcfg);
    }
}

void InterProcCFG::remove_ret_edges_from_g2() {
    property_map<IPCFGraph, bb_ptr_t>::type bb_ptr = get(bb_ptr_t(), ipcfg);
    property_map<IPCFGraph, edge_type_t>::type edge_type
	= get(edge_type_t(), ipcfg);
    property_map<IPCFGraph, edge_weight_t>::type edge_weight
	= get(edge_weight_t(), ipcfg);
    list<IPCFGEdge> toremove;

    graph_traits<IPCFGraph>::edge_iterator ei, ei_end;
    for (tie(ei, ei_end) = edges(ipcfg); ei != ei_end; ++ei) {
	BasicBlock *src = bb_ptr[target(*ei, ipcfg)];
	BasicBlock *dst = bb_ptr[source(*ei, ipcfg)];

	if (edge_type[*ei] == RETURN_EDGE && IS_G2(src) && IS_G2(dst)) {
	    debug2("Removing return edge %.8x --> %.8x\n", src->getAddress(),
		   dst->getAddress());
	    toremove.push_back(*ei);
	}
    }

    while (!toremove.empty()) {
	IPCFGEdge e = toremove.front();
	toremove.pop_front();
	remove_edge(e, ipcfg);
    }
}

void InterProcCFG::add_call_ret_edges_to_g1_and_g2(costmap &costs,
						   map<addr_t, Function *> 
						   &functions) {
    property_map<IPCFGraph, bb_ptr_t>::type bb_ptr = get(bb_ptr_t(), ipcfg);
    property_map<IPCFGraph, edge_type_t>::type edge_type
	= get(edge_type_t(), ipcfg);
    property_map<IPCFGraph, edge_weight_t>::type edge_weight
	= get(edge_weight_t(), ipcfg);

    list<BasicBlock *> toadd;

    graph_traits<IPCFGraph>::edge_iterator ei, ei_end;
    for (tie(ei, ei_end) = edges(ipcfg); ei != ei_end; ++ei) {
	BasicBlock *src = bb_ptr[target(*ei, ipcfg)];
	BasicBlock *dst = bb_ptr[source(*ei, ipcfg)];
	
	// Add an edge between the callsite in g1 and the callsite in g2
	if (edge_type[*ei] == CALL_TO_RET_EDGE) {
	    assert(src->isCall());

	    debug2("Found call-to-ret edge %.8x --> %.8x\n", src->getAddress(),
		   dst->getAddress());

	    functions_t::const_iterator cit;
	    Function *f = NULL;
	    for (cit = src->getCfg()->call_targets_begin(*src);
		 cit != src->getCfg()->call_targets_end(*src); ++cit) {
		debug("%.8x\n", *cit ? (*cit)->getAddress() : NULL);
		assert(!f);
		f = *cit;
	    }

#if 1
	    IPCFGEdge e = *ei;
	    bool okay; 

	    if (f) {
		// Connect the callsite with the return site
		debug2("Creating call-to-ret edge %.8x --> %.8x (%s %lld)\n", 
		       src->getAddress(), dst->getAddress(), f->getName(),
		       costs[f]);
		edge_weight[e] = costs[f];

		if (IS_G1(src) && IS_G1(dst)) {
		    dst = lookup_bb(G2_ADDR(src->getAddress()), functions);
		    assert(dst);
		    toadd.push_back(src);
		}
	    } else {
		debug("Call without targets @ %.8x!!!!\n", src->getAddress());
	    }
#endif
	}
    }

    while (!toadd.empty()) {
	BasicBlock *src, *dst;
	src = toadd.front();
	toadd.pop_front();

	IPCFGEdge e;
	dst = lookup_bb(G2_ADDR(src->getAddress()), functions);
	bool okay;

	debug2("Creating call-to-call edge %.8x --> %.8x (0)\n", 
	       src->getAddress(), dst->getAddress());
	tie(e, okay) = add_edge(bb_to_node[dst], bb_to_node[src], 
				ipcfg);
	assert(okay);
	edge_weight[e] = 0;
    }
}

///////////////////////////////////////////////////////////////////////////////

IntraProcCFG::IntraProcCFG(Function *f) {
    func = f;
    cfg = f->getCfg();

    exitnode = cfg->addBasicBlock(0);

    for (Cfg::const_exit_iterator eit = cfg->exits_begin();
	 eit != cfg->exits_end(); ++eit) {
	cfg->linkBasicBlocks(*eit, exitnode);
    }

    copy_nodes_to_ipcfg(func);
    func->getCfg()->computeWeakTopologicalOrdering();
    copy_edges_to_ipcfg(func, false, true, false);
}

IntraProcCFG::~IntraProcCFG() {
    for (Cfg::const_exit_iterator eit = cfg->exits_begin();
	 eit != cfg->exits_end(); ++eit) {
	cfg->unlinkBasicBlocks(*eit, exitnode);
    }    
}

void IntraProcCFG::add_calls_costs_to_edges(costmap &costs) {
    property_map<IPCFGraph, bb_ptr_t>::type bb_ptr = get(bb_ptr_t(), ipcfg);
    property_map<IPCFGraph, edge_type_t>::type edge_type
	= get(edge_type_t(), ipcfg);
    property_map<IPCFGraph, edge_weight_t>::type edge_weight
	= get(edge_weight_t(), ipcfg);

    graph_traits<IPCFGraph>::edge_iterator ei, ei_end;
    for (tie(ei, ei_end) = edges(ipcfg); ei != ei_end; ++ei) {
	if (edge_type[*ei] == CALL_TO_RET_EDGE) {
	    long long mincost = DISTANCE_INFINITY;
	    BasicBlock *caller = bb_ptr[target(*ei, ipcfg)];
	    BasicBlock *retaddr = bb_ptr[source(*ei, ipcfg)];
	    functions_t::const_iterator it;

	    for (it = cfg->call_targets_begin(*caller);
		 it != cfg->call_targets_end(*caller); ++it) {
		mincost = mincost < costs[*it] ? mincost : costs[*it];
	    }

	    debug2("Assigning cost %lld to edge %.8x --> %.8x\n", mincost,
		   caller->getAddress(), retaddr->getAddress());
	    edge_weight[*ei] = mincost;
	}
    }
}

long long IntraProcCFG::compute_shortest_path_to_exit() {
    property_map<IPCFGraph, vertex_distance_t>::type
 	distance = get(vertex_distance_t(), ipcfg);

    dijkstra_shortest_paths(ipcfg, bb_to_node[exitnode], 
			    distance_map(distance));

    return distance[bb_to_node[cfg->getEntry()]];
}

void compute_functions_costs(CallGraph *cg, costmap &costs) {
    vector<Function *> ordering;

    computeDFNumbering(cg, ordering);

    for (vector<Function *>::const_iterator it = ordering.begin();
	 it != ordering.end(); ++it) {
	debug2("Processing function %.8x %s\n", (*it)->getAddress(),
	       (*it)->getName());

	long long cost;

	if (((*it)->getCfg()->exits_begin() != (*it)->getCfg()->exits_end()) &&
	    strcmp((*it)->getName(), "abort") != 0) {
	    IntraProcCFG ipcfg(*it);

	    ipcfg.add_calls_costs_to_edges(costs);
	    cost = ipcfg.compute_shortest_path_to_exit();
	} else {
	    cost = DISTANCE_INFINITY;
	
	}
	debug2("Cost is %lld\n\n", cost);
	costs[*it] = cost;
    }
}

Prog prog, prog_bis;
map <addr_t, Function *> functions, functions_bis;

InterProcCFG *build_ipcfg(const char *fname, map<addr_t, Function *> &F) {
    
    CallGraph *callgraph, *callgraph_bis;
    InterProcCFG *ipcfg = NULL;
    costmap costs;

    unserialize(fname, prog, callgraph, functions);
    compute_functions_costs(callgraph, costs);
    unserialize(fname, prog_bis, callgraph_bis, functions_bis);

    // Add a copy of the functions
    for (map<addr_t, Function *>::const_iterator fit = functions_bis.begin();
	 fit != functions_bis.end(); ++fit) {
	Function *func = fit->second;
	string funcname = func->getName() + string("_bis");
	func->setAddress(G2_ADDR(func->getAddress()));
	func->setName(funcname.c_str());

	for (Cfg::const_bb_iterator bbit = func->getCfg()->bb_begin();
	     bbit != func->getCfg()->bb_end(); ++bbit) {
	    (*bbit)->setAddress(G2_ADDR((*bbit)->getAddress()));
	    for (instructions_t::const_iterator iit = (*bbit)->inst_begin();
		 iit != (*bbit)->inst_end(); ++iit) {
		(*iit)->setAddress(G2_ADDR((*iit)->getAddress()));
	    }
	}

	functions[func->getAddress()] = func;
	
    }

    for (map<addr_t, Function *>::const_iterator fit = functions.begin();
	 fit != functions.end(); ++fit) {
	F[fit->first] = fit->second;
    }

    ipcfg = new InterProcCFG(functions);

    ipcfg->remove_call_edges_from_g1();
    ipcfg->remove_ret_edges_from_g2();

    ipcfg->add_call_ret_edges_to_g1_and_g2(costs, functions);

    return ipcfg;
}

// Local Variables: 
// mode: c++
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
