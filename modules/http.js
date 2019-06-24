// module Http: inet/http server-side processing

var httpProtocol = require("http-protocol");

// Relevant documentation:
// * HTTP Server Methods and Properties: https://www.w3schools.com/nodejs/obj_http_server.asp
// * Node.js IncomingMessage Object: https://www.w3schools.com/nodejs/obj_http_incomingmessage.asp
// * Node.js HTTP ServerResponse Object: https://www.w3schools.com/nodejs/obj_http_serverresponse.asp

//
// params
//

var dbgDoLog             = false;
var paramReadSize        = 1024*32;
var paramSelectTimeoutMs = 10000; // select timeout in ms
var paramOurHttpVersion  = "HTTP/1.1";
var paramHttpCodeDescriptions = {
  200: "OK",
  201: "Created",
  202: "Accepted",
  203: "Non-Authoritative Information",
  204: "No Content",
  205: "Reset Content",
  206: "Partial Content",
  207: "Multi-Status",
  208: "Already Reported",
  226: "IM Used",
  300: "Multiple Choices",
  301: "Moved Permanently",
  // TODO the rest of 3xx
  400: "Bad Request",
  401: "Unauthorized",
  404: "Not Found",
  // TODO the rest of 4xx
  500: "Internal Server Error"
  // TODO the rest of 5xx
};

//
// helpers
//

function ckErr(res, fname) {
  if (res == -1)
    throw "http: " + fname + " failed: "+System.strerror(System.errno());
}

function warn(msg) {
  print("warning(http): "+msg);
}

function log(msg, servData) {
  if (dbgDoLog)
    print("LOG(http): "+msg);
}

function logServ(msg, servData) {
  if (dbgDoLog)
    log("SERVER(sock="+servData._servSock_+"): "+msg);
}

function logClnt(msg, clntData) {
  if (dbgDoLog)
    log("CLIENT(sock="+clntData._clntSock_+"): "+msg);
}

function Throw(msg) {
  throw "error(http): "+msg;
}

function findHttpCodeDescription(httpStatusCode) {
  var descr = paramHttpCodeDescriptions[httpStatusCode];
  if (descr != undefined)
    return descr
  Throw("Unknown httpCode="+httpStatusCode);
}

function delOneFromArray(arr, val) {
  for (var i = 0; i < arr.length; i++) {
    if (arr[i] === val) {
      arr.splice(i, 1); 
      i--;
      break; // only one
    }
  }
}


function dbgFormatData(buf, bufHead) {
  var nl = ".....> ";
  var str = buf.getSubString(bufHead, buf.size());
  str = str.replace(/\r/g, "");
  str = str.replace(/\n/g, "\n"+nl);
  return "\n"+nl+str;
}

//
// interface functions
//

function createServer(requestProcessor) {
  // create the socket
  var s = SocketApi.socket(SocketApi.PF_INET, SocketApi.SOCK_STREAM, 0);
  ckErr(s, "socket")

  // allow multiple binds to prevent previous stale binds to fail new listeners
  var res = SocketApi.setsockopt(s, SocketApi.SOL_SOCKET, SocketApi.SO_REUSEPORT, 1);
  ckErr(res, "setsockopt");

  log("createServer: created servSock="+s);

  // return the http handler object
  return {
    // fields
    _servSock_: s,                            // socket
    _userRequestProcessor_: requestProcessor, // user-supplied request processor
    _stop_: false,                            // stop signal
    _clients_: {},                            // all clients
    // internal functions
    _acceptConnection_: function(clntSock) {
      var Server = this;
      // create the client record
      this._clients_[clntSock] = { // clntData
        // socket
        _clntSock_: clntSock,
        // http buffers and processors
        bufOut: new Binary(),
        bufOutHead: 0, // the offset of the data to be written, tail is always the end of the buffer
        httpOutProcessor: httpProtocol.createProcessor(),
        bufIn: new Binary(),
        bufInHead: 0, // the offset of the data to be read, tail is always the end of the buffer
        httpInProcessor: httpProtocol.createProcessor(),
        // event handlers
        canRead: function() {
          var Client = this;
          logClnt("canRead triggered: ...", Client);
          var nbytes = SocketApi.read(Client._clntSock_, Client.bufIn, Client.bufIn.size(), paramReadSize);
          ckErr(nbytes, "read");
          logClnt("canRead triggered: read nbytes="+nbytes, Client);
          if (nbytes > 0) {
            Server._onNewClientData(clntSock, Client);
            return false; // no eof
          } else { // EOF: client has disconnected
            Server._onEof(clntSock, Client);
            return true; // eof
          }
        },
        canWrite: function() {
          var Client = this;
          var nbytes = SocketApi.write(Client._clntSock_, Client.bufOut, Client.bufOutHead, Client.bufOut.size()-Client.bufOutHead);
          logClnt("canWrite triggered: wrote "+nbytes+" bytes, asked to write "+(Client.bufOut.size()-Client.bufOutHead)+" bytes", Client);
          ckErr(nbytes, "write");
          Client.bufOutHead += nbytes;
        },
        onExcept: function() {
          throw "Unhandled except condition on a client socket @sock="+this._clntSock_;
        }
      };
    },
    _onNewClientData: function(clntSock, clntData) {
      logClnt("_onNewClientData >>>: data(unprocessed)="+dbgFormatData(clntData.bufIn, clntData.bufInHead), clntData);
      clntData.bufInHead += clntData.httpInProcessor.processData(clntData.bufIn, clntData.bufInHead);
      logClnt("_onNewClientData <<<: data(unprocessed)="+dbgFormatData(clntData.bufIn, clntData.bufInHead)+" -> httpState="+clntData.httpInProcessor.getState(), clntData);
      if (clntData.httpInProcessor.getState() == 'F') {
        this._onNewHttpRequest(clntSock, clntData, clntData.httpInProcessor.pick());
      }
    },
    _onEof: function(clntSock, clntData) {
      // warn if some data is left over
      if (clntData.bufOutHead < clntData.bufOut.size())
        warn("client @sock="+clntSock+" has disconnected and left "+(clntData.bufOut.size()-clntData.bufOutHead)+" bytes of outgoing data unsent");
      if (clntData.bufInHead < clntData.bufIn.size())
        warn("client @sock="+clntSock+" has disconnected and left "+(clntData.bufIn.size()-clntData.bufInHead)+" bytes of incoming data unprocessed");

      // close
      ckErr(SocketApi.close(clntSock), "close");

      // log
      logClnt("client disconnected", clntData);

      // delete the client record
      delete this._clients_[clntSock];
    },
    _onNewHttpRequest: function(clntSock, clntData, httpHeaders) {
      var Server = this;
      logClnt("_onNewHttpRequest: got the http request="+JSON.stringify(httpHeaders), clntData);
      // response
      var responseHeaders = httpProtocol.createHeaders();
      var responseBuf = null;
      // call user callback
      this._userRequestProcessor_(
      // IncomingMessage Object
      {
        headers:      httpHeaders.fields,
        httpVersion:  httpHeaders.headlineHttpVersion,
        method:       httpHeaders.headlineMethod,
        // rawHeaders: TODO
        // rawTrailers: TODO
        // setTimeout(): TODO
        // statusCode: TODO
        // socket: TODO
        // trailers: TODO
        url: httpHeaders.headlineUri
      },
      // ServerResponse Object
      {
        //addTrailers()
        end: function(txt) {
          if (responseBuf == null)
            responseBuf = new Binary();
          responseBuf.appendString(txt);
          // send
          Server._onSendHttpResponse(clntSock, clntData, responseHeaders, responseBuf);
        },
        //finished: TODO
        //getHeader(): TODO
        //headersSent: TODO
        //removeHeader(): TODO
        //sendDate: TODO
        //setHeader(): TODO
        //setTimeout: TODO
        //statusCode: TODO
        //statusMessage: TODO
        write: function(txt) {
          if (responseBuf == null)
            responseBuf = new Binary();
          responseBuf.appendString(txt);
        },
        //writeContinue:: TODO
        writeHead: function(httpStatusCode, keyVals) {
          responseHeaders.headline = httpProtocol.fmtResponseHeadline(paramOurHttpVersion, httpStatusCode, findHttpCodeDescription(httpStatusCode));
          Object.keys(keyVals).forEach(function(key) {
            responseHeaders.fields[key] = keyVals[key];
          });
        }
      });
    },
    _onSendHttpResponse: function(clntSock, clntData, headers, bodyBuf) {
      logClnt(">>> _onSendHttpResponse: headers="+JSON.stringify(headers)+" clntData.bufOut.size="+clntData.bufOut.size(), clntData);
      clntData.bufOut.append(httpProtocol.formResponse(headers, bodyBuf));
      logClnt("<<< _onSendHttpResponse: clntData.bufOut.size="+clntData.bufOut.size(), clntData);
    },
    // public API
    listen: function(port) {
      var Server = this;
      ckErr(SocketApi.bindInet(Server._servSock_, port), "bind");
      ckErr(SocketApi.listen(Server._servSock_, 10), "listen");
      while (!Server._stop_) {
        // form sock lists
        var lstRD = [Server._servSock_];
        var lstWR = [];
        Object.keys(Server._clients_).forEach(function(clntSock) {
          clntSock = parseInt(clntSock, 10);
          var clntData = Server._clients_[clntSock];
          lstRD.push(clntSock);
          if (clntData.bufOutHead < clntData.bufOut.size()) {
            logClnt("have "+(clntData.bufOut.size() - clntData.bufOutHead)+" bytes to write (bufOut.size="+clntData.bufOut.size()+", bufOutHead="+clntData.bufOutHead+")", clntData);
            lstWR.push(clntSock);
          }
        });
        // wait
        logServ("> calling select: lstRD="+JSON.stringify(lstRD)+" lstWR="+JSON.stringify(lstWR), Server);
        var res = SocketApi.select(lstRD, lstWR, undefined, paramSelectTimeoutMs);
        logServ("< calling select: res="+JSON.stringify(res), Server);
        ckErr(res[0][0], "socket");

        // read
        res[1].forEach(function(sock) {
          logServ("select -> RD triggered on socket="+sock, Server);
          if (sock == Server._servSock_) {
            // accept the connection and create the clntData record for this client connection
            var clntSock = SocketApi.accept(Server._servSock_);
            ckErr(clntSock, "accept");
            logServ("client accepted: clntSock="+clntSock, Server);
            Server._acceptConnection_(clntSock);
          } else {
            if (Server._clients_[sock].canRead()) {
              delOneFromArray(res[2], sock);
              delOneFromArray(res[3], sock);
            }
          }
        });

        // write
        res[2].forEach(function(sock) {
          logServ("select -> WR triggered on socket="+sock, Server)
          Server._clients_[sock].canWrite();
        });

        // except
        res[3].forEach(function(sock) {
          logServ("select -> EX triggered on socket="+sock, Server)
          Server._clients_[sock].onExcept();
        });
      }
    }
  };
}

/// exports

exports.createServer = createServer;
