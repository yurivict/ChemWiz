#include "process.h"

#include <string>
#include <array>
#include <memory>
#include <stdexcept>

std::string Process::exec(const std::string &cmd) {
  std::string result;
  std::array<char, 128> buffer;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
  if (!pipe)
    throw std::runtime_error("popen() failed!");

  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
    result += buffer.data();

  return result;
}

