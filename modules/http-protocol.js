// module HttpProtocol: http protocol parser, accepts text, returns the parsed http object

//
// helpers
//

function Throw(msg) {
  throw "Http error:"+msg;
}

function findLine(buf, off) { // finds \r\n in the string, doesn't tolerate just \n, returns the offset past \r\n
  var eol = buf.findChar(off, '\n');
  if (eol == -1)
    return -1;
  if (eol == off || buf.getChar(eol-1) != '\r')
    Throw("invalid string: off="+off+" eol="+eol+" chr="+buf.getChar(eol-1));
  return eol+1;
}

///
/// exports
///

exports.create = function() {
  var processor = {
    init: function() {
      this.state = 'I';
      this.headers = {headline: "", fields: {}, body: null};
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
            this.state = 'B';
            // find the length of the content
            if (this.headers.fields["Content-Length"] != undefined) {
              this.bodyEndSignal = 'L'; // 'L'ength-based, 'E'of-based
              this.bodyNeedLength = parseInt(this.headers.fields["Content-Length"], 10);
              this.bodyTodoLength = this.bodyNeedLength;
              this.bodyDoneLength = 0;
              this.headers.body = new Binary();
              break;
            } else if (this.headers.fields["Transfer-Encoding"] != undefined) {
              Throw("Transfer-Encoding not supported yet")
            } else {
              Throw("No Content-Length or Transfer-Encoding is specified: EOF isn't supported yet")
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

