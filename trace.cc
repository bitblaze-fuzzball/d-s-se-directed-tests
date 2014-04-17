#include <stdio.h>
#include <assert.h>
#include <list>
#include <fstream>

#include "debug.h"
#include "trace.h"

// TODO: 
// * Add iterator to traverse the instructions in the trace
// * Compress trace

Trace::Trace() {
    ;
}

Trace::~Trace() {
    ;
}

void Trace::append(unsigned int base, int size) {
    t.push_back(pair<unsigned int, int>(base, size));
}

unsigned int Trace::size() {
    return t.size();
}

// Local Variables: 
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
