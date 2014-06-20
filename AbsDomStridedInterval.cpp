// This is an implementation of bounded intervals, see:
// T. Reps, G. Balakrishnan, J. Lim: "Intermediate-Representation
// Recovery from Low-Level Code", PEPM'06 invited paper, pg. 6
// Handling of overflows in the paper is borrowed from the H.S. Warren's
// book "Hacker's Delight", pg. 56.

#include "AbsDomStridedInterval.h"
#include "Rand.h"
#include <iomanip>

using namespace absdomain;
using namespace utils;

namespace {

unsigned minOR(unsigned a, unsigned b, unsigned c, unsigned d) {
   unsigned m, temp; 
   m = (unsigned)1 << (CHAR_BIT * sizeof(unsigned) - 1); 
   while (m != 0) {
      if (~a & c & m) {
         temp = (a | m) & -m; 
         if (temp <= b) {
             a = temp; break;
         }
      } 
      else if (a & ~c & m) {
         temp = (c | m) & -m; 
         if (temp <= d) {
             c = temp; break;
         }
      } 
      m = m >> 1; 
   } 
   return a | c; 
} 

unsigned maxOR(unsigned a, unsigned b, unsigned c, unsigned d) {
   unsigned m, temp; 
   m = (unsigned)1 << (CHAR_BIT * sizeof(unsigned) - 1); 
   while (m != 0) {
      if (b & d & m) {
         temp = (b - m) | (m - 1); 
         if (temp >= a) {
             b = temp; break;
         }
         temp = (d - m) | (m - 1); 
         if (temp >= c) {
             d = temp; break;
         }
      } 
      m = m >> 1; 
   } 
   return b | d; 
} 

int rem(int d, unsigned m) {
    assert(m && "Modulo can't be zero.");
    assert(m < (unsigned)INT_MAX && "Stride out of bounds.");
    const int r = d % (int)m;
    return r >= 0 ? r : r + (int)m;
}

// Warning: Since all bounds are interpreted as signed, checking for
// unsigned overflows doesn't make much sense. Actually, it can produce
// unexpected results (e.g., inversion of bounds).

bool doesMulOverflow(int x, int y) {
    const int prod = x * y;
    return y != 0 && (prod / y) != x;
}

bool doesDivOverflow(int x, int y) {
    return x == INT_MIN && y == -1;
}

bool doesSumOverflow(int x, int y) {
    return ((x & y & ~(x+y)) | (~x & ~y & (x+y))) < 0;
}

bool doesSubOverflow(int x, int y) {
    return ((x & ~y & ~(x-y)) | (~x & y & (x-y))) < 0;
}

std::pair<int,int> shrinkAlign(int lo, int hi, unsigned s) {
    std::pair<int,int> bounds = std::make_pair(lo, hi);

    assert(lo < hi && "Invalid bounds.");
    assert(s > 0 && "Invalid stride, should be >0.");
    if (s == 1) {
        return bounds;
    }

    const int lres = rem(lo, s);
    const int hres = rem(hi, s);

    if (lres) {
        bounds.first = (bounds.first / (int)s) * (int)s;
    }
    if (hres) {
        bounds.second = (bounds.second / (int)s) * (int)s;
    }

    if (lres > hres) {
        return std::make_pair(INT_MIN,INT_MAX);
    } else {
        return bounds;
    }
}

std::pair<int,int> inflateAlign(int lo, int hi, unsigned s) {
    std::pair<int,int> bounds = std::make_pair(lo, hi);

    assert(s > 0 && "Invalid stride, should be >0.");
    if (s == 1) {
        return bounds;
    }

    const int lres = rem(lo, s);
    const int hres = rem(hi, s);

    if (lres) {
        bounds.first = (bounds.first / (int)s) * (int)s;
        if (bounds.first <= 0 && !doesSubOverflow(bounds.first, (int)s)){
            bounds.first -= s;
        }
    }
    if (hres) {
        bounds.second = (bounds.second / (int)s) * (int)s;
        if (bounds.second >= 0 && !doesSumOverflow(bounds.second,
                    (int)s)) {
            bounds.second += s;
        }
    }

    if (lres > hres) {
        return std::make_pair(INT_MIN,INT_MAX);
    } else {
        return bounds;
    }
}



} // End of the anonymous namespace

StridedInterval::StridedInterval(int low, int high, unsigned stride) :
    lo(low), hi(high), strd(stride) {
    assert(stride > 0 && "Stride must be > 0.");
}

StridedInterval::CacheTy::const_iterator_pair
    StridedInterval::equal_range(int h) {
    initCache();
    return cache->equal_range(h);
}

StridedIntervalPtr StridedInterval::get(int low, int high, unsigned
        stride) {
    assert(low < high && "Invalid interval, low < high must hold.");
    assert(stride > 0 && "Invalid stride, should be > 0.");
    if (low == INT_MIN && rem(low,stride) != 0) {
        low = (low / stride) * stride;
    }
    if (high == INT_MAX && rem(high,stride) != 0) {
        high = (high / stride) * stride;
    }
    assert(rem(low,stride) == 0 && rem(high,stride) == 0 &&
    "Misaligned lo/hi markers, they need to be multiples of stride.");
    const int h = computeHash(low, high, stride);
    CacheTy::const_iterator_pair CIP = equal_range(h);
    for (; CIP.first != CIP.second; ++CIP.first) {
        StridedIntervalPtr p = CIP.first->second;
        if (p->equal(low, high, stride)) {
            return p;
        }
    }
    // Interval not found in the cache
    StridedInterval* bi = new StridedInterval(low, high, stride);
    StridedIntervalPtr ptr(bi);
    assert(bi->getHash() == h && "Hash mismatch.");
    cache->insert(std::make_pair(h, ptr));
    return ptr;
}

StridedIntervalPtr StridedInterval::getBot() {
    const int low = 0;
    const int high = -1;
    const unsigned stride = 1;
    const int h = computeHash(low, high, stride);
    CacheTy::const_iterator_pair CIP = equal_range(h);
    for (; CIP.first != CIP.second; ++CIP.first) {
        StridedIntervalPtr p = CIP.first->second;
        if (p->equal(low, high, stride)) {
            return p;
        }
    }
    StridedInterval* si = new StridedInterval(low, high, stride);
    StridedIntervalPtr ptr(si);
    assert(si->getHash() == h && "Hash mismatch.");
    cache->insert(std::make_pair(h, ptr));
    return ptr;
}

StridedIntervalPtr StridedInterval::getTop() {
    return StridedInterval::get(INT_MIN, INT_MAX, 1);
}

StridedIntervalPtr StridedInterval::getRand(int limit, int strdlimit) {
    if (!limit) return getTop();
try_again:
    const int lo = rem(rnd::irand(),limit);
    const int off = rem(rnd::irand(),limit);
    const int hi = lo + (off < 0 ? -off : off);
    const int slimit = strdlimit+1 ? strdlimit+1 : 4;
    int strd = hi-lo > 0 ? 
        rem(rem(rnd::irand(),slimit),hi - lo) : 1;
    if (strd == 0) strd++;
    const int s = strd > 0 ? strd : -strd;
    std::pair<int,int> bnds = inflateAlign(lo, hi, s);
    if ((bnds.first == INT_MIN && bnds.second == INT_MAX) || bnds.first
            == bnds.second) {
        goto try_again;
    }
    return StridedInterval::get(bnds.first, bnds.second, s);
}

StridedIntervalPtr StridedInterval::get(int x, unsigned s) {
    assert(s > 0 && "Stride has to be > 0.");

    while (s != 0 && rem(x, s) != 0) { 
        s /= 2;
    }    
    if (s == 0) { 
        s++;
    }    

    if (doesSumOverflow(x, s)) {
        return getTop();
    } else {
        return get(x, x + (int)s, s);
    }
}

StridedIntervalPtr StridedInterval::join(_Self& adp) {
    if (isBot()) {
        return getBot();
    } else if (adp.isBot()) {
        return StridedIntervalPtr(this);
    } else {
        const int minLo = min(lo, adp.lo);
        const int maxHi = max(hi, adp.hi);
        const int s  = gcdSafe(strd, adp.strd);
        return get(minLo, maxHi, s);
    }
}

StridedIntervalPtr StridedInterval::meet(_Self& sip) {
    if (isBot() || sip.isBot()) {
        return getBot();
    }

    const StridedInterval* smaller = &sip;
    const StridedInterval* larger = this;
    if (smaller->lo > larger->lo) std::swap(smaller, larger);
    assert(smaller->lo <= larger->lo && 
        "Sorting failed to orient the bounds.");
    if (smaller->hi <= larger->lo) {
        return getBot(); // Empty intersection
    } else {
        return get(max(smaller->lo, larger->lo), min(smaller->hi,
                larger->hi), lcm(smaller->strd, larger->strd));
    }
}

StridedIntervalPtr StridedInterval::widen(_Self& with) {
    if (isTop() || with.isTop()) {
        return getTop();
    } else if (isBot()) {
        return with.isBot() ? getBot() : getTop();
    } else if (with.isBot() || *this == with) {
        return StridedIntervalPtr(this);
    }

    int stride = gcdSafe(strd, with.strd);
    int lb = lo, ub = hi;
    if (with.lo < lo) {
        return getTop();
        /*
        if (with.lo > 0) {
            lb = with.lo / 8;
            stride = gcdSafe(lb, stride);
        } else if (with.lo == 0) {
            lb = -(int)stride;
        } else if (doesMulOverflow(with.lo, 8)) {
            // with.lo is negative, but doubling the bound overflows
            return getTop();
        } else {
            // with.lo is negative, but we can double the bound
            lb = with.lo * 8;
        }// */
    }
    if (with.hi > hi) {
        return getTop();
        /*
        if (with.hi < 0) {
            ub = with.hi / 8;
            stride = gcdSafe(ub, stride);
        } else if (with.hi == 0) {
            ub = (int)stride;
        } else if (doesMulOverflow(with.hi, 8)) {
            // with.hi is positive, but doubling the bound overflows
            return getTop();
        } else {
            // with.hi is positive, but we can double the bound
            ub = with.hi * 8;
        }// */
    }
    return get(lb, ub, stride);
    //return rwiden(with, *getTop(), *getTop());
}

/*
StridedIntervalPtr StridedInterval::rwiden(_Self& with, _Self& lower,
        _Self& upper) {

    if (isTop() || with.isTop()) {
        return getTop();
    } else if (isBot()) {
        return with.isBot() ? getBot() : getTop();
    } else if (with.isBot() || *this == with) {
        return StridedIntervalPtr(this);
    }

    int stride = gcdSafe(strd, with.strd);
    int lb = lo, ub = hi;
    if (with.lo < lo) {
        if (!lower.isTop()) {
            assert(lower.hi < ub && "Lower bound too large.");
            lb = lower.hi;
            stride = gcdSafe(stride, lb);
        } else {
            return getTop();
        }
    }
    if (with.hi > hi) {
        if (!upper.isTop()) {
            assert(upper.lo > lb && "Upper bound too small.");
            ub = upper.lo;
            stride = gcdSafe(stride, ub);
        } else {
            return getTop();
        }
    }
    return get(lb, ub, stride);
}// */

StridedIntervalPtr StridedInterval::restrictUpperBound(int x) const {
    if (lo <= x && x <= hi) {
        std::pair<int,int> bounds = shrinkAlign(lo, x, strd);
        return get(bounds.first, bounds.second, strd);
    } else {
        return getBot();
    }
}

StridedIntervalPtr StridedInterval::restrictLowerBound(int x) const {
    if (lo <= x && x <= hi) {
        std::pair<int,int> bounds = shrinkAlign(x, hi, strd);
        return get(bounds.first, bounds.second, strd);
    } else {
        return getBot();
    }
}

bool StridedInterval::operator<=(const StridedInterval& a) const {
    if (doesSubOverflow(hi, (int)strd)) {
        return hi <= a.lo;
    } else {
        return hi - (int)strd <= a.lo;
    }
}

bool StridedInterval::operator<(const StridedInterval& a) const {
    return hi <= a.lo;
}

bool StridedInterval::operator>=(const StridedInterval& a) const {
    if (doesSubOverflow(a.hi, (int)a.strd)) {
        return a.hi <= lo;
    } else {
        return a.hi - (int)a.strd <= lo;
    }
}

bool StridedInterval::operator>(const StridedInterval& a) const {
    return a.hi <= lo;
}

bool StridedInterval::subsumes(const StridedInterval& a) const {
    // Handle special cases (only bottom has to be handled specially)
    if (isBot()) {
        return a.isBot();
    } else if (isTop() || a.isBot() || *this == a) {
        return true;
    } else if (a.isTop()) {
        return false;
    }
    return lo <= a.lo && hi >= a.hi && strd <= a.strd;
}

bool StridedInterval::withinBoundsOf(const StridedInterval& a) const {
    if (a.isBot()) {
        return isBot();
    } else if (a.isTop() || isBot() || *this == a) {
        return true;
    } else if (isTop()) {
        return false;
    }
    return a.lo <= lo && a.hi >= hi;
}

bool StridedInterval::subsumedBy(const StridedInterval& a) const {
    return a.subsumes(*this);
}

bool StridedInterval::overlaps(const StridedInterval& i) const {
    return (isBot() || i.isBot()) ? false : !(hi <= i.lo || i.hi <= lo);
}

void StridedInterval::split(const SIPtrVector& a, const SIPtrVector& b,
        SIPNVector & out) {
    
    typedef SIPtrVector::const_iterator const_iterator;

    if (a.empty()) {
        for (const_iterator I = b.begin(), E = b.end(); I != E; ++I) {
            out.push_back(std::make_pair(*I, splt::SECOND));
        }
        return;
    }

    if (b.empty()) {
        for (const_iterator I = a.begin(), E = a.end(); I != E; ++I) {
            out.push_back(std::make_pair(*I, splt::FIRST));
        }
        return;
    }


    const_iterator AI = a.begin(), AE = a.end(), BI = b.begin(), BE =
        b.end();

    int lowA = (*AI)->getLo();
    int lowB = (*BI)->getLo();

    for ( ; AI != AE && BI != BE; ) {
        // Intervals
        StridedIntervalPtr ai = *AI;
        StridedIntervalPtr bi = *BI;
        // Strides
        const unsigned strideA = ai->getStride();
        assert(strideA && "Invalid stride.");
        const unsigned strideB = bi->getStride();
        assert(strideB && "Invalid stride.");
        const unsigned strideAB = gcdSafe(strideA, strideB);
        // Alignment
        const bool alignedA = (rem(lowA,strideA) == 0);
        const bool alignedB = (rem(lowB,strideB) == 0);
        const bool alignedAB = (strideA == strideB);

        if (lowA == lowB) {
            const int minHi = min(ai->getHi(), bi->getHi());
            int strd = strideAB;
            if (alignedAB && strideA > strideB) {
                if (rem(lowA,strideA) == 0 && rem(minHi,strideA) == 0) {
                    strd = strideA;
                }
            } else if (alignedAB && strideA < strideB) {
                if (rem(lowA,strideB) == 0 && rem(minHi,strideB) == 0) {
                    strd = strideB;
                }
            }

            out.push_back(std::make_pair(get(lowA, minHi, strd),
                        splt::BOTH));
            assert((minHi > lowA || minHi > lowB) &&
                    "Termination failure.");
            lowA = minHi;
            lowB = minHi;
        } else if (lowA < lowB) {
            if (!alignedA) {
                std::pair<int,int> bnds =
                    inflateAlign(lowA,lowA,strideA);
                assert(bnds.second <= ai->getHi() &&
                        "Unexpected condition in bound adjustment.");
                const int hi = min(bnds.second, lowB);
                const int st = gcdSafe(lowA, gcdSafe(strideA, hi));
                out.push_back(std::make_pair(get(lowA, hi, st),
                            splt::FIRST));
                assert(hi > lowA && "Invalid bounds.");
                lowA = hi;
            } else if (ai->getHi() <= bi->getLo()) {
                out.push_back(std::make_pair(get(lowA, ai->getHi(),
                                strideA), splt::FIRST));
                assert(lowA < ai->getHi() && "Termination failure.");
                lowA = ai->getHi();
            } else {
                std::pair<int,int> bnds = inflateAlign(lowB, lowB,
                        strideA);
                int hi = min(bnds.first, ai->getHi());
                if (hi <= lowA) hi = lowB;
                const int st = gcdSafe(lowA, gcdSafe(strideA, hi));
                out.push_back(std::make_pair(get(lowA, hi, st),
                            splt::FIRST));
                assert(hi > lowA && "Invalid bounds.");
                lowA = hi;
            }
        } else {// lowB < lowA
            if (!alignedB) {
                std::pair<int,int> bnds =
                    inflateAlign(lowB,lowB,strideB);
                assert(bnds.second <= bi->getHi() &&
                        "Unexpected condition in bound adjustment.");
                const int hi = min(bnds.second, lowA);
                const int st = gcdSafe(lowB, gcdSafe(strideB, hi));
                out.push_back(std::make_pair(get(lowB,hi,st),
                            splt::SECOND));
                assert(lowB < hi && "Termination failure.");
                lowB = hi;
            } else if (bi->getHi() <= ai->getLo()) {
                out.push_back(std::make_pair(get(lowB, bi->getHi(),
                                strideB), splt::SECOND));
                assert(lowB < bi->getHi() && "Termination failure.");
                lowB = bi->getHi();
            } else {
                std::pair<int,int> bnds = inflateAlign(lowA, lowA,
                        strideB);
                int hi = min(bnds.first, bi->getHi());
                if (hi <= lowB) hi = lowA;
                const int st = gcdSafe(lowB, gcdSafe(strideB, hi));
                out.push_back(std::make_pair(get(lowB, hi, st),
                            splt::SECOND));
                assert(hi > lowB && "Invalid bounds.");
                lowB = hi;
            }
        }

        if (lowA == ai->getHi()) {
            assert(AI != AE && "Possible index out of bounds.");
            if (++AI != AE) {
                lowA = (*AI)->getLo();
            }
        }
        if (lowB == bi->getHi()) {
            assert(BI != BE && "Possible index out of bounds.");
            if (++BI != BE) {
                lowB = (*BI)->getLo();
            }
        }
    }

    while (AI != AE) {
        StridedIntervalPtr i = *AI;
        const unsigned stride = i->getStride();
        const unsigned gcd = gcdSafe(stride, lowA);
        if (stride == gcd) { // Aligned
            out.push_back(std::make_pair(get(lowA, i->getHi(), stride),
                        splt::FIRST));
            assert(lowA < i->getHi() && "Invalid bounds.");
            lowA = i->getHi();
        } else { // Misaligned
            std::pair<int,int> bnds = inflateAlign(lowA, lowA, stride);
            int hi = min(i->getHi(), bnds.first);
            if (hi <= lowA) hi = i->getHi();
            const int st = gcdSafe(lowA, gcdSafe(stride, hi));
            out.push_back(std::make_pair(get(lowA, hi, st),
                        splt::FIRST));
            assert(hi > lowA && "Invalid bounds.");
            lowA = hi;
        }

        if (lowA == i->getHi()) {
            ++AI;
        }
    }

    while (BI != BE) {
        StridedIntervalPtr i = *BI;
        const unsigned stride = i->getStride();
        const unsigned gcd = gcdSafe(stride, lowB);
        if (stride == gcd) { // Aligned
            out.push_back(std::make_pair(get(lowB, i->getHi(), stride),
                        splt::SECOND));
            assert(lowB < i->getHi() && "Invalid bounds.");
            lowB = i->getHi();
        } else { // Misaligned
            std::pair<int,int> bnds = inflateAlign(lowB, lowB, stride);
            int hi = min(i->getHi(), bnds.first);
            if (hi <= lowB) hi = i->getHi();
            const int st = gcdSafe(lowB, gcdSafe(stride, hi));
            out.push_back(std::make_pair(get(lowB, hi, st),
                        splt::SECOND));
            assert(hi > lowB && "Invalid bounds.");
            lowB = hi;
        }

        if (lowB == i->getHi()) {
            ++BI;
        }
    }
}

StridedIntervalPtr StridedInterval::operator+(int offset) {
    if (isBot() || isTop()) {
        return StridedIntervalPtr(this);
    }
    const int sumLo = lo + offset, sumHi = hi + offset;
    int u = lo & offset & ~sumLo & ~(hi & offset & ~sumHi);
    int v = ((lo ^ offset) | ~(lo ^ sumLo)) & (~hi & ~offset &
            (hi+offset));
    if ((u | v) < 0) {
        return getTop();
    } else if (sumLo == sumHi) {
        return get(sumLo, strd);
    } else {
        std::pair<int,int> bounds = shrinkAlign(sumLo, sumHi, strd);
        return get(bounds.first, bounds.second, strd);
    }
}

StridedIntervalPtr StridedInterval::operator+(StridedInterval& i){
    if (isBot()) { 
        return StridedIntervalPtr(&i); 
    } else if (i.isBot()) { 
        return StridedIntervalPtr(this); 
    }

    const int sumLo = lo + i.lo, sumHi = hi - strd + i.hi - i.strd;
    const int u = lo & i.lo & ~sumLo & ~(hi & i.hi & ~(hi+i.hi));
    const int v = ((lo ^ i.lo) | ~(lo ^ sumLo))&(~hi & ~i.hi &
            (hi+i.hi));
    if ((u | v) < 0) {
        return getTop();
    } else if (sumLo == sumHi) {
        return get(sumLo, umax(strd, i.strd));
    } else {
        const unsigned stride = gcdSafe(strd, i.strd);
        if (doesSumOverflow(sumHi, stride)) {
            return getTop();
        } else {
            assert(sumLo % stride == 0 && "Misaligned interval.");
            assert(sumHi % stride == 0 && "Misaligned interval.");
            return get(sumLo, sumHi+stride, stride);
        }
    }
}

StridedIntervalPtr StridedInterval::operator*(StridedInterval& i){
    if (isBot() || i.isBot()) {
        return getBot();
    } else if (isTop() || i.isTop()) {
        return getTop();
    }

    const int op1[4] = {lo, lo, (int)(hi-strd), (int)(hi-strd)};
    const int op2[4] = {i.lo, (int)(i.hi-i.strd), i.lo, (int)(i.hi-i.strd)};
    int minLo = INT_MAX;
    int maxHi = INT_MIN;

    for (int j = 0; j < 4; j++) {
        if (doesMulOverflow(op1[j], op2[j])) {
            return getTop();
        } else {
            const int prod = op1[j] * op2[j];
            minLo = min(minLo, prod);
            maxHi = max(maxHi, prod);
        }
    }

    if (minLo == maxHi && (minLo == INT_MAX || minLo == INT_MIN)) {
        return getTop();
    }

    if (minLo == maxHi) {
        return get(minLo, umax(strd, i.strd));
    } else {
        const unsigned stride = gcdSafe(strd, i.strd);
        if (doesSumOverflow(maxHi, stride)) {
            return getTop();
        } else {
            assert(minLo % stride == 0 && "Misaligned interval.");
            assert(maxHi % stride == 0 && "Misaligned interval.");
            return get(minLo, maxHi+stride, stride);
        }
    }
}

StridedIntervalPtr StridedInterval::operator-() {
    if (isBot()) { 
        return getBot(); 
    } else if (-hi+(int)strd == -lo) {
        return get(-lo, strd);
    } else if (lo != INT_MIN) {
        return get(-hi + strd, -lo + strd, strd);
    } else {
        return getTop();
    } 
}

StridedIntervalPtr StridedInterval::operator|(StridedInterval& i){
    if (isBot()) {
        return StridedIntervalPtr(&i);
    } else if (i.isBot()) {
        return StridedIntervalPtr(this);
    }

    const unsigned tlz1 = tlz(strd);
    const unsigned tlz2 = tlz(i.strd);
    const unsigned t = umax(tlz1,tlz2);
    const unsigned s = 1 << t;
    const unsigned mask = s - 1;
    const int r = (lo & mask) | (i.lo & mask); 

    const int a = lo & ~mask;
    const int b = (hi-strd) & ~mask;
    const int c = i.lo & ~mask;
    const int d = (i.hi-i.strd) & ~mask;
    const unsigned char key = 
        ((a < 0) << 3) |
        ((b < 0) << 2) |
        ((c < 0) << 1) |
        (d < 0);

    int lb = 0;
    int ub = 0;

    switch (key) {
        case 0x00 : // 0b0000_0000
        case 0x03 : // 0b0000_0011
        case 0x0C : // 0b0000_1100
        case 0x0F : // 0b0000_1111
            lb = minOR(a, b, c, d);
            ub = maxOR(a, b, c, d);
            break;
        case 0x0E : // 0b0000_1110, a,-1
            lb = a; ub = -1;
            break;
        case 0x0B : // 0b0000_1011, c,-1
            lb = c; ub = -1;
            break;
        case 0x0A : // 0b0000_1010, min(a,c),...
            lb = min(a, c);
            ub = maxOR(0, b, 0, d);
            break;
        case 0x08 : // 0b0000_1000, minOR(a,0xFF...)
            lb = minOR(a, -1, c, d);
            ub = maxOR(0, b, c, d);
            break;
        case 0x02 : // 0b0000_0010, minOR(a,b,c,0xFF...)
            lb = minOR(a, b, c, -1);
            ub = maxOR(a, b, 0, d);
            break;
        default:
            assert(false && 
            "Unhandled case in OR-propagation of strided intervals.");
    }

    const int lbnd = (lb & ~mask) | r;
    const int ubnd = (ub & ~mask) | r;
    //std::cout << '[' << lbnd << ',' << ubnd << ']';
    if (lbnd == ubnd) { // Special support for constants
        return get(lbnd, umax(strd, i.strd));
    } else if (doesSumOverflow(ubnd, s)) {
        return getTop();
    } else {
        std::pair<int,int> bnds = shrinkAlign(lbnd, ubnd+s, s);
        return get(bnds.first, bnds.second, s);
    }
}

StridedIntervalPtr StridedInterval::operator~() {
    if (isBot() || isTop()) {
        return StridedIntervalPtr(this);
    } else {
        const int tmpHi = hi - strd; // This should never underflow
        if (lo == tmpHi) { // Constant
            return get(~lo, strd);
        } else {
            const int newLo = ~tmpHi;
            const unsigned stride = gcdSafe(newLo, strd);
            if (doesSumOverflow(~lo, stride)) {
                return getTop();
            } else {
                std::pair<int,int> bnds = shrinkAlign(newLo,
                        ~lo+(int)stride, stride);
                return get(bnds.first, bnds.second, stride);
            }
        }
    }
}

StridedIntervalPtr StridedInterval::operator&(StridedInterval& i){
    if (isBot() || i.isBot()) {
        return getBot();
    } else if (isTop()) {
        return get(i.lo, i.hi, gcdSafe(strd, i.strd));
    } else if (i.isTop()) {
        return get(lo, hi, gcdSafe(strd, i.strd));
    } else if (isConstant() && i.isConstant()) {
        return get(lo & i.lo, umax(strd, i.strd));
    } else {
        return ~(*(*(~(*this)) | *(~i)));
    }
}

StridedIntervalPtr StridedInterval::operator^(StridedInterval& i){
    if (isBot()) {
        return StridedIntervalPtr(&i);
    } else if (i.isBot()) {
        return StridedIntervalPtr(this);
    } else if (isTop() || i.isTop()) {
        return getTop();
    } else if (isConstant() && i.isConstant()) {
        return get(lo ^ i.lo, umax(strd, i.strd));
    }

    StridedInterval& negi = *(~i);
    StridedInterval& negt = *(~(*this));
    return *(*this & negi) | *(negt & i);
}

StridedIntervalPtr StridedInterval::sdivide(StridedInterval& i) {
    if (isTop() || i.isBot() || i.containsZero()) {
        return getTop();
    } else if (isBot()) {
        return getBot();
    }

    const int op1[4] = {lo, lo, (int)(hi-strd), (int)(hi-strd)};
    const int op2[4] = {i.lo, (int)(i.hi-i.strd), i.lo, (int)(i.hi-i.strd)};
    int minLo = INT_MAX;
    int maxHi = INT_MIN;

    for (int j = 0; j < 4; j++) {
        if (doesDivOverflow(op1[j], op2[j])) {
            return getTop();
        } else {
            const int div = op1[j] / op2[j];
            minLo = min(minLo, div);
            maxHi = max(maxHi, div);
        }
    }

    if (minLo == maxHi && (minLo == INT_MAX || minLo == INT_MIN)) {
        return getTop();
    }
    
    if (minLo == maxHi) { // Special support for constants
        return get(minLo, umax(strd, i.strd));
    } else {
        const int s = gcdSafe(minLo, strd);
        if (doesSumOverflow(maxHi, s)) {
            return getTop();
        } else {
            std::pair<int,int> bounds = shrinkAlign(minLo,
                    maxHi+(int)s, s);
            return get(bounds.first, bounds.second, s);
        }
    }
}

StridedIntervalPtr StridedInterval::udivide(StridedInterval& i) {
    if (isTop() || i.isBot() || i.containsZero()) {
        return getTop();
    } else if (isBot()) {
        return getBot();
    }

    const unsigned op1[4] = {(unsigned)lo, (unsigned)lo,
        (unsigned)hi-strd, (unsigned)hi-strd};
    const unsigned op2[4] = {(unsigned)i.lo, (unsigned)i.hi-i.strd,
        (unsigned)i.lo, (unsigned)i.hi-i.strd};
    unsigned minLo = UINT_MAX;
    unsigned maxHi = 0;

    for (int j = 0; j < 4; j++) {
        const unsigned div = op1[j] / op2[j];
        minLo = umin(div, minLo);
        maxHi = umax(div, maxHi);
    }

    if (minLo == maxHi) {
        return get(minLo, umax(strd, i.strd));
    } else {
        const unsigned stride = gcdSafe(minLo, strd);
        if (doesSumOverflow(maxHi, stride) || (int)minLo > (int)maxHi) {
            return getTop();
        } else {
            std::pair<int,int> bounds = shrinkAlign(minLo,
                    (int)maxHi+(int)stride, stride);
            return get(minLo, bounds.second, stride);
        }
    }
}

StridedIntervalPtr StridedInterval::smod(StridedInterval& i) {
    if (isTop() || i.isBot() || i.containsZero()) {
        return getTop();
    } else if (isBot()) {
        return getBot();
    }

    const int op1[4] = {lo, lo, (int)(hi-strd), (int)(hi-strd)};
    const int op2[4] = {i.lo, (int)(i.hi-i.strd), i.lo, (int)(i.hi-i.strd)};
    int minLo = INT_MAX;
    int maxHi = INT_MIN;

    for (int j = 0; j < 4; j++) {
        const int mod = op1[j] % op2[j];
        minLo = min(minLo, mod);
        maxHi = max(maxHi, mod);
    }

    if (minLo == maxHi) {
        return get(minLo, umax(strd, i.strd));
    } else {
        const int s = gcdSafe(minLo, strd);
        if (doesSumOverflow(maxHi, s)) {
            return getTop();
        } else {
            std::pair<int,int> bounds = shrinkAlign(minLo,
                    maxHi+(int)s, s);
            return get(bounds.first, bounds.second, s);
        }
    }
}

StridedIntervalPtr StridedInterval::umod(StridedInterval& i) {
    if (i.isBot() || i.isTop() || i.containsZero()) {
        return getTop();
    } else if (isBot()) {
        return getBot();
    }

    const unsigned op1[4] = {(unsigned)lo, (unsigned)lo,
        (unsigned)hi-strd, (unsigned)hi-strd};
    const unsigned op2[4] = {(unsigned)i.lo, (unsigned)i.hi-i.strd,
        (unsigned)i.lo, (unsigned)i.hi-i.strd};
    unsigned minLo = UINT_MAX;
    unsigned maxHi = 0;

    for (int j = 0; j < 4; j++) {
        const unsigned mod = op1[j] % op2[j];
        minLo = umin(mod, minLo);
        maxHi = umax(mod, maxHi);
    }

    if (minLo == maxHi) {
        return get(minLo, umax(strd, i.strd));
    } else {
        const unsigned stride = gcdSafe(minLo, strd);
        if (doesSumOverflow(maxHi, stride) || (int)minLo > (int)maxHi) {
            return getTop();
        } else {
            std::pair<int,int> bounds = shrinkAlign(minLo,
                    (int)maxHi+(int)stride, stride);
            return get(minLo, bounds.second, stride);
        }
    }
}

StridedIntervalPtr StridedInterval::lshift(StridedInterval& i) {
    if (i.isBot() || i.isTop()) {
        return getTop();
    } else if (isBot()) {
        return getBot();
    }

    const int op1[4] = {lo, lo, (int)(hi-strd), (int)(hi-strd)};
    const int op2[4] = {i.lo, (int)(i.hi-i.strd), i.lo, (int)(i.hi-i.strd)};
    int minLo = INT_MAX;
    int maxHi = INT_MIN;

    for (int j = 0; j < 4; j++) {
        const int res = op1[j] << op2[j];
        minLo = min(minLo, res);
        maxHi = max(maxHi, res);
    }

    if (minLo == maxHi) {
        return get(minLo, umax(strd, i.strd));
    } else {
        const unsigned stride = gcdSafe(strd, i.strd);
        // Stride stays the same (left shift)
        if (doesSumOverflow(maxHi, strd)) {
            return getTop();
        } else {
            std::pair<int,int> bounds = shrinkAlign(minLo,
                    maxHi+(int)strd, strd);
            return get(bounds.first, bounds.second, strd);
        }
    }
}

StridedIntervalPtr StridedInterval::rshift(StridedInterval& i) {
    if (isBot()) {
        return getBot();
    } else if (i.isBot() || i.isZero()) {
        return StridedIntervalPtr(this);
    } else if (isTop() || i.isTop()) {
        return getTop();
    }

    const unsigned op1[4] = {(unsigned)lo, (unsigned)lo,
        (unsigned)hi-strd, (unsigned)hi-strd};
    const unsigned op2[4] = {(unsigned)i.lo, (unsigned)i.hi-i.strd,
        (unsigned)i.lo, (unsigned)i.hi-i.strd};
    unsigned minLo = UINT_MAX;
    unsigned maxHi = 0;

    for (int j = 0; j < 4; j++) {
        const unsigned res = op1[j] >> op2[j];
        minLo = umin(res, minLo);
        maxHi = umax(maxHi, res);
    }

    if (minLo == maxHi) {
        return get(minLo, umax(strd, i.strd));
    } else {
        const unsigned stride = gcdSafe(minLo, strd);
        if (doesSumOverflow(maxHi, stride) || (int)minLo > (int)maxHi) {
            return getTop();
        } else {
            std::pair<int,int> bounds = shrinkAlign(minLo,
                    (int)maxHi+(int)stride, stride);
            return get(minLo, bounds.second, stride);
        }
    }
}

StridedIntervalPtr StridedInterval::arshift(StridedInterval& i) {
    if (isBot()) {
        return getBot();
    } else if (i.isBot() || i.isZero()) {
        return StridedIntervalPtr(this);
    } else if (isTop() || i.isTop()) {
        return getTop();
    }

    const int op1[4] = {lo, lo, (int)(hi-strd), (int)(hi-strd)};
    const int op2[4] = {i.lo, (int)(i.hi-i.strd), i.lo, (int)(i.hi-i.strd)};
    int minLo = INT_MAX;
    int maxHi = INT_MIN;

    for (int j = 0; j < 4; j++) {
        const int res = op1[j] >> op2[j];
        minLo = min(minLo, res);
        maxHi = max(maxHi, res);
    }

    if (minLo == maxHi) {
        return get(minLo, umax(strd, i.strd));
    } else {
        const int s = gcdSafe(minLo, strd);
        if (doesSumOverflow(maxHi, s)) {
            return getTop();
        } else {
            std::pair<int,int> bounds = shrinkAlign(minLo,
                    maxHi+(int)s, s);
            return get(bounds.first, bounds.second, s);
        }
    }
}

// Propagating bounds through rotations would be challenging...

StridedIntervalPtr StridedInterval::lrotate(StridedInterval& i) {
    if (isBot()) {
        return getBot();
    } else if (i.isBot() || i.isZero()) {
        return StridedIntervalPtr(this);
    } else {
        return getTop();
    }
}

StridedIntervalPtr StridedInterval::rrotate(StridedInterval& i) {
    if (isBot()) {
        return getBot();
    } else if (i.isBot() || i.isZero()) {
        return StridedIntervalPtr(this);
    } else {
        return getTop();
    }
}

bool StridedInterval::isTop() const {
    return lo == INT_MIN && hi == INT_MAX;
}

bool StridedInterval::isBot() const {
    return lo > hi;
}

bool StridedInterval::containsZero() const {
    // Although high is not inclusive, it's used in calculations, so for
    // safety, it includes zero.
    return lo <= 0 && hi > 0;
}

void StridedInterval::print(std::ostream& os) const {
    const int width = sizeof(int) * 2;
    os << std::dec << std::noshowbase << strd << '[';
    if (lo == INT_MIN) {
        os << "-INFTY";
    } else if (lo == INT_MAX) {
        os << "+INFTY";
    } else {
#ifndef SHOWDECIMAL
        os << std::hex << std::showbase << lo;
#else
        os << lo;
#endif
    }
    os << ',';
    if (hi == INT_MIN) {
        os << "-INFTY";
    } else if (hi == INT_MAX) {
        os << "+INFTY";
    } else {
#ifndef SHOWDECIMAL
        os << std::hex << std::showbase << hi;
#else
        os << hi;
#endif
    }
    os << std::dec << std::noshowbase << ']';
}

template<> THREADLOCAL utils::Multimap<int, StridedIntervalPtr>*
    AbstractDomain<StridedInterval>::cache(0);
