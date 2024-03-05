# vim:ts=2:noet
CXX      = g++
OPT      = -O3
DBG   	 = -O0 -g
CXXFLAGS = -Wall -Wextra -std=gnu++17 \
					 -march=native -mtune=native -mcmodel=large -fopenmp \
					 -I$(DEVTOP)/MAQUETTE
VPATH    = $(DEVTOP)/__Build__/MOEX-Lead-Lag

HDRS     = MLL.hpp TickDataReader.hpp HY.hpp OtherMethods.hpp

all:   MLL MLL.d

MLL:   MLL.cpp $(HDRS)
	$(CXX) -o $(VPATH)/MLL   $(CXXFLAGS) $(OPT) $<

MLL.d: MLL.cpp $(HDRS)
	$(CXX) -o $(VPATH)/MLL.d $(CXXFLAGS) $(DBG) $<

clean:
	rm -fr $(VPATH)/MLL*

