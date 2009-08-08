from clay.ast import *
from clay.env import *
from clay.types import *
from clay.error import *
from clay.multimethod import multimethod
from clay.xprint import xprint



#
# env operations
#

def addIdent(env, name, entry) :
    assert type(name) is Identifier
    if env.hasEntry(name.s) :
        raiseError("name redefinition", name)
    env.add(name.s, entry)

def lookupIdent(env, name) :
    assert type(name) is Identifier
    entry = env.lookup(name.s)
    if entry is None :
        raiseError("undefined name", name)
    return entry

def lookupPredicate(env, name) :
    x = lookupIdent(env, name)
    if type(x) is not PredicateEntry :
        raiseError("not a predicate", name)
    return x

def lookupOverloadable(env, name) :
    x = lookupIdent(env, name)
    if type(x) is not OverloadableEntry :
        raiseError("not an overloadable", name)
    return x



#
# utilities
#

def oneTypeParam(container, memberList) :
    if len(memberList) != 1 :
        raiseError("exactly one type parameter expected", container)
    return memberList[0]

def nTypeParams(n, container, memberList) :
    if len(memberList) != n :
        raiseError("exactly %d type parameter(s) expected" % n, container)
    return memberList

def oneArg(container, memberList) :
    if len(memberList) != 1 :
        raiseError("exactly one argument expected", container)
    return memberList[0]

def nArgs(n, container, memberList) :
    if len(memberList) != n :
        raiseError("exactly %d argument(s) expected" % n, container)
    return memberList

def intLiteralValue(x) :
    if type(x) is not IntLiteral :
        raiseError("int literal expected", x)
    return x.value

def getFieldTypes(t) :
    assert isRecordType(t)
    assert len(t.entry.ast.typeVars) == len(t.typeParams)
    env = Env(t.entry.env)
    for tvarName, typeParam in zip(t.entry.ast.typeVars, t.typeParams) :
        tvarEntry = TypeVarEntry(env, tvarName, typeParam)
        addIdent(env, tvarName, tvarEntry)
    return [inferTypeType(f.type, env) for f in t.entry.ast.fields]

def bindTypeVariables(env, typeVarNames) :
    tvarEntries = []
    for tvarName in typeVarNames :
        tvarEntry = TypeVarEntry(env, tvarName, TypeVariable())
        tvarEntries.append(tvarEntry)
        addIdent(env, tvarName, tvarEntry)
    return tvarEntries

def reduceTypeVariables(typeVarEntries) :
    for entry in typeVarEntries :
        t = typeDeref(entry.type)
        if t is None :
            raiseError("unresolved type variable", entry.ast)
        entry.type = t

def evalTypeCondition(env, typeVarEntries, typeCondition) :
    def typeVariableType(entry) :
        assert type(entry) is TypeVarEntry
        assert type(entry.type) is TypeVariable
    savedTypeVars = [typeVariableType(e) for e in typeVarEntries]
    def restoreTypeVars() :
        for entry, typeValue in zip(typeVarEntries, savedTypeVars) :
            entry.type.type = typeValue
    typeArgs = [inferTypeType(x, env) for x in typeCondition.typeArgs]
    predicateEntry = lookupPredicate(env, typeCondition.name)
    for instanceEntry in predicateEntry.instances :
        if evalPredicateInstance(instanceEntry, typeArgs) :
            return True
        restoreTypeVars()
    return False

def evalPredicateInstance(instanceEntry, typeArgs) :
    ast = instanceEntry.ast
    env = Env(instanceEntry.env)
    typeVarEntries = bindTypeVariables(env, ast.typeVars)
    formalTypes = [inferTypeType(x, env) for x in ast.typeArgs]
    for typeArg, formalType in zip(typeArgs, formalTypes) :
        if not typeUnify(typeArg, formalType) :
            return False
    for typeCondition in ast.typeConditions :
        if not evalTypeCondition(env, typeVarEntries, typeCondition) :
            return False
    return True



#
# primitivesEnv
#

def primitivesEnv() :
    env = Env()
    def a(name, klass) :
        env.add(name, klass(env,None))
    a("Bool", BoolTypeEntry)
    a("Char", CharTypeEntry)
    a("Int", IntTypeEntry)
    a("Float", FloatTypeEntry)
    a("Double", DoubleTypeEntry)
    a("Void", VoidTypeEntry)
    a("Array", ArrayTypeEntry)
    a("Pointer", PointerTypeEntry)

    a("default", PrimOpDefault)
    a("pointerOffset", PrimOpPointerOffset)
    a("pointerDifference", PrimOpPointerDifference)
    a("pointerCast", PrimOpPointerCast)
    a("pointerEquals", PrimOpPointerEquals)
    a("pointerLesser", PrimOpPointerLesser)
    a("pointerLesserEquals", PrimOpPointerLesserEquals)
    a("pointerGreater", PrimOpPointerGreater)
    a("pointerGreaterEquals", PrimOpPointerGreaterEquals)
    a("allocate", PrimOpAllocate)
    a("free", PrimOpFree)
    a("allocateBlock", PrimOpAllocateBlock)
    a("freeBlock", PrimOpFreeBlock)
    a("blockSize", PrimOpBlockSize)
    a("array", PrimOpArray)
    a("arraySize", PrimOpArraySize)

    a("boolNot", PrimOpBoolNot)
    a("charEquals", PrimOpCharEquals)
    a("charLesser", PrimOpCharLesser)
    a("charLesserEquals", PrimOpCharLesserEquals)
    a("charGreater", PrimOpCharGreater)
    a("charGreaterEquals", PrimOpCharGreaterEquals)
    a("intAdd", PrimOpIntAdd)
    a("intSubtract", PrimOpIntSubtract)
    a("intMultiply", PrimOpIntMultiply)
    a("intDivide", PrimOpIntDivide)
    a("intModulus", PrimOpIntModulus)
    a("intNegate", PrimOpIntNegate)
    a("intEquals", PrimOpIntEquals)
    a("intLesser", PrimOpIntLesser)
    a("intLesserEquals", PrimOpIntLesserEquals)
    a("intGreater", PrimOpIntGreater)
    a("intGreaterEquals", PrimOpIntGreaterEquals)
    a("floatAdd", PrimOpFloatAdd)
    a("floatSubtract", PrimOpFloatSubtract)
    a("floatMultiply", PrimOpFloatMultiply)
    a("floatDivide", PrimOpFloatDivide)
    a("floatNegate", PrimOpFloatNegate)
    a("floatEquals", PrimOpFloatEquals)
    a("floatLesser", PrimOpFloatLesser)
    a("floatLesserEquals", PrimOpFloatLesserEquals)
    a("floatGreater", PrimOpFloatGreater)
    a("floatGreaterEquals", PrimOpFloatGreaterEquals)
    a("doubleAdd", PrimOpDoubleAdd)
    a("doubleSubtract", PrimOpDoubleSubtract)
    a("doubleMultiply", PrimOpDoubleMultiply)
    a("doubleDivide", PrimOpDoubleDivide)
    a("doubleNegate", PrimOpDoubleNegate)
    a("doubleEquals", PrimOpDoubleEquals)
    a("doubleLesser", PrimOpDoubleLesser)
    a("doubleLesserEquals", PrimOpDoubleLesserEquals)
    a("doubleGreater", PrimOpDoubleGreater)
    a("doubleGreaterEquals", PrimOpDoubleGreaterEquals)
    a("charToInt", PrimOpCharToInt)
    a("intToChar", PrimOpIntToChar)
    a("floatToInt", PrimOpFloatToInt)
    a("intToFloat", PrimOpIntToFloat)
    a("floatToDouble", PrimOpFloatToDouble)
    a("doubleToFloat", PrimOpDoubleToFloat)
    a("doubleToInt", PrimOpDoubleToInt)
    a("intToDouble", PrimOpIntToDouble)
    return env



#
# buildTopLevelEnv
#

def buildTopLevelEnv(program) :
    env = Env(primitivesEnv())
    for item in program.topLevelItems :
        addTopLevel(item, env)
    return env

addTopLevel = multimethod()

@addTopLevel.register(PredicateDef)
def foo(x, env) :
    addIdent(env, x.name, PredicateEntry(env,x))

@addTopLevel.register(InstanceDef)
def foo(x, env) :
    entry = InstanceEntry(env, x)
    lookupPredicate(env, x.name).instances.append(entry)

@addTopLevel.register(RecordDef)
def foo(x, env) :
    addIdent(env, x.name, RecordEntry(env,x))

@addTopLevel.register(VariableDef)
def foo(x, env) :
    addIdent(env, x.variable.name, VariableEntry(env,x))

@addTopLevel.register(ProcedureDef)
def foo(x, env) :
    addIdent(env, x.name, ProcedureEntry(env,x))

@addTopLevel.register(OverloadableDef)
def foo(x, env) :
    addIdent(env, x.name, OverloadableEntry(env,x))

@addTopLevel.register(OverloadDef)
def foo(x, env) :
    entry = OverloadEntry(env, x)
    lookupOverloadable(env, x.name).overloads.append(entry)



#
# inferType : (expr, env) -> (isValue, type)
#             throws RecursiveInferenceError
#

class RecursiveInferenceError(Exception) :
    pass

inferType = multimethod()

def inferValueType(x, env, verify=None) :
    isValue, resultType = inferType(x, env)
    if not isValue :
        raiseError("value expected", x)
    if verify is not None :
        if not verify(resultType) :
            raiseError("invalid type", x)
    return resultType

def inferTypeType(x, env, verify=None) :
    isValue, resultType = inferType(x, env)
    if isValue :
        raiseError("type expected", x)
    if verify is not None :
        if not verify(resultType) :
            raiseError("invalid type", x)
    return resultType

@inferType.register(AddressOfExpr)
def foo(x, env) :
    return True, PointerType(inferValueType(x.expr, env))

@inferType.register(IndexExpr)
def foo(x, env) :
    if type(x.expr) is NameRef :
        entry = lookupIdent(env, x.expr.name)
        handler = inferNamedIndexExprType.getHandler(type(entry))
        if handler is not None :
            return handler(entry, x, env)
    return inferArrayIndexingType(x, env)

def inferArrayIndexingType(x, env) :
    exprType = inferValueType(x.expr, env, isArrayType)
    index = oneArg(x, x.indexes)
    inferValueType(index, env, isIntType)
    return True, exprType.type


# begin inferNamedIndexExprType

inferNamedIndexExprType = multimethod()

@inferNamedIndexExprType.register(ArrayTypeEntry)
def foo(entry, x, env) :
    typeParam, size = nTypeParams(2, x, x.indexes)
    typeParam = inferTypeType(typeParam, env)
    return False, ArrayType(typeParam, intLiteralValue(size))

@inferNamedIndexExprType.register(PointerTypeEntry)
def foo(entry, x, env) :
    typeParam = oneTypeParam(x, x.indexes)
    return False, PointerType(inferTypeType(typeParam, env))

@inferNamedIndexExprType.register(RecordEntry)
def foo(entry, x, env) :
    n = len(entry.ast.typeVars)
    typeParams = nTypeParams(n, x, x.indexes)
    typeParams = [inferTypeType(y, env) for y in typeParams]
    return False, RecordType(entry, typeParams)

# end inferNamedIndexExprType


@inferType.register(CallExpr)
def foo(x, env) :
    if type(x.expr) is NameRef :
        entry = lookupIdent(env, x.expr.name)
        handler = inferNamedCallExprType.getHandler(type(entry))
        if handler is not None :
            return handler(entry, x, env)
    return inferIndirectCallType(x, env)

def inferIndirectCallType(x, env) :
    typeType = inferTypeType(x.expr, env, isRecordType)
    fieldTypes = getFieldTypes(typeType)
    if len(fieldTypes) != len(x.args) :
        raiseError("incorrect no. of arguments", x)
    for fieldType, arg in zip(fieldTypes, x.args) :
        argType = inferValueType(arg, env)
        if not typeEquals(fieldType, argType) :
            raiseError("type mismatch", arg)
    return True, typeType


# begin inferNamedCallExprType

inferNamedCallExprType = multimethod()

@inferNamedCallExprType.register(PrimOpDefault)
def foo(entry, x, env) :
    arg = oneArg(x, x.args)
    argType = inferTypeType(arg, env)
    return True, argType

@inferNamedCallExprType.register(PrimOpPointerOffset)
def foo(entry, x, env) :
    args = nArgs(2, x, x.args)
    type1 = inferValueType(args[0], env, isPointerType)
    type2 = inferValueType(args[1], env, isIntType)
    return True, type1

@inferNamedCallExprType.register(PrimOpPointerDifference)
def foo(entry, x, env) :
    args = nArgs(2, x, x.args)
    type1 = inferValueType(args[0], env, isPointerType)
    type2 = inferValueType(args[1], env, isPointerType)
    if not typeEquals(type1.type, type2.type) :
        raiseError("type mismatch", args[1])
    return True, intType

@inferNamedCallExprType.register(PrimOpPointerCast)
def foo(entry, x, env) :
    args = nArgs(2, x, x.args)
    destType = inferTypeType(args[0], env)
    exprType = inferValueType(args[0], env, isPointerType)
    return True, PointerType(destType)

def inferPointerComparison(entry, x, env) :
    args = nArgs(2, x, x.args)
    pType1 = inferValueType(args[0], env, isPointerType)
    pType2 = inferValueType(args[1], env, isPointerType)
    if not typeEquals(pType1, pType2) :
        raiseError("type mismatch", args[1])
    return True, boolType

def initInferPointerComparisons() :
    inferer = inferPointerComparison
    for primOp in (PrimOpPointerEquals, PrimOpPointerLesser,
                   PrimOpPointerLesserEquals, PrimOpPointerGreater,
                   PrimOpPointerGreaterEquals) :
        inferNamedCallExprType.addHandler(inferer, primOp)

initInferPointerComparisons()

@inferNamedCallExprType.register(PrimOpAllocate)
def foo(entry, x, env) :
    arg = oneArg(x, x.args)
    elementType = inferTypeType(arg, env)
    return True, PointerType(elementType)

@inferNamedCallExprType.register(PrimOpFree)
def foo(entry, x, env) :
    arg = oneArg(x, x.args)
    vType = inferValueType(arg, env, isPointerType)
    return True, voidType

@inferNamedCallExprType.register(PrimOpAllocateBlock)
def foo(entry, x, env) :
    args = nArgs(2, x, x.args)
    elementType = inferTypeType(args[0], env)
    sizeType = inferValueType(args[1], env, isIntType)
    return True, PointerType(elementType)

@inferNamedCallExprType.register(PrimOpFreeBlock)
def foo(entry, x, env) :
    arg = oneArg(x, x.args)
    vType = inferValueType(arg, env, isPointerType)
    return True, voidType

@inferNamedCallExprType.register(PrimOpBlockSize)
def foo(entry, x, env) :
    arg = oneArg(x, x.args)
    vType = inferValueType(arg, env, isPointerType)
    return True, intType

@inferNamedCallExprType.register(PrimOpArray)
def foo(entry, x, env) :
    args = nArgs(2, x, x.args)
    n = intLiteralValue(args[0])
    vType = inferValueType(args[1], env)
    return True, ArrayType(vType, n)

@inferNamedCallExprType.register(PrimOpArraySize)
def foo(entry, x, env) :
    arg = oneArg(x, x.args)
    inferValueType(arg, env, isArrayType)
    return True, intType

@inferNamedCallExprType.register(PrimOpBoolNot)
def foo(entry, x, env) :
    arg = oneArg(x, x.args)
    inferValueType(arg, env, isBoolType)
    return True, boolType

def inferCharComparison(entry, x, env) :
    args = nArgs(2, x, x.args)
    inferValueType(args[0], env, isCharType)
    inferValueType(args[1], env, isCharType)
    return True, boolType

inferNamedCallExprType.addHandler(inferCharComparison, PrimOpCharEquals)
inferNamedCallExprType.addHandler(inferCharComparison, PrimOpCharLesser)
inferNamedCallExprType.addHandler(inferCharComparison, PrimOpCharLesserEquals)
inferNamedCallExprType.addHandler(inferCharComparison, PrimOpCharGreater)
inferNamedCallExprType.addHandler(inferCharComparison, PrimOpCharGreaterEquals)

def inferIntBinaryOp(entry, x, env) :
    args = nArgs(2, x, x.args)
    inferValueType(args[0], env, isIntType)
    inferValueType(args[1], env, isIntType)
    return True, intType

inferNamedCallExprType.addHandler(inferIntBinaryOp, PrimOpIntAdd)
inferNamedCallExprType.addHandler(inferIntBinaryOp, PrimOpIntSubtract)
inferNamedCallExprType.addHandler(inferIntBinaryOp, PrimOpIntMultiply)
inferNamedCallExprType.addHandler(inferIntBinaryOp, PrimOpIntDivide)
inferNamedCallExprType.addHandler(inferIntBinaryOp, PrimOpIntModulus)

@inferNamedCallExprType.register(PrimOpIntNegate)
def foo(entry, x, env) :
    arg = oneArg(x, x.args)
    inferValueType(arg, env, isIntType)
    return True, intType

def inferIntComparison(entry, x, env) :
    args = nArgs(2, x, x.args)
    inferValueType(args[0], env, isIntType)
    inferValueType(args[1], env, isIntType)
    return True, boolType

inferNamedCallExprType.addHandler(inferIntComparison, PrimOpIntEquals)
inferNamedCallExprType.addHandler(inferIntComparison, PrimOpIntLesser)
inferNamedCallExprType.addHandler(inferIntComparison, PrimOpIntLesserEquals)
inferNamedCallExprType.addHandler(inferIntComparison, PrimOpIntGreater)
inferNamedCallExprType.addHandler(inferIntComparison, PrimOpIntGreaterEquals)

def inferFloatBinaryOp(entry, x, env) :
    args = nArgs(2, x, x.args)
    inferValueType(args[0], env, isFloatType)
    inferValueType(args[1], env, isFloatType)
    return True, floatType

inferNamedCallExprType.addHandler(inferFloatBinaryOp, PrimOpFloatAdd)
inferNamedCallExprType.addHandler(inferFloatBinaryOp, PrimOpFloatSubtract)
inferNamedCallExprType.addHandler(inferFloatBinaryOp, PrimOpFloatMultiply)
inferNamedCallExprType.addHandler(inferFloatBinaryOp, PrimOpFloatDivide)

@inferNamedCallExprType.register(PrimOpFloatNegate)
def foo(entry, x, env) :
    arg = oneArg(x, x.args)
    inferValueType(arg, env, isFloatType)
    return True, floatType

def inferFloatComparison(entry, x, env) :
    args = nArgs(2, x, x.args)
    inferValueType(args[0], env, isFloatType)
    inferValueType(args[1], env, isFloatType)
    return True, boolType

inferNamedCallExprType.addHandler(inferFloatComparison,
                                  PrimOpFloatEquals)
inferNamedCallExprType.addHandler(inferFloatComparison,
                                  PrimOpFloatLesser)
inferNamedCallExprType.addHandler(inferFloatComparison,
                                  PrimOpFloatLesserEquals)
inferNamedCallExprType.addHandler(inferFloatComparison,
                                  PrimOpFloatGreater)
inferNamedCallExprType.addHandler(inferFloatComparison,
                                  PrimOpFloatGreaterEquals)

def inferDoubleBinaryOp(entry, x, env) :
    args = nArgs(2, x, x.args)
    inferValueType(args[0], env, isDoubleType)
    inferValueType(args[1], env, isDoubleType)
    return True, doubleType

inferNamedCallExprType.addHandler(inferDoubleBinaryOp, PrimOpDoubleAdd)
inferNamedCallExprType.addHandler(inferDoubleBinaryOp, PrimOpDoubleSubtract)
inferNamedCallExprType.addHandler(inferDoubleBinaryOp, PrimOpDoubleMultiply)
inferNamedCallExprType.addHandler(inferDoubleBinaryOp, PrimOpDoubleDivide)

@inferNamedCallExprType.register(PrimOpDoubleNegate)
def foo(entry, x, env) :
    arg = oneArg(x, x.args)
    inferValueType(arg, env, isDoubleType)
    return True, doubleType

def inferDoubleComparison(entry, x, env) :
    args = nArgs(2, x, x.args)
    inferValueType(args[0], env, isDoubleType)
    inferValueType(args[1], env, isDoubleType)
    return True, boolType

inferNamedCallExprType.addHandler(inferDoubleComparison,
                                  PrimOpDoubleEquals)
inferNamedCallExprType.addHandler(inferDoubleComparison,
                                  PrimOpDoubleLesser)
inferNamedCallExprType.addHandler(inferDoubleComparison,
                                  PrimOpDoubleLesserEquals)
inferNamedCallExprType.addHandler(inferDoubleComparison,
                                  PrimOpDoubleGreater)
inferNamedCallExprType.addHandler(inferDoubleComparison,
                                  PrimOpDoubleGreaterEquals)

def conversionInferer(srcPredicate, destType) :
    def foo(entry, x, env) :
        arg = oneArg(x, x.args)
        inferValueType(arg, env, srcPredicate)
        return True, destType
    return foo

inferNamedCallExprType.addHandler(conversionInferer(isCharType, intType),
                                  PrimOpCharToInt)
inferNamedCallExprType.addHandler(conversionInferer(isIntType, charType),
                                  PrimOpIntToChar)
inferNamedCallExprType.addHandler(conversionInferer(isFloatType, intType),
                                  PrimOpFloatToInt)
inferNamedCallExprType.addHandler(conversionInferer(isIntType, floatType),
                                  PrimOpIntToFloat)
inferNamedCallExprType.addHandler(conversionInferer(isFloatType, doubleType),
                                  PrimOpFloatToDouble)
inferNamedCallExprType.addHandler(conversionInferer(isDoubleType, floatType),
                                  PrimOpDoubleToFloat)
inferNamedCallExprType.addHandler(conversionInferer(isDoubleType, intType),
                                  PrimOpDoubleToInt)
inferNamedCallExprType.addHandler(conversionInferer(isIntType, doubleType),
                                  PrimOpIntToDouble)

@inferNamedCallExprType.register(RecordEntry)
def foo(entry, x, env) :
    if len(entry.ast.fields) != len(x.args) :
        raiseError("incorrect no. of arguments", x)
    env2 = Env(entry.env)
    typeVarEntries = bindTypeVariables(env2, entry.ast.typeVars)
    for field, arg in zip(entry.ast.fields, x.args) :
        fieldType = inferTypeType(field.type, env2)
        argType = inferValueType(arg, env)
        if not typeUnify(fieldType, argType) :
            raiseError("type mismatch", arg)
    reduceTypeVariables(typeVarEntries)
    typeParams = [tvarEntry.type for tvarEntry in typeVarEntries]
    return True, RecordType(entry, typeParams)

@inferNamedCallExprType.register(ProcedureEntry)
def foo(entry, x, env) :
    argsInfo = tuple([inferType(y, env) for y in x.args])
    returnType = entry.returnTypes.get(argsInfo)
    if returnType is False :
        raise RecursiveInferenceError()
    if returnType is not None :
        return True, returnType
    entry.returnTypes[argsInfo] = False
    returnType = None
    try :
        proc = entry.ast.procedure
        result = inferProcedureType(entry.env, proc, argsInfo, x)
        if type(result) is ProcedureMismatch :
            raiseError(result.errorMessage, result.astNode)
        returnType = result
        return True, returnType
    finally :
        if returnType is None :
            del entry.returnTypes[argsInfo]
        else :
            entry.returnTypes[argsInfo] = returnType

@inferNamedCallExprType.register(OverloadableEntry)
def foo(entry, x, env) :
    argsInfo = tuple([inferType(y, env) for y in x.args])
    returnType = entry.returnTypes.get(argsInfo)
    if returnType is False :
        raise RecursiveInferenceError()
    if returnType is not None :
        return True, returnType
    entry.returnTypes[argsInfo] = False
    returnType = None
    try :
        for overload in entry.overloads :
            proc = overload.ast.procedure
            result = inferProcedureType(overload.env, proc, argsInfo, x)
            if type(result) is not ProcedureMismatch :
                returnType = result
                return True, returnType
        raiseError("no matching overload", x)
    finally :
        if returnType is None :
            del entry.returnTypes[argsInfo]
        else :
            entry.returnTypes[argsInfo] = returnType

class ProcedureMismatch(object) :
    def __init__(self, errorMessage, astNode) :
        self.errorMessage = errorMessage
        self.astNode = astNode

def inferProcedureType(env, ast, argsInfo, callExpr) :
    def mismatch(errorMessage, astNode) :
        return ProcedureMismatch(errorMessage, astNode)
    if len(ast.args) != len(argsInfo) :
        return mismatch("incorrect no. of arguments", callExpr)
    env = Env(env)
    typeVarEntries = bindTypeVariables(env, ast.typeVars)
    for i, formalArg in enumerate(ast.args) :
        isValue, argType = argsInfo[i]
        formalTypeExpr = None
        if type(formalArg) is ValueArgument :
            if not isValue :
                return mismatch("expected a value", callExpr.args[i])
            formalTypeExpr = formalArg.variable.type
        elif type(formalArg) is TypeArgument :
            if isValue :
                return mismatch("expected a type", callExpr.args[i])
            formalTypeExpr = formalArg.type
        else :
            assert False
        if formalTypeExpr is not None :
            formalType = inferTypeType(formalTypeExpr, env)
            if not typeUnify(formalType, argType) :
                return mismatch("type mismatch", callExpr.args[i])
    for typeCondition in ast.typeConditions :
        if not evalTypeCondition(env, typeVarEntries, typeCondition) :
            return mismatch("type condition failed", typeCondition)
    reduceTypeVariables(typeVarEntries)
    if ast.returnType is not None :
        return inferTypeType(ast.returnType, env)
    for i, formalArg in enumerate(ast.args) :
        isValue, argType = argsInfo[i]
        if type(formalArg) is ValueArgument :
            argName = formalArg.variable.name
            localVarEntry = LocalVariableEntry(env, argName, argType)
            addIdent(env, argName, localVarEntry)
    returnType = inferReturnType(ast.body, env)
    if returnType is None :
        raise RecursiveInferenceError()
    return returnType

# end inferNamedCallExprType


@inferType.register(FieldRef)
def foo(x, env) :
    exprType = inferValueType(x.expr, env, isRecordType)
    ast = exprType.entry.ast
    for field, fieldType in zip(ast.fields, getFieldTypes(exprType)) :
        if field.name.s == x.name.s :
            return True, fieldType
    raiseError("field not found", x)

@inferType.register(TupleRef)
def foo(x, env) :
    tupleType = inferValueType(x.expr, env, isTupleType)
    if (x.index < 0) or (x.index >= len(tupleType.types)) :
        raiseError("invalid tuple index", x)
    return True, tupleType.types[x.index]

@inferType.register(Dereference)
def foo(x, env) :
    exprType = inferValueType(x.expr, env, isPointerType)
    return True, exprType.type

@inferType.register(ArrayExpr)
def foo(x, env) :
    elementType = None
    for y in x.elements :
        currentType = inferValueType(y, env)
        if elementType is None :
            elementType = currentType
        elif not typeEquals(elementType, currentType) :
            raiseError("type mismatch", y)
    if elementType is None :
        raiseError("empty array expression", x)
    return True, ArrayType(elementType, len(x.elements))

@inferType.register(TupleExpr)
def foo(x, env) :
    elementTypes = [inferValueType(y, env) for y in x.elements]
    if len(elementTypes) == 0 :
        raiseError("empty tuple expression", x)
    if len(elementTypes) == 1 :
        return True, elementTypes[0]
    return True, TupleType(elementTypes)

@inferType.register(NameRef)
def foo(x, env) :
    entry = lookupIdent(env, x.name)
    handler = inferNamedExprType.getHandler(type(entry))
    if handler is not None :
        return handler(entry, x, env)
    raiseError("invalid expression", x)


# begin inferNamedExprType

inferNamedExprType = multimethod()

@inferNamedExprType.register(BoolTypeEntry)
def foo(entry, x, env) :
    return False, boolType

@inferNamedExprType.register(CharTypeEntry)
def foo(entry, x, env) :
    return False, charType

@inferNamedExprType.register(IntTypeEntry)
def foo(entry, x, env) :
    return False, intType

@inferNamedExprType.register(FloatTypeEntry)
def foo(entry, x, env) :
    return False, floatType

@inferNamedExprType.register(DoubleTypeEntry)
def foo(entry, x, env) :
    return False, doubleType

@inferNamedExprType.register(VoidTypeEntry)
def foo(entry, x, env) :
    return False, voidType

@inferNamedExprType.register(RecordEntry)
def foo(entry, x, env) :
    if len(entry.ast.typeVars) > 0 :
        raiseError("record type requires type parameters", x)
    return False, RecordType(entry, [])

@inferNamedExprType.register(VariableEntry)
def foo(entry, x, env) :
    if entry.type is False :
        raise RecursiveInferenceError()
    if entry.type is not None :
        return True, entry.type
    entry.type = False
    try :
        exprType = inferValueType(entry.ast.expr, entry.env)
        if entry.ast.variable.type is not None :
            declaredType = inferTypeType(entry.ast.variable.type, entry.env)
            if not typeEquals(exprType, declaredType) :
                raiseError("type mismatch", entry.ast.expr)
        entry.type = exprType
        return True, exprType
    finally :
        if entry.type is False :
            entry.type = None

@inferNamedExprType.register(TypeVarEntry)
def foo(entry, x, env) :
    return False, entry.type

@inferNamedExprType.register(LocalVariableEntry)
def foo(entry, x, env) :
    return True, entry.type

# end inferNamedExprType


@inferType.register(BoolLiteral)
def foo(x, env) :
    return True, boolType

@inferType.register(IntLiteral)
def foo(x, env) :
    return True, intType

@inferType.register(FloatLiteral)
def foo(x, env) :
    return True, floatType

@inferType.register(DoubleLiteral)
def foo(x, env) :
    return True, doubleType

@inferType.register(CharLiteral)
def foo(x, env) :
    return True, charType

@inferType.register(StringLiteral)
def foo(x, env) :
    raiseError("strings are not supported yet", x)



#
# inferReturnType (statement, env) -> optional(Type)
#

def inferReturnType(body, env) :
    collector = ReturnTypeCollector()
    collectReturns(body, env, collector)
    if collector.returnType is not None :
        return collector.returnType
    if not collector.hasFailure :
        return voidType
    return None

class ReturnTypeCollector(object) :
    def __init__(self) :
        self.returnType = None
        self.hasFailure = False

    def failure(self) :
        self.hasFailure = True

    def success(self, returnType) :
        if self.returnType is not None :
            return typeEquals(returnType, self.returnType)
        self.returnType = returnType
        return True

collectReturns = multimethod()

@collectReturns.register(Block)
def foo(x, env, collector) :
    for y in x.statements :
        if type(y) is LocalVariableDef :
            if env is None :
                continue
            try :
                exprType = inferValueType(y.expr, env)
                if y.variable.type is not None :
                    declaredType = inferTypeType(y.variable.type, env)
                    if not typeEquals(exprType, declaredType) :
                        raiseError("type mismatch", y.expr)
                localVar = LocalVariableEntry(env, y.variable, exprType)
                addIdent(env, y.variable.name, localVar)
            except RecursiveInferenceError :
                env = None
        else :
            collectReturns(y, env, collector)

@collectReturns.register(LocalVariableDef)
def foo(x, env, collector) :
    raiseError("invalid local variable definition", x)

@collectReturns.register(Assignment)
def foo(x, env, collector) :
    pass

@collectReturns.register(IfStatement)
def foo(x, env, collector) :
    collectReturns(x.thenPart, env, collector)
    if x.elsePart is not None :
        collectReturns(x.elsePart, env, collector)

@collectReturns.register(BreakStatement)
def foo(x, env, collector) :
    pass

@collectReturns.register(ContinueStatement)
def foo(x, env, collector) :
    pass

@collectReturns.register(WhileStatement)
def foo(x, env, collector) :
    collectReturns(x.body, env, collector)

@collectReturns.register(ForStatement)
def foo(x, env, collector) :
    if env is not None :
        try :
            exprType = inferValueType(x.expr, env, isArrayType)
            itemType = exprType.type
            if x.variable.type is not None :
                declaredType = inferTypeType(x.variable.type, env)
                if not typeEquals(itemType, declaredType) :
                    raiseError("type mismatch", x.expr)
            localVar = LocalVariableEntry(env, x.variable, itemType)
            addIdent(env, x.variable.name, localVar)
        except RecursiveInferenceError :
            env = None
    collectReturns(x.body, env, collector)

@collectReturns.register(ReturnStatement)
def foo(x, env, collector) :
    if x.expr is None :
        returnType = voidType
    elif env is None :
        returnType = None
    else :
        try :
            returnType = inferValueType(x.expr, env)
        except RecursiveInferenceError :
            returnType = None
    if returnType is None :
        collector.failure()
    else :
        if not collector.success(returnType) :
            raiseError("type mismatch", x)

@collectReturns.register(ExprStatement)
def foo(x, env, collector) :
    pass



#
# remove temp name used for multimethod instances
#

del foo
