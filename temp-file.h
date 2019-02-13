#pragma once

#include "obj.h"

#include <string>

// TempFile: temporary file that is created by some commands to pass large data to other commands
//           and deleted once the data isn't needed any more

class TempFile : public Obj {
  std::string      fullPath; // full file name with path
public:
  TempFile(const std::string &fileName = "", const std::string &content = "");
  ~TempFile();

  const std::string getFname() const {return fullPath;}

private:
  static std::string genName(const std::string &fileName);
}; // TempFile

