#include "AbsRegion.h"

using namespace absdomain;
using namespace utils;

#define ABSREG Region<StridedInterval>

template<> THREADLOCAL int ABSREG::counter(3);
template<> THREADLOCAL const memmap::MemMap* ABSREG::mmap(0);

#define ABSDOM ValueSet<StridedInterval>
#define ABSDOMPTR boost::intrusive_ptr<ABSDOM >
#define CPLXTYPE \
std::pair<boost::intrusive_ptr<Region<StridedInterval> >, \
          boost::intrusive_ptr<StridedInterval> >

template<> THREADLOCAL Multimap<int, ABSDOMPTR >*
    AbstractDomain<ABSDOM>::cache(0);

template<> THREADLOCAL 
Multimap<int, boost::intrusive_ptr<RedBlackTree<CPLXTYPE > > >*
    RedBlackTree<CPLXTYPE >::cache(0);

template<> THREADLOCAL RedBlackTree<CPLXTYPE >::Rbnode::RBVector*
RedBlackTree<CPLXTYPE >::stack(0);

template<> THREADLOCAL int RedBlackTree<CPLXTYPE >::opcounter(0);
template<> THREADLOCAL bool RedBlackTree<CPLXTYPE >::doCache(true);

#undef ABSREG
#undef ABSDOM
#undef ABSDOMPTR
#undef CPLXTYPE

#define CPLXTYPE std::pair<boost::intrusive_ptr<StridedInterval>, \
boost::intrusive_ptr<ValueSet<StridedInterval> > >

template<> THREADLOCAL 
Multimap<int, boost::intrusive_ptr<RedBlackTree<CPLXTYPE > > >*
    RedBlackTree<CPLXTYPE>::cache(0);

template<> THREADLOCAL RedBlackTree<CPLXTYPE >::Rbnode::RBVector*
RedBlackTree<CPLXTYPE >::stack(0);

template<> THREADLOCAL int RedBlackTree<CPLXTYPE >::opcounter(0);
template<> THREADLOCAL bool RedBlackTree<CPLXTYPE >::doCache(true);

#undef CPLXTYPE
#define CPLXTYPE boost::intrusive_ptr<Region< \
    StridedInterval> >

template<> THREADLOCAL 
Multimap<int, boost::intrusive_ptr<RedBlackTree<CPLXTYPE > > >*
    RedBlackTree<CPLXTYPE>::cache(0);

template<> THREADLOCAL RedBlackTree<CPLXTYPE >::Rbnode::RBVector*
RedBlackTree<CPLXTYPE >::stack(0);

template<> THREADLOCAL int RedBlackTree<CPLXTYPE >::opcounter(0);
template<> THREADLOCAL bool RedBlackTree<CPLXTYPE >::doCache(true);

#undef CPLXTYPE
