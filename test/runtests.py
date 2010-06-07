import sys
import os
import tempfile
import time
import cStringIO
from subprocess import Popen, PIPE
from multiprocessing import Pool, cpu_count

testDir = os.path.dirname(os.path.abspath(__file__))
compiler = os.path.join(testDir, "..", "build", "compiler", "src", "clay")

def runtest(path):
    clayfile = os.path.join(path, "main.clay")
    outfile = tempfile.NamedTemporaryFile(delete = False)
    outfile.close()
    process = Popen([compiler, "-o", outfile.name, clayfile], 
            stdout=PIPE, stderr=PIPE)
    if process.wait() != 0 :
        return "TEST %s: %s" % (path, "fail")
    process = Popen([outfile.name], stdout=PIPE)
    process.wait()
    refresult = open(os.path.join(path, "out.txt")).read()
    os.unlink(outfile.name)
    result = "ok"
    if refresult != process.stdout.read():
        result = "fail"
    return "TEST %s: %s" % (path, result)

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
    for i in range(len(paths)):
        print results.next()

runtests()
