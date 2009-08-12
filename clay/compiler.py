import ctypes
from clay.multimethod import multimethod
from clay.xprint import xprint, XObject, XField, XSymbol, xregister
from clay.machineops import *
from clay.ast import *
from clay.error import *



#
# error context
#

_errorContext = []

def contextPush(astNode) :
    _errorContext.append(astNode)

def contextPop() :
    _errorContext.pop()

def contextLocation() :
    for astNode in reversed(_errorContext) :
        if astNode.location is not None :
            return astNode.location
    return None

def error(msg) :
    raiseError(msg, contextLocation)



#
# Environment
#

class Environment(object) :
    def __init__(self, parent=None) :
        self.parent = parent
        self.entries = {}

    def hasEntry(self, name) :
        return name in self.entries

    def add(self, name, entry) :
        assert type(name) is str
        assert name not in self.entries
        self.entries[name] = entry

    def lookup(self, name) :
        assert type(name) is str
        entry = self.entries.get(name)
        if (entry is None) and (self.parent is not None) :
            return self.parent.lookup(name)
        return entry



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
        super(PrimitiveType, self).__init__()
        self.name = name

boolType = PrimitiveType("Bool")
intType = PrimitiveType("Int")
floatType = PrimitiveType("Float")
doubleType = PrimitiveType("Double")
charType = PrimitiveType("Char")

class TupleType(Type) :
    def __init__(self, types) :
        super(TupleType, self).__init__()
        self.types = types

class ArrayType(Type) :
    def __init__(self, type, size) :
        super(ArrayType, self).__init__()
        self.type = type
        self.size = size

class PointerType(Type) :
    def __init__(self, type) :
        super(PointerType, self).__init__()
        self.type = type

class RecordType(Type) :
    def __init__(self, record, typeParams) :
        super(RecordType, self).__init__()
        self.record = record
        self.typeParams = typeParams



#
# type predicates
#

def isType(t) : return isinstance(t, Type)
def isPrimitiveType(t) : return type(t) is PrimitiveType
def isBoolType(t) : return t is boolType
def isIntType(t) : return t is intType
def isFloatType(t) : return t is floatType
def isDoubleType(t) : return t is doubleType
def isCharType(t) : return t is charType

def isTupleType(t) : return type(t) is TupleType
def isArrayType(t) : return type(t) is ArrayType
def isPointerType(t) : return type(t) is PointerType
def isRecordType(t) : return type(t) is RecordType



#
# type equality
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
    sameRecord = (x.record is y.record)
    raise NotImplementedError
    return sameRecord and typeListEquals(x.typeParams, y.typeParams)



#
# type hash
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
    raise NotImplementedError
    childHashes = tuple([typeHash(t) for t in x.typeParams])
    return hash(("Record", id(x.record), childHashes))



#
# type unification
#

class TypeVariable(Type) :
    def __init__(self) :
        super(TypeVariable, self).__init__()
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
    raise NotImplementedError
    return (x.record is y.record) and typeListUnify(x.typeParams, y.typeParams)



#
# convert to ctypes
#

_ctypesTable = {}

def ctypesType(t) :
    ct = _ctypesTable.get(t)
    if ct is None :
        ct = makeCTypesType(t)
    return ct

makeCTypesType = multimethod()

@makeCTypesType.register(PrimitiveType)
def foo(t) :
    if isBoolType(t) :
        ct = ctypes.c_bool
    elif isIntType(t) :
        ct = ctypes.c_int
    elif isFloatType(t) :
        ct = ctypes.c_float
    elif isDoubleType(t) :
        ct = ctypes.c_double
    elif isCharType(t) :
        ct = ctypes.c_char
    else :
        assert False
    _ctypesTable[t] = ct
    return ct

@makeCTypesType.register(TupleType)
def foo(t) :
    fieldTypes = [ctypesType(x) for x in t.types]
    fieldNames = ["f%d" % x for x in range(len(t.types))]
    ct = type("Tuple", (ctypes.Structure,))
    ct._fields_ = zip(fieldNames, fieldTypes)
    _ctypesTable[t] = ct
    return ct

@makeCTypesType.register(ArrayType)
def foo(t) :
    ct = ctypesType(t.type) * t.size
    _ctypesTable[t] = ct
    return ct

@makeCTypesType.register(RecordType)
def foo(t) :
    ct = type("Record", (ctypes.Structure,))
    raise NotImplementedError



#
# typeSize
#

def typeSize(t) :
    return ctypes.sizeof(ctypesType(t))



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
          lambda x : (obj(x.record.name.s, *x.typeParams)
                      if x.typeParams
                      else XSymbol(x.record.name.s)))
xregister(TypeVariable, lambda x : XObject("TypeVariable", x.type))




#
# primitives env
#

class PrimitiveTypeConstructor(object) :
    def __init__(self, name) :
        self.name = name

class PrimitiveOperation(object) :
    def __init__(self, name) :
        self.name = name

primitivesEnv = Environment()
primitiveNames = None

def buildPrimitivesEnv() :
    names = {}
    def entry(name, value) :
        primitivesEnv.add(name, value)
        names[name] = name
    def typeConstructor(name) :
        entry(name, PrimitiveTypeConstructor(name))
    def operation(name) :
        entry(name, PrimitiveOperation(name))
    def overloadable(name) :
        x = Overloadable(Identifier(name))
        x.env = primitivesEnv
        entry(name, x)

    entry("Bool", boolType)
    entry("Int", intType)
    entry("Float", floatType)
    entry("Double", doubleType)
    entry("Char", charType)

    typeConstructor("Tuple")
    typeConstructor("Array")
    typeConstructor("Pointer")

    operation("default")
    operation("typeSize")

    operation("addressOf")
    operation("pointerDereference")
    operation("pointerOffset")
    operation("pointerSubtract")
    operation("pointerCast")
    operation("pointerCopy")
    operation("pointerEquals")
    operation("pointerLesser")
    operation("allocateMemory")
    operation("freeMemory")

    operation("TupleType")
    operation("tuple")
    operation("tupleFieldCount")
    operation("tupleFieldRef")

    operation("array")
    operation("arrayRef")

    operation("RecordType")
    operation("makeRecord")
    operation("recordFieldCount")
    operation("recordFieldRef")

    operation("boolCopy")
    operation("boolNot")

    operation("charCopy")
    operation("charEquals")
    operation("charLesser")

    operation("intCopy")
    operation("intEquals")
    operation("intLesser")
    operation("intAdd")
    operation("intSubtract")
    operation("intMultiply")
    operation("intDivide")
    operation("intModulus")
    operation("intNegate")

    operation("floatCopy")
    operation("floatEquals")
    operation("floatLesser")
    operation("floatAdd")
    operation("floatSubtract")
    operation("floatMultiply")
    operation("floatDivide")
    operation("floatNegate")

    operation("doubleCopy")
    operation("doubleEquals")
    operation("doubleLesser")
    operation("doubleAdd")
    operation("doubleSubtract")
    operation("doubleMultiply")
    operation("doubleDivide")
    operation("doubleNegate")

    operation("charToInt")
    operation("intToChar")
    operation("floatToInt")
    operation("intToFloat")
    operation("floatToDouble")
    operation("doubleToFloat")
    operation("doubleToInt")
    operation("intToDouble")

    overloadable("init")
    overloadable("destroy")
    overloadable("assign")
    overloadable("equals")
    overloadable("lesser")
    overloadable("lesserEquals")
    overloadable("greater")
    overloadable("greaterEquals")

    global primitiveNames
    primitiveNames = type("PrimitiveNames", (object,), names)

buildPrimitivesEnv()



#
# Value
#

class Value(object) :
    def __init__(self, type) :
        self.type = type
        self.ctypesType = ctypesType(type)
        p = mop_memAlloc(ctypes.sizeof(self.ctypesType))
        self.buf = ctypes.cast(p, ctypes.POINTER(self.ctypesType))
    def __del__(self) :
        mop_memFree(self.buf)

class RefValue(object) :
    def __init__(self, type) :
        self.type = type
        self.ptr = Value(PointerType(type))



#
# temp values
#

_tempValues = []

def tempValue(type) :
    v = Value(type)
    _tempValues.append(v)
    return v

def tempRefValue(type) :
    v = RefValue(type)
    _tempValues.append(v)
    return v

def clearTempValues() :
    del _tempValues[:]



#
# value operations
#

def initValue(a) :
    init = NameRef(Identifier("init"))
    call = Call(init, [a])
    evaluate(call, primitivesEnv)

def copyValue(dest, src) :
    init = NameRef(Identifier("init"))
    call = Call(init, [dest, src])
    evaluate(call, primitivesEnv)

def destroyValue(a) :
    destroy = NameRef(Identifier("destroy"))
    call = Call(destroy, [a])
    evaluate(call, primitivesEnv)

def assignValue(dest, src) :
    assign = NameRef(Identifier("assign"))
    call = Call(assign, [dest, src])
    evaluate(call, primitivesEnv)

def valueToRef(a) :
    ref = tempRefValue(a.type)
    addr = ctypes.c_void_p(ctypes.addressof(a.buf))
    ctypes.memmove(ref.ptr.buf, addr, ctypes.sizeof(addr))
    return ref

def refToValue(a) :
    v = tempValue(a.type)
    copyValue(v, a)
    return v

def valueToInt(a) :
    assert type(a) is Value
    assert isIntType(a.type)
    return a.buf[0]

def intToValue(i) :
    v = tempValue(intType)
    v.buf[0] = i
    return v

def valueToBool(a) :
    assert type(a) is Value
    assert isBoolType(a.type)
    return a.buf[0]

def boolToValue(b) :
    v = tempValue(boolType)
    v.buf[0] = b
    return v

def valueToChar(a) :
    assert type(a) is Value
    assert isCharType(a.type)
    return a.buf[0]

def charToValue(c) :
    v = tempValue(charType)
    v.buf[0] = c
    return v



#
# evaluate : (expr, env) -> Type|Value|RefValue|None
#

def evaluate(expr, env, verifier=None) :
    try :
        contextPush(expr)
        result = evalExpr(expr, env)
        if verifier is not None :
            result = verifier(result)
        return result
    finally :
        contextPop()

def verifyType(x) :
    if not isType(x) :
        error("type expected")
    return x

def verifyAssignable(x) :
    if type(x) is not RefValue :
        error("reference expected")
    return x

def verifyRef(x) :
    if type(x) is RefValue :
        return x
    if type(x) is Value :
        return valueToRef(x)
    error("reference/value expected")

def verifyValue(x) :
    if type(x) is Value :
        return x
    if type(x) is RefValue :
        return refToValue(x)
    error("value expected")

def verifyTypeParam(x) :
    if type(x) is RefValue :
        return refToValue(x)
    if x is None :
        error("type param expected")
    return x

def verifyIntValue(x) :
    if type(x) is RefValue :
        x = refToValue(x)
    if (type(x) is not Value) or (not isIntType(x.type)) :
        error("int value expected")
    return valueToInt(x)

def verifyBoolValue(x) :
    if type(x) is RefValue :
        x = refToValue(x)
    if (type(x) is not Value) or (not isBoolType(x.type)) :
        error("bool value expected")
    return valueToBool(x)

def evaluateAsType(expr, env) :
    return evaluate(expr, env, verifyType)

def evaluateAsAssignable(expr, env) :
    return evaluate(expr, env, verifyAssignable)

def evaluateAsRef(expr, env) :
    return evaluate(expr, env, verifyRef)

def evaluateAsValue(expr, env) :
    return evaluate(expr, env, verifyValue)

def evaluateAsTypeParam(expr, env) :
    return evaluate(expr, env, verifyTypeParam)

def evaluateAsIntValue(expr, env) :
    return evaluate(expr, env, verifyIntValue)

def evaluateAsBoolValue(expr, env) :
    return evaluate(expr, env, verifyBoolValue)

evalExpr = multimethod(defaultProc=lambda x, e : error("invalid expression"))

def initEvalExpr() :
    def f(x, env) : return x
    for t in [PrimitiveType, TupleType, ArrayType, PointerType, RecordType,
              TypeVariable, Value, RefValue] :
        evalExpr.addHandler(f, t)
initEvalExpr()

@evalExpr.register(BoolLiteral)
def foo(x, env) :
    raise NotImplementedError

@evalExpr.register(IntLiteral)
def foo(x, env) :
    raise NotImplementedError

@evalExpr.register(CharLiteral)
def foo(x, env) :
    raise NotImplementedError

@evalExpr.register(NameRef)
def foo(x, env) :
    raise NotImplementedError

@evalExpr.register(Tuple)
def foo(x, env) :
    raise NotImplementedError

@evalExpr.register(Indexing)
def foo(x, env) :
    raise NotImplementedError

@evalExpr.register(Call)
def foo(x, env) :
    raise NotImplementedError

@evalExpr.register(FieldRef)
def foo(x, env) :
    raise NotImplementedError

@evalExpr.register(TupleRef)
def foo(x, env) :
    raise NotImplementedError

@evalExpr.register(Dereference)
def foo(x, env) :
    raise NotImplementedError

@evalExpr.register(AddressOf)
def foo(x, env) :
    raise NotImplementedError



#
# remove temp name used for multimethod instances
#

del foo
