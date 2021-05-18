from subprocess import check_call, CalledProcessError
import fnmatch
import sys
import os
import os.path
import shutil
import stat

# create def call_or_false(argv, expectedcode=0):
def call_or_false(argv, expectedcode=0):
    try:
        check_call(argv)
        return True
    except OSError as ex:
        print "--- No!", ex
        return False
    except CalledProcessError as ex:
        if (ex.returncode != expectedcode):
            print "--- No! Error code", ex.returncode
            sys.exit(1)
        else:
            return True

# create def call_or_die(argv, expectedcode=0):
def call_or_die(argv, expectedcode=0):
    if not call_or_false(argv, expectedcode):
        sys.exit(1)

# create def cleanup(f):
def cleanup(f):
    try:
        os.remove(f)
    except OSError:
        pass

print "--- Is the compiler on the path and runnable?"
call_or_die(["clay"], 2)
print "--- Does it respond to `--help`?"
call_or_die(["clay", "--help"], 2)
print "--- Does it respond to `/?`?"
call_or_die(["clay", "/?"], 2)
print "--- Can it compile `examples/hello.clay`?"
call_or_die(["clay", "examples/hello.clay"])
try:
    print "--- Is the result executable?"
    call_or_die(["./hello"])
finally:
    cleanup("hello")
    cleanup("hello.exe")

print "--- Is clay-fix on the path and runnable?"
if call_or_false(["clay-fix"], 2):
    print "--- Does it respond to `--help`?"
    call_or_die(["clay-fix", "--help"], 2)
    print "--- Does it respond to `/?`?"
    call_or_die(["clay-fix", "--help"], 2)
    print "--- Creating a temporary v0.0 clone..."
    git = "git"
    if sys.platform == "win32":
        git = "git.cmd"
    call_or_die([git, "clone", ".", "tempv0.0", "-b", "v0.0"])
    try:
        print "--- How much clay can clay-fix fix?"
        libfiles = []
        for root, dirnames, filenames in os.walk('tempv0.0/lib-clay'):
            for filename in fnmatch.filter(filenames, '*.clay'):
                libfiles.append(os.path.join(root, filename))
        call_or_die(["clay-fix", "-v", "0.0"] + libfiles)
    finally:
        print "--- Cleaning up temporary v0.0 clone..."

        def onerror(f, p, x):
            if not os.access(p, os.W_OK):
                os.chmod(p, stat.S_IWUSR)
                f(p)
            else:
                raise
        shutil.rmtree("tempv0.0", onerror=onerror)
else:
    print "--- clay-fix not found; I'll assume it wasn't installed"

print "--- Is clay-bindgen on the path and runnable?"
if call_or_false(["clay-bindgen"], 2):
    print "--- Does it respond to `--help`?"
    call_or_die(["clay-bindgen", "--help"], 2)
    print "--- Does it respond to `/?`?"
    call_or_die(["clay-bindgen", "--help"], 2)
    print "--- Making a test C header file..."
    temph = open("temp-stdio.h", "w")
    try:
        temph.write("#include <stdio.h>\n")
        temph.close()
        print "--- Can clay-bindgen generate bindings for `stdio.h`?"
        call_or_die(["clay-bindgen", "temp-stdio.h"])
    finally:
        cleanup("temp-stdio.h")
else:
    print "--- clay-bindgen not found; I'll assume it wasn't installed"
