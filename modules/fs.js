// module fs: node-like module

// documentation: https://nodejs.org/api/fs.html#fs_file_system


// params
var paramFileReadStep = 1024*1024; // 1MB

//
// helpers
//

function resToCb(res, cb) {
  if (res != -1)
    cb(undefined); // success
  else
    cb(System.strerror(System.errno()));
}


//
// exports
//

exports.unlink = function(fname, cb) {
  resToCb(FileApi.unlink(fname), cb);
};

exports.rename = function(fname1, fname2, cb) {
  resToCb(FileApi.rename(fname1, fname2), cb);
};

exports.readFile = function readFile(path, cb) {
  // open
  var fd = FileApi.open(path, FileApi.O_RDONLY);
  if (fd == -1)
    return cb(System.strerror(System.errno()));
  // read
  var buf = new Binary();
  var done = 0;
  while (true) {
    var res = FileApi.read(fd, buf, done, paramFileReadStep);
    if (fd == -1) {
      var err = System.strerror(System.errno());
      FileApi.close(fd); // ignore the error
      return cb(err);
    }
    done += res;
    if (res < paramFileReadStep) { // eof
      FileApi.close(fd); // ignore the error
      return cb(undefined, buf.toString());
    }
  }
};
