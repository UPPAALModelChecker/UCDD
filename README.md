# Uppaal Clock Difference Diagram Library

Publications:

**"Clock Difference Diagrams"** by *Kim Guldstrand Larsen, Justin Pearson, Carsten Weise and Wang Yi*. "Nordic Journal of Computing", 1999. [[pdf](https://vbn.aau.dk/ws/files/425046823/CDD_26pages_nordic_journal_of_computing_1999.pdf)] [[bib](https://dblp.uni-trier.de/rec/journals/njc/LarsenPWY99.html?view=bibtex)]

**"Efficient Timed Reachability Analysis Using Clock Difference Diagrams"** by *Gerd Behrmann, Kim Guldstrand Larsen, Justin Pearson, Carsten Weise and Wang Yi*. "Computer Aided Verification, 11th International Conference, CAV'99", Trento, Italy, July 6-10, 1999, Proceedings. [[doi:10.1007/3-540-48683-6_30](https://doi.org/10.1007/3-540-48683-6_30)] [[bib](https://dblp.uni-trier.de/rec/conf/cav/BehrmannLPWY99.html?view=bibtex)]


## Compile
The following packages need to be installed: `cmake gcc xxHash doctest boost`.
Additionally, you need [UUtils](https://github.com/UPPAALModelChecker/UUtils) and [UDBM](https://github.com/UPPAALModelChecker/UDBM), which you can install system-wide, or locally by using the script `getlibs.sh`.

Get the dependencies, compile the source into `build` directory and run the unit tests:
```sh
./getlibs.sh # If UUtils and UDBM are not installed system-wide
cmake -B build -DCMAKE_BUILD_TYPE=Release -DTESTING=ON
cmake --build build
(cd build ; ctest --output-on-failure)
```
