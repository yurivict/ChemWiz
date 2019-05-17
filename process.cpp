#include "process.h"
#include "tm.h"
#include "util.h"
#include "xerror.h"

#include <iostream>
#include <string>
#include <array>
#include <memory>
#include <stdexcept>
#include <vector>

static bool ctlParamLogCommands = false;

std::string Process::exec(const std::string &cmd) {
  if (ctlParamLogCommands)
    std::cout << "[" << Tm::strYearToMicrosecond() << "] LOG(process.command): " << cmd << std::endl;
  std::string result;
  std::array<char, 128> buffer;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
  if (!pipe)
    throw std::runtime_error("popen() failed!");

  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
    result += buffer.data();

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
