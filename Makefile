# Usage:
# DISABLE_ASSERTIONS=1 make --- disables assertions
# ENABLE_OPTIMIZED=1 make   --- builds an optimized build

ENABLE_OPTIMIZED=1	# optimize and turn off tons of debug output to stdout that breaks lisp
CFLAGS_ASMIR = -Ifuzzball/libasmir -Ifuzzball/libasmir/src/include -IVEX/pub -Ibinutils/include
LDFLAGS_ASMIR = -Lbinutils/lib
LIBS_ASMIR = fuzzball/libasmir/src/libasmir.a VEX/libvex.a -lopcodes -lbfd -liberty -lz
NDEBUG:=$(shell if [ ! -z $(DISABLE_ASSERTIONS) ]; then echo "-DNDEBUG"; fi)
CFLAGS = -Wall -Wno-deprecated -Wextra -pipe -ffloat-store -Wno-unused -Wno-unused-parameter $(NDEBUG) -Iboost/include -Ijson_spirit
LDFLAGS = -Lboost/lib # -Wl,-rpath `pwd`/boost/lib
LIBS = -lboost_serialization -lboost_iostreams -lbz2

CFLAGS += $(CFLAGS_ASMIR)
LDFLAGS += $(LDFLAGS_ASMIR)
LIBS += $(LIBS_ASMIR)

CFLAGS += -Iinclude
LDFLAGS += -Llib

# These are similar to $(LDFLAGS), but with -ccopt/-cclib in front
# of each one. (Wonder if we could do this automatically)
LDFLAGS_OCAML = -cclib -lstdc++ -ccopt -Llib -ccopt -Lboost/lib -cclib -lboost_serialization -cclib -lboost_iostreams -cclib -lcamlidl -ccopt -Lfuzzball/libasmir/src -cclib -lasmir -ccopt -Lfuzzball/stp -cclib VEX/libvex.a -cclib -lopcodes -cclib -lbfd -cclib -lz -ccopt -L$(PIN_XED)/lib -cclib -lxed -cclib -lbz2 # -ccopt -Wl,-rpath=`pwd`/boost/lib

## PIN
TARGET_COMPILER = gnu
TARGET := ia32
CFLAGS += -m32
TOOL_LDFLAGS += -m32
INCL = ./include
#PIN_KIT = ./pin-2.13-65163-gcc.4.4.7-linux
#PIN_XED = $(PIN_KIT)/extras/xed2-ia32
PIN_KIT = ./pin-2.14-71313-gcc.4.4.7-linux
PIN_XED = $(PIN_KIT)/extras/xed-ia32
PIN_ROOT = $(PIN_KIT)
PIN_HOME = $(PIN_KIT)/source/tools
# This tells the PIN's makefile which build we want (optimized/debug)
ifeq ($(ENABLE_OPTIMIZED),1)
DEBUG=0
else
DEBUG=1
endif
CONFIG_ROOT := $(PIN_HOME)/Config
include $(CONFIG_ROOT)/makefile.config
# We want to control our own compiler warnings, thank you very much.
TOOL_CXXFLAGS := $(filter-out -Wall -Werror,$(TOOL_CXXFLAGS))

#CXXFLAGS = -I$(INCL) $(DBG) $(OPT) -MMD
OPTFLAGS := -O3 -s
DBGFLAGS := -O0 -g3 -gdwarf-2
override CFLAGS+=$(shell if [ -z $(ENABLE_OPTIMIZED) ]; then echo $(DBGFLAGS); else echo $(OPTFLAGS); fi)
CXXFLAGS := $(CFLAGS)
#override CXXFLAGS+=$(shell if [ -z $(ENABLE_OPTIMIZED) ]; then echo $(DBGFLAGS); else echo $(OPTFLAGS); fi)
###

# This -I is duplicated in Pin's $(TOOL_CXXFLAGS)
#CXXFLAGS += -I$(PIN_XED)/include
LDFLAGS += -L$(PIN_XED)/lib
LIBS += -lxed

# Because it took 28 years to standardize hash tables...
CXXFLAGS += -std=c++11

OBJs = cfg.o func.o callgraph.o instr.o 
OBJs += trace.o argv_readparam.o pintracer.o vineir.o static.o serialize.o
OBJs += AbsDomStridedInterval.o HashFunctions.o Rand.o Utilities.o 
OBJs += InterProcCFG.o count-coverage.o path-length-test.o RBTest.o
OBJs += AbsRegion.o RegionTest.o cfgs_for_ocaml.o cfgs_stubs.o 
OBJs += Registers.o PinDisasm.o dataflow.o read_assoc.o make-cfg.o
EXEs := pintracer$(PINTOOL_SUFFIX) vineir stridedtest count-coverage \
        path-length-test rbtest regiontest static cfg_fuzzball make-cfg

# all: $(EXEs)

# Pin's Makefiles now give us:
# all: objects libs dlls apps tools

objects: $(OBJs)
libs:
dlls:
apps: $(EXEs)
tools:


.PHONY: clean
clean:
	-@rm -f *.o $(EXEs) *.cmx *.cmi *.cmo \
	   cfgs_stubs.c cfgs.ml cfgs.mli cfgs.h

clean-deps:
	-@rm -f *.d

%.d: %.c
	$(CXX) $(CXXFLAGS) $(TOOL_CXXFLAGS) -MM -MT '$(patsubst src/%,obj/%,$(patsubst %.c,%.o,$<))' $< > $@
%.d: %.cc
	$(CXX) $(CXXFLAGS) $(TOOL_CXXFLAGS) -MM -MT '$(patsubst src/%,obj/%,$(patsubst %.cc,%.o,$<))' $< > $@
%.d: %.cpp
	$(CXX) $(CXXFLAGS) $(TOOL_CXXFLAGS) -MM -MT '$(patsubst src/%,obj/%,$(patsubst %.cpp,%.o,$<))' $< > $@

-include $(OBJs:.o=.d)

%.o: %.cc Makefile
	${CXX} ${COPT} $(CXXFLAGS) ${TOOL_CXXFLAGS} ${COMP_OBJ}$@ $<
%.o: %.cpp Makefile
	${CXX} ${COPT} $(CXXFLAGS) ${TOOL_CXXFLAGS} ${COMP_OBJ}$@ $<

cfgs_stubs.c: cfgs.idl
	camlidl -header $+

cfgs.ml: cfgs.idl
	camlidl -header $+

cfgs.mli: cfgs.idl
	camlidl -header $+

OCAMLINCLUDES := -I fuzzball/execution -I fuzzball/ocaml -I fuzzball/trace \
                 -I fuzzball/stp/ocaml -I fuzzball/logging

VINE_LIBS := fuzzball/stp/ocaml/stpvc.cmxa fuzzball/ocaml/vine.cmxa \
             fuzzball/trace/trace.cmxa \
             fuzzball/structs/structs.cmxa \
             fuzzball/logging/logging.cmxa \
             fuzzball/execution/execution.cmxa

VINE_LIBS_DBG := fuzzball/stp/ocaml/stpvc.cma fuzzball/ocaml/vine.cma \
                 fuzzball/trace/trace.cma \
                 fuzzball/structs/structs.cma \
                 fuzzball/logging/logging.cma \
                 fuzzball/execution/execution.cma

VINE_PKGS := -package str,extlib,unix,yojson,tyxml

%.cmi: %.mli
	ocamlopt $(OCAMLINCLUDES) -o $@ -c $+

%.cmo: %.ml
	ocamlc -g $(OCAMLINCLUDES) -o $@ -c $*.ml

%.cmx: %.ml
	ocamlopt $(OCAMLINCLUDES) -o $@ -c $*.ml

cfgs.cmx: cfgs.cmi

cfgs_test.cmx: cfgs.cmi
cfgs_test.cmo: cfgs.cmi

cfgs_test: cfgs_test.cmx cfgs.cmx cfgs_stubs.o cfgs_for_ocaml.o \
	cfg.o func.o callgraph.o instr.o serialize.o InterProcCFG.o \
	PinDisasm.o Utilities.o
	ocamlopt -o $@ $+ $(LDFLAGS_OCAML)

cfg_fuzzball.cmx: cfgs.cmi
cfg_fuzzball.cmo: cfgs.cmi

cfg_fuzzball: $(VINE_LIBS) \
           cfgs.cmx cfg_fuzzball.cmx \
           cfgs_stubs.o cfgs_for_ocaml.o PinDisasm.o \
           cfg.o func.o callgraph.o instr.o serialize.o InterProcCFG.o \
		   Utilities.o
	ocamlfind ocamlopt -o $@ $(OCAMLINCLUDES) $(VINE_PKGS) -linkpkg \
          $+ $(LDFLAGS_OCAML)

cfg_fuzzball.dbg: $(VINE_LIBS_DBG) \
           cfgs.cmo cfg_fuzzball.cmo \
           cfgs_stubs.o cfgs_for_ocaml.o PinDisasm.o \
           cfg.o func.o callgraph.o instr.o serialize.o InterProcCFG.o \
		   Utilities.o
	ocamlfind ocamlc -g -o $@ $(OCAMLINCLUDES) $(VINE_PKGS) -linkpkg \
          $+ $(LDFLAGS_OCAML)

pintracer$(PINTOOL_SUFFIX): trace.o argv_readparam.o pintracer.o \
	cfg.o func.o callgraph.o instr.o serialize.o PinDisasm.o Utilities.o \
	$(PIN_LIBNAMES) 
	${CXX} $(LDFLAGS) $(TOOL_LDFLAGS) $(TOOL_LPATHS) $(LINK_DEBUG) $+ \
	 ${LINK_EXE}$@ ${PIN_LPATHS} $(TOOL_LIBS) $(PIN_LIBS) $(EXTRA_LIBS) $(LIBS) $(DBG)

stridedtest: AbsDomStridedInterval.o HashFunctions.o Rand.o \
	Utilities.o StridedIntervalTest.cpp
	${CXX} -o $@ ${CXXFLAGS} $+ $(LDFLAGS) $(LIBS)

rbtest: RBTest.o
	${CXX} -o $@ ${CXXFLAGS} $+ $(LDFLAGS) $(LIBS)

regiontest: AbsDomStridedInterval.o Utilities.o HashFunctions.o Rand.o \
	AbsRegion.o RegionTest.o Registers.o
	${CXX} -o $@ ${CXXFLAGS} $+ $(LDFLAGS) $(LIBS)

static: static.o cfg.o func.o callgraph.o instr.o serialize.o AbsRegion.o \
	AbsDomStridedInterval.o Utilities.o HashFunctions.o Rand.o \
	Registers.o PinDisasm.o dataflow.o
	${CXX} ${CXXFLAGS} $+ -o $@ $(LDFLAGS) -lboost_program_options $(LIBS)

count-coverage: count-coverage.o cfg.o func.o callgraph.o instr.o serialize.o \
	PinDisasm.o Utilities.o
	${CXX} ${CXXFLAGS} $+ -o $@ $(LDFLAGS) -lboost_program_options $(LIBS)

path-length-test: path-length-test.o cfg.o func.o callgraph.o instr.o \
	serialize.o InterProcCFG.o PinDisasm.o Utilities.o
	${CXX} ${CXXFLAGS} $+ -o $@ $(LDFLAGS) -lboost_program_options $(LIBS)

vineir: vineir.o
	${CXX} -o $@ ${CXXFLAGS} $+ $(LDFLAGS) $(LIBS)

make-cfg: make-cfg.o cfg.o func.o callgraph.o instr.o serialize.o \
	Utilities.o PinDisasm.o argv_readparam.o read_assoc.o
	${CXX} ${CXXFLAGS} $+ -o $@ $(LDFLAGS) $(LIBS) -lelf

test_assoc: read_assoc.o
	${CXX} -o $@ ${CXXFLAGS} $^ $(LDFLAGS) $(LIBS)
