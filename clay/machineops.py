import ctypes
import ctypes.util

__all__ = [
    "mop_memAlloc", "mop_memFree",
    "mop_pointerOffset", "mop_pointerSubtract", "mop_pointerCopy",
    "mop_pointerEquals", "mop_pointerLesser",
    "mop_allocateMemory", "mop_freeMemory",
    "mop_boolCopy", "mop_boolNot",
    "mop_charCopy", "mop_charEquals", "mop_charLesser",
    "mop_intCopy", "mop_intEquals", "mop_intLesser",
    "mop_intAdd", "mop_intSubtract", "mop_intMultiply",
    "mop_intDivide", "mop_intModulus", "mop_intNegate",
    "mop_floatCopy", "mop_floatEquals", "mop_floatLesser",
    "mop_floatAdd", "mop_floatSubtract", "mop_floatMultiply",
    "mop_floatDivide", "mop_floatNegate",
    "mop_doubleCopy", "mop_doubleEquals", "mop_doubleLesser",
    "mop_doubleAdd", "mop_doubleSubtract", "mop_doubleMultiply",
    "mop_doubleDivide", "mop_doubleNegate",
    "mop_charToInt", "mop_intToChar", "mop_floatToInt", "mop_intToFloat",
    "mop_floatToDouble", "mop_doubleToFloat", "mop_doubleToInt",
    "mop_intToDouble"]

dll = ctypes.CDLL(ctypes.util.find_library("machineops"))

def getF(name, resultType=None) :
    f = getattr(dll, name)
    f.restype = resultType
    return f



#
# memAlloc, memFree
#

f_mop_memAlloc = getF("mop_memAlloc", ctypes.c_void_p)
def mop_memAlloc(size) :
    return ctypes.c_void_p(f_mop_memAlloc(size))

f_mop_memFree = getF("mop_memFree")
def mop_memFree(ptr) :
    f_mop_memFree(ptr)



#
# pointers
#

f_mop_pointerOffset = getF("mop_pointerOffset")
def mop_pointerOffset(ptr, offset, result) :
    f_mop_pointerOffset(ptr, offset, result)

f_mop_pointerSubtract = getF("mop_pointerSubtract")
def mop_pointerSubtract(ptr1, ptr2, result) :
    f_mop_pointerSubtract(ptr1, ptr2, result)

f_mop_pointerCopy = getF("mop_pointerCopy")
def mop_pointerCopy(dest, src) :
    f_mop_pointerCopy(dest, src)

f_mop_pointerEquals = getF("mop_pointerEquals")
def mop_pointerEquals(ptr1, ptr2, result) :
    f_mop_pointerEquals(ptr1, ptr2, result)

f_mop_pointerLesser = getF("mop_pointerLesser")
def mop_pointerLesser(ptr1, ptr2, result) :
    f_mop_pointerLesser(ptr1, ptr2, result)

f_mop_allocateMemory = getF("mop_allocateMemory")
def mop_allocateMemory(size, result) :
    f_mop_allocateMemory(size, result)

f_mop_freeMemory = getF("mop_freeMemory")
def mop_freeMemory(ptr) :
    f_mop_freeMemory(ptr)



#
# bool
#

f_mop_boolCopy = getF("mop_boolCopy")
def mop_boolCopy(dest, src) :
    f_mop_boolCopy(dest, src)

f_mop_boolNot = getF("mop_boolNot")
def mop_boolNot(a, result) :
    f_mop_boolNot(a, result)



#
# char
#

f_mop_charCopy = getF("mop_charCopy")
def mop_charCopy(dest, src) :
    f_mop_charCopy(dest, src)

f_mop_charEquals = getF("mop_charEquals")
def mop_charEquals(a, b, result) :
    f_mop_charEquals(a, b, result)

f_mop_charLesser = getF("mop_charLesser")
def mop_charLesser(a, b, result) :
    f_mop_charLesser(a, b, result)



#
# int
#

f_mop_intCopy = getF("mop_intCopy")
def mop_intCopy(dest, src) :
    f_mop_intCopy(dest, src)

f_mop_intEquals = getF("mop_intEquals")
def mop_intEquals(a, b, result) :
    f_mop_intEquals(a, b, result)

f_mop_intLesser = getF("mop_intLesser")
def mop_intLesser(a, b, result) :
    f_mop_intLesser(a, b, result)

f_mop_intAdd = getF("mop_intAdd")
def mop_intAdd(a, b, result) :
    f_mop_intAdd(a, b, result)

f_mop_intSubtract = getF("mop_intSubtract")
def mop_intSubtract(a, b, result) :
    f_mop_intSubtract(a, b, result)

f_mop_intMultiply = getF("mop_intMultiply")
def mop_intMultiply(a, b, result) :
    f_mop_intMultiply(a, b, result)

f_mop_intDivide = getF("mop_intDivide")
def mop_intDivide(a, b, result) :
    f_mop_intDivide(a, b, result)

f_mop_intModulus = getF("mop_intModulus")
def mop_intModulus(a, b, result) :
    f_mop_intModulus(a, b, result)

f_mop_intNegate = getF("mop_intNegate")
def mop_intNegate(a, result) :
    f_mop_intNegate(a, result)



#
# float
#

f_mop_floatCopy = getF("mop_floatCopy")
def mop_floatCopy(dest, src) :
    f_mop_floatCopy(dest, src)

f_mop_floatEquals = getF("mop_floatEquals")
def mop_floatEquals(a, b, result) :
    f_mop_floatEquals(a, b, result)

f_mop_floatLesser = getF("mop_floatLesser")
def mop_floatLesser(a, b, result) :
    f_mop_floatLesser(a, b, result)

f_mop_floatAdd = getF("mop_floatAdd")
def mop_floatAdd(a, b, result) :
    f_mop_floatAdd(a, b, result)

f_mop_floatSubtract = getF("mop_floatSubtract")
def mop_floatSubtract(a, b, result) :
    f_mop_floatSubtract(a, b, result)

f_mop_floatMultiply = getF("mop_floatMultiply")
def mop_floatMultiply(a, b, result) :
    f_mop_floatMultiply(a, b, result)

f_mop_floatDivide = getF("mop_floatDivide")
def mop_floatDivide(a, b, result) :
    f_mop_floatDivide(a, b, result)

f_mop_floatNegate = getF("mop_floatNegate")
def mop_floatNegate(a, result) :
    f_mop_floatNegate(a, result)



#
# double
#

f_mop_doubleCopy = getF("mop_doubleCopy")
def mop_doubleCopy(dest, src) :
    f_mop_doubleCopy(dest, src)

f_mop_doubleEquals = getF("mop_doubleEquals")
def mop_doubleEquals(a, b, result) :
    f_mop_doubleEquals(a, b, result)

f_mop_doubleLesser = getF("mop_doubleLesser")
def mop_doubleLesser(a, b, result) :
    f_mop_doubleLesser(a, b, result)

f_mop_doubleAdd = getF("mop_doubleAdd")
def mop_doubleAdd(a, b, result) :
    f_mop_doubleAdd(a, b, result)

f_mop_doubleSubtract = getF("mop_doubleSubtract")
def mop_doubleSubtract(a, b, result) :
    f_mop_doubleSubtract(a, b, result)

f_mop_doubleMultiply = getF("mop_doubleMultiply")
def mop_doubleMultiply(a, b, result) :
    f_mop_doubleMultiply(a, b, result)

f_mop_doubleDivide = getF("mop_doubleDivide")
def mop_doubleDivide(a, b, result) :
    f_mop_doubleDivide(a, b, result)

f_mop_doubleNegate = getF("mop_doubleNegate")
def mop_doubleNegate(a, result) :
    f_mop_doubleNegate(a, result)



#
# conversions
#

f_mop_charToInt = getF("mop_charToInt")
def mop_charToInt(a, result) :
    f_mop_charToInt(a, result)

f_mop_intToChar = getF("mop_intToChar")
def mop_intToChar(a, result) :
    f_mop_intToChar(a, result)

f_mop_floatToInt = getF("mop_floatToInt")
def mop_floatToInt(a, result) :
    f_mop_floatToInt(a, result)

f_mop_intToFloat = getF("mop_intToFloat")
def mop_intToFloat(a, result) :
    f_mop_intToFloat(a, result)

f_mop_floatToDouble = getF("mop_floatToDouble")
def mop_floatToDouble(a, result) :
    f_mop_floatToDouble(a, result)

f_mop_doubleToFloat = getF("mop_doubleToFloat")
def mop_doubleToFloat(a, result) :
    f_mop_doubleToFloat(a, result)

f_mop_doubleToInt = getF("mop_doubleToInt")
def mop_doubleToInt(a, result) :
    f_mop_doubleToInt(a, result)

f_mop_intToDouble = getF("mop_intToDouble")
def mop_intToDouble(a, result) :
    f_mop_intToDouble(a, result)
