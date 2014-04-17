// TODO:
// 1) hasn't been tested.
// 2) Add an artificially created chunk for **argv
// 3) add interface for getting the address of that **argv chunk
// 4) change the constructor to take the size of **argv (number of
// strings and size of each string) as parameters

#ifndef GLOB_MEMMAP_H
#define GLOB_MEMMAP_H

#include "prog.h"
#include <algorithm>

namespace memmap {

struct Comparator {
    typedef std::pair<size_t,size_t> IntPair;
    bool operator()(const IntPair& a, const IntPair& b) const {
        return a.first <= b.first;
    }
};

// Proxy to easiliy access initialized globals
class MemMap {
private:
    typedef std::pair<size_t,size_t> IntPair;
    typedef std::map<size_t,const Section*> Adr2SecMap;
    Adr2SecMap addr2sec;
    typedef std::vector<IntPair> IntPairVec;
    typedef IntPairVec::const_iterator const_iterator;
    IntPairVec secs;
    const Prog* prog;

public:
    MemMap(const Prog &p) : prog(&p) {
        for (sections_t::const_iterator sit = p.sec_begin(); 
             sit != p.sec_end(); sit++) {
	    if ((*sit)->isAllocated()) {
		secs.push_back(IntPair((*sit)->getAddress(), 
				       (*sit)->getAddress() + 
				       (*sit)->getSize() - 1));
		addr2sec[(*sit)->getAddress()] = *sit;
	    }
        }
        std::sort(secs.begin(), secs.end(), Comparator());
    }

    ~MemMap();

    // Returns the lower bound of the closest memory section
    size_t getLowerBound(size_t a) const {
        const IntPair p(a, a);
        const_iterator I = std::lower_bound(secs.begin(), secs.end(), p,
                Comparator());
        if (I != secs.begin()) {
            const IntPair found = *--I;
            return found.first;
        } else {
            return 0;
        }
    }

    const Section* getSection(size_t a) const {
        const IntPair p(a, a);
        const_iterator I = std::lower_bound(secs.begin(), secs.end(), p,
                Comparator());
        if (I != secs.begin()) {
            const IntPair found = *--I;
            if (I->first <= a && I->second >= a) {
                Adr2SecMap::const_iterator MI = addr2sec.find(found.first);
                assert(MI != addr2sec.end() && "Address not found.");
                return MI->second;
            }
        }
        return 0;
    }

    bool isValid(size_t a) const { return getSection(a) != 0; }


    byte_t get(size_t a) const {
        const Section* s = getSection(a);
        assert(s && "Requested section not found.");
            /* Too verbose
#ifndef NDEBUG
            debug3("%.8x is in section %s %.8x-%.8x\n", a, 
               s->getName(), s->getAddress(), s->getAddress() +
               s->getSize() - 1);
#endif
// */
        return s->getByte(a);
    }

    void check() const {
        for (sections_t::const_iterator sit = prog->sec_begin(); 
            sit != prog->sec_end(); ++sit) {
            addr_t a = (*sit)->getAddress();

            for (bytes_t::const_iterator bit = (*sit)->bytes_begin();
                bit != (*sit)->bytes_end(); ++bit) {

                byte_t b = *bit;
                assert(isValid(a));
                assert(get(a) == b);
                ++a;
            }
        }
    }
};


} // End of the memmap namespace

#endif // GLOB_MEMMAP_H


// Local Variables: 
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
