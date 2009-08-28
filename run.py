import sys
from clay.xprint import xprint
from clay.error import CompilerError
from clay.parser import parse
from clay.ast import *
from clay.env import buildTopLevelEnv
from clay.evaluator import evaluate
from clay.analyzer import analyze

def main() :
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
    except CompilerError, e :
        e.display()
        raise

if __name__ == "__main__" :
    main()
