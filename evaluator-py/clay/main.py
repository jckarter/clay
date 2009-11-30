import sys
import os
import traceback
from clay.xprint import xprint
from clay.error import CompilerError, error
from clay.ast import *
from clay.env import setModuleSearchPath, loadProgram
from clay.core import *
from clay.primitives import *
from clay.evaluator import *

def initLibrary() :
    base = os.path.split(__file__)[0]
    libDir = os.path.join(base, "..", "..", "lib-clay")
    setModuleSearchPath([libDir])

initLibrary()

def mainInner() :
    if len(sys.argv) != 2 :
        print "usage: %s <clayfile>" % sys.argv[0]
        return
    fileName = sys.argv[1]
    try :
        module = loadProgram(fileName)
        mainCall = Call(NameRef(Identifier("main")), [])
        result = evaluateRootExpr(mainCall, module.env, toTemporary)
        xprint(result)
    except CompilerError, e :
        e.display()
        #raise

def main() :
    try :
        mainInner()
    except Exception, e :
        traceback.print_exc()
    finally :
        os._exit(0)

if __name__ == "__main__" :
    main()
