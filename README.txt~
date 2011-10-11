
Clay
----

Clay is a programming language designed for Generic Programming.

Visit http://claylabs.com/clay for more information.


Pre-requisites
--------------

Clay requires LLVM 2.9 and Clang to build. Clang is used by the
clay-bindgen tool which generates Clay interfaces by parsing C header
files. The Clay build system needs CMake. Make sure you have CMake
version 2.6 or later.

NOTE: LLVM builds in debug mode by default. This makes the
Clay compiler crawl. If you're building LLVM from source, configure it
with "./configure --enable-optimized" for good performance.


Build Clay
----------

To build Clay, first run cmake to generate the Makefiles and then run make.
CMake can also build project files for IDE's such as Xcode. Look at CMake 
documentation for how to build those. It is recommended that Clay be built 
in a separate build directory.

    mkdir build
    cd build
    cmake -G "Unix Makefiles" ../
    make

The default installation directory for Clay will is /usr/local. To change 
the installtion path, pass -DCMAKE_INSTALL_PREFIX=/my/path to cmake.

To install Clay run 
    
    make install

To run the test suite

    make test

To compile a clay source file run

    clay ../test/hello.clay

The generated binary

    ./a.out
