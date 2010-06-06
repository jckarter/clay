#ifndef __CLAY_HPP
#define __CLAY_HPP

#include <string>
#include <vector>
#include <map>
#include <set>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <climits>
#include <cerrno>

#include <llvm/Type.h>
#include <llvm/DerivedTypes.h>
#include <llvm/Module.h>
#include <llvm/LLVMContext.h>
#include <llvm/Function.h>
#include <llvm/BasicBlock.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/System/Host.h>
#include <llvm/Target/TargetData.h>
#include <llvm/Target/TargetSelect.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/Intrinsics.h>

using std::string;
using std::vector;
using std::pair;
using std::make_pair;
using std::map;
using std::set;
using std::ostream;
using std::ostringstream;



//
// Pointer
//

template<class T>
class Pointer {
    T *p;
public :
    Pointer()
        : p(0) {}
    Pointer(T *p)
        : p(p) {
        if (p)
            p->incRef();
    }
    Pointer(const Pointer<T> &other)
        : p(other.p) {
        if (p)
            p->incRef();
    }
    ~Pointer() {
        if (p)
            p->decRef();
    }
    Pointer<T> &operator=(const Pointer<T> &other) {
        T *q = other.p;
        if (q) q->incRef();
        if (p) p->decRef();
        p = q;
        return *this;
    }
    T &operator*() { return *p; }
    const T &operator*() const { return *p; }
    T *operator->() { return p; }
    const T *operator->() const { return p; }
    T *ptr() const { return p; }
    bool operator!() const { return p == 0; }
    bool operator==(const Pointer<T> &other) const {
        return p == other.p;
    }
    bool operator!=(const Pointer<T> &other) const {
        return p != other.p;
    }
    bool operator<(const Pointer<T> &other) const {
        return p < other.p;
    }
};



//
// Object
//

struct Object {
    int refCount;
    int objKind;
    Object(int objKind)
        : refCount(0), objKind(objKind) {}
    virtual ~Object() {}
    void incRef() { ++refCount; }
    void decRef() {
        if (--refCount == 0)
            delete this;
    }
};

typedef Pointer<Object> ObjectPtr;



//
// ObjectKind
//

enum ObjectKind {
    SOURCE,
    LOCATION,
    TOKEN,

    IDENTIFIER,
    DOTTED_NAME,

    EXPRESSION,
    STATEMENT,

    FORMAL_ARG,
    RETURN_SPEC,
    CODE,

    RECORD,
    RECORD_FIELD,
    OVERLOAD,
    PROCEDURE,
    ENUMERATION,
    ENUM_MEMBER,
    GLOBAL_VARIABLE,
    EXTERNAL_PROCEDURE,
    EXTERNAL_ARG,
    EXTERNAL_VARIABLE,

    GLOBAL_ALIAS,

    IMPORT,
    MODULE_HOLDER,
    MODULE,

    ENV,

    PRIM_OP,

    TYPE,
    PATTERN,

    VALUE_HOLDER,
    MULTI_STATIC,
    MULTI_EXPR,

    PVALUE,
    MULTI_PVALUE,

    EVALUE,
    MULTI_EVALUE,

    CVALUE,
    MULTI_CVALUE,

    DONT_CARE,
};



//
// forwards
//

struct Source;
struct Location;

struct Token;

struct ANode;
struct Identifier;
struct DottedName;

struct Expr;
struct BoolLiteral;
struct IntLiteral;
struct FloatLiteral;
struct CharLiteral;
struct StringLiteral;
struct NameRef;
struct Tuple;
struct Array;
struct Indexing;
struct Call;
struct FieldRef;
struct TupleRef;
struct UnaryOp;
struct BinaryOp;
struct And;
struct Or;
struct Lambda;
struct Unpack;
struct New;
struct StaticExpr;
struct ForeignExpr;
struct ObjectExpr;

struct Statement;
struct Block;
struct Label;
struct Binding;
struct Assignment;
struct InitAssignment;
struct UpdateAssignment;
struct Goto;
struct Return;
struct If;
struct ExprStatement;
struct While;
struct Break;
struct Continue;
struct For;
struct ForeignStatement;
struct Try;

struct FormalArg;
struct ReturnSpec;
struct Code;

struct TopLevelItem;
struct Record;
struct RecordField;
struct Overload;
struct Procedure;
struct Enumeration;
struct EnumMember;
struct GlobalVariable;
struct ExternalProcedure;
struct ExternalArg;
struct ExternalVariable;

struct GlobalAlias;

struct Import;
struct ImportModule;
struct ImportStar;
struct ImportMembers;
struct ModuleHolder;
struct Module;

struct Env;

struct PrimOp;

struct Type;
struct BoolType;
struct IntegerType;
struct FloatType;
struct ArrayType;
struct TupleType;
struct PointerType;
struct CodePointerType;
struct CCodePointerType;
struct RecordType;
struct StaticType;
struct EnumType;

struct Pattern;
struct PatternCell;
struct ArrayTypePattern;
struct TupleTypePattern;
struct PointerTypePattern;
struct CodePointerTypePattern;
struct CCodePointerTypePattern;
struct RecordTypePattern;
struct StaticTypePattern;

struct ValueHolder;
struct MultiStatic;
struct MultiExpr;

struct PValue;
struct MultiPValue;

struct EValue;
struct MultiEValue;

struct CValue;
struct MultiCValue;



//
// Pointer typedefs
//

typedef Pointer<Source> SourcePtr;
typedef Pointer<Location> LocationPtr;

typedef Pointer<Token> TokenPtr;

typedef Pointer<ANode> ANodePtr;
typedef Pointer<Identifier> IdentifierPtr;
typedef Pointer<DottedName> DottedNamePtr;

typedef Pointer<Expr> ExprPtr;
typedef Pointer<BoolLiteral> BoolLiteralPtr;
typedef Pointer<IntLiteral> IntLiteralPtr;
typedef Pointer<FloatLiteral> FloatLiteralPtr;
typedef Pointer<CharLiteral> CharLiteralPtr;
typedef Pointer<StringLiteral> StringLiteralPtr;
typedef Pointer<NameRef> NameRefPtr;
typedef Pointer<Tuple> TuplePtr;
typedef Pointer<Array> ArrayPtr;
typedef Pointer<Indexing> IndexingPtr;
typedef Pointer<Call> CallPtr;
typedef Pointer<FieldRef> FieldRefPtr;
typedef Pointer<TupleRef> TupleRefPtr;
typedef Pointer<UnaryOp> UnaryOpPtr;
typedef Pointer<BinaryOp> BinaryOpPtr;
typedef Pointer<And> AndPtr;
typedef Pointer<Or> OrPtr;
typedef Pointer<Lambda> LambdaPtr;
typedef Pointer<Unpack> UnpackPtr;
typedef Pointer<New> NewPtr;
typedef Pointer<StaticExpr> StaticExprPtr;;
typedef Pointer<ForeignExpr> ForeignExprPtr;
typedef Pointer<ObjectExpr> ObjectExprPtr;

typedef Pointer<Statement> StatementPtr;
typedef Pointer<Block> BlockPtr;
typedef Pointer<Label> LabelPtr;
typedef Pointer<Binding> BindingPtr;
typedef Pointer<Assignment> AssignmentPtr;
typedef Pointer<InitAssignment> InitAssignmentPtr;
typedef Pointer<UpdateAssignment> UpdateAssignmentPtr;
typedef Pointer<Goto> GotoPtr;
typedef Pointer<Return> ReturnPtr;
typedef Pointer<If> IfPtr;
typedef Pointer<ExprStatement> ExprStatementPtr;
typedef Pointer<While> WhilePtr;
typedef Pointer<Break> BreakPtr;
typedef Pointer<Continue> ContinuePtr;
typedef Pointer<For> ForPtr;
typedef Pointer<ForeignStatement> ForeignStatementPtr;
typedef Pointer<Try> TryPtr;

typedef Pointer<FormalArg> FormalArgPtr;
typedef Pointer<ReturnSpec> ReturnSpecPtr;
typedef Pointer<Code> CodePtr;

typedef Pointer<TopLevelItem> TopLevelItemPtr;
typedef Pointer<Record> RecordPtr;
typedef Pointer<RecordField> RecordFieldPtr;
typedef Pointer<Overload> OverloadPtr;
typedef Pointer<Procedure> ProcedurePtr;
typedef Pointer<Enumeration> EnumerationPtr;
typedef Pointer<EnumMember> EnumMemberPtr;
typedef Pointer<GlobalVariable> GlobalVariablePtr;
typedef Pointer<ExternalProcedure> ExternalProcedurePtr;
typedef Pointer<ExternalArg> ExternalArgPtr;
typedef Pointer<ExternalVariable> ExternalVariablePtr;

typedef Pointer<GlobalAlias> GlobalAliasPtr;

typedef Pointer<Import> ImportPtr;
typedef Pointer<ImportModule> ImportModulePtr;
typedef Pointer<ImportStar> ImportStarPtr;
typedef Pointer<ImportMembers> ImportMembersPtr;
typedef Pointer<ModuleHolder> ModuleHolderPtr;
typedef Pointer<Module> ModulePtr;

typedef Pointer<Env> EnvPtr;

typedef Pointer<PrimOp> PrimOpPtr;

typedef Pointer<Type> TypePtr;
typedef Pointer<BoolType> BoolTypePtr;
typedef Pointer<IntegerType> IntegerTypePtr;
typedef Pointer<FloatType> FloatTypePtr;
typedef Pointer<ArrayType> ArrayTypePtr;
typedef Pointer<TupleType> TupleTypePtr;
typedef Pointer<PointerType> PointerTypePtr;
typedef Pointer<CodePointerType> CodePointerTypePtr;
typedef Pointer<CCodePointerType> CCodePointerTypePtr;
typedef Pointer<RecordType> RecordTypePtr;
typedef Pointer<StaticType> StaticTypePtr;
typedef Pointer<EnumType> EnumTypePtr;

typedef Pointer<Pattern> PatternPtr;
typedef Pointer<PatternCell> PatternCellPtr;
typedef Pointer<ArrayTypePattern> ArrayTypePatternPtr;
typedef Pointer<TupleTypePattern> TupleTypePatternPtr;
typedef Pointer<PointerTypePattern> PointerTypePatternPtr;
typedef Pointer<CodePointerTypePattern> CodePointerTypePatternPtr;
typedef Pointer<CCodePointerTypePattern> CCodePointerTypePatternPtr;
typedef Pointer<RecordTypePattern> RecordTypePatternPtr;
typedef Pointer<StaticTypePattern> StaticTypePatternPtr;

typedef Pointer<ValueHolder> ValueHolderPtr;
typedef Pointer<MultiStatic> MultiStaticPtr;
typedef Pointer<MultiExpr> MultiExprPtr;

typedef Pointer<PValue> PValuePtr;
typedef Pointer<MultiPValue> MultiPValuePtr;

typedef Pointer<EValue> EValuePtr;
typedef Pointer<MultiEValue> MultiEValuePtr;

typedef Pointer<CValue> CValuePtr;
typedef Pointer<MultiCValue> MultiCValuePtr;



//
// Source, Location
//

struct Source : public Object {
    string fileName;
    char *data;
    int size;
    Source(const string &fileName, char *data, int size)
        : Object(SOURCE), fileName(fileName), data(data), size(size) {}
    ~Source() {
        delete [] data;
    }
};

struct Location : public Object {
    SourcePtr source;
    int offset;
    Location(const SourcePtr &source, int offset)
        : Object(LOCATION), source(source), offset(offset) {}
};



//
// error module
//

void pushInvokeStack(ObjectPtr callable, const vector<TypePtr> &argsKey);
void popInvokeStack();

struct InvokeStackContext {
    InvokeStackContext(ObjectPtr callable, const vector<TypePtr> &argsKey) {
        pushInvokeStack(callable, argsKey);
    }
    ~InvokeStackContext() {
        popInvokeStack();
    }
};

void pushLocation(LocationPtr location);
void popLocation();

struct LocationContext {
    LocationPtr loc;
    LocationContext(LocationPtr loc)
        : loc(loc) {
        if (loc.ptr()) pushLocation(loc);
    }
    ~LocationContext() {
        if (loc.ptr())
            popLocation();
    }
private :
    LocationContext(const LocationContext &) {}
    void operator=(const LocationContext &) {}
};

void setAbortOnError(bool flag);
void error(const string &msg);
void fmtError(const char *fmt, ...);

template <class T>
void error(Pointer<T> context, const string &msg)
{
    if (context->location.ptr())
        pushLocation(context->location);
    error(msg);
}

void argumentError(unsigned int index, const string &msg);

void arityError(int expected, int received);
void arityError2(int minExpected, int received);

template <class T>
void arityError(Pointer<T> context, int expected, int received)
{
    if (context->location.ptr())
        pushLocation(context->location);
    arityError(expected, received);
}

template <class T>
void arityError2(Pointer<T> context, int minExpected, int received)
{
    if (context->location.ptr())
        pushLocation(context->location);
    arityError2(minExpected, received);
}

void ensureArity(MultiStaticPtr args, unsigned int size);
void ensureArity(MultiEValuePtr args, unsigned int size);
void ensureArity(MultiPValuePtr args, unsigned int size);
void ensureArity(MultiCValuePtr args, unsigned int size);

template <class T>
void ensureArity(const vector<T> &args, int size)
{
    if ((int)args.size() != size)
        arityError(size, args.size());
}

template <class T>
void ensureArity2(const vector<T> &args, int size, bool hasVarArgs)
{
    if (!hasVarArgs) 
        ensureArity(args, size);
    else if ((int)args.size() < size)
        arityError2(size, args.size());
}

struct DebugPrinter {
    static int indent;
    ObjectPtr obj;
    DebugPrinter(ObjectPtr obj);
    ~DebugPrinter();
};



//
// Token
//

enum TokenKind {
    T_SYMBOL,
    T_KEYWORD,
    T_IDENTIFIER,
    T_STRING_LITERAL,
    T_CHAR_LITERAL,
    T_INT_LITERAL,
    T_FLOAT_LITERAL,
    T_LITERAL_SUFFIX,
    T_SPACE,
    T_LINE_COMMENT,
    T_BLOCK_COMMENT,
};

struct Token : public Object {
    LocationPtr location;
    int tokenKind;
    string str;
    Token(int tokenKind)
        : Object(TOKEN), tokenKind(tokenKind) {}
    Token(int tokenKind, const string &str)
        : Object(TOKEN), tokenKind(tokenKind), str(str) {}
};



//
// lexer module
//

void tokenize(SourcePtr source, vector<TokenPtr> &tokens);



//
// AST
//

struct ANode : public Object {
    LocationPtr location;
    ANode(int objKind)
        : Object(objKind) {}
};

struct Identifier : public ANode {
    string str;
    Identifier(const string &str)
        : ANode(IDENTIFIER), str(str) {}
};

struct DottedName : public ANode {
    vector<IdentifierPtr> parts;
    DottedName()
        : ANode(DOTTED_NAME) {}
    DottedName(const vector<IdentifierPtr> &parts)
        : ANode(DOTTED_NAME), parts(parts) {}
};




//
// Expr
//

enum ExprKind {
    BOOL_LITERAL,
    INT_LITERAL,
    FLOAT_LITERAL,
    CHAR_LITERAL,
    STRING_LITERAL,

    NAME_REF,
    TUPLE,
    ARRAY,
    INDEXING,
    CALL,
    FIELD_REF,
    TUPLE_REF,
    UNARY_OP,
    BINARY_OP,
    AND,
    OR,

    LAMBDA,
    UNPACK,
    NEW,
    STATIC_EXPR,

    FOREIGN_EXPR,
    OBJECT_EXPR,
};

struct Expr : public ANode {
    ExprKind exprKind;
    Expr(ExprKind exprKind)
        : ANode(EXPRESSION), exprKind(exprKind) {}
};

struct BoolLiteral : public Expr {
    bool value;
    BoolLiteral(bool value)
        : Expr(BOOL_LITERAL), value(value) {}
};

struct IntLiteral : public Expr {
    string value;
    string suffix;
    IntLiteral(const string &value)
        : Expr(INT_LITERAL), value(value) {}
    IntLiteral(const string &value, const string &suffix)
        : Expr(INT_LITERAL), value(value), suffix(suffix) {}
};

struct FloatLiteral : public Expr {
    string value;
    string suffix;
    FloatLiteral(const string &value)
        : Expr(FLOAT_LITERAL), value(value) {}
    FloatLiteral(const string &value, const string &suffix)
        : Expr(FLOAT_LITERAL), value(value), suffix(suffix) {}
};

struct CharLiteral : public Expr {
    char value;
    ExprPtr desugared;
    CharLiteral(char value)
        : Expr(CHAR_LITERAL), value(value) {}
};

struct StringLiteral : public Expr {
    string value;
    StringLiteral(const string &value)
        : Expr(STRING_LITERAL), value(value) {}
};

struct NameRef : public Expr {
    IdentifierPtr name;
    NameRef(IdentifierPtr name)
        : Expr(NAME_REF), name(name) {}
};

struct Tuple : public Expr {
    vector<ExprPtr> args;
    Tuple()
        : Expr(TUPLE) {}
    Tuple(const vector<ExprPtr> &args)
        : Expr(TUPLE), args(args) {}
};

struct Array : public Expr {
    vector<ExprPtr> args;
    Array()
        : Expr(ARRAY) {}
    Array(const vector<ExprPtr> &args)
        : Expr(ARRAY), args(args) {}
};

struct Indexing : public Expr {
    ExprPtr expr;
    vector<ExprPtr> args;
    Indexing(ExprPtr expr)
        : Expr(INDEXING), expr(expr) {}
    Indexing(ExprPtr expr, const vector<ExprPtr> &args)
        : Expr(INDEXING), expr(expr), args(args) {}
};

struct Call : public Expr {
    ExprPtr expr;
    vector<ExprPtr> args;
    Call(ExprPtr expr)
        : Expr(CALL), expr(expr) {}
    Call(ExprPtr expr, const vector<ExprPtr> &args)
        : Expr(CALL), expr(expr), args(args) {}
};

struct FieldRef : public Expr {
    ExprPtr expr;
    IdentifierPtr name;
    FieldRef(ExprPtr expr, IdentifierPtr name)
        : Expr(FIELD_REF), expr(expr), name(name) {}
};

struct TupleRef : public Expr {
    ExprPtr expr;
    size_t index;
    TupleRef(ExprPtr expr, size_t index)
        : Expr(TUPLE_REF), expr(expr), index(index) {}
};

enum UnaryOpKind {
    DEREFERENCE,
    ADDRESS_OF,
    PLUS,
    MINUS,
    NOT,
};

struct UnaryOp : public Expr {
    int op;
    ExprPtr expr;
    ExprPtr desugared;
    UnaryOp(int op, ExprPtr expr)
        : Expr(UNARY_OP), op(op), expr(expr) {}
};


enum BinaryOpKind {
    ADD,
    SUBTRACT,
    MULTIPLY,
    DIVIDE,
    REMAINDER,
    EQUALS,
    NOT_EQUALS,
    LESSER,
    LESSER_EQUALS,
    GREATER,
    GREATER_EQUALS,
};

struct BinaryOp : public Expr {
    int op;
    ExprPtr expr1, expr2;
    ExprPtr desugared;
    BinaryOp(int op, ExprPtr expr1, ExprPtr expr2)
        : Expr(BINARY_OP), op(op), expr1(expr1), expr2(expr2) {}
};

struct And : public Expr {
    ExprPtr expr1, expr2;
    And(ExprPtr expr1, ExprPtr expr2)
        : Expr(AND), expr1(expr1), expr2(expr2) {}
};

struct Or : public Expr {
    ExprPtr expr1, expr2;
    Or(ExprPtr expr1, ExprPtr expr2)
        : Expr(OR), expr1(expr1), expr2(expr2) {}
};

struct Lambda : public Expr {
    bool isBlockLambda;
    vector<IdentifierPtr> formalArgs;
    StatementPtr body;

    ExprPtr converted;

    bool initialized;
    RecordPtr lambdaRecord;
    TypePtr lambdaType;

    Lambda(bool isBlockLambda) :
        Expr(LAMBDA), isBlockLambda(isBlockLambda),
        initialized(false) {}
    Lambda(bool isBlockLambda,
           const vector<IdentifierPtr> &formalArgs,
           StatementPtr body)
        : Expr(LAMBDA), isBlockLambda(isBlockLambda),
          formalArgs(formalArgs), body(body),
          initialized(false) {}
};

struct Unpack : public Expr {
    ExprPtr expr;
    Unpack(ExprPtr expr) :
        Expr(UNPACK), expr(expr) {}
};

struct New : public Expr {
    ExprPtr expr;
    ExprPtr desugared;
    New(ExprPtr expr) :
        Expr(NEW), expr(expr) {}
};

struct StaticExpr : public Expr {
    ExprPtr expr;
    ExprPtr desugared;
    StaticExpr(ExprPtr expr) :
        Expr(STATIC_EXPR), expr(expr) {}
};

struct ForeignExpr : public Expr {
    string moduleName;
    EnvPtr foreignEnv;
    ExprPtr expr;

    ForeignExpr(const string &moduleName, ExprPtr expr)
        : Expr(FOREIGN_EXPR), moduleName(moduleName), expr(expr) {}
    ForeignExpr(EnvPtr foreignEnv, ExprPtr expr)
        : Expr(FOREIGN_EXPR), foreignEnv(foreignEnv), expr(expr) {}

    ForeignExpr(const string &moduleName, EnvPtr foreignEnv, ExprPtr expr)
        : Expr(FOREIGN_EXPR), moduleName(moduleName),
          foreignEnv(foreignEnv), expr(expr) {}

    EnvPtr getEnv();
};

struct ObjectExpr : public Expr {
    ObjectPtr obj;
    ObjectExpr(ObjectPtr obj)
        : Expr(OBJECT_EXPR), obj(obj) {}
};



//
// Stmt
//

enum StatementKind {
    BLOCK,
    LABEL,
    BINDING,
    ASSIGNMENT,
    INIT_ASSIGNMENT,
    UPDATE_ASSIGNMENT,
    GOTO,
    RETURN,
    IF,
    EXPR_STATEMENT,
    WHILE,
    BREAK,
    CONTINUE,
    FOR,
    FOREIGN_STATEMENT,
    TRY,
};

struct Statement : public ANode {
    StatementKind stmtKind;
    Statement(StatementKind stmtKind)
        : ANode(STATEMENT), stmtKind(stmtKind) {}
};

struct Block : public Statement {
    vector<StatementPtr> statements;
    Block()
        : Statement(BLOCK) {}
    Block(const vector<StatementPtr> &statements)
        : Statement(BLOCK), statements(statements) {}
};

struct Label : public Statement {
    IdentifierPtr name;
    Label(IdentifierPtr name)
        : Statement(LABEL), name(name) {}
};

enum BindingKind {
    VAR,
    REF,
    ALIAS
};

struct Binding : public Statement {
    int bindingKind;
    vector<IdentifierPtr> names;
    ExprPtr expr;
    Binding(int bindingKind, const vector<IdentifierPtr> &names, ExprPtr expr)
        : Statement(BINDING), bindingKind(bindingKind),
          names(names), expr(expr) {}
};

struct Assignment : public Statement {
    ExprPtr left, right;
    Assignment(ExprPtr left, ExprPtr right)
        : Statement(ASSIGNMENT), left(left), right(right) {}
};

struct InitAssignment : public Statement {
    ExprPtr left, right;
    InitAssignment(ExprPtr left, ExprPtr right)
        : Statement(INIT_ASSIGNMENT), left(left), right(right) {}
};

enum UpdateOpKind {
    UPDATE_ADD,
    UPDATE_SUBTRACT,
    UPDATE_MULTIPLY,
    UPDATE_DIVIDE,
    UPDATE_REMAINDER,
};

struct UpdateAssignment : public Statement {
    int op;
    ExprPtr left, right;
    UpdateAssignment(int op, ExprPtr left, ExprPtr right)
        : Statement(UPDATE_ASSIGNMENT), op(op), left(left),
          right(right) {}
};

struct Goto : public Statement {
    IdentifierPtr labelName;
    Goto(IdentifierPtr labelName)
        : Statement(GOTO), labelName(labelName) {}
};

enum ReturnKind {
    RETURN_VALUE,
    RETURN_REF,
    RETURN_FORWARD,
};

struct Return : public Statement {
    ReturnKind returnKind;
    vector<ExprPtr> exprs;
    Return()
        : Statement(RETURN) {}
    Return(ReturnKind returnKind, const vector<ExprPtr> &exprs)
        : Statement(RETURN), returnKind(returnKind), exprs(exprs) {}
};

struct If : public Statement {
    ExprPtr condition;
    StatementPtr thenPart, elsePart;
    If(ExprPtr condition, StatementPtr thenPart)
        : Statement(IF), condition(condition), thenPart(thenPart) {}
    If(ExprPtr condition, StatementPtr thenPart, StatementPtr elsePart)
        : Statement(IF), condition(condition), thenPart(thenPart),
          elsePart(elsePart) {}
};

struct ExprStatement : public Statement {
    ExprPtr expr;
    ExprStatement(ExprPtr expr)
        : Statement(EXPR_STATEMENT), expr(expr) {}
};

struct While : public Statement {
    ExprPtr condition;
    StatementPtr body;
    While(ExprPtr condition, StatementPtr body)
        : Statement(WHILE), condition(condition), body(body) {}
};

struct Break : public Statement {
    Break()
        : Statement(BREAK) {}
};

struct Continue : public Statement {
    Continue()
        : Statement(CONTINUE) {}
};

struct For : public Statement {
    IdentifierPtr variable;
    ExprPtr expr;
    StatementPtr body;
    StatementPtr desugared;
    For(IdentifierPtr variable, ExprPtr expr, StatementPtr body)
        : Statement(FOR), variable(variable), expr(expr), body(body) {}
};

struct ForeignStatement : public Statement {
    string moduleName;
    EnvPtr foreignEnv;
    StatementPtr statement;
    ForeignStatement(const string &moduleName, StatementPtr statement)
        : Statement(FOREIGN_STATEMENT), moduleName(moduleName),
          statement(statement) {}
    ForeignStatement(EnvPtr foreignEnv, StatementPtr statement)
        : Statement(FOREIGN_STATEMENT), foreignEnv(foreignEnv),
          statement(statement) {}
    ForeignStatement(const string &moduleName, EnvPtr foreignEnv,
                     StatementPtr statement)
        : Statement(FOREIGN_STATEMENT), moduleName(moduleName),
          foreignEnv(foreignEnv), statement(statement) {}

    EnvPtr getEnv();
};

struct Try : public Statement {
    StatementPtr tryBlock;
    StatementPtr catchBlock;
    Try(StatementPtr tryBlock, StatementPtr catchBlock) 
        : Statement(TRY), tryBlock(tryBlock), catchBlock(catchBlock) {}
};



//
// Code
//

enum ValueTempness {
    LVALUE = 1,
    RVALUE = 2,
    LVALUE_OR_RVALUE = 3,
};

struct FormalArg : public ANode {
    IdentifierPtr name;
    ExprPtr type;
    ValueTempness tempness;
    FormalArg(IdentifierPtr name, ExprPtr type)
        : ANode(FORMAL_ARG), name(name), type(type),
          tempness(LVALUE_OR_RVALUE) {}
    FormalArg(IdentifierPtr name, ExprPtr type, ValueTempness tempness)
        : ANode(FORMAL_ARG), name(name), type(type),
          tempness(tempness) {}
};

struct ReturnSpec : public ANode {
    bool byRef;
    ExprPtr type;
    IdentifierPtr name; // can be valid only if byRef==false
    ReturnSpec(bool byRef, ExprPtr type)
        : ANode(RETURN_SPEC), byRef(byRef), type(type) {}
    ReturnSpec(bool byRef, ExprPtr type, IdentifierPtr name)
        : ANode(RETURN_SPEC), byRef(byRef), type(type), name(name) {}
};

struct Code : public ANode {
    vector<IdentifierPtr> patternVars;
    ExprPtr predicate;
    vector<FormalArgPtr> formalArgs;
    IdentifierPtr formalVarArg;
    vector<ReturnSpecPtr> returnSpecs;
    StatementPtr body;

    Code()
        : ANode(CODE) {}
    Code(const vector<IdentifierPtr> &patternVars,
         ExprPtr predicate,
         const vector<FormalArgPtr> &formalArgs,
         IdentifierPtr formalVarArg,
         const vector<ReturnSpecPtr> &returnSpecs,
         StatementPtr body)
        : ANode(CODE), patternVars(patternVars), predicate(predicate),
          formalArgs(formalArgs), formalVarArg(formalVarArg),
          returnSpecs(returnSpecs), body(body) {}
};



//
// Visibility
//

enum Visibility {
    PUBLIC,
    PRIVATE,
};



//
// TopLevelItem
//

struct TopLevelItem : public ANode {
    EnvPtr env;
    IdentifierPtr name; // for named top level items
    Visibility visibility; // valid only if name != NULL
    TopLevelItem(int objKind)
        : ANode(objKind) {}
    TopLevelItem(int objKind, Visibility visibility)
        : ANode(objKind), visibility(visibility) {}
    TopLevelItem(int objKind, IdentifierPtr name, Visibility visibility)
        : ANode(objKind), name(name), visibility(visibility) {}
};

struct Record : public TopLevelItem {
    vector<IdentifierPtr> patternVars;
    vector<RecordFieldPtr> fields;

    vector<OverloadPtr> overloads;
    bool builtinOverloadInitialized;

    Record(Visibility visibility)
        : TopLevelItem(RECORD, visibility),
          builtinOverloadInitialized(false) {}
    Record(IdentifierPtr name,
           Visibility visibility,
           const vector<IdentifierPtr> &patternVars,
           const vector<RecordFieldPtr> &fields)
        : TopLevelItem(RECORD, name, visibility), patternVars(patternVars),
          fields(fields), builtinOverloadInitialized(false) {}
};

struct RecordField : public ANode {
    IdentifierPtr name;
    ExprPtr type;
    RecordField(IdentifierPtr name, ExprPtr type)
        : ANode(RECORD_FIELD), name(name), type(type) {}
};

struct Overload : public TopLevelItem {
    ExprPtr target;
    CodePtr code;
    bool inlined;
    Overload(ExprPtr target, CodePtr code, bool inlined)
        : TopLevelItem(OVERLOAD), target(target), code(code),
          inlined(inlined) {}
};

struct Procedure : public TopLevelItem {
    vector<OverloadPtr> overloads;
    Procedure(IdentifierPtr name, Visibility visibility)
        : TopLevelItem(PROCEDURE, name, visibility) {}
};

struct Enumeration : public TopLevelItem {
    vector<EnumMemberPtr> members;
    TypePtr type;
    Enumeration(IdentifierPtr name, Visibility visibility)
        : TopLevelItem(ENUMERATION, name, visibility) {}
    Enumeration(IdentifierPtr name, Visibility visibility,
                const vector<EnumMemberPtr> &members)
        : TopLevelItem(ENUMERATION, name, visibility), members(members) {}
};

struct EnumMember : public ANode {
    IdentifierPtr name;
    int index;
    TypePtr type;
    EnumMember(IdentifierPtr name)
        : ANode(ENUM_MEMBER), name(name), index(-1) {}
};

struct GlobalVariable : public TopLevelItem {
    ExprPtr expr;

    bool analyzing;
    TypePtr type;

    llvm::GlobalVariable *llGlobal;

    GlobalVariable(IdentifierPtr name, Visibility visibility, ExprPtr expr)
        : TopLevelItem(GLOBAL_VARIABLE, name, visibility), expr(expr),
          analyzing(false), llGlobal(NULL) {}
};

struct ExternalProcedure : public TopLevelItem {
    vector<ExternalArgPtr> args;
    bool hasVarArgs;
    ExprPtr returnType;
    StatementPtr body;
    vector<ExprPtr> attributes;

    bool attributesVerified;
    bool attrDLLImport;
    bool attrDLLExport;
    bool attrStdCall;
    bool attrFastCall;
    string attrAsmLabel;

    bool analyzed;
    TypePtr returnType2;
    TypePtr ptrType;

    llvm::Function *llvmFunc;

    ExternalProcedure(Visibility visibility)
        : TopLevelItem(EXTERNAL_PROCEDURE, visibility), hasVarArgs(false),
          attributesVerified(false), analyzed(false), llvmFunc(NULL) {}
    ExternalProcedure(IdentifierPtr name,
                      Visibility visibility,
                      const vector<ExternalArgPtr> &args,
                      bool hasVarArgs,
                      ExprPtr returnType,
                      StatementPtr body,
                      const vector<ExprPtr> &attributes)
        : TopLevelItem(EXTERNAL_PROCEDURE, name, visibility), args(args),
          hasVarArgs(hasVarArgs), returnType(returnType), body(body),
          attributes(attributes), attributesVerified(false),
          analyzed(false), llvmFunc(NULL) {}
};

struct ExternalArg : public ANode {
    IdentifierPtr name;
    ExprPtr type;
    TypePtr type2;
    ExternalArg(IdentifierPtr name, ExprPtr type)
        : ANode(EXTERNAL_ARG), name(name), type(type) {}
};

struct ExternalVariable : public TopLevelItem {
    ExprPtr type;
    vector<ExprPtr> attributes;

    bool attributesVerified;
    bool attrDLLImport;
    bool attrDLLExport;

    TypePtr type2;
    llvm::GlobalVariable *llGlobal;

    ExternalVariable(Visibility visibility)
        : TopLevelItem(EXTERNAL_VARIABLE, visibility),
          attributesVerified(false), llGlobal(NULL) {}
    ExternalVariable(IdentifierPtr name,
                     Visibility visibility,
                     ExprPtr type,
                     const vector<ExprPtr> &attributes)
        : TopLevelItem(EXTERNAL_VARIABLE, name, visibility),
          type(type), attributes(attributes),
          attributesVerified(false), llGlobal(NULL) {}
};



//
// GlobalAlias
//

struct GlobalAlias : public TopLevelItem {
    vector<IdentifierPtr> params;
    IdentifierPtr varParam;
    ExprPtr expr;

    GlobalAlias(IdentifierPtr name,
                Visibility visibility,
                const vector<IdentifierPtr> &params,
                IdentifierPtr varParam,
                ExprPtr expr)
        : TopLevelItem(GLOBAL_ALIAS, name, visibility),
          params(params), varParam(varParam), expr(expr) {}
    bool hasParams() const {
        return !params.empty() || (varParam.ptr() != NULL);
    }
};



//
// Imports
//

enum ImportKind {
    IMPORT_MODULE,
    IMPORT_STAR,
    IMPORT_MEMBERS,
};

struct Import : public ANode {
    int importKind;
    DottedNamePtr dottedName;
    Visibility visibility;
    ModulePtr module;
    Import(int importKind, DottedNamePtr dottedName, Visibility visibility)
        : ANode(IMPORT), importKind(importKind), dottedName(dottedName),
          visibility(visibility) {}
};

struct ImportModule : public Import {
    IdentifierPtr alias;
    ImportModule(DottedNamePtr dottedName,
                 Visibility visibility,
                 IdentifierPtr alias)
        : Import(IMPORT_MODULE, dottedName, visibility), alias(alias) {}
};

struct ImportStar : public Import {
    ImportStar(DottedNamePtr dottedName, Visibility visibility)
        : Import(IMPORT_STAR, dottedName, visibility) {}
};

struct ImportedMember {
    IdentifierPtr name;
    IdentifierPtr alias;
    ImportedMember() {}
    ImportedMember(IdentifierPtr name, IdentifierPtr alias)
        : name(name), alias(alias) {}
};

struct ImportMembers : public Import {
    vector<ImportedMember> members;
    map<string, IdentifierPtr> aliasMap;
    ImportMembers(DottedNamePtr dottedName, Visibility visibility)
        : Import(IMPORT_MEMBERS, dottedName, visibility) {}
};



//
// Module
//

struct ModuleHolder : public Object {
    ImportModulePtr import;
    map<string, ModuleHolderPtr> children;
    ModuleHolder()
        : Object(MODULE_HOLDER) {}
};

struct Module : public ANode {
    vector<ImportPtr> imports;
    vector<TopLevelItemPtr> topLevelItems;

    ModuleHolderPtr rootHolder;
    map<string, ObjectPtr> globals;

    ModuleHolderPtr publicRootHolder;
    map<string, ObjectPtr> publicGlobals;

    EnvPtr env;
    bool initialized;

    bool lookupBusy;

    Module()
        : ANode(MODULE), initialized(false), lookupBusy(false) {}
    Module(const vector<ImportPtr> &imports,
           const vector<TopLevelItemPtr> &topLevelItems)
        : ANode(MODULE), imports(imports),
          initialized(false), lookupBusy(false) {}
};



//
// parser module
//

ModulePtr parse(SourcePtr source);



//
// printer module
//

ostream &operator<<(ostream &out, const Object &obj);

ostream &operator<<(ostream &out, const Object *obj);

template <class T>
ostream &operator<<(ostream &out, const Pointer<T> &p)
{
    out << *p;
    return out;
}

template <class T>
ostream &operator<<(ostream &out, const vector<T> &v)
{
    out << "[";
    typename vector<T>::const_iterator i, end;
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

void printNameList(ostream &out, const vector<ObjectPtr> &x);
void printNameList(ostream &out, const vector<TypePtr> &x);
void printName(ostream &out, ObjectPtr x);
string getCodeName(ObjectPtr x);



//
// clone ast
//

CodePtr clone(CodePtr x);
void clone(const vector<IdentifierPtr> &x, vector<IdentifierPtr> &out);
ExprPtr clone(ExprPtr x);
ExprPtr cloneOpt(ExprPtr x);
void clone(const vector<ExprPtr> &x, vector<ExprPtr> &out);
void clone(const vector<FormalArgPtr> &x, vector<FormalArgPtr> &out);
FormalArgPtr clone(FormalArgPtr x);
void clone(const vector<ReturnSpecPtr> &x, vector<ReturnSpecPtr> &out);
ReturnSpecPtr clone(ReturnSpecPtr x);
StatementPtr clone(StatementPtr x);
StatementPtr cloneOpt(StatementPtr x);
void clone(const vector<StatementPtr> &x, vector<StatementPtr> &out);



//
// Env
//

struct Env : public Object {
    ObjectPtr parent;
    map<string, ObjectPtr> entries;
    Env()
        : Object(ENV) {}
    Env(ModulePtr parent)
        : Object(ENV), parent(parent.ptr()) {}
    Env(EnvPtr parent)
        : Object(ENV), parent(parent.ptr()) {}
};



//
// env module
//

void addGlobal(ModulePtr module,
               IdentifierPtr name,
               Visibility visibility,
               ObjectPtr value);
ObjectPtr lookupModuleHolder(ModuleHolderPtr mh, IdentifierPtr name);
ObjectPtr lookupModuleMember(ModuleHolderPtr mh, IdentifierPtr name);
ObjectPtr lookupPrivate(ModulePtr module, IdentifierPtr name);
ObjectPtr lookupPublic(ModulePtr module, IdentifierPtr name);

void addLocal(EnvPtr env, IdentifierPtr name, ObjectPtr value);
ObjectPtr lookupEnv(EnvPtr env, IdentifierPtr name);

ObjectPtr lookupEnvEx(EnvPtr env, IdentifierPtr name,
                      EnvPtr nonLocalEnv, bool &isNonLocal,
                      bool &isGlobal);



//
// loader module
//

void addSearchPath(const string &path);
ModulePtr loadProgram(const string &fileName);

BlockPtr globalVarInitializers();
BlockPtr globalVarDestructors();

ModulePtr loadedModule(const string &module);

const string &primOpName(PrimOpPtr x);

ObjectPtr kernelName(const string &name);
ObjectPtr primName(const string &name);

ExprPtr moduleNameRef(const string &module, const string &name);
ExprPtr kernelNameRef(const string &name);
ExprPtr primNameRef(const string &name);



//
// PrimOp
//

enum PrimOpCode {
    PRIM_TypeP,
    PRIM_TypeSize,

    PRIM_primitiveCopy,

    PRIM_boolNot,

    PRIM_numericEqualsP,
    PRIM_numericLesserP,
    PRIM_numericAdd,
    PRIM_numericSubtract,
    PRIM_numericMultiply,
    PRIM_numericDivide,
    PRIM_numericNegate,

    PRIM_integerRemainder,
    PRIM_integerShiftLeft,
    PRIM_integerShiftRight,
    PRIM_integerBitwiseAnd,
    PRIM_integerBitwiseOr,
    PRIM_integerBitwiseXor,
    PRIM_integerBitwiseNot,

    PRIM_numericConvert,

    PRIM_Pointer,

    PRIM_addressOf,
    PRIM_pointerDereference,
    PRIM_pointerEqualsP,
    PRIM_pointerLesserP,
    PRIM_pointerOffset,
    PRIM_pointerToInt,
    PRIM_intToPointer,

    PRIM_CodePointerP,
    PRIM_CodePointer,
    PRIM_RefCodePointer,
    PRIM_makeCodePointer,

    PRIM_CCodePointerP,
    PRIM_CCodePointer,
    PRIM_StdCallCodePointer,
    PRIM_FastCallCodePointer,
    PRIM_makeCCodePointer,

    PRIM_pointerCast,

    PRIM_Array,
    PRIM_array,
    PRIM_arrayRef,

    PRIM_TupleP,
    PRIM_Tuple,
    PRIM_TupleElementCount,
    PRIM_TupleElementOffset,
    PRIM_tuple,
    PRIM_tupleRef,

    PRIM_RecordP,
    PRIM_RecordFieldCount,
    PRIM_RecordFieldOffset,
    PRIM_RecordFieldIndex,
    PRIM_recordFieldRef,
    PRIM_recordFieldRefByName,

    PRIM_Static,
    PRIM_StaticName,

    PRIM_EnumP,
    PRIM_enumToInt,
    PRIM_intToEnum,
};

struct PrimOp : public Object {
    int primOpCode;
    PrimOp(int primOpCode)
        : Object(PRIM_OP), primOpCode(primOpCode) {}
};



//
// llvm module
//

extern llvm::Module *llvmModule;
extern llvm::ExecutionEngine *llvmEngine;
extern const llvm::TargetData *llvmTargetData;

extern llvm::Function *llvmFunction;
extern llvm::IRBuilder<> *llvmInitBuilder;
extern llvm::IRBuilder<> *llvmBuilder;

void initLLVM();

llvm::BasicBlock *newBasicBlock(const char *name);



//
// types module
//

struct Type : public Object {
    int typeKind;
    llvm::PATypeHolder *llTypeHolder;

    bool typeSizeInitialized;
    size_t typeSize;

    bool overloadsInitialized;
    vector<OverloadPtr> overloads;

    Type(int typeKind)
        : Object(TYPE), typeKind(typeKind), llTypeHolder(NULL),
          typeSizeInitialized(false), overloadsInitialized(false) {}
    ~Type() {
        if (llTypeHolder)
            delete llTypeHolder;
    }
};

enum TypeKind {
    BOOL_TYPE,
    INTEGER_TYPE,
    FLOAT_TYPE,
    POINTER_TYPE,
    CODE_POINTER_TYPE,
    CCODE_POINTER_TYPE,
    ARRAY_TYPE,
    TUPLE_TYPE,
    RECORD_TYPE,
    STATIC_TYPE,
    ENUM_TYPE,
};

struct BoolType : public Type {
    BoolType() :
        Type(BOOL_TYPE) {}
};

struct IntegerType : public Type {
    int bits;
    bool isSigned;
    IntegerType(int bits, bool isSigned)
        : Type(INTEGER_TYPE), bits(bits), isSigned(isSigned) {}
};

struct FloatType : public Type {
    int bits;
    FloatType(int bits)
        : Type(FLOAT_TYPE), bits(bits) {}
};

struct PointerType : public Type {
    TypePtr pointeeType;
    PointerType(TypePtr pointeeType)
        : Type(POINTER_TYPE), pointeeType(pointeeType) {}
};

struct CodePointerType : public Type {
    vector<TypePtr> argTypes;

    vector<bool> returnIsRef;
    vector<TypePtr> returnTypes;

    CodePointerType(const vector<TypePtr> &argTypes,
                    const vector<bool> &returnIsRef,
                    const vector<TypePtr> &returnTypes)
        : Type(CODE_POINTER_TYPE), argTypes(argTypes),
          returnIsRef(returnIsRef), returnTypes(returnTypes) {}
};

enum CallingConv {
    CC_DEFAULT,
    CC_STDCALL,
    CC_FASTCALL
};

struct CCodePointerType : public Type {
    CallingConv callingConv;
    vector<TypePtr> argTypes;
    bool hasVarArgs;
    TypePtr returnType; // NULL if void return
    CCodePointerType(CallingConv callingConv,
                     const vector<TypePtr> &argTypes,
                     bool hasVarArgs,
                     TypePtr returnType)
        : Type(CCODE_POINTER_TYPE), callingConv(callingConv),
          argTypes(argTypes), hasVarArgs(hasVarArgs),
          returnType(returnType) {}
};

struct ArrayType : public Type {
    TypePtr elementType;
    int size;
    ArrayType(TypePtr elementType, int size)
        : Type(ARRAY_TYPE), elementType(elementType), size(size) {}
};

struct TupleType : public Type {
    vector<TypePtr> elementTypes;
    const llvm::StructLayout *layout;
    TupleType()
        : Type(TUPLE_TYPE), layout(NULL) {}
    TupleType(const vector<TypePtr> &elementTypes)
        : Type(TUPLE_TYPE), elementTypes(elementTypes),
          layout(NULL) {}
};

struct RecordType : public Type {
    RecordPtr record;
    vector<ObjectPtr> params;

    bool fieldsInitialized;
    vector<TypePtr> fieldTypes;
    map<string, size_t> fieldIndexMap;

    const llvm::StructLayout *layout;

    RecordType(RecordPtr record)
        : Type(RECORD_TYPE), record(record), fieldsInitialized(false),
          layout(NULL) {}
    RecordType(RecordPtr record, const vector<ObjectPtr> &params)
        : Type(RECORD_TYPE), record(record), params(params),
          fieldsInitialized(false), layout(NULL) {}
};

struct StaticType : public Type {
    ObjectPtr obj;
    StaticType(ObjectPtr obj)
        : Type(STATIC_TYPE), obj(obj) {}
};

struct EnumType : public Type {
    EnumerationPtr enumeration;
    EnumType(EnumerationPtr enumeration)
        : Type(ENUM_TYPE), enumeration(enumeration) {}
};


extern TypePtr boolType;
extern TypePtr int8Type;
extern TypePtr int16Type;
extern TypePtr int32Type;
extern TypePtr int64Type;
extern TypePtr uint8Type;
extern TypePtr uint16Type;
extern TypePtr uint32Type;
extern TypePtr uint64Type;
extern TypePtr float32Type;
extern TypePtr float64Type;

// aliases
extern TypePtr cIntType;
extern TypePtr cSizeTType;
extern TypePtr cPtrDiffTType;

void initTypes();

TypePtr integerType(int bits, bool isSigned);
TypePtr intType(int bits);
TypePtr uintType(int bits);
TypePtr floatType(int bits);
TypePtr pointerType(TypePtr pointeeType);
TypePtr codePointerType(const vector<TypePtr> &argTypes,
                        const vector<bool> &returnIsRef,
                        const vector<TypePtr> &returnTypes);
TypePtr cCodePointerType(CallingConv callingConv,
                         const vector<TypePtr> &argTypes,
                         bool hasVarArgs,
                         TypePtr returnType);
TypePtr arrayType(TypePtr elememtType, int size);
TypePtr tupleType(const vector<TypePtr> &elementTypes);
TypePtr recordType(RecordPtr record, const vector<ObjectPtr> &params);
TypePtr staticType(ObjectPtr obj);
TypePtr enumType(EnumerationPtr enumeration);

bool isPrimitiveType(TypePtr t);

const vector<TypePtr> &recordFieldTypes(RecordTypePtr t);
const map<string, size_t> &recordFieldIndexMap(RecordTypePtr t);

const llvm::StructLayout *tupleTypeLayout(TupleType *t);
const llvm::StructLayout *recordTypeLayout(RecordType *t);

const llvm::Type *llvmIntType(int bits);
const llvm::Type *llvmFloatType(int bits);
const llvm::Type *llvmPointerType(const llvm::Type *llType);
const llvm::Type *llvmPointerType(TypePtr t);
const llvm::Type *llvmArrayType(const llvm::Type *llType, int size);
const llvm::Type *llvmArrayType(TypePtr type, int size);
const llvm::Type *llvmVoidType();

const llvm::Type *llvmType(TypePtr t);

size_t typeSize(TypePtr t);
void typePrint(ostream &out, TypePtr t);



//
// Pattern
//

enum PatternKind {
    PATTERN_CELL,
    POINTER_TYPE_PATTERN,
    CODE_POINTER_TYPE_PATTERN,
    CCODE_POINTER_TYPE_PATTERN,
    ARRAY_TYPE_PATTERN,
    TUPLE_TYPE_PATTERN,
    RECORD_TYPE_PATTERN,
    STATIC_TYPE_PATTERN,
};

struct Pattern : public Object {
    int patternKind;
    Pattern(int patternKind)
        : Object(PATTERN), patternKind(patternKind) {}
};

struct PatternCell : public Pattern {
    IdentifierPtr name;
    ObjectPtr obj;
    PatternCell(IdentifierPtr name, ObjectPtr obj)
        : Pattern(PATTERN_CELL), name(name), obj(obj) {}
};

struct PointerTypePattern : public Pattern {
    PatternPtr pointeeType;
    PointerTypePattern(PatternPtr pointeeType)
        : Pattern(POINTER_TYPE_PATTERN), pointeeType(pointeeType) {}
};

struct CodePointerTypePattern : public Pattern {
    vector<PatternPtr> argTypes;
    vector<bool> returnIsRef;
    vector<PatternPtr> returnTypes;
    CodePointerTypePattern(const vector<PatternPtr> &argTypes,
                           const vector<bool> &returnIsRef,
                           const vector<PatternPtr> &returnTypes)
        : Pattern(CODE_POINTER_TYPE_PATTERN), argTypes(argTypes),
          returnIsRef(returnIsRef), returnTypes(returnTypes) {}
};

struct CCodePointerTypePattern : public Pattern {
    CallingConv callingConv;
    vector<PatternPtr> argTypes;
    bool hasVarArgs;
    PatternPtr returnType;
    CCodePointerTypePattern(CallingConv callingConv,
                            const vector<PatternPtr> &argTypes,
                            bool hasVarArgs,
                            PatternPtr returnType)
        : Pattern(CCODE_POINTER_TYPE_PATTERN), callingConv(callingConv),
          argTypes(argTypes), hasVarArgs(hasVarArgs),
          returnType(returnType) {}
};

struct ArrayTypePattern : public Pattern {
    PatternPtr elementType;
    PatternPtr size;
    ArrayTypePattern(PatternPtr elementType, PatternPtr size)
        : Pattern(ARRAY_TYPE_PATTERN), elementType(elementType),
          size(size) {}
};

struct TupleTypePattern : public Pattern {
    vector<PatternPtr> elementTypes;
    TupleTypePattern(const vector<PatternPtr> &elementTypes)
        : Pattern(TUPLE_TYPE_PATTERN), elementTypes(elementTypes) {}
};

struct RecordTypePattern : public Pattern {
    RecordPtr record;
    vector<PatternPtr> params;
    RecordTypePattern(RecordPtr record, const vector<PatternPtr> &params)
        : Pattern(RECORD_TYPE_PATTERN), record(record), params(params) {}
};

struct StaticTypePattern : public Pattern {
    PatternPtr obj;
    StaticTypePattern(PatternPtr obj)
        : Pattern(STATIC_TYPE_PATTERN), obj(obj) {}
};



//
// ValueHolder
//

struct ValueHolder : public Object {
    TypePtr type;
    char *buf;
    ValueHolder(TypePtr type);
    ~ValueHolder();
};



//
// MultiStatic
//

struct MultiStatic : public Object {
    vector<ObjectPtr> values;
    MultiStatic()
        : Object(MULTI_STATIC) {}
    MultiStatic(ObjectPtr x)
        : Object(MULTI_STATIC) {
        values.push_back(x);
    }
    MultiStatic(const vector<ObjectPtr> &values)
        : Object(MULTI_STATIC), values(values) {}
    unsigned size() { return values.size(); }
    void add(ObjectPtr x) { values.push_back(x); }
    void add(MultiStaticPtr x) {
        values.insert(values.end(), x->values.begin(), x->values.end());
    }
};



//
// MultiExpr
//

struct MultiExpr : public Object {
    vector<ExprPtr> values;
    MultiExpr()
        : Object(MULTI_EXPR) {}
    MultiExpr(ExprPtr x)
        : Object(MULTI_EXPR) {
        values.push_back(x);
    }
    MultiExpr(const vector<ExprPtr> &values)
        : Object(MULTI_EXPR), values(values) {}
    unsigned size() { return values.size(); }
    void add(ExprPtr x) { values.push_back(x); }
    void add(MultiExprPtr x) {
        values.insert(values.end(), x->values.begin(), x->values.end());
    }
};



//
// desugar
//

ExprPtr desugarCharLiteral(char c);
ExprPtr desugarUnaryOp(UnaryOpPtr x);
ExprPtr desugarBinaryOp(BinaryOpPtr x);
ExprPtr desugarNew(NewPtr x);
ExprPtr desugarStaticExpr(StaticExprPtr x);
const char *updateOperatorName(int op);
StatementPtr desugarForStatement(ForPtr x);



//
// patterns
//

PatternPtr evaluatePattern(ExprPtr expr, EnvPtr env);
PatternPtr evaluateStaticObjectPattern(ObjectPtr x);
PatternPtr evaluateIndexingPattern(ObjectPtr indexable,
                                   const vector<ExprPtr> &args,
                                   EnvPtr env);
PatternPtr evaluateAliasIndexingPattern(GlobalAliasPtr x,
                                        const vector<ExprPtr> &args,
                                        EnvPtr env);
bool unify(PatternPtr pattern, ObjectPtr obj);
ObjectPtr derefCell(PatternCellPtr cell);
ObjectPtr reducePattern(PatternPtr pattern);

void patternPrint(ostream &out, PatternPtr x);



//
// lambdas
//

void initializeLambda(LambdaPtr x, EnvPtr env);



//
// invoke tables
//

const vector<OverloadPtr> &callableOverloads(ObjectPtr x);

struct InvokeEntry : public Object {
    ObjectPtr callable;
    vector<TypePtr> argsKey;

    bool analyzed;
    bool analyzing;

    CodePtr code;
    EnvPtr env;
    vector<TypePtr> fixedArgTypes;
    vector<IdentifierPtr> fixedArgNames;
    IdentifierPtr varArgName;
    vector<TypePtr> varArgTypes;

    bool inlined; // if inlined the rest of InvokeEntry is not set

    ObjectPtr analysis;
    vector<bool> returnIsRef;
    vector<TypePtr> returnTypes;

    llvm::Function *llvmFunc;
    llvm::Function *llvmCWrapper;

    InvokeEntry(ObjectPtr callable,
                const vector<TypePtr> &argsKey)
        : Object(DONT_CARE),
          callable(callable), argsKey(argsKey),
          analyzed(false), analyzing(false), inlined(false),
          llvmFunc(NULL), llvmCWrapper(NULL) {}
};
typedef Pointer<InvokeEntry> InvokeEntryPtr;

struct InvokeSet : public Object {
    ObjectPtr callable;
    vector<TypePtr> argsKey;
    vector<InvokeEntryPtr> entries;
    vector<unsigned> overloadIndices;
    InvokeSet(ObjectPtr callable,
              const vector<TypePtr> &argsKey)
        : Object(DONT_CARE), callable(callable), argsKey(argsKey) {}
};
typedef Pointer<InvokeSet> InvokeSetPtr;


InvokeSetPtr lookupInvokeSet(ObjectPtr callable,
                             const vector<TypePtr> &argsKey);



//
// match invoke
//

enum MatchCode {
    MATCH_SUCCESS,
    MATCH_CALLABLE_ERROR,
    MATCH_ARITY_ERROR,
    MATCH_ARGUMENT_ERROR,
    MATCH_PREDICATE_ERROR,
};

struct MatchResult : public Object {
    int matchCode;
    MatchResult(int matchCode)
        : Object(DONT_CARE), matchCode(matchCode) {}
};
typedef Pointer<MatchResult> MatchResultPtr;

struct MatchSuccess : public MatchResult {
    EnvPtr env;
    vector<TypePtr> fixedArgTypes;
    vector<IdentifierPtr> fixedArgNames;
    IdentifierPtr varArgName;
    vector<TypePtr> varArgTypes;
    MatchSuccess(EnvPtr env)
        : MatchResult(MATCH_SUCCESS), env(env) {}
};
typedef Pointer<MatchSuccess> MatchSuccessPtr;

struct MatchCallableError : public MatchResult {
    MatchCallableError()
        : MatchResult(MATCH_CALLABLE_ERROR) {}
};

struct MatchArityError : public MatchResult {
    MatchArityError()
        : MatchResult(MATCH_ARITY_ERROR) {}
};

struct MatchArgumentError : public MatchResult {
    unsigned argIndex;
    MatchArgumentError(unsigned argIndex)
        : MatchResult(MATCH_ARGUMENT_ERROR), argIndex(argIndex) {}
};

struct MatchPredicateError : public MatchResult {
    MatchPredicateError()
        : MatchResult(MATCH_PREDICATE_ERROR) {}
};

MatchResultPtr matchInvoke(CodePtr code, EnvPtr codeEnv,
                           const vector<TypePtr> &argsKey,
                           ExprPtr callableExpr, ObjectPtr callable);

void signalMatchError(MatchResultPtr result,
                      const vector<LocationPtr> &argLocations);



//
// constructors
//

extern vector<OverloadPtr> typeOverloads;

void addTypeOverload(OverloadPtr x);
void initTypeOverloads(TypePtr t);
void initBuiltinConstructor(ArrayTypePtr t);
void initBuiltinConstructor(TupleTypePtr t);
void initBuiltinConstructor(RecordTypePtr t);
void initBuiltinConstructor(RecordPtr x);



//
// literals
//

ValueHolderPtr parseIntLiteral(IntLiteral *x);
ValueHolderPtr parseFloatLiteral(FloatLiteral *x);



//
// VarArgsInfo
//

struct VarArgsInfo : public Object {
    bool hasVarArgs;
    vector<ExprPtr> varArgs;
    VarArgsInfo(bool hasVarArgs)
        : Object(DONT_CARE), hasVarArgs(hasVarArgs) {}
};

typedef Pointer<VarArgsInfo> VarArgsInfoPtr;



//
// analyzer
//

struct PValue : public Object {
    TypePtr type;
    bool isTemp;
    PValue(TypePtr type, bool isTemp)
        : Object(PVALUE), type(type), isTemp(isTemp) {}
};

struct MultiPValue : public Object {
    vector<PValuePtr> values;
    MultiPValue()
        : Object(MULTI_PVALUE) {}
    MultiPValue(PValuePtr pv)
        : Object(MULTI_PVALUE) {
        values.push_back(pv);
    }
    MultiPValue(const vector<PValuePtr> &values)
        : Object(MULTI_PVALUE), values(values) {}
    unsigned size() { return values.size(); }
    void add(PValuePtr x) { values.push_back(x); }
    void add(MultiPValuePtr x) {
        values.insert(values.end(), x->values.begin(), x->values.end());
    }
};

MultiPValuePtr analyzeMulti(const vector<ExprPtr> &exprs, EnvPtr env);
PValuePtr analyzeOne(ExprPtr expr, EnvPtr env);
MultiPValuePtr analyzeExpr(ExprPtr expr, EnvPtr env);
MultiPValuePtr analyzeStaticObject(ObjectPtr x);
PValuePtr analyzeGlobalVariable(GlobalVariablePtr x);
PValuePtr analyzeExternalVariable(ExternalVariablePtr x);
void analyzeExternalProcedure(ExternalProcedurePtr x);
void verifyAttributes(ExternalProcedurePtr x);
void verifyAttributes(ExternalVariablePtr x);
MultiPValuePtr analyzeIndexingExpr(ExprPtr indexable,
                                   const vector<ExprPtr> &args,
                                   EnvPtr env);
PValuePtr analyzeTypeConstructor(ObjectPtr obj, MultiStaticPtr args);
MultiPValuePtr analyzeAliasIndexing(GlobalAliasPtr x,
                                    const vector<ExprPtr> &args,
                                    EnvPtr env);
MultiPValuePtr analyzeFieldRefExpr(ExprPtr base,
                                   IdentifierPtr name,
                                   EnvPtr env);
void computeArgsKey(MultiPValuePtr args,
                    vector<TypePtr> &argsKey,
                    vector<ValueTempness> &argsTempness);
MultiPValuePtr analyzeReturn(const vector<bool> &returnIsRef,
                             const vector<TypePtr> &returnTypes);
MultiPValuePtr analyzeCallExpr(ExprPtr callable,
                               const vector<ExprPtr> &args,
                               EnvPtr env);
MultiPValuePtr analyzeCallValue(PValuePtr callable,
                                MultiPValuePtr args);
MultiPValuePtr analyzeCallPointer(PValuePtr x,
                                  MultiPValuePtr args);
InvokeEntryPtr analyzeCallable(ObjectPtr x,
                               const vector<TypePtr> &argsKey,
                               const vector<ValueTempness> &argsTempness);

MultiPValuePtr analyzeCallInlined(InvokeEntryPtr entry,
                                  const vector<ExprPtr> &args,
                                  EnvPtr env);

void analyzeCodeBody(InvokeEntryPtr entry);

struct AnalysisContext : public Object {
    bool returnInitialized;
    vector<bool> returnIsRef;
    vector<TypePtr> returnTypes;
    AnalysisContext()
        : Object(DONT_CARE), returnInitialized(false) {}
};
typedef Pointer<AnalysisContext> AnalysisContextPtr;

bool analyzeStatement(StatementPtr stmt, EnvPtr env, AnalysisContextPtr ctx);
EnvPtr analyzeBinding(BindingPtr x, EnvPtr env);
bool returnKindToByRef(ReturnKind returnKind, PValuePtr pv);

MultiPValuePtr analyzePrimOpExpr(PrimOpPtr x,
                                 const vector<ExprPtr> &args,
                                 EnvPtr env);
MultiPValuePtr analyzePrimOp(PrimOpPtr x, MultiPValuePtr args);



//
// evaluator
//

bool objectEquals(ObjectPtr a, ObjectPtr b);
int objectHash(ObjectPtr a);

template <typename T>
bool objectVectorEquals(const vector<Pointer<T> > &a,
                        const vector<Pointer<T> > &b) {
    if (a.size() != b.size()) return false;
    for (unsigned i = 0; i < a.size(); ++i) {
        if (!objectEquals(a[i].ptr(), b[i].ptr()))
            return false;
    }
    return true;
}

template <typename T>
int objectVectorHash(const vector<Pointer<T> > &a) {
    int h = 0;
    for (unsigned i = 0; i < a.size(); ++i)
        h += objectHash(a[i].ptr());
    return h;
}

void evaluateReturnSpecs(const vector<ReturnSpecPtr> &returnSpecs,
                         EnvPtr env,
                         vector<bool> &isRef,
                         vector<TypePtr> &types);

MultiStaticPtr evaluateExprStatic(ExprPtr expr, EnvPtr env);
ObjectPtr evaluateOneStatic(ExprPtr expr, EnvPtr env);
MultiStaticPtr evaluateMultiStatic(const vector<ExprPtr> &exprs, EnvPtr env);

TypePtr evaluateType(ExprPtr expr, EnvPtr env);
IdentifierPtr evaluateIdentifier(ExprPtr expr, EnvPtr env);
bool evaluateBool(ExprPtr expr, EnvPtr env);

ValueHolderPtr intToValueHolder(int x);
ValueHolderPtr sizeTToValueHolder(size_t x);
ValueHolderPtr ptrDiffTToValueHolder(ptrdiff_t x);
ValueHolderPtr boolToValueHolder(bool x);

struct EValue : public Object {
    TypePtr type;
    char *addr;
    EValue(TypePtr type, char *addr)
        : Object(EVALUE), type(type), addr(addr) {}
};

struct MultiEValue : public Object {
    vector<EValuePtr> values;
    MultiEValue()
        : Object(MULTI_EVALUE) {}
    MultiEValue(EValuePtr pv)
        : Object(MULTI_EVALUE) {
        values.push_back(pv);
    }
    MultiEValue(const vector<EValuePtr> &values)
        : Object(MULTI_EVALUE), values(values) {}
    unsigned size() { return values.size(); }
    void add(EValuePtr x) { values.push_back(x); }
    void add(MultiEValuePtr x) {
        values.insert(values.end(), x->values.begin(), x->values.end());
    }
};

void evalValueInit(EValuePtr dest);
void evalValueDestroy(EValuePtr dest);
void evalValueCopy(EValuePtr dest, EValuePtr src);
void evalValueAssign(EValuePtr dest, EValuePtr src);
bool evalToBoolFlag(EValuePtr a);

int evalMarkStack();
void evalDestroyStack(int marker);
void evalPopStack(int marker);
void evalDestroyAndPopStack(int marker);
EValuePtr evalAllocValue(TypePtr t);



//
// codegen
//

void setExceptionsEnabled(bool enabled);

struct CValue : public Object {
    TypePtr type;
    llvm::Value *llValue;
    CValue(TypePtr type, llvm::Value *llValue)
        : Object(CVALUE), type(type), llValue(llValue) {}
};

struct MultiCValue : public Object {
    vector<CValuePtr> values;
    MultiCValue()
        : Object(MULTI_CVALUE) {}
    MultiCValue(CValuePtr pv)
        : Object(MULTI_CVALUE) {
        values.push_back(pv);
    }
    MultiCValue(const vector<CValuePtr> &values)
        : Object(MULTI_CVALUE), values(values) {}
    unsigned size() { return values.size(); }
    void add(CValuePtr x) { values.push_back(x); }
    void add(MultiCValuePtr x) {
        values.insert(values.end(), x->values.begin(), x->values.end());
    }
};

struct JumpTarget {
    llvm::BasicBlock *block;
    int stackMarker;
    JumpTarget() : block(NULL), stackMarker(-1) {}
    JumpTarget(llvm::BasicBlock *block, int stackMarker)
        : block(block), stackMarker(stackMarker) {}
};

struct CReturn {
    bool byRef;
    TypePtr type;
    CValuePtr value;
    CReturn(bool byRef, TypePtr type, CValuePtr value)
        : byRef(byRef), type(type), value(value) {}
};

struct CodegenContext : public Object {
    vector<CReturn> returns;
    JumpTarget returnTarget;
    map<string, JumpTarget> labels;
    vector<JumpTarget> breaks;
    vector<JumpTarget> continues;
    llvm::BasicBlock *catchBlock;
	llvm::BasicBlock *unwindBlock;
    llvm::Value *exception;
	int tryBlockStackMarker;

    CodegenContext(const vector<CReturn> &returns,
                   const JumpTarget &returnTarget)
        : Object(DONT_CARE),
          returns(returns), returnTarget(returnTarget),
          catchBlock(NULL), unwindBlock(NULL), exception(NULL) {}
};

typedef Pointer<CodegenContext> CodegenContextPtr;

void codegenGlobalVariable(GlobalVariablePtr x);
void codegenExternalVariable(ExternalVariablePtr x);
void codegenExternalProcedure(ExternalProcedurePtr x);

InvokeEntryPtr codegenCallable(ObjectPtr x,
                               const vector<TypePtr> &argsKey,
                               const vector<ValueTempness> &argsTempness);
void codegenCodeBody(InvokeEntryPtr entry, const string &callableName);
void codegenCWrapper(InvokeEntryPtr entry, const string &callableName);

void codegenSharedLib(ModulePtr module);
void codegenExe(ModulePtr module);


#endif
