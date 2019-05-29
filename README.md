# ChemWiz
Program for manipulations with molecules and performing computations with them

## This program is pre-alpha and doesn't do anything useful yet

### Dependencies
#### Build tools
* GNU Make
* C++ compiler

#### Header-only libraries
* Boost: for some container types and string formatting
* C++ Bitmap Library: to support the "Image" type (https://github.com/ArashPartow/bitmap)
* PicoSHA2: to compute cryptographic hashes (https://github.com/okdshin/PicoSHA2)
* Cblas: headers used by the rmsd++ headers, actual functions come from OpenBlas
* Lapacke: headers used by the rmsd++ headers
* nlohmann-json: for parsing of the periodic table json, etc.
* rang: to print colored and styled text to the terminal (https://github.com/agauniyal/rang)
* (optional) mmtf-cpp: for parsing chemical structures in the MMTF format (https://github.com/rcsb/mmtf-cpp)

#### Reglar library dependencies
* MuJS: for running JavaScript code that is used for scripting (https://mujs.com/)
* OpenSSL: for crypto used to access https:// URLs
* OpenBLAS: used by the 'rmsd' code for matrix computations
* SQLite3: to save results and persistent state across runs. The library is loaded dynamically by the JS code, the executable isn't linked with it.
* Qhull: to compute convex hulls of molecules (https://github.com/qhull/qhull)
* (optional) OpenBabel: for parsing molecules in SMILES format (http://openbabel.org/wiki/Main_Page)
* (optional) libdsrpdb: for parsing proteins in the PDB format (or perhaps any large and complex molecules) (https://graphics.stanford.edu/~drussel/pdb/)

#### Run-time dependencies
* (optional) Jmol: for converting molecules to 3-D images, the executable 'jmoldata' is run from the scripts (http://jmol.sourceforge.net/)
* (optional) ffmpeg: for converting frames (images) to movies, the executable 'ffmpeg' is run from the scripts (https://www.ffmpeg.org/)

All optional library dependencies have corresponding build switches in the form USE_xx.

### External code used as modules
* rmsd library (https://github.com/charnley/rmsd, BSD 2-Clause licensed): Jimmy Charnley Kromann's rmsd library for Root-Mean-Square-Deviation calculation

### Relevant bug reports

* Jmol produces invalid (not entirely valid) PNGs: https://sourceforge.net/p/jmol/bugs/604/

