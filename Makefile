# -------------------------------------------------------------------------------
#
# Building Script for LavaScript
#
# -------------------------------------------------------------------------------

PWD:=$(shell pwd)
SOURCE=$(shell find src/ -type f -name "*.cc")
INCLUDE:=$(shell find src/ -type f -name "*.h")
OBJECT=${SOURCE:.cc=.o}
TEST:=$(shell find unittest/ -type f -name "*-test.cc")
TESTOBJECT:=${TEST:.cc=.t}
CXX = g++
#SANITIZER=-fsanitize=address,undefined
LUA=luajit

# -------------------------------------------------------------------------------
#
# Zydis
#
# -------------------------------------------------------------------------------
ZYDIS:=zydis-2.0.0-alpha2
ZYDIS_LIB:=$(PWD)/dep/$(ZYDIS)/build/
ZYDIS_INC:=$(PWD)/dep/$(ZYDIS)/include/
ZYDIS_GEN:=$(PWD)/dep/$(ZYDIS)/build/

CXXFLAGS += -I$(ZYDIS_INC) -I$(ZYDIS_GEN)
LDFLAGS  += -L$(ZYDIS_LIB) -lZydis

# -------------------------------------------------------------------------------
#
# Release build as default
#
# -------------------------------------------------------------------------------
all: release

release : dep

dep:
	./build-dep.sh
.PHONY: dep

# ------------------------------------------------------------------------------
#
# Artifacts
#
# ------------------------------------------------------------------------------
INTERP_OBJECT:=src/interpreter/x64-interpreter.dasc.pp.o
INTERP_SOURCE:=src/interpreter/x64-interpreter.dasc.pp.cc

$(INTERP_SOURCE): src/interpreter/x64-interpreter.dasc
	./dynasm_pp.sh src/interpreter/x64-interpreter.dasc 1> src/interpreter/x64-interpreter.dasc.pp.cc

$(INTERP_OBJECT): $(INTERP_SOURCE)
	$(CXX) $(CXXFLAGS) -c src/interpreter/x64-interpreter.dasc.pp.cc -o $(INTERP_OBJECT) $(LDFLAGS)

# -------------------------------------------------------------------------------
#
# Flags for different types of build
#
# -------------------------------------------------------------------------------
RELEASE_FLAGS = -I$(PWD) -g3 -O3 -Wall
RELEASE_LIBS  =

TEST_DEF = -DLAVASCRIPT_DEBUG_LEVEL=3
TEST_FLAGS = -I$(PWD) -g3 -Wall $(TEST_DEF) $(SANITIZER)
TEST_LIBS = -lgtest -lpthread -lm

# -------------------------------------------------------------------------------
#
# Objects
#
# -------------------------------------------------------------------------------
src/%.o : src/%.cc src/%.h
	$(CXX) $(CXXFLAGS) -c -o $@ $< $(LDFLAGS)

# -------------------------------------------------------------------------------
#
#  Testing Library
#
# -------------------------------------------------------------------------------
unittest/%.t : unittest/%.cc  $(INTERP_OBJECT) $(OBJECT) $(INCLUDE) $(SOURCE)
	$(CXX) $(OBJECT) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

test: CXXFLAGS += $(TEST_FLAGS)
test: LDFLAGS  += $(TEST_LIBS)
test: $(TESTOBJECT)

# -------------------------------------------------------------------------------
#
#  Release
#
# -------------------------------------------------------------------------------
release: CXXFLAGS += $(RELEASE_FLAGS)
release: LDFLAGS += $(RELEASE_LIBS)
release: $(OBJECT) $(INTERP_OBJECT)
	ar rcs liblavascript.a $(OBJECT) $(INTERP_OBJECT)

.PHONY:clean
clean:
	rm -rf $(OBJECT)
	rm -rf $(INTERP_OBJECT)
	rm -rf $(INTERP_SOURCE)
	rm -rf $(TESTOBJECT)
	rm -rf liblavascript.a
