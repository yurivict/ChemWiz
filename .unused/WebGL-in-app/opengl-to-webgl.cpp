
#include <pugixml.hpp>

#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <cctype>

#include <boost/format.hpp>

#include <ctype.h>
#include <stdlib.h>

#include <rang.hpp>

#define StrEquals(a,b) (::strcmp(a,b) == 0)
#define Fail(msg...) {std::cerr << rang::fg::red << std::endl << msg << rang::style::reset << std::endl; abort();}

//
// types
//
struct TypeAndName {
  std::string type;
  std::string name;
  friend std::ostream& operator<<(std::ostream &os, const TypeAndName &tn) {
    os << tn.type << " " << tn.name;
    return os;
  }
};

typedef TypeAndName FuncDescr;

struct ArgDescr : TypeAndName {
  bool        ellipsis;
  bool        optional;  
  std::string optDeft; // the default value when optional=true
  // constr
  ArgDescr() : ellipsis(false), optional(false) { }
  ArgDescr(const TypeAndName &tn, bool newOptional = false, const std::string &newOptDeft = "")
  : TypeAndName(tn), ellipsis(false), optional(newOptional), optDeft(newOptDeft) { }
  ArgDescr(const std::string &newType, const std::string &newName, bool newOptional = false, const std::string &newOptDeft = "")
  : TypeAndName({newType, newName}), ellipsis(false), optional(newOptional), optDeft(newOptDeft) { }
  // print
  friend std::ostream& operator<<(std::ostream &os, const ArgDescr &arg) {
    if (!arg.ellipsis) {
      if (!arg.optional)
        os << (const TypeAndName&)arg;
      else
        os << "optional " << (const TypeAndName&)arg << " = " << arg.optDeft;
    } else {
      os << "...";
    }
    return os;
  }
};

struct Func {
  FuncDescr             fn;
  std::vector<ArgDescr> args;
  friend std::ostream& operator<<(std::ostream &os, const Func &f) {
    os << f.fn.type << " " << f.fn.name << "(";
    bool fst = true;
    for (auto const &a : f.args) {
      // comma
      if (fst)
        fst = false;
      else
        os << ", ";
      // arg
      os << a;
    }
    os << ")";
    return os;
  }
};

typedef std::map<std::string, std::vector<Func>> FuncMSet;

//
// helpers
//

static pugi::xml_document readXml(const char *fname) {
  std::ifstream is(fname);
  std::string src((std::istreambuf_iterator<char>(is)),
                   std::istreambuf_iterator<char>());

  pugi::xml_document xml;
  pugi::xml_parse_result result = xml.load_string(src.c_str());

  if (!result)
    Fail("Failed to parse XML")

  return xml;
}

static TypeAndName parseTypeAndName(const pugi::xml_node &node) {
  std::ostringstream osType;

  for (auto sub : node) {
    std::string txt = sub.value();
    if (!txt.empty())
      osType << txt;
    else if (StrEquals(sub.name(), "ptype"))
      osType << sub.child_value();
  }

  // trip trailing space
  auto type = osType.str();
  while (!type.empty())
    if (::isspace(type[type.size()-1]))
      type.resize(type.size()-1);
    else
      break;

  return {type, node.child_value("name")};
}

static FuncMSet parseWebGlCommands(const pugi::xml_document &xml) {
  FuncMSet wglcommands;
  auto skipSpace = [](auto &it, auto const ite) {
    while (it < ite && std::isspace(*it))
      it++;
  };
  auto getToken = [](auto &it, auto const ite) {
    auto ittb = it;
    auto itte = it;
    while (itte < ite && !std::isspace(*itte) && *itte != '(' && *itte != ')' && *itte != ',')
      itte++;
    if (ittb == itte)
      Fail("can't parse token: @" << std::string(it, ite))
    it = itte;
    return std::string(ittb, itte);
  };
  auto parseCmd = [skipSpace,getToken](const std::string &txt) {
    // [WebGLHandlesContextLoss] GLenum getError()
    // void framebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, WebGLTexture? texture, GLint level)
    auto it = txt.begin(), ite = txt.end();
    skipSpace(it, ite);
    // skip context description in brackets []
    if (*it == '[')
      it = std::find(it, ite, ']') + 1;
    skipSpace(it, ite);
    // function
    Func f;
    f.fn.type = getToken(it, ite); // function return type
    skipSpace(it, ite);
    f.fn.name = getToken(it, ite); // function name
    skipSpace(it, ite);
    if (*it != '(')
      Fail("no arguments found: can't find the opening brace")
    it++; // skip brace
    skipSpace(it, ite);
    bool fst = true;
    while (*it != ')') {
      if (fst)
        fst = false;
      else if (*it == ',') {
        it++; // skip comma
        skipSpace(it, ite);
      } else
        Fail("can't parse arguments: not a closing brace and not a comma separator")

      // ellipsis?
      if (*it == '.') {
        it++;
        if (it == ite || *it != '.')
          Fail("period not allowed in the arguments")
        it++;
        if (it == ite || *it != '.')
          Fail("period not allowed in the arguments")
        it++;
        skipSpace(it, ite);
        if (*it != ')')
          Fail("ellipsis should be followed by the closing brace")
        // add ellipsis
        ArgDescr arg;
        arg.ellipsis = true;
        f.args.push_back(arg);
        break;
      }

      // parse argtype and name
      ArgDescr arg;
      auto tok = getToken(it, ite); // the word "optional" or arg type
      skipSpace(it, ite);
      if (tok == "optional") {
        arg.optional = true;
        arg.type = getToken(it, ite);
        skipSpace(it, ite);
      } else {
        arg.type = tok;
      }
      arg.name = getToken(it, ite); // arg name
      skipSpace(it, ite);
      if (arg.optional) {
        if (*it != '=')
          Fail("no optDeft specification")
        it++;
        skipSpace(it, ite);
        arg.optDeft = getToken(it, ite);
        skipSpace(it, ite);
      }
      // add
      f.args.push_back(arg);
    }
    return f;
  };
  auto flattenLinks = [](auto &xmlPath) {
    std::ostringstream ss;
    for (auto ch : xmlPath.node().children())
      if (ch.type() == pugi::node_pcdata || (ch.type() == pugi::node_element && StrEquals(ch.name(), "a")))
        ss << " " << ch.text().get();
    return ss.str();
  };
  auto addNodes = [&xml,&wglcommands,flattenLinks,parseCmd](const char *filter) {
    for (auto nn : xml.select_nodes(filter)) {
      auto txt = flattenLinks(nn);
      if (!txt.empty()) {
        auto cmd = parseCmd(txt);
        wglcommands[cmd.fn.name].push_back(cmd);
      }
    }
  };

  // parse html/body/dl/dt
  addNodes("//html/body/dl[@class='methods']/dt[@class='idl-code']");
  // parse html/body/dl/dt/p
  addNodes("//html/body/dl[@class='methods']/dt/p[@class='idl-code']");

  return wglcommands;
}

static FuncMSet parseGlCommands(const pugi::xml_document &xml) {
  FuncMSet oglCommands;

  for (auto xmlCmd : xml.select_nodes("//registry/commands/command")) {
    Func oglCmd;

    oglCmd.fn = parseTypeAndName(xmlCmd.node().child("proto"));
    for (auto &child : xmlCmd.node().select_nodes("param"))
      oglCmd.args.push_back(parseTypeAndName(child.node()));

    oglCommands[oglCmd.fn.name].push_back(oglCmd);
  }

  return oglCommands;
}

static std::vector<std::string> parseGlEnums(const pugi::xml_document &xml) {
  std::vector<std::string> glenums;

  for (auto xmlEnum : xml.select_nodes("/registry/enums/enum"))
    glenums.push_back(xmlEnum.node().attribute("name").value());

  return glenums;
}

void printCommandMacro(const Func &wglFn, const Func &oglFn, std::ostream &os) {

  auto printArg = [](const std::string &fname, unsigned anum, const ArgDescr &arg, std::ostream &os) {
    bool openBrace = false;
    if (arg.type == "GLboolean")
      os << "GetArgBoolean";
    else if (arg.type == "GLshort")
      os << "GetArgInt32";
    else if (arg.type == "GLenum")
      os << "GetArgUInt32";
    else if (arg.type == "GLuint")
      os << "GetArgUInt32";
    else if (arg.type == "GLsizei")
      os << "GetArgUInt32";
    else if (arg.type == "GLint")
      os << "GetArgInt32";
    else if (arg.type == "GLbitfield")
      os << "GetArgUInt32";
    else if (arg.type == "GLfloat")
      os << "GetArgFloat";
    else if (arg.type == "GLclampf")
      os << "GetArgFloat";
    else if (arg.type == "GLsizeiptr")
      os << "GetArgUInt32";
    else if (arg.type == "GLintptr")
      os << "GetArgUInt32";
    else if (arg.type == "const GLchar *")
      os << "GetArgString";
    else if (arg.type == "WebGLProgram?")
      os << "GetArgUInt32"; // GLuint glCreateProgram(void); => so WebGLProgram is mapped to GLuint
    else if (arg.type == "WebGLShader?")
      os << "GetArgUInt32"; // GLuint glCreateShader(GLenum shaderType); => so WebGLShader is mapped to GLuint
    else if (arg.type == "WebGLBuffer?")
      os << "GetArgUInt32"; // void glBindBuffer(GLenum target, GLuint buffer); => so WebGLBuffer is mapped to GLuint
    else if (arg.type == "WebGLFramebuffer?")
      os << "GetArgUInt32"; // void glGenFramebuffers(GLsizei n, GLuint *framebuffers);  => so WebGLFramebuffer is mapped to GLuint (with a stretch, because gl.createFramebuffer()->glGenFramebuffers(...))
    else if (arg.type == "WebGLRenderbuffer?")
      os << "GetArgUInt32"; // void glGenRenderbuffers(GLsizei n, GLuint *renderbuffers);  => so WebGLRenderbuffer is mapped to GLuint (with a stretch, because gl.createRenderbuffer()->glGenRenderbuffers(...))
    else if (arg.type == "WebGLTexture?")
      os << "GetArgUInt32"; // likely
    else if (arg.type == "BufferDataSource?")
      os << "GetArgUInt32"; // name mismatch: BufferDataSource? vs. BufferSource?, but likely uint
    else if (arg.type == "DOMString")
      os << "GetArgUInt32"; // GLint glGetAttribLocation(GLuint program, const GLchar *name);
    else if (arg.type == "ArrayBufferView?") { // created as new Uint8Array
      os << "GetArg(Binary, "; // "data" in void glReadPixels(GLint x,  GLint y,  GLsizei width,  GLsizei height,  GLenum format,  GLenum type,  GLvoid * data);
      openBrace = true;
    } else if (arg.type == "TexImageSource?") {
      os << "GetArg(Binary, "; // multitude of sources is supported: see 6.9 Texture Upload Width and Height
      openBrace = true;
    } else if (arg.type == "const void *") // for compressedTexImage2D, translates to GLintptr, see https://developer.mozilla.org/en-US/docs/Web/API/WebGLRenderingContext/compressedTexImage2D
      os << "GetArgUInt32";
    else
      Fail("unknown argument type '" << arg.type << "' for function '" << fname << "'")
    if (!openBrace)
      os << "(";
    os << anum << ")";
  };
  auto printArgs = [printArg](const std::string &fname, const std::vector<ArgDescr> &args) {
    std::ostringstream ss;
    unsigned anum = 1;
    for (auto &arg : args) {
      if (anum > 1)
        ss << ", ";
        
      printArg(fname, anum, arg, ss);
      anum++;
    }
    return ss.str();
  };

  os << "GL_FUNC(" << wglFn.fn.name << ", " << wglFn.args.size() << ", {" << std::endl;
  if (wglFn.fn.type != "void")
    os << "  Return(" << oglFn.fn.name << "(" << printArgs(wglFn.fn.name, wglFn.args) << "));" << std::endl;
  else {
    os << "  " << oglFn.fn.name << "(" << printArgs(wglFn.fn.name, wglFn.args) << ");" << std::endl;
    os << "  ReturnVoid(J);" << std::endl;
  }
  os << "});" << std::endl;
}

static std::set<std::string> getOpenGL_ESx_names(const pugi::xml_document &xml, const char *versionNumber) {
  std::set<std::string> res;

  for (auto feature : xml.select_nodes(str(boost::format("//registry/feature[@number='%1%']") % versionNumber).c_str())) {
    // enums
    for (auto glenum : feature.node().select_nodes("require/enum"))
      res.insert(glenum.node().attribute("name").value());
    // commands
    for (auto glcommand : feature.node().select_nodes("require/command"))
      res.insert(glcommand.node().attribute("name").value());
  }

  return res;
}

// output functions

static void doOutput(const FuncMSet &wglCommands1, const FuncMSet &wglCommands2,
                     const FuncMSet &oglCommands, const std::vector<std::string> &oglEnums,
                     const std::set<std::string> &allES2names, const std::set<std::string> &allES3names,
                     std::ostream &os)
{
  // helpers
  auto isInSet = [](const std::string &name, const std::set<std::string> &xset) {
    return xset.find(name) != xset.end();
  };
  auto stripPrefix = [](const std::string &name) {
    if (name.size() < 4 || name[0] != 'G' || name[1] != 'L' || name[2] != '_')
      Fail("The name '" << name << "'doesn't begin with the prefix GL_")
    return name.substr(3);
  };
  auto oglNameToWglName = [](const std::string &wname) {
    auto oname = std::string("gl") + wname;
    oname[2] = std::toupper(oname[2]);
    return oname;
  };
  auto findOglCmdForWglCmd = [&oglCommands,oglNameToWglName](const Func &wglCmd) -> const Func& {
    auto oname = oglNameToWglName(wglCmd.fn.name);
    auto it = oglCommands.find(oname);
    if (it == oglCommands.end())
      Fail("findOglCmdForWglCmd: can't find a matching OGL command for wname=" << wglCmd.fn.name << ": tried OpenGL name '" << oname << "'")
    if (it->second.size() != 1)
      Fail("findOglCmdForWglCmd: multiple matching OGL commands '" << oname << "'for wname=" << wglCmd.fn.name)
    return it->second[0];
  };

  auto outputWebGlxCommands = [&os,isInSet,findOglCmdForWglCmd,oglNameToWglName](const FuncMSet &wglCommands, const FuncMSet &oglCommands, const std::set<std::string> &allESxnames) {
    os << "" << std::endl;
    os << "// gl commands" << std::endl;
    os << "" << std::endl;
    for (auto &wglCmds : wglCommands)
      for (auto &wglCmd : wglCmds.second)
        if (isInSet(oglNameToWglName(wglCmd.fn.name), allESxnames))
          printCommandMacro(wglCmd, findOglCmdForWglCmd(wglCmd), os);
    os << "" << std::endl;
  };
  //auto outputWebGlEnums = [&os,isInSet](const std::vector<std::string> &glenums, const std::set<std::string> &allESxnames) {
  //  os << "" << std::endl;
  //  os << "// gl enums" << std::endl;
  //  os << "" << std::endl;
  //  for (auto &glenum : glenums)
  //    if (isInSet(glenum, allESxnames))
  //      os << "GL_CONST(" << stripPrefix(glenum) << ")" << std::endl;
  //  os << "" << std::endl;
  //};

  // begin
  os << "// (!!!)" << std::endl;
  os << "// (!!!) This is the auto-generated file webgl-auto-generated-macros.h. DO NOT CHANGE IT MANUALLY!" << std::endl;
  os << "// (!!!) To regenerate this file run the command: gmake webgl-auto-generated-macros.h" << std::endl;
  os << "// (!!!)" << std::endl;
  os << "" << std::endl;

  // commands
  outputWebGlxCommands(wglCommands1, oglCommands, allES2names);
  //outputWebGlxCommands(wglCommands2, oglCommands, allES3names);

  // enums
  //outputWebGlEnums(oglEnums, allES2names, os);

  // end
  os << "// (!!!)" << std::endl;
  os << "// (!!!) The end of the auto-generated file webgl-auto-generated-macros.h" << std::endl;
  os << "// (!!!)" << std::endl;
}

//
// MAIN
//

int main(int argc, char *argv[]) {

  // args
  if (argc != 4)
    Fail("Usage: " << argv[0] << " {WebGL-1.0-spec.xml} {WebGL-2.0-spec.xml} {OpenGL-specs.xml}")

  const char *argWebGl1XmlFileName = argv[1];
  const char *argWebGl2XmlFileName = argv[2];
  const char *argOpenGlXmlFileName = argv[3];

  //
  // parse XMLs into DOM trees
  //
  pugi::xml_document xmlWebGl1 = readXml(argWebGl1XmlFileName);
  pugi::xml_document xmlWebGl2 = readXml(argWebGl2XmlFileName);
  pugi::xml_document xmlOpenGl = readXml(argOpenGlXmlFileName);

  //
  // extract OpenGL commands from XML
  //
  auto oglCommands = parseGlCommands(xmlOpenGl);
  std::vector<std::string> oglEnums = parseGlEnums(xmlOpenGl);

  //
  // extract WebGL commands from XMLs
  //
  auto wglCommands1 = parseWebGlCommands(xmlWebGl1);
  auto wglCommands2 = parseWebGlCommands(xmlWebGl2);

  for (auto const &cmdv : wglCommands1)
    for (auto const &cmd : cmdv.second)
      std::cout << "wglCommand: " << cmd << std::endl;

  //
  // filter out only OpenGL ES 2 functions
  //
  std::set<std::string> allES2names = getOpenGL_ESx_names(xmlOpenGl, "2.0");
  std::set<std::string> allES3names = getOpenGL_ESx_names(xmlOpenGl, "3.0");

  //
  // output
  //
  doOutput(wglCommands1, wglCommands2, oglCommands, oglEnums, allES2names, allES3names, std::cout);

  return 0;
}

