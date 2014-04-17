#include "AbsDomStridedInterval.h"

using namespace absdomain;
using namespace std;

int main(int, char**) {
    StridedIntervalPtr i1 = StridedInterval::get(4,8,4);
    StridedIntervalPtr i2 = StridedInterval::get(8,12,4);
    cout << *i1 << " join " << *i2 << " = " << *(i1->join(*i2)) << endl;
    cout << *i1 << " meet " << *i2 << " = " << *(i1->meet(*i2)) << endl;
    cout << *i1 << " widen " << *i2 << " = " << *(i1->widen(*i2)) <<
        endl;
    StridedIntervalPtr i3 = StridedInterval::get(0,12,4);
    cout << *i1 << " <= " << *i3 << " = " << (*i1 <= *i3) << endl;
    StridedIntervalPtr i4 = StridedInterval::get(12,16,2);
    cout << *i3 << " + " << *i4 << " = " << *(*i3 + *i4) << endl;
    exit(0);
}
