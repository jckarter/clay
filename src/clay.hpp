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
#include <llvm/ModuleProvider.h>
#include <llvm/Function.h>
#include <llvm/BasicBlock.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Target/TargetData.h>
#include <llvm/Target/TargetSelect.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/JIT.h>

using std::string;
using std::vector;
using std::map;
using std::set;
using std::ostream;
using std::ostringstream;



//
// Ptr
//

template<class T>
class Ptr {
    T *ptr;
public :
    Ptr()
        : ptr(0) {}
    Ptr(T *ptr)
        : ptr(ptr) {
        if (ptr)
            ptr->incRef();
    }
    Ptr(const Ptr<T> &other)
        : ptr(other.ptr) {
        if (ptr)
            ptr->incRef();
    }
    ~Ptr() {
        if (ptr)
            ptr->decRef();
    }
    Ptr<T> &operator=(const Ptr<T> &other) {
        T *p = other.ptr;
        if (p) p->incRef();
        if (ptr) ptr->decRef();
        ptr = p;
        return *this;
    }
    T &operator*() { return *ptr; }
    const T &operator*() const { return *ptr; }
    T *operator->() { return ptr; }
    const T *operator->() const { return ptr; }
    T *raw() const { return ptr; }
    bool operator!() const { return ptr == 0; }
    bool operator==(const Ptr<T> &other) const {
        return ptr == other.ptr;
    }
    bool operator!=(const Ptr<T> &other) const {
        return ptr != other.ptr;
    }
};



//
// Object
//

struct Object {
    int refCount;
    int objKind;
    int coIndex;
    Object(int objKind)
        : refCount(0), objKind(objKind), coIndex(-1) {}
    virtual ~Object() {}
    void incRef() { ++refCount; }
    void decRef() {
        if (--refCount == 0)
            delete this;
    }
};

typedef Ptr<Object> ObjectPtr;



//
// ObjectKind
//

enum ObjectKind {
    SOURCE,
    LOCATION,
    TOKEN,

    IDENTIFIER,
    DOTTED_NAME,

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

    SC_EXPR,
    VALUE_EXPR,

    BLOCK,
    LABEL,
    BINDING,
    ASSIGNMENT,
    GOTO,
    RETURN,
    RETURN_REF,
    IF,
    EXPR_STATEMENT,
    WHILE,
    BREAK,
    CONTINUE,
    FOR,

    CODE,
    VALUE_ARG,
    STATIC_ARG,

    RECORD,
    PROCEDURE,
    OVERLOAD,
    OVERLOADABLE,
    EXTERNAL_PROCEDURE,
    EXTERNAL_ARG,

    IMPORT,
    EXPORT,
    MODULE,

    ENV,

    PRIM_OP,

    TYPE,
    VALUE,
    PATTERN,

    ANALYSIS,

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
struct SCExpr;
struct ValueExpr;

struct Statement;
struct Block;
struct Label;
struct Binding;
struct Assignment;
struct Goto;
struct Return;
struct ReturnRef;
struct If;
struct ExprStatement;
struct While;
struct Break;
struct Continue;
struct For;

struct FormalArg;
struct ValueArg;
struct StaticArg;
struct Code;

struct TopLevelItem;
struct Record;
struct Procedure;
struct Overload;
struct Overloadable;
struct ExternalProcedure;
struct ExternalArg;

struct Import;
struct Export;
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
struct RecordType;

struct Value;

struct Pattern;
struct PatternCell;
struct ArrayTypePattern;
struct TupleTypePattern;
struct PointerTypePattern;
struct RecordTypePattern;

struct InvokeTable;
struct InvokeTableEntry;

struct MatchInvokeResult;
struct MatchInvokeSuccess;
struct MatchInvokeArgCountError;
struct MatchInvokeArgMismatch;
struct MatchInvokePredicateFailure;

struct Analysis;
struct ReturnInfo;



//
// Ptr typedefs
//

typedef Ptr<Source> SourcePtr;
typedef Ptr<Location> LocationPtr;

typedef Ptr<Token> TokenPtr;

typedef Ptr<ANode> ANodePtr;
typedef Ptr<Identifier> IdentifierPtr;
typedef Ptr<DottedName> DottedNamePtr;

typedef Ptr<Expr> ExprPtr;
typedef Ptr<BoolLiteral> BoolLiteralPtr;
typedef Ptr<IntLiteral> IntLiteralPtr;
typedef Ptr<FloatLiteral> FloatLiteralPtr;
typedef Ptr<CharLiteral> CharLiteralPtr;
typedef Ptr<StringLiteral> StringLiteralPtr;
typedef Ptr<NameRef> NameRefPtr;
typedef Ptr<Tuple> TuplePtr;
typedef Ptr<Array> ArrayPtr;
typedef Ptr<Indexing> IndexingPtr;
typedef Ptr<Call> CallPtr;
typedef Ptr<FieldRef> FieldRefPtr;
typedef Ptr<TupleRef> TupleRefPtr;
typedef Ptr<UnaryOp> UnaryOpPtr;
typedef Ptr<BinaryOp> BinaryOpPtr;
typedef Ptr<And> AndPtr;
typedef Ptr<Or> OrPtr;
typedef Ptr<SCExpr> SCExprPtr;
typedef Ptr<ValueExpr> ValueExprPtr;

typedef Ptr<Statement> StatementPtr;
typedef Ptr<Block> BlockPtr;
typedef Ptr<Label> LabelPtr;
typedef Ptr<Binding> BindingPtr;
typedef Ptr<Assignment> AssignmentPtr;
typedef Ptr<Goto> GotoPtr;
typedef Ptr<Return> ReturnPtr;
typedef Ptr<ReturnRef> ReturnRefPtr;
typedef Ptr<If> IfPtr;
typedef Ptr<ExprStatement> ExprStatementPtr;
typedef Ptr<While> WhilePtr;
typedef Ptr<Break> BreakPtr;
typedef Ptr<Continue> ContinuePtr;
typedef Ptr<For> ForPtr;

typedef Ptr<FormalArg> FormalArgPtr;
typedef Ptr<ValueArg> ValueArgPtr;
typedef Ptr<StaticArg> StaticArgPtr;
typedef Ptr<Code> CodePtr;

typedef Ptr<TopLevelItem> TopLevelItemPtr;
typedef Ptr<Record> RecordPtr;
typedef Ptr<Procedure> ProcedurePtr;
typedef Ptr<Overload> OverloadPtr;
typedef Ptr<Overloadable> OverloadablePtr;
typedef Ptr<ExternalProcedure> ExternalProcedurePtr;
typedef Ptr<ExternalArg> ExternalArgPtr;

typedef Ptr<Import> ImportPtr;
typedef Ptr<Export> ExportPtr;
typedef Ptr<Module> ModulePtr;

typedef Ptr<Env> EnvPtr;

typedef Ptr<PrimOp> PrimOpPtr;

typedef Ptr<Type> TypePtr;
typedef Ptr<BoolType> BoolTypePtr;
typedef Ptr<IntegerType> IntegerTypePtr;
typedef Ptr<FloatType> FloatTypePtr;
typedef Ptr<ArrayType> ArrayTypePtr;
typedef Ptr<TupleType> TupleTypePtr;
typedef Ptr<PointerType> PointerTypePtr;
typedef Ptr<RecordType> RecordTypePtr;

typedef Ptr<Value> ValuePtr;

typedef Ptr<Pattern> PatternPtr;
typedef Ptr<PatternCell> PatternCellPtr;
typedef Ptr<ArrayTypePattern> ArrayTypePatternPtr;
typedef Ptr<TupleTypePattern> TupleTypePatternPtr;
typedef Ptr<PointerTypePattern> PointerTypePatternPtr;
typedef Ptr<RecordTypePattern> RecordTypePatternPtr;

typedef Ptr<InvokeTable> InvokeTablePtr;
typedef Ptr<InvokeTableEntry> InvokeTableEntryPtr;

typedef Ptr<MatchInvokeResult> MatchInvokeResultPtr;
typedef Ptr<MatchInvokeSuccess> MatchInvokeSuccessPtr;
typedef Ptr<MatchInvokeArgCountError> MatchInvokeArgCountErrorPtr;
typedef Ptr<MatchInvokeArgMismatch> MatchInvokeArgMismatchPtr;
typedef Ptr<MatchInvokePredicateFailure> MatchInvokePredicateFailurePtr;

typedef Ptr<Analysis> AnalysisPtr;
typedef Ptr<ReturnInfo> ReturnInfoPtr;



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

void pushLocation(LocationPtr location);
void popLocation();

struct LocationContext {
    LocationPtr loc;
    LocationContext(LocationPtr loc)
        : loc(loc) {
        if (loc.raw()) pushLocation(loc);
    }
    ~LocationContext() {
        if (loc.raw())
            popLocation();
    }
private :
    LocationContext(const LocationContext &) {}
    void operator=(const LocationContext &) {}
};

void error(const string &msg);

void fmtError(const char *fmt, ...);

template <class T>
void error(Ptr<T> context, const string &msg) {
    if (context->location.raw())
        pushLocation(context->location);
    error(msg);
}

template <class T>
void ensureArity(const vector<T> &args, int size) {
    if ((int)args.size() != size)
        error("incorrect number of arguments");
}

void ensurePrimitiveType(TypePtr t);
void ensureSameType(TypePtr ta, TypePtr tb);
void ensureNumericType(TypePtr t);
void ensureIntegerType(TypePtr t);
void ensurePointerType(TypePtr t);
void ensureArrayType(TypePtr t);
void ensureTupleType(TypePtr t);
void ensureRecordType(TypePtr t);

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

struct Expr : public ANode {
    Expr(int objKind)
        : ANode(objKind) {}
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
    ExprPtr converted;
    CharLiteral(char value)
        : Expr(CHAR_LITERAL), value(value) {}
};

struct StringLiteral : public Expr {
    string value;
    ExprPtr converted;
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
    ExprPtr converted;
    Tuple()
        : Expr(TUPLE) {}
    Tuple(const vector<ExprPtr> &args)
        : Expr(TUPLE), args(args) {}
};

struct Array : public Expr {
    vector<ExprPtr> args;
    ExprPtr converted;
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
    int index;
    TupleRef(ExprPtr expr, int index)
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
    ExprPtr converted;
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
    ExprPtr converted;
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

struct SCExpr : public Expr {
    EnvPtr env;
    ExprPtr expr;
    SCExpr(EnvPtr env, ExprPtr expr)
        : Expr(SC_EXPR), env(env), expr(expr) {}
};

struct ValueExpr : public Expr {
    ValuePtr value;
    ValueExpr(ValuePtr value)
        : Expr(VALUE_EXPR), value(value) {}
};



//
// Stmt
//

struct Statement : public ANode {
    Statement(int objKind)
        : ANode(objKind) {}
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
    STATIC
};

struct Binding : public Statement {
    int bindingKind;
    IdentifierPtr name;
    ExprPtr expr;
    Binding(int bindingKind, IdentifierPtr name, ExprPtr expr)
        : Statement(BINDING), bindingKind(bindingKind),
          name(name), expr(expr) {}
};

struct Assignment : public Statement {
    ExprPtr left, right;
    Assignment(ExprPtr left, ExprPtr right)
        : Statement(ASSIGNMENT), left(left), right(right) {}
};

struct Goto : public Statement {
    IdentifierPtr labelName;
    Goto(IdentifierPtr labelName)
        : Statement(GOTO), labelName(labelName) {}
};

struct Return : public Statement {
    ExprPtr expr;
    Return()
        : Statement(RETURN) {}
    Return(ExprPtr expr)
        : Statement(RETURN), expr(expr) {}
};

struct ReturnRef : public Statement {
    ExprPtr expr;
    ReturnRef()
        : Statement(RETURN_REF) {}
    ReturnRef(ExprPtr expr)
        : Statement(RETURN_REF), expr(expr) {}
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
    StatementPtr converted;
    For(IdentifierPtr variable, ExprPtr expr, StatementPtr body)
        : Statement(FOR), variable(variable), expr(expr), body(body) {}
};



//
// Code
//

struct FormalArg : public ANode {
    FormalArg(int objKind)
        : ANode(objKind) {}
};

struct ValueArg : public FormalArg {
    IdentifierPtr name;
    ExprPtr type;
    ValueArg(IdentifierPtr name, ExprPtr type)
        : FormalArg(VALUE_ARG), name(name), type(type) {}
};

struct StaticArg : public FormalArg {
    ExprPtr pattern;
    StaticArg(ExprPtr pattern)
        : FormalArg(STATIC_ARG), pattern(pattern) {}
};

struct Code : public ANode {
    vector<IdentifierPtr> patternVars;
    ExprPtr predicate;
    vector<FormalArgPtr> formalArgs;
    StatementPtr body;
    Code()
        : ANode(CODE) {}
    Code(const vector<IdentifierPtr> &patternVars, ExprPtr predicate,
         const vector<FormalArgPtr> &formalArgs, StatementPtr body)
        : ANode(CODE), patternVars(patternVars), predicate(predicate),
          formalArgs(formalArgs), body(body) {}
};



//
// TopLevelItem
//

struct TopLevelItem : public ANode {
    EnvPtr env;
    TopLevelItem(int objKind)
        : ANode(objKind) {}
};

struct Record : public TopLevelItem {
    IdentifierPtr name;
    vector<IdentifierPtr> patternVars;
    vector<FormalArgPtr> formalArgs;
    Record()
        : TopLevelItem(RECORD) {}
    Record(IdentifierPtr name, const vector<IdentifierPtr> &patternVars,
           const vector<FormalArgPtr> &formalArgs)
        : TopLevelItem(RECORD), name(name), patternVars(patternVars),
          formalArgs(formalArgs) {}
};

struct Procedure : public TopLevelItem {
    IdentifierPtr name;
    CodePtr code;
    InvokeTablePtr invokeTable;
    Procedure(IdentifierPtr name, CodePtr code)
        : TopLevelItem(PROCEDURE), name(name), code(code) {}
};

struct Overload : public TopLevelItem {
    IdentifierPtr name;
    CodePtr code;
    Overload(IdentifierPtr name, CodePtr code)
        : TopLevelItem(OVERLOAD), name(name), code(code) {}
};

struct Overloadable : public TopLevelItem {
    IdentifierPtr name;
    vector<OverloadPtr> overloads;
    vector<InvokeTablePtr> invokeTables;
    Overloadable(IdentifierPtr name)
        : TopLevelItem(OVERLOADABLE), name(name) {}
};

struct ExternalProcedure : public TopLevelItem {
    IdentifierPtr name;
    vector<ExternalArgPtr> args;
    ExprPtr returnType;
    TypePtr returnType2;
    llvm::Function *llvmFunc;
    ExternalProcedure()
        : TopLevelItem(EXTERNAL_PROCEDURE), llvmFunc(NULL) {}
    ExternalProcedure(IdentifierPtr name, const vector<ExternalArgPtr> &args,
                      ExprPtr returnType)
        : TopLevelItem(EXTERNAL_PROCEDURE), name(name), args(args),
          returnType(returnType), llvmFunc(NULL) {}
};

struct ExternalArg : public ANode {
    IdentifierPtr name;
    ExprPtr type;
    TypePtr type2;
    ExternalArg(IdentifierPtr name, ExprPtr type)
        : ANode(EXTERNAL_ARG), name(name), type(type) {}
};



//
// Import, Export
//

struct Import : public ANode {
    DottedNamePtr dottedName;
    ModulePtr module;
    Import(DottedNamePtr dottedName)
        : ANode(IMPORT), dottedName(dottedName) {}
};

struct Export : public ANode {
    DottedNamePtr dottedName;
    ModulePtr module;
    Export(DottedNamePtr dottedName)
        : ANode(EXPORT), dottedName(dottedName) {}
};



//
// Module
//

struct Module : public ANode {
    vector<ImportPtr> imports;
    vector<ExportPtr> exports;
    vector<TopLevelItemPtr> topLevelItems;

    map<string, ObjectPtr> globals;
    EnvPtr env;
    bool initialized;

    bool lookupBusy;

    Module()
        : ANode(MODULE), initialized(false), lookupBusy(false) {}
    Module(const vector<ImportPtr> &imports, const vector<ExportPtr> &exports,
           const vector<TopLevelItemPtr> &topLevelItems)
        : ANode(MODULE), imports(imports), exports(exports),
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
ostream &operator<<(ostream &out, const Ptr<T> &p) {
    out << *p;
    return out;
}

template <class T>
ostream &operator<<(ostream &out, const vector<T> &v) {
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



//
// Env
//

struct Env : public Object {
    ObjectPtr parent;
    map<string, ObjectPtr> entries;
    Env()
        : Object(ENV) {}
    Env(ModulePtr parent)
        : Object(ENV), parent(parent.raw()) {}
    Env(EnvPtr parent)
        : Object(ENV), parent(parent.raw()) {}
};



//
// env module
//

void addGlobal(ModulePtr module, IdentifierPtr name, ObjectPtr value);
ObjectPtr lookupGlobal(ModulePtr module, IdentifierPtr name);

ObjectPtr lookupPublic(ModulePtr module, IdentifierPtr name);

void addLocal(EnvPtr env, IdentifierPtr name, ObjectPtr value);
ObjectPtr lookupEnv(EnvPtr env, IdentifierPtr name);



//
// loader module
//

void addSearchPath(const string &path);
ModulePtr loadProgram(const string &fileName);
ModulePtr loadedModule(const string &module);

ObjectPtr coreName(const string &name);
ObjectPtr primName(const string &name);
ExprPtr moduleNameRef(const string &module, const string &name);
ExprPtr coreNameRef(const string &name);
ExprPtr primNameRef(const string &name);



//
// PrimOp
//

enum PrimOpCode {
    PRIM_TypeP,
    PRIM_TypeSize,

    PRIM_primitiveInit,
    PRIM_primitiveDestroy,
    PRIM_primitiveCopy,
    PRIM_primitiveAssign,
    PRIM_primitiveEqualsP,
    PRIM_primitiveHash,

    PRIM_BoolTypeP,
    PRIM_boolNot,
    PRIM_boolTruth,

    PRIM_IntegerTypeP,
    PRIM_SignedIntegerTypeP,
    PRIM_FloatTypeP,
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

    PRIM_numericConvert,

    PRIM_VoidTypeP,

    PRIM_CompilerObjectTypeP,

    PRIM_PointerTypeP,
    PRIM_PointerType,
    PRIM_Pointer,
    PRIM_PointeeType,

    PRIM_addressOf,
    PRIM_pointerDereference,
    PRIM_pointerToInt,
    PRIM_intToPointer,
    PRIM_pointerCast,
    PRIM_allocateMemory,
    PRIM_freeMemory,

    PRIM_ArrayTypeP,
    PRIM_ArrayType,
    PRIM_Array,
    PRIM_ArrayElementType,
    PRIM_ArraySize,
    PRIM_array,
    PRIM_arrayRef,

    PRIM_TupleTypeP,
    PRIM_TupleType,
    PRIM_Tuple,
    PRIM_TupleSize,
    PRIM_TupleElementType,
    PRIM_TupleElementOffset,
    PRIM_tuple,
    PRIM_tupleRef,

    PRIM_RecordTypeP,
    PRIM_RecordType,
    PRIM_RecordFieldCount,
    PRIM_RecordFieldType,
    PRIM_RecordFieldOffset,
    PRIM_RecordFieldIndex,
    PRIM_recordFieldRef,
    PRIM_recordFieldRefByName,
    PRIM_recordInit,
    PRIM_recordDestroy,
    PRIM_recordCopy,
    PRIM_recordAssign,
    PRIM_recordEqualsP,
    PRIM_recordHash,
};

struct PrimOp : public Object {
    int primOpCode;
    PrimOp(int primOpCode)
        : Object(PRIM_OP), primOpCode(primOpCode) {}
};



//
// Type
//

struct Type : public Object {
    int typeKind;
    llvm::PATypeHolder *llTypeHolder;
    int typeSize;
    Type(int typeKind)
        : Object(TYPE), typeKind(typeKind), llTypeHolder(NULL),
          typeSize(-1) {}
    ~Type() {
        if (llTypeHolder)
            delete llTypeHolder;
    }
};

enum TypeKind {
    BOOL_TYPE,
    INTEGER_TYPE,
    FLOAT_TYPE,
    ARRAY_TYPE,
    TUPLE_TYPE,
    POINTER_TYPE,
    RECORD_TYPE,
    COMPILER_OBJECT_TYPE,
    VOID_TYPE,
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

struct PointerType : public Type {
    TypePtr pointeeType;
    PointerType(TypePtr pointeeType)
        : Type(POINTER_TYPE), pointeeType(pointeeType) {}
};

struct RecordType : public Type {
    RecordPtr record;
    vector<ValuePtr> params;

    bool fieldsInitialized;
    vector<TypePtr> fieldTypes;
    map<string,int> fieldIndexMap;

    const llvm::StructLayout *layout;

    RecordType(RecordPtr record)
        : Type(RECORD_TYPE), record(record), fieldsInitialized(false),
          layout(NULL) {}
    RecordType(RecordPtr record, const vector<ValuePtr> &params)
        : Type(RECORD_TYPE), record(record), params(params),
          fieldsInitialized(false), layout(NULL) {}
};

struct CompilerObjectType : public Type {
    CompilerObjectType()
        : Type(COMPILER_OBJECT_TYPE) {}
};

struct VoidType : public Type {
    VoidType()
        : Type(VOID_TYPE) {}
};



//
// types module
//

extern llvm::Module *llvmModule;
extern llvm::ExecutionEngine *llvmEngine;
extern const llvm::TargetData *llvmTargetData;

void initLLVM();

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

extern TypePtr compilerObjectType;

extern TypePtr voidType;

void initTypes();

TypePtr integerType(int bits, bool isSigned);
TypePtr intType(int bits);
TypePtr uintType(int bits);
TypePtr floatType(int bits);
TypePtr arrayType(TypePtr elememtType, int size);
TypePtr tupleType(const vector<TypePtr> &elementTypes);
TypePtr pointerType(TypePtr pointeeType);
TypePtr recordType(RecordPtr record, const vector<ValuePtr> &params);

const vector<TypePtr> &recordFieldTypes(RecordTypePtr t);
const map<string, int> &recordFieldIndexMap(RecordTypePtr t);

const llvm::Type *llvmType(TypePtr t);

int typeSize(TypePtr t);

void typePrint(TypePtr t, ostream &out);



//
// Value
//

struct Value : public Object {
    TypePtr type;
    char *buf;
    bool isOwned;
    Value(TypePtr type, char *buf, bool isOwned)
        : Object(VALUE), type(type), buf(buf), isOwned(isOwned) {}
    ~Value();
};

extern ValuePtr voidValue;

void initVoidValue();


//
// Pattern
//

enum PatternKind {
    PATTERN_CELL,
    ARRAY_TYPE_PATTERN,
    TUPLE_TYPE_PATTERN,
    POINTER_TYPE_PATTERN,
    RECORD_TYPE_PATTERN,
};

struct Pattern : public Object {
    int patternKind;
    Pattern(int patternKind)
        : Object(PATTERN), patternKind(patternKind) {}
};

struct PatternCell : public Pattern {
    IdentifierPtr name;
    ValuePtr value;
    PatternCell(IdentifierPtr name, ValuePtr value)
        : Pattern(PATTERN_CELL), name(name), value(value) {}
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

struct PointerTypePattern : public Pattern {
    PatternPtr pointeeType;
    PointerTypePattern(PatternPtr pointeeType)
        : Pattern(POINTER_TYPE_PATTERN), pointeeType(pointeeType) {}
};

struct RecordTypePattern : public Pattern {
    RecordPtr record;
    vector<PatternPtr> params;
    RecordTypePattern(RecordPtr record, const vector<PatternPtr> &params)
        : Pattern(RECORD_TYPE_PATTERN), record(record), params(params) {}
};



//
// InvokeTable
//

struct InvokeTable : public Object {
    vector<bool> isStaticFlags;
    vector<vector<InvokeTableEntryPtr> > data;
    InvokeTable() :
        Object(DONT_CARE) {}
};

struct InvokeTableEntry : public Object {
    vector<ObjectPtr> argsInfo;

    // applicable to procedures and overloadables
    EnvPtr env;
    CodePtr code;

    // results of analysis
    TypePtr returnType;
    bool returnByRef;
    bool analyzing;

    InvokeTableEntry() :
        Object(DONT_CARE), analyzing(false) {}
};



//
// evaluator module
//

int toCOIndex(ObjectPtr obj);
ObjectPtr fromCOIndex(int i);

ValuePtr boolToValue(bool x);
ValuePtr intToValue(int x);
int valueToInt(ValuePtr v);
bool valueToBool(ValuePtr v);
ValuePtr coToValue(ObjectPtr x);
ObjectPtr valueToCO(ValuePtr v);
TypePtr valueToType(ValuePtr v);
ObjectPtr lower(ValuePtr v);

void valuePrint(ValuePtr a, ostream &out);

ValuePtr cloneValue(ValuePtr src);
bool valueEquals(ValuePtr a, ValuePtr b);
int valueHash(ValuePtr a);

// the following versions of evaluate create their own temp block
// they also push the expression's location onto location stack
ValuePtr evaluateToStatic(ExprPtr expr, EnvPtr env);
ObjectPtr evaluateToCO(ExprPtr expr, EnvPtr env);
TypePtr evaluateType(ExprPtr expr, EnvPtr env);
TypePtr evaluateNonVoidType(ExprPtr expr, EnvPtr env);
bool evaluateToBool(ExprPtr expr, EnvPtr env);

PatternPtr evaluatePattern(ExprPtr expr, EnvPtr env);
bool unify(PatternPtr pattern, ValuePtr value);
bool unifyType(PatternPtr pattern, TypePtr type);
ValuePtr derefCell(PatternCellPtr cell);

ExprPtr convertCharLiteral(char c);
ExprPtr convertStringLiteral(const string &s);
ExprPtr convertTuple(TuplePtr x);
ExprPtr convertArray(ArrayPtr x);
ExprPtr convertUnaryOp(UnaryOpPtr x);
ExprPtr convertBinaryOp(BinaryOpPtr x);

ValuePtr invoke(ObjectPtr callable, const vector<ValuePtr> &args);

enum MatchInvokeResultKind {
    MATCH_INVOKE_SUCCESS,
    MATCH_INVOKE_ARG_COUNT_ERROR,
    MATCH_INVOKE_ARG_MISMATCH,
    MATCH_INVOKE_PREDICATE_FAILURE,
};

struct MatchInvokeResult : public Object {
    int resultKind;
    MatchInvokeResult(int resultKind)
        : Object(DONT_CARE), resultKind(resultKind) {}
};

struct MatchInvokeSuccess : public MatchInvokeResult {
    EnvPtr env;
    MatchInvokeSuccess(EnvPtr env)
        : MatchInvokeResult(MATCH_INVOKE_SUCCESS), env(env) {}
};

struct MatchInvokeArgCountError : public MatchInvokeResult {
    MatchInvokeArgCountError()
        : MatchInvokeResult(MATCH_INVOKE_ARG_COUNT_ERROR) {}
};

struct MatchInvokeArgMismatch : public MatchInvokeResult {
    int pos;
    MatchInvokeArgMismatch(int pos)
        : MatchInvokeResult(MATCH_INVOKE_ARG_MISMATCH), pos(pos) {}
};

struct MatchInvokePredicateFailure : public MatchInvokeResult {
    MatchInvokePredicateFailure()
        : MatchInvokeResult(MATCH_INVOKE_PREDICATE_FAILURE) {}
};

void signalMatchInvokeError(MatchInvokeResultPtr result);

StatementPtr convertForStatement(ForPtr x);

void initExternalProcedure(ExternalProcedurePtr x);



//
// analyzer module
//

struct Analysis : public Object {
    TypePtr type;
    bool isTemp;
    bool isStatic;
    ExprPtr expr;
    EnvPtr env;
    mutable ValuePtr value;

    Analysis(TypePtr type, bool isTemp, bool isStatic)
        : Object(ANALYSIS), type(type), isTemp(isTemp),
          isStatic(isStatic) {}
    ValuePtr evaluate() const;
    TypePtr evaluateType() const;
};

struct ReturnInfo : public Object{
    TypePtr type;
    bool isRef;
    ReturnInfo()
        : Object(DONT_CARE), type(NULL), isRef(false) {}
    ReturnInfo(TypePtr type, bool isRef)
        : Object(DONT_CARE), type(type), isRef(isRef) {}
    void set(TypePtr type, bool isRef);
};

AnalysisPtr analyze(ExprPtr expr, EnvPtr env);
ReturnInfoPtr analyzeInvoke(ObjectPtr obj, const vector<AnalysisPtr> &args);


#endif
