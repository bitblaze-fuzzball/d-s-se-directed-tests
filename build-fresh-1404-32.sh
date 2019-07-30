#!/bin/bash

# Ubuntu 14.04 32-bit is the easiest Ubuntu version to compile this
# program on, since the system GCC matches the version that Pin needs,
# and the system OCaml produces 32-bit executables. Systems that are
# significantly older, significantly newer, or 64-bit are all more
# complex to deal with, because more prereqs need to be custom
# compiled.

# The next set of commands are commented-out because if you're getting
# this script from the Git repository, you must have already run them.

# sudo apt-get -y install git
# git clone https://github.com/bitblaze-fuzzball/d-s-se-directed-tests.git
# cd d-s-se-directed-tests
# # git checkout cgc-branch

sudo apt-get -y install build-essential

sudo apt-get -y install ccache

git clone https://github.com/bitblaze-fuzzball/fuzzball
# cd fuzzball
# git checkout cgc-branch
# cd ..

## VEX
sudo apt-get -y install subversion
sudo apt-get -y build-dep valgrind
svn co -r3260 svn://svn.valgrind.org/vex/trunk VEX
cd VEX
patch -p0 <../fuzzball/vex-r3260.patch
make -f Makefile-gcc CC="ccache gcc"
cd ..

## STP
git clone https://github.com/bitblaze-fuzzball/stp.git
cd stp
sudo apt-get -y install cmake
sudo apt-get -y install bison flex
sudo apt-get -y install libboost-program-options-dev libboost-system-dev
mkdir build
cd build
cmake ..
make -j6
cp stp lib/libstp.a ../../fuzzball/stp
cd ..
cd ..


## GNU Binutils
sudo apt-get -y install binutils-dev libiberty-dev
sudo apt-get -y install zlib1g-dev

## FuzzBALL OCaml dependencies
sudo apt-get -y install ocaml
sudo apt-get -y install ocaml-native-compilers ocaml-findlib
sudo apt-get -y install camlidl libextlib-ocaml-dev

## FuzzBALL
cd fuzzball
./autogen.sh
./configure --with-vex=$(pwd)/../VEX CXXFLAGS=-g3 CC="ccache gcc" CXX="ccache g++"
make
cd ..

## Pin
wget https://software.intel.com/sites/landingpage/pintool/downloads/pin-2.14-71313-gcc.4.4.7-linux.tar.gz
tar xzf pin-2.14-71313-gcc.4.4.7-linux.tar.gz

## libelf
sudo apt-get -y install libelf-dev

## Boost dependencies of d-s-se
sudo apt-get -y install libboost-serialization-dev libboost-iostreams-dev
sudo apt-get -y install libbz2-dev

## Compile d-s-se tools themselves
make -j6 CXX="ccache g++"

## Dietlibc with sysenter disabled, useful for building subject programs
sudo apt-get -y install cvs
cvs -d :pserver:cvs@cvs.fefe.de:/cvs -z9 co dietlibc
cd dietlibc
perl -pi -e 'chomp; $_ = "/* $_ */" if /SYSENTER/; $_ .= "\n";' dietfeatures.h
make
cd ..

## Don't ask
mkdir /tmp/yyy
