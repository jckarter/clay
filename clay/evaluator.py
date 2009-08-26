import ctypes
from clay.xprint import *
from clay.error import *
from clay.multimethod import *
from clay.machineops import *
from clay.ast import *
from clay.env import *
from clay.coreops import *
from clay.types import *
from clay.unify import *



#
# VoidValue, voidValue
#

class VoidValue(object) :
    pass

voidValue = VoidValue()

xregister(VoidValue, lambda x : XSymbol("void"))



#
# evaluate
#

def evaluate(expr, env, converter=(lambda x : x)) :
    try :
        contextPush(expr)
        return converter(evaluate2(expr, env))
    finally :
        contextPop()



#
# evaluate2
#

evaluate2 = multimethod(errorMessage="invalid expression")

@evaluate2.register(BoolLiteral)
def foo(x, env) :
    return boolToValue(x.value)

@evaluate2.register(IntLiteral)
def foo(x, env) :
    return intToValue(x.value)

@evaluate2.register(CharLiteral)
def foo(x, env) :
    return charToValue(x.value)

@evaluate2.register(NameRef)
def foo(x, env) :
    return lookupIdent(env, x.name)

@evaluate2.register(Tuple)
def foo(x, env) :
    assert len(x.args) > 0
    first = evaluate(x.args[0], env)
    if len(x.args) == 1 :
        return first
    if isType(first) or isCell(first) :
        rest = [evaluate(y, env, toTypeOrCell) for y in x.args[1:]]
        return tupleType([first] + rest)
    else :
        args = [withContext(x.args[0], lambda : toReference(first))]
        args.extend([evaluate(y, env, toReference) for y in x.args[1:]])
        return makeTuple(args)

@evaluate2.register(Indexing)
def foo(x, env) :
    thing = evaluate(x.expr, env)
    return evaluateIndexing(thing, x.args, env)

@evaluate2.register(Call)
def foo(x, env) :
    thing = evaluate(x.expr, env)
    return evaluateCall(thing, x.args, env)

@evaluate2.register(FieldRef)
def foo(x, env) :
    thing = evaluate(x.expr, env, toRecordReference)
    return recordFieldRef(thing, recordFieldIndex(thing.type, x.name))

@evaluate2.register(TupleRef)
def foo(x, env) :
    thing = evaluate(x.expr, env, toReferenceWithTypeTag(tupleTypeTag))
    return tupleFieldRef(thing, x.index)

@evaluate2.register(Dereference)
def foo(x, env) :
    return evaluateCall(primitives.pointerDereference, [x.expr], env)

@evaluate2.register(AddressOf)
def foo(x, env) :
    return evaluateCall(primitives.addressOf, [x.expr], env)



#
# evaluateIndexing
#

evaluateIndexing = multimethod(errorMessage="invalid indexing")

@evaluateIndexing.register(Record)
def foo(x, args, env) :
    ensureArity(args, len(x.typeVars))
    typeParams = [evaluate(y, env, toStaticOrCell) for y in args]
    return recordType(x, typeParams)

@evaluateIndexing.register(primitives.Tuple)
def foo(x, args, env) :
    ensure(len(args) > 1, "tuple types need atleast two member types")
    elementTypes = [evaluate(y, env, toTypeOrCell) for y in args]
    return tupleType(elementTypes)

@evaluateIndexing.register(primitives.Array)
def foo(x, args, env) :
    ensureArity(args, 2)
    elementType = evaluate(args[0], env, toTypeOrCell)
    sizeValue = evaluate(args[1], env, toValueOrCell)
    return arrayType(elementType, sizeValue)

@evaluateIndexing.register(primitives.Pointer)
def foo(x, args, env) :
    ensureArity(args, 1)
    targetType = evaluate(args[0], env, toTypeOrCell)
    return pointerType(targetType)



#
# evaluateCall
#

evaluateCall = multimethod(errorMessage="invalid call")

@evaluateCall.register(Record)
def foo(x, args, env) :
    ensureArity(args, len(x.fields))
    recordEnv, cells = bindTypeVars(x.env, x.typeVars)
    fieldTypePatterns = computeFieldTypes(x.fields, recordEnv)
    argRefs = [evaluate(y, env, toReference) for y in args]
    for typePattern, argRef, arg in zip(fieldTypePatterns, argRefs, args) :
        withContext(arg, lambda : matchType(typePattern, argRef.type))
    typeParams = resolveTypeVars(x.typeVars, cells)
    recType = recordType(x, typeParams)
    return makeRecord(recType, argRefs)

@evaluateCall.register(Procedure)
def foo(x, args, env) :
    argWrappers = [ArgumentWrapper(y, env) for y in args]
    result = matchCodeSignature(x.env, x.code, argWrappers)
    if type(result) is MatchFailure :
        result.signalError(x.code, args)
    assert type(result) is Environment
    procEnv = result
    return evalCodeBody(x.code, procEnv)

@evaluateCall.register(Overloadable)
def foo(x, args, env) :
    argWrappers = [ArgumentWrapper(y, env) for y in args]
    for y in x.overloads :
        result = matchCodeSignature(y.env, y.code, argWrappers)
        if type(result) is MatchFailure :
            continue
        assert type(result) is Environment
        procEnv = result
        return evalCodeBody(y.code, procEnv)
    error("no matching overload")



#
# evaluate primitives
#

@evaluateCall.register(primitives.default)
def foo(x, args, env) :
    ensureArity(args, 1)
    t = evaluate(args[0], env, toType)
    v = tempValue(t)
    valueInit(v)
    return v

@evaluateCall.register(primitives.typeSize)
def foo(x, args, env) :
    ensureArity(args, 1)
    t = evaluate(args[0], env, toType)
    return intToValue(typeSize(t))



#
# machine op utils
#

def callMachineOp(f, args) :
    def toPtr(x) :
        if type(x) is Value :
            return ctypes.pointer(x.buf)
        elif type(x) is Reference :
            return ctypes.c_void_p(x.address)
        else :
            assert False
    ptrs = map(toPtr, args)
    return f(*ptrs)

def simpleMop(mop, args, env, argTypes, returnType) :
    ensureArity(args, len(argTypes))
    mopInput = []
    for arg, argType in zip(args, argTypes) :
        ref = evaluate(arg, env, toReferenceOfType(argType))
        mopInput.append(ref)
    if returnType is not None :
        result = tempValue(returnType)
        mopInput.append(result)
    else :
        result = voidValue
    callMachineOp(mop, mopInput)
    return result



#
# evaluate pointer primitives
#

def pointerToAddress(ptr) :
    void_ptr = ctypes.cast(ptr.buf, ctypes.c_void_p)
    return void_ptr.value

def addressToPointer(addr, ptrType) :
    void_ptr = ctypes.c_void_p(addr)
    ptr = tempValue(ptrType)
    ptr.buf = ctypes.cast(void_ptr, ptr.ctypesType)
    return ptr

@evaluateCall.register(primitives.addressOf)
def foo(x, args, env) :
    ensureArity(args, 1)
    valueRef = evaluate(args[0], env, toLValue)
    return addressToPointer(valueRef.address, pointerType(valueRef.type))

@evaluateCall.register(primitives.pointerDereference)
def foo(x, args, env) :
    ensureArity(args, 1)
    cell = Cell()
    ptr = evaluate(args[0], env, toValueOfType(pointerType(cell)))
    return Reference(cell.param, pointerToAddress(ptr))

@evaluateCall.register(primitives.pointerOffset)
def foo(x, args, env) :
    ensureArity(args, 2)
    cell = Cell()
    ptr = evaluate(args[0], env, toReferenceOfType(pointerType(cell)))
    offset = evaluate(args[1], env, toReferenceOfType(intType))
    result = tempValue(ptr.type)
    callMachineOp(mop_pointerOffset, [ptr, offset, result])
    return result

@evaluateCall.register(primitives.pointerSubtract)
def foo(x, args, env) :
    ensureArity(args, 2)
    cell = Cell()
    ptr1 = evaluate(args[0], env, toReferenceOfType(pointerType(cell)))
    ptr2 = evaluate(args[1], env, toReferenceOfType(pointerType(cell)))
    result = tempValue(intType)
    callMachineOp(mop_pointerSubtract, [ptr1, ptr2, result])
    return result

@evaluateCall.register(primitives.pointerCast)
def foo(x, args, env) :
    ensureArity(args, 2)
    cell = Cell()
    targetType = evaluate(args[0], env, toType)
    ptr = evaluate(args[1], env, toValueOfType(pointerType(cell)))
    return addressToPointer(pointerToAddress(ptr), pointerType(targetType))

@evaluateCall.register(primitives.pointerCopy)
def foo(x, args, env) :
    ensureArity(args, 2)
    cell = Cell()
    destRef = evaluate(args[0], env, toReferenceOfType(pointerType(cell)))
    srcRef = evaluate(args[1], env, toReferenceOfType(pointerType(cell)))
    callMachineOp(mop_pointerCopy, [destRef, srcRef])
    return voidValue

@evaluateCall.register(primitives.pointerEquals)
def foo(x, args, env) :
    ensureArity(args, 2)
    cell = Cell()
    ptr1 = evaluate(args[0], env, toReferenceOfType(pointerType(cell)))
    ptr2 = evaluate(args[1], env, toReferenceOfType(pointerType(cell)))
    result = tempValue(boolType)
    callMachineOp(mop_pointerEquals, [ptr1, ptr2, result])
    return result

@evaluateCall.register(primitives.pointerLesser)
def foo(x, args, env) :
    ensureArity(args, 2)
    cell = Cell()
    ptr1 = evaluate(args[0], env, toReferenceOfType(pointerType(cell)))
    ptr2 = evaluate(args[1], env, toReferenceOfType(pointerType(cell)))
    result = tempValue(boolType)
    callMachineOp(mop_pointerLesser, [ptr1, ptr2, result])
    return result

@evaluateCall.register(primitives.allocateMemory)
def foo(x, args, env) :
    ensureArity(args, 1)
    size = evaluate(args[0], env, toReferenceOfType(intType))
    result = tempValue(pointerType(intType))
    callMachineOp(mop_allocateMemory, [size, result])
    return result

@evaluateCall.register(primitives.freeMemory)
def foo(x, args, env) :
    ensureArity(args, 1)
    ptr = evaluate(args[0], env, toReferenceOfType(pointerType(intType)))
    callMachineOp(mop_freeMemory, [ptr])
    return voidValue



#
# evaluate tuple primitives
#

@evaluateCall.register(primitives.TupleType)
def foo(x, args, env) :
    ensureArity(args, 1)
    t = evaluate(args[0], env, toType)
    return boolToValue(isTupleType(t))

@evaluateCall.register(primitives.tuple)
def foo(x, args, env) :
    ensure(len(args) > 1, "tuples need atleast two members")
    valueRefs = [evaluate(y, env, toReference) for y in args]
    return makeTuple(valueRefs)

@evaluateCall.register(primitives.tupleFieldCount)
def foo(x, args, env) :
    ensureArity(args, 1)
    t = evaluate(args[0], env, toTypeWithTag(tupleTypeTag))
    return intToValue(len(t.params))

@evaluateCall.register(primitives.tupleFieldRef)
def foo(x, args, env) :
    ensureArity(args, 2)
    tupleRef = evaluate(args[0], env, toReferenceWithTypeTag(tupleTypeTag))
    i = evaluate(args[1], env, toInt)
    return tupleFieldRef(tupleRef, i)



#
# evaluate array primitives
#

@evaluateCall.register(primitives.array)
def foo(x, args, env) :
    ensureArity(args, 2)
    n = evaluate(args[0], env, toInt)
    v = evaluate(args[1], env, toReference)
    return makeArray(n, v)

@evaluateCall.register(primitives.arrayRef)
def foo(x, args, env) :
    ensureArity(args, 2)
    a = evaluate(args[0], env, toReference)
    i = evaluate(args[1], env, toInt)
    return arrayRef(a, i)



#
# evaluate record primitives
#

@evaluateCall.register(primitives.RecordType)
def foo(x, args, env) :
    ensureArity(args, 1)
    t = evaluate(args[0], env, toType)
    return boolToValue(isRecordType(t))

@evaluateCall.register(primitives.recordFieldCount)
def foo(x, args, env) :
    ensureArity(args, 1)
    t = evaluate(args[0], env, toRecordType)
    return intToValue(len(t.tag.fields))

@evaluateCall.register(primitives.recordFieldRef)
def foo(x, args, env) :
    ensureArity(args, 2)
    recRef = evaluate(args[0], env, toRecordReference)
    i = evaluate(args[1], env, toInt)
    return recordFieldRef(recRef, i)



#
# evaluate bool primitives
#

@evaluateCall.register(primitives.boolCopy)
def foo(x, args, env) :
    argTypes = [boolType, boolType]
    return simpleMop(mop_boolCopy, args, env, argTypes, None)

@evaluateCall.register(primitives.boolNot)
def foo(x, args, env) :
    argTypes = [boolType]
    return simpleMop(mop_boolNot, args, env, argTypes, boolType)



#
# evaluate char primitives
#

@evaluateCall.register(primitives.charCopy)
def foo(x, args, env) :
    argTypes = [charType, charType]
    return simpleMop(mop_charCopy, args, env, argTypes, None)

@evaluateCall.register(primitives.charEquals)
def foo(x, args, env) :
    argTypes = [charType, charType]
    return simpleMop(mop_charEquals, args, env, argTypes, boolType)

@evaluateCall.register(primitives.charLesser)
def foo(x, args, env) :
    argTypes = [charType, charType]
    return simpleMop(mop_charLesser, args, env, argTypes, boolType)



#
# evaluate int primitives
#

@evaluateCall.register(primitives.intCopy)
def foo(x, args, env) :
    argTypes = [intType, intType]
    return simpleMop(mop_intCopy, args, env, argTypes, None)

@evaluateCall.register(primitives.intEquals)
def foo(x, args, env) :
    argTypes = [intType, intType]
    return simpleMop(mop_intEquals, args, env, argTypes, boolType)

@evaluateCall.register(primitives.intLesser)
def foo(x, args, env) :
    argTypes = [intType, intType]
    return simpleMop(mop_intLesser, args, env, argTypes, boolType)

@evaluateCall.register(primitives.intAdd)
def foo(x, args, env) :
    argTypes = [intType, intType]
    return simpleMop(mop_intAdd, args, env, argTypes, intType)

@evaluateCall.register(primitives.intSubtract)
def foo(x, args, env) :
    argTypes = [intType, intType]
    return simpleMop(mop_intSubtract, args, env, argTypes, intType)

@evaluateCall.register(primitives.intMultiply)
def foo(x, args, env) :
    argTypes = [intType, intType]
    return simpleMop(mop_intMultiply, args, env, argTypes, intType)

@evaluateCall.register(primitives.intDivide)
def foo(x, args, env) :
    argTypes = [intType, intType]
    return simpleMop(mop_intDivide, args, env, argTypes, intType)

@evaluateCall.register(primitives.intModulus)
def foo(x, args, env) :
    argTypes = [intType, intType]
    return simpleMop(mop_intModulus, args, env, argTypes, intType)

@evaluateCall.register(primitives.intNegate)
def foo(x, args, env) :
    argTypes = [intType]
    return simpleMop(mop_intNegate, args, env, argTypes, intType)



#
# evaluate float primitives
#

@evaluateCall.register(primitives.floatCopy)
def foo(x, args, env) :
    argTypes = [floatType, floatType]
    return simpleMop(mop_floatCopy, args, env, argTypes, None)

@evaluateCall.register(primitives.floatEquals)
def foo(x, args, env) :
    argTypes = [floatType, floatType]
    return simpleMop(mop_floatEquals, args, env, argTypes, boolType)

@evaluateCall.register(primitives.floatLesser)
def foo(x, args, env) :
    argTypes = [floatType, floatType]
    return simpleMop(mop_floatLesser, args, env, argTypes, boolType)

@evaluateCall.register(primitives.floatAdd)
def foo(x, args, env) :
    argTypes = [floatType, floatType]
    return simpleMop(mop_floatAdd, args, env, argTypes, floatType)

@evaluateCall.register(primitives.floatSubtract)
def foo(x, args, env) :
    argTypes = [floatType, floatType]
    return simpleMop(mop_floatSubtract, args, env, argTypes, floatType)

@evaluateCall.register(primitives.floatMultiply)
def foo(x, args, env) :
    argTypes = [floatType, floatType]
    return simpleMop(mop_floatMultiply, args, env, argTypes, floatType)

@evaluateCall.register(primitives.floatDivide)
def foo(x, args, env) :
    argTypes = [floatType, floatType]
    return simpleMop(mop_floatDivide, args, env, argTypes, floatType)

@evaluateCall.register(primitives.floatNegate)
def foo(x, args, env) :
    argTypes = [floatType]
    return simpleMop(mop_floatNegate, args, env, argTypes, floatType)



#
# evaluate double primitives
#

@evaluateCall.register(primitives.doubleCopy)
def foo(x, args, env) :
    argTypes = [doubleType, doubleType]
    return simpleMop(mop_doubleCopy, args, env, argTypes, None)

@evaluateCall.register(primitives.doubleEquals)
def foo(x, args, env) :
    argTypes = [doubleType, doubleType]
    return simpleMop(mop_doubleEquals, args, env, argTypes, boolType)

@evaluateCall.register(primitives.doubleLesser)
def foo(x, args, env) :
    argTypes = [doubleType, doubleType]
    return simpleMop(mop_doubleLesser, args, env, argTypes, boolType)

@evaluateCall.register(primitives.doubleAdd)
def foo(x, args, env) :
    argTypes = [doubleType, doubleType]
    return simpleMop(mop_doubleAdd, args, env, argTypes, doubleType)

@evaluateCall.register(primitives.doubleSubtract)
def foo(x, args, env) :
    argTypes = [doubleType, doubleType]
    return simpleMop(mop_doubleSubtract, args, env, argTypes, doubleType)

@evaluateCall.register(primitives.doubleMultiply)
def foo(x, args, env) :
    argTypes = [doubleType, doubleType]
    return simpleMop(mop_doubleMultiply, args, env, argTypes, doubleType)

@evaluateCall.register(primitives.doubleDivide)
def foo(x, args, env) :
    argTypes = [doubleType, doubleType]
    return simpleMop(mop_doubleDivide, args, env, argTypes, doubleType)

@evaluateCall.register(primitives.doubleNegate)
def foo(x, args, env) :
    argTypes = [doubleType]
    return simpleMop(mop_doubleNegate, args, env, argTypes, doubleType)



#
# evaluate conversion primitives
#

@evaluateCall.register(primitives.charToInt)
def foo(x, args, env) :
    return simpleMop(mop_charToInt, args, env, [charType], intType)

@evaluateCall.register(primitives.intToChar)
def foo(x, args, env) :
    return simpleMop(mop_intToChar, args, env, [intType], charType)

@evaluateCall.register(primitives.floatToInt)
def foo(x, args, env) :
    return simpleMop(mop_floatToInt, args, env, [floatType], intType)

@evaluateCall.register(primitives.intToFloat)
def foo(x, args, env) :
    return simpleMop(mop_intToFloat, args, env, [intType], floatType)

@evaluateCall.register(primitives.floatToDouble)
def foo(x, args, env) :
    return simpleMop(mop_floatToDouble, args, env, [floatType], doubleType)

@evaluateCall.register(primitives.doubleToFloat)
def foo(x, args, env) :
    return simpleMop(mop_doubleToFloat, args, env, [doubleType], floatType)

@evaluateCall.register(primitives.doubleToInt)
def foo(x, args, env) :
    return simpleMop(mop_doubleToInt, args, env, [doubleType], intType)

@evaluateCall.register(primitives.intToDouble)
def foo(x, args, env) :
    return simpleMop(mop_intToDouble, args, env, [intType], doubleType)



#
# matchCodeSignature(env, code, args) -> Environment | MatchFailure
#

class MatchFailure(object) :
    def __init__(self, message, context=None) :
        self.message = message
        self.context = context
    def signalError(self) :
        try :
            if self.context is not None :
                contextPush(self.context)
            error(self.message)
        finally :
            if self.context is not None :
                contextPop()

class ArgumentWrapper(object) :
    def __init__(self, expr, env) :
        self.expr = expr
        self.env = env
        self.result_ = None

    def evaluate(self, converter=None) :
        if self.result_ is None :
            self.result_ = evaluate(self.expr, self.env)
        try :
            contextPush(self.expr)
            if converter is not None :
                return converter(self.result_)
            return self.result_
        finally :
            contextPop()

def matchCodeSignature(codeEnv, code, args) :
    def argMismatch(arg) :
        return MatchFailure("argument mismatch", arg.expr)
    def typeCondFailure(typeCond) :
        return MatchFailure("type condition failed", typeCond)
    if len(args) != len(code.formalArgs) :
        return MatchFailure("incorrect no. of arguments")
    codeEnv2, cells = bindTypeVars(codeEnv, code.typeVars)
    bindings = []
    for arg, formalArg in zip(args, code.formalArgs) :
        if type(formalArg) is ValueArgument :
            if formalArg.byRef :
                argResult = arg.evaluate(toReference)
            else :
                argResult = arg.evaluate(toValue)
            if formalArg.type is not None :
                typePattern = evaluate(formalArg.type, codeEnv2, toTypeOrCell)
                if not unify(typePattern, argResult.type) :
                    return argMismatch(arg)
            bindings.append((formalArg.name, argResult))
        elif type(formalArg) is StaticArgument :
            argResult = arg.evaluate(toStatic)
            formalType = formalArg.type
            typePattern = evaluate(formalType, codeEnv2, toStaticOrCell)
            if not unify(typePattern, argResult) :
                return argMismatch(arg)
        else :
            assert False
    typeParams = resolveTypeVars(code.typeVars, cells)
    codeEnv2 = extendEnv(codeEnv, code.typeVars, typeParams)
    for typeCondition in code.typeConditions :
        result = evaluate(typeCondition, codeEnv2, toBool)
        if not result :
            return typeCondFailure(typeCondition)
    for ident, value in bindings :
        addIdent(codeEnv2, ident, value)
    return codeEnv2



#
# evalCodeBody
#

def evalCodeBody(code, env) :
    returnType = None
    if code.returnType is not None :
        returnType = evaluate(code.returnType, env, toType)
    context = CodeContext(code.returnByRef, returnType, topTempValueSet())
    result = evalStatement(code.body, env, context)
    if result is None :
        if returnType is not None :
            ensure(equals(returnType, voidType),
                   "unexpected void return")
        result = voidValue
    return result



#
# evalStatement : (stmt, env, context) -> Thing | None
#

class CodeContext(object) :
    def __init__(self, returnByRef, returnType, callerTemps) :
        self.returnByRef = returnByRef
        self.returnType = returnType
        self.callerTemps = callerTemps

def evalStatement(stmt, env, context) :
    try :
        contextPush(stmt)
        pushTempValueSet()
        return evalStatement2(stmt, env, context)
    finally :
        popTempValueSet()
        contextPop()



#
# evalStatement2
#

evalStatement2 = multimethod(errorMessage="invalid statement")

@evalStatement2.register(Block)
def foo(x, env, context) :
    for statement in x.statements :
        if type(statement) is LocalBinding :
            converter = toValue
            if statement.type is not None :
                declaredType = evaluate(statement.type, env, toType)
                converter = toValueOfType(declaredType)
            right = evaluate(statement.expr, env, converter)
            addIdent(env, statement.name, right)
        else :
            result = evalStatement(statement, env, context)
            if result is not None :
                return result

@evalStatement2.register(Assignment)
def foo(x, env, context) :
    left = evaluate(x.left, env, toLValue)
    right = evaluate(x.right, env, toReference)
    valueAssign(left, right)

@evalStatement2.register(Return)
def foo(x, env, context) :
    if context.returnByRef :
        result = evaluate(x.expr, env, toLValue)
        resultType = result.type
    elif x.expr is None :
        result = voidValue
        resultType = voidType
    else :
        result = evaluate(x.expr, env, toStatic)
        resultType = result.type if isValue(result) else None
        if isValue(result) :
            topTempValueSet().removeTemp(result)
            context.callerTemps.addTemp(result)
    if context.returnType is not None :
        def check() :
            matchType(resultType, context.returnType)
        withContext(x.expr, check)
    return result

@evalStatement2.register(IfStatement)
def foo(x, env, context) :
    cond = evaluate(x.condition, env, toBool)
    if cond :
        return evalStatement(x.thenPart, env, context)
    elif x.elsePart is not None :
        return evalStatement(x.elsePart, env, context)

@evalStatement2.register(ExprStatement)
def foo(x, env, context) :
    evaluate(x.expr, env)



#
# remove temp name used for multimethod instances
#

del foo

# TODO: fix cyclic import
from clay.values import *
