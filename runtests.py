import sys
import os

tests = ("factorial platform " +
         "test1 test2 test3 test4 test5 test6 test7 test8 test9 test10 " +
         "test11 test12 test13 test14 test15 test16 test17 " +
         "test18 test19").split()

perfTests = ("insertionsort1 insertionsort2 insertionsort3 " +
             "mean quicksort1 quicksort2").split()

def runtest(input) :
    compiler = os.path.join(".", "clay")
    print "TEST:", input
    command = compiler + " " + input
    print command
    result = os.system(command)
    if result != 0 :
        print "Compilation failed"
        return False
    output = "a.exe" if sys.platform == "win32" else "a.out"
    os.system(os.path.join(".", output))
    return True

def runtests() :
    for x in tests :
        input = os.path.join("test", x) + ".clay"
        if not runtest(input) :
            print "Test Failed"
    for x in perfTests :
        input = os.path.join("test", "perf", x) + ".clay"
        if not runtest(input) :
            print "Test Failed"

runtests()
