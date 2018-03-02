ZYDIS:=zydis-2.0.0
ZYDIS_LIB:=$(PWD)/dep/$(ZYDIS)/build/
ZYDIS_INC:=$(PWD)/dep/$(ZYDIS)/include/
ZYDIS_GEN:=$(PWD)/dep/$(ZYDIS)/build/

CXXFLAGS += -I$(ZYDIS_INC) -I$(ZYDIS_GEN)
LDFLAGS  += -L$(ZYDIS_LIB) -lZydis
