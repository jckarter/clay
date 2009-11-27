from clay.xprint import *
from clay.ast import *
from clay.core import *
from clay.unifier import *
from clay.env import *
from clay.primitives import *



#
# record support
#

def recordFieldTypes(type_) :
    if type_.fieldTypes is not None :
        return type_.fieldTypes
    r = type_.record
    paramValues = [StaticValue(x) for x in type_.params]
    env = extendEnv(r.env, r.patternVars, paramValues)
    typeExprs = [x.type for x in r.args if type(x) is ValueRecordArg]
    types = [evaluateRootExpr(x, env, toTypeResult) for x in typeExprs]
    type_.fieldTypes = types
    return types

def recordFieldNames(type_) :
    if type_.fieldNames is not None :
        return type_.fieldNames
    names = [x.name for x in type_.record.args if type(x) is ValueRecordArg]
    type_.fieldNames = names
    return names

def recordFieldNamesMap(type_) :
    if type_.fieldNamesMap is not None :
        return type_.fieldNamesMap
    namesMap = dict([(n.s, i) for i, n in enumerate(recordFieldNames(type_))])
    type_.fieldNamesMap = namesMap
    return namesMap

def recordFieldCount(type_) :
    return len(recordFieldTypes(type_))

def recordFieldOffset(type_, index) :
    llt = llvmType(type_)
    return llvmTargetData.offset_of_element(llt, index)

def recordFieldRef(v, index) :
    offset = recordFieldOffset(v.type, index)
    fieldType = recordFieldTypes(v.type)[index]
    return Value(fieldType, False, v.address + offset)

def recordFieldRefs(v) :
    n = len(recordFieldTypes(v.type))
    return [recordFieldRef(v, i) for i in range(n)]

@makeLLVMType.register(RecordType)
def foo(t) :
    llt = llvm.Type.opaque()
    typeHandle = llvm.TypeHandle.new(llt)
    t.llvmType = llt
    recType = lltStruct([llvmType(x) for x in recordFieldTypes(t)])
    typeHandle.type.refine(recType)
    return typeHandle.type

@initValue2.register(RecordType)
def foo(t, v) :
    invoke(coreValue("init"), [v])

@destroyValue2.register(RecordType)
def foo(t, v) :
    invoke(coreValue("destroy"), [v])

@copyValue2.register(RecordType)
def foo(t, dest, src) :
    invoke(coreValue("copy"), [dest, src])

@equalValues2.register(RecordType)
def foo(t, a, b) :
    return toBoolResult(invoke(coreValue("equals?"), [a, b]))

@hashValue2.register(RecordType)
def foo(t, a) :
    return fromPrimValue(invoke(coreValue("hash"), [a]))

@xconvertValue2.register(RecordType)
def foo(t, a) :
    return XObject(t.record.name.s, t.params, recordFieldRefs(a))



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
    return installTemp(allocValue(type_))



#
# toTemporary, toReference
#

toTemporary = multimethod("toTemporary")

@toTemporary.register(Value)
def foo(v) :
    if not v.isOwned :
        v2 = allocTempValue(v.type)
        copyValue(v2, v)
        return v2
    return v

toReference = multimethod("toReference")

@toReference.register(Value)
def foo(x) :
    return Value(x.type, False, x.address)



#
# util procs
#

def toBoolResult(v) :
    return fromBoolValue(invoke(PrimObjects.boolTruth, [v]))

def toTempBool(x) :
    return installTemp(toBoolValue(x))

def toTempInt(x) :
    return installTemp(toInt32Value(x))

def toTempPrim(t, x) :
    return installTemp(toPrimValue(t, x))



#
# evaluateRootExpr, evaluate
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
    def inner() :
        return converter(evaluate2(expr, env))
    return withContext(expr, inner)

evaluate2 = multimethod("evaluate2")

@evaluate2.register(BoolLiteral)
def foo(x, env) :
    return toTempBool(x.value)

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
    if type(v) is StaticValue :
        v2 = allocTempValue(v.value.type)
        copyValue(v2, v.value)
        return v2
    ensure(type(v) is Value, "invalid value")
    return toReference(v)

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

@invokeIndexing.register(PrimClasses.Pointer)
def foo(x, args) :
    return invoke(PrimObjects.PointerType, args)

@invokeIndexing.register(PrimClasses.Array)
def foo(x, args) :
    return invoke(PrimObjects.ArrayType, args)

@invokeIndexing.register(PrimClasses.Tuple)
def foo(x, args) :
    return invoke(PrimObjects.TupleType, args)

@invokeIndexing.register(Record)
def foo(x, args) :
    return invoke(PrimObjects.RecordType, [toCOValue(x)] + args)



#
# invoke procedures and overloadables
#

invoke = multimethod("invoke")

@invoke.register(Procedure)
def foo(x, args) :
    if x.cache is None :
        x.cache = ProcedureCache(x)
    env = x.cache.lookup(args)
    if env is None :
        result = matchInvokeCode(x.code, args)
        if isinstance(result, MatchError) :
            result.signalError()
        env = result
        x.cache.add(args, env)
    env = bindValueArgs(env, args, x.code)
    return evalCodeBody(x.code, env)

class ProcedureCache(object) :
    def __init__(self, proc) :
        self.proc = proc
        self.staticMap = None
        self.data = {}
        self.initStaticMap()
    def initStaticMap(self) :
        self.staticMap = []
        for x in self.proc.code.formalArgs :
            self.staticMap.append(type(x) is StaticArgument)
    def lookup(self, args) :
        if len(args) != len(self.staticMap) :
            return None
        return self.data.get(self.key(args))
    def add(self, args, env) :
        self.data[self.key(args)] = env
    def key(self, args) :
        k = []
        for arg, isStatic in zip(args, self.staticMap) :
            if isStatic :
                k.append(lower(arg))
            else :
                k.append(arg.type)
        return tuple(k)

@invoke.register(Overloadable)
def foo(x, args) :
    if x.cache is None :
        x.cache = OverloadableCache(x.overloads)
    codeAndEnv = x.cache.lookup(args)
    if codeAndEnv is None :
        for y in x.overloads :
            result = matchInvokeCode(y.code, args)
            if not isinstance(result, MatchError) :
                codeAndEnv = (y.code, result)
                break
        if codeAndEnv is None :
            error("no matching overload")
        x.cache.add(args, codeAndEnv)
    code, env = codeAndEnv
    env = bindValueArgs(env, args, code)
    return evalCodeBody(code, env)

class OverloadableCache(object) :
    def __init__(self, overloads) :
        self.overloads = overloads
        self.arityMap = {}
        self.data = {}
        self.initArityMap()
    def initArityMap(self) :
        for x in self.overloads :
            staticMap = []
            for y in x.code.formalArgs :
                staticMap.append(type(y) is StaticArgument)
            z = self.arityMap.get(len(staticMap))
            if z is None :
                self.arityMap[len(staticMap)] = staticMap
            elif z != staticMap :
                msg = "inconsistency in position of static arguments"
                withContext(x.code, lambda : error(msg))
    def lookup(self, args) :
        n = len(args)
        staticMap = self.arityMap.get(n)
        if staticMap is None :
            return None
        return self.data.get(self.key(args, staticMap))
    def add(self, args, entry) :
        self.data[self.key(args, self.arityMap[len(args)])] = entry
    def key(self, args, staticMap) :
        k = []
        for arg, isStatic in zip(args, staticMap) :
            if isStatic :
                k.append(lower(arg))
            else :
                k.append(arg.type)
        return tuple(k)



#
# matchInvokeCode
#

class MatchError(object) :
    pass

class ArgCountError(MatchError) :
    def signalError(self) :
        error("incorrect no. of arguments")

class ArgMismatch(MatchError) :
    def __init__(self, pos) :
        super(ArgMismatch, self).__init__()
        self.pos = pos
    def signalError(self) :
        error("mismatch at argument %d" % (self.pos+1))

class PredicateFailure(MatchError) :
    def signalError(self) :
        error("procedure predicate failure")

def matchInvokeCode(x, args) :
    if len(args) != len(x.formalArgs) :
        return ArgCountError()
    cells = [Cell(y) for y in x.patternVars]
    patternEnv = extendEnv(x.env, x.patternVars, cells)
    for i, arg in enumerate(args) :
        if not matchArg(x.formalArgs[i], patternEnv, arg) :
            return ArgMismatch(i)
    cellValues = [StaticValue(y) for y in derefCells(cells)]
    env2 = extendEnv(x.env, x.patternVars, cellValues)
    if x.predicate is not None :
        result = evaluateRootExpr(x.predicate, env2, toBoolResult)
        if not result :
            return PredicateFailure()
    return env2

matchArg = multimethod("matchArg")

@matchArg.register(ValueArgument)
def foo(farg, env, arg) :
    if farg.type is None :
        return True
    pattern = evaluatePattern(farg.type, env)
    return unify(pattern, toCOValue(arg.type))

@matchArg.register(StaticArgument)
def foo(farg, env, arg) :
    pattern = evaluatePattern(farg.pattern, env)
    return unify(pattern, arg)

def bindValueArgs(env, args, code) :
    names, values = [], []
    for arg, farg in zip(args, code.formalArgs) :
        if type(farg) is ValueArgument :
            names.append(farg.name)
            values.append(toReference(arg))
    return extendEnv(env, names, values)



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
    return evaluate(x, env, toTemporary)

@evaluatePattern2.register(NameRef)
def foo(x, env) :
    v = lookupIdent(env, x.name)
    if type(v) is Cell :
        return v
    return evaluate(x, env, toTemporary)

@evaluatePattern2.register(Indexing)
def foo(x, env) :
    thing = evaluate(x.expr, env)
    args = [evaluatePattern(y, env) for y in x.args]
    return invokeIndexingPattern(lower(thing), args)

invokeIndexingPattern = multimethod("invokeIndexingPattern")

@invokeIndexingPattern.register(object)
def foo(x, args) :
    return invokeIndexing(x, args)

@invokeIndexingPattern.register(PrimClasses.Pointer)
def foo(x, args) :
    ensureArity(args, 1)
    return PointerTypePattern(args[0])

@invokeIndexingPattern.register(PrimClasses.Array)
def foo(x, args) :
    ensureArity(args, 2)
    return ArrayTypePattern(args[0], args[1])

@invokeIndexingPattern.register(PrimClasses.Tuple)
def foo(x, args) :
    ensure(len(args) >= 2, "tuple type needs atleast 2 elements")
    return TupleTypePattern(args)

@invokeIndexingPattern.register(Record)
def foo(x, args) :
    ensureArity(args, len(x.patternVars))
    return RecordTypePattern(x, args)



#
# evalCodeBody, evalStatement
#

def evalCodeBody(code, env) :
    result = evalStatement(code.body, env)
    if result is None :
        result = ReturnResult(None)
    return result.asFinalResult()

class StatementResult(object) :
    pass

class GotoResult(StatementResult) :
    def __init__(self, labelName) :
        super(GotoResult, self).__init__()
        self.labelName = labelName
    def asFinalResult(self) :
        withContext(self.labelName, lambda : error("label not found"))

class BreakResult(StatementResult) :
    def __init__(self, stmt) :
        super(BreakResult, self).__init__()
        self.stmt = stmt
    def asFinalResult(self) :
        withContext(self.stmt, lambda : error("invalid break statement"))

class ContinueResult(StatementResult) :
    def __init__(self, stmt) :
        super(ContinueResult, self).__init__()
        self.stmt = stmt
    def asFinalResult(self) :
        withContext(self.stmt, lambda : error("invalid continue statement"))

class ReturnResult(StatementResult) :
    def __init__(self, result) :
        super(ReturnResult, self).__init__()
        self.result = result
    def asFinalResult(self) :
        x = self.result
        if (x is not None) and x.isOwned :
            installTemp(x)
        return x

def evalStatement(x, env) :
    return withContext(x, lambda : evalStatement2(x, env))

evalStatement2 = multimethod("evalStatement2")

@evalStatement2.register(Block)
def foo(x, env) :
    i = 0
    labels = {}
    evalCollectLabels(x.statements, i, labels, env)
    while i < len(x.statements) :
        y = x.statements[i]
        if type(y) in (VarBinding, RefBinding, StaticBinding) :
            env = evalBinding(y, env)
            evalCollectLabels(x.statements, i+1, labels, env)
        elif type(y) is Label :
            pass
        else :
            result = evalStatement(y, env)
            if type(result) is GotoResult :
                envAndPos = labels.get(result.labelName.s)
                if envAndPos is not None :
                    env, i = envAndPos
                    continue
            if result is not None :
                return result
        i += 1

def evalCollectLabels(statements, startIndex, labels, env) :
    i = startIndex
    while i < len(statements) :
        x = statements[i]
        if type(x) is Label :
            labels[x.name.s] = (env, i)
        elif type(x) in (VarBinding, RefBinding, StaticBinding) :
            break
        i += 1

evalBinding = multimethod("evalBinding")

@evalBinding.register(VarBinding)
def foo(x, env) :
    right = evaluateRootExpr(x.expr, env, toTemporary)
    return extendEnv(env, [x.name], [right])

@evalBinding.register(RefBinding)
def foo(x, env) :
    right = evaluateRootExpr(x.expr, env)
    return extendEnv(env, [x.name], [right])

@evalBinding.register(StaticBinding)
def foo(x, env) :
    right = evaluateRootExpr(x.expr, env, toTemporary)
    return extendEnv(env, [x.name], [StaticValue(right)])


@evalStatement2.register(Assignment)
def foo(x, env) :
    pushTempsBlock()
    try :
        left = evaluate(x.left, env)
        ensure(not left.isOwned, "cannot assign to a temp")
        right = evaluate(x.right, env)
        assignValue(left, right)
    finally :
        popTempsBlock()

@evalStatement2.register(Goto)
def foo(x, env) :
    return GotoResult(x.labelName)

@evalStatement2.register(Return)
def foo(x, env) :
    if x.expr is None :
        return ReturnResult(None)
    result = evaluateRootExpr(x.expr, env, toTemporary)
    return ReturnResult(result)

@evalStatement2.register(ReturnRef)
def foo(x, env) :
    result = evaluateRootExpr(x.expr, env)
    ensure(not result.isOwned, "cannot return a temp by reference")
    return ReturnResult(result)

@evalStatement2.register(IfStatement)
def foo(x, env) :
    cond = evaluateRootExpr(x.condition, env, toBoolResult)
    if cond :
        return evalStatement(x.thenPart, env)
    elif x.elsePart is not None :
        return evalStatement(x.elsePart, env)

@evalStatement2.register(ExprStatement)
def foo(x, env) :
    evaluateRootExpr(x.expr, env)

@evalStatement2.register(While)
def foo(x, env) :
    while True :
        cond = evaluateRootExpr(x.condition, env, toBoolResult)
        if not cond :
            break
        result = evalStatement(x.body, env)
        if type(result) is BreakResult :
            break
        elif type(result) is ContinueResult :
            continue
        elif result is not None :
            return result

@evalStatement2.register(Break)
def foo(x, env) :
    return BreakResult(x)

@evalStatement2.register(Continue)
def foo(x, env) :
    return ContinueResult(x)

@evalStatement2.register(For)
def foo(x, env) :
    return evalStatement(convertForStatement(x), env)



#
# de-sugar for statement
#

def convertForStatement(x) :
    exprVar = Identifier("%expr")
    iterVar = Identifier("%iter")
    block = Block(
        [RefBinding(exprVar, x.expr),
         VarBinding(iterVar, Call(coreNameRef("iterator"),
                                  [NameRef(exprVar)])),
         While(Call(coreNameRef("hasNext?"), [NameRef(iterVar)]),
               Block([RefBinding(x.variable, Call(coreNameRef("next"),
                                                  [NameRef(iterVar)])),
                      x.body]))])
    return block



#
# invoke pattern matching record constructor
#

@invoke.register(Record)
def foo(x, args) :
    ensureArity(args, len(x.args))
    cells = [Cell(y) for y in x.patternVars]
    patternEnv = extendEnv(x.env, x.patternVars, cells)
    for i, arg in enumerate(args) :
        if not matchRecordArg(x.args[i], patternEnv, arg) :
            error("mismatch at argument %d" % (i + 1))
    type_ = recordType(x, derefCells(cells))
    v = allocTempValue(type_)
    for f, arg in zip(recordFieldRefs(v), args) :
        copyValue(f, arg)
    return v

matchRecordArg = multimethod("matchRecordArg")

@matchRecordArg.register(ValueRecordArg)
def foo(farg, env, arg) :
    pattern = evaluatePattern(farg.type, env)
    return unify(pattern, toCOValue(arg.type))

@matchRecordArg.register(StaticRecordArg)
def foo(farg, env, arg) :
    pattern = evaluatePattern(farg.pattern, env)
    return unify(pattern, arg)



#
# invoke constructors for array types, tuple types, and record types.
# invoke default constructor, copy constructor for all types
#

@invoke.register(Type)
def foo(x, args) :
    if len(args) == 0 :
        # default constructor
        v = allocTempValue(x)
        initValue(v)
        return v
    if (len(args) == 1) and (args[0].type == x) :
        # copy constructor
        v = allocTempValue(args[0].type)
        copyValue(v, args[0])
        return v
    return invokeConstructor(x, args)

invokeConstructor = multimethod("invokeConstructor")

@invokeConstructor.register(ArrayType)
def foo(x, args) :
    ensureArity(args, x.size)
    v = allocTempValue(x)
    for i, element in enumerate(arrayElementRefs(v)) :
        ensure(args[i].type == element.type,
               "type mismatch at argument %d" % (i+1))
        copyValue(element, args[i])
    return v

@invokeConstructor.register(TupleType)
def foo(x, args) :
    ensureArity(args, len(x.elementTypes))
    v = allocTempValue(x)
    for i, field in enumerate(tupleFieldRefs(v)) :
        ensure(args[i].type == field.type,
               "type mismatch at argument %d" % (i+1))
        copyValue(field, args[i])
    return v

@invokeConstructor.register(RecordType)
def foo(x, args) :
    ensureArity(args, recordFieldCount(x))
    v = allocTempValue(x)
    for i, field in enumerate(recordFieldRefs(v)) :
        ensure(args[i].type == field.type,
               "type mismatch at argument %d" % (i+1))
        copyValue(field, args[i])
    return v



#
# invoke external procedure
#

@invoke.register(ExternalProcedure)
def foo(x, args) :
    ensureArity(args, len(x.args))
    initExternalProcedure(x)
    for i, farg in enumerate(x.args) :
        ensure(args[i].type == farg.type2,
               "type mismatch at argument %d" % (i+1))
    return evalLLVMCall(x.llvmFunc, args, x.returnType2)

def initExternalProcedure(x) :
    if x.llvmFunc is not None :
        return
    argTypes = [evaluateRootExpr(y.type, x.env, toTypeResult) for y in x.args]
    returnType = evaluateRootExpr(x.returnType, x.env, toTypeResult)
    for arg, argType in zip(x.args, argTypes) :
        arg.type2 = argType
    x.returnType2 = returnType
    func = generateExternalFunc(argTypes, returnType, x.name.s)
    x.llvmFunc = generateExternalWrapper(func, argTypes, returnType, x.name.s)

def generateExternalFunc(argTypes, returnType, name) :
    llvmArgTypes = [llvmType(y) for y in argTypes]
    if returnType == voidType :
        llvmReturnType = lltVoid()
    else :
        llvmReturnType = llvmType(returnType)
    llvmFuncType = lltFunction(llvmReturnType, llvmArgTypes)
    func = llvmModule.add_function(llvmFuncType, name)
    func.linkage = llvm.LINKAGE_EXTERNAL
    return func

def generateExternalWrapper(func, argTypes, returnType, name) :
    llvmArgTypes = [llvmType(pointerType(y)) for y in argTypes]
    if returnType != voidType :
        llvmArgTypes.append(llvmType(pointerType(returnType)))
    llvmFuncType = lltFunction(lltVoid(), llvmArgTypes)
    wrapper = llvmModule.add_function(llvmFuncType, "wrapper_%s" % name)
    wrapper.linkage = llvm.LINKAGE_INTERNAL
    block = wrapper.append_basic_block("code")
    builder = llvm.Builder.new(block)
    llvmArgs = [builder.load(wrapper.args[i]) for i in range(len(argTypes))]
    result = builder.call(func, llvmArgs)
    if returnType != voidType :
        builder.store(result, wrapper.args[-1])
    builder.ret_void()
    return wrapper

def evalLLVMCall(func, args, outputType) :
    llvmArgs = [toGenericValue(pointerType(x.type), x.address) for x in args]
    if outputType != voidType :
        out = allocTempValue(outputType)
        llvmArgs.append(toGenericValue(pointerType(out.type), out.address))
    else :
        out = None
    llvmExecutionEngine.run_function(func, llvmArgs)
    return out



#
# invoke primitives
#

def ensureArgType(args, i, type_) :
    if args[i].type != type_ :
        error("type mismatch at argument %d" % (i+1))

def ensureArgTypes(args, types) :
    ensureArity(args, len(types))
    for i in range(len(args)) :
        ensureArgType(args, i, types[i])

def ensureType(args, i) :
    t = lower(args[i])
    if not isinstance(t, Type) :
        error("invalid type at argument %d" % (i+1))
    return t

@invoke.register(PrimClasses.TypeP)
def foo(x, args) :
    ensureArgTypes(args, [compilerObjectType])
    return toTempBool(isinstance(fromCOValue(args[0]), Type))

@invoke.register(PrimClasses.TypeSize)
def foo(x, args) :
    ensureArity(args, 1)
    t = ensureType(args, 0)
    return toTempInt(typeSize(t))

@invoke.register(PrimClasses.primitiveCopy)
def foo(x, args) :
    ensureArity(args, 2)
    t = args[0].type
    if (not isinstance(t, BoolType) and not isinstance(t, IntegerType) and
        not isinstance(t, FloatType) and not isinstance(t, PointerType)) :
        error("not a primitive type at argument 0")
    if t != args[1].type :
        error("arguments types don't match")
    copyValue(args[0], args[1])
    return None

@invoke.register(PrimClasses.BoolTypeP)
def foo(x, args) :
    ensureArgTypes(args, [compilerObjectType])
    return toTempBool(isinstance(fromCOValue(args[0]), BoolType))

@invoke.register(PrimClasses.boolNot)
def foo(x, args) :
    ensureArgTypes(args, [boolType])
    return toTempBool(not fromBoolValue(args[0]))

@invoke.register(PrimClasses.boolTruth)
def foo(x, args) :
    ensureArgTypes(args, [boolType])
    return toTempBool(fromBoolValue(args[0]))


@invoke.register(PrimClasses.IntegerTypeP)
def foo(x, args) :
    ensureArgTypes(args, [compilerObjectType])
    return toTempBool(isinstance(fromCOValue(args[0]), IntegerType))

@invoke.register(PrimClasses.SignedIntegerTypeP)
def foo(x, args) :
    ensureArgTypes(args, [compilerObjectType])
    t = fromCOValue(args[0])
    return toTempBool(isinstance(t, IntegerType) and t.signed)

@invoke.register(PrimClasses.FloatTypeP)
def foo(x, args) :
    ensureArgTypes(args, [compilerObjectType])
    return toTempBool(isinstance(fromCOValue(args[0]), FloatType))

def isNumericType(t) :
    return isinstance(t, IntegerType) or isinstance(t, FloatType)

def ensureNumeric(args, i) :
    if not isNumericType(args[i].type) :
        error("invalid numeric type at argument %d" % (i+1))

def ensureNumericBinaryOp(args) :
    ensureArity(args, 2)
    ensureNumeric(args, 0)
    ensureNumeric(args, 1)
    if args[0].type != args[1].type :
        error("argument types don't match")
    return args[0].type, fromPrimValue(args[0]), fromPrimValue(args[1])

@invoke.register(PrimClasses.numericEqualsP)
def foo(x, args) :
    _, a, b = ensureNumericBinaryOp(args)
    return toTempBool(a == b)

@invoke.register(PrimClasses.numericLesserP)
def foo(x, args) :
    _, a, b = ensureNumericBinaryOp(args)
    return toTempBool(a < b)

@invoke.register(PrimClasses.numericAdd)
def foo(x, args) :
    t, a, b = ensureNumericBinaryOp(args)
    return toTempPrim(t, a + b)

@invoke.register(PrimClasses.numericSubtract)
def foo(x, args) :
    t, a, b = ensureNumericBinaryOp(args)
    return toTempPrim(t, a - b)

@invoke.register(PrimClasses.numericMultiply)
def foo(x, args) :
    t, a, b = ensureNumericBinaryOp(args)
    return toTempPrim(t, a * b)

@invoke.register(PrimClasses.numericDivide)
def foo(x, args) :
    t, a, b = ensureNumericBinaryOp(args)
    return toTempPrim(t, a / b)

@invoke.register(PrimClasses.numericRemainder)
def foo(x, args) :
    t, a, b = ensureNumericBinaryOp(args)
    return toTempPrim(t, a % b)


@invoke.register(PrimClasses.numericNegate)
def foo(x, args) :
    ensureArity(args, 1)
    ensureNumeric(args, 0)
    return toTempPrim(args[0].type, -fromPrimValue(args[0]))

@invoke.register(PrimClasses.numericConvert)
def foo(x, args) :
    ensureArity(args, 2)
    ensureNumeric(args, 1)
    t = toTypeResult(args[0])
    v = fromPrimValue(args[1])
    if isinstance(t, IntegerType) :
        v = int(v)
    return toTempPrim(t, v)

def ensureInteger(args, i) :
    if not isinstance(args[i].type, IntegerType) :
        error("invalid integer type at argument %d" % (i+1))

def ensureIntegerBinaryOp(args) :
    ensureArity(args, 2)
    ensureInteger(args, 0)
    ensureInteger(args, 1)
    if args[0].type != args[1].type :
        error("argument types don't match")
    return args[0].type, fromPrimValue(args[0]), fromPrimValue(args[1])

@invoke.register(PrimClasses.shiftLeft)
def foo(x, args) :
    t, a, b = ensureIntegerBinaryOp(args)
    return toTempPrim(t, a << b)

@invoke.register(PrimClasses.shiftRight)
def foo(x, args) :
    t, a, b = ensureIntegerBinaryOp(args)
    return toTempPrim(t, a >> b)

@invoke.register(PrimClasses.bitwiseAnd)
def foo(x, args) :
    t, a, b = ensureIntegerBinaryOp(args)
    return toTempPrim(t, a & b)

@invoke.register(PrimClasses.bitwiseOr)
def foo(x, args) :
    t, a, b = ensureIntegerBinaryOp(args)
    return toTempPrim(t, a | b)

@invoke.register(PrimClasses.bitwiseXor)
def foo(x, args) :
    t, a, b = ensureIntegerBinaryOp(args)
    return toTempPrim(t, a ^ b)


@invoke.register(PrimClasses.VoidTypeP)
def foo(x, args) :
    ensureArgTypes(args, [compilerObjectType])
    return toTempBool(isinstance(fromCOValue(args[0]), VoidType))

@invoke.register(PrimClasses.CompilerObjectTypeP)
def foo(x, args) :
    ensureArgTypes(args, [compilerObjectType])
    return toTempBool(isinstance(fromCOValue(args[0]), CompilerObjectType))


@invoke.register(PrimClasses.PointerTypeP)
def foo(x, args) :
    ensureArgTypes(args, [compilerObjectType])
    return toTempBool(isinstance(fromCOValue(args[0]), PointerType))

@invoke.register(PrimClasses.PointerType)
def foo(x, args) :
    ensureArity(args, 1)
    t = ensureType(args, 0)
    return installTemp(toCOValue(pointerType(t)))

def ensurePointerType(args, i) :
    t = lower(args[i])
    if not isinstance(t, PointerType) :
        error("invalid pointer type at argument %d" % (i+1))
    return t

def ensurePointer(args, i) :
    if not isinstance(args[i].type, PointerType) :
        error("invalid pointer type at argument %d" % (i+1))

@invoke.register(PrimClasses.PointeeType)
def foo(x, args) :
    ensureArity(args, 1)
    t = ensurePointerType(args, 0)
    return installTemp(toCOValue(t.pointeeType))


@invoke.register(PrimClasses.addressOf)
def foo(x, args) :
    ensureArity(args, 1)
    if args[0].isOwned :
        error("cannot get address of temporary")
    return installTemp(toPointerValue(args[0].type, args[0].address))

@invoke.register(PrimClasses.pointerDereference)
def foo(x, args) :
    ensureArity(args, 1)
    ensurePointer(args, 0)
    t = args[0].type.pointeeType
    return Value(t, False, fromPointerValue(args[0]))

@invoke.register(PrimClasses.pointerToInt)
def foo(x, args) :
    ensureArity(args, 2)
    ensurePointer(args, 1)
    t = toTypeResult(args[0])
    if not isinstance(t, IntegerType) :
        error("invalid integer type at argument 1")
    return toTempPrim(t, fromPointerValue(args[1]))

@invoke.register(PrimClasses.intToPointer)
def foo(x, args) :
    ensureArity(args, 2)
    t = ensureType(args, 0)
    ensureInteger(args, 1)
    return installTemp(toPointerValue(t, fromPrimValue(args[1])))

@invoke.register(PrimClasses.pointerCast)
def foo(x, args) :
    ensureArity(args, 2)
    t = ensureType(args, 0)
    ensurePointer(args, 1)
    return installTemp(toPointerValue(t, fromPointerValue(args[1])))

@invoke.register(PrimClasses.allocateMemory)
def foo(x, args) :
    ensureArity(args, 2)
    t = ensureType(args, 0)
    ensureInteger(args, 1)
    n = fromPrimValue(args[1])
    addr = allocMem(typeSize(t) * n)
    return installTemp(toPointerValue(t, addr))

@invoke.register(PrimClasses.freeMemory)
def foo(x, args) :
    ensureArity(args, 1)
    ensurePointer(args, 0)
    freeMem(fromPointerValue(args[0]))
    return None


@invoke.register(PrimClasses.ArrayTypeP)
def foo(x, args) :
    ensureArgTypes(args, [compilerObjectType])
    return toTempBool(isinstance(fromCOValue(args[0]), ArrayType))

@invoke.register(PrimClasses.ArrayType)
def foo(x, args) :
    ensureArity(args, 2)
    t = ensureType(args, 0)
    ensureInteger(args, 1)
    n = fromPrimValue(args[1])
    if n < 0 :
        error("negative array size")
    return installTemp(toCOValue(arrayType(t, n)))

def ensureArrayType(args, i) :
    t = lower(args[i])
    if not isinstance(t, ArrayType) :
        error("invalid array type at argument %d" % (i+1))
    return t

def ensureArray(args, i) :
    if not isinstance(args[i].type, ArrayType) :
        error("invalid array type at argument %d" % (i+1))

@invoke.register(PrimClasses.ArrayElementType)
def foo(x, args) :
    ensureArity(args, 1)
    t = ensureArrayType(args, 0)
    return installTemp(toCOValue(t.elementType))

@invoke.register(PrimClasses.ArraySize)
def foo(x, args) :
    ensureArity(args, 1)
    t = ensureArrayType(args, 0)
    return toTempInt(t.size)

@invoke.register(PrimClasses.array)
def foo(x, args) :
    cell = Cell("array_element_type")
    for i, x in enumerate(args) :
        if not unify(cell, toCOValue(x.type)) :
            error("type mismatch at argument %d" % (i+1))
    t = arrayType(toTypeResult(resolvePattern(cell)), len(args))
    return invoke(t, args)

@invoke.register(PrimClasses.arrayRef)
def foo(x, args) :
    ensureArity(args, 2)
    ensureArray(args, 0)
    ensureInteger(args, 1)
    return arrayElementRef(args[0], fromPrimValue(args[1]))


@invoke.register(PrimClasses.TupleTypeP)
def foo(x, args) :
    ensureArgTypes(args, [compilerObjectType])
    return toTempBool(isinstance(fromCOValue(args[0]), TupleType))

@invoke.register(PrimClasses.TupleType)
def foo(x, args) :
    ensure(len(args) >= 2, "atleast two elements required for tuple type")
    elementTypes = [ensureType(args, i) for i in range(len(args))]
    return installTemp(toCOValue(tupleType(elementTypes)))

def ensureTupleType(args, i) :
    t = lower(args[i])
    if not isinstance(t, TupleType) :
        error("invalid tuple type at argument %d" % (i+1))
    return t

def ensureTuple(args, i) :
    if not isinstance(args[i].type, TupleType) :
        error("invalid tuple type at argument %d" % (i+1))

@invoke.register(PrimClasses.TupleElementType)
def foo(x, args) :
    ensureArity(args, 2)
    t = ensureTupleType(args, 0)
    ensureInteger(args, 1)
    i = fromPrimValue(args[1])
    if (i < 0) or (i >= len(t.elementTypes)) :
        error("tuple index out of range")
    return installTemp(toCOValue(t.elementTypes[i]))

@invoke.register(PrimClasses.TupleFieldCount)
def foo(x, args) :
    ensureArity(args, 1)
    t = ensureTupleType(args, 0)
    return toTempInt(len(t.elementTypes))

@invoke.register(PrimClasses.TupleFieldOffset)
def foo(x, args) :
    ensureArity(args, 2)
    t = ensureTupleType(args, 0)
    ensureInteger(args, 1)
    i = fromPrimValue(args[1])
    if (i < 0) or (i >= len(t.elementTypes)) :
        error("tuple index out of range")
    return toTempInt(tupleFieldOffset(t, i))

@invoke.register(PrimClasses.tuple)
def foo(x, args) :
    ensure(len(args) >= 2, "atleast two elements required for tuple type")
    t = tupleType([y.type for y in args])
    return invoke(t, args)

@invoke.register(PrimClasses.tupleFieldRef)
def foo(x, args) :
    ensureArity(args, 2)
    ensureTuple(args, 0)
    ensureInteger(args, 1)
    i = fromPrimValue(args[1])
    if (i < 0) or (i >= len(args[0].type.elementTypes)) :
        error("tuple index out of range")
    return tupleFieldRef(args[0], i)


@invoke.register(PrimClasses.RecordTypeP)
def foo(x, args) :
    ensureArgTypes(args, [compilerObjectType])
    return toTempBool(isinstance(fromCOValue(args[0]), RecordType))

@invoke.register(PrimClasses.RecordType)
def foo(x, args) :
    ensure(len(args) >= 2, "atleast two argument required")
    r = lower(args[0])
    ensure(isinstance(r, Record), "invalid record at argument 0")
    ensureArity(args, 1+len(r.patternVars))
    params = [toTemporary(y) for y in args[1:]]
    t = recordType(r, params)
    return installTemp(toCOValue(t))

def ensureRecordType(args, i) :
    t = lower(args[i])
    if not isinstance(t, RecordType) :
        error("invalid record type at argument %d" % (i+1))
    return t

def ensureRecord(args, i) :
    if not isinstance(args[i].type, RecordType) :
        error("invalid record type at argument %d" % (i+1))

@invoke.register(PrimClasses.RecordElementType)
def foo(x, args) :
    ensureArity(args, 2)
    t = ensureRecordType(args, 0)
    ensureInteger(args, 1)
    i = fromPrimValue(args[1])
    if (i < 0) or (i >= len(recordFieldTypes(t))) :
        error("record field index out of range")
    return installTemp(toCOValue(recordFieldTypes(t)[i]))

@invoke.register(PrimClasses.RecordFieldCount)
def foo(x, args) :
    ensureArity(args, 1)
    t = ensureRecordType(args, 0)
    return toTempInt(len(recordFieldTypes(t)))

@invoke.register(PrimClasses.RecordFieldOffset)
def foo(x, args) :
    ensureArity(args, 2)
    t = ensureRecordType(args, 0)
    ensureInteger(args, 1)
    i = fromPrimValue(args[1])
    if (i < 0) or (i >= len(recordFieldTypes(t))) :
        error("record field index out of range")
    return toTempInt(recordFieldOffset(t, i))

def ensureName(args, i) :
    name = lower(args[i])
    if not isinstance(name, str) :
        error("invalid identifier at argument %d" % (i+1))
    return name

@invoke.register(PrimClasses.RecordFieldIndex)
def foo(x, args) :
    ensureArgTypes(args, [compilerObjectType, compilerObjectType])
    t = ensureRecordType(args, 0)
    name = ensureName(args, 1)
    i = recordFieldNamesMap(t).get(name)
    if i is None :
        error("field not found: %s" % name)
    return toTempInt(i)

@invoke.register(PrimClasses.recordFieldRef)
def foo(x, args) :
    ensureArity(args, 2)
    ensureRecord(args, 0)
    ensureInteger(args, 1)
    i = fromPrimValue(args[1])
    if (i < 0) or (i >= len(recordFieldTypes(args[0].type))) :
        error("record field index out of range")
    return recordFieldRef(args[0], i)

@invoke.register(PrimClasses.recordFieldRefByName)
def foo(x, args) :
    ensureArity(args, 2)
    ensureRecord(args, 0)
    name = ensureName(args, 1)
    i = recordFieldNamesMap(args[0].type).get(name)
    if i is None :
        error("field not found: %s" % name)
    return recordFieldRef(args[0], i)



#
# remove temp name used for multimethod instances
#

del foo
