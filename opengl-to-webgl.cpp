
#include <pugixml.hpp>

#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>


//
// types
//
typedef std::pair<std::string,std::string> TypeAndName;

struct Func {
  TypeAndName              fn;
  std::vector<TypeAndName> args;
};

//
// helpers
//
TypeAndName parseTypeAndName(const pugi::xml_node &node) {
  std::ostringstream osType;

  for (auto sub : node) {
    std::string txt = sub.value();
    if (!txt.empty())
      osType << txt;
    else if (::strcmp(sub.name(), "ptype") == 0)
      osType << sub.child_value();
  }

  return TypeAndName(osType.str(), node.child_value("name"));
}

void dbgPrintExplicit(const Func &f, std::ostream &os) {
  os << "[DBG] glFn: fret=" << f.fn.first << " fname=" << f.fn.second << " (";
  bool fst = true;
  for (auto &a : f.args) {
    if (fst)
      fst = false;
    else
      os << ", ";

    os << "atype=" << a.first << " aname=" << a.second;
  }
  os << ")" << std::endl;
}

//
// MAIN
//

int main(int argc, char *argv[]) {

  //
  // parse XML into DOM
  //
  std::ifstream is(argv[1]);
  std::string src((std::istreambuf_iterator<char>(is)),
                   std::istreambuf_iterator<char>());

  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_string(src.c_str());

  if (!result) {
    std::cout << "Failed to parse XML\n";
    return 1;
  }

  //
  // all parsed functions
  //
  std::vector<Func> funcs;

  //
  // extract gl-commands from DOM
  //
  pugi::xpath_node_set cmds = doc.select_nodes("/registry/commands/command");
  for (auto cmd : cmds) {
    Func f;

    f.fn = parseTypeAndName(cmd.node().child("proto"));
    for (auto &child : cmd.node().select_nodes("param"))
      f.args.push_back(parseTypeAndName(child.node()));

    funcs.push_back(f);
  }

  //
  // output
  //
  for (auto &f : funcs)
    dbgPrintExplicit(f, std::cout);
}

