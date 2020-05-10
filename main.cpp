
#include "xerror.h"
#include "common.h"
#include "js-binding.h"
#include "js-support.h"
#include "util.h"
#include "tm.h"

#include <mujs.h>
#include <rang.hpp>

#include <boost/exception/diagnostic_information.hpp>
#include <boost/exception_ptr.hpp>

#include <iostream>

static void printTime(std::time_t tm, const char *when) {
  std::cout << "-- " << PROGRAM_NAME << " is " << when << " at " << ctime(&tm); // ctime contains EOL
}

static int usage(const char *ename) {
  std::cerr << ename << " expects arguments: [(file.js|-s js-script) ...]" << std::endl;
  return 1;
}

static int main_guarded(int argc, char* argv[]) {

  // arguments parsing: expect at least one argument
  if (argc < 2)
    return usage(argv[0]);

  // time printouts
  printTime(Tm::start(), "starting");
  static Util::OnExit doPrintTime([](){
    auto tmFinish = Tm::now();
    printTime(tmFinish, "finishing");
    std::cout << "-- ran for " << (tmFinish-Tm::start()) << " second(s) (wallclock) --" << std::endl;
  });

  // run JavaScript, everything else is done from there
  unsigned errs = 0;
  if (js_State *J = js_newstate(NULL, NULL, JS_STRICT)) {
    // register args as scriptArgs (following how mujs does it)
    js_newarray(J);
    for (unsigned i = 2; i < argc; i++) {
      js_pushstring(J, argv[i]);
      js_setindex(J, -2, i-2);
    }
    js_setglobal(J, "scriptArgs");

    // register our functions
    JsBinding::registerFunctions(J);
    JsSupport::registerFuncRequire(J);
    JsSupport::registerFuncImportCodeString(J);
    JsSupport::registerErrorToString(J);

    // execute supplied JavaScript file or string argument
    for (int i = 1; i < argc && !errs; i++) {
      if (argv[i][0] != '-')
        errs += js_dofile(J, argv[i]);
      else if (argv[i][1] == 's' && argv[i][2] == 0 && i+1 < argc) {
        errs += js_dostring(J, argv[++i]);
      } else {
        return usage(argv[0]);
      }
    }

    // free the interpreter
    js_freestate(J);
  } else {
    ERROR("Failed to create the JavaScript interpreter")
  }

  if (errs)
    std::cerr << rang::fg::red << "The process finished with errors!" << rang::style::reset << std::endl;

  return !errs ? 0 : 1;
}

int main(int argc, char* argv[]) {
  try {
    return main_guarded(argc, argv);
#if defined(USE_EXCEPTIONS)
  } catch (const Exception &e) {
    std::cerr << "error: " << e.what() << std::endl;
    return 1;
#endif
  } catch (const std::runtime_error &e) {
    std::cerr << "error (runtime error): " << e.what() << std::endl;
    return 1;
  } catch (const std::exception& e) {
    std::cerr << "error (exception): " << e.what() << std::endl;
    return 1;
  } catch (const boost::exception &e) {
    std::cerr << "error (boost): " << boost::diagnostic_information(e) << std::endl;
    return 1;
  } catch (...) {
    std::cerr << "error (UNKNOWN): " << std::endl;
    return 1;
  }
}
