# -------------------------------------------------------------------------------
#
# Building Script for LavaScript
#
# -------------------------------------------------------------------------------

PWD:=$(shell pwd)

SOURCE:=$(shell find src/ -type f -name "*.cc")
INCLUDE:=$(shell find src/ -type f -name "*.h")
OBJECT:=${SOURCE:.cc=.o}
TEST:=$(shell find test/ -type f -name "*-test.cc")
TESTOBJECT:=${TEST:.cc=.t}
CXX = g++


# -------------------------------------------------------------------------------
#
# Flags for different types of build
#
# -------------------------------------------------------------------------------
RELEASE_FLAGS = -I$(PWD) -g3 -O3 -Wall
RELEASE_LIBS  =

TEST_DEF = -DLAVASCRIPT_CHECK_OBJECTS

TEST_FLAGS = -I$(PWD) -g3 -Wall -fsanitize=address $(TEST_DEF) -Werror
TEST_LIBS = -lgtest -lpthread


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
test/%.t : test/%.cc $(OBJECT) $(INCLUDE) $(SOURCE)
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
release: $(OBJECT)
	ar rcs liblavascript.a $(OBJECT)

.PHONY:clean
clean:
	rm -rf $(OBJECT)
	rm -rf $(TESTOBJECT)
	rm -rf liblavascript.a
