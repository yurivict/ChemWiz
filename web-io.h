#pragma once

#include <string>
#include <vector>

namespace WebIo {
  // types
  typedef std::vector<uint8_t> Binary;
  // declarations
  Binary* download(const std::string &host, const std::string &service, const std::string &target);
  Binary* downloadUrl(const std::string &url);
}; // WebIo
