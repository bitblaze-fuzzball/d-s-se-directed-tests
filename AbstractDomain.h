#ifndef ABSTRACT_DOMAIN_H
#define ABSTRACT_DOMAIN_H

#include <iostream>
#include <vector>
#include <tr1/tuple>
#include "Counted.h"
#include "CompilerPortability.h"
#include "Utilities.h"

namespace absdomain {

namespace splt {

enum SplitTy {
    FIRST = 0,
    SECOND,
    BOTH,
    NONE,
    TERMINATOR
};

} // End of the splt namespace

// T is the class that inherits the base class.
template <class T>
class AbstractDomain : public utils::pt::Counted {
public:

    typedef typename boost::intrusive_ptr<T> DomainPtr;
    typedef utils::Multimap<int, DomainPtr>  CacheTy;

protected:

    // Warning: Has to be initialized for every subtype!
    static THREADLOCAL CacheTy* cache;

    static void initCache() {
        if (!cache) cache = new CacheTy();
    }

    AbstractDomain() {}
    virtual ~AbstractDomain() {}

public:

    static void shutdown() { delete cache; cache = 0; }

    virtual DomainPtr join(T&) = 0;
    virtual DomainPtr meet(T&) = 0;
    virtual DomainPtr widen(T&)= 0;
    //virtual DomainPtr rwiden(T&, T&, T&) = 0; // Restricted widening

    virtual bool isTop() const = 0;
    virtual bool isBot() const = 0;
    bool isTopOrBottom() const { return isTop() || isBot(); }

    static DomainPtr getBot() { return T::getBot(); }
    static DomainPtr getTop() { return T::getTop(); }

    
    // These are operators on the actual objects, i.e., strict
    // (dis)equality among objects, ignoring the specific semantics of
    // the abstract domain.
    virtual bool operator==(const T&) const = 0;
    virtual bool operator!=(const T&) const = 0;
    
    // These are all operators over the abstract domain and might not be
    // total.
    virtual bool ai_equal(const T&)    const = 0;
    virtual bool ai_distinct(const T&) const = 0;
    virtual bool operator<=(const T&) const  = 0;
    virtual bool operator<(const T&) const   = 0;
    virtual bool operator>=(const T&) const  = 0;
    virtual bool operator>(const T&) const   = 0;

    virtual bool subsumes(const T&) const    = 0;
    virtual bool subsumedBy(const T&) const  = 0;
    virtual bool withinBoundsOf(const T&) const= 0;
    virtual bool isConstant() const          = 0;
    virtual bool isZero() const              = 0;
    virtual bool isTrue() const              = 0;
    virtual bool isFalse() const             = 0;
    virtual bool isMaybe() const {
        return !isTrue() && !isFalse();
    }

    // These function should be implemented for interval-like
    // domains, that can split longer intervals into smaller ones, like
    // in the ASI analysis, see: Ramalingam, G., Field, J., and Tip, F.:
    // Aggregate structure identification and its application to program
    // analysis, In Proc. Symp. on Princ. of Prog. Lang., 1999.
    typedef std::vector<DomainPtr> DomainPtrVec;
    typedef std::vector<std::pair<DomainPtr, splt::SplitTy> >
        DomainSplitVec;
    virtual void split(const DomainPtrVec&, DomainSplitVec&) {}

    virtual DomainPtr operator+(int offset) = 0;
    virtual DomainPtr operator+(T&)         = 0;
    virtual DomainPtr operator*(T&)         = 0;
    virtual DomainPtr operator-()           = 0;
    virtual DomainPtr operator-(int offset) = 0;
    virtual DomainPtr operator-(T&)         = 0;
    virtual DomainPtr operator++()          = 0;
    virtual DomainPtr operator--()          = 0;
    virtual DomainPtr operator|(T&)         = 0;
    virtual DomainPtr operator~()           = 0;
    virtual DomainPtr operator&(T&)         = 0;
    virtual DomainPtr operator^(T&)         = 0;

    virtual DomainPtr sdivide(T&) = 0;
    virtual DomainPtr udivide(T&) = 0;
    virtual DomainPtr smod(T&)    = 0;
    virtual DomainPtr umod(T&)    = 0;
    virtual DomainPtr lshift(T&)  = 0;
    virtual DomainPtr rshift(T&)  = 0;
    virtual DomainPtr arshift(T&) = 0;
    virtual DomainPtr lrotate(T&) = 0;
    virtual DomainPtr rrotate(T&) = 0;

    virtual int getHash() const = 0;
    virtual void print(std::ostream&) const = 0;

    // Prints to std::cerr, for debugging. Definition in the cpp file,
    // some debuggers have problems with inlined functions.
    void print() const;
};

template <class T>
inline void AbstractDomain<T>::print() const {
    print(std::cerr);
}

template <class T>
inline std::ostream& operator<<(std::ostream& os, const
        AbstractDomain<T>& p) {
    p.print(os); return os;
}

} // End of the absdomain namespace

#endif // ABSTRACT_DOMAIN_H
