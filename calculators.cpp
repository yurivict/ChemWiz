#include "calculators.h"

#include <string>
#include <map>
#include <functional>

namespace Calculators {

// the registry of available CalcEngine types
class Registry : public std::map<std::string, std::function<Engine*()>> {
 typedef mapped_type Fn;
public:
  Registry() {
    // fill the registry
    auto &rr = *this;
    rr["erkale"] = Fn([]() {return new engines::Erkale;});
  }
  Fn get(const std::string &kind) const {
    auto i = find(kind);
    return i != end() ? i->second : Fn([]() {return nullptr;});
  }
}; // Registry

static Registry registry;

Engine* Engine::create(const std::string &kind) {
  return registry.get(kind)();
}

}
