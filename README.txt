The file "README" (no .txt extension) has some notes from the public
version of this code which may be helpful; this file is for notes
specific to FuzzBomb/CGC version of the code.

"make-cfg" is a substitue for pintracer.so that creates CFGs using
only recursive disassembly from the entry point (or other entry points
supplied on the command line), without doing dynamic tracing.

Like the cgc-branch of FuzzBALL, this branch contains modifications
originally created as part of the FuzzBomb project for the DARPA Cyber
Grand Challenge (CGC, https://cgc.darpa.mil/). It was joint work
between the University of Minnesota and SIFT:

http://www.sift.net/research/artificial-intelligence/fuzzbomb

The original development was in SVN; SVN log entries are included in
the Git commit comments, though the authorship information is not
accurate in the Git metadata. Here's the interpretation of SVN user
names:

jbenton = J. Benton (https://ti.arc.nasa.gov/profile/j-benton/)
jthayer = Jordan Thayer (http://www.jordanthayer.com/)
musliner = David Musliner (http://www.sift.net/staff/david-musliner)
sfriedman = Scott Friedman (http://www.sift.net/staff/scott-friedman)
smccamant = Stephen McCamant <mccamant@cs.umn.edu>

This code still has a lot of old and finicky build dependencies.
Though we haven't resolved any of the hard aspects of this in the
process of making the open-source release, I can document here one
configuration that works at least for compiling, used to check that
the FuzzBomb/CGC changes compile correctly.

The biggest compatibility constraint on the code is the Pin tool. It's
based on an old version of Pin which is only binary-compatible with
old versions of g++, and to build a 32-bit pintool to analyze 32-bit
binaries, all of the code in this directory and all of the code in
FuzzBALL that it links with in cfg_fuzzball needs to be 32-bit too. I
tested on a more modern Ubuntu 16.04.6 64-bit system at UMN, but I
don't have sudo access there, so I recompiled various tools from
source to work around limitations of the system packages. I used:

- g++ (GCC) 4.8.5, recompiled from source with attention to 32-bit
target support.

  In particular the UMN machines are missing a symlink from
  /usr/include/asm to /usr/include/x86-64-linux-gnu/asm that would
  normally be part of multi-arch GCC, so I manually created a symlink
  from $prefix/x86_64-unknown-linux-gnu/include/asm pointing at
  /usr/include/x86_64-linux-gnu/asm, where $prefix is the installation
  prefix of my GCC, in my case
  /project/mccamant/soft/amd64/gnu/gcc/4.8.5.

- bzip2-1.0.6 libbz2.a, recompiled for 32-bit (used by Boost)

- Boost 1.55, recompiled for 32-bit

- GNU Binutils 2.23.2, recompiled for 32-bit

- libelf 0.8.13, recompiled for 32-bit

- VEX SVN r2737, recompiled for 32-bit

- libstp.a from STP FuzzBALL branch commit a5d1d904, 32-bit (old but
not important)

- 32-bit host/target OCaml 4.05.0, configured with:
  ./configure -cc "gcc -m32" -as "as --32" -aspp "gcc -m32 -c" -prefix /project/mccamant/soft/i386/caml/ocaml/4.05.0 -host i386-linux-gnu -partialld "ld -r -melf_i386"

- FuzzBALL from the current tip of the cgc-branch, 6572a4e1,
  configured and built 32-bit with:
  ./configure --with-vex=/export/scratch2/mccamant/cgc/d-s-se-git/merge-to-gh/VEX CXXFLAGS=-g3 CC="gcc -m32" CXX="g++ -m32"
  make CC="gcc -m32" CXX="g++ -m32"

- Intel Pin 2.14, downloaded from:
  https://software.intel.com/sites/landingpage/pintool/downloads/pin-2.14-71313-gcc.4.4.7-linux.tar.gz

I recommend using CCache when compiling (e.g., I did make -j8
CXX="ccache g++"), because the Makefile is set up to recompile
everything when the Makefile itself changes.
