
# optional dependencies
USE_DSRPDB=     yes
USE_MMTF=       yes
USE_OPENBABEL=  yes

# general options
USE_EXCEPTIONS= no # exceptions aren't really functional as of yet, and aren't currently needed because all errors are fatal

SRCS_CPP=	main.cpp obj.cpp molecule.cpp molecule-xyz.cpp molecule-pdb.cpp js-binding.cpp process.cpp common.cpp Vec3-ext.cpp tm.cpp temp-file.cpp web-io.cpp \
		op-rmsd.cpp molecule-qhull.cpp periodic-table-data.cpp structure-db.cpp
HEADERS=	common.h xerror.h obj.h molecule.h js-binding.h util.h process.h Vec3.h Mat3.h Vec3-ext.h tm.h temp-file.h web-io.h op-rmsd.h periodic-table-data.h \
		structure-db.h
APP=		chemwiz
CXX?=		clang++80
CFLAGS=		-O3 -Wall $(shell pkg-config --static --cflags mujs) -DPROGRAM_NAME=\"ChemWiz\"
CXXFLAGS=	$(CFLAGS) -std=c++17

# for MuJS
LDFLAGS+=	$(shell pkg-config --static --libs-only-L mujs)
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

ifeq ($(USE_OPENBABEL), yes)
SRCS_CPP+=	molecule-ob.cpp
CXXFLAGS+=	-DUSE_OPENBABEL $(shell pkg-config --cflags openbabel-2.0)
LDFLAGS+=	$(shell pkg-config --libs openbabel-2.0)
endif

ifeq ($(USE_EXCEPTIONS), yes)
CXXFLAGS+=	-DUSE_EXCEPTIONS
endif

# for libqhull
LDFLAGS+=	-lqhull_r
# for boost compressor/recompressor
LDFLAGS+=	-lboost_iostreams -lz
# for OpenSSL used to access https URLs
LDFLAGS+=	$(shell pkg-config --libs openssl) -pthread

# for OpenBLAS that is needed for the rmsd library
LDFLAGS+=	-lopenblas
LDFLAGS+=	-Wl,-rpath=/usr/local/lib/gcc8 /usr/local/lib/gcc8/libgcc_s.so # FreeBSD-specific

OBJS:=		$(SRCS_CPP:.cpp=.o) $(SRCS_C:.c=.o)

$(APP): $(OBJS) $(HEADERS) Makefile
	$(CXX) -o $(APP) $(OBJS) $(LDFLAGS) $(LDLIBS)

$(OBJS): $(HEADERS) Makefile

test:
	./$(APP) qa/run-all-tests.js

clean:
	rm -f $(OBJS) $(APP)
