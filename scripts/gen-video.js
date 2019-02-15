// module GenVideo: uses ffmpeg to convert a set of pngs to mp4

var fps = 25
//var qual = 15 // 15..25 are good, the lower the better

function createInsFile(pngs) {
  var insTxt = ""
  for (var i = 0; i < pngs.length; i++) {
    insTxt = insTxt+"file '"+pngs[i].fname()+"'\n"
  }
  return new TempFile("txt", insTxt)
}

function pngsToMp4(pngs) {
  var mp4 = new TempFile("mp4")
  system("ffmpeg -loglevel quiet -r "+fps+" -f concat -safe 0 -i "+createInsFile(pngs).fname()+" -vcodec libx264 "+mp4.fname())
  return mp4
}

exports.pngsToMp4 = pngsToMp4