# UPPAAL CDD Library
## Build 
The following packages need to be installed: `cmake gcc xxHash doctest boost`.
Additionally, you need [UUtils](https://github.com/UPPAALModelChecker/UUtils) and [UDBM](https://github.com/UPPAALModelChecker/UDBM), which you can install system-wide, or locally by using the script `getlibs.sh`.

Compile source:
```sh
./getlibs.sh # If UUtils and UDBM are not installed system-wide
cmake -B build/ -DCMAKE_BUILD_TYPE=Release
cmake --build build/
```
