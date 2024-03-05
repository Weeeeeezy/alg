# vim:ts=2:noet
ENV        = /opt/env-GCC
CXX        = /usr/bin/g++
OPT        = -O3
DBG        = -O0 -g
VPATH      = $(DEVTOP)/__Build__/Predictors
MKL_LIBS   = /opt/intel/mkl/lib/intel64_lin
INTEL_LIBS = /opt/intel/lib/intel64_lin
MKL_INCS   = /opt/intel/mkl/include

CXXFLAGS   = -Wall -std=gnu++14 -march=native -fopenmp \
					   -I$(ENV)/include -isystem $(ENV)/include  \
					   -L$(ENV)/lib  -L$(VPATH)									 \
					   -I$(DEVTOP)/MAQUETTE/3rdParty           \
					   -isystem $(DEVTOP)/MAQUETTE/3rdParty    \
             -I$(DEVTOP)/MAQUETTE/UHFT							 \
					   -I$(MKL_INCS) -isystem $(MKL_INCS)

HDRS     = Predictors.h

LDFLAGS  = -L$(MKL_LIBS)    -Wl,-rpath=$(MKL_LIBS)     \
					 -L$(INTEL_LIBS)  -Wl,-rpath=$(INTEL_LIBS)   \
					 -lmkl_intel_lp64 -lmkl_intel_thread -lmkl_core -liomp5

all: ShortPred ShortPred.d

ShortPred:      ShortPred.cpp libPredictors.a
	$(CXX) -o $(VPATH)/ShortPred   $(CXXFLAGS) $(OPT) $(LDFLAGS) \
	ShortPred.cpp -lPredictors

ShortPred.d:    ShortPred.cpp libPredictors.d.a
	$(CXX) -o $(VPATH)/ShortPred.d $(CXXFLAGS) $(DBG) $(LDFLAGS) \
	ShortPred.cpp -lPredictors.d

libPredictors.a:   Predictors1.o   Predictors2.o   utxx_error.o
	cd $(VPATH) && ar rv $@ Predictors1.o   Predictors2.o   utxx_error.o

libPredictors.d.a: Predictors1.d.o Predictors2.d.o utxx_error.o
	cd $(VPATH) && ar rv $@ Predictors1.d.o Predictors2.d.o utxx_error.o

Predictors1.o:   Predictors1.cpp $(HDRS)
	$(CXX) -c -o $(VPATH)/Predictors1.o   $(CXXFLAGS) $(OPT) Predictors1.cpp

Predictors1.d.o: Predictors1.cpp $(HDRS)
	$(CXX) -c -o $(VPATH)/Predictors1.d.o $(CXXFLAGS) $(DBG) Predictors1.cpp
 
Predictors2.o:   Predictors2.cpp $(HDRS)
	$(CXX) -c -o $(VPATH)/Predictors2.o   $(CXXFLAGS) $(OPT) Predictors2.cpp

Predictors2.d.o: Predictors2.cpp $(HDRS)
	$(CXX) -c -o $(VPATH)/Predictors2.d.o $(CXXFLAGS) $(DBG) Predictors2.cpp
 
utxx_error.o:   $(DEVTOP)/MAQUETTE/3rdParty/utxx/error.cpp
	$(CXX) -c -o $(VPATH)/utxx_error.o   $(CXXFLAGS) $(OPT) $<

clean:
	rm -fr $(VPATH)/*

