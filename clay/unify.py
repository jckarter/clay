from clay.xprint import *
from clay.error import *
from clay.multimethod import *
from clay.coreops import *



#
# Cell
#

class Cell(object) :
    def __init__(self, param=None) :
        self.param = param

def isCell(x) : return type(x) is Cell

xregister(Cell, lambda x : XObject("Cell", x.param))



#
# toValueOrCell, toTypeOrCell, toStaticOrCell
#

toValueOrCell = multimethod(defaultProc=toValue)
toValueOrCell.register(Cell)(lambda x : x)

toTypeOrCell = multimethod(defaultProc=toType)
toTypeOrCell.register(Cell)(lambda x : x)

toStaticOrCell = multimethod(defaultProc=toStatic)
toStaticOrCell.register(Cell)(lambda x : x)



#
# unification
#

def unify(a, b) :
    if isCell(a) :
        if a.param is None :
            a.param = b
            return True
        return unify(a.param, b)
    elif isCell(b) :
        if b.param is None :
            b.param = a
            return True
        return unify(a, b.param)
    if isType(a) and isType(b) :
        if a.tag is not b.tag :
            return False
        for x, y in zip(a.params, b.params) :
            if not unify(x, y) :
                return False
        return True
    return equals(a, b)

def matchType(a, b) :
    ensure(unify(a, b), "type mismatch")

def cellDeref(a) :
    while type(a) is Cell :
        a = a.param
    return a

def ensureDeref(a) :
    t = cellDeref(a)
    if t is None :
        error("unresolved variable")
    return t



#
# type pattern matching converters
#

def toValueOfType(pattern) :
    def f(x) :
        v = toValue(x)
        matchType(pattern, v.type)
        return v
    return f

def toLValueOfType(pattern) :
    def f(x) :
        v = toLValue(x)
        matchType(pattern, v.type)
        return v
    return f

def toReferenceOfType(pattern) :
    def f(x) :
        r = toReference(x)
        matchType(pattern, r.type)
        return r
    return f



#
# type variables
#

def bindTypeVars(parentEnv, typeVars) :
    cells = [Cell() for _ in typeVars]
    env = extendEnv(parentEnv, typeVars, cells)
    return env, cells

def resolveTypeVars(typeVars, cells) :
    typeValues = []
    for typeVar, cell in zip(typeVars, cells) :
        typeValues.append(withContext(typeVar, lambda : ensureDeref(cell)))
    return typeValues


# TODO: fix cyclic import
from clay.types import *
