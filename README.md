downloading:
- `git clone --recursive thisGit`

setting up:
- cd into the cloned dir
- `cd external/seastar`
- `sudo ./install-dependencies.sh`
- install doxygen (required by seastar, even tho documentation doesnt need to be built)
- `cd dpdk`
- `git apply ../../../*.patch` (fixes the dpdk build process if your path contains spaces)
- `cd ..`
- `./configure.py --mode=release --enable-dpdk --c++-standard 23 --compiler g++ --c-compiler gcc` (cmake breaks trying to compile seastar)
- `ninja -C build/release` (build seastar)

you can now open the project in clion, a profile is already set up.

or, to generate externally:
- `mkdir build`
- `cmake -DCMAKE_CXX_COMPILER="g++" -DCMAKE_C_COMPILER="gcc" -DCMAKE_PREFIX_PATH="$(pwd)/external/seastar/build/release;$(pwd)/external/seastar/build/release/_cooking/installed" -DCMAKE_MODULE_PATH="$(pwd)/external/seastar/cmake" -GNinja -DCMAKE_EXPORT_COMPILE_COMMANDS="TRUE" -B build`
- build contains cmake files

build externally:
- `cd build`
- `ninja`