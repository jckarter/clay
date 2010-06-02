
Clay
----

Clay is a programming language designed for Generic Programming.

Visit http://claylanguage.org for more information.


Pre-requisites
--------------

Clay requires LLVM to build. I recommend tracking LLVM SVN as that's
what we do. The Clay build system needs CMake. Make sure you have CMake
version 2.6 or later.

WARNING: LLVM builds in debug mode by default. This makes the
Clay compiler crawl. Configure LLVM with "--enable-optimized" for good
performance.


Build Clay
----------

To build Clay, first run cmake to generate the Makefiles and then run make.
CMake can also build project files for IDE's such as Xcode. Look at CMake 
documentation for how to build those. It is recommended that Clay be built 
in a separate build directory.

    $ mkdir build
    $ cd build
    $ cmake -G "Unix Makefiles" ../
    $ make

The default installation directory for Clay will is /usr/local. To change 
the installtion path, pass -DCMAKE_INSTALL_PREFIX=/my/path to cmake.

To install Clay run 
    
    $ make install

To run the test suite

    $ make test

To compile a clay source file run

    $ clay test/hello.clay

The generated binary

    $ ./a.out
