from subprocess import check_call, CalledProcessError
from sys import argv, platform
import os

clayobj = argv[1]
buildFlags = argv[2:]

if platform == 'linux':
    buildFlags += ['-lm']

try:
    check_call(["clang", "-c", "-o", "temp-external_test.o", "external_test.c"] + buildFlags)
    check_call(["clang", "-o", "temp.exe", "temp-external_test.o", clayobj] + buildFlags)
    check_call(["./temp.exe"])
except CalledProcessError as ex:
    print "!! error code", ex.returncode

