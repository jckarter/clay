import sys
from clay.xprint import xprint
from clay.error import CompilerError, error
from clay.ast import *
from clay.env import loadProgram
from clay.coreops import cleanupGlobals
from clay.evaluator import evaluate, InvokeBindings
from clay.analyzer import analyze
from clay import compiler
from clay import llvmwrapper as llvm

def loadAnalyzeAndEval() :
    fileName = sys.argv[1]
    try :
        module = loadProgram(fileName)
        mainCall = Call(NameRef(Identifier("main")), [])
        result = analyze(mainCall, module.env)
        xprint(result)
        result = evaluate(mainCall, module.env)
        xprint(result)
    except CompilerError, e :
        e.display()
        raise

def loadAndCompile() :
    if len(sys.argv) < 2 :
        print "usage: clayc input.clay [output.ll]"
    fileName = sys.argv[1]
    if len(sys.argv) == 3 :
        outputFileName = sys.argv[2]
    else :
        outputFileName = None
    try :
        module = loadProgram(fileName)
        mainProc = module.env.lookup("main")
        if type(mainProc) is not Procedure :
            error("'main' is not a procedure")
        bindings = InvokeBindings([], [], [], [])
        compiler.compileCode("main", mainProc.code, mainProc.env, bindings)
        output = str(compiler.llvmModule)
        if outputFileName is None :
            print output
        else :
            f = file(outputFileName, "w")
            f.write(output)
    except CompilerError, e :
        e.display()

def main() :
    try :
        # loadAnalyzeAndEval()
        loadAndCompile()
    finally :
        cleanupGlobals()

if __name__ == "__main__" :
    main()
