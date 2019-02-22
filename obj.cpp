#include "obj.h"

#include <boost/format.hpp>

#include <string>

Obj::Obj() {
}

Obj::~Obj() {
}

std::string Obj::id() const {
  return str(boost::format("%1%") % this);
}
