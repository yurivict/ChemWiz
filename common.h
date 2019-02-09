#pragma once

#include <cmath>
//#include <map>
//#include <string>

typedef double Float;

[[ noreturn ]] void unreachable();
#define warning(msg...) {std::cerr << "warning: " << msg << std::endl;}

inline bool isFloatEqual(Float f1, Float f2) {
  return fabs(f1 - f2) < 0.001;
}

//template<typename K, typename V>
//inline std::map<K,V> operator+(const std::map<K,V> &m1, const std::map<K,V> &m2) {
//  auto m = m1;
//  for (auto i : m2)
//    m[i.first] = i.second;
//  return m;
//}

template<template<typename _K, typename _V, class _C, class _A> class Map, typename K, typename V, class C, class A>
inline Map<K,V,C,A> operator+(const Map<K,V,C,A> &m1, const Map<K,V,C,A> &m2) {
  auto m = m1;
  for (auto i : m2)
    m[i.first] = i.second;
  return m;
}

