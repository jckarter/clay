import ctypes
from clay.xprint import *
from clay.cleanup import *
from clay.error import *
from clay.multimethod import *
from clay.ast import *
from clay.env import *
import clay.llvmwrapper as llvm



#
# Type, Value, Reference
#

class Type(object) :
    def __init__(self, tag, params) :
        self.tag = tag
        self.params = params
        self.interned = False
        self.ctypesType = None
        self.llvmType = None

class Value(object) :
    def __init__(self, type_) :
        self.type = type_
        self.ctypesType = ctypesType(type_)
        self.buf = self.ctypesType()
    def __del__(self) :
        #FIXME: ugly hack
        if valueDestroy is None :
            return
        valueDestroy(toReference(self))
    def __eq__(self, other) :
        return equals(self, other)
    def __hash__(self) :
        return valueHash(toReference(self))

class Reference(object) :
    def __init__(self, type_, address) :
        self.type = type_
        self.address = address
    def __eq__(self, other) :
        return equals(self, other)
    def __hash__(self) :
        return valueHash(self)

def isType(t) : return type(t) is Type
def isValue(x) : return type(x) is Value
def isReference(x) : return type(x) is Reference



#
# types
#

class BoolTag(object) : pass

class IntegerTag(object) :
    def __init__(self, bits, signed) :
        self.bits = bits
        self.signed = signed

class FloatingPointTag(object) :
    def __init__(self, bits) :
        self.bits = bits

class VoidTag(object) : pass
class CompilerObjectTag(object) : pass

class TupleTag(object) : pass
class ArrayTag(object) : pass
class PointerTag(object) : pass

boolTag = BoolTag()

int8Tag  = IntegerTag(8,  signed=True)
int16Tag = IntegerTag(16, signed=True)
int32Tag = IntegerTag(32, signed=True)
int64Tag = IntegerTag(64, signed=True)

uint8Tag  = IntegerTag(8,  signed=False)
uint16Tag = IntegerTag(16, signed=False)
uint32Tag = IntegerTag(32, signed=False)
uint64Tag = IntegerTag(64, signed=False)

float32Tag = FloatingPointTag(32)
float64Tag = FloatingPointTag(64)

voidTag = VoidTag()
compilerObjectTag = CompilerObjectTag()

tupleTag = TupleTag()
arrayTag = ArrayTag()
pointerTag = PointerTag()

typesTable = {}
internTypes = [True]

def enableInternedTypes(flag) :
    internTypes.append(flag)

def restoreInternedTypes() :
    internTypes.pop()

def makeType(tag, params) :
    if not internTypes[-1] :
        return Type(tag, params)
    key = (tag, params)
    t = typesTable.get(key)
    if t is None :
        t = Type(tag, params)
        t.interned = True
        typesTable[key] = t
    return t

def _cleanupTypesTable() :
    global typesTable
    typesTable = {}
installGlobalsCleanupHook(_cleanupTypesTable)

boolType = makeType(boolTag, ())

int8Type  = makeType(int8Tag, ())
int16Type = makeType(int16Tag, ())
int32Type = makeType(int32Tag, ())
int64Type = makeType(int64Tag, ())

uint8Type  = makeType(uint8Tag, ())
uint16Type = makeType(uint16Tag, ())
uint32Type = makeType(uint32Tag, ())
uint64Type = makeType(uint64Tag, ())

nativeIntType = int32Type

float32Type = makeType(float32Tag, ())
float64Type = makeType(float64Tag, ())

voidType = makeType(voidTag, ())
compilerObjectType = makeType(compilerObjectTag, ())

def tupleType(types) :
    assert len(types) >= 2
    return makeType(tupleTag, types)

def arrayType(type_, sizeValue) :
    return makeType(arrayTag, (type_, sizeValue))

def arrayTypePattern(type_, sizeValue) :
    return Type(arrayTag, (type_, sizeValue))

def pointerType(type_) :
    return makeType(pointerTag, (type_,))

def pointerTypePattern(type_) :
    return Type(pointerTag, (type_,))

def recordType(record, params) :
    return makeType(record, params)



#
# VoidValue, voidValue
#

class VoidValue(object) :
    def __init__(self) :
        self.type = voidType

voidValue = VoidValue()

def isVoidValue(x) :
    return type(x) is VoidValue

xregister(VoidValue, lambda x : XSymbol("void"))



#
# install primitive types
#

installPrimitive("Bool", boolType)

installPrimitive("Int8",  int8Type)
installPrimitive("Int16", int16Type)
installPrimitive("Int32", int32Type)
installPrimitive("Int64", int64Type)

installPrimitive("UInt8",  uint8Type)
installPrimitive("UInt16", uint16Type)
installPrimitive("UInt32", uint32Type)
installPrimitive("UInt64", uint64Type)

installPrimitive("Float32", float32Type)
installPrimitive("Float64", float64Type)

installPrimitive("Void", voidType)
installPrimitive("CompilerObject", compilerObjectType)



#
# type predicates
#

def isBoolType(t) : return t.tag is boolTag

def isIntegerType(t) : return type(t.tag) is IntegerTag

def isInt8Type(t)  : return t.tag is int8Tag
def isInt16Type(t) : return t.tag is int16Tag
def isInt32Type(t) : return t.tag is int32Tag
def isInt64Type(t) : return t.tag is int64Tag

def isUInt8Type(t)  : return t.tag is uint8Tag
def isUInt16Type(t) : return t.tag is uint16Tag
def isUInt32Type(t) : return t.tag is uint32Tag
def isUInt64Type(t) : return t.tag is uint64Tag

def isFloatingPointType(t) : return type(t.tag) is FloatingPointTag

def isFloat32Type(t) : return t.tag is float32Tag
def isFloat64Type(t) : return t.tag is float64Tag

def isVoidType(t) : return t.tag is voidTag
def isCompilerObjectType(t) : return t.tag is compilerObjectTag

def isTupleType(t) : return t.tag is tupleTag
def isArrayType(t) : return t.tag is arrayTag
def isPointerType(t) : return t.tag is pointerTag
def isRecordType(t) : return type(t.tag) is Record

_simpleTags = set([boolTag, int8Tag, int16Tag, int32Tag, int64Tag,
                   uint8Tag, uint16Tag, uint32Tag, uint64Tag,
                   float32Tag, float64Tag, pointerTag])
def isSimpleType(t) :
    return t.tag in _simpleTags



#
# equals
#

equals = multimethod(n=2, defaultProc=(lambda x, y : x is y))

@equals.register(Value, Value)
def foo(a, b) :
    return typeAndValueEquals(a, b)

@equals.register(Value, Reference)
def foo(a, b) :
    return typeAndValueEquals(a, b)

@equals.register(Reference, Value)
def foo(a, b) :
    return typeAndValueEquals(a, b)

@equals.register(Reference, Reference)
def foo(a, b) :
    return typeAndValueEquals(a, b)

def typeAndValueEquals(a, b) :
    if not equals(a.type, b.type) :
        return False
    return valueEquals(toReference(a), toReference(b))



#
# toType
#

toType = multimethod(errorMessage="type expected")

@toType.register(Type)
def foo(x) :
    return x

@toType.register(Value)
def foo(x) :
    return toType(toReference(x))

@toType.register(Reference)
def foo(x) :
    ensure(isCompilerObjectType(x.type), "invalid type")
    return toType(reduceCompilerObject(x))



#
# multimethodRegisterMany
#

def multimethodRegisterMany(mm, classList, proc) :
    for c in classList :
        mm.register(c)(proc)



#
# toStatic
#

toStatic = multimethod(errorMessage="invalid static value")

toStatic.register(Type)(lambda x : x)
toStatic.register(Record)(lambda x : x)
toStatic.register(Procedure)(lambda x : x)
toStatic.register(Overloadable)(lambda x : x)
multimethodRegisterMany(toStatic, primitivesClassList, lambda x : x)

@toStatic.register(Value)
def foo(x) :
    if isCompilerObjectType(x.type) :
        return reduceCompilerObject(x)
    return x

@toStatic.register(Reference)
def foo(x) :
    if isCompilerObjectType(x.type) :
        return reduceCompilerObject(x)
    return toValue(x)



#
# toValue
#

toValue = multimethod(errorMessage="invalid value")

toValue.register(Value)(lambda x : x)

@toValue.register(Reference)
def foo(x) :
    v = tempValue(x.type)
    valueCopy(toReference(v), x)
    return v

toValue.register(Type)(lambda x : liftCompilerObject(x))
toValue.register(Record)(lambda x : liftCompilerObject(x))
toValue.register(Procedure)(lambda x : liftCompilerObject(x))
toValue.register(Overloadable)(lambda x : liftCompilerObject(x))
multimethodRegisterMany(toValue, primitivesClassList,
                        lambda x : liftCompilerObject(x))



#
# toReference
#

toReference = multimethod(errorMessage="invalid reference")

toReference.register(Reference)(lambda x : x)

@toReference.register(Value)
def foo(x) :
    return Reference(x.type, ctypes.addressof(x.buf))

def liftCompilerObjectAsRef(x) :
    return toReference(liftCompilerObject(x))

toReference.register(Type)(lambda x : liftCompilerObjectAsRef(x))
toReference.register(Record)(lambda x : liftCompilerObjectAsRef(x))
toReference.register(Procedure)(lambda x : liftCompilerObjectAsRef(x))
toReference.register(Overloadable)(lambda x : liftCompilerObjectAsRef(x))
multimethodRegisterMany(toReference, primitivesClassList,
                        lambda x : liftCompilerObjectAsRef(x))



#
# toLValue
#

toLValue = multimethod(errorMessage="invalid l-value")

toLValue.register(Reference)(lambda x : x)



#
# toValueOrReference
#

toValueOrReference = multimethod(defaultProc=toValue)

toValueOrReference.register(Value)(lambda x : x)
toValueOrReference.register(Reference)(lambda x : x)



#
# type printer
#

xregister(Type, lambda t : (XObject(tagName(t), *t.params,
                                    opening="[", closing="]")
                            if t.params
                            else XSymbol(tagName(t))))

def tagName(t) :
    if type(t.tag) is Record :
        return t.tag.name.s
    return _tagNames[t.tag]

_tagNames = {}
_tagNames[boolTag] = "Bool"
_tagNames[int8Tag] = "Int8"
_tagNames[int16Tag] = "Int16"
_tagNames[int32Tag] = "Int32"
_tagNames[int64Tag] = "Int64"
_tagNames[uint8Tag] = "UInt8"
_tagNames[uint16Tag] = "UInt16"
_tagNames[uint32Tag] = "UInt32"
_tagNames[uint64Tag] = "UInt64"
_tagNames[float32Tag] = "Float32"
_tagNames[float64Tag] = "Float64"
_tagNames[voidTag] = "Void"
_tagNames[tupleTag] = "Tuple"
_tagNames[arrayTag] = "Array"
_tagNames[pointerTag] = "Pointer"



#
# type checking converters
#

def toTypeWithTag(tag) :
    def f(x) :
        t = toType(x)
        ensure(t.tag is tag, "type mismatch")
        return t
    return f

def toIntegerType(t) :
    ensure(isType(t) and isIntegerType(t), "integer type expected")
    return t

def toFloatingPointType(t) :
    ensure(isType(t) and isFloatingPointType(t),
           "floating point type expected")
    return t

def toNumericType(t) :
    ensure(isType(t) and (isIntegerType(t) or isFloatingPointType(t)),
           "numeric type expected")
    return t

def toRecordType(t) :
    ensure(isType(t) and isRecordType(t), "record type expected")
    return t

def toReferenceWithTag(tag) :
    def f(x) :
        r = toReference(x)
        ensure(r.type.tag is tag, "type mismatch")
        return r
    return f

def toIntegerReference(x) :
    r = toReference(x)
    ensure(isIntegerType(r.type), "integer type expected")
    return r

def toFloatingPointReference(x) :
    r = toReference(x)
    ensure(isFloatingPointType(r.type), "floating point type expected")
    return r

def toNumericReference(x) :
    r = toReference(x)
    ensure(isIntegerType(r.type) or isFloatingPointType(r.type),
           "numeric type expected")
    return r

def toRecordReference(x) :
    r = toReference(x)
    ensure(isRecordType(r.type), "record type expected")
    return r



#
# Cell
#

class Cell(object) :
    def __init__(self, param=None) :
        self.param = param

def isCell(x) : return type(x) is Cell

xregister(Cell, lambda x : XObject("Cell", x.param))



#
# toValueOrCell, toTypeOrCell, toStaticOrCell
#

toValueOrCell = multimethod(defaultProc=toValue)
toValueOrCell.register(Cell)(lambda x : x)

toTypeOrCell = multimethod(defaultProc=toType)
toTypeOrCell.register(Cell)(lambda x : x)

toStaticOrCell = multimethod(defaultProc=toStatic)
toStaticOrCell.register(Cell)(lambda x : x)



#
# unification
#

def unify(a, b) :
    if isType(a) and isType(b) :
        if a.interned and b.interned :
            return a is b
        if a.tag is not b.tag :
            return False
        for x, y in zip(a.params, b.params) :
            if not unify(x, y) :
                return False
        return True
    if isCell(a) :
        if a.param is None :
            a.param = b
            return True
        return unify(a.param, b)
    if isCell(b) :
        if b.param is None :
            b.param = a
            return True
        return unify(a, b.param)
    return equals(a, b)

def matchType(a, b) :
    ensure(unify(a, b), "type mismatch")

def cellDeref(a) :
    while type(a) is Cell :
        a = a.param
    return a

def ensureDeref(a) :
    t = cellDeref(a)
    if t is None :
        error("unresolved variable")
    return t



#
# type pattern matching converters
#

def toValueOfType(pattern) :
    def f(x) :
        v = toValue(x)
        matchType(pattern, v.type)
        return v
    return f

def toReferenceOfType(pattern) :
    def f(x) :
        r = toReference(x)
        matchType(pattern, r.type)
        return r
    return f

def toLValueOfType(pattern) :
    def f(x) :
        v = toLValue(x)
        matchType(pattern, v.type)
        return v
    return f

def toValueOrReferenceOfType(pattern) :
    def f(x) :
        y = toValueOrReference(x)
        matchType(pattern, y.type)
        return y
    return f



#
# type variables
#

def bindTypeVars(parentEnv, typeVars) :
    cells = [Cell() for _ in typeVars]
    env = extendEnv(parentEnv, typeVars, cells)
    return env, cells

def resolveTypeVars(typeVars, cells) :
    typeValues = []
    for typeVar, cell in zip(typeVars, cells) :
        typeValues.append(withContext(typeVar, lambda : ensureDeref(cell)))
    return typeValues



#
# llvmType, llvmTargetData, llvmExecutionEngine, llvmModule
#

def llvmType(t) :
    assert isType(t)
    if t.llvmType is not None :
        return t.llvmType
    llt = makeLLVMType(t.tag, t)
    t.llvmType = llt
    return llt

makeLLVMType = multimethod(errorMessage="invalid type tag")

@makeLLVMType.register(BoolTag)
def foo(tag, t) :
    return llvm.Type.int(8)

@makeLLVMType.register(IntegerTag)
def foo(tag, t) :
    return llvm.Type.int(tag.bits)

@makeLLVMType.register(FloatingPointTag)
def foo(tag, t) :
    if tag.bits == 32 :
        return llvm.Type.float()
    elif tag.bits == 64 :
        return llvm.Type.double()
    else :
        assert False

@makeLLVMType.register(CompilerObjectTag)
def foo(tag, t) :
    error("compiler object types are not supported at runtime")

@makeLLVMType.register(TupleTag)
def foo(tag, t) :
    fieldLLVMTypes = [llvmType(x) for x in t.params]
    return llvm.Type.struct(fieldLLVMTypes)

@makeLLVMType.register(ArrayTag)
def foo(tag, t) :
    ensure(len(t.params) == 2, "invalid array type")
    elementType = llvmType(t.params[0])
    count = toNativeInt(t.params[1])
    return llvm.Type.array(elementType, count)

@makeLLVMType.register(PointerTag)
def foo(tag, t) :
    ensure(len(t.params) == 1, "invalid pointer type")
    pointeeType = llvmType(t.params[0])
    return llvm.Type.pointer(pointeeType)

@makeLLVMType.register(Record)
def foo(tag, t) :
    llt = llvm.Type.opaque()
    typeHandle = llvm.TypeHandle.new(llt)
    t.llvmType = llt
    fieldLLVMTypes = [llvmType(x) for x in recordFieldTypes(t)]
    recType = llvm.Type.struct(fieldLLVMTypes)
    typeHandle.type.refine(recType)
    return typeHandle.type

_llvmTargetData = None
def llvmTargetData() :
    global _llvmTargetData
    if _llvmTargetData is None :
        _llvmTargetData = llvmExecutionEngine().target_data
    return _llvmTargetData

_llvmModuleProvider = None
_llvmExecutionEngine = None
def llvmExecutionEngine() :
    global _llvmModuleProvider, _llvmExecutionEngine
    if _llvmExecutionEngine is None :
        _llvmModuleProvider = llvm.ModuleProvider.new(llvmModule())
        _llvmExecutionEngine = llvm.ExecutionEngine.new(_llvmModuleProvider)
    return _llvmExecutionEngine

_llvmModule = None
def llvmModule() :
    global _llvmModule
    if _llvmModule is None :
        _llvmModule = llvm.Module.new("clay_module")
        _llvmModule.data_layout = str(llvmTargetData())
    return _llvmModule

def _cleanupLLVMGlobals() :
    global _llvmTargetData
    global _llvmModuleProvider, _llvmExecutionEngine
    global _llvmModule
    _llvmTargetData = None
    _llvmModuleProvider = None
    _llvmExecutionEngine = None
    _llvmModule = None

installGlobalsCleanupHook(_cleanupLLVMGlobals)



#
# convert to ctypes
#

def ctypesType(t) :
    assert isType(t)
    if t.ctypesType is not None :
        return t.ctypesType
    ct = makeCTypesType(t.tag, t)
    t.ctypesType = ct
    return ct

makeCTypesType = multimethod(errorMessage="invalid type tag")

@makeCTypesType.register(BoolTag)
def foo(tag, t) :
    return ctypes.c_bool

@makeCTypesType.register(IntegerTag)
def foo(tag, t) :
    s = "c_%sint%d" % (("" if tag.signed else "u"), tag.bits)
    return getattr(ctypes, s)

@makeCTypesType.register(FloatingPointTag)
def foo(tag, t) :
    if tag.bits == 32 :
        return ctypes.c_float
    elif tag.bits == 64 :
        return ctypes.c_double
    else :
        assert False

@makeCTypesType.register(CompilerObjectTag)
def foo(tag, t) :
    return ctypes.c_void_p

@makeCTypesType.register(TupleTag)
def foo(tag, t) :
    fieldCTypes = [ctypesType(x) for x in t.params]
    fieldCNames = ["f%d" % x for x in range(len(t.params))]
    ct = type("Tuple", (ctypes.Structure,), {})
    ct._fields_ = zip(fieldCNames, fieldCTypes)
    return ct

@makeCTypesType.register(ArrayTag)
def foo(tag, t) :
    ensure(len(t.params) == 2, "invalid array type")
    return ctypesType(t.params[0]) * toNativeInt(t.params[1])

@makeCTypesType.register(PointerTag)
def foo(tag, t) :
    ensure(len(t.params) == 1, "invalid pointer type")
    return ctypes.POINTER(ctypesType(t.params[0]))

@makeCTypesType.register(Record)
def foo(tag, t) :
    ct = type("Record", (ctypes.Structure,), {})
    t.ctypesType = ct
    fieldCTypes = [ctypesType(x) for x in recordFieldTypes(t)]
    fieldCNames = [f.name.s for f in recordValueArgs(t.tag)]
    ct._fields_ = zip(fieldCNames, fieldCTypes)
    return ct



#
# typeSize
#

def typeSize(t) :
    return ctypes.sizeof(ctypesType(t))



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

def tempValue(type_) :
    v = Value(type_)
    _tempsBlocks[-1].append(v)
    return v



#
# toNativeInt, toBool
#

toNativeInt = multimethod(errorMessage="int expected")

@toNativeInt.register(Value)
def foo(x) :
    ensure(x.type == nativeIntType, "not int type")
    return x.buf.value

toNativeInt.register(Reference)(lambda x : toNativeInt(toValue(x)))

toBool = multimethod(errorMessage="bool expected")

@toBool.register(Value)
def foo(x) :
    ensure(x.type == boolType, "not bool type")
    return x.buf.value

toBool.register(Reference)(lambda x : toBool(toValue(x)))



#
# boolToValue, intToValue, floatToValue
#

def boolToValue(x) :
    v = tempValue(boolType)
    v.buf.value = x
    return v

def intToValue(x, type_=None) :
    if type_ is None :
        type_ = nativeIntType
    v = tempValue(type_)
    v.buf.value = x
    return v

def floatToValue(x, type_=None) :
    if type_ is None :
        type_ = float64Type
    v = tempValue(type_)
    v.buf.value = x
    return v



#
# compiler object operations
#

_Ptr = ctypes.c_void_p
_PtrPtr = ctypes.POINTER(_Ptr)

def compilerObjectFetch(addr) :
    return ctypes.cast(_Ptr(addr), _PtrPtr).contents

def compilerObjectInit(addr) :
    p = compilerObjectFetch(addr)
    p.value = id(None)
    ctypes.pythonapi.Py_IncRef(p)

def compilerObjectDestroy(addr) :
    p = compilerObjectFetch(addr)
    ctypes.pythonapi.Py_DecRef(p)
    p.value = 0

def compilerObjectCopy(destAddr, srcAddr) :
    dest = compilerObjectFetch(destAddr)
    src = compilerObjectFetch(srcAddr)
    dest.value = src.value
    ctypes.pythonapi.Py_IncRef(dest)

def compilerObjectAssign(destAddr, srcAddr) :
    if destAddr == srcAddr :
        return
    compilerObjectDestroy(destAddr)
    compilerObjectCopy(destAddr, srcAddr)

def makeCompilerObject(x) :
    v = tempValue(compilerObjectType)
    setCompilerObject(toReference(v).address, x)
    return v

def getCompilerObject(addr) :
    p = compilerObjectFetch(addr)
    return ctypes.cast(p, ctypes.py_object).value

def setCompilerObject(addr, x) :
    # assume compiler-object at addr has not been initialized
    p = compilerObjectFetch(addr)
    p.value = id(x)
    ctypes.pythonapi.Py_IncRef(p)

def compilerObjectEquals(addr1, addr2) :
    return getCompilerObject(addr1) == getCompilerObject(addr2)

def compilerObjectHash(addr) :
    return hash(getCompilerObject(addr))



#
# liftCompilerObject, reduceCompilerObject
#

liftCompilerObject = multimethod(defaultProc=lambda x : x)

liftCompilerObject.register(Type)(makeCompilerObject)
liftCompilerObject.register(Record)(makeCompilerObject)
liftCompilerObject.register(Procedure)(makeCompilerObject)
liftCompilerObject.register(Overloadable)(makeCompilerObject)
multimethodRegisterMany(liftCompilerObject, primitivesClassList,
                        makeCompilerObject)

reduceCompilerObject = multimethod(defaultProc=lambda x : x)

@reduceCompilerObject.register(Value)
def foo(x) :
    if isCompilerObjectType(x.type) :
        return getCompilerObject(toReference(x).address)
    return x

@reduceCompilerObject.register(Reference)
def foo(x) :
    if isCompilerObjectType(x.type) :
        return getCompilerObject(x.address)
    return x



#
# core value operations
#

def _callBuiltin(builtinName, args, converter=None) :
    env = Environment(loadedModule("_core").env)
    variables = [Identifier("v%d" % i) for i in range(len(args))]
    for variable, arg in zip(variables, args) :
        addIdent(env, variable, arg)
    argNames = map(NameRef, variables)
    builtin = NameRef(Identifier(builtinName))
    call = Call(builtin, argNames)
    if converter is None :
        return evaluateRootExpr(call, env)
    return evaluateRootExpr(call, env, converter)

def valueInit(a) :
    if isSimpleType(a.type) :
        return
    if isCompilerObjectType(a.type) :
        compilerObjectInit(a.address)
        return
    _callBuiltin("init", [a])

def valueCopy(dest, src) :
    assert dest.type == src.type
    if isSimpleType(dest.type) :
        _simpleValueCopy(dest, src)
        return
    if isCompilerObjectType(dest.type) :
        compilerObjectCopy(dest.address, src.address)
        return
    _callBuiltin("copy", [dest, src])

def valueAssign(dest, src) :
    if isSimpleType(dest.type) and (dest.type == src.type) :
        _simpleValueCopy(dest, src)
        return
    if isCompilerObjectType(dest.type) and isCompilerObjectType(src.type) :
        compilerObjectAssign(dest.address, src.address)
        return
    _callBuiltin("assign", [dest, src])

def _simpleValueCopy(dest, src) :
    destPtr = ctypes.c_void_p(dest.address)
    srcPtr = ctypes.c_void_p(src.address)
    size = typeSize(dest.type)
    ctypes.memmove(destPtr, srcPtr, size)

def valueDestroy(a) :
    if isSimpleType(a.type) :
        _simpleValueDestroy(a)
        return
    if isCompilerObjectType(a.type) :
        compilerObjectDestroy(a.address)
        return
    _callBuiltin("destroy", [a])

def _simpleValueDestroy(a) :
    ptr = ctypes.c_void_p(a.address)
    ctypes.memset(ptr, 0, typeSize(a.type))

def valueEquals(a, b) :
    # TODO: add bypass for simple types
    if isCompilerObjectType(a.type) and isCompilerObjectType(b.type) :
        return compilerObjectEquals(a.address, b.address)
    return _callBuiltin("equals?", [a, b], toBool)

def valueHash(a) :
    if isCompilerObjectType(a.type) :
        return compilerObjectHash(a.address)
    return _callBuiltin("hash", [a], toNativeInt)



#
# arrays, tuples, records
#

def arrayRef(a, i) :
    assert isReference(a)
    cell, sizeCell = Cell(), Cell()
    matchType(arrayTypePattern(cell, sizeCell), a.type)
    elementType = cell.param
    return Reference(elementType, a.address + i*typeSize(elementType))

def makeArray(argRefs) :
    assert len(argRefs) > 0
    t = arrayType(argRefs[0].type, intToValue(len(argRefs)))
    value = tempValue(t)
    valueRef = toReference(value)
    for i, argRef in enumerate(argRefs) :
        left = arrayRef(valueRef, i)
        valueCopy(left, argRef)
    return value

def tupleFieldRef(a, i) :
    assert isReference(a) and isTupleType(a.type)
    ensure((0 <= i < len(a.type.params)), "tuple index out of range")
    fieldName = "f%d" % i
    ctypesField = getattr(ctypesType(a.type), fieldName)
    fieldType = toType(a.type.params[i])
    return Reference(fieldType, a.address + ctypesField.offset)

def makeTuple(argRefs) :
    t = tupleType(tuple([x.type for x in argRefs]))
    value = tempValue(t)
    valueRef = toReference(value)
    for i, argRef in enumerate(argRefs) :
        left = tupleFieldRef(valueRef, i)
        valueCopy(left, argRef)
    return value

def recordValueArgs(r) :
    if r.valueArgs_ is None :
        r.valueArgs_ = [a for a in r.args if type(a) is ValueRecordArg]
    return r.valueArgs_

def recordValueNames(r) :
    if r.valueNames_ is None :
        names = {}
        for i, valueArg in enumerate(recordValueArgs(r)) :
            if valueArg.name.s in names :
                withContext(valueArg, lambda : error("duplicate field: %s" %
                                                     valueArg.name.s))
            names[valueArg.name.s] = i
        r.valueNames_ = names
    return r.valueNames_

def recordFieldRef(a, i) :
    assert isReference(a) and isRecordType(a.type)
    valueArgs = recordValueArgs(a.type.tag)
    nFields = len(valueArgs)
    ensure((0 <= i < nFields), "record field index out of range")
    fieldName = valueArgs[i].name.s
    fieldType = recordFieldTypes(a.type)[i]
    ctypesField = getattr(ctypesType(a.type), fieldName)
    return Reference(fieldType, a.address + ctypesField.offset)

def recordFieldIndex(recType, ident) :
    valueNames = recordValueNames(recType.tag)
    i = valueNames.get(ident.s)
    if i is not None :
        return i
    error("record field not found: %s" % ident.s)

def makeRecord(recType, argRefs) :
    value = tempValue(recType)
    valueRef = toReference(value)
    for i, argRef in enumerate(argRefs) :
        left = recordFieldRef(valueRef, i)
        valueCopy(left, argRef)
    return value

_recordFieldTypes = {}
def recordFieldTypes(recType) :
    assert isType(recType)
    fieldTypes = _recordFieldTypes.get(recType)
    if fieldTypes is None :
        record = recType.tag
        env = extendEnv(record.env, record.typeVars, recType.params)
        valueArgs = recordValueArgs(record)
        fieldTypes = [evaluateRootExpr(x.type, env, toType) for x in valueArgs]
        _recordFieldTypes[recType] = fieldTypes
    return fieldTypes

def _cleanupRecordFieldTypes() :
    global _recordFieldTypes
    _recordFieldTypes = {}

installGlobalsCleanupHook(_cleanupRecordFieldTypes)



#
# value printer
#

def makeXConvertBuiltin(suffix) :
    def xconvert(r) :
        v = toValue(r)
        return XSymbol(str(v.buf.value) + suffix)
    return xconvert

def xconvertBool(r) :
    v = toValue(r)
    return XSymbol("true" if v.buf.value else "false")

def xconvertPointer(r) :
    return XObject("Ptr", r.type.params[0])

def xconvertTuple(r) :
    elements = [tupleFieldRef(r, i) for i in range(len(r.type.params))]
    return tuple(map(xconvertReference, elements))

def xconvertArray(r) :
    arraySize = toNativeInt(r.type.params[1])
    elements = [arrayRef(r, i) for i in range(arraySize)]
    return map(xconvertReference, elements)

def xconvertRecord(r) :
    fieldCount = len(recordValueArgs(r.type.tag))
    fields = [recordFieldRef(r, i) for i in range(fieldCount)]
    fields = map(xconvertReference, fields)
    return XObject(r.type.tag.name.s, *fields)

def xconvertValue(x) :
    return xconvertReference(toReference(x))

def xconvertReference(x) :
    converter = getXConverter(x.type.tag)
    pushTempsBlock()
    try :
        if converter is not None :
            return converter(x)
        if isRecordType(x.type) :
            return xconvertRecord(x)
        return x
    finally :
        popTempsBlock()

xregister(Value, xconvertValue)
xregister(Reference, xconvertReference)

def getXConverter(tag) :
    if not _xconverters :
        _xconverters[boolTag] = xconvertBool
        for t in [int8Tag, int16Tag, int32Tag, int64Tag,
                  uint8Tag, uint16Tag, uint32Tag, uint64Tag,
                  float32Tag, float64Tag] :
            if type(t) is IntegerTag :
                if t.signed :
                    suffix = "#i%d" % t.bits
                else :
                    suffix = "#u%d" % t.bits
            else :
                assert type(t) is FloatingPointTag
                suffix = "#f%d" % t.bits
            _xconverters[t] = makeXConvertBuiltin(suffix)
        _xconverters[pointerTag] = xconvertPointer
        _xconverters[tupleTag] = xconvertTuple
        _xconverters[arrayTag] = xconvertArray
    return _xconverters.get(tag)

_xconverters = {}



#
# evaluateRootExpr, evaluatePattern
#

def evaluateRootExpr(expr, env, converter=(lambda x : x)) :
    pushTempsBlock()
    try :
        return evaluate(expr, env, converter)
    finally :
        popTempsBlock()

def evaluatePattern(expr, env, converter) :
    enableInternedTypes(False)
    try :
        return evaluateRootExpr(expr, env, converter)
    finally :
        restoreInternedTypes()



#
# evaluate
#

def evaluate(expr, env, converter=(lambda x : x)) :
    return withContext(expr, lambda : converter(evaluate2(expr, env)))



#
# evaluate2
#

evaluate2 = multimethod(errorMessage="invalid expression")

@evaluate2.register(BoolLiteral)
def foo(x, env) :
    return boolToValue(x.value)

_suffixTypes = {
    "i8"  : int8Type,
    "i16" : int16Type,
    "i32" : int32Type,
    "i64" : int64Type,
    "u8"  : uint8Type,
    "u16" : uint16Type,
    "u32" : uint32Type,
    "u64" : uint64Type,
    "f32" : float32Type,
    "f64" : float64Type}

@evaluate2.register(IntLiteral)
def foo(x, env) :
    if x.suffix is None :
        return intToValue(x.value)
    type_ = _suffixTypes.get(x.suffix)
    ensure(type_ is not None, "invalid suffix: %s" % x.suffix)
    if isFloatingPointType(type_) :
        return floatToValue(x.value, type_)
    return intToValue(x.value, type_)

@evaluate2.register(FloatLiteral)
def foo(x, env) :
    if x.suffix is None :
        return floatToValue(x.value)
    type_ = _suffixTypes.get(x.suffix)
    ensure(type_ is not None, "invalid suffix: %s" % x.suffix)
    ensure(not isIntegerType(type_), "floating point suffix expected")
    return floatToValue(x.value, type_)

@evaluate2.register(CharLiteral)
def foo(x, env) :
    assert len(x.value) == 1
    return evaluate(convertCharLiteral(x), env)

@evaluate2.register(StringLiteral)
def foo(x, env) :
    return evaluate(convertStringLiteral(x), env)

@evaluate2.register(NameRef)
def foo(x, env) :
    return evaluateNameRef(lookupIdent(env, x.name))

@evaluate2.register(Tuple)
def foo(x, env) :
    if len(x.args) == 1 :
        return evaluate(x.args[0], env)
    return evaluate(Call(primitiveNameRef("tuple"), x.args), env)

@evaluate2.register(Array)
def foo(x, env) :
    return evaluate(Call(primitiveNameRef("array"), x.args), env)

@evaluate2.register(Indexing)
def foo(x, env) :
    thing = evaluate(x.expr, env)
    thing = reduceCompilerObject(thing)
    return evaluateIndexing(thing, x.args, env)

@evaluate2.register(Call)
def foo(x, env) :
    thing = evaluate(x.expr, env)
    thing = reduceCompilerObject(thing)
    return evaluateCall(thing, x.args, env)

@evaluate2.register(FieldRef)
def foo(x, env) :
    thingRef = evaluate(x.expr, env, toRecordReference)
    return recordFieldRef(thingRef, recordFieldIndex(thingRef.type, x.name))

@evaluate2.register(TupleRef)
def foo(x, env) :
    toTupleReference = toReferenceWithTag(tupleTag)
    thingRef = evaluate(x.expr, env, toTupleReference)
    return tupleFieldRef(thingRef, x.index)

@evaluate2.register(Dereference)
def foo(x, env) :
    primOp = primitiveNameRef("pointerDereference")
    return evaluate(Call(primOp, [x.expr]), env)

@evaluate2.register(AddressOf)
def foo(x, env) :
    return evaluate(Call(primitiveNameRef("addressOf"), [x.expr]), env)

@evaluate2.register(UnaryOpExpr)
def foo(x, env) :
    return evaluate(convertUnaryOpExpr(x), env)

@evaluate2.register(BinaryOpExpr)
def foo(x, env) :
    return evaluate(convertBinaryOpExpr(x), env)

@evaluate2.register(NotExpr)
def foo(x, env) :
    v = evaluate(x.expr, env, toBool)
    return boolToValue(not v)

@evaluate2.register(AndExpr)
def foo(x, env) :
    v = evaluate(x.expr1, env, toBool)
    if not v :
        return boolToValue(False)
    return boolToValue(evaluate(x.expr2, env, toBool))

@evaluate2.register(OrExpr)
def foo(x, env) :
    v = evaluate(x.expr1, env, toBool)
    if v :
        return boolToValue(True)
    return boolToValue(evaluate(x.expr2, env, toBool))

@evaluate2.register(StaticExpr)
def foo(x, env) :
    return evaluate(x.expr, env, toStatic)

@evaluate2.register(SCExpression)
def foo(x, env) :
    return evaluate(x.expr, x.env)



#
# convertCharLiteral, convertStringLiteral
#

def convertCharLiteral(x) :
    return convertCharLiteral2(x.value)

def convertCharLiteral2(c) :
    nameRef = NameRef(Identifier("Char"))
    nameRef = SCExpression(loadedModule("_char").env, nameRef)
    return Call(nameRef, [IntLiteral(ord(c), "i8")])

def convertStringLiteral(x) :
    nameRef = NameRef(Identifier("string"))
    nameRef = SCExpression(loadedModule("_string").env, nameRef)
    charArray = Array([convertCharLiteral2(c) for c in x.value])
    return Call(nameRef, [charArray])



#
# convertUnaryOpExpr, convertBinaryOpExpr
#

_unaryOps = {"+" : "plus",
             "-" : "minus"}

def convertUnaryOpExpr(x) :
    return Call(coreNameRef(_unaryOps[x.op]), [x.expr])

_binaryOps = {"+" : "add",
              "-" : "subtract",
              "*" : "multiply",
              "/" : "divide",
              "%" : "remainder",
              "==" : "equals?",
              "!=" : "notEquals?",
              "<" : "lesser?",
              "<=" : "lesserEquals?",
              ">" : "greater?",
              ">=" : "greaterEquals?"}

def convertBinaryOpExpr(x) :
    return Call(coreNameRef(_binaryOps[x.op]), [x.expr1, x.expr2])



#
# evaluateNameRef
#

evaluateNameRef = multimethod(defaultProc=(lambda x : x))

@evaluateNameRef.register(Value)
def foo(x) :
    return toReference(x)

@evaluateNameRef.register(Record)
def foo(x) :
    if (len(x.typeVars) == 0) :
        return recordType(x, ())
    return x



#
# evaluateIndexing
#

evaluateIndexing = multimethod(errorMessage="invalid indexing")

@evaluateIndexing.register(Record)
def foo(x, args, env) :
    ensureArity(args, len(x.typeVars))
    typeParams = [evaluate(y, env, toStaticOrCell) for y in args]
    return recordType(x, tuple(typeParams))

@evaluateIndexing.register(primitives.Tuple)
def foo(x, args, env) :
    ensure(len(args) > 1, "tuple types need atleast two member types")
    elementTypes = [evaluate(y, env, toTypeOrCell) for y in args]
    return tupleType(tuple(elementTypes))

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

@evaluateCall.register(Type)
def foo(x, args, env) :
    if len(args) == 0 :
        v = tempValue(x)
        valueInit(toReference(v))
        return v
    elif isRecordType(x) :
        fieldTypes = recordFieldTypes(x)
        ensureArity(args, len(fieldTypes))
        argRefs = []
        for arg, fieldType in zip(args, fieldTypes) :
            argRef = evaluate(arg, env, toReferenceOfType(fieldType))
            argRefs.append(argRef)
        return makeRecord(x, argRefs)
    elif isArrayType(x) :
        elementType = toType(x.params[0])
        ensureArity(args, toNativeInt(x.params[1]))
        converter = toReferenceOfType(elementType)
        argRefs = [evaluate(y, env, converter) for y in args]
        return makeArray(argRefs)
    elif isTupleType(x) :
        ensureArity(args, len(x.params))
        argRefs = []
        for arg, param in zip(args, x.params) :
            argRef = evaluate(arg, env, toReferenceOfType(toType(param)))
            argRefs.append(argRef)
        return makeTuple(argRefs)
    else :
        error("only array, tuple, and record types " +
              "can be constructed this way.")

@evaluateCall.register(Record)
def foo(x, args, env) :
    if len(args) == 0 :
        v = tempValue(toType(x))
        valueInit(toReference(v))
        return v
    actualArgs = [ActualArgument(y, env) for y in args]
    result = matchRecordInvoke(x, actualArgs)
    if type(result) is InvokeError :
        result.signalError()
    assert type(result) is InvokeBindings
    recType = recordType(x, tuple(result.typeParams))
    return makeRecord(recType, result.params)

@evaluateCall.register(Procedure)
def foo(x, args, env) :
    actualArgs = [ActualArgument(y, env) for y in args]
    result = matchInvoke(x.code, x.env, actualArgs)
    if type(result) is InvokeError :
        result.signalError()
    assert type(result) is InvokeBindings
    return evalInvoke(x.code, x.env, result)

@evaluateCall.register(Overloadable)
def foo(x, args, env) :
    actualArgs = [ActualArgument(y, env) for y in args]
    for y in x.overloads :
        result = matchInvoke(y.code, y.env, actualArgs)
        if type(result) is InvokeError :
            continue
        assert type(result) is InvokeBindings
        return evalInvoke(y.code, y.env, result)
    error("no matching overload")

@evaluateCall.register(ExternalProcedure)
def foo(x, args, env) :
    initExternalProcedure(x)
    ensureArity(args, len(x.args))
    argRefs = []
    for arg, externalArg in zip(args, x.args) :
        expectedType = evaluateRootExpr(externalArg.type, x.env, toType)
        argRef = evaluate(arg, env, toReferenceOfType(expectedType))
        argRefs.append(argRef)
    returnType = evaluateRootExpr(x.returnType, x.env, toType)
    return evalLLVMCall(x.llvmFunc, argRefs, returnType)



#
# makeExternalFunc
#

def initExternalProcedure(x) :
    if x.llvmFunc is not None :
        return
    argTypes = [evaluateRootExpr(y.type, x.env, toType) for y in x.args]
    returnType = evaluateRootExpr(x.returnType, x.env, toType)

    # generate external func
    llvmArgTypes = [llvmType(y) for y in argTypes]
    if isVoidType(returnType) :
        llvmReturnType = llvm.Type.void()
    else :
        llvmReturnType = llvmType(returnType)
    llvmFuncType = llvm.Type.function(llvmReturnType, llvmArgTypes)
    func = llvmModule().add_function(llvmFuncType, x.name.s)
    func.linkage = llvm.LINKAGE_EXTERNAL

    # generate wrapper
    llvmArgTypes2 = [llvmType(pointerType(y)) for y in argTypes]
    if not isVoidType(returnType) :
        llvmArgTypes2.append(llvmType(pointerType(returnType)))
    llvmFuncType2 = llvm.Type.function(llvm.Type.void(), llvmArgTypes2)
    func2 = llvmModule().add_function(llvmFuncType2, "wrapper_%s" % x.name.s)
    func2.linkage = llvm.LINKAGE_INTERNAL
    block = func2.append_basic_block("code")
    builder = llvm.Builder.new(block)
    llvmArgs = [builder.load(func2.args[i]) for i in range(len(argTypes))]
    result = builder.call(func, llvmArgs)
    if not isVoidType(returnType) :
        builder.store(result, func2.args[-1])
    builder.ret_void()

    x.llvmFunc = func2



#
# codegen for primitives
#

_primitiveFuncs = {}

def primitiveFunc(primitive, inputTypes, outputType) :
    key = (primitive, tuple(inputTypes), outputType)
    func = _primitiveFuncs.get(key)
    if func is None :
        func = codegenPrimitive(primitive, inputTypes, outputType)
        _primitiveFuncs[key] = func
    return func

def _cleanPrimitiveFuncs() :
    global _primitiveFuncs
    _primitiveFuncs = {}

installGlobalsCleanupHook(_cleanPrimitiveFuncs)

def codegenPrimitive(primitive, inputTypes, outputType) :
    llvmArgs = [llvmType(pointerType(inputType)) for inputType in inputTypes]
    if not isVoidType(outputType) :
        llvmArgs.append(llvmType(pointerType(outputType)))
    llvmFuncType = llvm.Type.function(llvm.Type.void(), llvmArgs)
    name = "clayprim_%s" % primitive.name
    func = llvmModule().add_function(llvmFuncType, name)
    func.linkage = llvm.LINKAGE_INTERNAL
    block = func.append_basic_block("code")
    builder = llvm.Builder.new(block)
    codegenPrimitiveBody(primitive, inputTypes, outputType, builder, func)
    return func


codegenPrimitiveBody = multimethod(errorMessage="invalid primitive")

@codegenPrimitiveBody.register(primitives.primitiveCopy)
def foo(x, inputTypes, outputType, builder, func) :
    v = builder.load(func.args[1])
    builder.store(v, func.args[0])
    builder.ret_void()

@codegenPrimitiveBody.register(primitives.pointerToInt)
def foo(x, inputTypes, outputType, builder, func) :
    ptr = builder.load(func.args[0])
    i = builder.ptrtoint(ptr, llvmType(outputType))
    builder.store(i, func.args[-1])
    builder.ret_void()

@codegenPrimitiveBody.register(primitives.intToPointer)
def foo(x, inputTypes, outputType, builder, func) :
    i = builder.load(func.args[0])
    ptr = builder.inttoptr(i, llvmType(outputType))
    builder.store(ptr, func.args[-1])
    builder.ret_void()

@codegenPrimitiveBody.register(primitives.allocateMemory)
def foo(x, inputTypes, outputType, builder, func) :
    n = builder.load(func.args[0])
    llt = llvmType(outputType.params[0])
    ptr = builder.malloc_array(llt, n)
    builder.store(ptr, func.args[-1])
    builder.ret_void()

@codegenPrimitiveBody.register(primitives.freeMemory)
def foo(x, inputTypes, outputType, builder, func) :
    ptr = builder.load(func.args[0])
    builder.free(ptr)
    builder.ret_void()

@codegenPrimitiveBody.register(getattr(primitives, "numericEquals?"))
def foo(x, inputTypes, outputType, builder, func) :
    x1 = builder.load(func.args[0])
    x2 = builder.load(func.args[1])
    if isIntegerType(inputTypes[0]) :
        cond = builder.icmp(llvm.ICMP_EQ, x1, x2)
    elif isFloatingPointType(inputTypes[0]) :
        cond = builder.fcmp(llvm.FCMP_OEQ, x1, x2)
    else :
        assert False
    result = builder.zext(cond, llvmType(outputType))
    builder.store(result, func.args[-1])
    builder.ret_void()

@codegenPrimitiveBody.register(getattr(primitives, "numericLesser?"))
def foo(x, inputTypes, outputType, builder, func) :
    x1 = builder.load(func.args[0])
    x2 = builder.load(func.args[1])
    if isIntegerType(inputTypes[0]) :
        if inputTypes[0].tag.signed :
            cond = builder.icmp(llvm.ICMP_SLT, x1, x2)
        else :
            cond = builder.icmp(llvm.ICMP_ULT, x1, x2)
    elif isFloatingPointType(inputTypes[0]) :
        cond = builder.fcmp(llvm.FCMP_OLT, x1, x2)
    else :
        assert False
    result = builder.zext(cond, llvmType(outputType))
    builder.store(result, func.args[-1])
    builder.ret_void()

@codegenPrimitiveBody.register(primitives.numericAdd)
def foo(x, inputTypes, outputType, builder, func) :
    x1 = builder.load(func.args[0])
    x2 = builder.load(func.args[1])
    if isIntegerType(inputTypes[0]) :
        result = builder.add(x1, x2)
    elif isFloatingPointType(inputTypes[0]) :
        result = builder.fadd(x1, x2)
    else :
        assert False
    builder.store(result, func.args[-1])
    builder.ret_void()

@codegenPrimitiveBody.register(primitives.numericSubtract)
def foo(x, inputTypes, outputType, builder, func) :
    x1 = builder.load(func.args[0])
    x2 = builder.load(func.args[1])
    if isIntegerType(inputTypes[0]) :
        result = builder.sub(x1, x2)
    elif isFloatingPointType(inputTypes[0]) :
        result = builder.fsub(x1, x2)
    else :
        assert False
    builder.store(result, func.args[-1])
    builder.ret_void()

@codegenPrimitiveBody.register(primitives.numericMultiply)
def foo(x, inputTypes, outputType, builder, func) :
    x1 = builder.load(func.args[0])
    x2 = builder.load(func.args[1])
    if isIntegerType(inputTypes[0]) :
        result = builder.mul(x1, x2)
    elif isFloatingPointType(inputTypes[0]) :
        result = builder.fmul(x1, x2)
    else :
        assert False
    builder.store(result, func.args[-1])
    builder.ret_void()

@codegenPrimitiveBody.register(primitives.numericMultiply)
def foo(x, inputTypes, outputType, builder, func) :
    x1 = builder.load(func.args[0])
    x2 = builder.load(func.args[1])
    if isIntegerType(inputTypes[0]) :
        result = builder.mul(x1, x2)
    elif isFloatingPointType(inputTypes[0]) :
        result = builder.fmul(x1, x2)
    else :
        assert False
    builder.store(result, func.args[-1])
    builder.ret_void()

@codegenPrimitiveBody.register(primitives.numericDivide)
def foo(x, inputTypes, outputType, builder, func) :
    x1 = builder.load(func.args[0])
    x2 = builder.load(func.args[1])
    if isIntegerType(inputTypes[0]) :
        if inputTypes[0].tag.signed :
            result = builder.sdiv(x1, x2)
        else :
            result = builder.udiv(x1, x2)
    elif isFloatingPointType(inputTypes[0]) :
        result = builder.fdiv(x1, x2)
    else :
        assert False
    builder.store(result, func.args[-1])
    builder.ret_void()

@codegenPrimitiveBody.register(primitives.numericRemainder)
def foo(x, inputTypes, outputType, builder, func) :
    x1 = builder.load(func.args[0])
    x2 = builder.load(func.args[1])
    if isIntegerType(inputTypes[0]) :
        if inputTypes[0].tag.signed :
            result = builder.srem(x1, x2)
        else :
            result = builder.urem(x1, x2)
    elif isFloatingPointType(inputTypes[0]) :
        result = builder.frem(x1, x2)
    else :
        assert False
    builder.store(result, func.args[-1])
    builder.ret_void()

@codegenPrimitiveBody.register(primitives.numericNegate)
def foo(x, inputTypes, outputType, builder, func) :
    x1 = builder.load(func.args[0])
    if isIntegerType(inputTypes[0]) :
        result = builder.neg(x1)
    elif isFloatingPointType(inputTypes[0]) :
        result = builder.fneg(x1)
    else :
        assert False
    builder.store(result, func.args[-1])
    builder.ret_void()

@codegenPrimitiveBody.register(primitives.numericConvert)
def foo(x, inputTypes, outputType, builder, func) :
    x1 = builder.load(func.args[0])
    inType = inputTypes[0]
    outType = outputType
    if isIntegerType(inType) :
        if isIntegerType(outType) :
            if outType.tag.bits < inType.tag.bits :
                result = builder.trunc(x1, llvmType(outType))
            elif outType.tag.bits > inType.tag.bits :
                if inType.tag.signed :
                    result = builder.sext(x1, llvmType(outType))
                else :
                    result = builder.zext(x1, llvmType(outType))
            else :
                result = x1
        elif isFloatingPointType(outType) :
            if inType.tag.signed :
                result = builder.sitofp(x1, llvmType(outType))
            else :
                result = builder.uitofp(x1, llvmType(outType))
        else :
            assert False
    elif isFloatingPointType(inType) :
        if isIntegerType(outType) :
            if outType.tag.signed :
                result = builder.fptosi(x1, llvmType(outType))
            else :
                result = builder.fptoui(x1, llvmType(outType))
        elif isFloatingPointType(outType) :
            if outType.tag.bits < inType.tag.bits :
                result = builder.fptrunc(x1, llvmType(outType))
            elif outType.tag.bits > inType.tag.bits :
                result = builder.fpext(x1, llvmType(outType))
            else :
                result = x1
        else :
            assert False
    else :
        assert False
    builder.store(result, func.args[-1])
    builder.ret_void()



#
# evalPrimitiveCall, evalLLVMCall
#

def evalPrimitiveCall(primitive, inRefs, outputType) :
    func = primitiveFunc(primitive, [x.type for x in inRefs], outputType)
    return evalLLVMCall(func, inRefs, outputType)

def evalLLVMCall(func, inRefs, outputType) :
    llvmArgs = []
    for inRef in inRefs :
        llt = llvmType(pointerType(inRef.type))
        gv = llvm.GenericValue.pointer(llt, inRef.address)
        llvmArgs.append(gv)
    if not isVoidType(outputType) :
        output = tempValue(outputType)
        outputRef = toReference(output)
        llt = llvmType(pointerType(outputType))
        gv = llvm.GenericValue.pointer(llt, outputRef.address)
        llvmArgs.append(gv)
    else :
        output = voidValue
    ee = llvmExecutionEngine()
    ee.run_function(func, llvmArgs)
    return output



#
# evaluate primitives
#

@evaluateCall.register(primitives.typeSize)
def foo(x, args, env) :
    ensureArity(args, 1)
    t = evaluate(args[0], env, toType)
    return intToValue(typeSize(t))

@evaluateCall.register(primitives.primitiveCopy)
def foo(x, args, env) :
    ensureArity(args, 2)
    cell = Cell()
    destRef = evaluate(args[0], env, toReferenceOfType(cell))
    srcRef = evaluate(args[1], env, toReferenceOfType(cell))
    ensure(isSimpleType(cell.param), "expected simple type")
    return evalPrimitiveCall(x, [destRef, srcRef], voidType)



#
# evaluate compiler object primitives
#

@evaluateCall.register(primitives.compilerObjectInit)
def foo(x, args, env) :
    ensureArity(args, 1)
    a = evaluate(args[0], env, toReferenceOfType(compilerObjectType))
    compilerObjectInit(a.address)
    return voidValue

@evaluateCall.register(primitives.compilerObjectDestroy)
def foo(x, args, env) :
    ensureArity(args, 1)
    a = evaluate(args[0], env, toReferenceOfType(compilerObjectType))
    compilerObjectDestroy(a.address)
    return voidValue

@evaluateCall.register(primitives.compilerObjectCopy)
def foo(x, args, env) :
    ensureArity(args, 2)
    dest = evaluate(args[0], env, toReferenceOfType(compilerObjectType))
    src = evaluate(args[1], env, toReferenceOfType(compilerObjectType))
    compilerObjectCopy(dest.address, src.address)
    return voidValue

@evaluateCall.register(primitives.compilerObjectAssign)
def foo(x, args, env) :
    ensureArity(args, 2)
    dest = evaluate(args[0], env, toReferenceOfType(compilerObjectType))
    src = evaluate(args[1], env, toReferenceOfType(compilerObjectType))
    compilerObjectAssign(dest.address, src.address)
    return voidValue

@evaluateCall.register(getattr(primitives, "compilerObjectEquals?"))
def foo(x, args, env) :
    ensureArity(args, 2)
    a = evaluate(args[0], env, toReferenceOfType(compilerObjectType))
    b = evaluate(args[1], env, toReferenceOfType(compilerObjectType))
    return boolToValue(compilerObjectEquals(a.address, b.address))

@evaluateCall.register(primitives.compilerObjectHash)
def foo(x, args, env) :
    ensureArity(args, 1)
    a = evaluate(args[0], env, toReferenceOfType(compilerObjectType))
    return intToValue(compilerObjectHash(a.address))



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
    ptr = evaluate(args[0], env, toValueOfType(pointerTypePattern(cell)))
    return Reference(cell.param, pointerToAddress(ptr))

@evaluateCall.register(primitives.pointerToInt)
def foo(x, args, env) :
    ensureArity(args, 2)
    cell = Cell()
    converter = toReferenceOfType(pointerTypePattern(cell))
    ptrRef = evaluate(args[0], env, converter)
    outType = evaluate(args[1], env, toIntegerType)
    return evalPrimitiveCall(x, [ptrRef], outType)

@evaluateCall.register(primitives.intToPointer)
def foo(x, args, env) :
    ensureArity(args, 2)
    intRef = evaluate(args[0], env, toIntegerReference)
    pointeeType = evaluate(args[1], env, toType)
    return evalPrimitiveCall(x, [intRef], pointerType(pointeeType))

@evaluateCall.register(primitives.pointerCast)
def foo(x, args, env) :
    ensureArity(args, 2)
    cell = Cell()
    targetType = evaluate(args[0], env, toType)
    ptr = evaluate(args[1], env, toValueOfType(pointerTypePattern(cell)))
    return addressToPointer(pointerToAddress(ptr), pointerType(targetType))

@evaluateCall.register(primitives.allocateMemory)
def foo(x, args, env) :
    ensureArity(args, 2)
    type_ = evaluate(args[0], env, toType)
    nRef = evaluate(args[1], env, toReferenceOfType(nativeIntType))
    return evalPrimitiveCall(x, [nRef], pointerType(type_))

@evaluateCall.register(primitives.freeMemory)
def foo(x, args, env) :
    ensureArity(args, 1)
    cell = Cell()
    converter = toReferenceOfType(pointerTypePattern(cell))
    ptrRef = evaluate(args[0], env, converter)
    return evalPrimitiveCall(x, [ptrRef], voidType)



#
# evaluate tuple primitives
#

@evaluateCall.register(getattr(primitives, "TupleType?"))
def foo(x, args, env) :
    ensureArity(args, 1)
    t = evaluate(args[0], env, toType)
    return boolToValue(isTupleType(t))

@evaluateCall.register(primitives.tuple)
def foo(x, args, env) :
    ensure(len(args) > 1, "tuples need atleast two members")
    argRefs = [evaluate(y, env, toReference) for y in args]
    return makeTuple(argRefs)

@evaluateCall.register(primitives.tupleFieldCount)
def foo(x, args, env) :
    ensureArity(args, 1)
    t = evaluate(args[0], env, toTypeWithTag(tupleTag))
    return intToValue(len(t.params))

@evaluateCall.register(primitives.tupleFieldRef)
def foo(x, args, env) :
    ensureArity(args, 2)
    tupleRef = evaluate(args[0], env, toReferenceWithTag(tupleTag))
    i = evaluate(args[1], env, toNativeInt)
    return tupleFieldRef(tupleRef, i)



#
# evaluate array primitives
#

@evaluateCall.register(primitives.array)
def foo(x, args, env) :
    ensure(len(args) > 0, "empty array expressions are not allowed")
    firstArg = evaluate(args[0], env, toReference)
    converter = toReferenceOfType(firstArg.type)
    argRefs = [evaluate(y, env, converter) for y in args[1:]]
    argRefs.insert(0, firstArg)
    return makeArray(argRefs)

@evaluateCall.register(primitives.arrayRef)
def foo(x, args, env) :
    ensureArity(args, 2)
    a = evaluate(args[0], env, toReferenceWithTag(arrayTag))
    i = evaluate(args[1], env, toNativeInt)
    return arrayRef(a, i)



#
# evaluate record primitives
#

@evaluateCall.register(getattr(primitives, "RecordType?"))
def foo(x, args, env) :
    ensureArity(args, 1)
    t = evaluate(args[0], env, toType)
    return boolToValue(isRecordType(t))

@evaluateCall.register(primitives.recordFieldCount)
def foo(x, args, env) :
    ensureArity(args, 1)
    t = evaluate(args[0], env, toRecordType)
    return intToValue(len(recordValueArgs(t.tag)))

@evaluateCall.register(primitives.recordFieldRef)
def foo(x, args, env) :
    ensureArity(args, 2)
    recRef = evaluate(args[0], env, toRecordReference)
    i = evaluate(args[1], env, toNativeInt)
    return recordFieldRef(recRef, i)



#
# evaluate numeric primitives
#

def _eval2NumericArgs(args, env) :
    ensureArity(args, 2)
    a, b = [evaluate(x, env, toNumericReference) for x in args]
    ensure(a.type == b.type, "argument types mismatch")
    return (a, b)

@evaluateCall.register(getattr(primitives, "numericEquals?"))
def foo(x, args, env) :
    x1, x2 = _eval2NumericArgs(args, env)
    return evalPrimitiveCall(x, [x1, x2], boolType)

@evaluateCall.register(getattr(primitives, "numericLesser?"))
def foo(x, args, env) :
    x1, x2 = _eval2NumericArgs(args, env)
    return evalPrimitiveCall(x, [x1, x2], boolType)

@evaluateCall.register(primitives.numericAdd)
def foo(x, args, env) :
    x1, x2 = _eval2NumericArgs(args, env)
    return evalPrimitiveCall(x, [x1, x2], x1.type)

@evaluateCall.register(primitives.numericSubtract)
def foo(x, args, env) :
    x1, x2 = _eval2NumericArgs(args, env)
    return evalPrimitiveCall(x, [x1, x2], x1.type)

@evaluateCall.register(primitives.numericMultiply)
def foo(x, args, env) :
    x1, x2 = _eval2NumericArgs(args, env)
    return evalPrimitiveCall(x, [x1, x2], x1.type)

@evaluateCall.register(primitives.numericDivide)
def foo(x, args, env) :
    x1, x2 = _eval2NumericArgs(args, env)
    return evalPrimitiveCall(x, [x1, x2], x1.type)

@evaluateCall.register(primitives.numericRemainder)
def foo(x, args, env) :
    x1, x2 = _eval2NumericArgs(args, env)
    return evalPrimitiveCall(x, [x1, x2], x1.type)

@evaluateCall.register(primitives.numericNegate)
def foo(x, args, env) :
    ensureArity(args, 1)
    x1 = evaluate(args[0], env, toNumericReference)
    return evalPrimitiveCall(x, [x1], x1.type)

@evaluateCall.register(primitives.numericConvert)
def foo(x, args, env) :
    ensureArity(args, 2)
    destType = evaluate(args[0], env, toNumericType)
    src = evaluate(args[1], env, toNumericReference)
    return evalPrimitiveCall(x, [src], destType)



#
# wrapper for actual arguments
#

class ActualArgument(object) :
    def __init__(self, expr, env) :
        self.expr = expr
        self.env = env
        self.result_ = evaluate(self.expr, self.env)

    def asReference(self) :
        handler = toReference.getHandler(type(self.result_))
        if handler is None :
            return None
        return withContext(self.expr, lambda : handler(self.result_))

    def asStatic(self) :
        handler = toStatic.getHandler(type(self.result_))
        if handler is None :
            return None
        return withContext(self.expr, lambda : handler(self.result_))



#
# matchInvoke(code, env, actualArgs) -> InvokeBindings | InvokeError
#

class InvokeError(object) :
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

class InvokeBindings(object) :
    def __init__(self, typeVars, typeParams, vars, params) :
        self.typeVars = typeVars
        self.typeParams = typeParams
        self.vars = vars
        self.params = params

def matchInvoke(code, codeEnv, actualArgs) :
    def argMismatch(actualArg) :
        return InvokeError("argument mismatch", actualArg.expr)
    def predicateFailure(typeCond) :
        return InvokeError("predicate failed", typeCond)
    if len(actualArgs) != len(code.formalArgs) :
        return InvokeError("incorrect no. of arguments")
    codeEnv2, cells = bindTypeVars(codeEnv, code.typeVars)
    vars, params = [], []
    for actualArg, formalArg in zip(actualArgs, code.formalArgs) :
        if type(formalArg) is ValueArgument :
            arg = actualArg.asReference()
            if arg is None :
                return argMismatch(actualArg)
            if formalArg.type is not None :
                typePattern = evaluatePattern(formalArg.type, codeEnv2,
                                              toTypeOrCell)
                if not unify(typePattern, arg.type) :
                    return argMismatch(actualArg)
            vars.append(formalArg.name)
            params.append(arg)
        elif type(formalArg) is StaticArgument :
            arg = actualArg.asStatic()
            if arg is None :
                return argMismatch(actualArg)
            pattern = evaluatePattern(formalArg.pattern, codeEnv2,
                                      toStaticOrCell)
            if not unify(pattern, arg) :
                return argMismatch(actualArg)
        else :
            assert False
    typeParams = resolveTypeVars(code.typeVars, cells)
    if code.predicate is not None :
        codeEnv2 = extendEnv(codeEnv, code.typeVars, typeParams)
        if not evaluateRootExpr(code.predicate, codeEnv2, toBool) :
            return predicateFailure(code.predicate)
    return InvokeBindings(code.typeVars, typeParams, vars, params)



#
# matchRecordInvoke(record, actualArgs) -> InvokeBindings | InvokeError
#

def matchRecordInvoke(record, actualArgs) :
    def argMismatch(actualArg) :
        return InvokeError("argument mismatch", actualArg.expr)
    if len(actualArgs) != len(record.args) :
        return InvokeError("incorrect no. of arguments")
    env, cells = bindTypeVars(record.env, record.typeVars)
    vars, params = [], []
    for actualArg, formalArg in zip(actualArgs, record.args) :
        if type(formalArg) is ValueRecordArg :
            arg = actualArg.asReference()
            if arg is None :
                return argMismatch(actualArg)
            typePattern = evaluatePattern(formalArg.type, env, toTypeOrCell)
            if not unify(typePattern, arg.type) :
                return argMismatch(actualArg)
            vars.append(formalArg.name)
            params.append(arg)
        elif type(formalArg) is StaticRecordArg :
            arg = actualArg.asStatic()
            if arg is None :
                return argMismatch(actualArg)
            pattern = evaluatePattern(formalArg.pattern, env, toStaticOrCell)
            if not unify(pattern, arg) :
                return argMismatch(actualArg)
        else :
            assert False
    typeParams = resolveTypeVars(record.typeVars, cells)
    return InvokeBindings(record.typeVars, typeParams, vars, params)



#
# evalInvoke
#

def evalInvoke(code, env, bindings) :
    env = extendEnv(env, bindings.typeVars + bindings.vars,
                    bindings.typeParams + bindings.params)
    returnType = None
    if code.returnType is not None :
        returnType = evaluateRootExpr(code.returnType, env, toType)
    context = CodeContext(code.returnByRef, returnType)
    result = evalStatement(code.body, env, context)
    if type(result) is Goto :
        withContext(result.labelName, lambda : error("label not found"))
    if type(result) is Break :
        withContext(result, lambda : error("invalid break statement"))
    if type(result) is Continue :
        withContext(result, lambda : error("invalid continue statement"))
    if isValue(result) :
        installTemp(result)
    if result is None :
        if returnType is not None :
            ensure(returnType == voidType, "unexpected void return")
        result = voidValue
    return result



#
# evalStatement : (stmt, env, context) -> Thing | None
#

class CodeContext(object) :
    def __init__(self, returnByRef, returnType) :
        self.returnByRef = returnByRef
        self.returnType = returnType

def evalStatement(stmt, env, context) :
    return withContext(stmt, lambda : evalStatement2(stmt, env, context))



#
# evalStatement2
#

evalStatement2 = multimethod(errorMessage="invalid statement")

@evalStatement2.register(Block)
def foo(x, env, context) :
    i = 0
    labels = {}
    evalCollectLabels(x.statements, i, labels, env)
    while i < len(x.statements) :
        statement = x.statements[i]
        if type(statement) in (VarBinding, RefBinding, StaticBinding) :
            env = evalBinding(statement, env, context)
            evalCollectLabels(x.statements, i+1, labels, env)
        elif type(statement) is Label :
            pass
        else :
            result = evalStatement(statement, env, context)
            if type(result) is Goto :
                envAndPos = labels.get(result.labelName.s)
                if envAndPos is None :
                    return result
                else :
                    env, i = envAndPos
                    continue
            if result is not None :
                return result
        i += 1

evalBinding = multimethod(errorMessage="invalid binding")

@evalBinding.register(VarBinding)
def foo(x, env, context) :
    converter = toValue
    if x.type is not None :
        declaredType = evaluateRootExpr(x.type, env, toType)
        converter = toValueOfType(declaredType)
    right = evaluateRootExpr(x.expr, env, converter)
    return extendEnv(env, [x.name], [right])

@evalBinding.register(RefBinding)
def foo(x, env, context) :
    converter = toValueOrReference
    if x.type is not None :
        declaredType = evaluateRootExpr(x.type, env, toType)
        converter = toValueOrReferenceOfType(declaredType)
    right = evaluateRootExpr(x.expr, env, converter)
    return extendEnv(env, [x.name], [right])

@evalBinding.register(StaticBinding)
def foo(x, env, context) :
    right = evaluateRootExpr(x.expr, env, toStatic)
    return extendEnv(env, [x.name], [right])

def evalCollectLabels(statements, startIndex, labels, env) :
    i = startIndex
    while i < len(statements) :
        stmt = statements[i]
        if type(stmt) is Label :
            labels[stmt.name.s] = (env, i)
        elif type(stmt) in (VarBinding, RefBinding, StaticBinding) :
            break
        i += 1

@evalStatement2.register(Assignment)
def foo(x, env, context) :
    pushTempsBlock()
    try :
        left = evaluate(x.left, env, toLValue)
        rightRef = evaluate(x.right, env, toReference)
        valueAssign(left, rightRef)
    finally :
        popTempsBlock()

@evalStatement2.register(Goto)
def foo(x, env, context) :
    return x

@evalStatement2.register(Return)
def foo(x, env, context) :
    if context.returnByRef :
        result = evaluateRootExpr(x.expr, env, toLValue)
        resultType = result.type
    elif x.expr is None :
        result = voidValue
        resultType = voidType
    else :
        result = evaluateRootExpr(x.expr, env, toStatic)
        resultType = result.type if isValue(result) else None
    if context.returnType is not None :
        def check() :
            matchType(resultType, context.returnType)
        withContext(x.expr, check)
    return result

@evalStatement2.register(IfStatement)
def foo(x, env, context) :
    cond = evaluateRootExpr(x.condition, env, toBool)
    if cond :
        return evalStatement(x.thenPart, env, context)
    elif x.elsePart is not None :
        return evalStatement(x.elsePart, env, context)

@evalStatement2.register(ExprStatement)
def foo(x, env, context) :
    evaluateRootExpr(x.expr, env)

@evalStatement2.register(While)
def foo(x, env, context) :
    while True :
        cond = evaluateRootExpr(x.condition, env, toBool)
        if not cond :
            break
        result = evalStatement(x.body, env, context)
        if type(result) is Break :
            break
        if type(result) is Continue :
            continue
        if result is not None :
            return result

@evalStatement2.register(Break)
def foo(x, env, context) :
    return x

@evalStatement2.register(Continue)
def foo(x, env, context) :
    return x

@evalStatement2.register(For)
def foo(x, env, context) :
    return evalStatement(convertForStatement(x), env, context)



#
# expand for statement to use while
#

def convertForStatement(x) :
    exprVar = Identifier("%expr")
    iterVar = Identifier("%iter")
    block = Block(
        [RefBinding(exprVar, None, x.expr),
         VarBinding(iterVar, None, Call(coreNameRef("iterator"),
                                        [NameRef(exprVar)])),
         While(Call(coreNameRef("hasNext?"), [NameRef(iterVar)]),
               Block([RefBinding(x.variable, x.type,
                                 Call(coreNameRef("next"),
                                      [NameRef(iterVar)])),
                      x.body]))])
    return block



#
# remove temp name used for multimethod instances
#

del foo
