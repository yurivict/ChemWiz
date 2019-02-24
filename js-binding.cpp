
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <string.h>

#include <fstream>
#include <sstream>
#include <map>
#include <cmath>
#include <memory>

#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>

#include <unistd.h>
#include <mujs.h>

#include "js-binding.h"
#include "obj.h"
#include "molecule.h"
#include "calculators.h"
#include "temp-file.h"
#include "tm.h"
#include "process.h"
#include "web-io.h"
#include "op-rmsd.h"

// types defined locally
typedef std::vector<uint8_t> Binary;

static const char *TAG_Obj        = "Obj";
static const char *TAG_Binary     = "Binary";
static const char *TAG_Molecule   = "Molecule";
static const char *TAG_Atom       = "Atom";
static const char *TAG_TempFile   = "TempFile";
static const char *TAG_CalcEngine = "CalcEngine";

#define AssertNargs(n)           assert(js_gettop(J) == n+1);
#define AssertNargsRange(n1,n2)  assert(n1+1 <= js_gettop(J) && js_gettop(J) <= n2+1);
#define AssertNargs2(nmin,nmax)  assert(nmin+1 <= js_gettop(J) && js_gettop(J) <= nmax+1);
#define AssertStack(n)           assert(js_gettop(J) == n);
#define DbgPrintStackLevel(loc)  std::cout << "DBG JS Stack: @" << loc << " level=" << js_gettop(J) << std::endl
#define GetNArgs()               (js_gettop(J)-1)
#define GetArg(type, n)          ((type*)js_touserdata(J, n, TAG_##type))
#define GetArgBoolean(n)         js_toboolean(J, n)
#define GetArgFloat(n)           js_tonumber(J, n)
#define GetArgString(n)          std::string(js_tostring(J, n))
#define GetArgStringCptr(n)      js_tostring(J, n)
#define GetArgInt32(n)           js_toint32(J, n)
#define GetArgUInt32(n)          js_touint32(J, n)
#define GetArgSSMap(n)           objToMap(J, n)
#define GetArgVec3(n)            objToVec3(J, n)
#define GetArgMat3x3(n)          objToMat3x3(J, n)
#define GetArgMatNxX(n,N)        objToMatNxX<N>(J, n)
#define GetArgElement(n)         elementFromString(GetArgString(n))
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

namespace JsBinding {

//
// helper functions
//

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

static void calcEngineFinalize(js_State *J, void *p) {
  delete (CalcEngine*)p;
}

static void tempFileFinalize(js_State *J, void *p) {
  delete (TempFile*)p;
}

template<typename A, typename FnNew>
static void returnArrayUserData(js_State *J, const A &arr, const char *tag, void(*finalize)(js_State *J, void *p), FnNew fnNew) {
  js_newarray(J);
  unsigned idx = 0;
  for (auto a : arr) {
    fnNew(J, a);
    js_setindex(J, -2, idx++);
  }
}

static std::map<std::string,std::string> objToMap(js_State *J, int idx) {
  std::map<std::string,std::string> mres;
  if (!js_isobject(J, idx))
    ERROR("objToMap: not an object");
  js_pushiterator(J, idx/*idx*/, 1/*own*/);
  const char *key;
  while ((key = js_nextiterator(J, -1))) {
    js_getproperty(J, idx, key);
    if (!js_isstring(J, -1))
      ERROR("objToMap: value isn't a string");
    const char *val = js_tostring(J, -1);
    mres[key] = val;
    js_pop(J, 1);
  }
  return mres;
}

static Vec3 objToVec3(js_State *J, int idx) {
  if (!js_isarray(J, idx))
    ERROR("objToVec3: not an array");
  if (js_getlength(J, idx) != 3)
    ERROR("objToVec3: array size isn't 3");

  Vec3 v;

  for (unsigned i = 0; i < 3; i++) {
    js_getindex(J, idx, i);
    v[i] = js_tonumber(J, -1);
    js_pop(J, 1);
  }

  return v;
}

template<unsigned N>
static void objToVecVa(js_State *J, int idx, std::valarray<double> *va, unsigned vaIdx) {
  if (!js_isarray(J, idx))
    ERROR("objToVec: not an array");
  if (js_getlength(J, idx) != N)
    ERROR("objToVec: array size isn't " << N);

  for (unsigned i = 0; i < N; i++) {
    js_getindex(J, idx, i);
    (*va)[vaIdx + i] = js_tonumber(J, -1);
    js_pop(J, 1);
  }
}

static Mat3 objToMat3x3(js_State *J, int idx) {
  if (!js_isarray(J, idx))
    ERROR("objToMat3x3: not an array");
  if (js_getlength(J, idx) != 3)
    ERROR("objToMat3x3: array size isn't 3");

  Mat3 m;

  for (unsigned i = 0; i < 3; i++) {
    js_getindex(J, idx, i);
    m[i] = objToVec3(J, -1);
    js_pop(J, 1);
  }

  return m;
}

template<unsigned N>
static std::valarray<double>* objToMatNxX(js_State *J, int idx) {
  if (!js_isarray(J, idx))
    ERROR("objToMatNxX: not an array");
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

static void PushVec(js_State *J, const Vec3 &v) {
  js_newarray(J);
  unsigned idx = 0;
  for (auto c : v) {
    js_pushnumber(J, c);
    js_setindex(J, -2, idx++);
  }
}

static void ReturnVoid(js_State *J) {
  js_pushundefined(J);
}

//static void ReturnNull(js_State *J) {
//  js_pushnull(J);
//}

static void ReturnBoolean(js_State *J, bool b) {
  js_pushboolean(J, b ? 1 : 0);
}

static void ReturnFloat(js_State *J, Float f) {
  js_pushnumber(J, f);
}

static void ReturnUnsigned(js_State *J, unsigned u) {
  js_pushnumber(J, u);
}

static void ReturnString(js_State *J, const std::string &s) {
  js_pushstring(J, s.c_str());
}

static void ReturnVec(js_State *J, const Vec3 &v) {
  PushVec(J, v);
}

static void ReturnMat(js_State *J, const Mat3 &m) {
  js_newarray(J);
  unsigned idx = 0;
  for (auto &r : m) {
    PushVec(J, r);
    js_setindex(J, -2, idx++);
  }
}

// convenience macro to return objects
#define Return(type, v) Js##type::xnewo(J, v)
#define ReturnZ(type, v) Js##type::xnewoZ(J, v)

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
  Return(Obj, new Obj);
}

namespace prototype {

static void id(js_State *J) {
  AssertNargs(0)
  ReturnString(J, GetArg(Obj, 0)->id());
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
  Return(Binary, b);
}

namespace prototype {

static void dupl(js_State *J) {
  AssertNargs(0)
  Return(Binary, new Binary(*GetArg(Binary, 0)));
}

static void size(js_State *J) {
  AssertNargs(0)
  ReturnUnsigned(J, GetArg(Binary, 0)->size());
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
  Return(Binary, b);
}

static void toString(js_State *J) {
  AssertNargs(0)
  auto b = GetArg(Binary, 0);
  ReturnString(J, std::string(b->begin(), b->end()));
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
  Return(Atom, new Atom(elementFromString(GetArgString(1)), GetArgVec3(2)));
}

namespace prototype {

static void id(js_State *J) {
  AssertNargs(0)
  ReturnString(J, GetArg(Atom, 0)->id());
}

static void dupl(js_State *J) {
  AssertNargs(0)
  Return(Atom, new Atom(*GetArg(Atom, 0)));
}

static void str(js_State *J) {
  AssertNargs(0)
  auto a = GetArg(Atom, 0);
  ReturnString(J, str(boost::format("atom{%1% elt=%2% pos=%3%}") % a % a->elt % a->pos));
}

static void isEqual(js_State *J) {
  AssertNargs(1)
  ReturnBoolean(J, GetArg(Atom, 0)->isEqual(*GetArg(Atom, 1)));
}

static void getElement(js_State *J) {
  AssertNargs(0)
  ReturnString(J, str(boost::format("%1%") % GetArg(Atom, 0)->elt));
}

static void getPos(js_State *J) {
  AssertNargs(0)
  ReturnVec(J, GetArg(Atom, 0)->pos);
}

static void setPos(js_State *J) {
  AssertNargs(1)
  GetArg(Atom, 0)->pos = GetArgVec3(1);
  ReturnVoid(J);
}

static void getName(js_State *J) {
  AssertNargs(0)
  ReturnString(J, GetArg(Atom, 0)->name);
}

static void setName(js_State *J) {
  AssertNargs(1)
  GetArg(Atom, 0)->name = GetArgString(1);
  ReturnVoid(J);
}

static void getHetAtm(js_State *J) {
  AssertNargs(0)
  ReturnBoolean(J, GetArg(Atom, 0)->isHetAtm);
}

static void setHetAtm(js_State *J) {
  AssertNargs(1)
  GetArg(Atom, 0)->isHetAtm = GetArgBoolean(1);
  ReturnVoid(J);
}

static void getNumBonds(js_State *J) {
  AssertNargs(0)
  auto a = GetArg(Atom, 0);
  ReturnUnsigned(J, a->nbonds());
}

static void getBonds(js_State *J) {
  AssertNargs(0)
  auto a = GetArg(Atom, 0);
  returnArrayUserData<std::vector<Atom*>, void(*)(js_State*,Atom*)>(J, a->bonds, TAG_Atom, atomFinalize, JsAtom::xnewo);
}

static void hasBond(js_State *J) {
  AssertNargs(1)
  ReturnBoolean(J, GetArg(Atom, 0)->hasBond(GetArg(Atom, 1)));
}

static void getOtherBondOf3(js_State *J) {
  AssertNargs(2)
  Return(Atom, GetArg(Atom, 0)->getOtherBondOf3(GetArg(Atom,1), GetArg(Atom,2)));
}

static void findSingleNeighbor(js_State *J) {
  AssertNargs(1)
  ReturnZ(Atom, GetArg(Atom, 0)->findSingleNeighbor(GetArgElement(1)));
}

static void findSingleNeighbor2(js_State *J) {
  AssertNargs(2)
  ReturnZ(Atom, GetArg(Atom, 0)->findSingleNeighbor2(GetArgElement(1), GetArgElement(2)));
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
    ADD_METHOD_JS (Atom, findBonds, function(filter) {return this.getBonds().filter(filter)})
    ADD_METHOD_JS (Atom, angleBetweenRad, function(a1, a2) {var p = this.getPos(); return vecAngleRad(vecMinus(a1.getPos(),p), vecMinus(a2.getPos(),p))})
    ADD_METHOD_JS (Atom, angleBetweenDeg, function(a1, a2) {var p = this.getPos(); return vecAngleDeg(vecMinus(a1.getPos(),p), vecMinus(a2.getPos(),p))})
    ADD_METHOD_JS (Atom, distance, function(othr) {return vecLength(vecMinus(this.getPos(), othr.getPos()))})
  }
  js_pop(J, 2);
  AssertStack(0);
}

} // JsAtom

namespace JsMolecule {

static void xnewo(js_State *J, Molecule *m) {
  js_getglobal(J, TAG_Molecule);
  js_getproperty(J, -1, "prototype");
  js_newuserdata(J, TAG_Molecule, m, moleculeFinalize);
}

static void xnew(js_State *J) {
  Return(Molecule, new Molecule("created-in-script"));
}

namespace prototype {

static void id(js_State *J) {
  AssertNargs(0)
  ReturnString(J, GetArg(Molecule, 0)->id());
}

static void dupl(js_State *J) {
  AssertNargs(0)
  Return(Molecule, new Molecule(*GetArg(Molecule, 0)));
}

static void str(js_State *J) {
  AssertNargs(0)
  auto m = GetArg(Molecule, 0);
  ReturnString(J, str(boost::format("molecule{%1%, id=%2%}") % m % m->idx.c_str()));
}

static void numAtoms(js_State *J) {
  AssertNargs(0)
  ReturnUnsigned(J, GetArg(Molecule, 0)->getNumAtoms());
}

static void getAtoms(js_State *J) {
  AssertNargs(0)
  auto m = GetArg(Molecule, 0);
  returnArrayUserData<std::vector<Atom*>, void(*)(js_State*,Atom*)>(J, m->atoms, TAG_Atom, atomFinalize, JsAtom::xnewo);
}

static void addAtom(js_State *J) {
  AssertNargs(1)
  GetArg(Molecule, 0)->add(GetArg(Atom, 1));
  ReturnVoid(J);
}

static void appendAminoAcid(js_State *J) {
  AssertNargs(1)
  auto m = GetArg(Molecule, 0);
  Molecule aa(*GetArg(Molecule, 1)); // copy because it will be altered
  m->appendAsAminoAcidChain(aa);
}

static void findAaCterm(js_State *J) {
  AssertNargs(0)
  auto m = GetArg(Molecule, 0);
  auto Cterm = m->findAaCterm();
  returnArrayUserData<std::array<Atom*,5>, void(*)(js_State*,Atom*)>(J, Cterm, TAG_Atom, atomFinalize, JsAtom::xnewo);
}

static void findAaNterm(js_State *J) {
  AssertNargs(0)
  auto m = GetArg(Molecule, 0);
  auto Nterm = m->findAaNterm();
  returnArrayUserData<std::array<Atom*,3>, void(*)(js_State*,Atom*)>(J, Nterm, TAG_Atom, atomFinalize, JsAtom::xnewo);
}

static void findAaLast(js_State *J) {
  AssertNargs(0)
  auto m = GetArg(Molecule, 0);
  auto aa = m->findAaLast();
  returnArrayUserData<std::vector<Atom*>, void(*)(js_State*,Atom*)>(J, aa, TAG_Atom, atomFinalize, JsAtom::xnewo);
}

static void isEqual(js_State *J) {
  AssertNargs(1)
  ReturnBoolean(J, GetArg(Molecule, 0)->isEqual(*GetArg(Molecule, 1)));
}

static void toXyz(js_State *J) {
  AssertNargs(0)
  std::ostringstream ss;
  ss << *GetArg(Molecule, 0);
  ReturnString(J, ss.str());
}

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
    ADD_METHOD_CPP(Molecule, getAtoms, 0)
    ADD_METHOD_CPP(Molecule, addAtom, 1)
    ADD_METHOD_JS (Molecule, findAtoms, function(filter) {return this.getAtoms().filter(filter)})
    ADD_METHOD_CPP(Molecule, appendAminoAcid, 1)
    ADD_METHOD_CPP(Molecule, findAaCterm, 0)
    ADD_METHOD_CPP(Molecule, findAaNterm, 0)
    ADD_METHOD_CPP(Molecule, findAaLast, 0)
    ADD_METHOD_CPP(Molecule, isEqual, 1)
    ADD_METHOD_CPP(Molecule, toXyz, 0)
  }
  js_pop(J, 2);
  AssertStack(0);
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
    Return(TempFile, new TempFile);
    break;
  case 1: // (fileName)
    Return(TempFile, new TempFile(GetArgString(1)));
    break;
  case 2: // (fileName, content)
    Return(TempFile, new TempFile(GetArgString(1), GetArgString(2)));
    break;
  }
}

namespace prototype {

static void str(js_State *J) {
  AssertNargs(0)
  auto f = GetArg(TempFile, 0);
  ReturnString(J, str(boost::format("temp-file{%1%}") % f->getFname()));
}

static void fname(js_State *J) {
  AssertNargs(0)
  ReturnString(J, GetArg(TempFile, 0)->getFname());
}

static void toBinary(js_State *J) {
  AssertNargs(0)
  Return(Binary, GetArg(TempFile, 0)->toBinary());
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

namespace JsCalcEngine {

static void xnewo(js_State *J, CalcEngine *ce) {
  js_getglobal(J, TAG_CalcEngine);
  js_getproperty(J, -1, "prototype");
  js_newuserdata(J, TAG_CalcEngine, ce, calcEngineFinalize);
}

static void xnew(js_State *J) {
  AssertNargs(1)

  if (auto e = CalcEngine::create(GetArgString(1)))
    Return(CalcEngine, new Calculators::engines::Erkale);
  else
    ERROR(str(boost::format("unknown calc-engine '%1%' requested") % GetArgString(1)));
}

namespace prototype {

static void str(js_State *J) {
  AssertNargs(0)
  auto f = GetArg(CalcEngine, 0);
  ReturnString(J, str(boost::format("calc-engine{%1%}") % f->kind()));
}

static void kind(js_State *J) {
  AssertNargs(0)
  ReturnString(J, GetArg(CalcEngine, 0)->kind());
}

static void calcEnergy(js_State *J) {
  AssertNargsRange(1,2)
  ReturnFloat(J, GetArg(CalcEngine, 0)->calcEnergy(*GetArg(Molecule, 1),  GetNArgs() == 2 ? GetArgSSMap(2) : Calculators::Params()));
}

static void calcOptimized(js_State *J) {
  AssertNargsRange(1,2)
  Return(Molecule, GetArg(CalcEngine, 0)->calcOptimized(*GetArg(Molecule, 1),  GetNArgs() == 2 ? GetArgSSMap(2) : Calculators::Params()));
}

}

static void init(js_State *J) {
  InitObjectRegistry(J, TAG_CalcEngine);
  js_pushglobal(J);
  ADD_JS_CONSTRUCTOR(CalcEngine)
  js_getglobal(J, TAG_CalcEngine);
  js_getproperty(J, -1, "prototype");
  StackPopPrevious()
  { // methods
    ADD_METHOD_CPP(CalcEngine, str, 0)
    ADD_METHOD_CPP(CalcEngine, kind, 0)
    ADD_METHOD_CPP(CalcEngine, calcEnergy, 2/*1..2*/)
    ADD_METHOD_CPP(CalcEngine, calcOptimized, 2/*1..2*/)
  }
  js_pop(J, 2);
  AssertStack(0);
}

} // JsCalcEngine

//
// exported functions
//

static void print(js_State *J) {
  unsigned int i, top = js_gettop(J);
  for (i = 1; i < top; ++i) {
    auto s = GetArgString(i);
    if (i > 1) putchar(' ');
    fputs(s.c_str(), stdout);
  }
  putchar('\n');
  ReturnVoid(J);
}

static void printn(js_State *J) {
  unsigned int i, top = js_gettop(J);
  for (i = 1; i < top; ++i) {
    auto s = GetArgString(i);
    if (i > 1) putchar(' ');
    fputs(s.c_str(), stdout);
  }
  ReturnVoid(J);
}

static void fileExists(js_State *J) {
  AssertNargs(1)
  ReturnBoolean(J, std::ifstream(GetArgString(1)).good());
}

static void fileRead(js_State *J) { // reads file into a string
  AssertNargs(1)
  auto fname = GetArgString(1);

  using it = std::istreambuf_iterator<char>;

  std::ifstream file;
  file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  try {
    file.open(fname, std::ios::in);
    ReturnString(J, std::string(it(file), it()));
    file.close();
  } catch (std::ifstream::failure e) {
    ERROR(str(boost::format("can't read the file '%1%': %2%") % fname % e.what()));
  }
}

static void fileWrite(js_State *J) { // writes string into a file
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

static void sleep(js_State *J) {
  AssertNargs(1)
  auto secs = GetArgUInt32(1);
  ::sleep(secs);
  ReturnVoid(J);
}

static void tmStart(js_State *J) {
  AssertNargs(0)
  js_pushnumber(J, Tm::start());
}

static void tmNow(js_State *J) {
  AssertNargs(0)
  js_pushnumber(J, Tm::now());
}

static void tmWallclock(js_State *J) {
  AssertNargs(0)
  js_pushnumber(J, Tm::wallclock());
}

static void system(js_State *J) {
  AssertNargs(1)
  ReturnString(J, Process::exec(GetArgString(1)));
}

static void formatFp(js_State *J) {
  AssertNargs(2)
  char buf[32];
  ::sprintf(buf, "%.*lf", GetArgUInt32(2), GetArgFloat(1));
  ReturnString(J, buf);
}

static void download(js_State *J) {
  AssertNargs(3)
  Return(Binary, WebIo::download(GetArgString(1), GetArgString(2), GetArgString(3)));
}

static void downloadUrl(js_State *J) {
  AssertNargs(1)
  Return(Binary, WebIo::downloadUrl(GetArgString(1)));
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

  Return(Binary, b);
}

static void gzip(js_State *J) {
  namespace io = boost::iostreams;
  gxzip(J, io::gzip_compressor());
}

static void gunzip(js_State *J) {
  namespace io = boost::iostreams;
  gxzip(J, io::gzip_decompressor());
}

static void readXyzFile(js_State *J) {
  AssertNargs(1)
  Return(Molecule, Molecule::readXyzFile(GetArgString(1)));
}

static void writeXyzFile(js_State *J) {
  AssertNargs(2)
  std::ofstream(GetArgString(2), std::ios::out) << *GetArg(Molecule, 1);
  ReturnVoid(J);
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
static void readMmtfFile(js_State *J) {
  AssertNargs(1)
  auto mols = Molecule::readMmtfFile(GetArgString(1));
  for (auto m : mols)
    JsMolecule::xnewo(J, m);
}

static void readMmtfBuffer(js_State *J) {
  AssertNargs(1)
  auto mols = Molecule::readMmtfBuffer(GetArg(Binary, 1));
  for (auto m : mols)
    JsMolecule::xnewo(J, m);
}
#endif

#if defined(USE_OPENBABEL)
static void moleculeFromSMILES(js_State *J) {
  AssertNargs(2)
  Return(Molecule, Molecule::createFromSMILES(GetArgString(1), GetArgString(2)));
}
#endif

//
// vector and matrix operations
//

static void vecPlus(js_State *J) {
  AssertNargs(2)
  ReturnVec(J, GetArgVec3(1)+GetArgVec3(2));
}

static void vecMinus(js_State *J) {
  AssertNargs(2)
  ReturnVec(J, GetArgVec3(1)-GetArgVec3(2));
}

static void vecLength(js_State *J) {
  AssertNargs(1)
  auto v = GetArgVec3(1);
  ReturnFloat(J, v.len());
}

static void vecAngleRad(js_State *J) { // angle in radians between v1-v and v2-v, ASSUMES that v1!=v and v2!=v
  AssertNargs(2)
  ReturnFloat(J, GetArgVec3(1).angle(GetArgVec3(2)));
}

static void vecAngleDeg(js_State *J) { // angle in degrees between v1-v and v2-v, ASSUMES that v1!=v and v2!=v
  AssertNargs(2)
  ReturnFloat(J, GetArgVec3(1).angle(GetArgVec3(2))/M_PI*180);
}

/*
static void matPlus(js_State *J) {
  AssertNargs(2)
  auto m1 = GetArgMat3x3(1);
  auto m2 = GetArgMat3x3(2);
  ReturnMat(J, m1+m2);
}

static void matMinus(js_State *J) {
  AssertNargs(2)
  auto m1 = GetArgMat3x3(1);
  auto m2 = GetArgMat3x3(2);
  ReturnMat(J, m1-m2);
}
*/
static void mulMatVec(js_State *J) {
  AssertNargs(2)
  ReturnVec(J, GetArgMat3x3(1)*GetArgVec3(2));
}

static void mulMatMat(js_State *J) {
  AssertNargs(2)
  ReturnMat(J, GetArgMat3x3(1)*GetArgMat3x3(2));
}

static void dotVecVec(js_State *J) {
  AssertNargs(2)
  ReturnFloat(J, GetArgVec3(1)*GetArgVec3(2));
}

static void crossVecVec(js_State *J) {
  AssertNargs(2)
  ReturnVec(J, GetArgVec3(1).cross(GetArgVec3(2)));
}

static void matRotate(js_State *J) {
  AssertNargs(1)
  ReturnMat(J, Mat3::rotate(GetArgVec3(1)));
}

static void rmsd(js_State *J) {
  AssertNargs(2)
  std::unique_ptr<std::valarray<double>> v1(GetArgMatNxX(1, 3/*N=3*/));
  std::unique_ptr<std::valarray<double>> v2(GetArgMatNxX(2, 3/*N=3*/));
  ReturnFloat(J, Op::rmsd(*v1, *v2));
}

static void jsB_propf(js_State *J, const char *name, js_CFunction cfun, int n) { // borrowed from the MuJS sources
  const char *pname = strrchr(name, '.');
  pname = pname ? pname + 1 : name;
  js_newcfunction(J, cfun, name, n);
  js_defproperty(J, -2, pname, JS_DONTENUM);
}

void registerFunctions(js_State *J) {

  //
  // init types
  //

  JsObj::init(J);
  JsBinary::init(J);
  JsAtom::init(J);
  JsMolecule::init(J);
  JsTempFile::init(J);
  JsCalcEngine::init(J);

#define ADD_JS_FUNCTION(name, num) \
  js_newcfunction(J, JsBinding::name, #name, num); \
  js_setglobal(J, #name);

// macros to define static functions in global namespaces
#define BEGIN_NAMESPACE()                         js_newobject(J);
#define ADD_STATIC_FUNCTION(ns, jsfn, cfn, nargs) jsB_propf(J, #ns "." #jsfn, cfn, nargs);
#define END_NAMESPACE(name)                       js_defglobal(J, #name, JS_DONTENUM);

  //
  // Misc
  //

  ADD_JS_FUNCTION(print, 1)
  ADD_JS_FUNCTION(printn, 1)
  ADD_JS_FUNCTION(fileExists, 1)
  ADD_JS_FUNCTION(fileRead, 1)
  ADD_JS_FUNCTION(fileWrite, 2)
  ADD_JS_FUNCTION(sleep, 1)
  ADD_JS_FUNCTION(tmStart, 0)
  ADD_JS_FUNCTION(tmNow, 0)
  ADD_JS_FUNCTION(tmWallclock, 0)
  ADD_JS_FUNCTION(system, 1)
  ADD_JS_FUNCTION(formatFp, 2)
  ADD_JS_FUNCTION(download, 3)
  ADD_JS_FUNCTION(downloadUrl, 1)
  ADD_JS_FUNCTION(gzip, 1)
  ADD_JS_FUNCTION(gunzip, 1)

  //
  // Read/Write functions
  //
  ADD_JS_FUNCTION(readXyzFile, 1)
  ADD_JS_FUNCTION(writeXyzFile, 2)
#if defined(USE_DSRPDB)
  ADD_JS_FUNCTION(readPdbFile, 1)
#endif
#if defined(USE_MMTF)
  ADD_JS_FUNCTION(readMmtfFile, 1)
  ADD_JS_FUNCTION(readMmtfBuffer, 1)
#endif
#if defined(USE_OPENBABEL)
  ADD_JS_FUNCTION(moleculeFromSMILES, 2)
#endif

  //
  // vector and matrix methods
  //

  ADD_JS_FUNCTION(vecPlus, 2)
  ADD_JS_FUNCTION(vecMinus, 2)
  ADD_JS_FUNCTION(vecLength, 1)
  ADD_JS_FUNCTION(vecAngleRad, 2)
  ADD_JS_FUNCTION(vecAngleDeg, 2)
  //ADD_JS_FUNCTION(matPlus, 2)
  //ADD_JS_FUNCTION(matMinus, 2)
  ADD_JS_FUNCTION(mulMatVec, 2)
  ADD_JS_FUNCTION(mulMatMat, 2)
  ADD_JS_FUNCTION(dotVecVec, 2)
  ADD_JS_FUNCTION(crossVecVec, 2)
  ADD_JS_FUNCTION(matRotate, 1)

  BEGIN_NAMESPACE()
    ADD_STATIC_FUNCTION(Vec3, rmsd, rmsd, 2)
  END_NAMESPACE(Vec3)

#undef BEGIN_NAMESPACE
#undef ADD_STATIC_FUNCTION
#undef END_NAMESPACE
#undef ADD_JS_FUNCTION

  //
  // require() inspired from mujs
  //

  js_dostring(J,
    "function require(name) {\n"
    "  var cache = require.cache;\n"
    "  if (name in cache) return cache[name];\n"
    "  var exports = {};\n"
    "  cache[name] = exports;\n"
    "  Function('exports', fileRead(name.indexOf('/')==-1 ? 'modules/'+name+'.js' : name+'.js'))(exports);\n"
    "  return exports;\n"
    "}\n"
    "require.cache = Object.create(null);\n"
  );
}

}; // JsBinding

