#ifndef __CLAY_HPP
#define __CLAY_HPP

#include <string>
#include <vector>
using std::string;
using std::vector;



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
    T *raw() { return ptr; }
    operator bool() const { return ptr != 0; }
    bool operator!() const { return ptr == 0; }
};



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

    IMPORT,
    EXPORT,
    MODULE,
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

struct Import;
struct Export;
struct Module;



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

typedef Ptr<Import> ImportPtr;
typedef Ptr<Export> ExportPtr;
typedef Ptr<Module> ModulePtr;



//
// Source, Location

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
    string value;
    BoolLiteral(const string &value)
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
    AND,
    OR,
};

struct BinaryOp : public Expr {
    int op;
    ExprPtr expr1, expr2;
    BinaryOp(int op, ExprPtr expr1, ExprPtr expr2)
        : Expr(BINARY_OP), op(op), expr1(expr1), expr2(expr2) {}
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
    Code(const vector<IdentifierPtr> &patternVars, ExprPtr predicate,
         const vector<FormalArgPtr> &formalArgs, StatementPtr body)
        : ANode(CODE), patternVars(patternVars), predicate(predicate),
          formalArgs(formalArgs), body(body) {}
};



//
// TopLevelItem
//

struct TopLevelItem : public ANode {
    TopLevelItem(int objKind)
        : ANode(objKind) {}
};

struct Record : public TopLevelItem {
    IdentifierPtr name;
    vector<IdentifierPtr> patternVars;
    vector<FormalArgPtr> formalArgs;
    Record(IdentifierPtr name, const vector<IdentifierPtr> &patternVars,
           const vector<FormalArgPtr> &formalArgs)
        : TopLevelItem(RECORD), name(name), patternVars(patternVars),
          formalArgs(formalArgs) {}
};

struct Procedure : public TopLevelItem {
    IdentifierPtr name;
    CodePtr code;
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
    Overloadable(IdentifierPtr name)
        : TopLevelItem(OVERLOADABLE), name(name) {}
};

struct ExternalProcedure : public TopLevelItem {
    IdentifierPtr name;
    vector<ValueArgPtr> args;
    ExprPtr returnType;
    ExternalProcedure(IdentifierPtr name, const vector<ValueArgPtr> &args,
                      ExprPtr returnType)
        : TopLevelItem(EXTERNAL_PROCEDURE), name(name), args(args),
          returnType(returnType) {}
};



//
// Import, Export
//

struct Import : public ANode {
    DottedNamePtr dottedName;
    Import(DottedNamePtr dottedName)
        : ANode(IMPORT), dottedName(dottedName) {}
};

struct Export : public ANode {
    DottedNamePtr dottedName;
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
    Module(const vector<ImportPtr> &imports, const vector<ExportPtr> &exports,
           const vector<TopLevelItemPtr> &topLevelItems)
        : ANode(MODULE), imports(imports), exports(exports) {}
};



//
// error reporting
//

void pushLocation(LocationPtr location);
void popLocation();

struct LocationPusher {
    LocationPusher(LocationPtr location)  {
        pushLocation(location);
    }
    ~LocationPusher() {
        popLocation();
    }
private :
    LocationPusher(const LocationPusher &) {}
    void operator=(const LocationPusher &) {}
};

void error(const string &msg);



//
// load source
//

SourcePtr loadSource(const string &fileName);



//
// lexer
//

void tokenize(SourcePtr source, vector<TokenPtr> &tokens);
const char *tokenName(int tokenKind);


#endif
