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
    return out;
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
    case IDENTIFIER_LITERAL : {
        const IdentifierLiteral *y = (const IdentifierLiteral *)x;
        out << "IdentifierLiteral(" << y->value << ")";
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
    case STATIC_INDEXING : {
        const StaticIndexing *y = (const StaticIndexing *)x;
        out << "StaticIndexing(" << y->expr << ", " << y->index << ")";
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
    case IF_EXPR : {
        const IfExpr *y = (const IfExpr *)x;
        out << "IfExpr(" << y->condition << ", " << y->thenPart
            << ", " << y->elsePart << ")";
        break;
    }
    case LAMBDA : {
        const Lambda *y = (const Lambda *)x;
        out << "Lambda(" << y->captureByRef << ", " << y->formalArgs
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
    case DISPATCH_EXPR : {
        const DispatchExpr *y = (const DispatchExpr *)x;
        out << "DispatchExpr(" << y->expr << ")";
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
        out << ", " << y->names << ", " << y->values << ")";
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
        out << "Return(" << y->returnKind << ", " << y->values << ")";
        break;
    }
    case IF : {
        const If *y = (const If *)x;
        out << "If(" << y->condition << ", " << y->thenPart;
        out << ", " << y->elsePart << ")";
        break;
    }
    case SWITCH : {
        const Switch *y = (const Switch *)x;
        out << "Switch(" << y->expr << ", " << y->caseBlocks
            << ", " << y->defaultCase << ")";
        break;
    }
    case CASE_BODY : {
        const CaseBody *y = (const CaseBody *)x;
        out << "CaseBody(" << y->statements << ")";
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
        out << "For(" << y->variables << ", " << y->expr;
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
        out << "Try(" << y->tryBlock << ", " << y->catchBlocks << ")";
        break;
    }
    case THROW : {
        const Throw *y = (const Throw *)x;
        out << "Throw(" << y->expr << ")";
        break;
    }
    case STATIC_FOR : {
        const StaticFor *y = (const StaticFor *)x;
        out << "StaticFor(" << y->variable << ", " << y->values
            << ", " << y->body << ")";
        break;
    }
    case UNREACHABLE : {
        out << "Unreachable()";
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

    case EXPR_LIST : {
        const ExprList *y = (const ExprList *)x;
        out << "ExprList(" << y->exprs << ")";
        break;
    }

    case STATEMENT : {
        const Statement *y = (const Statement *)x;
        printStatement(out, y);
        break;
    }

    case CASE_BLOCK : {
        const CaseBlock *y = (const CaseBlock *)x;
        out << "CaseBlock(" << y->caseLabels << ", " << y->body << ")";
        break;
    }

    case CATCH : {
        const Catch *y = (const Catch *)x;
        out << "Catch(" << y->exceptionVar << ", "
            << y->exceptionType << ", " << y->body << ")";
        break;
    }

    case CODE : {
        const Code *y = (const Code *)x;
        out << "Code(" << y->patternVars << ", " << y->predicate;
        out << ", " << y->formalArgs << ", " << y->formalVarArg;
        out << ", " << y->returnSpecs << ", " << y->varReturnSpec;
        out << ", " << y->body << ")";
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
        out << "ReturnSpec(" << y->type << ", " << y->name << ")";
        break;
    }

    case RECORD : {
        const Record *y = (const Record *)x;
        out << "Record(" << y->name << ", " << y->params;
        out << ", " << y->varParam << ", " << y->body << ")";
        break;
    }
    case RECORD_BODY : {
        const RecordBody *y = (const RecordBody *)x;
        out << "RecordBody(" << y->isComputed << ", " << y->computed;
        out << ", " << y->fields << ")";
        break;
    }
    case RECORD_FIELD : {
        const RecordField *y = (const RecordField *)x;
        out << "RecordField(" << y->name << ", " << y->type << ")";
        break;
    }

    case VARIANT : {
        const Variant *y = (const Variant *)x;
        out << "Variant(" << y->name << ", " << y->params;
        out << ", " << y->varParam << ")";
        break;
    }
    case INSTANCE : {
        const Instance *y = (const Instance *)x;
        out << "Instance(" << y->patternVars << ", " << y->predicate;
        out << ", " << y->target << ", " << y->member << ")";
        break;
    }

    case OVERLOAD : {
        const Overload *y = (const Overload *)x;
        out << "Overload(" << y->target << ", " << y->code << ", "
            << y->callByName << ", " << y->isInline << ")";
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
        out << "GlobalVariable(" << y->name << ", " << y->params
            << ", " << y->varParam << ", " << y->expr << ")";
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
        EValuePtr ev = new EValue(y->type, y->buf);
        out << "ValueHolder(";
        printValue(out, ev);
        out << ")";
        break;
    }

    case MULTI_STATIC : {
        const MultiStatic *y = (const MultiStatic *)x;
        out << "MultiStatic(" << y->values << ")";
        break;
    }

    case PVALUE : {
        const PValue *y = (const PValue *)x;
        out << "PValue(" << y->type << ", " << y->isTemp << ")";
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

static int _safeNames = 0;

void enableSafePrintName()
{
    ++_safeNames;
}

void disableSafePrintName()
{
    --_safeNames;
    assert(_safeNames >= 0);
}

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

static void printStaticOrTupleOfStatics(ostream &out, TypePtr t) {
    switch (t->typeKind) {
    case STATIC_TYPE : {
        StaticType *st = (StaticType *)t.ptr();
        printName(out, st->obj);
        break;
    }
    case TUPLE_TYPE : {
        TupleType *tt = (TupleType *)t.ptr();
        out << "(";
        for (unsigned i = 0; i < tt->elementTypes.size(); ++i) {
            if (i != 0)
                out << ", ";
            printStaticOrTupleOfStatics(out, tt->elementTypes[i]);
        }
        out << ")";
        break;
    }
    default :
        std::cout << "unexpected type: " << t << '\n';
        assert(false);
    }
}

static bool isSafe(char ch)
{
    if ((ch >= '0') && (ch <= '9'))
        return true;
    if ((ch >= 'a') && (ch <= 'z'))
        return true;
    if ((ch >= 'A') && (ch <= 'Z'))
        return true;
    if (ch == '_')
        return true;
    return false;
}

void printStaticName(ostream &out, ObjectPtr x)
{
    if (x->objKind == IDENTIFIER) {
        Identifier *y = (Identifier *)x.ptr();
        out << y->str;
    }
    else {
        printName(out, x);
    }
}

void printName(ostream &out, ObjectPtr x)
{
    switch (x->objKind) {
    case IDENTIFIER : {
        Identifier *y = (Identifier *)x.ptr();
        out << "#";
        if (_safeNames > 0) {
            for (unsigned i = 0; i < y->str.size(); ++i) {
                char ch = y->str[i];
                if (isSafe(ch))
                    out << ch;
                else
                    out << 'c' << (unsigned int)ch;
            }
        }
        else {
            out << y->str;
        }
        break;
    }
    case GLOBAL_VARIABLE : {
        GlobalVariable *y = (GlobalVariable *)x.ptr();
        out << y->name->str;
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
    case VARIANT : {
        Variant *y = (Variant *)x.ptr();
        out << y->name->str;
        break;
    }
    case PROCEDURE : {
        Procedure *y = (Procedure *)x.ptr();
        out << y->name->str;
        break;
    }
    case MODULE_HOLDER : {
        ModuleHolder *y = (ModuleHolder *)x.ptr();
        if (y->import.ptr()) {
            DottedNamePtr dname = y->import->dottedName;
            for (unsigned i = 0; i < dname->parts.size(); ++i) {
                if (i != 0)
                    out << ".";
                out << dname->parts[i]->str;
            }
        }
        else {
            out << "ModuleHolder()";
        }
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
        if (isStaticOrTupleOfStatics(y->type)) {
            printStaticOrTupleOfStatics(out, y->type);
        }
        else {
            EValuePtr ev = new EValue(y->type, y->buf);
            out << "ValueHolder(";
            printValue(out, ev);
            out << ")";
        }
        break;
    }
    default : {
        out << "UnknownNamedObject(" << x->objKind << ")";
        break;
    }
    }
}



//
// printValue
//

void printValue(ostream &out, EValuePtr ev)
{
    printName(out, ev->type.ptr());
    switch (ev->type->typeKind) {
    case BOOL_TYPE : {
        out << "(";
        char v = *((char *)ev->addr);
        out << (v ? "true" : "false");
        out << ")";
        break;
    }
    case INTEGER_TYPE : {
        IntegerType *t = (IntegerType *)ev->type.ptr();
        out << "(";
        if (t->isSigned) {
            switch (t->bits) {
            case 8 :
                out << int(*((char *)ev->addr));
                break;
            case 16 :
                out << *((short *)ev->addr);
                break;
            case 32 :
                out << *((int *)ev->addr);
                break;
            case 64 :
                out << *((long long *)ev->addr);
                break;
            default :
                assert(false);
            }
        }
        else {
            switch (t->bits) {
            case 8 :
                out << int(*((unsigned char *)ev->addr));
                break;
            case 16 :
                out << *((unsigned short *)ev->addr);
                break;
            case 32 :
                out << *((unsigned int *)ev->addr);
                break;
            case 64 :
                out << *((unsigned long long *)ev->addr);
                break;
            default :
                assert(false);
            }
        }
        out << ")";
        break;
    }
    case FLOAT_TYPE : {
        FloatType *t = (FloatType *)ev->type.ptr();
        out << "(";
        switch (t->bits) {
        case 32 :
            out << *((float *)ev->addr);
            break;
        case 64 :
            out << *((double *)ev->addr);
            break;
        default :
            assert(false);
        }
        out << ")";
    }
    default :
        break;
    }
}
