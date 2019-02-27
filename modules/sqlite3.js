// module SQLite3

var soLib = "/usr/local/lib/libsqlite3.so"
var soHandle = Dl.open(soLib, 0)
// SQLite3 library symbols CAVEAT no way to unload the library yet
var sqlite3_open = Dl.sym(soHandle, "sqlite3_open")
var sqlite3_close = Dl.sym(soHandle, "sqlite3_close")

// error processing
function ckErr(err) {
  if (err != 0)
    throw "SQLite Error: err="+err
}

exports.openDatabase = function(loc) {
  var open = Invoke.int32StrOPtr(sqlite3_open, loc)
  ckErr(open[0])
  return { // db
    dbHandle: open[1],
    prepare: function(sSql) {
      print("SQLite3: called prepare function")
    },
    run: function(sSql) {
      print("SQLite3: called run function")
    },
    close: function() {
      ckErr(Invoke.int32Ptr(sqlite3_close, this.dbHandle))
    }
  }
}

