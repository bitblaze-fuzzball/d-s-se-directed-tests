#ifndef __WARNING_H__
#define __WARNING_H__

extern addr_t current_instruction;
addr_t last_program_instruction();
void __enter_function(addr_t caller, const std::string);
void __exit_function();
void __warning(addr_t, addr_t);

#define WARNING(msg, ...) {						\
  if (current_instruction != 0) {					\
    fprintf(DEBUG_FILE, "%s ### %.8x ### %.8x\n", msg,			\
	    current_instruction, last_program_instruction());		\
    if (strstr(msg, "Write out of bounds")) {				\
      __warning(current_instruction, last_program_instruction());	\
    }									\
  }									\
  }

#include "types.h"
#include <iostream>
#include <fstream>
#include <map>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/map.hpp>

class Warning {
private:
    std::set<addr_t> slice;
    addr_t addr;

    friend class boost::serialization::access;
    template<class Archive>
	void serialize(Archive & ar, const unsigned int version) {
	ar & slice;
	ar & addr;
	(void)version;
    }

public:
    Warning() {};

    Warning(addr_t a) {
	addr = a;
    }

    void addToSlice(addr_t b) {
	slice.insert(b);
    }

    addr_t getAddress() const {
	return addr;
    }

    typedef std::set<addr_t>::const_iterator slice_iterator;

    slice_iterator slice_begin() const {
	return slice.begin();
    }

    slice_iterator slice_end() const {
	return slice.end();
    }

    bool slice_find(addr_t b) const {
	return slice.find(b) != slice.end(); 
    }

    size_t getSliceSize() const {
        return slice.size();
    }
};

struct warningcmp {
    bool operator()(const Warning *w1, const Warning *w2) const {
	return w1->getAddress() < w2->getAddress();
    }
};

typedef std::set<Warning *, warningcmp> warnings_t;

#include <errno.h>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/bzip2.hpp>

inline void serialize(const char *f, const warnings_t &ww) {
    std::ofstream ofs(f,
            std::ios::out|std::ios::binary|std::ios::trunc);
    boost::iostreams::filtering_streambuf<boost::iostreams::output> out;
    out.push(boost::iostreams::bzip2_compressor());
    out.push(ofs);
    boost::archive::binary_oarchive oa(out);
    oa << ww;
}

inline void unserialize(const char *f, warnings_t &ww) {
    try {
        std::ifstream ifs(f, std::ios::in|std::ios::binary);
        if (!ifs.is_open()) {
            fprintf(stderr, "Failed to open %s: %s\n", f,
                    strerror(errno));
            exit(1);
        }
        boost::iostreams::filtering_streambuf<boost::iostreams::input>
            in;
        in.push(boost::iostreams::bzip2_decompressor());
        in.push(ifs);
        boost::archive::binary_iarchive ia(in);

        ia >> ww;
    } catch (boost::iostreams::bzip2_error) {
        std::ifstream ifs(f, std::ios::in|std::ios::binary);
        if (!ifs.is_open()) {
            fprintf(stderr, "Failed to open %s: %s\n", f,
                    strerror(errno));
            exit(1);
        }
        boost::archive::binary_iarchive ia(ifs);

        ia >> ww;
    }
}

#endif

// Local Variables: 
// mode: c++
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:
