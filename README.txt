
Clay
----

Clay is a programming language designed for Generic Programming.

Visit http://tachyon.in/clay for more information.


Pre-requisites
--------------

Clay requires LLVM and Clang to build. Clang is used by the
clay-bindgen tool which generates Clay interfaces by parsing C header
files. I recommend tracking LLVM SVN as that's what we do. The Clay
build system needs CMake. Make sure you have CMake version 2.6 or
later.

WARNING: LLVM builds in debug mode by default. This makes the
Clay compiler crawl. Configure LLVM with "--enable-optimized" for good
performance.

Here are the commands for building LLVM and Clang. These steps result
in the LLVM repo installed as $HOME/code/llvm, and the binaries are
installed under /usr/local/.

    mkdir -p $HOME/code
    cd $HOME/code
    svn co http://llvm.org/svn/llvm-project/llvm/trunk llvm
    cd llvm/tools
    svn co http://llvm.org/svn/llvm-project/cfe/trunk clang
    cd ..
    ./configure --enable-targets=host-only --enable-optimized
    make
    make install


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
