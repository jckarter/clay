from subprocess import check_call, CalledProcessError
from sys import argv, platform
import os

def runExternalTest(extraFlags=[]):
    clayobj = argv[1]
    buildFlags = argv[2:]

    linkFlags = []
    if platform == 'linux' or platform == 'linux2':
        linkFlags += ['-lm']

    NULL = open(os.devnull, 'w')

    try:
        if platform == 'win32':
            os.rename(clayobj, "temp-main.obj")
            check_call(["cl", "/nologo",
                "/Fetemp.exe", "/Fotemp-external_test.obj", "external_test.cpp",
                "temp-main.obj"], stdout=NULL)
        else:
            os.rename(clayobj, "temp-main.o")
            check_call(["clang++", "-c", "-o", "temp-external_test.o",
                "external_test.cpp"] + buildFlags + extraFlags)
            check_call(["clang++", "-o", "temp.exe", "temp-external_test.o",
                "temp-main.o"] + linkFlags + buildFlags + extraFlags)
        check_call(["./temp.exe"])
    except CalledProcessError as ex:
        print "!! error code", ex.returncode

