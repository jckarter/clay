from clay.xprint import *
from clay.error import *
from clay.multimethod import *
from clay.ast import *
from clay.env import *
from clay.coreops import *



#
# types
#

class Type(object) :
    def __init__(self, tag, params) :
        self.tag = tag
        self.params = params
    def __eq__(self, other) :
        return equals(self, other)
    def __hash__(self) :
        return hash((self.tag, tuple(self.params)))

class BoolTag(object) : pass

class IntegerTag(object) :
    def __init__(self, bits, signed) :
        self.bits = bits
        self.signed = signed

class FloatingPointTag(object) :
    def __init__(self, bits) :
        self.bits = bits

class VoidTag(object) : pass

class TupleTag(object) : pass
class ArrayTag(object) : pass
class PointerTag(object) : pass

boolTag = BoolTag()

int8Tag  = IntegerTag(8,  signed=True)
int16Tag = IntegerTag(16, signed=True)
int32Tag = IntegerTag(32, signed=True)
int64Tag = IntegerTag(64, signed=True)

uint8Tag  = IntegerTag(8,  signed=False)
uint16Tag = IntegerTag(16, signed=False)
uint32Tag = IntegerTag(32, signed=False)
uint64Tag = IntegerTag(64, signed=False)

float32Tag = FloatingPointTag(32)
float64Tag = FloatingPointTag(64)

voidTag = VoidTag()

tupleTag = TupleTag()
arrayTag = ArrayTag()
pointerTag = PointerTag()

boolType = Type(boolTag, [])

int8Type  = Type(int8Tag, [])
int16Type = Type(int16Tag, [])
int32Type = Type(int32Tag, [])
int64Type = Type(int64Tag, [])

uint8Type  = Type(uint8Tag, [])
uint16Type = Type(uint16Tag, [])
uint32Type = Type(uint32Tag, [])
uint64Type = Type(uint64Tag, [])

nativeIntType = int32Type

float32Type = Type(float32Tag, [])
float64Type = Type(float64Tag, [])

voidType = Type(voidTag, [])

def tupleType(types) :
    assert len(types) >= 2
    return Type(tupleTag, types)

def arrayType(type_, sizeValue) :
    return Type(arrayTag, [type_, sizeValue])

def pointerType(type_) :
    return Type(pointerTag, [type_])

def recordType(record, params) :
    return Type(record, params)



#
# VoidValue, voidValue
#

class VoidValue(object) :
    def __init__(self) :
        self.type = voidType

voidValue = VoidValue()

def isVoidValue(x) :
    return type(x) is VoidValue

xregister(VoidValue, lambda x : XSymbol("void"))



#
# install primitive types
#

installPrimitive("Bool", boolType)

installPrimitive("Int8",  int8Type)
installPrimitive("Int16", int16Type)
installPrimitive("Int32", int32Type)
installPrimitive("Int64", int64Type)

installPrimitive("UInt8",  uint8Type)
installPrimitive("UInt16", uint16Type)
installPrimitive("UInt32", uint32Type)
installPrimitive("UInt64", uint64Type)

installPrimitive("Float32", float32Type)
installPrimitive("Float64", float64Type)

installPrimitive("Void", voidType)



#
# type predicates
#

def isType(t) : return type(t) is Type
def isBoolType(t) : return t.tag is boolTag

def isIntegerType(t) : return type(t.tag) is IntegerTag

def isInt8Type(t)  : return t.tag is int8Tag
def isInt16Type(t) : return t.tag is int16Tag
def isInt32Type(t) : return t.tag is int32Tag
def isInt64Type(t) : return t.tag is int64Tag

def isUInt8Type(t)  : return t.tag is uint8Tag
def isUInt16Type(t) : return t.tag is uint16Tag
def isUInt32Type(t) : return t.tag is uint32Tag
def isUInt64Type(t) : return t.tag is uint64Tag

def isFloatingPointType(t) : return type(t.tag) is FloatingPointTag

def isFloat32Type(t) : return t.tag is float32Tag
def isFloat64Type(t) : return t.tag is float64Tag

def isVoidType(t) : return t.tag is voidTag

def isTupleType(t) : return t.tag is tupleTag
def isArrayType(t) : return t.tag is arrayTag
def isPointerType(t) : return t.tag is pointerTag
def isRecordType(t) : return type(t.tag) is Record

_simpleTags = set([boolTag, int8Tag, int16Tag, int32Tag, int64Tag,
                   uint8Tag, uint16Tag, uint32Tag, uint64Tag,
                   float32Tag, float64Tag, pointerTag])
def isSimpleType(t) :
    return t.tag in _simpleTags



#
# equals
#

@equals.register(Type, Type)
def foo(a, b) :
    if a.tag is not b.tag :
        return False
    for x, y in zip(a.params, b.params) :
        if not equals(x, y) :
            return False
    return True



#
# toType, toStatic
#

@toType.register(Type)
def foo(x) :
    return x

@toType.register(Record)
def foo(x) :
    ensure(len(x.typeVars) == 0, "record type parameters expected")
    return recordType(x, [])

@toStatic.register(Type)
def foo(x) :
    return x



#
# type printer
#


xregister(Type, lambda t : (XObject(tagName(t), *t.params,
                                    opening="[", closing="]")
                            if t.params
                            else XSymbol(tagName(t))))

def tagName(t) :
    if type(t.tag) is Record :
        return t.tag.name.s
    return _tagNames[t.tag]

_tagNames = {}
_tagNames[boolTag] = "Bool"
_tagNames[int8Tag] = "Int8"
_tagNames[int16Tag] = "Int16"
_tagNames[int32Tag] = "Int32"
_tagNames[int64Tag] = "Int64"
_tagNames[uint8Tag] = "UInt8"
_tagNames[uint16Tag] = "UInt16"
_tagNames[uint32Tag] = "UInt32"
_tagNames[uint64Tag] = "UInt64"
_tagNames[float32Tag] = "Float32"
_tagNames[float64Tag] = "Float64"
_tagNames[voidTag] = "Void"
_tagNames[tupleTag] = "Tuple"
_tagNames[arrayTag] = "Array"
_tagNames[pointerTag] = "Pointer"



#
# type checking converters
#

def toTypeWithTag(tag) :
    def f(x) :
        t = toType(x)
        ensure(t.tag is tag, "type mismatch")
        return t
    return f

def toIntegerType(t) :
    ensure(isType(t) and isIntegerType(t), "integer type expected")
    return t

def toFloatingPointType(t) :
    ensure(isType(t) and isFloatingPointType(t),
           "floating point type expected")
    return t

def toNumericType(t) :
    ensure(isType(t) and (isIntegerType(t) or isFloatingPointType(t)),
           "numeric type expected")
    return t

def toRecordType(t) :
    ensure(isType(t) and isRecordType(t), "record type expected")
    return t

def toReferenceWithTag(tag) :
    def f(x) :
        r = toReference(x)
        ensure(r.type.tag is tag, "type mismatch")
        return r
    return f

def toIntegerReference(x) :
    r = toReference(x)
    ensure(isIntegerType(r.type), "integer type expected")
    return r

def toFloatingPointReference(x) :
    r = toReference(x)
    ensure(isFloatingPointType(r.type), "floating point type expected")
    return r

def toNumericReference(x) :
    r = toReference(x)
    ensure(isIntegerType(r.type) or isFloatingPointType(r.type),
           "numeric type expected")
    return r

def toRecordReference(x) :
    r = toReference(x)
    ensure(isRecordType(r.type), "record type expected")
    return r



#
# remove temp name used for multimethod instances
#

del foo
