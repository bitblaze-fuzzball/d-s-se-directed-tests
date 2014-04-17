// TODO: 
// 1) implement reverse iterators

#ifndef RBNODE_H
#define RBNODE_H

#include "Counted.h"
#include <vector>
#include <climits>
#include <cassert>
#include <iostream>

namespace utils {

namespace rbt {

enum ColorTy {
    RED = 0,
    BLACK = 1
};

} // End of the rbt namespace

// In the entire file, type T has to satisfy the following:
// * must have a serialization operator<<(std::ostream&, const T&)
// * must have an STL-compatible <=, <, >, >= operators
// * must have a specialization of the C-compatible comparator returning
// {-1,0,1} utils::Cmp<T>
// * must have a specialization of utils::Hash<T>
// * must have a specialization of utils::Max<T>, if you don't need the
// interval tree functionality, Max<T> can do whatever, but it must
// exist.
// * if interval trees are used, must have a specialization of
// utils::Overlap<T>

template <typename T>
class RedBlackTree;

template <typename T>
class RBNode : public pt::Counted {
    typedef RBNode _Self;

    _Self* children[2];
    T content;
    // The main purpose of having the size field is to be able to
    // quickly check tree equivalence --- trees that have different
    // number of nodes can't be equivalent.
    const unsigned size;
    // Hash has to be stored locally, as constant tree traversals in the
    // constructor would incurr quadratic overhead.
    const int hash;
    // The max field is used only when the RB tree is used as an
    // interval tree.
    const int max;
    // Note: The color field could be merged into the size field (31
    // bits is enough for the tree size) if memory is at stake. This
    // would be a 2nd-order optimization.
    const rbt::ColorTy color;

    void doCheck() const {
#ifdef CHECK_RB_PROP
        check();
#endif
    }

    // The RedBlackTree wrapper needs access to ref.
    template <typename T1>
    friend class RedBlackTree;

public:

    typedef std::vector<_Self*> RBVector;
    typedef std::vector<const _Self*> RBConstVector;

    // *** Constructors ***

    // DANGER: None of these constructors increment the refcount of the
    // created node. This class should be used with extreme care and
    // only if you know what you are doing. If not, use RedBlackTree
    // wrapper instead. Actually, the only reason why constructors were
    // made public was for testing purposes.

    RBNode() : size(0), hash(0), max(0), color(rbt::BLACK) {}

    RBNode(T elem, rbt::ColorTy col = rbt::BLACK) : 
        content(elem), 
        size(1), 
        hash(getHash(elem)),
        max(Max<T>()(elem)),
        color(col) {
            children[0] = 0; children[1] = 0;
        }

    RBNode(T elem, _Self* l, _Self* r, rbt::ColorTy cty = rbt::BLACK) :
        content(elem),
        size((l ? l->treeSize() : 0) + (r ? r->treeSize() : 0) + 1),
        hash(getHash(elem, l, r)),
        max(Max<T>()(elem, l ? l->getMax() : INT_MIN, r ? r->getMax() :
                    INT_MIN)),
        color(cty) {
            if (l) l->ref(); 
            if (r) r->ref();
            children[0] = l; children[1] = r;
        }

    RBNode(_Self* n, _Self* l, _Self* r, rbt::ColorTy cty = rbt::BLACK):
        content(n->get()), 
        size((l ? l->treeSize() : 0) + (r ? r->treeSize() : 0) + 1), 
        hash(getHash(n->get(), l, r)),
        max(Max<T>()(n->get(), l ? l->getMax() : INT_MIN, r ?
                    r->getMax() : INT_MIN)),
        color(cty) {
            if (l) l->ref(); 
            if (r) r->ref();
            children[0] = l; children[1] = r;
        }

    ~RBNode() {
        if (getLeft() && getLeft()->unref()) delete getLeft();
        if (getRight() && getRight()->unref()) delete getRight();
    }

    // *** Essential class interface ***

    bool hasARedChild() const {
        return (getLeft() && getLeft()->isRed()) ||
               (getRight() && getRight()->isRed());
    }

    _Self* getLeft() const { return children[0]; }
    _Self* getRight() const { return children[1]; }
    bool hasLeftChild() const { return getLeft() != 0; }
    bool hasRightChild() const { return getRight() != 0; }
    bool isLeaf() const { return getLeft() == 0 && getRight() == 0; }

    rbt::ColorTy getColor() const { return color; }
    bool isRed() const { return getColor() == rbt::RED; }
    bool isBlack() const { return getColor() == rbt::BLACK; }

    T get() const { return content; }
    unsigned treeSize() const { return size; }
    int getHash() const { return hash; }
    int getMax() const { return max; }
    static int getHash(T elem) { Hash<T> h; return h(elem); }
    // Returns a hash that a new node, with element elem and with
    // children l and r, would have.
    static int getHash(T, _Self*, _Self*);

    _Self* find(T);
    const _Self* find(T) const;
    _Self* find(RBVector&, T);
    _Self* insert(RBVector&, T);
    _Self* erase(RBVector&, T);
    _Self* erase(RBVector&);
    _Self* replace(RBVector&, T, T with);
    
    // These two functions have to be specialized for every type used in
    // the interval tree. Unspecialized versions don't do anything. To
    // get fully functional interval trees, Max<T> must be implemented
    // according to the semantics of interval trees.

    // Returns the first found overlapping node
    const _Self* findFirstOverlapping(T) const;
    // Returns all overlapping nodes
    void findAllOverlapping(T, std::vector<T>&) const;

    // *** Iterators ***

    // Note: The interface currently doesn't provide const_iterator.
    // Nodes themselves are mutable (one has to be able to insert new
    // nodes into the tree), but the only mutable parts are the children
    // pointers, so using the non-const iterator is fairly safe.

    class iterator {
        typedef iterator        _Self;
    public:
        typedef RBNode*         value_type;
        typedef std::forward_iterator_tag iterator_category;
        typedef size_t          difference_type;
        typedef RBNode&         reference;
        typedef RBNode*         pointer;
    private:
        typedef std::vector<pointer> trail_type;
        trail_type trail;
    public:
        // No need for copy ctor, copy assign. Defaults are fine.
        
        explicit iterator() {} // End iterator
        explicit iterator(pointer p) {
            while (p) { trail.push_back(p); p = p->getLeft(); }
        }

        bool operator==(const _Self& B) { return trail == B.trail; }
        bool operator!=(const _Self& B) { return trail != B.trail; }

        _Self& operator++(); /* Preinc */
        _Self operator++(int) { return _Self(++(*this)); } /* Postinc */

        reference operator*() {
            assert(!trail.empty() && "Invalid dereference.");
            return *trail.back();
        }
        reference operator*() const {
            assert(!trail.empty() && "Invalid dereference.");
            return *trail.back();
        }
        pointer operator->() {
            assert(!trail.empty() && 
                "Accessing element of the end iterator?.");
            return trail.back();
        }
        pointer operator->() const {
            assert(!trail.empty() && 
                "Accessing element of the end iterator?.");
            return trail.back();
        }
    };
    
    class const_iterator {
        typedef const_iterator   _Self;
    public:
        typedef const RBNode*   value_type;
        typedef std::forward_iterator_tag iterator_category;
        typedef size_t          difference_type;
        typedef const RBNode&   reference;
        typedef const RBNode*   pointer;
    private:
        typedef std::vector<pointer> trail_type;
        trail_type trail;
    public:
        // No need for copy ctor, copy assign. Defaults are fine.
        
        explicit const_iterator() {} // End iterator
        explicit const_iterator(pointer p) {
            while (p) { trail.push_back(p); p = p->getLeft(); }
        }

        bool operator==(const _Self& B) { return trail == B.trail; }
        bool operator!=(const _Self& B) { return trail != B.trail; }

        _Self& operator++(); /* Preinc */
        _Self operator++(int) { return _Self(++(*this)); } /* Postinc */

        reference operator*() const {
            assert(!trail.empty() && "Invalid dereference.");
            return *trail.back();
        }
        pointer operator->() const {
            assert(!trail.empty() && 
                "Accessing element of the end iterator?.");
            return trail.back();
        }
    };

    iterator begin() { return iterator(this); }
    iterator end() { return iterator(); }
    const_iterator begin() const { return const_iterator(this); }
    const_iterator end() const { return const_iterator(); }

    // *** Debug functions ***

    bool check() const;

    // Dumps a single node in the dot format
    void toDotNode(std::ostream& os) const;
    // Dumps the entire tree in the dot format
    void toDotTree(std::ostream& os) const;
    // Prints out the node
    void print() const { std::cout << get() << std::endl; }

private:

    static void dotPrologue(std::ostream& os) {
        os << "digraph RBTree {" << std::endl;
    }

    static void dotEpilogue(std::ostream& os) {
        os << "}\n";
    }

    _Self* rebuildSpine(RBVector&, _Self*, _Self*);
    _Self* rebuildSpine(RBVector&, _Self*, _Self*, _Self*, _Self*);
    _Self* rebuildAndRebalanceSpine(RBVector&, _Self*, _Self*, _Self*,
            _Self*);
};

// *** Implementation ***

template <typename T>
inline std::ostream& operator<<(std::ostream& os, const RBNode<T>& nd) {
    RBNode<T>* child = nd.getLeft();
    if (child) {
        child->toDotNode(os);
        os << "\t\"" << nd.get() << "\" -> \"" << child->get() << 
            "\" [label=\"<\"];" << std::endl;
        os << *child;
    }
    child = nd.getRight();
    if (child) {
        child->toDotNode(os);
        os << "\t\"" << nd.get() << "\" -> \"" << 
            child->get() << "\" [label=\">\"];" << std::endl;
        os << *child;
    }
    return os;
}

template <typename T>
inline bool RBNode<T>::check() const {
    if (isRed()) { // Root must be black
        return false;
    }
    
    RBConstVector frontier;
    int blackNodesOnCurPath = 0;
    int blackNodesOnAnyPath = -1;
    Cmp<T> cmp;
    
    const _Self* nd = this;
    const _Self* child = 0;
    while (nd) {
        frontier.push_back(nd);

        if (nd->isBlack()) {
            blackNodesOnCurPath++;
        } else if (nd->hasARedChild()) {
            // Red nodes must have black children
            return false;
        }

        if (nd->hasLeftChild()) {
            child = nd->getLeft();
            if (cmp(child->get(), nd->get()) != -1) {
                return false;
            }
            nd = child;
        } else {
            // Got to the end of the left path
            if (blackNodesOnAnyPath == -1) {
                blackNodesOnAnyPath = blackNodesOnCurPath;
            } else if (blackNodesOnCurPath != blackNodesOnAnyPath) {
                return false;
            }

            bool rightChildVisited = false;

            // Find next node that has an unvisited right child
            while (!frontier.empty()) {
                // Proceed with the right child if there is one
                const _Self* leaf = frontier.back();
                if (leaf->hasRightChild() && !rightChildVisited) {
                    child = leaf->getRight();
                    if (cmp(child->get(), leaf->get()) != 1) {
                        return false;
                    }
                    nd = child;
                    break; // Proceed with the outer loop
                }

                rightChildVisited = false;
                frontier.pop_back();
                if (leaf->isBlack()) {
                    assert(blackNodesOnCurPath > 0 && "Cnt underflow.");
                    blackNodesOnCurPath--;
                }

                const _Self* parent = frontier.empty() ? 0 : frontier.back();

                if (!parent) {
                    nd = 0; // Done, will break out of the outer loop
                    break;
                }

                if (parent->getLeft() == leaf) {
                    if (parent->hasRightChild()) {
                        nd = parent->getRight();
                        break; // Proceed with the outer loop
                    }
                } else {
                    assert(parent->getRight() == leaf &&
                            "Unexpected tree shape.");
                    rightChildVisited = true;
                }
            }
        }
    }
    
    return true;
}

template <typename T>
int RBNode<T>::getHash(T elem, _Self* l, _Self* r) {
    // Size of the subtree must not be a part of size, as the same
    // trees can have different structure, depending on the order in
    // which elements were added. Hash function has to be symmetric,
    // as well.
    int h = getHash(elem);
    if (l) h ^= l->getHash();
    if (r) h ^= r->getHash();
    return h;
}

template <typename T>
inline RBNode<T>* RBNode<T>::find(T elem) {
    _Self* nd = this;
    Cmp<T> cmp;

    while (nd) {
        const int cmpResult = cmp(elem, nd->get());
        if (cmpResult == 0) { // Element found
           return nd;
        } else {
            nd = (cmpResult < 0) ? nd->getLeft() : nd->getRight();
        }
    }
    return 0;
}

template <typename T>
inline const RBNode<T>* RBNode<T>::find(T elem) const {
    const _Self* nd = this;
    Cmp<T> cmp;

    while (nd) {
        const int cmpResult = cmp(elem, nd->get());
        if (cmpResult == 0) { // Element found
           return nd;
        } else {
            nd = (cmpResult < 0) ? nd->getLeft() : nd->getRight();
        }
    }
    return 0;
}

template <typename T>
inline RBNode<T>* RBNode<T>::find(RBNode<T>::RBVector& path, T elem) {
    _Self* nd = this;
    Cmp<T> cmp;

    while (nd) {
        path.push_back(nd);
        const int cmpResult = cmp(elem, nd->get());
        if (cmpResult == 0) { // Element found
           return nd;
        } else {
            nd = (cmpResult < 0) ? nd->getLeft() : nd->getRight();
        }
    }
    return 0;
}

template <typename T>
inline RBNode<T>* RBNode<T>::insert(RBNode<T>::RBVector& path, T elem) {
    Cmp<T> cmp;

    // Find where to place the new node
    bool goLeft = false;
    for (_Self* nd = this; nd; ) {
        const int cmpResult = cmp(elem, nd->get());

        if (cmpResult == 0) { // Element found
            return this;
        } else { // Element not found
            path.push_back(nd);
            goLeft = (cmpResult < 0); // 1st arg smaller than 2nd
            nd = goLeft ?  nd->getLeft() : nd->getRight();
        }
    }

    // Node not found. Insert the new node and copy the path (spine).
    assert(!path.empty() && "Invalid path.");
    _Self* parent = path.back();
    path.pop_back();
    _Self* left = 0;
    _Self* right = 0;
    assert(parent && "Unexpected parent NULL ptr.");

    // Resolve red-red conflicts using Okasaki's balancing and Eker's
    // case optimization.
    while (parent->isRed()) {
        assert(!path.empty() && "Red nodes always have black parent.");
        _Self* grandparent = path.back();
        path.pop_back();
        assert(grandparent && "Unexpected grandparent NULL ptr.");

        if (goLeft) {
            if (grandparent->getLeft() == parent) {
                left = new _Self(elem, left, right);
                right = new _Self(grandparent,
                        parent->getRight(), grandparent->getRight());
                elem = parent->get();
            } else {
                assert(grandparent->getRight() == parent && 
                        "Invalid tree structure.");
                left = new _Self(grandparent,
                        grandparent->getLeft(), left);
                right = new _Self(parent, right,
                        parent->getRight());
            }
        } else {
            if (grandparent->getLeft() == parent) {
                left = new _Self(parent, parent->getLeft(),
                        left);
                right = new _Self(grandparent, right,
                        grandparent->getRight());
            } else {
                assert(grandparent->getRight() == parent && 
                        "Invalid tree structure.");
                right = new _Self(elem, left, right);
                left = new _Self(grandparent,
                        grandparent->getLeft(), parent->getLeft());
                elem = parent->get();
            }
        }

        if (path.empty()) { // New root must be black
            return new _Self(elem, left, right);
        } else {
            parent = path.back();
            path.pop_back();
            goLeft = (parent->getLeft() == grandparent);
        }
    }

    // Create a red node
    _Self* n = new _Self(elem, left, right, rbt::RED);
    // Copy the rest of the spine without rebalancing
    _Self* newRoot = goLeft ? 
        new _Self(parent, n, parent->getRight()) :
        new _Self(parent, parent->getLeft(), n);

    return rebuildSpine(path, newRoot, parent);
}

// Ugly, but better than duplicating code.
#define ITERATOR_PREINC(IT) \
template <typename T> \
inline typename RBNode<T>::IT& RBNode<T>::IT::operator++() { \
    /* Preinc */ \
    if (trail.empty()) return *this; \
    pointer p = trail.back(); \
    pointer right = p->getRight(); \
    if (!right) { \
        while (!trail.empty()) { \
            pointer nd = trail.back(); \
            trail.pop_back(); \
            if (!trail.empty()) { \
                pointer parent = trail.back(); \
                if (parent->getLeft() == nd) return *this; \
            } \
        } \
    } \
\
    if (!right) { \
        assert(trail.empty() && \
            "Next nd not found, but trail is not empty?"); \
        return *this; \
    } \
\
    trail.push_back(right); \
    for (pointer n = right->getLeft(); n; n = n->getLeft()) { \
        trail.push_back(n); \
    } \
\
    return *this; \
}

ITERATOR_PREINC(iterator)
ITERATOR_PREINC(const_iterator)

template <typename T>
inline RBNode<T>* RBNode<T>::erase(RBNode<T>::RBVector& path, T elem) {
    _Self* nd = find(path, elem);
    // Return this node as root, if the node is not found.
    return nd ? nd->erase(path) : this;
}

template <typename T>
inline RBNode<T>* RBNode<T>::replace(RBNode<T>::RBVector& path, T
        replace, T with){

    assert(!Equal<T>()(replace, with) &&
            "Trying to replace node with itself?");

    _Self* nd = find(path, replace);
    if (!nd) return 0;

    assert(!path.empty() && "Invalid path.");
    assert(path.back() == nd && "Unexpected path structure.");

    _Self surrogate(with, nd->getLeft(), nd->getRight(),
            nd->getColor());
    return rebuildSpine(path, nd->getLeft(), nd->getLeft(), nd,
            &surrogate);
}

template <typename T>
inline void RBNode<T>::toDotNode(std::ostream& os) const {
    os << "\t\"" << content << "\" [style=filled, fillcolor=" <<
        (isBlack() ? "black, fontcolor=white]" : "red]") << std::endl;
}

template <typename T>
inline void RBNode<T>::toDotTree(std::ostream& os) const {
    dotPrologue(os);
    toDotNode(os);
    os << *this;
    dotEpilogue(os);
}

template <typename T>
inline RBNode<T>* RBNode<T>::erase(RBNode<T>::RBVector& path) {

    assert(!path.empty() && "Unexpected empty victim path.");
    _Self* victim = path.back();
    //victim->print();
    path.pop_back();
    _Self* child = victim->getLeft();

    if (child) {
        _Self* n = victim->getRight();
        if (n) { // The victim has two children
            path.push_back(victim); // Return the victim back on path
            do {
                path.push_back(n);
                n = n->getLeft();
            } while (n);

            _Self* surrogate = path.back();
            path.pop_back();
            assert(surrogate && "Unexpected surrogate NULL ptr.");
            child = surrogate->getRight();

            if (surrogate->isRed()) {
                return rebuildSpine(path, child, surrogate, victim,
                        surrogate);
            } else if (child && child->isRed()) {
                return rebuildSpine(path, new _Self(child,
                            child->getLeft(), child->getRight()),
                        surrogate, victim, surrogate);
            }

            // Increase the black length for the child
            return rebuildAndRebalanceSpine(path, child, surrogate,
                    victim, surrogate);
        }
    } else {
        child = victim->getRight();
    }

    // Victim has no children or one child only

    if (path.empty()) {
        return (child && child->isRed()) ?
            new _Self(child, child->getLeft(),
                    child->getRight()) : child;
    }

    if (victim->isRed()) {
        return rebuildSpine(path, child, victim);
    } else if (child && child->isRed()) {
        return rebuildSpine(path, new _Self(child,
                    child->getLeft(), child->getRight()), victim);
    }

    // Increase black length for the child
    return rebuildAndRebalanceSpine(path, child, victim, 0, 0);
}

// The following two functions have to be specialized for each type that
// requires interval tree functionality.

template <typename T>
inline const RBNode<T>* RBNode<T>::findFirstOverlapping(T elem) const {
    const _Self* nd = this;
    //nd->print();
    //toDotTree(std::cout);
    Overlap<T> olap;
    while (nd && !olap(nd->get(), elem)) {
        _Self* left = nd->getLeft();
        if (left && left->getMax() >= olap.low(elem)) {
            nd = left;
        } else {
            nd = nd->getRight();
        }
    }

    return nd;
}

template <typename T>
inline void RBNode<T>::findAllOverlapping(T elem, std::vector<T>& vec)
    const {
    const _Self* nd = findFirstOverlapping(elem);

    if (nd) {
        assert(Overlap<T>()(nd->get(), elem) && 
            "Found a non-overlapping elem?");
        vec.push_back(nd->get());
        if (nd->getLeft()) {
            nd->getLeft()->findAllOverlapping(elem, vec);
        }
        if (nd->getRight()) {
            nd->getRight()->findAllOverlapping(elem, vec);
        }
    }
}

template <typename T>
inline RBNode<T>* RBNode<T>::rebuildSpine(RBNode<T>::RBVector&
        path, RBNode<T>* nd, RBNode<T>* old) {

    // Rebuild spine
    while (!path.empty()) {
        _Self* parent = path.back();
        path.pop_back();
        _Self* left = parent->getLeft();

        if (left == old) {
            left = nd;
            nd = parent->getRight();
        }

        nd = new _Self(parent, left, nd, parent->getColor());
        old = parent;
    }

    //nd->print();
    return nd;
}

template <typename T>
inline RBNode<T>* RBNode<T>::rebuildSpine(RBNode<T>::RBVector&
        path, RBNode<T>* nd, RBNode<T>* old, RBNode<T>* victim,
        RBNode<T>* surrogate) {

    // Rebuild spine replacing the victim data with the surrogate node
    while (!path.empty()) {
        _Self* parent = path.back();
        path.pop_back();
        _Self* left = parent->getLeft();
        _Self* source = parent;

        if (source == victim) {
            source = surrogate;
        }

        if (left == old) {
            left = nd;
            nd = parent->getRight();
        }

        nd = new _Self(source, left, nd, parent->getColor());
        old = parent;
    }

    return nd;
}

// Rembalancing is done according to Chapter 14 of Thomas H. Cormen,
// Charles E. Leierson and Ronald L. Rivest: Introduction to Algorithms,
// MIT Press and McGraw-Hill, 2000.
template <typename T>
inline RBNode<T>*
RBNode<T>::rebuildAndRebalanceSpine(RBNode<T>::RBVector& path,
    // "old" node is the node that nd is replacing in the tree.
    RBNode<T>* nd, RBNode<T>* old, RBNode<T>* victim, RBNode<T>*
    surrogate) {

    _Self* b = 0;

    assert((!nd || nd->isBlack()) && 
        "Node is supposed to be NULL or black.");

    for (;;) {
        assert(!path.empty() && "Unexpected empty stack.");
        b = path.back(); // old's parent
        path.pop_back();
        assert((b->getLeft() == old || b->getRight() == old) &&
            "Unexpected stack.");

        _Self* b2 = (b == victim) ? surrogate : b;

        if (b->getLeft() == old) {
            _Self* d = b->getRight(); // old nd's siebling
            _Self* c = d->getLeft();
            _Self* e = d->getRight();

            // nd will replace b->getLeft(). Thus, every path from
            // b->getRight() must contain at least 1 black node,
            // meanding that d exists.
            assert(d && "Unexpected NULL ptr.");

            if (d->isRed()) {
                // Case 1; 3 subcases

                // Since d is red, every path from d must contain at
                // least one black node. Thus, c and e exist.
                assert(c && "Unexpected NULL ptr.");
                assert(d && "Unexpected NULL ptr.");

                _Self* p = c->getLeft();
                _Self* q = c->getRight();
                _Self* t = 0;

                if (q && q->isRed()) {
                    // Transformations: 1, 4
                    t = new _Self(c,
                            new _Self(b2, nd, p),
                            new _Self(q, q->getLeft(),
                                q->getRight()), rbt::RED);
                } else if (p && p->isRed()) {
                    // Transformations: 1, 3, 4
                    t = new _Self(p,
                            new _Self(b2, nd, p->getLeft()),
                            new _Self(c, p->getRight(), q), rbt::RED);
                } else {
                    // Transformations: 1, 2
                    t = new _Self(b2, nd,
                            new _Self(c, p, q, rbt::RED));
                }

                nd = new _Self(d, t, e);
                break;
            } else { // d is black

                if (e && e->isRed()) {   // Case 4
                    nd = new _Self(d,
                            new _Self(b2, nd, c),
                            new _Self(e, e->getLeft(), e->getRight()),
                            b->getColor());
                } else if (c && c->isRed()) { // Case 3
                    nd = new _Self(c,
                            new _Self(b2, nd, c->getLeft()),
                            new _Self(d, c->getRight(), e),
                            b->getColor());
                } else {                 // Case 2
                    nd = new _Self(b2, nd, 
                            new _Self(d, c, e, rbt::RED));
                    goto climbUp;
                }

                break;
            }
        } else { // Dual of the previous branch
            _Self* d = b->getLeft(); // old nd's siebling
            _Self* c = d->getRight();
            _Self* e = d->getLeft();

            // nd will replace b->getRight(). Thus, every path from
            // b->getLeft() must contain at least 1 black node,
            // meanding that d exists.
            assert(d && "Unexpected NULL ptr.");

            if (d->isRed()) {
                // Case 1; 3 subcases

                // Since d is red, every path from d must contain at
                // least one black node. Thus, c and e exist.
                assert(c && "Unexpected NULL ptr.");
                assert(d && "Unexpected NULL ptr.");

                _Self* p = c->getRight();
                _Self* q = c->getLeft();
                _Self* t = 0;

                if (q && q->isRed()) {
                    // Transformations: 1, 4
                    t = new _Self(c,
                            new _Self(q, q->getLeft(), q->getRight()),
                            new _Self(b2, p, nd),
                            rbt::RED);
                } else if (p && p->isRed()) {
                    // Transformations: 1, 3, 4
                    t = new _Self(p,
                            new _Self(c, q, p->getLeft()),
                            new _Self(b2, p->getRight(), nd),
                            rbt::RED);
                } else {
                    // Transformations: 1, 2
                    t = new _Self(b2, new _Self(c, q, p, rbt::RED), nd);
                }

                nd = new _Self(d, e, t);
                break;
            } else { // d is black
                if (e && e->isRed()) {        // Case 4
                    nd = new _Self(d,
                            new _Self(e, e->getLeft(), e->getRight()),
                            new _Self(b2, c, nd),
                            b->getColor());
                } else if (c && c->isRed()) { // Case 3
                    nd = new _Self(c,
                            new _Self(d, e, c->getLeft()),
                            new _Self(b2, c->getRight(), nd),
                            b->getColor());
                } else {                      // Case 2
                    nd = new _Self(b2, new _Self(d, e, c, rbt::RED),
                            nd);
                    goto climbUp;
                }

                break;
            }
        }

climbUp:
        if (path.empty()) {
            return nd;
        }

        assert(b && "Unexpected NULL ptr.");
        if (b->isRed()) {
            return rebuildSpine(path, nd, b, victim, surrogate);
        } else {
            old = b;
        }
    }

    if (path.empty()) {
        return nd;
    } else {
        return rebuildSpine(path, nd, b, victim, surrogate);
    }
}


} // End of the utils namespace

#endif // RBNODE_H
