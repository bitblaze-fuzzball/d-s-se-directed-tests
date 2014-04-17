#include "serialize.h"
#include <iostream>
#include <fstream>
#include <map>

#include <errno.h>

#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/bzip2.hpp>

void unserialize(const char *f, Prog &prog, CallGraph *&callgraph,
        std::map<unsigned int, Function *> &functions) {

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

        // Unserialize the CFGs
        ia >> prog;
    } catch (boost::iostreams::bzip2_error) {
        std::ifstream ifs(f, std::ios::in|std::ios::binary);
        if (!ifs.is_open()) {
            fprintf(stderr, "Failed to open %s: %s\n", f,
                    strerror(errno));
            exit(1);
        }
        boost::archive::binary_iarchive ia(ifs);

        // Unserialize the CFGs
        ia >> prog;
    }

    callgraph = prog.getCallGraph();
    for (CallGraph::const_func_iterator fit = callgraph->func_begin();
            fit != callgraph->func_end(); fit++) {

        Function *f = *fit;
        functions[f->getAddress()] = f;
    }
}

void serialize(const char *f, const Prog &prog) {
    std::ofstream ofs(f,
            std::ios::out|std::ios::binary|std::ios::trunc);
    boost::iostreams::filtering_streambuf<boost::iostreams::output> out;
    out.push(boost::iostreams::bzip2_compressor());
    out.push(ofs);
    boost::archive::binary_oarchive oa(out);
    oa << prog;
}

// Local Variables: 
// c-basic-offset: 4
// compile-command: "dchroot -c typeinfer -d make"
// End:


