#ifndef __PROG_H__
#define __PROG_H__

#include "types.h"
#include "callgraph.h"
#include <list>
#include <vector>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/vector.hpp>

#define NARGV_PTRS_DEFAULT 0x10
#define NARGV_STRLEN_DEFAULT 0x10

class Section;
class Prog;
class CallGraph;
class Module;

typedef std::list<Section *> sections_t;
typedef std::list<Module *> modules_t;
typedef std::vector<byte_t> bytes_t;

class Section {
private:
    bytes_t bytes;
    addr_t address;
    size_t size;
    unsigned int flags;
    bool allocated;
    std::string name;
    
    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version) {
	ar & address;
	ar & size;
	ar & flags;
	ar & bytes;
	ar & name;
	ar & allocated;
	(void)version;
    }

    Section() {;}

    friend class Prog;

public:
    Section(addr_t a, byte_t * b, size_t s, unsigned int f, const char *n) {
	address = a;
	size = s;
	allocated = b != NULL;
	if (b) {
	    for (size_t i = 0; i < s; i++) {
		bytes.push_back(b[i]);
	    }
	}
	flags = f;
	name = n;
    }

    ~Section() {;}

    size_t getSize() {
	return size;
    }

    addr_t getAddress() {
	return address;
    }

    const char *getName() const {
        return name.c_str();
    }

    byte_t getByte(addr_t a) const {
	assert(a >= address && a < address + size);
	return bytes[a - address];
    }

    bytes_t::const_iterator bytes_begin() {
	return bytes.begin();
    }

    bytes_t::const_iterator bytes_end() {
	return bytes.end();
    }

    unsigned int getFlags() {
	return flags;
    }

    bool isAllocated() {
	return allocated;
    }
};

class Module {
private:
    addr_t address;
    size_t size;
    std::string name;
    bool mainexe;

    friend class Prog;

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version) {
	ar & address;
	ar & size;
	ar & mainexe;
	ar & name;
	(void)version;
    }

public:
    Module() {;}

    Module(addr_t a, size_t s, const char *n, bool m) {
	address = a; size = s; name = n;
	mainexe = m;
    }
    ~Module() {;}
};

class Prog {
private:
    CallGraph callgraph;
    sections_t sections;
    modules_t modules;
  
    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version) {
	ar & sections;
	ar & modules;
	ar & callgraph;
	(void)version;
    }

public:
    Prog() {;}

    ~Prog() {
	for (sections_t::iterator sit = sections.begin(); sit != sections.end();
	     sit++) {
	    delete *sit;
	}
	for (modules_t::iterator mit = modules.begin(); mit != modules.end();
	     mit++) {
	    delete *mit;
	}
    }

    void addModule(addr_t a, size_t s, const char *n, bool m) {
	modules.push_back(new Module(a, s, n, m));
    }

    void addSection(addr_t a, byte_t * b, size_t s, unsigned int f, 
		    const char *n) {
	sections.push_back(new Section(a, b, s, f, n));
    }

    sections_t::const_iterator sec_begin() const { 
	return sections.begin();
    }

    sections_t::const_iterator sec_end() const { 
	return sections.end();
    }

    CallGraph *getCallGraph() {
	return &callgraph;
    }

    /* Because the stack has its own memory region, it shouldn't be
       important for correct static analysis that offsets within the
       stack region match up with the concrete addresses used for the
       stack when a program is executing regularly. Therefore the
       historical behavior is to start the stack offsets from 0; this
       means that most stack offsets will be small negative numbers.
       For debugging, it can also be useful to set the initial stack
       offset to match the initial stack offset that a program has in
       concrete execution, so that stack operations can easily be
       compared. */
    static addr_t getDefaultInitialStackPtr() {
	// Arbitrary, but typical for 32-bit Linux/x86 with a 32-bit kernel
        // return 0xbfffc420;
	// Example of an initial stack address in main() from an
	// execution under FuzzBALL:
        // return 0xbfffcf78;
	// Historical default:
	return 0;
    }

    /* In real execution, the argument pointers and the argument
       strings are stored in higher (older) parts of the stack section
       that used for stack frames. Currently the static analysis
       instead creates and argv region as a global region; the
       important thing there is probably just that its address not
       overlap with any of the program's static code or data
       areas.  */
    static addr_t getDefaultArgvAddress() {
	// Typically arguments are stored somewhat higher on the stack
	// than the initial stack pointer
        return getDefaultInitialStackPtr() + 0x1000;
    }

    void createSection(const addr_t a, const char* name, 
            size_t Nobjs = NARGV_PTRS_DEFAULT, size_t objLen =
            NARGV_STRLEN_DEFAULT) {
        const size_t ptrSize = sizeof(size_t);
        const size_t ptrArraySize = Nobjs * ptrSize;
        byte_t* mem = new byte_t[ptrArraySize];

        for (size_t i = 0; i < Nobjs; i++) {
            addr_t objAdr = a + ptrArraySize + i * objLen;
            for (size_t j = 0; j < ptrSize; j++) {
                const byte_t data = (byte_t)(objAdr & 0xFF);
                mem[i * ptrSize + j] = data;
                objAdr >>= CHAR_BIT;
            }
        }
        addSection((addr_t)a, mem, (size_t)ptrArraySize, (unsigned)3,
                name);
        delete[] mem;
    }

    addr_t getBase(addr_t a) {
	for (modules_t::iterator mit = modules.begin(); mit != modules.end();
	     mit++) {
	    if (a >= (*mit)->address && a < (*mit)->address + (*mit)->size) {
		return (*mit)->address;
	    }
	}
	assert(0);
    }

    bool isPlt(addr_t a) {
	for (modules_t::iterator mit = modules.begin(); mit != modules.end();
	     mit++) {
	    Module *m = *mit;
	    if (a >= m->address && a < m->address + m->size) {
		for (sections_t::iterator sit = sections.begin(); 
		     sit != sections.end(); sit++) {
		    Section *s = *sit;
		    if (a >= s->address && a < s->address + s->size) {
			if (s->name == std::string(".plt@") + m->name) {
			    return true;
			} else {
			    return false;
			}
		    }
		}
		assert(0);
	    }
	}
	assert(0);
    return false;
    }

    addr_t getBase(addr_t a, const char *n) {
	for (modules_t::iterator mit = modules.begin(); mit != modules.end();
	     mit++) {
	    Module *m = *mit;
	    if (a >= m->address && a < m->address + m->size) {
		// Module found
		for (sections_t::iterator sit = sections.begin(); 
		     sit != sections.end(); sit++) {
		    Section *s = *sit;
		    if (s->name == std::string(n) + std::string("@") + 
			m->name) {
			// Section found
			return s->address;
		    }
		}
		assert(0);
	    }
	}
	assert(0);
    return 0;
    }

    std::string tostring() {
	std::string r = "";
	for (modules_t::iterator mit = modules.begin(); mit != modules.end();
	     mit++) {
	    char tmp[512];
	    sprintf(tmp, "\t[+] %.8x-%.8x     %s\n", (*mit)->address, 
		    (*mit)->address + (*mit)->size - 1, (*mit)->name.c_str());
	    r += tmp;

	    for (sections_t::iterator sit = sections.begin(); 
		 sit != sections.end(); sit++) {
		if ((*sit)->address >= (*mit)->address && 
		    (*sit)->address + (*sit)->size <= 
		    (*mit)->address + (*mit)->size) {
		    sprintf(tmp, "\t\t[-] %.8x-%.8x     %s\n", (*sit)->address, 
			    (*sit)->address + (*sit)->size - 1, 
			    (*sit)->name.c_str());
		    r += tmp;
		}
	    }
	}

	return r;
    }

    size_t guessGlobalSize() {
        size_t s = 0;
        for (sections_t::iterator sit = sections.begin(); 
             sit != sections.end(); ++sit) {
            if (strstr((*sit)->name.c_str(), ".data@")) {
                s += (*sit)->size;
            }
        }

        return s;
    }
};

// Local Variables: 
// mode: c++
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:

#endif
