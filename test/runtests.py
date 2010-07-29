import os
import itertools
import pickle
import sys
import imp
from subprocess import Popen, PIPE
from multiprocessing import Pool, cpu_count
import time

testDir = os.path.dirname(os.path.abspath(__file__))

if sys.platform == "win32" :
    compiler = os.path.join(testDir, "..", "build", "compiler", "src", "Release", "clay.exe")
    compiler2 = os.path.join(testDir, "..", "clay.exe") # for binary distributions
else :
    compiler = os.path.join(testDir, "..", "build", "compiler", "src", "clay")
    compiler2 = os.path.join(testDir, "..", "clay") # for binary distributions

if not os.path.exists(compiler) :
    compiler = compiler2
    if not os.path.exists(compiler) :
        print "could not find the clay compiler"
        sys.exit(-1)

CONFIG_FUNCTIONS = ["match", "pre_build", "post_build", "post_run", "cmdline"]


impCount = 0

def loadConfigModule(path) :
    global impCount
    module = imp.load_source("config" + str(impCount),
                             path)
    impCount += 1
    return module

class TestCase(object):
    allCases = []
    def __init__(self, folder, base = None):
        entries = os.listdir(folder)
        mainPath = os.path.join(folder, "main.clay")
        configPath = os.path.join(folder, "config.py")
        self.path = folder
        if base:
            self.loadConfig(base)
        if os.path.isfile(mainPath):
            self.loadTest(mainPath)
        if os.path.isfile(configPath):
            module = loadConfigModule(configPath)
            self.loadConfig(module)
        for entry in entries:
            fullpath = os.path.join(folder, entry)
            if not os.path.isdir(fullpath):
                continue
            TestCase(fullpath, self)
    
    def loadTest(self, testfile):
        TestCase.allCases.append(self)
        self.testfile = testfile

    def loadConfig(self, obj):
        for func in CONFIG_FUNCTIONS:
            if not hasattr(obj, func):
                continue
            setattr(self, func, getattr(obj, func))

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
        return os.path.relpath(self.path, testDir)

    def runtest(self):
        outfilename = "a.exe" if sys.platform == "win32" else "a.out"
        outfilename = os.path.join(".", outfilename)
        process = Popen(self.cmdline(compiler), stdout=PIPE, stderr=PIPE)
        if process.wait() != 0 :
            return "fail"
        process = Popen([outfilename], stdout=PIPE)
        process.wait()
        result = process.stdout.read()
        self.removefile(outfilename)
        return result

    def removefile(self, filename) :
        # on windows, sometimes deleting a file
        # deleting a file immediately after executing it 
        # results in a 'access denied' error.
        # so we wait and try again a few times.
        attempts = 1
        while (attempts <= 3) and os.path.exists(filename) :
            try :
                os.unlink(filename)
            except OSError :
                time.sleep(1)
            attempts += 1

def findTestCases():
    TestCase(testDir)
    return TestCase.allCases

def runtest(t):
    return t.run()

def runtests() :
    testcases = findTestCases()
    pool = Pool(processes = cpu_count())
    if sys.platform != "win32" :
        # run tests using a pool of processes
        results = pool.imap(runtest, testcases)
    else :
        # fallback on sequential processing,
        # as multiprocessing + custom-imports don't work on win32
        results = itertools.imap(runtest, testcases)
    failed = []
    for test in testcases:
        res = results.next()
        print "TEST %s: %s" % (test.name(), res)
        if res == "fail":
            failed.append(test.name())
    if len(failed) == 0:
        print "\nALL TESTS PASSED"
    else:
        print "\nFailed tests:" 
        print "\n".join(failed)
        print "\nFAILED %d OF %d TESTS" % (len(failed), len(testcases)) 

if __name__ == "__main__":
    runtests()
