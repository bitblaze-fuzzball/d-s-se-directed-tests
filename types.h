#ifndef __TYPES_H__
#define __TYPES_H__

#include <set>
#include <vector>
#include <map>

class BasicBlock;
class Function;
class Instruction;
class Cfg;
class CallGraph;

typedef unsigned int addr_t;
typedef unsigned char byte_t;
typedef std::map<addr_t, Function *> functions_map_t;

#if 0
#warning "Extreme DANGER! These associative containers are sorted " \
    "by pointers and will cause non-determinism, sooner-or-later. " \
    "This is a BIG NO-NO."
#endif

typedef std::set<Function *> functions_t;
typedef std::set<BasicBlock *> basicblocks_t;
typedef std::vector<Instruction *> instructions_t;
typedef std::pair<addr_t, addr_t> range_t;

#endif
