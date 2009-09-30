import sys
import os
import tempfile
import optparse
from clay.xprint import xprint
from clay.cleanup import cleanupGlobals
from clay.error import CompilerError, error
from clay.ast import *
from clay.env import setModuleSearchPath, loadProgram
from clay.evaluator import evaluate, InvokeBindings
from clay.analyzer import analyze
from clay import compiler

def initLibrary() :
    base = os.path.split(__file__)[0]
    libDir = os.path.join(base, "..", "..", "lib-clay")
    setModuleSearchPath([libDir])

initLibrary()

tempFiles = []

def newTempFile(suffix) :
    fd, fpath = tempfile.mkstemp(suffix=suffix, prefix="claytmp", dir=".")
    os.close(fd)
    tempFiles.append(fpath)
    return fpath

def clearTempFiles() :
    for f in tempFiles :
        os.remove(f)

def loadAnalyzeAndEval(fileName) :
    module = loadProgram(fileName)
    mainCall = Call(NameRef(Identifier("main")), [])
    result = analyze(mainCall, module.env)
    xprint(result)
    result = evaluate(mainCall, module.env)
    xprint(result)

def loadAndCompile(fileName) :
    module = loadProgram(fileName)
    mainProc = module.env.lookup("main")
    if type(mainProc) is not Procedure :
        error("'main' is not a procedure")
    bindings = InvokeBindings([], [], [], [])
    compiler.compileCode("main", mainProc.code, mainProc.env, bindings,
                         internal=False)
    return compiler.llvmModule()

def compileAndMakeExe() :
    parser = optparse.OptionParser()
    parser.add_option("--eval", action="store_true",
                      dest="evaluate", default=False)
    parser.add_option("-o", action="store",
                      dest="output", default=None)
    (options, args) = parser.parse_args()
    if len(args) == 0 :
        parser.error("provide clay file to compile")
    if len(args) > 1 :
        parser.error("only one input file allowed: %s" % str(args))

    if options.evaluate :
        loadAnalyzeAndEval(args[0])
        return
    llvmModule = loadAndCompile(args[0])

    llFile = newTempFile(".ll")
    f = file(llFile, "w")
    print>>f, str(llvmModule)
    f.close()

    bcFile = newTempFile(".bc")
    os.system("llvm-as -f -o '%s' '%s'" % (bcFile, llFile))
    optBcFile = newTempFile("-opt.bc")
    os.system("opt -O3 -f -o '%s' '%s'" % (optBcFile, bcFile))
    cFile = newTempFile(".c")
    os.system("llc -march=c -f -o '%s' '%s'" % (cFile, optBcFile))

    if options.output is not None :
        exeFile = options.output
    else :
        exeFile = "a.exe"

    stubFile = os.path.join(os.path.split(__file__)[0], "stub.c")

    os.system("gcc -O3 -o '%s' '%s' '%s'" % (exeFile, stubFile, cFile))

def main() :
    try :
        compileAndMakeExe()
    except CompilerError, e :
        e.display()
    finally :
        clearTempFiles()
        cleanupGlobals()

if __name__ == "__main__" :
    main()
