from clay.xprint import *
from clay.cleanup import *
from clay.error import *
from clay.multimethod import *
from clay.coreops import *
from clay.env import *
from clay.types import *
from clay.unify import *
from clay.evaluator import *



#
# RTValue, RTReference (RT for runtime)
#

class RTValue(object) :
    def __init__(self, type_) :
        self.type = type_

class RTReference(object) :
    def __init__(self, type_) :
        self.type = type_

def isRTValue(x) : return type(x) is RTValue
def isRTReference(x) : return type(x) is RTReference



#
# toStatic
#

@toStatic.register(RTValue)
def foo(x) :
    error("invalid static value")

@toStatic.register(RTReference)
def foo(x) :
    error("invalid static value")



#
# toRTValue, toRTLValue, toRTReference
#

toRTValue = multimethod(errorMessage="invalid value")
toRTLValue = multimethod(errorMessage="invalid reference")
toRTReference = multimethod(errorMessage="invalid reference")

toRTValue.register(RTValue)(lambda x : x)
toRTValue.register(RTReference)(lambda x : RTValue(x.type))
toRTValue.register(Value)(lambda x : RTValue(x.type))

toRTLValue.register(RTReference)(lambda x : x)

toRTReference.register(RTReference)(lambda x : x)
toRTReference.register(RTValue)(lambda x : RTReference(x.type))
toRTReference.register(Value)(lambda x : RTReference(x.type))



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
# analyze
#

def analyze(expr, env, converter=(lambda x : x)) :
    try :
        contextPush(expr)
        return converter(analyze2(expr, env))
    finally :
        contextPop()



#
# analyze2
#

analyze2 = multimethod(errorMessage="invalid expression")

@analyze2.register(BoolLiteral)
def foo(x, env) :
    return evaluate(x, env)

@analyze2.register(IntLiteral)
def foo(x, env) :
    return evaluate(x, env)

@analyze2.register(FloatLiteral)
def foo(x, env) :
    return evaluate(x, env)

@analyze2.register(NameRef)
def foo(x, env) :
    return analyzeNameRef(lookupIdent(env, x.name))

@analyze2.register(Tuple)
def foo(x, env) :
    if len(x.args) == 1 :
        return analyze(x.args[0], env)
    return analyze(Call(primitiveNameRef("tuple"), x.args), env)

@analyze2.register(Indexing)
def foo(x, env) :
    thing = analyze(x.expr, env)
    return analyzeIndexing(thing, x.args, env)

@analyze2.register(Call)
def foo(x, env) :
    thing = analyze(x.expr, env)
    return analyzeCall(thing, x.args, env)

@analyze2.register(FieldRef)
def foo(x, env) :
    thing = analyze(x.expr, env, toRTRecordReference)
    i = recordFieldIndex(thing.type, x.name)
    fieldType = recordFieldTypes(thing.type)[i]
    return RTReference(fieldType)

@analyze2.register(TupleRef)
def foo(x, env) :
    thing = analyze(x.expr, env, toRTReferenceWithTag(tupleTag))
    nFields = len(thing.type.params)
    ensure((0 <= x.index < nFields), "tuple field index out of range")
    return RTReference(thing.type.params[x.index])

@analyze2.register(Dereference)
def foo(x, env) :
    primOp = primitiveNameRef("pointerDereference")
    return analyze(Call(primOp, [x.expr]), env)

@analyze2.register(AddressOf)
def foo(x, env) :
    return analyze(Call(primitiveNameRef("addressOf"), [x.expr]), env)

@analyze2.register(UnaryOpExpr)
def foo(x, env) :
    return analyze(convertUnaryOpExpr(x), env)

@analyze2.register(BinaryOpExpr)
def foo(x, env) :
    return analyze(convertBinaryOpExpr(x), env)

@analyze2.register(NotExpr)
def foo(x, env) :
    analyze(x.expr, env, toRTValueOfType(boolType))
    return RTValue(boolType)

@analyze2.register(AndExpr)
def foo(x, env) :
    analyze(x.expr1, env, toRTValueOfType(boolType))
    analyze(x.expr2, env, toRTValueOfType(boolType))
    return RTValue(boolType)

@analyze2.register(OrExpr)
def foo(x, env) :
    analyze(x.expr1, env, toRTValueOfType(boolType))
    analyze(x.expr2, env, toRTValueOfType(boolType))
    return RTValue(boolType)

@analyze2.register(StaticExpr)
def foo(x, env) :
    return evaluate(x.expr, env, toStatic)

@analyze2.register(SCExpression)
def foo(x, env) :
    return analyze(x.expr, x.env)



#
# analyzeNameRef
#

analyzeNameRef = multimethod(defaultProc=(lambda x : x))

@analyzeNameRef.register(Reference)
def foo(x) :
    return toValue(x)

@analyzeNameRef.register(RTValue)
def foo(x) :
    return toRTReference(x)



#
# analyzeIndexing
#

analyzeIndexing = multimethod(errorMessage="invalid indexing")

@analyzeIndexing.register(Record)
def foo(x, args, env) :
    ensureArity(args, len(x.typeVars))
    typeParams = [analyze(y, env, toStaticOrCell) for y in args]
    return recordType(x, typeParams)

@analyzeIndexing.register(primitives.Tuple)
def foo(x, args, env) :
    ensure(len(args) > 1, "tuple types need atleast two member types")
    elementTypes = [analyze(y, env, toTypeOrCell) for y in args]
    return tupleType(elementTypes)

@analyzeIndexing.register(primitives.Array)
def foo(x, args, env) :
    ensureArity(args, 2)
    elementType = analyze(args[0], env, toTypeOrCell)
    sizeValue = analyze(args[1], env, toValueOrCell)
    return arrayType(elementType, sizeValue)

@analyzeIndexing.register(primitives.Pointer)
def foo(x, args, env) :
    ensureArity(args, 1)
    targetType = analyze(args[0], env, toTypeOrCell)
    return pointerType(targetType)



#
# analyzeCall
#

analyzeCall = multimethod(errorMessage="invalid call")

@analyzeCall.register(Type)
def foo(x, args, env) :
    if len(args) == 0 :
        return RTValue(x)
    elif isRecordType(x) :
        fieldTypes = recordFieldTypes(x)
        ensureArity(args, len(fieldTypes))
        argRefs = []
        for arg, fieldType in zip(args, fieldTypes) :
            argRef = analyze(arg, env, toRTReferenceOfType(fieldType))
            argRefs.append(argRef)
        return RTValue(x)
    elif isArrayType(x) :
        elementType = toType(x.params[0])
        ensureArity(args, toNativeInt(x.params[1]))
        for arg in args :
            analyze(arg, env, toRTReferenceOfType(elementType))
        return RTValue(x)
    elif isTupleType(x) :
        ensureArity(args, len(x.params))
        for i, arg in enumerate(args) :
            fieldType = toType(x.params[i])
            analyze(arg, env, toRTReferenceOfType(fieldType))
        return RTValue(x)
    else :
        error("only array, tuple, and record types " +
              "can be constructed this way.")

@analyzeCall.register(Record)
def foo(x, args, env) :
    if len(args) == 0 :
        return RTValue(toType(x))
    actualArgs = [RTActualArgument(y, env) for y in args]
    result = matchRecordInvoke(x, actualArgs)
    if type(result) is InvokeError :
        result.signalError()
    assert type(result) is InvokeBindings
    recType = recordType(x, result.typeParams)
    return RTValue(recType)

@analyzeCall.register(Procedure)
def foo(x, args, env) :
    actualArgs = [RTActualArgument(y, env) for y in args]
    result = matchInvoke(x.code, x.env, actualArgs)
    if type(result) is InvokeError :
        result.signalError()
    assert type(result) is InvokeBindings
    return analyzeInvoke(x.code, x.env, result)

@analyzeCall.register(Overloadable)
def foo(x, args, env) :
    actualArgs = [RTActualArgument(y, env) for y in args]
    for y in x.overloads :
        result = matchInvoke(y.code, y.env, actualArgs)
        if type(result) is InvokeError :
            continue
        assert type(result) is InvokeBindings
        return analyzeInvoke(y.code, y.env, result)
    error("no matching overload")



#
# analyze primitives
#

@analyzeCall.register(primitives._print)
def foo(x, args, env) :
    ensureArity(args, 1)
    analyze(args[0], env, toRTReference)
    return voidValue

@analyzeCall.register(primitives.typeSize)
def foo(x, args, env) :
    ensureArity(args, 1)
    analyze(args[0], env, toType)
    return RTValue(nativeIntType)

@analyzeCall.register(primitives.primitiveCopy)
def foo(x, args, env) :
    ensureArity(args, 2)
    cell = Cell()
    analyze(args[0], env, toRTReferenceOfType(cell))
    analyze(args[1], env, toRTReferenceOfType(cell))
    ensure(isSimpleType(cell.param), "expected simple type")
    return voidValue



#
# analyze pointer primitives
#

@analyzeCall.register(primitives.addressOf)
def foo(x, args, env) :
    ensureArity(args, 1)
    valueRef = analyze(args[0], env, toRTLValue)
    return RTValue(pointerType(valueRef.type))

@analyzeCall.register(primitives.pointerDereference)
def foo(x, args, env) :
    ensureArity(args, 1)
    cell = Cell()
    analyze(args[0], env, toRTValueOfType(pointerType(cell)))
    return RTReference(cell.param)

@analyzeCall.register(primitives.pointerToInt)
def foo(x, args, env) :
    ensureArity(args, 2)
    cell = Cell()
    analyze(args[0], env, toRTReferenceOfType(pointerType(cell)))
    outType = analyze(args[1], env, toIntegerType)
    return RTValue(outType)

@analyzeCall.register(primitives.intToPointer)
def foo(x, args, env) :
    ensureArity(args, 2)
    analyze(args[0], env, toRTIntegerReference)
    pointeeType = analyze(args[1], env, toType)
    return RTValue(pointerType(pointeeType))

@analyzeCall.register(primitives.pointerCast)
def foo(x, args, env) :
    ensureArity(args, 2)
    cell = Cell()
    targetType = analyze(args[0], env, toType)
    analyze(args[1], env, toRTValueOfType(pointerType(cell)))
    return RTValue(pointerType(targetType))

@analyzeCall.register(primitives.allocateMemory)
def foo(x, args, env) :
    ensureArity(args, 2)
    type_ = analyze(args[0], env, toType)
    analyze(args[1], env, toRTReferenceOfType(nativeIntType))
    return RTValue(pointerType(type_))

@analyzeCall.register(primitives.freeMemory)
def foo(x, args, env) :
    ensureArity(args, 1)
    cell = Cell()
    analyze(args[0], env, toRTReferenceOfType(pointerType(cell)))
    return voidValue



#
# analyze tuple primitives
#

@analyzeCall.register(primitives.TupleType)
def foo(x, args, env) :
    ensureArity(args, 1)
    analyze(args[0], env, toType)
    return RTValue(boolType)

@analyzeCall.register(primitives.tuple)
def foo(x, args, env) :
    ensure(len(args) > 1, "tuples need atleast two members")
    valueRefs = [analyze(y, env, toRTReference) for y in args]
    tupType = tupleType([y.type for y in valueRefs])
    return RTValue(tupType)

@analyzeCall.register(primitives.tupleFieldCount)
def foo(x, args, env) :
    ensureArity(args, 1)
    analyze(args[0], env, toTypeWithTag(tupleTag))
    return RTValue(nativeIntType)

@analyzeCall.register(primitives.tupleFieldRef)
def foo(x, args, env) :
    ensureArity(args, 2)
    thing = analyze(args[0], env, toRTReferenceWithTag(tupleTag))
    i = analyze(args[1], env, toNativeInt)
    nFields = len(thing.type.params)
    ensure((0 <= i < nFields), "tuple field index out of range")
    return RTReference(thing.type.params[i])



#
# analyze array primitives
#

@analyzeCall.register(primitives.arrayRef)
def foo(x, args, env) :
    ensureArity(args, 2)
    a = analyze(args[0], env, toRTReferenceWithTag(arrayTag))
    analyze(args[1], env, toRTValueOfType(nativeIntType))
    return RTReference(a.type.params[0])



#
# analyze record primitives
#

@analyzeCall.register(primitives.RecordType)
def foo(x, args, env) :
    ensureArity(args, 1)
    analyze(args[0], env, toType)
    return RTValue(boolType)

@analyzeCall.register(primitives.recordFieldCount)
def foo(x, args, env) :
    ensureArity(args, 1)
    analyze(args[0], env, toRecordType)
    return RTValue(nativeIntType)

@analyzeCall.register(primitives.recordFieldRef)
def foo(x, args, env) :
    ensureArity(args, 2)
    thing = analyze(args[0], env, toRTRecordReference)
    i = analyze(args[1], env, toNativeInt)
    nFields = len(recordValueArgs(thing.type.tag))
    ensure((0 <= i < nFields), "record field index out of range")
    fieldType = recordFieldTypes(thing.type)[i]
    return RTReference(fieldType)



#
# analyze numeric primitives
#

@analyzeCall.register(primitives.numericEquals)
def foo(x, args, env) :
    ensureArity(args, 2)
    x1 = analyze(args[0], env, toRTNumericReference)
    x2 = analyze(args[1], env, toRTNumericReference)
    ensure(x1.type == x2.type, "argument types mismatch")
    return RTValue(boolType)

@analyzeCall.register(primitives.numericLesser)
def foo(x, args, env) :
    ensureArity(args, 2)
    x1 = analyze(args[0], env, toRTNumericReference)
    x2 = analyze(args[1], env, toRTNumericReference)
    ensure(x1.type == x2.type, "argument types mismatch")
    return RTValue(boolType)

@analyzeCall.register(primitives.numericAdd)
def foo(x, args, env) :
    ensureArity(args, 2)
    x1 = analyze(args[0], env, toRTNumericReference)
    x2 = analyze(args[1], env, toRTNumericReference)
    ensure(x1.type == x2.type, "argument types mismatch")
    return RTValue(x1.type)

@analyzeCall.register(primitives.numericSubtract)
def foo(x, args, env) :
    ensureArity(args, 2)
    x1 = analyze(args[0], env, toRTNumericReference)
    x2 = analyze(args[1], env, toRTNumericReference)
    ensure(x1.type == x2.type, "argument types mismatch")
    return RTValue(x1.type)

@analyzeCall.register(primitives.numericMultiply)
def foo(x, args, env) :
    ensureArity(args, 2)
    x1 = analyze(args[0], env, toRTNumericReference)
    x2 = analyze(args[1], env, toRTNumericReference)
    ensure(x1.type == x2.type, "argument types mismatch")
    return RTValue(x1.type)

@analyzeCall.register(primitives.numericDivide)
def foo(x, args, env) :
    ensureArity(args, 2)
    x1 = analyze(args[0], env, toRTNumericReference)
    x2 = analyze(args[1], env, toRTNumericReference)
    ensure(x1.type == x2.type, "argument types mismatch")
    return RTValue(x1.type)

@analyzeCall.register(primitives.numericRemainder)
def foo(x, args, env) :
    ensureArity(args, 2)
    x1 = analyze(args[0], env, toRTNumericReference)
    x2 = analyze(args[1], env, toRTNumericReference)
    ensure(x1.type == x2.type, "argument types mismatch")
    return RTValue(x1.type)

@analyzeCall.register(primitives.numericNegate)
def foo(x, args, env) :
    ensureArity(args, 1)
    x1 = analyze(args[0], env, toRTNumericReference)
    return RTValue(x1.type)

@analyzeCall.register(primitives.numericConvert)
def foo(x, args, env) :
    ensureArity(args, 2)
    destType = analyze(args[0], env, toNumericType)
    analyze(args[1], env, toRTNumericReference)
    return RTValue(destType)



#
# analyze actual arguments
#

class RTActualArgument(object) :
    def __init__(self, expr, env) :
        self.expr = expr
        self.env = env
        self.result_ = analyze(self.expr, self.env)

    def asReference(self) :
        return withContext(self.expr, lambda : toRTReference(self.result_))

    def asStatic(self) :
        return withContext(self.expr, lambda : toStatic(self.result_))



#
# analyzeInvoke
#

_invokeTable = {}

def _cleanupInvokeTable() :
    global _invokeTable
    _invokeTable = {}

installGlobalsCleanupHook(_cleanupInvokeTable)

class RecursiveAnalysisError(Exception) :
    pass

def analyzeInvoke(code, env, bindings) :
    key = tuple([code] + bindings.typeParams +
                [p.type for p in bindings.params])
    result = _invokeTable.get(key)
    if result is not None :
        if result is False :
            raise RecursiveAnalysisError()
        return result
    try :
        result = None
        _invokeTable[key] = False
        env = extendEnv(env, bindings.typeVars + bindings.vars,
                        bindings.typeParams + bindings.params)
        result = analyzeInvoke2(code, env)
        _invokeTable[key] = result
        return result
    finally :
        if result is None :
            del _invokeTable[key]

def analyzeInvoke2(code, env) :
    if code.returnType is not None :
        returnType = analyze(code.returnType, env, toType)
        if code.returnByRef :
            return RTReference(returnType)
        else :
            return RTValue(returnType)
    context = CodeContext(code.returnByRef, None)
    result = analyzeStatement(code.body, env, context)
    if result is not None :
        return result
    return voidValue



#
# analyzeStatement
#

def analyzeStatement(x, env, context) :
    pushTempsBlock()
    result = withContext(x, lambda : analyzeStatement2(x, env, context))
    popTempsBlock()
    return result



#
# analyzeStatement2
#

analyzeStatement2 = multimethod(errorMessage="invalid statement")

@analyzeStatement2.register(Block)
def foo(x, env, context) :
    env = Environment(env)
    for y in x.statements :
        if type(y) is LocalBinding :
            converter = toRTValue
            if y.type is not None :
                declaredType = analyze(y.type, env, toType)
                converter = toRTValueOfType(declaredType)
            right = analyze(y.expr, env, converter)
            addIdent(env, y.name, right)
        elif type(y) is Label :
            # ignore labels during analysis
            pass
        else :
            result = analyzeStatement(y, env, context)
            if result is not None :
                return result

@analyzeStatement2.register(Assignment)
def foo(x, env, context) :
    # nothing to do
    pass

@analyzeStatement2.register(Goto)
def foo(x, env, context) :
    # ignore goto during analysis
    pass

@analyzeStatement2.register(Return)
def foo(x, env, context) :
    if context.returnByRef :
        return analyze(x.expr, env, toRTLValue)
    if x.expr is None :
        return voidValue
    return analyze(x.expr, env, toRTValue)

@analyzeStatement2.register(IfStatement)
def foo(x, env, context) :
    analyze(x.condition, env, toRTValueOfType(boolType))
    failed = 0
    try :
        result = analyzeStatement(x.thenPart, env, context)
        if result is not None :
            return result
    except RecursiveAnalysisError :
        failed += 1
    if x.elsePart is None :
        return
    try :
        result = analyzeStatement(x.elsePart, env, context)
        if result is not None :
            return result
    except RecursiveAnalysisError :
        failed += 1
    if failed == 2 :
        raise RecursiveAnalysisError()

@analyzeStatement2.register(ExprStatement)
def foo(x, env, context) :
    # nothing to do
    pass

@analyzeStatement2.register(While)
def foo(x, env, context) :
    analyze(x.condition, env, toRTValueOfType(boolType))
    try :
        result = analyzeStatement(x.body, env, context)
        if result is not None :
            return result
    except RecursiveAnalysisError :
        pass

@analyzeStatement2.register(Break)
def foo(x, env, context) :
    # ignore break during analysis
    pass

@analyzeStatement2.register(Continue)
def foo(x, env, context) :
    # ignore continue during analysis
    pass

@analyzeStatement2.register(For)
def foo(x, env, context) :
    return analyzeStatement(convertForStatement(x), env, context)



#
# remove temp name used for multimethod instances
#

del foo
