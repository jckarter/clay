from clay.core import *

class PrimitiveOp(object) :
    pass

class TypePredicatePrimOp(PrimitiveOp) :
    def __init__(self, klass) :
        super(TypePredicatePrimOp, self).__init__()
        self.klass = klass

class PredicatePrimOp(PrimitiveOp) :
    def __init__(self, pred) :
        super(PredicatePrimOp, self).__init__()
        self.pred = pred

class SimplePrimOp(PrimitiveOp) :
    def __init__(self, argTypes, returnType, returnByRef=False,
                 cells=None, condition=None) :
        super(SimplePrimOp, self).__init__()
        self.argTypes = argTypes
        self.returnType = returnType
        self.returnByRef = returnByRef
        if cells is None :
            cells = []
        self.cells = cells
        self.condition = condition


#
# install primitives
#

Primitives = None

def installPrimitives() :
    primitiveTypes = {
        "Bool": boolType,
        "Int8"  : int8Type,
        "Int16" : int16Type,
        "Int32" : int32Type,
        "Int64" : int64Type,
        "UInt8"  : uint8Type,
        "UInt16" : uint16Type,
        "UInt32" : uint32Type,
        "UInt64" : uint64Type,
        "Float32" : float32Type,
        "Float64" : float64Type,
        "Void" : voidType,
        "CompilerObject" : compilerObjectType,
        }
    for k, t in primitiveTypes.items() :
        installPrimitive(k, toCOValue(t))

    primClasses = {}

    def safeName(s) :
        if s.endswith("?") :
            return s[:-1] + "P"
        return s

    def primOp(name, klass, *args, **kwargs) :
        name2 = safeName(name)
        primClass = type("Prim_%s" % name2, (klass,), {})
        primClasses[name2] = primClass
        prim = primClass(*args, **kwargs)
        installPrimitive(name, toCOValue(prim))

    primOp("Type?",              TypePredicatePrimOp, Type)
    primOp("TypeSize",           SimplePrimOp,        [compilerObjectType], int32Type)

    primOp("BoolType?",          TypePredicatePrimOp, BoolType)

    primOp("IntegerType?",       TypePredicatePrimOp, IntegerType)
    primOp("SignedIntegerType?", PredicatePrimOp,     lambda t : isinstance(t, IntegerType) and t.signed)

    primOp("FloatType?",         TypePredicatePrimOp, FloatType)

    def numericTypeP(t) :
        return isinstance(t, IntegerType) or isinstance(t, FloatType)

    def binaryOp(name, pred, returnType=None) :
        type_ = Cell()
        if returnType is None :
            returnType = type_
        primOp(name, SimplePrimOp, [type_, type_], returnType,
               cells=[type_], condition=pred)

    binaryOp("numericEquals?",   numericTypeP, returnType=boolType)
    binaryOp("numericLesser?",   numericTypeP, returnType=boolType)
    binaryOp("numericAdd",       numericTypeP)
    binaryOp("numericSubtract",  numericTypeP)
    binaryOp("numericMultiply",  numericTypeP)
    binaryOp("numericDivide",    numericTypeP)
    binaryOp("numericRemainder", numericTypeP)

    a = Cell()
    primOp("numericNegate",  SimplePrimOp, [a], a, cells=[a], condition=numericTypeP)
    primOp("numericConvert", PrimitiveOp)

    def integerTypeP(t) :
        return isinstance(t, IntegerType)

    binaryOp("shiftLeft",  integerTypeP)
    binaryOp("shiftRight", integerTypeP)
    binaryOp("bitwiseAnd", integerTypeP)
    binaryOp("bitwiseOr",  integerTypeP)
    binaryOp("bitwiseXor", integerTypeP)

    primOp("VoidType?",       TypePredicatePrimOp, VoidType)
    primOp("CompilerObject?", TypePredicatePrimOp, CompilerObjectType)
    primOp("PointerType?",    TypePredicatePrimOp, PointerType)
    primOp("PointerType",     SimplePrimOp,        [compilerObjectType], compilerObjectType)
    primOp("PointeeType",     SimplePrimOp,        [compilerObjectType], compilerObjectType)

    a = Cell()
    primOp("addressOf",          SimplePrimOp, [a], PointerTypePattern(a), cells=[a])
    a = Cell()
    primOp("pointerDereference", SimplePrimOp, [PointerTypePattern(a)], a, returnByRef=True, cells=[a])
    primOp("pointerToInt",       PrimitiveOp)
    primOp("intToPointer",       PrimitiveOp)
    primOp("pointerCast",        PrimitiveOp)
    primOp("allocateMemory",     PrimitiveOp)
    a = Cell()
    primOp("freeMemory",         SimplePrimOp, [PointerTypePattern(a)], voidType, cells=[a])

    primOp("ArrayType?",       TypePredicatePrimOp, ArrayType)
    primOp("ArrayType",        SimplePrimOp,        [compilerObjectType, int32Type], compilerObjectType)
    primOp("ArrayElementType", SimplePrimOp,        [compilerObjectType], compilerObjectType)
    primOp("ArraySize",        SimplePrimOp,        [compilerObjectType], int32Type)
    primOp("array",            PrimitiveOp)
    a, b = Cell(), Cell()
    primOp("arrayRef",         SimplePrimOp,        [ArrayTypePattern(a, b), int32Type], a, returnByRef=True, cells=[a, b])

    primOp("TupleType?",       TypePredicatePrimOp, TupleType)
    primOp("TupleType",        PrimitiveOp)
    primOp("TupleElementType", SimplePrimOp,        [compilerObjectType, int32Type], compilerObjectType)
    primOp("TupleFieldCount",  SimplePrimOp,        [compilerObjectType], int32Type)
    primOp("TupleFieldOffset", SimplePrimOp,        [compilerObjectType, int32Type], int32Type)
    primOp("tuple",            PrimitiveOp)
    primOp("tupleFieldRef",    PrimitiveOp)

    primOp("RecordType?",          TypePredicatePrimOp, RecordType)
    primOp("RecordType",           PrimitiveOp)
    primOp("RecordElementType",    SimplePrimOp,        [compilerObjectType, int32Type], compilerObjectType)
    primOp("RecordFieldCount",     SimplePrimOp,        [compilerObjectType], int32Type)
    primOp("RecordFieldOffset",    SimplePrimOp,        [compilerObjectType, int32Type], int32Type)
    primOp("RecordFieldIndex",     SimplePrimOp,        [compilerObjectType, compilerObjectType], int32Type)
    primOp("makeRecordInstance",   PrimitiveOp)
    primOp("recordFieldRef",       PrimitiveOp)
    primOp("recordFieldRefByName", PrimitiveOp)

    primOp("makeInstance", PrimitiveOp)

    global Primitives
    Primitives = type("Primitives", (object,), primClasses)

installPrimitives()
