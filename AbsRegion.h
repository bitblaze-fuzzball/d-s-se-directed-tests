// TODO:
// 1) If we had an Interval base-class of StridedInterval, that would be
// a better abstraction of the size field in Region.
// 2) Optimize operations on ValueSet so as to use more incrementality
// for small changes
// 3) Misaligned writes could be handled more precisely if the bounds
// of the written values were analyzed carefully.
// 4) Writing regions indirectly through ValueSet write function is a
// bit too heavy-weight, optimize that.
// 5) add collect functionality to collect equivalent chunks remaining
// after splitting.
// 6) implement restrict in Region
// 7) finish more precise implementation of misaligned writes
// 8) handle misaligned joins/widen more precisely

#ifndef ABSTRACT_REGION_H
#define ABSTRACT_REGION_H

#include "ValSet.h"
#include "Utilities.h"
#include "HashFunctions.h"
#include "MemMap.h"
#include "Registers.h"
#include "AbsDomStridedInterval.h"

#include "warning.h"

namespace absdomain {

// Info: The T type has to satisfy all requirements described in
// RedBlackTree.h, and it has to inherit AbstractDomain.

template <typename T>
class Region : public utils::pt::Counted {
public:
    typedef Region<T> _Self;
    typedef typename boost::intrusive_ptr<_Self> RegionPtr;
    typedef boost::intrusive_ptr<T> TPtr;
    typedef std::pair<RegionPtr, TPtr> AlocPair;
    typedef typename ValueSet<T>::VSetPtr VSetPtr;
    typedef std::pair<TPtr, VSetPtr> ContentPair;
    typedef typename std::vector<ContentPair> ContentVector;
    typedef typename ContentVector::const_iterator content_iterator;
    typedef typename utils::RedBlackTree<ContentPair> RBTree;
    typedef typename RBTree::RedBlackTreePtr RBTreePtr;
    typedef typename std::vector<std::pair<TPtr,splt::SplitTy> >
        PairVector;
    typedef typename PairVector::const_iterator split_iterator;

private:
    typedef memmap::MemMap MemMap;

    static THREADLOCAL int counter;
    static THREADLOCAL const MemMap* mmap;
    
    RBTreePtr rbt;
    StridedIntervalPtr size;
    const int id                : 28;
    const rg::RegionTy regty    :  4;

    Region(rg::RegionTy ty, int i, StridedIntervalPtr size) :
        rbt(RBTree::get()), size(size), id(i), regty(ty) {
            assert(check() && "Invalid region constructed.");
            assert(i < (1 << 28) && "Id out of bounds.");
            assert(rbt.get() && "Empty RB tree?");
        }
    
    Region(const Region& r) : rbt(r.rbt), size(r.size), id(r.id),
    regty(r.regty) {
        assert(check() && "Invalid region constructed.");
        assert(rbt.get() && "Empty RB tree?");
    }
    
    Region(const Region& r, rg::RegionTy ty) : rbt(r.rbt), size(r.size),
    id(r.id), regty(ty) {
        assert(check() && "Invalid region constructed.");
        assert(rbt.get() && "Empty RB tree?");
    }
    
    Region(const Region& r, RBTreePtr p) : rbt(p), size(r.size),
    id(r.id), regty(r.regty) {
        assert(check() && "Invalid region constructed.");
        assert(rbt.get() && "Empty RB tree?");
    }

    void split(_Self&, ContentVector&, ContentVector&,
            std::vector<TPtr>&, std::vector<TPtr>&, PairVector&);

    static bool weaklyUpdatable(rg::RegionTy);
    static bool stronglyUpdatable(rg::RegionTy);

    struct LessOrEqual {
        bool operator()(const ContentPair& a, const ContentPair& b)
            const;
    };

    static TPtr getIntervalFor(reg::RegEnumTy ty) {
        assert(ty < reg::TERM_REG && "Register index out of bounds.");
        const struct regs* r = &regs[(unsigned)ty];
        return T::get(r->begin, r->end, r->size);
    }

public:
    typedef typename RBTree::const_iterator const_iterator;

    static RegionPtr getFresh(rg::RegionTy ty = rg::STRONG_GLOBAL,
            StridedIntervalPtr size = StridedInterval::getTop());

    static int getNextId() {
      return counter;
    }
    
    static rg::RegionTy weak2strong(rg::RegionTy);
    static rg::RegionTy strong2weak(rg::RegionTy);

    // Set memory map containing values initialized by the static
    // initializers.
    static void setMemMap(const MemMap* m) { mmap = m; }

    RegionPtr getWeaklyUpdatable() {
        // Register region is always strongly updatable
        return isStronglyUpdatable() ?  RegionPtr(new
        Region(*this, strong2weak(getRegionType()))) : RegionPtr(this);
    }

    RegionPtr getStronglyUpdatable() {
        // Register region is always strongly updatable
        return isWeaklyUpdatable() ? RegionPtr(new
        Region(*this, weak2strong(getRegionType()))) : RegionPtr(this);
    }
    
    static void shutdown() { RBTree::shutdown(); }
    
    const_iterator begin() const {
        return const_cast<const RBTree&>(*rbt).begin(); 
    }
    const_iterator end() const {
        return const_cast<const RBTree&>(*rbt).end(); 
    }

    bool isStronglyUpdatable() const { return stronglyUpdatable(regty);}
    bool isWeaklyUpdatable() const { return weaklyUpdatable(regty); }
    rg::RegionTy getRegionType() const { return regty; }
    int getId() const { return id; }
    bool subsumes(const _Self& a) const;
    bool subsumedBy(const _Self& a) const { return a.subsumes(*this); }
    bool withinBoundsOf(const _Self&) const;
    
    bool isGlobal() const { 
        return regty == rg::WEAK_GLOBAL || regty == rg::STRONG_GLOBAL;
    }
    bool isRegister() const { 
        return regty == rg::WEAK_REGISTER || regty ==
            rg::STRONG_REGISTER; 
    }
    bool isStack() const { 
        return regty == rg::STRONG_STACK || regty == rg::WEAK_STACK; 
    }
    bool isHeap() const { 
        return regty == rg::STRONG_HEAP || regty == rg::WEAK_HEAP;
    }

    VSetPtr read(TPtr) const;
    VSetPtr read(reg::RegEnumTy ty) {
        assert(isRegister() && 
            "Can't use this function for non-register regions.");
        return read(getIntervalFor(ty));
    }
    RegionPtr write(TPtr, VSetPtr);
    RegionPtr write(TPtr, AlocPair);
    RegionPtr write(reg::RegEnumTy ty, VSetPtr p) {
        assert(rbt.get() && "About to dereference a NULL ptr.");
        assert(isRegister() && 
            "Can't use this function for non-register regions.");
        return write(getIntervalFor(ty), p);
    }
    RegionPtr write(reg::RegEnumTy, AlocPair);

    RegionPtr join(_Self&);
    RegionPtr meet(_Self&);
    RegionPtr widen(_Self&);
    // Warning: In the classes that inherit abstract domain, the last
    // two parameters (lower and upper bound) have to be TOP, if no
    // restriction should be performed. Since Region is not an abstract
    // domain in the classical sense (i.e., no operators are defined on
    // regions), TOP/BOT are not defined for regions. Thus, the role of
    // TOP is played by *this. So, if you want to avoid restricting
    // upper (or lower) bound, pass *this as the corresponding
    // parameters.
    //RegionPtr rwiden(_Self&, _Self&, _Self&);

    // Adds n to all addresses, effectively shifting the address space.
    RegionPtr discardFrame(int);

    int getHash() const;
    // Number of distinctive addresses
    int getSize() const { return rbt->treeSize(); }
    // Actual range of valid sizes of the memory region
    StridedIntervalPtr getSizeRange() const { return size; }
    void print(std::ostream&) const;

    // Implemented as friends for symmetry
    template <typename T2>
    friend bool operator==(const Region<T2>&, const Region<T2>&);
    template <typename T2>
    friend bool operator!=(const Region<T2>&, const Region<T2>&);

    // *** Debug ***
    bool check() const;
    static bool checkWrite(TPtr, AlocPair);
    static bool checkWrite(TPtr, VSetPtr);
};

template <class T>
inline std::ostream& operator<<(std::ostream& os, const Region<T>& r) {
    r.print(os); return os;
}

// *** Implementation ***

template <typename T>
inline rg::RegionTy Region<T>::weak2strong(rg::RegionTy ty) {
    switch (ty) {
        case rg::WEAK_HEAP:     return rg::STRONG_HEAP;
        case rg::WEAK_STACK:    return rg::STRONG_STACK;
        case rg::WEAK_REGISTER: return rg::STRONG_REGISTER;
        case rg::WEAK_GLOBAL:   return rg::STRONG_GLOBAL;
        default: return ty;
    }
}

template <typename T>
inline rg::RegionTy Region<T>::strong2weak(rg::RegionTy ty) {
    switch (ty) {
        case rg::STRONG_HEAP:       return rg::WEAK_HEAP;
        case rg::STRONG_STACK:      return rg::WEAK_STACK;
        case rg::STRONG_REGISTER:   return rg::WEAK_REGISTER;
        case rg::STRONG_GLOBAL:     return rg::WEAK_GLOBAL;
        default: return ty;
    }
}

template <typename T>
inline bool Region<T>::weaklyUpdatable(rg::RegionTy ty) {
    return ty < rg::STRONG_GLOBAL;
}

template <typename T>
inline bool Region<T>::stronglyUpdatable(rg::RegionTy ty) {
    return ty >= rg::STRONG_GLOBAL && ty < rg::TERMINAL;
}

template <typename T>
inline bool Region<T>::checkWrite(TPtr p, AlocPair ap) {
    return true;
}

template <typename T>
inline bool Region<T>::checkWrite(TPtr p, VSetPtr vs) {
    for (typename ValueSet<T>::const_iterator I = vs->begin(), E =
            vs->end(); I != E; ++I) {
        if (!checkWrite(p, I->get())) {
            return false;
        }
    }
    return true;
}

template <typename T>
inline typename Region<T>::RegionPtr Region<T>::getFresh(rg::RegionTy
        ty, StridedIntervalPtr size) {

    int i = 0;
    switch (ty) {
        case rg::WEAK_GLOBAL:
        case rg::STRONG_GLOBAL:
            // Do nothing, zero is the right id
            break;
        case rg::WEAK_REGISTER:
        case rg::STRONG_REGISTER:
            i = 1;
            break;
        case rg::WEAK_STACK:
        case rg::STRONG_STACK:
            i = 2;
            break;
        default:
            i = counter++;
    }
    return RegionPtr(new Region(ty, i, size));
}

template <typename T>
inline bool Region<T>::subsumes(const _Self& r) const {
    assert(getId() == r.getId() && 
            "Different regions can never subsume each other.");

    const_iterator TI = begin(), TE = end(), RI = r.begin(), RE =
        r.end();

    if (RI == RE || *this == r) {
        return true;
    } else if (TI == TE) {
        return false;
    }

    for (; TI != TE && RI != RE; ) {
        ContentPair tcp = TI->get();
        ContentPair rcp = RI->get();
        TPtr tp = tcp.first;
        TPtr rp = rcp.first;

        //std::cout << *tp << " <> " << *rp << ' ';

        if (*tp < *rp) {
            // Entire tp missing in the other region
            ++TI;
            //std::cout << "++TI ";
            continue;
        } else if (*rp < *tp) {
            // Entire rp is missing in this region
            //std::cout << "false\n";
            return false;
        } else if (utils::Equal<ContentPair>()(tcp, rcp)) {
            //std::cout << "++TI, ++RI ";
            ++TI; ++RI;
            continue;
        } else if (tp->subsumes(*rp)) {
            //std::cout << "s++RI ";
            ++RI;
            if (tp->getHi() == rp->getHi()) {
                // End reached simultaneously
                ++TI;
            } else if (RI != RE && !tp->overlaps(*RI->get().first)) {
                // tp is subsuming rp, and the next interval after rp is
                // not overlapping with tp, thus, we can advance the
                // iterator
                ++TI;
                //std::cout << "sn++TI ";
            }
        } else if (tp->getLo() == rp->getLo() && tp->isConstant() &&
                rp->isConstant()) {
            ++TI; ++RI;
        } else {
            assert(tp->overlaps(*rp) && "Unexpected condition.");
            return false;
        }

        //std::cout << '|';
        if (!tcp.second->subsumes(*rcp.second)) {
            //std::cout << "THIS: " << *tcp.second << ", THAT: " <<
            //    *rcp.second << std::endl;
            //std::cout << "false!\n";
            return false;
        }
        //std::cout << "true\n";
    }

    return RI == RE;
}

// FIX
template <typename T>
inline bool Region<T>::withinBoundsOf(const _Self& r) const {
    assert(false && "Not fully implemented, doesn't take chunks "
            "into account.");
    const_iterator TI = begin(), TE = end(), RI = r.begin(), RE =
        r.end();
    
    for (; TI != TE && RI != RE; ) {
        ContentPair tcp = TI->get();
        ContentPair rcp = RI->get();
        TPtr tp = tcp.first;
        TPtr rp = rcp.first;

        if (tp->withinBoundsOf(*rp)) {
            if (!tcp.second->withinBoundsOf(*rcp.second)) {
                return false;
            }
            ++TI; ++RI;
        } else if (*rp < *tp) {
            ++RI;
        } else {
            return false;
        }
    }
    return TI == TE;
}

template <typename T>
inline typename Region<T>::VSetPtr Region<T>::read(TPtr p) const {
    
    assert(p->getStride() > 0 && "Addresses can't have zero strides.");

    if (!size->subsumes(*p)) {
      WARNING("*** Read out of bounds.");
        return ValueSet<T>::getBot();
    } else if (p->isBot()) { // There's nothing at the bottom
        return ValueSet<T>::getBot();
    }

    if (p->containsZero() && !isRegister() && !isStack()) {
      WARNING("*** Possible NULL ptr dereference (read).");
        return ValueSet<T>::getTop();
    }

    {
        ContentPair cp = rbt->match(std::make_pair(p,
                    ValueSet<T>::getBot()));
        if (cp.first.get()) {
            return cp.second;
        }
    }

    std::vector<ContentPair> overlapping;
    ContentPair cp(p, VSetPtr(0));
    rbt->findAllOverlapping(cp, overlapping);

    if (overlapping.empty()) {
        if (isGlobal() && mmap) {
            int val = 0;
            int shift = 0;
            for (int adr = p->getLo(); adr < p->getHi(); adr++) {
                if (mmap->isValid((unsigned)adr)) {
                    val |= mmap->get((unsigned)adr) << shift;
                    shift += CHAR_BIT;
                } else {
                    shift = -1;
                    break;
                }
            }
            if (shift > 0) {
                return ValueSet<T>::get(val,sizeof(int));
            }
        }

        // Complaining about uninitialized reads doesn't make much sense
        // for registers.
        if (!isRegister()) {
            bool valid = false;
            if (mmap) {
                // Check whether we are pointing to an array of strings
                // to which argv pointers point. The strings can't be
                // initialized, as that would concretize symbolic
                // analysis.
                const size_t lower =
                    mmap->getLowerBound((unsigned)p->getLo());
                const size_t upper =
                    NARGV_PTRS_DEFAULT * 4  +
                    NARGV_PTRS_DEFAULT * NARGV_STRLEN_DEFAULT;

                if (p->getLo() >= (int)lower && p->getHi() <=
                        (int)upper) {
                    // This address is initialized, but with TOP, don't
                    // complain
                    valid = true;
                }
            }

            if (!valid) {
	      WARNING("*** Read of uninitialized address.");
                /*
                static int cnt = 0;
                if (++cnt == 1) {
                    exit(1);
                }// */
            }
        }
        return ValueSet<T>::getTop();
    }

    std::sort(overlapping.begin(), overlapping.end(), LessOrEqual());

    TPtr t;
    VSetPtr vs;
    int shiftFor = 0;
    for (typename std::vector<ContentPair>::const_iterator I =
            overlapping.begin(), E = overlapping.end(); I != E; ++I) {
        TPtr i = I->first;
        VSetPtr c = I->second;

        // Note: this is a realtively simple case, when we read a
        // smaller chunk from a larger interval. It actually happens in
        // practice. 
        if (i->subsumes(*p)) {
            assert(overlapping.size() == 1 &&
                "There should be only one subsuming interval.");

            if (i->getStride() == p->getStride()) {
                // Read of one element from an array
                return c;
            }
            const int shift = i->getHi() - p->getHi();
            assert(shift >= 0 && "Shift shouldn't be negative.");
            const int bytes = p->getStride();
            const int mask = (1 << (bytes * CHAR_BIT)) - 1;
            VSetPtr shft = ValueSet<T>::get(shift,
                    utils::umax(i->getStride(), p->getStride()));
            vs = *c->rshift(*shft) & *ValueSet<T>::get(mask,
                    p->getStride());
        } else if (p->subsumes(*i)) {
            if (i->getStride() == p->getStride()) {
                // Read of an interval spanning multiple strides, and
                // the interval overlaps a smaller interval with the
                // same stride. Not much we can do here...
	      WARNING("*** Misaligned read discovered (middle).");
                return ValueSet<T>::getTop();
            } else if (!vs.get()) { // First
                t = i;
                vs = I->second;
                shiftFor += i->getStride();
            } else {
                assert(t.get() && "Unexpected NULL ptr.");
                if (i->getLo() != t->getHi()) {
                    // There's an uncovered gap.
		  WARNING("*** Misaligned read discovered (gap).");
                    // Misaligned read, partially unintialized
                    return ValueSet<T>::getTop();
                } else {
                    t = i;
                    // Little endian
                    vs = *vs->lshift(*ValueSet<T>::get(shiftFor *
                                CHAR_BIT)) | *vs;
                    shiftFor += i->getStride();
                }
            }
        } else {
            // It would be possible to construct a value for a read that
            // overlaps to adjacent intervals, but that's more work...
	  WARNING("*** Misaligned read discovered (partial)", current_instruction);
            return ValueSet<T>::getTop();
        }

    }

    assert(vs.get() && "Undefined value set?");
    return vs;
}

template <typename T>
inline typename Region<T>::RegionPtr Region<T>::write(TPtr p,VSetPtr s){
    assert(checkWrite(p, s) && "Invalid write.");

    assert(p->getStride() > 0 && "Addresses can't have zero strides.");

    if (!size->subsumes(*p) || p->isTop()) {
        WARNING("*** Write out of bounds");
        return RegionPtr(this);
    } else if (isStack() && p->getLo() > 0) {
        WARNING("*** Write out of bounds");
    } else if (p->isBot()) { // Ignore writes to bot
        return RegionPtr(this);
    }

    if (p->containsZero() && !isRegister() && !isStack()) {
      WARNING("*** Possible NULL ptr dereference (write).");
    }

    ContentVector overlapping;
    ContentPair cp(p,s);
    rbt->findAllOverlapping(cp, overlapping);

    if (overlapping.empty()) { // Fresh location written
        return RegionPtr(new Region(*this, rbt->insert(cp)));
    }
    
    RBTreePtr mp(rbt);

    if (overlapping.size() == 1) {
        TPtr i = overlapping[0].first;
        VSetPtr vs = overlapping[0].second;

        if (isStronglyUpdatable()) {
            if (*i == *p) {
                cp.second = s;
                mp = mp->replace(overlapping[0], cp);
            } else if (p->getStride() == i->getStride()) {
                // Aligned, but split is needed, bail out
                goto handle_misalignment;
            } else if (i->subsumes(*p)) {
                const int shift = i->getHi() - p->getHi();
                const int bytes = p->getStride();
                int mask = (1 << (bytes * CHAR_BIT)) - 1;
                assert(shift >= 0 && "Invalid shift.");

                // Construct shift and masks
                VSetPtr shft = ValueSet<T>::get(bytes, i->getStride());
                VSetPtr msk  = ValueSet<T>::get(mask, i->getStride());
                // Align written value and shift it back
                VSetPtr ap =  (*s->rshift(*shft) & *msk)->lshift(*shft);
                // Construct a window
                mask <<= bytes;
                VSetPtr w = ValueSet<T>::get(~mask, i->getStride());
                // Mask overwritten value
                VSetPtr overw = *vs & *w;
                // Fill in the window
                cp.second = *overw | *ap;
                // Erase the old, and insert the new val set
                mp = mp->erase(overlapping[0]);
                mp = mp->insert(cp);
            } else if (p->subsumes(*i)) {
                cp.second = s;
                mp = mp->erase(overlapping[0]);
                mp = mp->insert(cp);
            } else {
                goto handle_misalignment;
            }
        } else {
            if (*i == *p) {
                cp.second = vs->join(*s);
                mp = mp->replace(overlapping[0], cp);
            } else if (p->getStride() == i->getStride()) {
                // Aligned, but split is needed, bail out
                goto handle_misalignment;
            } else if (i->subsumes(*p)) {
                const int shift = i->getHi() - p->getHi();
                const int bytes = p->getStride();
                int mask = (1 << (bytes * CHAR_BIT)) - 1;
                assert(shift >= 0 && "Invalid shift.");

                // Construct a mask
                VSetPtr shft = ValueSet<T>::get(bytes, i->getStride());
                VSetPtr msk  = ValueSet<T>::get(mask, i->getStride());
                // Align both written and overwritten value
                VSetPtr ai = *vs->rshift(*shft) & *msk;
                VSetPtr ap =  *s->rshift(*shft) & *msk;
                // Join them together and shift it back
                VSetPtr jn = ai->join(*ap)->lshift(*shft);
                // Construct a window
                mask <<= bytes;
                VSetPtr w = ValueSet<T>::get(~mask, i->getStride());
                // Mask overwritten value
                VSetPtr overw = *vs & *w;
                // Fill in the window
                cp.second = *overw | *jn;
                // Erase the old, and insert the new val set
                mp = mp->erase(overlapping[0]);
                mp = mp->insert(cp);
            } else if (p->subsumes(*i)) {
                const int shift = p->getHi() - i->getHi();
                const int bytes = p->getStride();
                int mask = (1 << (bytes * CHAR_BIT)) - 1;
                assert(shift >= 0 && "Invalid shift.");

                // Construct a mask and the window
                VSetPtr shft = ValueSet<T>::get(bytes, p->getStride());
                VSetPtr msk  = ValueSet<T>::get(mask, p->getStride());
                // Align both values
                VSetPtr ai = *vs->rshift(*shft) & *msk;
                VSetPtr ap =  *s->rshift(*shft) & *msk;
                // Join them together and shift it back
                VSetPtr jn = ai->join(*ap)->lshift(*shft);
                // Construct a window
                mask <<= bytes;
                VSetPtr w    = ValueSet<T>::get(~mask, p->getStride());
                // Mask written value
                VSetPtr written = *s & *w;
                // Fill in the window
                cp.second = *written | *jn;
                // Erase the old, and insert the new val set
                mp = mp->erase(overlapping[0]);
                mp = mp->insert(cp);
            } else {
                goto handle_misalignment;
            }
        }
        return RegionPtr(new Region(*this, mp));
    }

handle_misalignment:
    
    // Sort the overlapping intervals
    std::sort(overlapping.begin(), overlapping.end(), LessOrEqual());
    // Pick over the actual intervals
    std::vector<TPtr> intervals;
    PairVector out;
    std::transform(overlapping.begin(), overlapping.end(),
            std::back_inserter(intervals),
            utils::select1st<ContentPair>());
    p->split(intervals, out);

    // Erase all overlapping intervals
    for (content_iterator I = overlapping.begin(), E =
            overlapping.end(); I != E; ++I) {
        mp = mp->erase(*I);
    }

    unsigned idx = 0;
    for (split_iterator I = out.begin(), E = out.end(); I != E; ++I) {

        TPtr i = I->first;
        const int code = I->second;
        ContentPair np;
        switch (code) {
            case splt::FIRST:
                assert(idx < intervals.size() && 
                        "Index out of bounds.");
                assert(i->withinBoundsOf(*intervals[idx]) &&
                        "Invalid interval chunk.");
                if (i->getStride() == intervals[idx]->getStride()) {
                    // Aligned
                    np = std::make_pair(i, overlapping[idx].second);
                } else {
                    // Misaligned
                    np = std::make_pair(i, ValueSet<T>::getTop());
                }
                if (i->getHi() == intervals[idx]->getHi()) {
                    idx++;
                }
                break;
            case splt::SECOND:
                assert(i->withinBoundsOf(*p) &&
                    "Invalid interval chunk.");
                if (i->getStride() == p->getStride()) {
                    // Aligned
                    np = std::make_pair(i, s);
                } else {
                    // Misaligned
                    np = std::make_pair(i, ValueSet<T>::getTop());
                }
                break;
            case splt::BOTH:
                assert(idx < intervals.size() && 
                        "Index out of bounds.");
                assert(i->withinBoundsOf(*intervals[idx]) &&
                        "Invalid interval chunk.");
                assert(i->withinBoundsOf(*p) &&
                    "Invalid interval chunk.");
                if (i->getStride() == p->getStride()) {
                    // Aligned
                    if (isStronglyUpdatable()) {
                        np = std::make_pair(i, s);
                    } else {
                        np = std::make_pair(i,
                                s->join(*overlapping[idx].second));
                    }
                } else {
                    // Misaligned
                    np = std::make_pair(i, ValueSet<T>::getTop());
                }
                if (i->getHi() == intervals[idx]->getHi()) {
                    idx++;
                }
                break;
            default:
                assert(false && "Unrecognized split code.");
                break;
        }
        
        if (mp.get()) {
            mp = mp->insert(np);
        } else {
            mp = RBTree::get(np);
        }
#ifndef DEBUG
        RegionPtr r = RegionPtr(new Region(*this, mp));
#endif
    }
    return RegionPtr(new Region(*this, mp));
}

template <typename T>
inline typename Region<T>::RegionPtr Region<T>::write(TPtr p, AlocPair
        ap) {

    assert(checkWrite(p, ap) && "Invalid write.");
    return write(p, ValueSet<T>::get(ap));
}

template <typename T>
inline typename Region<T>::RegionPtr Region<T>::write(reg::RegEnumTy r,
        AlocPair ap) {
    return write(r, ValueSet<T>::get(ap));
}

template <typename T>
inline typename Region<T>::RegionPtr Region<T>::join(_Self& r) {

    assert(getId() == r.getId() && 
            "Can't join regions with different IDs.");
    if (r == *this) { return RegionPtr(this); }
    ContentVector cv1, cv2;
    std::vector<TPtr> ptvec1, ptvec2;
    PairVector out;
    split(r, cv1, cv2, ptvec1, ptvec2, out);
    RBTreePtr mp;
    
    unsigned idx1 = 0, idx2 = 0;
    for (split_iterator I = out.begin(), E = out.end(); I != E; ++I) {

        TPtr i = I->first;
        const int code = I->second;
        ContentPair np;
        switch (code) {
            case splt::FIRST:
                assert(idx1 < ptvec1.size() && "Index out of bounds.");
                if (i->getStride() == ptvec1[idx1]->getStride()) {
                    // Aligned
                    np = std::make_pair(i, cv1[idx1].second);
                } else {
                    // Misaligned
                    np = std::make_pair(i, ValueSet<T>::getTop());
                }
                if (i->getHi() == ptvec1[idx1]->getHi()) {
                    idx1++;
                }
                break;
            case splt::SECOND:
                assert(idx2 < ptvec2.size() && "Index out of bounds.");
                if (i->getStride() == ptvec2[idx2]->getStride()) {
                    // Aligned
                    np = std::make_pair(i, cv2[idx2].second);
                } else {
                    // Misaligned
                    np = std::make_pair(i, ValueSet<T>::getTop());
                }
                if (i->getHi() == ptvec2[idx2]->getHi()) {
                    idx2++;
                }
                break;
            case splt::BOTH: 
            {
                assert(idx1 < ptvec1.size() && "Index out of bounds.");
                assert(idx2 < ptvec2.size() && "Index out of bounds.");
                const bool aligned = ptvec1[idx1]->getStride() ==
                    ptvec2[idx2]->getStride() && i->getStride() ==
                    ptvec1[idx1]->getStride();
                assert(cv1[idx1].first->overlaps(*cv2[idx2].first) &&
                        "Joining non-overlapping intervals?");
                if (aligned) {
                    np = std::make_pair(i,
                            cv1[idx1].second->join(*cv2[idx2].second));
                } else {
                    np = std::make_pair(i, ValueSet<T>::getTop());
                }
                if (i->getHi() == ptvec1[idx1]->getHi()) {
                    idx1++;
                }
                if (i->getHi() == ptvec2[idx2]->getHi()) {
                    idx2++;
                }
            } break;
            default:
                assert(false && "Unrecognized split code.");
                break;
        }

        if (mp.get()) {
            typename PairVector::const_iterator J = I;
            mp = mp->insert(np);
        } else {
            mp = RBTree::get(np);
        }
#ifndef DEBUG
        RegionPtr r = RegionPtr(new Region(*this, mp));
#endif
    }

    RegionPtr res = RegionPtr(new Region(*this, mp));
    // assert3(this->withinBoundsOf(*res) && "Invalid bnds.");
    // assert3(r.withinBoundsOf(*res) && "Invalid bnds.");
    return res;
}

template <typename T>
inline typename Region<T>::RegionPtr Region<T>::meet(_Self& r) {

    assert(getId() == r.getId() && 
            "Can't meet regions with different IDs.");
    if (r == *this) { return RegionPtr(this); }

    ContentVector cv1, cv2;
    std::vector<TPtr> ptvec1, ptvec2;
    PairVector out;
    split(r, cv1, cv2, ptvec1, ptvec2, out);
    
    RBTreePtr mp;
    
    unsigned idx1 = 0, idx2 = 0;
    for (split_iterator I = out.begin(), E = out.end(); I != E; ++I) {

        TPtr i = I->first;
        const int code = I->second;
        ContentPair np;
        switch (code) {
            case splt::FIRST:
                assert(idx1 < ptvec1.size() && "Index out of bounds.");
                if (i->getHi() == ptvec1[idx1]->getHi()) {
                    idx1++;
                }
                break;
            case splt::SECOND:
                assert(idx2 < ptvec2.size() && "Index out of bounds.");
                if (i->getHi() == ptvec2[idx2]->getHi()) {
                    idx2++;
                }
                break;
            case splt::BOTH:
                assert(idx1 < ptvec1.size() && "Index out of bounds.");
                assert(idx2 < ptvec2.size() && "Index out of bounds.");
                const bool aligned = ptvec1[idx1]->getStride() ==
                    ptvec2[idx2]->getStride() && i->getStride() ==
                    ptvec1[idx1]->getStride();
                assert(cv1[idx1].first->overlaps(*cv2[idx2].first) &&
                        "Meeting non-overlapping intervals?");
                if (aligned) {
                    np = std::make_pair(i,
                            cv1[idx1].second->meet(*cv2[idx2].second));
                } else {
                    np = std::make_pair(i, ValueSet<T>::getTop());
                }
                if (i->getHi() == ptvec1[idx1]->getHi()) {
                    idx1++;
                }
                if (i->getHi() == ptvec2[idx2]->getHi()) {
                    idx2++;
                }
                break;
            default:
                assert(false && "Unrecognized split code.");
                break;
        }

        if (mp.get()) {
            typename PairVector::const_iterator J = I;
            mp = mp->insert(np);
        } else {
            mp = RBTree::get(np);
        }
#ifndef DEBUG
        RegionPtr r = RegionPtr(new Region(*this, mp));
#endif
    }

    return RegionPtr(new Region(*this, mp));
}

template <typename T>
inline void Region<T>::split(_Self& with, 
        ContentVector& cv1, ContentVector& cv2, 
        std::vector<TPtr>& ptvec1, std::vector<TPtr>& ptvec2,
        PairVector& out) {

    for (const_iterator I = begin(), E = end(); I != E; ++I) {
        cv1.push_back(I->get());
    }
    for (const_iterator I = with.begin(), E = with.end(); I != E; ++I) {
        cv2.push_back(I->get());
    }
    
    std::transform(cv1.begin(), cv1.end(), std::back_inserter(ptvec1),
            utils::select1st<ContentPair>());
    std::transform(cv2.begin(), cv2.end(), std::back_inserter(ptvec2),
            utils::select1st<ContentPair>());

    T::split(ptvec1, ptvec2, out);
}

template <typename T>
inline typename Region<T>::RegionPtr Region<T>::widen(_Self& with) {
    assert(getId() == with.getId() && 
            "Can't widen regions with different IDs.");
    if (with == *this) { return RegionPtr(this); }

    ContentVector cv1, cv2;
    std::vector<TPtr> ptvec1, ptvec2;
    PairVector out;
    split(with, cv1, cv2, ptvec1, ptvec2, out);
    RBTreePtr mp;

    unsigned idx1 = 0, idx2 = 0;
    for (split_iterator I = out.begin(), E = out.end(); I != E; ++I) {
        TPtr i = I->first;
        const int code = I->second;
        ContentPair np;
        switch (code) {
            case splt::FIRST:
                assert(idx1 < ptvec1.size() && "Index out of bounds.");
                np = std::make_pair(i, cv1[idx1].second);
                break;
            case splt::SECOND:
                assert(idx2 < ptvec2.size() && "Index out of bounds.");
                // Warning: I'm not sure whether this is widening
                // enough. Technically, it should widen the address to
                // infinity, but I'm hoping that the location that
                // stores the address itself will be widened to
                // infinity, which will take care of the address
                // widening. Anyways, if there are any issues in
                // non-termination of widening, this spot is the prime
                // suspect.
                np = std::make_pair(i, cv2[idx2].second);
                break;
            case splt::BOTH: {
                assert(idx1 < ptvec1.size() && "Index out of bounds.");
                assert(idx2 < ptvec2.size() && "Index out of bounds.");
                const bool aligned = ptvec1[idx1]->getStride() ==
                    ptvec2[idx2]->getStride() && i->getStride() ==
                    ptvec1[idx1]->getStride();
                assert(cv1[idx1].first->overlaps(*cv2[idx2].first) &&
                        "Widening non-overlapping intervals?");

                if (aligned) {
                    np = std::make_pair(i,
                            cv1[idx1].second->widen(*cv2[idx2].second));
                } else {
                    np = std::make_pair(i, ValueSet<T>::getTop());
                }
            } break;
            default:
                assert(false && "Unrecognized split code.");
                break;
        }

        if (code == splt::FIRST || code == splt::BOTH) {
            if (i->getHi() == ptvec1[idx1]->getHi()) {
                idx1++;
            }
        }
        if (code == splt::SECOND || code == splt::BOTH) {
            if (i->getHi() == ptvec2[idx2]->getHi()) {
                idx2++;
            }
        }

        if (mp.get()) {
            typename PairVector::const_iterator J = I;
            mp = mp->insert(np);
        } else {
            mp = RBTree::get(np);
        }
#ifndef DEBUG
        RegionPtr r = RegionPtr(new Region(*this, mp));
#endif
    }

    RegionPtr res = RegionPtr(new Region(*this, mp));
    // assert3(this->withinBoundsOf(*res) && "Invalid bnds.");
    // assert3(with.withinBoundsOf(*res) && "Invalid bnds.");
    return res;

    //return rwiden(with, *this, *this);
}

/*
template <typename T>
inline typename Region<T>::RegionPtr Region<T>::rwiden(_Self& with,
        _Self& lower, _Self& upper) {
    
    assert(getId() == with.getId() && 
            "Can't widen regions with different IDs.");
    
    if (with == *this) { return RegionPtr(this); }

    ContentVector cv1, cv2;
    std::vector<TPtr> ptvec1, ptvec2;
    PairVector out;
    split(with, cv1, cv2, ptvec1, ptvec2, out);
    RBTreePtr mp;

    const_iterator
        LI = (this == &lower) ? lower.end() : lower.begin(), 
        LE = lower.end(),
        UI = (this == &upper) ? upper.end() : upper.begin(), 
        UE = upper.end();

    unsigned idx1 = 0, idx2 = 0;
    for (split_iterator I = out.begin(), E = out.end(); I != E; ++I) {

        TPtr i = I->first;
        const int code = I->second;
        ContentPair np;
        switch (code) {
            case splt::FIRST:
                assert(idx1 < ptvec1.size() && "Index out of bounds.");
                np = std::make_pair(i, cv1[idx1].second);
                break;
            case splt::SECOND:
                assert(idx2 < ptvec2.size() && "Index out of bounds.");
                // Warning: I'm not sure whether this is widening
                // enough. Technically, it should widen the address to
                // infinity, but I'm hoping that the location that
                // stores the address itself will be widened to
                // infinity, which will take care of the address
                // widening. Anyways, if there are any issues in
                // non-termination of widening, this spot is the prime
                // suspect.
                np = std::make_pair(i, cv2[idx2].second);
                break;
            case splt::BOTH: {
                assert(idx1 < ptvec1.size() && "Index out of bounds.");
                assert(idx2 < ptvec2.size() && "Index out of bounds.");

                const bool aligned = ptvec1[idx1]->getStride() ==
                    ptvec2[idx2]->getStride() && i->getStride() ==
                    ptvec1[idx1]->getStride();

                assert(cv1[idx1].first->overlaps(*cv2[idx2].first) &&
                        "Widening non-overlapping intervals?");

                if (aligned) {
                    // Skip over lower bounds useless for restriction
                    while (LI != LE && LI->get().first->getLo() <=
                        i->getLo() && !LI->get().first->subsumes(*i))
                    { ++LI; }
                    // Skip over upper bounds useless for restriction
                    while (UI != UE && UI->get().first->getLo() <=
                        i->getLo() && !UI->get().first->subsumes(*i))
                    { ++UI; }

                    VSetPtr lb = (LI == LE) ?
                        ValueSet<T>::getTop() : LI->get().second;
                    VSetPtr ub = (UI == UE) ?
                        ValueSet<T>::getTop() : UI->get().second;

                    np = std::make_pair(i,
                            cv1[idx1].second->rwiden(*cv2[idx2].second,
                                *lb, *ub));
                } else {
                    np = std::make_pair(i, ValueSet<T>::getTop());
                }
            } break;
            default:
                assert(false && "Unrecognized split code.");
                break;
        }

        if (code == splt::FIRST || code == splt::BOTH) {
            if (i->getHi() == ptvec1[idx1]->getHi()) {
                idx1++;
            }
        }
        if (code == splt::SECOND || code == splt::BOTH) {
            if (i->getHi() == ptvec2[idx2]->getHi()) {
                idx2++;
            }
        }

        if (mp.get()) {
            typename PairVector::const_iterator J = I;
            mp = mp->insert(np);
        } else {
            mp = RBTree::get(np);
        }
#ifndef DEBUG
        RegionPtr r = RegionPtr(new Region(*this, mp));
#endif
    }

    RegionPtr res = RegionPtr(new Region(*this, mp));
    // assert3(this->withinBoundsOf(*res) && "Invalid bnds.");
    // assert3(with.withinBoundsOf(*res) && "Invalid bnds.");
    return res;
}// */

template <typename T>
inline typename Region<T>::RegionPtr Region<T>::discardFrame(int
        boundary) {

    assert((getRegionType() == rg::WEAK_STACK ||
            getRegionType() == rg::STRONG_STACK) &&
            "Must be a stack region type.");

    debug3("\tDiscarding frame at boundary: %d\n", boundary);
    assert(boundary <= 0 && "Invalid boundary.");

    RBTreePtr t;
    for (const_iterator I = begin(), E = end(); I != E; ++I) {
        ContentPair p = I->get();

        if (p.first->getLo() > boundary) {
            t = (t.get()) ? t->insert(p) : RBTree::get(p);
#ifndef DEBUG
            RegionPtr r = RegionPtr(new Region(*this, t));
#endif
        }
    }
    return RegionPtr(new Region(*this, t));
}
    
template <typename T>
inline int Region<T>::getHash() const {
    const int h = rbt->getHash() ^ size->getHash() ^ 
        utils::inthash(id) ^ utils::inthash(regty);
    return isStronglyUpdatable() ? h : ~h;
}

template <typename T>
inline void Region<T>::print(std::ostream& os) const {
    os << "Region_" << getId() << '_';
    switch (getRegionType()) {
        case rg::WEAK_GLOBAL:
            os << "WG";
            break;
        case rg::STRONG_GLOBAL:
            os << "SG";
            break;
        case rg::STRONG_REGISTER:
            os << "SR";
            break;
        case rg::WEAK_REGISTER:
            os << "WR";
            break;
        case rg::WEAK_STACK:
            os << "WS";
            break;
        case rg::WEAK_HEAP:
            os << "WH";
            break;
        case rg::STRONG_STACK:
            os << "SS";
            break;
        case rg::STRONG_HEAP:
            os << "SH";
            break;
        default:
            assert(false && "Unrecognized region type.");
    }
    os << ' ';

    for (const_iterator I = begin(), E = end(); I != E; ++I) {
        ContentPair p = I->get();
        TPtr ptr = p.first;
        os << '(';
        if (isRegister()) {
            const char* regName = getRegNameAtAddress(ptr->getLo(),
                    ptr->getHi());
            if (regName) {
                os << regName;
            } else {
                os << *ptr;
            }
        } else {
            os << *ptr;
        }
        os << ':' << *p.second << ')';
        const_iterator J = I;
        ++J;
        if (J != E) {
            os << " ";
        }
    }
}
    
template <typename T>
inline bool Region<T>::check() const  {
    // Check that all addresses are non-overlapping and have a non-empty
    // definition
    for (const_iterator I = begin(), E = end(); I != E;) {
        const_iterator J = I;
        ++J;
        ContentPair p = I->get();
        if (J != E) {
            ContentPair nxt = J->get();
            // No overlapping address intervals
            if (p.first->overlaps(*nxt.first)) {
                return false;
            }
        }
        // Addresses must have Stride >= 1
        if (p.first->getStride() < 1) {
            return false;
        }
        I = J;
    }

    return true;
}

template <typename T>
inline bool operator==(const Region<T>& a, const Region<T>& b) {
    return a.id == b.id && 
        *a.getSizeRange() == *b.getSizeRange() &&
        *a.rbt == *b.rbt;
}

template <typename T>
inline bool operator!=(const Region<T>& a, const Region<T>& b) {
    return a.id != b.id || 
        *a.getSizeRange() != *b.getSizeRange() ||
        *a.rbt != *b.rbt;
}


} // End of the absdomain namespace

// Specializations required by the RedBlackTree
namespace utils {

#define REGION typename \
    boost::intrusive_ptr<absdomain::Region<T> >

// Note: This comparator has to be kept in sync with the find(RegionTy,
// Id) function in AbsState.
template <typename T>
struct Cmp<REGION > {
    int operator()(REGION a, REGION b) const {
        const int aid = a->getId();
        const int bid = b->getId();
        // A set of regions represents state. Each region can be
        // represented once and only once in the state. Thus, comparison
        // based on the type and id is sufficient.
        return aid == bid ? 0 : aid < bid ? -1 : 1;
    }
};

template <typename T>
struct Hash<REGION > {
    std::size_t operator()(REGION r) const {
        return r->getHash();
    }
};

template <typename T>
struct Equal<REGION > {
    bool operator()(REGION a, REGION b) const {
        return *a == *b;
    }
};

// Note: The default Max functor is OK for regions, as they are not
// stored in an interval RB tree.

#undef REGION
#define ALOCP typename \
    std::pair<boost::intrusive_ptr<absdomain::Region<T> >, \
              boost::intrusive_ptr<T> >

template <typename T> 
struct Cmp<ALOCP > {
    Cmp<boost::intrusive_ptr<absdomain::Region<T> > > cmpR;
    Cmp<boost::intrusive_ptr<T> > cmpT;
    int operator()(ALOCP a, ALOCP b) const {
        const int r = cmpR(a.first, b.first);
        const int t = cmpT(a.second, b.second);
        return r==0 && t==0 ? 0 :
            (r < 0 || (r==0 && t < 0)) ? -1 : 1;
    }
};

template <typename T> 
struct Hash<ALOCP > {
    std::size_t operator()(ALOCP p) const {
        return
            static_cast<std::size_t>(
                    utils::inthash(p.first->getId()) -
                    p.second->getHash());
    }
};

template <typename T>
struct Equal<ALOCP > {
    bool operator()(ALOCP a, ALOCP b) const {
        return *a.first == *b.first && *a.second == *b.second;
    }
};

// Note: The default Max functor is OK for the ALOCP pair, as that pair
// is not stored in an interval RB tree.

template <typename T>
inline std::ostream& operator<<(std::ostream& os, const ALOCP& p) {
    os << 'R' << p.first->getId() << ' ' << *p.second;
    return os;
}

#undef ALOCP
#define ALOCP typename \
    std::pair<boost::intrusive_ptr<T>, \
        boost::intrusive_ptr<absdomain::ValueSet<T> > >

template <typename T> 
struct Cmp<ALOCP > {
    Cmp<boost::intrusive_ptr<T> > cmpT;
    int operator()(ALOCP a, ALOCP b) const {
        // The addresses are not supposed to be overlapping, so compare
        // just the first pair.
        return cmpT(a.first, b.first);
    }
};

template <typename T>
struct Equal<ALOCP > {
    bool operator()(ALOCP a, ALOCP b) const {
        return *a.first == *b.first && *a.second == *b.second;
    }
};

template <typename T> 
struct Hash<ALOCP > {
    Hash<boost::intrusive_ptr<T> > hash;
    std::size_t operator()(ALOCP p) const {
        return hash(p.first) - p.second->getHash();
    }
};

template <typename T> 
struct Max<ALOCP > {
    Max<boost::intrusive_ptr<T> > max;
    int operator()(ALOCP p) const {
        return max(p.first);
    }
    int operator()(ALOCP a, int x, int y) const {
        return max(a.first, x, y);
    }
};

template <typename T> 
struct Overlap<ALOCP > {
    bool operator()(ALOCP a, ALOCP b) const {
        return a.first->overlaps(*b.first);
    }
    int low(ALOCP p) const {
        return p.first->getLo();
    }
    int high(ALOCP p) const {
        return p.first->getHi();
    }
};

template <class T>
inline std::ostream& operator<<(std::ostream& os, const ALOCP& p) {
    os << '@' << *p.first << '=' << *p.second;
    return os;
}

#undef ALOCP

} // End of the utils namespace

namespace absdomain {

template <typename T>
inline bool Region<T>::LessOrEqual::operator()(const ContentPair& a,
        const ContentPair& b) const {
    return utils::Cmp<ContentPair>()(a, b) <= 0;
}

} // End of the absdomain namespace

#endif // ABSTRACT_REGION_H
