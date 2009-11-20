from clay.ast import *
from clay.core import *
from clay.unifier import *
from clay.env import *
from clay.primitives import *



#
# lift
#

lift = multimethod("lift")

lift.register(str)(toCOValue)
lift.register(Type)(toCOValue)
lift.register(Record)(toCOValue)
lift.register(Procedure)(toCOValue)
lift.register(Overloadable)(toCOValue)
lift.register(ExternalProcedure)(toCOValue)
lift.register(Value)(lambda x : x)



#
# temp values
#

_tempsBlocks = []

def pushTempsBlock() :
    block = []
    _tempsBlocks.append(block)

def popTempsBlock() :
    block = _tempsBlocks.pop()
    while block :
        block.pop()

def installTemp(value) :
    _tempsBlocks[-1].append(value)
    return value

def allocTempValue(type_) :
    v = allocValue(type_)
    installTemp(v)
    return v



#
# asStatic
#

asStatic = multimethod("asStatic")

@asStatic.register(Value)
def foo(v) :
    if not v.isOwned :
        v2 = allocTempValue(v.type)
        copyValue(v2, v)
        return v2
    return v



#
#
#

def evaluateRootExpr(expr, env, converter=(lambda x : x)) :
    pushTempsBlock()
    try :
        return evaluate(expr, env, converter)
    finally :
        popTempsBlock()

def evaluateList(exprList, env) :
    return [evaluate(x, env) for x in exprList]

def evaluate(expr, env, converter=(lambda x : x)) :
    return withContext(expr, lambda : converter(evaluate2(expr, env)))

evaluate2 = multimethod("evaluate2")

@evaluate2.register(BoolLiteral)
def foo(x, env) :
    return installTemp(toBoolValue(x.value))

_suffixMap = {
    "i8" : toInt8Value,
    "i16" : toInt16Value,
    "i32" : toInt32Value,
    "i64" : toInt64Value,
    "u8" : toUInt8Value,
    "u16" : toUInt16Value,
    "u32" : toUInt32Value,
    "u64" : toUInt64Value,
    "f32" : toFloat32Value,
    "f64" : toFloat64Value}

@evaluate2.register(IntLiteral)
def foo(x, env) :
    if x.suffix is None :
        f = toInt32Value
    else :
        f = _suffixMap.get(x.suffix)
        ensure(f is not None, "invalid suffix: %s" % x.suffix)
    return installTemp(f(x.value))

@evaluate2.register(FloatLiteral)
def foo(x, env) :
    if x.suffix is None :
        f = toFloat64Value
    else :
        ensure(x.suffix in ["f32", "f64"],
               "invalid floating point suffix: %s" % x.suffix)
        f = _suffixMap[x.suffix]
    return installTemp(f(x.value))

@evaluate2.register(CharLiteral)
def foo(x, env) :
    return evaluate(convertCharLiteral(x.value), env)

@evaluate2.register(StringLiteral)
def foo(x, env) :
    return evaluate(convertStringLiteral(x.value), env)

@evaluate2.register(NameRef)
def foo(x, env) :
    v = lookupIdent(env, x.name)
    ensure(type(v) is Value, "invalid value")
    if v.isOwned :
        v2 = allocTempValue(v.type)
        copyValue(v2, v)
        return v2
    return v

@evaluate2.register(Tuple)
def foo(x, env) :
    return evaluate(convertTuple(x), env)

@evaluate2.register(Array)
def foo(x, env) :
    return evaluate(convertArray(x), env)

@evaluate2.register(Indexing)
def foo(x, env) :
    thing = evaluate(x.expr, env)
    args = evaluateList(x.args, env)
    return invokeIndexing(lower(thing), args)

@evaluate2.register(Call)
def foo(x, env) :
    thing = evaluate(x.expr, env)
    args = evaluateList(x.args, env)
    return invoke(lower(thing), args)

@evaluate2.register(FieldRef)
def foo(x, env) :
    thing = evaluate(x.expr, env)
    name = toCOValue(x.name.s)
    return invoke(PrimObjects.recordFieldRefByName, [thing, name])

@evaluate2.register(TupleRef)
def foo(x, env) :
    thing = evaluate(x.expr, env)
    index = toInt32Value(x.index)
    return invoke(PrimObjects.tupleFieldRef, [thing, index])

@evaluate2.register(Dereference)
def foo(x, env) :
    thing = evaluate(x.expr, env)
    return invoke(PrimObjects.pointerDereference, [thing])

@evaluate2.register(AddressOf)
def foo(x, env) :
    thing = evaluate(x.expr, env)
    return invoke(PrimObjects.addressOf, [thing])

@evaluate2.register(UnaryOpExpr)
def foo(x, env) :
    return evaluate(convertUnaryOpExpr(x), env)

@evaluate2.register(BinaryOpExpr)
def foo(x, env) :
    return evaluate(convertBinaryOpExpr(x), env)

@evaluate2.register(NotExpr)
def foo(x, env) :
    thing = evaluate(x.expr, env)
    return invoke(PrimObjects.boolNot, [thing])

@evaluate2.register(AndExpr)
def foo(x, env) :
    v1 = evaluate(x.expr1, env)
    flag1 = invoke(PrimObjects.boolTruth, [v1])
    if not fromBoolValue(flag1) :
        return v1
    return evaluate(x.expr2, env)

@evaluate2.register(OrExpr)
def foo(x, env) :
    v1 = evaluate(x.expr1, env)
    flag1 = invoke(PrimObjects.boolTruth, [v1])
    if fromBoolValue(flag1) :
        return v1
    return evaluate(x.expr2, env)

@evaluate2.register(StaticExpr)
def foo(x, env) :
    return evaluate(x.expr, env, toStatic)

@evaluate2.register(SCExpression)
def foo(x, env) :
    return evaluate(x.expr, x.env)



#
# de-sugar syntax
#

def convertCharLiteral(c) :
    nameRef = NameRef(Identifier("Char"))
    nameRef = SCExpression(loadedModule("_char").env, nameRef)
    return Call(nameRef, [IntLiteral(ord(c), "i8")])

def convertStringLiteral(s) :
    nameRef = NameRef(Identifier("string"))
    nameRef = SCExpression(loadedModule("_string").env, nameRef)
    charArray = Array([convertCharLiteral(c) for c in s])
    return Call(nameRef, [charArray])

def convertTuple(x) :
    if len(x.args) == 1 :
        return x.args[0]
    return Call(primitiveNameRef("tuple"), x.args)

def convertArray(x) :
    return Call(primitiveNameRef("array"), x.args)

_unaryOps = {"+" : "plus",
             "-" : "minus"}

def convertUnaryOpExpr(x) :
    return Call(coreNameRef(_unaryOps[x.op]), [x.expr])

_binaryOps = {"+"  : "add",
              "-"  : "subtract",
              "*"  : "multiply",
              "/"  : "divide",
              "%"  : "remainder",
              "==" : "equals?",
              "!=" : "notEquals?",
              "<"  : "lesser?",
              "<=" : "lesserEquals?",
              ">"  : "greater?",
              ">=" : "greaterEquals?"}

def convertBinaryOpExpr(x) :
    return Call(coreNameRef(_binaryOps[x.op]), [x.expr1, x.expr2])



#
# invokeIndexing
#

invokeIndexing = multimethod("invokeIndexing")

@invokeIndexing.register(TypeConstructorPrimOp)
def foo(x, args) :
    return invoke(x.constructorPrim, args)

@invokeIndexing.register(Record)
def foo(x, args) :
    return invoke(PrimObjects.RecordType, [toCOValue(x)] + args)



#
# invoke
#

invoke = multimethod("invoke")

@invoke.register(Procedure)
def foo(x, args) :
    raise NotImplementedError

@invoke.register(Overloadable)
def foo(x, args) :
    raise NotImplementedError

@invoke.register(ExternalProcedure)
def foo(x, args) :
    raise NotImplementedError



#
# matchInvokeCode
#

class MatchError(object) :
    pass

class ArgCountError(MatchError) :
    pass

class ArgMismatch(MatchError) :
    def __init__(self, pos) :
        self.pos = pos

class PredicateFailure(MatchError) :
    pass

def matchInvokeCode(x, args) :
    if len(args) != len(x.formalArgs) :
        return ArgCountError()
    cells = [Cell(y) for y in x.typeVars]
    patternEnv = extendEnv(x.env, x.typeVars, cells)
    for i, farg in enumerate(x.formalArgs) :
        if not matchArg(farg, args[i], patternEnv) :
            return ArgMismatch(i)
    env2 = extendEnv(x.env, x.typeVars, derefCells(cells))
    if x.predicate is not None :
        result = evaluateRootExpr(x.predicate, env2, toPredicateResult)
        if not result :
            return PredicateFailure()
    for arg, farg in zip(args, x.formalArgs) :
        if type(farg) is ValueArgument :
            addIdent(env2, farg.name, asReference(arg))
    return env2

matchArg = multimethod("matchArg")

@matchArg.register(ValueArgument)
def foo(farg, arg, env) :
    if farg.type is None :
        return True
    pattern = evaluatePattern(farg.type, env)
    return unify(pattern, toCOValue(arg.type)) :

@matchArg.register(StaticArgument)
def foo(farg, arg, env) :
    pattern = evaluatePattern(farg.pattern, env)
    return unify(pattern, arg)

def toPredicateResult(v) :
    return fromBoolValue(invoke(PrimObjects.boolTruth, [v]))



#
# evaluatePattern
#

def evaluatePattern(x, env) :
    pushTempsBlock()
    try :
        return evaluatePattern2(x, env)
    finally :
        popTempsBlock()

evaluatePattern2 = multimethod("evaluatePattern2")

@evaluatePattern2.register(object)
def foo(x, env) :
    return evaluate(x, env, toStatic)

@evaluatePattern2.register(NameRef)
def foo(x, env) :
    v = lookupIdent(env, x.name)
    if type(v) is Cell :
        return v
    return evaluate(x, env, toStatic)

@evaluatePattern2.register(Indexing)
def foo(x, env) :
    thing = evaluate(x.expr, env)
    args = [evaluatePattern(y, env) for y in x.args]
    return invokeIndexingPattern(lower(thing), args)

invokeIndexingPattern = multimethod("invokeIndexingPattern")

@invokeIndexingPattern.register(object)
def foo(x, args) :
    return invokeIndexing(x, args)

@invokeIndexingPattern.register(PrimObjects.Pointer)
def foo(x, args) :
    ensureArity(args, 1)
    return PointerTypePattern(args[0])

@invokeIndexingPattern.register(PrimObjects.Array)
def foo(x, args) :
    ensureArity(args, 2)
    return ArrayTypePattern(args[0], args[1])

@invokeIndexingPattern.register(PrimObjects.Tuple)
def foo(x, args) :
    ensure(len(args) >= 2, "tuple type needs atleast 2 elements")
    return TupleTypePattern(args)

@invokeIndexingPattern.register(Record)
def foo(x, args) :
    ensureArity(args, len(x.typeVars))
    return RecordTypePattern(x, args)



#
# remove temp name used for multimethod instances
#

del foo
