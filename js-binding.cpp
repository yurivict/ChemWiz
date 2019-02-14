
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
#include "temp-file.h"
#include "tm.h"
#include "process.h"

// types defined locally
typedef std::vector<uint8_t> Binary;

static const char *TAG_Binary     = "Binary";
static const char *TAG_Molecule   = "Molecule";
static const char *TAG_Atom       = "Atom";
static const char *TAG_CalcEngine = "CalcEngine";
static const char *TAG_TempFile   = "TempFile";

#define AssertNargs(n)           assert(js_gettop(J) == n+1);
#define AssertNargsRange(n1,n2)  assert(n1+1 <= js_gettop(J) && js_gettop(J) <= n2+1);
#define AssertNargs2(nmin,nmax)  assert(nmin+1 <= js_gettop(J) && js_gettop(J) <= nmax+1);
#define AssertStack(n)           assert(js_gettop(J) == n);
#define DbgPrintStackLevel(loc)  std::cout << "DBG JS Stack: @" << loc << " level=" << js_gettop(J) << std::endl
#define GetNArgs()               (js_gettop(J)-1)
#define GetArg(type, n)          ((type*)js_touserdata(J, n, TAG_##type))
#define GetArgUInt32(n)          js_touint32(J, n)
#define GetArgString(n)          std::string(js_tostring(J, n))
#define GetArgSSMap(n)           objToMap(J, n)
#define GetArgVec(n)             objToVec(J, n)
#define GetArgMat(n)             objToMat(J, n)
#define StackPopPrevious(n)      {js_rot2(J); js_pop(J, 1);}

#define ADD_JS_METHOD(cls, method, nargs) \
  AssertStack(2); \
  js_newcfunction(J, prototype::method, #cls ".prototype." #method, nargs); /*PUSH a function object wrapping a C function pointer*/ \
  js_defproperty(J, -2, #method, JS_DONTENUM); /*POP a value from the top of the stack and set the value of the named property of the object (in prototype).*/ \
  AssertStack(2);

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

static Vec3 objToVec(js_State *J, int idx) {
  if (!js_isarray(J, idx))
    ERROR("objToVec: not an array");
  if (js_getlength(J, idx) != 3)
    ERROR("objToVec: array size isn't 3");

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
    ERROR("objToMat: not an array");
  if (js_getlength(J, idx) != 3)
    ERROR("objToMat: array size isn't 3");

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

static void ReturnVoid(js_State *J) {
  js_pushundefined(J);
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

//
// Define object types
//

namespace JsBinary {

static void xnewo(js_State *J, Binary *b) {
  js_getglobal(J, TAG_Binary);
  js_getproperty(J, -1, "prototype");
  js_newuserdata(J, TAG_Binary, b, binaryFinalize);
}

static void xnew(js_State *J) {
  AssertNargs(1)
  auto b = new Binary;
  b->resize(GetArgUInt32(1));
  xnewo(J, b);
}

namespace prototype {

static void dupl(js_State *J) {
  AssertNargs(0)
  xnewo(J, new Binary(*GetArg(Binary, 0)));
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
  xnewo(J, b);
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
    ADD_JS_METHOD(Binary, dupl, 0)
    ADD_JS_METHOD(Binary, size, 0)
    ADD_JS_METHOD(Binary, resize, 1)
    ADD_JS_METHOD(Binary, append, 1)
    ADD_JS_METHOD(Binary, concatenate, 1)
    ADD_JS_METHOD(Binary, toFile, 1)
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

static void xnew(js_State *J) {
  AssertNargs(2)
  auto elt = GetArgString(1);
  auto pos = GetArgVec(2);
  xnewo(J, new Atom(elementFromString(elt), pos));
}

namespace prototype {

static void dupl(js_State *J) {
  AssertNargs(0)
  xnewo(J, new Atom(*GetArg(Atom, 0)));
}

static void str(js_State *J) {
  AssertNargs(0)
  auto a = GetArg(Atom, 0);
  ReturnString(J, str(boost::format("atom{%1% elt=%2% pos=%3%}") % a % a->elt % a->pos));
}

static void getElement(js_State *J) {
  AssertNargs(0)
  ReturnString(J, str(boost::format("%1%") % GetArg(Atom, 0)->elt));
}

static void getPos(js_State *J) {
  AssertNargs(0)
  auto a = GetArg(Atom, 0);
  ReturnVec(J, a->pos);
}

static void setPos(js_State *J) {
  AssertNargs(1)
  GetArg(Atom, 0)->pos = GetArgVec(1);
  ReturnVoid(J);
}

static void getNumBonds(js_State *J) {
  AssertNargs(0)
  auto a = GetArg(Atom, 0);
  ReturnUnsigned(J, a->bonds.size());
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
    ADD_JS_METHOD(Atom, dupl, 0)
    ADD_JS_METHOD(Atom, str, 0)
    ADD_JS_METHOD(Atom, getElement, 0)
    ADD_JS_METHOD(Atom, getPos, 0)
    ADD_JS_METHOD(Atom, setPos, 1)
    ADD_JS_METHOD(Atom, getNumBonds, 0)
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
  xnewo(J, new Molecule("created-in-script"));
}

namespace prototype {

static void dupl(js_State *J) {
  AssertNargs(0)
  xnewo(J, new Molecule(*GetArg(Molecule, 0)));
}

static void str(js_State *J) {
  AssertNargs(0)
  auto m = GetArg(Molecule, 0);
  char str[256];
  sprintf(str, "molecule{%p, id=%s}", m, m->id.c_str());
  ReturnString(J, str);
}

static void numAtoms(js_State *J) {
  AssertNargs(0)
  auto m = (const Molecule*)js_touserdata(J, 0, TAG_Molecule);
  ReturnUnsigned(J, m->getNumAtoms());
}

static void getAtoms(js_State *J) {
  AssertNargs(0)
  auto m = GetArg(Molecule, 0);
  returnArrayUserData<std::vector<Atom*>, void(*)(js_State*,Atom*)>(J, m->atoms, TAG_Atom, atomFinalize, JsAtom::xnewo);
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

} // prototype

static void init(js_State *J) {
  InitObjectRegistry(J, TAG_Molecule);
  js_pushglobal(J);
  ADD_JS_CONSTRUCTOR(Molecule)
  js_getglobal(J, TAG_Molecule);          // PUSH Object => {-1: Molecule}
  js_getproperty(J, -1, "prototype");     // PUSH prototype => {-1: Molecule, -2: Molecule.prototype}
  StackPopPrevious()
  { // methods
    ADD_JS_METHOD(Molecule, dupl, 0)
    ADD_JS_METHOD(Molecule, str, 0)
    ADD_JS_METHOD(Molecule, numAtoms, 0)
    ADD_JS_METHOD(Molecule, getAtoms, 0)
    ADD_JS_METHOD(Molecule, appendAminoAcid, 1)
    ADD_JS_METHOD(Molecule, findAaCterm, 0)
    ADD_JS_METHOD(Molecule, findAaNterm, 0)
    ADD_JS_METHOD(Molecule, findAaLast, 0)
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
    xnewo(J, new TempFile);
    break;
  case 1: // (fileName)
    xnewo(J, new TempFile(GetArgString(1)));
    break;
  case 2: // (fileName, content)
    xnewo(J, new TempFile(GetArgString(1), GetArgString(2)));
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
  JsBinary::xnewo(J, GetArg(TempFile, 0)->toBinary());
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
    ADD_JS_METHOD(TempFile, str, 0)
    ADD_JS_METHOD(TempFile, fname, 0)
    ADD_JS_METHOD(TempFile, toBinary, 0)
    //ADD_JS_METHOD(TempFile, writeBinary, 1) // to write binary data into the temp file
  }
  js_pop(J, 2);
  AssertStack(0);
}

} // JsTempFile

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

static void pi(js_State *J) {
  AssertNargs(0)
  js_pushnumber(J, M_PI);
}

static void system(js_State *J) {
  AssertNargs(1)
  js_pushnumber(J, M_PI);
  ReturnString(J, Process::exec(GetArgString(1)));
}

#if defined(USE_DSRPDB)
static void readPdbFile(js_State *J) {
  AssertNargs(1)
  auto fname = GetArgString(1);
  auto pdbs = Molecule::readPdbFile(fname);
  for (auto pdb : pdbs)
    JsMolecule::xnewo(J, pdb);
}
#endif

static void readXyzFile(js_State *J) {
  AssertNargs(1)
  auto fname = GetArgString(1);
  auto m = Molecule::readXyzFile(fname);
  //js_newuserdata(J, TAG_Molecule, m, moleculeFinalize);
  JsMolecule::xnewo(J, m);
  //js_newcfunction(J, File_prototype_readByte, "File.prototype.readByte", 0);
  //js_defproperty(J, -2, "readByte", JS_DONTENUM);
}

static void writeXyzFile(js_State *J) {
  AssertNargs(2)
  auto m = GetArg(Molecule, 1);
  auto fname = GetArgString(2);
  std::ofstream file(fname, std::ios::out);
  file << *m;
  ReturnVoid(J);
}

//
// Calc engines
//

static void getErkaleCalcEngine(js_State *J) {
  AssertNargs(0)
  js_newuserdata(J, TAG_CalcEngine, new Calculators::engines::Erkale, calcEngineFinalize);
}

static void calcMoleculeEnergy(js_State *J) {
  AssertNargs2(2,3)
  auto m = GetArg(Molecule, 1);
  auto ce = GetArg(CalcEngine, 2);
  const Calculators::Params params = GetNArgs() == 3 ? GetArgSSMap(3) : Calculators::Params();
  ReturnFloat(J, ce->calcEnergy(*m, params));
}

static void calcMoleculeOptimize(js_State *J) {
  AssertNargs2(2,3)
  auto m = GetArg(Molecule, 1);
  auto ce = GetArg(CalcEngine, 2);
  const Calculators::Params params = GetNArgs() == 3 ? GetArgSSMap(3) : Calculators::Params();
  JsMolecule::xnewo(J, ce->calcOptimized(*m, params));
}

static void vecPlus(js_State *J) {
  AssertNargs(2)
  ReturnVec(J, GetArgVec(1)+GetArgVec(2));
}

static void vecMinus(js_State *J) {
  AssertNargs(2)
  ReturnVec(J, GetArgVec(1)-GetArgVec(2));
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
  ReturnVec(J, GetArgMat(1)*GetArgVec(2));
}

static void mulMatMat(js_State *J) {
  AssertNargs(2)
  ReturnMat(J, GetArgMat(1)*GetArgMat(2));
}

static void dotVecVec(js_State *J) {
  AssertNargs(2)
  ReturnFloat(J, GetArgVec(1)*GetArgVec(2));
}

static void crossVecVec(js_State *J) {
  AssertNargs(2)
  ReturnVec(J, GetArgVec(1).cross(GetArgVec(2)));
}

static void matRotate(js_State *J) {
  AssertNargs(1)
  ReturnMat(J, Mat3::rotate(GetArgVec(1)));
}

static const char *require_js =
  "function require(name) {\n"
  "  var cache = require.cache;\n"
  "  if (name in cache) return cache[name];\n"
  "  var exports = {};\n"
  "  cache[name] = exports;\n"
  "  Function('exports', read(name+'.js'))(exports);\n"
  "  return exports;\n"
  "}\n"
  "require.cache = Object.create(null);\n"
;

static void readfile(js_State *J) {
  const char *filename = js_tostring(J, 1);
  FILE *f;
  char *s;
  int size, t;

  f = fopen(filename, "rb");
  if (!f) {
    js_error(J, "opening file %s failed", filename);
  }
  if (fseek(f, 0, SEEK_END) < 0) {
     js_error(J, "seeking file %s failed", filename);
  }
  size = ftell(f);
  if (size < 0) {
    fclose(f);
    js_error(J, "telling file %s failed", filename);
  }
  if (fseek(f, 0, SEEK_SET) < 0) {
    fclose(f);
    js_error(J, "seeking file %s failed", filename);
  }
  s = (char *) malloc(size + 1);
  if (!s) {
    fclose(f);
    js_error(J, "out of memory");
  }
  t = fread(s, 1, size, f);
  if (t != size) {
    free(s);
    fclose(f);
    js_error(J, "reading file %s failed", filename);
  }
  fclose(f);
  s[size] = '\0';
  js_pushstring(J, s);
  free(s);
}

void registerFunctions(js_State *J) {

  //
  // require() and read() inspired from mujs
  //

  js_newcfunction(J, readfile, "read", 1);
  js_setglobal(J, "read");
  js_dostring(J, require_js);

  //
  // init types
  //

  JsBinary::init(J);
  JsAtom::init(J);
  JsMolecule::init(J);
  JsTempFile::init(J);

#define ADD_JS_FUNCTION(name, num) \
  js_newcfunction(J, JsBinding::name, #name, num); \
  js_setglobal(J, #name);

  //
  // Misc
  //

  ADD_JS_FUNCTION(print, 1)
  ADD_JS_FUNCTION(sleep, 1)
  ADD_JS_FUNCTION(tmStart, 0)
  ADD_JS_FUNCTION(tmNow, 0)
  ADD_JS_FUNCTION(tmWallclock, 0)
  ADD_JS_FUNCTION(pi, 0)
  ADD_JS_FUNCTION(system, 1)

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

