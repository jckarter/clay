from clay.ast import *
from clay.env import *
from clay.types import *
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
    return env.lookup_name(name)

def lookup_predicate(name, env) :
    x = lookup_name(name, env)
    if type(x) is not PredicateEntry :
        raise_error("not a predicate", name)
    return x

def lookup_overloadable(name, env) :
    x = lookup_name(name, env)
    if type(x) is not OverloadableEntry :
        raise_error("not an overloadable", name)
    return x

def lookup_nameref(nameref, env) :
    return env.lookup_nameref(nameref)



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
# compile_expr : (expr, env) -> (is_value, type)
#

compile_expr = multimethod()

def compile_expr_as_value(x, env) :
    is_value,result_type = compile_expr(x, env)
    if not is_value :
        raise_error("value expected", x)
    return result_type

def compile_expr_as_type(x, env) :
    is_value,result_type = compile_expr(x, env)
    if is_value :
        raise_error("type expected", x)
    return result_type

def list_as_one(container, memberlist, kind) :
    if len(memberlist) > 1 :
        raise_error("only one %s allowed" % kind, memberlist[1])
    if len(memberlist) < 1 :
        raise_error("missing %s" % kind, container)
    return memberlist[0]

def list_as_n(n, container, memberlist, kind) :
    if n == len(memberlist) :
        return memberlist
    if len(memberlist) == 0 :
        raise_error("missing %s" % kind, container)
    raise_error("incorrect number of %s" % kind, container)

@compile_expr.register(AddressOfExpr)
def f(x, env) :
    return True, RefType(compile_expr_as_value(x.expr, env))

@compile_expr.register(IndexExpr)
def f(x, env) :
    if type(x.expr) is NameRef :
        entry = lookup_nameref(x.expr, env)
        result = compile_named_index_expr(entry, x, env)
        if result is not False :
            return result
    indexable_type = compile_expr_as_value(x.expr, env)
    if type(indexable_type) is ArrayType :
        return True, indexable_type.type
    elif type(indexable_type) is ArrayValueType :
        return True, indexable_type.type
    else :
        raise_error("array type expected", x.expr)


# begin compile_named_index_expr

compile_named_index_expr = multimethod(default=False)

@compile_named_index_expr.register(ArrayTypeEntry)
def f(entry, x, env) :
    type_param = list_as_one(x, x.indexes, "type parameter")
    return False, ArrayType(compile_expr_as_type(type_param, env))

@compile_named_index_expr.register(ArrayValueTypeEntry)
def f(entry, x, env) :
    type_param,size = list_as_n(2, x, x.indexes, "type parameters")
    if type(size) is not IntLiteral :
        raise_error("int literal expected", size)
    y = compile_expr_as_type(type_param, env)
    return False, ArrayValueType(y, size.value)

@compile_named_index_expr.register(RefTypeEntry)
def f(entry, x, env) :
    type_param = list_as_one(x, x.indexes, "type parameter")
    return False, RefType(compile_expr_as_type(type_param, env))

@compile_named_index_expr.register(RecordEntry)
def f(entry, x, env) :
    n_type_vars = len(entry.record_def.type_vars)
    type_params = list_as_n(n_type_vars, x, x.indexes, "type parameters")
    type_params = [compile_expr_as_type(y,env) for y in type_params]
    return False, RecordType(entry, type_params)

@compile_named_index_expr.register(StructEntry)
def f (entry, x, env) :
    n_type_vars = len(entry.record_def.type_vars)
    type_params = list_as_n(n_type_vars, x, x.indexes, "type parameters")
    type_params = [compile_expr_as_type(y,env) for y in type_params]
    return False, RecordType(entry, type_params)

# end compile_named_index_expr


@compile_expr.register(CallExpr)
def f(x, env) :
    if type(x.expr) is NameRef :
        entry = lookup_nameref(x.expr, env)
        result = compile_named_call_expr(entry, x, env)
        if result is not False :
            return result
    is_value,callable_type = compile_expr(x.expr, env)
    if is_value :
        raise_error("invalid call", x)
    if type(callable_type) is RecordType :
        field_types = compute_record_field_types(callable_type)
        if len(x.args) != len(field_types) :
            raise_error("incorrect number of arguments", x)
        for arg, field_type in zip(x.args, field_types) :
            arg_type = compile_expr_as_value(arg, env)
            if not type_equals(arg_type, field_type) :
                raise_error("type mismatch in argument", arg)
        return callable_type
    elif type(callable_type) is StructType :
        field_types = compute_struct_field_types(callable_type)
        if len(x.args) != len(field_types) :
            raise_error("incorrect number of arguments", x)
        for arg, field_type in zip(x.args, field_types) :
            arg_type = compile_expr_as_value(arg, env)
            if not type_equals(arg_type, field_type) :
                raise_error("type mismatch in argument", arg)
        return callable_type
    else :
        raise_error("invalid call", x)

def compute_record_field_types(record_type) :
    rentry = record_type.record_entry
    rdef = rentry.record_def
    env = Env(rentry.env)
    assert len(rdef.type_vars) == len(record_type.type_params)
    for tvar, tparam in zip(rdef.type_vars, record_type.type_params) :
        env.add_name(tvar, tparam)
    return [compile_expr_as_type(f.type,env) for f in rdef.fields]

def compute_struct_field_types(struct_type) :
    sentry = struct_type.struct_entry
    sdef = sentry.struct_def
    env = Env(sentry.env)
    assert len(sdef.type_vars) == len(struct_type.type_params)
    for tvar, tparam in zip(sdef.type_vars, struct_type.type_params) :
        env.add_name(tvar, tparam)
    return [compile_expr_as_type(f.type,env) for f in sdef.fields]

# begin compile_named_call_expr

compile_named_call_expr = multimethod(default=False)

# end compile_named_call_expr



#
# remove temp name used for multimethod instances
#

del f
