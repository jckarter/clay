from clay.parserlib import *
from clay import tokens as t
from clay import lexer
from clay.ast import *
from clay.error import raise_error

__all__ = ["parse"]

#
# primitives
#

def token_type(klass) :
    def predicate(x) : return type(x) is klass
    return modify(condition(predicate), lambda x : x.value)

def token_value(klass, value) :
    def predicate(x) :
        return (type(x) is klass) and (x.value == value)
    return modify(condition(predicate), lambda x : x.value)

def symbol(s) : return token_value(t.Symbol, s)
def keyword(s) : return token_value(t.Keyword, s)

identifier = token_type(t.Identifier)

semicolon = symbol(";")
comma = symbol(",")


#
# astnode
#

def astnode(parser, constructor) :
    def parser_proc(input) :
        start_tok = input.pos
        result = parser(input)
        if result is Failure :
            return Failure
        node = constructor(result)
        node.location = input.data[start_tok].location
        return node
    return parser_proc


#
# literals
#

string_literal = astnode(token_type(t.StringLiteral),
                         lambda x : StringLiteral(x))
char_literal = astnode(token_type(t.CharLiteral), lambda x : CharLiteral(x))
int_literal = astnode(token_type(t.IntLiteral), lambda x : IntLiteral(x))
bool_literal = choice(astnode(keyword("true"), lambda x : BoolLiteral(True)),
                      astnode(keyword("false"), lambda x : BoolLiteral(False)))
literal = choice(bool_literal, int_literal, char_literal, string_literal)


#
# atomic expr
#

expression2 = lambda input : expression(input)
expression_list = listof(expression2, comma)
opt_expression_list = modify(optional(expression_list),
                             lambda x : [] if x is None else x)

array_expr = astnode(sequence(symbol("["), expression_list, symbol("]")),
                     lambda x : ArrayExpr(x[1]))
tuple_expr = astnode(sequence(symbol("("), expression_list, symbol(")")),
                     lambda x : TupleExpr(x[1]))
name_ref = astnode(identifier, lambda x : NameRef(x))
atomic_expr = choice(array_expr, tuple_expr, name_ref, literal)


#
# suffix expr
#

name = astnode(identifier, lambda x : Name(x))

index_suffix = astnode(sequence(symbol("["), expression_list, symbol("]")),
                       lambda x : IndexExpr(None,x[1]))
call_suffix = astnode(sequence(symbol("("), opt_expression_list, symbol(")")),
                      lambda x : CallExpr(None,x[1]))
field_ref_suffix = astnode(sequence(symbol("."), name),
                           lambda x : FieldRef(None,x[1]))
tuple_ref_suffix = astnode(sequence(symbol("."), int_literal),
                           lambda x : TupleRef(None,x[1]))
pointer_ref_suffix = astnode(symbol("^"), lambda x : PointerRef(None))

suffix = choice(index_suffix, call_suffix, field_ref_suffix,
                tuple_ref_suffix, pointer_ref_suffix)

def fold_suffixes(expr, suffixes) :
    for suffix in suffixes :
        suffix.expr = expr
        suffix.location = expr.location
        expr = suffix
    return expr

suffix_expr = modify(sequence(atomic_expr, zeroplus(suffix)),
                     lambda x : fold_suffixes(x[0], x[1]))


#
# expression
#

address_of_expr = astnode(sequence(symbol("&"), suffix_expr),
                          lambda x : AddressOfExpr(x[1]))

expression = choice(address_of_expr, suffix_expr)


#
# type spec, variable, variable_list
#

type_spec = modify(sequence(symbol(":"), expression), lambda x : x[1])
opt_type_spec = optional(type_spec)

variable = astnode(sequence(name, opt_type_spec),
                   lambda x : Variable(x[0],x[1]))
variable_list = listof(variable, comma)

#
# statements
#

statement2 = lambda input : statement(input)

return_statement = astnode(sequence(keyword("return"), opt_expression_list,
                                    semicolon),
                           lambda x : ReturnStatement(x[1]))
for_statement = astnode(sequence(keyword("for"), symbol("("), keyword("var"),
                                 variable_list, keyword("in"), expression,
                                 symbol(")"), statement2),
                        lambda x : ForStatement(x[3], x[5], x[7]))
paren_condition = modify(sequence(symbol("("), expression, symbol(")")),
                         lambda x : x[1])
while_statement = astnode(sequence(keyword("while"), paren_condition,
                                   statement2),
                          lambda x : WhileStatement(x[1],x[2]))
continue_statement = astnode(sequence(keyword("continue"), semicolon),
                             lambda x : ContinueStatement())
break_statement = astnode(sequence(keyword("break"), semicolon),
                          lambda x : BreakStatement())
else_part = modify(sequence(keyword("else"), statement2),
                   lambda x : x[1])
if_statement = astnode(sequence(keyword("if"), paren_condition,
                                statement2, optional(else_part)),
                       lambda x : IfStatement(x[1],x[2],x[3]))

assignment = astnode(sequence(expression_list, symbol("="), expression,
                              semicolon),
                     lambda x : Assignment(x[0], x[2]))
local_variable_def = astnode(sequence(keyword("var"), variable_list,
                                      symbol("="), expression, semicolon),
                             lambda x : LocalVariableDef(x[1],x[3]))

block = astnode(sequence(symbol("{"), oneplus(statement2), symbol("}")),
                lambda x : Block(x[1]))

expr_statement = astnode(sequence(expression, semicolon),
                         lambda x : ExprStatement(x[0]))

statement = choice(block, local_variable_def, assignment,
                   if_statement, break_statement, continue_statement,
                   while_statement, for_statement, return_statement,
                   expr_statement)


#
# top level items
#

name_list = listof(name, comma)
type_vars = modify(sequence(symbol("["), name_list, symbol("]")),
                   lambda x : x[1])
opt_type_vars = modify(optional(type_vars), lambda x : [] if x is None else x)
field_def = astnode(sequence(name, type_spec, semicolon),
                    lambda x : Field(x[0],x[1]))
record_def = astnode(sequence(keyword("record"), name, opt_type_vars,
                              symbol("{"), oneplus(field_def), symbol("}")),
                     lambda x : RecordDef(x[1],x[2],x[4]))
struct_def = astnode(sequence(keyword("struct"), name, opt_type_vars,
                              symbol("{"), oneplus(field_def), symbol("}")),
                     lambda x : StructDef(x[1],x[2],x[4]))
variable_def = astnode(sequence(keyword("var"), variable_list,
                                symbol("="), expression, semicolon),
                       lambda x : VariableDef(x[1],x[3]))

opt_name_list = modify(optional(name_list), lambda x : [] if x is None else x)
is_ref = modify(optional(keyword("ref")), lambda x : x is not None)
value_argument = astnode(sequence(is_ref, variable),
                         lambda x : ValueArgument(x[0],x[1]))
type_argument = astnode(sequence(keyword("type"), expression),
                        lambda x : TypeArgument(x[1]))
argument = choice(value_argument, type_argument)
opt_arguments = modify(optional(listof(argument, comma)),
                       lambda x : [] if x is None else x)
type_condition = astnode(sequence(name, symbol("("), opt_expression_list,
                                  symbol(")")),
                         lambda x : TypeCondition(x[0],x[2]))
type_conditions = modify(sequence(keyword("if"), listof(type_condition,comma)),
                         lambda x : x[1])
opt_type_conditions = modify(optional(type_conditions),
                             lambda x : [] if x is None else x)
procedure_def = astnode(sequence(keyword("def"), name, opt_type_vars,
                                 symbol("("), opt_arguments, symbol(")"),
                                 opt_type_spec, opt_type_conditions, block),
                        lambda x : ProcedureDef(x[1],x[2],x[4],x[6],x[7],x[8]))

overloadable_def = astnode(sequence(keyword("overloadable"), name, semicolon),
                           lambda x : OverloadableDef(x[1]))

overload_def = astnode(sequence(keyword("overload"), name, opt_type_vars,
                                symbol("("), opt_arguments, symbol(")"),
                                opt_type_spec, opt_type_conditions, block),
                       lambda x : OverloadDef(x[1],x[2],x[4],x[6],x[7],x[8]))

predicate_def = astnode(sequence(keyword("predicate"), name, semicolon),
                        lambda x : PredicateDef(x[1]))

instance_def = astnode(sequence(keyword("instance"), name, opt_type_vars,
                                symbol("("), opt_expression_list, symbol(")"),
                                opt_type_conditions, semicolon),
                       lambda x : InstanceDef(x[1],x[2],x[4],x[6]))

top_level_item = choice(predicate_def, instance_def, record_def, struct_def,
                        variable_def, procedure_def, overloadable_def,
                        overload_def)


#
# program
#

program = astnode(oneplus(top_level_item), lambda x : Program(x))


#
# parse
#

def parse(data, file_name) :
    tokens = lexer.tokenize(data, file_name)
    input = Input(tokens)
    result = program(input)
    if (result is Failure) or (input.pos < len(tokens)) :
        if input.max_pos == len(tokens) :
            location = Location(data, len(data), file_name)
        else :
            location = tokens[input.max_pos].location
        raise_error("parse error", location)
    return result
