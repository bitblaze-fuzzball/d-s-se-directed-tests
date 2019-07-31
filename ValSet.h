#ifndef ABSDOM_VALSET_H
#define ABSDOM_VALSET_H

#include "AbstractDomain.h"
#include "RedBlackTree.h"
#include <set>

namespace absdomain {

namespace rg {

enum RegionTy {
    WEAK_GLOBAL = 0,
    WEAK_REGISTER,
    WEAK_STACK,
    WEAK_HEAP,
    STRONG_GLOBAL,
    STRONG_REGISTER,
    STRONG_STACK,
    STRONG_HEAP,
    TERMINAL
};

enum ExtremesTy {
    NORMAL = 0, // Neither TOP nor BOTTOM
    TOP,
    BOT
};

} // End of the rg namespace

// Info: The T type has to satisfy all requirements described in
// RedBlackTree.h, and it has to inherit AbstractDomain.

template <typename T>
class Region;

// Note: region,value pairs not present in the value set are treated as
// the region,bottom pair.

template <typename T>
class ValueSet : public AbstractDomain<ValueSet<T> > {
public:
    typedef ValueSet<T> _Self;
    typedef T BaseDomain;
    typedef boost::intrusive_ptr<T> TPtr;
    typedef boost::intrusive_ptr<_Self> VSetPtr;
    typedef boost::intrusive_ptr<Region<T> > RegionPtr;
    typedef std::pair<RegionPtr, TPtr> AlocPair;

private:
    typedef typename utils::RedBlackTree<AlocPair> RBTree;
    typedef typename RBTree::RedBlackTreePtr RBTreePtr;
    typedef TPtr (T::*OperatorTy)(T&);
    typedef bool (T::*ComparatorTy)(const T&) const;

    RBTreePtr rbt;
    // Replacing this field with two statics (top,bot) doesn't actually
    // save any memory, so just leave it.
    rg::ExtremesTy ty;

    ValueSet(RBTreePtr p) : rbt(p), ty(rg::NORMAL) {
        assert(p.get() && "Unexpected NULL ptr.");
        assert(check() && "Invalid valset.");
    }
    ValueSet(rg::ExtremesTy t) : rbt(RBTree::get()), ty(t) {
        assert(check() && "Invalid valset.");
    }

    // Apply T.*OperatorTy to all elements in _Self
    VSetPtr apply(_Self&, OperatorTy);
    // Apply T.*OperatorTy to top values in each region
    VSetPtr apply_to_top(OperatorTy);
    // Apply T.*ComparatorTy
    bool compare(const _Self&, ComparatorTy) const;

public:
    typedef typename RBTree::const_iterator const_iterator;

    static VSetPtr get(int, unsigned s = 1);
    static VSetPtr get(AlocPair);
    static VSetPtr get(std::vector<AlocPair>&);
    template <typename Iter>
    static VSetPtr getFromIter(Iter,Iter);
    static VSetPtr getBot();
    static VSetPtr getTop();

    static void shutdown() { RBTree::shutdown(); }

    const_iterator begin() const { 
        return const_cast<const RBTree&>(*rbt).begin(); 
    }
    const_iterator end() const {
        return const_cast<const RBTree&>(*rbt).end(); 
    }

    VSetPtr join(_Self&);
    VSetPtr meet(_Self&);
    VSetPtr widen(_Self&);
    //VSetPtr rwiden(_Self&, _Self&, _Self&);

    VSetPtr insert(AlocPair p) {
        return isTop() ? VSetPtr(this) : VSetPtr(new
                ValueSet(rbt->insert(p)));
    }
    VSetPtr erase(AlocPair p) {
        return isBot() ? VSetPtr(this) : 
            VSetPtr(new ValueSet(rbt->erase(p)));
    }

    bool empty() const { return rbt->empty(); }
    bool isTop() const { return empty() && ty == rg::TOP; }
    bool isBot() const { return empty() && ty == rg::BOT; }
    bool isConstant() const;
    bool isZero() const;
    size_t getSize() const { return rbt->treeSize(); }
    bool isTrue() const;
    bool isFalse() const;

    bool operator==(const _Self& a) const {
        return ty == a.ty && *rbt == *a.rbt;
    }
    bool operator!=(const _Self& a) const {
        return ty != a.ty || *rbt != *a.rbt;
    }

    // Warning: These are potentially expensive quadratic-time
    // operations, as they cross-compare elements in both sets. Also,
    // they are not total, so don't use them for sorting.
    //
    // Warning: These operators allow comparisons among non-global and
    // global (constant) regions. For instance, R0:1[1,2] == R3:1[1,2]
    // will return true, as it should. On the other hand, equal/distinct
    // would return false, as regions are different.
    bool ai_equal(const _Self&) const;
    bool ai_distinct(const _Self&) const;
    bool operator<=(const _Self&) const;
    bool operator<(const _Self&) const ;
    bool operator>=(const _Self& a) const;
    bool operator>(const _Self& a) const;

    bool subsumes(const _Self& a) const;
    bool subsumedBy(const _Self& a) const { return a.subsumes(*this); }
    bool withinBoundsOf(const _Self&) const;

    // These operators are precise on any ValueSet
    VSetPtr operator+(int offset) ;
    VSetPtr operator-()           ;
    VSetPtr operator~()           ;
    VSetPtr operator-(int offset) { return *this + (-offset); }
    VSetPtr operator++()          { return *this + 1; }
    VSetPtr operator--()          { return *this + (-1); }

    // These operators are precise only under certain conditions, for
    // instance, if there is only one element in both sets and both
    // elements belong to the same region. See the comments in the
    // implementation (below) for more details.
    VSetPtr operator+(_Self&);
    VSetPtr operator*(_Self&);
    VSetPtr operator-(_Self&);
    VSetPtr operator|(_Self&);
    VSetPtr operator&(_Self&);
    VSetPtr operator^(_Self&);

    VSetPtr sdivide(_Self&);
    VSetPtr udivide(_Self&);
    VSetPtr smod(_Self&);
    VSetPtr umod(_Self&);
    VSetPtr lshift(_Self&);
    VSetPtr rshift(_Self&);
    VSetPtr arshift(_Self&);
    VSetPtr lrotate(_Self&);
    VSetPtr rrotate(_Self&);

    // ValueSet is already cached by using RB trees.
    int getHash() const {
        return rbt->getHash();
    }
    void print(std::ostream&) const;

    bool check() const;
};

// *** Implementation ***

template <typename T>
inline typename ValueSet<T>::VSetPtr ValueSet<T>::get(int x, unsigned s)
{
    return get(std::make_pair(Region<T>::getFresh(), T::get(x, s)));
}

template <typename T>
inline typename ValueSet<T>::VSetPtr ValueSet<T>::get(AlocPair p) {
    return VSetPtr(new ValueSet(RBTree::get(p)));
}

template <typename T>
inline typename ValueSet<T>::VSetPtr
ValueSet<T>::get(std::vector<AlocPair>& v) {
    return v.empty() ? getBot() : VSetPtr(new ValueSet(RBTree::get(v)));
}

template <typename T>
template <typename Iter>
inline typename ValueSet<T>::VSetPtr
ValueSet<T>::getFromIter(Iter I, Iter E) {
    return I == E ? getBot() : VSetPtr(new ValueSet(RBTree::get(I, E)));
}

template <typename T>
inline typename ValueSet<T>::VSetPtr ValueSet<T>::getBot() {
    return VSetPtr(new ValueSet(rg::BOT));
}

template <typename T>
inline typename ValueSet<T>::VSetPtr ValueSet<T>::getTop() {
    return VSetPtr(new ValueSet(rg::TOP));
}

// Note: The code for join, meet, widen is annoyingly repetitive, but
// the differences are significant enough to make the design of a single
// do-all function difficult. Think about how the same could be done
// with less code (like with apply function for binary operators).

template <typename T>
inline typename ValueSet<T>::VSetPtr ValueSet<T>::join(_Self& p) {
    // Handle special cases first
    if (isBot() || p.isTop()) {
        return VSetPtr(&p);
    } else if (p.isBot() || isTop()) {
        return VSetPtr(this);
    } else if (*this == p) {
        return VSetPtr(&p);
    }

    assert(p.rbt.get() && rbt.get() && "Unexpected NULL ptr(s).");

    std::vector<AlocPair> tmp;
    utils::Cmp<RegionPtr> cmp;
    
    const_iterator TI = begin(), TE = end(), OI = p.begin(), OE =
        p.end(); 
    for (; TI != TE && OI != OE; ) {

        AlocPair tp = TI->get();
        AlocPair op = OI->get();

        const int c = cmp(tp.first, op.first);

        if (c == 0) {
            TPtr jv = tp.second->join(*op.second);
            tmp.push_back(std::make_pair(tp.first, jv));
            ++TI; ++OI;
        } else if (c < 0) {
            tmp.push_back(tp);
            ++TI;
        } else { // c > 0
            tmp.push_back(op);
            ++OI;
        }
    }

    // Handle unprocessed tails
    for (; TI != TE; ++TI) {
        tmp.push_back(TI->get());
    }

    for (; OI != OE; ++OI) {
        tmp.push_back(OI->get());
    }

    VSetPtr res = get(tmp);
    assert(res->subsumes(*this) && "Invalid join.");
    assert(res->subsumes(p) && "Invalid join.");
    return res;
}

template <typename T>
inline typename ValueSet<T>::VSetPtr ValueSet<T>::meet(_Self& p) {
    // Handle special cases first
    if (isBot() || p.isBot()) {
        return getBot();
    } else if (isTop()) {
        return VSetPtr(&p);
    } else if (p.isTop()) {
        return VSetPtr(this);
    } else if (*this == p) {
        return VSetPtr(&p);
    }

    assert(p.rbt.get() && rbt.get() && "Unexpected NULL ptr(s).");

    std::vector<AlocPair> tmp;
    utils::Cmp<RegionPtr> cmp;
    
    for (const_iterator TI = begin(), TE = end(), OI = p.begin(), OE =
            p.end(); TI != TE && OI != OE; ) {
        
        AlocPair tp = TI->get();
        AlocPair op = OI->get();
        const int c = cmp(tp.first, op.first);

        if (c == 0) {
            TPtr mv = tp.second->meet(*op.second);
            tmp.push_back(std::make_pair(tp.first, mv));
            ++TI; ++OI;
        } else if (c < 0) {
            ++TI;
        } else { // oId < tId
            ++OI;
        }
    }

    return get(tmp);
}

template <typename T>
inline typename ValueSet<T>::VSetPtr ValueSet<T>::widen(_Self& with) {
    // Handle special cases first
    if (isTop() || with.isTop()) {
        return getTop();
    } else if (isBot()) {
        return with.isBot() ? getBot() : getTop();
    } else if (with.isBot() || *this == with) {
        return VSetPtr(this);
    }
    
    assert(with.rbt.get() && rbt.get() && "Unexpected NULL ptr(s).");

    std::vector<AlocPair> tmp;
    utils::Cmp<RegionPtr> cmp;

    const_iterator 
        TI = begin(), TE = end(),
        WI = with.begin(), WE = with.end();

    for (; TI != TE && WI != WE; ) {
        AlocPair tp = TI->get();
        AlocPair wp = WI->get();
        const int c = cmp(tp.first, wp.first);
 
        if (c == 0) {
            TPtr res = tp.second->widen(*wp.second);
            tmp.push_back(std::make_pair(tp.first, res));
            ++TI; ++WI;
        } else if   (c < 0) {
            tmp.push_back(tp);
            ++TI;
        } else { // (c > 0) 
            tmp.push_back(wp);
            ++WI;
         }
    }
 
    for (; TI != TE; ++TI) { // Elements from 'this' missing
        tmp.push_back(TI->get());
    }
 
    for (; WI != WE; ++WI) { // Elements from 'with' missing
        tmp.push_back(WI->get());
    }
 
    VSetPtr res = get(tmp);
    return res;
    //return rwiden(with, *getTop(), *getTop());
}

/*
template <typename T>
inline typename ValueSet<T>::VSetPtr ValueSet<T>::rwiden(_Self& with,
        _Self& lower, _Self& upper) {

    // Handle special cases first
    if (isTop() || with.isTop()) {
        return getTop();
    } else if (isBot()) {
        return with.isBot() ? getBot() : getTop();
    } else if (with.isBot() || *this == with) {
        return VSetPtr(this);
    }

    assert(with.rbt.get() && rbt.get() && "Unexpected NULL ptr(s).");

    std::vector<AlocPair> tmp;
    utils::Cmp<RegionPtr> cmp;

    const_iterator 
        TI = begin(), TE = end(),
        WI = with.begin(), WE = with.end(),
        LI = lower.begin(), LE = lower.end(),
        UI = upper.begin(), UE = upper.end();

    assert((!lower.isTop() || LI == LE) && "Non-empty top?");
    assert((!upper.isTop() || UI == UE) && "Non-empty top?");

    for (; TI != TE && WI != WE; ) {
        AlocPair tp = TI->get();
        AlocPair wp = WI->get();
        const int c = cmp(tp.first, wp.first);

        if (c == 0) {
            // Skip over lower bounds useless for restriction
            while (LI != LE && cmp(LI->get().first, tp.first) < 0) {
                ++LI;
            }
            // Skip over upper bounds useless for restriction
            while (UI != UE && cmp(UI->get().first, tp.first) < 0) {
                ++UI;
            }

            T& lb = (LI != LE && LI->get().first == tp.first) ? 
                *LI->get().second : *T::getTop();
            T& ub = (UI != UE && UI->get().first == tp.first) ? 
                *UI->get().second : *T::getTop();
            TPtr res = tp.second->rwiden(*wp.second, lb, ub);
            tmp.push_back(std::make_pair(tp.first, res));
            ++TI; ++WI;
        } else if   (c < 0) {
            tmp.push_back(tp);
            ++TI;
        } else { // (c > 0) 
            tmp.push_back(wp);
            ++WI;
        }
    }

    for (; TI != TE; ++TI) { // Elements from 'this' missing
        tmp.push_back(TI->get());
    }

    for (; WI != WE; ++WI) { // Elements from 'with' missing
        tmp.push_back(WI->get());
    }

    VSetPtr res = get(tmp);
    assert(res->subsumes(*this) && "Invalid widen.");
    assert(res->subsumes(with) && "Invalid widen.");
    return res;
}// */

template <typename T>
inline bool ValueSet<T>::compare(const _Self& a, ComparatorTy op) const{
    for (const_iterator TI = begin(), TE = end(); TI != TE; ++TI) {
        AlocPair tp = TI->get();
        for (const_iterator AI = a.begin(), AE = a.end(); AI != AE;
                ++AI) {
            AlocPair ap = AI->get();
            // Regions can be compared iff they are the same singleton
            // region type, or have the same ID
            if (tp.first->getId() == ap.first->getId() ||
                tp.first->isGlobal() || ap.first->isGlobal()) {
                if (((*tp.second).*op)(*ap.second)) {
                    // Comparable regions, result is true
                    continue;
                }
            }
            // Incomparable regions or the result is false
            return false;
        }
    }
    return true;
}
    
template <typename T>
inline bool ValueSet<T>::isConstant() const {
    for (const_iterator I = begin(), E = end(); I != E; ++I) {
        AlocPair p = I->get();
        if (!p.second->isConstant()) {
            return false;
        }
    }
    return true;
}

template <typename T>
inline bool ValueSet<T>::isZero() const {
    for (const_iterator I = begin(), E = end(); I != E; ++I) {
        AlocPair p = I->get();
        if (!p.second->isZero()) {
            return false;
        }
    }
    return true;
}

template <typename T>
inline bool ValueSet<T>::isTrue() const {
    for (const_iterator I = begin(), E = end(); I != E; ++I) {
        AlocPair p = I->get();
        if (!p.second->isTrue()) {
            return false;
        }
    }
    return true;
}

template <typename T>
inline bool ValueSet<T>::isFalse() const {
    for (const_iterator I = begin(), E = end(); I != E; ++I) {
        AlocPair p = I->get();
        if (!p.second->isFalse()) {
            return false;
        }
    }
    return true;
}

template <typename T>
inline bool ValueSet<T>::ai_equal(const _Self& a) const {
    return compare(a, &T::operator==);
}

template <typename T>
inline bool ValueSet<T>::ai_distinct(const _Self& a) const {
    return compare(a, &T::operator!=);
}

template <typename T>
inline bool ValueSet<T>::operator<=(const _Self& a) const {
    return compare(a, &T::operator<=);
}

template <typename T>
inline bool ValueSet<T>::operator<(const _Self& a) const {
    return compare(a, &T::operator<);
}

template <typename T>
inline bool ValueSet<T>::operator>=(const _Self& a) const {
    return compare(a, &T::operator>=);
}

template <typename T>
inline bool ValueSet<T>::operator>(const _Self& a) const {
    return compare(a, &T::operator>);
}

template <typename T>
inline bool ValueSet<T>::subsumes(const _Self& a) const {
    if (isTop() || a.isBot() || *this == a) {
        return true;
    } else if (isBot() || a.isTop()) {
        return false;
    } else if (a.rbt->treeSize() > rbt->treeSize()) {
        return false;
    }

    const_iterator TI = begin(), TE = end(), AI = a.begin(), AE =
        a.end();
    utils::Cmp<RegionPtr> cmp;

    for (; TI != TE && AI != AE; ) {
        AlocPair tp = TI->get();
        AlocPair ap = AI->get();
        const int c = cmp(tp.first, ap.first);

        if (c == 0) {
            // Equality returns true for comparisons between glob and
            // other regions, as long as they have the same content.
            if (!tp.second->subsumes(*ap.second)) {
                return false;
            }
            ++TI; ++AI;
        } else if (c < 0) { // A is missing an element
            ++TI;
        } else { // c > 0 ---  'this' is missing an element
            return false;
        }
    }

    return AI == AE;
}

template <typename T>
inline bool ValueSet<T>::withinBoundsOf(const _Self& a) const {
    if (a.isBot()) {
        return isBot();
    } else if (a.isTop() || isBot() || *this == a) {
        return true;
    } else if (isTop()) {
        return false;
    }

    const_iterator TI = begin(), TE = end(), AI = a.begin(), AE =
        a.end();
    utils::Cmp<RegionPtr> cmp;

    for (; TI != TE && AI != AE; ) {
        AlocPair tp = TI->get();
        AlocPair ap = AI->get();
        const int c = cmp(tp.first, ap.first);

        if (c == 0) {
            if (!tp.second->withinBoundsOf(*ap.second)) {
                return false;
            }
            ++TI; ++AI;
        } else if (c < 0) {
            return false;
        } else {// c > 0
            ++AI;
        }
    }

    return TI == TE;
}

template <typename T>
inline typename ValueSet<T>::VSetPtr ValueSet<T>::operator+(int offset){
    if (isTop() || isBot()) {
        return VSetPtr(this);
    }
    assert(rbt.get() && "Unexpected NULL ptr(s).");

    std::vector<AlocPair> tmp;

    for (const_iterator I = begin(), E = end(); I != E; ++I) {
        AlocPair p = I->get();
        tmp.push_back(std::make_pair(p.first, *p.second + offset));
    }
    
    return get(tmp);
}

template <typename T>
inline typename ValueSet<T>::VSetPtr ValueSet<T>::operator-() {
    if (isTop() || isBot()) {
        return VSetPtr(this);
    }
    assert(rbt.get() && "Unexpected NULL ptr(s).");

    std::vector<AlocPair> tmp;

    for (const_iterator I = begin(), E = end(); I != E; ++I) {
        AlocPair p = I->get();
        tmp.push_back(std::make_pair(p.first, -*p.second));
    }
    
    return get(tmp);
}

template <typename T>
inline typename ValueSet<T>::VSetPtr ValueSet<T>::operator~() {
    if (isTop() || isBot()) {
        return VSetPtr(this);
    }
    assert(rbt.get() && "Unexpected NULL ptr(s).");

    std::vector<AlocPair> tmp;

    for (const_iterator I = begin(), E = end(); I != E; ++I) {
        AlocPair p = I->get();
        tmp.push_back(std::make_pair(p.first, ~*p.second));
    }
    
    return get(tmp);
}

template <typename T>
inline typename ValueSet<T>::VSetPtr ValueSet<T>::apply(_Self& a,
        OperatorTy op) {

    const_iterator TI = begin(), TE = end(), AI = a.begin(), AE =
        a.end();

    // Check whether operation can be performed. It can be performed
    // either if regions have the same id, or if they are the same
    // singleton type, or one of the operands is a constant
    for (; TI != TE; ++TI) {
        AlocPair tp = TI->get();
        for (; AI != AE; ++AI) {
            AlocPair ap = AI->get();

            if (tp.first->getId() != ap.first->getId() &&
                !tp.first->isGlobal() && !ap.first->isGlobal()) {
                return getTop();
            }
        }
    }

    // Ok, we can perform the operation
    TI = begin(), TE = end(), AI = a.begin(), AE = a.end();
    VSetPtr vs = getBot();

    for (; TI != TE; ++TI) {
        AlocPair tp = TI->get();
        for (; AI != AE; ++AI) {
            AlocPair ap = AI->get();

            vs = vs->join(*get(std::make_pair(tp.first->isGlobal() ?
                            ap.first : tp.first,
                            ((*tp.second).*op)(*ap.second))));
        }
    }

    return vs;
}

template <typename T>
inline typename ValueSet<T>::VSetPtr
ValueSet<T>::apply_to_top(OperatorTy op) {

    const_iterator TI = begin(), TE = end();

    VSetPtr vs = getBot();

    for (; TI != TE; ++TI) {
        AlocPair tp = TI->get();

	TPtr op_result = ((*tp.second).*op)(*(T::getTop()));

	vs = vs->join(*get(std::make_pair(tp.first, op_result)));
    }

    return vs;
}


template <typename T>
inline typename ValueSet<T>::VSetPtr ValueSet<T>::operator+(_Self& a) {
    // Handle special cases first
    if (isTop() || a.isTop()) {
        return getTop();
    } else if (isBot()) {
        return VSetPtr(&a);
    } else if (a.isBot()) {
        return VSetPtr(this);
    }

    return apply(a, &T::operator+);
}

template <typename T>
inline typename ValueSet<T>::VSetPtr ValueSet<T>::operator*(_Self& a) {
    // Handle special cases first
    if (isTop() || a.isTop()) {
        return getTop();
    } else if (isBot() || a.isBot()) {
        return getBot();
    }

    return apply(a, &T::operator*);
}


template <typename T>
inline typename ValueSet<T>::VSetPtr ValueSet<T>::operator-(_Self& a) {
    // Handle special cases first
    if (isTop() || a.isTop()) {
        return getTop();
    } else if (isBot()) {
        return -a;
    } else if (a.isBot()) {
        return VSetPtr(this);
    }

    return apply(a, &T::operator-);
}

template <typename T>
inline typename ValueSet<T>::VSetPtr ValueSet<T>::operator|(_Self& a) {
    // Handle special cases first
    if (isTop() || a.isTop()) {
        return getTop();
    } else if (isBot()) {
        return VSetPtr(&a);
    } else if (a.isBot()) {
        return VSetPtr(this);
    }

    return apply(a, &T::operator|);
}

template <typename T>
inline typename ValueSet<T>::VSetPtr ValueSet<T>::operator&(_Self& a) {
    // Handle special cases first
    if (isTop()) {
	return a.apply_to_top(&T::operator&);
    } else if (a.isTop()) {
	return apply_to_top(&T::operator&);
    } else if (isBot() || a.isBot()) {
        return getBot();
    }

    return apply(a, &T::operator&);
}

template <typename T>
inline typename ValueSet<T>::VSetPtr ValueSet<T>::operator^(_Self& a) {
    // Handle special cases first
    if (isTop() || a.isTop()) {
        return getTop();
    } else if (isBot()) {
        return VSetPtr(&a);
    } else if (a.isBot()) {
        return VSetPtr(this);
    }

    return apply(a, &T::operator^);
}

template <typename T>
inline typename ValueSet<T>::VSetPtr ValueSet<T>::sdivide(_Self& a) {
    // Handle special cases first
    if (isTop() || a.isBot()) {
        return getTop();
    } else if (isBot()) {
        return getBot();
    }

    return apply(a, &T::sdivide);
}

template <typename T>
inline typename ValueSet<T>::VSetPtr ValueSet<T>::udivide(_Self& a) {
    // Handle special cases first
    if (isTop() || a.isBot()) {
        return getTop();
    } else if (isBot()) {
        return getBot();
    }

    return apply(a, &T::udivide);
}

template <typename T>
inline typename ValueSet<T>::VSetPtr ValueSet<T>::smod(_Self& a) {
    // Handle special cases first
    if (a.isBot() || a.isTop()) {
        return getTop();
    } else if (isBot()) {
        return getBot();
    }

    return apply(a, &T::smod);
}

template <typename T>
inline typename ValueSet<T>::VSetPtr ValueSet<T>::umod(_Self& a) {
    // Handle special cases first
    if (a.isBot() || a.isTop()) {
        return getTop();
    } else if (isBot()) {
        return getBot();
    }

    return apply(a, &T::umod);
}

template <typename T>
inline typename ValueSet<T>::VSetPtr ValueSet<T>::lshift(_Self& a) {
    // Handle special cases first
    if (isBot()) {
        return getBot();
    } else if (a.isBot() || a.isZero()) {
        return VSetPtr(this);
    } else if (isTop() || a.isTop()) {
        return getTop();
    }

    return apply(a, &T::lshift);
}

template <typename T>
inline typename ValueSet<T>::VSetPtr ValueSet<T>::rshift(_Self& a) {
    // Handle special cases first
    if (isBot()) {
        return getBot();
    } else if (a.isBot() || a.isZero()) {
        return VSetPtr(this);
    } else if (isTop() || a.isTop()) {
        return getTop();
    }

    return apply(a, &T::rshift);
}

template <typename T>
inline typename ValueSet<T>::VSetPtr ValueSet<T>::arshift(_Self& a) {
    // Handle special cases first
    if (isBot()) {
        return getBot();
    } else if (a.isBot() || a.isZero()) {
        return VSetPtr(this);
    } else if (isTop() || a.isTop()) {
        return getTop();
    }

    return apply(a, &T::arshift);
}

template <typename T>
inline typename ValueSet<T>::VSetPtr ValueSet<T>::lrotate(_Self& a) {
    // Handle special cases first
    if (isBot()) {
        return getBot();
    } else if (a.isBot() || a.isZero()) {
        return VSetPtr(this);
    } else if (isTop() || a.isTop()) {
        return getTop();
    }

    return apply(a, &T::lrotate);
}

template <typename T>
inline typename ValueSet<T>::VSetPtr ValueSet<T>::rrotate(_Self& a) {
    // Handle special cases first
    if (isBot()) {
        return getBot();
    } else if (a.isBot() || a.isZero()) {
        return VSetPtr(this);
    } else if (isTop() || a.isTop()) {
        return getTop();
    }

    return apply(a, &T::rrotate);
}

template <typename T>
inline void ValueSet<T>::print(std::ostream& os) const {
    if (isTop()) {
        os << "TOP";
    } else if (isBot()) {
        os << "BOT";
    } else {
        os << '{';
        for (const_iterator I = begin(), E = end(); I != E; ++I) {
            AlocPair p = I->get();
            os << "R" << p.first->getId() << ":" << *p.second;
            const_iterator J = I;
            ++J;
            if (J != E) {
                os << " ";
            }
        }
        os << '}';
    }
}

template <typename T>
inline bool ValueSet<T>::check() const {
    std::set<int> ids;
    for (const_iterator I = begin(), E = end(); I != E; ++I) {
        RegionPtr r = I->get().first;
        const int id = r->getId();
        if (ids.find(id) != ids.end()) {
            return false;
        }
        ids.insert(id);
    }
    return true;
}

} // end of the absdomain namespace

#endif // ABSDOM_VALSET_H
