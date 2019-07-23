#!/bin/bash -x
# This preamble stuff is commented out because you need to run it
# before running this script (including, in order to get a copy of
# this script in the first place)

# # x86-64 software local to our research group
# module use /project/mccamant/soft/amd64/.modules
# # 32-bit x86 software local to our research group
# module use /project/mccamant/soft/i386/.modules-i386

# # 32-bit-hosted and -generating version of OCaml
# # Compiled with:
# # ./configure -cc "gcc -m32" -as "as --32" -aspp "gcc -m32 -c" -prefix /project/mccamant/soft/i386/caml/ocaml/4.05.0 -host i386-linux-gnu -partialld "ld -r -melf_i386"
# # Plus a bunch of OCaml libraries and tools: biniou-1.0.9,
# # camlidl-camlidl106, camlp4-4.05-2, cppo-0.9.3, easy-format-1.0.2,
# # extlib-1.6.1, findlib-1.5.5, ocamlbuild-0.12.0, tyxml-3.4.0,
# # uutf-0.9.4, yojson-1.1.8
# module add i386/caml/ocaml/4.05.0.prepend

# # 32-bit-capable (though 64-bit hosted) version of GCC
# # Compiled with:
# # ../configure prefix=/project/mccamant/soft/amd64/gnu/gcc/4.8.5 --enable-languages=c,c++,fortran,java,lto,objc CFLAGS=-I/usr/include/x86_64-linux-gnu
# # We need to use this old version consistently because the Pin 2.14
# # binaries were built with a similarly old GCC, and newer G++ is not
# # ABI compatible.
# module add gnu/gcc/4.8.5

# # ccache helps accelerate re-builds especially when tools or
# # dependencies might have changed a bit.
# module add build/ccache

# # Store ccache's cache on a local filesystem:
# export CCACHE_DIR=/export/scratch2/$(whoami)/ccache

# # Check out the d-s-se code tree
# git clone https://github.com/bitblaze-fuzzball/d-s-se-directed-tests.git
# cd d-s-se-directed-tests

# Check out FuzzBALL into "fuzzball"
git clone https://github.com/bitblaze-fuzzball/fuzzball
cd fuzzball
./autogen.sh
cd ..

# Check out and compile VEX, similar to the FuzzBALL instructions but
# forcing 32-bit
svn co -r3260 svn://svn.valgrind.org/vex/trunk VEX
cd VEX
patch -p0 <../fuzzball/vex-r3260.patch
make -f Makefile-gcc CC="ccache gcc -m32"
cd ..

# Compile LibASMIR and FuzzBALL, using the VEX compiled above, and the
# binutils and STP in our local repository.
cd fuzzball
./configure --with-vex=$(pwd)/../VEX --with-binutils=/project/mccamant/soft/i386/gnu/binutils/2.32 CXXFLAGS=-g3 CC="ccache gcc -m32" CXX="ccache g++ -m32"
# STP built with:
# cmake .. -DBUILD_STATIC_BIN=ON -DCMAKE_C_COMPILER=/project/mccamant/soft/amd64/gnu/gcc/4.8.5/bin/gcc -DCMAKE_CXX_COMPILER=/project/mccamant/soft/amd64/gnu/gcc/4.8.5/bin/g++ -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32 -DENABLE_PYTHON_INTERFACE=OFF -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DBoost_DIR=/project/mccamant/soft/i386/lib/boost/1.70-gcc48/lib/cmake -DCMAKE_EXE_LINKER_FLAGS=-m32 -DBOOST_ROOT=/project/mccamant/soft/i386/lib/boost/1.70-gcc48
cp /project/mccamant/soft/i386/solver/stp/fuzzball-2014-10-08/bin/stp stp
cp /project/mccamant/soft/i386/solver/stp/fuzzball-2014-10-08/lib/libstp.a stp
make CC="ccache gcc -m32" CXX="ccache g++ -m32"
cd ..

wget https://software.intel.com/sites/landingpage/pintool/downloads/pin-2.14-71313-gcc.4.4.7-linux.tar.gz
tar xzf pin-2.14-71313-gcc.4.4.7-linux.tar.gz

# Binutils built with:
# ../configure --prefix=/project/mccamant/soft/i386/gnu/binutils/2.32 --build=i386-linux-gnu --host=i386-linux-gnu --disable-plugins --enable-targets=alpha-linux-gnu,arm-linux-gnueabi,hppa-linux-gnu,i486-linux-gnu,ia64-linux-gnu,m68k-linux-gnu,m68k-rtems,mips-linux-gnu,mipsel-linux-gnu,mips64-linux-gnu,mips64el-linux-gnu,powerpc-linux-gnu,powerpc64-linux-gnu,s390-linux-gnu,s390x-linux-gnu,sh-linux-gnu,sparc-linux-gnu,sparc64-linux-gnu,x86_64-linux-gnu,m32r-linux-gnu,spu,aarch64-linux-gnu CFLAGS=-m32
ln -s /project/mccamant/soft/i386/gnu/binutils/2.32 binutils

# Boost built with:
# ./bootstrap.sh --prefix=/project/mccamant/soft/i386/lib/boost/1.70-gcc48
# ./b2 toolset=gcc-4.8 architecture=x86 address-model=32 -s BZIP2_LIBPATH=/project/mccamant/soft/i386/lib/bzip2/1.0.6/lib -s ZLIB_LIBPATH=/project/mccamant/soft/i386/lib/zlib/1.2.11/lib link=static -j8 install
ln -s /project/mccamant/soft/i386/lib/boost/1.70-gcc48 boost

mkdir include
mkdir include/caml
cp /project/mccamant/soft/i386/caml/ocaml/4.05.0/lib/ocaml/caml/camlidlruntime.h include/caml

# Elfutils built with:
# ./configure --prefix=/project/mccamant/soft/i386/lib/elfutils/0.176 CC="ccache gcc-4.8 -m32" LDFLAGS="-L/project/mccamant/soft/i386/lib/zlib/1.2.11/lib -L/project/mccamant/soft/i386/lib/bzip2/1.0.6/lib -Wl,-rpath /project/mccamant/soft/i386/lib/zlib/1.2.11/lib -Wl,-rpath /project/mccamant/soft/i386/lib/elfutils/0.176/lib" --disable-textrelcheck

mkdir lib
ln -s /project/mccamant/soft/i386/lib/elfutils/0.176/lib/libelf.a lib

# If all the prerequisites are set up correctly, you can the compile
# the d-s-s-e code with the following command:

# make -j8 CXX="ccache g++"
