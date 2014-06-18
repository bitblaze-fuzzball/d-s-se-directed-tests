#ifndef __GRAPH_H__
#define __GRAPH_H__

#include <list>
#include <vector>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/adj_list_serialize.hpp> 
#include <boost/graph/reverse_graph.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/set.hpp>
#include <boost/graph/dominator_tree.hpp>

#include "bourdoncle_wto.h"

// definition of basic boost::graph properties
enum vertex_properties_t { vertex_properties };
enum edge_properties_t { edge_properties };
namespace boost {
    BOOST_INSTALL_PROPERTY(vertex, properties);
    BOOST_INSTALL_PROPERTY(edge, properties);
}

template<typename VERTEX, typename EDGE>
class Graph {
private:
    typedef VERTEX vertex_t;
    typedef EDGE edge_t;
    
    typedef boost::adjacency_list<
	boost::setS,
	boost::setS,
	boost::bidirectionalS,
	boost::property<boost::vertex_index_t, int,
			boost::property<vertex_properties_t, vertex_t > >,
	boost::property<edge_properties_t, edge_t>
	> graph_t;

protected:
    typedef typename boost::graph_traits<graph_t>::vertex_descriptor vertex_descriptor;
    typedef typename boost::graph_traits<graph_t>::edge_descriptor edge_descriptor;
  
    typedef typename boost::graph_traits<graph_t>::vertex_iterator vertex_iterator;
    typedef typename boost::graph_traits<graph_t>::edge_iterator edge_iterator;
    typedef typename boost::graph_traits<graph_t>::adjacency_iterator adjacency_iterator;
    typedef typename boost::graph_traits<graph_t>::out_edge_iterator out_edge_iterator;
    typedef typename boost::graph_traits<graph_t>::in_edge_iterator in_edge_iterator;
    typedef typename boost::graph_traits<graph_t>::degree_size_type degree_t;
  
    typedef std::pair<adjacency_iterator, adjacency_iterator> adjacency_vertex_range_t;
    typedef std::pair<out_edge_iterator, out_edge_iterator> out_edge_range_t;
    typedef std::pair<vertex_iterator, vertex_iterator> vertex_range_t;
    typedef std::pair<edge_iterator, edge_iterator> edge_range_t;

protected:
    graph_t graph;

    // Map between vertex descriptors and vertex properties and vice versa
    typename boost::property_map<graph_t, vertex_properties_t>::type vertex_map;
    // std::map<vertex_descriptor, vertex_t> vertex_map;
    std::map<vertex_t, vertex_descriptor> vertex_rev_map;
    // Map between edge descriptors and edge properties and vice versa
    // std::map<edge_descriptor, edge_t> edge_map;
    typename boost::property_map<graph_t,edge_properties_t>::type edge_map;
    std::map<edge_t, edge_descriptor> edge_rev_map;

    // Entry
    vertex_descriptor entry_vertex;

    // Immediate dominators
    std::map<vertex_descriptor, vertex_descriptor> idoms;
    bool idoms_computed;
    // Immediate post-dominators
    std::map<vertex_descriptor, vertex_descriptor> ipostdoms;
    bool ipostdoms_computed;
    // DFS numbering
    std::map<vertex_descriptor, int> dfnum;
    bool dfnum_computed;
    // Weak topological ordering
    WTO<vertex_descriptor> wto;
    bool wto_computed;

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version) {
	vertex_iterator vi, ve;
	edge_iterator ei, ee;
	vertex_t entry_;
	bool has_entry;

	ar & graph;

	if (Archive::is_saving::value) {
	    if (entry_vertex != boost::graph_traits<graph_t>::null_vertex()) {
		assert(hasVertex(entry_vertex));
	    }

	    has_entry = entry_vertex != boost::graph_traits<graph_t>::null_vertex();
	    ar & has_entry;
	    if (has_entry) {
		ar & vertex_map[entry_vertex];
	    }
	} else {
	    // Recreate the inverse property maps
	    for (boost::tie(vi, ve) = vertices(graph); vi != ve; vi++) {
		vertex_rev_map[vertex_map[*vi]] = *vi;
	    }
	    for (boost::tie(ei, ee) = edges(graph); ei != ee; ei++) {
		edge_rev_map[edge_map[*ei]] = *ei;
	    }

	    ar & has_entry;
	    if (has_entry) {
		ar & entry_;
		entry_vertex = vertex_rev_map[entry_];
	    } else {
		entry_vertex = boost::graph_traits<graph_t>::null_vertex();
	    }
	}
    }

    // Internal functions to access/manipulate vertices
    bool hasVertex(const vertex_descriptor vd) const {
	vertex_iterator vi, ve;

	for (boost::tie(vi, ve) = boost::vertices(graph); vi != ve; vi++) {
	    if (*vi == vd)
		return true;
	}
	return false;
    }

    void delVertex(const vertex_descriptor vd) {
	out_edge_iterator oi, oe;
	in_edge_iterator  ii, ie;

	assert(hasVertex(vd));
	
	boost::tie(oi, oe) = out_edges(vd, graph);
	assert(oi == oe);
	boost::tie(ii, ie) = in_edges(vd, graph);
	assert(ii == ie);

	if (vd == entry_vertex)
	    entry_vertex = boost::graph_traits<graph_t>::null_vertex();

	vertex_rev_map.erase(vertex_map[vd]);
	boost::remove_vertex(vd, graph);
    }

    // Internal functions to access/manipulate edges
    edge_t delEdge(const vertex_descriptor vd1, const vertex_descriptor vd2) {
	out_edge_iterator ei, ee;
	edge_t e = NULL;

	assert(hasVertex(vd1));
	assert(hasVertex(vd2));

	for (boost::tie(ei, ee) = out_edges(vd1, graph); ei != ee; ei++) {
	    if (boost::target(*ei, graph) == vd2) {
		e = edge_map[*ei];
		edge_rev_map.erase(e);

		boost::remove_edge(*ei, graph);
		break;
	    }
	}

	return e;
    }

    bool hasEdge(const vertex_descriptor vd1, const vertex_descriptor vd2) {
	out_edge_iterator ei, ee;

	assert(hasVertex(vd1));
	assert(hasVertex(vd2));

	for (boost::tie(ei, ee) = out_edges(vd1, graph); ei != ee; ei++) {
	    if (boost::target(*ei, graph) == vd2) {
		return true;
	    }
	}

	return false;
    }

    bool addEdge(const vertex_descriptor vd1, 
		 const vertex_descriptor vd2, const edge_t e) {
	assert(hasVertex(vd1));
	assert(hasVertex(vd2));

	edge_descriptor ed;
	bool inserted;
	boost::tie(ed, inserted) = boost::add_edge(vd1, vd2, graph);

	if (inserted) {
	    edge_map[ed] = e;
	    edge_rev_map[e] = ed;
	}

	return inserted;
    }

    edge_descriptor getEdge(const vertex_descriptor vd1, 
			    const vertex_descriptor vd2) const {
	assert(hasVertex(vd1));
	assert(hasVertex(vd2));

	out_edge_iterator ei, ee;
	
	for (boost::tie(ei, ee) = out_edges(vd1, graph); ei != ee; ei++) {
	    if (boost::target(*ei, graph) == vd2) {
		return *ei;
	    }
	}

	assert(false);
    return *ei;
    }

    // Dominators
    bool doesDominate(vertex_descriptor v1, vertex_descriptor v2) {
	vertex_descriptor idom;

	assert(hasVertex(v1));
	assert(hasVertex(v2));
	assert(idoms.find(v2) != idoms.end());

	// Traverse idoms until v1 or NULL is found
	idom = v2;
	do {
	    idom = idoms[idom];
	} while (idom != v1 && idom != boost::graph_traits<graph_t>::null_vertex());

	return idom == v1;
    }

    vertex_descriptor getDominator(vertex_descriptor v) {
	assert(hasVertex(v));
	assert(idoms.find(v) != idoms.end());
	
	return idoms[v];
    }

    // Post-dominators, dual to dominators
    bool doesPostDominate(vertex_descriptor v1, vertex_descriptor v2) {
	vertex_descriptor idom;

	assert(hasVertex(v1));
	assert(hasVertex(v2));
	assert(ipostdoms.find(v2) != ipostdoms.end());

	// Traverse idoms until v1 or NULL is found
	idom = v2;
	do {
	    idom = ipostdoms[idom];
	} while (idom != v1 && idom != boost::graph_traits<graph_t>::null_vertex());

	return idom == v1;
    }

    vertex_descriptor getPostDominator(vertex_descriptor v) {
	assert(hasVertex(v));
	assert(ipostdoms.find(v) != ipostdoms.end());
	
	return ipostdoms[v];
    }

    int getNumSuccessors(const vertex_descriptor v) const {
	return boost::out_degree(v, graph);
    }

    int getNumPredecessors(const vertex_descriptor v) const {
	return boost::in_degree(v, graph);
    }

public:
    Graph() {
	vertex_map = boost::get(vertex_properties, graph);
	edge_map = boost::get(edge_properties, graph);
	entry_vertex = boost::graph_traits<graph_t>::null_vertex();
	wto_computed = idoms_computed = ipostdoms_computed = false;
        dfnum_computed = false;
    }

    ~Graph() {
	for (typename std::map<edge_t, edge_descriptor>::iterator it = edge_rev_map.begin();
	     it != edge_rev_map.end(); it++) {
	    delete it->first;
	}

	for (typename std::map<vertex_t, vertex_descriptor>::iterator it = vertex_rev_map.begin();
	     it != vertex_rev_map.end(); it++) {
	    delete it->first;
	}
    }

    // XXX: this should be private (made public because I'm tired of writing
    // iterators)
    vertex_t getVertex(const vertex_descriptor vd) {
	assert(hasVertex(vd));

	return vertex_map[vd];
    }

    vertex_descriptor getVertex(const vertex_t v) {
	assert(hasVertex(v));
	return vertex_rev_map[v];
    }

    // Public functions to access/manipulate vertices
    bool hasVertex(const vertex_t v) const {
	return vertex_rev_map.find(v) != vertex_rev_map.end();
    }
    
    vertex_descriptor addVertex(const vertex_t v) {
	vertex_descriptor vd;

	assert(!hasVertex(v));
	
	vd = boost::add_vertex(graph);
	vertex_map[vd] = v;
	vertex_rev_map[v] = vd;

	return vd;
    }

    void delVertex(const vertex_t v) {
	assert(hasVertex(v));

	delVertex(vertex_rev_map[v]);
    }

    size_t getNumVertex() {
	return boost::num_vertices(graph);
    }

    size_t getNumEdges() {
	return boost::num_edges(graph);
    }

    // Public functions to access/manipulate edges
    bool hasEdge(const vertex_t v1, const vertex_t v2) {
	assert(hasVertex(v1));
	assert(hasVertex(v2));

	return hasEdge(vertex_rev_map[v1], vertex_rev_map[v2]);
    }

    bool addEdge(const vertex_t v1, const vertex_t v2, const edge_t e) {
	assert(hasVertex(v1));
	assert(hasVertex(v2));

	return addEdge(vertex_rev_map[v1], vertex_rev_map[v2], e);
    }

    edge_t delEdge(const vertex_t v1, const vertex_t v2) {
	assert(hasVertex(v1));
	assert(hasVertex(v2));

	return delEdge(vertex_rev_map[v1], vertex_rev_map[v2]);
    }

    edge_t getEdge(const vertex_t v1, const vertex_t v2) const {
	assert(hasVertex(v1));
	assert(hasVertex(v2));

	edge_descriptor e = getEdge(vertex_rev_map.find(v1)->second,
				    vertex_rev_map.find(v2)->second);

	return edge_map[e];
    }

    void setEntry(const vertex_t v) {
	assert(hasVertex(v));

	entry_vertex = vertex_rev_map[v];
    }

    int getNumSuccessors(const vertex_t v) const {
	assert(hasVertex(v));

	return getNumSuccessors(vertex_rev_map.find(v)->second);
    }

    int getNumPredecessors(const vertex_t v) const {
	assert(hasVertex(v));

	return getNumPredecessors(vertex_rev_map.find(v)->second);
    }

    // Graph analysis
    void loops() {
	vertex_iterator vi, ve;
	out_edge_iterator ei, ee;
	
	// Check for the existence of a backedge to a dominator
	for (boost::tie(vi, ve) = boost::vertices(graph); vi != ve; vi++) {
	    for (boost::tie(ei, ee) = boost::out_edges(*vi, graph); ei != ee; ei++) {
		vertex_descriptor n, s;
		n = *vi; 
		s = target(*ei, graph);

		if (doesDominate(s, n)) {
		    debug("FOUND LOOP\n");
		}
	    }
	}
    }

    bool doesDominate(vertex_t v1, vertex_t v2) {
	assert(hasVertex(v1));
	assert(hasVertex(v2));

	return doesDominate(vertex_rev_map[v1], vertex_rev_map[v2]);
    }

    vertex_t getDominator(vertex_t v) {
	vertex_descriptor vd; 

	assert(hasVertex(v));

	vd = getDominator(vertex_rev_map[v]);
	return vd != boost::graph_traits<graph_t>::null_vertex() ? vertex_map[vd] : NULL;
    }

    bool doesPostDominate(vertex_t v1, vertex_t v2) {
	assert(hasVertex(v1));
	assert(hasVertex(v2));

	return doesPostDominate(vertex_rev_map[v1], vertex_rev_map[v2]);
    }

    vertex_t getPostDominator(vertex_t v) {
	vertex_descriptor vd; 

	assert(hasVertex(v));

	vd = getPostDominator(vertex_rev_map[v]);
	return vd != boost::graph_traits<graph_t>::null_vertex() ? vertex_map[vd] : NULL;
    }

    void computeDFNumbering() {
        std::vector<boost::default_color_type> color(boost::num_vertices(graph));
        vertex_descriptor dtime[boost::num_vertices(graph)];
        int time = 0;

        // boost::depth_first_search(graph, boost::visitor(boost::make_dfs_visitor(boost::stamp_times(dtime, time, boost::on_discover_vertex()))));

                                  ///, boost::make_iterator_property_map(color.begin(), boost::get(boost::vertex_index, graph)), entry_vertex);

        dfnum_computed = true;
    }

    void computeDominators() {
	typedef typename boost::property_map<graph_t, boost::vertex_index_t>::type IndexMap;
	typedef typename std::vector<vertex_descriptor>::iterator VectorDescIter;
	typedef typename boost::iterator_property_map<VectorDescIter, IndexMap> PredMap;

	std::vector<vertex_descriptor> domTreePredVector;
	IndexMap indexMap(get(boost::vertex_index, graph));
	vertex_iterator uItr, uEnd;
	int j = 0;

	for(boost::tie(uItr, uEnd) = boost::vertices(graph); 
	    uItr != uEnd; ++uItr, ++j ) {
	    boost::put(indexMap, *uItr, j);
	}

	// Lengauer-Tarjan dominator tree algorithm
	domTreePredVector = 
	    std::vector<vertex_descriptor>(num_vertices(graph), 
					   boost::graph_traits<graph_t>::null_vertex());

	PredMap domTreePredMap = 
	    boost::make_iterator_property_map(domTreePredVector.begin(), 
					      indexMap);

	boost::lengauer_tarjan_dominator_tree(graph, entry_vertex, domTreePredMap);
	
	for(boost::tie(uItr, uEnd) = boost::vertices(graph); uItr != uEnd; ++uItr) {
	    idoms[*uItr] = get(domTreePredMap, *uItr);
	}

	idoms_computed = true;
    }

    void computePostDominators(std::set<vertex_t> *exits) {
	typedef typename 
	    boost::property_map<graph_t, boost::vertex_index_t>::type
	    IndexMap;
	typedef typename std::vector<vertex_descriptor> VertVector;
	typedef typename VertVector::iterator VertVectorIter;
	typedef boost::iterator_property_map<VertVectorIter, IndexMap> PredMap;

	typedef typename boost::reverse_graph<graph_t, graph_t &>
	    reverse_graph_t;
	typedef typename 
	    boost::property_map<reverse_graph_t, boost::vertex_index_t>::type
	    RIndexMap;
	typedef typename
	    boost::graph_traits<reverse_graph_t>::vertex_descriptor
	    rvertex_descriptor;
	typedef typename std::vector<rvertex_descriptor> RVertVector;
	typedef typename RVertVector::iterator RVertVectorIter;
	typedef typename boost::graph_traits<reverse_graph_t>::vertex_iterator
	    rvertex_iterator;
	typedef boost::iterator_property_map<RVertVectorIter, IndexMap>
	    RPredMap;

	if (boost::num_vertices(graph) == 0) {
	    ipostdoms_computed = true;
	    return;
	}

	vertex_descriptor exit_vertex;
	vertex_descriptor fake_exit =
	    boost::graph_traits<graph_t>::null_vertex();
	if (exits->size() == 1) {
	    exit_vertex = vertex_rev_map[*(exits->begin())];
	} else if (exits->size() > 1) {
	    fake_exit = boost::add_vertex(graph);
	    typename std::set<vertex_t>::iterator ex_it;
	    for (ex_it = exits->begin(); ex_it != exits->end(); ex_it++) {
		vertex_descriptor old_exit = vertex_rev_map[*ex_it];
		edge_descriptor ed;
		bool inserted;
		boost::tie(ed, inserted) = 
		    boost::add_edge(old_exit, fake_exit, graph);
		assert(inserted);
	    }
	    exit_vertex = fake_exit;
	} else {
	    int num_sinks = 0;
	    vertex_iterator v_it, v_end;
	    for (boost::tie(v_it, v_end) = boost::vertices(graph);
		 v_it != v_end; ++v_it) {
		if (boost::out_degree(*v_it, graph) == 0) {
		    num_sinks++;
		    exit_vertex = *v_it;
		}
	    }
	    // Ideally we'd hope num_sinks == 1 here. If it's > 1, the
	    // choice of exit_vertex is arbitrary.
	    if (num_sinks == 0) {
		boost::tie(v_it, v_end) = boost::vertices(graph);
		assert(v_it != v_end);
		exit_vertex = *v_it; // really arbitrary
	    }
	}

	reverse_graph_t reverse_graph = boost::make_reverse_graph(graph);

	VertVector domTreePredVector(boost::num_vertices(reverse_graph));
	RIndexMap indexMap(get(boost::vertex_index, reverse_graph));
	int j = 0;

	rvertex_iterator rItr, rEnd;
	for (boost::tie(rItr, rEnd) = boost::vertices(reverse_graph); 
	    rItr != rEnd; ++rItr, ++j ) {
	    boost::put(indexMap, *rItr, j);
	}

	RPredMap domTreePredMap = 
	    boost::make_iterator_property_map(domTreePredVector.begin(), 
					      indexMap);

	boost::lengauer_tarjan_dominator_tree(reverse_graph, exit_vertex,
					      domTreePredMap);
	
	vertex_iterator uItr, uEnd;
	for (boost::tie(uItr, uEnd) = boost::vertices(graph);
	    uItr != uEnd; ++uItr) {
	    ipostdoms[*uItr] = get(domTreePredMap, *uItr);
	}

	if (fake_exit != boost::graph_traits<graph_t>::null_vertex()) {
	    for (typename std::set<vertex_t>::iterator ex_it = exits->begin();
		 ex_it != exits->end(); ex_it++) {
		vertex_descriptor old_exit = vertex_rev_map[*ex_it];
		boost::remove_edge(old_exit, fake_exit, graph);
	    }
	    boost::remove_vertex(fake_exit, graph);
	}

	ipostdoms_computed = true;
    }

    void computeWeakTopologicalOrdering() {
	if (getNumVertex() > 0 && !wto_computed)
	    bourdoncle_partition(graph, entry_vertex, wto);

	wto_computed = true;
    }

    int getComponentNo(vertex_t v) {
	assert(wto_computed);
	return wto.getComponentNo(vertex_rev_map[v]);
    }

    vertex_t getComponent(vertex_t v) {
	assert(wto_computed);
	assert(vertex_rev_map.find(v) != vertex_rev_map.end());
	return vertex_map[wto.getComponent(vertex_rev_map[v])];
    }

    // Return true if c is the header of a component and if s is within the
    // component, even if s is within a component nested in c
    bool isSubComponentOf(vertex_t s, vertex_t c) {
	assert(wto_computed);
	assert(vertex_rev_map.find(s) != vertex_rev_map.end());
	assert(vertex_rev_map.find(c) != vertex_rev_map.end());
	return wto.isSubComponentOf(vertex_rev_map[s], vertex_rev_map[c]);
    }

    // Return true if v is the header of a component
    bool isComponentEntry(vertex_t v) {
	assert(wto_computed);
	assert(vertex_rev_map.find(v) != vertex_rev_map.end());
	return v == getComponent(v);
    }

    // Assuming that v is the header of an SCC, return the vertex
    // for the header of the immediately enclosing SCC.
    // If v's SCC is outermost, return v itself.
    vertex_t getEnclosingComponent(vertex_t v) {
	assert(wto_computed);
	return vertex_map[wto.getEnclosingComponent(vertex_rev_map[v])];
    }

    // Returns true if v is part of the SCC whose header is component,
    // including nested subcomponents.
    bool withinComponent(vertex_t component, vertex_t v) {
	assert(getComponent(component) == component);
	vertex_t vc = getComponent(v);
	while (vc != component) {
	    vertex_t new_vc = getEnclosingComponent(vc);
	    if (new_vc == vc)
		break;
	    vc = new_vc;
	}
	return vc == component;
    }

    // Iterators
    class const_vertex_iterator {
        typedef const_vertex_iterator        _Self;
    public:
        typedef std::random_access_iterator_tag iterator_category;
        typedef vertex_t              value_type;
        typedef size_t                difference_type;
        typedef const vertex_t        &reference;
        typedef const vertex_t        *pointer;
    private:
	const Graph *g;
	vertex_iterator vit;

        explicit const_vertex_iterator(const Graph *g_, vertex_iterator vit_) {
	    g = g_;
	    vit = vit_;
	}

    public:
        // No need for copy ctor, copy assign. Defaults are fine.
        const_vertex_iterator() {
	    g = NULL;
	}

        explicit const_vertex_iterator(const Graph *g_) {
	    g = g_;
	    vit = boost::vertices(g->graph).first;
	}

        explicit const_vertex_iterator(const Graph *g_, bool) {
	    g = g_;
	    vit = boost::vertices(g->graph).second;
	}

        bool operator==(const _Self& B) { 
	    return vit == B.vit; 
	}

        bool operator!=(const _Self& B) { 
	    return vit != B.vit; 
	}

        bool operator<(const _Self& B) { 
	    return vit < B.vit; 
	}

        bool operator>(const _Self& B) { 
	    return vit > B.vit; 
	}

        bool operator<=(const _Self& B) { 
	    return vit <= B.vit; 
	}

        bool operator>=(const _Self& B) { 
	    return vit >= B.vit; 
	}

        _Self &operator++() { 
	    vit++; 
	    return *this; 
	} 

        _Self operator++(int) { 
	    return _Self(g, vit++); 
	} 

        _Self &operator--() { 
	    vit--; 
	    return *this; 
	}

        _Self operator--(int) { 
	    return _Self(g, vit--); 
	}

        reference operator*() const { 
	    return g->vertex_map[*vit];
	}

        pointer operator->() const { 
	    return g->vertex_map[*vit];
	}
    };

    const_vertex_iterator vertices_begin() const {
	return Graph::const_vertex_iterator(this); 
    }

    const_vertex_iterator vertices_end() const {
	return Graph::const_vertex_iterator(this, false); 
    }

    class const_succ_iterator {
        typedef const_succ_iterator        _Self;
    public:
        typedef std::random_access_iterator_tag iterator_category;
        typedef vertex_t              value_type;
        typedef size_t                difference_type;
        typedef const vertex_t        &reference;
        typedef const vertex_t        *pointer;
    private:
	const Graph *g;
	vertex_descriptor v;
	out_edge_iterator vit;

        explicit const_succ_iterator(const Graph *g_, vertex_descriptor v_, out_edge_iterator vit_) {
	    g = g_;
	    v = v_;
	    vit = vit_;
	}

    public:
        // No need for copy ctor, copy assign. Defaults are fine.
        const_succ_iterator() {
	    g = NULL;
	}

        explicit const_succ_iterator(const Graph *g_, vertex_descriptor v_) {
	    g = g_;
	    v = v_;
	    vit = boost::out_edges(v, g->graph).first;
	}

        explicit const_succ_iterator(const Graph *g_, vertex_descriptor v_, bool) {
	    g = g_;
	    v = v_;
	    vit = boost::out_edges(v_, g->graph).second;
	}

        bool operator==(const _Self& B) { 
	    return vit == B.vit; 
	}

        bool operator!=(const _Self& B) { 
	    return vit != B.vit; 
	}

        bool operator<(const _Self& B) { 
	    return vit < B.vit; 
	}

        bool operator>(const _Self& B) { 
	    return vit > B.vit; 
	}

        bool operator<=(const _Self& B) { 
	    return vit <= B.vit; 
	}

        bool operator>=(const _Self& B) { 
	    return vit >= B.vit; 
	}

        _Self &operator++() { 
	    vit++; 
	    return *this; 
	} 

        _Self operator++(int) { 
	    return _Self(g, v, vit++); 
	} 

        _Self &operator--() { 
	    vit--; 
	    return *this; 
	}

        _Self operator--(int) { 
	    return _Self(g, v, vit--); 
	}

        reference operator*() const { 
	    return g->vertex_map[boost::target(*vit, g->graph)];
	}

        pointer operator->() const { 
	    return g->vertex_map[boost::target(*vit, g->graph)];
	}
    };

    const_succ_iterator succ_begin(vertex_descriptor v) const { 
	return Graph::const_succ_iterator(this, v); 
    }

    const_succ_iterator succ_end(vertex_descriptor v) const {
	return Graph::const_succ_iterator(this, v, false); 
    }

    const_succ_iterator succ_begin(vertex_t v) const {
	assert(vertex_rev_map.find(v) != vertex_rev_map.end());
	return succ_begin(vertex_rev_map.find(v)->second);
    }

    const_succ_iterator succ_end(vertex_t v) const {
	assert(vertex_rev_map.find(v) != vertex_rev_map.end());
	return succ_end(vertex_rev_map.find(v)->second);
    }

    class const_pred_iterator {
        typedef const_pred_iterator        _Self;
    public:
        typedef std::random_access_iterator_tag iterator_category;
        typedef vertex_t              value_type;
        typedef size_t                difference_type;
        typedef const vertex_t        &reference;
        typedef const vertex_t        *pointer;
    private:
	const Graph *g;
	vertex_descriptor v;
	in_edge_iterator vit;

        explicit const_pred_iterator(const Graph *g_, vertex_descriptor v_, in_edge_iterator vit_) {
	    g = g_;
	    v = v_;
	    vit = vit_;
	}

    public:
        // No need for copy ctor, copy assign. Defaults are fine.
        const_pred_iterator() {
	    g = NULL;
	}

        explicit const_pred_iterator(const Graph *g_, vertex_descriptor v_) {
	    g = g_;
	    v = v_;
	    vit = boost::in_edges(v, g->graph).first;
	}

        explicit const_pred_iterator(const Graph *g_, vertex_descriptor v_, bool) {
	    g = g_;
	    v = v_;
	    vit = boost::in_edges(v_, g->graph).second;
	}

        bool operator==(const _Self& B) { 
	    return vit == B.vit; 
	}

        bool operator!=(const _Self& B) { 
	    return vit != B.vit; 
	}

        bool operator<(const _Self& B) { 
	    return vit < B.vit; 
	}

        bool operator>(const _Self& B) { 
	    return vit > B.vit; 
	}

        bool operator<=(const _Self& B) { 
	    return vit <= B.vit; 
	}

        bool operator>=(const _Self& B) { 
	    return vit >= B.vit; 
	}

        _Self &operator++() { 
	    vit++; 
	    return *this; 
	} 

        _Self operator++(int) { 
	    return _Self(g, v, vit++); 
	} 

        _Self &operator--() { 
	    vit--; 
	    return *this; 
	}

        _Self operator--(int) { 
	    return _Self(g, v, vit--); 
	}

        reference operator*() const { 
	    return g->vertex_map[boost::source(*vit, g->graph)];
	}

        pointer operator->() const { 
	    return g->vertex_map[boost::source(*vit, g->graph)];
	}
    };

    const_pred_iterator pred_begin(vertex_descriptor v) const { 
	return Graph::const_pred_iterator(this, v); 
    }

    const_pred_iterator pred_end(vertex_descriptor v) const {
	return Graph::const_pred_iterator(this, v, false); 
    }

    const_pred_iterator pred_begin(vertex_t v) const {
	assert(hasVertex(v));
	return pred_begin(vertex_rev_map.find(v)->second);
    }

    const_pred_iterator pred_end(vertex_t v) const {
	assert(hasVertex(v));
	return pred_end(vertex_rev_map.find(v)->second);
    }

    class const_edge_iterator {
        typedef const_edge_iterator        _Self;
    public:
        typedef std::random_access_iterator_tag iterator_category;
        typedef edge_t              value_type;
        typedef size_t              difference_type;
        typedef const edge_t        &reference;
        typedef const edge_t        *pointer;
    private:
	const Graph *g;
	edge_iterator eit;

        explicit const_edge_iterator(const Graph *g_, edge_iterator eit_) {
	    g = g_;
	    eit = eit_;
	}

    public:
        // No need for copy ctor, copy assign. Defaults are fine.
        const_edge_iterator() {
	    g = NULL;
	}

        explicit const_edge_iterator(const Graph *g_) {
	    g = g_;
	    eit = boost::edges(g->graph).first;
	}

        explicit const_edge_iterator(const Graph *g_, bool) {
	    g = g_;
	    eit = boost::edges(g->graph).second;
	}

        bool operator==(const _Self& B) { 
	    return eit == B.eit; 
	}

        bool operator!=(const _Self& B) { 
	    return eit != B.eit; 
	}

        bool operator<(const _Self& B) { 
	    return eit < B.eit; 
	}

        bool operator>(const _Self& B) { 
	    return eit > B.eit; 
	}

        bool operator<=(const _Self& B) { 
	    return eit <= B.eit; 
	}

        bool operator>=(const _Self& B) { 
	    return eit >= B.eit; 
	}

        _Self &operator++() { 
	    eit++; 
	    return *this; 
	} 

        _Self operator++(int) { 
	    return _Self(g, eit++); 
	} 

        _Self &operator--() { 
	    eit--; 
	    return *this; 
	}

        _Self operator--(int) { 
	    return _Self(g, eit--); 
	}

        reference operator*() const { 
	    return g->edge_map[*eit];
	}

        pointer operator->() const { 
	    return g->edge_map[*eit];
	}
    };

    const_edge_iterator edges_begin() const { 
	return Graph::const_edge_iterator(this); 
    }

    const_edge_iterator edges_end() const {
	return Graph::const_edge_iterator(this, false); 
    }

    typedef typename WTO<vertex_descriptor>::const_iterator const_wto_iterator;
    typedef typename WTO<vertex_descriptor>::const_reverse_iterator const_reverse_wto_iterator;

    // Question: Can these two functions be made const, perhaps the
    // computation of the WTO could be changed into an assertion that
    // WTO was already computed. The more const functions the better.
    const_wto_iterator wto_begin() const {
	return wto.begin();
    }

    const_wto_iterator wto_end() const {
	return wto.end();
    }

    const_wto_iterator wto_begin(vertex_t v) const {
	assert(vertex_rev_map.find(v) != vertex_rev_map.end());
	return wto.begin(vertex_rev_map.find(v)->second);
    }

    const_wto_iterator wto_end(vertex_t v) const {
	assert(vertex_rev_map.find(v) != vertex_rev_map.end());
	return wto.end(vertex_rev_map.find(v)->second);
    }

    const_reverse_wto_iterator wto_rbegin() const {
	return wto.rbegin();
    }

    const_reverse_wto_iterator wto_rend() const {
	return wto.rend();
    }

    const_reverse_wto_iterator wto_rbegin(vertex_t v) const {
	assert(vertex_rev_map.find(v) != vertex_rev_map.end());
	return wto.rbegin(vertex_rev_map.find(v)->second);
    }

    const_reverse_wto_iterator wto_rend(vertex_t v) const {
	assert(vertex_rev_map.find(v) != vertex_rev_map.end());
	return wto.rend(vertex_rev_map.find(v)->second);
    }
};

#endif

// Local Variables: 
// mode: c++
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
