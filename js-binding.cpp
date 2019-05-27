
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#include <fstream>
#include <sstream>
#include <map>
#include <cmath>
#include <memory>

#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>

#include <mujs.h>
#include <rang.hpp>

#include "js-binding.h"
#include "obj.h"
#include "molecule.h"
#include "temp-file.h"
#include "structure-db.h"
#include "tm.h"
#include "process.h"
#include "web-io.h"
#include "op-rmsd.h"
#include "periodic-table-data.h"
#include "Vec3.h"
#include "Vec3-ext.h"
#include "util.h"

// types defined locally
typedef std::vector<uint8_t> Binary;

static const char *TAG_Obj         = "Obj";
static const char *TAG_Binary      = "Binary";
static const char *TAG_Molecule    = "Molecule";
static const char *TAG_Atom        = "Atom";
static const char *TAG_TempFile    = "TempFile";
static const char *TAG_StructureDb = "StructureDb";

static void ckErr(js_State *J, int err) {
  if (err) {
    std::cerr << js_trystring(J, -1, "Error") << std::endl;
    js_pop(J, 1);
    abort();
  }
}

#define AssertNargs(n)           assert(js_gettop(J) == n+1);
#define AssertNargsRange(n1,n2)  assert(n1+1 <= js_gettop(J) && js_gettop(J) <= n2+1);
#define AssertNargs2(nmin,nmax)  assert(nmin+1 <= js_gettop(J) && js_gettop(J) <= nmax+1);
#define AssertStack(n)           assert(js_gettop(J) == n);
#define DbgPrintStackLevel(loc)  std::cout << "DBG JS Stack: @" << loc << " level=" << js_gettop(J) << std::endl
#define DbgPrintStackObject(loc, idx) std::cout << "DBG JS Stack: @" << loc << " stack[" << idx << "]=" << js_tostring(J, idx) << std::endl
#define DbgPrintStack(loc)       std::cout << "DBG JS Stack: @" << loc << ">>>" << std::endl; \
                                 for (int i = 1; i < js_gettop(J); i++) \
                                   std::cout << "  -- @idx=" << -i << ": " << js_tostring(J, -i)  << std::endl; \
                                 std::cout << "DBG JS Stack: @" << loc << "<<<" << std::endl; \
                                 abort(); // because DbgPrintStack damages stack by converting all values to strings
#define GetNArgs()               (js_gettop(J)-1)
#define GetArg(type, n)          ((type*)js_touserdata(J, n, TAG_##type))
#define GetArgZ(type, n)         (!js_isundefined(J, n) ? (type*)js_touserdata(J, n, TAG_##type) : nullptr)
#define GetArgBoolean(n)         js_toboolean(J, n)
#define GetArgFloat(n)           js_tonumber(J, n)
#define GetArgString(n)          std::string(js_tostring(J, n))
#define GetArgStringCptr(n)      js_tostring(J, n)
#define GetArgInt32(n)           js_toint32(J, n)
#define GetArgUInt32(n)          js_touint32(J, n)
#define GetArgSSMap(n)           objToMap(J, n)
#define GetArgVec3(n)            objToVec3(J, n, __func__)
#define GetArgUnsignedArray(n)      objToTypedArray<unsigned,false>(J, n, __func__)
#define GetArgUnsignedArrayZ(n)     objToTypedArray<unsigned,true>(J, n, __func__)
#define GetArgFloatArray(n)         objToTypedArray<double, false>(J, n, __func__)
#define GetArgFloatArrayZ(n)        objToTypedArray<double, true>(J, n, __func__)
#define GetArgStringArray(n)        objToTypedArray<std::string,false>(J, n, __func__)
#define GetArgStringArrayZ(n)       objToTypedArray<std::string,true>(J, n, __func__)
#define GetArgUnsignedArrayArray(n) objToTypedArray<std::vector<unsigned>>(J, n, __func__)
#define GetArgFloatArrayArray(n)    objToTypedArray<std::vector<double>>(J, n, __func__)
#define GetArgStringArrayArray(n)   objToTypedArray<std::vector<std::string>>(J, n, __func__)
#define GetArgMat3x3(n)          objToMat3x3(J, n, __func__)
#define GetArgMatNxX(n,N)        objToMatNxX<N>(J, n)
#define GetArgElement(n)         Element(PeriodicTableData::get().elementFromSymbol(GetArgString(n)))
#define GetArgPtr(n)             StrPtr::s2p(GetArgString(n))
#define StackPopPrevious(n)      {js_rot2(J); js_pop(J, 1);}

// add method defined by the C++ code
#define ADD_METHOD_CPP(cls, method, nargs) \
  AssertStack(2); \
  js_newcfunction(J, prototype::method, #cls ".prototype." #method, nargs); /*PUSH a function object wrapping a C function pointer*/ \
  js_defproperty(J, -2, #method, JS_DONTENUM); /*POP a value from the top of the stack and set the value of the named property of the object (in prototype).*/ \
  AssertStack(2);

// add method defined in JavaScript
#define ADD_METHOD_JS(cls, method, code...) \
  js_dostring(J, str(boost::format("%1%.prototype['%2%'] = %3%") % #cls % #method % #code).c_str());

static void InitObjectRegistry(js_State *J, const char *tag) {
  js_newobject(J);
  js_setregistry(J, tag);
}

#define ADD_JS_CONSTRUCTOR(cls) /*@lev=1, a generic constructor, nargs=0 below doesn't seem to enforce number of args, or differentiate constructors by number of args*/ \
  js_getregistry(J, TAG_##cls);                                               /*@lev=2*/ \
  js_newcconstructor(J, Js##cls::xnew, Js##cls::xnew, TAG_##cls, 0/*nargs*/); /*@lev=2*/ \
  js_defproperty(J, -2, TAG_##cls, JS_DONTENUM);                              /*@lev=1*/

namespace MuJS {

// code borrowed from the MuJS sources

static void jsB_propf(js_State *J, const char *name, js_CFunction cfun, int n) {
  const char *pname = strrchr(name, '.');
  pname = pname ? pname + 1 : name;
  js_newcfunction(J, cfun, name, n);
  js_defproperty(J, -2, pname, JS_DONTENUM);
}

} // MuJS

namespace JsBinding {

//
// helper classes and functions
//

class StrPtr { // class that maps pointer<->string for the purpose of passing pointers to JS
public:
  static void* s2p(const std::string &s) {
    void *ptr = 0;
    ::sscanf(s.c_str(), "%p", &ptr);
    return ptr;
  }
  static std::string p2s(void *ptr) {
    char buf[sizeof(void*)*2+1];
    ::sprintf(buf, "%p", ptr);
    return buf;
  }
}; // StrPtr

static void binaryFinalize(js_State *J, void *p) {
  delete (Binary*)p;
}

static void moleculeFinalize(js_State *J, void *p) {
  delete (Molecule*)p;
}

static void atomFinalize(js_State *J, void *p) {
  auto a = (Atom*)p;
  // delete only unattached atoms, otherwise they are deteled by their Molecule object
  if (!a->molecule)
    delete a;
}

static void tempFileFinalize(js_State *J, void *p) {
  delete (TempFile*)p;
}

template<typename A, typename FnNew>
static void returnArrayOfUserData(js_State *J, const A &arr, const char *tag, void(*finalize)(js_State *J, void *p), FnNew fnNew) {
  js_newarray(J);
  unsigned idx = 0;
  for (auto a : arr) {
    fnNew(J, a);
    js_setindex(J, -2, idx++);
  }
}

template<typename A, typename FnNew>
static void returnArrayOfArrayOfUserData(js_State *J, const A &arr, const char *tag, void(*finalize)(js_State *J, void *p), FnNew fnNew) {
  js_newarray(J);
  unsigned idx = 0;
  for (auto &a : arr) {
    returnArrayOfUserData<typename A::value_type>(J, a, tag, finalize, fnNew);
    js_setindex(J, -2, idx++);
  }
}

static std::map<std::string,std::string> objToMap(js_State *J, int idx) {
  std::map<std::string,std::string> mres;
  if (!js_isobject(J, idx))
    js_typeerror(J, "objToMap: not an object");
  js_pushiterator(J, idx/*idx*/, 1/*own*/);
  const char *key;
  while ((key = js_nextiterator(J, -1))) {
    js_getproperty(J, idx, key);
    if (!js_isstring(J, -1))
      js_typeerror(J, "objToMap: value isn't a string");
    const char *val = js_tostring(J, -1);
    mres[key] = val;
    js_pop(J, 1);
  }
  return mres;
}

static Vec3 objToVec3(js_State *J, int idx, const char *fname) {
  if (!js_isarray(J, idx))
    js_typeerror(J, "Vec3: not an array in arg#%d of the function '%s'", idx, fname);
  if (js_getlength(J, idx) != 3)
    js_typeerror(J, "Vec3: array size isn't 3 (len=%d) in arg#%d of the function '%s'", js_getlength(J, idx), idx, fname);

  Vec3 v;

  for (unsigned i = 0; i < 3; i++) {
    js_getindex(J, idx, i);
    v[i] = js_tonumber(J, -1);
    js_pop(J, 1);
  }

  return v;
}

// templetize js_toxx() functions as Jsx<type>::totype(J, idx)

template<typename T> struct Jsx {static T totype(js_State *J, int idx);};
template<> struct Jsx<unsigned> {
static unsigned totype(js_State *J, int idx) {
  auto i = js_tointeger(J, idx);
  if (i < 0)
    ERROR("expect unsigned, got a negative value " << i)
  return (unsigned)i;
}};
template<> struct Jsx<double> {static double totype(js_State *J, int idx) {return js_tonumber(J, idx);}};
template<> struct Jsx<std::string> {static std::string totype(js_State *J, int idx) {return js_tostring(J, idx);}};
template<typename T> struct Jsx<std::vector<T>> {
static std::vector<T> totype(js_State *J, int idx) {
  std::vector<T> v;
  for (unsigned i = 0, len = js_getlength(J, idx); i < len; i++) {
    js_getindex(J, idx, i);
    v.push_back(Jsx<T>::totype(J, -1));
    js_pop(J, 1);
  }
  return v;
}};

template<typename T, bool AllowMissingAll = false, bool AllowMissingElt = false>
static std::vector<T> objToTypedArray(js_State *J, int idx, const char *fname) {
  // checks
  if (AllowMissingAll && js_isundefined(J, idx))
    return std::vector<T>();
  if (!js_isarray(J, idx))
    js_typeerror(J, "not an array in arg#%d of the function '%s'", idx, fname);

  // read the array
  std::vector<T> v;
  for (unsigned i = 0, len = js_getlength(J, idx); i < len; i++) {
    js_getindex(J, idx, i);
    if (AllowMissingElt && js_isundefined(J, -1))
      v.push_back(T());
    else
      v.push_back(Jsx<T>::totype(J, -1));
    js_pop(J, 1);
  }

  return v;
}

template<unsigned N>
static void objToVecVa(js_State *J, int idx, std::valarray<double> *va, unsigned vaIdx) {
  if (!js_isarray(J, idx))
    js_typeerror(J, "objToVec: not an array");
  if (js_getlength(J, idx) != N)
    js_typeerror(J, "Vec3: array size isn't %d (len=%d)", N, js_getlength(J, idx));

  for (unsigned i = 0; i < N; i++) {
    js_getindex(J, idx, i);
    (*va)[vaIdx + i] = js_tonumber(J, -1);
    js_pop(J, 1);
  }
}

static Mat3 objToMat3x3(js_State *J, int idx, const char *fname) {
  if (!js_isarray(J, idx))
    js_typeerror(J, "objToMat3x3: not an array in arg#%d of the function '%s'", idx, fname);
  if (js_getlength(J, idx) != 3)
    js_typeerror(J, "objToMat3x3: array size isn't 3 (len=%d) in arg#%d of the function '%s'", js_getlength(J, idx), idx, fname);

  Mat3 m;

  for (unsigned i = 0; i < 3; i++) {
    js_getindex(J, idx, i);
    m[i] = objToVec3(J, -1, __func__);
    js_pop(J, 1);
  }

  return m;
}

template<unsigned N>
static std::valarray<double>* objToMatNxX(js_State *J, int idx) {
  if (!js_isarray(J, idx))
    js_typeerror(J, "MatNxX: not an array (N=%d)", N);
  auto len = js_getlength(J, idx);

  auto m = new std::valarray<double>;
  m->resize(len*N);

  for (unsigned i = 0, vaIdx = 0; i < len; i++, vaIdx += N) {
    js_getindex(J, idx, i);
    objToVecVa<N>(J, -1, m, vaIdx);
    js_pop(J, 1);
  }

  return m;
}

// Return helpers

static void Push(js_State *J, const bool &val) {
  js_pushboolean(J, val ? 1 : 0);
}

static void Push(js_State *J, const Float &val) {
  js_pushnumber(J, val);
}

static void Push(js_State *J, const unsigned &val) {
  js_pushnumber(J, val);
}

static void Push(js_State *J, const int &val) {
  js_pushnumber(J, val);
}

static void Push(js_State *J, const std::string &val) {
  js_pushstring(J, val.c_str());
}

static void Push(js_State *J, const Vec3 &val) {
  js_newarray(J);
  unsigned idx = 0;
  for (auto c : val) {
    js_pushnumber(J, c);
    js_setindex(J, -2, idx++);
  }
}

static void Push(js_State *J, const Mat3 &val) {
  js_newarray(J);
  unsigned idx = 0;
  for (auto &r : val) {
    Push(J, r);
    js_setindex(J, -2, idx++);
  }
}

static void Push(js_State *J, void* const &val) {
  Push(J, StrPtr::p2s(val));
}

template<typename T, std::size_t SZ>
static void Push(js_State *J, const std::array<T, SZ> &val) {
  js_newarray(J);
  unsigned idx = 0;
  for (auto const &v : val) {
    Push(J, v);
    js_setindex(J, -2, idx++);
  }
}

template<typename T>
static void Push(js_State *J, const std::vector<T> &val) {
  js_newarray(J);
  unsigned idx = 0;
  for (auto const &v : val) {
    Push(J, v);
    js_setindex(J, -2, idx++);
  }
}

template<typename T>
static void Return(js_State *J, const T &val) {
  Push(J, val);
}

static void ReturnVoid(js_State *J) {
  js_pushundefined(J);
}

//static void ReturnNull(js_State *J) {
//  js_pushnull(J);
//}

// convenience macro to return objects
#define ReturnObj(type, v) Js##type::xnewo(J, v)
#define ReturnObjZ(type, v) Js##type::xnewoZ(J, v)

//
// Define object types
//

namespace JsObj {

static void xnewo(js_State *J, Obj *o) {
  js_getglobal(J, TAG_Obj);
  js_getproperty(J, -1, "prototype");
  js_newuserdata(J, TAG_Obj, o, 0/*finalize*/);
}

static void xnew(js_State *J) {
  AssertNargs(0)
  ReturnObj(Obj, new Obj);
}

namespace prototype {

static void id(js_State *J) {
  AssertNargs(0)
  Return(J, GetArg(Obj, 0)->id());
}

}

static void init(js_State *J) {
  InitObjectRegistry(J, TAG_Obj);
  js_pushglobal(J);
  ADD_JS_CONSTRUCTOR(Obj)
  js_getglobal(J, TAG_Obj);
  js_getproperty(J, -1, "prototype");
  StackPopPrevious()
  { // methods
    ADD_METHOD_CPP(Obj, id, 0)
  }
  js_pop(J, 2);
  AssertStack(0);
}

} // JsObj

namespace JsBinary {

static void xnewo(js_State *J, Binary *b) {
  js_getglobal(J, TAG_Binary);
  js_getproperty(J, -1, "prototype");
  js_newuserdata(J, TAG_Binary, b, binaryFinalize);
}

static void xnew(js_State *J) {
  AssertNargs(1)
  auto b = new Binary;
  auto str = GetArgStringCptr(1);
  auto strLen = ::strlen(str);
  b->resize(strLen);
  ::memcpy(&(*b)[0], str, strLen); // FIXME inefficient copying char* -> Binary, should do this in one step
  ReturnObj(Binary, b);
}

namespace prototype {

static void dupl(js_State *J) {
  AssertNargs(0)
  ReturnObj(Binary, new Binary(*GetArg(Binary, 0)));
}

static void size(js_State *J) {
  AssertNargs(0)
  Return(J, (unsigned)GetArg(Binary, 0)->size());
}

static void resize(js_State *J) {
  AssertNargs(1)
  GetArg(Binary, 0)->resize(GetArgUInt32(1));
  ReturnVoid(J);
}

static void append(js_State *J) {
  AssertNargs(1)
  auto b = GetArg(Binary, 0);
  auto b1 = GetArg(Binary, 1);
  b->insert(b->end(), b1->begin(), b1->end());
  ReturnVoid(J);
}

static void concatenate(js_State *J) {
  AssertNargs(1)
  auto b = new Binary(*GetArg(Binary, 0));
  auto b1 = GetArg(Binary, 1);
  b->insert(b->end(), b1->begin(), b1->end());
  ReturnObj(Binary, b);
}

static void toString(js_State *J) {
  AssertNargs(0)
  auto b = GetArg(Binary, 0);
  Return(J, std::string(b->begin(), b->end()));
}

static void toFile(js_State *J) {
  AssertNargs(1)
  std::ofstream file(GetArgString(1), std::ios::out | std::ios::binary);
  auto b = GetArg(Binary, 0);
  file.write((char*)&(*b)[0], b->size());
  ReturnVoid(J);
}

}

static void init(js_State *J) {
  InitObjectRegistry(J, TAG_Binary);
  js_pushglobal(J);
  ADD_JS_CONSTRUCTOR(Binary)
  js_getglobal(J, TAG_Binary);
  js_getproperty(J, -1, "prototype");
  StackPopPrevious()
  { // methods
    ADD_METHOD_CPP(Binary, dupl, 0)
    ADD_METHOD_CPP(Binary, size, 0)
    ADD_METHOD_CPP(Binary, resize, 1)
    ADD_METHOD_CPP(Binary, append, 1)
    ADD_METHOD_CPP(Binary, concatenate, 1)
    ADD_METHOD_CPP(Binary, toString, 0)
    ADD_METHOD_CPP(Binary, toFile, 1)
  }
  js_pop(J, 2);
  AssertStack(0);
}

} // JsBinary

namespace JsAtom {

static void xnewo(js_State *J, Atom *a) {
  js_getglobal(J, TAG_Atom);
  js_getproperty(J, -1, "prototype");
  js_rot2(J);
  js_pop(J,1);
  js_newuserdata(J, TAG_Atom, a, atomFinalize);
}

static void xnewoZ(js_State *J, Atom *a) { // object or undefined when a==nulllptr
  if (a)
    xnewo(J, a);
  else
    js_pushundefined(J);
}

static void xnew(js_State *J) {
  AssertNargs(2)
  ReturnObj(Atom, new Atom(Element(PeriodicTableData::get().elementFromSymbol(GetArgString(1))), GetArgVec3(2)));
}

namespace prototype {

static void id(js_State *J) {
  AssertNargs(0)
  Return(J, GetArg(Atom, 0)->id());
}

static void dupl(js_State *J) {
  AssertNargs(0)
  ReturnObj(Atom, new Atom(*GetArg(Atom, 0)));
}

static void str(js_State *J) {
  AssertNargs(0)
  auto a = GetArg(Atom, 0);
  Return(J, str(boost::format("atom{%1% elt=%2% pos=%3%}") % a % a->elt % a->pos));
}

static void isEqual(js_State *J) {
  AssertNargs(1)
  Return(J, GetArg(Atom, 0)->isEqual(*GetArg(Atom, 1)));
}

static void getElement(js_State *J) {
  AssertNargs(0)
  Return(J, str(boost::format("%1%") % GetArg(Atom, 0)->elt));
}

static void getPos(js_State *J) {
  AssertNargs(0)
  Return(J, GetArg(Atom, 0)->pos);
}

static void setPos(js_State *J) {
  AssertNargs(1)
  GetArg(Atom, 0)->pos = GetArgVec3(1);
  ReturnVoid(J);
}

static void getName(js_State *J) {
  AssertNargs(0)
  Return(J, GetArg(Atom, 0)->name);
}

static void setName(js_State *J) {
  AssertNargs(1)
  GetArg(Atom, 0)->name = GetArgString(1);
  ReturnVoid(J);
}

static void getHetAtm(js_State *J) {
  AssertNargs(0)
  Return(J, GetArg(Atom, 0)->isHetAtm);
}

static void setHetAtm(js_State *J) {
  AssertNargs(1)
  GetArg(Atom, 0)->isHetAtm = GetArgBoolean(1);
  ReturnVoid(J);
}

static void getNumBonds(js_State *J) {
  AssertNargs(0)
  auto a = GetArg(Atom, 0);
  Return(J, a->nbonds());
}

static void getBonds(js_State *J) {
  AssertNargs(0)
  auto a = GetArg(Atom, 0);
  returnArrayOfUserData<std::vector<Atom*>, void(*)(js_State*,Atom*)>(J, a->bonds, TAG_Atom, atomFinalize, JsAtom::xnewo);
}

static void hasBond(js_State *J) {
  AssertNargs(1)
  Return(J, GetArg(Atom, 0)->hasBond(GetArg(Atom, 1)));
}

static void getOtherBondOf3(js_State *J) {
  AssertNargs(2)
  ReturnObj(Atom, GetArg(Atom, 0)->getOtherBondOf3(GetArg(Atom,1), GetArg(Atom,2)));
}

static void findSingleNeighbor(js_State *J) {
  AssertNargs(1)
  ReturnObjZ(Atom, GetArg(Atom, 0)->findSingleNeighbor(GetArgElement(1)));
}

static void findSingleNeighbor2(js_State *J) {
  AssertNargs(2)
  ReturnObjZ(Atom, GetArg(Atom, 0)->findSingleNeighbor2(GetArgElement(1), GetArgElement(2)));
}

static void snapToGrid(js_State *J) {
  AssertNargs(1)
  GetArg(Atom, 0)->snapToGrid(GetArgVec3(1));
  ReturnVoid(J);
}

static void getChain(js_State *J) {
  AssertNargs(0)
  Return(J, GetArg(Atom, 0)->chain);
}

static void getGroup(js_State *J) {
  AssertNargs(0)
  Return(J, GetArg(Atom, 0)->group);
}

static void getSecStruct(js_State *J) {
  AssertNargs(0)
  Return(J, GetArg(Atom, 0)->secStructKind);
}

static void getSecStructStr(js_State *J) {
  AssertNargs(0)
  std::ostringstream ss;
  ss << GetArg(Atom, 0)->secStructKind;
  Return(J, ss.str());
}

} // prototype

static void init(js_State *J) {
  InitObjectRegistry(J, TAG_Atom);
  js_pushglobal(J);
  ADD_JS_CONSTRUCTOR(Atom)
  js_getglobal(J, TAG_Atom);              // PUSH Object => {-1: Atom}
  js_getproperty(J, -1, "prototype");     // PUSH prototype => {-1: Atom, -2: Atom.prototype}
  StackPopPrevious()
  { // methods
    ADD_METHOD_CPP(Atom, id, 0)
    ADD_METHOD_CPP(Atom, dupl, 0)
    ADD_METHOD_CPP(Atom, str, 0)
    ADD_METHOD_CPP(Atom, isEqual, 1)
    ADD_METHOD_CPP(Atom, getElement, 0)
    ADD_METHOD_CPP(Atom, getPos, 0)
    ADD_METHOD_CPP(Atom, setPos, 1)
    ADD_METHOD_CPP(Atom, getName, 0)
    ADD_METHOD_CPP(Atom, setName, 1)
    ADD_METHOD_CPP(Atom, getHetAtm, 0)
    ADD_METHOD_CPP(Atom, setHetAtm, 1)
    ADD_METHOD_CPP(Atom, getNumBonds, 0)
    ADD_METHOD_CPP(Atom, getBonds, 0)
    ADD_METHOD_CPP(Atom, hasBond, 1)
    ADD_METHOD_CPP(Atom, getOtherBondOf3, 2)
    ADD_METHOD_CPP(Atom, findSingleNeighbor, 1)
    ADD_METHOD_CPP(Atom, findSingleNeighbor2, 2)
    ADD_METHOD_CPP(Atom, snapToGrid, 1)
    ADD_METHOD_CPP(Atom, getChain, 0)
    ADD_METHOD_CPP(Atom, getGroup, 0)
    ADD_METHOD_CPP(Atom, getSecStruct, 0)
    ADD_METHOD_CPP(Atom, getSecStructStr, 0)
    ADD_METHOD_JS (Atom, findBonds, function(filter) {return this.getBonds().filter(filter)})
    ADD_METHOD_JS (Atom, angleBetweenRad, function(a1, a2) {var p = this.getPos(); return Vec3.angleRad(Vec3.minus(a1.getPos(),p), Vec3.minus(a2.getPos(),p))})
    ADD_METHOD_JS (Atom, angleBetweenDeg, function(a1, a2) {var p = this.getPos(); return Vec3.angleDeg(Vec3.minus(a1.getPos(),p), Vec3.minus(a2.getPos(),p))})
    ADD_METHOD_JS (Atom, distance, function(othr) {return Vec3.length(Vec3.minus(this.getPos(), othr.getPos()))})
  }
  js_pop(J, 2);
  AssertStack(0);
}

} // JsAtom

namespace JsMolecule {

namespace helpers {

static void pushAaBackboneAsJsObject(js_State *J, const Molecule::AaBackbone &aaBackbone) {
  auto addFld = [J](const char *fld, Atom *atom) {
    JsAtom::xnewo(J, atom);
    js_setproperty(J, -2, fld);
  };
  auto addFldZ = [J, addFld](const char *fld, Atom *atom) { // can be null (optional fields)
    if (atom)
      addFld(fld, atom);
    else {
      js_pushnull(J);
      js_setproperty(J, -2, fld);
    }
  };
  js_newobject(J);
  addFld ("N",       aaBackbone.N);
  addFld ("HCn1",    aaBackbone.HCn1);
  addFldZ("Hn2",     aaBackbone.Hn2);
  addFld ("Cmain",   aaBackbone.Cmain);
  addFld ("Hc",      aaBackbone.Hc);
  addFld ("Coo",     aaBackbone.Coo);
  addFld ("O2",      aaBackbone.O2);
  addFldZ("O1",      aaBackbone.O1);
  addFldZ("Ho",      aaBackbone.Ho);
  addFld ("payload", aaBackbone.payload);
}

static Molecule::AaBackbone popAaBackboneAsJsObject(js_State *J) {
  Molecule::AaBackbone aaBackbone;
  auto addFld = [J](const char *fld, Atom *&atom) {
    js_getproperty(J, -1, fld);
    atom = (Atom*)js_touserdata(J, -1, TAG_Atom);
    js_pop(J,1);
  };
  auto addFldZ = [J](const char *fld, Atom *&atom) {
    js_getproperty(J, -1, fld);
    if (!js_isnull(J, -1))
      atom = (Atom*)js_touserdata(J, -1, TAG_Atom);
    else
      atom = nullptr;
    js_pop(J,1);
  };
  addFld ("N",       aaBackbone.N);
  addFld ("HCn1",    aaBackbone.HCn1);
  addFldZ("Hn2",     aaBackbone.Hn2);
  addFld ("Cmain",   aaBackbone.Cmain);
  addFld ("Hc",      aaBackbone.Hc);
  addFld ("Coo",     aaBackbone.Coo);
  addFld ("O2",      aaBackbone.O2);
  addFldZ("O1",      aaBackbone.O1);
  addFldZ("Ho",      aaBackbone.Ho);
  addFld ("payload", aaBackbone.payload);
  js_pop(J,1);
  return aaBackbone;
}

static void pushAaBackboneAsJsObjectZ(js_State *J, const Molecule::AaBackbone &aaBackbone) {
  if (aaBackbone.Cmain)
    pushAaBackboneAsJsObject(J, aaBackbone);
  else
    js_pushnull(J); // null is returned when it is not found
}

static std::vector<Molecule::AaBackbone>* readAaBackboneArray(js_State *J, int argno) {
  std::unique_ptr<std::vector<Molecule::AaBackbone>> aaBackboneArray(new std::vector<Molecule::AaBackbone>);
  {
    auto len = js_getlength(J, argno);
    aaBackboneArray->resize(len);
    for (unsigned i = 0; i < len; i++) {
      js_getindex(J, 1/*argno*/, i);
      (*aaBackboneArray)[i] = popAaBackboneAsJsObject(J);
    }
  }
  return aaBackboneArray.release();
}

} // helpers

static void xnewo(js_State *J, Molecule *m) {
  js_getglobal(J, TAG_Molecule);
  js_getproperty(J, -1, "prototype");
  js_rot2(J);
  js_pop(J,1);
  js_newuserdata(J, TAG_Molecule, m, moleculeFinalize);
}

static void xnew(js_State *J) {
  ReturnObj(Molecule, new Molecule("created-in-script"));
}

namespace prototype {

static void id(js_State *J) {
  AssertNargs(0)
  Return(J, GetArg(Molecule, 0)->id());
}

static void dupl(js_State *J) {
  AssertNargs(0)
  ReturnObj(Molecule, new Molecule(*GetArg(Molecule, 0)));
}

static void str(js_State *J) {
  AssertNargs(0)
  auto m = GetArg(Molecule, 0);
  Return(J, str(boost::format("molecule{%1%, id=%2%}") % m % m->idx.c_str()));
}

static void numAtoms(js_State *J) {
  AssertNargs(0)
  Return(J, GetArg(Molecule, 0)->getNumAtoms());
}

static void getAtom(js_State *J) {
  AssertNargs(1)
  ReturnObj(Atom, GetArg(Molecule, 0)->getAtom(GetArgUInt32(1)));
}

static void getAtoms(js_State *J) {
  AssertNargs(0)
  auto m = GetArg(Molecule, 0);
  returnArrayOfUserData<std::vector<Atom*>, void(*)(js_State*,Atom*)>(J, m->atoms, TAG_Atom, atomFinalize, JsAtom::xnewo);
}

static void addAtom(js_State *J) {
  AssertNargs(1)
  GetArg(Molecule, 0)->add(GetArg(Atom, 1));
  ReturnVoid(J);
}

static void numChains(js_State *J) {
  AssertNargs(0)
  Return(J, GetArg(Molecule, 0)->numChains());
}

static void numGroups(js_State *J) {
  AssertNargs(0)
  Return(J, GetArg(Molecule, 0)->numGroups());
}

static void addMolecule(js_State *J) {
  AssertNargs(1)
  GetArg(Molecule, 0)->add(*GetArg(Molecule, 1));
}

static void appendAminoAcid(js_State *J) {
  AssertNargs(2)
  Molecule aa(*GetArg(Molecule, 1)); // copy because it will be altered
  GetArg(Molecule, 0)->appendAsAminoAcidChain(aa, GetArgFloatArrayZ(2));
}

static void readAminoAcidAnglesFromAaChain(js_State *J) {
  AssertNargs(1)
  // GetArg(Molecule, 0); // semi-static: "this" argument isn't used

  // binary is disabled:
  // std::unique_ptr<std::vector<Molecule::AaBackbone>> aaBackboneArray(Util::createContainerFromBufferOfDifferentType<Binary, std::vector<Molecule::AaBackbone>>(*GetArg(Binary, 1)));

  // read the AaBackbone array argument
  std::unique_ptr<std::vector<Molecule::AaBackbone>> aaBackboneArray(helpers::readAaBackboneArray(J, 1/*argno*/));

  // return the angles array
  Return(J, Molecule::readAminoAcidAnglesFromAaChain(*aaBackboneArray));
}

static void setAminoAcidAnglesInAaChain(js_State *J) {
  AssertNargs(3)
  // GetArg(Molecule, 0); // semi-static: "this" argument isn't used

  // read the AaBackbone array argument
  std::unique_ptr<std::vector<Molecule::AaBackbone>> aaBackboneArray(helpers::readAaBackboneArray(J, 1/*argno*/));
  // setAminoAcidAnglesInAaChain
  Molecule::setAminoAcidAnglesInAaChain(*aaBackboneArray, GetArgUnsignedArray(2), GetArgFloatArrayArray(3));

  ReturnVoid(J);
}

static void detectBonds(js_State *J) {
  AssertNargs(0)
  GetArg(Molecule, 0)->detectBonds();
  ReturnVoid(J);
}

static void isEqual(js_State *J) {
  AssertNargs(1)
  Return(J, GetArg(Molecule, 0)->isEqual(*GetArg(Molecule, 1)));
}

static void toXyz(js_State *J) {
  AssertNargs(0)
  std::ostringstream ss;
  ss << *GetArg(Molecule, 0);
  Return(J, ss.str());
}

static void toXyzCoords(js_State *J) {
  AssertNargs(0)
  std::ostringstream ss;
  GetArg(Molecule, 0)->prnCoords(ss);
  Return(J, ss.str());
}

static void findComponents(js_State *J) {
  AssertNargs(0)
  returnArrayOfArrayOfUserData<std::vector<std::vector<Atom*>>, void(*)(js_State*,Atom*)>(J, GetArg(Molecule, 0)->findComponents(), TAG_Atom, atomFinalize, JsAtom::xnewo);
}

static void computeConvexHullFacets(js_State *J) { // arguments: (withFurthestdist)
  AssertNargs(1)
  // withFurthestdist argument
  std::unique_ptr<std::vector<double>> withFurthestdist;
  if (js_isarray(J, 1))
    withFurthestdist.reset(new std::vector<double>);
  else if (!js_isundefined(J, 1))
    js_typeerror(J, "withFurthestdist should be either an array or undefined");
  // call the C++ function and fill the array
  js_newarray(J);
  {
    unsigned idx = 0;
    for (auto &facetNormal : GetArg(Molecule, 0)->computeConvexHullFacets(withFurthestdist.get())) {
      Return(J, facetNormal);
      js_setindex(J, -2, idx++);
    }
  }
  // return withFurthestdist when requested
  if (withFurthestdist.get()) {
    unsigned idx = 0;
    for (auto d : *withFurthestdist.get()) {
      js_pushnumber(J, d);
      js_setindex(J, 1, idx++);
    }
  }
}

static void centerOfMass(js_State *J) {
  AssertNargs(0)
  Return(J, GetArg(Molecule, 0)->centerOfMass());
}

static void centerAt(js_State *J) {
  AssertNargs(1)
  GetArg(Molecule, 0)->centerAt(GetArgVec3(1));
  ReturnVoid(J);
}

static void snapToGrid(js_State *J) {
  AssertNargs(1)
  GetArg(Molecule, 0)->snapToGrid(GetArgVec3(1));
  ReturnVoid(J);
}

static void findAaBackbone1(js_State *J) {
  AssertNargs(0)
  helpers::pushAaBackboneAsJsObjectZ(J, GetArg(Molecule, 0)->findAaBackbone1());
}

static void findAaBackboneFirst(js_State *J) {
  AssertNargs(0)
  helpers::pushAaBackboneAsJsObjectZ(J, GetArg(Molecule, 0)->findAaBackboneFirst());
}

static void findAaBackboneLast(js_State *J) {
  AssertNargs(0)
  helpers::pushAaBackboneAsJsObjectZ(J, GetArg(Molecule, 0)->findAaBackboneLast());
}

static void findAaBackbones(js_State *J) {
  AssertNargs(0)
  // return array
  js_newarray(J);
  unsigned idx = 0;
  for (auto &aaBackbone : GetArg(Molecule, 0)->findAaBackbones()) {
    helpers::pushAaBackboneAsJsObject(J, aaBackbone);
    js_setindex(J, -2, idx++);
  }
}

static void getAminoAcidSingleAngle(js_State *J) { // (Molecule*, AaBackbones *, unsigned idx, unsigned angleId)
  AssertNargs(3)
  Return(J, GetArg(Molecule, 0)->getAminoAcidSingleAngle(
    *std::unique_ptr<std::vector<Molecule::AaBackbone>>(helpers::readAaBackboneArray(J, 1/*argno*/)), // aaBackbones
    GetArgUInt32(2), // idx
    (Molecule::AaAngles::Type)GetArgUInt32(3))); // angleId
}

static void getAminoAcidSingleJunctionAngles(js_State *J) { // (Molecule*, AaBackbones *, unsigned idx)
  AssertNargs(2)
  Return(J, GetArg(Molecule, 0)->getAminoAcidSingleJunctionAngles(
    *std::unique_ptr<std::vector<Molecule::AaBackbone>>(helpers::readAaBackboneArray(J, 1/*argno*/)), // aaBackbones
    GetArgUInt32(2))); // idx
}

static void getAminoAcidSequenceAngles(js_State *J) { // (Molecule*, AaBackbones *, unsigned[] idxs)
  AssertNargs(2)
  Return(J, GetArg(Molecule, 0)->getAminoAcidSequenceAngles(
    *std::unique_ptr<std::vector<Molecule::AaBackbone>>(helpers::readAaBackboneArray(J, 1/*argno*/)), // aaBackbones
    GetArgUnsignedArray(2))); // idxs
}

static void setAminoAcidSingleAngle(js_State *J) { // (Molecule*, AaBackbones *, unsigned idx, unsigned angleId, double newAngle)
  AssertNargs(4)
  Return(J, GetArg(Molecule, 0)->setAminoAcidSingleAngle(
    *std::unique_ptr<std::vector<Molecule::AaBackbone>>(helpers::readAaBackboneArray(J, 1/*argno*/)), // aaBackbones
    GetArgUInt32(2), // idx
    (Molecule::AaAngles::Type)GetArgUInt32(3), // angleId
    GetArgFloat(4))); // newAngle
}

static void setAminoAcidSingleJunctionAngles(js_State *J) { // (Molecule*, AaBackbones *, unsigned idx, double[] newAngles)
  AssertNargs(3)
  GetArg(Molecule, 0)->setAminoAcidSingleJunctionAngles(
    *std::unique_ptr<std::vector<Molecule::AaBackbone>>(helpers::readAaBackboneArray(J, 1/*argno*/)), // aaBackbones
    GetArgUInt32(2), // idx
    GetArgFloatArray(3)); // newAngles
}

static void setAminoAcidSequenceAngles(js_State *J) { // (Molecule*, AaBackbones *, unsigned[] idxs, double[][] newAngles)
  AssertNargs(3)
  GetArg(Molecule, 0)->setAminoAcidSequenceAngles(
    *std::unique_ptr<std::vector<Molecule::AaBackbone>>(helpers::readAaBackboneArray(J, 1/*argno*/)), // aaBackbones
    GetArgUnsignedArray(2), // idxs
    GetArgFloatArrayArray(3)); // newAngles
}

/* binary methods are disabled for now
static void findAaBackbonesBin(js_State *J) { // returns the same as findAaBackbones but as a binary array in order to pass it to other functions easily
  AssertNargs(0)
  auto aaBackbonesBinary = Util::createContainerFromBufferOfDifferentType<std::vector<Molecule::AaBackbone>, Binary>(GetArg(Molecule, 0)->findAaBackbones());
  Return(Binary, aaBackbonesBinary);
}
*/
} // prototype

static void init(js_State *J) {
  InitObjectRegistry(J, TAG_Molecule);
  js_pushglobal(J);
  ADD_JS_CONSTRUCTOR(Molecule)
  js_getglobal(J, TAG_Molecule);          // PUSH Object => {-1: Molecule}
  js_getproperty(J, -1, "prototype");     // PUSH prototype => {-1: Molecule, -2: Molecule.prototype}
  StackPopPrevious()
  { // methods
    ADD_METHOD_CPP(Molecule, id, 0)
    ADD_METHOD_CPP(Molecule, dupl, 0)
    ADD_METHOD_CPP(Molecule, str, 0)
    ADD_METHOD_CPP(Molecule, numAtoms, 0)
    ADD_METHOD_CPP(Molecule, getAtom, 1)
    ADD_METHOD_CPP(Molecule, getAtoms, 0)
    ADD_METHOD_CPP(Molecule, addAtom, 1)
    ADD_METHOD_CPP(Molecule, numChains, 0)
    ADD_METHOD_CPP(Molecule, numGroups, 0)
    ADD_METHOD_JS (Molecule, findAtoms, function(filter) {return this.getAtoms().filter(filter)})
    ADD_METHOD_JS (Molecule, transform, function(rot,shift) {
      for (var i = 0; i < this.numAtoms(); i++) {
        var a = this.getAtom(i);
        a.setPos(Vec3.plus(Mat3.mulv(rot, a.getPos()), shift));
      }
    })
    ADD_METHOD_CPP(Molecule, addMolecule, 1)
    ADD_METHOD_CPP(Molecule, appendAminoAcid, 2)
    ADD_METHOD_CPP(Molecule, readAminoAcidAnglesFromAaChain, 1)
    ADD_METHOD_CPP(Molecule, setAminoAcidAnglesInAaChain, 3)
    ADD_METHOD_CPP(Molecule, detectBonds, 0)
    ADD_METHOD_CPP(Molecule, isEqual, 1)
    ADD_METHOD_CPP(Molecule, toXyz, 0)
    ADD_METHOD_CPP(Molecule, toXyzCoords, 0)
    ADD_METHOD_CPP(Molecule, findComponents, 0)
    ADD_METHOD_CPP(Molecule, computeConvexHullFacets, 1)
    ADD_METHOD_CPP(Molecule, centerOfMass, 0)
    ADD_METHOD_CPP(Molecule, centerAt, 1)
    ADD_METHOD_CPP(Molecule, snapToGrid, 1)
    ADD_METHOD_CPP(Molecule, findAaBackbone1, 0)
    ADD_METHOD_CPP(Molecule, findAaBackboneFirst, 0)
    ADD_METHOD_CPP(Molecule, findAaBackboneLast, 0)
    ADD_METHOD_CPP(Molecule, findAaBackbones, 0)
    ADD_METHOD_CPP(Molecule, getAminoAcidSingleAngle, 3)
    ADD_METHOD_CPP(Molecule, getAminoAcidSingleJunctionAngles, 2)
    ADD_METHOD_CPP(Molecule, getAminoAcidSequenceAngles, 2)
    ADD_METHOD_CPP(Molecule, setAminoAcidSingleAngle, 4)
    ADD_METHOD_CPP(Molecule, setAminoAcidSingleJunctionAngles, 3)
    ADD_METHOD_CPP(Molecule, setAminoAcidSequenceAngles, 3)
    //ADD_METHOD_CPP(Molecule, findAaBackbonesBin, 0)
    ADD_METHOD_JS (Molecule, extractCoords, function(m) {
      var res = [];
      var atoms = this.getAtoms();
      for (var i = 0; i < atoms.length; i++)
        res.push(atoms[i].getPos());
      return res;
    });
    ADD_METHOD_JS (Molecule, rmsd, function(othr) {return Vec3.rmsd(this.extractCoords(), othr.extractCoords())})
    ADD_METHOD_JS (Molecule, allElements, function() {
      var mm = {};
      var atoms = this.getAtoms();
      for (var i = 0; i < atoms.length; i++)
        mm[atoms[i].getElement()] = 1;
      return Object.keys(mm);
    })
  }
  js_pop(J, 2);
  AssertStack(0);
}

// static functions in the Molecule namespace

static void fromXyzOne(js_State *J) {
  AssertNargs(1)
  ReturnObj(Molecule, Molecule::readXyzFileOne(GetArgString(1)));
}

static void fromXyzMany(js_State *J) {
  AssertNargs(1)
  returnArrayOfUserData<std::vector<Molecule*>, void(*)(js_State*,Molecule*)>(J, Molecule::readXyzFileMany(GetArgString(1)), TAG_Molecule, moleculeFinalize, JsMolecule::xnewo);
}

static void listNeighborsHierarchically(js_State *J) {
  AssertNargs(4)
  auto atoms = Molecule::listNeighborsHierarchically(GetArg(Atom, 1), GetArgBoolean(2), GetArgZ(Atom, 3), GetArgZ(Atom, 4));
  returnArrayOfUserData<std::vector<Atom*>, void(*)(js_State*,Atom*)>(J, Util::containerToVec(atoms), TAG_Atom, atomFinalize, JsAtom::xnewo);
}

#if defined(USE_OPENBABEL)
static void fromSMILES(js_State *J) {
  AssertNargs(2)
  ReturnObj(Molecule, Molecule::createFromSMILES(GetArgString(1), GetArgString(2)/*opt*/));
}
#endif

static void angleIntToStr(js_State *J) {
  AssertNargs(1)
  std::ostringstream ss;
  ss << (Molecule::AaAngles::Type)GetArgUInt32(1);
  Return(J, ss.str());
}

static void angleStrToInt(js_State *J) {
  AssertNargs(1)
  std::string str = GetArgString(1);
  std::istringstream is;
  is.str(str);
  Molecule::AaAngles::Type t;
  is >> t;
  Return(J, (unsigned)t);
}

} // JsMolecule

namespace JsTempFile {

static void xnewo(js_State *J, TempFile *f) {
  js_getglobal(J, TAG_TempFile);
  js_getproperty(J, -1, "prototype");
  js_newuserdata(J, TAG_TempFile, f, tempFileFinalize);
}

static void xnew(js_State *J) {
  AssertNargsRange(0,2)
  switch (GetNArgs()) {
  case 0: // ()
    ReturnObj(TempFile, new TempFile);
    break;
  case 1: // (fileName)
    ReturnObj(TempFile, new TempFile(GetArgString(1)));
    break;
  case 2: // (fileName, content)
    ReturnObj(TempFile, new TempFile(GetArgString(1), GetArgString(2)));
    break;
  }
}

namespace prototype {

static void str(js_State *J) {
  AssertNargs(0)
  auto f = GetArg(TempFile, 0);
  Return(J, str(boost::format("temp-file{%1%}") % f->getFname()));
}

static void fname(js_State *J) {
  AssertNargs(0)
  Return(J, GetArg(TempFile, 0)->getFname());
}

static void toBinary(js_State *J) {
  AssertNargs(0)
  ReturnObj(Binary, GetArg(TempFile, 0)->toBinary());
}

static void toPermanent(js_State *J) {
  AssertNargs(1)
  GetArg(TempFile, 0)->toPermanent(GetArgString(1));
  ReturnVoid(J);
}

}

static void init(js_State *J) {
  InitObjectRegistry(J, TAG_TempFile);
  js_pushglobal(J);
  ADD_JS_CONSTRUCTOR(TempFile)
  js_getglobal(J, TAG_TempFile);
  js_getproperty(J, -1, "prototype");
  StackPopPrevious()
  { // methods
    ADD_METHOD_CPP(TempFile, str, 0)
    ADD_METHOD_CPP(TempFile, fname, 0)
    ADD_METHOD_CPP(TempFile, toBinary, 0)
    ADD_METHOD_CPP(TempFile, toPermanent, 1)
    //ADD_METHOD_CPP(TempFile, writeBinary, 1) // to write binary data into the temp file
  }
  js_pop(J, 2);
  AssertStack(0);
}

} // JsTempFile

namespace JsStructureDb {

static void xnewo(js_State *J, StructureDb *f) {
  js_getglobal(J, TAG_StructureDb);
  js_getproperty(J, -1, "prototype");
  js_newuserdata(J, TAG_StructureDb, f, tempFileFinalize);
}

static void xnew(js_State *J) {
  AssertNargs(0)
  ReturnObj(StructureDb, new StructureDb);
}

namespace prototype {

static void str(js_State *J) {
  AssertNargs(0)
  auto sdb = GetArg(StructureDb, 0);
  Return(J, str(boost::format("structure-db{size=%1%}") % sdb->size()));
}

static void toString(js_State *J) {
  AssertNargs(0)
  auto sdb = GetArg(StructureDb, 0);
  Return(J, str(boost::format("structure-db{size=%1%}") % sdb->size()));
}

static void add(js_State *J) {
  AssertNargs(2)
  GetArg(StructureDb, 0)->add(GetArg(Molecule, 1), GetArgString(2));
  ReturnVoid(J);
}

static void find(js_State *J) {
  AssertNargs(1)
  Return(J, GetArg(StructureDb, 0)->find(GetArg(Molecule, 1)));
}

static void moleculeSignature(js_State *J) {
  AssertNargs(1)
  /*XXX StructureDb::moleculeSignature is static, and "this" argument isn't used*/
  auto signature = StructureDb::computeMoleculeSignature(GetArg(Molecule, 1));
  std::ostringstream ss;
  ss << signature;
  Return(J, ss.str());
}

}

static void init(js_State *J) {
  InitObjectRegistry(J, TAG_StructureDb);
  js_pushglobal(J);
  ADD_JS_CONSTRUCTOR(StructureDb)
  js_getglobal(J, TAG_StructureDb);
  js_getproperty(J, -1, "prototype");
  StackPopPrevious()
  { // methods
    ADD_METHOD_CPP(StructureDb, str, 0)
    ADD_METHOD_CPP(StructureDb, toString, 0)
    ADD_METHOD_CPP(StructureDb, add, 2)
    ADD_METHOD_CPP(StructureDb, find, 1)
    ADD_METHOD_CPP(StructureDb, moleculeSignature, 1)
  }
  js_pop(J, 2);
  AssertStack(0);
}

} // JsStructureDb

//
// exported functions
//

namespace helpers {
  static void applyAttrs(std::ostream &os, const std::vector<std::string> &attrs) {
    for (auto &attr : attrs) {
      if (attr == "clr.fg.black")        os << rang::fg::black;
      else if (attr == "clr.fg.red")     os << rang::fg::red;
      else if (attr == "clr.fg.green")   os << rang::fg::green;
      else if (attr == "clr.fg.yellow")  os << rang::fg::yellow;
      else if (attr == "clr.fg.blue")    os << rang::fg::blue;
      else if (attr == "clr.fg.magenta") os << rang::fg::magenta;
      else if (attr == "clr.fg.cyan")    os << rang::fg::cyan;
      else if (attr == "clr.fg.gray")    os << rang::fg::gray;
      else if (attr == "clr.fg.reset")   os << rang::fg::reset;
      else if (attr == "clr.bg.black")   os << rang::bg::black;
      else if (attr == "clr.bg.red")     os << rang::bg::red;
      else if (attr == "clr.bg.green")   os << rang::bg::green;
      else if (attr == "clr.bg.yellow")  os << rang::bg::yellow;
      else if (attr == "clr.bg.blue")    os << rang::bg::blue;
      else if (attr == "clr.bg.magenta") os << rang::bg::magenta;
      else if (attr == "clr.bg.cyan")    os << rang::bg::cyan;
      else if (attr == "clr.bg.gray")    os << rang::bg::gray;
      else if (attr == "clr.bg.reset")   os << rang::bg::reset;
      else
        ERROR("unknown attr supplied: " << attr)
    }
  }
}

static void print(js_State *J) {
  AssertNargs(1)
  std::cout << GetArgString(1) << std::endl;
  ReturnVoid(J);
}

static void printn(js_State *J) {
  AssertNargs(1)
  std::cout << GetArgString(1);
  ReturnVoid(J);
}

static void printa(js_State *J) {
  AssertNargs(2)
  helpers::applyAttrs(std::cout, GetArgStringArray(1));
  std::cout << GetArgString(2) << rang::fg::reset << rang::bg::reset << std::endl;
  ReturnVoid(J);
}

static void printna(js_State *J) {
  AssertNargs(2)
  helpers::applyAttrs(std::cout, GetArgStringArray(1));
  std::cout << GetArgString(2);
  ReturnVoid(J);
}

static void flush(js_State *J) {
  AssertNargs(0)
  std::cout.flush();
}

namespace JsSystem {

static void numCPUs(js_State *J) {
  AssertNargs(0)
#if defined(__FreeBSD__)
  int buf;
  size_t size = sizeof(buf);
  auto res = sysctlbyname("hw.ncpu", &buf, &size, NULL, 0);
  if (res)
    ERROR_SYSCALL(sysctlbyname)
  Return(J, (unsigned) buf);
#else
#  error "numCPUs not defined for your system"
#endif
}

static void setCtlParam(js_State *J) {
  AssertNargs(2)
  auto nameParts = Util::split<'.'>(GetArgString(1));
  auto val = GetArgString(2);
  if (nameParts.size() < 2)
    ERROR("setCtlParam: name is expected to have at least 2 parts")
  if (nameParts[0] == "temp-file") {
    nameParts.erase(nameParts.begin());
    TempFile::setCtlParam(nameParts, val);
  } else if (nameParts[0] == "process") {
    nameParts.erase(nameParts.begin());
    Process::setCtlParam(nameParts, val);
  } else {
    ERROR("setCtlParam: unknown name domain '" << nameParts[0] << "'")
  }
  ReturnVoid(J);
}

} // JsSystem

namespace JsFile {

static void exists(js_State *J) {
  AssertNargs(1)
  Return(J, std::ifstream(GetArgString(1)).good());
}

static void read(js_State *J) { // reads file into a string
  AssertNargs(1)
  auto fname = GetArgString(1);

  using it = std::istreambuf_iterator<char>;

  std::ifstream file;
  file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  try {
    file.open(fname, std::ios::in);
    Return(J, std::string(it(file), it()));
    file.close();
  } catch (std::ifstream::failure e) {
    ERROR(str(boost::format("can't read the file '%1%': %2%") % fname % e.what()));
  }
}

static void write(js_State *J) { // writes string into a file
  AssertNargs(2)
  auto fname = GetArgString(2);

  std::ofstream file;
  file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  try {
    file.open(fname, std::ios::out);
    file << GetArgString(1);
    file.close();
  } catch (std::ifstream::failure e) {
    ERROR(str(boost::format("can't write the file '%1%': %2%") % fname % e.what()));
  }

  ReturnVoid(J);
}

static void mkdir(js_State *J) { // creates a directory
  AssertNargs(1)
  // N/A yet: std::filesystem::create_directory(GetArgString(1));
  ::mkdir(GetArgString(1).c_str(), 0777);
}

static void ckdir(js_State *J) { // checks if the directory exists
  AssertNargs(1)
  struct stat fileStat;
  Return(J, ::stat(GetArgString(1).c_str(), &fileStat)==0 && S_ISDIR(fileStat.st_mode));
}

static void unlink(js_State *J) { // removes a single file
  AssertNargs(1)
  if (::unlink(GetArgString(1).c_str()) != 0)
    ERROR_SYSCALL1(unlink, "file '" << GetArgString(1) << "'")
  ReturnVoid(J);
}

} // JsFile

static void sleep(js_State *J) {
  AssertNargs(1)
  auto secs = GetArgUInt32(1);
  ::sleep(secs);
  ReturnVoid(J);
}

static void waitForUserInput(js_State *J) {
  AssertNargs(0)
  std::cout << "{pid=" << getpid() << "} press Enter ..." << std::endl;
  (void)::getc(stdin);
  ReturnVoid(J);
}

namespace JsTime {

static void start(js_State *J) {
  AssertNargs(0)
  js_pushnumber(J, Tm::start());
}

static void now(js_State *J) {
  AssertNargs(0)
  js_pushnumber(J, Tm::now());
}

static void wallclock(js_State *J) {
  AssertNargs(0)
  js_pushnumber(J, Tm::wallclock());
}

static void currentDateTimeToSec(js_State *J) { // format is YYYY-MM-DD.HH:mm:ss
  AssertNargs(0)
  time_t     now = time(0);
  struct tm  tstruct;
  char       buf[80];
  tstruct = *localtime(&now);
  strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);
  Return(J, std::string(buf));
}

static void currentDateTimeToMs(js_State *J) { // format is YYYY-MM-DD.HH:mm:ss
  AssertNargs(0)

  timeval curTime;
  gettimeofday(&curTime, NULL);

  struct tm  tstruct;
  char       bufs[80], bufms[80];

  tstruct = *localtime(&curTime.tv_sec);
  strftime(bufs, sizeof(bufs), "%Y-%m-%d.%X", &tstruct);
  sprintf(bufms, "%s.%03ld", bufs, curTime.tv_usec/1000);

  Return(J, std::string(bufms));
}

} // JsTime

static void system(js_State *J) {
  AssertNargs(1)
  Return(J, Process::exec(GetArgString(1)));
}

static void formatFp(js_State *J) {
  AssertNargs(2)
  char buf[32];
  ::sprintf(buf, "%.*lf", GetArgUInt32(2), GetArgFloat(1));
  Return(J, buf);
}

static void download(js_State *J) {
  AssertNargs(3)
  ReturnObj(Binary, WebIo::download(GetArgString(1), GetArgString(2), GetArgString(3)));
}

static void downloadUrl(js_State *J) {
  AssertNargs(1)
  ReturnObj(Binary, WebIo::downloadUrl(GetArgString(1)));
}

template<typename C>
static void gxzip(js_State *J, C xcompressor) {
  AssertNargs(1)

  namespace io = boost::iostreams;

  auto copyContainer = [](auto const &src, auto &dst) {
    dst.resize(src.size());
    ::memcpy(&dst[0], &src[0], src.size());
  };

  auto binIn = GetArg(Binary, 1);
  std::string bufIn;

  copyContainer(*binIn, bufIn); // XXX TODO optimize: need to copy between containers

  std::istringstream is(bufIn);
  std::ostringstream os;

  io::filtering_streambuf<io::input> in;
  in.push(xcompressor);
  in.push(is);

  io::copy(in, os);

  Binary *b = new Binary;
  copyContainer(os.str(), *b); // XXX TODO optimize: need to copy between containers

  ReturnObj(Binary, b);
}

static void gzip(js_State *J) {
  namespace io = boost::iostreams;
  gxzip(J, io::gzip_compressor());
}

static void gunzip(js_State *J) {
  namespace io = boost::iostreams;
  gxzip(J, io::gzip_decompressor());
}

static void writeXyzFile(js_State *J) {
  AssertNargs(2)
  std::ofstream(GetArgString(2), std::ios::out) << *GetArg(Molecule, 1);
  ReturnVoid(J);
}

namespace Pdb {
static void readBuffer(js_State *J) {
  AssertNargs(1)
  auto mols = Molecule::readPdbBuffer(GetArgString(1));
  for (auto m : mols)
    JsMolecule::xnewo(J, m);
}
}

#if defined(USE_DSRPDB)
static void readPdbFile(js_State *J) {
  AssertNargs(1)
  auto mols = Molecule::readPdbFile(GetArgString(1));
  for (auto m : mols)
    JsMolecule::xnewo(J, m);
}
#endif

#if defined(USE_MMTF)
namespace Mmtf {
  static void readFile(js_State *J) {
    AssertNargs(1)
    auto mols = Molecule::readMmtfFile(GetArgString(1));
    for (auto m : mols)
      JsMolecule::xnewo(J, m);
  }

  static void readBuffer(js_State *J) {
    AssertNargs(1)
    auto mols = Molecule::readMmtfBuffer(GetArg(Binary, 1));
    for (auto m : mols)
      JsMolecule::xnewo(J, m);
  }
}
#endif

//
// vector and matrix operations
//

namespace JsVec3 {

static void almostEquals(js_State *J) {
  AssertNargs(3)
  Return(J, Vec3::almostEquals(GetArgVec3(1), GetArgVec3(2), GetArgFloat(3)));
}

static void zero(js_State *J) {
  AssertNargs(0)
  Return(J, Vec3::zero());
}

static void length(js_State *J) {
  AssertNargs(1)
  Return(J, GetArgVec3(1).len());
}

static void normalize(js_State *J) {
  AssertNargs(1)
  Return(J, GetArgVec3(1).normalize());
}

static void normalizeZ(js_State *J) {
  AssertNargs(1)
  Return(J, GetArgVec3(1).normalizeZ());
}

static void normalizeZl(js_State *J) {
  AssertNargs(2)
  Return(J, GetArgVec3(1).normalizeZ(GetArgFloat(2)));
}

static void plus(js_State *J) {
  AssertNargs(2)
  Return(J, GetArgVec3(1)+GetArgVec3(2));
}

static void minus(js_State *J) {
  AssertNargs(2)
  Return(J, GetArgVec3(1)-GetArgVec3(2));
}

static void muln(js_State *J) {
  AssertNargs(2)
  Return(J, GetArgVec3(1)*GetArgFloat(2));
}

static void dot(js_State *J) {
  AssertNargs(2)
  Return(J, GetArgVec3(1)*GetArgVec3(2));
}

static void cross(js_State *J) {
  AssertNargs(2)
  Return(J, GetArgVec3(1).cross(GetArgVec3(2)));
}

static void angleRad(js_State *J) { // angle in radians between v1-v and v2-v, ASSUMES that v1!=v and v2!=v
  AssertNargs(2)
  Return(J, GetArgVec3(1).angle(GetArgVec3(2)));
}

static void angleDeg(js_State *J) { // angle in degrees between v1-v and v2-v, ASSUMES that v1!=v and v2!=v
  AssertNargs(2)
  Return(J, GetArgVec3(1).angle(GetArgVec3(2))/M_PI*180);
}

static void rmsd(js_State *J) {
  AssertNargs(2)
  std::unique_ptr<std::valarray<double>> v1(GetArgMatNxX(1, 3/*N=3*/));
  std::unique_ptr<std::valarray<double>> v2(GetArgMatNxX(2, 3/*N=3*/));
  Return(J, Op::rmsd(*v1, *v2));
}

static void snapToGrid(js_State *J) {
  AssertNargs(2)
  auto v = GetArgVec3(1);
  v.snapToGrid(GetArgVec3(2));
  Return(J, v);
}

static void project(js_State *J) {
  AssertNargs(2)
  Return(J, GetArgVec3(1).project(GetArgVec3(2)));
}

static void orthogonal(js_State *J) {
  AssertNargs(2)
  Return(J, GetArgVec3(1).orthogonal(GetArgVec3(2)));
}

static void rotateCornerToCorner(js_State *J) {
  AssertNargs(4)
  Return(J, Vec3Extra::rotateCornerToCorner(GetArgVec3(1), GetArgVec3(2), GetArgVec3(3), GetArgVec3(4)));
}

static void angleAxis1x1(js_State *J) {
  AssertNargs(3)
  Return(J, Vec3Extra::angleAxis1x1(GetArgVec3(1), GetArgVec3(2), GetArgVec3(3)));
}

static void angleAxis2x1(js_State *J) {
  AssertNargs(4)
  Return(J, Vec3Extra::angleAxis2x1(GetArgVec3(1), GetArgVec3(2), GetArgVec3(3), GetArgVec3(4)));
}

} // JsVec3

namespace JsMat3 {

static void almostEquals(js_State *J) {
  AssertNargs(3)
  Return(J, Mat3::almostEquals(GetArgMat3x3(1), GetArgMat3x3(2), GetArgFloat(3)));
}

static void zero(js_State *J) {
  AssertNargs(0)
  Return(J, Mat3::zero());
}

static void identity(js_State *J) {
  AssertNargs(0)
  Return(J, Mat3::identity());
}

static void transpose(js_State *J) {
  AssertNargs(1)
  Return(J, GetArgMat3x3(1).transpose());
}

static void plus(js_State *J) {
  AssertNargs(2)
  Return(J, GetArgMat3x3(1)+GetArgMat3x3(2));
}

static void minus(js_State *J) {
  AssertNargs(2)
  Return(J, GetArgMat3x3(1)-GetArgMat3x3(2));
}

static void muln(js_State *J) {
  AssertNargs(2)
  Return(J, GetArgMat3x3(1)*GetArgFloat(2));
}

static void mulv(js_State *J) {
  AssertNargs(2)
  Return(J, GetArgMat3x3(1)*GetArgVec3(2));
}

static void mul(js_State *J) {
  AssertNargs(2)
  Return(J, GetArgMat3x3(1)*GetArgMat3x3(2));
}

static void rotate(js_State *J) {
  AssertNargs(1)
  Return(J, Mat3::rotate(GetArgVec3(1)));
}

} // JsMat3

namespace JsDl {

static void open(js_State *J) {
  AssertNargs(2)
  auto handle = dlopen(GetArgString(1).c_str(), GetArgUInt32(2));
  if (handle == 0)
    ERROR("dlopen for '" << GetArgString(1) << "' failed: " << dlerror())
  Return(J, handle);
}

static void sym(js_State *J) {
  AssertNargs(2)
  Return(J, dlsym(GetArgPtr(1), GetArgString(2).c_str()));
}

static void close(js_State *J) {
  AssertNargs(1)
  if (dlclose(GetArgPtr(1)) != 0)
    ERROR("dlclose failed: " << dlerror())
  ReturnVoid(J);
}

} // JsDl

namespace JsInvoke {

static void intStrOPtr(js_State *J) {
  typedef int(*Fn)(char*, void**);
  AssertNargs(2)
  void *ptr = 0;
  int fnRes = Fn(GetArgPtr(1))((char*)GetArgString(2).c_str(), &ptr);
  // return array
  js_newarray(J);
    js_pushnumber(J, fnRes);
    js_setindex(J, -2, 0/*index*/);
    js_pushstring(J, StrPtr::p2s(ptr).c_str());
    js_setindex(J, -2, 1/*index*/);
}

// helper for intPtrStrCb4intOpaqueIntSArrSArrPtrOStr
static int cb4intOpaqueIntSArrSArr(void* optr, int argInt, char **argSArr1, char **argSArr2) {
  auto J = (js_State*)optr;
  // test if the callback is passed to us, XXX testing every time is inefficient, TODO pass this info here
  if (!js_iscallable(J, -2))
    return 0; // no function is passed
  // pass the arguments for the callback
  js_copy(J, -2/*argument#5 (cb) in the above call*/);           // cbFunc
  js_pushundefined(J);                                           // 'this' argument
  js_copy(J, -3/*argument#5 (cbUserObject) in the above call*/); // arg 1: opaqueObject
  js_pushnumber(J, argInt);                                      // arg 2: int
  js_newarray(J);                                                // arg 3: argSArr1
    for (unsigned idx = 0; argSArr1[idx]; idx++) {
      js_pushstring(J, argSArr1[idx]);
      js_setindex(J, -2, idx);
    }
  js_newarray(J);                                                // arg 4: argSArr2
    for (unsigned idx = 0; argSArr2[idx]; idx++) {
      js_pushstring(J, argSArr2[idx]);
      js_setindex(J, -2, idx);
    }
  // call the callback
  js_call(J, 4/*number of args above*/);
  // return the value
  auto cbRetValue = (int)js_tonumber(J, -1);
  js_pop(J, 1);
  return cbRetValue;
}

static void intPtrStrCb4intOpaqueIntSArrSArrPtrOStr(js_State *J) {
  typedef int(*Fn)(void*, char*, int (*callback)(void*,int,char**,char**), void*, char**);
  AssertNargs(5)
  auto fnReturnArr2 = [J](int i, char *strz) {
    js_newarray(J);
      js_pushnumber(J, i);
      js_setindex(J, -2, 0/*index*/);
      // str
      if (strz)
        js_pushstring(J, strz);
      else
        js_pushundefined(J); // no string has been returned
      js_setindex(J, -2, 1/*index*/);
  };
  // check if the callback is callable
  if (!js_iscallable(J, 4) && !js_isundefined(J, 4))
    js_typeerror(J, "callback should be either callable or undefined");
  // call the callback
  char *ostr = nullptr;
  int fnRes = Fn(GetArgPtr(1))(GetArgPtr(2), (char*)GetArgString(3).c_str(), cb4intOpaqueIntSArrSArr, J/*as userData*/, &ostr);
  // return array
  fnReturnArr2(fnRes, ostr);
}

static void intPtr(js_State *J) {
  typedef int(*Fn)(void*);
  AssertNargs(2)
  Return(J, Fn(GetArgPtr(1))(GetArgPtr(2)));
}

static void strInt(js_State *J) {
  typedef char*(*Fn)(int);
  AssertNargs(2)
  Return(J, Fn(GetArgPtr(1))(GetArgInt32(2)));
}

} // JsInvoke

void registerFunctions(js_State *J) {

  //
  // init types
  //

  JsObj::init(J);
  JsBinary::init(J);
  JsAtom::init(J);
  JsMolecule::init(J);
  JsTempFile::init(J);
  JsStructureDb::init(J);

#define ADD_JS_FUNCTION(name, num) \
  js_newcfunction(J, JsBinding::name, #name, num); \
  js_setglobal(J, #name);

// macros to define static functions in global namespaces
#define BEGIN_NAMESPACE(ns)                       js_newobject(J);
#define ADD_NS_FUNCTION_CPP(ns, jsfn, cfn, nargs) MuJS::jsB_propf(J, #ns "." #jsfn, cfn, nargs);
#define ADD_NS_FUNCTION_JS(ns, func, code...)     ckErr(J, js_ploadstring(J, "[static function " #ns "." #func "]", "(" #code ")")); \
                                                  js_call(J, -1); \
                                                  js_defproperty(J, -2, #func, JS_DONTENUM);
#define END_NAMESPACE(name)                       js_defglobal(J, #name, JS_DONTENUM);

  //
  // Misc
  //

  ADD_JS_FUNCTION(print, 1)
  ADD_JS_FUNCTION(printn, 1)
  ADD_JS_FUNCTION(printa, 2)
  ADD_JS_FUNCTION(printna, 2)
  ADD_JS_FUNCTION(flush, 0)
  BEGIN_NAMESPACE(System)
    ADD_NS_FUNCTION_CPP(System, numCPUs,            JsSystem::numCPUs, 0)
    ADD_NS_FUNCTION_CPP(System, setCtlParam,        JsSystem::setCtlParam, 2)
  END_NAMESPACE(System)
  BEGIN_NAMESPACE(File)
    ADD_NS_FUNCTION_CPP(File, exists, JsFile::exists, 1)
    ADD_NS_FUNCTION_CPP(File, read,   JsFile::read, 1)
    ADD_NS_FUNCTION_CPP(File, write,  JsFile::write, 2)
    ADD_NS_FUNCTION_CPP(File, mkdir,  JsFile::mkdir, 1)
    ADD_NS_FUNCTION_CPP(File, ckdir,  JsFile::ckdir, 1)
    ADD_NS_FUNCTION_CPP(File, unlink, JsFile::unlink, 1)
  END_NAMESPACE(File)
  ADD_JS_FUNCTION(sleep, 1)
  ADD_JS_FUNCTION(waitForUserInput, 0)
  BEGIN_NAMESPACE(Time)
    ADD_NS_FUNCTION_CPP(Time, start,                JsTime::start, 0)
    ADD_NS_FUNCTION_CPP(Time, now,                  JsTime::now, 0)
    ADD_NS_FUNCTION_CPP(Time, wallclock,            JsTime::wallclock, 0)
    ADD_NS_FUNCTION_CPP(Time, currentDateTimeToSec, JsTime::currentDateTimeToSec, 0)
    ADD_NS_FUNCTION_CPP(Time, currentDateTimeToMs,  JsTime::currentDateTimeToMs, 0)
    ADD_NS_FUNCTION_JS (Time, timeTheCode, function(name, func) {
      var t1 = Time.now();
      func();
      var t2 = Time.now();
      print("-- tm: step "+name+" took "+(t2-t1)+" sec(s) (wallclock) --");
    })
  END_NAMESPACE(Time)
  ADD_JS_FUNCTION(system, 1)
  ADD_JS_FUNCTION(formatFp, 2)
  ADD_JS_FUNCTION(download, 3)
  ADD_JS_FUNCTION(downloadUrl, 1)
  ADD_JS_FUNCTION(gzip, 1)
  ADD_JS_FUNCTION(gunzip, 1)

  //
  // Read/Write functions
  //
  ADD_JS_FUNCTION(writeXyzFile, 2)
  BEGIN_NAMESPACE(Pdb)
    ADD_NS_FUNCTION_CPP(Pdb, readBuffer,  Pdb::readBuffer, 1)
  END_NAMESPACE(Pdb)
#if defined(USE_DSRPDB)
  ADD_JS_FUNCTION(readPdbFile, 1)
#endif
#if defined(USE_MMTF)
  BEGIN_NAMESPACE(Mmtf)
    ADD_NS_FUNCTION_CPP(Mmtf, readFile,   Mmtf::readFile, 1)
    ADD_NS_FUNCTION_CPP(Mmtf, readBuffer, Mmtf::readBuffer, 1)
  END_NAMESPACE(Mmtf)
#endif

  //
  // vector and matrix functions
  //

  BEGIN_NAMESPACE(Vec3)
    ADD_NS_FUNCTION_CPP(Vec3, almostEquals, JsVec3::almostEquals, 3)
    ADD_NS_FUNCTION_CPP(Vec3, zero,         JsVec3::zero, 0)
    ADD_NS_FUNCTION_CPP(Vec3, length,       JsVec3::length, 1)
    ADD_NS_FUNCTION_CPP(Vec3, normalize,    JsVec3::normalize, 1)
    ADD_NS_FUNCTION_CPP(Vec3, normalizeZ,   JsVec3::normalizeZ, 1)
    ADD_NS_FUNCTION_CPP(Vec3, normalizeZl,  JsVec3::normalizeZl, 2)
    ADD_NS_FUNCTION_CPP(Vec3, plus,         JsVec3::plus, 2)
    ADD_NS_FUNCTION_CPP(Vec3, minus,        JsVec3::minus, 2)
    ADD_NS_FUNCTION_CPP(Vec3, muln,         JsVec3::muln, 2)
    ADD_NS_FUNCTION_CPP(Vec3, dot,          JsVec3::dot, 2)
    ADD_NS_FUNCTION_CPP(Vec3, cross,        JsVec3::cross, 2)
    ADD_NS_FUNCTION_CPP(Vec3, angleRad,     JsVec3::angleRad, 2)
    ADD_NS_FUNCTION_CPP(Vec3, angleDeg,     JsVec3::angleDeg, 2)
    ADD_NS_FUNCTION_CPP(Vec3, rmsd,         JsVec3::rmsd, 2)
    ADD_NS_FUNCTION_CPP(Vec3, snapToGrid,   JsVec3::snapToGrid, 2)
    ADD_NS_FUNCTION_CPP(Vec3, project,      JsVec3::project, 1)
    ADD_NS_FUNCTION_CPP(Vec3, orthogonal,   JsVec3::orthogonal, 2)
    // -ext methods
    ADD_NS_FUNCTION_CPP(Vec3, rotateCornerToCorner, JsVec3::rotateCornerToCorner, 4)
    ADD_NS_FUNCTION_CPP(Vec3, angleAxis1x1, JsVec3::angleAxis1x1, 3)
    ADD_NS_FUNCTION_CPP(Vec3, angleAxis2x1, JsVec3::angleAxis2x1, 4)
  END_NAMESPACE(Vec3)
  BEGIN_NAMESPACE(Mat3)
    ADD_NS_FUNCTION_CPP(Mat3, almostEquals, JsMat3::almostEquals, 3)
    ADD_NS_FUNCTION_CPP(Mat3, zero,         JsMat3::zero, 0)
    ADD_NS_FUNCTION_CPP(Mat3, identity,     JsMat3::identity, 0)
    ADD_NS_FUNCTION_CPP(Mat3, transpose,    JsMat3::transpose, 1)
    ADD_NS_FUNCTION_CPP(Mat3, plus,         JsMat3::plus, 2)
    ADD_NS_FUNCTION_CPP(Mat3, minus,        JsMat3::minus, 2)
    ADD_NS_FUNCTION_CPP(Mat3, muln,         JsMat3::muln, 2)
    ADD_NS_FUNCTION_CPP(Mat3, mulv,         JsMat3::mulv, 2)
    ADD_NS_FUNCTION_CPP(Mat3, mul,          JsMat3::mul, 2)
    ADD_NS_FUNCTION_CPP(Mat3, rotate,       JsMat3::rotate, 1)
  END_NAMESPACE(Mat3)
  BEGIN_NAMESPACE(Dl)
    ADD_NS_FUNCTION_CPP(Dl, open,           JsDl::open, 2)
    ADD_NS_FUNCTION_CPP(Dl, sym,            JsDl::sym, 2)
    ADD_NS_FUNCTION_CPP(Dl, close,          JsDl::close, 1)
  END_NAMESPACE(Dl)
  BEGIN_NAMESPACE(Invoke)
    ADD_NS_FUNCTION_CPP(Invoke, intStrOPtr,                               JsInvoke::intStrOPtr, 1)
    ADD_NS_FUNCTION_CPP(Invoke, intPtrStrCb4intOpaqueIntSArrSArrPtrOStr,  JsInvoke::intPtrStrCb4intOpaqueIntSArrSArrPtrOStr, 5)
    ADD_NS_FUNCTION_CPP(Invoke, intPtr,                                   JsInvoke::intPtr, 2)
    ADD_NS_FUNCTION_CPP(Invoke, strInt,                                   JsInvoke::strInt, 2)
  END_NAMESPACE(Invoke)
  BEGIN_NAMESPACE(Moleculex) // TODO figure out how to have the same namespace for methodsand functions
    ADD_NS_FUNCTION_CPP(Moleculex, fromXyzOne, JsMolecule::fromXyzOne, 1)
    ADD_NS_FUNCTION_CPP(Moleculex, fromXyzMany, JsMolecule::fromXyzMany, 1)
    ADD_NS_FUNCTION_CPP(Moleculex, listNeighborsHierarchically, JsMolecule::listNeighborsHierarchically, 4)
#if defined(USE_OPENBABEL)
    ADD_NS_FUNCTION_CPP(Moleculex, fromSMILES, JsMolecule::fromSMILES, 2)
#endif
    ADD_NS_FUNCTION_JS (Moleculex, rmsd, function(m1, m2) {return Vec3.rmsd(m1.extractCoords(), m2.extractCoords())})
    ADD_NS_FUNCTION_CPP(Moleculex, angleIntToStr, JsMolecule::angleIntToStr, 1)
    ADD_NS_FUNCTION_CPP(Moleculex, angleStrToInt, JsMolecule::angleStrToInt, 1)
  END_NAMESPACE(Moleculex)
  BEGIN_NAMESPACE(Arrayx)
    ADD_NS_FUNCTION_JS (Arrayx, min, function(arr) {
      var idx = 0;
      var mi = -1;
      var mv = 0;
      arr.forEach(function(v) {
        if (idx == 0) {
          mv = v;
          mi = idx;
        } else if (v < mv) {
          mv = v;
          mi = idx;
        };
        idx++;
      });
      return [mi, mv]
    })
    ADD_NS_FUNCTION_JS (Arrayx, max, function(arr) {
      var idx = 0;
      var mi = -1;
      var mv = 0;
      arr.forEach(function(v) {
        if (idx == 0) {
          mv = v;
          mi = idx;
        } else if (v > mv) {
          mv = v;
          mi = idx;
        };
        idx++;
      });
      return [mi, mv]
    })
    ADD_NS_FUNCTION_JS (Arrayx, removeEmpty, function(arr) {
      return arr.filter(function (e) {
        return e != ""
      })
    })
  END_NAMESPACE(Arrayx)

#undef BEGIN_NAMESPACE
#undef ADD_NS_FUNCTION_CPP
#undef ADD_NS_FUNCTION_JS
#undef END_NAMESPACE
#undef ADD_JS_FUNCTION

  //
  // require() inspired from mujs
  //

  js_dostring(J,
    "function require(name) {\n"
    "  var cache = require.cache\n"
    "  if (name in cache) return cache[name]\n"
    "  var exports = {}\n"
    "  cache[name] = exports\n"
    "  Function('exports', File.read(name.indexOf('/')==-1 ? 'modules/'+name+'.js' : name+'.js'))(exports)\n"
    "  return exports\n"
    "}\n"
    "require.cache = Object.create(null)\n"
  );
}

} // JsBinding

