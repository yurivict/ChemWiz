
//
// tests
//

var testContentLength1 = [
  "GET /index.html HTTP/1.1\r\n",
  "Abc: yes\r\n",
  "Abx: 123abcde\r\n",
  "Content-Length: 4\r\n",
  "\r\n",
  "abcd"
];
var testContentLength2 = [
  "GET /index.html HTTP/1.1\r\n",
  "Content-type: Unknown\r\n",
  "Content-Length: 10\r\n",
  "Miscellaneous: misc\r\n",
  "\r\n",
  "abcdexyzwv"
];

function substChar(str, c, s) {
  var off = 0;
  while (true) {
    var i = str.indexOf(c, off);
    if (i == -1)
      break;
    str = str.substring(0, i)+s+str.substring(i+1, str.length);
    off = i;
  }
  return str;
}

function doTest(testCase, expectedJson) {
  var buf = new Binary;
  var bufHead = 0;

  testCase.forEach(function(line) {
    //var l = line;
    //l = substChar(l, '\r', "{{{r}}}");
    //l = substChar(l, '\n', "{{{n}}}");

    //print("before-line: bufHead="+bufHead+" '"+l+"': state="+processor.getState());

    buf.appendString(line);
    bufHead += processor.processData(buf, bufHead);
      
    //print("after-line: bufHead="+bufHead+" '"+l+"': state="+processor.getState());
  });

  if (processor.getState() != 'F')
    throw "processor isn't at state=F"
  if (buf.size() != bufHead)
    throw "wrong bufHead="+bufHead+" buf.size="+buf.size();

  var headersJson = JSON.stringify(processor.headers);
  if (headersJson != expectedJson)
    throw "mismatch: headersJson={{{"+headersJson+"}}}, expectedJson={{{"+expectedJson+"}}}";
  else
    return ""; // success
}

var processor = null;

exports.run = function() {
  try {
    processor = require("http-protocol").create();
    var msg = doTest(testContentLength1, '{"body":"abcd","fields":{"Abc":"yes","Abx":"123abcde","Content-Length":"4"},"headline":"GET /index.html HTTP/1.1"}')
    processor.init();
    var msg = doTest(testContentLength1, '{"body":"abcd","fields":{"Abc":"yes","Abx":"123abcde","Content-Length":"4"},"headline":"GET /index.html HTTP/1.1"}')
    processor.init();
    var msg = doTest(testContentLength2, '{"body":"abcdexyzwv","fields":{"Content-Length":"10","Content-type":"Unknown","Miscellaneous":"misc"},"headline":"GET /index.html HTTP/1.1"}')
    processor.init();

    return "OK"
  } catch (err) {
    return ["FAIL", err];
  }
}
