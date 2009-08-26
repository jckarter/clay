import ctypes
from clay.xprint import *
from clay.error import *
from clay.multimethod import *
from clay.machineops import *
from clay.ast import *
from clay.types import *



#
# utility
#

def ensureArity(args, n) :
    ensure(len(args) == n, "incorrect no. arguments")



#
# Value, Reference, Cell
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

class Cell(object) :
    def __init__(self, param=None) :
        self.param = param

def isValue(x) : return type(x) is Value
def isReference(x) : return type(x) is Reference
def isCell(x) : return type(x) is Cell



#
# VoidValue, voidValue
#

class VoidValue(object) :
    pass

voidValue = VoidValue()



#
# toInt
#

@toInt.register(Value)
def foo(x) :
    ensure(equals(x.type, intType), "not int type")
    return x.buf.value

@toInt.register(Reference)
def foo(x) :
    return toInt(toValue(x))



#
# toBool
#

@toBool.register(Value)
def foo(x) :
    ensure(equals(x.type, boolType), "not bool type")
    return x.buf.value

@toBool.register(Reference)
def foo(x) :
    return toBool(toValue(x))



#
# toValue
#

toValue = multimethod(errorMessage="invalid value")

@toValue.register(bool)
def foo(x) :
    v = tempValue(boolType)
    v.buf.value = x
    return v

@toValue.register(int)
def foo(x) :
    v = tempValue(intType)
    v.buf.value = x
    return v

@toValue.register(Reference)
def foo(x) :
    v = tempValue(x.type)
    valueCopy(v, x)
    return v

@toValue.register(Value)
def foo(x) :
    return x



#
# toLValue
#

toLValue = multimethod(errorMessage="invalid l-value")

@toLValue.register(Reference)
def foo(x) :
    return x



#
# toReference
#

toReference = multimethod(errorMessage="invalid reference")

@toReference.register(Value)
def foo(x) :
    return Reference(x.type, ctypes.addressof(x.buf))

@toReference.register(Reference)
def foo(x) :
    return x



#
# toTypeOrValue
#

toTypeOrValue = multimethod(errorMessage="invalid type param")

@toTypeOrValue.register(Value)
def foo(x) :
    return x

@toTypeOrValue.register(Reference)
def foo(x) :
    return toValue(x)

@toTypeOrValue.register(Type)
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

@equals.register(Value, Value)
def foo(a, b) :
    return typeAndValueEquals(a, b)

@equals.register(Value, Reference)
def foo(a, b) :
    return typeAndValueEquals(a, b)

@equals.register(Reference, Value)
def foo(a, b) :
    return typeAndValueEquals(a, b)

@equals.register(Reference, Reference)
def foo(a, b) :
    return typeAndValueEquals(a, b)

def typeAndValueEquals(a, b) :
    return equals(a.type, b.type) and valueEquals(a, b)



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
    if isSimpleType(a.type) :
        return
    callBuiltin("init", [a])

def simpleValueCopy(dest, src) :
    destRef = toReference(dest)
    destPtr = ctypes.c_void_p(destRef.address)
    srcRef = toReference(src)
    srcPtr = ctypes.c_void_p(srcRef.address)
    size = typeSize(dest.type)
    ctypes.memmove(destPtr, srcPtr, size)

def valueCopy(dest, src) :
    assert equals(dest.type, src.type)
    if isSimpleType(dest.type) :
        simpleValueCopy(dest, src)
        return
    callBuiltin("copy", [dest, src])

def valueAssign(dest, src) :
    assert equals(dest.type, src.type)
    if isSimpleType(dest.type) :
        simpleValueCopy(dest, src)
        return
    callBuiltin("assign", [dest, src])

def valueDestroy(a) :
    if isSimpleType(a.type) :
        return
    callBuiltin("destroy", [a])

def valueEquals(a, b) :
    assert equals(a.type, b.type)
    # TODO: add bypass for simple types
    return callBuiltin("equals", [a, b], toBool)

def valueHash(a) :
    return callBuiltin("hash", [a], toInt)



#
# value printer
#

def xconvertBuiltin(r) :
    return toValue(r).buf.value

def xconvertBool(r) :
    return XSymbol("true" if toValue(r).buf.value else "false")

def xconvertPointer(r) :
    return XObject("Ptr", r.type.params[0])

def xconvertTuple(r) :
    elements = [tupleFieldRef(r, i) for i in range(len(r.type.params))]
    return tuple(elements)

def xconvertArray(r) :
    arraySize = toInt(r.type.params[1])
    return [arrayRef(r, i) for i in range(arraySize)]

def xconvertRecord(r) :
    fieldCount = len(r.type.tag.fields)
    fields = [recordFieldRef(r, i) for i in range(fieldCount)]
    return XObject(r.type.tag.name.s, *fields)

def xconvertValue(x) :
    return xconvertReference(toReference(x))

def xconvertReference(x) :
    converter = _xconverters.get(x.type.tag)
    if converter is not None :
        return converter(x)
    if isRecordType(x.type) :
        return xconvertRecord(x)
    return x

xregister(Value, xconvertValue)
xregister(Reference, xconvertReference)
xregister(Cell, lambda x : XObject("Cell", x.param))
xregister(VoidValue, lambda x : XSymbol("void"))

_xconverters = {}

_xconverters[boolTypeTag] = xconvertBool
_xconverters[intTypeTag] = xconvertBuiltin
_xconverters[floatTypeTag] = xconvertBuiltin
_xconverters[doubleTypeTag] = xconvertBuiltin
_xconverters[charTypeTag] = xconvertBuiltin
_xconverters[pointerTypeTag] = xconvertPointer
_xconverters[tupleTypeTag] = xconvertTuple
_xconverters[arrayTypeTag] = xconvertArray



#
# unification
#

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
        ensure(type(x) is Overloadable, "invalid overloadable")
    entry = lookupIdent(env, item.name, verifyOverloadable)
    entry.overloads.insert(0, item)



#
# temp values
#

class TempValueSet(object) :
    def __init__(self) :
        self.values = []

    def tempValue(self, type_) :
        v = Value(type_)
        self.values.append(v)
        return v

    def removeTemp(self, v) :
        for i, x in enumerate(self.values) :
            if v is x :
                self.values.pop(i)
                return
        error("temp not found")

    def addTemp(self, v) :
        self.values.append(v)

_tempValueSets = [TempValueSet()]

def pushTempValueSet() :
    _tempValueSets.append(TempValueSet())

def popTempValueSet() :
    tempValueSet = _tempValueSets.pop()
    for temp in reversed(tempValueSet.values) :
        valueDestroy(temp)

def topTempValueSet() :
    return _tempValueSets[-1]

def tempValue(type_) :
    return topTempValueSet().tempValue(type_)



#
# bindings
#

def bindVariables(parentEnv, variables, objects) :
    env = Environment(parentEnv)
    for variable, object_ in zip(variables, objects) :
        addIdent(env, variable, object_)
    return env

def bindTypeVars(parentEnv, typeVars) :
    cells = [Cell() for _ in typeVars]
    env = bindVariables(parentEnv, typeVars, cells)
    return env, cells

def matchPattern(pattern, typeParam, context=None) :
    try :
        if context is not None :
            contextPush(context)
        if not unify(pattern, typeParam) :
            error("type mismatch")
    finally :
        if context is not None :
            contextPop()

def matchType(typeA, typeB, context) :
    try :
        contextPush(context)
        if not equals(typeA, typeB) :
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
# type checking converters
#

def toValueOfType(pattern) :
    def f(x) :
        v = toValue(x)
        matchPattern(pattern, v.type)
        return v
    return f

def toReferenceOfType(pattern) :
    def f(x) :
        r = toReference(x)
        matchPattern(pattern, r.type)
        return r
    return f

def toReferenceWithTypeTag(tag) :
    def f(x) :
        r = toReference(x)
        ensure(r.type.tag is tag, "type mismatch")
        return r
    return f

def toTypeWithTag(tag) :
    def f(x) :
        t = toType(x)
        ensure(t.tag is tag, "type mismatch")
        return t
    return f

def toRecordReference(x) :
    r = toReference(x)
    ensure(isRecordType(r.type), "record type expected")
    return r

def toRecordType(t) :
    ensure(isType(t) and isRecordType(t), "record type expected")
    return t



#
# arrays, tuples, records
#

def arrayRef(a, i) :
    assert isReference(a)
    cell, sizeCell = Cell(), Cell()
    matchPattern(arrayType(cell, sizeCell), a.type)
    ensure((0 <= i < toInt(sizeCell.param)), "array index out of range")
    elementType = cell.param
    return Reference(elementType, a.address + i*typeSize(elementType))

def makeArray(n, defaultElement) :
    assert isReference(defaultElement)
    value = tempValue(arrayType(defaultElement.type, toValue(n)))
    valueRef = toReference(value)
    for i in range(n) :
        elementRef = arrayRef(valueRef, i)
        valueCopy(elementRef, defaultElement)
    return value

def tupleFieldRef(a, i) :
    assert isReference(a) and isTupleType(a.type)
    ensure((0 <= i < len(a.type.params)), "tuple index out of range")
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
    nFields = len(a.type.tag.fields)
    ensure((0 <= i < nFields), "record field index out of range")
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

def evaluate(expr, env, converter=None) :
    try :
        contextPush(expr)
        result = evaluate2(expr, env)
        if converter is not None :
            result = converter(result)
        return result
    finally :
        contextPop()



#
# evaluate2
#

evaluate2 = multimethod(errorMessage="invalid expression")

@evaluate2.register(BoolLiteral)
def foo(x, env) :
    return toValue(x.value)

@evaluate2.register(IntLiteral)
def foo(x, env) :
    return toValue(x.value)

@evaluate2.register(CharLiteral)
def foo(x, env) :
    v = tempValue(charType)
    v.buf.value = x.value
    return v

@evaluate2.register(NameRef)
def foo(x, env) :
    return lookupIdent(env, x.name)

@evaluate2.register(Tuple)
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

@evaluate2.register(Indexing)
def foo(x, env) :
    thing = evaluate(x.expr, env)
    return evaluateIndexing(thing, x.args, env)

@evaluate2.register(Call)
def foo(x, env) :
    thing = evaluate(x.expr, env)
    return evaluateCall(thing, x.args, env)

@evaluate2.register(FieldRef)
def foo(x, env) :
    thing = evaluate(x.expr, env, toRecordReference)
    return recordFieldRef(thing, recordFieldIndex(thing.type, x.name))

@evaluate2.register(TupleRef)
def foo(x, env) :
    thing = evaluate(x.expr, env, toReferenceWithTypeTag(tupleTypeTag))
    return tupleFieldRef(thing, x.index)

@evaluate2.register(Dereference)
def foo(x, env) :
    return evaluateCall(primitives.pointerDereference, [x.expr], env)

@evaluate2.register(AddressOf)
def foo(x, env) :
    return evaluateCall(primitives.addressOf, [x.expr], env)



#
# evaluateIndexing
#

evaluateIndexing = multimethod(errorMessage="invalid indexing")

@evaluateIndexing.register(Record)
def foo(x, args, env) :
    ensureArity(args, len(x.typeVars))
    typeParams = [evaluate(y, env, toTypeOrValueOrCell) for y in args]
    return recordType(x, typeParams)

@evaluateIndexing.register(primitives.Tuple)
def foo(x, args, env) :
    ensure(len(args) > 1, "tuple types need atleast two member types")
    elementTypes = [evaluate(y, env, toTypeOrCell) for y in args]
    return tupleType(elementTypes)

@evaluateIndexing.register(primitives.Array)
def foo(x, args, env) :
    ensureArity(args, 2)
    elementType = evaluate(args[0], env, toTypeOrCell)
    sizeValue = evaluate(args[1], env, toValueOrCell)
    return arrayType(elementType, sizeValue)

@evaluateIndexing.register(primitives.Pointer)
def foo(x, args, env) :
    ensureArity(args, 1)
    targetType = evaluate(args[0], env, toTypeOrCell)
    return pointerType(targetType)



#
# evaluateCall
#

evaluateCall = multimethod(errorMessage="invalid call")

@evaluateCall.register(Record)
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

@evaluateCall.register(Procedure)
def foo(x, args, env) :
    argWrappers = [ArgumentWrapper(y, env) for y in args]
    result = matchCodeSignature(x.env, x.code, argWrappers)
    if type(result) is MatchFailure :
        result.signalError(x.code, args)
    assert type(result) is Environment
    procEnv = result
    return evalCodeBody(x.code, procEnv)

@evaluateCall.register(Overloadable)
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



#
# evaluate primitives
#

@evaluateCall.register(primitives.default)
def foo(x, args, env) :
    ensureArity(args, 1)
    t = evaluate(args[0], env, toType)
    v = tempValue(t)
    valueInit(v)
    return v

@evaluateCall.register(primitives.typeSize)
def foo(x, args, env) :
    ensureArity(args, 1)
    t = evaluate(args[0], env, toType)
    return toValue(typeSize(t))



#
# machine op utils
#

def callMachineOp(f, args) :
    def toPtr(x) :
        if type(x) is Value :
            return ctypes.pointer(x.buf)
        elif type(x) is Reference :
            return ctypes.c_void_p(x.address)
        else :
            assert False
    ptrs = map(toPtr, args)
    return f(*ptrs)

def simpleMop(mop, args, env, argTypes, returnType) :
    ensureArity(args, len(argTypes))
    mopInput = []
    for arg, argType in zip(args, argTypes) :
        ref = evaluate(arg, env, toReferenceOfType(argType))
        mopInput.append(ref)
    if returnType is not None :
        result = tempValue(returnType)
        mopInput.append(result)
    else :
        result = voidValue
    callMachineOp(mop, mopInput)
    return result



#
# evaluate pointer primitives
#

def pointerToAddress(ptr) :
    void_ptr = ctypes.cast(ptr.buf, ctypes.c_void_p)
    return void_ptr.value

def addressToPointer(addr, ptrType) :
    void_ptr = ctypes.c_void_p(addr)
    ptr = tempValue(ptrType)
    ptr.buf = ctypes.cast(void_ptr, ptr.ctypesType)
    return ptr

@evaluateCall.register(primitives.addressOf)
def foo(x, args, env) :
    ensureArity(args, 1)
    valueRef = evaluate(args[0], env, toLValue)
    return addressToPointer(valueRef.address, pointerType(valueRef.type))

@evaluateCall.register(primitives.pointerDereference)
def foo(x, args, env) :
    ensureArity(args, 1)
    cell = Cell()
    ptr = evaluate(args[0], env, toValueOfType(pointerType(cell)))
    return Reference(cell.param, pointerToAddress(ptr))

@evaluateCall.register(primitives.pointerOffset)
def foo(x, args, env) :
    ensureArity(args, 2)
    cell = Cell()
    ptr = evaluate(args[0], env, toValueOfType(pointerType(cell)))
    offset = evaluate(args[0], env, toValueOfType(intType))
    result = tempValue(ptr.type)
    callMachineOp(mop_pointerOffset, [ptr, offset, result])
    return result

@evaluateCall.register(primitives.pointerSubtract)
def foo(x, args, env) :
    ensureArity(args, 2)
    cell = Cell()
    ptr1 = evaluate(args[0], env, toValueOfType(pointerType(cell)))
    ptr2 = evaluate(args[0], env, toValueOfType(pointerType(cell)))
    result = tempValue(intType)
    callMachineOp(mop_pointerSubtract, [ptr1, ptr2, result])
    return result

@evaluateCall.register(primitives.pointerCast)
def foo(x, args, env) :
    ensureArity(args, 2)
    cell = Cell()
    targetType = evaluate(args[0], env, toType)
    ptr = evaluate(args[1], env, toValueOfType(pointerType(cell)))
    return addressToPointer(pointerToAddress(ptr), pointerType(targetType))

@evaluateCall.register(primitives.pointerCopy)
def foo(x, args, env) :
    ensureArity(args, 2)
    cell = Cell()
    destRef = evaluate(args[0], env, toReferenceOfType(pointerType(cell)))
    srcRef = evaluate(args[1], env, toReferenceOfType(pointerType(cell)))
    callMachineOp(mop_pointerCopy, [destRef, srcRef])
    return voidValue

@evaluateCall.register(primitives.pointerEquals)
def foo(x, args, env) :
    ensureArity(args, 2)
    cell = Cell()
    ptr1 = evaluate(args[0], env, toValueOfType(pointerType(cell)))
    ptr2 = evaluate(args[1], env, toValueOfType(pointerType(cell)))
    result = tempValue(boolType)
    callMachineOp(mop_pointerEquals, [ptr1, ptr2, result])
    return result

@evaluateCall.register(primitives.pointerLesser)
def foo(x, args, env) :
    ensureArity(args, 2)
    cell = Cell()
    ptr1 = evaluate(args[0], env, toValueOfType(pointerType(cell)))
    ptr2 = evaluate(args[1], env, toValueOfType(pointerType(cell)))
    result = tempValue(boolType)
    callMachineOp(mop_pointerLesser, [ptr1, ptr2, result])
    return result

@evaluateCall.register(primitives.allocateMemory)
def foo(x, args, env) :
    ensureArity(args, 1)
    size = evaluate(args[0], env, toValueOfType(intType))
    result = tempValue(pointerType(intType))
    callMachineOp(mop_allocateMemory, [size, result])
    return result

@evaluateCall.register(primitives.freeMemory)
def foo(x, args, env) :
    ensureArity(args, 1)
    ptr = evaluate(args[0], env, toValueOfType(pointerType(intType)))
    callMachineOp(mop_freeMemory, [ptr])
    return voidValue



#
# evaluate tuple primitives
#

@evaluateCall.register(primitives.TupleType)
def foo(x, args, env) :
    ensureArity(args, 1)
    t = evaluate(args[0], env, toType)
    return toValue(isTupleType(t))

@evaluateCall.register(primitives.tuple)
def foo(x, args, env) :
    ensure(len(args) > 1, "tuples need atleast two members")
    valueRefs = [evaluate(y, env, toReference) for y in args]
    return makeTuple(valueRefs)

@evaluateCall.register(primitives.tupleFieldCount)
def foo(x, args, env) :
    ensureArity(args, 1)
    t = evaluate(args[0], env, toTypeWithTag(tupleTypeTag))
    return toValue(len(t.params))

@evaluateCall.register(primitives.tupleFieldRef)
def foo(x, args, env) :
    ensureArity(args, 2)
    tupleRef = evaluate(args[0], env, toReferenceWithTypeTag(tupleTypeTag))
    i = evaluate(args[1], env, toInt)
    return tupleFieldRef(tupleRef, i)



#
# evaluate array primitives
#

@evaluateCall.register(primitives.array)
def foo(x, args, env) :
    ensureArity(args, 2)
    n = evaluate(args[0], env, toInt)
    v = evaluate(args[1], env, toReference)
    return makeArray(n, v)

@evaluateCall.register(primitives.arrayRef)
def foo(x, args, env) :
    ensureArity(args, 2)
    a = evaluate(args[0], env, toReference)
    i = evaluate(args[1], env, toInt)
    return arrayRef(a, i)



#
# evaluate record primitives
#

@evaluateCall.register(primitives.RecordType)
def foo(x, args, env) :
    ensureArity(args, 1)
    t = evaluate(args[0], env, toType)
    return toValue(isRecordType(t))

@evaluateCall.register(primitives.recordFieldCount)
def foo(x, args, env) :
    ensureArity(args, 1)
    t = evaluate(args[0], env, toRecordType)
    return len(t.tag.fields)

@evaluateCall.register(primitives.recordFieldRef)
def foo(x, args, env) :
    ensureArity(args, 2)
    recRef = evaluate(args[0], env, toRecordReference)
    i = evaluate(args[1], env, toInt)
    return recordFieldRef(recRef, i)



#
# evaluate bool primitives
#

@evaluateCall.register(primitives.boolCopy)
def foo(x, args, env) :
    argTypes = [boolType, boolType]
    return simpleMop(mop_boolCopy, args, env, argTypes, None)

@evaluateCall.register(primitives.boolNot)
def foo(x, args, env) :
    argTypes = [boolType]
    return simpleMop(mop_boolNot, args, env, argTypes, boolType)



#
# evaluate char primitives
#

@evaluateCall.register(primitives.charCopy)
def foo(x, args, env) :
    argTypes = [charType, charType]
    return simpleMop(mop_charCopy, args, env, argTypes, None)

@evaluateCall.register(primitives.charEquals)
def foo(x, args, env) :
    argTypes = [charType, charType]
    return simpleMop(mop_charEquals, args, env, argTypes, boolType)

@evaluateCall.register(primitives.charLesser)
def foo(x, args, env) :
    argTypes = [charType, charType]
    return simpleMop(mop_charLesser, args, env, argTypes, boolType)



#
# evaluate int primitives
#

@evaluateCall.register(primitives.intCopy)
def foo(x, args, env) :
    argTypes = [intType, intType]
    return simpleMop(mop_intCopy, args, env, argTypes, None)

@evaluateCall.register(primitives.intEquals)
def foo(x, args, env) :
    argTypes = [intType, intType]
    return simpleMop(mop_intEquals, args, env, argTypes, boolType)

@evaluateCall.register(primitives.intLesser)
def foo(x, args, env) :
    argTypes = [intType, intType]
    return simpleMop(mop_intLesser, args, env, argTypes, boolType)

@evaluateCall.register(primitives.intAdd)
def foo(x, args, env) :
    argTypes = [intType, intType]
    return simpleMop(mop_intAdd, args, env, argTypes, intType)

@evaluateCall.register(primitives.intSubtract)
def foo(x, args, env) :
    argTypes = [intType, intType]
    return simpleMop(mop_intSubtract, args, env, argTypes, intType)

@evaluateCall.register(primitives.intMultiply)
def foo(x, args, env) :
    argTypes = [intType, intType]
    return simpleMop(mop_intMultiply, args, env, argTypes, intType)

@evaluateCall.register(primitives.intDivide)
def foo(x, args, env) :
    argTypes = [intType, intType]
    return simpleMop(mop_intDivide, args, env, argTypes, intType)

@evaluateCall.register(primitives.intModulus)
def foo(x, args, env) :
    argTypes = [intType, intType]
    return simpleMop(mop_intModulus, args, env, argTypes, intType)

@evaluateCall.register(primitives.intNegate)
def foo(x, args, env) :
    argTypes = [intType]
    return simpleMop(mop_intNegate, args, env, argTypes, intType)



#
# evaluate float primitives
#

@evaluateCall.register(primitives.floatCopy)
def foo(x, args, env) :
    argTypes = [floatType, floatType]
    return simpleMop(mop_floatCopy, args, env, argTypes, None)

@evaluateCall.register(primitives.floatEquals)
def foo(x, args, env) :
    argTypes = [floatType, floatType]
    return simpleMop(mop_floatEquals, args, env, argTypes, boolType)

@evaluateCall.register(primitives.floatLesser)
def foo(x, args, env) :
    argTypes = [floatType, floatType]
    return simpleMop(mop_floatLesser, args, env, argTypes, boolType)

@evaluateCall.register(primitives.floatAdd)
def foo(x, args, env) :
    argTypes = [floatType, floatType]
    return simpleMop(mop_floatAdd, args, env, argTypes, floatType)

@evaluateCall.register(primitives.floatSubtract)
def foo(x, args, env) :
    argTypes = [floatType, floatType]
    return simpleMop(mop_floatSubtract, args, env, argTypes, floatType)

@evaluateCall.register(primitives.floatMultiply)
def foo(x, args, env) :
    argTypes = [floatType, floatType]
    return simpleMop(mop_floatMultiply, args, env, argTypes, floatType)

@evaluateCall.register(primitives.floatDivide)
def foo(x, args, env) :
    argTypes = [floatType, floatType]
    return simpleMop(mop_floatDivide, args, env, argTypes, floatType)

@evaluateCall.register(primitives.floatNegate)
def foo(x, args, env) :
    argTypes = [floatType]
    return simpleMop(mop_floatNegate, args, env, argTypes, floatType)



#
# evaluate double primitives
#

@evaluateCall.register(primitives.doubleCopy)
def foo(x, args, env) :
    argTypes = [doubleType, doubleType]
    return simpleMop(mop_doubleCopy, args, env, argTypes, None)

@evaluateCall.register(primitives.doubleEquals)
def foo(x, args, env) :
    argTypes = [doubleType, doubleType]
    return simpleMop(mop_doubleEquals, args, env, argTypes, boolType)

@evaluateCall.register(primitives.doubleLesser)
def foo(x, args, env) :
    argTypes = [doubleType, doubleType]
    return simpleMop(mop_doubleLesser, args, env, argTypes, boolType)

@evaluateCall.register(primitives.doubleAdd)
def foo(x, args, env) :
    argTypes = [doubleType, doubleType]
    return simpleMop(mop_doubleAdd, args, env, argTypes, doubleType)

@evaluateCall.register(primitives.doubleSubtract)
def foo(x, args, env) :
    argTypes = [doubleType, doubleType]
    return simpleMop(mop_doubleSubtract, args, env, argTypes, doubleType)

@evaluateCall.register(primitives.doubleMultiply)
def foo(x, args, env) :
    argTypes = [doubleType, doubleType]
    return simpleMop(mop_doubleMultiply, args, env, argTypes, doubleType)

@evaluateCall.register(primitives.doubleDivide)
def foo(x, args, env) :
    argTypes = [doubleType, doubleType]
    return simpleMop(mop_doubleDivide, args, env, argTypes, doubleType)

@evaluateCall.register(primitives.doubleNegate)
def foo(x, args, env) :
    argTypes = [doubleType]
    return simpleMop(mop_doubleNegate, args, env, argTypes, doubleType)



#
# evaluate conversion primitives
#

@evaluateCall.register(primitives.charToInt)
def foo(x, args, env) :
    return simpleMop(mop_charToInt, args, env, [charType], intType)

@evaluateCall.register(primitives.intToChar)
def foo(x, args, env) :
    return simpleMop(mop_intToChar, args, env, [intType], charType)

@evaluateCall.register(primitives.floatToInt)
def foo(x, args, env) :
    return simpleMop(mop_floatToInt, args, env, [floatType], intType)

@evaluateCall.register(primitives.intToFloat)
def foo(x, args, env) :
    return simpleMop(mop_intToFloat, args, env, [intType], floatType)

@evaluateCall.register(primitives.floatToDouble)
def foo(x, args, env) :
    return simpleMop(mop_floatToDouble, args, env, [floatType], doubleType)

@evaluateCall.register(primitives.doubleToFloat)
def foo(x, args, env) :
    return simpleMop(mop_doubleToFloat, args, env, [doubleType], floatType)

@evaluateCall.register(primitives.doubleToInt)
def foo(x, args, env) :
    return simpleMop(mop_doubleToInt, args, env, [doubleType], intType)

@evaluateCall.register(primitives.intToDouble)
def foo(x, args, env) :
    return simpleMop(mop_intToDouble, args, env, [intType], doubleType)



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
    for typeCondition in code.typeConditions :
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
    context = CodeContext(code.returnByRef, returnType, topTempValueSet())
    result = evalStatement(code.body, env, context)
    if result is None :
        if returnType is not None :
            ensure(equals(returnType, voidType),
                   "unexpected void return")
        result = voidValue
    return result



#
# evalStatement : (stmt, env, context) -> Thing | None
#

class CodeContext(object) :
    def __init__(self, returnByRef, returnType, callerTemps) :
        self.returnByRef = returnByRef
        self.returnType = returnType
        self.callerTemps = callerTemps

def evalStatement(stmt, env, context) :
    try :
        contextPush(stmt)
        pushTempValueSet()
        return evalStatement2(stmt, env, context)
    finally :
        popTempValueSet()
        contextPop()



#
# evalStatement2
#

evalStatement2 = multimethod(errorMessage="invalid statement")

@evalStatement2.register(Block)
def foo(x, env, context) :
    for statement in x.statements :
        if type(statement) is LocalBinding :
            right = evaluate(statement.expr, env, toValue)
            if statement.type is not None :
                declaredType = evaluate(statement.type, env, toType)
                matchType(declaredType, right.type, statement.expr)
            addIdent(env, statement.name, right)
        else :
            result = evalStatement(statement, env, context)
            if result is not None :
                return result

@evalStatement2.register(Assignment)
def foo(x, env, context) :
    left = evaluate(x.left, env, toLValue)
    right = evaluate(x.right, env, toReference)
    valueAssign(left, right)

@evalStatement2.register(Return)
def foo(x, env, context) :
    if context.returnByRef :
        result = evaluate(x.expr, env, toLValue)
        resultType = result.type
    elif x.expr is None :
        result = voidValue
        resultType = voidType
    else :
        result = evaluate(x.expr, env, toTypeOrValue)
        resultType = result.type if isValue(result) else None
        if isValue(result) :
            topTempValueSet().removeTemp(result)
            context.callerTemps.addTemp(result)
    if context.returnType is not None :
        matchType(resultType, context.returnType, x.expr)
    return result

@evalStatement2.register(IfStatement)
def foo(x, env, context) :
    cond = evaluate(x.condition, env, toBool)
    if cond :
        return evalStatement(x.thenPart, env, context)
    elif x.elsePart is not None :
        return evalStatement(x.elsePart, env, context)

@evalStatement2.register(ExprStatement)
def foo(x, env, context) :
    evaluate(x.expr, env)



#
# remove temp name used for multimethod instances
#

del foo
