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
           "PredicateEntry", "RecordEntry", "StructEntry", "VariableDefEntry",
           "VariableEntry", "ProcedureEntry", "OverloadableEntry",
           "TypeVarEntry", "LocalVariableDefEntry", "LocalVariableEntry"]



#
# Env
#

class Env(object) :
    def __init__(self, parent=None) :
        self.parent = parent
        self.entries = {}

    def add_(self, name, value) :
        assert type(name) is str
        assert not name in self.entries
        self.entries[name] = value

    def add_builtin(self, name, value) :
        assert type(name) is str
        self.add_(name, value)

    def add_name(self, name, value) :
        assert type(name) is Name
        if name.s in self.entries :
            raise_error("name redefinition", name)
        self.add_(name.s, value)

    def lookup_(self, name) :
        assert type(name) is str
        entry = self.entries.get(name)
        if (entry is None) and (self.parent is not None) :
            return self.parent.lookup_(name)
        return entry

    def lookup_name(self, name) :
        assert type(name) is Name
        entry = self.lookup_(name.s)
        if entry is None :
            raise_error("undefined name", name)
        return entry

    def lookup_nameref(self, nameref) :
        assert type(nameref) is NameRef
        entry = self.lookup_(nameref.s)
        if entry is None :
            raise_error("undefined name", nameref)
        return entry



#
# env entries
#

class EnvEntry(object) :
    pass



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
    def __init__(self, predicate_def) :
        self.predicate_def = predicate_def
        self.instances = []

class RecordEntry(EnvEntry) :
    def __init__(self, record_def) :
        self.record_def = record_def

class StructEntry(EnvEntry) :
    def __init__(self, struct_def) :
        self.struct_def = struct_def

class VariableDefEntry(object) :
    def __init__(self, variable_def) :
        self.variable_def = variable_def

class VariableEntry(EnvEntry) :
    def __init__(self, def_entry, index) :
        self.def_entry = def_entry
        self.index = index

class ProcedureEntry(EnvEntry) :
    def __init__(self, procedure_def) :
        self.procedure_def = procedure_def

class OverloadableEntry(EnvEntry) :
    def __init__(self, overloadable_def) :
        self.overloadable_def = overloadable_def
        self.overloads = []


#
# local entries
#

class TypeVarEntry(EnvEntry) :
    def __init__(self, type) :
        self.type = type

class ValueArgumentEntry(EnvEntry) :
    def __init__(self, arg) :
        self.arg = arg

class LocalVariableDefEntry(object) :
    def __init__(self, local_variable_def) :
        self.local_variable_def = local_variable_def

class LocalVariableEntry(EnvEntry) :
    def __init__(self, def_entry, index) :
        self.def_entry = def_entry
        self.index = index
