import sys
from clay.error import SourceError
from clay.parser import parse
from clay import ast
from clay.compiler import buildTopLevelEnv, inferValueType
from clay.xprint import xprint

def main() :
    fileName = sys.argv[1]
    data = file(fileName).read()
    try :
        program = parse(data, fileName)
        xprint(program)
        env = buildTopLevelEnv(program)
        e = ast.CallExpr(ast.NameRef(ast.Identifier("main")), [])
        xprint(inferValueType(e, env))
    except SourceError, e :
        e.display()
        return

if __name__ == "__main__" :
    main()
