from subprocess import check_call, CalledProcessError
from sys import argv
import os

try:
    check_call(["gcc", "-c", "-o", "temp-external_test.o", "external_test.c"])
    check_call(["clang", "-o", "temp.exe", "temp-external_test.o", argv[1]])
    check_call(["./temp.exe"])
except CalledProcessError as ex:
    print "!! error code", ex.returncode

