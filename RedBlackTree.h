// This is an implementation of persistent red-black trees using
// Okasaki's algorithm and Eker's optimizations:
//
// Okasaki, Chris: Red-black trees in a functional setting, J. Funct.
// Program.  vol 9, num 4, 1999, pages 471--477,
// http://dx.doi.org/10.1017/S0956796899003494, Cambridge University
// Press, New York, NY, USA
//
// Eker, Steven: Associative-commutative rewriting on large terms,
// RTA'03: Proceedings of the 14th international conference on Rewriting
// techniques and applications, 2003, pages 14--29, Valencia, Spain,
// Springer-Verlag, Berlin, Heidelberg
//
// Thomas H. Cormen, Charles E. Leiserson, Ronald L. Rivest:
// Introduction to Algorithms, MIT Press and McGraw-Hill, 2000, chapter
// 14 and section 15.3

#ifndef REDBLACK_TREE_H
#define REDBLACK_TREE_H

#define CLEANUPPERIOD 10000

#include "Utilities.h"
#include "debug.h"
#include "RBNode.h"
#include "CompilerPortability.h"
#include <functional>
#include <algorithm>

namespace utils {

template <typename T>
class RedBlackTree : public pt::Counted {
public:
    typedef RBNode<T> Rbnode;
    typedef boost::intrusive_ptr<RedBlackTree> RedBlackTreePtr;
private:

    typedef RedBlackTree _Self;
    typedef typename utils::Multimap<int, RedBlackTreePtr> CacheTy;
    typedef typename CacheTy::const_iterator const_cache_iterator;
    typedef std::pair<const_cache_iterator, const_cache_iterator>
        const_cache_iterator_pair;
    typedef typename CacheTy::iterator cache_iterator;
    typedef std::pair<cache_iterator, cache_iterator>
        cache_iterator_pair;

    Rbnode* root;

    static THREADLOCAL typename Rbnode::RBVector *stack;
    static THREADLOCAL CacheTy *cache;
    static THREADLOCAL int opcounter;
    static THREADLOCAL bool doCache;
    
    RedBlackTree();
    RedBlackTree(Rbnode* n);
    static void initStackAndCache();

    ~RedBlackTree() {
        if (getRoot() && getRoot()->unref()) delete root;
    }

    static void cleanup();

    // Finds a tree in the cache that is almost the same as this tree,
    // but has an additional (or doesn't have) element elem. Returns
    // RedBlackTreePtr(0) if there's no such tree.
    RedBlackTreePtr findWith(T);
    RedBlackTreePtr findWithout(T);
    static int getHash(T elem) { return Rbnode::getHash(elem); }

    template <typename Iter>
    static Rbnode* get(Iter, Iter, rbt::ColorTy);

    Rbnode* getRoot() { return root; }
    const Rbnode* find(T elem) const { 
        return getRoot() ? getRoot()->find(elem) : 0; 
    }
    Rbnode* find(T elem) { 
        return getRoot() ? getRoot()->find(elem) : 0; 
    }
    bool findTree(const Rbnode*) const;

    // This function does something only when tree is used as an
    // interval tree (see comments above).
    const Rbnode* findFirstOverlapping(T elem) const {
        return getRoot() ? getRoot()->findFirstOverlapping(elem) : 0;
    }

public:
    typedef boost::intrusive_ptr<Rbnode> RbnodePtr;
    typedef typename Rbnode::const_iterator const_iterator;

    // Checks the red-black tree invariants:
    // 1. Root is black
    // 2. Red nodes have black children
    // 3. Every path has the same number of black nodes
    bool check() const { return getRoot() ? getRoot()->check() : true; }

    // Creates an empty tree, occasionally convenient to have
    static RedBlackTreePtr get();

    // Creates a tree with a single element
    static RedBlackTreePtr get(T);
    // Note: the vector with elements has to be properly sorted.
    static RedBlackTreePtr get(std::vector<T>&);
    // Note: the range determined by the iterators has to be properly
    // sorted.
    template <typename Iter>
    static RedBlackTreePtr get(Iter, Iter);

    // Call at the end of the program, or when all prior cashed trees
    // need to be destroyed.
    static void shutdown() {
        if (stack) { stack->clear(); } 
        delete stack; delete cache;
        stack = 0; cache = 0;
    }

    T match(T elem) const {
        const Rbnode* n = find(elem);
        return n ? n->get() : T();
    }
    
    const Rbnode* getRoot() const { return root; }

    // These two functions do something only when tree is used as an
    // interval tree (see comments above).
    void findAllOverlapping(T elem, std::vector<T>& found) const {
        if (getRoot()) getRoot()->findAllOverlapping(elem, found);
    }
 
    bool hasOverlapping(T elem) const {
        return findFirstOverlapping(elem) != 0;
    }

    RedBlackTreePtr insert(T);
    RedBlackTreePtr erase(T);
    RedBlackTreePtr replace(T, T with);

    int getHash() const { return getRoot() ? getRoot()->getHash() : 0; }
    unsigned treeSize() const { 
        return getRoot() ? getRoot()->treeSize() : (unsigned)0; 
    }
    bool empty() const { return !root; }

    const_iterator begin() const;
    const_iterator end() const;

    // Implemented as friend for symmetry
    template <typename T2>
    friend bool operator==(const RedBlackTree<T2>&, const
            RedBlackTree<T2>&);

    template <typename T2>
    friend bool operator!=(const RedBlackTree<T2>&, const
            RedBlackTree<T2>&);
};

// *** Implementation ***

template <typename T>
RedBlackTree<T>::RedBlackTree(Rbnode* n) : root(n) {
    initStackAndCache();

    if (++opcounter % (CLEANUPPERIOD + 1) == 0) {
        cleanup();
    }

    assert(root && "Tree initialized with a NULL node?");
    n->ref();

    if (doCache) {
        assert(findTree(n) == 0 && "Inserting a duplicate into cache.");
        cache->insert(std::make_pair(getHash(), RedBlackTreePtr(this)));
    }
}

template <typename T>
void RedBlackTree<T>::initStackAndCache() {
    if (!stack) stack = new typename Rbnode::RBVector();
    if (!cache) cache = new CacheTy();
    stack->clear();
}

template <typename T>
RedBlackTree<T>::RedBlackTree() : root(0) {
    initStackAndCache();
    if (doCache) {
        cache->insert(std::make_pair(getHash(), RedBlackTreePtr(this)));
    }
}

template <typename T>
inline void RedBlackTree<T>::cleanup() {
    for (cache_iterator I = cache->begin(), E = cache->end(); I != E;) {
        if (I->second->count() == 1) {
            cache->erase(I++);
        } else {
            ++I;
        }
    }
}

template <typename T>
inline typename RedBlackTree<T>::RedBlackTreePtr
RedBlackTree<T>::findWith(T elem) {
    initStackAndCache();
    assert(find(elem) == 0 &&
        "The node to be inserted already in the tree.");

    const int h = getHash() ^ getHash(elem);
    const_cache_iterator_pair CIP = cache->equal_range(h);
    Equal<T> eq;

    for (; CIP.first != CIP.second; ++CIP.first) {
        RedBlackTreePtr p = CIP.first->second;
        // The tree in the cache must be by 1 larger than this one (as
        // it contains the additional element elem)
        if (p->treeSize() != treeSize() + 1) {
            continue;
        }
        
        // Note: this tree is smaller
        const_iterator I = begin(), OI = p->begin(), E = end(), OE =
            p->end();

        bool match = true;
        for ( ; OI != OE; ++OI) {
            if (eq(OI->get(), elem)) {
                // continue
            } else if (I != E && eq(OI->get(), I->get())) {
                ++I;
            } else {
                match = false;
                break;
            }
        }

        if (match && I == E) {
            assert(p->find(elem) && "Matched a wrong tree.");
            assert(p->getHash() == h && "Invalid hash.");
            return p;
        }
    }

    RedBlackTreePtr t;
    if (getRoot()) {
        assert(stack && stack->empty() && "Garbage on stack?");
        t = RedBlackTreePtr(new
                RedBlackTree(getRoot()->insert(*stack, elem)));
    } else {
        t = RedBlackTree::get(elem);
    }
    assert(t->getHash() == h && "Invalid hash.");
    return t;
}
    
template <typename T>
inline bool RedBlackTree<T>::findTree(const Rbnode* n) const {
    const int h = n->getHash();
    Equal<T> eq;
    const_cache_iterator_pair CIP = cache->equal_range(h);
    for (; CIP.first != CIP.second; ++CIP.first) {
        RedBlackTreePtr p = CIP.first->second;
        if (p->treeSize() != n->treeSize()) {
            continue;
        }
        
        const_iterator TI = p->begin(), TE = p->end(), NI = n->begin(),
                       NE = n->end();
        bool match = true;

        for (; TI != TE && NI != NE && match; ++TI, ++NI) {
            match = eq(TI->get(), NI->get());
        }

        if (match && NI == NE && TI == TE) {
            return true;
        }
    }
    return false;
}

template <typename T>
template <typename Iter>
inline typename RedBlackTree<T>::Rbnode*
RedBlackTree<T>::get(Iter I, Iter E, rbt::ColorTy color){
    const size_t size = std::distance(I, E);

    if (I == E) {
        return 0;
    } if (I + 1 == E) {
        return new Rbnode(*I);
    } else if (I + 2 == E) {
        Rbnode* l = new Rbnode(*I, rbt::RED);
        return new Rbnode(*(I+1), l, 0);
    } else {
        assert(size > 2 && "Invalid iterator distance.");
        const rbt::ColorTy childColor = (color == rbt::RED) ? rbt::BLACK
            : rbt::RED;
        Rbnode* left = get(I, I + size/2, childColor);
        Rbnode* right = get(I + size/2 + 1, E, childColor);
        return new Rbnode(*(I + size/2), left, right, color);
    }
}

template <typename T>
inline typename RedBlackTree<T>::RedBlackTreePtr
RedBlackTree<T>::findWithout(T elem) {
    initStackAndCache();
    assert(find(elem) != 0 &&"The node to be deleted not in the tree.");
    assert(treeSize() > 0 && "Removing elements from an empty tree?");

    // XORing will effectively remove the hash of the element from the
    // tree hash.
    const int h = getHash() ^ getHash(elem);
    Equal<T> eq;

    const_cache_iterator_pair CIP = cache->equal_range(h);
    for (; CIP.first != CIP.second; ++CIP.first) {
        RedBlackTreePtr p = CIP.first->second;
        // The tree in the cache must be by 1 smaller than this one (as
        // it contains the additional element elem)
        if (p->treeSize() != treeSize() - 1) {
            continue;
        }

        // Note: this tree is larger
        const_iterator I = begin(), OI = p->begin(), E = end(), OE =
            p->end();
        
        bool match = true;
        for ( ; I != E; ++I) {
            if (eq(I->get(), elem)) {
                // continue
            } else if (OI != OE && eq(OI->get(), I->get())) {
                ++OI;
            } else {
                match = false;
                break;
            }
        }

        if (match && OI == OE) {
            assert(!p->find(elem) && "Matched a wrong tree.");
            assert(p->getHash() == h && "Invalid hash.");
            return p;
        }
    }

    assert(getRoot() && "Unexpected NULL ptr.");
    RedBlackTree *t = new RedBlackTree(getRoot()->erase(*stack, elem));
    assert(t->getHash() == h && "Invalid hash.");
    return RedBlackTreePtr(t);
}

template <typename T>
inline typename RedBlackTree<T>::RedBlackTreePtr
RedBlackTree<T>::get(T elem) {
    initStackAndCache();
    Equal<T> eq;
    const int h = getHash(elem);
    const_cache_iterator_pair CIP = cache->equal_range(h);
    for (; CIP.first != CIP.second; ++CIP.first) {
        RedBlackTreePtr p = CIP.first->second;
        if (p->treeSize()== (unsigned)1 && eq(p->getRoot()->get(), elem)) {
            assert(p->getHash() == h && "Invalid hash.");
            return p;
        }
    }

    RedBlackTree *t = new RedBlackTree(new Rbnode(elem));
    assert(t->getHash() == h && "Invalid hash.");
    return RedBlackTreePtr(t);
}

template <typename T>
inline typename RedBlackTree<T>::RedBlackTreePtr RedBlackTree<T>::get(){
    initStackAndCache();
    const int h = 0;
    const_cache_iterator_pair CIP = cache->equal_range(h);
    for (; CIP.first != CIP.second; ++CIP.first) {
        RedBlackTreePtr p = CIP.first->second;
        if (p->treeSize() == (unsigned)0) {
            assert(p->getHash() == h && "Invalid hash.");
            return p;
        }
    }

    RedBlackTree *t = new RedBlackTree();
    assert(t->getHash() == h && "Invalid hash.");
    return RedBlackTreePtr(t);
}

template <typename T>
inline typename RedBlackTree<T>::RedBlackTreePtr
RedBlackTree<T>::get(std::vector<T>& elems) {
    initStackAndCache();
    typedef typename std::vector<T>::const_iterator const_iterator;
    return get<const_iterator>(elems.begin(), elems.end());
}

template <typename T>
template <typename Iter>
inline typename RedBlackTree<T>::RedBlackTreePtr
RedBlackTree<T>::get(Iter I, Iter E) {
    initStackAndCache();
    const size_t size = std::distance(I, E);

    // Compute hash of the tree
    int h = 0;
    for (Iter II = I; II != E; ++II) {
        h ^= getHash(*II);
    }
    Equal<T> eq;
    
    // Check cache first
    const_cache_iterator_pair CIP = cache->equal_range(h);
    for (; CIP.first != CIP.second; ++CIP.first) {
        RedBlackTreePtr p = CIP.first->second;

        if (p->treeSize() != size) {
            continue;
        }

        Iter II = I;
        bool match = true;

        const_iterator OI = p->begin(), OE = p->end(); 
        for (; OI != OE && II != E; ++OI, ++II) {
            const Rbnode* rn = &*OI;
            if (!eq(rn->get(), *II)) {
                match = false;
                break;
            }
        }

        if (match && OI == OE && II == E) return p;
    }

    // Construct a balanced tree from the sorted array
    Rbnode* t = get(I, E, rbt::BLACK);
    assert(t->getHash() == h && "Invalid hash.");
    t->check();
    return RedBlackTreePtr(new RedBlackTree(t));
}

template <typename T>
inline typename RedBlackTree<T>::RedBlackTreePtr
RedBlackTree<T>::insert(T elem) {
    // Already in the tree
    return find(elem) ? RedBlackTreePtr(this) : findWith(elem);
}

template <typename T>
inline typename RedBlackTree<T>::RedBlackTreePtr
RedBlackTree<T>::erase(T elem) {
    // Not in the tree, nothing to do
    return find(elem) ? findWithout(elem) : RedBlackTreePtr(this);
}

template <typename T>
inline typename RedBlackTree<T>::RedBlackTreePtr
RedBlackTree<T>::replace(T match, T with) {
    Equal<T> eq;
    if (eq(match, with)) {
        return RedBlackTreePtr(this);
    }

    initStackAndCache();
    const int h = getHash() ^ getHash(match) ^ getHash(with);
    const_cache_iterator_pair CIP = cache->equal_range(h);
    for (; CIP.first != CIP.second; ++CIP.first) {
        RedBlackTreePtr p = CIP.first->second;
        if (p->treeSize() != treeSize()) {
            continue;
        }
        const_iterator TI = begin(), TE = end(), PI = p->begin(),
                       PE=p->end();
        bool match = false;
        for ( ; TI != TE && PI != PE; ++TI, ++PI) {
            const Rbnode* trn = &*TI;
            const Rbnode* prn = &*PI;
            if (!eq(trn->get(), prn->get())) {
                if (!match && eq(prn->get(), with)) {
                    match = true;
                } else {
                    match = false;
                    break;
                }
            }
        }

        if (match && TI == TE && PI == PE) {
            assert(p->getHash() == h && "Invalid hash.");
            return p;
        }
    }

    RedBlackTree *t = new RedBlackTree(getRoot()->replace(*stack, match,
                with));
    assert(t->getHash() && "Invalid hash.");
    return RedBlackTreePtr(t);
} 

template <typename T>
inline typename RedBlackTree<T>::const_iterator RedBlackTree<T>::begin()
    const {
    if (getRoot()) {
        return getRoot()->begin();
    } else {
        typename Rbnode::const_iterator E;
        return E;
    }
}

template <typename T>
inline typename RedBlackTree<T>::const_iterator RedBlackTree<T>::end()
    const {
    if (getRoot()) {
        return getRoot()->end();
    } else {
        typename Rbnode::const_iterator E;
        return E;
    }
}

template <typename T>
inline bool operator==(const RedBlackTree<T>& A,
                const RedBlackTree<T>& B) {

    typedef RBNode<T> NodeTy;
    const NodeTy* rootA = A.getRoot();
    const NodeTy* rootB = B.getRoot();

    if (!rootA || !rootB) {
        return rootA == rootB;
    }

#ifndef NDEBUG
    if (ASSERT_LEVEL > 3) {
        bool treesEqual = true;

        if (rootA->treeSize() != rootB->treeSize()) {
            treesEqual = false;
        }
        Equal<T> eq;

        for (typename NodeTy::const_iterator AI = rootA->begin(), AE =
                rootA->end(), BI = rootB->begin(), BE = rootB->end(); AI
                != AE && BI != BE && treesEqual; ++AI, ++BI) {
            treesEqual = eq(AI->get(), BI->get());
        }

        if ((rootA == rootB) != treesEqual) {
            rootA->toDotTree(std::cout);
            rootB->toDotTree(std::cout);
        }

        assert((rootA == rootB) == treesEqual && 
                "Trees not singletons?");
    }
#endif

    return rootA == rootB;
}

template <typename T>
inline bool operator!=(const RedBlackTree<T>& A, const
        RedBlackTree<T>& B) {

    assert4((A.getRoot() == B.getRoot()) == (A == B) &&
            "Trees not singletons?");
    return A.getRoot() != B.getRoot();
}

} // End of the utils namespace

#endif // REDBLACK_TREE_H
