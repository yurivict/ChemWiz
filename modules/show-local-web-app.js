// module ShowLocalWebApp: open the browser and navigate to the local web app

var httpServeDirectory = require("http-serve-directory");


//
// exports
//

exports.showDir = function(dir) {
  // BUG: need to be able to wait for the browser process and to stop the http server once the browser is done
  Process.system("daemon qt5-QtWebEngine-browser/browser http://localhost:8181 2>/dev/null"); // XXX race condition: the app can open before the http server
  httpServeDirectory.serve(dir, 8181);
};
