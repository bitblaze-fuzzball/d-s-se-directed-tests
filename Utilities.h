#ifndef UTILITIES_H
#define UTILITIES_H

#include <tr1/unordered_map>
#include <cstddef>

namespace utils {

template <class K, class T>
class Multimap : public std::tr1::unordered_multimap<K,T> {
    typedef typename std::tr1::unordered_multimap<K,T> _Base;
public:
    typedef typename _Base::iterator iterator;
    typedef typename _Base::const_iterator const_iterator;
    typedef std::pair<iterator,iterator> iterator_pair;
    typedef std::pair<const_iterator,const_iterator>
        const_iterator_pair;
};

template <class K, class T>
class Map : public std::tr1::unordered_map<K,T> {
    typedef typename std::tr1::unordered_map<K,T> _Base;
public:
    typedef typename _Base::iterator iterator;
    typedef typename _Base::const_iterator const_iterator;
    typedef std::pair<iterator,iterator> iterator_pair;
    typedef std::pair<const_iterator,const_iterator>
        const_iterator_pair;
};

template <typename T>
struct Hash {
    size_t operator()(T e) const {
        return static_cast<std::size_t>(e);
    }   
};

template <typename T>
struct Hash<const T *const> {
    std::size_t operator()(const T *const x) const {
        return reinterpret_cast<std::size_t>(x);
    }
};

template <typename T>
struct Max {
    int operator()(T a) const { (void)a; return 0; }
    int operator()(T a, int b, int c) const {
	(void)a; (void)b; (void)c; return 0;
    }
};

template <typename T>
struct Cmp {
    int operator()(T a, T b) const {
        return a == b ? 0 : a < b ? -1 : 1;
    }
};

template <typename T>
struct Equal {
    int operator()(T a, T b) const {
        return a == b;
    }
};

/* Usage example:
std::transform(m.begin(), m.end(), std::back_inserter(vk),
               select1st<std::map<K, I>::value_type>()) ; 
// */

// Select 1st from the pair.
template <typename Pair>
struct select1st {
    typedef typename Pair::first_type result_type;
    const result_type &operator()(const Pair &p) const { 
        return p.first; 
    }   
};

// Select 2nd from the pair.
template <typename Pair>
struct select2nd {
    typedef typename Pair::second_type result_type ;
    const result_type &operator()(const Pair &p) const { 
        return p.second; 
    }   
}; 

// This is a default overlap functor used below. Use this for types that
// don't use interval trees.
template <typename T>
struct Overlap {
    bool operator()(T, T) const { return false; }
    int low(T) const { return 0; }
    int high(T) const { return 0; }
};

unsigned gcdSafe(unsigned, unsigned);
int lcm(int, int);
int tlz(unsigned);
int min(int, int);
int max(int, int);
unsigned umin(unsigned, unsigned);
unsigned umax(unsigned, unsigned);

} // End of utils namespace

#endif // UTILITIES_H
