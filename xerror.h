#pragma once

#if defined(USE_EXCEPTIONS)

#include <exception>
#include <string>

class Exception : public std::exception {
  std::string msg;
public:
  Exception(const std::string &newMsg) : msg(newMsg) {
  }
  virtual const char* what() const noexcept {
    return msg.c_str();
  }
}; // Exception

#define ERROR(msg...) {std::ostringstream __ss__; __ss__ << msg; throw Exception(__ss__.str());}

#else // no exceptions

#include <cstdlib>
#include <iostream>

#define ERROR(msg...) {std::cerr << msg << std::endl; std::exit(1);}

#endif
