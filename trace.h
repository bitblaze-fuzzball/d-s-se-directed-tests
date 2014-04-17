#ifndef __TRACE_H__
#define __TRACE_H__

#include <list>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/list.hpp>

using namespace std;

namespace boost { 
    namespace serialization {
	template<typename Archive>
	void serialize(Archive &ar, pair<unsigned int, int> &p, 
		       const unsigned int) {
	    ar & p.first;
	    ar & p.second;
	}
    }
}

class Trace {
private:
    typedef list<pair<unsigned int, int> > instr_t;
    instr_t t;

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version) {
	ar & t;
    }

public:
    Trace(); 
    ~Trace();

    void append(unsigned int, int);
    unsigned int size();
    
    class const_iterator {
        typedef const_iterator        _Self;
    public:
        typedef std::random_access_iterator_tag iterator_category;
        typedef instr_t              value_type;
        typedef size_t               difference_type;
        typedef const unsigned int   &reference;
        typedef const unsigned int   *pointer;
    private:
	Trace *t;
	instr_t::iterator tit;

        explicit const_iterator(Trace *t_, instr_t::iterator tit_) {
	    t = t_;
	    tit = tit_;
	}

    public:
        // No need for copy ctor, copy assign. Defaults are fine.
        const_iterator() {
	    t = NULL;
	}

        explicit const_iterator(Trace *t_) {
	    t = t_;
	    tit = t->t.begin();
	}

        explicit const_iterator(Trace *t_, bool) {
	    t = t_;
	    tit = t->t.end();
	}

        bool operator==(const _Self& B) { 
	    return tit == B.tit; 
	}

        bool operator!=(const _Self& B) { 
	    return tit != B.tit; 
	}

        _Self &operator++() { 
	    tit++; 
	    return *this; 
	} 

        _Self operator++(int) { 
	    return _Self(t, tit++); 
	} 

        _Self &operator--() { 
	    tit--; 
	    return *this; 
	}

        _Self operator--(int) { 
	    return _Self(t, tit--); 
	}

        reference operator*() const { 
	    return tit->first;
	}

        pointer operator->() const { 
	    return &(tit->first);
	}
    };

};

#endif

// Local Variables: 
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
