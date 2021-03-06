#pragma once

#include "obj.h"
#include "mytypes.h"

#include <string>
#include <vector>

// TempFile: temporary file that is created by some commands to pass large data to other commands
//           and deleted once the data isn't needed any more

class TempFile : public Obj {
  std::string      fullPath; // full file name with path
  mutable bool     madePermanent;
public:
  TempFile(const std::string &ext, const std::string &content);
  TempFile(const std::string &ext, const Binary *content);
  ~TempFile();

  const std::string getFname() const {return fullPath;}
  std::vector<uint8_t>* toBinary() const; // converts the file content into the memory block
  void toPermanent(const std::string &permFileName) const; // makes temporary file permanent (moves it)
  static void setCtlParam(const std::vector<std::string> &name, const std::string &value);

private:
  static std::string genName(const std::string &ext);
}; // TempFile

