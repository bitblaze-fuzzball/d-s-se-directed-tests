#include "AbsRegion.h"
#include "Rand.h"
#include "debug.h"
#include "types.h"
#include <cstdio>

#define MAXITER 100
#define RNDLIMIT 2000
#define STRDLIMIT 256

using namespace absdomain;

typedef ValueSet<StridedInterval> ValSet;
typedef Region<StridedInterval> RegionTy;
typedef boost::intrusive_ptr<RegionTy> RegionPtr;
typedef boost::intrusive_ptr<ValSet> VSetPtr;
typedef std::pair<StridedIntervalPtr, VSetPtr> ContentPair;

int DEBUG_LEVEL = 2;
int ASSERT_LEVEL = 3;
FILE* DEBUG_FILE = stderr;
addr_t current_instruction;
addr_t last_program_instruction() { return 0; }
void __warning(addr_t, addr_t) {;}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: <executable> #of_iterations" << std::endl;
        exit(EXIT_FAILURE);
    }
    utils::rnd::init(2039281);
    const int maxiter = std::atoi(argv[1]);
    StridedIntervalPtr limits = StridedInterval::get(0,100000,1);
    RegionPtr r2 = RegionTy::getFresh(rg::WEAK_HEAP, limits);
    
    ValSet::VSetPtr bot = ValSet::getBot();
    ValSet::VSetPtr top = ValSet::getTop();
    std::cout << *bot << " " << *top << std::endl;
    StridedIntervalPtr sitop = StridedInterval::getTop();
    StridedIntervalPtr sibot = StridedInterval::getBot();
    StridedIntervalPtr adr1 = StridedInterval::get(100000,100004,4);

    RegionPtr r0 = RegionTy::getFresh();
    RegionPtr r1 = RegionTy::getFresh(rg::STRONG_HEAP, limits);
    RegionPtr r3 = RegionTy::getFresh(rg::STRONG_STACK,limits);

    // T0: Subsumption checks
    assert(adr1->subsumes(*sibot) && "Bot not subsumed?");
    assert(!sibot->subsumes(*adr1) && "Bot subsumes something else?");
    assert(sitop->subsumes(*adr1) && 
            "Top doesn't subsume something else?");
    assert(sitop->subsumes(*sibot) && "Top doesn't subsume bot?");
    assert(!sibot->subsumes(*sitop) && "Bot subsumes top?");
    assert(!adr1->subsumes(*sitop) && "Something else subsumes top?");

    // T1: read/write out of bounds
    VSetPtr res1 = r1->read(adr1);
    assert(!res1.get() && "Unexpected result of read out of bounds.");
    RegionPtr r = r1->write(adr1, top);
    assert(r == r1 && "Unexpected result of write out of bounds.");

    // T2: double read/write to the same location with different values
    StridedIntervalPtr adr2 = StridedInterval::get(4,20,4);
    StridedIntervalPtr adr3 = StridedInterval::get(24,32,2);
    StridedIntervalPtr cns1 = StridedInterval::get(5,6,1);
    StridedIntervalPtr cns2 = StridedInterval::get(2,10,2);
    StridedIntervalPtr cns3 = StridedInterval::get(24,60,4);
    StridedIntervalPtr rng1 = StridedInterval::get(6,13,1);
    r = r->write(adr2, std::make_pair(r0,cns1));
    r = r->write(adr2, std::make_pair(r0,cns1));
    std::cout << *r << std::endl;

    // T3: double write to the same address
    r = r->write(adr3, std::make_pair(r1,rng1));
    r = r->write(adr3, std::make_pair(r1,rng1));
    std::cout << *r << std::endl;

    // T4: read followed by write returns the same result
    //utils::Cmp<ContentPair> cmp;
    VSetPtr vsp1 = r->read(adr3);
    assert(*vsp1 == *ValSet::get(std::make_pair(r1,rng1))
            && "Unexpected value read back.");

    // T5: operations on valsets
    StridedIntervalPtr si1 = StridedInterval::get(40,48,4);
    VSetPtr vsp2 = vsp1->insert(std::make_pair(r3,si1));
    assert(*vsp2->erase(std::make_pair(r3,si1)) == *vsp1 &&
            "Unexpected result of erase.");
    assert(vsp2->subsumes(*vsp1) && "Unexpected result.");
    assert(vsp1->subsumedBy(*vsp2) && "Unexpected result.");
    // Note that adding a non-multiple of stride will cause problems,
    // that's fine.
    VSetPtr vsp3 = *vsp2 + 4;
    std::cout << "Before: " << *vsp2 << std::endl;
    std::cout << "After+: " << *vsp3 << std::endl;
    VSetPtr vsp4 = vsp2->meet(*vsp3);
    VSetPtr vsp5 = vsp2->join(*vsp3);
    VSetPtr vsp6 = vsp2->widen(*vsp3);
    std::cout << "Meet: " << *vsp4 << std::endl;
    std::cout << "Join: " << *vsp5 << std::endl;
    std::cout << "Widen: " << *vsp6 << std::endl;
    VSetPtr vsp7 = *vsp2 | *vsp3;
    VSetPtr vsp8 = *vsp2 & *vsp3;
    VSetPtr vsp9 = *vsp2 ^ *vsp3;
    std::cout << "OR : " << *vsp7 << std::endl;
    std::cout << "AND: " << *vsp8 << std::endl;
    std::cout << "XOR: " << *vsp9 << std::endl;
    VSetPtr vsp10 = ValSet::get(std::make_pair(r0,cns1));
    VSetPtr vsp11 = *vsp2 | *vsp10;
    VSetPtr vsp12 = *vsp2 & *vsp10;
    VSetPtr vsp13 = *vsp2 ^ *vsp10;
    std::cout << "--- " << *vsp2 << " , " << *vsp10 << " ---" <<
        std::endl;
    std::cout << "OR : " << *vsp11 << std::endl;
    std::cout << "AND: " << *vsp12 << std::endl;
    std::cout << "XOR: " << *vsp13 << std::endl;
    std::cout << *(*vsp11 + *vsp10) << std::endl;
    std::cout << *(*vsp12 - *vsp10) << std::endl;

    // T6: operations on TOP/BOT
    std::cout << "------- BOT:BOT ---------" << std::endl;
    std::cout << *(*bot - *bot) << std::endl;
    std::cout << *(*bot + *bot) << std::endl;
    std::cout << *(*bot | *bot) << std::endl;
    std::cout << *(*bot & *bot) << std::endl; 
    std::cout << *(*bot ^ *bot) << std::endl; 
    std::cout << *bot->join(*bot) << std::endl;
    std::cout << *bot->meet(*bot) << std::endl; 
    std::cout << *bot->widen(*bot) << std::endl;

    std::cout << "------- BOT:TOP ---------" << std::endl;
    std::cout << *(*bot - *top) << std::endl;  // TOP
    std::cout << *(*bot + *top) << std::endl;  // TOP
    std::cout << *(*bot | *top) << std::endl;  // TOP
    std::cout << *(*bot & *top) << std::endl;  // BOT
    std::cout << *(*bot ^ *top) << std::endl;  // TOP
    std::cout << *bot->join(*top) << std::endl; // TOP
    std::cout << *bot->meet(*top) << std::endl; // BOT
    std::cout << *bot->widen(*top) << std::endl;// TOP

    std::cout << "------- TOP:BOT ---------" << std::endl;
    std::cout << *(*top - *bot) << std::endl; // TOP
    std::cout << *(*top + *bot) << std::endl; // TOP
    std::cout << *(*top | *bot) << std::endl; // TOP
    std::cout << *(*top & *bot) << std::endl; // BOT
    std::cout << *(*top ^ *bot) << std::endl; // TOP
    std::cout << *top->join(*bot) << std::endl;// TOP
    std::cout << *top->meet(*bot) << std::endl;// BOT
    std::cout << *top->widen(*bot) << std::endl;//BOT

    std::cout << "------- TOP:TOP ---------" << std::endl;
    std::cout << *(*top - *top) << std::endl;
    std::cout << *(*top + *top) << std::endl;
    std::cout << *(*top | *top) << std::endl;
    std::cout << *(*top & *top) << std::endl;
    std::cout << *(*top ^ *top) << std::endl;
    std::cout << *top->join(*top) << std::endl;
    std::cout << *top->meet(*top) << std::endl;
    std::cout << *top->widen(*top) << std::endl;

    // T7: reading/writing TOP/BOT
    r = r->write(sitop, std::make_pair(r1,rng1));
    r = r->write(sibot, std::make_pair(r1,rng1));
    vsp1 = r->read(sitop);
    vsp1 = r->read(sibot);
    
    // T8: misaligned reads/writes
    std::cout << "--- Misaligned reads/writes ---" << std::endl;
    StridedIntervalPtr si2 = StridedInterval::get(40,80,4);
    StridedIntervalPtr si3 = StridedInterval::get(34,46,2);
    StridedIntervalPtr si4 = StridedInterval::get(48,58,2);
    StridedIntervalPtr si5 = StridedInterval::get(43,51,1);
    StridedIntervalPtr si6 = StridedInterval::get(32,64,4);
    StridedIntervalPtr si7 = StridedInterval::get(32,64,4);
    std::cout << "    " << *r << std::endl;

    RegionPtr rp = r->write(si2, std::make_pair(r1, cns2));
    std::cout << "#######################################" << std::endl;
    std::cout << "W1: " << *rp << std::endl;
    RegionPtr r4 = rp->write(si3, std::make_pair(r2, cns2));
    std::cout << "W2: " << *r4 << std::endl;
    std::cout << "#######################################" << std::endl;
    std::cout << "W1: " << *rp << std::endl;
    RegionPtr r5 = rp->write(si4, std::make_pair(r2, cns2));
    std::cout << "W3: " << *r5 << std::endl;
    std::cout << "#######################################" << std::endl;
    std::cout << "W1: " << *rp << std::endl;
    RegionPtr r6 = rp->write(si5, std::make_pair(r2, cns2));
    std::cout << "W3: " << *r6 << std::endl;
    std::cout << "#######################################" << std::endl;
    RegionPtr rr = r->write(si3, std::make_pair(r4, cns3));
    rr = rr->write(si4, std::make_pair(r2, cns2));
    std::cout << "W3: " << *rr << std::endl;
    RegionPtr r7 = rr->write(si5, std::make_pair(r2, cns2));
    std::cout << "W4: " << *r7 << std::endl;
    std::cout << "#######################################" << std::endl;
    std::cout << "W4: " << *rr << std::endl;
    RegionPtr r8 = rr->write(si7, std::make_pair(r1,cns3));
    std::cout << "W5: " << *r8 << std::endl;
    std::cout << "#######################################" << std::endl;
    StridedIntervalPtr si8 = StridedInterval::get(64,80,8);
    StridedIntervalPtr si9 = StridedInterval::get(83,87,1);
    StridedIntervalPtr si10 = StridedInterval::get(88,100,4);
    StridedIntervalPtr si11 = StridedInterval::get(52,90,2);
    std::cout << "W5: " << *rr << std::endl;
    RegionPtr r9 = rr->write(si8, std::make_pair(r0,cns1));
    RegionPtr r10 = r9->write(si9, std::make_pair(r2,cns2));
    RegionPtr r11 = r10->write(si10, std::make_pair(r3,cns3));
    std::cout << "W6: " << *r11 << std::endl;
    RegionPtr r12 = r10->write(si11, std::make_pair(r1,adr2));
    std::cout << "W7: " << *r12 << std::endl;
    RegionPtr r13 = r10->getWeaklyUpdatable();
    RegionPtr r14 = r13->write(si11, std::make_pair(r1,cns3));
    std::cout << "W8: " << *r14 << std::endl;

    // T9: performance stress test
    StridedIntervalPtr si12, si13;
    RegionPtr r15 = RegionTy::getFresh(rg::STRONG_HEAP);
    RegionPtr r16 = RegionTy::getFresh(rg::WEAK_HEAP);
    for (int i = 0; i < maxiter; i++) {
skip_zero_stride:
        si13 = StridedInterval::getRand(RNDLIMIT, STRDLIMIT);
        si12 = StridedInterval::getRand(RNDLIMIT, STRDLIMIT);
        if (si12->getStride() == 0) {
            goto skip_zero_stride;
        }
        if (!RegionTy::checkWrite(si12, std::make_pair(r2,si13))) {
            goto skip_zero_stride;
        }
        r15 = r15->write(si12, std::make_pair(r2, si13));
        if (i+1==maxiter) {
            std::cout << "Fragments: " << r15->getSize() << std::endl;
        }
        r16 = r16->write(si12, std::make_pair(r2, si13));
    }

    // Test of deallocation (run with valgrind, it should report 0
    // leaks)
    RegionTy::shutdown();
    ValSet::shutdown();
    StridedInterval::shutdown();
    utils::rnd::shutdown();
};
