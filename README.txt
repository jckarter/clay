Clay 0.2-WIP
------------

Clay is a programming language designed for Generic Programming.

Visit http://claylabs.com/clay for more information.

This is a work-in-progress branch. Documentation may be out of date, and
source code compatibility may break at any time. The latest release branch
is `v0.1`; for normal use you will want to switch to that branch:

    git checkout v0.1

Pre-requisites
--------------

Clay requires CMake version 2.6 or later, and LLVM and Clang 3.1. Note that
LLVM versions are not backward-compatible so developer versions of LLVM and/or
later releases are not compatible with this version of Clay.

CMake Configuration
-------------------

Clay uses CMake to configure and control its build system. The build can
be customized by passing cmake arguments of the form
"-D<variable>=<value>" to set CMake variables. In addition to standard
CMake variables such as CMAKE_INSTALL_PREFIX and CMAKE_BUILD_TYPE, Clay's
build system uses the following variables:

* LLVM_DIR can be set to the install prefix of an LLVM 3.1 installation.
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

    clay ../examples/hello.clay

The generated binary

    ./hello

Build Clay on Windows with Visual C++
-------------------------------------

I use Visual C++ Express 2010 to build LLVM and Clay for Windows.
From a Visual Studio command prompt, build LLVM and Clang using cmake
and the MSVC compiler. There are some issues with Debug builds and
LLVM, so both LLVM and Clay will need to be built as Release. The
default LLVM install directory needs Administrator permissions, so
you may want to set a CMAKE_INSTALL_PREFIX as well. to change it.
Place the Clang repository in llvm-3.1/tools/clang so that LLVM builds
it automatically and compile LLVM with the following commands:

    mkdir build
    cd build
    cmake -G "NMake Makefiles" -DCMAKE_INSTALL_PREFIX=c:\llvm31-install -DCMAKE_BUILD_TYPE=Release ..
    nmake install

Then to build Clay, run CMake and generate nmake makefiles:

    mkdir build
    cd build
    cmake -G "NMake Makefiles" -DLLVM_DIR=c:\llvm31-install -DCMAKE_BUILD_TYPE=Release ..
    nmake

Building under Cygwin or Mingw, or with Visual C++ using cmake's
Visual Studio Solution generator, may also work, but I haven't tried
it.

