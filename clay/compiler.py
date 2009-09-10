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
# utility operations
#

def rtTupleFieldRef(a, i) :
    assert isRTReference(a) and isTupleType(a.type)
    ensure((0 <= i < len(a.type.params)), "tuple index out of range")
    elementType = toType(a.type.params[i])
    elementPtr = llvmBuilder.gep(a.llvmValue, [0, i])
    return RTReference(elementType, elementPtr)

def rtMakeTuple(argRefs) :
    t = tupleType([x.type for x in argRefs])
    value = tempRTValue(t)
    valueRef = toRTReference(value)
    for i, argRef in enumerate(argRefs) :
        left = rtTupleFieldRef(valueRef, i)
        rtValueCopy(left, argRef)
    return value

def rtRecordFieldRef(a, i) :
    assert isRTReference(a) and isRecordType(a.type)
    nFields = len(a.type.tag.fields)
    ensure((0 <= i < nFields), "record field index out of range")
    fieldType = recordFieldTypes(a.type)[i]
    fieldPtr = llvmBuilder.gep(a.llvmValue, [0, i])
    return RTReference(fieldType, fieldPtr)

def rtMakeRecord(recType, argRefs) :
    value = tempRTValue(recType)
    valueRef = toRTReference(value)
    for i, argRef in enumerate(argRefs) :
        left = rtRecordFieldRef(valueRef, i)
        rtValueCopy(left, argRef)
    return value



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
    assert len(x.args) > 0
    cargs = [compile(y, env) for y in x.args]
    if len(x.args) == 1 :
        return cargs[1]
    if isType(cargs[0]) or isCell(cargs[0]) :
        elementTypes = convertObjects(toTypeOrCell, cargs, x.args)
        return tupleType(elementTypes)
    else :
        argRefs = convertObjects(toRTReference, cargs, x.args)
        return rtMakeTuple(argRefs)

@compile2.register(Indexing)
def foo(x, env) :
    thing = compile(x.expr, env)
    return compileIndexing(thing, x.args, env)

@compile2.register(Call)
def foo(x, env) :
    thing = compile(x.expr, env)
    return compileCall(thing, x.args, env)

@compile2.register(FieldRef)
def foo(x, env) :
    thing = compile(x.expr, env)
    thingRef = convertObject(toRTRecordReference, thing, x.expr)
    return rtRecordFieldRef(thingRef, recordFieldIndex(thing.type, x.name))

@compile2.register(TupleRef)
def foo(x, env) :
    thing = compile(x.expr, env)
    toRTTupleReference = toRTReferenceWithTypeTag(tupleTypeTag)
    thingRef = convertObject(toRTTupleReference, thing, x.expr)
    return rtTupleFieldRef(thingRef, x.index)

@compile2.register(Dereference)
def foo(x, env) :
    return compileCall(primitives.pointerDereference, [x.expr], env)

@compile2.register(AddressOf)
def foo(x, env) :
    return compileCall(primitives.addressOf, [x.expr], env)

@compile2.register(StaticExpr)
def foo(x, env) :
    return evaluate(x.expr, env, toStatic)



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
# compileIndexing
#

compileIndexing = multimethod(errorMessage="invalid indexing")

@compileIndexing.register(Record)
def foo(x, args, env) :
    ensureArity(args, len(x.typeVars))
    typeParams = [evaluate(y, env, toStaticOrCell) for y in args]
    return recordType(x, typeParams)

@compileIndexing.register(primitives.Tuple)
def foo(x, args, env) :
    ensure(len(args) > 1, "tuple types need atleast two member types")
    elementTypes = [evaluate(y, env, toStaticOrCell) for y in args]
    return tupleType(elementTypes)

@compileIndexing.register(primitives.Array)
def foo(x, args, env) :
    ensureArity(args, 2)
    elementType = evaluate(args[0], env, toTypeOrCell)
    sizeValue = evaluate(args[1], env, toValueOrCell)
    return arrayType(elementType, sizeValue)

@compileIndexing.register(primitives.Pointer)
def foo(x, args, env) :
    ensureArity(args, 1)
    pointeeType = evaluate(args[0], env, toTypeOrCell)
    return pointerType(pointeeType)



#
# compileCall
#

compileCall = multimethod(errorMessage="invalid call")

@compileCall.register(Record)
def foo(x, args, env) :
    ensureArity(args, len(x.fields))
    recordEnv, cells = bindTypeVars(x.env, x.typeVars)
    fieldTypePatterns = computeFieldTypes(x.fields, recordEnv)
    cargs = [compile(y, env) for y in args]
    argRefs = convertObjects(toRTReference, cargs, args)
    for typePattern, argRef, arg in zip(fieldTypePatterns, argRefs, args) :
        withContext(arg, lambda : matchType(typePattern, argRef.type))
    typeParams = resolveTypeVars(x.typeVars, cells)
    recType = recordType(x, typeParams)
    return rtMakeRecord(recType, argRefs)

@compileCall.register(Procedure)
def foo(x, args, env) :
    actualArgs = [RTActualArgument(y, env) for y in args]
    result = matchCodeSignature(x.env, x.code, actualArgs)
    if type(result) is MatchFailure :
        result.signalError()
    bindingNames, bindingValues = result
    return compileCodeBody(x.code, x.env, bindingNames, bindingValues)

@compileCall.register(Overloadable)
def foo(x, args, env) :
    actualArgs = [RTActualArgument(y, env) for y in args]
    for y in x.overloads :
        result = matchCodeSignature(y.env, y.code, actualArgs)
        if type(result) is MatchFailure :
            continue
        bindingNames, bindingValues = result
        return compileCodeBody(y.code, y.env, bindingNames, bindingValues)
    error("no matching overload")



#
# compile primitives
#

@compileCall.register(primitives.default)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.typeSize)
def foo(x, args, env) :
    raise NotImplementedError



#
# compile pointer primitives
#

@compileCall.register(primitives.addressOf)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.pointerDereference)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.pointerOffset)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.pointerSubtract)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.pointerCast)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.pointerCopy)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.pointerEquals)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.pointerLesser)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.allocateMemory)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.freeMemory)
def foo(x, args, env) :
    raise NotImplementedError



#
# compile tuple primitives
#

@compileCall.register(primitives.TupleType)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.tuple)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.tupleFieldCount)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.tupleFieldRef)
def foo(x, args, env) :
    raise NotImplementedError



#
# compile array primitives
#

@compileCall.register(primitives.array)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.arrayRef)
def foo(x, args, env) :
    raise NotImplementedError



#
# compile record primitives
#

@compileCall.register(primitives.RecordType)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.recordFieldCount)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.recordFieldRef)
def foo(x, args, env) :
    raise NotImplementedError



#
# compile bool primitives
#

@compileCall.register(primitives.boolCopy)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.boolNot)
def foo(x, args, env) :
    raise NotImplementedError



#
# compile char primitives
#

@compileCall.register(primitives.charCopy)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.charEquals)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.charLesser)
def foo(x, args, env) :
    raise NotImplementedError



#
# compile int primitives
#

@compileCall.register(primitives.intCopy)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.intEquals)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.intLesser)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.intAdd)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.intSubtract)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.intMultiply)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.intDivide)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.intModulus)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.intNegate)
def foo(x, args, env) :
    raise NotImplementedError



#
# compile float primitives
#

@compileCall.register(primitives.floatCopy)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.floatEquals)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.floatLesser)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.floatAdd)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.floatSubtract)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.floatMultiply)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.floatDivide)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.floatNegate)
def foo(x, args, env) :
    raise NotImplementedError



#
# compile double primitives
#

@compileCall.register(primitives.doubleCopy)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.doubleEquals)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.doubleLesser)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.doubleAdd)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.doubleSubtract)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.doubleMultiply)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.doubleDivide)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.doubleNegate)
def foo(x, args, env) :
    raise NotImplementedError



#
# compile conversion primitives
#

@compileCall.register(primitives.charToInt)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.intToChar)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.floatToInt)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.intToFloat)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.floatToDouble)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.doubleToFloat)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.doubleToInt)
def foo(x, args, env) :
    raise NotImplementedError

@compileCall.register(primitives.intToDouble)
def foo(x, args, env) :
    raise NotImplementedError



#
# compile actual arguments
#

class RTActualArgument(object) :
    def __init__(self, expr, env) :
        self.expr = expr
        self.env = env
        self.result_ = compile(self.expr, self.env)

    def asReference(self) :
        return withContext(self.expr, lambda : toRTReference(self.result_))

    def asValue(self) :
        return withContext(self.expr, lambda : toRTValue(self.result_))

    def asStatic(self) :
        return withContext(self.expr, lambda : toStatic(self.result_))



#
# compileCodeBody
#

def compileCodeBody(code, env, bindingNames, bindingValues) :
    raise NotImplementedError



#
# remove temp name used for multimethod instances
#

del foo
