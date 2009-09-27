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
class IntTag(object) : pass
class FloatTag(object) : pass
class DoubleTag(object) : pass
class VoidTag(object) : pass
class TupleTag(object) : pass
class ArrayTag(object) : pass
class PointerTag(object) : pass

boolTag = BoolTag()
intTag = IntTag()
floatTag = FloatTag()
doubleTag = DoubleTag()
voidTag = VoidTag()

tupleTag = TupleTag()
arrayTag = ArrayTag()
pointerTag = PointerTag()

boolType = Type(boolTag, [])
intType = Type(intTag, [])
floatType = Type(floatTag, [])
doubleType = Type(doubleTag, [])
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
installPrimitive("Int", intType)
installPrimitive("Float", floatType)
installPrimitive("Double", doubleType)
installPrimitive("Void", voidType)



#
# type predicates
#

def isType(t) : return type(t) is Type
def isBoolType(t) : return t.tag is boolTag
def isIntType(t) : return t.tag is intTag
def isFloatType(t) : return t.tag is floatTag
def isDoubleType(t) : return t.tag is doubleTag
def isVoidType(t) : return t.tag is voidTag

def isTupleType(t) : return t.tag is tupleTag
def isArrayType(t) : return t.tag is arrayTag
def isPointerType(t) : return t.tag is pointerTag
def isRecordType(t) : return type(t.tag) is Record

_simpleTags = (boolTag, intTag, floatTag, doubleTag, pointerTag)
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
_tagNames[intTag] = "Int"
_tagNames[floatTag] = "Float"
_tagNames[doubleTag] = "Double"
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

def toRecordType(t) :
    ensure(isType(t) and isRecordType(t), "record type expected")
    return t

def toReferenceWithTag(tag) :
    def f(x) :
        r = toReference(x)
        ensure(r.type.tag is tag, "type mismatch")
        return r
    return f

def toRecordReference(x) :
    r = toReference(x)
    ensure(isRecordType(r.type), "record type expected")
    return r



#
# remove temp name used for multimethod instances
#

del foo
