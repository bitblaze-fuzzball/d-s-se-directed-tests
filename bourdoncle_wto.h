#include "debug.h"
#include <map>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>

typedef std::map<unsigned int, unsigned int> dfn_t;
typedef std::list<unsigned int> stack__t;

template <class V>
class WTO;

template <class graph_t>
void bourdoncle_partition(const graph_t &g, 
			  typename boost::graph_traits<graph_t>::vertex_descriptor entry_vertex,
			  WTO<typename boost::graph_traits<graph_t>::vertex_descriptor> &wto_);

template <class V>
class WTO {
public:
    typedef typename std::pair<V, int> component_t;

private:
    typedef typename std::list<component_t> ordering_t;
    ordering_t ordering;
    // Map a vertex to the vertex representing the entry of the component
    // (we use this to detect backedges)
    std::map<V, V> components;
    std::map<V, std::list<V> > sub_components;
    std::map<V, int> components_no;
    std::map<V, V> enclosing_component;

    template <class graph_t>
    friend void bourdoncle_partition(const graph_t &g, 
				     typename boost::graph_traits<graph_t>::vertex_descriptor entry_vertex,
				     WTO<typename boost::graph_traits<graph_t>::vertex_descriptor> &wto_);

public:

    WTO() {;}
    ~WTO() {;}

    void add(V v, int c) {
	ordering.push_front(std::pair<V, int>(v, c));
    }

    void finalize() {
	std::list<V> components_;
	int last_component = -1;

	typename ordering_t::const_iterator it;
	for (it = ordering.begin(); it != ordering.end(); it++) {
	    int c = it->second;
	    V v = it->first;

	    if (c > last_component) {
		if (components_.size() == 0)
		    enclosing_component[v] = v;
		else
		    enclosing_component[v] = components_.back();

		// We need to push the vertex multiple times if it is an entry
		// for multiple nested components
		for (int z = last_component; z < c; z++) {
		    components_.push_back(v);
		}
	    }

	    if (c < last_component) {
		components_.pop_back();
	    }

	    assert(!components_.empty());
	    components[v] = components_.back();

	    sub_components[v];
	    typename std::list<V>::const_iterator cit;
	    for (cit = components_.begin(); cit != components_.end(); cit++) {
		sub_components[v].push_back(*cit);
	    }
	    components_no[v] = c;

	    last_component = c;
	}
    }

    int getComponentNo(V v) {
	assert(components_no.find(v) != components_no.end());
	return components_no[v];
    }

    V getComponent(V v) {
	assert(components.find(v) != components.end());
	return components[v];
    }

    bool isComponentEntry(V v) {
	return v == getComponent(v);
    }

    // Return true if c is the header of a component and if s is within the
    // component, even if s is within a component nested in c
    bool isSubComponentOf(V s, V c) {
	assert(components.find(s) != components.end());
	assert(components.find(c) != components.end());
	assert(sub_components.find(s) != sub_components.end());
	
	if (!isComponentEntry(c))
	    return false;

	typename std::list<V>::const_iterator sit;
	for (sit = sub_components[s].begin(); sit != sub_components[s].end(); 
	     sit++) {
	    if (*sit == c) {
		return true;
	    }
	}

	return false;
    }

    V getEnclosingComponent(V v) {
	assert(enclosing_component.find(v) != enclosing_component.end());
	return enclosing_component[v];
    }

    // Iterators
    class const_iterator {
        typedef const_iterator        _Self;
    public:
        typedef std::random_access_iterator_tag iterator_category;
        typedef V                     value_type;
        typedef size_t                difference_type;
        typedef const V               &reference;
        typedef const V               *pointer;
    private:
	const ordering_t *o;
	typedef typename std::list<component_t>::const_iterator ordering_t_iterator;
	ordering_t_iterator it;

        explicit const_iterator(const ordering_t &o_, ordering_t_iterator it_) {
	    o = &o_;
	    it = it_;
	}

    public:
        // No need for copy ctor, copy assign. Defaults are fine.
	const_iterator() {
	    ;
	}

        explicit const_iterator(const ordering_t &o_) {
	    o = &o_;
	    it = o->begin();
	}

        explicit const_iterator(const ordering_t &o_, bool) {
	    o = &o_;
	    it = o->end();
	}

        bool operator==(const _Self& B) { 
	    return it == B.it; 
	}

        bool operator!=(const _Self& B) { 
	    return it != B.it; 
	}

        bool operator<(const _Self& B) { 
	    return it < B.it; 
	}

        bool operator>(const _Self& B) { 
	    return it > B.it; 
	}

        bool operator<=(const _Self& B) { 
	    return it <= B.it; 
	}

        bool operator>=(const _Self& B) { 
	    return it >= B.it; 
	}

        _Self &operator++() { 
	    it++; 
	    return *this; 
	} 

        _Self operator++(int) { 
	    return _Self(*o, it++); 
	} 

        _Self &operator--() { 
	    it--; 
	    return *this; 
	}

        _Self operator--(int) { 
	    return _Self(*o, it--); 
	}

        reference operator*() const { 
	    return it->first;
	}

        pointer operator->() const { 
	    return it->first;
	}
    };

    const_iterator begin() const { 
	return const_iterator(ordering);
    }

    const_iterator end() const { 
	return const_iterator(ordering, false);
    }

    const_iterator begin(V v) const { 
	assert(0);
    }

    const_iterator end(V v) const { 
	assert(0);
    }

    class const_reverse_iterator {
        typedef const_reverse_iterator        _Self;
    public:
        typedef std::random_access_iterator_tag iterator_category;
        typedef V                     value_type;
        typedef size_t                difference_type;
        typedef const V               &reference;
        typedef const V               *pointer;
    private:
	const ordering_t *o;
	typedef typename std::list<component_t>::const_reverse_iterator ordering_t_iterator;
	ordering_t_iterator it;

        explicit const_reverse_iterator(const ordering_t &o_, ordering_t_iterator it_) {
	    o = &o_;
	    it = it_;
	}

    public:
        // No need for copy ctor, copy assign. Defaults are fine.
	const_reverse_iterator() {
	    ;
	}

        explicit const_reverse_iterator(const ordering_t &o_) {
	    o = &o_;
	    it = o->rbegin();
	}

        explicit const_reverse_iterator(const ordering_t &o_, bool) {
	    o = &o_;
	    it = o->rend();
	}

        bool operator==(const _Self& B) { 
	    return it == B.it; 
	}

        bool operator!=(const _Self& B) { 
	    return it != B.it; 
	}

        bool operator<(const _Self& B) { 
	    return it < B.it; 
	}

        bool operator>(const _Self& B) { 
	    return it > B.it; 
	}

        bool operator<=(const _Self& B) { 
	    return it <= B.it; 
	}

        bool operator>=(const _Self& B) { 
	    return it >= B.it; 
	}

        _Self &operator++() { 
	    it++; 
	    return *this; 
	} 

        _Self operator++(int) { 
	    return _Self(*o, it++); 
	} 

        _Self &operator--() { 
	    it--; 
	    return *this; 
	}

        _Self operator--(int) { 
	    return _Self(*o, it--); 
	}

        reference operator*() const { 
	    return it->first;
	}

        pointer operator->() const { 
	    return it->first;
	}
    };

    const_reverse_iterator rbegin() const { 
	return const_reverse_iterator(ordering);
    }

    const_reverse_iterator rend() const { 
	return const_reverse_iterator(ordering, false);
    }

    const_reverse_iterator rbegin(V v) const { 
	assert(0);
    }

    const_reverse_iterator rend(V v) const { 
	assert(0);
    }
};

template <class graph_t>
void bourdoncle_component(const graph_t &graph, const unsigned int vertex, 
			  WTO<typename boost::graph_traits<graph_t>::vertex_descriptor> &wto, dfn_t &DFN, unsigned int &NUM, 
			  std::map<typename boost::graph_traits<graph_t>::vertex_descriptor, unsigned int> &index_map, 
			  std::vector<typename boost::graph_traits<graph_t>::vertex_descriptor> &index_rev_map, 
			  stack__t &stack, int &component, int &prevcomponent) {
    typedef typename boost::graph_traits<graph_t>::out_edge_iterator out_edge_iterator;
    out_edge_iterator ei, ee;
    int c;

    for (boost::tie(ei, ee) = boost::out_edges(index_rev_map[vertex], graph); ei != ee; ei++) {
	unsigned int succ;
	succ = index_map[boost::target(*ei, graph)];
	    
	if (DFN[succ] == 0) {
	    bourdoncle_visit(graph, succ, wto, DFN, NUM, index_map, index_rev_map, stack, component, prevcomponent);
	}
    }  

    wto.add(index_rev_map[vertex], component);
}


template <class graph_t>
unsigned int bourdoncle_visit(const graph_t &graph, const unsigned int vertex, 
			      WTO<typename boost::graph_traits<graph_t>::vertex_descriptor> &wto, dfn_t &DFN, unsigned int &NUM, 
			      std::map<typename boost::graph_traits<graph_t>::vertex_descriptor, unsigned int> &index_map, 
			      std::vector<typename boost::graph_traits<graph_t>::vertex_descriptor> &index_rev_map, 
			      stack__t &stack, int &component, int &prevcomponent) {
    typedef typename boost::graph_traits<graph_t>::out_edge_iterator out_edge_iterator;
    out_edge_iterator ei, ee;
    unsigned int head, min, succ, element;
    bool loop;

    stack.push_back(vertex);
	
    NUM += 1;
    head = DFN[vertex] = NUM;
    loop = false;

    for (boost::tie(ei, ee) = boost::out_edges(index_rev_map[vertex], graph); 
	 ei != ee; ei++) {
	succ = index_map[boost::target(*ei, graph)];

	if (DFN[succ] == 0) {
	    min = bourdoncle_visit(graph, succ, wto, DFN, NUM, index_map, index_rev_map, stack, component, prevcomponent);
	} else {
	    min = DFN[succ];
	}

	if (min <= head) {
	    head = min;
	    loop = true;
	}
    }

    if (head == DFN[vertex]) {
	DFN[vertex] = (unsigned int) -1;
	element = stack.back(); stack.pop_back();
	if (loop) {
	    int c;

	    while (element != vertex) {
		DFN[element] = 0;
		element = stack.back();	stack.pop_back();
	    }

	    component++;
	    bourdoncle_component(graph, vertex, wto, DFN, NUM, index_map, index_rev_map, stack, component, prevcomponent);
	    component--;
	} else {
/*
	    if (component > prevcomponent)
		wto.add(index_rev_map[vertex], prevcomponent);
	    else
		wto.add(index_rev_map[vertex], component);
	    prevcomponent = component;
// */
		wto.add(index_rev_map[vertex], component);
	}
    }

    return head;
}

// Hierarchical decomposition of a directed graph into strongly connected
// components and subcomponents
template <class graph_t>
void bourdoncle_partition(const graph_t &g, 
			  typename boost::graph_traits<graph_t>::vertex_descriptor entry_vertex,
			  WTO<typename boost::graph_traits<graph_t>::vertex_descriptor> &wto) {
    typedef typename boost::graph_traits<graph_t>::vertex_descriptor vertex_descriptor;  
    typedef typename boost::graph_traits<graph_t>::vertex_iterator vertex_iterator;
    typedef typename boost::graph_traits<graph_t>::edge_iterator edge_iterator;

    typedef std::map<vertex_descriptor, unsigned int> index_map_t;
    typedef std::vector<vertex_descriptor> index_rev_map_t;
    typedef typename std::list<std::pair<vertex_descriptor, int> > ordering_t;

    vertex_descriptor vd;
    vertex_iterator vi, ve;
    dfn_t DFN;
    index_rev_map_t index_rev_map;
    index_map_t index_map;
    stack__t stack;
    int component = 0, prevcomponent = 0;
    unsigned int NUM = 0;
    int j = 0;

    for (boost::tie(vi, ve) = boost::vertices(g); vi != ve; vi++) {
	index_map[*vi] = j;
	index_rev_map.push_back(*vi);
	DFN[j] = 0;
	j++;
    } 

    bourdoncle_visit(g, index_map[entry_vertex], wto, DFN, NUM,
            index_map, index_rev_map, stack, component, prevcomponent);

    wto.finalize();

#if 0
    {   // Just a debug block
        j = -1;
        debug2("WTO: ");
        for (typename ordering_t::iterator it = wto.ordering.begin(); it
                != wto.ordering.end(); it++) {

	    int k = j;
	    while (k > 0 && it->second < k--) {
		debug2(")");
	    }

            debug2("%s%s%d", j != -1 ? " " :
                    "", j != -1 && j < it->second ? "(" : "",
                    index_map[it->first]);
            j = it->second;
        }
        debug2("\n");
    }
#endif
}

// Local Variables: 
// mode: c++
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:

