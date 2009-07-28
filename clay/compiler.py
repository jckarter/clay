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
def f(x, env) :
    addIdent(env, x.name, PredicateEntry(env,x))

@addTopLevel.register(InstanceDef)
def f(x, env) :
    entry = InstanceEntry(env, x)
    lookupPredicate(env, x.name).instances.append(entry)

@addTopLevel.register(RecordDef)
def f(x, env) :
    addIdent(env, x.name, RecordEntry(env,x))

@addTopLevel.register(StructDef)
def f(x, env) :
    addIdent(env, x.name, StructEntry(env,x))

@addTopLevel.register(VariableDef)
def f(x, env) :
    defEntry = VariableDefEntry(env, x)
    for i,variable in enumerate(x.variables) :
        addIdent(env, variable.name, VariableEntry(env,defEntry,i))

@addTopLevel.register(ProcedureDef)
def f(x, env) :
    addIdent(env, x.name, ProcedureEntry(env,x))

@addTopLevel.register(OverloadableDef)
def f(x, env) :
    addIdent(env, x.name, OverloadableEntry(env,x))

@addTopLevel.register(OverloadDef)
def f(x, env) :
    entry = OverloadEntry(env, x)
    lookupOverloadable(env, x.name).overloads.append(entry)



#
# compileExpr : (expr, env) -> (isValue, type)
#

compileExpr = multimethod()

def compileExprAsValue(x, env) :
    isValue,resultType = compileExpr(x, env)
    if not isValue :
        raiseError("value expected", x)
    return resultType

def compileExprAsType(x, env) :
    isValue,resultType = compileExpr(x, env)
    if isValue :
        raiseError("type expected", x)
    return resultType

def takeOne(container, memberList, kind) :
    if len(memberList) > 1 :
        raiseError("only one %s allowed" % kind, memberList[1])
    if len(memberList) < 1 :
        raiseError("missing %s" % kind, container)
    return memberList[0]

def takeN(n, container, memberList, kind) :
    if n == len(memberList) :
        return memberList
    if len(memberList) == 0 :
        raiseError("missing %s" % kind, container)
    raiseError("incorrect number of %s" % kind, container)

@compileExpr.register(AddressOfExpr)
def f(x, env) :
    return True, RefType(compileExprAsValue(x.expr, env))

@compileExpr.register(IndexExpr)
def f(x, env) :
    if type(x.expr) is NameRef :
        entry = lookupIdent(env, x.expr.name)
        result = compileNamedIndexExpr(entry, x, env)
        if result is not False :
            return result
    indexableType = compileExprAsValue(x.expr, env)
    if isArrayType(indexableType) :
        return True, indexableType.type
    elif isArrayValueType(indexableType) :
        return True, indexableType.type
    else :
        raiseError("array type expected", x.expr)



# begin compileNamedIndexExpr

compileNamedIndexExpr = multimethod(default=False)

@compileNamedIndexExpr.register(ArrayTypeEntry)
def f(entry, x, env) :
    typeParam = takeOne(x, x.indexes, "type parameter")
    return False, ArrayType(compileExprAsType(typeParam, env))

@compileNamedIndexExpr.register(ArrayValueTypeEntry)
def f(entry, x, env) :
    typeParam,size = takeN(2, x, x.indexes, "type parameters")
    if type(size) is not IntLiteral :
        raiseError("int literal expected", size)
    if size.value < 0 :
        raiseError("array size cannot be negative", size)
    y = compileExprAsType(typeParam, env)
    return False, ArrayValueType(y, size.value)

@compileNamedIndexExpr.register(RefTypeEntry)
def f(entry, x, env) :
    typeParam = takeOne(x, x.indexes, "type parameter")
    return False, RefType(compileExprAsType(typeParam, env))

@compileNamedIndexExpr.register(RecordEntry)
def f(entry, x, env) :
    nTypeVars = len(entry.ast.typeVars)
    typeParams = takeN(nTypeVars, x, x.indexes, "type parameters")
    typeParams = [compileExprAsType(y,env) for y in typeParams]
    return False, RecordType(entry, typeParams)

@compileNamedIndexExpr.register(StructEntry)
def f(entry, x, env) :
    nTypeVars = len(entry.ast.typeVars)
    typeParams = takeN(nTypeVars, x, x.indexes, "type parameters")
    typeParams = [compileExprAsType(y,env) for y in typeParams]
    return False, StructType(entry, typeParams)

# end compileNamedIndexExpr


@compileExpr.register(CallExpr)
def f(x, env) :
    if type(x.expr) is NameRef :
        entry = lookupIdent(env, x.expr.name)
        result = compileNamedCallExpr(entry, x, env)
        if result is not False :
            return result
    isValue,callableType = compileExpr(x.expr, env)
    if isValue :
        raiseError("invalid call", x)
    if isRecordType(callableType) or isStructType(callableType) :
        fieldTypes = computeFieldTypes(callableType)
        if len(x.args) != len(fieldTypes) :
            raiseError("incorrect number of arguments", x)
        for arg, fieldType in zip(x.args, fieldTypes) :
            argType = compileExprAsValue(arg, env)
            if not typeEquals(argType, fieldType) :
                raiseError("type mismatch in argument", arg)
        return callableType
    else :
        raiseError("invalid call", x)

def computeFieldTypes(t) :
    assert isRecordType(t) or isStructType(t)
    ast = t.entry.ast
    env = Env(t.entry.env)
    assert len(ast.typeVars) == len(t.typeParams)
    for tvar, tparam in zip(ast.typeVars, t.typeParams) :
        addIdent(env, tvar, tparam)
    return [compileExprAsType(field.type, env) for field in ast.fields]



# begin compileNamedCallExpr

compileNamedCallExpr = multimethod(default=False)

@compileNamedCallExpr.register(PrimOpDefault)
def f(entry, x, env) :
    argType = compileExprAsType(takeOne(x,x.args,"argument"), env)
    return True, argType

@compileNamedCallExpr.register(PrimOpArray)
def f(entry, x, env) :
    if len(x.args) == 1 :
        elementType = compileExprAsType(x.args[0], env)
        return True, ArrayType(elementType)
    elif len(x.args) == 2 :
        isValue, arg1Type = compileExpr(x.args[0], env)
        arg2Type = compileExprAsValue(x.args[1], env)
        if isValue :
            if not isIntType(arg1Type) :
                raiseError("Int type expected", x.args[0])
            return True, ArrayType(arg2Type)
        else :
            if not isIntType(arg2Type) :
                raiseError("Int type expected", x.args[1])
            return True, ArrayType(arg1Type)
    else :
        raiseError("incorrect number of arguments", x)

@compileNamedCallExpr.register(PrimOpArraySize)
def f(entry, x, env) :
    arg = takeOne(x, x.args, "argument")
    argType = compileExprAsValue(arg, env)
    if not isArrayType(argType) :
        raiseError("Array type expected", arg)
    return True, intType

@compileNamedCallExpr.register(PrimOpArrayValue)
def f(entry, x, env) :
    args = takeN(2, x, x.args, "arguments")
    if type(args[0]) is IntLiteral :
        size = args[0].value
        if size < 0 :
            raiseError("invalid array size", args[0])
        elementType = compileExprAsValue(args[1], env)
        return True, ArrayValueType(elementType, size)
    else :
        arg1Type = compileExprAsType(args[0], env)
        if type(args[1]) is not IntLiteral :
            raiseError("Int literal expected", args[1])
        size = args[1].value
        if size < 0 :
            raiseError("invalid array size", args[1])
        return True, ArrayValueType(arg1Type, size)

@compileNamedCallExpr.register(PrimOpBoolNot)
def f(entry, x, env) :
    arg = takeOne(x, x.xargs, "argument")
    argType = compileExprAsValue(arg, env)
    if not isBoolType(argType) :
        raiseError("Bool type expected", arg)
    return True, boolType

@compileNamedCallExpr.register(PrimOpCharToInt)
def f(entry, x, env) :
    arg = takeOne(x, x.args, "argument")
    argType = compileExprAsValue(arg, env)
    if not isCharType(argType) :
        raiseError("Char type expected", arg)
    return True, intType

@compileNamedCallExpr.register(PrimOpIntToChar)
def f(entry, x, env) :
    arg = takeOne(x, x.args, "argument")
    argType = compileExprAsValue(arg, env)
    if not isIntType(argType) :
        raiseError("Int type expected", arg)
    return True, charType

@compileNamedCallExpr.register(PrimOpCharEquals)
def f(entry, x, env) :
    args = takeN(2, x, x.args, "arguments")
    for y in args :
        argType = compileExprAsValue(y, env)
        if not isCharType(argType) :
            raiseError("Char type expected", y)
    return True, boolType

@compileNamedCallExpr.register(PrimOpCharLesser)
def f(entry, x, env) :
    args = takeN(2, x, x.args, "arguments")
    for y in args :
        argType = compileExprAsValue(y, env)
        if not isCharType(argType) :
            raiseError("Char type expected", y)
    return True, boolType

@compileNamedCallExpr.register(PrimOpCharLesserEquals)
def f(entry, x, env) :
    args = takeN(2, x, x.args, "arguments")
    for y in args :
        argType = compileExprAsValue(y, env)
        if not isCharType(argType) :
            raiseError("Char type expected", y)
    return True, boolType

@compileNamedCallExpr.register(PrimOpCharGreater)
def f(entry, x, env) :
    args = takeN(2, x, x.args, "arguments")
    for y in args :
        argType = compileExprAsValue(y, env)
        if not isCharType(argType) :
            raiseError("Char type expected", y)
    return True, boolType

@compileNamedCallExpr.register(PrimOpCharGreaterEquals)
def f(entry, x, env) :
    args = takeN(2, x, x.args, "arguments")
    for y in args :
        argType = compileExprAsValue(y, env)
        if not isCharType(argType) :
            raiseError("Char type expected", y)
    return True, boolType

@compileNamedCallExpr.register(PrimOpIntAdd)
def f(entry, x, env) :
    args = takeN(2, x, x.args, "arguments")
    for y in args :
        argType = compileExprAsValue(y, env)
        if not isIntType(argType) :
            raiseError("Int type expected", y)
    return True, intType

@compileNamedCallExpr.register(PrimOpIntSubtract)
def f(entry, x, env) :
    args = takeN(2, x, x.args, "arguments")
    for y in args :
        argType = compileExprAsValue(y, env)
        if not isIntType(argType) :
            raiseError("Int type expected", y)
    return True, intType

@compileNamedCallExpr.register(PrimOpIntMultiply)
def f(entry, x, env) :
    args = takeN(2, x, x.args, "arguments")
    for y in args :
        argType = compileExprAsValue(y, env)
        if not isIntType(argType) :
            raiseError("Int type expected", y)
    return True, intType

@compileNamedCallExpr.register(PrimOpIntDivide)
def f(entry, x, env) :
    args = takeN(2, x, x.args, "arguments")
    for y in args :
        argType = compileExprAsValue(y, env)
        if not isIntType(argType) :
            raiseError("Int type expected", y)
    return True, intType

@compileNamedCallExpr.register(PrimOpIntModulus)
def f(entry, x, env) :
    args = takeN(2, x, x.args, "arguments")
    for y in args :
        argType = compileExprAsValue(y, env)
        if not isIntType(argType) :
            raiseError("Int type expected", y)
    return True, intType

@compileNamedCallExpr.register(PrimOpIntNegate)
def f(entry, x, env) :
    arg = takeOne(x, x.args, "argument")
    argType = compileExprAsValue(arg, env)
    if not isIntType(argType) :
        raiseError("Int type expected", arg)
    return True, intType

@compileNamedCallExpr.register(PrimOpIntEquals)
def f(entry, x, env) :
    args = takeN(2, x, x.args, "arguments")
    for y in args :
        argType = compileExprAsValue(y, env)
        if not isIntType(argType) :
            raiseError("Int type expected", y)
    return True, boolType

@compileNamedCallExpr.register(PrimOpIntLesser)
def f(entry, x, env) :
    args = takeN(2, x, x.args, "arguments")
    for y in args :
        argType = compileExprAsValue(y, env)
        if not isIntType(argType) :
            raiseError("Int type expected", y)
    return True, boolType

@compileNamedCallExpr.register(PrimOpIntLesserEquals)
def f(entry, x, env) :
    args = takeN(2, x, x.args, "arguments")
    for y in args :
        argType = compileExprAsValue(y, env)
        if not isIntType(argType) :
            raiseError("Int type expected", y)
    return True, boolType

@compileNamedCallExpr.register(PrimOpIntGreater)
def f(entry, x, env) :
    args = takeN(2, x, x.args, "arguments")
    for y in args :
        argType = compileExprAsValue(y, env)
        if not isIntType(argType) :
            raiseError("Int type expected", y)
    return True, boolType

@compileNamedCallExpr.register(PrimOpIntGreaterEquals)
def f(entry, x, env) :
    args = takeN(2, x, x.args, "arguments")
    for y in args :
        argType = compileExprAsValue(y, env)
        if not isIntType(argType) :
            raiseError("Int type expected", y)
    return True, boolType

def bindTypeVariables(tvarNames, env) :
    tvars = []
    for tvarName in tvarNames :
        tvar = TypeVariable(tvarName)
        tvars.append(tvar)
        addIdent(env, tvarName, tvar)
    return tvars

@compileNamedCallExpr.register(RecordEntry)
def f(entry, x, env) :
    ast = entry.ast
    env2 = Env(entry.env)
    tvars = bindTypeVariables(ast.typeVars, env2)
    if len(x.args) != len(ast.fields) :
        raiseError("incorrect number of arguments", x)
    for field, arg in zip(ast.fields, x.args) :
        fieldType = compileExprAsType(field.type, env2)
        argType = compileExprAsValue(arg, env)
        if not typeUnify(argType, fieldType) :
            raiseError("type mismatch", arg)
    typeParams = [tvar.deref() for tvar in tvars]
    return True, RecordType(entry, typeParams)

@compileNamedCallExpr.register(StructEntry)
def f(entry, x, env) :
    ast = entry.ast
    env2 = Env(entry.env)
    tvars = bindTypeVariables(ast.typeVars, env2)
    if len(x.args) != len(ast.fields) :
        raiseError("incorrect number of arguments", x)
    for field, arg in zip(ast.fields, x.args) :
        fieldType = compileExprAsType(field.type, env2)
        argType = compileExprAsValue(arg, env)
        if not typeUnify(argType, fieldType) :
            raiseError("type mismatch", arg)
    typeParams = [tvar.deref() for tvar in tvars]
    return True, StructType(entry, typeParams)

@compileNamedCallExpr.register(ProcedureEntry)
def f(entry, x, env) :
    assert False

@compileNamedCallExpr.register(OverloadableEntry)
def f(entry, x, env) :
    assert False

@compileNamedCallExpr.register(TypeVarEntry)
def f(entry, x, env) :
    assert False

# end compileNamedCallExpr



#
# remove temp name used for multimethod instances
#

del f
