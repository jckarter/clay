import sys
from clay.error import SourceError
from clay.parser import parse
from clay import ast
from clay.compiler import buildTopLevelEnv, evaluate
from clay.xprint import xprint

def main() :
    fileName = sys.argv[1]
    data = file(fileName).read()
    try :
        program = parse(data, fileName)
        env = buildTopLevelEnv(program)
        mainName = ast.NameRef(ast.Identifier("main"))
        mainCall = ast.Call(mainName, [])
        result = evaluate(mainCall, env)
        xprint(result)
    except SourceError, e :
        e.display()
        raise

if __name__ == "__main__" :
    main()
