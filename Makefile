
# optional dependencies
USE_DSRPDB=     yes
USE_MMTF=       yes
USE_OPENBABEL=  yes

# general options
USE_EXCEPTIONS= no # exceptions aren't really functional as of yet, and aren't currently needed because all errors are fatal

BROWSER_SUBDIR=	qt5-QtWebEngine-browser
SRCS_CPP=	main.cpp obj.cpp molecule.cpp molecule-xyz.cpp molecule-pdb.cpp util.cpp process.cpp common.cpp Vec3-ext.cpp tm.cpp temp-file.cpp web-io.cpp \
		js-binding.cpp js-support.cpp image.cpp \
		op-rmsd.cpp molecule-qhull.cpp periodic-table-data.cpp binary.cpp structure-db.cpp float-array.cpp \
		linear-algebra.cpp neural-network.cpp
HEADERS=	common.h xerror.h obj.h molecule.h js-binding.h util.h process.h Vec3.h Mat3.h Vec3-ext.h tm.h temp-file.h web-io.h op-rmsd.h periodic-table-data.h \
		structure-db.h stl-ext.h js-support.h mytypes.h
APP=		chemwiz
APPS=		$(APP) $(BROWSER_SUBDIR)/browser
CXX?=		c++
CFLAGS=		-O3 -Wall -Wconditional-uninitialized $(shell pkg-config --static --cflags mujs) -DPROGRAM_NAME=\"ChemWiz\"
CFLAGS+=	-Icontrib/date/include/date
CFLAGS+=	-I/usr/local/include/minidnn
CFLAGS+=	-I/usr/local/include/eigen3/
CXXFLAGS=	$(CFLAGS) -std=c++17
DEP_FILES=	$(SRCS_CPP:%.cpp=%.d)
DEP_FILES_MASK=	*.d

# for MuJS
LDFLAGS+=	$(shell pkg-config --static --libs-only-L mujs gl glfw3)
LDLIBS=		$(shell pkg-config --static --libs-only-l mujs gl glfw3)

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
CXXFLAGS+=	-DUSE_OPENBABEL $(shell pkg-config --cflags openbabel-3)
LDFLAGS+=	$(shell pkg-config --libs openbabel-3)
endif

ifeq ($(USE_EXCEPTIONS), yes)
CXXFLAGS+=	-DUSE_EXCEPTIONS
endif

# for libqhull (from the qhull package)
LDFLAGS+=	-lqhull_r
# for boost compressor/recompressor
LDFLAGS+=	-lboost_iostreams -lz
# for OpenSSL used to access https URLs
LDFLAGS+=	-lssl -lcrypto -pthread
# for threads
LDFLAGS+=	-pthread

# for OpenBLAS that is needed for the rmsd library
LDFLAGS+=	-lopenblas
LDFLAGS+=	-Wl,-rpath=/usr/local/lib/gcc9 /usr/local/lib/gcc9/libgcc_s.so # FreeBSD-specific

OBJS:=		$(SRCS_CPP:.cpp=.o) $(SRCS_C:.c=.o)

.PHONY: all clean clean-deps

#
# build rules
#
all: $(APPS)

$(APP): $(OBJS)
	$(CXX) -o $(APP) $(OBJS) $(LDFLAGS) $(LDLIBS)

$(BROWSER_SUBDIR)/browser: $(BROWSER_SUBDIR)/browser.pro $(BROWSER_SUBDIR)/main.cpp
	cd $(BROWSER_SUBDIR) && CXX=$(CXX) qmake && $(MAKE)

#
# auto-dependencies
#
%.d: %.cpp
	@echo "scanning dependencies for $<"
	@$(CXX) -M $< $(CXXFLAGS) > $@

%.o: %.cpp %.d

#
# other targets
#
test:
	./$(APP) qa/run-all-tests.js

clean:
	rm -f $(OBJS) $(BROWSER_SUBDIR)/main.o $(APPS) $(DEP_FILES_MASK)

clean-deps:
	rm -f $(DEP_FILES)

-include $(DEP_FILES)
