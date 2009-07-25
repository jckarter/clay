from clay.multimethod import multimethod

__all__ = ["Type", "PrimitiveType", "bool_type", "int_type", "char_type",
           "void_type", "TupleType", "ArrayType", "ArrayValueType", "RefType",
           "RecordType", "StructType",
           "type_equals",
           "is_primitive_type", "is_bool_type", "is_int_type", "is_char_type",
           "is_void_type", "is_tuple_type", "is_array_type",
           "is_array_value_type", "is_ref_type",
           "is_record_type", "is_struct_type"]



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
    def __init__(self, record_entry, type_params) :
        self.record_entry = record_entry
        self.type_params = type_params

class StructType(Type) :
    def __init__(self, struct_entry, type_params) :
        self.struct_entry = struct_entry
        self.type_params = type_params



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
# type predicates
#

def is_primitive_type(t) : return type(t) is PrimitiveType
def is_bool_type(t) : return type_equals(t, bool_type)
def is_int_type(t) : return type_equals(t, int_type)
def is_char_type(t) : return type_equals(t, char_type)
def is_void_type(t) : return type_equals(t, void_type)
def is_tuple_type(t) : return type(t) is TupleType
def is_array_type(t) : return type(t) is ArrayType
def is_array_value_type(t) : return type(t) is ArrayValueType
def is_ref_type(t) : return type(t) is RefType
def is_record_type(t) : return type(t) is RecordType
def is_struct_type(t) : return type(t) is StructType
