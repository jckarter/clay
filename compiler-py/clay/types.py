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
installPrimitive("Char", charType)
installPrimitive("Void", voidType)



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

simpleTypeTags = (boolTypeTag, intTypeTag, floatTypeTag,
                  doubleTypeTag, charTypeTag, pointerTypeTag)
def isSimpleType(t) :
    return t.tag in simpleTypeTags



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

def toReferenceWithTypeTag(tag) :
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
