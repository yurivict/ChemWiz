var p = readXyzFile("molecules/Amino_Acids/L-Valine.xyz").appendAminoAcid(readXyzFile("molecules/Amino_Acids/L-Valine.xyz"))

function rotateAtoms(M, atoms) {
  for (var i = 0; i < atoms.length; i++) {
    var a = atoms[i]
    a.setPos(mulMatVec(M, a.getPos()))
  }
}

for (var i = 0; i < 1000; i++) {
  print("--cycle#"+i+"--")
  rotateAtoms(matRotate([0.5,0.5,-0.5]), p.findAaLast(p))
}

