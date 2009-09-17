from clay.error import *
from clay.multimethod import *

__all__ = ["equals", "hashify", "toType", "toInt", "toBool",
           "toValue", "toLValue", "toReference", "toStatic",
           "installGlobalsCleanupHook", "cleanupGlobals"]



#
# equals, hashify
#

equals = multimethod(n=2, defaultProc=(lambda x, y : x is y))

hashify = multimethod(defaultProc=hash)



#
# converters
#

toType = multimethod(errorMessage="type expected")

toInt = multimethod(errorMessage="int expected")

toBool = multimethod(errorMessage="bool expected")

toValue = multimethod(errorMessage="invalid value")

toLValue = multimethod(errorMessage="invalid l-value")

toReference = multimethod(errorMessage="invalid reference")

toStatic = multimethod(errorMessage="invalid static value")



#
# globals cleanup hooks
#

_globalsCleanupHooks = []

def installGlobalsCleanupHook(f) :
    _globalsCleanupHooks.append(f)

def cleanupGlobals() :
    for f in _globalsCleanupHooks :
        f()
