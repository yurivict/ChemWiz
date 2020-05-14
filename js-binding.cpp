
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/socket.h> // for SocketApi
#include <sys/select.h> // for SocketApi
#include <sys/un.h>     // for SocketApi
#include <netinet/in.h> // for SocketApi
#include <errno.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <cmath>
#include <memory>
#include <algorithm>
#include <limits>

#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>

#include <mujs.h>
#include <rang.hpp>

#include "js-binding.h"
#include "js-support.h"
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

// TODO implement some functions from node interface: https://nodejs.org/api/process.html#process_process_stdout

// tag strings of all objects
static const char *TAG_Obj         = "Obj";
static const char *TAG_Molecule    = "Molecule";
static const char *TAG_Atom        = "Atom";
static const char *TAG_TempFile    = "TempFile";
static const char *TAG_StructureDb = "StructureDb";

extern const char *TAG_Binary;

// helper macros
#define DbgPrintStackLevel(loc)  std::cout << "DBG JS Stack: @" << loc << " level=" << js_gettop(J) << std::endl
#define DbgPrintStackObject(loc, idx) std::cout << "DBG JS Stack: @" << loc << " stack[" << idx << "]=" << js_tostring(J, idx) << std::endl
#define DbgPrintStack(loc)       std::cout << "DBG JS Stack: @" << loc << ">>>" << std::endl; \
                                 for (int i = 1; i < js_gettop(J); i++) \
                                   std::cout << "  -- @idx=" << -i << ": " << js_tostring(J, -i)  << std::endl; \
                                 std::cout << "DBG JS Stack: @" << loc << "<<<" << std::endl; \
                                 abort(); // because DbgPrintStack damages stack by converting all values to strings
//#define GetArgSSMap(n)           objToMap(J, n)
#define GetArgUnsignedArray(n)      objToTypedArray<unsigned,false>(J, n, __func__)
#define GetArgUnsignedArrayZ(n)     objToTypedArray<unsigned,true>(J, n, __func__)
#define GetArgFloatArray(n)         objToTypedArray<double, false>(J, n, __func__)
#define GetArgFloatArrayZ(n)        objToTypedArray<double, true>(J, n, __func__)
#define GetArgStringArray(n)        objToTypedArray<std::string,false>(J, n, __func__)
#define GetArgStringArrayZ(n)       objToTypedArray<std::string,true>(J, n, __func__)
#define GetArgUnsignedArrayArray(n) objToTypedArray<std::vector<unsigned>>(J, n, __func__)
#define GetArgFloatArrayArray(n)    objToTypedArray<std::vector<double>>(J, n, __func__)
#define GetArgStringArrayArray(n)   objToTypedArray<std::vector<std::string>>(J, n, __func__)
#define GetArgMatNxX(n,N)        objToMatNxX<N>(J, n)
#define GetArgElement(n)         Element(PeriodicTableData::get().elementFromSymbol(GetArgString(n)))
#define GetArgPtr(n)             StrPtr::s2p(GetArgString(n))

//
// local template specializations
//
inline void Push(js_State *J, const struct stat &s) {
  Push(J, std::map<std::string, uint64_t>{
    {"dev",s.st_dev}, {"ino", s.st_ino}, {"mode", s.st_mode}, {"nlink", s.st_nlink}, {"uid", s.st_uid}, {"gid", s.st_gid} // TODO the rest
  });
}

namespace JsBinding {

// externally defined types
namespace JsBinary {
  extern void init(js_State *J);
  extern void xnewo(js_State *J, Binary *b);
}
namespace JsImage {
  extern void init(js_State *J);
}
namespace JsImageDrawer {
  extern void init(js_State *J);
}
namespace JsFloatArray {
  extern void initFloat4(js_State *J);
  extern void initFloat8(js_State *J);
}
namespace JsLinearAlgebra {
  extern void init(js_State *J);
}
namespace JsNeuralNetwork {
  extern void init(js_State *J);
}


//
// helper classes and functions
//

static void moleculeFinalize(js_State *J, void *p) {
  delete (Molecule*)p;
}

static void atomFinalize(js_State *J, void *p) {
  auto a = (Atom*)p;
  // delete only unattached atoms, otherwise they are deteled by their Molecule object
  if (!a->molecule)
    delete a;
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
/*
static std::map<std::string,std::string> objToMap(js_State *J, int idx) {
  std::map<std::string,std::string> mres;
  if (!js_isobject(J, idx))
    js_typeerror(J, "objToMap: not an object");
  js_pushiterator(J, idx, 1/ *own* /);
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
*/

Vec3 objToVec3(js_State *J, int idx, const char *fname) {
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

Mat3 objToMat3x3(js_State *J, int idx, const char *fname) {
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

  for (int i = 0, vaIdx = 0; i < len; i++, vaIdx += N) {
    js_getindex(J, idx, i);
    objToVecVa<N>(J, -1, m, vaIdx);
    js_pop(J, 1);
  }

  return m;
}

//
// Define object types
//

namespace JsObj {

static void xnewo(js_State *J, Obj *o) {
  js_getglobal(J, TAG_Obj);
  js_getproperty(J, -1, "prototype");
  js_newuserdata(J, TAG_Obj, o, 0/*finalize*/);
}

static void init(js_State *J) {
  JsSupport::beginDefineClass(J, TAG_Obj, [](js_State *J) {
    AssertNargs(0)
    ReturnObj(new Obj);
  });
  { // methods
    ADD_METHOD_CPP(Obj, id, {
      AssertNargs(0)
      Return(J, GetArg(Obj, 0)->id());
    }, 0)
  }
  JsSupport::endDefineClass(J);
}

} // JsObj

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

static void init(js_State *J) {
  JsSupport::beginDefineClass(J, TAG_Atom, [](js_State *J) {
    AssertNargs(2)
    ReturnObj(new Atom(Element(PeriodicTableData::get().elementFromSymbol(GetArgString(1))), GetArgVec3(2)));
  });
  { // methods
    ADD_METHOD_CPP(Atom, id, {
      AssertNargs(0)
      Return(J, GetArg(Atom, 0)->id());
    }, 0)
    ADD_METHOD_CPP(Atom, dupl, {
      AssertNargs(0)
      ReturnObj(new Atom(*GetArg(Atom, 0)));
    }, 0)
    ADD_METHOD_CPP(Atom, str, {
      AssertNargs(0)
      auto a = GetArg(Atom, 0);
      Return(J, str(boost::format("atom{%1% elt=%2% pos=%3%}") % a % a->elt % a->pos));
    }, 0)
    ADD_METHOD_CPP(Atom, isEqual, {
      AssertNargs(1)
      Return(J, GetArg(Atom, 0)->isEqual(*GetArg(Atom, 1)));
    }, 1)
    ADD_METHOD_CPP(Atom, getElement, {
      AssertNargs(0)
      Return(J, str(boost::format("%1%") % GetArg(Atom, 0)->elt));
    }, 0)
    ADD_METHOD_CPP(Atom, getPos, {
      AssertNargs(0)
      Return(J, GetArg(Atom, 0)->pos);
    }, 0)
    ADD_METHOD_CPP(Atom, setPos, {
      AssertNargs(1)
      GetArg(Atom, 0)->pos = GetArgVec3(1);
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPP(Atom, getName, {
      AssertNargs(0)
      Return(J, GetArg(Atom, 0)->name);
    }, 0)
    ADD_METHOD_CPP(Atom, setName, {
      AssertNargs(1)
      GetArg(Atom, 0)->name = GetArgString(1);
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPP(Atom, getHetAtm, {
      AssertNargs(0)
      Return(J, GetArg(Atom, 0)->isHetAtm);
    }, 0)
    ADD_METHOD_CPP(Atom, setHetAtm, {
      AssertNargs(1)
      GetArg(Atom, 0)->isHetAtm = GetArgBoolean(1);
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPP(Atom, getNumBonds, {
      AssertNargs(0)
      auto a = GetArg(Atom, 0);
      Return(J, a->nbonds());
    }, 0)
    ADD_METHOD_CPP(Atom, getBonds, {
      AssertNargs(0)
      auto a = GetArg(Atom, 0);
      returnArrayOfUserData(J, a->bonds, TAG_Atom, atomFinalize, JsAtom::xnewo);
    }, 0)
    ADD_METHOD_CPP(Atom, hasBond, {
      AssertNargs(1)
      Return(J, GetArg(Atom, 0)->hasBond(GetArg(Atom, 1)));
    }, 1)
    ADD_METHOD_CPP(Atom, getOtherBondOf3, {
      AssertNargs(2)
      ReturnObj(GetArg(Atom, 0)->getOtherBondOf3(GetArg(Atom,1), GetArg(Atom,2)));
    }, 2)
    ADD_METHOD_CPP(Atom, bondsAsString, {
      AssertNargs(0)
      Return(J, GetArg(Atom, 0)->bondsAsString());
    }, 0)
    ADD_METHOD_CPP(Atom, findSingleNeighbor, {
      AssertNargs(1)
      ReturnObjZ(GetArg(Atom, 0)->findSingleNeighbor(GetArgElement(1)));
    }, 1)
    ADD_METHOD_CPP(Atom, findSingleNeighbor2, {
      AssertNargs(2)
      ReturnObjZ(GetArg(Atom, 0)->findSingleNeighbor2(GetArgElement(1), GetArgElement(2)));
    }, 2)
    ADD_METHOD_CPP(Atom, snapToGrid, {
      AssertNargs(1)
      GetArg(Atom, 0)->snapToGrid(GetArgVec3(1));
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPP(Atom, getChain, {
      AssertNargs(0)
      Return(J, GetArg(Atom, 0)->chain);
    }, 0)
    ADD_METHOD_CPP(Atom, getGroup, {
      AssertNargs(0)
      Return(J, GetArg(Atom, 0)->group);
    }, 0)
    ADD_METHOD_CPP(Atom, getSecStruct, {
      AssertNargs(0)
      Return(J, GetArg(Atom, 0)->secStructKind);
    }, 0)
    ADD_METHOD_CPP(Atom, getSecStructStr, {
      AssertNargs(0)
      std::ostringstream ss;
      ss << GetArg(Atom, 0)->secStructKind;
      Return(J, ss.str());
    }, 0)
    ADD_METHOD_JS (Atom, findBonds, function(filter) {return this.getBonds().filter(filter)})
    ADD_METHOD_JS (Atom, angleBetweenRad, function(a1, a2) {var p = this.getPos(); return Vec3.angleRad(Vec3.minus(a1.getPos(),p), Vec3.minus(a2.getPos(),p))})
    ADD_METHOD_JS (Atom, angleBetweenDeg, function(a1, a2) {var p = this.getPos(); return Vec3.angleDeg(Vec3.minus(a1.getPos(),p), Vec3.minus(a2.getPos(),p))})
    ADD_METHOD_JS (Atom, distance, function(othr) {return Vec3.length(Vec3.minus(this.getPos(), othr.getPos()))})
  }
  JsSupport::endDefineClass(J);
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
    for (int i = 0; i < len; i++) {
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

static void init(js_State *J) {
  JsSupport::beginDefineClass(J, TAG_Molecule, [](js_State *J) {
    AssertNargs(0)
    ReturnObj(new Molecule("created-in-script"));
  });
  { // methods
    ADD_METHOD_CPP(Molecule, id, {
      AssertNargs(0)
      Return(J, GetArg(Molecule, 0)->id());
    }, 0)
    ADD_METHOD_CPP(Molecule, dupl, {
      AssertNargs(0)
      ReturnObj(new Molecule(*GetArg(Molecule, 0)));
    }, 0)
    ADD_METHOD_CPP(Molecule, str, {
      AssertNargs(0)
      auto m = GetArg(Molecule, 0);
      Return(J, str(boost::format("molecule{%1%, id=%2%}") % m % m->idx.c_str()));
    }, 0)
    ADD_METHOD_CPP(Molecule, numAtoms, {
      AssertNargs(0)
      Return(J, GetArg(Molecule, 0)->getNumAtoms());
    }, 0)
    ADD_METHOD_CPP(Molecule, getAtom, {
      AssertNargs(1)
      ReturnObjExt(Atom, GetArg(Molecule, 0)->getAtom(GetArgUInt32(1)));
    }, 1)
    ADD_METHOD_CPP(Molecule, getAtoms, {
      AssertNargs(0)
      auto m = GetArg(Molecule, 0);
      returnArrayOfUserData(J, m->atoms, TAG_Atom, atomFinalize, JsAtom::xnewo);
    }, 0)
    ADD_METHOD_CPP(Molecule, addAtom, {
      AssertNargs(1)
      GetArg(Molecule, 0)->add(GetArg(Atom, 1));
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPP(Molecule, numChains, {
      AssertNargs(0)
      Return(J, GetArg(Molecule, 0)->numChains());
    }, 0)
    ADD_METHOD_CPP(Molecule, numGroups, {
      AssertNargs(0)
      Return(J, GetArg(Molecule, 0)->numGroups());
    }, 0)
    ADD_METHOD_JS (Molecule, findAtoms, function(filter) {return this.getAtoms().filter(filter)})
    ADD_METHOD_JS (Molecule, transform, function(rot,shift) {
      for (var i = 0; i < this.numAtoms(); i++) {
        var a = this.getAtom(i);
        a.setPos(Vec3.plus(Mat3.mulv(rot, a.getPos()), shift));
      }
    })
    ADD_METHOD_CPP(Molecule, addMolecule, {
      AssertNargs(1)
      GetArg(Molecule, 0)->add(*GetArg(Molecule, 1));
    }, 1)
    ADD_METHOD_CPP(Molecule, minDistBetweenAtoms, {
      AssertNargs(0)
      auto &atoms = GetArg(Molecule, 0)->atoms;
      double dist = std::numeric_limits<double>::max();
      for (unsigned a1 = 0, ae = atoms.size(); a1<ae; a1++) {
        auto atom1 = atoms[a1];
        for (unsigned a2 = a1+1; a2<ae; a2++) {
	  double d = (atoms[a2]->pos - atom1->pos).len();
          if (d < dist)
	    dist = d;
        }
      }
      Return(J, dist);
    }, 0)
    ADD_METHOD_CPP(Molecule, maxDistBetweenAtoms, {
      AssertNargs(0)
      auto &atoms = GetArg(Molecule, 0)->atoms;
      double dist = 0;
      for (unsigned a1 = 0, ae = atoms.size(); a1<ae; a1++) {
        auto atom1 = atoms[a1];
        for (unsigned a2 = a1+1; a2<ae; a2++) {
	  double d = (atoms[a2]->pos - atom1->pos).len();
          if (d > dist)
	    dist = d;
        }
      }
      Return(J, dist);
    }, 0)
    ADD_METHOD_CPP(Molecule, appendAminoAcid, {
      AssertNargs(2)
      Molecule aa(*GetArg(Molecule, 1)); // copy because it will be altered
      GetArg(Molecule, 0)->appendAsAminoAcidChain(aa, GetArgFloatArrayZ(2));
    }, 2)
    ADD_METHOD_CPP(Molecule, readAminoAcidAnglesFromAaChain, {
      AssertNargs(1)
      // GetArg(Molecule, 0); // semi-static: "this" argument isn't used

      // binary is disabled:
      // std::unique_ptr<std::vector<Molecule::AaBackbone>> aaBackboneArray(Util::createContainerFromBufferOfDifferentType<Binary, std::vector<Molecule::AaBackbone>>(*GetArg(Binary, 1)));

      // read the AaBackbone array argument
      std::unique_ptr<std::vector<Molecule::AaBackbone>> aaBackboneArray(helpers::readAaBackboneArray(J, 1/*argno*/));

      // return the angles array
      Return(J, Molecule::readAminoAcidAnglesFromAaChain(*aaBackboneArray));
    }, 1)
    ADD_METHOD_CPP(Molecule, setAminoAcidAnglesInAaChain, {
      AssertNargs(3)
      // GetArg(Molecule, 0); // semi-static: "this" argument isn't used

      // read the AaBackbone array argument
      std::unique_ptr<std::vector<Molecule::AaBackbone>> aaBackboneArray(helpers::readAaBackboneArray(J, 1/*argno*/));
      // setAminoAcidAnglesInAaChain
      Molecule::setAminoAcidAnglesInAaChain(*aaBackboneArray, GetArgUnsignedArray(2), GetArgFloatArrayArray(3));

      ReturnVoid(J);
    }, 3)
    ADD_METHOD_CPP(Molecule, detectBonds, {
      AssertNargs(0)
      GetArg(Molecule, 0)->detectBonds();
      ReturnVoid(J);
    }, 0)
    ADD_METHOD_CPP(Molecule, isEqual, {
      AssertNargs(1)
      Return(J, GetArg(Molecule, 0)->isEqual(*GetArg(Molecule, 1)));
    }, 1)
    ADD_METHOD_CPP(Molecule, toXyz, {
      AssertNargs(0)
      std::ostringstream ss;
      ss << *GetArg(Molecule, 0);
      Return(J, ss.str());
    }, 0)
    ADD_METHOD_CPP(Molecule, toXyzCoords, {
      AssertNargs(0)
      std::ostringstream ss;
      GetArg(Molecule, 0)->prnCoords(ss);
      Return(J, ss.str());
    }, 0)
    ADD_METHOD_CPP(Molecule, findComponents, {
      AssertNargs(0)
      returnArrayOfArrayOfUserData(J, GetArg(Molecule, 0)->findComponents(), TAG_Atom, atomFinalize, JsAtom::xnewo);
    }, 0)
    ADD_METHOD_CPP(Molecule, computeConvexHullFacets, {
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
    }, 1)
    ADD_METHOD_CPP(Molecule, centerOfMass, {
      AssertNargs(0)
      Return(J, GetArg(Molecule, 0)->centerOfMass());
    }, 0)
    ADD_METHOD_CPP(Molecule, centerAt, {
      AssertNargs(1)
      GetArg(Molecule, 0)->centerAt(GetArgVec3(1));
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPP(Molecule, scale, {
      AssertNargs(1)
      GetArg(Molecule, 0)->scale(GetArgFloat(1));
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPP(Molecule, snapToGrid, {
      AssertNargs(1)
      GetArg(Molecule, 0)->snapToGrid(GetArgVec3(1));
      ReturnVoid(J);
    }, 1)
    ADD_METHOD_CPP(Molecule, findAaBackbone1, {
      AssertNargs(0)
      helpers::pushAaBackboneAsJsObjectZ(J, GetArg(Molecule, 0)->findAaBackbone1());
    }, 0)
    ADD_METHOD_CPP(Molecule, findAaBackboneFirst, {
      AssertNargs(0)
      helpers::pushAaBackboneAsJsObjectZ(J, GetArg(Molecule, 0)->findAaBackboneFirst());
    }, 0)
    ADD_METHOD_CPP(Molecule, findAaBackboneLast, {
      AssertNargs(0)
      helpers::pushAaBackboneAsJsObjectZ(J, GetArg(Molecule, 0)->findAaBackboneLast());
    }, 0)
    ADD_METHOD_CPP(Molecule, findAaBackbones, {
      AssertNargs(0)
      // return array
      js_newarray(J);
      unsigned idx = 0;
      for (auto &aaBackbone : GetArg(Molecule, 0)->findAaBackbones()) {
        helpers::pushAaBackboneAsJsObject(J, aaBackbone);
        js_setindex(J, -2, idx++);
      }
    }, 0)
    ADD_METHOD_CPP(Molecule, getAminoAcidSingleAngle, {
      AssertNargs(3)
      Return(J, GetArg(Molecule, 0)->getAminoAcidSingleAngle(
        *std::unique_ptr<std::vector<Molecule::AaBackbone>>(helpers::readAaBackboneArray(J, 1/*argno*/)), // aaBackbones
        GetArgUInt32(2), // junctionIdx
        (Molecule::AaAngles::Type)GetArgUInt32(3))); // angleId
    }, 3)
    ADD_METHOD_CPP(Molecule, getAminoAcidSingleJunctionAngles, {
      AssertNargs(2)
      Return(J, GetArg(Molecule, 0)->getAminoAcidSingleJunctionAngles(
        *std::unique_ptr<std::vector<Molecule::AaBackbone>>(helpers::readAaBackboneArray(J, 1/*argno*/)), // aaBackbones
        GetArgUInt32(2))); // junctionIdx
    }, 2)
    ADD_METHOD_CPP(Molecule, getAminoAcidSingleJunctionAnglesM, {
      AssertNargs(2)
      Return(J, GetArg(Molecule, 0)->getAminoAcidSingleJunctionAnglesM(
        *std::unique_ptr<std::vector<Molecule::AaBackbone>>(helpers::readAaBackboneArray(J, 1/*argno*/)), // aaBackbones
        GetArgUInt32(2))); // junctionIdx
    }, 2)
    ADD_METHOD_CPP(Molecule, getAminoAcidSequenceAngles, {
      AssertNargs(2)
      Return(J, GetArg(Molecule, 0)->getAminoAcidSequenceAngles(
        *std::unique_ptr<std::vector<Molecule::AaBackbone>>(helpers::readAaBackboneArray(J, 1/*argno*/)), // aaBackbones
        GetArgUnsignedArray(2))); // junctionIdxs
    }, 2)
    ADD_METHOD_CPP(Molecule, getAminoAcidSequenceAnglesM, {
      AssertNargs(2)
      Return(J, GetArg(Molecule, 0)->getAminoAcidSequenceAnglesM(
        *std::unique_ptr<std::vector<Molecule::AaBackbone>>(helpers::readAaBackboneArray(J, 1/*argno*/)), // aaBackbones
        GetArgUnsignedArray(2))); // junctionIdxs
    }, 2)
    ADD_METHOD_CPP(Molecule, setAminoAcidSingleAngle, {
      AssertNargs(4)
      Return(J, GetArg(Molecule, 0)->setAminoAcidSingleAngle(
        *std::unique_ptr<std::vector<Molecule::AaBackbone>>(helpers::readAaBackboneArray(J, 1/*argno*/)), // aaBackbones
        GetArgUInt32(2), // junctionIdx
        (Molecule::AaAngles::Type)GetArgUInt32(3), // angleId
        GetArgFloat(4))); // newAngle
    }, 4)
    ADD_METHOD_CPP(Molecule, setAminoAcidSingleJunctionAngles, {
      AssertNargs(3)
      GetArg(Molecule, 0)->setAminoAcidSingleJunctionAngles(
        *std::unique_ptr<std::vector<Molecule::AaBackbone>>(helpers::readAaBackboneArray(J, 1/*argno*/)), // aaBackbones
        GetArgUInt32(2), // junctionIdx
        GetArgFloatArray(3)); // newAngles
    }, 3)
    ADD_METHOD_CPP(Molecule, setAminoAcidSequenceAngles, {
      AssertNargs(3)
      GetArg(Molecule, 0)->setAminoAcidSequenceAngles(
        *std::unique_ptr<std::vector<Molecule::AaBackbone>>(helpers::readAaBackboneArray(J, 1/*argno*/)), // aaBackbones
        GetArgUnsignedArray(2), // junctionIdxs
        GetArgFloatArrayArray(3)); // newAngles
    }, 3)
    //ADD_METHOD_CPP(Molecule, findAaBackbonesBin, {
    //  AssertNargs(0)
    //  auto aaBackbonesBinary = Util::createContainerFromBufferOfDifferentType<std::vector<Molecule::AaBackbone>, Binary>(GetArg(Molecule, 0)->findAaBackbones());
    //  Return(Binary, aaBackbonesBinary);
    //}, 0)
    ADD_METHOD_JS(Molecule, extractCoords, function(m) {
      var res = [];
      var atoms = this.getAtoms();
      for (var i = 0; i < atoms.length; i++)
        res.push(atoms[i].getPos());
      return res;
    });
    ADD_METHOD_JS(Molecule, rmsd, function(othr) {return Vec3.rmsd(this.extractCoords(), othr.extractCoords())})
    ADD_METHOD_JS(Molecule, allElements, function() {
      var mm = {};
      var atoms = this.getAtoms();
      for (var i = 0; i < atoms.length; i++)
        mm[atoms[i].getElement()] = 1;
      return Object.keys(mm);
    })
  }
  JsSupport::endDefineClass(J);
}

// static functions in the Molecule namespace

static void fromXyzOne(js_State *J) {
  AssertNargs(1)
  ReturnObj(Molecule::readXyzFileOne(GetArgString(1)));
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
  ReturnObj(Molecule::createFromSMILES(GetArgString(1), GetArgString(2)/*opt*/));
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
  js_newuserdata(J, TAG_TempFile, f, [](js_State *J, void *p) {
    delete (TempFile*)p;
  });
}

static void init(js_State *J) {
  JsSupport::beginDefineClass(J, TAG_TempFile, [](js_State *J) {
    AssertNargsRange(0,2)
    switch (GetNArgs()) {
    case 0: // ()
      ReturnObj(new TempFile("", ""));
      break;
    case 1: // (fileName)
      ReturnObj(new TempFile(GetArgString(1), ""));
      break;
    case 2: // (fileName, text content)
      if (::strcmp(js_typeof(J, 2), "string") == 0)
        ReturnObj(new TempFile(GetArgString(1), GetArgString(2)));
      else
        ReturnObj(new TempFile(GetArgString(1), GetArg(Binary, 2)));
      break;
    }
  });
  { // methods
    ADD_METHOD_CPP(TempFile, str, {
      AssertNargs(0)
      auto f = GetArg(TempFile, 0);
      Return(J, str(boost::format("temp-file{%1%}") % f->getFname()));
    }, 0)
    ADD_METHOD_CPP(TempFile, fname, {
      AssertNargs(0)
      Return(J, GetArg(TempFile, 0)->getFname());
    }, 0)
    ADD_METHOD_CPP(TempFile, toBinary, {
      AssertNargs(0)
      ReturnObjExt(Binary, GetArg(TempFile, 0)->toBinary());
    }, 0)
    ADD_METHOD_CPP(TempFile, toPermanent, {
      AssertNargs(1)
      GetArg(TempFile, 0)->toPermanent(GetArgString(1));
      ReturnVoid(J);
    }, 1)
    //ADD_METHOD_CPP(TempFile, writeBinary, 1) // to write binary data into the temp file
  }
  JsSupport::endDefineClass(J);
}

} // JsTempFile

namespace JsStructureDb {

static void xnewo(js_State *J, StructureDb *f) {
  js_getglobal(J, TAG_StructureDb);
  js_getproperty(J, -1, "prototype");
  js_newuserdata(J, TAG_StructureDb, f, [](js_State *J, void *p) {
    delete (StructureDb*)p;
  });
}

static void init(js_State *J) {
  JsSupport::beginDefineClass(J, TAG_StructureDb, [](js_State *J) {
    AssertNargs(0)
    ReturnObj(new StructureDb);
  });
  { // methods
    ADD_METHOD_CPP(StructureDb, str, {
      AssertNargs(0)
      auto sdb = GetArg(StructureDb, 0);
      Return(J, str(boost::format("structure-db{size=%1%}") % sdb->size()));
    }, 0)
    ADD_METHOD_CPP(StructureDb, toString, {
      AssertNargs(0)
      auto sdb = GetArg(StructureDb, 0);
      Return(J, str(boost::format("structure-db{size=%1%}") % sdb->size()));
    }, 0)
    ADD_METHOD_CPP(StructureDb, add, {
      AssertNargs(2)
      GetArg(StructureDb, 0)->add(GetArg(Molecule, 1), GetArgString(2));
      ReturnVoid(J);
    }, 2)
    ADD_METHOD_CPP(StructureDb, find, {
      AssertNargs(1)
      Return(J, GetArg(StructureDb, 0)->find(GetArg(Molecule, 1)));
    }, 1)
    ADD_METHOD_CPP(StructureDb, moleculeSignature, {
      AssertNargs(1)
      /*XXX StructureDb::moleculeSignature is static, and "this" argument isn't used*/
      auto signature = StructureDb::computeMoleculeSignature(GetArg(Molecule, 1));
      std::ostringstream ss;
      ss << signature;
      Return(J, ss.str());
    }, 1)
  }
  JsSupport::endDefineClass(J);
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
  } catch (std::ifstream::failure e) { // gcc warning: catching polymorphic type 'class std::ios_base::failure' by value
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
  } catch (std::ifstream::failure e) { // gcc warning: catching polymorphic type 'class std::ios_base::failure' by value
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

static void formatFp(js_State *J) {
  AssertNargs(2)
  char buf[32];
  ::sprintf(buf, "%.*lf", GetArgUInt32(2), GetArgFloat(1));
  Return(J, std::string(buf));
}

static void download(js_State *J) {
  AssertNargs(3)
  ReturnObjExt(Binary, WebIo::download(GetArgString(1), GetArgString(2), GetArgString(3)));
}

static void downloadUrl(js_State *J) {
  AssertNargs(1)
  ReturnObjExt(Binary, WebIo::downloadUrl(GetArgString(1)));
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

  ReturnObjExt(Binary, b);
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
    js_newarray(J);
    unsigned idx = 0;
    for (auto m : Molecule::readMmtfFile(GetArgString(1))) {
      JsMolecule::xnewo(J, m);
      js_setindex(J, -2, idx++);
    }
  }

  static void readBuffer(js_State *J) {
    AssertNargs(1)
    js_newarray(J);
    unsigned idx = 0;
    for (auto m : Molecule::readMmtfBuffer(GetArg(Binary, 1))) {
      JsMolecule::xnewo(J, m);
      js_setindex(J, -2, idx++);
    }
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

static void divn(js_State *J) {
  AssertNargs(2)
  Return(J, GetArgVec3(1)/GetArgFloat(2));
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

static void divn(js_State *J) {
  AssertNargs(2)
  Return(J, GetArgMat3x3(1)/GetArgFloat(2));
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
  JsAtom::init(J);
  JsMolecule::init(J);
  JsTempFile::init(J);
  JsStructureDb::init(J);
  // externally defined
  JsBinary::init(J);
  JsImage::init(J);
  JsImageDrawer::init(J);
  JsFloatArray::initFloat4(J);
  JsFloatArray::initFloat8(J);
  JsLinearAlgebra::init(J);
  JsNeuralNetwork::init(J);

  //
  // Misc
  //

  ADD_JS_FUNCTION(print, 1)
  ADD_JS_FUNCTION(printn, 1)
  ADD_JS_FUNCTION(printa, 2)
  ADD_JS_FUNCTION(printna, 2)
  ADD_JS_FUNCTION(flush, 0)
  ADD_JS_FUNCTIONinline(getenv, {
    AssertNargs(1)
    if (auto v = ::getenv(GetArgString(1).c_str()))
      Return(J, v);
    else
      ReturnNull(J);
  }, 0)
  BEGIN_NAMESPACE(System)
    ADD_NS_FUNCTION_CPP(System, numCPUs,            JsSystem::numCPUs, 0)
    ADD_NS_FUNCTION_CPP(System, setCtlParam,        JsSystem::setCtlParam, 2)
    ADD_NS_FUNCTION_CPPnew(System, errno, {
      AssertNargs(0)
      Return(J, errno);
    }, 0)
    ADD_NS_FUNCTION_CPPnew(System, strerror, {
      AssertNargs(1)
      Return(J, strerror(GetArgInt32(1)));
    }, 1)
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
    ADD_NS_FUNCTION_CPPnew(Time, start, {
      AssertNargs(0)
      js_pushnumber(J, Tm::start());
    }, 0)
    ADD_NS_FUNCTION_CPPnew(Time, now, {
      AssertNargs(0)
      js_pushnumber(J, Tm::now());
    }, 0)
    ADD_NS_FUNCTION_CPPnew(Time, wallclock, {
      AssertNargs(0)
      js_pushnumber(J, Tm::wallclock());
    }, 0)
    ADD_NS_FUNCTION_CPPnew(Time, currentDateTimeToSec, { // format is YYYY-MM-DD.HH:mm:ss
      AssertNargs(0)
      time_t     now = time(0);
      struct tm  tstruct;
      char       buf[80];
      tstruct = *localtime(&now);
      strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);
      Return(J, std::string(buf));
    }, 0)
    ADD_NS_FUNCTION_CPPnew(Time, currentDateTimeToMs, { // format is YYYY-MM-DD.HH:mm:ss
      AssertNargs(0)

      timeval curTime;
      gettimeofday(&curTime, NULL);

      struct tm  tstruct;
      char       bufs[64];
      char       bufms[64];

      tstruct = *localtime(&curTime.tv_sec);
      strftime(bufs, sizeof(bufs), "%Y-%m-%d.%X", &tstruct);
      sprintf(bufms, "%s.%03ld", bufs, curTime.tv_usec/1000);
      Return(J, std::string(bufms));
    }, 0)
    ADD_NS_FUNCTION_JS (Time, timeTheCode, function(name, func) {
      var t1 = Time.now();
      func();
      var t2 = Time.now();
      print("-- tm: step "+name+" took "+(t2-t1)+" sec(s) (wallclock) --");
    })
  END_NAMESPACE(Time)
  BEGIN_NAMESPACE(LegacyRng)
    ADD_NS_FUNCTION_CPPnew(LegacyRng, srand, {
      AssertNargs(1)
      ::srand(GetArgUInt32(1));
      ReturnVoid(J);
    }, 1)
    ADD_NS_FUNCTION_CPPnew(LegacyRng, rand, {
      AssertNargs(0)
      Return(J, ::rand());
    }, 0)
  END_NAMESPACE(LegacyRng)
  BEGIN_NAMESPACE(Process)
    ADD_NS_FUNCTION_CPPnew(Process, system, {
      AssertNargs(1)
      Return(J, ::system(GetArgString(1).c_str()));
    }, 1)
    ADD_NS_FUNCTION_CPPnew(Process, runCaptureOutput, {
      AssertNargs(1)
      try {
        Return(J, Process::exec(GetArgString(1)));
      } catch (std::runtime_error e) {
        JS_ERROR(e.what());
      }
    }, 1)
  END_NAMESPACE(Process)
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
    ADD_NS_FUNCTION_CPP(Vec3, divn,         JsVec3::divn, 2)
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
    ADD_NS_FUNCTION_CPP(Mat3, divn,         JsMat3::divn, 2)
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
  BEGIN_NAMESPACE(FileApi)
    ADD_NS_FUNCTION_CPPnew(FileApi, open, {
      AssertNargs(2)
      Return(J, ::open(GetArgString(1).c_str(), GetArgUInt32(2)));
    }, 2)
    ADD_NS_FUNCTION_CPPnew(FileApi, close, {
      AssertNargs(1)
      Return(J, ::close(GetArgUInt32(1)));
    }, 1)
    ADD_NS_FUNCTION_CPPnew(FileApi, read, { // (fd, buf, off, size)
      AssertNargs(4)
      auto fd = GetArgUInt32(1);
      auto buf = GetArg(Binary, 2);
      auto off = GetArgUInt32(3);
      auto size = GetArgUInt32(4);
      auto bufSz = buf->size();
      bool resized = off+size > bufSz;
      if (resized)
        buf->resize(off+size);

      auto res = ::read(fd, &(*buf)[off], size);

      if (resized) {
        if (res >= 0) {
          if (off + res < buf->size())
            buf->resize(std::max<unsigned>(bufSz, off + res));
        } else {
          buf->resize(bufSz); // resize back because of the error
        }
      }

      Return(J, res);
    }, 4)
    ADD_NS_FUNCTION_CPPnew(FileApi, unlink, {
      AssertNargs(1)
      Return(J, ::unlink(GetArgString(1).c_str()));
    }, 1)
    ADD_NS_FUNCTION_CPPnew(FileApi, rename, {
      AssertNargs(2)
      Return(J, ::rename(GetArgString(1).c_str(), GetArgString(2).c_str()));
    }, 2)
    ADD_NS_FUNCTION_CPPnew(FileApi, stat, {
      AssertNargs(1)

      struct stat s;
      if (::stat(GetArgString(1).c_str(), &s) != -1)
        Return(J, s);
      else
        Return(J, -1);
    }, 1)
    ADD_NS_FUNCTION_CPPnew(FileApi, lstat, {
      AssertNargs(1)

      struct stat s;
      if (::lstat(GetArgString(1).c_str(), &s) != -1)
        Return(J, s);
      else
        Return(J, -1);
    }, 1)
  END_NAMESPACE(FileApi)
  JsSupport::addNsConstInt(J, "FileApi", "O_RDONLY",    O_RDONLY);
  JsSupport::addNsConstInt(J, "FileApi", "O_WRONLY",    O_WRONLY);
  JsSupport::addNsConstInt(J, "FileApi", "O_RDWR",      O_RDWR);
  JsSupport::addNsConstInt(J, "FileApi", "S_IFDIR",     O_RDWR);
  BEGIN_NAMESPACE(SocketApi)
    ADD_NS_FUNCTION_CPPnew(SocketApi, socket, {
      AssertNargs(3)
      Return(J, ::socket(GetArgInt32(1), GetArgInt32(2), GetArgInt32(3)));
    }, 3)
    ADD_NS_FUNCTION_CPPnew(SocketApi, bindUnix, { // (int sock, string unixAddr)
      AssertNargs(2)

      struct sockaddr_un addr;
      ::memset(&addr, 0, sizeof(struct sockaddr_un));
      addr.sun_family = AF_UNIX;
      ::strncpy(addr.sun_path, GetArgString(2).c_str(), sizeof(addr.sun_path) - 1);

      Return(J, ::bind(GetArgInt32(1), (struct sockaddr*)&addr, sizeof(struct sockaddr_un)));
    }, 2)
    ADD_NS_FUNCTION_CPPnew(SocketApi, bindInet, { // (int sock, unsigned port)
      AssertNargs(2)

      struct sockaddr_in addr;
      ::memset(&addr, 0, sizeof(struct sockaddr_in));
      addr.sin_family = AF_INET;
      addr.sin_port = htons(GetArgUInt32(2));
      addr.sin_addr.s_addr = INADDR_ANY;

      Return(J, ::bind(GetArgInt32(1), (struct sockaddr*)&addr, sizeof(struct sockaddr_in)));
    }, 2)
    ADD_NS_FUNCTION_CPPnew(SocketApi, accept, { // (int sock) // XXX this loses the host address
      AssertNargs(1)

      struct sockaddr addr;
      socklen_t addrlen = sizeof(addr);

      Return(J, ::accept(GetArgInt32(1), &addr, &addrlen));
    }, 1)
    ADD_NS_FUNCTION_CPPnew(SocketApi, getsockopt, { // (int socket, int level, int optname) -> [res, prevVal]
      AssertNargs(3)

      int optval = 0;
      socklen_t len = sizeof(optval);

      int res = ::getsockopt(GetArgInt32(1), GetArgInt32(2), GetArgInt32(3), &optval, &len);

      Return(J, std::array<int,2>({res, optval}));
    }, 3)
    ADD_NS_FUNCTION_CPPnew(SocketApi, setsockopt, { // (int socket, int level, int optname int newVal) -> res
      AssertNargs(4)

      int optval = GetArgInt32(4);

      Return(J, ::setsockopt(GetArgInt32(1), GetArgInt32(2), GetArgInt32(3), &optval, sizeof(optval)));
    }, 4)
    ADD_NS_FUNCTION_CPPnew(SocketApi, read, { // (int sock, Binary buff, unsigned off, unsigned nbytes)
      AssertNargs(4)
      auto fd = GetArgUInt32(1);
      auto buf = GetArg(Binary, 2);
      auto off = GetArgUInt32(3);
      auto len = GetArgUInt32(4);
      auto bufSz = buf->size();
      bool needToResize = bufSz < off+len;
      if (needToResize)
        buf->resize(off+len);

      auto res = ::read(fd, (void*)&(*buf)[off], len);

      if (needToResize) {
        if (res <= 0)
          buf->resize(bufSz);
        else if (res < len)
          buf->resize(off+res);
      }

      Return(J, res);
    }, 4)
    ADD_NS_FUNCTION_CPPnew(SocketApi, write, { // (int sock, Binary buff, unsigned off, unsigned nbytes)
      AssertNargs(4)
      Return(J, ::write(GetArgInt32(1), (void*)&(*GetArg(Binary, 2))[GetArgUInt32(3)], GetArgUInt32(4)));
    }, 4)
    ADD_NS_FUNCTION_CPPnew(SocketApi, close, {
      AssertNargs(1)
      Return(J, ::close(GetArgInt32(1)));
    }, 1)
    ADD_NS_FUNCTION_CPPnew(SocketApi, listen, {
      AssertNargs(2)
      Return(J, ::listen(GetArgInt32(1), GetArgInt32(2)));
    }, 2)
    ADD_NS_FUNCTION_CPPnew(SocketApi, select, {
      AssertNargs(4)

      int nfds = 0;
      fd_set readfds;
      fd_set writefds;
      fd_set exceptfds;

      auto lstRD = GetArgUInt32ArrayZ(1);
      auto lstWR = GetArgUInt32ArrayZ(2);
      auto lstEX = GetArgUInt32ArrayZ(3);

      auto socListToFdSet = [&nfds](const std::vector<int> &fdVec, fd_set &fdSet) {
        FD_ZERO(&fdSet);
        for (auto fd : fdVec) {
          FD_SET(fd, &fdSet);
          if (fd > nfds)
            nfds = fd;
        }
      };
      auto fdSetToSockList = [](const std::vector<int> &fdVec, fd_set &fdSet, std::vector<int> &fdVecOut) {
        for (auto fd : fdVec)
          if (FD_ISSET(fd, &fdSet))
            fdVecOut.push_back(fd);
      };
      socListToFdSet(lstRD, readfds);
      socListToFdSet(lstWR, writefds);
      socListToFdSet(lstEX, exceptfds);

      auto tmMs = GetArgInt32(4);
      struct timeval timeout;
      timeout.tv_sec = tmMs/1000;
      timeout.tv_usec = (tmMs%1000)*1000;

      auto res = ::select(nfds+1, &readfds, &writefds, &exceptfds, &timeout);

      std::vector<std::vector<int>> resVec; // return 4 lists: {errorCode}, {readfds}, {writefds}, {exceptfds}
      resVec.resize(4);
      resVec[0].push_back(res);

      if (res > 0) { // some sockets are ready
        fdSetToSockList(lstRD, readfds,   resVec[1]);
        fdSetToSockList(lstWR, writefds,  resVec[2]);
        fdSetToSockList(lstEX, exceptfds, resVec[3]);
      }

      Return(J, resVec);
    }, 4)
  END_NAMESPACE(SocketApi)
  // CAVEAT FIXME These can't be placed earlier due to some glitch with how namespaces are defined
  JsSupport::addNsConstInt(J, "SocketApi", "PF_LOCAL",    PF_LOCAL);
  JsSupport::addNsConstInt(J, "SocketApi", "PF_UNIX",     PF_UNIX);
  JsSupport::addNsConstInt(J, "SocketApi", "PF_INET",     PF_INET);
  JsSupport::addNsConstInt(J, "SocketApi", "PF_INET6",    PF_INET6);
  JsSupport::addNsConstInt(J, "SocketApi", "SOL_SOCKET",  SOL_SOCKET);
  JsSupport::addNsConstInt(J, "SocketApi", "SO_REUSEPORT",SO_REUSEPORT);
  JsSupport::addNsConstInt(J, "SocketApi", "SOCK_STREAM", SOCK_STREAM);
  JsSupport::addNsConstInt(J, "SocketApi", "SOCK_DGRAM",  SOCK_DGRAM);
  JsSupport::addNsConstInt(J, "SocketApi", "SOCK_RAW",    SOCK_RAW);
}

} // JsBinding

