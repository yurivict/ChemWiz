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

//
// imgsToMp4: takes an array of images as TmpFiles objects and converts them to the mpeg4 video
//
function imgsToMp4(pngs) {
  var mp4 = new TempFile("mp4")
  Process.system("ffmpeg -loglevel quiet -r "+fps+" -f concat -safe 0 -i "+createInsFile(pngs).fname()+" -vcodec libx264 "+mp4.fname())
  return mp4
}

exports.imgsToMp4 = imgsToMp4
