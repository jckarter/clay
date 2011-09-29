import re
import os
import glob
import pickle
import platform
import signal
import sys
from subprocess import Popen, PIPE
from multiprocessing import Pool, cpu_count
import time



#
# testRoot
#

testRoot = os.path.dirname(os.path.abspath(__file__))
runTestRoot = testRoot



#
# getClayPlatform, fileForPlatform
#

def getClayPlatform():
    if sys.platform == "win32" or sys.platform == "cygwin":
        return "windows"
    if sys.platform == "darwin":
        return "macosx"
    if sys.platform.startswith("freebsd"):
        return "freebsd"
    if sys.platform.startswith("linux"):
        return "linux"

def getClayBits():
    if platform.architecture()[0] == "64bit":
        return "64"
    if platform.architecture()[0] == "32bit":
        return "32"

clayPlatform = getClayPlatform()
clayBits = getClayBits()

def fileForPlatform(folder, name, ext):
    platformName = os.path.join(folder, "%s.%s.%s.%s" % (name, clayPlatform, clayBits, ext))
    if os.path.isfile(platformName):
        return platformName
    platformName = os.path.join(folder, "%s.%s.%s" % (name, clayPlatform, ext))
    if os.path.isfile(platformName):
        return platformName
    platformName = os.path.join(folder, "%s.%s.%s" % (name, clayBits, ext))
    if os.path.isfile(platformName):
        return platformName
    return os.path.join(folder, "%s.%s" % (name, ext))


#
# getCompilerPath
#

def getCompilerPath() :
    buildPath = ["..", "compiler"]
    if sys.platform == "win32" :
        compiler = os.path.join(testRoot, *(buildPath + ["clay.exe"]))
    else :
        compiler = os.path.join(testRoot, *(buildPath + ["clay"]))

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
        libClay = os.path.join(testRoot, "..", "lib-clay")
        libContrib = os.path.join(testRoot, "..", "lib-contrib")
        return [clay, "-I" + self.path, "-I" + libClay, "-I" + libContrib] + self.buildflags + [self.testfile]

    def pre_build(self) :
        pass

    def post_build(self) :
        pass

    def post_run(self) :
        [os.unlink(f) for f in glob.glob("temp*")]
        [os.unlink(f) for f in glob.glob("*.data")]

    def match(self, resultout, resulterr, returncode) :
        compilererrfile = fileForPlatform(".", "compilererr", "txt")
        if os.path.isfile(compilererrfile):
            if returncode != "compiler error":
                return "compiler did not fail"
            errpattern = open(compilererrfile).read()
            errpattern = errpattern.replace('\r', '').strip()

            if re.search(errpattern, resulterr):
                return "ok"
            else:
                return "unexpected compiler error"

        else:
            if returncode == "compiler error":
                return "compiler error"
            outfile = fileForPlatform(".", "out", "txt")
            errfile = fileForPlatform(".", "err", "txt")
            if not os.path.isfile(outfile) :
                return "out.txt missing"
            refout = open(outfile).read()
            referr = ""
            if os.path.isfile(errfile):
                referr = open(errfile).read()
            resultout = resultout.replace('\r', '')
            refout    = refout.replace('\r', '')
            resulterr = resulterr.replace('\r', '')
            referr    = referr.replace('\r', '')
            if resultout == refout and resulterr == referr:
                return "ok"
            elif resultout != refout and resulterr != referr:
                return "out.txt and err.txt mismatch"
            elif resultout != refout:
                return "out.txt mismatch"
            elif resulterr != referr:
                return "err.txt mismatch"

    def run(self):
        os.chdir(self.path)
        self.pre_build()
        self.post_build()
        resultout, resulterr, returncode = self.runtest()
        self.post_run()
        return self.match(resultout, resulterr, returncode)

    def name(self):
        return os.path.relpath(self.path, testRoot)

    def runtest(self):
        testbasename = os.path.basename(self.testfile)
        outfilenameext = ".exe" if sys.platform == "win32" else ""
        outfilename = testbasename.replace('.clay', outfilenameext)
        outfilename = os.path.join(".", outfilename)
        process = Popen(self.cmdline(compiler), stdout=PIPE, stderr=PIPE)
        compilerout, compilererr = process.communicate()
        if process.returncode != 0 :
            return "", compilerout, "compiler error"
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
        if returncode == 0:
            return "ok"
        elif returncode == "compiler error":
            return "compiler error"
        else:
            return "fail"

class TestDisabledCase(TestCase):
    def runtest(self):
        return "disabled", "", None
    def match(self, resultout, resulterr, returncode) :
        return "disabled"


#
# runtests
#

def findTestCase(folder, base = None):
    testPath = fileForPlatform(folder, "test", "clay")
    mainPath = fileForPlatform(folder, "main", "clay")
    testDisabledPath = fileForPlatform(folder, "test-disabled", "clay")
    mainDisabledPath = fileForPlatform(folder, "main-disabled", "clay")
    if os.path.isfile(testPath):
        TestModuleCase(folder, testPath, base)
    elif os.path.isfile(testDisabledPath):
        TestDisabledCase(folder, testDisabledPath, base)
    elif os.path.isfile(mainDisabledPath):
        TestDisabledCase(folder, mainDisabledPath, base)
    else:
        TestCase(folder, mainPath, base)

def findTestCases():
    findTestCase(runTestRoot)
    return TestCase.allCases

def initWorker():
    signal.signal(signal.SIGINT, signal.SIG_IGN)

def runTest(t):
    return t.run()

def runTests() :
    testcases = findTestCases()
    pool = Pool(processes = cpu_count(), initializer=initWorker)
    results = pool.imap(runTest, testcases)
    succeeded = []
    failed = []
    disabled = []
    try:
        for test in testcases:
            res = results.next()
            print "TEST %s: %s" % (test.name(), res)
            if res == "disabled":
                disabled.append(test.name())
            elif res != "ok":
                failed.append(test.name())
            else:
                succeeded.append(test.name())
    except KeyboardInterrupt:
        print "\nInterrupted!"
        pool.terminate()

    print "\nPASSED %d TESTS" % len(succeeded)
    if len(disabled) != 0:
        print "(%d tests disabled)" % len(disabled)
    if len(failed) != 0:
        print "\nFAILED %d TESTS" % len(failed)
        print "Failed tests:\n ",
        print "\n  ".join(failed)


def main() :
    global testRoot
    global runTestRoot
    if len(sys.argv) > 1 :
        runTestRoot = os.path.join(testRoot, *sys.argv[1:])
    else:
        runTestRoot = testRoot
    startTime = time.time()
    runTests()
    endTime = time.time()
    print ""
    print "time taken = %f seconds" % (endTime - startTime)


if __name__ == "__main__":
    main()
