
USE_DSRPDB=     yes
USE_EXCEPTIONS= no


SRCS_CPP=	main.cpp molecule.cpp js-binding.cpp calc-engine-erkale.cpp process.cpp common.cpp Vec3-ext.cpp tm.cpp
HEADERS=	common.h xerror.h Mat3.h molecule.h Vec3.h js-binding.h calculators.h util.h process.h Vec3-ext.h tm.h
APP=		chemwiz
CXX?=		clang++80
CFLAGS=		-O3 -Wall $(shell pkg-config --static --cflags mujs) -DPROGRAM_NAME=\"ChemWiz\"
CXXFLAGS=	$(CFLAGS) -std=c++17
LDFLAGS=	$(shell pkg-config --static --libs-only-L mujs)
LDLIBS=		$(shell pkg-config --static --libs-only-l mujs)

ifeq ($(USE_DSRPDB), yes)
SRCS_CPP+=	molecule-dsrpdb.cpp
CXXFLAGS+=	-DUSE_DSRPDB
LDFLAGS+=	-ldsrpdb
endif

ifeq ($(USE_EXCEPTIONS), yes)
CXXFLAGS+=	-DUSE_EXCEPTIONS
endif

OBJS:=		$(SRCS_CPP:.cpp=.o) $(SRCS_C:.c=.o)

$(APP): $(OBJS) $(HEADERS) Makefile
	$(CXX) -o $(APP) $(OBJS) $(LDFLAGS) $(LDLIBS)

$(OBJS): $(HEADERS) Makefile

clean:
	rm -f $(OBJS) $(APP)
