import sys
import os
from clay.xprint import xprint
from clay.error import CompilerError, error
from clay.ast import *
from clay.env import setModuleSearchPath, loadProgram
from clay.core import allocMem

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
        xprint(module)
    except CompilerError, e :
        e.display()

def main() :
    try :
        mainInner()
    finally :
        os._exit(0)

# if __name__ == "__main__" :
#     main()
