#pragma once

// StlExt module contains simple extentions of stl types

#include <vector>

namespace std_ext {

template<class T, class Allocator = std::allocator<T>>
class vector_range {
  typedef std::vector<T, Allocator> vec_type;
  typedef typename vec_type::const_iterator const_iterator;
  const vec_type *vec;
   // range
  const_iterator it1;
  const_iterator it2;
public:
  vector_range(const vec_type *newVec, const_iterator newIt1, const_iterator newIt2) : vec(newVec), it1(newIt1), it2(newIt2) { }
  const_iterator begin() const {return it1;}
  const_iterator end() const {return it2;}
}; // vector_range

}; // std_ext
