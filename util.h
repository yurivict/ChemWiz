#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <fstream>

namespace Util {

template<char Sep>
std::vector<std::string> split(const std::string &s) {
  std::stringstream ss(s);
  std::string to;

  std::vector<std::string> res;
  while (std::getline(ss, to, Sep))
    res.push_back(to);
  return res;
}

inline std::vector<std::string> splitLines(const std::string &s) {
  return split<'\n'>(s);
}

inline std::vector<std::string> splitSpaces(const std::string &s) {
  std::vector<std::string> res;
  for (auto &s : split<' '>(s))
    if (!s.empty())
      res.push_back(s);
  return res;
}

inline void writeFile(const std::string &str, const std::string &fname) {
  std::ofstream file(fname, std::ios::out);
  file << str;
}

template<typename Fn>
class OnExit {
  Fn fn;
public:
  OnExit(Fn &&newFn) : fn(newFn) { }
  ~OnExit() {
    fn();
  }
}; // OnExit


}; // Util
