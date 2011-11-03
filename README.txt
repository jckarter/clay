##########

This fork of the Clay language is used to develop and test language additions
to aid development of scientific and numerical packages. The intention is to
exploit the conciseness, type genericity, and performance of Clay for user
friendly light-weight scientific programming.

So far additions to the compiler & std lib include:


80-bit floating point support
-----------------------------
Float80 (Real) type added to core language. Uses f80 suffix.
Added a compiler flag (-f80) which compiles double literals as f80.
This works well on my intel i5 running ArchLinux, but probably needs further
work to be a complete language feature.


Complex number support
----------------------
Complex numbers support in the compiler including imaginary literals (j suffix).
All floating point types are supported as complex components. Basic complex
number functionality is provided in the accompanying library which is imported
in prelude.
This works well as is, however some attention is probably needed to tidy-up
the expression of literals and need to add real/imag fieldRef to compiler.


Math library
------------
Overloaded math functions from libm supporting upto 80-bit floating point.
Covered just about everything in libm/complex.


Upcoming Clay libraries:
    - linalg: Supporting dense/sparse matrices/vectors and many functions
    based on the functionality of the Meschach c library.

    - numerics: Numerical routines and wrappers e.g. calculus, etc.

    - stats: Basic statistcal routines either native or c wrappers.

    - modelling codes: Some scientific/math modelling routines e.g. inverse
    laplace, etc.

See seperate project pages on Github for code and progress.



##########


Clay
----

Clay is a programming language designed for Generic Programming.

Visit http://claylabs.com/clay for more information.


Pre-requisites
--------------

Clay requires LLVM 2.9 and the Clang source to build. The Clang source
is currently used by the clay-bindgen tool which generates Clay interfaces
by parsing C header files; unfortunately it predates libclang, so libclang
by itself is not sufficient. The Clay build system needs CMake. Make sure
you have CMake version 2.6 or later.

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
