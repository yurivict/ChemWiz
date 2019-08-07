// module fs: node-like module

// documentation: https://nodejs.org/api/fs.html#fs_file_system

// TODO FIX error object so that it has this structure: "errno":-2,"code":"ENOENT","syscall":"open","path":"xxxxx"
// docs:
// * promises API: https://nodejs.org/dist/latest-v10.x/docs/api/fs.html#fs_fs_promises_api (require('fs').promises)

// params
var paramFileReadStep = 1024*1024; // 1MB

//
// consts
//

var errnoToCode = {
  1:  "EPERM",
  2:  "ENOENT",
  3:  "ESRCH",
  4:  "EINTR",
  5:  "EIO",
  12: "ENOMEM",
  13: "EACCES",
  17: "EEXIST"
  // TODO the rest
};

var errnoToDescr = {
  1:  "Operation not permitted",
  2:  "No such file or directory",
  3:  "No such process",
  4:  "Interrupted system call",
  5:  "Input/output error",
  12: "Cannot allocate memory",
  13: "Permission denied",
  17: "File exists"
  // TODO the rest
};

//
// helpers
//

function resToCb(res, cb) {
  if (res != -1)
    cb(undefined); // success
  else
    cb(System.strerror(System.errno()));
}

function mkErr(syscall, path) {
  var errno = System.errno();
  var code = errnoToCode[errno];
  if (!code)
    throw "TODO error code for errno="+errno+" is not known";
  return {"errno": errno, "code": code, "syscall": syscall, "path": path};
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

exports.lstat = function(path, callback) { // fs.lstat(path[, options], callback): https://nodejs.org/dist/latest-v10.x/docs/api/fs.html#fs_fs_lstat_path_options_callback
  var s = FileApi.lstat(path);
  if (typeof s === "object")
    callback(undefined, s);
  else
    callback(mkErr("lstat", path));
};

exports.lstatSync = function(path) {
  var s = FileApi.lstat(path);
  if (typeof s === "object")
    return s;
  else
    throw "lstat failed: path="+path+" error="+errnoToCode[System.errno()];
};

exports.stat = function(path, callback) {
  var s = FileApi.stat(path);
  if (typeof s === "object")
    callback(undefined, s);
  else
    callback(mkErr("stat", path));
};

exports.statSync = function(path) {
  var s = FileApi.stat(path);
  if (typeof s === "object")
    return addStatsMethods(s);
  else {
    var errno = System.errno();
    throw errnoToCode[errno]+": "+errnoToDescr[errno].toLowerCase()+", stat '"+path+"'";
  }
};

