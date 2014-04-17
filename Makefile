# Usage:
# DISABLE_ASSERTIONS=1 make --- disables assertions
# ENABLE_OPTIMIZED=1 make   --- builds an optimized build

CFLAGS_ASMIR = -Ilibasmir -Ilibasmir/src/include -IVEX/pub 
LDFLAGS_ASMIR = libasmir/src/libasmir.a VEX/libvex.a -lopcodes -lbfd -lz -lsqlite3
NDEBUG:=$(shell if [ ! -z $(DISABLE_ASSERTIONS) ]; then echo "-DNDEBUG"; fi)
CFLAGS = -Wall -Wno-deprecated -Wextra -pipe -ffloat-store -Wno-unused $(NDEBUG)
LDFLAGS = -lboost_serialization -lboost_iostreams

CFLAGS += $(CFLAGS_ASMIR)
LDFLAGS += $(LDFLAGS_ASMIR)

# These are similar to $(LDFLAGS), but with -ccopt/-cclib in front
# of each one. (Wonder if we could do this automatically)
LDFLAGS_OCAML = -cclib -lstdc++ -cclib -lboost_serialization -cclib -lboost_iostreams -cclib -lcamlidl -ccopt -Llibasmir/src -cclib -lasmir -ccopt -Lvine/stp -cclib VEX/libvex.a -cclib -lopcodes -cclib -lbfd -cclib -lz -cclib -lsqlite3 -ccopt -L$(PIN_KIT)/extras/xed2-ia32/lib -cclib -lxed

## PIN
TARGET_COMPILER = gnu
INCL = ./include
PIN_KIT = ./pin-2.8-33586-gcc.3.4.6-ia32_intel64-linux
PIN_HOME = $(PIN_KIT)/source/tools
# This tells the PIN's makefile which build we want (optimized/debug)
ifeq ($(ENABLE_OPTIMIZED),1)
DEBUG=0
else
DEBUG=1
endif
include $(PIN_HOME)/makefile.gnu.config
CXXFLAGS = -I$(INCL) $(DBG) $(OPT) -MMD
OPTFLAGS := -O3 -s
DBGFLAGS := -O0 -g3 -gdwarf-2
CXXFLAGS = $(CFLAGS)
override CXXFLAGS+=$(shell if [ -z $(ENABLE_OPTIMIZED) ]; then echo $(DBGFLAGS); else echo $(OPTFLAGS); fi)
override CFLAGS+=$(shell if [ -z $(ENABLE_OPTIMIZED) ]; then echo $(DBGFLAGS); else echo $(OPTFLAGS); fi)
###

LDFLAGS += -L$(PIN_KIT)/extras/xed2-ia32/lib/ -lxed 

OBJs = cfg.o func.o callgraph.o instr.o 
OBJs += trace.o argv_readparam.o pintracer.o vineir.o static.o serialize.o
OBJs += AbsDomStridedInterval.o HashFunctions.o Rand.o Utilities.o 
OBJs += InterProcCFG.o count-coverage.o path-length-test.o RBTest.o
OBJS += AbsRegion.o RegionTest.o cfgs_for_ocaml.o cfgs_stubs.o 
OBJS += Registers.o PinDisasm.o dataflow.o
EXEs = pintracer$(PINTOOL_SUFFIX) vineir stridedtest count-coverage \
       path-length-test rbtest regiontest static

all: $(EXEs)

.PHONY: clean
clean:
	-@rm -f *.o *.d $(EXEs) *.cmx *.cmi *.cmo \
	   cfgs_stubs.c cfgs.ml cfgs.mli cfgs.h

%.d: %.c
	$(CXX) $(CXXFLAGS) $(PIN_CXXFLAGS) -MM -MT '$(patsubst src/%,obj/%,$(patsubst %.c,%.o,$<))' $< > $@
%.d: %.cc
	$(CXX) $(CXXFLAGS) $(PIN_CXXFLAGS) -MM -MT '$(patsubst src/%,obj/%,$(patsubst %.cc,%.o,$<))' $< > $@
%.d: %.cpp
	$(CXX) $(CXXFLAGS) $(PIN_CXXFLAGS) -MM -MT '$(patsubst src/%,obj/%,$(patsubst %.cpp,%.o,$<))' $< > $@

-include $(OBJs:.o=.d)

%.o: %.cc Makefile
	${CXX} ${COPT} $(CXXFLAGS) ${CFLAGS} ${PIN_CXXFLAGS} ${OUTOPT}$@ $<
%.o: %.cpp Makefile
	${CXX} ${COPT} $(CXXFLAGS) ${CFLAGS} ${PIN_CXXFLAGS} ${OUTOPT}$@ $<

cfgs_stubs.c: cfgs.idl
	camlidl -header $+

cfgs.ml: cfgs.idl
	camlidl -header $+

cfgs.mli: cfgs.idl
	camlidl -header $+

# Check out https://bullseye.cs.berkeley.edu/svn/vine/projects/z3
# to vine/projects/z3, create a symlink "trunk -> ." inside vine,
# and then do "make" in vine/projects/z3.

Z3_DIR = vine/projects/z3/z3-2.15-32bit/z3

LDFLAGS_Z3 = -ccopt -L$(Z3_DIR)/lib -ccopt -L$(Z3_DIR)/ocaml \
             -cclib $(Z3_DIR)/lib/libz3.a -cclib -lgmp

OCAMLINCLUDES := -I vine/execution -I vine/ocaml -I vine/trace \
                 -I vine/stp/ocaml -I vine/projects/z3

VINE_LIBS := vine/stp/ocaml/stpvc.cmxa vine/ocaml/vine.cmxa \
             vine/trace/trace.cmxa vine/execution/execution.cmxa \
	     $(Z3_DIR)/ocaml/z3.cmxa
VINE_LIBS_DBG := vine/stp/ocaml/stpvc.cma vine/ocaml/vine.cma \
                 vine/trace/trace.cma vine/execution/execution.cma \
	         $(Z3_DIR)/ocaml/z3.cma

VINE_PKGS := -package str,ocamlgraph,extlib,unix

%.cmi: %.mli
	ocamlopt $(OCAMLINCLUDES) -o $@ -c $+

%.cmo: %.ml
	ocamlc -g $(OCAMLINCLUDES) -o $@ -c $*.ml

%.cmx: %.ml
	ocamlopt $(OCAMLINCLUDES) -o $@ -c $*.ml

cfgs_test.cmx: cfgs.cmi
cfgs_test.cmo: cfgs.cmi

cfgs_test: cfgs_test.cmx cfgs.cmx cfgs_stubs.o cfgs_for_ocaml.o \
	cfg.o func.o callgraph.o instr.o serialize.o InterProcCFG.o \
	PinDisasm.o Utilities.o
	ocamlopt -o $@ $+ $(LDFLAGS_OCAML)

cfg_fuzzball.cmx: cfgs.cmi
cfg_fuzzball.cmo: cfgs.cmi

cfg_fuzzball: $(VINE_LIBS) \
           vine/projects/z3/z3vc.cmx \
	   vine/projects/z3/z3vc_query_engine.cmx \
           cfg_fuzzball.cmx cfgs.cmx \
           cfgs_stubs.o cfgs_for_ocaml.o PinDisasm.o \
           cfg.o func.o callgraph.o instr.o serialize.o InterProcCFG.o \
		   Utilities.o
	ocamlfind ocamlopt -o $@ $(OCAMLINCLUDES) $(VINE_PKGS) -linkpkg \
          $+ $(LDFLAGS_OCAML) $(LDFLAGS_Z3)

cfg_fuzzball.dbg: $(VINE_LIBS_DBG) \
           vine/projects/z3/z3vc.cmo \
	   vine/projects/z3/z3vc_query_engine.cmo \
           cfg_fuzzball.cmo cfgs.cmo \
           cfgs_stubs.o cfgs_for_ocaml.o PinDisasm.o \
           cfg.o func.o callgraph.o instr.o serialize.o InterProcCFG.o \
		   Utilities.o
	ocamlfind ocamlc -g -o $@ $(OCAMLINCLUDES) $(VINE_PKGS) -linkpkg \
          $+ $(LDFLAGS_OCAML) $(LDFLAGS_Z3)

pintracer$(PINTOOL_SUFFIX): trace.o argv_readparam.o pintracer.o \
	cfg.o func.o callgraph.o instr.o serialize.o PinDisasm.o Utilities.o \
	$(PIN_LIBNAMES) 
	${CXX} $(PIN_LDFLAGS) $(LINK_DEBUG) $+ \
	${LINK_OUT}$@ ${PIN_LPATHS} $(PIN_LIBS) $(EXTRA_LIBS) $(DBG) $(LDFLAGS)

stridedtest: AbsDomStridedInterval.o HashFunctions.o Rand.o \
	Utilities.o StridedIntervalTest.cpp
	${CXX} -o $@ ${CXXFLAGS} $+ $(LDFLAGS)

rbtest: RBTest.o
	${CXX} -o $@ ${CXXFLAGS} $+ $(LDFLAGS)

regiontest: AbsDomStridedInterval.o Utilities.o HashFunctions.o Rand.o \
	AbsRegion.o RegionTest.o Registers.o
	${CXX} -o $@ ${CXXFLAGS} $+ $(LDFLAGS)

static: static.o cfg.o func.o callgraph.o instr.o serialize.o AbsRegion.o \
	AbsDomStridedInterval.o Utilities.o HashFunctions.o Rand.o \
	Registers.o PinDisasm.o dataflow.o
	${CXX} ${CXXFLAGS} $+ -o $@ $(LDFLAGS) -lboost_program_options

count-coverage: count-coverage.o cfg.o func.o callgraph.o instr.o serialize.o \
	PinDisasm.o Utilities.o
	${CXX} ${CXXFLAGS} $+ -o $@ $(LDFLAGS) -lboost_program_options

path-length-test: path-length-test.o cfg.o func.o callgraph.o instr.o \
	serialize.o InterProcCFG.o PinDisasm.o Utilities.o
	${CXX} ${CXXFLAGS} $+ -o $@ $(LDFLAGS) -lboost_program_options

vineir: vineir.o
	${CXX} $+ -o $@ $(LDFLAGS)
