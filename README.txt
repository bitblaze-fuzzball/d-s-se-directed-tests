The file "README" (no .txt extension) has some notes from the public
version of this code which may be helpful; this file is for CGC-specific
notes.

"make-cfg", unfinished as of this writing, will be a substitue for
pintracer.so that creates CFGs using only recursive disassembly from
the entry point, no tracing.

Commands to build on the 32-bit FuzzBomb VM:

sudo apt-get install libboost-serialization-dev libboost-iostreams-dev
# CCache is optional, but can help make up for pessimism in the Makefile
sudo apt-get install ccache
sudo apt-get install libbz2-dev

ln -s ../fuzzball .
ln -s ~/fuzzball-external-deps/vex-r2737 VEX
cp /usr/bin/stp fuzzball/stp/

wget http://software.intel.com/sites/landingpage/pintool/downloads/pin-2.13-65163-gcc.4.4.7-linux.tar.gz
tar xzf pin-2.13-65163-gcc.4.4.7-linux.tar.gz

make CXX="ccache g++"
