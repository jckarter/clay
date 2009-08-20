import ctypes
from clay.multimethod import multimethod as orig_multimethod
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

def ensure(cond, msg) :
    if not cond :
        error(msg)

def ensureArity(args, n) :
    ensure(len(args) == n, "incorrect no. arguments")



#
# multimethod
#

def multimethod(n=1, errorMessage=None, defaultProc=None) :
    if errorMessage is not None :
        assert defaultProc is None
        defaultProc = lambda *args : error(errorMessage)
    if defaultProc is not None :
        return orig_multimethod(n, defaultProc=defaultProc)
    return orig_multimethod(n)



#
# types
#

class Type(object) :
    def __init__(self, tag, params) :
        self.tag = tag
        self.params = params
    def __eq__(self, other) :
        return typeEquals(self, other)
    def __hash__(self) :
        return typeHash(self)

class BoolTypeTag(object) : pass
class IntTypeTag(object) : pass
class FloatTypeTag(object) : pass
class DoubleTypeTag(object) : pass
class CharTypeTag(object) : pass
class VoidTypeTag(object) : pass
class TupleTypeTag(object) : pass
class ArrayTypeTag(object) : pass
class PointerTypeTag(object) : pass

boolTypeTag = BoolTypeTag()
intTypeTag = IntTypeTag()
floatTypeTag = FloatTypeTag()
doubleTypeTag = DoubleTypeTag()
charTypeTag = CharTypeTag()
voidTypeTag = VoidTypeTag()

tupleTypeTag = TupleTypeTag()
arrayTypeTag = ArrayTypeTag()
pointerTypeTag = PointerTypeTag()

boolType = Type(boolTypeTag, [])
intType = Type(intTypeTag, [])
floatType = Type(floatTypeTag, [])
doubleType = Type(doubleTypeTag, [])
charType = Type(charTypeTag, [])
voidType = Type(voidTypeTag, [])

def tupleType(types) :
    assert len(types) >= 2
    return Type(tupleTypeTag, types)

def arrayType(type_, sizeValue) :
    return Type(arrayTypeTag, [type_, sizeValue])

def pointerType(type_) :
    return Type(pointerTypeTag, [type_])

def recordType(record, params) :
    return Type(record, params)



#
# type predicates
#

def isType(t) : return type(t) is Type
def isBoolType(t) : return t.tag is boolTypeTag
def isIntType(t) : return t.tag is intTypeTag
def isFloatType(t) : return t.tag is floatTypeTag
def isDoubleType(t) : return t.tag is doubleTypeTag
def isCharType(t) : return t.tag is charTypeTag
def isVoidType(t) : return t.tag is voidTypeTag

def isTupleType(t) : return t.tag is tupleTypeTag
def isArrayType(t) : return t.tag is arrayTypeTag
def isPointerType(t) : return t.tag is pointerTypeTag
def isRecordType(t) : return type(t.tag) is Record



#
# typeEquals, typeHash
#

def typeEquals(a, b) :
    assert isType(a) and isType(b)
    return (a.tag is b.tag) and listEquals(a.params, b.params)


def typeHash(a) :
    assert isType(a)
    hash((a.tag, listHash(a.params)))



#
# convert to ctypes
#

_ctypesTable = {}
_ctypesTable[boolType] = ctypes.c_bool
_ctypesTable[intType] = ctypes.c_int
_ctypesTable[floatType] = ctypes.c_float
_ctypesTable[doubleType] = ctypes.c_double
_ctypesTable[charType] = ctypes.c_wchar
_ctypesTable[voidType] = ctypes.c_bool

def ctypesType(t) :
    ct = _ctypesTable.get(t)
    if ct is None :
        ct = makeCTypesType(t.tag, t)
    return ct

makeCTypesType = multimethod(errorMessage="invalid type tag")

@makeCTypesType.register(TupleTypeTag)
def foo(tag, t) :
    fieldCTypes = [ctypesType(toType(x)) for x in t.params]
    fieldCNames = ["f%d" % x for x in range(len(t.params))]
    ct = type("Tuple", (ctypes.Structure,))
    ct._fields_ = zip(fieldCNames, fieldCTypes)
    _ctypesTable[t] = ct
    return ct

@makeCTypesType.register(ArrayTypeTag)
def foo(tag, t) :
    ensure(len(t.params) == 2, "invalid array type")
    ct = ctypesType(toType(t.params[0])) * toInt(t.params[1])
    _ctypesTable[t] = ct
    return ct

@makeCTypesType.register(PointerTypeTag)
def foo(tag, t) :
    ensure(len(t.params) == 1, "invalid pointer type")
    ct = ctypes.POINTER(ctypesType(toType(t.params[0])))
    _ctypesTable[t] = ct
    return ct

@makeCTypesType.register(Record)
def foo(tag, t) :
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


xregister(Type, lambda t : (XObject(tagName(t), *t.params,
                                    opening="[", closing="[")
                            if t.params
                            else XSymbol(tagName(t))))

def tagName(t) :
    if type(t.tag) is Record :
        return t.tag.name.s
    return _tagNames[t.tag]

_tagNames = {}
_tagNames[boolTypeTag] = "Bool"
_tagNames[intTypeTag] = "Int"
_tagNames[floatTypeTag] = "Float"
_tagNames[doubleTypeTag] = "Double"
_tagNames[charTypeTag] = "Char"
_tagNames[voidTypeTag] = "Void"
_tagNames[tupleTypeTag] = "Tuple"
_tagNames[arrayTypeTag] = "Array"
_tagNames[pointerTypeTag] = "Pointer"



#
# Value, Reference
#

class Value(object) :
    def __init__(self, type_) :
        self.type = type_
        self.ctypesType = ctypesType(type_)
        self.buf = self.ctypesType()
    def __eq__(self, other) :
        return equals(self, other)
    def __hash__(self) :
        return valueHash(self)

class Reference(object) :
    def __init__(self, type_, address) :
        self.type = type_
        self.address = address
    def __eq__(self, other) :
        return equals(self, other)
    def __hash__(self) :
        return valueHash(self)

def isValue(x) : return type(x) is Value
def isReference(x) : return type(x) is Reference



#
# VoidValue, voidValue
#

class VoidValue(object) :
    pass

voidValue = VoidValue()



#
# toInt
#

toInt = multimethod(errorMessage="invalid int")

toInt.register(Value)
def foo(x) :
    ensure(x.type == intType, "not int type")
    return x.buf.value

toInt.register(Reference)
def foo(x) :
    return toInt(toValue(x))



#
# toBool
#

toBool = multimethod(errorMessage="invalid bool")

toBool.register(Value)
def foo(x) :
    ensure(x.type == boolType, "not bool type")
    return x.buf.value

toBool.register(Reference)
def foo(x) :
    return toBool(toValue(x))



#
# toValue
#

toValue = multimethod(errorMessage="invalid value")

toValue.register(bool)
def foo(x) :
    v = tempValue(boolType)
    v.buf.value = x
    return v

toValue.register(int)
def foo(x) :
    v = tempValue(intType)
    v.buf.value = x
    return v

toValue.register(Reference)
def foo(x) :
    v = tempValue(x.type)
    valueCopy(v, x)
    return v

toValue.register(Value)
def foo(x) :
    return x



#
# toLValue
#

toLValue = multimethod(errorMessage="invalid l-value")

toLValue.register(Reference)
def foo(x) :
    return x



#
# toReference
#

toReference = multimethod(errorMessage="invalid reference")

toReference.register(Value)
def foo(x) :
    return Reference(x.type, ctypes.addressof(x.buf))

toReference.register(Reference)
def foo(x) :
    return x



#
# toType
#

toType = multimethod(errorMessage="invalid type")

toType.register(Type)
def foo(x) :
    return x



#
# toTypeOrValue
#

toTypeOrValue = multimethod(errorMessage="invalid type param")

toTypeOrValue.register(Value)
def foo(x) :
    return x

toTypeOrValue.register(Reference)
def foo(x) :
    return toValue(x)

toTypeOrValue.register(Type)
def foo(x) :
    return x



#
# toValueOrCell, toTypeOrCell, toTypeOrValueOrCell
#

toValueOrCell = multimethod(defaultProc=toValue)
toValueOrCell.register(Cell)(lambda x : x)

toTypeOrCell = multimethod(defaultProc=toType)
toTypeOrCell.register(Cell)(lambda x : x)

toTypeOrValueOrCell = multimethod(defaultProc=toTypeOrValue)
toTypeOrValueOrCell.register(Cell)(lambda x : x)



#
# equals
#

equals = multimethod(n=2, errorMessage="invalid equals test")

equals.register(Value, Value)
def foo(a, b) :
    return valueEquals(a, b)

equals.register(Value, Reference)
def foo(a, b) :
    return valueEquals(a, b)

equals.register(Reference, Value)
def foo(a, b) :
    return valueEquals(a, b)

equals.register(Reference, Reference)
def foo(a, b) :
    return valueEquals(a, b)

equals.register(Type, Type)
def foo(a, b) :
    return typeEquals(a, b)

def listEquals(a, b) :
    for x, y in zip(a, b) :
        if not equals(x, y) :
            return False
    return True

def listHash(a) :
    return hash(tuple(map(hash, a)))



#
# core value operations
#

def callBuiltin(builtinName, args, converter=None) :
    env = Environment(primitivesEnv)
    variables = [Identifier("v%d" % i) for i in range(len(args))]
    for variable, arg in zip(variables, args) :
        addIdent(env, variable, arg)
    argNames = map(NameRef, variables)
    builtin = NameRef(Identifier(builtinName))
    call = Call(builtin, argNames)
    if converter is None :
        return evaluate(call, env)
    return evaluate(call, env, converter)

def valueInit(a) :
    callBuiltin("init", [a])

def valueCopy(dest, src) :
    callBuiltin("copy", [dest, src])

def valueAssign(dest, src) :
    callBuiltin("assign", [dest, src])

def valueDestroy(a) :
    callBuiltin("destroy", [a])

def valueEquals(a, b) :
    return callBuiltin("equals", [a, b], toBool)

def valueHash(a) :
    return callBuiltin("hash", [a], toInt)



#
# unification
#

class Cell(object) :
    def __init__(self, param) :
        self.param = param

def isCell(x) :
    return type(x) is Cell

def unify(a, b) :
    assert type(a) in (Value, Type, Cell)
    assert type(b) in (Value, Type, Cell)
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
    if isValue(a) and isValue(b) :
        return equals(a, b)
    if isType(a) and isType(b) :
        if a.tag is not b.tag :
            return False
        for x, y in zip(a.params, b.params) :
            if not unify(x, y) :
                return False
        return True
    return False

def cellDeref(a) :
    while type(a) is Cell :
        a = a.param
    return a



#
# Environment
#

# env entries are:
# Record, Procedure, Overloadable, Value, Reference, Type, Cell,
# and primitives

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
# install primitives
#

primitivesEnv = Environment()
primitives = None

def installPrimitives() :
    primClasses = {}
    def entry(name, value) :
        primitivesEnv.add(name, value)
    def primitive(name) :
        primClass = type("Primitive%s" % name, (object,), {})
        primClasses[name] = primClass
        entry(name, primClass())
    def overloadable(name) :
        x = Overloadable(Identifier(name))
        x.env = primitivesEnv
        entry(name, x)

    entry("Bool", boolType)
    entry("Int", intType)
    entry("Float", floatType)
    entry("Double", doubleType)
    entry("Char", charType)
    entry("Void", voidType)

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
    overloadable("copy")
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
    for temp in reversed(_tempValues) :
        valueDestroy(temp)
    _tempValues = _savedTempValueSets.pop()

def tempValue(type_) :
    v = Value(type_)
    _tempValues.append(v)
    return v



#
# bindings
#

def bindVariables(parentEnv, variables, objects) :
    env = Environment(parentEnv)
    for variable, object_ in zip(variables, objects) :
        addIdent(env, variable, object_)
    return env

def bindTypeVars(parentEnv, typeVars) :
    cells = [Cell(None) for _ in typeVars]
    env = bindVariables(parentEnv, typeVars, cells)
    return env, cells

def matchPattern(pattern, typeParam, context) :
    try :
        contextPush(context)
        if not unify(pattern, typeParam) :
            error("type mismatch")
    finally :
        contextPop()

def matchType(typeA, typeB, context) :
    try :
        contextPush(context)
        if not typeEquals(typeA, typeB) :
            error("type mismatch")
    finally :
        contextPop()

def resolveTypeVar(typeVar, cell) :
    try :
        contextPush(typeVar)
        t = cellDeref(cell)
        if t is None :
            error("unresolved type var")
        return t
    finally :
        contextPop()

def resolveTypeVars(typeVars, cells) :
    typeValues = []
    for typeVar, cell in zip(typeVars, cells) :
        typeValues.append(resolveTypeVar(typeVar, cell))
    return typeValues



#
# tuples, records
#

def tupleFieldRef(a, i) :
    assert isReference(a) and isTupleType(a.type)
    fieldName = "f%d" % i
    ctypesField = getattr(ctypesType(a.type), fieldName)
    fieldType = toType(a.type.params[i])
    return Reference(fieldType, a.address + ctypesField.offset)

def makeTuple(argRefs) :
    t = tupleType([x.type for x in argRefs])
    value = tempValue(t)
    valueRef = toReference(value)
    for i, argRef in enumerate(argRefs) :
        left = tupleFieldRef(valueRef, i)
        valueCopy(left, argRef)
    return value

def recordFieldRef(a, i) :
    assert isReference(a) and isRecordType(a.type)
    fieldName = a.type.tag.fields[i].name.s
    fieldType = recordFieldTypes(a.type)[i]
    ctypesField = getattr(ctypesType(a.type), fieldName)
    return Reference(fieldType, a.address + ctypesField.offset)

def recordFieldIndex(recType, ident) :
    for i, f in enumerate(recType.tag.fields) :
        if f.name.s == ident.s :
            return i
    error("record field not found: %s" % ident.s)

def makeRecord(recType, argRefs) :
    ensureArity(argRefs, len(recType.tag.fields))
    value = tempValue(recType)
    valueRef = toReference(value)
    for i, argRef in enumerate(argRefs) :
        left = recordFieldRef(valueRef, i)
        valueCopy(left, argRef)
    return value

def computeFieldTypes(fields, env) :
    typeExprs = [f.type for f in fields]
    fieldTypes = [evaluate(x, env, toTypeOrCell) for x in typeExprs]
    return fieldTypes

_recordFieldTypes = {}
def recordFieldTypes(recType) :
    assert isType(recType)
    fieldTypes = _recordFieldTypes.get(recType)
    if fieldTypes is None :
        record = recType.tag
        env = bindVariables(record.env, record.typeVars, recType.params)
        fieldTypes = computeFieldTypes(record.fields, env)
        _recordFieldTypes[recType] = fieldTypes
    return fieldTypes



#
# evaluate
#

def evaluate(expr, env, converter) :
    try :
        contextPush(expr)
        result = evalExpr(expr, env)
        if converter is not None :
            result = converter(result)
        return result
    finally :
        contextPop()



#
# evalExpr
#

evalExpr = multimethod(errorMessage="invalid expression")

@evalExpr.register(BoolLiteral)
def foo(x, env) :
    return toValue(x.value)


@evalExpr.register(IntLiteral)
def foo(x, env) :
    return toValue(x.value)

@evalExpr.register(CharLiteral)
def foo(x, env) :
    v = tempValue(charType)
    v.buf.value = x.value
    return v

@evalExpr.register(NameRef)
def foo(x, env) :
    return lookupIdent(env, x.name)

@evalExpr.register(Tuple)
def foo(x, env) :
    assert len(x.args) > 0
    first = evaluate(x.args[0], env)
    if len(x.args) == 1 :
        return first
    if isType(first) or isCell(first) :
        rest = [evaluate(y, env, toTypeOrCell) for y in x.args[1:]]
        return tupleType([first] + rest)
    else :
        args = [toReference(first)]
        args.extend([evaluate(y, env, toReference) for y in x.args[1:]])
        return makeTuple(args)

@evalExpr.register(Indexing)
def foo(x, env) :
    thing = evaluate(x.expr, env)
    return evalIndexing(thing, x.args, env)

@evalExpr.register(Call)
def foo(x, env) :
    thing = evaluate(x.expr, env)
    return evalCall(thing, x.args, env)

@evalExpr.register(FieldRef)
def foo(x, env) :
    thing = evaluate(x.expr, env, toReference)
    ensure(isRecordType(thing.type), "invalid record")
    return recordFieldRef(thing, recordFieldIndex(thing.type, x.name))

@evalExpr.register(TupleRef)
def foo(x, env) :
    thing = evaluate(x.expr, env, toReference)
    ensure(isTupleType(thing.type), "invalid tuple")
    return tupleFieldRef(thing, x.index)

@evalExpr.register(Dereference)
def foo(x, env) :
    thing = evaluate(x.expr, env, toValue)
    ensure(isPointerType(thing.type), "invalid pointer")
    void_ptr = ctypes.cast(thing.buf, ctypes.c_void_p)
    return Reference(thing.type, void_ptr.value)

@evalExpr.register(AddressOf)
def foo(x, env) :
    thing = evaluate(x.expr, env, toLValue)
    ptr = tempValue(pointerType(thing.type))
    void_ptr = ctypes.c_void_p(thing.address)
    ptr.buf = ctypes.cast(void_ptr, ptr.ctypesType)
    return ptr



#
# evalIndexing
#

evalIndexing = multimethod(errorMessage="invalid indexing")

@evalIndexing.register(Record)
def foo(x, args, env) :
    ensureArity(args, len(x.typeVars))
    typeParams = [evaluate(y, env, toTypeOrValueOrCell) for y in args]
    return recordType(x, typeParams)

@evalIndexing.register(primitives.Tuple)
def foo(x, args, env) :
    ensure(len(args) > 1, "tuple types need atleast two member types")
    elementTypes = [evaluate(y, env, toTypeOrCell) for y in args]
    return tupleType(elementTypes)

@evalIndexing.register(primitives.Array)
def foo(x, args, env) :
    ensureArity(args, 2)
    elementType = evaluate(args[0], env, toTypeOrCell)
    sizeValue = evaluate(args[1], env, toValueOrCell)
    return arrayType(elementType, sizeValue)

@evalIndexing.register(primitives.Pointer)
def foo(x, args, env) :
    ensureArity(args, 1)
    targetType = evaluate(args[0], env, toTypeOrCell)
    return pointerType(targetType)



#
# evalCall
#

evalCall = multimethod(errorMessage="invalid call")

@evalCall.register(Record)
def foo(x, args, env) :
    ensureArity(args, len(x.fields))
    recordEnv, cells = bindTypeVars(x.env, x.typeVars)
    fieldTypePatterns = computeFieldTypes(x.fields, recordEnv)
    argRefs = [evaluate(y, env, toReference) for y in args]
    for typePattern, argRef, arg in zip(fieldTypePatterns, argRefs, args) :
        matchPattern(typePattern, argRef.type, arg)
    typeParams = resolveTypeVars(x.typeVars, cells)
    recType = recordType(x, typeParams)
    return makeRecord(recType, argRefs)

@evalCall.register(Procedure)
def foo(x, args, env) :
    argWrappers = [ArgumentWrapper(y, env) for y in args]
    result = matchCodeSignature(x.env, x.code, argWrappers)
    if type(result) is MatchFailure :
        result.signalError(x.code, args)
    assert type(result) is Environment
    procEnv = result
    return evalCodeBody(x.code, procEnv)

@evalCall.register(Overloadable)
def foo(x, args, env) :
    argWrappers = [ArgumentWrapper(y, env) for y in args]
    for y in x.overloads :
        result = matchCodeSignature(y.env, y.code, argWrappers)
        if type(result) is MatchFailure :
            continue
        assert type(result) is Environment
        procEnv = result
        return evalCodeBody(y.code, procEnv)
    error("no matching overload")

@evalCall.register(primitives.default)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.typeSize)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.addressOf)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.pointerDereference)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.pointerOffset)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.pointerSubtract)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.pointerCast)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.pointerCopy)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.pointerEquals)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.pointerLesser)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.allocateMemory)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.freeMemory)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.TupleType)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.tuple)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.tupleFieldCount)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.tupleFieldRef)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.array)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.arrayRef)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.RecordType)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.makeRecord)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.recordFieldCount)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.recordFieldRef)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.boolCopy)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.boolNot)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.charCopy)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.charEquals)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.charLesser)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.intCopy)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.intEquals)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.intLesser)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.intAdd)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.intSubtract)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.intMultiply)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.intDivide)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.intModulus)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.intNegate)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.floatCopy)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.floatEquals)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.floatLesser)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.floatAdd)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.floatSubtract)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.floatMultiply)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.floatDivide)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.floatNegate)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.doubleCopy)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.doubleEquals)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.doubleLesser)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.doubleAdd)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.doubleSubtract)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.doubleMultiply)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.doubleDivide)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.doubleNegate)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.charToInt)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.intToChar)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.floatToInt)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.intToFloat)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.floatToDouble)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.doubleToFloat)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.doubleToInt)
def foo(x, args, env) :
    raise NotImplementedError

@evalCall.register(primitives.intToDouble)
def foo(x, args, env) :
    raise NotImplementedError



#
# matchCodeSignature(env, code, args) -> Environment | MatchFailure
#

class MatchFailure(object) :
    def __init__(self, message, context=None) :
        self.message = message
        self.context = context
    def signalError(self) :
        try :
            if self.context is not None :
                contextPush(self.context)
            error(self.message)
        finally :
            if self.context is not None :
                contextPop()

class ArgumentWrapper(object) :
    def __init__(self, expr, env) :
        self.expr = expr
        self.env = env
        self.result_ = None

    def evaluate(self, converter=None) :
        if self.result_ is None :
            self.result_ = evaluate(self.expr, self.env)
        try :
            contextPush(self.expr)
            if converter is not None :
                return converter(self.result_)
            return self.result_
        finally :
            contextPop()

def matchCodeSignature(codeEnv, code, args) :
    def argMismatch(arg) :
        return MatchFailure("argument mismatch", arg.expr)
    def typeCondFailure(typeCond) :
        return MatchFailure("type condition failed", typeCond)
    if len(args) != len(code.formalArgs) :
        return MatchFailure("incorrect no. of arguments")
    codeEnv2, cells = bindTypeVars(codeEnv, code.typeVars)
    bindings = []
    for arg, formalArg in zip(args, code.formalArgs) :
        if type(formalArg) is ValueArgument :
            if formalArg.byRef :
                argResult = arg.evaluate(toReference)
            else :
                argResult = arg.evaluate(toValue)
            if formalArg.type is not None :
                typePattern = evaluate(formalArg.type, codeEnv2, toTypeOrCell)
                if not unify(typePattern, argResult.type) :
                    return argMismatch(arg)
            bindings.append((formalArg.name, argResult))
        elif type(formalArg) is StaticArgument :
            argResult = arg.evaluate(toTypeOrValue)
            formalType = formalArg.type
            typePattern = evaluate(formalType, codeEnv2, toTypeOrValueOrCell)
            if not unify(typePattern, argResult) :
                return argMismatch(arg)
        else :
            assert False
    typeParams = resolveTypeVars(code.typeVars, cells)
    codeEnv2 = bindVariables(codeEnv, code.typeVars, typeParams)
    for typeCondition in enumerate(code.typeConditions) :
        result = evaluate(typeCondition, codeEnv2, toBool)
        if not result :
            return typeCondFailure(typeCondition)
    for ident, value in bindings :
        addIdent(codeEnv2, ident, value)
    return codeEnv2



#
# evalCodeBody
#

def evalCodeBody(code, env) :
    returnType = None
    if code.returnType is not None :
        returnType = evaluate(code.returnType, env, toType)
    context = CodeContext(code.returnByRef, returnType)
    result = evalStatement(code.block, env, context)
    if result is None :
        if returnType is not None :
            ensure(typeEquals(returnType, voidType),
                   "unexpected void return")
        result = voidValue
    return result



#
# evalStatement : (stmt, env, context) -> Thing | None
#

class CodeContext(object) :
    def __init__(self, returnByRef, returnType) :
        self.returnByRef = returnByRef
        self.returnType = returnType

def evalStatement(stmt, env, context) :
    try :
        contextPush(stmt)
        pushTempValueSet()
        return evalStmt(stmt, env, context)
    finally :
        popTempValueSet()
        contextPop()



#
# evalStmt
#

evalStmt = multimethod(errorMessage="invalid statement")

@evalStmt.register(Block)
def foo(x, env, context) :
    localValues = []
    try :
        for statement in x.statements :
            if type(statement) is LocalBinding :
                lb = statement
                right = evaluate(lb.expr, env, toReference)
                if lb.type is not None :
                    declaredType = evaluate(lb.type, env, toType)
                    matchType(declaredType, right.type, lb.expr)
                value = Value(right.type)
                valueCopy(value, right)
                localValues.append(value)
                addIdent(lb.name, value)
            else :
                result = evalStatement(statement, env, context)
                if result is not None :
                    return result
    finally :
        for v in reversed(localValues) :
            valueDestroy(v)

@evalStmt.register(Assignment)
def foo(x, env, context) :
    left = evaluate(x.left, env, toLValue)
    right = evaluate(x.right, env, toReference)
    valueAssign(left, right)

@evalStmt.register(Return)
def foo(x, env, context) :
    if context.returnByRef :
        result = evaluate(x.expr, env, toLValue)
        resultType = result.type
    elif x.expr is None :
        result = voidValue
        resultType = voidType
    else :
        result = evaluate(x.expr, env, toTypeOrValue)
        resultType = result.type if isType(result) else None
    if context.returnType is not None :
        matchType(resultType, context.returnType, x.expr)
    return result

@evalStmt.register(IfStatement)
def foo(x, env, context) :
    cond = evaluate(x.condition, env, toBool)
    if cond :
        return evalStatement(x.thenPart, env, context)
    elif x.elsePart is not None :
        return evalStatement(x.elsePart, env, context)

@evalStmt.register(ExprStatement)
def foo(x, env, context) :
    evaluate(x.expr, env)



#
# remove temp name used for multimethod instances
#

del foo
