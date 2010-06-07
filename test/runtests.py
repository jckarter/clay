import os
import sys
from subprocess import Popen, PIPE
from multiprocessing import Pool, cpu_count

testDir = os.path.dirname(os.path.abspath(__file__))
compiler = os.path.join(testDir, "..", "build", "compiler", "src", "clay")

def runtest(path):
    outfilename = "a.exe" if sys.platform == "win32" else "a.out"
    outfilename = os.path.join(path, outfilename)
    process = Popen([compiler, "main.clay"], 
            stdout=PIPE, stderr=PIPE, cwd=path)
    if process.wait() != 0 :
        return "fail"
    process = Popen([outfilename], cwd=path, stdout=PIPE)
    process.wait()
    refresult = open(os.path.join(path, "out.txt")).read()
    os.unlink(outfilename)
    if refresult != process.stdout.read():
        return "fail"
    return "ok"

def cantest(filenames):
    if "main.clay" not in filenames:
        return False
    if "out.txt" in filenames or "err.txt" in filenames:
        return True
    return False

def runtests() :
    if not os.path.exists(compiler):
        print "Could not find the clay compiler"
        return
    paths = []
    for dirpath, dirnames, filenames in os.walk(testDir):
        if cantest(filenames):
            paths.append(dirpath)
    pool = Pool(processes = cpu_count())
    results = pool.imap(runtest, 
            [os.path.join(testDir, path) for path in paths])
    okCount = 0
    failed = []
    for path in paths:
        res = results.next()
        p = os.path.relpath(path, testDir)
        print "TEST %s: %s" % (p, res)
        if res == "fail":
            failed.append(p)
    if len(failed) == 0:
        print "\nALL TESTS PASSED"
    else:
        print "\nFailed tests:" 
        print "\n".join(failed)
        print "\nFAILED %d OF %d TESTS" % (len(failed), len(paths)) 

if __name__ == "__main__":
    runtests()
