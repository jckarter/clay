from clay.ast import *

__all__ = ["Env", "EnvEntry",
           "BoolTypeEntry", "CharTypeEntry", "IntTypeEntry", "VoidTypeEntry",
           "ArrayTypeEntry", "PointerTypeEntry",
           "PrimOpDefault",
           "PrimOpPointerOffset", "PrimOpPointerDifference",
           "PrimOpPointerCast", "PrimOpAllocate", "PrimOpFree",
           "PrimOpAllocateBlock", "PrimOpFreeBlock", "PrimOpBlockSize",
           "PrimOpArray", "PrimOpArraySize",
           "PrimOpBoolNot",
           "PrimOpCharToInt", "PrimOpIntToChar", "PrimOpCharEquals",
           "PrimOpCharLesser", "PrimOpCharLesserEquals", "PrimOpCharGreater",
           "PrimOpCharGreaterEquals",
           "PrimOpIntAdd", "PrimOpIntSubtract", "PrimOpIntMultiply",
           "PrimOpIntDivide", "PrimOpIntModulus", "PrimOpIntNegate",
           "PrimOpIntEquals", "PrimOpIntLesser", "PrimOpIntLesserEquals",
           "PrimOpIntGreater", "PrimOpIntGreaterEquals",
           "PredicateEntry", "InstanceEntry", "RecordEntry",
           "VariableDefEntry", "VariableEntry", "ProcedureEntry",
           "OverloadableEntry", "OverloadEntry", "TypeVarEntry",
           "LocalVariableEntry"]



#
# Env
#

class Env(object) :
    def __init__(self, parent=None) :
        self.parent = parent
        self.entries = {}

    def hasEntry(self, name) :
        return name in self.entries

    def add(self, name, entry) :
        assert type(name) is str
        assert name not in self.entries
        assert isinstance(entry, EnvEntry)
        self.entries[name] = entry

    def lookup(self, name) :
        assert type(name) is str
        entry = self.entries.get(name)
        if (entry is None) and (self.parent is not None) :
            return self.parent.lookup(name)
        return entry



#
# env entries
#

class EnvEntry(object) :
    def __init__(self, env, ast) :
        self.env = env
        self.ast = ast



#
# builtin entries
#

class BoolTypeEntry(EnvEntry) : pass
class CharTypeEntry(EnvEntry) : pass
class IntTypeEntry(EnvEntry) : pass
class VoidTypeEntry(EnvEntry) : pass
class ArrayTypeEntry(EnvEntry) : pass
class PointerTypeEntry(EnvEntry) : pass

class PrimOpDefault(EnvEntry) : pass
class PrimOpPointerOffset(EnvEntry) : pass
class PrimOpPointerDifference(EnvEntry) : pass
class PrimOpPointerCast(EnvEntry) : pass
class PrimOpAllocate(EnvEntry) : pass
class PrimOpFree(EnvEntry) : pass
class PrimOpAllocateBlock(EnvEntry) : pass
class PrimOpFreeBlock(EnvEntry) : pass
class PrimOpBlockSize(EnvEntry) : pass
class PrimOpArray(EnvEntry) : pass
class PrimOpArraySize(EnvEntry) : pass

class PrimOpBoolNot(EnvEntry) : pass

class PrimOpCharToInt(EnvEntry) : pass
class PrimOpIntToChar(EnvEntry) : pass
class PrimOpCharEquals(EnvEntry) : pass
class PrimOpCharLesser(EnvEntry) : pass
class PrimOpCharLesserEquals(EnvEntry) : pass
class PrimOpCharGreater(EnvEntry) : pass
class PrimOpCharGreaterEquals(EnvEntry) : pass

class PrimOpIntAdd(EnvEntry) : pass
class PrimOpIntSubtract(EnvEntry) : pass
class PrimOpIntMultiply(EnvEntry) : pass
class PrimOpIntDivide(EnvEntry) : pass
class PrimOpIntModulus(EnvEntry) : pass
class PrimOpIntNegate(EnvEntry) : pass
class PrimOpIntEquals(EnvEntry) : pass
class PrimOpIntLesser(EnvEntry) : pass
class PrimOpIntLesserEquals(EnvEntry) : pass
class PrimOpIntGreater(EnvEntry) : pass
class PrimOpIntGreaterEquals(EnvEntry) : pass



#
# global entries
#

class PredicateEntry(EnvEntry) :
    def __init__(self, env, ast) :
        super(PredicateEntry, self).__init__(env, ast)
        self.instances = []

class InstanceEntry(EnvEntry) : pass

class RecordEntry(EnvEntry) : pass

class VariableDefEntry(EnvEntry) :
    def __init__(self, env, ast) :
        super(VariableDefEntry).__init__(env, ast)
        self.typeList = None

class VariableEntry(EnvEntry) :
    def __init__(self, env, defEntry, index) :
        ast = defEntry.ast.variables[index]
        super(VariableEntry, self).__init__(env, ast)
        self.defEntry = defEntry
        self.index = index

class ProcedureEntry(EnvEntry) :
    def __init__(self, env, ast) :
        super(ProcedureEntry, self).__init__(env, ast)
        self.returnTypes = {}

class OverloadableEntry(EnvEntry) :
    def __init__(self, env, ast) :
        super(OverloadableEntry, self).__init__(env, ast)
        self.overloads = []

class OverloadEntry(EnvEntry) : pass



#
# local entries
#

class TypeVarEntry(EnvEntry) :
    def __init__(self, env, ast, type) :
        super(TypeVarEntry, self).__init__(env, ast)
        self.type = type

class LocalVariableEntry(EnvEntry) :
    def __init__(self, env, ast, type) :
        super(LocalVariableEntry, self).__init__(env, ast)
        self.type = type
