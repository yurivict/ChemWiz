
# optional dependencies
USE_DSRPDB=     yes
USE_MMTF=       yes

# general options
USE_EXCEPTIONS= no # exceptions aren't really functional as of yet, and aren't currently needed because all errors are fatal

SRCS_CPP=	main.cpp obj.cpp molecule.cpp js-binding.cpp calc-engine-erkale.cpp process.cpp common.cpp Vec3-ext.cpp tm.cpp temp-file.cpp
HEADERS=	common.h xerror.h obj.h molecule.h js-binding.h calculators.h util.h process.h Vec3.h Mat3.h Vec3-ext.h tm.h temp-file.h
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

ifeq ($(USE_MMTF), yes)
SRCS_CPP+=	molecule-mmtf.cpp
CXXFLAGS+=	-DUSE_MMTF
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
