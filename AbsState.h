#ifndef ABSTRACT_STATE_H
#define ABSTRACT_STATE_H

#include "AbsRegion.h"
#include "RedBlackTree.h"
#include "Counted.h"
#include <set>
#include <boost/intrusive_ptr.hpp>
#ifndef NDEBUG
#include <sstream>
#endif

namespace absdomain {

template <typename T>
class AbsState : public utils::pt::Counted {
    typedef AbsState<T> _Self;
public:
    typedef typename boost::intrusive_ptr<T> TPtr;
    typedef typename absdomain::Region<T> RegionType;
    typedef typename boost::intrusive_ptr<Region<T> > RegionPtr;
    typedef boost::intrusive_ptr<_Self> StatePtr;
private:
    typedef typename utils::RedBlackTree<RegionPtr> RBTree;
    typedef typename boost::intrusive_ptr<RBTree> RBTreePtr;

    RBTreePtr rbt;

    // Find region with the given ID
public:
    RegionPtr find(int);

    // Checks whether the region is in the set
    bool knownId(RegionPtr p) {
        return find(p->getId()) != 0;
    }

    static TPtr getIntervalFor(reg::RegEnumTy ty) {
        assert(ty < reg::TERM_REG && "Register index out of bounds.");
        const struct regs* r = &regs[(unsigned)ty];
        assert(r && "Unexpected NULL ptr.");
        return T::get(r->begin, r->end, r->size);
    }

private:
    AbsState() {}
    AbsState(RBTreePtr p) : rbt(p) {}

public:
    typedef std::vector<StatePtr> StateVectorTy;
    typedef std::vector<RegionPtr> RegionVecTy;
    typedef std::pair<RegionPtr, TPtr> AlocPair;
    typedef ValueSet<T> ValSetTy;
    typedef boost::intrusive_ptr<ValSetTy> VSetPtr;
    typedef typename RBTree::const_iterator const_iterator;
    //typedef typename RBTree::iterator iterator;

    // Creates a state with initialized register, stack, and global
    // region. ESP points to the beginning of the stack. All three are
    // strongly updatable.
    static StatePtr getInitForMain();
    // Returns a state composed of three regions, they are expected to
    // be global, local, and register regions.
    static StatePtr get(RegionPtr, RegionPtr, RegionPtr);
    static StatePtr get(RegionVecTy&);

    RegionPtr getGlobal()   { return find(0); }
    RegionPtr getRegister() { return find(1); }
    RegionPtr getStack()    { return find(2); }

    bool empty() const { return rbt.get() == 0 || rbt->empty(); }

    const_iterator begin() const { 
        return const_cast<const RBTree&>(*rbt).begin(); 
    }
    const_iterator end() const { 
        return const_cast<const RBTree&>(*rbt).end(); 
    }
    //iterator begin() { return rbt->begin(); }
    //iterator end() { return rbt->end(); }

    StatePtr addNewRegion(rg::RegionTy);
    StatePtr addNewRegion(rg::RegionTy, TPtr);
    // Converts all the regions, except for globals and registers, to
    // the weakly updatable versions.
    StatePtr getWeaklyUpdatable();
    StatePtr getStronglyUpdatable();
    // Replaces the current stack with another (used when switching the
    // context from the callee's back to the caller's context)
    StatePtr replaceStack(RegionPtr);
    StatePtr replaceRegister(RegionPtr);
    StatePtr replaceHeap(int, RegionPtr);
    StatePtr removeStack();
    StatePtr addStack(RegionPtr);
    StatePtr discardFrame(int);

    VSetPtr read(AlocPair);
    VSetPtr read(reg::RegEnumTy ty) {
        assert(rbt.get() && "About to dereference a NULL ptr.");
        return read(std::make_pair(getRegister(), getIntervalFor(ty)));
    }
    StatePtr write(AlocPair, VSetPtr);
    VSetPtr read(VSetPtr);
    StatePtr write(VSetPtr, VSetPtr);
    StatePtr write(reg::RegEnumTy ty, VSetPtr p) {
        assert(rbt.get() && "About to dereference a NULL ptr.");
        return write(std::make_pair(getRegister(), getIntervalFor(ty)),
                p);
    }

    StatePtr join(_Self&);
    StatePtr meet(_Self&);
    StatePtr widen(_Self&);
    // Warning: In the classes that inherit abstract domain, the last
    // two parameters (lower and upper bound) have to be TOP, if no
    // restriction should be performed. Since AbsState is not an abstract
    // domain in the classical sense (i.e., no operators are defined on
    // regions), TOP/BOT are not defined for regions. Thus, the role of
    // TOP is played by *this. So, if you want to avoid restricting
    // upper (or lower) bound, pass *this as the corresponding
    // parameters.
    //StatePtr rwiden(_Self&, _Self&, _Self&);

    bool subsumes(const _Self&) const;
    bool subsumedBy(const _Self& a) const { return a.subsumes(*this); }

    int getHash() const { return rbt->getHash(); }
    
    bool operator==(const _Self& a) const {
		return *rbt == *a.rbt;
	}
    bool operator!=(const _Self& a) const {
		return *rbt != *a.rbt;
	}

    // *** Debug ***

    void print(std::ostream&) const;

    // Invariants:
    // - exactly one region of type reg, stack, glob
    // - each region ID is a singleton in the set
    // - can't have both a weak and a strong version of the same region
    // - the register region has to be bounded
    bool check() const;
};

template <class T>
inline std::ostream& operator<<(std::ostream& os, const AbsState<T>& s){
    s.print(os); return os;
}

// Note: This function has to be kept in sync with the Cmp<Region>
// functor.
template <class T>
inline typename boost::intrusive_ptr<Region<T> > AbsState<T>::find(int
        id) {

    typedef typename RBTree::Rbnode RbndTy;
    const RbndTy* nd = const_cast<const RBTree&>(*rbt).getRoot();

    while (nd) {
        const int ndid = nd->get()->getId();
        if (ndid == id) {
            break;
        } else {
            nd = (id < ndid) ? nd->getLeft() :
                nd->getRight();
        }
    }

    return nd ? nd->get() : RegionPtr(0);
}

#define BOOSTPTR boost::intrusive_ptr<AbsState<T> >

template <typename T>
inline BOOSTPTR AbsState<T>::getInitForMain() {
    RegionPtr g = RegionType::getFresh();
    RegionPtr r = RegionType::getFresh(rg::STRONG_REGISTER);
    RegionPtr l = RegionType::getFresh(rg::STRONG_STACK);

    // Initialize EBP and ESP to point to zero-offset stack
    r = r->write(reg::ESP_REG, std::make_pair(l, T::get(0,4,4)));
    r = r->write(reg::EBP_REG, std::make_pair(l, T::get(0,4,4)));
    // Initialize registers to zero
    r = r->write(reg::EAX_REG, std::make_pair(g, T::get(0,4,4)));
    r = r->write(reg::EBX_REG, std::make_pair(g, T::get(0,4,4)));
    r = r->write(reg::ECX_REG, std::make_pair(g, T::get(0,4,4)));
    r = r->write(reg::EDX_REG, std::make_pair(g, T::get(0,4,4)));
    r = r->write(reg::ESI_REG, std::make_pair(g, T::get(0,4,4)));
    r = r->write(reg::EDI_REG, std::make_pair(g, T::get(0,4,4)));
    // Initialize return addr, argc, argv to (global,top)
    l = l->write(T::get(0,4,4), std::make_pair(g, T::getTop()));
    l = l->write(T::get(4,8,4), std::make_pair(g, T::getTop()));
    assert(Prog::getDefaultArgvAddress() % 4 == 0 &&
            "Argv array address not properly aligned.");
    l = l->write(T::get(8,12,4), std::make_pair(g,
                T::get(Prog::getDefaultArgvAddress(), 4)));
    RBTreePtr t = RBTree::get(g);
    t = t->insert(r);
    t = t->insert(l);
    return StatePtr(new _Self(t));
}
    
template <typename T>
inline BOOSTPTR AbsState<T>::get(RegionPtr r1, RegionPtr r2, RegionPtr
        r3) {
    RBTreePtr t = RBTree::get(r1);
    t = t->insert(r2);
    t = t->insert(r3);
    return StatePtr(new _Self(t));
}

template <typename T>
inline BOOSTPTR AbsState<T>::get(RegionVecTy& v) {
    assert(!v.empty() && 
    "Attempted to initialize state with an empty vector of regions.");
    return StatePtr(new _Self(RBTree::get(v)));
}
    
template <typename T>
inline BOOSTPTR AbsState<T>::replaceStack(RegionPtr r) {
    assert(r->isStack() && "The replacement is not a stack region.");
    return StatePtr(new _Self(rbt->replace(getStack(), r)));
}

template <typename T>
inline BOOSTPTR AbsState<T>::replaceRegister(RegionPtr r) {
    assert(r->isRegister() && "The replacement is not a stack region.");
    return StatePtr(new _Self(rbt->replace(getRegister(), r)));
}

template <typename T>
inline BOOSTPTR AbsState<T>::replaceHeap(int id, RegionPtr r) {
    assert(r.get());
    assert(r->isHeap());
    assert(r->getId() == id);
    /*
    if (!find(id).get()) {
        std::cout << "State: " << *this << std::endl;
        std::cout << "Region: " << *r << std::endl;
        std::cout << "ID: " << id << std::endl;
    }// */
    assert(find(id).get());
    assert(find(id)->isHeap());
    return StatePtr(new _Self(rbt->replace(find(id), r)));
}

template <typename T>
inline BOOSTPTR AbsState<T>::removeStack() {
    RegionPtr stack = getStack();
    assert(stack.get() && 
        "State doesn't have a stack region, so it can't be removed.");
    return StatePtr(new _Self(rbt->erase(stack)));
}

template <typename T>
inline BOOSTPTR AbsState<T>::addStack(RegionPtr stack) {
    assert(getStack().get() == 0 && 
        "State has a stack region, can't add a new one.");
    assert(stack->isStack() && "Have to pass stack to addStack.");
    return StatePtr(new _Self(rbt->insert(stack)));
}

template <typename T>
inline BOOSTPTR AbsState<T>::discardFrame(int boundary) {
    RegionPtr stack = getStack();
    assert(stack.get() && 
        "State doesn't have a stack region, so it can't be removed.");
    return replaceStack(stack->discardFrame(boundary));
}
    
template <typename T>
inline BOOSTPTR AbsState<T>::addNewRegion(rg::RegionTy ty) {
    return StatePtr(new _Self(rbt->insert(RegionType::getFresh(ty))));
}

template <typename T>
inline BOOSTPTR AbsState<T>::addNewRegion(rg::RegionTy ty, TPtr size) {
    return StatePtr(new _Self(rbt->insert(RegionType::getFresh(ty,
                        size))));
}

template <typename T>
inline BOOSTPTR AbsState<T>::getWeaklyUpdatable() {
    RBTreePtr t;
    for (const_iterator I = rbt->begin(), E = rbt->end(); I !=E; ++I){
        t->insert(I->getWeaklyUpdatable());
    }
    return StatePtr(new _Self(t));
}

template <typename T>
inline BOOSTPTR AbsState<T>::getStronglyUpdatable() {
    RBTreePtr t;
    for (const_iterator I = rbt->begin(), E = rbt->end(); I != E; ++I) {
        t->insert(I->getStronglyUpdatable());
    }
    return StatePtr(new _Self(t));
}

template <typename T>
inline boost::intrusive_ptr<ValueSet<T> > AbsState<T>::read(AlocPair p){
    assert(knownId(p.first) && "Reading a non existing region?");
    RegionPtr regionToRead =  p.first;
    TPtr      addressToRead = p.second;
    RegionPtr regionToReadCurrVersion = find(regionToRead->getId());
    assert(regionToReadCurrVersion.get() && 
        "Couldn't find region with the same type-id.");
    assert(RegionType::weak2strong(regionToReadCurrVersion->getRegionType())
            == RegionType::weak2strong(regionToRead->getRegionType()) &&
            "Region type mismatch.");
    return regionToReadCurrVersion->read(addressToRead);
}

template <typename T>
inline BOOSTPTR AbsState<T>::write(AlocPair p, VSetPtr v) {
    RegionPtr writeToRegion  = p.first;
    TPtr      writeToAddress = p.second;
    RegionPtr writeToRegionCurrVersion =
        find(writeToRegion->getId());
    assert(writeToRegionCurrVersion.get() && 
        "Couldn't find region with the same type-id.");
    assert(RegionType::weak2strong(writeToRegionCurrVersion->getRegionType())
            == RegionType::weak2strong(writeToRegion->getRegionType())
            && "Region type mismatch.");
    RegionPtr newVersion = writeToRegionCurrVersion->write(
            writeToAddress, v);
    return StatePtr(new _Self(rbt->replace(writeToRegionCurrVersion,
                    newVersion)));
}

template <typename T>
inline boost::intrusive_ptr<ValueSet<T> > AbsState<T>::read(VSetPtr
        adr) {
    VSetPtr v = ValSetTy::getTop();
    for (typename ValSetTy::const_iterator I = adr->begin(), E =
            adr->end(); I !=E; ++I){
        if (I == adr->begin()) {
            v = read(I->get());
        } else {
            v = v->join(*read(I->get()));
        }
    }
    return v;
}

template <typename T>
inline BOOSTPTR AbsState<T>::write(VSetPtr adr, VSetPtr val) {
    if (adr->isTop()) {
        WARNING("*** Write out of bounds");
    }
    StatePtr res = StatePtr(this);
    for (typename ValSetTy::const_iterator I = adr->begin(), E =
            adr->end(); I != E; ++I){
        res = write(I->get(), val);
    }
    return res;
}
    
template <typename T>
inline BOOSTPTR AbsState<T>::join(_Self& s) {
    const_iterator 
        TI = const_cast<const RBTree&>(*rbt).begin(), 
        TE = const_cast<const RBTree&>(*rbt).end(), 
        SI = const_cast<const RBTree&>(*s.rbt).begin(), 
        SE = const_cast<const RBTree&>(*s.rbt).end();

    RegionVecTy tmp;
    utils::Cmp<RegionPtr> cmp;

    for (; TI != TE && SI != SE; ) {
        RegionPtr tr = TI->get();
        RegionPtr sr = SI->get();

        const int c = cmp(tr, sr);

        if (c == 0) {
            tmp.push_back(tr->join(*sr));
            ++TI; ++SI;
        } else if (c < 0) {
            tmp.push_back(tr);
            ++TI;
        } else { // c > 0
            tmp.push_back(sr);
            ++SI;
        }
    }

    for (; TI != TE; ++TI) {
        tmp.push_back(TI->get());
    }

    for (; SI != SE; ++SI) {
        tmp.push_back(SI->get());
    }

    StatePtr res = get(tmp);
    //assert(res->subsumes(s) && "Incorrect join.");
    /*
    if (!res->subsumes(*this)) {
        std::cout << "FAILED JOIN: " << std::endl;
        std::cout << *this;
        std::cout << "---------- Joined with -------------" <<
            std::endl;
        std::cout << *s;
        std::cout << "---------- Result ------------" << std::endl;
        std::cout << *res;
    }// */
    //assert(res->subsumes(*this) && "Incorrect join.");
#ifndef NDEBUG
    std::stringstream ss;
    ss << *this << " join " << s << " = " << *res << std::endl;
    debug3("\t\t[JOIN]\n%s", ss.str().c_str());
#endif
    return res;
}

template <typename T>
inline BOOSTPTR AbsState<T>::meet(_Self& s) {
    const_iterator 
        TI = const_cast<const RBTree&>(*rbt).begin(), 
        TE = const_cast<const RBTree&>(*rbt).end(), 
        SI = const_cast<const RBTree&>(*s.rbt).begin(), 
        SE = const_cast<const RBTree&>(*s.rbt).end();

    RegionVecTy tmp;
    utils::Cmp<RegionPtr> cmp;

    for (; TI != TE && SI != SE; ) {
        RegionPtr tr = TI->get();
        RegionPtr sr = SI->get();

        const int c = cmp(tr, sr);

        if (c == 0) {
            tmp.push_back(tr->meet(*sr));
            ++TI; ++SI;
        } else if (c < 0) {
            ++TI;
        } else { // c > 0
            ++SI;
        }
    }
    
    StatePtr res = get(tmp);
#ifndef NDEBUG
    std::stringstream ss;
    ss << *this << " meet " << *s << " = " << *res << std::endl;
    debug3("\t\t[MEET]\n%s", ss.str().c_str());
#endif
    return res;
}

template <typename T>
inline BOOSTPTR AbsState<T>::widen(_Self& with) {
    if (with == *this) { return StatePtr(this); }
   
    const_iterator 
        TI = const_cast<const RBTree&>(*rbt).begin(), 
        TE = const_cast<const RBTree&>(*rbt).end(), 
        WI = const_cast<const RBTree&>(*with.rbt).begin(), 
        WE = const_cast<const RBTree&>(*with.rbt).end();

    RegionVecTy tmp;
    utils::Cmp<RegionPtr> cmp;

    for (; TI != TE && WI != WE; ) {
        RegionPtr tr = TI->get();
        RegionPtr wr = WI->get();

        const int c = cmp(tr, wr);

        if (c == 0) {
            tmp.push_back(tr->widen(*wr));
            ++TI; ++WI;
        } else if (c < 0) {
            tmp.push_back(tr);
            ++TI;
        } else { // c > 0
            // Strictly speaking, this is incorrect, as widening bottom
            // with anything should give top. However, this
            // incorrectness will, in the worst case, cause additional
            // iterations during widening.
            tmp.push_back(wr);
            ++WI;
        }
    }

    for (; TI != TE; ++TI) {
        tmp.push_back(TI->get());
    }

    for (; WI != WE; ++WI) {
        tmp.push_back(WI->get());
    }

    StatePtr res = get(tmp);
    //assert(res->subsumes(with) && "Incorrect widening.");
    //assert(res->subsumes(*this) && "Incorrect widnening.");
#ifndef NDEBUG
    std::stringstream ss;
    ss << *this << " widen " << with << " = " << *res << std::endl;
    debug3("\t\t[WIDEN]\n%s", ss.str().c_str());
#endif
    return res;

    //return rwiden(with, *this, *this);
}

/*
template <typename T>
inline BOOSTPTR AbsState<T>::rwiden(_Self& with, _Self& lower, _Self&
        upper) {

    const_iterator 
        TI = const_cast<const RBTree&>(*rbt).begin(), 
        TE = const_cast<const RBTree&>(*rbt).end(), 
        WI = const_cast<const RBTree&>(*with.rbt).begin(), 
        WE = const_cast<const RBTree&>(*with.rbt).end(),
        LI = (this == &lower) ? lower.end() : lower.begin(), 
        LE = lower.end(),
        UI = (this == &upper) ? upper.end() : upper.begin(),
        UE = upper.end();

    RegionVecTy tmp;
    utils::Cmp<RegionPtr> cmp;

    for (; TI != TE && WI != WE; ) {
        RegionPtr tr = TI->get();
        RegionPtr wr = WI->get();

        const int c = cmp(tr, wr);

        if (c == 0) {
            // Skip over lower bounds useless for restriction
            while (LI != LE && LI->get()->getId() < tr->getId()) {++LI;}
            // Skip over upper bounds useless for restriction
            while (UI != UE && UI->get()->getId() < tr->getId()) {++UI;}

            RegionType& lb = (LI != LE && LI->get()->getId() ==
                    tr->getId()) ? *LI->get() : *tr;
            RegionType& ub = (UI != UE && UI->get()->getId() ==
                    tr->getId()) ? *UI->get() : *tr;

            tmp.push_back(tr->rwiden(*wr, lb, ub));
            ++TI; ++WI;
        } else if (c < 0) {
            // Widening with bottom gives bottom
            ++TI;
        } else { // c > 0
            // Strictly speaking, this is incorrect, as widening bottom
            // with anything should give top. However, this
            // incorrectness will, in the worst case, cause additional
            // iterations during widening.
            tmp.push_back(tr);
            ++WI;
        }
    }
    
    StatePtr res = get(tmp);
    //assert(res->subsumes(with) && "Incorrect widening.");
    //assert(res->subsumes(*this) && "Incorrect widnening.");
#ifndef NDEBUG
    std::stringstream ss;
    ss << *this << " widen " << with << " = " << *res << std::endl;
    debug3("\t\t[WIDEN]\n%s", ss.str().c_str());
#endif
    return res;
}// */

#undef BOOSTPTR

template <typename T>
inline bool AbsState<T>::subsumes(const _Self& s) const {
    if (*this == s) { return true; }

    const_iterator TI = begin(), TE = end(), SI = s.begin(), SE =
        s.end();
    for (; TI != TE && SI != SE;) {
        RegionPtr tr = TI->get();
        RegionPtr sr = SI->get();
        if (tr->getId() == sr->getId()) {
            if (!tr->subsumes(*sr)) {
                return false;
            }
            ++TI; ++SI;
        } else if (tr->getId() < sr->getId()) {
            ++TI;
        } else { // tr->getId() > sr->getId()
            return false;
        }
    }

    return SI == SE;
}

template <typename T>
inline void AbsState<T>::print(std::ostream& os) const {
    for (const_iterator I = 
            const_cast<const RBTree&>(*rbt).begin(), 
            E = const_cast<const RBTree&>(*rbt).end(); I != E; ++I) {
        const Region<T>& r = *I->get();
        os << r << std::endl;
    }
}

// Invariants:
// - exactly one region of type reg, stack, glob
// - each region ID is a singleton in the set
// - can't have both a weak and a strong version of the same region
// - the register region has to be bounded
template <typename T>
inline bool AbsState<T>::check() const {
    std::set<int> idSet;
    bool seen[rg::TERMINAL];
    for (unsigned i = 0; i < (unsigned)rg::TERMINAL; i++) {
        seen[i] = false;
    }

    for (const_iterator I = rbt->begin(), E = rbt->end(); I != E; ++I) {
        RegionPtr r = *I;
        r->check();
        const int id = r->getId();
        if (idSet.find(id) != idSet.end()) {
            return false;
        }
        idSet.insert(id);
        const rg::RegionTy ty = r->getRegionType();
        if (ty != rg::STRONG_HEAP && ty != rg::WEAK_HEAP) {
            if (seen[ty]) {
                return false;
            }
            seen[ty] = true;
        }
        if (ty == rg::WEAK_STACK || ty == rg::STRONG_STACK) {
            seen[rg::STRONG_STACK] = true;
        }
        if (ty == rg::WEAK_REGISTER || ty == rg::STRONG_REGISTER) {
            if (*getIntervalFor(reg::TERM_REG) != r->getSize()) {
                return false;
            }
        }
    }

    if (    (!seen[rg::WEAK_REGISTER] && !seen[rg::STRONG_REGISTER]) ||
            (!seen[rg::WEAK_STACK] && !seen[rg::STRONG_STACK]) ||
            (!seen[rg::WEAK_GLOBAL] && !seen[rg::STRONG_GLOBAL])) {
        return false;
    }

    return true;
}

} // End of the absdomain namespace

#endif // ABSTRACT_STATE_H

// Local Variables: 
// mode: c++
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
