#pragma once

#include <string>

// Obj: base for all objects that are bound to JavaScript and primarily used from JS

class Obj {
public:
  Obj();
  ~Obj();
  std::string id() const;
}; // Obj

