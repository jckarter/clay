import ctypes
from clay.xprint import *
from clay.error import *
from clay.multimethod import *
import clay.llvmwrapper as llvm



#
# types
#

class Type(object) :
    def __init__(self) :
        self.interned = False
        self.llvmType = None

class BoolType(Type) :
    pass

class IntegerType(Type) :
    def __init__(self, bits, signed) :
        super(IntegerType, self).__init__()
        self.bits = bits
        self.signed = signed

class FloatType(Type) :
    def __init__(self, bits) :
        super(FloatType, self).__init__()
        self.bits = bits

class VoidType(Type) :
    pass

class CompilerObjectType(Type) :
    pass

class PointerType(Type) :
    def __init__(self, pointeeType) :
        super(PointerType, self).__init__()
        self.pointeeType = pointeeType

class TupleType(Type) :
    def __init__(self, elementTypes) :
        super(TupleType, self).__init__()
        self.elementTypes = elementTypes

class ArrayType(Type) :
    def __init__(self, elementType, size) :
        super(ArrayType, self).__init__()
        self.elementType = elementType
        self.size = size

class RecordType(Type) :
    def __init__(self, record, params) :
        super(RecordType, self).__init__()
        self.record = record
        self.params = params
        self.fieldTypes = None



#
# equalTypes
#

equalTypes = multimethod("equalTypes", n=2)

@equalTypes.register(Type, Type)
def foo(a, b) :
    return False

@equalTypes.register(BoolType, BoolType)
def foo(a, b) :
    return True

@equalTypes.register(IntegerType, IntegerType)
def foo(a, b) :
    return (a.bits == b.bits) and (a.signed == b.signed)

@equalTypes.register(FloatType, FloatType)
def foo(a, b) :
    return (a.bits == b.bits)

@equalTypes.register(VoidType, VoidType)
def foo(a, b) :
    return True

@equalTypes.register(CompilerObjectType, CompilerObjectType)
def foo(a, b) :
    return True

@equalTypes.register(PointerType, PointerType)
def foo(a, b) :
    return equalTypes(a.pointeeType, b.pointeeType)

@equalTypes.register(TupleType, TupleType)
def foo(a, b) :
    if len(a.elementTypes) != len(b.elementTypes) :
        return False
    for t1, t2 in zip(a.elementTypes, b.elementTypes) :
        if not equalTypes(t1, t2) :
            return False
    return True

@equalTypes.register(ArrayType, ArrayType)
def foo(a, b) :
    return equalTypes(a.elementType, b.elementType) and (a.size == b.size)

@equalTypes.register(RecordType, RecordType)
def foo(a, b) :
    return (a.record == b.record) and (a.params == b.params)



#
# hashType
#

hashType = multimethod("hashType")

@hashType.register(BoolType)
def foo(t) :
    return id(BoolType)

@hashType.register(IntegerType)
def foo(t) :
    return hash((id(IntegerType), t.bits, t.signed))

@hashType.register(FloatType)
def foo(t) :
    return hash((id(FloatType), t.bits))

@hashType.register(VoidType)
def foo(t) :
    return id(VoidType)

@hashType.register(CompilerObjectType)
def foo(t) :
    return id(CompilerObjectType)

@hashType.register(PointerType)
def foo(t) :
    return hash((id(PointerType), t.pointeeType))

@hashType.register(TupleType)
def foo(t) :
    return hash((id(TupleType),) + tuple(t.elementTypes))

@hashType.register(ArrayType)
def foo(t) :
    return hash((id(ArrayType), t.elementType, t.size))

@hashType.register(RecordType)
def foo(t) :
    return hash((id(RecordType), t.record) + tuple(t.params))



#
# intern types
#

class TypeHolder(object) :
    def __init__(self, type_) :
        self.type = type_
    def __eq__(self, other) :
        if not isinstance(other, TypeHolder) :
            return False
        return equalTypes(self.type, other.type)
    def __hash__(self) :
        return hashType(self.type)

_typesTable = {}

def internType(t) :
    holder = TypeHolder(t)
    entry = _typesTable.get(holder)
    if entry is not None :
        assert entry.interned
        return entry
    t.interned = True
    _typesTable[holder] = t
    return t

boolType = internType(BoolType())

int8Type = internType(IntegerType(bits=8, signed=True))
int16Type = internType(IntegerType(bits=16, signed=True))
int32Type = internType(IntegerType(bits=32, signed=True))
int64Type = internType(IntegerType(bits=64, signed=True))
uint8Type = internType(IntegerType(bits=8, signed=False))
uint16Type = internType(IntegerType(bits=16, signed=False))
uint32Type = internType(IntegerType(bits=32, signed=False))
uint64Type = internType(IntegerType(bits=64, signed=False))

float32Type = internType(FloatType(32))
float64Type = internType(FloatType(64))

voidType = internType(VoidType())

compilerObjectType = internType(CompilerObjectType())

def pointerType(pointeeType) :
    return internType(PointerType(pointeeType))

def tupleType(elementTypes) :
    return internType(TupleType(elementTypes))

def arrayType(elementType, size) :
    return internType(ArrayType(elementType, size))

def recordType(record, params) :
    return internType(RecordType(record, params))



#
# initialize llvm
#

llvmModule = None
llvmModuleProvider = None
llvmExecutionEngine = None
llvmTargetData = None

def initLLVM() :
    global llvmModule, llvmModuleProvider, llvmExecutionEngine
    global llvmTargetData
    llvmModule = llvm.Module.new("clay_module")
    llvmModuleProvider = llvm.ModuleProvider.new(llvmModule)
    llvmExecutionEngine = llvm.ExecutionEngine.new(llvmModuleProvider)
    llvmTargetData = llvmExecutionEngine.target_data
    llvmModule.data_layout = str(llvmTargetData)

initLLVM()

def lltFunction(retType, argTypes) :
    return llvm.Type.function(retType, argTypes)
def lltInt(bits) : return llvm.Type.int(bits)
def lltFloat() : return llvm.Type.float()
def lltDouble() : return llvm.Type.double()
def lltPointer(pointeeType) : return llvm.Type.pointer(pointeeType)
def lltVoid() : return llvm.Type.void()
def lltStruct(fieldTypes) : return llvm.Type.struct(fieldTypes)
def lltArray(elementType, n) : return llvm.Type.array(elementType, n)



#
# llvmType
#

def llvmType(t) :
    if t.llvmType is not None :
        return t.llvmType
    llt = makeLLVMType(t)
    t.llvmType = llt
    return llt

makeLLVMType = multimethod("makeLLVMType", errorMessage="invalid type")

@makeLLVMType.register(BoolType)
def foo(t) :
    return lltInt(8)

@makeLLVMType.register(IntegerType)
def foo(t) :
    return lltInt(t.bits)

@makeLLVMType.register(FloatType)
def foo(t) :
    if t.bits == 32 : return lltFloat()
    elif t.bits == 64 : return lltDouble()
    else :
        assert False

@makeLLVMType.register(VoidType)
def foo(t) :
    error("Void type is not a concrete type")

@makeLLVMType.register(CompilerObjectType)
def foo(t) :
    return lltPointer(lltInt(8))

@makeLLVMType.register(PointerType)
def foo(t) :
    return lltPointer(llvmType(t.pointeeType))

@makeLLVMType.register(TupleType)
def foo(t) :
    return lltStruct([llvmType(x) for x in t.elementTypes])

@makeLLVMType.register(ArrayType)
def foo(t) :
    return lltArray(llvmType(t.elementType), t.size)



#
# toGenericValue
#

toGenericValue = multimethod("toGenericValue")

@toGenericValue.register(BoolType)
def foo(type_, x) :
    return llvm.GenericValue.int(llvmType(type_), int(x))

@toGenericValue.register(IntegerType)
def foo(type_, x) :
    if type_.signed :
        return llvm.GenericValue.int_signed(llvmType(type_), x)
    return llvm.GenericValue.int(llvmType(type_), x)

@toGenericValue.register(FloatType)
def foo(type_, x) :
    return llvm.GenericValue.real(llvmType(type_), x)

@toGenericValue.register(PointerType)
def foo(type_, x) :
    return llvm.GenericValue.pointer(llvmType(type_), x)



#
# typeSize
#

def typeSize(t) :
    n = llvmTargetData.abi_size(llvmType(t))
    return int(n)



#
# allocMem, freeMem
#

def codegen_allocMem() :
    llvmFuncType = lltFunction(lltPointer(lltInt(8)), [lltInt(32)])
    func = llvmModule.add_function(llvmFuncType, "internal_allocMem")
    func.linkage = llvm.LINKAGE_INTERNAL
    block = func.append_basic_block("code")
    builder = llvm.Builder.new(block)
    ptr = builder.malloc_array(lltInt(8), func.args[0])
    builder.ret(ptr)
    return func
code_allocMem = codegen_allocMem()

def codegen_freeMem() :
    llvmFuncType = lltFunction(lltVoid(), [lltPointer(lltInt(8))])
    func = llvmModule.add_function(llvmFuncType, "internal_freeMem")
    func.linkage = llvm.LINKAGE_INTERNAL
    block = func.append_basic_block("code")
    builder = llvm.Builder.new(block)
    builder.free(func.args[0])
    builder.ret_void()
    return func
code_freeMem = codegen_freeMem()

def allocMem(n) :
    args = [toGenericValue(int32Type, n)]
    result = llvmExecutionEngine.run_function(code_allocMem, args)
    return result.as_pointer()

def freeMem(addr) :
    args = [toGenericValue(pointerType(int8Type), addr)]
    llvmExecutionEngine.run_function(code_freeMem, args)



#
# Value
#

class Value(object) :
    def __init__(self, type_, isOwned, address) :
        self.type = type_
        self.isOwned = isOwned
        self.address = address
    def __del__(self) :
        if self.isOwned :
            destroyValue(self)
            freeMem(self.address)
    def __eq__(self, other) :
        return equalValues(self, other)
    def __hash__(self) :
        return hashValue(self)



#
# allocValue
#

def allocValue(type_) :
    address = allocMem(typeSize(type_))
    v = Value(type_, True, address)
    return v



#
# create values of primitive types
#

def codegen_toPrimValue(t) :
    llt = llvmType(t)
    llvmFuncType = lltFunction(lltVoid(), [llt, lltPointer(llt)])
    func = llvmModule.add_function(llvmFuncType, "internal_toPrimValue")
    func.linkage = llvm.LINKAGE_INTERNAL
    block = func.append_basic_block("code")
    builder = llvm.Builder.new(block)
    builder.store(func.args[0], func.args[-1])
    builder.ret_void()
    return func

code_toIntValue = {}
code_toIntValue[8] = codegen_toPrimValue(int8Type)
code_toIntValue[16] = codegen_toPrimValue(int16Type)
code_toIntValue[32] = codegen_toPrimValue(int32Type)
code_toIntValue[64] = codegen_toPrimValue(int64Type)

code_toFloatValue = {}
code_toFloatValue[32] = codegen_toPrimValue(float32Type)
code_toFloatValue[64] = codegen_toPrimValue(float64Type)

code_ptrToValue = codegen_toPrimValue(pointerType(int8Type))

toPrimValueCode = multimethod("toPrimValueCode")
toPrimValueCode.register(BoolType)(lambda t : code_toIntValue[8])
toPrimValueCode.register(IntegerType)(lambda t : code_toIntValue[t.bits])
toPrimValueCode.register(FloatType)(lambda t : code_toFloatValue[t.bits])
toPrimValueCode.register(PointerType)(lambda t : code_ptrToValue)

def invoke_toPrimValue(type_, x) :
    args = [toGenericValue(type_, x)]
    v = allocValue(type_)
    args.append(toGenericValue(pointerType(type_), v.address))
    llvmExecutionEngine.run_function(toPrimValueCode(type_), args)
    return v

def toBoolValue(x) : return invoke_toPrimValue(boolType, x)
def toInt8Value(x) : return invoke_toPrimValue(int8Type, x)
def toInt16Value(x) : return invoke_toPrimValue(int16Type, x)
def toInt32Value(x) : return invoke_toPrimValue(int32Type, x)
def toInt64Value(x) : return invoke_toPrimValue(int64Type, x)
def toUInt8Value(x) : return invoke_toPrimValue(uint8Type, x)
def toUInt16Value(x) : return invoke_toPrimValue(uint16Type, x)
def toUInt32Value(x) : return invoke_toPrimValue(uint32Type, x)
def toUInt64Value(x) : return invoke_toPrimValue(uint64Type, x)
def toFloat32Value(x) : return invoke_toPrimValue(float32Type, x)
def toFloat64Value(x) : return invoke_toPrimValue(float64Type, x)

def toPointerValue(pointeeType, addr) :
    int8PtrType = pointerType(int8Type)
    args = [toGenericValue(pointerType(int8Type), addr)]
    v = allocValue(pointerType(pointerType))
    args.append(toGenericValue(pointerType(int8PtrType), v.address))
    llvmExecutionEngine.run_function(code_ptrToValue, args)
    return v



#
# access values of primitive types
#

def codegen_fromPrimValue(t) :
    llt = llvmType(t)
    llvmFuncType = lltFunction(llt, [lltPointer(llt)])
    func = llvmModule.add_function(llvmFuncType, "internal_fromPrimValue")
    func.linkage = llvm.LINKAGE_INTERNAL
    block = func.append_basic_block("code")
    builder = llvm.Builder.new(block)
    x = builder.load(func.args[0])
    builder.ret(x)
    return func

code_fromIntValue = {}
code_fromIntValue[8] = codegen_fromPrimValue(int8Type)
code_fromIntValue[16] = codegen_fromPrimValue(int16Type)
code_fromIntValue[32] = codegen_fromPrimValue(int32Type)
code_fromIntValue[64] = codegen_fromPrimValue(int64Type)

code_fromFloatValue = {}
code_fromFloatValue[32] = codegen_fromPrimValue(float32Type)
code_fromFloatValue[64] = codegen_fromPrimValue(float64Type)

code_fromPtrValue = codegen_fromPrimValue(pointerType(int8Type))

def fromBoolValue(v) :
    ensure(v.type == boolType, "invalid bool type")
    args = [toGenericValue(pointerType(boolType), v.address)]
    result = llvmExecutionEngine.run_function(code_fromIntValue[8], args)
    return bool(result.as_int())

def fromIntValue(v) :
    ensure(isinstance(v.type, IntegerType), "invalid integer type")
    args = [toGenericValue(pointerType(v.type), v.address)]
    func = code_fromIntValue[v.type.bits]
    result = llvmExecutionEngine.run_function(func, args)
    if v.type.signed :
        return int(result.as_int_signed())
    return int(result.as_int())

def fromFloatValue(v) :
    ensure(isinstance(v.type, FloatType), "invalid float type")
    args = [toGenericValue(pointerType(v.type), v.address)]
    bits = v.type.bits
    result = llvmExecutionEngine.run_function(code_fromFloatValue[bits], args)
    return result.as_real(llvmType(v.type))

def fromPointerValue(v) :
    ensure(isinstance(v.type, PointerType), "invalid pointer type")
    args = [toGenericValue(pointerType(int8Type), v.address)]
    result = llvmExecutionEngine.run_function(code_fromPtrValue, args)
    return result.as_pointer()



#
# compiler objects
#

_Ptr = ctypes.c_void_p
_PtrPtr = ctypes.POINTER(_Ptr)

def coValueAsPtr(v) :
    return ctypes.cast(_Ptr(v.address), _PtrPtr).contents

def toCOValue(x) :
    v = allocValue(compilerObjectType)
    p = coValueAsPtr(v)
    p.value = id(x)
    ctypes.pythonapi.Py_IncRef(p)
    return v

def fromCOValue(v) :
    p = coValueAsPtr(v)
    return ctypes.cast(p, ctypes.py_object).value

def initCOValue(v) :
    p = coValueAsPtr(v)
    p.value = id(None)
    ctypes.pythonapi.Py_IncRef(p)

def destroyCOValue(v) :
    p = coValueAsPtr(v)
    ctypes.pythonapi.Py_DecRef(p)
    p.value = 0

def copyCOValue(dest, src) :
    destP = coValueAsPtr(dest)
    srcP = coValueAsPtr(src)
    destP.value = srcP.value
    ctypes.pythonapi.Py_IncRef(destP)

def assignCOValue(dest, src) :
    if dest.address == src.address :
        return
    destroyCOValue(dest)
    copyCOValue(dest, src)

def equalCOValues(a, b) :
    return fromCOValue(a) == fromCOValue(b)

def hashCOValue(a) :
    return hash(fromCOValue(a))



#
# field access for tuples, arrays
#

def tupleFieldOffset(type_, index) :
    llt = llvmType(type_)
    return llvmTargetData.offset_of_element(llt, index)

def tupleFieldRef(v, index) :
    offset = tupleFieldOffset(v.type, index)
    fieldType = v.type.elementTypes[index]
    return Value(fieldType, False, v.address + offset)

def tupleFieldRefs(v) :
    n = len(v.type.elementTypes)
    return [tupleFieldRef(v, i) for i in range(n)]

def arrayElementOffset(type_, index) :
    return index * typeSize(type_.elementType)

def arrayElementRef(v, index) :
    offset = arrayElementOffset(v.type, index)
    elementType = v.type.elementType
    return Value(elementType, False, v.address + offset)

def arrayElementRefs(v) :
    n = v.type.size
    return [arrayElementRef(v, i) for i in range(n)]



#
# initValue
#

def initValue(v) :
    initValue2(v.type, v)

initValue2 = multimethod("initValue2")

def initPrim(t, v) :
    pass

initValue2.register(BoolType)(initPrim)
initValue2.register(IntegerType)(initPrim)
initValue2.register(FloatType)(initPrim)
initValue2.register(CompilerObjectType)(lambda t,v : initCOValue(v))
initValue2.register(PointerType)(initPrim)

@initValue2.register(TupleType)
def foo(t, v) :
    for x in tupleFieldRefs(v) :
        initValue(x)

@initValue2.register(ArrayType)
def foo(t, v) :
    for x in arrayElementRefs(v) :
        initValue(x)



#
# destroyValue
#

def destroyValue(v) :
    destroyValue2(v.type, v)

destroyValue2 = multimethod("destroyValue2")

def destroyPrim(t, v) :
    pass

destroyValue2.register(BoolType)(destroyPrim)
destroyValue2.register(IntegerType)(destroyPrim)
destroyValue2.register(FloatType)(destroyPrim)
destroyValue2.register(CompilerObjectType)(lambda t,v : destroyCOValue(v))
destroyValue2.register(PointerType)(destroyPrim)

@destroyValue2.register(TupleType)
def foo(t, v) :
    for x in tupleFieldRefs(v) :
        destroyValue(x)

@destroyValue2.register(ArrayType)
def foo(t, v) :
    for x in arrayElementRefs(v) :
        destroyValue(x)



#
# copyValue
#

def copyValue(dest, src) :
    assert dest.type == src.type
    copyValue2(dest.type, dest, src)

copyValue2 = multimethod("copyValue2")

def copyPrim(t, dest, src) :
    destAddr = ctypes.c_void_p(dest.address)
    srcAddr = ctypes.c_void_p(src.address)
    ctypes.memmove(destAddr, srcAddr, typeSize(t))

copyValue2.register(BoolType)(copyPrim)
copyValue2.register(IntegerType)(copyPrim)
copyValue2.register(FloatType)(copyPrim)
copyValue2.register(CompilerObjectType)(lambda t,d,s : copyCOValue(d,s))
copyValue2.register(PointerType)(copyPrim)

@copyValue2.register(TupleType)
def foo(t, dest, src) :
    for x, y in zip(tupleFieldRefs(dest), tupleFieldRefs(src)) :
        copyValue(x, y)

@copyValue2.register(ArrayType)
def foo(t, dest, src) :
    for x, y in zip(arrayElementRefs(dest), arrayElementRefs(src)) :
        copyValue(x, y)



#
# assignValue
#

def assignValue(dest, src) :
    assert dest.type == src.type
    if dest.address == src.address :
        return
    destroyValue(dest)
    copyValue(dest, src)



#
# equalValues
#

def equalValues(a, b) :
    if a.type != b.type :
        return False
    return equalValues2(a.type, a, b)

equalValues2 = multimethod("equalValues2")

@equalValues2.register(BoolType)
def foo(t, a, b) :
    return fromBoolValue(a) == fromBoolValue(b)

@equalValues2.register(IntegerType)
def foo(t, a, b) :
    return fromIntValue(a) == fromIntValue(b)

@equalValues2.register(FloatType)
def foo(t, a, b) :
    return fromFloatValue(a) == fromFloatValue(b)

@equalValues2.register(CompilerObjectType)
def foo(t, a, b) :
    return equalCOValues(a, b)

@equalValues2.register(PointerType)
def foo(t, a, b) :
    return fromPointerValue(a) == fromPointerValue(b)

@equalValues2.register(TupleType)
def foo(t, a, b) :
    for x, y in zip(tupleFieldRefs(a), tupleFieldRefs(b)) :
        if not equalValues(x, y) :
            return False
    return True

@equalValues2.register(ArrayType)
def foo(t, a, b) :
    for x, y in zip(arrayElementRefs(a), arrayElementRefs(b)) :
        if not equalValues(x, y) :
            return False
    return True



#
# hashValue
#

def hashValue(a) :
    return hashValue2(a.type, a)

hashValue2 = multimethod("hashValue2")

@hashValue2.register(BoolType)
def foo(t, a) :
    return int(fromBoolValue(a))

@hashValue2.register(IntegerType)
def foo(t, a) :
    return int(fromIntValue(a))

@hashValue2.register(FloatType)
def foo(t, a) :
    x = fromFloatValue(a)
    return int(x * 1000)

@hashValue2.register(CompilerObjectType)
def foo(t, a) :
    return hash(fromCOValue(a))

@hashValue2.register(PointerType)
def foo(t, a) :
    return fromPointerValue(a)

@hashValue2.register(TupleType)
def foo(t, a) :
    h = 0
    for x in tupleFieldRefs(a) :
        h += hashValue(x)
    return h

@hashValue2.register(ArrayType)
def foo(t, a) :
    h = 0
    for x in arrayElementRefs(a) :
        h += hashValue(x)
    return h



#
# lower
#

def lower(v) :
    assert type(v) is Value
    if v.type == compilerObjectType :
        return fromCOValue(v)
    return v



#
# utility converters
#

def toTypeResult(v) :
    t = lower(v)
    ensure(isinstance(t, Type), "invalid type")
    return t



#
# remove temp name used for multimethod instances
#

del foo
