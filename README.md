# ChemWiz
Program for manipulations with molecules and performing computations with them

## This program is pre-alpha and doesn't do anything useful yet

### Dependencies
* GNU Make (build)
* C++ compiler (build)
* MuJS: for running JavaScript code that is used for scripting (https://mujs.com/)
* Boost: for some container types and string formatting (build: header-only library)
* OpenSSL: for crypto used to access https:// URLs
* OpenBLAS: used by the 'rmsd' code for matrix computations
* (optional, library) libdsrpdb: for parsing proteins in the PDB format (or perhaps any large and complex molecules) (https://graphics.stanford.edu/~drussel/pdb/)
* (optional, build-time headers-only) mmtf-cpp: for parsing chemical structures in the MMTF format (https://github.com/rcsb/mmtf-cpp)
* (optional, run-time) Jmol: for converting molecules to 3-D images, the executable 'jmoldata' is run from the scripts (http://jmol.sourceforge.net/)
* (optional, run-time) ffmpeg: for converting frames (images) to movies, the executable 'ffmpeg' is run from the scripts (https://www.ffmpeg.org/)

All optional library dependencies have corresponding build switches in the form USE_xx.

### External code used as modules
* rmsd library (https://github.com/charnley/rmsd, BSD 2-Clause licensed): Jimmy Charnley Kromann's rmsd library for Root-Mean-Square-Deviation calculation

### Relevant bug reports

* Jmol produces invalid (not entirely valid) PNGs: https://sourceforge.net/p/jmol/bugs/604/

