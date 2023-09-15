# Uppaal Clock Difference Diagram Library

*Clock Difference Diagrams* (CDD) is a [BDD](https://en.wikipedia.org/wiki/Binary_decision_diagram)-like
data-structure for effective representation and manipulation of certain non-convex subsets of the Euclidean space,
notably those encountered in verification of timed automata.
It is shown that all set-theoretic operations including inclusion checking may be carried out efficiently on Clock Difference Diagrams.
Other clock operations needed for fully symbolic analysis of timed automata e.g. future- and
reset-operations, can be obtained based on a tight normal form for CDD.
Experimental results demonstrate significant space-savings.
For instance, in nine industrial examples the savings are on average 42% and with a moderate increase in runtime.

**"Clock Difference Diagrams"** by *Kim Guldstrand Larsen, Justin Pearson, Carsten Weise and Wang Yi*. "Nordic Journal of Computing", 1999. [[pdf](https://vbn.aau.dk/ws/files/425046823/CDD_26pages_nordic_journal_of_computing_1999.pdf)] [[bib](https://dblp.uni-trier.de/rec/journals/njc/LarsenPWY99.html?view=bibtex)]

**"Efficient Timed Reachability Analysis Using Clock Difference Diagrams"** by *Gerd Behrmann, Kim Guldstrand Larsen, Justin Pearson, Carsten Weise and Wang Yi*. "Computer Aided Verification, 11th International Conference, CAV'99", Trento, Italy, July 6-10, 1999, Proceedings. [[doi:10.1007/3-540-48683-6_30](https://doi.org/10.1007/3-540-48683-6_30)] [[bib](https://dblp.uni-trier.de/rec/conf/cav/BehrmannLPWY99.html?view=bibtex)]


## Compile
The following packages need to be installed: `cmake gcc xxHash doctest boost`.

In addition, [UUtils](https://github.com/UPPAALModelChecker/UUtils) and [UDBM](https://github.com/UPPAALModelChecker/UDBM) are needed, which can be installed system-wide, or locally by running the script [`getlibs.sh`](getlibs.sh).

Get the dependencies, compile the source into `build` directory and run the unit tests:
```sh
unset CMAKE_TOOLCHAIN_FILE # If it was set before
cmake -B build
cmake --build build
(cd build ; ctest --output-on-failure)
```

### Cross-compile For Linux 32-bit (i686):
```sh
export CMAKE_TOOLCHAIN_FILE=$PWD/cmake/toolchain/i686-linux.cmake 
cmake -B build-linux32
cmake --build build-linux32
(cd build-linux32 ; ctest --output-on-failure)
```

### Cross-compile For Windows 64-bit (x64_86) using MinGW/[MSYS2](https://www.msys2.org/):
```sh
export CMAKE_TOOLCHAIN_FILE=$PWD/cmake/toolchain/x86_64-w64-mingw32.cmake
cmake -B build-win64 -DSTATIC=ON
cmake --build build-win64
(cd build-win64 ; ctest --output-on-failure)
```

### Cross-compile For Windows 32-bit (i686) using MinGW/[MSYS2](https://www.msys2.org/):
```sh
export CMAKE_TOOLCHAIN_FILE=$PWD/cmake/toolchain/i686-w64-mingw32.cmake
cmake -B build-win32 -DSTATIC=ON
cmake --build build-win32
(cd build-win32 ; ctest --output-on-failure)
```

## Debugging
Address-sanitizer can be enabled by adding `-DASAN=ON` option to `cmake` build generation line.
This relies on the compiler support (currently GCC does not support sanitizers on Windows).
