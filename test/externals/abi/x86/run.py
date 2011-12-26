from subprocess import check_call, CalledProcessError
from sys import argv, platform
import os

clayobj = argv[1]
buildFlags = argv[2:]

linkFlags = [];
if platform == 'linux' or platform == 'linux2':
    linkFlags += ['-lm']

try:
    check_call(["clang", "-c", "-o", "temp-external_test_x86.o", "external_test_x86.c"] + buildFlags)
    check_call(["clang", "-o", "temp.exe", "temp-external_test_x86.o", clayobj] + linkFlags)
    check_call(["./temp.exe"])
except CalledProcessError as ex:
    print "!! error code", ex.returncode

