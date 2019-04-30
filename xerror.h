#pragma once

#if defined(USE_EXCEPTIONS)

#include <exception>
#include <string>

#include <errno.h>

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

#include <rang.hpp>

#define ERROR(msg...) {std::cerr << ::rang::fg::red << msg << ::rang::style::reset << std::endl; std::exit(1);}

#endif

// syscall failure reporting
#define ERROR_SYSCALL(syscall) ERROR("system call '" << #syscall << "' failed: " << strerror(errno))
