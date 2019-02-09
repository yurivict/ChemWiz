

SRCS_CPP=	main.cpp molecule.cpp molecule-dsrpdb.cpp js-binding.cpp calc-engine-erkale.cpp process.cpp common.cpp Vec3-ext.cpp tm.cpp
HEADERS=	common.h Exception.h Mat3.h molecule.h Vec3.h js-binding.h calculators.h util.h process.h Vec3-ext.h tm.h
APP=		chemwiz
CXX?=		clang++80
CC?=		clang80
CFLAGS=		-O3 -Wall -I/usr/local/include -DPROGRAM_NAME=\"ChemWiz\"
CXXFLAGS=	$(CFLAGS) -std=c++17
LDFLAGS=	-L/usr/local/lib -lm -ldsrpdb -lmujs

OBJS:=		$(SRCS_CPP:.cpp=.o) $(SRCS_C:.c=.o)

$(APP): $(OBJS) $(HEADERS) Makefile
	$(CXX) -o $(APP) $(OBJS) $(LDFLAGS)

$(OBJS): $(HEADERS) Makefile
