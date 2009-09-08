import ctypes
from clay.xprint import *
from clay.error import *
from clay.multimethod import *
from clay.coreops import *
from clay.env import *
from clay.types import *
from clay.unify import *
from clay.evaluator import *
import clay.llvmwrapper as llvm


llvmModule = llvm.Module.new("foo")


#
# convert to llvm types
#

_llvmTypesTable = None

def _initLLVMTypesTable() :
    global _llvmTypesTable
    _llvmTypesTable = {}
    def llvmInt(ct) :
        return llvm.Type.int(ctypes.sizeof(ct) * 8)
    _llvmTypesTable[boolType] = llvmInt(ctypes.c_bool)
    _llvmTypesTable[intType] = llvmInt(ctypes.c_int)
    _llvmTypesTable[floatType] = llvm.Type.float()
    _llvmTypesTable[doubleType] = llvm.Type.double()
    _llvmTypesTable[charType] = llvmInt(ctypes.c_wchar)

def _cleanupLLVMTypesTable() :
    global _llvmTypesTable
    _llvmTypesTable = None

installGlobalsCleanupHook(_cleanupLLVMTypesTable)

llvmType = multimethod(errorMessage="invalid type")

@llvmType.register(Type)
def foo(t) :
    if _llvmTypesTable is None :
        _initLLVMTypesTable()
    llt = _llvmTypesTable.get(t)
    if llt is None :
        llt = makeLLVMType(t.tag, t)
    return llt

makeLLVMType = multimethod(errorMessage="invalid type tag")

@makeLLVMType.register(TupleTypeTag)
def foo(tag, t) :
    fieldLLVMTypes = [llvmType(x) for x in t.params]
    llt = llvm.Type.struct(fieldLLVMTypes)
    _llvmTypesTable[t] = llt
    return llt

@makeLLVMType.register(ArrayTypeTag)
def foo(tag, t) :
    ensure(len(t.params) == 2, "invalid array type")
    elementType = llvmType(t.params[0])
    count = toInt(t.params[1])
    llt = llvm.Type.array(elementType, count)
    _llvmTypesTable[t] = llt
    return llt

@makeLLVMType.register(PointerTypeTag)
def foo(tag, t) :
    ensure(len(t.params) == 1, "invalid pointer type")
    pointeeType = llvmType(t.params[0])
    llt = llvm.Type.pointer(pointeeType)
    _llvmTypesTable[t] = llt
    return llt

@makeLLVMType.register(Record)
def foo(tag, t) :
    llt = llvm.Type.opaque()
    typeHandle = llvm.TypeHandle.new(llt)
    _llvmTypesTable[t] = llt
    fieldLLVMTypes = [llvmType(x) for x in recordFieldTypes(t)]
    recType = llvm.Type.struct(fieldLLVMTypes)
    typeHandle.type.refine(recType)
    llt = typeHandle.type
    _llvmTypesTable[t] = llt
    return llt



#
# remove temp name used for multimethod instances
#

del foo
