
Clay
----

Clay is a programming language designed for Generic Programming.

Visit http://claylanguage.org for more information.


Pre-requisites
--------------

Clay requires LLVM to build. I recommend tracking LLVM SVN as that's
what we do.

WARNING: LLVM builds in debug mode by default. This makes the
Clay compiler crawl. Configure LLVM with "--enable-optimized" for good
performance.


Build Clay
----------

To build Clay, run 'make'

    $ make

The compiler binary is placed in the 'bin' directory. If you want, you
can add the 'bin' directory to PATH for convenience.

    $ bin/clay test/hello.clay

The generated binary.

    $ ./a.out
