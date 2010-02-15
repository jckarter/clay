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
#include <llvm/Target/TargetData.h>
#include <llvm/Target/TargetSelect.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/JIT.h>

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
        return p <= other.p;
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
    OBJECT_EXPR,

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
    RECORD_FIELD,
    PROCEDURE,
    OVERLOAD,
    OVERLOADABLE,
    EXTERNAL_PROCEDURE,
    EXTERNAL_ARG,

    IMPORT,
    MODULE_HOLDER,
    MODULE,

    ENV,

    PRIM_OP,

    TYPE,
    PATTERN,

    VOID_TYPE,
    VALUE_HOLDER,
    PVALUE,

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
struct ObjectExpr;

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
struct RecordField;
struct Procedure;
struct Overload;
struct Overloadable;
struct ExternalProcedure;
struct ExternalArg;

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
struct FunctionPointerType;
struct RecordType;

struct Pattern;
struct PatternCell;
struct ArrayTypePattern;
struct TupleTypePattern;
struct PointerTypePattern;
struct FunctionPointerTypePattern;
struct RecordTypePattern;

struct VoidType;
struct ValueHolder;
struct PValue;



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
typedef Pointer<SCExpr> SCExprPtr;
typedef Pointer<ObjectExpr> ObjectExprPtr;

typedef Pointer<Statement> StatementPtr;
typedef Pointer<Block> BlockPtr;
typedef Pointer<Label> LabelPtr;
typedef Pointer<Binding> BindingPtr;
typedef Pointer<Assignment> AssignmentPtr;
typedef Pointer<Goto> GotoPtr;
typedef Pointer<Return> ReturnPtr;
typedef Pointer<ReturnRef> ReturnRefPtr;
typedef Pointer<If> IfPtr;
typedef Pointer<ExprStatement> ExprStatementPtr;
typedef Pointer<While> WhilePtr;
typedef Pointer<Break> BreakPtr;
typedef Pointer<Continue> ContinuePtr;
typedef Pointer<For> ForPtr;

typedef Pointer<FormalArg> FormalArgPtr;
typedef Pointer<ValueArg> ValueArgPtr;
typedef Pointer<StaticArg> StaticArgPtr;
typedef Pointer<Code> CodePtr;

typedef Pointer<TopLevelItem> TopLevelItemPtr;
typedef Pointer<Record> RecordPtr;
typedef Pointer<RecordField> RecordFieldPtr;
typedef Pointer<Procedure> ProcedurePtr;
typedef Pointer<Overload> OverloadPtr;
typedef Pointer<Overloadable> OverloadablePtr;
typedef Pointer<ExternalProcedure> ExternalProcedurePtr;
typedef Pointer<ExternalArg> ExternalArgPtr;

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
typedef Pointer<FunctionPointerType> FunctionPointerTypePtr;
typedef Pointer<RecordType> RecordTypePtr;

typedef Pointer<Pattern> PatternPtr;
typedef Pointer<PatternCell> PatternCellPtr;
typedef Pointer<ArrayTypePattern> ArrayTypePatternPtr;
typedef Pointer<TupleTypePattern> TupleTypePatternPtr;
typedef Pointer<PointerTypePattern> PointerTypePatternPtr;
typedef Pointer<FunctionPointerTypePattern> FunctionPointerTypePatternPtr;
typedef Pointer<RecordTypePattern> RecordTypePatternPtr;

typedef Pointer<VoidType> VoidTypePtr;
typedef Pointer<ValueHolder> ValueHolderPtr;
typedef Pointer<PValue> PValuePtr;



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

void error(const string &msg);
void fmtError(const char *fmt, ...);

template <class T>
void error(Pointer<T> context, const string &msg)
{
    if (context->location.ptr())
        pushLocation(context->location);
    error(msg);
}

template <class T>
void ensureArity(const vector<T> &args, int size)
{
    if ((int)args.size() != size)
        error("incorrect number of arguments");
}

template <class T>
void ensureArity2(const vector<T> &args, int size, bool hasVarArgs)
{
    if (!hasVarArgs) 
        ensureArity(args, size);
    else if ((int)args.size() < size)
        error("incorrect no of arguments");
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
    ExprPtr desugared;
    CharLiteral(char value)
        : Expr(CHAR_LITERAL), value(value) {}
};

struct StringLiteral : public Expr {
    string value;
    ExprPtr desugared;
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
    ExprPtr desugared;
    Tuple()
        : Expr(TUPLE) {}
    Tuple(const vector<ExprPtr> &args)
        : Expr(TUPLE), args(args) {}
};

struct Array : public Expr {
    vector<ExprPtr> args;
    ExprPtr desugared;
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

struct SCExpr : public Expr {
    EnvPtr env;
    ExprPtr expr;
    SCExpr(EnvPtr env, ExprPtr expr)
        : Expr(SC_EXPR), env(env), expr(expr) {}
};

struct ObjectExpr : public Expr {
    ObjectPtr obj;
    ObjectExpr(ObjectPtr obj)
        : Expr(OBJECT_EXPR), obj(obj) {}
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
    StatementPtr desugared;
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
    Code(const vector<IdentifierPtr> &patternVars,
         ExprPtr predicate,
         const vector<FormalArgPtr> &formalArgs,
         StatementPtr body)
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
    vector<RecordFieldPtr> fields;
    Record()
        : TopLevelItem(RECORD) {}
    Record(IdentifierPtr name,
           const vector<IdentifierPtr> &patternVars,
           const vector<RecordFieldPtr> &fields)
        : TopLevelItem(RECORD), name(name), patternVars(patternVars),
          fields(fields) {}
};

struct RecordField : public ANode {
    IdentifierPtr name;
    ExprPtr type;
    RecordField(IdentifierPtr name, ExprPtr type)
        : ANode(RECORD_FIELD), name(name), type(type) {}
};

struct Procedure : public TopLevelItem {
    IdentifierPtr name;
    CodePtr code;
    bool staticFlagsInitialized;
    Procedure(IdentifierPtr name, CodePtr code)
        : TopLevelItem(PROCEDURE), name(name), code(code),
          staticFlagsInitialized(false) {}
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
    bool staticFlagsInitialized;
    Overloadable(IdentifierPtr name)
        : TopLevelItem(OVERLOADABLE), name(name),
          staticFlagsInitialized(false) {}
};

struct ExternalProcedure : public TopLevelItem {
    IdentifierPtr name;
    vector<ExternalArgPtr> args;
    bool hasVarArgs;
    ExprPtr returnType;
    ObjectPtr returnType2;
    llvm::Function *llvmFunc;
    ExternalProcedure()
        : TopLevelItem(EXTERNAL_PROCEDURE), hasVarArgs(false), llvmFunc(NULL) {}
    ExternalProcedure(IdentifierPtr name,
                      const vector<ExternalArgPtr> &args,
                      bool hasVarArgs,
                      ExprPtr returnType)
        : TopLevelItem(EXTERNAL_PROCEDURE), name(name), args(args), 
          hasVarArgs(hasVarArgs), returnType(returnType), llvmFunc(NULL) {}
};

struct ExternalArg : public ANode {
    IdentifierPtr name;
    ExprPtr type;
    TypePtr type2;
    ExternalArg(IdentifierPtr name, ExprPtr type)
        : ANode(EXTERNAL_ARG), name(name), type(type) {}
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
    ModulePtr module;
    Import(int importKind, DottedNamePtr dottedName)
        : ANode(IMPORT), importKind(importKind), dottedName(dottedName) {}
};

struct ImportModule : public Import {
    IdentifierPtr alias;
    ImportModule(DottedNamePtr dottedName, IdentifierPtr alias)
        : Import(IMPORT_MODULE, dottedName), alias(alias) {}
};

struct ImportStar : public Import {
    ImportStar(DottedNamePtr dottedName)
        : Import(IMPORT_STAR, dottedName) {}
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
    ImportMembers(DottedNamePtr dottedName)
        : Import(IMPORT_MEMBERS, dottedName) {}
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

void addGlobal(ModulePtr module, IdentifierPtr name, ObjectPtr value);
ObjectPtr lookupModuleHolder(ModuleHolderPtr mh, IdentifierPtr name);
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
    PRIM_pointerToInt,
    PRIM_intToPointer,
    PRIM_allocateMemory,
    PRIM_freeMemory,

    PRIM_FunctionPointerTypeP,
    PRIM_FunctionPointer,
    PRIM_makeFunctionPointer,

    PRIM_pointerCast,

    PRIM_Array,
    PRIM_array,
    PRIM_arrayRef,

    PRIM_TupleTypeP,
    PRIM_Tuple,
    PRIM_TupleElementCount,
    PRIM_TupleElementOffset,
    PRIM_tuple,
    PRIM_tupleRef,

    PRIM_RecordTypeP,
    PRIM_RecordFieldCount,
    PRIM_RecordFieldOffset,
    PRIM_RecordFieldIndex,
    PRIM_recordFieldRef,
    PRIM_recordFieldRefByName,
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
    POINTER_TYPE,
    FUNCTION_POINTER_TYPE,
    ARRAY_TYPE,
    TUPLE_TYPE,
    RECORD_TYPE,
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

struct FunctionPointerType : public Type {
    vector<TypePtr> argTypes;
    ObjectPtr returnType;
    FunctionPointerType(const vector<TypePtr> &argTypes, ObjectPtr returnType)
        : Type(FUNCTION_POINTER_TYPE), argTypes(argTypes),
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
    map<string,int> fieldIndexMap;

    const llvm::StructLayout *layout;

    RecordType(RecordPtr record)
        : Type(RECORD_TYPE), record(record), fieldsInitialized(false),
          layout(NULL) {}
    RecordType(RecordPtr record, const vector<ObjectPtr> &params)
        : Type(RECORD_TYPE), record(record), params(params),
          fieldsInitialized(false), layout(NULL) {}
};



//
// llvm module
//

extern llvm::Module *llvmModule;
extern llvm::ExecutionEngine *llvmEngine;
extern const llvm::TargetData *llvmTargetData;

extern llvm::Function *llvmFunction;
extern llvm::IRBuilder<> *initBuilder;
extern llvm::IRBuilder<> *builder;

void initLLVM();



//
// types module
//

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

extern VoidTypePtr voidType;

void initTypes();

TypePtr integerType(int bits, bool isSigned);
TypePtr intType(int bits);
TypePtr uintType(int bits);
TypePtr floatType(int bits);
TypePtr pointerType(TypePtr pointeeType);
TypePtr functionPointerType(const vector<TypePtr> &argTypes,
                            ObjectPtr returnType);
TypePtr arrayType(TypePtr elememtType, int size);
TypePtr tupleType(const vector<TypePtr> &elementTypes);
TypePtr recordType(RecordPtr record, const vector<ObjectPtr> &params);

const vector<TypePtr> &recordFieldTypes(RecordTypePtr t);
const map<string, int> &recordFieldIndexMap(RecordTypePtr t);

const llvm::StructLayout *tupleTypeLayout(TupleType *t);
const llvm::StructLayout *recordTypeLayout(RecordType *t);

const llvm::Type *llvmReturnType(ObjectPtr returnType);
const llvm::Type *llvmType(TypePtr t);
int typeSize(TypePtr t);
void typePrint(TypePtr t, ostream &out);



//
// Pattern
//

enum PatternKind {
    PATTERN_CELL,
    POINTER_TYPE_PATTERN,
    FUNCTION_POINTER_TYPE_PATTERN,
    ARRAY_TYPE_PATTERN,
    TUPLE_TYPE_PATTERN,
    RECORD_TYPE_PATTERN,
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

struct FunctionPointerTypePattern : public Pattern {
    vector<PatternPtr> argTypes;
    PatternPtr returnType;
    FunctionPointerTypePattern(const vector<PatternPtr> &argTypes,
                               PatternPtr returnType)
        : Pattern(FUNCTION_POINTER_TYPE_PATTERN), argTypes(argTypes),
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



//
// VoidType
//

struct VoidType : public Object {
    VoidType()
        : Object(VOID_TYPE) {}
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
// desugar
//

ExprPtr desugarCharLiteral(char c);
ExprPtr desugarStringLiteral(const string &s);
ExprPtr desugarTuple(TuplePtr x);
ExprPtr desugarArray(ArrayPtr x);
ExprPtr desugarUnaryOp(UnaryOpPtr x);
ExprPtr desugarBinaryOp(BinaryOpPtr x);
StatementPtr desugarForStatement(ForPtr x);



//
// invoke tables
//

void initIsStaticFlags(ProcedurePtr x);
void initIsStaticFlags(OverloadablePtr x);
const vector<bool> &lookupIsStaticFlags(ObjectPtr callable, unsigned nArgs);
bool computeArgsKey(ObjectPtr callable,
                    const vector<ExprPtr> &args,
                    EnvPtr env,
                    vector<ObjectPtr> &argsKey,
                    vector<LocationPtr> &argLocations);


struct InvokeEntry : public Object {
    ObjectPtr callable;
    vector<ObjectPtr> argsKey;

    bool analyzed;
    bool analyzing;
    EnvPtr env;
    CodePtr code;
    ObjectPtr analysis;

    InvokeEntry(ObjectPtr callable, const vector<ObjectPtr> &argsKey)
        : Object(DONT_CARE), callable(callable), argsKey(argsKey),
          analyzed(false), analyzing(false) {}
};

typedef Pointer<InvokeEntry> InvokeEntryPtr;

InvokeEntryPtr lookupInvoke(ObjectPtr callable,
                            const vector<ObjectPtr> &argsKey);



//
// match invoke
//

enum MatchCode {
    MATCH_SUCCESS,
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
    MatchSuccess(EnvPtr env)
        : MatchResult(MATCH_SUCCESS), env(env) {}
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

MatchResultPtr matchInvoke(CodePtr code, EnvPtr env,
                           const vector<ObjectPtr> &argsKey);

void signalMatchError(MatchResultPtr result,
                      const vector<LocationPtr> &argLocations);



//
// analyzer
//

struct PValue : public Object {
    TypePtr type;
    bool isTemp;
    PValue(TypePtr type, bool isTemp)
        : Object(PVALUE), type(type), isTemp(isTemp) {}
};

ObjectPtr analyze(ExprPtr expr, EnvPtr env);
PValuePtr analyzeValue(ExprPtr expr, EnvPtr env);
PValuePtr analyzePointerValue(ExprPtr expr, EnvPtr env);
PValuePtr analyzeArrayValue(ExprPtr expr, EnvPtr env);
PValuePtr analyzeTupleValue(ExprPtr expr, EnvPtr env);
PValuePtr analyzeRecordValue(ExprPtr expr, EnvPtr env);
ObjectPtr analyzeIndexing(ObjectPtr x,
                          const vector<ExprPtr> &args,
                          EnvPtr env);
ObjectPtr analyzeInvoke(ObjectPtr x, const vector<ExprPtr> &args, EnvPtr env);
ObjectPtr analyzeInvokeType(TypePtr x,
                            const vector<ExprPtr> &args,
                            EnvPtr env);
ObjectPtr analyzeInvokeRecord(RecordPtr x,
                              const vector<ExprPtr> &args,
                              EnvPtr env);
ObjectPtr analyzeInvokeProcedure(ProcedurePtr x,
                                 const vector<ExprPtr> &args,
                                 EnvPtr env);
ObjectPtr analyzeInvokeProcedure2(ProcedurePtr x,
                                  const vector<ObjectPtr> &argsKey,
                                  const vector<LocationPtr> &argLocations);
ObjectPtr analyzeInvokeOverloadable(OverloadablePtr x,
                                    const vector<ExprPtr> &args,
                                    EnvPtr env);
ObjectPtr analyzeInvokeOverloadable2(OverloadablePtr x,
                                     const vector<ObjectPtr> &argsKey,
                                     const vector<LocationPtr> &argLocations);
EnvPtr bindDynamicArgs(EnvPtr env, CodePtr code,
                       const vector<ObjectPtr> &argsKey);
ObjectPtr analyzeCodeBody(CodePtr code, EnvPtr env);
bool analyzeStatement(StatementPtr stmt, EnvPtr env, ObjectPtr &result);
EnvPtr analyzeBinding(BindingPtr x, EnvPtr env);
ObjectPtr analyzeInvokeExternal(ExternalProcedurePtr x,
                                const vector<ExprPtr> &args,
                                EnvPtr env);
ObjectPtr analyzeInvokeValue(PValuePtr x,
                             const vector<ExprPtr> &args,
                             EnvPtr env);
ObjectPtr analyzeInvokePrimOp(PrimOpPtr x,
                              const vector<ExprPtr> &args,
                              EnvPtr env);

ObjectPtr evaluateStatic(ExprPtr expr, EnvPtr env);

TypePtr evaluateType(ExprPtr expr, EnvPtr env);
ObjectPtr evaluateReturnType(ExprPtr expr, EnvPtr env);
TypePtr evaluateNumericType(ExprPtr expr, EnvPtr env);
TypePtr evaluateIntegerType(ExprPtr expr, EnvPtr env);
TypePtr evaluatePointerOrFunctionPointerType(ExprPtr expr, EnvPtr env);
TypePtr evaluateTupleType(ExprPtr expr, EnvPtr env);
TypePtr evaluateRecordType(ExprPtr expr, EnvPtr env);

IdentifierPtr evaluateIdentifier(ExprPtr expr, EnvPtr env);

int evaluateInt(ExprPtr expr, EnvPtr env);
bool evaluateBool(ExprPtr expr, EnvPtr env);

ValueHolderPtr intToValueHolder(int x);
ValueHolderPtr boolToValueHolder(bool x);

PatternPtr evaluatePattern(ExprPtr expr, EnvPtr env);
bool unify(PatternPtr pattern, ObjectPtr obj);
ObjectPtr derefCell(PatternCellPtr cell);


//
// object ops
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

#endif
