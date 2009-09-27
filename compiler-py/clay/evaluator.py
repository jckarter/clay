import ctypes
from clay.xprint import *
from clay.cleanup import *
from clay.error import *
from clay.multimethod import *
from clay.machineops import *
from clay.ast import *
from clay.env import *
from clay.coreops import *
from clay.types import *
from clay.unify import *



#
# convert to ctypes
#

_ctypesTable = None

def _initCTypesTable() :
    global _ctypesTable
    _ctypesTable = {}
    _ctypesTable[boolType] = ctypes.c_bool
    _ctypesTable[intType] = ctypes.c_int
    _ctypesTable[floatType] = ctypes.c_float
    _ctypesTable[doubleType] = ctypes.c_double

def _cleanupCTypesTable() :
    global _ctypesTable
    _ctypesTable = None

installGlobalsCleanupHook(_cleanupCTypesTable)

ctypesType = multimethod(errorMessage="invalid type")

@ctypesType.register(Type)
def foo(t) :
    if _ctypesTable is None :
        _initCTypesTable()
    ct = _ctypesTable.get(t)
    if ct is None :
        ct = makeCTypesType(t.tag, t)
    return ct

makeCTypesType = multimethod(errorMessage="invalid type tag")

@makeCTypesType.register(TupleTag)
def foo(tag, t) :
    fieldCTypes = [ctypesType(x) for x in t.params]
    fieldCNames = ["f%d" % x for x in range(len(t.params))]
    ct = type("Tuple", (ctypes.Structure,), {})
    ct._fields_ = zip(fieldCNames, fieldCTypes)
    _ctypesTable[t] = ct
    return ct

@makeCTypesType.register(ArrayTag)
def foo(tag, t) :
    ensure(len(t.params) == 2, "invalid array type")
    ct = ctypesType(t.params[0]) * toInt(t.params[1])
    _ctypesTable[t] = ct
    return ct

@makeCTypesType.register(PointerTag)
def foo(tag, t) :
    ensure(len(t.params) == 1, "invalid pointer type")
    ct = ctypes.POINTER(ctypesType(t.params[0]))
    _ctypesTable[t] = ct
    return ct

@makeCTypesType.register(Record)
def foo(tag, t) :
    ct = type("Record", (ctypes.Structure,), {})
    _ctypesTable[t] = ct
    fieldCTypes = [ctypesType(x) for x in recordFieldTypes(t)]
    fieldCNames = [f.name.s for f in recordValueArgs(t.tag)]
    ct._fields_ = zip(fieldCNames, fieldCTypes)
    return ct



#
# typeSize
#

def typeSize(t) :
    return ctypes.sizeof(ctypesType(t))



#
# Value, Reference
#

class Value(object) :
    def __init__(self, type_) :
        self.type = type_
        self.ctypesType = ctypesType(type_)
        self.buf = self.ctypesType()
    def __del__(self) :
        valueDestroy(self)
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
# temp values
#

_tempsBlocks = [[]]

def _cleanupTempsBlocks() :
    global _tempsBlocks
    _tempsBlocks = [[]]

installGlobalsCleanupHook(_cleanupTempsBlocks)

def pushTempsBlock() :
    block = []
    _tempsBlocks.append(block)

def popTempsBlock() :
    block = _tempsBlocks.pop()
    while block :
        block.pop()

def installTemp(value) :
    _tempsBlocks[-1].append(value)

def tempValue(type_) :
    v = Value(type_)
    _tempsBlocks[-1].append(v)
    return v



#
# toInt, toBool
#

@toInt.register(Value)
def foo(x) :
    ensure(equals(x.type, intType), "not int type")
    return x.buf.value

@toBool.register(Value)
def foo(x) :
    ensure(equals(x.type, boolType), "not bool type")
    return x.buf.value

toInt.register(Reference)(lambda x : toInt(toValue(x)))
toBool.register(Reference)(lambda x : toBool(toValue(x)))



#
# toValue, toLValue, toReference, toValueOrReference, toStatic
#

toValue.register(Value)(lambda x : x)

@toValue.register(Reference)
def foo(x) :
    v = tempValue(x.type)
    valueCopy(v, x)
    return v

toLValue.register(Reference)(lambda x : x)

toReference.register(Reference)(lambda x : x)

@toReference.register(Value)
def foo(x) :
    return Reference(x.type, ctypes.addressof(x.buf))

toValueOrReference.register(Value)(lambda x : x)
toValueOrReference.register(Reference)(lambda x : x)

toStatic.register(Reference)(lambda x : toValue(x))



#
# boolToValue, intToValue
#

def boolToValue(x) :
    v = tempValue(boolType)
    v.buf.value = x
    return v

def intToValue(x) :
    v = tempValue(intType)
    v.buf.value = x
    return v



#
# core value operations
#

def _callBuiltin(builtinName, args, converter=None) :
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
    _callBuiltin("init", [a])

def valueCopy(dest, src) :
    assert equals(dest.type, src.type)
    if isSimpleType(dest.type) :
        _simpleValueCopy(dest, src)
        return
    _callBuiltin("copy", [dest, src])

def valueAssign(dest, src) :
    assert equals(dest.type, src.type)
    if isSimpleType(dest.type) :
        _simpleValueCopy(dest, src)
        return
    _callBuiltin("assign", [dest, src])

def _simpleValueCopy(dest, src) :
    destRef = toReference(dest)
    destPtr = ctypes.c_void_p(destRef.address)
    srcRef = toReference(src)
    srcPtr = ctypes.c_void_p(srcRef.address)
    size = typeSize(dest.type)
    ctypes.memmove(destPtr, srcPtr, size)

def valueDestroy(a) :
    if isSimpleType(a.type) :
        ctypes.memset(ctypes.pointer(a.buf), 0, typeSize(a.type))
        return
    _callBuiltin("destroy", [a])

def valueEquals(a, b) :
    assert equals(a.type, b.type)
    # TODO: add bypass for simple types
    return _callBuiltin("equals", [a, b], toBool)

def valueHash(a) :
    return _callBuiltin("hash", [a], toInt)



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
# arrays, tuples, records
#

def arrayRef(a, i) :
    assert isReference(a)
    cell, sizeCell = Cell(), Cell()
    matchType(arrayType(cell, sizeCell), a.type)
    elementType = cell.param
    return Reference(elementType, a.address + i*typeSize(elementType))

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

def recordValueArgs(r) :
    if r.valueArgs_ is None :
        r.valueArgs_ = [a for a in r.args if type(a) is ValueRecordArg]
    return r.valueArgs_

def recordValueNames(r) :
    if r.valueNames_ is None :
        names = {}
        for i, valueArg in enumerate(recordValueArgs(r)) :
            if valueArg.name.s in names :
                withContext(valueArg, lambda : error("duplicate field: %s" %
                                                     valueArg.name.s))
            names[valueArg.name.s] = i
        r.valueNames_ = names
    return r.valueNames_

def recordFieldRef(a, i) :
    assert isReference(a) and isRecordType(a.type)
    valueArgs = recordValueArgs(a.type.tag)
    nFields = len(valueArgs)
    ensure((0 <= i < nFields), "record field index out of range")
    fieldName = valueArgs[i].name.s
    fieldType = recordFieldTypes(a.type)[i]
    ctypesField = getattr(ctypesType(a.type), fieldName)
    return Reference(fieldType, a.address + ctypesField.offset)

def recordFieldIndex(recType, ident) :
    valueNames = recordValueNames(recType.tag)
    i = valueNames.get(ident.s)
    if i is not None :
        return i
    error("record field not found: %s" % ident.s)

def makeRecord(recType, argRefs) :
    value = tempValue(recType)
    valueRef = toReference(value)
    for i, argRef in enumerate(argRefs) :
        left = recordFieldRef(valueRef, i)
        valueCopy(left, argRef)
    return value

_recordFieldTypes = {}
def recordFieldTypes(recType) :
    assert isType(recType)
    fieldTypes = _recordFieldTypes.get(recType)
    if fieldTypes is None :
        record = recType.tag
        env = extendEnv(record.env, record.typeVars, recType.params)
        valueArgs = recordValueArgs(record)
        fieldTypes = [evaluate(x.type, env, toType) for x in valueArgs]
        _recordFieldTypes[recType] = fieldTypes
    return fieldTypes

def _cleanupRecordFieldTypes() :
    global _recordFieldTypes
    _recordFieldTypes = {}

installGlobalsCleanupHook(_cleanupRecordFieldTypes)



#
# value printer
#

def xconvertBuiltin(r) :
    v = toValue(r)
    return v.buf.value

def xconvertBool(r) :
    v = toValue(r)
    return XSymbol("true" if v.buf.value else "false")

def xconvertPointer(r) :
    return XObject("Ptr", r.type.params[0])

def xconvertTuple(r) :
    elements = [tupleFieldRef(r, i) for i in range(len(r.type.params))]
    return tuple(map(xconvertReference, elements))

def xconvertArray(r) :
    arraySize = toInt(r.type.params[1])
    elements = [arrayRef(r, i) for i in range(arraySize)]
    return map(xconvertReference, elements)

def xconvertRecord(r) :
    fieldCount = len(recordValueArgs(r.type.tag))
    fields = [recordFieldRef(r, i) for i in range(fieldCount)]
    fields = map(xconvertReference, fields)
    return XObject(r.type.tag.name.s, *fields)

def xconvertValue(x) :
    return xconvertReference(toReference(x))

def xconvertReference(x) :
    converter = getXConverter(x.type.tag)
    if converter is not None :
        return converter(x)
    if isRecordType(x.type) :
        return xconvertRecord(x)
    return x

xregister(Value, xconvertValue)
xregister(Reference, xconvertReference)

def getXConverter(tag) :
    if not _xconverters :
        _xconverters[boolTag] = xconvertBool
        _xconverters[intTag] = xconvertBuiltin
        _xconverters[floatTag] = xconvertBuiltin
        _xconverters[doubleTag] = xconvertBuiltin
        _xconverters[pointerTag] = xconvertPointer
        _xconverters[tupleTag] = xconvertTuple
        _xconverters[arrayTag] = xconvertArray
    return _xconverters.get(tag)

_xconverters = {}



#
# converter utilities
#

def convertObject(converter, object_, context) :
    return withContext(context, lambda : converter(object_))

def convertObjects(converter, objects, contexts) :
    result = []
    for object_, context in zip(objects, contexts) :
        result.append(convertObject(converter, object_, context))
    return result



#
# evaluateRootExpr
#

def evaluateRootExpr(expr, env, converter=(lambda x : x)) :
    pushTempsBlock()
    result = evaluate(expr, env, converter)
    popTempsBlock()
    return result



#
# evaluate
#

def evaluate(expr, env, converter=(lambda x : x)) :
    return withContext(expr, lambda : converter(evaluate2(expr, env)))



#
# evaluate2
#

evaluate2 = multimethod(errorMessage="invalid expression")

@evaluate2.register(BoolLiteral)
def foo(x, env) :
    return boolToValue(x.value)

@evaluate2.register(IntLiteral)
def foo(x, env) :
    return intToValue(x.value)

@evaluate2.register(NameRef)
def foo(x, env) :
    return evaluateNameRef(lookupIdent(env, x.name))

@evaluate2.register(Tuple)
def foo(x, env) :
    assert len(x.args) > 0
    eargs = [evaluate(y, env) for y in x.args]
    if len(x.args) == 1 :
        return eargs[0]
    if isType(eargs[0]) or isCell(eargs[0]) :
        elementTypes = convertObjects(toTypeOrCell, eargs, x.args)
        return tupleType(elementTypes)
    else :
        argRefs = convertObjects(toReference, eargs, x.args)
        return makeTuple(argRefs)

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
    thing = evaluate(x.expr, env)
    thingRef = convertObject(toRecordReference, thing, x.expr)
    return recordFieldRef(thingRef, recordFieldIndex(thing.type, x.name))

@evaluate2.register(TupleRef)
def foo(x, env) :
    thing = evaluate(x.expr, env)
    toTupleReference = toReferenceWithTag(tupleTag)
    thingRef = convertObject(toTupleReference, thing, x.expr)
    return tupleFieldRef(thingRef, x.index)

@evaluate2.register(Dereference)
def foo(x, env) :
    return evaluateCall(primitives.pointerDereference(), [x.expr], env)

@evaluate2.register(AddressOf)
def foo(x, env) :
    return evaluateCall(primitives.addressOf(), [x.expr], env)

@evaluate2.register(UnaryOpExpr)
def foo(x, env) :
    return evaluate(convertUnaryOpExpr(x), env)

@evaluate2.register(BinaryOpExpr)
def foo(x, env) :
    return evaluate(convertBinaryOpExpr(x), env)

@evaluate2.register(NotExpr)
def foo(x, env) :
    v = evaluate(x.expr, env, toBool)
    return boolToValue(not v)

@evaluate2.register(AndExpr)
def foo(x, env) :
    v = evaluate(x.expr1, env, toBool)
    if not v :
        return boolToValue(False)
    return boolToValue(evaluate(x.expr2, env, toBool))

@evaluate2.register(OrExpr)
def foo(x, env) :
    v = evaluate(x.expr1, env, toBool)
    if v :
        return boolToValue(True)
    return boolToValue(evaluate(x.expr2, env, toBool))

@evaluate2.register(StaticExpr)
def foo(x, env) :
    return evaluate(x.expr, env, toStatic)

@evaluate2.register(SCExpression)
def foo(x, env) :
    return evaluate(x.expr, x.env)



#
# convertUnaryOpExpr, convertBinaryOpExpr
#

_unaryOps = {"+" : "plus",
             "-" : "minus"}

def convertUnaryOpExpr(x) :
    return Call(primitiveNameRef(_unaryOps[x.op]), [x.expr])

_binaryOps = {"+" : "add",
              "-" : "subtract",
              "*" : "multiply",
              "/" : "divide",
              "%" : "modulus",
              "==" : "equals",
              "!=" : "notEquals",
              "<" : "lesser",
              "<=" : "lesserEquals",
              ">" : "greater",
              ">=" : "greaterEquals"}

def convertBinaryOpExpr(x) :
    return Call(primitiveNameRef(_binaryOps[x.op]), [x.expr1, x.expr2])



#
# evaluateNameRef
#

evaluateNameRef = multimethod(defaultProc=(lambda x : x))

@evaluateNameRef.register(Value)
def foo(x) :
    return toReference(x)



#
# evaluateIndexing
#

evaluateIndexing = multimethod(errorMessage="invalid indexing")

@evaluateIndexing.register(Record)
def foo(x, args, env) :
    ensureArity(args, len(x.typeVars))
    typeParams = [evaluate(y, env, toStaticOrCell) for y in args]
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

@evaluateCall.register(Type)
def foo(x, args, env) :
    ensure(isRecordType(x), "only record type constructors are supported")
    fieldTypes = recordFieldTypes(x)
    ensureArity(args, len(fieldTypes))
    argRefs = []
    for arg, fieldType in zip(args, fieldTypes) :
        argRef = evaluate(arg, env, toReferenceOfType(fieldType))
        argRefs.append(argRef)
    return makeRecord(x, argRefs)

@evaluateCall.register(Record)
def foo(x, args, env) :
    actualArgs = [ActualArgument(y, env) for y in args]
    result = matchRecordInvoke(x, actualArgs)
    if type(result) is InvokeError :
        result.signalError()
    assert type(result) is InvokeBindings
    recType = recordType(x, result.typeParams)
    return makeRecord(recType, result.params)

@evaluateCall.register(Procedure)
def foo(x, args, env) :
    actualArgs = [ActualArgument(y, env) for y in args]
    result = matchInvoke(x.code, x.env, actualArgs)
    if type(result) is InvokeError :
        result.signalError()
    assert type(result) is InvokeBindings
    return evalInvoke(x.code, x.env, result)

@evaluateCall.register(Overloadable)
def foo(x, args, env) :
    actualArgs = [ActualArgument(y, env) for y in args]
    for y in x.overloads :
        result = matchInvoke(y.code, y.env, actualArgs)
        if type(result) is InvokeError :
            continue
        assert type(result) is InvokeBindings
        return evalInvoke(y.code, y.env, result)
    error("no matching overload")



#
# evaluate primitives
#

@evaluateCall.register(primitives._print)
def foo(x, args, env) :
    ensureArity(args, 1)
    v = evaluate(args[0], env, toReference)
    xprint(v)
    return voidValue

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
    return intToValue(typeSize(t))



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
    eargs = []
    mopInput = []
    for arg, argType in zip(args, argTypes) :
        earg = evaluate(arg, env)
        eargs.append(earg)
        ref = convertObject(toReferenceOfType(argType), earg, arg)
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
    eargs = [evaluate(y, env) for y in args]
    cell = Cell()
    converter = toReferenceOfType(pointerType(cell))
    ptr = convertObject(converter, eargs[0], args[0])
    offset = convertObject(toReferenceOfType(intType), eargs[1], args[1])
    result = tempValue(ptr.type)
    callMachineOp(mop_pointerOffset, [ptr, offset, result])
    return result

@evaluateCall.register(primitives.pointerSubtract)
def foo(x, args, env) :
    ensureArity(args, 2)
    eargs = [evaluate(y, env) for y in args]
    cell = Cell()
    converter = toReferenceOfType(pointerType(cell))
    ptr1 = convertObject(converter, eargs[0], args[0])
    ptr2 = convertObject(converter, eargs[1], args[1])
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
    eargs = [evaluate(y, env) for y in args]
    cell = Cell()
    converter = toReferenceOfType(pointerType(cell))
    destRef = convertObject(converter, eargs[0], args[0])
    srcRef = convertObject(converter, eargs[1], args[1])
    callMachineOp(mop_pointerCopy, [destRef, srcRef])
    return voidValue

@evaluateCall.register(primitives.pointerEquals)
def foo(x, args, env) :
    ensureArity(args, 2)
    eargs = [evaluate(y, env) for y in args]
    cell = Cell()
    converter = toReferenceOfType(pointerType(cell))
    ptr1 = convertObject(converter, eargs[0], args[0])
    ptr2 = convertObject(converter, eargs[1], args[1])
    result = tempValue(boolType)
    callMachineOp(mop_pointerEquals, [ptr1, ptr2, result])
    return result

@evaluateCall.register(primitives.pointerLesser)
def foo(x, args, env) :
    ensureArity(args, 2)
    eargs = [evaluate(y, env) for y in args]
    cell = Cell()
    converter = toReferenceOfType(pointerType(cell))
    ptr1 = convertObject(converter, eargs[0], args[0])
    ptr2 = convertObject(converter, eargs[1], args[1])
    result = tempValue(boolType)
    callMachineOp(mop_pointerLesser, [ptr1, ptr2, result])
    return result

@evaluateCall.register(primitives.allocateMemory)
def foo(x, args, env) :
    ensureArity(args, 1)
    size = evaluate(args[0], env)
    sizeRef = convertObject(toReferenceOfType(intType), size, args[0])
    result = tempValue(pointerType(intType))
    callMachineOp(mop_allocateMemory, [sizeRef, result])
    return result

@evaluateCall.register(primitives.freeMemory)
def foo(x, args, env) :
    ensureArity(args, 1)
    ptr = evaluate(args[0], env)
    converter = toReferenceOfType(pointerType(intType))
    ptrRef = convertObject(converter, ptr, args[0])
    callMachineOp(mop_freeMemory, [ptrRef])
    return voidValue



#
# evaluate tuple primitives
#

@evaluateCall.register(primitives.TupleType)
def foo(x, args, env) :
    ensureArity(args, 1)
    t = evaluate(args[0], env, toType)
    return boolToValue(isTupleType(t))

@evaluateCall.register(primitives.tuple)
def foo(x, args, env) :
    ensure(len(args) > 1, "tuples need atleast two members")
    eargs = [evaluate(y, env) for y in args]
    argRefs = convertObjects(toReference, eargs, args)
    return makeTuple(argRefs)

@evaluateCall.register(primitives.tupleFieldCount)
def foo(x, args, env) :
    ensureArity(args, 1)
    t = evaluate(args[0], env, toTypeWithTag(tupleTag))
    return intToValue(len(t.params))

@evaluateCall.register(primitives.tupleFieldRef)
def foo(x, args, env) :
    ensureArity(args, 2)
    eargs = [evaluate(y, env) for y in args]
    converter = toReferenceWithTag(tupleTag)
    tupleRef = convertObject(converter, eargs[0], args[0])
    i = convertObject(toInt, eargs[1], args[1])
    return tupleFieldRef(tupleRef, i)



#
# evaluate array primitives
#

@evaluateCall.register(primitives.array)
def foo(x, args, env) :
    ensureArity(args, 2)
    eargs = [evaluate(y, env) for y in args]
    elementType = convertObject(toType, eargs[0], args[0])
    n = convertObject(toInt, eargs[1], args[1])
    a = tempValue(arrayType(elementType, intToValue(n)))
    valueInit(a)
    return a

@evaluateCall.register(primitives.arrayRef)
def foo(x, args, env) :
    ensureArity(args, 2)
    eargs = [evaluate(y, env) for y in args]
    converter = toReferenceWithTag(arrayTag)
    a = convertObject(converter, eargs[0], args[0])
    i = convertObject(toInt, eargs[1], args[1])
    return arrayRef(a, i)



#
# evaluate record primitives
#

@evaluateCall.register(primitives.RecordType)
def foo(x, args, env) :
    ensureArity(args, 1)
    t = evaluate(args[0], env, toType)
    return boolToValue(isRecordType(t))

@evaluateCall.register(primitives.recordFieldCount)
def foo(x, args, env) :
    ensureArity(args, 1)
    t = evaluate(args[0], env, toRecordType)
    return intToValue(len(recordValueArgs(t.tag)))

@evaluateCall.register(primitives.recordFieldRef)
def foo(x, args, env) :
    ensureArity(args, 2)
    eargs = [evaluate(y, env) for y in args]
    recRef = convertObject(toRecordReference, eargs[0], args[0])
    i = convertObject(toInt, eargs[1], args[1])
    return recordFieldRef(recRef, i)



#
# evaluate bool primitives
#

@evaluateCall.register(primitives.boolCopy)
def foo(x, args, env) :
    argTypes = [boolType, boolType]
    return simpleMop(mop_boolCopy, args, env, argTypes, None)



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
# wrapper for actual arguments
#

class ActualArgument(object) :
    def __init__(self, expr, env) :
        self.expr = expr
        self.env = env
        self.result_ = evaluate(self.expr, self.env)

    def asReference(self) :
        return withContext(self.expr, lambda : toReference(self.result_))

    def asStatic(self) :
        return withContext(self.expr, lambda : toStatic(self.result_))



#
# matchInvoke(code, env, actualArgs) -> InvokeBindings | InvokeError
#

class InvokeError(object) :
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

class InvokeBindings(object) :
    def __init__(self, typeVars, typeParams, vars, params) :
        self.typeVars = typeVars
        self.typeParams = typeParams
        self.vars = vars
        self.params = params

def matchInvoke(code, codeEnv, actualArgs) :
    def argMismatch(actualArg) :
        return InvokeError("argument mismatch", actualArg.expr)
    def predicateFailure(typeCond) :
        return InvokeError("predicate failed", typeCond)
    if len(actualArgs) != len(code.formalArgs) :
        return InvokeError("incorrect no. of arguments")
    codeEnv2, cells = bindTypeVars(codeEnv, code.typeVars)
    vars, params = [], []
    for actualArg, formalArg in zip(actualArgs, code.formalArgs) :
        if type(formalArg) is ValueArgument :
            arg = actualArg.asReference()
            if formalArg.type is not None :
                typePattern = evaluate(formalArg.type, codeEnv2, toTypeOrCell)
                if not unify(typePattern, arg.type) :
                    return argMismatch(actualArg)
            vars.append(formalArg.name)
            params.append(arg)
        elif type(formalArg) is StaticArgument :
            arg = actualArg.asStatic()
            pattern = evaluate(formalArg.pattern, codeEnv2, toStaticOrCell)
            if not unify(pattern, arg) :
                return argMismatch(actualArg)
        else :
            assert False
    typeParams = resolveTypeVars(code.typeVars, cells)
    if code.predicate is not None :
        codeEnv2 = extendEnv(codeEnv, code.typeVars, typeParams)
        if not evaluate(code.predicate, codeEnv2, toBool) :
            return predicateFailure(code.predicate)
    return InvokeBindings(code.typeVars, typeParams, vars, params)



#
# matchRecordInvoke(record, actualArgs) -> InvokeBindings | InvokeError
#

def matchRecordInvoke(record, actualArgs) :
    def argMismatch(actualArg) :
        return InvokeError("argument mismatch", actualArg.expr)
    if len(actualArgs) != len(record.args) :
        return InvokeError("incorrect no. of arguments")
    env, cells = bindTypeVars(record.env, record.typeVars)
    vars, params = [], []
    for actualArg, formalArg in zip(actualArgs, record.args) :
        if type(formalArg) is ValueRecordArg :
            arg = actualArg.asReference()
            typePattern = evaluate(formalArg.type, env, toTypeOrCell)
            if not unify(typePattern, arg.type) :
                return argMismatch(actualArg)
            vars.append(formalArg.name)
            params.append(arg)
        elif type(formalArg) is StaticRecordArg :
            arg = actualArg.asStatic()
            pattern = evaluate(formalArg.pattern, env, toStaticOrCell)
            if not unify(pattern, arg) :
                return argMismatch(actualArg)
        else :
            assert False
    typeParams = resolveTypeVars(record.typeVars, cells)
    return InvokeBindings(record.typeVars, typeParams, vars, params)



#
# evalInvoke
#

def evalInvoke(code, env, bindings) :
    env = extendEnv(env, bindings.typeVars + bindings.vars,
                    bindings.typeParams + bindings.params)
    returnType = None
    if code.returnType is not None :
        returnType = evaluate(code.returnType, env, toType)
    context = CodeContext(code.returnByRef, returnType)
    result = evalStatement(code.body, env, context)
    if type(result) is Goto :
        withContext(result.labelName, lambda : error("label not found"))
    if type(result) is Break :
        withContext(result, lambda : error("invalid break statement"))
    if type(result) is Continue :
        withContext(result, lambda : error("invalid continue statement"))
    if isValue(result) :
        installTemp(result)
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
    def __init__(self, returnByRef, returnType) :
        self.returnByRef = returnByRef
        self.returnType = returnType

def evalStatement(stmt, env, context) :
    return withContext(stmt, lambda : evalStatement2(stmt, env, context))



#
# evalStatement2
#

evalStatement2 = multimethod(errorMessage="invalid statement")

@evalStatement2.register(Block)
def foo(x, env, context) :
    i = 0
    labels = {}
    evalCollectLabels(x.statements, i, labels, env)
    while i < len(x.statements) :
        statement = x.statements[i]
        if type(statement) is LocalBinding :
            converter = toValueOrReference
            if statement.type is not None :
                declaredType = evaluate(statement.type, env, toType)
                converter = toValueOrReferenceOfType(declaredType)
            right = evaluateRootExpr(statement.expr, env, converter)
            if not statement.byRef :
                right = toValue(right)
            env = extendEnv(env, [statement.name], [right])
            evalCollectLabels(x.statements, i+1, labels, env)
        elif type(statement) is Label :
            pass
        else :
            result = evalStatement(statement, env, context)
            if type(result) is Goto :
                envAndPos = labels.get(result.labelName.s)
                if envAndPos is None :
                    return result
                else :
                    env, i = envAndPos
                    continue
            if result is not None :
                return result
        i += 1

def evalCollectLabels(statements, startIndex, labels, env) :
    i = startIndex
    while i < len(statements) :
        stmt = statements[i]
        if type(stmt) is Label :
            labels[stmt.name.s] = (env, i)
        elif type(stmt) is LocalBinding :
            break
        i += 1

@evalStatement2.register(Assignment)
def foo(x, env, context) :
    pushTempsBlock()
    left = evaluate(x.left, env, toLValue)
    right = evaluate(x.right, env)
    rightRef = convertObject(toReferenceOfType(left.type), right, x.right)
    valueAssign(left, rightRef)
    popTempsBlock()

@evalStatement2.register(Goto)
def foo(x, env, context) :
    return x

@evalStatement2.register(Return)
def foo(x, env, context) :
    if context.returnByRef :
        result = evaluateRootExpr(x.expr, env, toLValue)
        resultType = result.type
    elif x.expr is None :
        result = voidValue
        resultType = voidType
    else :
        result = evaluateRootExpr(x.expr, env, toStatic)
        resultType = result.type if isValue(result) else None
    if context.returnType is not None :
        def check() :
            matchType(resultType, context.returnType)
        withContext(x.expr, check)
    return result

@evalStatement2.register(IfStatement)
def foo(x, env, context) :
    cond = evaluateRootExpr(x.condition, env, toBool)
    if cond :
        return evalStatement(x.thenPart, env, context)
    elif x.elsePart is not None :
        return evalStatement(x.elsePart, env, context)

@evalStatement2.register(ExprStatement)
def foo(x, env, context) :
    evaluateRootExpr(x.expr, env)

@evalStatement2.register(While)
def foo(x, env, context) :
    while True :
        cond = evaluateRootExpr(x.condition, env, toBool)
        if not cond :
            break
        result = evalStatement(x.body, env, context)
        if type(result) is Break :
            break
        if type(result) is Continue :
            continue
        if result is not None :
            return result

@evalStatement2.register(Break)
def foo(x, env, context) :
    return x

@evalStatement2.register(Continue)
def foo(x, env, context) :
    return x

@evalStatement2.register(For)
def foo(x, env, context) :
    return evalStatement(convertForStatement(x), env, context)



#
# expand for statement to use while
#

def convertForStatement(x) :
    exprVar = Identifier("%expr")
    iterVar = Identifier("%iter")
    block = Block(
        [LocalBinding(True, exprVar, None, x.expr),
         LocalBinding(False, iterVar, None,
                      Call(primitiveNameRef("iterator"), [NameRef(exprVar)])),
         While(Call(primitiveNameRef("hasNext"), [NameRef(iterVar)]),
               Block([LocalBinding(True, x.variable, x.type,
                                   Call(primitiveNameRef("next"),
                                        [NameRef(iterVar)])),
                      x.body]))])
    return block



#
# remove temp name used for multimethod instances
#

del foo
