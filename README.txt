
Clay
----

Clay is a programming language designed for Generic Programming.

Visit http://claylabs.com/clay for more information.


Pre-requisites
--------------

Clay requires LLVM 3.0 and CMake version 2.6 or later.

NOTE: The version of libclang included in clang 3.0 is missing
some features necessary to build the bindgen tool. To build bindgen,
a patch against the clang 3.0 source is provided in tools/libclang.diff.

Build Clay on Unix-like Systems
-------------------------------

To build Clay, first run cmake to generate the Makefiles and then run make.
CMake can also build project files for IDEs such as Xcode. Look at CMake 
documentation for how to build those. It is recommended that Clay be built 
in a separate build directory.

    mkdir build
    cd build
    cmake -G "Unix Makefiles" ../
    make

The default installation directory for Clay is /usr/local. To change 
the installation path, pass -DCMAKE_INSTALL_PREFIX=/my/path to cmake.

To install Clay run

    make install

To run the test suite

    make test

To compile a clay source file run

    clay ../test/hello.clay

The generated binary

    ./hello

Build Clay on Windows with Visual C++
-------------------------------------

I use Visual C++ Express 2010 to build LLVM and Clay for Windows.
From a Visual Studio command prompt, build LLVM and Clang using cmake
and the MSVC compiler. Then to build Clay, run CMake and generate
nmake makefiles:

    mkdir build
    cd build
    cmake -G "NMake Makefiles" -DLLVM_DIR=<path to LLVM> ..
    nmake

Building to target Cygwin or Mingw, or with Visual C++ using cmake's
Visual Studio Solution generator, may also work, but I haven't tried
it.

