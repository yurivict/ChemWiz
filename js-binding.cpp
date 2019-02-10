
#include <stdio.h>
#include <assert.h>
#include <math.h>

#include <fstream>
#include <map>

#include <boost/format.hpp>

#include <unistd.h>
#include <mujs.h>

#include "js-binding.h"
#include "molecule.h"
#include "calculators.h"
#include "tm.h"

static const char *TAG_Molecule   = "Molecule";
static const char *TAG_Atom       = "Atom";
static const char *TAG_CalcEngine = "CalcEngine";

#define AssertNargs(n)  assert(js_gettop(J) == n+1);
#define AssertNargs2(nmin,nmax)  assert(nmin+1 <= js_gettop(J) && js_gettop(J) <= nmax+1);
#define GetNArgs() (js_gettop(J)-1)
#define GetArg(type, n) ((type*)js_touserdata(J, n, TAG_##type))
#define GetArgUInt32(n) js_touint32(J, n)
#define GetArgString(n) std::string(js_tostring(J, n))
#define GetArgSSMap(n) objToMap(J, n)
#define GetArgVec(n) objToVec(J, n)
#define GetArgMat(n) objToMat(J, n)

namespace JsBinding {

//
// helper functions
//

static void moleculeFinalize(js_State *J, void *p) {
  Molecule *m = (Molecule*)p;
  if (Molecule::dbgIsAllocated(m))
    delete m;
  else
    std::cerr << "ERROR bogus molecule in moleculeFinalize: " << m << " (duplicate finalize invocation)" << std::endl;
}

static void atomFinalize(js_State *J, void *p) {
  // noop: Atom objects are part of Molecule objects, and aren't separately destroyed when bound to JS for now
}

static void calcEngineFinalize(js_State *J, void *p) {
  CalcEngine *ce = (CalcEngine*)p;
  delete ce;
}

template<typename A>
static void returnArrayUserData(js_State *J, const A &arr, const char *tag, void(*finalize)(js_State *J, void *p)) {
  js_newarray(J);
  unsigned idx = 0;
  for (auto a : arr) {
    js_pushnull(J);
    js_newuserdata(J, tag, a, finalize);
    js_setindex(J, -2, idx++);
  }
}

static std::map<std::string,std::string> objToMap(js_State *J, int idx) {
  std::map<std::string,std::string> mres;
  if (!js_isobject(J, idx))
    throw Exception("objToMap: not an object");
  js_pushiterator(J, idx/*idx*/, 1/*own*/);
  const char *key;
  while ((key = js_nextiterator(J, -1))) {
    js_getproperty(J, idx, key);
    if (!js_isstring(J, -1))
      throw Exception("objToMap: value isn't a string");
    const char *val = js_tostring(J, -1);
    mres[key] = val;
    js_pop(J, 1);
  }
  return mres;
}

static Vec3 objToVec(js_State *J, int idx) {
  if (!js_isarray(J, idx))
    throw Exception("objToVec: not an array");
  if (js_getlength(J, idx) != 3)
    throw Exception("objToVec: array size isn't 3");

  Vec3 v;

  for (unsigned i = 0; i < 3; i++) {
    js_getindex(J, idx, i);
    v[i] = js_tonumber(J, -1);
    js_pop(J, 1);
  }

  return v;
}

static Mat3 objToMat(js_State *J, int idx) {
  if (!js_isarray(J, idx))
    throw Exception("objToMat: not an array");
  if (js_getlength(J, idx) != 3)
    throw Exception("objToMat: array size isn't 3");

  Mat3 m;

  for (unsigned i = 0; i < 3; i++) {
    js_getindex(J, idx, i);
    m[i] = objToVec(J, -1);
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
  js_pushundefined(J);
}

static void sleep(js_State *J) {
  AssertNargs(1)
  auto secs = GetArgUInt32(1);
  ::sleep(secs);
  js_pushundefined(J);
}

static void myimport(js_State *J) {
  AssertNargs(1)
  auto fname = GetArgString(1);
  if (js_dofile(J, fname.c_str()))
    throw Exception(str(boost::format("failed to import the JS module '%1%'") % fname));
  js_pushundefined(J);
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

static void pi(js_State *J) {
  AssertNargs(0)
  js_pushnumber(J, M_PI);
}

static void moleculeToString(js_State *J) {
  AssertNargs(1)
  auto m = GetArg(Molecule, 1);
  char str[256];
  sprintf(str, "molecule{%p, id=%s}", m, m->id.c_str());
  js_pushstring(J, str);
}

static void moleculeDupl(js_State *J) {
  AssertNargs(1)
  auto m = new Molecule(*GetArg(Molecule, 1));
  js_newuserdata(J, TAG_Molecule, m, moleculeFinalize);
}

static void atomToString(js_State *J) {
  AssertNargs(1)
  auto a = GetArg(Atom, 1);
  auto s = str(boost::format("atom{%1% elt=%2% pos=%3%}") % a % a->elt % a->pos);
  js_pushstring(J, s.c_str());
}

static void moleculeGetAtoms(js_State *J) {
  AssertNargs(1)
  auto m = GetArg(Molecule, 1);
  returnArrayUserData<std::vector<Atom*>>(J, m->atoms, TAG_Atom, atomFinalize);
}

static void moleculeAppendAminoAcid(js_State *J) {
  AssertNargs(2)
  auto m = GetArg(Molecule, 1);
  Molecule aa(*GetArg(Molecule, 2)); // copy because it will be altered
  m->appendAsAminoAcidChain(aa);
  js_newuserdata(J, TAG_Molecule, m, moleculeFinalize);
}

static void moleculeFindAaCterm(js_State *J) {
  AssertNargs(1)
  auto m = GetArg(Molecule, 1);
  auto Cterm = m->findAaCterm();
  returnArrayUserData<std::array<Atom*,5>>(J, Cterm, TAG_Atom, atomFinalize);
}

static void moleculeFindAaNterm(js_State *J) {
  AssertNargs(1)
  auto m = GetArg(Molecule, 1);
  auto Nterm = m->findAaNterm();
  returnArrayUserData<std::array<Atom*,3>>(J, Nterm, TAG_Atom, atomFinalize);
}

static void moleculeFindAaLast(js_State *J) {
  AssertNargs(1)
  auto m = GetArg(Molecule, 1);
  auto aa = m->findAaLast();
  returnArrayUserData<std::vector<Atom*>>(J, aa, TAG_Atom, atomFinalize);
}

static void atomGetElement(js_State *J) {
  AssertNargs(1)
  auto a = GetArg(Atom, 1);
  std::stringstream ss;
  ss << a->elt;
  js_pushstring(J, ss.str().c_str());
}

static void atomGetPos(js_State *J) {
  AssertNargs(1)
  auto a = GetArg(Atom, 1);
  ReturnVec(J, a->pos);
}

static void atomGetNumBonds(js_State *J) {
  AssertNargs(1)
  auto a = GetArg(Atom, 1);
  js_pushnumber(J, a->bonds.size());
}

#if defined(USE_DSRPDB)
static void readPdbFile(js_State *J) {
  AssertNargs(1)
  auto fname = GetArgString(1);
  auto pdbs = Molecule::readPdbFile(fname);
  for (auto pdb : pdbs)
    js_newuserdata(J, TAG_Molecule, pdb, moleculeFinalize);
}
#endif

static void readXyzFile(js_State *J) {
  AssertNargs(1)
  auto fname = GetArgString(1);
  auto m = Molecule::readXyzFile(fname);
  js_newuserdata(J, TAG_Molecule, m, moleculeFinalize);
}

static void writeXyzFile(js_State *J) {
  //std::cout << "> writeXyzFile" << std::endl;
  AssertNargs(2)
  auto m = GetArg(Molecule, 1);
  auto fname = GetArgString(2);
  std::ofstream file(fname, std::ios::out);
  //std::cout << "* writeXyzFile " << fname << std::endl;
  file << *m;
  js_pushundefined(J);
  //std::cout << "< writeXyzFile" << std::endl;
}

//
// Calc engines
//

static void getErkaleCalcEngine(js_State *J) {
  //std::cout << "getErkaleCalcEngine: js_gettop(J)=" << js_gettop(J) << std::endl;
  AssertNargs(0)
  js_newuserdata(J, TAG_CalcEngine, new Calculators::engines::Erkale, calcEngineFinalize);
}

static void calcMoleculeEnergy(js_State *J) {
  AssertNargs2(2,3)
  auto m = GetArg(Molecule, 1);
  auto ce = GetArg(CalcEngine, 2);
  const Calculators::Params params = GetNArgs() == 3 ? GetArgSSMap(3) : Calculators::Params();
  // calculate and return
  js_pushnumber(J, ce->calcEnergy(*m, params));
}

static void calcMoleculeOptimize(js_State *J) {
  AssertNargs2(2,3)
  auto m = GetArg(Molecule, 1);
  auto ce = GetArg(CalcEngine, 2);
  const Calculators::Params params = GetNArgs() == 3 ? GetArgSSMap(3) : Calculators::Params();
  // calculate and return
  js_newuserdata(J, TAG_Molecule, ce->calcOptimized(*m, params), moleculeFinalize);
}

static void vecPlus(js_State *J) {
  AssertNargs(2)
  auto v1 = GetArgVec(1);
  auto v2 = GetArgVec(2);
  ReturnVec(J, v1+v2);
}

static void vecMinus(js_State *J) {
  AssertNargs(2)
  auto v1 = GetArgVec(1);
  auto v2 = GetArgVec(2);
  ReturnVec(J, v1-v2);
}
/*
static void matPlus(js_State *J) {
  AssertNargs(2)
  auto m1 = GetArgMat(1);
  auto m2 = GetArgMat(2);
  ReturnMat(J, m1+m2);
}

static void matMinus(js_State *J) {
  AssertNargs(2)
  auto m1 = GetArgMat(1);
  auto m2 = GetArgMat(2);
  ReturnMat(J, m1-m2);
}
*/
static void mulMatVec(js_State *J) {
  AssertNargs(2)
  auto m = GetArgMat(1);
  auto v = GetArgVec(2);
  // calculate and return
  ReturnVec(J, m*v);
}

static void mulMatMat(js_State *J) {
  AssertNargs(2)
  auto m1 = GetArgMat(1);
  auto m2 = GetArgMat(2);
  // calculate and return
  ReturnMat(J, m1*m2);
}

static void dotVecVec(js_State *J) {
  AssertNargs(2)
  auto v1 = GetArgVec(1);
  auto v2 = GetArgVec(2);
  // calculate and return
  js_pushnumber(J, v1*v2);
}

static void crossVecVec(js_State *J) {
  AssertNargs(2)
  auto v1 = GetArgVec(1);
  auto v2 = GetArgVec(2);
  // calculate and return
  ReturnVec(J, v1.cross(v2));
}

static void matRotate(js_State *J) {
  AssertNargs(1)
  auto v = GetArgVec(1);
  // calculate and return
  ReturnMat(J, Mat3::rotate(v));
}


void registerFunctions(js_State *J) {
#define ADD_JS_FUNCTION(name, num) \
  js_newcfunction(J, JsBinding::name, #name, num); \
  js_setglobal(J, #name);

  //
  // Misc
  //

  ADD_JS_FUNCTION(print, 1)
  ADD_JS_FUNCTION(sleep, 1)
  ADD_JS_FUNCTION(myimport, 1)
  ADD_JS_FUNCTION(tmStart, 0)
  ADD_JS_FUNCTION(tmNow, 0)
  ADD_JS_FUNCTION(tmWallclock, 0)
  ADD_JS_FUNCTION(pi, 0)

  //
  // Molecule methods
  //

  ADD_JS_FUNCTION(moleculeToString, 1)
  ADD_JS_FUNCTION(moleculeDupl, 1)
  ADD_JS_FUNCTION(moleculeGetAtoms, 1)
  ADD_JS_FUNCTION(moleculeAppendAminoAcid, 2)
  ADD_JS_FUNCTION(moleculeFindAaCterm, 1)
  ADD_JS_FUNCTION(moleculeFindAaNterm, 1)
  ADD_JS_FUNCTION(moleculeFindAaLast, 1)

  //
  // Atom methods
  //

  ADD_JS_FUNCTION(atomToString, 1)
  ADD_JS_FUNCTION(atomGetElement, 1)
  ADD_JS_FUNCTION(atomGetPos, 1)
  ADD_JS_FUNCTION(atomGetNumBonds, 1)

  //
  // Read/Write functions
  //
#if defined(USE_DSRPDB)
  ADD_JS_FUNCTION(readPdbFile, 1)
#endif
  ADD_JS_FUNCTION(readXyzFile, 1)
  ADD_JS_FUNCTION(writeXyzFile, 2)

  //
  // Calc engines and calculations
  //

  ADD_JS_FUNCTION(getErkaleCalcEngine, 0)
  ADD_JS_FUNCTION(calcMoleculeEnergy, 2)
  ADD_JS_FUNCTION(calcMoleculeOptimize, 2)

  //
  // vector and matrix methods
  //

  ADD_JS_FUNCTION(vecPlus, 2)
  ADD_JS_FUNCTION(vecMinus, 2)
  //ADD_JS_FUNCTION(matPlus, 2)
  //ADD_JS_FUNCTION(matMinus, 2)
  ADD_JS_FUNCTION(mulMatVec, 2)
  ADD_JS_FUNCTION(mulMatMat, 2)
  ADD_JS_FUNCTION(dotVecVec, 2)
  ADD_JS_FUNCTION(crossVecVec, 2)
  ADD_JS_FUNCTION(matRotate, 1)


#undef ADD_JS_FUNCTION
}

}; // JsBinding

