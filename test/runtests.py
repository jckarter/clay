import sys
import os

tests = ("factorial platform " +
         "test1 test2 test3 test4 test5 test6 test7 test8 test9 test10 " +
         "test11 test12 test13 test14 test15 test16 test17 " +
         "test18 test19 test20 test21 test22 test23 exception " +
         "test24 test25 test26 test27 test28 test29 test30 " +
         "test31 test32 test33 test34 test35 test36 test37 " +
         "test38 test39 test40").split()

perfTests = ("insertionsort1 insertionsort2 " +
             "mean quicksort1 quicksort2").split()

win32Tests = "beep".split()

testDir = os.path.dirname(__file__)
compiler = os.path.join(testDir, "..", "bin", "clay")

def runtest(input) :
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
    paths = []
    for x in tests :
        p = x + ".clay"
        paths.append(p)
    if sys.platform == "win32" :
        for x in win32Tests :
            p = os.path.join("win32", x) + ".clay"
            paths.append(p)
    for x in perfTests :
        p = os.path.join("perf", x) + ".clay"
        paths.append(p)
    for p in paths :
        if not runtest(os.path.join(testDir, p)) :
            print "Test Failed"

runtests()
