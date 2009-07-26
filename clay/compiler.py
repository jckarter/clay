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

def lookup_field(fieldname, env, node) :
    ftype = env.lookup_str(fieldname)
    if ftype is None :
        raise_error("invalid field", node)
    return ftype



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
    entry = InstanceEntry(x)
    entry.env = env
    lookup_predicate(x.name, env).instances.append(entry)

@add_top_level.register(RecordDef)
def f(x, env) :
    env.add_name(x.name, RecordEntry(x))

@add_top_level.register(StructDef)
def f(x, env) :
    env.add_name(x.name, StructEntry(x))

@add_top_level.register(VariableDef)
def f(x, env) :
    def_entry = VariableDefEntry(x)
    def_entry.env = env
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
    entry = OverloadEntry(x)
    entry.env = env
    lookup_overloadable(x.name, env).overloads.append(entry)



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

def take_one(container, memberlist, kind) :
    if len(memberlist) > 1 :
        raise_error("only one %s allowed" % kind, memberlist[1])
    if len(memberlist) < 1 :
        raise_error("missing %s" % kind, container)
    return memberlist[0]

def take_n(n, container, memberlist, kind) :
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
    if is_array_type(indexable_type) :
        return True, indexable_type.type
    elif is_array_value_type(indexable_type) :
        return True, indexable_type.type
    else :
        raise_error("array type expected", x.expr)



# begin compile_named_index_expr

compile_named_index_expr = multimethod(default=False)

@compile_named_index_expr.register(ArrayTypeEntry)
def f(entry, x, env) :
    type_param = take_one(x, x.indexes, "type parameter")
    return False, ArrayType(compile_expr_as_type(type_param, env))

@compile_named_index_expr.register(ArrayValueTypeEntry)
def f(entry, x, env) :
    type_param,size = take_n(2, x, x.indexes, "type parameters")
    if type(size) is not IntLiteral :
        raise_error("int literal expected", size)
    if size.value < 0 :
        raise_error("array size cannot be negative", size)
    y = compile_expr_as_type(type_param, env)
    return False, ArrayValueType(y, size.value)

@compile_named_index_expr.register(RefTypeEntry)
def f(entry, x, env) :
    type_param = take_one(x, x.indexes, "type parameter")
    return False, RefType(compile_expr_as_type(type_param, env))

@compile_named_index_expr.register(RecordEntry)
def f(entry, x, env) :
    n_type_vars = len(entry.record_def.type_vars)
    type_params = take_n(n_type_vars, x, x.indexes, "type parameters")
    type_params = [compile_expr_as_type(y,env) for y in type_params]
    return False, RecordType(entry, type_params)

@compile_named_index_expr.register(StructEntry)
def f(entry, x, env) :
    n_type_vars = len(entry.record_def.type_vars)
    type_params = take_n(n_type_vars, x, x.indexes, "type parameters")
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
    if is_record_type(callable_type) :
        field_types = compute_record_field_types(callable_type)
        if len(x.args) != len(field_types) :
            raise_error("incorrect number of arguments", x)
        for arg, field_type in zip(x.args, field_types) :
            arg_type = compile_expr_as_value(arg, env)
            if not type_equals(arg_type, field_type) :
                raise_error("type mismatch in argument", arg)
        return callable_type
    elif is_struct_type(callable_type) :
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

@compile_named_call_expr.register(PrimOpDefault)
def f(entry, x, env) :
    arg_type = compile_expr_as_type(take_one(x,x.args,"argument"), env)
    return True, arg_type

@compile_named_call_expr.register(PrimOpRefGet)
def f(entry, x, env) :
    arg = take_one(x, x.args, "argument")
    ref_type = compile_expr_as_value(arg, env)
    if not is_ref_type(ref_type) :
        raise_error("Ref type expected", arg)
    return True, ref_type.type

@compile_named_call_expr.register(PrimOpRefSet)
def f(entry, x, env) :
    args = take_n(2, x, x.args, "arguments")
    ref_type, value_type = [compile_expr_as_value(y,env) for y in args]
    if not is_ref_type(ref_type) :
        raise_error("Ref type expected", args[0])
    if not type_equals(ref_type.type, value_type) :
        raise_error("type mismatch", args[1])
    return True, void_type

@compile_named_call_expr.register(PrimOpRefOffset)
def f(entry, x, env) :
    args = take_n(2, x, x.args, "arguments")
    ref_type, offset_type = [compile_expr_as_value(y,env) for y in args]
    if not is_ref_type(ref_type) :
        raise_error("Ref type expected", args[0])
    if not is_int_type(offset_type) :
        raise_error("Int type expected", args[1])
    return True, ref_type

@compile_named_call_expr.register(PrimOpRefDifference)
def f(entry, x, env) :
    args = take_n(2, x, x.args, "arguments")
    ref_type1, ref_type2 = [compile_expr_as_value(y,env) for y in args]
    if not is_ref_type(ref_type1) :
        raise_error("Ref type expected", args[0])
    if not is_ref_type(ref_type2) :
        raise_error("Ref type expected", args[1])
    if not type_equals(ref_type1, ref_type2) :
        raise_error("reference types differ", args[1])
    return True, int_type

@compile_named_call_expr.register(PrimOpTupleRef)
def f(entry, x, env) :
    args = take_n(2, x, x.args, "arguments")
    tuple_ref = compile_expr_as_value(args[0], env)
    if (not is_ref_type(tuple_ref)) or (not is_tuple_type(tuple_ref.type)) :
        raise_error("reference to tuple type expected", args[0])
    if type(args[1]) is not IntLiteral :
        raise_error("int literal expected", args[1])
    i = args[1].value
    if (i < 0) or (i >= len(tuple_ref.type.types)) :
        raise_error("tuple index out of range", args[1])
    return True, tuple_ref.type.types[i]

@compile_named_call_expr.register(PrimOpNewArray)
def f(entry, x, env) :
    if len(x.args) == 1 :
        element_type = compile_expr_as_type(x.args[0], env)
        return True, ArrayType(element_type)
    elif len(x.args) == 2 :
        is_value, arg1_type = compile_expr(x.args[0], env)
        arg2_type = compile_expr_as_value(x.args[1], env)
        if is_value :
            if not is_int_type(arg1_type) :
                raise_error("Int type expected", x.args[0])
            return True, ArrayType(arg2_type)
        else :
            if not is_int_type(arg2_type) :
                raise_error("Int type expected", x.args[1])
            return True, ArrayType(arg1_type)
    else :
        raise_error("incorrect number of arguments", x)

@compile_named_call_expr.register(PrimOpArraySize)
def f(entry, x, env) :
    arg = take_one(x, x.args, "argument")
    arg_type = compile_expr_as_value(arg, env)
    if not is_array_type(arg_type) :
        raise_error("Array type expected", arg)
    return True, int_type

@compile_named_call_expr.register(PrimOpArrayRef)
def f(entry, x, env) :
    args = take_n(2, x, x.args, "arguments")
    arg_type1, arg_type2 = [compile_expr_as_value(y,env) for y in args]
    if not is_array_type(arg_type1) :
        raise_error("Array type expected", args[0])
    if not is_int_type(arg_type2) :
        raise_error("Int type expected", args[1])
    return True, RefType(arg_type1.type)

@compile_named_call_expr.register(PrimOpArrayValue)
def f(entry, x, env) :
    args = take_n(2, x, x.args, "arguments")
    if type(args[0]) is IntLiteral :
        size = args[0].value
        if size < 0 :
            raise_error("invalid array size", args[0])
        element_type = compile_expr_as_value(args[1], env)
        return True, ArrayValueType(element_type, size)
    else :
        arg1_type = compile_expr_as_type(args[0], env)
        if type(args[1]) is not IntLiteral :
            raise_error("Int literal expected", args[1])
        size = args[1].value
        if size < 0 :
            raise_error("invalid array size", args[1])
        return True, ArrayValueType(arg1_type, size)

@compile_named_call_expr.register(PrimOpArrayValueRef)
def f(entry, x, env) :
    args = take_n(2, x, x.args, "arguments")
    arg1_type, arg2_type = [compile_expr_as_value(y, env) for y in args]
    if not is_array_value_type(arg1_type) :
        raise_error("ArrayValue type expected", args[0])
    if not is_int_type(arg2_type) :
        raise_error("Int type expected", args[1])
    return True, arg1_type.type

def compute_record_field_type_map(record_type) :
    ftypes = compute_record_field_types(record_type)
    fields = record_type.record_entry.record_def.fields
    env = Env()
    for f,ftype in zip(fields,ftypes) :
        env.add_name(f.name, ftype)
    return env

def compute_struct_field_type_map(struct_type) :
    ftypes = compute_struct_field_types(struct_type)
    fields = struct_type.struct_entry.struct_def.fields
    env = Env()
    for f,ftype in zip(fields,ftypes) :
        env.add_name(f.name, ftype)
    return env

@compile_named_call_expr.register(PrimOpRecordRef)
def f(entry, x, env) :
    args = take_n(2, x, x.args, "arguments")
    arg1_type = compile_expr_as_value(args[0], env)
    if not is_record_type(arg1_type) :
        raise_error("record type expected", args[0])
    if type(args[1]) is not StringLiteral :
        raise_error("string literal expected", args[1])
    type_map = compute_record_field_type_map(arg1_type)
    ftype = lookup_field(args[1].value, type_map, args[1])
    return True, RefType(ftype)

@compile_named_call_expr.register(PrimOpStructRef)
def f(entry, x, env) :
    args = take_n(2, x, x.args, "arguments")
    arg1_type = compile_expr_as_value(args[0], env)
    if (not is_ref_type(arg1_type)) or (not is_struct_type(arg1_type.type)) :
        raise_error("reference to struct type expected", args[0])
    if type(args[1]) is not StringLiteral :
        raise_error("string literal expected", args[1])
    type_map = compute_struct_field_type_map(arg1_type.type)
    ftype = lookup_field(args[1].value, type_map, args[1])
    return True, RefType(ftype)

@compile_named_call_expr.register(PrimOpBoolNot)
def f(entry, x, env) :
    arg = take_one(x, x.xargs, "argument")
    arg_type = compile_expr_as_value(arg, env)
    if not is_bool_type(arg_type) :
        raise_error("Bool type expected", arg)
    return True, bool_type

@compile_named_call_expr.register(PrimOpCharToInt)
def f(entry, x, env) :
    arg = take_one(x, x.args, "argument")
    arg_type = compile_expr_as_value(arg, env)
    if not is_char_type(arg_type) :
        raise_error("Char type expected", arg)
    return True, int_type

@compile_named_call_expr.register(PrimOpIntToChar)
def f(entry, x, env) :
    arg = take_one(x, x.args, "argument")
    arg_type = compile_expr_as_value(arg, env)
    if not is_int_type(arg_type) :
        raise_error("Int type expected", arg)
    return True, char_type

@compile_named_call_expr.register(PrimOpCharEquals)
def f(entry, x, env) :
    args = take_n(2, x, x.args, "arguments")
    for y in args :
        arg_type = compile_expr_as_value(y, env)
        if not is_char_type(arg_type) :
            raise_error("Char type expected", y)
    return True, bool_type

@compile_named_call_expr.register(PrimOpCharLesser)
def f(entry, x, env) :
    args = take_n(2, x, x.args, "arguments")
    for y in args :
        arg_type = compile_expr_as_value(y, env)
        if not is_char_type(arg_type) :
            raise_error("Char type expected", y)
    return True, bool_type

@compile_named_call_expr.register(PrimOpCharLesserEquals)
def f(entry, x, env) :
    args = take_n(2, x, x.args, "arguments")
    for y in args :
        arg_type = compile_expr_as_value(y, env)
        if not is_char_type(arg_type) :
            raise_error("Char type expected", y)
    return True, bool_type

@compile_named_call_expr.register(PrimOpCharGreater)
def f(entry, x, env) :
    args = take_n(2, x, x.args, "arguments")
    for y in args :
        arg_type = compile_expr_as_value(y, env)
        if not is_char_type(arg_type) :
            raise_error("Char type expected", y)
    return True, bool_type

@compile_named_call_expr.register(PrimOpCharGreaterEquals)
def f(entry, x, env) :
    args = take_n(2, x, x.args, "arguments")
    for y in args :
        arg_type = compile_expr_as_value(y, env)
        if not is_char_type(arg_type) :
            raise_error("Char type expected", y)
    return True, bool_type

@compile_named_call_expr.register(PrimOpIntAdd)
def f(entry, x, env) :
    args = take_n(2, x, x.args, "arguments")
    for y in args :
        arg_type = compile_expr_as_value(y, env)
        if not is_int_type(arg_type) :
            raise_error("Int type expected", y)
    return True, int_type

@compile_named_call_expr.register(PrimOpIntSubtract)
def f(entry, x, env) :
    args = take_n(2, x, x.args, "arguments")
    for y in args :
        arg_type = compile_expr_as_value(y, env)
        if not is_int_type(arg_type) :
            raise_error("Int type expected", y)
    return True, int_type

@compile_named_call_expr.register(PrimOpIntMultiply)
def f(entry, x, env) :
    args = take_n(2, x, x.args, "arguments")
    for y in args :
        arg_type = compile_expr_as_value(y, env)
        if not is_int_type(arg_type) :
            raise_error("Int type expected", y)
    return True, int_type

@compile_named_call_expr.register(PrimOpIntDivide)
def f(entry, x, env) :
    args = take_n(2, x, x.args, "arguments")
    for y in args :
        arg_type = compile_expr_as_value(y, env)
        if not is_int_type(arg_type) :
            raise_error("Int type expected", y)
    return True, int_type

@compile_named_call_expr.register(PrimOpIntModulus)
def f(entry, x, env) :
    args = take_n(2, x, x.args, "arguments")
    for y in args :
        arg_type = compile_expr_as_value(y, env)
        if not is_int_type(arg_type) :
            raise_error("Int type expected", y)
    return True, int_type

@compile_named_call_expr.register(PrimOpIntNegate)
def f(entry, x, env) :
    arg = take_one(x, x.args, "argument")
    arg_type = compile_expr_as_value(arg, env)
    if not is_int_type(arg_type) :
        raise_error("Int type expected", arg)
    return True, int_type

@compile_named_call_expr.register(PrimOpIntEquals)
def f(entry, x, env) :
    args = take_n(2, x, x.args, "arguments")
    for y in args :
        arg_type = compile_expr_as_value(y, env)
        if not is_int_type(arg_type) :
            raise_error("Int type expected", y)
    return True, bool_type

@compile_named_call_expr.register(PrimOpIntLesser)
def f(entry, x, env) :
    args = take_n(2, x, x.args, "arguments")
    for y in args :
        arg_type = compile_expr_as_value(y, env)
        if not is_int_type(arg_type) :
            raise_error("Int type expected", y)
    return True, bool_type

@compile_named_call_expr.register(PrimOpIntLesserEquals)
def f(entry, x, env) :
    args = take_n(2, x, x.args, "arguments")
    for y in args :
        arg_type = compile_expr_as_value(y, env)
        if not is_int_type(arg_type) :
            raise_error("Int type expected", y)
    return True, bool_type

@compile_named_call_expr.register(PrimOpIntGreater)
def f(entry, x, env) :
    args = take_n(2, x, x.args, "arguments")
    for y in args :
        arg_type = compile_expr_as_value(y, env)
        if not is_int_type(arg_type) :
            raise_error("Int type expected", y)
    return True, bool_type

@compile_named_call_expr.register(PrimOpIntGreaterEquals)
def f(entry, x, env) :
    args = take_n(2, x, x.args, "arguments")
    for y in args :
        arg_type = compile_expr_as_value(y, env)
        if not is_int_type(arg_type) :
            raise_error("Int type expected", y)
    return True, bool_type

def bind_type_variables(tvar_names, env) :
    tvars = []
    for tvar_name in tvar_names :
        tvar = TypeVariable(tvar_name)
        tvars.append(tvar)
        env.add_name(tvar_name, tvar)
    return tvars

@compile_named_call_expr.register(RecordEntry)
def f(entry, x, env) :
    rdef = entry.record_def
    env2 = Env(entry.env)
    tvars = bind_type_variables(rdef.type_vars, env2)
    if len(x.args) != len(rdef.fields) :
        raise_error("incorrect number of arguments", x)
    for field, arg in zip(rdef.fields, x.args) :
        field_type = compile_expr_as_type(field.type, env2)
        arg_type = compile_expr_as_value(arg, env)
        if not type_unify(arg_type, field_type) :
            raise_error("type mismatch", arg)
    type_params = [tvar.deref() for tvar in tvars]
    return True, RecordType(entry, type_params)

@compile_named_call_expr.register(StructEntry)
def f(entry, x, env) :
    sdef = entry.struct_def
    env2 = Env(entry.env)
    tvars = bind_type_variables(sdef.type_vars, env2)
    if len(x.args) != len(sdef.fields) :
        raise_error("incorrect number of arguments", x)
    for field, arg in zip(sdef.fields, x.args) :
        field_type = compile_expr_as_type(field.type, env2)
        arg_type = compile_expr_as_value(arg, env)
        if not type_unify(arg_type, field_type) :
            raise_error("type mismatch", arg)
    type_params = [tvar.deref() for tvar in tvars]
    return True, StructType(entry, type_params)

@compile_named_call_expr.register(ProcedureEntry)
def f(entry, x, env) :
    assert False

@compile_named_call_expr.register(OverloadableEntry)
def f(entry, x, env) :
    assert False

@compile_named_call_expr.register(TypeVarEntry)
def f(entry, x, env) :
    assert False

# end compile_named_call_expr



#
# remove temp name used for multimethod instances
#

del f
