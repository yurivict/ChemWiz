# ChemWiz
Program for manipulations with molecules and performing computations with them

## This program is pre-alpha and doesn't do anything useful yet

### Dependencies
* GNU Make (build)
* C++ compiler (build)
* MuJS: for running JavaScript code that is used for scripting (https://mujs.com/)
* Boost: for some container types and string formatting (build: header-only library)
* (optional, library) libdsrpdb: for parsing proteins in the PDB format (or perhaps any large and complex molecules) (https://graphics.stanford.edu/~drussel/pdb/)
* (optional, run-time) Jmol: for converting molecules to 3-D images, the executable 'jmoldata' is run from the scripts (http://jmol.sourceforge.net/)
* (optional, run-time) ffmpeg: for converting frames (images) to movies, the executable 'ffmpeg' is run from the scripts (https://www.ffmpeg.org/)

All optional library dependencies have corresponding build switches in the form USE_xx.

