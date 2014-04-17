#ifndef UTILS_COUNTED_H
#define UTILS_COUNTED_H

#include <boost/intrusive_ptr.hpp>
#include <cassert>
#include <stdexcept>

namespace utils {
// Enclosure into another namespace is needed to assure that
// intrusive_ptr* don't get automatically specialized for arbitrary
// classes.
namespace pt {

class Counted {
    mutable size_t counter; 
    Counted(const Counted&);
    Counted& operator=(const Counted&);

    friend void intrusive_ptr_add_ref(Counted*);
    friend void intrusive_ptr_release(Counted*);
    
protected:

    void ref() { // Increment ref counter
        counter++;
        if (counter == 0) {
            assert(false && "Ref counter overflow.");
            throw(std::overflow_error("Ref counter overflow."));
        }
    } 

    bool unref() { // Decrement ref counter
        if (counter <= 0) {
            assert(false && "Ref counter underflow.");
            throw(std::underflow_error("Ref counter underflow."));
        }
        return --counter == 0;
    }

public:
    Counted() : counter(0) {}
    size_t count() const { return counter; }
    virtual ~Counted() {}
};

inline void intrusive_ptr_add_ref(Counted* p) { 
    if (p) {
        p->ref(); 
    }
}

inline void intrusive_ptr_release(Counted* p) {
    if (p) {
        if (p->unref()) { // Zero active references left
            delete p;
        }
    }
}

} // End of pt namespace
} // End of utils namespace

#endif // UTILS_COUNTED_H
