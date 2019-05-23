#pragma once

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <fstream>
#include <cstring>

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

template<typename C>
std::vector<typename C::value_type> containerToVec(const C &c) {
  std::vector<typename C::value_type> res;
  for (auto i : c)
    res.push_back(i);
  return res;
}

template<typename Csrc, typename Cdst>
Cdst* createContainerFromBufferOfDifferentType(const Csrc &arg) { // ASSUME that binary arrays align to an element of Cdst
  auto res = new Cdst;
  // copy memory
  if (auto memsz = arg.size()*sizeof(typename Csrc::value_type)) {
    res->resize(memsz/sizeof(typename Cdst::value_type));
    std::memcpy(res->data(), arg.data(), memsz);
  }
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
class OnExit { // (!!) should always be used with the 'static' prefix to prevent 'exit' function calls elsewhere from escaping it XXX any better way to enforce this?
  Fn fn;
public:
  OnExit(Fn &&newFn) : fn(newFn) { }
  ~OnExit() {
    fn();
  }
}; // OnExit

inline bool realEof(std::ifstream &f) {
  // to get a reliable reslt from eof() we need to peek() first
  (void)f.peek();
  return f.eof();
}

bool strAsBool(const std::string &str);

// DoubleMap maps objects of two types back and forth
template<typename T1, typename T2>
class DoubleMap {
protected:
  typedef T1 type1;
  typedef T2 type2;
private:
  std::map<T1,T2> m12;
  std::map<T2,T1> m21;
public: // iface
  void add(const T1 &t1, const T2 &t2) {
    m12[t1] = t2;
    m21[t2] = t1;
  }
  const T2* get12(const T1 &t1) const {auto f = m12.find(t1); return f != m12.end() ? &f->second : nullptr;}
  const T1* get21(const T2 &t2) const {auto f = m21.find(t2); return f != m21.end() ? &f->second : nullptr;}
}; // DoubleMap

}; // Util
