export ACLOCAL_PATH=/usr/share/aclocal
cmake -G "Unix Makefiles" -DCMAKE_VERBOSE_MAKEFILE=off -DCMAKE_INSTALL_PREFIX=build/root_fs -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug -DCMAKE_COLOR_DIAGNOSTICS=ON -DCMAKE_COLOR_MAKEFILE=ON -DCMAKE_C_FLAGS="-g3 -O0" -DCMAKE_CXX_FLAGS="-g3 -O0" -S . -B build &&
cmake --build build &&
cmake --install build
