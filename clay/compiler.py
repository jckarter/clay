from clay.ast import *
from clay.env import *
from clay.types import *
from clay.error import *
from clay.multimethod import multimethod



#
# env operations
#

def add_ident(env, name, entry) :
    assert type(name) is Identifier
    if env.has_entry(name.s) :
        raise_error("name redefinition", name)
    env.add(name.s, entry)

def lookup_ident(env, name) :
    assert type(name) is Identifier
    entry = env.lookup(name.s)
    if entry is None :
        raise_error("undefined name", name)
    return entry

def lookup_predicate(env, name) :
    x = lookup_ident(env, name)
    if type(x) is not PredicateEntry :
        raise_error("not a predicate", name)
    return x

def lookup_overloadable(env, name) :
    x = lookup_ident(env, name)
    if type(x) is not OverloadableEntry :
        raise_error("not an overloadable", name)
    return x



#
# primitives_env
#

def primitives_env() :
    env = Env()
    def a(name, klass) :
        env.add(name, klass(env,None))
    a("Bool", BoolTypeEntry)
    a("Char", CharTypeEntry)
    a("Int", IntTypeEntry)
    a("Void", VoidTypeEntry)
    a("Array", ArrayTypeEntry)
    a("Ref", RefTypeEntry)

    a("default", PrimOpDefault)
    a("array", PrimOpArray)
    a("arraySize", PrimOpArraySize)
    a("arrayValue", PrimOpArrayValue)

    a("boolNot", PrimOpBoolNot)
    a("charToInt", PrimOpCharToInt)
    a("intToChar", PrimOpIntToChar)
    a("charEquals", PrimOpCharEquals)
    a("charLesser", PrimOpCharLesser)
    a("charLesserEquals", PrimOpCharLesserEquals)
    a("charGreater", PrimOpCharGreater)
    a("charGreaterEquals", PrimOpCharGreaterEquals)
    a("intAdd", PrimOpIntAdd)
    a("intSubtract", PrimOpIntSubtract)
    a("intMultiply", PrimOpIntMultiply)
    a("intDivide", PrimOpIntDivide)
    a("intModulus", PrimOpIntModulus)
    a("intNegate", PrimOpIntNegate)
    a("intEquals", PrimOpIntEquals)
    a("intLesser", PrimOpIntLesser)
    a("intLesserEquals", PrimOpIntLesserEquals)
    a("intGreater", PrimOpIntGreater)
    a("intGreaterEquals", PrimOpIntGreaterEquals)
    return env



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
    add_ident(env, x.name, PredicateEntry(env,x))

@add_top_level.register(InstanceDef)
def f(x, env) :
    entry = InstanceEntry(env, x)
    lookup_predicate(env, x.name).instances.append(entry)

@add_top_level.register(RecordDef)
def f(x, env) :
    add_ident(env, x.name, RecordEntry(env,x))

@add_top_level.register(StructDef)
def f(x, env) :
    add_ident(env, x.name, StructEntry(env,x))

@add_top_level.register(VariableDef)
def f(x, env) :
    def_entry = VariableDefEntry(env, x)
    for i,variable in enumerate(x.variables) :
        add_ident(env, variable.name, VariableEntry(env,def_entry,i))

@add_top_level.register(ProcedureDef)
def f(x, env) :
    add_ident(env, x.name, ProcedureEntry(env,x))

@add_top_level.register(OverloadableDef)
def f(x, env) :
    add_ident(env, x.name, OverloadableEntry(env,x))

@add_top_level.register(OverloadDef)
def f(x, env) :
    entry = OverloadEntry(env, x)
    lookup_overloadable(env, x.name).overloads.append(entry)



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
        entry = lookup_ident(env, x.expr.name)
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
    n_type_vars = len(entry.ast.type_vars)
    type_params = take_n(n_type_vars, x, x.indexes, "type parameters")
    type_params = [compile_expr_as_type(y,env) for y in type_params]
    return False, RecordType(entry, type_params)

@compile_named_index_expr.register(StructEntry)
def f(entry, x, env) :
    n_type_vars = len(entry.ast.type_vars)
    type_params = take_n(n_type_vars, x, x.indexes, "type parameters")
    type_params = [compile_expr_as_type(y,env) for y in type_params]
    return False, StructType(entry, type_params)

# end compile_named_index_expr


@compile_expr.register(CallExpr)
def f(x, env) :
    if type(x.expr) is NameRef :
        entry = lookup_ident(env, x.expr.name)
        result = compile_named_call_expr(entry, x, env)
        if result is not False :
            return result
    is_value,callable_type = compile_expr(x.expr, env)
    if is_value :
        raise_error("invalid call", x)
    if is_record_type(callable_type) or is_struct_type(callable_type) :
        field_types = compute_field_types(callable_type)
        if len(x.args) != len(field_types) :
            raise_error("incorrect number of arguments", x)
        for arg, field_type in zip(x.args, field_types) :
            arg_type = compile_expr_as_value(arg, env)
            if not type_equals(arg_type, field_type) :
                raise_error("type mismatch in argument", arg)
        return callable_type
    else :
        raise_error("invalid call", x)

def compute_field_types(t) :
    assert is_record_type(t) or is_struct_type(t)
    ast = t.entry.ast
    env = Env(t.entry.env)
    assert len(ast.type_vars) == len(t.type_params)
    for tvar, tparam in zip(ast.type_vars, t.type_params) :
        add_ident(env, tvar, tparam)
    return [compile_expr_as_type(field.type, env) for field in ast.fields]



# begin compile_named_call_expr

compile_named_call_expr = multimethod(default=False)

@compile_named_call_expr.register(PrimOpDefault)
def f(entry, x, env) :
    arg_type = compile_expr_as_type(take_one(x,x.args,"argument"), env)
    return True, arg_type

@compile_named_call_expr.register(PrimOpArray)
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
        add_ident(env, tvar_name, tvar)
    return tvars

@compile_named_call_expr.register(RecordEntry)
def f(entry, x, env) :
    ast = entry.ast
    env2 = Env(entry.env)
    tvars = bind_type_variables(ast.type_vars, env2)
    if len(x.args) != len(ast.fields) :
        raise_error("incorrect number of arguments", x)
    for field, arg in zip(ast.fields, x.args) :
        field_type = compile_expr_as_type(field.type, env2)
        arg_type = compile_expr_as_value(arg, env)
        if not type_unify(arg_type, field_type) :
            raise_error("type mismatch", arg)
    type_params = [tvar.deref() for tvar in tvars]
    return True, RecordType(entry, type_params)

@compile_named_call_expr.register(StructEntry)
def f(entry, x, env) :
    ast = entry.ast
    env2 = Env(entry.env)
    tvars = bind_type_variables(ast.type_vars, env2)
    if len(x.args) != len(ast.fields) :
        raise_error("incorrect number of arguments", x)
    for field, arg in zip(ast.fields, x.args) :
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
