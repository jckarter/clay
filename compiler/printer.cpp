#include "clay.hpp"
#include "evaluator.hpp"
#include "codegen.hpp"
#include "loader.hpp"
#include "patterns.hpp"
#include "objects.hpp"


#pragma clang diagnostic ignored "-Wcovered-switch-default"


namespace clay {

using namespace std;



void Object::print() const {
    llvm::errs() << *this << "\n";
}

std::string Object::toString() const {
    std::string r;
    llvm::raw_string_ostream os(r);
    os << *this;
    os.flush();
    return r;
}

//
// overload <<
//

static void print(llvm::raw_ostream &out, const Object *x);

llvm::raw_ostream &operator<<(llvm::raw_ostream &out, const Object &obj) {
    print(out, &obj);
    return out;
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &out, const Object *obj) {
    print(out, obj);
    return out;
}

static llvm::raw_ostream &writeFloat(llvm::raw_ostream &out, float x) {
    char buf[64];
    snprintf(buf, 63, "%.8g", x);
    buf[63] = 0;
    return out << buf;
}

static llvm::raw_ostream &writeFloat(llvm::raw_ostream &out, double x) {
    char buf[64];
    snprintf(buf, 63, "%.16g", x);
    buf[63] = 0;
    return out << buf;
}

static llvm::raw_ostream &writeFloat(llvm::raw_ostream &out, long double x) {
    char buf[64];
    snprintf(buf, 63, "%.19Lg", x);
    buf[63] = 0;
    return out << buf;
}

static llvm::raw_ostream &writeFloat(llvm::raw_ostream &out, clay_cfloat x) {
    char buf[128];
    snprintf(buf, 127, "%.8g%+.8gj", clay_creal(x), clay_cimag(x));
    buf[127] = 0;
    return out << buf;
}

static llvm::raw_ostream &writeFloat(llvm::raw_ostream &out, clay_cdouble x) {
    char buf[128];
    snprintf(buf, 127, "%.16g%+.16gj", clay_creal(x), clay_cimag(x));
    buf[127] = 0;
    return out << buf;
}

static llvm::raw_ostream &writeFloat(llvm::raw_ostream &out, clay_cldouble x) {
    char buf[128];
    snprintf(buf, 127, "%.19Lg%+.19Lgj", clay_creal(x), clay_cimag(x));
    buf[127] = 0;
    return out << buf;
}

template <class T>
llvm::raw_ostream &operator<<(llvm::raw_ostream &out, llvm::ArrayRef<T> v)
{
    out << "[";
    const T *i, *end;
    bool first = true;
    for (i = v.begin(), end = v.end(); i != end; ++i) {
        if (!first)
            out << ", ";
        first = false;
        out << *i;
    }
    out << "]";
    return out;
}

template <class T>
llvm::raw_ostream &operator<<(llvm::raw_ostream &out, const vector<T> &v)
{
    return out << llvm::makeArrayRef(v);
}

template <class T>
llvm::raw_ostream &operator<<(llvm::raw_ostream &out, const llvm::SmallVector<T, 3> &v)
{
    return out << llvm::makeArrayRef(v);
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &out, PVData const &pv)
{
    return out << "PVData(" << pv.type << ", " << pv.isTemp << ")";
}


//
// big vec
//

template <class T>
struct BigVec {
    llvm::ArrayRef<T> v;
    BigVec(llvm::ArrayRef<T> v)
        : v(v) {}
};

template <class T>
BigVec<T> bigVec(const vector<T> &v) {
    return BigVec<T>(v);
}

static int indent = 0;

template <class T>
llvm::raw_ostream &operator<<(llvm::raw_ostream &out, const BigVec<T> &v) {
    ++indent;
    out << "[";
    T const *i, *end;
    bool first = true;
    for (i = v.v.begin(), end = v.v.end(); i != end; ++i) {
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

llvm::raw_ostream &operator<<(llvm::raw_ostream &out, const PatternVar &pvar)
{
    out << "PatternVar(" << pvar.isMulti << ", " << pvar.name << ")";
    return out;
}



//
// print
//

static void printExpr(llvm::raw_ostream &out, const Expr *x) {
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
    case PAREN : {
        const Paren *y = (const Paren *)x;
        out << "Paren(" << y->args << ")";
        break;
    }
    case INDEXING : {
        const Indexing *y = (const Indexing *)x;
        out << "Indexing(" << y->expr << ", " << y->args << ")";
        break;
    }
    case CALL : {
        const Call *y = (const Call *)x;
        out << "Call(" << y->expr << ", " << y->parenArgs << ")";
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
    
    case VARIADIC_OP : {
        const VariadicOp *y = (const VariadicOp *)x;
        out << "VariadicOp(";
        switch (y->op) {
        case DEREFERENCE :
            out << "DEREFERENCE";
            break;
        case ADDRESS_OF :
            out << "ADDRESS_OF";
            break;
        case NOT :
            out << "NOT";
            break;
        case PREFIX_OP :
            out << "PREFIX_OP";
            break;
        case INFIX_OP :
            out << "INFIX_OP";
            break;
        case IF_EXPR :
            out << "IF_EXPR";
            break;
        default :
            assert(false);
        }
        out << ", "  << y->exprs << ")";
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
        out << "Lambda(" << y->captureBy << ", " << y->formalArgs
            << ", " << y->hasVarArg << ", " << y->body << ")";
        break;
    }
    case UNPACK : {
        const Unpack *y = (const Unpack *)x;
        out << "Unpack(" << y->expr << ")";
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
    case EVAL_EXPR : {
        const EvalExpr *eval = (const EvalExpr *)x;
        out << "EvalExpr(" << eval->args << ")";
        break;
    }
    case FILE_EXPR : {
        out << "FILEExpr()";
        break;
    }
    case LINE_EXPR : {
        out << "LINEExpr()";
        break;
    }
    case COLUMN_EXPR : {
        out << "COLUMNExpr()";
        break;
    }
    case ARG_EXPR : {
        const ARGExpr *arg = (const ARGExpr *)x;
        out << "ARGExpr(" << arg->name << ")";
        break;
    }
    }
}

static void printStatement(llvm::raw_ostream &out, const Statement *x) {
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
        case FORWARD :
            out << "FORWARD";
            break;
        default :
            assert(false);
        }
        out << ", " << y->args << ", " << y->values << ")";
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
    case VARIADIC_ASSIGNMENT : {
        const VariadicAssignment *y = (const VariadicAssignment *)x;
        out << "VariadicAssignment(" << y->op ;
        out << ", " << y->exprs->exprs << ")";
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
        out << "Throw(" << y->expr << ", " << y->context << ")";
        break;
    }
    case STATIC_FOR : {
        const StaticFor *y = (const StaticFor *)x;
        out << "StaticFor(" << y->variable << ", " << y->values
            << ", " << y->body << ")";
        break;
    }
    case FINALLY : {
        const Finally *y = (const Finally *)x;
        out << "Finally(" << y->body << ")";
        break;
    }
    case ONERROR : {
        const OnError *y = (const OnError *)x;
        out << "OnError(" << y->body << ")";
        break;
    }
    case UNREACHABLE : {
        out << "Unreachable()";
        break;
    }
    case EVAL_STATEMENT : {
        const EvalStatement *eval = (const EvalStatement *)x;
        out << "EvalStatement(" << eval->args << ")";
        break;
    }
    case STATIC_ASSERT_STATEMENT : {
        const StaticAssertStatement *staticAssert = (const StaticAssertStatement*)x;
        out << "StaticAssertStatement(" << staticAssert->cond;
        for (size_t i = 0; i < staticAssert->message->size(); ++i) {
            out << ", " << staticAssert->message->exprs[i];
        }
        out << ")";
        break;
    }
    }
}

static void print(llvm::raw_ostream &out, const Object *x) {
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
    case IDENTIFIER : {
        const Identifier *y = (const Identifier *)x;
        out << "Identifier(" << y->str << ")";
        break;
    }
    case DOTTED_NAME : {
        const DottedName *y = (const DottedName *)x;
        out << "DottedName(" << llvm::makeArrayRef(y->parts) << ")";
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
        out << ", " << y->formalArgs << ", " << y->hasVarArg;
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

    case RECORD_DECL : {
        const RecordDecl *y = (const RecordDecl *)x;
        out << "RecordDecl(" << y->name << ", " << y->params;
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

    case VARIANT_DECL : {
        const VariantDecl *y = (const VariantDecl *)x;
        out << "VariantDecl(" << y->name << ", " << y->params;
        out << ", " << y->varParam << ")";
        break;
    }
    case INSTANCE_DECL : {
        const InstanceDecl *y = (const InstanceDecl *)x;
        out << "InstanceDecl(" << y->patternVars << ", " << y->predicate;
        out << ", " << y->target << ", " << y->members << ")";
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
    case INTRINSIC : {
        const IntrinsicSymbol *intrin = (const IntrinsicSymbol *)x;
        out << "IntrinsicSymbol(" << intrin->name << ", " << intrin->id << ")";
        break;
    }

    case ENUM_DECL : {
        const EnumDecl *y = (const EnumDecl *)x;
        out << "EnumDecl(" << y->name << ", " << y->members << ")";
        break;
    }
    case ENUM_MEMBER : {
        const EnumMember *y = (const EnumMember *)x;
        out << "EnumMember(" << y->name << ")";
        break;
    }

    case NEW_TYPE_DECL : {
        const NewTypeDecl *y = (const NewTypeDecl *)x;
        out << "NewTypeDecl(" << y->name << ")";
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
            out << "ImportMembers(" << z->visibility << ", " << z->dottedName << ", [";
            for (size_t i = 0; i < z->members.size(); ++i) {
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

    case EVAL_TOPLEVEL : {
        const EvalTopLevel *eval = (const EvalTopLevel *)x;
        out << "EvalTopLevel(" << eval->args << ")";
        break;
    }

    case STATIC_ASSERT_TOP_LEVEL : {
        const StaticAssertTopLevel *staticAssert = (const StaticAssertTopLevel*)x;
        out << "StaticAssertTopLevel(" << staticAssert->cond;
        for (size_t i = 0; i < staticAssert->message->size(); ++i) {
            out << ", " << staticAssert->message->exprs[i];
        }
        out << ")";
        break;
    }

    case MODULE_DECLARATION : {
        const ModuleDeclaration *y = (const ModuleDeclaration *)x;
        out << "ModuleDeclaration(" << y->name << ", " << y->attributes << ")";
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
        printTypeAndValue(out, ev);
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
        out << "PValue(" << y->data << ")";
        break;
    }

    case MULTI_PVALUE : {
        const MultiPValue *y = (const MultiPValue *)x;
        out << "MultiPValue(" << llvm::makeArrayRef(y->values) << ")";
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
            
    case DOCUMENTATION : {
        const Documentation *d = (const Documentation *)x;
        out << "Documentation(" << d->text << ")";
        break;
    }

    default :
        out << "UnknownObj(" << x->objKind << ")";
        break;
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

void printNameList(llvm::raw_ostream &out, llvm::ArrayRef<ObjectPtr> x)
{
    for (size_t i = 0; i < x.size(); ++i) {
        if (i != 0)
            out << ", ";
        printName(out, x[i]);
    }
}

void printNameList(llvm::raw_ostream &out, llvm::ArrayRef<ObjectPtr> x, llvm::ArrayRef<unsigned> dispatchIndices)
{
    for (size_t i = 0; i < x.size(); ++i) {
        if (i != 0)
            out << ", ";
        if (find(dispatchIndices.begin(), dispatchIndices.end(), i) != dispatchIndices.end())
            out << "*";
        printName(out, x[i]);
    }
}

void printNameList(llvm::raw_ostream &out, llvm::ArrayRef<TypePtr> x)
{
    for (size_t i = 0; i < x.size(); ++i) {
        if (i != 0)
            out << ", ";
        printName(out, x[i].ptr());
    }
}

static void printStaticOrTupleOfStatics(llvm::raw_ostream &out, TypePtr t) {
    switch (t->typeKind) {
    case STATIC_TYPE : {
        StaticType *st = (StaticType *)t.ptr();
        printName(out, st->obj);
        break;
    }
    case TUPLE_TYPE : {
        TupleType *tt = (TupleType *)t.ptr();
        out << "[";
        for (size_t i = 0; i < tt->elementTypes.size(); ++i) {
            if (i != 0)
                out << ", ";
            printStaticOrTupleOfStatics(out, tt->elementTypes[i]);
        }
        out << "]";
        break;
    }
    default :
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

void printStaticName(llvm::raw_ostream &out, ObjectPtr x)
{
    if (x->objKind == IDENTIFIER) {
        Identifier *y = (Identifier *)x.ptr();
        out << y->str;
    }
    else {
        printName(out, x);
    }
}

void printName(llvm::raw_ostream &out, ObjectPtr x)
{
    switch (x->objKind) {
    case IDENTIFIER : {
        Identifier *y = (Identifier *)x.ptr();
        if (_safeNames > 0) {
            out << "#";
            for (unsigned i = 0; i < y->str.size(); ++i) {
                char ch = y->str[i];
                if (isSafe(ch))
                    out << ch;
                else
                    out << 'c' << (unsigned int)ch;
            }
        }
        else {
            out << "\"";
            for (unsigned i = 0; i < y->str.size(); ++i) {
                char ch = y->str[i];
                switch (ch) {
                case '\0':
                    out << "\\0";
                    break;
                case '\n':
                    out << "\\n";
                    break;
                case '\r':
                    out << "\\r";
                    break;
                case '\t':
                    out << "\\t";
                    break;
                case '\f':
                    out << "\\f";
                    break;
                case '\\':
                    out << "\\\\";
                    break;
                case '\'':
                    out << "\\'";
                    break;
                case '\"':
                    out << "\\\"";
                    break;
                default:
                    if (ch >= '\x20' && ch <= '\x7E')
                        out << ch;
                    else {
                        out << "\\x";
                        if (ch < 16)
                            out << '0';
                        out.write_hex((unsigned long long int)ch);
                    }
                    break;
                }
            }
            out << "\"";
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
    case RECORD_DECL : {
        RecordDecl *y = (RecordDecl *)x.ptr();
        out << y->name->str;
        break;
    }
    case VARIANT_DECL : {
        VariantDecl *y = (VariantDecl *)x.ptr();
        out << y->name->str;
        break;
    }
    case PROCEDURE : {
        Procedure *y = (Procedure *)x.ptr();
        out << y->name->str;
        break;
    }
    case MODULE : {
        Module *m = (Module *)x.ptr();
        out << m->moduleName;
        break;
    }
    case INTRINSIC : {
        IntrinsicSymbol *intrin = (IntrinsicSymbol *)x.ptr();
        out << intrin->name->str;
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
            printValue(out, ev);
        }
        break;
    }
    case EXTERNAL_PROCEDURE : {
        ExternalProcedure *proc = (ExternalProcedure*)x.ptr();
        out << "external " << proc->name->str;
        break;
    }
    default : {
        out << "UnknownNamedObject(" << x->objKind << ")";
        break;
    }
    }
}



//
// printTypeAndValue, printValue
//

void printTypeAndValue(llvm::raw_ostream &out, EValuePtr ev)
{
    printName(out, ev->type.ptr());
    out << "(";
    printValue(out, ev);
    out << ")";
}

void printValue(llvm::raw_ostream &out, EValuePtr ev)
{
    switch (ev->type->typeKind) {
    case BOOL_TYPE : {
        bool v = ev->as<bool>();
        out << (v ? "true" : "false");
        break;
    }
    case INTEGER_TYPE : {
        IntegerType *t = (IntegerType *)ev->type.ptr();
        if (t->isSigned) {
            switch (t->bits) {
            case 8 :
                out << int(ev->as<signed char>());
                break;
            case 16 :
                out << ev->as<short>();
                break;
            case 32 :
                out << ev->as<int>();
                break;
            case 64 :
                out << ev->as<long long>();
                break;
            default :
                assert(false);
            }
        }
        else {
            switch (t->bits) {
            case 8 :
                out << int(ev->as<unsigned char>());
                break;
            case 16 :
                out << ev->as<unsigned short>();
                break;
            case 32 :
                out << ev->as<unsigned int>();
                break;
            case 64 :
                out << ev->as<unsigned long long>();
                break;
            default :
                assert(false);
            }
        }
        break;
    }
    case FLOAT_TYPE : {
        FloatType *t = (FloatType *)ev->type.ptr();
        switch (t->bits) {
        case 32 :
            writeFloat(out, ev->as<float>());
            break;
        case 64 :
            writeFloat(out, ev->as<double>());
            break;
        case 80 :
            writeFloat(out, ev->as<long double>());
            break;
        default :
            assert(false);
        }
        break;
    }
    case COMPLEX_TYPE : {
        ComplexType *t = (ComplexType *)ev->type.ptr();
        switch (t->bits) {
        case 32 :
            writeFloat(out, ev->as<clay_cfloat>());
            break;
        case 64 :
            writeFloat(out, ev->as<clay_cdouble>());
            break;
        case 80 :
            writeFloat(out, ev->as<clay_cldouble>());
            break;
        default :
            assert(false);
        }
        break;
    }
    case ENUM_TYPE : {
        EnumType *t = (EnumType *)ev->type.ptr();
        llvm::ArrayRef<EnumMemberPtr> members = t->enumeration->members;
        int member = ev->as<int>();

        if (member >= 0 && size_t(member) < members.size()) {
            out << members[(size_t)member]->name->str;
        } else {
            printName(out, t);
            out << '(' << member << ')';
        }
        break;
    }
    default :
        break;
    }
}

string shortString(llvm::StringRef in) {
    string out;
    bool wasSpace = false;

    for (const char *i = in.begin(); i != in.end(); ++i) {
        if (isspace(*i)) {
            if (!wasSpace) {
                out.push_back(' ');
                wasSpace = true;
            }
        } else {
            out.push_back(*i);
            wasSpace = false;
        }
    }
    return out;
}



//
// patternPrint
//

void patternPrint(llvm::raw_ostream &out, PatternPtr x)
{
    switch (x->kind) {
    case PATTERN_CELL : {
        PatternCell *y = (PatternCell *)x.ptr();
        out << "PatternCell(" << y->obj << ")";
        break;
    }
    case PATTERN_STRUCT : {
        PatternStruct *y = (PatternStruct *)x.ptr();
        out << "PatternStruct(" << y->head << ", " << y->params << ")";
        break;
    }
    default :
        assert(false);
    }
}

void patternPrint(llvm::raw_ostream &out, MultiPatternPtr x)
{
    switch (x->kind) {
    case MULTI_PATTERN_CELL : {
        MultiPatternCell *y = (MultiPatternCell *)x.ptr();
        out << "MultiPatternCell(" << y->data << ")";
        break;
    }
    case MULTI_PATTERN_LIST : {
        MultiPatternList *y = (MultiPatternList *)x.ptr();
        out << "MultiPatternList(" << y->items << ", " << y->tail << ")";
        break;
    }
    default :
        assert(false);
    }
}

// typePrint

void typePrint(llvm::raw_ostream &out, TypePtr t) {
    switch (t->typeKind) {
    case BOOL_TYPE :
        out << "Bool";
        break;
    case INTEGER_TYPE : {
        IntegerType *x = (IntegerType *)t.ptr();
        if (!x->isSigned)
            out << "U";
        out << "Int" << x->bits;
        break;
    }
    case FLOAT_TYPE : {
        FloatType *x = (FloatType *)t.ptr();
        if(x->isImaginary) {
            out << "Imag" << x->bits;
        } else {
            out << "Float" << x->bits;
        }
        break;
    }
    case COMPLEX_TYPE : {
        ComplexType *x = (ComplexType *)t.ptr();
        out << "Complex" << x->bits;
        break;
    }
    case POINTER_TYPE : {
        PointerType *x = (PointerType *)t.ptr();
        out << "Pointer[" << x->pointeeType << "]";
        break;
    }
    case CODE_POINTER_TYPE : {
        CodePointerType *x = (CodePointerType *)t.ptr();
        out << "CodePointer[[";
        for (size_t i = 0; i < x->argTypes.size(); ++i) {
            if (i != 0)
                out << ", ";
            out << x->argTypes[i];
        }
        out << "], [";
        for (size_t i = 0; i < x->returnTypes.size(); ++i) {
            if (i != 0)
                out << ", ";
            if (x->returnIsRef[i])
                out << "ByRef[" << x->returnTypes[i] << "]";
            else
                out << x->returnTypes[i];
        }
        out << "]]";
        break;
    }
    case CCODE_POINTER_TYPE : {
        CCodePointerType *x = (CCodePointerType *)t.ptr();
        out << "ExternalCodePointer[";

        switch (x->callingConv) {
        case CC_DEFAULT :
            out << "AttributeCCall, ";
            break;
        case CC_STDCALL :
            out << "AttributeStdCall, ";
            break;
        case CC_FASTCALL :
            out << "AttributeFastCall, ";
            break;
        case CC_THISCALL :
            out << "AttributeThisCall, ";
            break;
        case CC_LLVM :
            out << "AttributeLLVMCall, ";
            break;
        default :
            assert(false);
        }
        if (x->hasVarArgs)
            out << "true, ";
        else
            out << "false, ";
        out << "[";
        for (size_t i = 0; i < x->argTypes.size(); ++i) {
            if (i != 0)
                out << ", ";
            out << x->argTypes[i];
        }
        out << "], [";
        if (x->returnType.ptr())
            out << x->returnType;
        out << "]]";
        break;
    }
    case ARRAY_TYPE : {
        ArrayType *x = (ArrayType *)t.ptr();
        out << "Array[" << x->elementType << ", " << x->size << "]";
        break;
    }
    case VEC_TYPE : {
        VecType *x = (VecType *)t.ptr();
        out << "Vec[" << x->elementType << ", " << x->size << "]";
        break;
    }
    case TUPLE_TYPE : {
        TupleType *x = (TupleType *)t.ptr();
        out << "Tuple" << x->elementTypes;
        break;
    }
    case UNION_TYPE : {
        UnionType *x = (UnionType *)t.ptr();
        out << "Union" << x->memberTypes;
        break;
    }
    case RECORD_TYPE : {
        RecordType *x = (RecordType *)t.ptr();
        out << x->record->name->str;
        if (!x->params.empty()) {
            out << "[";
            printNameList(out, x->params);
            out << "]";
        }
        break;
    }
    case VARIANT_TYPE : {
        VariantType *x = (VariantType *)t.ptr();
        out << x->variant->name->str;
        if (!x->params.empty()) {
            out << "[";
            printNameList(out, x->params);
            out << "]";
        }
        break;
    }
    case STATIC_TYPE : {
        StaticType *x = (StaticType *)t.ptr();
        out << "Static[";
        printName(out, x->obj);
        out << "]";
        break;
    }
    case ENUM_TYPE : {
        EnumType *x = (EnumType *)t.ptr();
        out << x->enumeration->name->str;
        break;
    }
    case NEW_TYPE : {
        NewType *x = (NewType *)t.ptr();
        out << x->newtype->name->str;
        break;
    }
    default :
        assert(false);
    }
}

}


std::string clay::DottedName::join() const {
    std::string s;
    llvm::raw_string_ostream ss(s);
    for (llvm::SmallVector<clay::IdentifierPtr, 2>::const_iterator part = parts.begin();
            part != parts.end(); ++part)
    {
        if (part != parts.begin())
            ss << ".";
        ss << (*part)->str;
    }
    ss.flush();
    return s;
}
