#ifndef __FUNC_H__
#define __FUNC_H__

#include "cfg.h"
#include <set>
#include <map>

class Cfg;
class Function;
class Prog;

class Function {
private:
    Cfg          *cfg;
    Prog         *prog;
    std::string  name;
    std::string  module;
    std::string  prototype;
    addr_t       address;
    size_t       size;
    int          argumentsno;
    
    bool pending;

    friend class boost::serialization::access;
    template<class Archive>
	void serialize(Archive & ar, const unsigned int version) {
	ar & address;
	ar & size;
	ar & name;
	ar & module;
	ar & cfg;
	(void)version;
    }

    void guessArgumentsNo();

public:
    Function() {argumentsno = -1; cfg = NULL; pending = false; prog = NULL;}
    Function(addr_t a);
    Function(std::string n, addr_t a, size_t l, std::string m);
    ~Function();

    const char *getName() const;
    const char *getModule() const;
    Cfg *getCfg();
    addr_t getAddress() const;
    size_t getSize() const;
    int getArgumentsNo();
    
    void setSize(size_t s) { size = s; }
    void setModule(const char *m) { module = m; }
    void setName(const char *n) { name = n; }
    void setAddress(addr_t a) { address = a; }

    bool isPending() { return pending; }
    void setPending(bool b) { pending = b; }

    void setProg(Prog *p) { prog = p; }
    Prog *getProg() { assert(prog); return prog; }
};

#endif

// Local Variables: 
// mode: c++
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
