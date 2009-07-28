from clay.multimethod import multimethod
from clay.error import raiseError

__all__ = ["Type", "boolType", "intType", "charType", "voidType",
           "TupleType", "ArrayType", "ArrayValueType", "RefType",
           "RecordType", "StructType",
           "isPrimitiveType", "isBoolType", "isIntType", "isCharType",
           "isVoidType", "isTupleType", "isArrayType",
           "isArrayValueType", "isRefType",
           "isRecordType", "isStructType",
           "typeEquals", "typeListEquals", "typeHash",
           "typeUnify", "typeListUnify", "TypeVariable"]



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
charType = PrimitiveType("Char")
voidType = PrimitiveType("Void")

class TupleType(Type) :
    def __init__(self, types) :
        self.types = types

class ArrayType(Type) :
    def __init__(self, type) :
        self.type = type

class ArrayValueType(Type) :
    def __init__(self, type, size) :
        self.type = type
        self.size = size

class RefType(Type) :
    def __init__(self, type) :
        self.type = type

class RecordType(Type) :
    def __init__(self, entry, typeParams) :
        self.entry = entry
        self.typeParams = typeParams

class StructType(Type) :
    def __init__(self, entry, typeParams) :
        self.entry = entry
        self.typeParams = typeParams



#
# type predicates
#

def isPrimitiveType(t) : return type(t) is PrimitiveType
def isBoolType(t) : return t is boolType
def isIntType(t) : return t is intType
def isCharType(t) : return t is charType
def isVoidType(t) : return t is voidType
def isTupleType(t) : return type(t) is TupleType
def isArrayType(t) : return type(t) is ArrayType
def isArrayValueType(t) : return type(t) is ArrayValueType
def isRefType(t) : return type(t) is RefType
def isRecordType(t) : return type(t) is RecordType
def isStructType(t) : return type(t) is StructType



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
    return typeEquals(x.type, y.type)

@typeEquals.register(ArrayValueType, ArrayValueType)
def foo(x, y) :
    return typeEquals(x.type, y.type) and (x.size == y.size)

@typeEquals.register(RefType, RefType)
def foo(x, y) :
    return typeEquals(x.type, y.type)

@typeEquals.register(RecordType, RecordType)
def foo(x, y) :
    return (x.entry is y.entry) and typeListEquals(x.typeParams, y.typeParams)

@typeEquals.register(StructType, StructType)
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
    return hash(("Array", typeHash(x.type)))

@typeHash.register(ArrayValueType)
def foo(x) :
    return hash(("ArrayValue", typeHash(x.type), x.size))

@typeHash.register(RefType)
def foo(x) :
    return hash(("Ref", typeHash(x.type)))

@typeHash.register(RecordType)
def foo(x) :
    childHashes = tuple([typeHash(t) for t in x.typeParams])
    return hash(("Record", id(x.entry), childHashes))

@typeHash.register(StructType)
def foo(x) :
    childHashes = tuple([typeHash(t) for t in x.typeParams])
    return hash(("Struct", id(x.entry), childHashes))



#
# type unification
#

class TypeVariable(Type) :
    def __init__(self, name) :
        self.name = name
        self.type = None

    def deref(self) :
        t = self.deref_()
        if t is None :
            raiseError("unresolved type variable", self.name)
        return t

    def deref_(self) :
        if type(self.type) is TypeVariable :
            return self.type.deref_()
        return self.type


typeUnify = multimethod(n=2, defaultProc=typeUnifyVariables)

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
    assert False

@typeUnify.register(PrimitiveType, PrimitiveType)
def foo(x, y) :
    return x.name == y.name

@typeUnify.register(TupleType, TupleType)
def foo(x, y) :
    return typeListUnify(x.types, y.types)

@typeUnify.register(ArrayType, ArrayType)
def foo(x, y) :
    return typeUnify(x.type, y.type)

@typeUnify.register(ArrayValueType, ArrayValueType)
def foo(x, y) :
    return (x.size == y.size) and typeUnify(x.type, y.type)

@typeUnify.register(RefType, RefType)
def foo(x, y) :
    return typeUnify(x.type, y.type)

@typeUnify.register(RecordType, RecordType)
def foo(x, y) :
    return (x.entry is y.entry) and typeListUnify(x.typeParams, y.typeParams)

@typeUnify.register(StructType, StructType)
def foo(x, y) :
    return (x.entry is y.entry) and typeListUnify(x.typeParams, y.typeParams)



#
# remove temp name used for multimethod instances
#

del foo
