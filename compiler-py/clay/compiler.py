import ctypes
from clay.xprint import *
from clay.cleanup import *
from clay.error import *
from clay.multimethod import *
from clay.env import *
from clay.evaluator import *
from clay import analyzer
import clay.llvmwrapper as llvm



#
# global llvm data
#

llvmFunction = None
llvmInitBuilder = None
llvmBuilder = None

def _cleanCompilerLLVMGlobals() :
    global llvmFunction, llvmInitBuilder, llvmBuilder
    llvmFunction = None
    llvmInitBuilder = None
    llvmBuilder = None

installGlobalsCleanupHook(_cleanCompilerLLVMGlobals)



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
# return locations
#

class RTReturnLocation(object) :
    def __init__(self, type_, llvmValue) :
        self.type = type_
        self.llvmValue = llvmValue
        self.used = False
    def use(self, type_) :
        ensure(type_ == self.type, "type mismatch")
        ensure(not self.used, "double use")
        self.used = True
        return RTValue(self.type, self.llvmValue)

_rtReturnLocations = []

def pushRTReturnLocation(returnLoc) :
    _rtReturnLocations.append(returnLoc)

def topRTReturnLocation() :
    return _rtReturnLocations[-1]

def popRTReturnLocation() :
    return _rtReturnLocations.pop()



#
# temp values
#

class RTTempsBlock(object) :
    def __init__(self) :
        self.temps = []

    def add(self, temp) :
        self.temps.append(temp)

    def newTemp(self, type_) :
        llt = llvmType(type_)
        ptr = llvmInitBuilder.alloca(llt)
        value = RTValue(type_, ptr)
        self.add(value)
        return value

    def cleanup(self) :
        for temp in reversed(self.temps) :
            rtValueDestroy(temp)

_rtTempsBlocks = []

def pushRTTempsBlock() :
    block = RTTempsBlock()
    _rtTempsBlocks.append(block)

def popRTTempsBlock() :
    block = _rtTempsBlocks.pop()
    block.cleanup()

def tempRTValue(type_) :
    returnLoc = topRTReturnLocation()
    if returnLoc is not None :
        return returnLoc.use(type_)
    return _rtTempsBlocks[-1].newTemp(type_)



#
# toRTValue, toRTReference, toRTLValue, toRTValueOrReference
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
        constValue = llvm.Constant.int(llvmType(x.type), int(x.buf.value))
    elif isIntegerType(x.type) :
        constValue = llvm.Constant.int(llvmType(x.type), x.buf.value)
    elif isFloatingPointType(x.type) :
        constValue = llvm.Constant.real(llvmType(x.type), x.buf.value)
    else :
        error("unsupported type in toRTValue")
    temp = tempRTValue(x.type)
    llvmBuilder.store(constValue, temp.llvmValue)
    return temp


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


toRTLValue = multimethod(errorMessage="invalid reference")

@toRTLValue.register(RTReference)
def foo(x) :
    return x


toRTValueOrReference = multimethod(defaultProc=toRTValue)

toRTValueOrReference.register(RTValue)(lambda x : x)
toRTValueOrReference.register(RTReference)(lambda x : x)



#
# type checking converters
#

def toRTReferenceWithTag(tag) :
    def f(x) :
        r = toRTReference(x)
        ensure(r.type.tag is tag, "type mismatch")
        return r
    return f

def toRTIntegerReference(x) :
    r = toRTReference(x)
    ensure(isIntegerType(r.type), "integer type expected")
    return r

def toRTFloatingPointReference(x) :
    r = toRTReference(x)
    ensure(isFloatingPointType(r.type), "floating point type expected")
    return r

def toRTNumericReference(x) :
    r = toRTReference(x)
    ensure(isIntegerType(r.type) or isFloatingPointType(r.type),
           "numeric type expected")
    return r

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

def toRTReferenceOfType(pattern) :
    def f(x) :
        r = toRTReference(x)
        matchType(pattern, r.type)
        return r
    return f

def toRTLValueOfType(pattern) :
    def f(x) :
        v = toRTLValue(x)
        matchType(pattern, v.type)
        return v
    return f

def toRTValueOrReferenceOfType(pattern) :
    def f(x) :
        v = toRTValueOrReference(x)
        matchType(pattern, v.type)
        return v
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
    env = Environment(loadedModule("_core").env)
    variables = [Identifier("v%d" % i) for i in range(len(args))]
    for variable, arg in zip(variables, args) :
        addIdent(env, variable, arg)
    argNames = map(NameRef, variables)
    builtin = NameRef(Identifier(builtinName))
    call = Call(builtin, argNames)
    return compile(call, env)

def rtValueInit(a) :
    if isSimpleType(a.type) :
        return
    _compileBuiltin("init", [a])

def rtValueCopy(dest, src) :
    assert dest.type == src.type
    if isSimpleType(dest.type) :
        temp = llvmBuilder.load(src.llvmValue)
        llvmBuilder.store(temp, dest.llvmValue)
        return
    _compileBuiltin("copy", [dest, src])

def rtValueAssign(dest, src) :
    if isSimpleType(dest.type) and (dest.type == src.type) :
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

def rtMakeArray(arrayTy, argExprs, env) :
    elementType = toType(arrayTy.params[0])
    count = toNativeInt(arrayTy.params[1])
    ensureArity(argExprs, count)
    value = tempRTValue(arrayTy)
    valueRef = toRTReference(value)
    zero = llvm.Constant.int(llvmType(nativeIntType), 0)
    for i, argExpr in enumerate(argExprs) :
        iVal = llvm.Constant.int(llvmType(nativeIntType), i)
        elementAddr = llvmBuilder.gep(valueRef.llvmValue, [zero, iVal])
        returnLoc = RTReturnLocation(elementType, elementAddr)
        compile(argExpr, env, toRTValueOfType(elementType),
                returnLocation=returnLoc)
    return value

def rtTupleFieldRef(a, i) :
    assert isRTReference(a) and isTupleType(a.type)
    ensure((0 <= i < len(a.type.params)), "tuple index out of range")
    elementType = toType(a.type.params[i])
    zero = llvm.Constant.int(llvmType(nativeIntType), 0)
    iValue = llvm.Constant.int(llvmType(nativeIntType), i)
    elementPtr = llvmBuilder.gep(a.llvmValue, [zero, iValue])
    return RTReference(elementType, elementPtr)

def rtMakeTuple(argTypes, argExprs, env) :
    t = tupleType(tuple(argTypes))
    value = tempRTValue(t)
    valueRef = toRTReference(value)
    ensureArity(argExprs, len(argTypes))
    for i, argExpr in enumerate(argExprs) :
        left = rtTupleFieldRef(valueRef, i)
        returnLoc = RTReturnLocation(left.type, left.llvmValue)
        compile(argExpr, env, toRTValueOfType(argTypes[i]),
                returnLocation=returnLoc)
    return value

def rtRecordFieldRef(a, i) :
    assert isRTReference(a) and isRecordType(a.type)
    nFields = len(recordValueArgs(a.type.tag))
    ensure((0 <= i < nFields), "record field index out of range")
    fieldType = recordFieldTypes(a.type)[i]
    zero = llvm.Constant.int(llvmType(nativeIntType), 0)
    iValue = llvm.Constant.int(llvmType(nativeIntType), i)
    fieldPtr = llvmBuilder.gep(a.llvmValue, [zero, iValue])
    return RTReference(fieldType, fieldPtr)

def rtMakeRecord(recType, argExprs, env) :
    value = tempRTValue(recType)
    valueRef = toRTReference(value)
    fieldTypes = recordFieldTypes(recType)
    ensureArity(argExprs, len(fieldTypes))
    for i, argExpr in enumerate(argExprs) :
        left = rtRecordFieldRef(valueRef, i)
        returnLoc = RTReturnLocation(left.type, left.llvmValue)
        compile(argExpr, env, toRTValueOfType(fieldTypes[i]),
                returnLocation=returnLoc)
    return value



#
# compileRootExpr
#

def compileRootExpr(expr, env, converter=(lambda x : x), returnLocation=None) :
    pushRTTempsBlock()
    pushTempsBlock()
    try :
        return compile(expr, env, converter, returnLocation)
    finally :
        popTempsBlock()
        popRTTempsBlock()



#
# compile
#

def compile(expr, env, converter=(lambda x : x), returnLocation=None) :
    contextPush(expr)
    pushRTReturnLocation(returnLocation)
    try :
        result = compile2(expr, env)
        result = converter(result)
    finally :
        popRTReturnLocation()
        contextPop()
    if returnLocation is not None :
        assert returnLocation.used
    return result

def compileDelegate(expr, env) :
    return compile2(expr, env)



#
# compile2
#

compile2 = multimethod(errorMessage="invalid expression")

@compile2.register(BoolLiteral)
def foo(x, env) :
    return evaluateRootExpr(x, env)

@compile2.register(IntLiteral)
def foo(x, env) :
    return evaluateRootExpr(x, env)

@compile2.register(FloatLiteral)
def foo(x, env) :
    return evaluateRootExpr(x, env)

@compile2.register(CharLiteral)
def foo(x, env) :
    return compileDelegate(convertCharLiteral(x), env)

@compile2.register(StringLiteral)
def foo(x, env) :
    return compileDelegate(convertStringLiteral(x), env)

@compile2.register(NameRef)
def foo(x, env) :
    return compileNameRef(lookupIdent(env, x.name))

@compile2.register(Tuple)
def foo(x, env) :
    if len(x.args) == 1:
        return compileDelegate(x.args[0], env)
    return compileDelegate(Call(primitiveNameRef("tuple"), x.args), env)

@compile2.register(Array)
def foo(x, env) :
    return compileDelegate(Call(primitiveNameRef("array"), x.args), env)

@compile2.register(Indexing)
def foo(x, env) :
    thing = compile(x.expr, env)
    thing = lower(thing)
    return compileIndexing(thing, x.args, env)

@compile2.register(Call)
def foo(x, env) :
    thing = compile(x.expr, env)
    thing = lower(thing)
    return compileCall(thing, x.args, env)

@compile2.register(FieldRef)
def foo(x, env) :
    thingRef = compile(x.expr, env, toRTRecordReference)
    return rtRecordFieldRef(thingRef, recordFieldIndex(thingRef.type, x.name))

@compile2.register(TupleRef)
def foo(x, env) :
    toRTTupleReference = toRTReferenceWithTag(tupleTag)
    thingRef = compile(x.expr, env, toRTTupleReference)
    return rtTupleFieldRef(thingRef, x.index)

@compile2.register(Dereference)
def foo(x, env) :
    primOp = primitiveNameRef("pointerDereference")
    return compileDelegate(Call(primOp, [x.expr]), env)

@compile2.register(AddressOf)
def foo(x, env) :
    return compileDelegate(Call(primitiveNameRef("addressOf"), [x.expr]), env)

@compile2.register(UnaryOpExpr)
def foo(x, env) :
    return compileDelegate(convertUnaryOpExpr(x), env)

@compile2.register(BinaryOpExpr)
def foo(x, env) :
    return compileDelegate(convertBinaryOpExpr(x), env)

@compile2.register(NotExpr)
def foo(x, env) :
    v, = rtLoadLLVM([x.expr], env, [boolType])
    zero = llvm.Constant.int(llvmType(boolType), 0)
    flag = llvmBuilder.icmp(llvm.ICMP_EQ, v, zero)
    flag = llvmBuilder.zext(flag, llvmType(boolType))
    result = tempRTValue(boolType)
    llvmBuilder.store(flag, result.llvmValue)
    return result

@compile2.register(AndExpr)
def foo(x, env) :
    result = tempRTValue(boolType)
    trueBlock = llvmFunction.append_basic_block("andOne")
    falseBlock = llvmFunction.append_basic_block("andTwo")
    mergeBlock = llvmFunction.append_basic_block("andMerge")

    v1Ref = compile(x.expr1, env, toRTReferenceOfType(boolType))
    v1 = llvmBuilder.load(v1Ref.llvmValue)
    zero = llvm.Constant.int(llvmType(boolType), 0)
    b1 = llvmBuilder.icmp(llvm.ICMP_NE, v1, zero)
    llvmBuilder.cbranch(b1, trueBlock, falseBlock)

    llvmBuilder.position_at_end(trueBlock)
    v2Ref = compile(x.expr2, env, toRTReferenceOfType(boolType))
    v2 = llvmBuilder.load(v2Ref.llvmValue)
    llvmBuilder.store(v2, result.llvmValue)
    llvmBuilder.branch(mergeBlock)

    llvmBuilder.position_at_end(falseBlock)
    llvmBuilder.store(v1, result.llvmValue)
    llvmBuilder.branch(mergeBlock)

    llvmBuilder.position_at_end(mergeBlock)
    return result

@compile2.register(OrExpr)
def foo(x, env) :
    result = tempRTValue(boolType)
    trueBlock = llvmFunction.append_basic_block("orOne")
    falseBlock = llvmFunction.append_basic_block("orTwo")
    mergeBlock = llvmFunction.append_basic_block("orMerge")

    v1Ref = compile(x.expr1, env, toRTReferenceOfType(boolType))
    v1 = llvmBuilder.load(v1Ref.llvmValue)
    zero = llvm.Constant.int(llvmType(boolType), 0)
    b1 = llvmBuilder.icmp(llvm.ICMP_NE, v1, zero)
    llvmBuilder.cbranch(b1, trueBlock, falseBlock)

    llvmBuilder.position_at_end(trueBlock)
    llvmBuilder.store(v1, result.llvmValue)
    llvmBuilder.branch(mergeBlock)

    llvmBuilder.position_at_end(falseBlock)
    v2Ref = compile(x.expr2, env, toRTReferenceOfType(boolType))
    v2 = llvmBuilder.load(v2Ref.llvmValue)
    llvmBuilder.store(v2, result.llvmValue)
    llvmBuilder.branch(mergeBlock)

    llvmBuilder.position_at_end(mergeBlock)
    return result

@compile2.register(StaticExpr)
def foo(x, env) :
    return evaluateRootExpr(x.expr, env, toStatic)

@compile2.register(SCExpression)
def foo(x, env) :
    return compileDelegate(x.expr, x.env)



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

@compileNameRef.register(Record)
def foo(x) :
    if (len(x.typeVars) == 0) :
        return recordType(x, ())
    return x



#
# compileIndexing
#

compileIndexing = multimethod(errorMessage="invalid indexing")

@compileIndexing.register(Record)
def foo(x, args, env) :
    ensureArity(args, len(x.typeVars))
    typeParams = [compile(y, env, toStaticOrCell) for y in args]
    return recordType(x, tuple(typeParams))

@compileIndexing.register(primitives.Tuple)
def foo(x, args, env) :
    ensure(len(args) > 1, "tuple types need atleast two member types")
    elementTypes = [compile(y, env, toTypeOrCell) for y in args]
    return tupleType(tuple(elementTypes))

@compileIndexing.register(primitives.Array)
def foo(x, args, env) :
    ensureArity(args, 2)
    elementType = compile(args[0], env, toTypeOrCell)
    sizeValue = compile(args[1], env, toValueOrCell)
    return arrayType(elementType, sizeValue)

@compileIndexing.register(primitives.Pointer)
def foo(x, args, env) :
    ensureArity(args, 1)
    pointeeType = compile(args[0], env, toTypeOrCell)
    return pointerType(pointeeType)



#
# compileCall
#

compileCall = multimethod(errorMessage="invalid call")

@compileCall.register(Type)
def foo(x, args, env) :
    if len(args) == 0 :
        v = tempRTValue(x)
        rtValueInit(v)
        return v
    elif isRecordType(x) :
        return rtMakeRecord(x, args, env)
    elif isArrayType(x) :
        return rtMakeArray(x, args, env)
    elif isTupleType(x) :
        fieldTypes = [toType(y) for y in x.params]
        return rtMakeTuple(fieldTypes, args, env)
    else :
        error("only array, tuple, and record types " +
              "can be constructed this way.")

@compileCall.register(Record)
def foo(x, args, env) :
    if len(args) == 0 :
        v = tempRTValue(toType(x))
        rtValueInit(v)
        return v
    result = analyzer.analyzeCall(x, args, envForAnalysis(env))
    recType = result.type
    nonStaticArgs = []
    for formalArg, actualArg in zip(x.args, args) :
        if type(formalArg) is ValueRecordArg :
            nonStaticArgs.append(actualArg)
    return rtMakeRecord(recType, nonStaticArgs, env)

@compileCall.register(Procedure)
def foo(x, args, env) :
    actualArgs = [RTActualArgument(y, env) for y in args]
    result = matchInvoke(x.code, x.env, actualArgs)
    if type(result) is InvokeError :
        result.signalError()
    assert type(result) is InvokeBindings
    return compileInvoke(x.name.s, x.code, x.env, result)

@compileCall.register(Overloadable)
def foo(x, args, env) :
    env2 = envForAnalysis(env)
    actualArgs = [analyzer.RTActualArgument(arg, env2) for arg in args]
    for y in x.overloads :
        result = matchInvoke(y.code, envForAnalysis(y.env), actualArgs)
        if type(result) is InvokeError :
            continue
        assert type(result) is InvokeBindings
        actualArgs = [RTActualArgument(arg, env) for arg in args]
        result = matchInvoke(y.code, y.env, actualArgs)
        assert type(result) is InvokeBindings
        return compileInvoke(x.name.s, y.code, y.env, result)
    error("no matching overload")

@compileCall.register(ExternalProcedure)
def foo(x, args, env) :
    initExternalProcedure(x)
    ensureArity(args, len(x.args))
    argRefs = []
    for arg, externalArg in zip(args, x.args) :
        expectedType = compile(externalArg.type, x.env, toType)
        argRef = compile(arg, env, toRTReferenceOfType(expectedType))
        argRefs.append(argRef)
    returnType = compile(x.returnType, x.env, toType)
    return compileLLVMCall(x.llvmFunc, argRefs, returnType)



#
# utility methods for compiling primitives
#

def rtLoadRefs(args, env, types) :
    ensureArity(args, len(types))
    argRefs = []
    for arg, type_ in zip(args, types) :
        argRef = compile(arg, env, toRTReferenceOfType(type_))
        argRefs.append(argRef)
    return argRefs

def rtLoadLLVM(args, env, types) :
    argRefs = rtLoadRefs(args, env, types)
    return [llvmBuilder.load(x.llvmValue) for x in argRefs]



#
# compilePrimitiveCall, compileLLVMCall
#

def compilePrimitiveCall(primitive, inRefs, outputType) :
    func = primitiveFunc(primitive, [x.type for x in inRefs], outputType)
    return compileLLVMCall(func, inRefs, outputType)

def compileLLVMCall(func, inRefs, outputType) :
    llvmArgs = [inRef.llvmValue for inRef in inRefs]
    if not isVoidType(outputType) :
        result = tempRTValue(outputType)
        llvmArgs.append(result.llvmValue)
    else :
        result = voidValue
    llvmBuilder.call(func, llvmArgs)
    return result



#
# compile primitives
#

@compileCall.register(primitives.typeSize)
def foo(x, args, env) :
    ensureArity(args, 1)
    t = compile(args[0], env, toType)
    ptr = llvm.Constant.null(llvmType(pointerType(t)))
    one = llvm.Constant.int(llvmType(nativeIntType), 1)
    offsetPtr = llvmBuilder.gep(ptr, [one])
    offsetInt = llvmBuilder.ptrtoint(offsetPtr, llvmType(nativeIntType))
    v = tempRTValue(nativeIntType)
    llvmBuilder.store(offsetInt, v.llvmValue)
    return v

@compileCall.register(primitives.primitiveCopy)
def foo(x, args, env) :
    ensureArity(args, 2)
    cell = Cell()
    destRef = compile(args[0], env, toRTReferenceOfType(cell))
    srcRef = compile(args[1], env, toRTReferenceOfType(cell))
    ensure(isSimpleType(cell.param), "expected simple type")
    return compilePrimitiveCall(x, [destRef, srcRef], voidType)



#
# compile compiler object primitives
#

@compileCall.register(primitives.compilerObjectInit)
def foo(x, args, env) :
    error("compiler objects are not supported at runtime")

@compileCall.register(primitives.compilerObjectDestroy)
def foo(x, args, env) :
    error("compiler objects are not supported at runtime")

@compileCall.register(primitives.compilerObjectCopy)
def foo(x, args, env) :
    error("compiler objects are not supported at runtime")

@compileCall.register(primitives.compilerObjectAssign)
def foo(x, args, env) :
    error("compiler objects are not supported at runtime")

@compileCall.register(getattr(primitives, "compilerObjectEquals?"))
def foo(x, args, env) :
    error("compiler objects are not supported at runtime")

@compileCall.register(primitives.compilerObjectHash)
def foo(x, args, env) :
    error("compiler objects are not supported at runtime")



#
# compile pointer primitives
#

@compileCall.register(primitives.addressOf)
def foo(x, args, env) :
    ensureArity(args, 1)
    valueRef = compile(args[0], env, toRTLValue)
    ptrValue = tempRTValue(pointerType(valueRef.type))
    llvmBuilder.store(valueRef.llvmValue, ptrValue.llvmValue)
    return ptrValue

@compileCall.register(primitives.pointerDereference)
def foo(x, args, env) :
    cell = Cell()
    ptr, = rtLoadLLVM(args, env, [pointerTypePattern(cell)])
    return RTReference(cell.param, ptr)

@compileCall.register(primitives.pointerToInt)
def foo(x, args, env) :
    ensureArity(args, 2)
    cell = Cell()
    converter = toRTReferenceOfType(pointerTypePattern(cell))
    ptrRef = compile(args[0], env, converter)
    outType = compile(args[1], env, toIntegerType)
    return compilePrimitiveCall(x, [ptrRef], outType)

@compileCall.register(primitives.intToPointer)
def foo(x, args, env) :
    ensureArity(args, 2)
    intRef = compile(args[0], env, toRTIntegerReference)
    pointeeType = compile(args[1], env, toType)
    return compilePrimitiveCall(x, [intRef], pointerType(pointeeType))

@compileCall.register(primitives.pointerCast)
def foo(x, args, env) :
    ensureArity(args, 2)
    cell = Cell()
    targetType = compile(args[0], env, toType)
    targetPtrType = pointerType(targetType)
    converter = toRTReferenceOfType(pointerTypePattern(cell))
    ptrRef = compile(args[1], env, converter)
    ptr = llvmBuilder.load(ptrRef.llvmValue)
    ptr = llvmBuilder.bitcast(ptr, llvmType(targetPtrType))
    result = tempRTValue(targetPtrType)
    llvmBuilder.store(ptr, result.llvmValue)
    return result

@compileCall.register(primitives.allocateMemory)
def foo(x, args, env) :
    ensureArity(args, 2)
    type_ = compile(args[0], env, toType)
    nRef = compile(args[1], env, toRTReferenceOfType(nativeIntType))
    return compilePrimitiveCall(x, [nRef], pointerType(type_))

@compileCall.register(primitives.freeMemory)
def foo(x, args, env) :
    ensureArity(args, 1)
    cell = Cell()
    converter = toRTReferenceOfType(pointerTypePattern(cell))
    ptrRef = compile(args[0], env, converter)
    return compilePrimitiveCall(x, [ptrRef], voidType)



#
# compile tuple primitives
#

@compileCall.register(getattr(primitives, "TupleType?"))
def foo(x, args, env) :
    ensureArity(args, 1)
    t = compile(args[0], env, toType)
    return toRTValue(boolToValue(isTupleType(t)))

@compileCall.register(primitives.tuple)
def foo(x, args, env) :
    ensure(len(args) > 1, "tuples need atleast two members")
    argTypes = []
    env2 = envForAnalysis(env)
    for arg in args :
        result = analyzer.analyze(arg, env2, analyzer.toRTReference)
        argTypes.append(result.type)
    return rtMakeTuple(argTypes, args, env)

@compileCall.register(primitives.tupleFieldCount)
def foo(x, args, env) :
    ensureArity(args, 1)
    t = compile(args[0], env, toTypeWithTag(tupleTag))
    return toRTValue(intToValue(len(t.params)))

@compileCall.register(primitives.tupleFieldRef)
def foo(x, args, env) :
    ensureArity(args, 2)
    toRTTupleReference = toRTReferenceWithTag(tupleTag)
    tupleRef = compile(args[0], env, toRTTupleReference)
    i = compile(args[1], env, toNativeInt)
    return rtTupleFieldRef(tupleRef, i)



#
# compile array primitives
#

@compileCall.register(primitives.array)
def foo(x, args, env) :
    ensure(len(args) > 0, "empty array expressions are not allowed")
    env2 = envForAnalysis(env)
    firstArg = analyzer.analyze(args[0], env2, analyzer.toRTReference)
    arrayTy = arrayType(firstArg.type, intToValue(len(args)))
    return rtMakeArray(arrayTy, args, env)

@compileCall.register(primitives.arrayRef)
def foo(x, args, env) :
    ensureArity(args, 2)
    aRef = compile(args[0], env, toRTReferenceWithTag(arrayTag))
    iRef = compile(args[1], env, toRTReferenceOfType(nativeIntType))
    i = llvmBuilder.load(iRef.llvmValue)
    zero = llvm.Constant.int(llvmType(nativeIntType), 0)
    element = llvmBuilder.gep(aRef.llvmValue, [zero, i])
    return RTReference(aRef.type.params[0], element)



#
# compile record primitives
#

@compileCall.register(getattr(primitives, "RecordType?"))
def foo(x, args, env) :
    ensureArity(args, 1)
    t = compile(args[0], env, toType)
    return toRTValue(boolToValue(isRecordType(t)))

@compileCall.register(primitives.recordFieldCount)
def foo(x, args, env) :
    ensureArity(args, 1)
    t = compile(args[0], env, toRecordType)
    return toRTValue(intToValue(len(t.tag.fields)))

@compileCall.register(primitives.recordFieldRef)
def foo(x, args, env) :
    ensureArity(args, 2)
    recRef = compile(args[0], env, toRTRecordReference)
    i = compile(args[1], env, toNativeInt)
    return rtRecordFieldRef(recRef, i)



#
# compile numeric primitives
#

def _compile2NumericArgs(args, env) :
    ensureArity(args, 2)
    a, b = [compile(x, env, toRTNumericReference) for x in args]
    ensure(a.type == b.type, "argument types mismatch")
    return (a, b)
    
@compileCall.register(getattr(primitives, "numericEquals?"))
def foo(x, args, env) :
    x1, x2 = _compile2NumericArgs(args, env)
    return compilePrimitiveCall(x, [x1, x2], boolType)

@compileCall.register(getattr(primitives, "numericLesser?"))
def foo(x, args, env) :
    x1, x2 = _compile2NumericArgs(args, env)
    return compilePrimitiveCall(x, [x1, x2], boolType)

@compileCall.register(primitives.numericAdd)
def foo(x, args, env) :
    x1, x2 = _compile2NumericArgs(args, env)
    return compilePrimitiveCall(x, [x1, x2], x1.type)

@compileCall.register(primitives.numericSubtract)
def foo(x, args, env) :
    x1, x2 = _compile2NumericArgs(args, env)
    return compilePrimitiveCall(x, [x1, x2], x1.type)

@compileCall.register(primitives.numericMultiply)
def foo(x, args, env) :
    x1, x2 = _compile2NumericArgs(args, env)
    return compilePrimitiveCall(x, [x1, x2], x1.type)

@compileCall.register(primitives.numericDivide)
def foo(x, args, env) :
    x1, x2 = _compile2NumericArgs(args, env)
    return compilePrimitiveCall(x, [x1, x2], x1.type)

@compileCall.register(primitives.numericRemainder)
def foo(x, args, env) :
    x1, x2 = _compile2NumericArgs(args, env)
    return compilePrimitiveCall(x, [x1, x2], x1.type)

@compileCall.register(primitives.numericNegate)
def foo(x, args, env) :
    ensureArity(args, 1)
    x1 = compile(args[0], env, toRTNumericReference)
    return compilePrimitiveCall(x, [x1], x1.type)

@compileCall.register(primitives.numericConvert)
def foo(x, args, env) :
    ensureArity(args, 2)
    destType = compile(args[0], env, toNumericType)
    src = compile(args[1], env, toRTNumericReference)
    return compilePrimitiveCall(x, [src], destType)



#
# environment for analysis
#

def analysisFilter(x) :
    if isRTValue(x) :
        return analyzer.RTValue(x.type)
    elif isRTReference(x) :
        return analyzer.RTReference(x.type)
    return x

def envForAnalysis(env) :
    return Environment(env, filter=analysisFilter)



#
# compile actual arguments
#

class RTActualArgument(object) :
    def __init__(self, expr, env) :
        self.expr = expr
        self.env = env
        self.result_ = None

    def asReference(self) :
        assert self.result_ is None
        self.result_ = compile(self.expr, self.env, toRTReference)
        return self.result_

    def asStatic(self) :
        assert self.result_ is None
        self.result_ = compile(self.expr, self.env, toStatic)
        return self.result_



#
# compileInvoke
#

def compileInvoke(name, code, env, bindings) :
    compiledCode = compileCode(name, code, env, bindings)
    llvmArgs = []
    for p in bindings.params :
        llvmArgs.append(p.llvmValue)
    if isVoidType(compiledCode.returnType) :
        llvmBuilder.call(compiledCode.llvmFunc, llvmArgs)
        return voidValue
    elif compiledCode.returnByRef :
        ptr = llvmBuilder.call(compiledCode.llvmFunc, llvmArgs)
        return RTReference(compiledCode.returnType, ptr)
    else :
        retVal = tempRTValue(compiledCode.returnType)
        llvmArgs.append(retVal.llvmValue)
        llvmBuilder.call(compiledCode.llvmFunc, llvmArgs)
        return retVal



#
# compileCode
#

class CompiledCode(object) :
    def __init__(self, returnByRef, returnType, llvmFunc) :
        self.returnByRef = returnByRef
        self.returnType = returnType
        self.llvmFunc = llvmFunc

_compiledCodeTable = {}

def _cleanupCompiledCodeTable() :
    global _compiledCodeTable
    _compiledCodeTable = {}

installGlobalsCleanupHook(_cleanupCompiledCodeTable)

def compileCode(name, code, env, bindings, internal=True) :
    key = tuple([code] + bindings.typeParams +
                [p.type for p in bindings.params])
    compiledCode = _compiledCodeTable.get(key)
    if compiledCode is None :
        compiledCode = compileCodeHeader(name, code, env, bindings, internal)
        _compiledCodeTable[key] = compiledCode
        compileCodeBody(code, env, bindings, compiledCode)
    return compiledCode



#
# compileCodeHeader
#

def compileCodeHeader(name, code, env, bindings, internal) :
    llvmArgTypes = []
    for param in bindings.params :
        llt = llvm.Type.pointer(llvmType(param.type))
        llvmArgTypes.append(llt)
    compiledCode = analyzeCodeToCompile(code, env, bindings)
    if isVoidType(compiledCode.returnType) :
        llvmReturnType = llvm.Type.void()
    elif compiledCode.returnByRef :
        llvmReturnType = llvmType(pointerType(compiledCode.returnType))
    else :
        llvmReturnType = llvm.Type.void()
        llvmArgTypes.append(llvmType(pointerType(compiledCode.returnType)))
    funcType = llvm.Type.function(llvmReturnType, llvmArgTypes)
    func = llvmModule().add_function(funcType, "clay_" + name)
    if internal :
        func.linkage = llvm.LINKAGE_INTERNAL
    compiledCode.llvmFunc = func
    for i, var in enumerate(bindings.vars) :
        func.args[i].name = var.s
    return compiledCode

def analyzeCodeToCompile(code, env, bindings) :
    bindings2 = bindingsForAnalysis(bindings)
    result = analyzer.analyzeInvoke(code, env, bindings2)
    if analyzer.isRTValue(result) :
        returnByRef = False
        returnType = result.type
    elif analyzer.isRTReference(result) :
        returnByRef = True
        returnType = result.type
    elif isVoidValue(result) :
        returnByRef = False
        returnType = voidType
    else :
        assert False, result
    return CompiledCode(returnByRef, returnType, None)

def bindingsForAnalysis(bindings) :
    params = []
    for p in bindings.params :
        if isRTValue(p) :
            p = analyzer.RTValue(p.type)
        elif isRTReference(p) :
            p = analyzer.RTReference(p.type)
        else :
            assert False, p
        params.append(p)
    bindings2 = InvokeBindings(
        bindings.typeVars, bindings.typeParams,
        bindings.vars, params)
    return bindings2



#
# CompilerCodeContext
#

class ScopeMarker(object) :
    pass

def isScopeMarker(x) : return type(x) is ScopeMarker

class CompilerCodeContext(object) :
    def __init__(self, compiledCode) :
        self.returnByRef = compiledCode.returnByRef
        self.returnType = compiledCode.returnType
        self.llvmFunc = compiledCode.llvmFunc
        self.scopeStack = []
        self.labels = {}
        self.breakInfo = []
        self.continueInfo = []

    def pushNewValue(self, type_) :
        llt = llvmType(type_)
        ptr = llvmInitBuilder.alloca(llt)
        v = RTValue(type_, ptr)
        self.scopeStack.append(v)
        return v

    def pushMarker(self) :
        marker = ScopeMarker()
        self.scopeStack.append(marker)
        return marker

    def cleanAll(self) :
        for x in reversed(self.scopeStack) :
            if isRTValue(x) :
                rtValueDestroy(x)

    def cleanUpto(self, marker) :
        assert isScopeMarker(marker)
        for x in reversed(self.scopeStack) :
            if isRTValue(x) :
                rtValueDestroy(x)
            if x is marker :
                return
        assert False

    def popUpto(self, marker) :
        assert isScopeMarker(marker)
        while True :
            thing = self.scopeStack.pop()
            if thing is marker :
                break

    def addLabel(self, name, marker) :
        if name.s in self.labels :
            withContext(name, lambda : error("label redefinition"))
        llvmBlock = self.llvmFunc.append_basic_block(name.s)
        self.labels[name.s] = (llvmBlock, marker)

    def lookupLabel(self, name) :
        result = self.labels.get(name.s)
        if result is None :
            withContext(name, lambda : error("label not found"))
        llvmBlock, marker = result
        return llvmBlock, marker

    def lookupLabelBlock(self, name) :
        llvmBlock, _ = self.lookupLabel(name)
        return llvmBlock

    def pushLoopInfo(self, breakBlock, breakMarker,
                    continueBlock, continueMarker) :
        self.breakInfo.append((breakBlock, breakMarker))
        self.continueInfo.append((continueBlock, continueMarker))

    def popLoopInfo(self) :
        self.breakInfo.pop()
        self.continueInfo.pop()

    def lookupBreak(self) :
        if not self.breakInfo :
            error("invalid break statement")
        llvmBlock, marker = self.breakInfo[-1]
        return llvmBlock, marker

    def lookupContinue(self) :
        if not self.continueInfo :
            error("invalid continue statement")
        llvmBlock, marker = self.continueInfo[-1]
        return llvmBlock, marker



#
# compileCodeBody
#

def compileCodeBody(code, env, bindings, compiledCode) :
    func = compiledCode.llvmFunc
    initBlock = func.append_basic_block("entry")
    codeBlock = func.append_basic_block("code")
    global llvmFunction, llvmBuilder, llvmInitBuilder
    savedLLVMFunction = llvmFunction
    savedLLVMBuilder = llvmBuilder
    savedLLVMInitBuilder = llvmInitBuilder
    llvmFunction = func
    llvmInitBuilder = llvm.Builder.new(initBlock)
    llvmBuilder = llvm.Builder.new(codeBlock)
    params = []
    for i, p in enumerate(bindings.params) :
        if isRTValue(p) :
            p = RTValue(p.type, func.args[i])
        elif isRTReference(p) :
            p = RTReference(p.type, func.args[i])
        else :
            assert False, p
        params.append(p)
    env = extendEnv(env, bindings.typeVars + bindings.vars,
                    bindings.typeParams + params)
    codeContext = CompilerCodeContext(compiledCode)
    isTerminated = compileStatement(code.body, env, codeContext)
    if not isTerminated :
        llvmBuilder.ret_void()
    llvmInitBuilder.branch(codeBlock)
    llvmFunction = savedLLVMFunction
    llvmBuilder = savedLLVMBuilder
    llvmInitBuilder = savedLLVMInitBuilder



#
# compileStatement
#

def compileStatement(x, env, codeContext) :
    pushTempsBlock()
    try :
        return withContext(x, lambda : compileStatement2(x, env, codeContext))
    finally :
        popTempsBlock()



#
# compileStatement2
#

compileStatement2 = multimethod(errorMessage="invalid statement")

@compileStatement2.register(Block)
def foo(x, env, codeContext) :
    env = Environment(env)
    blockMarker = codeContext.pushMarker()
    compilerCollectLabels(x.statements, 0, blockMarker, codeContext)
    isTerminated = False
    for i, y in enumerate(x.statements) :
        if type(y) is Label :
            labelBlock = codeContext.lookupLabelBlock(y.name)
            llvmBuilder.branch(labelBlock)
            llvmBuilder.position_at_end(labelBlock)
            isTerminated = False
        elif isTerminated :
            withContext(y, lambda : error("unreachable code"))
        elif type(y) in (VarBinding, RefBinding, StaticBinding) :
            env = compileBinding(y, env, codeContext)
            marker = codeContext.pushMarker()
            compilerCollectLabels(x.statements, i+1, marker, codeContext)
        else :
            isTerminated = compileStatement(y, env, codeContext)
    if not isTerminated :
        codeContext.cleanUpto(blockMarker)
    codeContext.popUpto(blockMarker)
    return isTerminated

compileBinding = multimethod(errorMessage="invalid binding")

@compileBinding.register(VarBinding)
def foo(x, env, codeContext) :
    converter = analyzer.toRTValue
    if x.type is not None :
        declaredType = compileRootExpr(x.type, env, toType)
        converter = analyzer.toRTValueOfType(declaredType)
    result = analyzer.analyzeRootExpr(x.expr, envForAnalysis(env), converter)
    v = codeContext.pushNewValue(result.type)
    returnLoc = RTReturnLocation(v.type, v.llvmValue)
    right = compileRootExpr(x.expr, env, toRTValueOfType(v.type),
                            returnLocation=returnLoc)
    return extendEnv(env, [x.name], [right])

@compileBinding.register(RefBinding)
def foo(x, env, codeContext) :
    converter = analyzer.toRTValueOrReference
    if x.type is not None :
        declaredType = compileRootExpr(x.type, env, toType)
        converter = analyzer.toRTValueOrReferenceOfType(declaredType)
    result = analyzer.analyzeRootExpr(x.expr, envForAnalysis(env), converter)
    if analyzer.isRTValue(result.type) :
        v = codeContext.pushNewValue(result.type)
        returnLoc = RTReturnLocation(v.type, v.llvmValue)
        right = compileRootExpr(x.expr, env, toRTValueOfType(v.type),
                                returnLocation=returnLoc)
    else :
        right = compileRootExpr(x.expr, env, toRTReferenceOfType(result.type))
    return extendEnv(env, [x.name], [right])

@compileBinding.register(StaticBinding)
def foo(x, env, codeContext) :
    right = evaluateRootExpr(x.expr, env, toStatic)
    return extendEnv(env, [x.name], [right])

def compilerCollectLabels(statements, i, marker, codeContext) :
    while i < len(statements) :
        stmt = statements[i]
        if type(stmt) is Label :
            codeContext.addLabel(stmt.name, marker)
        elif type(stmt) in (VarBinding, RefBinding, StaticBinding) :
            break
        i += 1

@compileStatement2.register(Assignment)
def foo(x, env, codeContext) :
    pushRTTempsBlock()
    try :
        left = compile(x.left, env, toRTLValue)
        right = compile(x.right, env, toRTReference)
        rtValueAssign(left, right)
    finally :
        popRTTempsBlock()
    return False

@compileStatement2.register(Goto)
def foo(x, env, codeContext) :
    llvmBlock, marker = codeContext.lookupLabel(x.labelName)
    codeContext.cleanUpto(marker)
    llvmBuilder.branch(llvmBlock)
    return True

@compileStatement2.register(Return)
def foo(x, env, codeContext) :
    retType = codeContext.returnType
    if isVoidType(retType) :
        ensure(x.expr is None, "void return expected")
        codeContext.cleanAll()
        llvmBuilder.ret_void()
        return True
    ensure(x.expr is not None, "return value expected")
    if codeContext.returnByRef :
        result = compileRootExpr(x.expr, env, toRTLValueOfType(retType))
        codeContext.cleanAll()
        llvmBuilder.ret(result.llvmValue)
        return True
    returnLoc = RTReturnLocation(retType, codeContext.llvmFunc.args[-1])
    compileRootExpr(x.expr, env, toRTValueOfType(retType),
                    returnLocation=returnLoc)
    codeContext.cleanAll()
    llvmBuilder.ret_void()
    return True

def toLLVMBool(x) :
    condRef = toRTReferenceOfType(boolType)(x)
    cond = llvmBuilder.load(condRef.llvmValue)
    zero = llvm.Constant.int(llvmType(boolType), 0)
    return llvmBuilder.icmp(llvm.ICMP_NE, cond, zero)

@compileStatement2.register(IfStatement)
def foo(x, env, codeContext) :
    condResult = compileRootExpr(x.condition, env, toLLVMBool)
    thenBlock = codeContext.llvmFunc.append_basic_block("then")
    elseBlock = codeContext.llvmFunc.append_basic_block("else")
    mergeBlock = None
    llvmBuilder.cbranch(condResult, thenBlock, elseBlock)
    llvmBuilder.position_at_end(thenBlock)
    thenTerminated = compileStatement(x.thenPart, env, codeContext)
    if not thenTerminated :
        if mergeBlock is None :
            mergeBlock = codeContext.llvmFunc.append_basic_block("merge")
        llvmBuilder.branch(mergeBlock)
    elseTerminated = False
    llvmBuilder.position_at_end(elseBlock)
    if x.elsePart is not None :
        elseTerminated = compileStatement(x.elsePart, env, codeContext)
    if not elseTerminated :
        if mergeBlock is None :
            mergeBlock = codeContext.llvmFunc.append_basic_block("merge")
        llvmBuilder.branch(mergeBlock)
    if mergeBlock is not None :
        assert not (thenTerminated and elseTerminated)
        llvmBuilder.position_at_end(mergeBlock)
        return False
    assert thenTerminated and elseTerminated
    return True

@compileStatement2.register(ExprStatement)
def foo(x, env, codeContext) :
    compileRootExpr(x.expr, env)
    return False

@compileStatement2.register(While)
def foo(x, env, codeContext) :
    condBlock = codeContext.llvmFunc.append_basic_block("while")
    bodyBlock = codeContext.llvmFunc.append_basic_block("body")
    exitBlock = codeContext.llvmFunc.append_basic_block("exit")
    marker = codeContext.pushMarker()
    codeContext.pushLoopInfo(exitBlock, marker, condBlock, marker)
    llvmBuilder.branch(condBlock)
    llvmBuilder.position_at_end(condBlock)
    condResult = compileRootExpr(x.condition, env, toLLVMBool)
    llvmBuilder.cbranch(condResult, bodyBlock, exitBlock)
    llvmBuilder.position_at_end(bodyBlock)
    bodyTerminated = compileStatement(x.body, env, codeContext)
    if not bodyTerminated :
        llvmBuilder.branch(condBlock)
    llvmBuilder.position_at_end(exitBlock)
    codeContext.popLoopInfo()
    codeContext.popUpto(marker)
    return False

@compileStatement2.register(Break)
def foo(x, env, codeContext) :
    llvmBlock, marker = codeContext.lookupBreak()
    codeContext.cleanUpto(marker)
    llvmBuilder.branch(llvmBlock)
    return True

@compileStatement2.register(Continue)
def foo(x, env, codeContext) :
    llvmBlock, marker = codeContext.lookupContinue()
    codeContext.cleanUpto(marker)
    llvmBuilder.branch(llvmBlock)
    return True

@compileStatement2.register(For)
def foo(x, env, codeContext) :
    return compileStatement(convertForStatement(x), env, codeContext)



#
# remove temp name used for multimethod instances
#

del foo
