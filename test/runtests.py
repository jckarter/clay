#!/usr/bin/env python2.7

import re
import os
import glob
import pickle
import platform
import signal
import sys
import difflib
from StringIO import StringIO
from subprocess import Popen, PIPE
from multiprocessing import Pool, cpu_count
import time


#
# testLogFile
# 

testLogFile = open("testlog.txt", "w")


#
# testRoot
#

testBuildFlags = []
testRoot = os.path.dirname(os.path.abspath(__file__))
runTestRoot = testRoot

def indented(txt):
    return ' ' + txt.replace('\n', '\n ')


#
# getClayPlatform, fileForPlatform
#

def getClayPlatform():
    if sys.platform in ["win32", "cygwin"]:
        return "windows"
    if sys.platform == "darwin":
        return "macosx"
    if sys.platform.startswith("freebsd"):
        return "freebsd"
    if sys.platform.startswith("linux"):
        return "linux"

def getClayPlatformFamily():
    if sys.platform in ["win32", "cygwin"]:
        return "windows"
    else:
        return "unix"

def getClayArchitecture():
    if platform.machine() in ["i386", "x86_64", "x86-64", "amd64", "AMD64", "x64"]:
        return "x86"
    else:
        return "unknown"

# XXX this needs to determine the bitness of clay executables, not of python
def getClayBits():
    if platform.architecture()[0] == "64bit":
        return "64"
    if platform.architecture()[0] == "32bit":
        return "32"

clayPlatform = getClayPlatform()
clayPlatformFamily = getClayPlatformFamily()
clayArchitecture = getClayArchitecture()
clayBits = getClayBits()

def fileForPlatform(folder, name, ext):
    platformNames = [
        "%s.%s.%s.%s.%s" % (name, clayPlatform, clayArchitecture, clayBits, ext),
        "%s.%s.%s.%s.%s" % (name, clayPlatformFamily, clayArchitecture, clayBits, ext),
        "%s.%s.%s.%s" % (name, clayPlatform, clayArchitecture, ext),
        "%s.%s.%s.%s" % (name, clayPlatform, clayBits, ext),
        "%s.%s.%s.%s" % (name, clayPlatformFamily, clayArchitecture, ext),
        "%s.%s.%s.%s" % (name, clayPlatformFamily, clayBits, ext),
        "%s.%s.%s.%s" % (name, clayArchitecture, clayBits, ext),
        "%s.%s.%s" % (name, clayPlatform, ext),
        "%s.%s.%s" % (name, clayPlatformFamily, ext),
        "%s.%s.%s" % (name, clayArchitecture, ext),
        "%s.%s.%s" % (name, clayBits, ext),
    ]
    for platformName in platformNames:
        fullName = os.path.join(folder, platformName)
        if os.path.isfile(fullName):
            return fullName

    return os.path.join(folder, "%s.%s" % (name, ext))


#
# getCompilerPath
#

def getCompilerPath() :
    buildPath = ["..", "build", "compiler", "src"]
    if sys.platform == "win32" :
        compiler = os.path.join(testRoot, *(buildPath + ["clay.exe"]))
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
        r = [clay, "-I" + self.path, "-o", "test.exe"] + testBuildFlags + self.buildflags + [self.testfile]
        return r

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
            errpattern = open(compilererrfile).read()
            errpattern = errpattern.replace('\r', '').strip()

            if returncode != "compiler error":
                self.testLogBuffer.write("Failure: compiler did not fail\n")
                self.testLogBuffer.write("Expected:\n")
                self.testLogBuffer.write(indented(errpattern.replace))
                return "compiler did not fail"
            if re.search(errpattern, resulterr):
                return "ok"
            else:
                self.testLogBuffer.write("Failure: unexpected compiler error\n")
                self.testLogBuffer.write("Expected:\n")
                self.testLogBuffer.write(indented(errpattern))
                self.testLogBuffer.write("Actual:\n")
                self.testLogBuffer.write(indented(resulterr))
                return "unexpected compiler error"

        else:
            if returncode == "compiler error":
                self.testLogBuffer.write("Failure: compiler error\n")
                self.testLogBuffer.write("Error:\n")
                self.testLogBuffer.write(indented(resulterr))
                return "compiler error"
            outfile = fileForPlatform(".", "out", "txt")
            errfile = fileForPlatform(".", "err", "txt")
            if not os.path.isfile(outfile) :
                self.testLogBuffer.write("Failure: out.txt missing")
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
                self.testLogBuffer.write("Failure: out.txt and err.txt mismatch\n")
                self.testLogBuffer.write("Diff-Stdout:\n")
                for line in difflib.unified_diff(
                    refout.splitlines(True), resultout.splitlines(True),
                    fromfile="out.txt", tofile="stdout"
                ):
                    self.testLogBuffer.write(indented(line))
                self.testLogBuffer.write("Diff-Stderr:\n")
                for line in difflib.unified_diff(
                    referr.splitlines(True), resulterr.splitlines(True),
                    fromfile="err.txt", tofile="stderr"
                ):
                    self.testLogBuffer.write(indented(line))
                return "out.txt and err.txt mismatch"
            elif resultout != refout:
                self.testLogBuffer.write("Failure: out.txt mismatch\n")
                self.testLogBuffer.write("Diff-Stdout:\n")
                for line in difflib.unified_diff(
                    refout.splitlines(True), resultout.splitlines(True),
                    fromfile="out.txt", tofile="stdout"
                ):
                    self.testLogBuffer.write(indented(line))
                return "out.txt mismatch"
            elif resulterr != referr:
                self.testLogBuffer.write("Diff-Stderr:\n")
                for line in difflib.unified_diff(
                    referr.splitlines(True), resulterr.splitlines(True),
                    fromfile="err.txt", tofile="stderr"
                ):
                    self.testLogBuffer.write(indented(line))
                return "err.txt mismatch"

    def run(self):
        self.testLogBuffer = StringIO()
        print >>self.testLogBuffer
        self.testLogBuffer.write("[%s]\n" % self.name())
        os.chdir(self.path)
        self.pre_build()
        self.post_build()
        resultout, resulterr, returncode = self.runtest()
        self.post_run()
        r = self.match(resultout, resulterr, returncode)
        log = self.testLogBuffer.getvalue()
        self.testLogBuffer.close()

        return (r, log)

    def name(self):
        return os.path.relpath(self.path, testRoot)

    def runtest(self):
        outfilename = "test.exe"
        outfilename = os.path.join(".", outfilename)
        process = Popen(self.cmdline(compiler), stdout=PIPE, stderr=PIPE)
        compilerout, compilererr = process.communicate()
        if process.returncode != 0 :
            return "", "%s\n%s" % (compilerout, compilererr), "compiler error"
        if self.runscript is None:
            commandline = [outfilename]
        else:
            commandline = [sys.executable, self.runscript, outfilename] + testBuildFlags

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
            self.testLogBuffer.write("compiler error")
            self.testLogBuffer.write("--------------")
            self.testLogBuffer.write(resulterr)
            return "compiler error"
        else:
            self.testLogBuffer.write("fail")
            self.testLogBuffer.write("----")
            self.testLogBuffer.write(resultout)
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
        print "[Tests]"
        for test in testcases:
            res, log = results.next()
            if res != "ok" and res != "disabled":
                print >>testLogFile, log,
            testLogFile.flush()
            print "%s: %s" % (test.name(), res)
            if res == "disabled":
                disabled.append(test.name())
            elif res != "ok":
                failed.append('%s: %s' % (test.name(), res))
            else:
                succeeded.append(test.name())
    except KeyboardInterrupt:
        print "\nInterrupted!"
        pool.terminate()

    if failed:
        print
        print "[Failed]\n",
        print "\n".join(failed)

    print
    print "[Summary]"
    print "Passed: %d" % len(succeeded)
    if disabled:
        print "Disabled: %d" % len(disabled)
    if failed:
        print "Failed: %d" % len(failed)
    testLogFile.flush()
    print "\n# Test log written to testlog.txt"

def usage(argv0):
    print "Usage: %s [buildflags... --] [root] [root...]" % argv0
    print "  Runs the Clay test suite. If any root paths are given, only tests"
    print "  in those subdirectories (paths relative to the test/ directory) will"
    print "  be run. Build flags can be passed to the compiler followed by '--'."

def main() :
    global testLogFile
    global testBuildFlags
    global testRoot
    global runTestRoot

    if len(sys.argv) > 1 :
        if sys.argv[1] == "--help" or sys.argv[1] == "/?":
            usage(sys.argv[0])
            return

        try:
            buildFlagSeparator = sys.argv.index("--")
            testBuildFlags = sys.argv[1:buildFlagSeparator]
            runTestRoot = os.path.join(testRoot, *sys.argv[buildFlagSeparator+1:])
        except ValueError:
            testBuildFlags = []
            runTestRoot = os.path.join(testRoot, *sys.argv[1:])
    else:
        testBuildFlags = []
        runTestRoot = testRoot
    startTime = time.time()
    runTests()
    endTime = time.time()
    print
    print "time-taken-seconds: %f" % (endTime - startTime)


if __name__ == "__main__":
    main()
