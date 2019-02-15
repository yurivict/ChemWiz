#include "temp-file.h"

#include <unistd.h>

#include <boost/format.hpp>

#include <iostream>
#include <vector>
#include <fstream>
#include <cstdio>
#include <iterator>
#include <stdexcept>
//#include <experimental/filesystem>

static std::string dirLocation = "./";      // where these files are created /tmp breaks std::rename below (cross-device error)
static const char *deftExt = "tmp";         // serial number of the file
static unsigned sno = 0;                    // serial number of the file (XXX thread unsafe)

TempFile::TempFile(const std::string &ext, const std::string &content)
: fullPath(genName(ext))
{
  if (!content.empty())
    std::ofstream(fullPath, std::ios::out) << content;
}

TempFile::~TempFile() {
  // the file may or may not exist
  (void)std::remove(fullPath.c_str()); // ignore the error code
}

std::vector<uint8_t>* TempFile::toBinary() const {
  std::ifstream file(fullPath, std::ios::binary);
  return new std::vector<uint8_t>(std::istreambuf_iterator<char>{file}, {});
}

void TempFile::toPermanent(const std::string &permFileName) const {
  // fails with cross-device error when tmp files are on /tmp
  if (int err = std::rename(fullPath.c_str(), permFileName.c_str()))
    throw std::runtime_error("rename() failed!");

  // not available on FreeBSD: https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=235749
  //std::experimental::filesystem::copy(fullPath, permFileName);
}

/// intenals

std::string TempFile::genName(const std::string &ext) {
  return str(boost::format("%1%/%2%-pid%3%-sno%4%.%5%") %
    dirLocation %
    PROGRAM_NAME %
    ::getpid() %
    ++sno %
    (ext.empty() ? deftExt : ext.c_str()));
}

