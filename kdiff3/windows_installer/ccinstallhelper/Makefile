# Project: Clearcase install helper
# Generates ccinstallhelper.dll with gcc.
# Can be used for Cygwin and MingW (MingW ignores -mno-cygwin)
#
PROJ := ccinstallhelper

CXX  ?= g++.exe

ifdef DEBUG
  CXXFLAGS ?= -g
else
  CXXFLAGS ?= -Os
  LDFLAGS += -s
endif
CXXFLAGS += -ansi -pedantic -Wall -W
LIBS :=  -luuid -lole32
DEFFILE = $(PROJ).def
STATICLIB = $(PROJ).a
EXPLIB = $(PROJ).exp

SRC-CXX = $(wildcard *.cpp)
SRC-RC = $(wildcard *.rc)

OBJ  := $(SRC-CXX:.cpp=.o)
RES  := $(SRC-RC:.rc=.res)
OBJ += $(RES)
DLL  := $(PROJ).dll

.PHONY: all clean

.SUFFIXES: .rc .res

all: .depend $(DLL)

debug:
	$(MAKE) DEBUG=YES UNICODE=YES

release:
	$(MAKE) 

.depend: Makefile $(SRC-RC) $(SRC-CXX)
	$(CXX) -M $(CXXFLAGS) $(SRC-RC) $(SRC-CXX) > .depend
	
include .depend

clean: clean-custom
	${RM}  $(OBJ) $(DLL) ${EXPLIB} $(STATICLIB)

$(DLL): $(OBJ)
	dllwrap.exe \
		--mno-cygwin \
		--def $(DEFFILE) \
		--output-exp ${EXPLIB} \
		--driver-name c++ -L/usr/local/lib -L/usr/lib/mingw \
		--implib $(STATICLIB) \
		$(OBJ) $(LDFLAGS) $(LIBS) \
		-o $@

.cpp.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@ 

.rc.res:
	windres.exe  $< -J rc -o $@ -O coff -DMING
