import ctypes
from clay.xprint import *
from clay.error import *
from clay.multimethod import *
from clay.coreops import *
from clay.env import *
from clay.types import *
from clay.unify import *
from clay.evaluator import *
import clay.llvmwrapper as llvm



#
# convert to llvm types
#

_llvmTypesTable = None

def _initLLVMTypesTable() :
    global _llvmTypesTable
    _llvmTypesTable = {}
    def llvmInt(ct) :
        return llvm.Type.int(ctypes.sizeof(ct) * 8)
    _llvmTypesTable[boolType] = llvmInt(ctypes.c_bool)
    _llvmTypesTable[intType] = llvmInt(ctypes.c_int)
    _llvmTypesTable[floatType] = llvm.Type.float()
    _llvmTypesTable[doubleType] = llvm.Type.double()
    _llvmTypesTable[charType] = llvmInt(ctypes.c_wchar)

def _cleanupLLVMTypesTable() :
    global _llvmTypesTable
    _llvmTypesTable = None

installGlobalsCleanupHook(_cleanupLLVMTypesTable)

llvmType = multimethod(errorMessage="invalid type")

@llvmType.register(Type)
def foo(t) :
    if _llvmTypesTable is None :
        _initLLVMTypesTable()
    llt = _llvmTypesTable.get(t)
    if llt is None :
        llt = makeLLVMType(t.tag, t)
    return llt

makeLLVMType = multimethod(errorMessage="invalid type tag")

@makeLLVMType.register(TupleTypeTag)
def foo(tag, t) :
    fieldLLVMTypes = [llvmType(x) for x in t.params]
    llt = llvm.Type.struct(fieldLLVMTypes)
    _llvmTypesTable[t] = llt
    return llt

@makeLLVMType.register(ArrayTypeTag)
def foo(tag, t) :
    ensure(len(t.params) == 2, "invalid array type")
    elementType = llvmType(t.params[0])
    count = toInt(t.params[1])
    llt = llvm.Type.array(elementType, count)
    _llvmTypesTable[t] = llt
    return llt

@makeLLVMType.register(PointerTypeTag)
def foo(tag, t) :
    ensure(len(t.params) == 1, "invalid pointer type")
    pointeeType = llvmType(t.params[0])
    llt = llvm.Type.pointer(pointeeType)
    _llvmTypesTable[t] = llt
    return llt

@makeLLVMType.register(Record)
def foo(tag, t) :
    llt = llvm.Type.opaque()
    typeHandle = llvm.TypeHandle.new(llt)
    _llvmTypesTable[t] = llt
    fieldLLVMTypes = [llvmType(x) for x in recordFieldTypes(t)]
    recType = llvm.Type.struct(fieldLLVMTypes)
    typeHandle.type.refine(recType)
    llt = typeHandle.type
    _llvmTypesTable[t] = llt
    return llt



#
# global llvm data
#

llvmModule = llvm.Module.new("foo")
llvmInitBuilder = None
llvmBuilder = None



#
# RTValue, RTReference
#

class RTValue(object) :
    def __init__(self, type_, llvmValue) :
        self.type = type_
        self.llvmValue = llvmValue

class RTReference(object) :
    def __init__(self, type_, llvmValue) :
        self.type = type_
        self.llvmValue = llvmValue

def isRTValue(x) : return type(x) is RTValue
def isRTReference(x) : return type(x) is RTReference



#
# temp values
#

_tempRTValues = []

def tempRTValue(type_) :
    llt = llvmType(type_)
    ptr = llvmInitBuilder.alloca(llt)
    value = RTValue(type_, ptr)
    tempRTValues.append(value)
    return value



#
# toRTValue, toRTLValue, toRTReference
#

toRTValue = multimethod(errorMessage="invalid value")

@toRTValue.register(RTValue)
def foo(x) :
    return x

@toRTValue.register(RTReference)
def foo(x) :
    temp = tempRTValue(x.type)
    rtValueCopy(temp, x)
    return temp

@toRTValue.register(Value)
def foo(x) :
    if isBoolType(x.type) :
        constValue = Constant.int(llvmType(x.type), x.buf.value)
    elif isIntType(x.type) :
        constValue = Constant.int(llvmType(x.type), x.buf.value)
    elif isCharType(x.type) :
        constValue = Constant.int(llvmType(x.type), ord(x.buf.value))
    else :
        error("unsupported type in toRTValue")
    temp = tempRTValue(x.type)
    llvmBuilder.store(constValue, temp)
    return temp


toRTLValue = multimethod(errorMessage="invalid reference")

@toRTLValue.register(RTReference)
def foo(x) :
    return x


toRTReference = multimethod(errorMessage="invalid reference")

@toRTReference.register(RTReference)
def foo(x) :
    return x

@toRTReference.register(RTValue)
def foo(x) :
    return RTReference(x.type, x.llvmValue)

@toRTReference.register(Value)
def foo(x) :
    return toRTReference(toRTValue(x))



#
# type checking converters
#

def toRTReferenceWithTypeTag(tag) :
    def f(x) :
        r = toRTReference(x)
        ensure(r.type.tag is tag, "type mismatch")
        return r
    return f

def toRTRecordReference(x) :
    r = toRTReference(x)
    ensure(isRecordType(r.type), "record type expected")
    return r



#
# type pattern matching converters
#

def toRTValueOfType(pattern) :
    def f(x) :
        v = toRTValue(x)
        matchType(pattern, v.type)
        return v
    return f

def toRTLValueOfType(pattern) :
    def f(x) :
        v = toRTLValue(x)
        matchType(pattern, v.type)
        return v
    return f

def toRTReferenceOfType(pattern) :
    def f(x) :
        r = toRTReference(x)
        matchType(pattern, r.type)
        return r
    return f



#
# printer
#

xregister(RTValue, lambda x : XObject("RTValue", x.type))
xregister(RTReference, lambda x : XObject("RTReference", x.type))



#
# core value operations
#

def _compileBuiltin(builtinName, args) :
    env = Environment(primitivesEnv)
    variables = [Identifier("v%d" % i) for i in range(len(args))]
    for variable, arg in zip(variables, args) :
        addIdent(env, variable, arg)
    argNames = map(NameRef, variables)
    builtin = NameRef(Identifier(builtinName))
    call = Call(builtin, argNames)
    return compile(call, env, converter)

def rtValueInit(a) :
    if isSimpleType(a.type) :
        return
    _compileBuiltin("init", [a])

def rtValueCopy(dest, src) :
    assert equals(dest.type, src.type)
    if isSimpleType(dest.type) :
        temp = llvmBuilder.load(src.llvmValue)
        llvmBuilder.store(temp, dest.llvmValue)
        return
    _compileBuiltin("copy", [dest, src])

def rtValueAssign(dest, src) :
    assert equals(dest.type, src.type)
    if isSimpleType(dest.type) :
        temp = llvmBuilder.load(src.llvmValue)
        llvmBuilder.store(temp, dest.llvmValue)
        return
    _compileBuiltin("assign", [dest, src])

def rtValueDestroy(a) :
    if isSimpleType(a.type) :
        return
    _compileBuiltin("destroy", [a])



#
# compile
#

def compile(expr, env, converter=(lambda x : x)) :
    try :
        contextPush(expr)
        return converter(compile2(expr, env))
    finally :
        contextPop()



#
# compile2
#

compile2 = multimethod(errorMessage="invalid expression")

@compile2.register(BoolLiteral)
def foo(x, env) :
    return boolToValue(x.value)

@compile2.register(IntLiteral)
def foo(x, env) :
    return intToValue(x.value)

@compile2.register(CharLiteral)
def foo(x, env) :
    return charToValue(x.value)

@compile2.register(NameRef)
def foo(x, env) :
    return compileNameRef(lookupIdent(env, x.name))

@compile2.register(Tuple)
def foo(x, env) :
    raise NotImplementedError

@compile2.register(Indexing)
def foo(x, env) :
    raise NotImplementedError

@compile2.register(Call)
def foo(x, env) :
    raise NotImplementedError

@compile2.register(FieldRef)
def foo(x, env) :
    raise NotImplementedError

@compile2.register(TupleRef)
def foo(x, env) :
    raise NotImplementedError

@compile2.register(Dereference)
def foo(x, env) :
    raise NotImplementedError

@compile2.register(AddressOf)
def foo(x, env) :
    raise NotImplementedError

@compile2.register(StaticExpr)
def foo(x, env) :
    raise NotImplementedError



#
# compileNameRef
#

compileNameRef = multimethod(defaultProc=(lambda x : x))

@compileNameRef.register(Reference)
def foo(x) :
    return toValue(x)

@compileNameRef.register(RTValue)
def foo(x) :
    return toRTReference(x)



#
# remove temp name used for multimethod instances
#

del foo
