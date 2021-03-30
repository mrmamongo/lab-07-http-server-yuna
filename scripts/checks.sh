#!/usr/bin/env bash
cd /home \
    && wget http://downloads.sourceforge.net/project/boost/boost/1.68.0/boost_1_68_0.tar.gz \
    && tar xfz boost_1_68_0.tar.gz \
    && rm boost_1_68_0.tar.gz \
    && cd boost_1_68_0 \
    && ./bootstrap.sh --with-libraries=system \
    && ./b2 install
set -e

declare -r FILTER=-build/c++11,-runtime/references,\
-whitespace/braces,-whitespace/indent,-whitespace/comments,-build/include_order

find ./include/ ./scripts/ ./sources/ -name "*.cpp" -or -name "*.hpp" -or -name ".h" | xargs cpplint --filter=$FILTER

export CTEST_OUTPUT_ON_FAILURE=true
# address sanitizer
#CMAKE_LINKER_OPTS="-DCMAKE_EXE_LINKER='-fuse-ld=gold'"
CMAKE_CONFIG_OPTS="-DHUNTER_CONFIGURATION_TYPES=Debug -DCMAKE_BUILD_TYPE=Debug"
CMAKE_TOOLCHAIN_OPTS="-DCMAKE_TOOLCHAIN_FILE='$(pwd)/tools/polly/sanitize-address-cxx17-pic.cmake'"
CMAKE_OPTS="$CMAKE_LINKER_OPTS $CMAKE_CONFIG_OPTS $CMAKE_TOOLCHAIN_OPTS"
cmake -H. -B_builds/sanitize-address-cxx17 $CMAKE_OPTS
cmake --build _builds/sanitize-address-cxx17
./_builds/sanitize-address-cxx17/tests
# thread sanitizer
CMAKE_TOOLCHAIN_OPTS="-DCMAKE_TOOLCHAIN_FILE='$(pwd)/tools/polly/sanitize-thread-cxx17-pic.cmake'"
CMAKE_OPTS="$CMAKE_LINKER_OPTS $CMAKE_CONFIG_OPTS $CMAKE_TOOLCHAIN_OPTS"
cmake -H. -B_builds/sanitize-thread-cxx17 $CMAKE_OPTS
cmake --build _builds/sanitize-thread-cxx17
./_builds/sanitize-thread-cxx17/tests
