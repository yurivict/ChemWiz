#pragma once

#include <string>
#include <vector>

class Process {
public:
  static std::string exec(const std::string &cmd);
  static void setCtlParam(const std::vector<std::string> &name, const std::string &value);
}; // Process

