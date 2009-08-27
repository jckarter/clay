import ctypes
from clay.xprint import *
from clay.error import *
from clay.multimethod import *
from clay.coreops import *
from clay.env import *
from clay.types import *
from clay.unify import *



#
# convert to ctypes
#

_ctypesTable = {}
_ctypesTable[boolType] = ctypes.c_bool
_ctypesTable[intType] = ctypes.c_int
_ctypesTable[floatType] = ctypes.c_float
_ctypesTable[doubleType] = ctypes.c_double
_ctypesTable[charType] = ctypes.c_wchar

ctypesType = multimethod(errorMessage="invalid type")

@ctypesType.register(Type)
def foo(t) :
    ct = _ctypesTable.get(t)
    if ct is None :
        ct = makeCTypesType(t.tag, t)
    return ct

makeCTypesType = multimethod(errorMessage="invalid type tag")

@makeCTypesType.register(TupleTypeTag)
def foo(tag, t) :
    fieldCTypes = [ctypesType(x) for x in t.params]
    fieldCNames = ["f%d" % x for x in range(len(t.params))]
    ct = type("Tuple", (ctypes.Structure,), {})
    ct._fields_ = zip(fieldCNames, fieldCTypes)
    _ctypesTable[t] = ct
    return ct

@makeCTypesType.register(ArrayTypeTag)
def foo(tag, t) :
    ensure(len(t.params) == 2, "invalid array type")
    ct = ctypesType(t.params[0]) * toInt(t.params[1])
    _ctypesTable[t] = ct
    return ct

@makeCTypesType.register(PointerTypeTag)
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
    fieldCNames = [f.name.s for f in t.tag.fields]
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
# toValue, toLValue, toReference, toStatic
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

toStatic.register(Value)(lambda x : x)
toStatic.register(Reference)(lambda x : toValue(x))



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
# boolToValue, intToValue, charToValue
#

def boolToValue(x) :
    v = tempValue(boolType)
    v.buf.value = x
    return v

def intToValue(x) :
    v = tempValue(intType)
    v.buf.value = x
    return v

def charToValue(x) :
    v = tempValue(charType)
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
        return
    _callBuiltin("destroy", [a])

def valueEquals(a, b) :
    assert equals(a.type, b.type)
    # TODO: add bypass for simple types
    return _callBuiltin("equals", [a, b], toBool)

def valueHash(a) :
    return _callBuiltin("hash", [a], toInt)



#
# equals, hashify
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


@hashify.register(Value)
def foo(a) :
    return valueHash(a)

@hashify.register(Reference)
def foo(a) :
    return valueHash(a)



#
# arrays, tuples, records
#

def arrayRef(a, i) :
    assert isReference(a)
    cell, sizeCell = Cell(), Cell()
    matchType(arrayType(cell, sizeCell), a.type)
    ensure((0 <= i < toInt(sizeCell.param)), "array index out of range")
    elementType = cell.param
    return Reference(elementType, a.address + i*typeSize(elementType))

def makeArray(n, defaultElement) :
    assert isReference(defaultElement)
    value = tempValue(arrayType(defaultElement.type, intToValue(n)))
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
        env = extendEnv(record.env, record.typeVars, recType.params)
        fieldTypes = computeFieldTypes(record.fields, env)
        _recordFieldTypes[recType] = fieldTypes
    return fieldTypes



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
        _xconverters[boolTypeTag] = xconvertBool
        _xconverters[intTypeTag] = xconvertBuiltin
        _xconverters[floatTypeTag] = xconvertBuiltin
        _xconverters[doubleTypeTag] = xconvertBuiltin
        _xconverters[charTypeTag] = xconvertBuiltin
        _xconverters[pointerTypeTag] = xconvertPointer
        _xconverters[tupleTypeTag] = xconvertTuple
        _xconverters[arrayTypeTag] = xconvertArray
    return _xconverters.get(tag)

_xconverters = {}



#
# remove temp name used for multimethod instances
#

del foo


# TODO: fix cyclic import
from clay.evaluator import *
