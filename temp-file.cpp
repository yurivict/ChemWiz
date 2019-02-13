#include "temp-file.h"

#include <unistd.h>

#include <boost/format.hpp>

#include <cstdio>

static std::string dirLocation = "/tmp";    // where these files are created
static const char *deftFileName = "file";   // serial number of the file
static unsigned sno = 0;                    // serial number of the file (XXX thread unsafe)

TempFile::TempFile(const std::string &fileName) : fullPath(genName(fileName)) {
}

TempFile::~TempFile() {
  // the file may or may not exist
  (void)std::remove(fullPath.c_str()); // ignore the error code
}

/// intenals

std::string TempFile::genName(const std::string &fileName) {
  return str(boost::format("%1%/%2%-%3%-pid%4%-sno%5%.tmp") %
    dirLocation %
    PROGRAM_NAME %
    (fileName.empty() ? deftFileName : fileName.c_str()) %
    ::getpid() %
    ++sno);
}

