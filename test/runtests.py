import sys
import os
import tempfile
import time
import cStringIO
from subprocess import Popen, PIPE
from multiprocessing import Pool, cpu_count

tests = ("factorial platform " +
         "test1 test2 test3 test4 test5 test6 test7 test8 test9 test10 " +
         "test11 test12 test13 test14 test15 test16 test17 " +
         "test18 test19 test20 test21 test22 test23 exception " +
         "test24 test25 test26 test27 test28 test29 test30 " +
         "test31 test32 test33 test34 test35 test36 test37 " +
         "test38 test39 test40 test41 test42").split()

perfTests = ("insertionsort1 insertionsort2 " +
             "mean quicksort1 quicksort2").split()

win32Tests = "beep".split()

testDir = os.path.dirname(__file__)
compiler = os.path.join("compiler", "src", "clay")

def runtest(clayfile):
    outstream = cStringIO.StringIO()
    print >>outstream, "TEST:", clayfile
    print >>outstream, compiler, clayfile
    outfile = tempfile.NamedTemporaryFile(delete = False)
    outfile.close()
    process = Popen([compiler, "-o", outfile.name, clayfile], 
            stdout=PIPE, stderr=PIPE)
    if process.wait() != 0 :
        print >>outstream, "Compilation failed"
        return outstream.getvalue()
    print >>outstream, process.stdout.read(),
    process = Popen([outfile.name], stdout=PIPE).stdout
    print >>outstream, process.read(),
    os.unlink(outfile.name)
    return outstream.getvalue()

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
    pool = Pool(processes = cpu_count())
    results = pool.imap(runtest, 
            [os.path.join(testDir, path) for path in paths])
    for i in range(len(paths)):
        print results.next(),

runtests()
