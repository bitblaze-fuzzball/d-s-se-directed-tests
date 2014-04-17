#ifndef __SERIALIZE_H__
#define __SERIALIZE_H__

#include "cfg.h"
#include "callgraph.h"
#include "func.h"
#include "prog.h"
#include <map>

void unserialize(const char *, Prog &, CallGraph *&, std::map<unsigned
        int, Function *> &);
void serialize(const char *, const Prog &);

#endif // !__SERIALIZE_H__

// Local Variables: 
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
