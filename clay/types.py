from clay.multimethod import multimethod
from clay.error import raiseError

__all__ = ["Type", "bool_type", "int_type", "char_type", "void_type",
           "TupleType", "ArrayType", "ArrayValueType", "RefType",
           "RecordType", "StructType",
           "is_primitive_type", "is_bool_type", "is_int_type", "is_char_type",
           "is_void_type", "is_tuple_type", "is_array_type",
           "is_array_value_type", "is_ref_type",
           "is_record_type", "is_struct_type",
           "type_equals", "type_list_equals",
           "type_unify", "type_list_unify", "TypeVariable"]



#
# types
#

class Type(object) :
    pass

class PrimitiveType(Type) :
    def __init__(self, name) :
        self.name = name

bool_type = PrimitiveType("Bool")
int_type = PrimitiveType("Int")
char_type = PrimitiveType("Char")
void_type = PrimitiveType("Void")

class TupleType(Type) :
    def __init__(self, types) :
        self.types = types

class ArrayType(Type) :
    def __init__(self, type) :
        self.type = type

class ArrayValueType(Type) :
    def __init__(self, type, size) :
        self.type = type
        self.size = size

class RefType(Type) :
    def __init__(self, type) :
        self.type = type

class RecordType(Type) :
    def __init__(self, entry, type_params) :
        self.entry = entry
        self.type_params = type_params

class StructType(Type) :
    def __init__(self, entry, type_params) :
        self.entry = entry
        self.type_params = type_params



#
# type predicates
#

def is_primitive_type(t) : return type(t) is PrimitiveType
def is_bool_type(t) : return t is bool_type
def is_int_type(t) : return t is int_type
def is_char_type(t) : return t is char_type
def is_void_type(t) : return t is void_type
def is_tuple_type(t) : return type(t) is TupleType
def is_array_type(t) : return type(t) is ArrayType
def is_array_value_type(t) : return type(t) is ArrayValueType
def is_ref_type(t) : return type(t) is RefType
def is_record_type(t) : return type(t) is RecordType
def is_struct_type(t) : return type(t) is StructType



#
# type_equals
#

type_equals = multimethod(n=2, default=False)

def type_list_equals(a, b) :
    if len(a) != len(b) :
        return False
    for x,y in zip(a,b) :
        if not type_equals(x, y) :
            return False
    return True

@type_equals.register(PrimitiveType, PrimitiveType)
def f(x, y) :
    return x.name == y.name

@type_equals.register(TupleType, TupleType)
def f(x, y) :
    return type_list_equals(x.types, y.types)

@type_equals.register(ArrayType, ArrayType)
def f(x, y) :
    return type_equals(x.type, y.type)

@type_equals.register(ArrayValueType, ArrayValueType)
def f(x, y) :
    return type_equals(x.type, y.type) and (x.size == y.size)

@type_equals.register(RefType, RefType)
def f(x, y) :
    return type_equals(x.type, y.type)

@type_equals.register(RecordType, RecordType)
def f(x, y) :
    return (x.record_entry is y.record_entry) and \
        type_list_equals(x.type_params, y.type_params)

@type_equals.register(StructType, StructType)
def f(x, y) :
    return (x.struct_entry is y.struct_entry) and \
        type_list_equals(x.type_params, y.type_params)



#
# type unification
#

class TypeVariable(Type) :
    def __init__(self, name) :
        self.name = name
        self.type = None

    def deref(self) :
        t = self.deref_()
        if t is None :
            raiseError("unresolved type variable", self.name)
        return t

    def deref_(self) :
        if type(self.type) is TypeVariable :
            return self.type.deref_()
        return self.type


type_unify = multimethod(n=2, defaultProc=type_unify_variables)

def type_list_unify(a, b) :
    if len(a) != len(b) :
        return False
    for x,y in zip(a,b) :
        if not type_unify(x,y) :
            return False
    return True

def type_unify_variables(x, y) :
    if type(x) is TypeVariable :
        if x.type is None :
            x.type = y
            return True
        return type_unify(x.type, y)
    elif type(y) is TypeVariable :
        if y.type is None :
            y.type = x
            return True
        return type_unify(x, y.type)
    assert False

@type_unify.register(PrimitiveType, PrimitiveType)
def f(x, y) :
    return x.name == y.name

@type_unify.register(TupleType, TupleType)
def f(x, y) :
    return type_list_unify(x.types, y.types)

@type_unify.register(ArrayType, ArrayType)
def f(x, y) :
    return type_unify(x.type, y.type)

@type_unify.register(ArrayValueType, ArrayValueType)
def f(x, y) :
    return (x.size == y.size) and type_unify(x.type, y.type)

@type_unify.register(RefType, RefType)
def f(x, y) :
    return type_unify(x.type, y.type)

@type_unify.register(RecordType, RecordType)
def f(x, y) :
    return (x.record_entry is y.record_entry) and \
        type_list_unify(x.type_params, y.type_params)

@type_unify.register(StructType, StructType)
def f(x, y) :
    return (x.struct_entry is y.struct_entry) and \
        type_list_unify(x.type_params, y.type_params)



#
# remove temp name used for multimethod instances
#

del f
