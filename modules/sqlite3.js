// module SQLite3

//
// library
//

var soLib = "/usr/local/lib/libsqlite3.so"
var soHandle = Dl.open(soLib, 0)

//
// symbols
//

// CAVEAT no way to unload the library yet
var sqlite3_open   = Dl.sym(soHandle, "sqlite3_open")
var sqlite3_exec   = Dl.sym(soHandle, "sqlite3_exec")
var sqlite3_close  = Dl.sym(soHandle, "sqlite3_close")
var sqlite3_errstr = Dl.sym(soHandle, "sqlite3_errstr")

// error codes
var enums = {
  SQLITE_ABORT: 4
}

//
// error processing
//

function errStr(errCode) {
  return Invoke.strInt(sqlite3_errstr, errCode)
}

function formatErrorMessage(errCode) {
  return errStr(errCode)+" (errCode="+errCode+")"
}

function throwError(msg) {
  throw "SQLite Error: " + msg
}

function ckErr(err) {
  if (err != 0)
    throwError(formatErrorMessage(err))
}

function ckErr2(err2) { // for functions that return errorCode+message
  if (err2[0]!=0 || err2[1]) {
    if (err2[0] == enums.SQLITE_ABORT)
      return // ignore query aborts since they are user-induced errors
    throwError(formatErrorMessage(err2[0]) + ": "+err2[1])
  }
}

//
// interface
//

exports.openDatabase = function(loc) {
  var resOpen = Invoke.intStrOPtr(sqlite3_open, loc)
  ckErr(resOpen[0])
  return { // db
    dbHandle: resOpen[1],
    prepare: function(sSql) {
      throw "SQLite3: called prepare function"
    },
    run: function(sSql, rowCb, opaque) { // opaque objects are dysfunctional in JavaScript, but we support it to maintain the SQLite interface compatibility
      ckErr2(Invoke.intPtrStrCb4intOpaqueIntSArrSArrPtrOStr(sqlite3_exec, this.dbHandle, sSql, rowCb, opaque))
    },
    close: function() {
      ckErr(Invoke.intPtr(sqlite3_close, this.dbHandle))
    }
  }
}

