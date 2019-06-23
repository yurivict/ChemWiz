// module HttpProtocol: http protocol parser, accepts text, returns the parsed http object

//
// helpers
//

function warn(msg) {
  print("warning(http-protocol): "+msg);
}

function Throw(msg) {
  throw "error(http-protocol):"+msg;
}

function findLine(buf, off) { // finds \r\n in the string, doesn't tolerate just \n, returns the offset past \r\n
  var eol = buf.findChar(off, '\n');
  if (eol == -1)
    return -1;
  if (eol == off || buf.getChar(eol-1) != '\r')
    Throw("invalid string: off="+off+" eol="+eol+" chr="+buf.getChar(eol-1));
  return eol+1;
}

function parseHeadline(headers) {
  // rfc2616-sec5: Method SP Request-URI SP HTTP-Version CRLF
  var spaceAtUri     = headers.headline.indexOf(' ');
  var spaceAtVersion = headers.headline.lastIndexOf(' ');

  headers.headlineMethod      = headers.headline.substring(0, spaceAtUri);
  headers.headlineUri         = headers.headline.substring(spaceAtUri+1, spaceAtVersion);
  headers.headlineHttpVersion = headers.headline.substring(spaceAtVersion+1, headers.headline.length);
}


//
// exported functions
//

function createProcessor() {
  var processor = {
    init: function() {
      this.state = 'I';
      this.headers = createHeaders();
    },
    pick: function() {
      var headers = this.headers;
      this.init();
      return headers;
    },
    processData: function(buf, bufHead) {
      var off = bufHead;
      var bufLen = buf.size();
      while (this.state != 'F' && off < bufLen) {
        switch (this.state) {
        case 'I':
          var off1 = findLine(buf, off);
          if (off1 == -1)
            return off - bufHead;
          this.headers.headline = buf.getSubString(off, off1-2);
          parseHeadline(this.headers);
          off = off1;
          this.state = 'H';
          break;
        case 'H':
          var off1 = findLine(buf, off);
          if (off1 == -1)
            return off - bufHead;
          if (off+2 < off1) { // has some chars
            var offColon = buf.findChar(off, ':');
            if (offColon == -1 || offColon >= off1)
              Throw("no colon in the headers when one is expected");
            var key = buf.getSubString(off, offColon);
            var val = buf.getSubString(offColon + 1, off1-2).trim();
            this.headers.fields[key] = val;
            off = off1;
          } else { // empty line ends headers
            off += 2;
            if (this.headers.headlineMethod != "POST") {
              // body only exists for POST requests
              this.state = 'F';
              this.headers.body = null;
            } else {
              this.state = 'B';
              // find the length of the content
              if (this.headers.fields["Content-Length"] != undefined) {
                this.headers.body = new Binary();
                this.bodyEndSignal = 'L'; // 'L'ength-based, 'E'of-based
                this.bodyNeedLength = parseInt(this.headers.fields["Content-Length"], 10);
                this.bodyTodoLength = this.bodyNeedLength;
                this.bodyDoneLength = 0;
                break;
              } else if (this.headers.fields["Transfer-Encoding"] != undefined) {
                Throw("Transfer-Encoding not supported yet")
              } else {
                Throw("No Content-Length or Transfer-Encoding is specified: EOF isn't supported yet")
              }
            }
            continue; // end of headers: go to the next cycle
          }
          break;
        case 'B': // 'B'ody
          switch (this.bodyEndSignal) {
          case 'L': // 'L'ength-based
            var bufLen = buf.size();
            var available = bufLen - off;
            var stepLength = available <= this.bodyNeedLength ? available : this.bodyNeedLength;
            this.headers.body.appendRange(buf, off, off+stepLength);
            off += stepLength;
            available -= stepLength;
            this.bodyNeedLength -= stepLength;
            if (this.bodyNeedLength == 0) {
              this.state = 'F';
              this.headers.body = this.headers.body.toString();
            }
            return off - bufHead;
          default:
            Throw("unknown bodyEndSignal: "+this.bodyEndSignal);
          }
          break;
        default:
          Throw("unknown state: "+this.state);
        }
      }
      return off - bufHead;
    },
    isFinished: function() {
      return this.state === 'F';
    },
    getState: function() {
      return this.state;
    },
    // internals
    state: 'I', // 'I'nitial (expect the http headline), 'H'eaders (expect a header line), 'B'ody (expect the body data, until the size is reached), 'F'inished
  };
  processor.init();
  return processor;
}

function createHeaders() {
  return {headline: "", fields: {}, body: null};
}

function fmtResponseHeadline(httpVersion, httpStatusCode, httpReasonPhrase) {
  // rfc2616-sec6: Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF
  return httpVersion+" "+httpStatusCode+" "+httpReasonPhrase;
}

function formResponse(headers, body) {
  var buf = new Binary();

  var addLine = function(line) {
    buf.appendString(line+"\r\n");
  }

  // correct headers if needed
  if (body != undefined)
    headers.fields["Content-Length"] = body.size();
  else if (headers.fields["Content-Length"] != undefined) {
    warn("Content-Length is set when there is no body");
    headers.fields["Content-Length"] = undefined;
  }

  // format headers/body
  addLine(headers.headline);
  Object.keys(headers.fields).forEach(function(key) {
    addLine(key+": "+headers.fields[key]);
  });
  addLine("");
  if (body != undefined)
    buf.append(body);

  return buf;
}

///
/// exports
///

exports.createProcessor = createProcessor;
exports.createHeaders = createHeaders;
exports.fmtResponseHeadline = fmtResponseHeadline;
exports.formResponse = formResponse;
