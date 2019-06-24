// module HttpServeDirectory: serves the directory over http

var fs = require('fs');
var http = require('http');

//
// helpers
//

function log(msg) {
  //print("LOG(HttpServeDirectory): "+msg);
}

function extname(filePath) {
  var dot = filePath.lastIndexOf('.');
  return dot != -1 ? filePath.substring(dot+1, filePath.length) : "";
}

function page404(url) {
  return "<html>\n"+
         "  <body>"+
         "    <h2>Requested file not found: "+url+"</h2>"+
         "  </body>"+
         "</html>";
}

var extToContentType = {
  'js':   'text/javascript',
  'css':  'text/css',
  'json': 'application/json',
  'png':  'image/png',
  'jpg':  'image/jpg',
  'wav':  'audio/wav'
};

function pathToContentType(filePath) {
  var contentType = extToContentType[extname(filePath)];
  if (contentType)
    return contentType;
  else
    return 'text/html';
}

//
// exports
//

exports.serve = function(dir, port) {
  http.createServer(function (request, response) {
    log(">>> REQ url="+request.url);
    var filePath = request.url;
    if (filePath == '/')
      filePath = '/index.html';
    if (filePath.charAt(0) != '/')
      filePath = '/'+filePath;
    filePath = dir+filePath;

    fs.readFile(filePath, function(error, content) {
      if (!error) {
        log("<<< REQ url="+request.url+" -> ct="+pathToContentType(filePath)+" file="+filePath+" nbytes="+content.length);
        response.writeHead(200, {'Content-Type': pathToContentType(filePath)});
        response.end(content, 'utf-8');
      } else {
        log("<<< REQ url="+request.url+" -> 404");
        response.writeHead(404, {'Content-Type': 'text/html'});
        response.end(page404(request.url), 'utf-8');
      }
    });
  }).listen(port);
};
