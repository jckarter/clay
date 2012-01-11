#!/usr/bin/env python2.7

import re
import os
import glob
import pickle
import platform
import signal
import sys
import difflib
import argparse
from StringIO import StringIO
from subprocess import Popen, PIPE, call
from multiprocessing import Pool, cpu_count
import time
import tempfile
import ConfigParser


#
# test properties
#

class TestOptions:
    testBuildFlags = []
    testRoot = os.path.dirname(os.path.abspath(__file__))
    runTestRoot = os.path.dirname(os.path.abspath(__file__))
    testLogFile = "testlog.txt"
    clayCompiler = None
    clayPlatform = None


#
# utils
#

def indented(txt):
    return ' ' + txt.rstrip('\n').replace('\n', '\n ') + '\n'


#
# getClayCompiler
#

# stolen from http://stackoverflow.com/questions/377017/test-if-executable-exists-in-python
def which(program):
    for path in os.environ["PATH"].split(os.pathsep):
        exe_file = os.path.join(path, program)
        if os.path.exists(fpath) and os.access(fpath, os.X_OK):
            return exe_file

    return None

def getClayCompiler(opt) :
    buildPath = ["..", "build", "compiler", "src"]
    clayexe = None
    if sys.platform == "win32" :
        clayexe = "clay.exe"
    else :
        clayexe = "clay"

    compiler = os.path.join(opt.testRoot, *(buildPath + [clayexe]))
    if not os.path.exists(compiler) :
        compiler = which(clayexe)
        if compiler is None:
            print "could not find the clay compiler"
            sys.exit(1)
    return compiler



#
# getClayPlatform, fileForPlatform
#

def getClayPlatform(opt):
    with tempfile.NamedTemporaryFile(suffix='.clay', delete=False) as platformclay:
        print >>platformclay, 'import platform.(OSString, OSFamilyString, CPUString, CPUBits);'
        print >>platformclay, 'main() {'
        print >>platformclay, '    println("[Target]");'
        print >>platformclay, '    println("platform: ", OSString);'
        print >>platformclay, '    println("platform-family: ", OSFamilyString);'
        print >>platformclay, '    println("architecture: ", CPUString);'
        print >>platformclay, '    println("bits: ", CPUBits);'
        print >>platformclay, '    return 0;'
        print >>platformclay, '}'
        platformclay.close()

        returncode = call([opt.clayCompiler] + opt.testBuildFlags + ['-o', 'test.exe', platformclay.name])
        if returncode != 0:
            print "could not build an executable with", opt.clayCompiler, opt.testBuildFlags
            sys.exit(1)

        process = Popen(["./test.exe"], stdout=PIPE)
        config = ConfigParser.ConfigParser()
        config.readfp(process.stdout)

        process.communicate()
        if process.returncode != 0:
            print "executable failed with exit code", process.returncode, "built with", opt.clayCompiler, opt.testBuildFlags

        return (
            config.get('Target', 'platform'),
            config.get('Target', 'platform-family'),
            config.get('Target', 'architecture'),
            config.get('Target', 'bits')
        )

def fileForPlatform(opt, folder, name, ext):
    platform, family, arch, bits = opt.clayPlatform
    platformNames = [
        "%s.%s.%s.%s.%s" % (name, platform, arch, bits, ext),
        "%s.%s.%s.%s.%s" % (name, family, arch, bits, ext),
        "%s.%s.%s.%s" % (name, platform, arch, ext),
        "%s.%s.%s.%s" % (name, platform, bits, ext),
        "%s.%s.%s.%s" % (name, family, arch, ext),
        "%s.%s.%s.%s" % (name, family, bits, ext),
        "%s.%s.%s.%s" % (name, arch, bits, ext),
        "%s.%s.%s" % (name, platform, ext),
        "%s.%s.%s" % (name, family, ext),
        "%s.%s.%s" % (name, arch, ext),
        "%s.%s.%s" % (name, bits, ext),
    ]
    for platformName in platformNames:
        fullName = os.path.join(folder, platformName)
        if os.path.isfile(fullName):
            return fullName

    return os.path.join(folder, "%s.%s" % (name, ext))


#
# TestCase
#

class TestCase(object):
    allCases = []
    opt = None
    def __init__(self, opt, folder, testfile, base = None):
        self.opt = opt
        entries = os.listdir(folder)
        self.testfile = os.path.basename(testfile)
        self.path = folder
        if os.path.isfile(testfile):
            self.loadTest(testfile)

        runscript = fileForPlatform(self.opt, folder, "run", "py")
        if os.path.isfile(runscript):
            self.runscript = runscript
        else:
            self.runscript = None

        buildflags = fileForPlatform(self.opt, folder, "buildflags", "txt")
        if os.path.isfile(buildflags):
            self.buildflags = open(buildflags).read().split()
        else:
            self.buildflags = []

        for entry in entries:
            fullpath = os.path.join(folder, entry)
            if not os.path.isdir(fullpath):
                continue
            findTestCase(self.opt, fullpath, self)
    
    def loadTest(self, testfile):
        TestCase.allCases.append(self)
        self.testfile = testfile

    def cmdline(self, clay) :
        r = [clay, "-I" + self.path, "-o", "test.exe"] + self.opt.testBuildFlags + self.buildflags + [self.testfile]
        return r

    def pre_build(self) :
        pass

    def post_build(self) :
        pass

    def post_run(self) :
        [os.unlink(f) for f in glob.glob("temp*")]
        [os.unlink(f) for f in glob.glob("*.data")]

    def match(self, resultout, resulterr, returncode) :
        compilererrfile = fileForPlatform(self.opt, ".", "compilererr", "txt")
        if os.path.isfile(compilererrfile):
            errpattern = open(compilererrfile).read()
            errpattern = errpattern.replace('\r', '').strip()
            resulterr = resulterr.replace('\r', '')

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
            outfile = fileForPlatform(self.opt, ".", "out", "txt")
            errfile = fileForPlatform(self.opt, ".", "err", "txt")
            if not os.path.isfile(outfile) :
                self.testLogBuffer.write("Failure: out.txt missing")
                self.testLogBuffer.write("Stdout:\n")
                self.testLogBuffer.write(indented(resultout))
                self.testLogBuffer.write("Stderr:\n")
                self.testLogBuffer.write(indented(resulterr))
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
        return os.path.relpath(self.path, self.opt.testRoot)

    def runtest(self):
        outfilename = "test.exe"
        outfilename = os.path.join(".", outfilename)
        process = Popen(self.cmdline(self.opt.clayCompiler), stdout=PIPE, stderr=PIPE)
        compilerout, compilererr = process.communicate()
        if process.returncode != 0 :
            return "", "%s\n%s" % (compilerout, compilererr), "compiler error"
        if self.runscript is None:
            commandline = [outfilename]
        else:
            commandline = [sys.executable, self.runscript, outfilename] + self.opt.testBuildFlags

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
            self.testLogBuffer.write("Failure: compiler error\n")
            self.testLogBuffer.write("Error:\n")
            self.testLogBuffer.write(indented(resulterr))
            return "compiler error"
        else:
            self.testLogBuffer.write("Failure: exit code %d\n" % returncode)
            self.testLogBuffer.write("Stdout:\n")
            self.testLogBuffer.write(indented(resultout))
            self.testLogBuffer.write("Stderr:\n")
            self.testLogBuffer.write(indented(resulterr))
            return "exit code %d" % returncode

class TestDisabledCase(TestCase):
    def runtest(self):
        return "disabled", "", None
    def match(self, resultout, resulterr, returncode) :
        return "disabled"


#
# runtests
#

def findTestCase(opt, folder, base = None):
    testPath = fileForPlatform(opt, folder, "test", "clay")
    mainPath = fileForPlatform(opt, folder, "main", "clay")
    testDisabledPath = fileForPlatform(opt, folder, "test-disabled", "clay")
    mainDisabledPath = fileForPlatform(opt, folder, "main-disabled", "clay")
    if os.path.isfile(testPath):
        TestModuleCase(opt, folder, testPath, base)
    elif os.path.isfile(testDisabledPath):
        TestDisabledCase(opt, folder, testDisabledPath, base)
    elif os.path.isfile(mainDisabledPath):
        TestDisabledCase(opt, folder, mainDisabledPath, base)
    else:
        TestCase(opt, folder, mainPath, base)

def findTestCases(opt):
    findTestCase(opt, opt.runTestRoot)
    return TestCase.allCases

def initWorker():
    signal.signal(signal.SIGINT, signal.SIG_IGN)

def runTest(t):
    return t.run()

def runTests(opt):
    testcases = findTestCases(opt)
    try:
        pool = Pool(processes = cpu_count(), initializer=initWorker)
        results = pool.imap(runTest, testcases)
    except ImportError:
        pool = None
        results = (runTest(t) for t in testcases)
    succeeded = []
    failed = []
    disabled = []
    logfile = open(opt.testLogFile, 'w')
    try:
        print "[TargetPlatform]"
        platform, family, arch, bits = opt.clayPlatform
        print "platform:", platform
        print "cpu:", arch
        print "bits:", bits
        print
        print "[Tests]"
        for test in testcases:
            res, log = results.next()
            if res != "ok" and res != "disabled":
                logfile.write(log)
            logfile.flush()
            print "%s: %s" % (test.name(), res)
            if res == "disabled":
                disabled.append(test.name())
            elif res != "ok":
                failed.append('%s: %s' % (test.name(), res))
            else:
                succeeded.append(test.name())
    except KeyboardInterrupt:
        print "\nInterrupted!"
        if pool:
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
    logfile.flush()
    print "\n# Test log written to " + opt.testLogFile

def main() :
    opt = TestOptions()

    argp = argparse.ArgumentParser(description="Run the Clay test suite.")
    argp.add_argument("-I",
        metavar="path",
        dest='libClayPaths',
        action='append',
        help="Specifies the Clay module path(s) to use.")
    argp.add_argument("--arch",
        metavar="arch",
        dest='arch',
        help="Specifies a Darwin target architecture to build for. (Apple only)")
    argp.add_argument("--target",
        metavar="cpu-vendor-os",
        dest='target',
        help="Specifies the target triple to build for.")
    argp.add_argument("--buildflags",
        nargs='+',
        metavar="flags",
        dest='buildFlags',
        help="Additional build flags to pass through to the Clay compiler.")
    argp.add_argument("--root",
        metavar="path",
        dest='testRoot',
        help="Specifies the root directory out of which to run tests.")
    argp.add_argument("--log",
        metavar="path",
        dest='logfile',
        help="Log detailed test failures to the given path (default testlog.txt).")
    argp.add_argument("testname",
        nargs='?',
        help="Only run tests under the specified subdirectory in the test root.")

    args = argp.parse_args()

    if args.arch is not None:
        opt.testBuildFlags += ['-arch', args.arch]
    if args.libClayPaths is not None:
        opt.testBuildFlags += ['-I' + os.path.abspath(path) for path in args.libClayPaths]
    if args.target is not None:
        opt.testBuildFlags += ['-target', args.target]
    if args.testRoot is not None:
        opt.testRoot = os.path.abspath(args.testRoot)
    if args.logfile is not None:
        opt.testLogFile = args.logfile

    if args.testname is not None:
        opt.runTestRoot = os.path.join(opt.testRoot, args.testname)
    else:
        opt.runTestRoot = opt.testRoot

    startTime = time.time()
    opt.clayCompiler = getClayCompiler(opt)
    opt.clayPlatform = getClayPlatform(opt)
    runTests(opt)
    endTime = time.time()
    print
    print "time-taken-seconds: %f" % (endTime - startTime)


if __name__ == "__main__":
    main()
