#include "process.h"
#include "tm.h"
#include "util.h"
#include "misc.h"
#include "xerror.h"

#include <errno.h>

#include <rang.hpp>

#include <iostream>
#include <string>
#include <array>
#include <memory>
#include <stdexcept>
#include <vector>

static bool ctlParamLogCommands = false;

std::string Process::exec(const std::string &cmd) {
  if (ctlParamLogCommands)
    std::cout << rang::fg::cyan << "[" << Tm::strYearToMicrosecond() << "] LOG(process.command): " << cmd << rang::style::reset << std::endl;
  std::string result;
  std::array<char, 128> buffer;

  // open the process
  FILE *pipe = ::popen(cmd.c_str(), "r");
  if (!pipe)
    throw std::runtime_error("popen() failed, can't allocate memory");

  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
    result += buffer.data();

  // close the process
  int res = ::pclose(pipe);
  if (res == -1)
    throw std::runtime_error(STR("process failed, wait4 or other failure caused pclose() to fail: " << strerror(errno) << " (errno=" << errno << ")"));
  if (res != 0)
    throw std::runtime_error(STR("process failed with the error code " << res));


  return result;
}

void Process::setCtlParam(const std::vector<std::string> &name, const std::string &value) {
  if (name.size() != 1)
    ERROR("Process::setCtlParam: name should have just one part")
  if (name[0] == "log-commands")
    ctlParamLogCommands = Util::strAsBool(value);
  else
    ERROR("TempFile::setCtlParam: unknown parameter '" << name[0] << "'")
}
