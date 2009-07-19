from clay.parserlib import *
from clay import tokens as t
from clay import lexer
from clay.ast import *
from clay.location import Location
from clay.error import SourceError;

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
        start = input.data[start_tok].location.start
        node.location = input.data[input.pos-1].location.new_start(start)
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

array_expr = astnode(sequence(symbol("["), expression_list, symbol("]")),
                     lambda x : ArrayExpr(x[1]))
tuple_expr = astnode(sequence(symbol("("), expression_list, symbol(")")),
                     lambda x : TupleExpr(x[1]))
name_ref = astnode(identifier, lambda x : NameRef(x))
atomic_expr = choice(array_expr, tuple_expr, name_ref, literal)


#
# suffix expr
#

index_suffix = astnode(sequence(symbol("["), expression_list, symbol("]")),
                       lambda x : IndexExpr(None,x[1]))
call_suffix = astnode(sequence(symbol("("), expression_list, symbol(")")),
                      lambda x : CallExpr(None,x[1]))
field_ref_suffix = astnode(sequence(symbol("."), identifier),
                           lambda x : FieldRef(None,x[1]))

suffix = choice(index_suffix, call_suffix, field_ref_suffix)

def fold_suffixes(expr, suffixes) :
    for suffix in suffixes :
        suffix.expr = expr
        suffix.location.start = expr.location.start
        expr = suffix
    return expr

suffix_expr = modify(sequence(atomic_expr, zeroplus(suffix)),
                     lambda x : fold_suffixes(x[0], x[1]))


#
# expression
#

expression = suffix_expr


#
# type spec
#

type_spec = modify(sequence(symbol(":"), expression), lambda x : x[1])


#
# statements
#

return_statement = astnode(sequence(keyword("return"), optional(expression),
                                    semicolon),
                           lambda x : ReturnStatement(x[1]))
assignment = astnode(sequence(identifier, symbol("="), expression, semicolon),
                     lambda x : Assignment(x[0], x[2]))
local_variable_def = astnode(sequence(keyword("var"), identifier, type_spec,
                                      symbol("="), expression, semicolon),
                             lambda x : LocalVariableDef(x[1],x[2],x[4]))

statement2 = lambda input : statement(input)

if_condition = modify(sequence(symbol("("), expression, symbol(")")),
                      lambda x : x[1])
else_part = modify(sequence(keyword("else"), statement2),
                   lambda x : x[1])
if_statement = astnode(sequence(keyword("if"), if_condition,
                                statement2, optional(else_part)),
                       lambda x : IfStatement(x[1],x[2],x[3]))
block = astnode(sequence(symbol("{"), oneplus(statement2), symbol("}")),
                lambda x : Block(x[1]))

expr_statement = modify(sequence(expression, semicolon),
                        lambda x : x[0])

statement = choice(block, local_variable_def, assignment,
                   if_statement, return_statement, expr_statement)


#
# top level items
#

name_list = listof(identifier, comma)
type_vars = modify(sequence(symbol("["), name_list, symbol("]")),
                   lambda x : x[1])
opt_type_vars = modify(optional(type_vars), lambda x : [] if x is None else x)
field_def = astnode(sequence(identifier, type_spec),
                   lambda x : Field(x[0],x[1]))
field_list = listof(field_def, comma)
record_def = astnode(sequence(keyword("record"), identifier, opt_type_vars,
                              symbol("{"), field_list, symbol("}")),
                     lambda x : RecordDef(x[1],x[2],x[4]))
variable_def = astnode(sequence(keyword("var"), identifier, type_spec,
                                symbol("="), expression, semicolon),
                       lambda x : VariableDef(x[1],x[2],x[4]))

opt_name_list = modify(optional(name_list), lambda x : [] if x is None else x)
argument = astnode(sequence(identifier, type_spec),
                   lambda x : Argument(x[0],x[1]))
opt_arguments = modify(optional(listof(argument, comma)),
                       lambda x : [] if x is None else x)
procedure_def = astnode(sequence(keyword("def"), identifier, opt_type_vars,
                                 symbol("("), opt_arguments, symbol(")"),
                                 type_spec, block),
                        lambda x : ProcedureDef(x[1],x[2],x[4],x[6],x[7]))

top_level_item = choice(record_def, variable_def, procedure_def)


#
# program
#

program = astnode(oneplus(top_level_item), lambda x : Program(x))


#
# parse
#

class ParseError(SourceError) :
    pass

def parse(data, file_name) :
    tokens = lexer.tokenize(data, file_name)
    input = Input(tokens)
    result = program(input)
    if (result is Failure) or (input.pos < len(tokens)) :
        if input.max_pos == len(tokens) :
            pos = len(data)
        else :
            pos = tokens[input.max_pos].location.start
        raise ParseError("parse error", data, pos, file_name)
    return result
