#include "clay.hpp"
#include <sstream>

using namespace std;



//
// overload <<
//

static void print(const Object *x, ostream &out);

ostream &operator<<(ostream &out, const Object &obj) {
    print(&obj, out);
    return out;
}

ostream &operator<<(ostream &out, const Object *obj) {
    print(obj, out);
    return out;
}



//
// big vec
//

template <class T>
struct BigVec {
    const vector<T> *v;
    BigVec(const vector<T> &v)
        : v(&v) {}
};

template <class T>
BigVec<T> bigVec(const vector<T> &v) {
    return BigVec<T>(v);
}

static int indent = 0;

template <class T>
ostream &operator<<(ostream &out, const BigVec<T> &v) {
    ++indent;
    out << "[";
    typename vector<T>::const_iterator i, end;
    bool first = true;
    for (i = v.v->begin(), end = v.v->end(); i != end; ++i) {
        if (!first)
            out << ",";
        first = false;
        out << "\n";
        for (int j = 0; j < indent; ++j)
            out << "  ";
        out << *i;
    }
    out << "]";
    --indent;
    return out;
}



//
// print
//

static void print(const Object *x, ostream &out) {
    if (x == NULL) {
        out << "NULL";
        return;
    }

    switch (x->objKind) {

    case SOURCE : {
        Source *y = (Source *)x;
        out << "Source(" << y->fileName << ")";
        break;
    }
    case LOCATION : {
        Location *y = (Location *)x;
        out << "Location(" << y->source << ", " << y->offset << ")";
        break;
    }
    case TOKEN : {
        Token *y = (Token *)x;
        out << "Token(";
        switch (y->tokenKind) {
        case T_SYMBOL :
            out << "T_SYMBOL";
            break;
        case T_KEYWORD :
            out << "T_KEYWORD";
            break;
        case T_IDENTIFIER :
            out << "T_IDENTIFIER";
            break;
        case T_STRING_LITERAL :
            out << "T_STRING_LITERAL";
            break;
        case T_CHAR_LITERAL :
            out << "T_CHAR_LITERAL";
            break;
        case T_INT_LITERAL :
            out << "T_INT_LITERAL";
            break;
        case T_FLOAT_LITERAL :
            out << "T_FLOAT_LITERAL";
            break;
        case T_LITERAL_SUFFIX :
            out << "T_LITERAL_SUFFIX";
            break;
        case T_SPACE :
            out << "T_SPACE";
            break;
        case T_LINE_COMMENT :
            out << "T_LINE_COMMENT";
            break;
        case T_BLOCK_COMMENT :
            out << "T_BLOCK_COMMENT";
            break;
        default :
            assert(false);
        }
        out << ", " << y->str << ")";
        break;
    }

    case IDENTIFIER : {
        Identifier *y = (Identifier *)x;
        out << "Identifier(" << y->str << ")";
        break;
    }
    case DOTTED_NAME : {
        DottedName *y = (DottedName *)x;
        out << "DottedName(" << y->parts << ")";
        break;
    }

    case BOOL_LITERAL : {
        BoolLiteral *y = (BoolLiteral *)x;
        out << "BoolLiteral(" << y->value << ")";
        break;
    }
    case INT_LITERAL : {
        IntLiteral *y = (IntLiteral *)x;
        out << "IntLiteral(" << y->value;
        if (!y->suffix.empty())
            out << ", " << y->suffix;
        out << ")";
        break;
    }
    case FLOAT_LITERAL : {
        FloatLiteral *y = (FloatLiteral *)x;
        out << "FloatLiteral(" << y->value;
        if (!y->suffix.empty())
            out << ", " << y->suffix;
        out << ")";
        break;
    }
    case CHAR_LITERAL : {
        CharLiteral *y = (CharLiteral *)x;
        out << "CharLiteral(" << y->value << ")";
        break;
    }
    case STRING_LITERAL : {
        StringLiteral *y = (StringLiteral *)x;
        out << "StringLiteral(" << y->value << ")";
        break;
    }

    case NAME_REF : {
        NameRef *y = (NameRef *)x;
        out << "NameRef(" << y->name << ")";
        break;
    }
    case TUPLE : {
        Tuple *y = (Tuple *)x;
        out << "Tuple(" << y->args << ")";
        break;
    }
    case ARRAY : {
        Array *y = (Array *)x;
        out << "Array(" << y->args << ")";
        break;
    }
    case INDEXING : {
        Indexing *y = (Indexing *)x;
        out << "Indexing(" << y->expr << ", " << y->args << ")";
        break;
    }
    case CALL : {
        Call *y = (Call *)x;
        out << "Call(" << y->expr << ", " << y->args << ")";
        break;
    }
    case FIELD_REF : {
        FieldRef *y = (FieldRef *)x;
        out << "FieldRef(" << y->expr << ", " << y->name << ")";
        break;
    }
    case TUPLE_REF : {
        TupleRef *y = (TupleRef *)x;
        out << "TupleRef(" << y->expr << ", " << y->index << ")";
        break;
    }
    case UNARY_OP : {
        UnaryOp *y = (UnaryOp *)x;
        out << "UnaryOp(";
        switch (y->op) {
        case DEREFERENCE :
            out << "DEREFERENCE";
            break;
        case ADDRESS_OF :
            out << "ADDRESS_OF";
            break;
        case PLUS :
            out << "PLUS";
            break;
        case MINUS :
            out << "MINUS";
            break;
        case NOT :
            out << "NOT";
            break;
        default :
            assert(false);
        }
        out << ", " << y->expr << ")";
        break;
    }
    case BINARY_OP : {
        BinaryOp *y = (BinaryOp *)x;
        out << "BinaryOp(";
        switch (y->op) {
        case ADD :
            out << "ADD";
            break;
        case SUBTRACT :
            out << "SUBTRACT";
            break;
        case MULTIPLY :
            out << "MULTIPLY";
            break;
        case DIVIDE :
            out << "DIVIDE";
            break;
        case REMAINDER :
            out << "REMAINDER";
            break;
        case EQUALS :
            out << "EQUALS";
            break;
        case NOT_EQUALS :
            out << "NOT_EQUALS";
            break;
        case LESSER :
            out << "LESSER";
            break;
        case LESSER_EQUALS :
            out << "LESSER_EQUALS";
            break;
        case GREATER :
            out << "GREATER";
            break;
        case GREATER_EQUALS :
            out << "GREATER_EQUALS";
            break;
        default :
            assert(false);
        }
        out << ", " << y->expr1 << ", " << y->expr2 << ")";
        break;
    }
    case AND : {
        And *y = (And *)x;
        out << "And(" << y->expr1 << ", " << y->expr2 << ")";
        break;
    }
    case OR : {
        Or *y = (Or *)x;
        out << "Or(" << y->expr1 << ", " << y->expr2 << ")";
        break;
    }
    case SC_EXPR : {
        SCExpr *y = (SCExpr *)x;
        out << "SCExpr(" << y->expr << ")";
        break;
    }

    case BLOCK : {
        Block *y = (Block *)x;
        out << "Block(" << bigVec(y->statements) << ")";
        break;
    }
    case LABEL : {
        Label *y = (Label *)x;
        out << "Label(" << y->name << ")";
        break;
    }
    case BINDING : {
        Binding *y = (Binding *)x;
        out << "Binding(";
        switch (y->bindingKind) {
        case VAR :
            out << "VAR";
            break;
        case REF :
            out << "REF";
            break;
        case STATIC :
            out << "STATIC";
            break;
        default :
            assert(false);
        }
        out << ", " << y->name << ", " << y->expr << ")";
        break;
    }
    case ASSIGNMENT : {
        Assignment *y = (Assignment *)x;
        out << "Assignment(" << y->left << ", " << y->right << ")";
        break;
    }
    case GOTO : {
        Goto *y = (Goto *)x;
        out << "Goto(" << y->labelName << ")";
        break;
    }
    case RETURN : {
        Return *y = (Return *)x;
        out << "Return(" << y->expr << ")";
        break;
    }
    case RETURN_REF : {
        ReturnRef *y = (ReturnRef *)x;
        out << "ReturnRef(" << y->expr << ")";
        break;
    }
    case IF : {
        If *y = (If *)x;
        out << "If(" << y->condition << ", " << y->thenPart;
        out << ", " << y->elsePart << ")";
        break;
    }
    case EXPR_STATEMENT : {
        ExprStatement *y = (ExprStatement *)x;
        out << "ExprStatement(" << y->expr << ")";
        break;
    }
    case WHILE : {
        While *y = (While *)x;
        out << "While(" << y->condition << ", " << y->body << ")";
        break;
    }
    case BREAK : {
        out << "Break()";
        break;
    }
    case CONTINUE : {
        out << "Continue()";
        break;
    }
    case FOR : {
        For *y = (For *)x;
        out << "For(" << y->variable << ", " << y->expr;
        out << ", " << y->body << ")";
        break;
    }

    case CODE : {
        Code *y = (Code *)x;
        out << "Code(" << y->patternVars << ", " << y->predicate;
        out << ", " << y->formalArgs << ", " << y->body << ")";
        break;
    }
    case VALUE_ARG : {
        ValueArg *y = (ValueArg *)x;
        out << "ValueArg(" << y->name << ", " << y->type << ")";
        break;
    }
    case STATIC_ARG : {
        StaticArg *y = (StaticArg *)x;
        out << "StaticArg(" << y->pattern << ")";
        break;
    }

    case RECORD : {
        Record *y = (Record *)x;
        out << "Record(" << y->name << ", " << y->patternVars;
        out << ", " << y->fields << ")";
        break;
    }
    case RECORD_FIELD : {
        RecordField *y = (RecordField *)x;
        out << "RecordField(" << y->name << ", " << y->type << ")";
        break;
    }
    case PROCEDURE : {
        Procedure *y = (Procedure *)x;
        out << "Procedure(" << y->name << ", " << y->code << ")";
        break;
    }
    case OVERLOAD : {
        Overload *y = (Overload *)x;
        out << "Overload(" << y->name << ", " << y->code << ")";
        break;
    }
    case OVERLOADABLE : {
        Overloadable *y = (Overloadable *)x;
        out << "Overloadable(" << y->name << ")";
        break;
    }
    case EXTERNAL_PROCEDURE : {
        ExternalProcedure *y = (ExternalProcedure *)x;
        out << "ExternalProcedure(" << y->name << ", " << y->args;
        out << ", " << y->hasVarArgs << ", " << y->returnType << ")";
        break;
    }
    case EXTERNAL_ARG: {
        ExternalArg *y = (ExternalArg *)x;
        out << "ExternalArg(" << y->name << ", " << y->type << ")";
        break;
    }

    case IMPORT : {
        Import *y = (Import *)x;
        switch (y->importKind) {
        case IMPORT_MODULE : {
            ImportModule *z = (ImportModule *)y;
            out << "Import(" << z->dottedName << ", " << z->alias << ")";
            break;
        }
        case IMPORT_STAR : {
            ImportStar *z = (ImportStar *)y;
            out << "ImportStar(" << z->dottedName << ")";
            break;
        }
        case IMPORT_MEMBERS : {
            ImportMembers *z = (ImportMembers *)y;
            out << "ImportMembers(" << z->dottedName << ", [";
            for (unsigned i = 0; i < z->members.size(); ++i) {
                if (i != 0)
                    out << ", ";
                ImportedMember &a = z->members[i];
                out << "(" << a.name << ", " << a.alias << ")";
            }
            out << "])";
            break;
        }
        default :
            assert(false);
        }
        break;
    }
    case MODULE : {
        Module *y = (Module *)x;
        out << "Module(" << bigVec(y->imports) << ", "
            << bigVec(y->topLevelItems) << ")";
        break;
    }

    case PRIM_OP : {
        PrimOp *y = (PrimOp *)x;
        out << "PrimOp(" << y->primOpCode << ")";
        break;
    }

    case TYPE : {
        Type *y = (Type *)x;
        typePrint(y, out);
        break;
    }

    default :
        out << "UnknownObj(" << x->objKind << ")";
        assert(false);
    }
}
