// This file implements strided intervals. See T. Reps, G. Balakrishnan,
// J. Lim: Intermediate-representation recovery from low-level code,
// Workshop on partial evaluation and semantics-based program
// manipulation, ACM 2006.
//
// Notes: The semantics of the intervals implemented here is [low,
// high), in other words, the lower bound is inclusive, while the high
// bound is exclusive. The same semantics is used for C++ STL iterators.

// TODOs:
// 1) There might be many different types of intervals. StridedInterval
// needs a superclass Interval with operations common for all intervals.
// 2) Profile the code, and if there are a lot of different types of
// strides, think about splitting interval into a separate class
// BoundedInterval, so that the same interval can be shared among
// multiple strided intervals (with different strides but the same
// interval).
// 3) Cache cleanup functionality, say, every 100K additions

#ifndef ABSDOMAIN_BOUNDED_STRIDED_INTERVAL_H
#define ABSDOMAIN_BOUNDED_STRIDED_INTERVAL_H

#include "AbstractDomain.h"
#include "HashFunctions.h"
#include "Utilities.h"
#include <climits>

namespace absdomain {


class StridedInterval;

typedef boost::intrusive_ptr<StridedInterval> StridedIntervalPtr;

class StridedInterval : public AbstractDomain<StridedInterval> {
    // Note: Currently, I don't forsee this class being inherited from,
    // so none of the functions are virtual. If the design changes,
    // virtualize the members as needed...

    typedef StridedInterval _Self;

    const int lo, hi;
    const unsigned strd;

    StridedInterval(int low, int high, unsigned stride = 1);
    StridedInterval(const StridedInterval& i) :
        lo(i.lo), hi(i.hi), strd(i.strd) {
        assert(strd > 0 && "Stride must be > 0.");
    }

    static CacheTy::const_iterator_pair equal_range(int);
    static int computeHash(int h,int l, unsigned s) {
        return utils::inthash((int)s) ^ 
            (utils::inthash(h) - utils::inthash(l));
    }

    bool equal(int low, int high, unsigned stride) const {
        return lo == low && hi == high && strd == stride;
    }

public:
    // Use for creating intervals where lo <= hi
    // A comment on the meaning of strided intervals:
    // s[lo,hi] = {i | lo <= i < hi AND i from {lo + j*s | j from Z}}
    // Thus, constants (lo==hi) will have stride=0, and in all other
    // cases, stride > 0. If those conditions are not met, this function
    // will complain.
    static StridedIntervalPtr get(int lo, int hi, unsigned stride);
    // Use for creating bottom
    static StridedIntervalPtr getBot();
    // Use for creating top
    static StridedIntervalPtr getTop();
    // Use mostly for testing
    static StridedIntervalPtr getRand(int,int);
    // Use for generating constants
    static StridedIntervalPtr get(int, unsigned s = 1);

    StridedIntervalPtr join(_Self&);  
    StridedIntervalPtr meet(_Self&);
    StridedIntervalPtr widen(_Self&);
    // The bounds are strict, i.e., if the lower bound is [0,4], then 4
    // is the lowest possible value of the result of widening (keep in
    // mind that the upper bound is exclusive).
    //StridedIntervalPtr rwiden(_Self&, _Self&, _Self&);


    bool operator==(const StridedInterval& a) const {
        assert((!equal(a.lo, a.hi, a.strd) || &a == this) && 
                "Singleton property violated.");
        return &a == this;
    }
    bool operator!=(const StridedInterval& a) const {
        assert((equal(a.lo, a.hi, a.strd) || &a != this) &&
                "Singleton property violated.");
        return &a != this;
    }
    
    // Restricts the interval. E.g., 4[0,16].restrictUpperBound(12) =
    // 4[0,12], and 4[0,16].restrictUpperBound(18) = bottom. Keep in
    // mind that the upper bound is exclusive, while the lower is
    // inclusive.
    StridedIntervalPtr restrictUpperBound(int) const;
    StridedIntervalPtr restrictLowerBound(int) const;

    // Warning: These are comparisons on the strided intervals, and
    // might not be total. Thus, they are unsuitable for sorting sets of
    // strided intervals! Use Cmp class (specialized below) for sorting.
    bool ai_equal(const StridedInterval& i) const {
        return *this == i;
    }
    bool ai_distinct(const StridedInterval& i) const {
        return *this != i;
    }
    bool operator<=(const StridedInterval&) const;
    bool operator<(const StridedInterval&) const;
    bool operator>=(const StridedInterval&) const;
    bool operator>(const StridedInterval&) const;

    bool subsumes(const StridedInterval&) const;
    bool subsumedBy(const StridedInterval&) const;
    bool withinBoundsOf(const StridedInterval&) const;

    typedef std::vector<StridedIntervalPtr> SIPtrVector;
    typedef std::vector<std::pair<StridedIntervalPtr,splt::SplitTy> >
        SIPNVector;

    static void split(const SIPtrVector&, const SIPtrVector&,
            SIPNVector&);
    void split(const SIPtrVector& v, SIPNVector& s) {
        split(v, SIPtrVector(1, StridedIntervalPtr(this)), s);
    }

    bool overlaps(const StridedInterval&) const;
    bool adjacent(const StridedInterval& i) const {
        return hi == i.lo || lo == i.hi;
    }

    StridedIntervalPtr operator+(int offset);
    StridedIntervalPtr operator+(StridedInterval&);
    StridedIntervalPtr operator*(StridedInterval&);
    StridedIntervalPtr operator-();
    StridedIntervalPtr operator-(int offset) {
        return *this + (-offset);
    }
    StridedIntervalPtr operator-(StridedInterval& i) {
        return *this + *(-StridedInterval(i));
    }
    StridedIntervalPtr operator++() {
        return *this + 1;
    }
    StridedIntervalPtr operator--() {
        return *this - 1;
    }

    // Propagation of intervals across bitwise OR
    StridedIntervalPtr operator|(StridedInterval&);
    // Propagation of intervals across bitwise negation
    StridedIntervalPtr operator~();
    // Propagation of intervals across bitwise AND
    StridedIntervalPtr operator&(StridedInterval&);
    // Propagation of intervals across bitwise XOR
    StridedIntervalPtr operator^(StridedInterval&);

    StridedIntervalPtr sdivide(StridedInterval&);
    StridedIntervalPtr udivide(StridedInterval&);
    StridedIntervalPtr smod(StridedInterval&);
    StridedIntervalPtr umod(StridedInterval&);
    StridedIntervalPtr lshift(StridedInterval&);
    StridedIntervalPtr rshift(StridedInterval&);
    StridedIntervalPtr arshift(StridedInterval&);
    StridedIntervalPtr lrotate(StridedInterval&);
    StridedIntervalPtr rrotate(StridedInterval&);

    bool isTop() const;
    bool isBot() const;
    bool isZero() const { return lo == hi && lo == 0; }
    bool containsZero() const;
    bool isConstant() const { return lo + (int)strd == hi; }
    bool isTrue() const { return lo == 1 && isConstant(); }
    bool isFalse() const { return lo == 0 && isConstant(); }

    int getHash() const { return computeHash(lo,hi,strd); }
    int getHi() const { return hi; }
    int getLo() const { return lo; }
    unsigned getStride() const { return strd; }

    void print(std::ostream&) const;
};

} // End of the absdomain namespace

// Specializations required by RedBlackTree
namespace utils {

template <> struct Cmp<absdomain::StridedIntervalPtr> {
    int operator()(absdomain::StridedIntervalPtr a,
            absdomain::StridedIntervalPtr b) const {
        assert(a.get() && b.get() && "Comparing with a NULL ptr?");
        const int res = (a.get() == b.get()) ? 0 : a->getLo() <
            b->getLo() ? -1 : 1;
        return res;
    }
};

template <> struct Hash<absdomain::StridedIntervalPtr> {
    std::size_t operator()(absdomain::StridedIntervalPtr p) const {
        assert(p.get() && "Requested hash of a NULL ptr?");
        return static_cast<std::size_t>(p->getHash());
    }
};

template <> struct Max<absdomain::StridedIntervalPtr> {
    int operator()(absdomain::StridedIntervalPtr a) const {
        assert(a.get() && "Requested max of a NULL ptr?");
        return a->getHi();
    }
    int operator()(absdomain::StridedIntervalPtr a,
            int x, int y) const {
        int max = utils::max(x,y);
        if (a.get() && a->getHi() > max) max = a->getHi();
        return max;
    }
};

template <> struct Overlap<absdomain::StridedIntervalPtr> {
    bool operator()(absdomain::StridedIntervalPtr a,
            absdomain::StridedIntervalPtr b) const {
        assert(a.get() && b.get() && "Unexpected NULL ptr.");
        return a->overlaps(*b);
    }
    int low(absdomain::StridedIntervalPtr p) const {
        assert(p.get() && "Unexpected NULL ptr.");
        return p->getLo();
    }
    int high(absdomain::StridedIntervalPtr p) const {
        assert(p.get() && "Unexpected NULL ptr.");
        return p->getHi();
    }
};

} // End of the utils namespace

#endif // ABSDOMAIN_BOUNDED_STRIDED_INTERVAL_H
