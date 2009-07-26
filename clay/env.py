from clay.ast import *
from clay.error import raise_error

__all__ = ["Env", "EnvEntry",
           "BoolTypeEntry", "CharTypeEntry", "IntTypeEntry", "VoidTypeEntry",
           "ArrayTypeEntry", "ArrayValueTypeEntry", "RefTypeEntry",
           "PrimOpDefault",
           "PrimOpRefGet", "PrimOpRefSet", "PrimOpRefOffset",
           "PrimOpRefDifference",
           "PrimOpTupleRef",
           "PrimOpNewArray", "PrimOpArraySize", "PrimOpArrayRef",
           "PrimOpArrayValue", "PrimOpArrayValueRef",
           "PrimOpRecordRef",
           "PrimOpStructRef",
           "PrimOpBoolNot",
           "PrimOpCharToInt", "PrimOpIntToChar", "PrimOpCharEquals",
           "PrimOpCharLesser", "PrimOpCharLesserEquals", "PrimOpCharGreater",
           "PrimOpCharGreaterEquals",
           "PrimOpIntAdd", "PrimOpIntSubtract", "PrimOpIntMultiply",
           "PrimOpIntDivide", "PrimOpIntModulus", "PrimOpIntNegate",
           "PrimOpIntEquals", "PrimOpIntLesser", "PrimOpIntLesserEquals",
           "PrimOpIntGreater", "PrimOpIntGreaterEquals",
           "PredicateEntry", "InstanceEntry", "RecordEntry", "StructEntry",
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

    def has_entry(self, name) :
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
class ArrayValueTypeEntry(EnvEntry) : pass
class RefTypeEntry(EnvEntry) : pass

class PrimOpDefault(EnvEntry) : pass

class PrimOpRefGet(EnvEntry) : pass
class PrimOpRefSet(EnvEntry) : pass
class PrimOpRefOffset(EnvEntry) : pass
class PrimOpRefDifference(EnvEntry) : pass

class PrimOpTupleRef(EnvEntry) : pass

class PrimOpNewArray(EnvEntry) : pass
class PrimOpArraySize(EnvEntry) : pass
class PrimOpArrayRef(EnvEntry) : pass

class PrimOpArrayValue(EnvEntry) : pass
class PrimOpArrayValueRef(EnvEntry) : pass

class PrimOpRecordRef(EnvEntry) : pass

class PrimOpStructRef(EnvEntry) : pass

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

class StructEntry(EnvEntry) : pass

class VariableDefEntry(object) : pass

class VariableEntry(EnvEntry) :
    def __init__(self, env, def_entry, index) :
        ast = def_entry.ast.variables[index]
        super(VariableEntry, self).__init__(env, ast)
        self.def_entry = def_entry
        self.index = index

class ProcedureEntry(EnvEntry) : pass

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
