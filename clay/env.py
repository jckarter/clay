from clay.error import *
from clay.ast import *



#
# Environment
#

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
    withContext(ident, lambda : env.add(ident.s, value))

def lookupIdent(env, ident, verifier=None) :
    if verifier is None :
        verifier = lambda x : x
    return withContext(ident, lambda : verifier(env.lookup(ident.s)))



#
# install primitives
#

primitivesEnv = Environment()
primitives = None

def installPrimitive(name, value) :
    primitivesEnv.add(name, value)

def installDefaultPrimitives() :
    primClasses = {}
    def entry(name, value) :
        installPrimitive(name, value)
    def primitive(name) :
        primClass = type("Primitive%s" % name, (object,), {})
        primClasses[name] = primClass
        entry(name, primClass())
    def overloadable(name) :
        x = Overloadable(Identifier(name))
        x.env = primitivesEnv
        entry(name, x)

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

installDefaultPrimitives()



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
        return x
    entry = lookupIdent(env, item.name, verifyOverloadable)
    entry.overloads.insert(0, item)



#
# extendEnv
#

def extendEnv(parentEnv, variables, objects) :
    env = Environment(parentEnv)
    for variable, object_ in zip(variables, objects) :
        addIdent(env, variable, object_)
    return env
