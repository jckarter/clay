import sys
from clay.xprint import xprint
from clay.error import CompilerError, error
from clay.parser import parse
from clay.ast import *
from clay.env import buildTopLevelEnv
from clay.coreops import cleanupGlobals
from clay.evaluator import evaluate, InvokeBindings
from clay.analyzer import analyze
from clay import compiler
from clay import llvmwrapper as llvm

def testAnalyzeEval() :
    fileName = sys.argv[1]
    data = file(fileName).read()
    try :
        program = parse(data, fileName)
        env = buildTopLevelEnv(program)
        mainCall = Call(NameRef(Identifier("main")), [])
        result = analyze(mainCall, env)
        xprint(result)
        result = evaluate(mainCall, env)
        xprint(result)
        llt = compiler.llvmType(result.type)
        print "llvm type =", llt
    except CompilerError, e :
        e.display()
        raise

def process() :
    fileName = sys.argv[1]
    data = file(fileName).read()
    try :
        program = parse(data, fileName)
        env = buildTopLevelEnv(program)
        mainProc = env.lookup("main")
        if type(mainProc) is not Procedure :
            error("'main' is not a procedure")
        bindings = InvokeBindings([], [], [], [])
        compiler.compileCode("main", mainProc.code, mainProc.env, bindings)
        print str(compiler.llvmModule)
    except CompilerError, e :
        e.display()
        raise

def main() :
    try :
        process()
    finally :
        cleanupGlobals()

if __name__ == "__main__" :
    main()
