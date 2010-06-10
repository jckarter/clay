#include "clay.hpp"
#include <sstream>

using namespace std;



//
// overload <<
//

static void print(ostream &out, const Object *x);

ostream &operator<<(ostream &out, const Object &obj) {
    print(out, &obj);
    return out;
}

ostream &operator<<(ostream &out, const Object *obj) {
    print(out, obj);
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
// print PatternVar
//

ostream &operator<<(ostream &out, const PatternVar &pvar)
{
    out << "PatternVar(" << pvar.isMulti << ", " << pvar.name << ")";
}



//
// print
//

static void printExpr(ostream &out, const Expr *x) {
    switch (x->exprKind) {
    case BOOL_LITERAL : {
        const BoolLiteral *y = (const BoolLiteral *)x;
        out << "BoolLiteral(" << y->value << ")";
        break;
    }
    case INT_LITERAL : {
        const IntLiteral *y = (const IntLiteral *)x;
        out << "IntLiteral(" << y->value;
        if (!y->suffix.empty())
            out << ", " << y->suffix;
        out << ")";
        break;
    }
    case FLOAT_LITERAL : {
        const FloatLiteral *y = (const FloatLiteral *)x;
        out << "FloatLiteral(" << y->value;
        if (!y->suffix.empty())
            out << ", " << y->suffix;
        out << ")";
        break;
    }
    case CHAR_LITERAL : {
        const CharLiteral *y = (const CharLiteral *)x;
        out << "CharLiteral(" << y->value << ")";
        break;
    }
    case STRING_LITERAL : {
        const StringLiteral *y = (const StringLiteral *)x;
        out << "StringLiteral(" << y->value << ")";
        break;
    }

    case NAME_REF : {
        const NameRef *y = (const NameRef *)x;
        out << "NameRef(" << y->name << ")";
        break;
    }
    case TUPLE : {
        const Tuple *y = (const Tuple *)x;
        out << "Tuple(" << y->args << ")";
        break;
    }
    case ARRAY : {
        const Array *y = (const Array *)x;
        out << "Array(" << y->args << ")";
        break;
    }
    case INDEXING : {
        const Indexing *y = (const Indexing *)x;
        out << "Indexing(" << y->expr << ", " << y->args << ")";
        break;
    }
    case CALL : {
        const Call *y = (const Call *)x;
        out << "Call(" << y->expr << ", " << y->args << ")";
        break;
    }
    case FIELD_REF : {
        const FieldRef *y = (const FieldRef *)x;
        out << "FieldRef(" << y->expr << ", " << y->name << ")";
        break;
    }
    case TUPLE_REF : {
        const TupleRef *y = (const TupleRef *)x;
        out << "TupleRef(" << y->expr << ", " << y->index << ")";
        break;
    }
    case UNARY_OP : {
        const UnaryOp *y = (const UnaryOp *)x;
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
        const BinaryOp *y = (const BinaryOp *)x;
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
        const And *y = (const And *)x;
        out << "And(" << y->expr1 << ", " << y->expr2 << ")";
        break;
    }
    case OR : {
        const Or *y = (const Or *)x;
        out << "Or(" << y->expr1 << ", " << y->expr2 << ")";
        break;
    }
    case LAMBDA : {
        const Lambda *y = (const Lambda *)x;
        out << "Lambda(" << y->isBlockLambda << ", " << y->formalArgs
            << ", " << y->body << ")";
        break;
    }
    case UNPACK : {
        const Unpack *y = (const Unpack *)x;
        out << "Unpack(" << y->expr << ")";
        break;
    }
    case NEW : {
        const New *y = (const New *)x;
        out << "New(" << y->expr << ")";
        break;
    }
    case STATIC_EXPR : {
        const StaticExpr *y = (const StaticExpr *)x;
        out << "StaticExpr(" << y->expr << ")";
        break;
    }
    case FOREIGN_EXPR : {
        const ForeignExpr *y = (const ForeignExpr *)x;
        out << "ForeignExpr(" << y->expr << ")";
        break;
    }
    case OBJECT_EXPR : {
        const ObjectExpr *y = (const ObjectExpr *)x;
        out << "ObjectExpr(" << y->obj << ")";
        break;
    }
    }
}

static void printStatement(ostream &out, const Statement *x) {
    switch (x->stmtKind) {
    case BLOCK : {
        const Block *y = (const Block *)x;
        out << "Block(" << bigVec(y->statements) << ")";
        break;
    }
    case LABEL : {
        const Label *y = (const Label *)x;
        out << "Label(" << y->name << ")";
        break;
    }
    case BINDING : {
        const Binding *y = (const Binding *)x;
        out << "Binding(";
        switch (y->bindingKind) {
        case VAR :
            out << "VAR";
            break;
        case REF :
            out << "REF";
            break;
        case ALIAS :
            out << "ALIAS";
            break;
        default :
            assert(false);
        }
        out << ", " << y->names << ", " << y->expr << ")";
        break;
    }
    case ASSIGNMENT : {
        const Assignment *y = (const Assignment *)x;
        out << "Assignment(" << y->left << ", " << y->right << ")";
        break;
    }
    case INIT_ASSIGNMENT : {
        const InitAssignment *y = (const InitAssignment *)x;
        out << "InitAssignment(" << y->left << ", " << y->right << ")";
        break;
    }
    case UPDATE_ASSIGNMENT : {
        const UpdateAssignment *y = (const UpdateAssignment *)x;
        out << "UpdateAssignment(" << y->op << ", " << y->left;
        out << ", " << y->right << ")";
        break;
    }
    case GOTO : {
        const Goto *y = (const Goto *)x;
        out << "Goto(" << y->labelName << ")";
        break;
    }
    case RETURN : {
        const Return *y = (const Return *)x;
        out << "Return(" << y->returnKind << ", " << y->exprs << ")";
        break;
    }
    case IF : {
        const If *y = (const If *)x;
        out << "If(" << y->condition << ", " << y->thenPart;
        out << ", " << y->elsePart << ")";
        break;
    }
    case EXPR_STATEMENT : {
        const ExprStatement *y = (const ExprStatement *)x;
        out << "ExprStatement(" << y->expr << ")";
        break;
    }
    case WHILE : {
        const While *y = (const While *)x;
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
        const For *y = (const For *)x;
        out << "For(" << y->variable << ", " << y->expr;
        out << ", " << y->body << ")";
        break;
    }
    case FOREIGN_STATEMENT : {
        const ForeignStatement *y = (const ForeignStatement *)x;
        out << "ForeignStatement(" << y->statement << ")";
        break;
    }
    case TRY : {
        const Try *y = (const Try *)x;
        out << "Try(" << y->tryBlock << ", " << y->catchBlock;
        break;
    }
    }
}

static void print(ostream &out, const Object *x) {
    if (x == NULL) {
        out << "NULL";
        return;
    }

    switch (x->objKind) {

    case SOURCE : {
        const Source *y = (const Source *)x;
        out << "Source(" << y->fileName << ")";
        break;
    }
    case LOCATION : {
        const Location *y = (const Location *)x;
        out << "Location(" << y->source << ", " << y->offset << ")";
        break;
    }
    case TOKEN : {
        const Token *y = (const Token *)x;
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
        const Identifier *y = (const Identifier *)x;
        out << "Identifier(" << y->str << ")";
        break;
    }
    case DOTTED_NAME : {
        const DottedName *y = (const DottedName *)x;
        out << "DottedName(" << y->parts << ")";
        break;
    }

    case EXPRESSION : {
        const Expr *y = (const Expr *)x;
        printExpr(out, y);
        break;
    }

    case STATEMENT : {
        const Statement *y = (const Statement *)x;
        printStatement(out, y);
        break;
    }

    case CODE : {
        const Code *y = (const Code *)x;
        out << "Code(" << y->patternVars << ", " << y->predicate;
        out << ", " << y->formalArgs << ", " << y->formalVarArg;
        out << ", " << y->formalVarArgType;
        out << ", " << y->returnSpecs << ", " << y->body << ")";
        break;
    }
    case FORMAL_ARG : {
        const FormalArg *y = (const FormalArg *)x;
        out << "FormalArg(" << y->name << ", " << y->type << ", "
            << y->tempness << ")";
        break;
    }
    case RETURN_SPEC : {
        const ReturnSpec *y = (const ReturnSpec *)x;
        out << "ReturnSpec(" << y->byRef << ", " << y->type;
        out << ", " << y->name << ")";
        break;
    }

    case RECORD : {
        const Record *y = (const Record *)x;
        out << "Record(" << y->name << ", " << y->patternVars;
        out << ", " << y->fields << ")";
        break;
    }
    case RECORD_FIELD : {
        const RecordField *y = (const RecordField *)x;
        out << "RecordField(" << y->name << ", " << y->type << ")";
        break;
    }
    case OVERLOAD : {
        const Overload *y = (const Overload *)x;
        out << "Overload(" << y->target << ", " << y->code << ", "
            << y->inlined << ")";
        break;
    }
    case PROCEDURE : {
        const Procedure *y = (const Procedure *)x;
        out << "Procedure(" << y->name << ")";
        break;
    }

    case ENUMERATION : {
        const Enumeration *y = (const Enumeration *)x;
        out << "Enumeration(" << y->name << ", " << y->members << ")";
        break;
    }
    case ENUM_MEMBER : {
        const EnumMember *y = (const EnumMember *)x;
        out << "EnumMember(" << y->name << ")";
        break;
    }

    case GLOBAL_VARIABLE : {
        const GlobalVariable *y = (const GlobalVariable *)x;
        out << "GlobalVariable(" << y->name << ", " << y->expr << ")";
        break;
    }

    case EXTERNAL_PROCEDURE : {
        const ExternalProcedure *y = (const ExternalProcedure *)x;
        out << "ExternalProcedure(" << y->name << ", " << y->args;
        out << ", " << y->hasVarArgs << ", " << y->returnType;
        out << ", " << y->body << ")";
        break;
    }
    case EXTERNAL_ARG: {
        const ExternalArg *y = (const ExternalArg *)x;
        out << "ExternalArg(" << y->name << ", " << y->type << ")";
        break;
    }

    case EXTERNAL_VARIABLE : {
        const ExternalVariable *y = (const ExternalVariable *)x;
        out << "ExternalVariable(" << y->name << ", " << y->type << ")";
        break;
    }

    case GLOBAL_ALIAS : {
        const GlobalAlias *y = (const GlobalAlias *)x;
        out << "GlobalAlias(" << y->name << ", " << y->params
            << ", " << y->varParam << ", " << y->expr << ")";
        break;
    }

    case IMPORT : {
        const Import *y = (const Import *)x;
        switch (y->importKind) {
        case IMPORT_MODULE : {
            const ImportModule *z = (const ImportModule *)y;
            out << "Import(" << z->dottedName << ", " << z->alias << ")";
            break;
        }
        case IMPORT_STAR : {
            const ImportStar *z = (const ImportStar *)y;
            out << "ImportStar(" << z->dottedName << ")";
            break;
        }
        case IMPORT_MEMBERS : {
            const ImportMembers *z = (const ImportMembers *)y;
            out << "ImportMembers(" << z->dottedName << ", [";
            for (unsigned i = 0; i < z->members.size(); ++i) {
                if (i != 0)
                    out << ", ";
                const ImportedMember &a = z->members[i];
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
    case MODULE_HOLDER : {
        out << "ModuleHolder()";
        break;
    }
    case MODULE : {
        const Module *y = (const Module *)x;
        out << "Module(" << bigVec(y->imports) << ", "
            << bigVec(y->topLevelItems) << ")";
        break;
    }

    case PRIM_OP : {
        const PrimOp *y = (const PrimOp *)x;
        out << "PrimOp(" << primOpName(const_cast<PrimOp *>(y)) << ")";
        break;
    }

    case TYPE : {
        const Type *y = (const Type *)x;
        typePrint(out, const_cast<Type *>(y));
        break;
    }

    case PATTERN : {
        const Pattern *y = (const Pattern *)x;
        patternPrint(out, const_cast<Pattern *>(y));
        break;
    }
    case MULTI_PATTERN : {
        const MultiPattern *y = (const MultiPattern *)x;
        patternPrint(out, const_cast<MultiPattern *>(y));
        break;
    }

    case VALUE_HOLDER : {
        const ValueHolder *y = (const ValueHolder *)x;
        out << "ValueHolder(" << y->type << ")";
        break;
    }

    case MULTI_STATIC : {
        const MultiStatic *y = (const MultiStatic *)x;
        out << "MultiStatic(" << y->values << ")";
        break;
    }

    case MULTI_EXPR : {
        const MultiExpr *y = (const MultiExpr *)x;
        out << "MultiExpr(" << y->values << ")";
        break;
    }

    case PVALUE : {
        const PValue *y = (const PValue *)x;
        out << "PValue(" << y->type << ")";
        break;
    }

    case MULTI_PVALUE : {
        const MultiPValue *y = (const MultiPValue *)x;
        out << "MultiPValue(" << y->values << ")";
        break;
    }

    case EVALUE : {
        const EValue *y = (const EValue *)x;
        out << "EValue(" << y->type << ")";
        break;
    }

    case MULTI_EVALUE : {
        const MultiEValue *y = (const MultiEValue *)x;
        out << "MultiEValue(" << y->values << ")";
        break;
    }

    case CVALUE : {
        const CValue *y = (const CValue *)x;
        out << "CValue(" << y->type << ")";
        break;
    }

    case MULTI_CVALUE : {
        const MultiCValue *y = (const MultiCValue *)x;
        out << "MultiCValue(" << y->values << ")";
        break;
    }

    default :
        out << "UnknownObj(" << x->objKind << ")";
        assert(false);
    }
}



//
// printName
//

void printNameList(ostream &out, const vector<ObjectPtr> &x)
{
    for (unsigned i = 0; i < x.size(); ++i) {
        if (i != 0)
            out << ", ";
        printName(out, x[i]);
    }
}

void printNameList(ostream &out, const vector<TypePtr> &x)
{
    for (unsigned i = 0; i < x.size(); ++i) {
        if (i != 0)
            out << ", ";
        printName(out, x[i].ptr());
    }
}

void printName(ostream &out, ObjectPtr x)
{
    switch (x->objKind) {
    case IDENTIFIER : {
        Identifier *y = (Identifier *)x.ptr();
        out << "#" << y->str;
        break;
    }
    case GLOBAL_ALIAS : {
        GlobalAlias *y = (GlobalAlias *)x.ptr();
        out << y->name->str;
        break;
    }
    case RECORD : {
        Record *y = (Record *)x.ptr();
        out << y->name->str;
        break;
    }
    case PROCEDURE : {
        Procedure *y = (Procedure *)x.ptr();
        out << y->name->str;
        break;
    }
    case MODULE_HOLDER : {
        out << "ModuleHolder()";
        break;
    }
    case PRIM_OP : {
        out << primOpName((PrimOp *)x.ptr());
        break;
    }
    case TYPE : {
        typePrint(out, (Type *)x.ptr());
        break;
    }
    case VALUE_HOLDER : {
        ValueHolder *y = (ValueHolder *)x.ptr();
        out << "ValueHolder(" << y->type << ")";
        break;
    }
    default : {
        out << "UnknownNamedObject(" << x->objKind << ")";
        break;
    }
    }
}



//
// getCodeName
//

string getCodeName(ObjectPtr x)
{
    switch (x->objKind) {
    case TYPE : {
        ostringstream sout;
        sout << x;
        return sout.str();
    }
    case RECORD : {
        Record *y = (Record *)x.ptr();
        return y->name->str;
    }
    case PROCEDURE : {
        Procedure *y = (Procedure *)x.ptr();
        return y->name->str;
    }
    default :
        assert(false);
        return "";
    }
}

