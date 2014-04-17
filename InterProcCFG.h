#include "func.h"
#include "cfg.h"
#include "callgraph.h"
#include "warning.h"
#include "influence.h"

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>

#include <stdio.h>
#include <vector>
#include <string>
#include <map>
#include <tr1/unordered_map>

using namespace std;
using namespace boost;

typedef map<Function *, long long> costmap;

class InterProcCFG {
 protected:
    enum edge_type {
	INTRA_EDGE,
	BACK_EDGE,
	CALL_EDGE,
	RETURN_EDGE,
	CALL_TO_RET_EDGE,
    };

    struct bb_ptr_t { typedef vertex_property_tag kind; };

    struct edge_type_t { typedef edge_property_tag kind; };

    struct paths_to_header_t { typedef vertex_property_tag kind; };

    typedef adjacency_list<listS, vecS, directedS,
	property<bb_ptr_t, BasicBlock *,
	property<vertex_distance_t, unsigned long long,
        property<paths_to_header_t, long,
	property<vertex_color_t, default_color_type> > > >,
	property<edge_weight_t, long long int,
	property<edge_type_t, edge_type> > >
	IPCFGraph;
    typedef graph_traits<IPCFGraph>::vertex_descriptor IPCFGNode;
    typedef graph_traits<IPCFGraph>::edge_descriptor IPCFGEdge;
    
    typedef map<string, vector<IPCFGNode> > subgraph_map;

    subgraph_map subgraphs;
    map<BasicBlock *, IPCFGNode> bb_to_node;
    IPCFGraph ipcfg;
    IPCFGNode target_node;
    Warning *warning;
    set<addr_t> interesting_loops;

    tr1::unordered_map<addr_t, BasicBlock *> bb_lookup_cache;

    void copy_nodes_to_ipcfg(Function *func);
    void make_call_ret_edges(Instruction *i, bool include_returns);
    void copy_edges_to_ipcfg(Function *func,
			     bool include_returns,
			     bool include_call_to_rets,
			     bool include_calls);
    long compute_pth(bool first_call, BasicBlock *bb);
    BasicBlock* lookup_bb(addr_t addr,
			  map<addr_t, Function *> &functions);
    bool is_loop_exit_cond(BasicBlock *bb);
    bool edge_is_loop_exit(BasicBlock *from_bb, BasicBlock *to_bb);

 public:
    void to_vcg(ostream &out);
    void set_target_addr(addr_t target_addr,
			 map<addr_t, Function *> &functions);
    void set_target_warning(Warning *w,
			 map<addr_t, Function *> &functions);
    void compute_shortest_paths();
    long long lookup_distance(addr_t source_addr,
			      map<addr_t, Function *> &functions);
    int lookup_influence(addr_t addr,
			 map<addr_t, Function *> &functions,
			 influence_map_t *influence);
    long lookup_pth(addr_t addr,
		    map<addr_t, Function *> &functions);
    bool is_loop_exit_cond(addr_t branch_addr,
			   map<addr_t, Function *> &functions);
    bool edge_is_loop_exit(addr_t from_addr, addr_t to_addr,
			   map<addr_t, Function *> &functions);
    bool is_interesting_loop(addr_t addr,
			     map<addr_t, Function *> &functions);
    addr_t get_component(addr_t addr,
			 map<addr_t, Function *> &functions);
    bool same_bb(addr_t addr1, addr_t addr2,
		 map<addr_t, Function *> &functions);

    InterProcCFG(map<addr_t, Function *> &functions);
    InterProcCFG() {};

    void remove_call_edges_from_g1();
    void remove_ret_edges_from_g2();
    void add_call_ret_edges_to_g1_and_g2(costmap &costs, map<addr_t, Function *> &);
};

class IntraProcCFG : public InterProcCFG {
private:
    Function *func;
    Cfg *cfg;
    BasicBlock *exitnode;
    
public:
    IntraProcCFG(Function *func);
    ~IntraProcCFG();

    void add_calls_costs_to_edges(costmap &costs);
    long long compute_shortest_path_to_exit();
};

InterProcCFG *build_ipcfg(const char *fname, map<addr_t, Function *> &);

// Local Variables: 
// mode: c++
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
