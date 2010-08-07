import os
import glob
import pickle
import sys
from subprocess import Popen, PIPE
from multiprocessing import Pool, cpu_count
import time



#
# testRoot
#

testRoot = os.path.dirname(os.path.abspath(__file__))



#
# getCompilerPath
#

def getCompilerPath() :
    buildPath = ["..", "build", "compiler", "src"]
    if sys.platform == "win32" :
        compiler = os.path.join(testRoot, *(buildPath + ["Release", "clay.exe"]))
        compiler2 = os.path.join(testRoot, "..", "clay.exe") # for binary distributions
    else :
        compiler = os.path.join(testRoot, *(buildPath + ["clay"]))
        compiler2 = os.path.join(testRoot, "..", "clay") # for binary distributions

    if not os.path.exists(compiler) :
        compiler = compiler2
        if not os.path.exists(compiler) :
            print "could not find the clay compiler"
            sys.exit(-1)
    return compiler

compiler = getCompilerPath()



#
# TestCase
#

class TestCase(object):
    allCases = []
    def __init__(self, folder, base = None):
        entries = os.listdir(folder)
        mainPath = os.path.join(folder, "main.clay")
        self.path = folder
        if os.path.isfile(mainPath):
            self.loadTest(mainPath)
        for entry in entries:
            fullpath = os.path.join(folder, entry)
            if not os.path.isdir(fullpath):
                continue
            TestCase(fullpath, self)
    
    def loadTest(self, testfile):
        TestCase.allCases.append(self)
        self.testfile = testfile

    def cmdline(self, clay) :
        return [clay, "main.clay"]

    def pre_build(self) :
        pass

    def post_build(self) :
        pass

    def post_run(self) :
        [os.unlink(f) for f in glob.glob("temp*")]
        [os.unlink(f) for f in glob.glob("*.data")]

    def match(self, output) :
        refoutput = open("out.txt").read()
        output = output.replace('\r', '')
        refoutput = refoutput.replace('\r', '')
        return output == refoutput

    def run(self):
        os.chdir(self.path)
        self.pre_build()
        self.post_build()
        result = self.runtest()
        self.post_run()
        if self.match(result):
            return "ok"
        return "fail"

    def name(self):
        return os.path.relpath(self.path, testRoot)

    def runtest(self):
        outfilename = "a.exe" if sys.platform == "win32" else "a.out"
        outfilename = os.path.join(".", outfilename)
        process = Popen(self.cmdline(compiler))
        if process.wait() != 0 :
            return "fail"
        process = Popen([outfilename], stdout=PIPE)
        result = process.stdout.read()
        process.wait()
        self.removefile(outfilename)
        return result

    def removefile(self, filename) :
        # on windows, sometimes, deleting a file
        # immediately after executing it 
        # results in a 'access denied' error.
        # so we wait and try again a few times.
        attempts = 1
        while (attempts <= 3) and os.path.exists(filename) :
            try :
                os.unlink(filename)
            except OSError :
                time.sleep(1)
            attempts += 1



#
# runtests
#

def findTestCases():
    TestCase(testRoot)
    return TestCase.allCases

def runtest(t):
    return t.run()

def runtests() :
    testcases = findTestCases()
    pool = Pool(processes = cpu_count())
    results = pool.imap(runtest, testcases)
    failed = []
    for test in testcases:
        res = results.next()
        print "TEST %s: %s" % (test.name(), res)
        if res == "fail":
            failed.append(test.name())
    if len(failed) == 0:
        print "\nPASSED ALL %d TESTS" % len(testcases)
    else:
        print "\nFailed tests:" 
        print "\n".join(failed)
        print "\nFAILED %d OF %d TESTS" % (len(failed), len(testcases)) 

if __name__ == "__main__":
    startTime = time.time()
    runtests()
    endTime = time.time()
    print ""
    print "time taken = %f seconds" % (endTime - startTime)
