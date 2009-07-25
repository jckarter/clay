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
