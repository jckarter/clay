from clay.core import *
from clay.env import *

class PrimitiveOp(object) :
    pass


#
# initialize primitives
#

PrimClasses = None
PrimObjects = None

def initializePrimitives() :
    def primType(name, type_) :
        installPrimitive(name, StaticValue(toCOValue(type_)))

    primClasses = {}
    primObjects = {}

    def safeName(s) :
        if s.endswith("?") :
            return s[:-1] + "P"
        return s

    def primOp(name) :
        name2 = safeName(name)
        primClass = type("Prim_%s" % name2, (PrimitiveOp,), {})
        primClasses[name2] = primClass
        prim = primClass()
        primObjects[name2] = prim
        installPrimitive(name, StaticValue(toCOValue(prim)))

    primType("Bool", boolType)
    primType("Int8", int8Type)
    primType("Int16", int16Type)
    primType("Int32", int32Type)
    primType("Int64", int64Type)
    primType("UInt8", uint8Type)
    primType("UInt16", uint16Type)
    primType("UInt32", uint32Type)
    primType("UInt64", uint64Type)
    primType("Float32", float32Type)
    primType("Float64", float64Type)
    primType("Void", voidType)
    primType("CompilerObject", compilerObjectType)

    primOp("Type?")
    primOp("TypeSize")

    primOp("primitiveCopy")

    primOp("BoolType?")
    primOp("boolNot")
    primOp("boolTruth")

    primOp("IntegerType?")
    primOp("SignedIntegerType?")

    primOp("FloatType?")

    primOp("numericEquals?")
    primOp("numericLesser?")
    primOp("numericAdd")
    primOp("numericSubtract")
    primOp("numericMultiply")
    primOp("numericDivide")
    primOp("numericRemainder")

    primOp("numericNegate")
    primOp("numericConvert")

    primOp("shiftLeft")
    primOp("shiftRight")
    primOp("bitwiseAnd")
    primOp("bitwiseOr")
    primOp("bitwiseXor")

    primOp("VoidType?")
    primOp("CompilerObjectType?")

    primOp("PointerType?")
    primOp("PointerType")
    primOp("Pointer")
    primOp("PointeeType")

    primOp("addressOf")
    primOp("pointerDereference")
    primOp("pointerToInt")
    primOp("intToPointer")
    primOp("pointerCast")
    primOp("allocateMemory")
    primOp("freeMemory")

    primOp("ArrayType?")
    primOp("ArrayType")
    primOp("Array")
    primOp("ArrayElementType")
    primOp("ArraySize")
    primOp("array")
    primOp("arrayRef")

    primOp("TupleType?")
    primOp("TupleType")
    primOp("Tuple")
    primOp("TupleElementType")
    primOp("TupleFieldCount")
    primOp("TupleFieldOffset")
    primOp("tuple")
    primOp("tupleFieldRef")

    primOp("RecordType?")
    primOp("RecordType")
    primOp("RecordElementType")
    primOp("RecordFieldCount")
    primOp("RecordFieldOffset")
    primOp("RecordFieldIndex")
    primOp("recordFieldRef")
    primOp("recordFieldRefByName")

    global PrimClasses, PrimObjects
    PrimClasses = type("PrimClasses", (object,), primClasses)
    PrimObjects = type("PrimObjects", (object,), primObjects)

initializePrimitives()
