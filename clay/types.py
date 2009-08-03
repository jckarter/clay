from clay.multimethod import multimethod
from clay.xprint import xprint, XObject, XField, XSymbol, xregister

__all__ = ["Type", "boolType", "intType", "floatType", "doubleType",
           "charType", "voidType",
           "TupleType", "ArrayType", "PointerType", "RecordType",
           "isPrimitiveType", "isBoolType", "isIntType", "isFloatType",
           "isDoubleType", "isCharType",
           "isVoidType", "isTupleType", "isArrayType", "isPointerType",
           "isRecordType",
           "typeEquals", "typeListEquals", "typeHash",
           "typeUnify", "typeListUnify", "TypeVariable", "typeDeref"]



#
# types
#

class Type(object) :
    def __eq__(self, other) :
        return typeEquals(self, other)
    def __ne__(self, other) :
        return not typeEquals(self, other)
    def __hash__(self) :
        return typeHash(self)

class PrimitiveType(Type) :
    def __init__(self, name) :
        self.name = name

boolType = PrimitiveType("Bool")
intType = PrimitiveType("Int")
floatType = PrimitiveType("Float")
doubleType = PrimitiveType("Double")
charType = PrimitiveType("Char")
voidType = PrimitiveType("Void")

class TupleType(Type) :
    def __init__(self, types) :
        self.types = types

class ArrayType(Type) :
    def __init__(self, type, size) :
        self.type = type
        self.size = size

class PointerType(Type) :
    def __init__(self, type) :
        self.type = type

class RecordType(Type) :
    def __init__(self, entry, typeParams) :
        self.entry = entry
        self.typeParams = typeParams



#
# type predicates
#

def isPrimitiveType(t) : return type(t) is PrimitiveType
def isBoolType(t) : return t is boolType
def isIntType(t) : return t is intType
def isFloatType(t) : return t is floatType
def isDoubleType(t) : return t is doubleType
def isCharType(t) : return t is charType
def isVoidType(t) : return t is voidType
def isTupleType(t) : return type(t) is TupleType
def isArrayType(t) : return type(t) is ArrayType
def isPointerType(t) : return type(t) is PointerType
def isRecordType(t) : return type(t) is RecordType



#
# typeEquals
#

typeEquals = multimethod(n=2, default=False)

def typeListEquals(a, b) :
    if len(a) != len(b) :
        return False
    for x,y in zip(a,b) :
        if not typeEquals(x, y) :
            return False
    return True

@typeEquals.register(PrimitiveType, PrimitiveType)
def foo(x, y) :
    return x.name == y.name

@typeEquals.register(TupleType, TupleType)
def foo(x, y) :
    return typeListEquals(x.types, y.types)

@typeEquals.register(ArrayType, ArrayType)
def foo(x, y) :
    return typeEquals(x.type, y.type) and (x.size == y.size)

@typeEquals.register(PointerType, PointerType)
def foo(x, y) :
    return typeEquals(x.type, y.type)

@typeEquals.register(RecordType, RecordType)
def foo(x, y) :
    return (x.entry is y.entry) and typeListEquals(x.typeParams, y.typeParams)



#
# typeHash
#

typeHash = multimethod()

@typeHash.register(PrimitiveType)
def foo(x) :
    return hash(x.name)

@typeHash.register(TupleType)
def foo(x) :
    childHashes = tuple([typeHash(t) for t in x.types])
    return hash(("Tuple", childHashes))

@typeHash.register(ArrayType)
def foo(x) :
    return hash(("Array", typeHash(x.type), x.size))

@typeHash.register(PointerType)
def foo(x) :
    return hash(("Pointer", typeHash(x.type)))

@typeHash.register(RecordType)
def foo(x) :
    childHashes = tuple([typeHash(t) for t in x.typeParams])
    return hash(("Record", id(x.entry), childHashes))



#
# type unification
#

class TypeVariable(Type) :
    def __init__(self) :
        self.type = None

def typeDeref(t) :
    while type(t) is TypeVariable :
        t = t.type
    return t

def typeListUnify(a, b) :
    if len(a) != len(b) :
        return False
    for x,y in zip(a,b) :
        if not typeUnify(x,y) :
            return False
    return True

def typeUnifyVariables(x, y) :
    if type(x) is TypeVariable :
        if x.type is None :
            x.type = y
            return True
        return typeUnify(x.type, y)
    elif type(y) is TypeVariable :
        if y.type is None :
            y.type = x
            return True
        return typeUnify(x, y.type)
    return False

typeUnify = multimethod(n=2, defaultProc=typeUnifyVariables)

@typeUnify.register(PrimitiveType, PrimitiveType)
def foo(x, y) :
    return x.name == y.name

@typeUnify.register(TupleType, TupleType)
def foo(x, y) :
    return typeListUnify(x.types, y.types)

@typeUnify.register(ArrayType, ArrayType)
def foo(x, y) :
    return (x.size == y.size) and typeUnify(x.type, y.type)

@typeUnify.register(PointerType, PointerType)
def foo(x, y) :
    return typeUnify(x.type, y.type)

@typeUnify.register(RecordType, RecordType)
def foo(x, y) :
    return (x.entry is y.entry) and typeListUnify(x.typeParams, y.typeParams)



#
# type printer
#

def obj(name, *fields) :
    return XObject(name, *fields, opening="[", closing="]")

xregister(PrimitiveType, lambda x : XSymbol(x.name))
xregister(TupleType, lambda x : tuple(x.types))
xregister(ArrayType, lambda x : obj("Array", x.type, x.size))
xregister(PointerType, lambda x : obj("Pointer", x.type))
xregister(RecordType,
          lambda x : (obj(x.entry.ast.name.s, *x.typeParams)
                      if x.typeParams
                      else XSymbol(x.entry.ast.name.s)))
xregister(TypeVariable, lambda x : XObject("TypeVariable", x.type))



#
# remove temp name used for multimethod instances
#

del foo
