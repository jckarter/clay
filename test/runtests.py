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
# getClayPlatform, fileForPlatform
#

def getClayPlatform():
    if sys.platform == "win32" or sys.platform == "cygwin":
        return "windows"
    if sys.platform == "darwin":
        return "macosx"
    if sys.platform.startswith("linux"):
        return "linux"

clayPlatform = getClayPlatform()

def fileForPlatform(folder, name, ext):
    platformName = os.path.join(folder, "%s.%s.%s" % (name, clayPlatform, ext))
    if os.path.isfile(platformName):
        return platformName
    else:
        return os.path.join(folder, "%s.%s" % (name, ext))


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
    def __init__(self, folder, testfile, base = None):
        entries = os.listdir(folder)
        self.testfile = os.path.basename(testfile)
        self.path = folder
        if os.path.isfile(testfile):
            self.loadTest(testfile)

        runscript = fileForPlatform(folder, "run", "py")
        if os.path.isfile(runscript):
            self.runscript = runscript
        else:
            self.runscript = None

        buildflags = fileForPlatform(folder, "buildflags", "txt")
        if os.path.isfile(buildflags):
            self.buildflags = open(buildflags).read().split()
        else:
            self.buildflags = []

        for entry in entries:
            fullpath = os.path.join(folder, entry)
            if not os.path.isdir(fullpath):
                continue
            findTestCase(fullpath, self)
    
    def loadTest(self, testfile):
        TestCase.allCases.append(self)
        self.testfile = testfile

    def cmdline(self, clay) :
        return [clay, "-I" + self.path] + self.buildflags + [self.testfile]

    def pre_build(self) :
        pass

    def post_build(self) :
        pass

    def post_run(self) :
        [os.unlink(f) for f in glob.glob("temp*")]
        [os.unlink(f) for f in glob.glob("*.data")]

    def match(self, resultout, resulterr, returncode) :
        outfile = fileForPlatform(".", "out", "txt")
        errfile = fileForPlatform(".", "err", "txt")
        if not os.path.isfile(outfile) :
            return False
        refout = open(outfile).read()
        referr = ""
        if os.path.isfile(errfile):
            referr = open(errfile).read()
        resultout = resultout.replace('\r', '')
        refout    = refout.replace('\r', '')
        resulterr = resulterr.replace('\r', '')
        referr    = referr.replace('\r', '')
        return resultout == refout and resulterr == referr

    def run(self):
        os.chdir(self.path)
        self.pre_build()
        self.post_build()
        resultout, resulterr, returncode = self.runtest()
        self.post_run()
        if self.match(resultout, resulterr, returncode):
            return "ok"
        return "fail"

    def name(self):
        return os.path.relpath(self.path, testRoot)

    def runtest(self):
        outfilename = "a.exe" if sys.platform == "win32" else "a.out"
        outfilename = os.path.join(".", outfilename)
        process = Popen(self.cmdline(compiler))
        if process.wait() != 0 :
            return "fail", "", None
        if self.runscript is None:
            commandline = [outfilename]
        else:
            commandline = [sys.executable, self.runscript, outfilename]

        process = Popen(commandline, stdout=PIPE, stderr=PIPE)
        resultout, resulterr = process.communicate()
        self.removefile(outfilename)
        return resultout, resulterr, process.returncode

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

class TestModuleCase(TestCase):
    def match(self, resultout, resulterr, returncode) :
        return returncode == 0
        

#
# runtests
#

def findTestCase(folder, base = None):
    testPath = fileForPlatform(folder, "test", "clay")
    mainPath = fileForPlatform(folder, "main", "clay")
    if os.path.isfile(testPath) :
        TestModuleCase(folder, testPath, base)
    else :
        TestCase(folder, mainPath, base)

def findTestCases():
    findTestCase(testRoot)
    return TestCase.allCases

def runTest(t):
    return t.run()

def runTests() :
    testcases = findTestCases()
    pool = Pool(processes = cpu_count())
    results = pool.imap(runTest, testcases)
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


def main() :
    global testRoot
    if len(sys.argv) > 1 :
        testRoot = os.path.join(testRoot, *sys.argv[1:])
    startTime = time.time()
    runTests()
    endTime = time.time()
    print ""
    print "time taken = %f seconds" % (endTime - startTime)


if __name__ == "__main__":
    main()
