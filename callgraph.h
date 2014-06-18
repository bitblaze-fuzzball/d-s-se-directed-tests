#ifndef __CALLGRAPH_H__
#define __CALLGRAPH_H__

#include "debug.h"
#include "graph.h"
#include "func.h"

class Prog;

class CallGraphEdge {
 private:
    Function *source;
    Function *target;

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version) {
	ar & source;
	ar & target;
	(void)version;
    }

public:
    CallGraphEdge() {;}
    ~CallGraphEdge() {;}
    CallGraphEdge(Function *s, Function *t) {
	source = s;
	target = t;
    }

    Function *getSource() { 
	return source; 
    }

    Function *getTarget() {
	return target;
    }
};

class CallGraph : public Graph<Function *, CallGraphEdge *> {
private:
    typedef Graph<Function *, CallGraphEdge *> callgraph_t;
    
    std::map<addr_t, Function *> addr2func;

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version) {
	callgraph_t::serialize(ar, version);
	ar & addr2func;
        ar & main;
    }

    Function *main;

public:
    CallGraph() { main = NULL; }
    ~CallGraph() {;}

    void addCall(Function *caller, Function *callee);
    std::string dot();
    std::string vcg();

    Function *getMain() const { return main; }
    void setMain(Function *m) { main = m; callgraph_t::setEntry(m); }

    typedef callgraph_t::const_vertex_iterator const_func_iterator;
    typedef callgraph_t::const_edge_iterator const_edge_iterator;

    const_func_iterator func_begin() { return vertices_begin(); }
    const_func_iterator func_end() { return vertices_end(); }
    const_edge_iterator edge_begin() { return edges_begin(); }
    const_edge_iterator edge_end() { return edges_end(); }
};

#endif

// Local Variables: 
// mode: c++
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
