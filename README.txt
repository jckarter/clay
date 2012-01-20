
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


CMake Configuration
-------------------

Clay uses CMake to configure and control its build system. The build can
be customized by passing cmake arguments of the form
"-D<variable>=<value>" to set CMake variables. In addition to standard
CMake variables such as CMAKE_INSTALL_PREFIX and CMAKE_BUILD_TYPE, Clay's
build system uses the following variables:

* LLVM_DIR can be set to the install prefix of an LLVM 3.0 installation.
  If not set, CMake will look for an 'llvm-config' script on the PATH.
* PYTHON_EXECUTABLE can be set to the path of a Python 2.x interpreter.
  Clay uses a Python 2 script to drive its test suite. Some platforms
  (notably Arch Linux) use Python 3 as their 'python' executable, and
  this will confuse CMake. Pointing PYTHON_EXECUTABLE at 'python2' may
  be necessary on these platforms.


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
Vanilla LLVM 3.0 unfortunately has a number of bugs when targeting
Visual Studio; I have fixes for many of these bugs in Github
mirrors of LLVM and Clang 3.0. If you're interested in using Clay
on Windows, you should use these patched versions:

    https://github.com/jckarter/llvm-3.0
    https://github.com/jckarter/clang-3.0

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

