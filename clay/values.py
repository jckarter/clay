import ctypes
from clay.xprint import *
from clay.error import *
from clay.multimethod import *
from clay.coreops import *
from clay.env import *



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
from clay.types import *
