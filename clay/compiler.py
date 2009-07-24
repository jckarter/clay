from clay.ast import *
from clay.env import *
from clay.error import *
from clay.multimethod import multimethod



#
# primitives_env
#

def primitives_env() :
    env = Env()
    a = env.add_builtin
    a("Bool", BoolTypeEntry())
    a("Char", CharTypeEntry())
    a("Int", IntTypeEntry())
    a("Void", VoidTypeEntry())
    a("Array", ArrayTypeEntry())
    a("Ref", RefTypeEntry())
    a("default", PrimOpDefault())
    a("ref_get", PrimOpRefGet())
    a("ref_set", PrimOpRefSet())
    a("ref_offset", PrimOpRefOffset())
    a("ref_difference", PrimOpRefDifference())
    a("tuple_ref", PrimOpTupleRef())
    a("new_array", PrimOpNewArray())
    a("array_size", PrimOpArraySize())
    a("array_ref", PrimOpArrayRef())
    a("array_value", PrimOpArrayValue())
    a("array_value_ref", PrimOpArrayValueRef())
    a("record_ref", PrimOpRecordRef())
    a("struct_ref", PrimOpStructRef())
    a("bool_not", PrimOpBoolNot())
    a("char_to_int", PrimOpCharToInt())
    a("int_to_char", PrimOpIntToChar())
    a("char_equals", PrimOpCharEquals())
    a("char_lesser", PrimOpCharLesser())
    a("char_lesser_equals", PrimOpCharLesserEquals())
    a("char_greater", PrimOpCharGreater())
    a("char_greater_equals", PrimOpCharGreaterEquals())
    a("int_add", PrimOpIntAdd())
    a("int_subtract", PrimOpIntSubtract())
    a("int_multiply", PrimOpIntMultiply())
    a("int_divide", PrimOpIntDivide())
    a("int_modulus", PrimOpIntModulus())
    a("int_negate", PrimOpIntNegate())
    a("int_equals", PrimOpIntEquals())
    a("int_lesser", PrimOpIntLesser())
    a("int_lesser_equals", PrimOpIntLesserEquals())
    a("int_greater", PrimOpIntGreater())
    a("int_greater_equals", PrimOpIntGreaterEquals())
    return env



#
# lookup
#

def lookup_name(name, env) :
    return env.lookup(name)

def lookup_predicate(name, env) :
    x = lookup_name(name, env)
    if type(x) is not PredicateEntry :
        raise ASTError("not a predicate", name)
    return x

def lookup_overloadable(name, env) :
    x = lookup_name(name, env)
    if type(x) is not OverloadableEntry :
        raise ASTError("not an overloadable", name)
    return x



#
# build_top_level_env
#

def build_top_level_env(program) :
    env = Env(primitives_env())
    for item in program.top_level_items :
        add_top_level(item, env)

add_top_level = multimethod()

@add_top_level.register(PredicateDef)
def f(x, env) :
    env.add_name(x.name, PredicateEntry(x))

@add_top_level.register(InstanceDef)
def f(x, env) :
    lookup_predicate(x.name, env).instances.append(x)

@add_top_level.register(RecordDef)
def f(x, env) :
    env.add_name(x.name, RecordEntry(x))

@add_top_level.register(StructDef)
def f(x, env) :
    env.add_name(x.name, StructEntry(x))

@add_top_level.register(VariableDef)
def f(x, env) :
    def_entry = VariableDefEntry(x)
    for i,variable in enumerate(x.variables) :
        env.add_name(variable.name, VariableEntry(def_entry,i))

@add_top_level.register(ProcedureDef)
def f(x, env) :
    env.add_name(x.name, ProcedureEntry(x))

@add_top_level.register(OverloadableDef)
def f(x, env) :
    env.add_name(x.name, OverloadableEntry(x))

@add_top_level.register(OverloadDef)
def f(x, env) :
    lookup_overloadable(x.name, env).overloads.append(x)



#
# remove temp name used for multimethod instances
#

del f
