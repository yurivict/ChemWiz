#pragma once

#include <string>

namespace WebIo {
  std::string download(const std::string &host, const std::string &service, const std::string &target);
  std::string downloadUrl(const std::string &url);
}; // WebIo
