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

# env entries are:
# Record, Procedure, Overloadable, Value, RefValue, Constant
# primitives

class Environment(object) :
    def __init__(self, parent=None) :
        self.parent = parent
        self.entries = {}

    def hasEntry(self, name) :
        return name in self.entries

    def add(self, name, entry) :
        assert type(name) is str
        if name in self.entries :
            error("duplicate definition: %s" % name)
        self.entries[name] = entry

    def lookupInternal(self, name) :
        assert type(name) is str
        entry = self.entries.get(name)
        if (entry is None) and (self.parent is not None) :
            return self.parent.lookupInternal(name)
        return entry

    def lookup(self, name) :
        result = self.lookupInternal(name)
        if result is None :
            error("undefined name: %s" % name)
        return result


def addIdent(env, ident, value) :
    try :
        contextPush(ident)
        env.add(ident.s, value)
    finally :
        contextPop()

def lookupIdent(env, ident, verifier=None) :
    try :
        contextPush(ident)
        result = env.lookup(ident.s)
        if verifier is not None :
            verifier(result)
        return result
    finally :
        contextPop()



#
# Value
#

class Value(object) :
    def __init__(self, type) :
        self.type = type
        self.ctypesType = ctypesType(type)
        self.buf = self.ctypesType()

class RefValue(object) :
    def __init__(self, value, offset=0, type=None) :
        if type is not None :
            self.type = type
        else :
            self.type = value.type
        if isRefValue(value) :
            address = value.address
        elif isValue(value) :
            address = ctypes.addressof(value.buf)
        else :
            assert False
        self.address = address + offset

def isValue(x) : return type(x) is Value
def isRefValue(x) : return type(x) is RefValue



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
    def sizeAsValue(self) :
        if type(self.size) is int :
            return intToValue(self.size)
        return self.size

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
    return sameRecord and typeListEquals(x.typeParams, y.typeParams)

@typeEquals.register(Value, Value)
def foo(x, y) :
    return valueEquals(x, y)



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
    childHashes = tuple([typeHash(t) for t in x.typeParams])
    return hash(("Record", id(x.record), childHashes))

@typeHash.register(Value)
def foo(x) :
    return valueHash(x)



#
# type unification
#

class TypeCell(Type) :
    def __init__(self) :
        super(TypeCell, self).__init__()
        self.type = None

def typeDeref(t) :
    while type(t) is TypeCell :
        t = t.type
    return t

def typeListUnify(a, b) :
    if len(a) != len(b) :
        return False
    for x,y in zip(a,b) :
        if not typeUnify(x,y) :
            return False
    return True

def typeUnifyCells(x, y) :
    if type(x) is TypeCell :
        if x.type is None :
            x.type = y
            return True
        return typeUnify(x.type, y)
    elif type(y) is TypeCell :
        if y.type is None :
            y.type = x
            return True
        return typeUnify(x, y.type)
    return False

typeUnify = multimethod(n=2, defaultProc=typeUnifyCells)

@typeUnify.register(PrimitiveType, PrimitiveType)
def foo(x, y) :
    return x.name == y.name

@typeUnify.register(TupleType, TupleType)
def foo(x, y) :
    return typeListUnify(x.types, y.types)

@typeUnify.register(ArrayType, ArrayType)
def foo(x, y) :
    sizeResult = typeUnify(x.sizeAsValue(), y.sizeAsValue())
    return sizeResult and typeUnify(x.type, y.type)

@typeUnify.register(PointerType, PointerType)
def foo(x, y) :
    return typeUnify(x.type, y.type)

@typeUnify.register(RecordType, RecordType)
def foo(x, y) :
    return (x.record is y.record) and typeListUnify(x.typeParams, y.typeParams)

@typeUnify.register(Value, Value)
def foo(x, y) :
    return valueEquals(x, y)



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
    fieldCTypes = [ctypesType(x) for x in t.types]
    fieldCNames = ["f%d" % x for x in range(len(t.types))]
    ct = type("Tuple", (ctypes.Structure,))
    ct._fields_ = zip(fieldCNames, fieldCTypes)
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
    _ctypesTable[t] = ct
    fieldCTypes = [ctypesType(x) for x in recordFieldTypes(t)]
    fieldCNames = [f.name.s for f in t.record.fields]
    ct._fields_ = zip(fieldCNames, fieldCTypes)
    return ct



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
xregister(TypeCell, lambda x : XObject("TypeCell", x.type))



#
# env entries
#

class Constant(object) :
    def __init__(self, data) :
        assert isValue(data) or isType(data)
        self.data = data



#
# install primitives
#

primitivesEnv = Environment()
primitives = None

def installPrimitives() :
    primClasses = {}
    def entry(name, value) :
        primitivesEnv.add(name, value)
    def constant(name, value) :
        entry(name, Constant(value))
    def primitive(name) :
        primClass = type("Primitive%s" % name, (object,), {})
        primClasses[name] = primClass
        entry(name, primClass())
    def overloadable(name) :
        x = Overloadable(Identifier(name))
        x.env = primitivesEnv
        entry(name, x)

    constant("Bool", boolType)
    constant("Int", intType)
    constant("Float", floatType)
    constant("Double", doubleType)
    constant("Char", charType)

    primitive("Tuple")
    primitive("Array")
    primitive("Pointer")

    primitive("default")
    primitive("typeSize")

    primitive("addressOf")
    primitive("pointerDereference")
    primitive("pointerOffset")
    primitive("pointerSubtract")
    primitive("pointerCast")
    primitive("pointerCopy")
    primitive("pointerEquals")
    primitive("pointerLesser")
    primitive("allocateMemory")
    primitive("freeMemory")

    primitive("TupleType")
    primitive("tuple")
    primitive("tupleFieldCount")
    primitive("tupleFieldRef")

    primitive("array")
    primitive("arrayRef")

    primitive("RecordType")
    primitive("makeRecord")
    primitive("recordFieldCount")
    primitive("recordFieldRef")

    primitive("boolCopy")
    primitive("boolNot")

    primitive("charCopy")
    primitive("charEquals")
    primitive("charLesser")

    primitive("intCopy")
    primitive("intEquals")
    primitive("intLesser")
    primitive("intAdd")
    primitive("intSubtract")
    primitive("intMultiply")
    primitive("intDivide")
    primitive("intModulus")
    primitive("intNegate")

    primitive("floatCopy")
    primitive("floatEquals")
    primitive("floatLesser")
    primitive("floatAdd")
    primitive("floatSubtract")
    primitive("floatMultiply")
    primitive("floatDivide")
    primitive("floatNegate")

    primitive("doubleCopy")
    primitive("doubleEquals")
    primitive("doubleLesser")
    primitive("doubleAdd")
    primitive("doubleSubtract")
    primitive("doubleMultiply")
    primitive("doubleDivide")
    primitive("doubleNegate")

    primitive("charToInt")
    primitive("intToChar")
    primitive("floatToInt")
    primitive("intToFloat")
    primitive("floatToDouble")
    primitive("doubleToFloat")
    primitive("doubleToInt")
    primitive("intToDouble")

    overloadable("init")
    overloadable("destroy")
    overloadable("assign")
    overloadable("equals")
    overloadable("lesser")
    overloadable("lesserEquals")
    overloadable("greater")
    overloadable("greaterEquals")
    overloadable("hash")

    Primitives = type("Primitives", (object,), primClasses)
    global primitives
    primitives = Primitives()

installPrimitives()



#
# temp values
#

_tempValues = []
_savedTempValueSets = []

def pushTempValueSet() :
    global _tempValues
    _savedTempValueSets.append(_tempValues)
    _tempValues = []

def popTempValueSet(old) :
    global _tempValues
    for temp in _tempValues :
        valueDestroy(temp)
    _tempValues = _savedTempValueSets.pop()

def tempValue(type) :
    v = Value(type)
    _tempValues.append(v)
    return v

def clearTempValues() :
    del _tempValues[:]



#
# value operations
#

def valueInit(a) :
    init = NameRef(Identifier("init"))
    call = Call(init, [a])
    evaluate(call, primitivesEnv)

def valueInitCopy(dest, src) :
    init = NameRef(Identifier("init"))
    call = Call(init, [dest, src])
    evaluate(call, primitivesEnv)

def valueDestroy(a) :
    destroy = NameRef(Identifier("destroy"))
    call = Call(destroy, [a])
    evaluate(call, primitivesEnv)

def valueAssign(dest, src) :
    assign = NameRef(Identifier("assign"))
    call = Call(assign, [dest, src])
    evaluate(call, primitivesEnv)

def valueEquals(a, b) :
    equals = NameRef(Identifier("equals"))
    call = Call(equals, [a, b])
    return evaluateAsBoolValue(call, primitivesEnv)

def valueHash(a) :
    hash = NameRef(Identifier("hash"))
    call = Call(hash, [a])
    return evaluateAsIntValue(call, primitivesEnv)

def valueToRef(a) :
    return RefValue(a)

def refToValue(a) :
    v = tempValue(a.type)
    valueInitCopy(v, a)
    return v

def valueToInt(a) :
    assert isValue(a) and isIntType(a.type)
    return a.buf.value

def intToValue(i) :
    v = tempValue(intType)
    v.buf.value = i
    return v

def valueToBool(a) :
    assert isValue(a) and isBoolType(a.type)
    return a.buf.value

def boolToValue(b) :
    v = tempValue(boolType)
    v.buf.value = b
    return v

def valueToChar(a) :
    assert isValue(a) and isCharType(a.type)
    return a.buf.value

def charToValue(c) :
    v = tempValue(charType)
    v.buf.value = c
    return v

def tupleFieldRef(a, i) :
    assert isRefValue(a) and isTupleType(a.type)
    fieldName = "f%d" % i
    ctypesField = getattr(ctypesType(a.type), fieldName)
    fieldType = a.type.types[i]
    return RefValue(a, ctypesField.offset, fieldType)

def makeTuple(argValueRefs) :
    valueType = TupleType([x.type for x in argValueRefs])
    value = tempValue(valueType)
    valueRef = RefValue(value)
    for i, arg in enumerate(argValueRefs) :
        left = tupleFieldRef(valueRef, i)
        valueInitCopy(left, arg)
    return value

def recordFieldRef(a, i) :
    assert isRefValue(a) and isRecordType(a.type)
    fieldName = a.type.record.fields[i].name.s
    fieldType = recordFieldTypes(a.type)[i]
    ctypesField = getattr(ctypesType(a.type), fieldName)
    return RefValue(a, ctypesField.offset, fieldType)

def recordFieldIndex(recordType, ident) :
    for i, f in enumerate(recordType.record.fields) :
        if f.name.s == ident.s :
            return i
    error("record field not found: %s" % ident.s)

def makeRecord(recordType, argValueRefs) :
    ensureArity(argValueRefs, len(recordType.record.fields))
    value = tempValue(recordType)
    valueRef = RefValue(value)
    for i, argValueRef in enumerate(argValueRefs) :
        left = recordFieldRef(valueRef, i)
        valueInitCopy(left, argValueRef)
    return value



#
# build top level env
#

def buildTopLevelEnv(program) :
    env = Environment(primitivesEnv)
    for item in program.topLevelItems :
        item.env = env
        if type(item) is Overload :
            installOverload(env, item)
        else :
            addIdent(env, item.name, item)
    return env

def installOverload(env, item) :
    def verifyOverloadable(x) :
        if type(x) is not Overloadable :
            error("invalid overloadable")
    entry = lookupIdent(env, item.name, verifyOverloadable)
    entry.overloads.insert(0, item)



#
# eval utilities
#

def ensureArity(someList, n) :
    if len(someList) != n :
        error("incorrect no. of arguments")

def ensureMinArity(someList, n) :
    if len(someList) < n :
        error("insufficient no. of arguments")

def bindTypeVars(parentEnv, typeVars) :
    env = Environment(parentEnv)
    typeCells = []
    for typeVar in typeVars :
        typeCell = TypeCell()
        typeCells.append(typeCell)
        addIdent(env, typeVar, Constant(typeCell))
    return env, typeCells

def resolveTypeVar(typeVar, typeValue) :
    try :
        contextPush(typeVar)
        t = typeDeref(typeValue)
        if t is None :
            error("unresolved type var")
        return t
    finally :
        contextPop()

def resolveTypeVars(typeVars, typeCells) :
    return [resolveTypeVar(v, c) for v, c in zip(typeVars, typeCells)]

def bindTypeParams(parentEnv, typeVars, typeValues) :
    env = Environment(parentEnv)
    for typeVar, typeValue in zip(typeVars, typeValues) :
        result = resolveTypeVar(typeVar, typeValue)
        addIdent(env, typeVar, Constant(result))
    return env

def evalFieldTypes(fields, env) :
    return [evaluateAsType(f.type, env) for f in fields]

_recordFieldTypes = {}
# assumes recordType is a complete type
def recordFieldTypes(recordType) :
    fieldTypes = _recordFieldTypes.get(recordType)
    if fieldTypes is None :
        record = recordType.record
        typeParams = recordType.typeParams
        env = bindTypeParams(record.env, record.typeVars, typeParams)
        fieldTypes = evalFieldTypes(record.fields, env)
        _recordFieldTypes[recordType] = fieldTypes
    return fieldTypes

def matchType(expr, exprType, typePattern) :
    try :
        contextPush(expr)
        if not typeUnify(exprType, typePattern) :
            error("type mismatch")
    finally :
        contextPop()



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

def isNonVoid(x) :
    return x is not None

def verifyNonVoid(x) :
    if not isNonVoid(x) :
        error("unexpected void expression")
    return x

def verifyType(x) :
    if not isType(x) :
        error("type expected")
    return x

def isAssignable(x) :
    return isRefValue(x)

def verifyAssignable(x) :
    if not isAssignable(x) :
        error("reference expected")
    return x

def toRefParam(x) :
    if isRefValue(x) :
        return x
    if isValue(x) :
        return valueToRef(x)
    return None

def verifyRefParam(x) :
    result = toRefParam(x)
    if result is None :
        error("reference/value expected")
    return result

def toValue(x) :
    if isValue(x) :
        return x
    if isRefValue(x) :
        return refToValue(x)
    return None

def verifyValue(x) :
    result = toValue(x)
    if result is None :
        error("value expected")
    return result

def toTypeParam(x) :
    if isRefValue(x) :
        return refToValue(x)
    return x

def verifyTypeParam(x) :
    result = toTypeParam(x)
    if result is None :
        error("type/value expected")
    return x

def toIntValue(x) :
    if isRefValue(x) :
        x = refToValue(x)
    if isValue(x) and isIntType(x.type) :
        return valueToInt(x)
    return None

def verifyIntValue(x) :
    result = toIntValue(x)
    if result is None :
        error("int value expected")
    return result

def toBoolValue(x) :
    if isRefValue(x) :
        x = refToValue(x)
    if isValue(x) and isBoolType(x.type) :
        return valueToBool(x)
    return None

def verifyBoolValue(x) :
    result = toBoolValue(x)
    if result is None :
        error("bool value expected")
    return result

def evaluateNonVoid(expr, env) :
    return evaluate(expr, env, verifyNonVoid)

def evaluateAsType(expr, env) :
    return evaluate(expr, env, verifyType)

def evaluateAsAssignable(expr, env) :
    return evaluate(expr, env, verifyAssignable)

def evaluateAsRefParam(expr, env) :
    return evaluate(expr, env, verifyRefParam)

def evaluateAsValue(expr, env) :
    return evaluate(expr, env, verifyValue)

def evaluateAsTypeParam(expr, env) :
    return evaluate(expr, env, verifyTypeParam)

def evaluateAsIntValue(expr, env) :
    return evaluate(expr, env, verifyIntValue)

def evaluateAsBoolValue(expr, env) :
    return evaluate(expr, env, verifyBoolValue)



#
# evalExpr
#

def badExpression(*args) :
    error("invalid expression")

evalExpr = multimethod(defaultProc=badExpression)

@evalExpr.register(Value)
def foo(x, env) :
    return x

@evalExpr.register(RefValue)
def foo(x, env) :
    return x

@evalExpr.register(Constant)
def foo(x, env) :
    return x.data

@evalExpr.register(BoolLiteral)
def foo(x, env) :
    return boolToValue(x.value)

@evalExpr.register(IntLiteral)
def foo(x, env) :
    return intToValue(x.value)

@evalExpr.register(CharLiteral)
def foo(x, env) :
    return charToValue(x.value)

@evalExpr.register(NameRef)
def foo(x, env) :
    return evalNameRef(lookupIdent(env, x.name))

@evalExpr.register(Tuple)
def foo(x, env) :
    assert len(x.args) > 0
    first = evaluateNonVoid(x.args[0], env)
    if len(x.args) == 1 :
        return first
    if isType(first) :
        rest = [evaluateAsType(arg, env) for arg in x.args[1:]]
        return TupleType([first] + rest)
    else :
        args = [verifyRefParam(first)]
        args.extend([evaluateAsRefParam(arg, env) for arg in x.args[1:]])
        return makeTuple(args)

@evalExpr.register(Indexing)
def foo(x, env) :
    if type(x.expr) is NameRef :
        entry = lookupIdent(env, x.expr.name)
        handler = evalNamedIndexing.getHandler(type(entry))
        if handler is not None :
            return handler(entry, x.args, env)
    error("invalid expression")

@evalExpr.register(Call)
def foo(x, env) :
    if type(x.expr) is NameRef :
        entry = lookupIdent(env, x.expr.name)
        handler = evalNamedCall.getHandler(type(entry))
        if handler is not None :
            return handler(entry, x.args, env)
    recType = evaluateAsType(x.expr, env)
    if not isRecordType(recType) :
        error("invalid expression")
    argValueRefs = [evaluateAsRefParam(arg, env) for arg in x.args]
    return makeRecord(recType, argValueRefs)

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
# evalNameRef
#

evalNameRef = multimethod(defaultProc=badExpression)

@evalNameRef.register(Value)
def foo(x) :
    return valueToRef(x)

@evalNameRef.register(RefValue)
def foo(x) :
    return x

@evalNameRef.register(Constant)
def foo(x) :
    if isType(x.data) :
        return x.data
    elif isValue(x.data) :
        a = tempValue(x.data.type)
        valueInitCopy(a, x.data)
        return a
    assert False

@evalNameRef.register(Record)
def foo(x) :
    if len(x.typesVars) > 0 :
        error("record type requires type parameters")
    return RecordType(x, [])



#
# evalNamedIndexing
#

evalNamedIndexing = multimethod(defaultProc=badExpression)

@evalNamedIndexing.register(Record)
def foo(x, args, env) :
    ensureArity(args, len(x.typeVars))
    typeParams = [evaluateAsTypeParam(y, env) for y in args]
    return RecordType(x, typeParams)

@evalNamedIndexing.register(primitives.Tuple)
def foo(x, args, env) :
    ensureMinArity(args, 2)
    types = [evaluateAsType(y, env) for y in args]
    return TupleType(types)

@evalNamedIndexing.register(primitives.Array)
def foo(x, args, env) :
    ensureArity(args, 2)
    elementType = evaluateAsType(args[0], env)
    def verifyArraySize(y) :
        if isType(y) and (type(y) is TypeCell) :
            return y
        if isValue(y) and isIntType(y.type) :
            return valueToInt(y)
        if isRefValue(y) and isIntType(y.type) :
            return valueToInt(refToValue(y))
        error("invalid array size parameter")
    size = evaluate(args[1], env, verifyArraySize)
    return ArrayType(elementType, size)

@evalNamedIndexing.register(primitives.Pointer)
def foo(x, args, env) :
    ensureArity(args, 1)
    targetType = evaluateAsType(args[0], env)
    return PointerType(targetType)



#
# evalNamedCall
#

evalNamedCall = multimethod(defaultProc=badExpression)

@evalNamedCall.register(Record)
def foo(x, args, env) :
    recEnv, typeCells = bindTypeVars(x.env, x.typeVars)
    fieldTypePatterns = evalFieldTypes(x.fields, recEnv)
    ensureArity(args, len(x.fields))
    argValueRefs = [evaluateAsRefParam(arg, env) for arg in args]
    for i, typePattern in enumerate(fieldTypePatterns) :
        matchType(args[i], argValueRefs[i].type, typePattern)
    typeParams = resolveTypeVars(x.typeVars, typeCells)
    recType = RecordType(x, typeParams)
    return makeRecord(recType, argValueRefs)

@evalNamedCall.register(Procedure)
def foo(x, args, env) :
    argResults = [evaluate(arg, env) for arg in args]
    result = matchCodeSignature(x.env, x.code, argResults)
    if type(result) is MatchFailure :
        result.signalError(x, args)
    assert type(result) is Environment
    procEnv = result
    return evalCodeBody(x.code, procEnv)

@evalNamedCall.register(Overloadable)
def foo(x, args, env) :
    argResults = [evaluate(arg, env) for arg in args]
    for y in x.overloads :
        result = matchCodeSignature(y.env, y.code, argResults)
        if type(result) is MatchFailure :
            continue
        assert type(result) is Environment
        procEnv = result
        return evalCodeBody(y.code, procEnv)
    error("no matching overload")

@evalNamedCall.register(Constant)
def foo(x, args, env) :
    if not isRecordType(x.data) :
        error("invalid expression")
    recType = x.data
    argValueRefs = [evaluateAsRefParam(y, env) for y in args]
    return makeRecord(recType, argValueRefs)

@evalNamedCall.register(primitives.default)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.typeSize)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.addressOf)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.pointerDereference)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.pointerOffset)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.pointerSubtract)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.pointerCast)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.pointerCopy)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.pointerEquals)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.pointerLesser)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.allocateMemory)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.freeMemory)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.TupleType)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.tuple)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.tupleFieldCount)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.tupleFieldRef)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.array)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.arrayRef)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.RecordType)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.makeRecord)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.recordFieldCount)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.recordFieldRef)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.boolCopy)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.boolNot)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.charCopy)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.charEquals)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.charLesser)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.intCopy)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.intEquals)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.intLesser)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.intAdd)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.intSubtract)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.intMultiply)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.intDivide)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.intModulus)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.intNegate)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.floatCopy)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.floatEquals)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.floatLesser)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.floatAdd)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.floatSubtract)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.floatMultiply)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.floatDivide)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.floatNegate)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.doubleCopy)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.doubleEquals)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.doubleLesser)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.doubleAdd)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.doubleSubtract)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.doubleMultiply)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.doubleDivide)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.doubleNegate)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.charToInt)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.intToChar)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.floatToInt)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.intToFloat)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.floatToDouble)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.doubleToFloat)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.doubleToInt)
def foo(x, args, env) :
    raise NotImplementedError

@evalNamedCall.register(primitives.intToDouble)
def foo(x, args, env) :
    raise NotImplementedError



#
# matchCodeSignature (env, code, args) -> Environment | MatchFailure
#

class MatchFailure(object) :
    def __init__(self, message, argOffset=None, typeCondOffset=None) :
        self.message = message
        self.argOffset = argOffset
        self.typeCondOffset = typeCondOffset

    def getContext(self, proc, args) :
        if self.argOffset is not None :
            return args[self.argOffset]
        elif self.typeCondOffset is not None :
            return proc.code.typeConditions[self.typeCondOffset]
        return None

    def signalError(self, proc, args) :
        context = self.getContext(proc, args)
        try :
            if context is not None :
                contextPush(context)
            error(self.message)
        finally :
            if context is not None :
                contextPop()

def matchCodeSignature(env, code, args) :
    def argMismatch(i) :
        return MatchFailure("argument mismatch", argOffset=i)
    def typeCondFailure(i) :
        return MatchFailure("type condition failed", typeCondOffset=i)
    if len(args) != len(code.formalArgs) :
        return MatchFailure("incorrect no. of arguments")
    procEnv, typeCells = bindTypeVars(env, code.typeVars)
    bindings = []
    for i, formalArg in enumerate(code.formalArgs) :
        arg = args[i]
        if type(formalArg) is ValueArgument :
            if formalArg.byRef :
                arg = toRefParam(arg)
            else :
                arg = toValue(arg)
            if arg is None :
                return argMismatch(i)
            if formalArg.type is not None :
                typePattern = evaluateAsType(formalArg.type, procEnv)
                if not typeUnify(typePattern, arg.type) :
                    return argMismatch(i)
            bindings.append((formalArg.name, arg))
        elif type(formalArg) is StaticArgument :
            arg = toTypeParam(arg)
            if arg is None :
                return argMismatch(i)
            typePattern = evaluateAsType(formalArg.type, procEnv)
            if not typeUnify(typePattern, arg) :
                return argMismatch(i)
        else :
            assert False
    typeValues = resolveTypeVars(code.typeVars, typeCells)
    procEnv = bindTypeParams(env, code.typeVars, typeValues)
    for i, typeCondition in enumerate(code.typeConditions) :
        result = evaluateAsBoolValue(typeCondition, procEnv)
        if not result :
            return typeCondFailure(i)
    for ident, value in bindings :
        addIdent(procEnv, ident, value)
    return procEnv



#
# evalCodeBody
#

def evalCodeBody(code, env) :
    returnType = None
    if code.returnType is not None :
        returnType = evaluateAsType(code.returnType, env)
    context = CodeContext(code.returnByRef, returnType)
    result = evalInnerStatement(code.block, env, context)
    if type(result) is ReturnValue :
        result = result.thing
    return result



#
# evalStatement : (stmt, env, code) -> ReturnValue(Type|Value|RefValue|None)
#                                    | Environment
#                                    | None
#

class CodeContext(object) :
    def __init__(self, returnByRef, returnType) :
        self.returnByRef = returnByRef
        self.returnType = returnType

class ReturnValue(object) :
    def __init__(self, thing) :
        self.thing = thing

def evalStatement(stmt, env, context) :
    try :
        contextPush(stmt)
        pushTempValueSet()
        return evalStmt(stmt, env, context)
    finally :
        popTempValueSet()
        contextPop()

def evalInnerStatement(stmt, env, context) :
    result = evalStatement(stmt, env, context)
    if type(result) is Environment :
        result = None
    return result

evalStmt = multimethod()

@evalStmt.register(Block)
def foo(x, env, context) :
    for statement in x.statements :
        result = evalStatement(statement, env, context)
        if type(result) is Environment :
            env = result
        if type(result) is ReturnValue :
            return result

@evalStmt.register(Assignment)
def foo(x, env, context) :
    assignable = evaluateAsAssignable(x.assignable, env)
    right = evaluateAsRefParam(x.expr, env)
    valueAssign(assignable, right)

@evalStmt.register(LocalBinding)
def foo(x, env, context) :
    newEnv = Environment(env)
    refValue = evaluateAsRefParam(x.expr, env)
    if x.type is not None :
        declaredType = evaluateAsType(x.type, env)
        if not typeEquals(declaredType, refValue.type) :
            error("type mismatch")
    value = Value(refValue.type)
    valueInitCopy(value, refValue)
    addIdent(newEnv, x.name, value)
    return newEnv

@evalStmt.register(Return)
def foo(x, env, context) :
    result = None
    if context.returnByRef :
        if x.expr is None :
            error("return value expected")
        result = evaluateAsAssignable(x.expr, env)
    elif x.expr is not None :
        result = evaluateAsTypeParam(x.expr, env)
    if context.returnType is not None :
        if not (isValue(result) or isRefValue(result)) :
            error("return value expected")
        if not typeEquals(context.returnType, result.type) :
            error("return type mismatch")
    return ReturnValue(result)

@evalStmt.register(IfStatement)
def foo(x, env, context) :
    if evaluateAsBoolValue(x.condition, env) :
        return evalInnerStatement(x.thenPart, env, context)
    elif x.elsePart is not None :
        return evalInnerStatement(x.elsePart, env, context)

@evalStmt.register(ExprStatement)
def foo(x, env, context) :
    evaluate(x.expr, env)



#
# remove temp name used for multimethod instances
#

del foo
