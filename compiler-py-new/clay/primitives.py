
#
# install primitives
#

primitives = None
primitivesClassList = []

def installDefaultPrimitives() :
    primClasses = {}
    def entry(name, value) :
        installPrimitive(name, value)
    def safeName(s) :
        if s.endswith("?") :
            return s[:-1] + "_P"
        return s
    def primitive(name) :
        primClass = type("Primitive%s" % name, (object,), {})
        primitivesClassList.append(primClass)
        primClasses[safeName(name)] = primClass
        prim = primClass()
        prim.name = name
        entry(name, prim)

    primitive("Tuple")
    primitive("Array")
    primitive("Pointer")

    primitive("typeSize")

    primitive("primitiveCopy")

    primitive("compilerObjectInit")
    primitive("compilerObjectDestroy")
    primitive("compilerObjectCopy")
    primitive("compilerObjectAssign")
    primitive("compilerObjectEquals?")
    primitive("compilerObjectHash")

    primitive("addressOf")
    primitive("pointerDereference")
    primitive("pointerToInt")
    primitive("intToPointer")
    primitive("pointerCast")
    primitive("allocateMemory")
    primitive("freeMemory")

    primitive("TupleType?")
    primitive("tuple")
    primitive("tupleFieldCount")
    primitive("tupleFieldRef")

    primitive("array")
    primitive("arrayRef")

    primitive("RecordType?")
    primitive("recordFieldCount")
    primitive("recordFieldRef")

    primitive("numericEquals?")
    primitive("numericLesser?")
    primitive("numericAdd")
    primitive("numericSubtract")
    primitive("numericMultiply")
    primitive("numericDivide")
    primitive("numericRemainder")
    primitive("numericNegate")
    primitive("numericConvert")

    primitive("shiftLeft")
    primitive("shiftRight")
    primitive("bitwiseAnd")
    primitive("bitwiseOr")
    primitive("bitwiseXor")

    Primitives = type("Primitives", (object,), primClasses)
    global primitives
    primitives = Primitives()

installDefaultPrimitives()
