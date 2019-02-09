#pragma once

#include <exception>
#include <string>

class Exception : public std::exception {
  std::string msg;
public:
  Exception(const std::string &newMsg) : msg(newMsg) { }
  virtual const char* what() const noexcept {
    return msg.c_str();
  }
}; // Exception

