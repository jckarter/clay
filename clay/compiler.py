from clay.ast import *
from clay.env import *
from clay.types import *
from clay.error import *
from clay.multimethod import multimethod



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
# primitivesEnv
#

def primitivesEnv() :
    env = Env()
    def a(name, klass) :
        env.add(name, klass(env,None))
    a("Bool", BoolTypeEntry)
    a("Char", CharTypeEntry)
    a("Int", IntTypeEntry)
    a("Void", VoidTypeEntry)
    a("Array", ArrayTypeEntry)
    a("Ref", RefTypeEntry)

    a("default", PrimOpDefault)
    a("array", PrimOpArray)
    a("arraySize", PrimOpArraySize)
    a("arrayValue", PrimOpArrayValue)

    a("boolNot", PrimOpBoolNot)
    a("charToInt", PrimOpCharToInt)
    a("intToChar", PrimOpIntToChar)
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
    return env



#
# buildTopLevelEnv
#

def buildTopLevelEnv(program) :
    env = Env(primitivesEnv())
    for item in program.topLevelItems :
        addTopLevel(item, env)

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

@addTopLevel.register(StructDef)
def foo(x, env) :
    addIdent(env, x.name, StructEntry(env,x))

@addTopLevel.register(VariableDef)
def foo(x, env) :
    defEntry = VariableDefEntry(env, x)
    for i,variable in enumerate(x.variables) :
        addIdent(env, variable.name, VariableEntry(env,defEntry,i))

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
# error checking getters
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
    if len(memberList) !+ n :
        raiseError("exactly %d argument(s) expected" % n, container)
    return memberList

def intLiteralValue(x) :
    if type(x) is not IntLiteral :
        raiseError("int literal expected", x)
    return x.value



#
# inferType : (expr, env) -> (isValue, type)
#             throws RecursiveInferenceError
#

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
    if not isValue :
        raiseError("type expected", x)
    if verify is not None :
        if not verify(resultType) :
            raiseError("invalid type", x)
    return resultType

@inferType.register(AddressOfExpr)
def foo(x, env) :
    return True, RefType(inferValueType(x.expr, env))

@inferType.register(IndexExpr)
def foo(x, env) :
    if type(x.expr) is NameRef :
        entry = lookupIdent(env, x.expr.name)
        handler = inferNamedIndexExpr.getHandler(type(entry))
        if handler is not None :
            return handler(entry, x, env)
    return inferArrayIndexing(x, env)

def inferArrayIndexing(x, env) :
    def isIndexable(t) :
        return isArrayType(t) or isArrayValueType(t)
    indexableType = inferValueType(x.expr, env, isIndexable)
    index = oneArg(x, x.indexes)
    indexType = inferValueType(index, env, isIntType)
    return True, indexableType.type


# begin inferNamedIndexExpr

inferNamedIndexExpr = multimethod()

@inferNamedIndexExpr.register(ArrayTypeEntry)
def foo(entry, x, env) :
    typeParam = oneTypeParam(x, x.indexes)
    return False, ArrayType(inferTypeType(typeParam, env))

@inferNamedIndexExpr.register(ArrayValueTypeEntry)
def foo(entry, x, env) :
    typeParam, size = nTypeParams(2, x, x.indexes)
    typeParam = inferTypeType(typeParam, env)
    return False, ArrayValueType(typeParam, intLiteralValue(size))

@inferNamedIndexExpr.register(RefTypeEntry)
def foo(entry, x, env) :
    typeParam = oneTypeParam(x, x.indexes)
    return False, RefType(inferTypeType(typeParam, env))

@inferNamedIndexExpr.register(RecordEntry)
def foo(entry, x, env) :
    assert False

@inferNamedIndexExpr.register(StructEntry)
def foo(entry, x, env) :
    assert False

# end inferNamedIndexExpr


@inferType.register(CallExpr)
def foo(x, env) :
    if type(x.expr) is NameRef :
        entry = lookupIdent(env, x.expr.name)
        handler = inferNamedCallExpr.getHandler(type(entry))
        if handler is not None :
            return handler(entry, x, env)
    return inferIndirectCall(x, env)

def insertIndirectCall(x, env) :
    def isCallable(t) :
        return isRecordType(t) or isStructType(t)
    callableType = inferTypeType(x.expr, env, isCallable)
    assert False


# begin inferNamedCallExpr

inferNamedCallExpr = multimethod()

@inferNamedCallExpr.register(PrimOpDefault)
def foo(entry, x, env) :
    arg = oneArg(x, x.args)
    argType = inferTypeType(arg, env)
    return True, argType

@inferNamedCallExpr.register(PrimOpArray)
def foo(entry, x, env) :
    args = nArgs(2, x, x.args)
    nType = inferValueType(args[0], env, isIntType)
    vType = inferValueType(args[1], env)
    return True, ArrayType(vType)

@inferNamedCallExpr.register(PrimOpArraySize)
def foo(entry, x, env) :
    arg = oneArg(x, x.args)
    arrayType = inferValueType(arg, env, isArrayType)
    return True, intType

@inferNamedCallExpr.register(PrimOpArrayValue)
def foo(entry, x, env) :
    args = nArgs(2, x, x.args)
    n = intLiteralValue(args[0])
    vType = inferValueType(args[1], env)
    return True, ArrayValueType(vType, n)

@inferNamedCallExpr.register(PrimOpBoolNot)
def foo(entry, x, env) :
    arg = oneArg(x, x.args)
    argType = inferValueType(arg, env, isBoolType)
    return True, boolType

@inferNamedCallExpr.register(PrimOpCharToInt)
def foo(entry, x, env) :
    arg = oneArg(x, x.args)
    argType = inferValueType(arg, env, isCharType)
    return True, intType

@inferNamedCallExpr.register(PrimOpIntToChar)
def foo(entry, x, env) :
    arg = oneArg(x, x.args)
    argType = inferValueType(arg, env, isIntType)
    return True, charType

def inferCharComparison(entry, x, env) :
    args = nArgs(2, x, x.args)
    arg1Type = inferValueType(args[0], env, isCharType)
    arg2Type = inferValueType(args[1], env, isCharType)
    return True, boolType

inferNamedCallExpr.addHandler(inferCharComparison, PrimOpCharEquals)
inferNamedCallExpr.addHandler(inferCharComparison, PrimOpCharLesser)
inferNamedCallExpr.addHandler(inferCharComparison, PrimOpCharLesserEquals)
inferNamedCallExpr.addHandler(inferCharComparison, PrimOpCharGreater)
inferNamedCallExpr.addHandler(inferCharComparison, PrimOpCharGreaterEquals)

def inferIntBinaryOp(entry, x, env) :
    args = nArgs(2, x, x.args)
    arg1Type = inferValueType(args[0], env, isIntType)
    arg2Type = inferValueType(args[1], env, isIntType)
    return True, intType

inferNamedCallExpr.addHandler(inferIntBinaryOp, PrimOpIntAdd)
inferNamedCallExpr.addHandler(inferIntBinaryOp, PrimOpIntSubtract)
inferNamedCallExpr.addHandler(inferIntBinaryOp, PrimOpIntMultiply)
inferNamedCallExpr.addHandler(inferIntBinaryOp, PrimOpIntDivide)
inferNamedCallExpr.addHandler(inferIntBinaryOp, PrimOpIntModulus)

@inferNamedCallExpr.register(PrimOpIntNegate)
def foo(entry, x, env) :
    arg = oneArg(x, x.args)
    argType = inferValueType(arg, env, isIntType)
    return True, intType

def inferIntComparison(entry, x, env) :
    args = nArgs(2, x, x.args)
    arg1Type = inferValueType(args[0], env, isIntType)
    arg2Type = inferValueType(args[1], env, isIntType)
    return True, boolType

inferNamedCallExpr.addHandler(inferIntComparison, PrimOpIntEquals)
inferNamedCallExpr.addHandler(inferIntComparison, PrimOpIntLesser)
inferNamedCallExpr.addHandler(inferIntComparison, PrimOpIntLesserEquals)
inferNamedCallExpr.addHandler(inferIntComparison, PrimOpIntGreater)
inferNamedCallExpr.addHandler(inferIntComparison, PrimOpIntGreaterEquals)

@inferNamedCallExpr.register(RecordEntry)
def foo(entry, x, env) :
    assert False

@inferNamedCallExpr.register(StructEntry)
def foo(entry, x, env) :
    assert False

@inferNamedCallExpr.register(ProcedureEntry)
def foo(entry, x, env) :
    assert False

@inferNamedCallExpr.register(OverloadableEntry)
def foo(entry, x, env) :
    assert False

@inferNamedCallExpr.register(TypeVarEntry)
def foo(entry, x, env) :
    assert False

# end inferNamedCallExpr




#
# remove temp name used for multimethod instances
#

del foo
