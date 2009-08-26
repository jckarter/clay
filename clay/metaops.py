from clay.multimethod import *

__all__ = ["equals", "hashify", "toInt", "toBool"]

equals = multimethod(n=2, defaultProc=(lambda x, y : x is y))

hashify = multimethod(defaultProc=hash)

toInt = multimethod(errorMessage="int expected")

toBool = multimethod(errorMessage="bool expected")
