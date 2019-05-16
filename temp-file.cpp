#include "temp-file.h"
#include "util.h"
#include "tm.h"
#include "xerror.h"

#include <unistd.h>
#include <rang.hpp>

#include <boost/format.hpp>

#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <fstream>
#include <cstdio>
#include <iterator>
#include <stdexcept>
//#include <experimental/filesystem>

static std::string dirLocation = ".";       // where these files are created /tmp breaks std::rename below (cross-device error)
static const char *deftExt = "tmp";         // default file extension
static unsigned sno = 0;                    // serial number of the file (XXX thread unsafe)

static bool ctlParamKeepFiles = false;

class TempFile__KeptFileRegistry : public std::vector<std::string> {
public:
  TempFile__KeptFileRegistry() { }
  ~TempFile__KeptFileRegistry() {
    if (!empty()) {
      std::ostringstream ssRmShellFileName;
      ssRmShellFileName << "temp-files-to-remove-" << Tm::strYearToSecond() << ".sh";
      std::ofstream rmShellFile(ssRmShellFileName.str(), std::ios::trunc);
      rmShellFile << "#!/bin/sh" << std::endl;
      rmShellFile << std::endl;
      for (auto f : *this)
        rmShellFile << "rm " << f << std::endl;
      rmShellFile.close();
      std::cout << rang::fg::yellow << "Some TempFile objects weren't removed from disk (" << size() << " of them),"
                                    << " please run the script to remove them: " << ssRmShellFileName.str()
                                    << rang::style::reset << std::endl;
    }
  }
}; // TempFile__KeptFileRegistry

static TempFile__KeptFileRegistry keptFileRegistry;

TempFile::TempFile(const std::string &ext, const std::string &content)
: fullPath(genName(ext)),
  madePermanent(false)
{
  if (!content.empty())
    std::ofstream(fullPath, std::ios::out) << content;
}

TempFile::~TempFile() {
  if (!madePermanent) {
    // the file may or may not exist
    if (!ctlParamKeepFiles)
      (void)std::remove(fullPath.c_str()); // ignore the error code
    else
      keptFileRegistry.push_back(fullPath);
  }
}

std::vector<uint8_t>* TempFile::toBinary() const {
  std::ifstream file(fullPath, std::ios::binary);
  return new std::vector<uint8_t>(std::istreambuf_iterator<char>{file}, {});
}

void TempFile::toPermanent(const std::string &permFileName) const {
  if (madePermanent)
    ERROR("temp-file can only be made permanent once!")
  // fails with cross-device error when tmp files are on /tmp
  if (int err = std::rename(fullPath.c_str(), permFileName.c_str()))
    throw std::runtime_error("rename() failed!");
  madePermanent = true;

  // not available on FreeBSD: https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=235749
  //std::experimental::filesystem::copy(fullPath, permFileName);
}

void TempFile::setCtlParam(const std::vector<std::string> &name, const std::string &value) {
  if (name.size() != 1)
    ERROR("TempFile::setCtlParam: name should have just one part")
  if (name[0] == "keep-files")
    ctlParamKeepFiles = Util::strAsBool(value);
  else
    ERROR("TempFile::setCtlParam: unknown parameter '" << name[0] << "'")
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

