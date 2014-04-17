#include "RedBlackTree.h"

using namespace utils;

typedef RBNode<int> IRBNd;
typedef boost::intrusive_ptr<IRBNd> RbnodePtr;

typedef RedBlackTree<int> RBTree;
typedef boost::intrusive_ptr<RBTree> RbtreePtr;

int main(int, char**) {
    IRBNd::RBVector stack;
    RbnodePtr root = RbnodePtr(new IRBNd(7));

    root = root->insert(stack, 47);
    stack.clear();
    root = root->insert(stack, 20);
    stack.clear();
    root = root->insert(stack, 35);
    stack.clear();
    root = root->insert(stack, 28);
    stack.clear();
    root = root->insert(stack, 23);
    stack.clear();
    root = root->insert(stack, 15);
    stack.clear();
    root = root->insert(stack, 16);
    stack.clear();
    root = root->insert(stack, 30);
    stack.clear();
    root = root->insert(stack, 17);
    stack.clear();
    root = root->insert(stack, 26);
    stack.clear();
    root = root->insert(stack, 41);
    stack.clear();
    root = root->insert(stack, 10);
    stack.clear();
    root = root->insert(stack, 14);
    stack.clear();
    root = root->insert(stack, 3);
    stack.clear();
    root = root->insert(stack, 19);
    stack.clear();
    root = root->insert(stack, 21);
    stack.clear();
    root = root->insert(stack, 12);
    stack.clear();
    root = root->insert(stack, 28);
    stack.clear();
    root = root->insert(stack, 39);
    stack.clear();
    
    assert(root->check() && "Invalid tree.");
    root->toDotTree(std::cout);
    std::cout << std::flush;

    std::cout << std::endl;
    std::cout << std::endl;
    std::cout << std::endl;
    for (IRBNd::iterator I = root->begin(), E = root->end(); I != E;
            ++I) {
        std::cout << I->get() << " ";
    }
    std::cout << std::endl;

    root = root->erase(stack, 41);
    stack.clear();
    root = root->erase(stack, 20);
    stack.clear();
    root = root->erase(stack, 17);
    stack.clear();
    root = root->erase(stack, 28);
    stack.clear();
    root = root->erase(stack, 23);
    stack.clear();
    root = root->erase(stack, 15);
    stack.clear();
    root = root->erase(stack, 16);
    stack.clear();
    root = root->erase(stack, 26);
    stack.clear();
    root = root->erase(stack, 47);
    stack.clear();
    root = root->erase(stack, 10);
    stack.clear();
    root = root->erase(stack, 21);
    stack.clear();
    root = root->erase(stack, 14);
    stack.clear();
    root = root->erase(stack, 3);
    stack.clear();
    root = root->erase(stack, 19);
    stack.clear();
    root = root->erase(stack, 12);
    stack.clear();
    root = root->erase(stack, 28);
    stack.clear();
    root = root->erase(stack, 35);
    stack.clear();
    root = root->erase(stack, 39);
    stack.clear();
    root = root->erase(stack, 30);
    stack.clear();

    assert(root->check() && "Invalid tree.");

    // Remove the last node
    root = root->erase(stack, 7);
    stack.clear();

    assert(!root && "Last node not properly erased.");

    // Now, try the same exercise with the RedBlackTree wrapper.
    RbtreePtr t = RBTree::get(7);
    t = t->insert(47);
    t = t->insert(20);
    t = t->insert(35);
    t = t->insert(28);
    t = t->insert(23);
    t = t->insert(15);
    t = t->insert(16);
    t = t->insert(30);
    t = t->insert(17);
    t = t->insert(26);
    t = t->insert(41);
    t = t->insert(10);
    t = t->insert(14);
    t = t->insert(3);
    t = t->insert(19);
    t = t->insert(21);
    t = t->insert(12);
    t = t->insert(39);
    t->check();

    t = t->erase(41);
    t = t->erase(20);
    t = t->erase(17);
    t = t->erase(28);
    t = t->erase(23);
    t = t->erase(15);
    t = t->erase(16);
    t = t->erase(26);
    t = t->erase(47);
    t = t->erase(10);
    t = t->erase(21);
    t = t->erase(14);
    t = t->erase(3);
    t = t->erase(19);
    t = t->erase(12);
    t = t->erase(28);
    t = t->erase(35);
    t = t->erase(39);
    t = t->erase(30);
    t->check();
    t = t->erase(7);

    RBTree::shutdown();
}

template <> THREADLOCAL RedBlackTree<int>::CacheTy*
RedBlackTree<int>::cache(0);
template <> THREADLOCAL RedBlackTree<int>::Rbnode::RBVector* 
RedBlackTree<int>::stack(0);
template<> THREADLOCAL int RedBlackTree<int>::opcounter(0);
template<> THREADLOCAL bool RedBlackTree<int>::doCache(true);
