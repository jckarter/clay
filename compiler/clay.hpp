#pragma once


#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#endif

#include <string>
#include <vector>
#include <exception>
#include <map>
#include <set>
#include <iomanip>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <climits>
#include <cerrno>

#ifdef _MSC_VER
// LLVM headers spew warnings on MSVC
#pragma warning(push)
#pragma warning(disable: 4146 4244 4267 4355 4146 4800 4996)
#endif

#include <llvm/ADT/FoldingSet.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/Triple.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/Assembly/Writer.h>
#include <llvm/Assembly/Parser.h>
#include <llvm/Assembly/PrintModulePass.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/CodeGen/LinkAllAsmWriterComponents.h>
#include <llvm/CodeGen/LinkAllCodegenComponents.h>
#include <llvm/CodeGen/ValueTypes.h>
#include <llvm/Config/config.h>
#include <llvm/DebugInfo.h>
#include <llvm/DIBuilder.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/LinkAllIR.h>
#include <llvm/Linker.h>
#include <llvm/PassManager.h>
#include <llvm/Support/Allocator.h>
#include <llvm/Support/Dwarf.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/PathV2.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Vectorize.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif


#include "refcounted.hpp"


namespace clay {

template<bool Cond> struct StaticAssertChecker;
template<> struct StaticAssertChecker<true> { typedef int type; };
template<> struct StaticAssertChecker<false> { };

#define _CLAY_CAT(a,b) a##b
#define CLAY_CAT(a,b) _CLAY_CAT(a,b)

#define CLAY_STATIC_ASSERT(cond) \
    static const ::clay::StaticAssertChecker<(cond)>::type CLAY_CAT(static_assert_, __LINE__);

using std::string;
using std::vector;
using std::pair;
using std::make_pair;
using std::map;
using std::set;

}

#ifdef _MSC_VER

#define strtoll _strtoi64
#define strtoull _strtoui64
#define strtold strtod
#define snprintf _snprintf
#define CLAY_ALIGN(n) __declspec(align(n))

#include <complex>

namespace clay {

typedef std::complex<float> clay_cfloat;
typedef std::complex<double> clay_cdouble;
typedef std::complex<long double> clay_cldouble;

template<typename T>
inline T clay_creal(std::complex<T> const &c) { return std::real(c); }

template<typename T>
inline T clay_cimag(std::complex<T> const &c) { return std::imag(c); }

}

#else

#define CLAY_ALIGN(n) __attribute__((aligned(n)))

namespace clay {

typedef _Complex float clay_cfloat;
typedef _Complex double clay_cdouble;
typedef _Complex long double clay_cldouble;

inline float clay_creal(_Complex float c) { return __real__ c; }
inline double clay_creal(_Complex double c) { return __real__ c; }
inline long double clay_creal(_Complex long double c) { return __real__ c; }

inline float clay_cimag(_Complex float c) { return __imag__ c; }
inline double clay_cimag(_Complex double c) { return __imag__ c; }
inline long double clay_cimag(_Complex long double c) { return __imag__ c; }

}

#endif

#define CLAY_LANGUAGE_VERSION "0.2-WIP"
#define CLAY_COMPILER_VERSION "0.2git"

namespace clay {


//
// container typedefs
//

typedef llvm::SmallString<260> PathString;


//
// Target-specific types
//
typedef int ptrdiff32_t;
typedef long long ptrdiff64_t;

typedef unsigned size32_t;
typedef unsigned long long size64_t;


//
// ObjectKind
//

#define OBJECT_KIND_MAP(XX) \
    XX(SOURCE) \
    XX(LOCATION) \
\
    XX(IDENTIFIER) \
    XX(DOTTED_NAME) \
\
    XX(EXPRESSION) \
    XX(EXPR_LIST) \
    XX(STATEMENT) \
    XX(CASE_BLOCK) \
    XX(CATCH) \
\
    XX(FORMAL_ARG) \
    XX(RETURN_SPEC) \
    XX(LLVM_CODE) \
    XX(CODE) \
\
    XX(RECORD_DECL) \
    XX(RECORD_BODY) \
    XX(RECORD_FIELD) \
    XX(VARIANT_DECL) \
    XX(INSTANCE_DECL) \
    XX(NEW_TYPE_DECL) \
    XX(OVERLOAD) \
    XX(PROCEDURE) \
    XX(INTRINSIC) \
    XX(ENUM_DECL) \
    XX(ENUM_MEMBER) \
    XX(GLOBAL_VARIABLE) \
    XX(EXTERNAL_PROCEDURE) \
    XX(EXTERNAL_ARG) \
    XX(EXTERNAL_VARIABLE) \
    XX(EVAL_TOPLEVEL) \
\
    XX(GLOBAL_ALIAS) \
\
    XX(IMPORT) \
    XX(MODULE_DECLARATION) \
    XX(MODULE) \
\
    XX(ENV) \
\
    XX(PRIM_OP) \
\
    XX(TYPE) \
\
    XX(PATTERN) \
    XX(MULTI_PATTERN) \
\
    XX(VALUE_HOLDER) \
    XX(MULTI_STATIC) \
\
    XX(PVALUE) \
    XX(MULTI_PVALUE) \
\
    XX(EVALUE) \
    XX(MULTI_EVALUE) \
\
    XX(CVALUE) \
    XX(MULTI_CVALUE) \
\
    XX(DOCUMENTATION) \
\
    XX(STATIC_ASSERT_TOP_LEVEL) \


#define OBJECT_KIND_GEN(e) e,
enum ObjectKind {
    OBJECT_KIND_MAP(OBJECT_KIND_GEN)
};
#undef OBJECT_KIND_GEN

llvm::raw_ostream &operator<<(llvm::raw_ostream &os, ObjectKind);

//
// Object
//

struct Object : public RefCounted {
    const ObjectKind objKind;
    Object(ObjectKind objKind)
        : objKind(objKind) {}
    virtual ~Object() {}

    // print to stderr, for debugging
    void print() const;
    // use only in debugger
    std::string toString() const;
};

typedef Pointer<Object> ObjectPtr;



//
// forwards
//

struct Source;

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
struct FILEExpr;
struct LINEExpr;
struct COLUMNExpr;
struct ARGExpr;
struct Tuple;
struct Paren;
struct Indexing;
struct Call;
struct FieldRef;
struct StaticIndexing;
struct VariadicOp;
struct And;
struct Or;
struct Lambda;
struct Unpack;
struct StaticExpr;
struct DispatchExpr;
struct ForeignExpr;
struct ObjectExpr;
struct EvalExpr;

struct ExprList;

struct Statement;
struct Block;
struct Label;
struct Binding;
struct Assignment;
struct InitAssignment;
struct VariadicAssignment;
struct Goto;
struct Switch;
struct CaseBlock;
struct Return;
struct If;
struct ExprStatement;
struct While;
struct Break;
struct Continue;
struct For;
struct ForeignStatement;
struct Try;
struct Catch;
struct Throw;
struct StaticFor;
struct Finally;
struct OnError;
struct Unreachable;
struct EvalStatement;
struct StaticAssertStatement;

struct FormalArg;
struct ReturnSpec;
struct LLVMCode;
struct Code;

struct TopLevelItem;
struct RecordDecl;
struct RecordBody;
struct RecordField;
struct VariantDecl;
struct InstanceDecl;
struct NewTypeDecl;
struct Overload;
struct Procedure;
struct IntrinsicSymbol;
struct EnumDecl;
struct EnumMember;
struct GlobalVariable;
struct GVarInstance;
struct ExternalProcedure;
struct ExternalArg;
struct ExternalVariable;
struct EvalTopLevel;
struct StaticAssertTopLevel;
struct Documentation;

struct GlobalAlias;

struct Import;
struct ImportModule;
struct ImportStar;
struct ImportMembers;
struct Module;
struct ModuleDeclaration;

struct Env;

struct PrimOp;

struct Type;
struct BoolType;
struct IntegerType;
struct FloatType;
struct ComplexType;
struct ArrayType;
struct VecType;
struct TupleType;
struct UnionType;
struct PointerType;
struct CodePointerType;
struct CCodePointerType;
struct RecordType;
struct VariantType;
struct StaticType;
struct EnumType;
struct NewType;

struct Pattern;
struct PatternCell;
struct PatternStruct;

struct MultiPattern;
struct MultiPatternCell;
struct MultiPatternList;

struct ValueHolder;
struct MultiStatic;

struct PValue;
struct MultiPValue;

struct EValue;
struct MultiEValue;

struct CValue;
struct MultiCValue;

struct ObjectTable;

struct ValueStackEntry;

struct MatchResult;
struct MatchFailureError;


//
// Pointer typedefs
//

typedef Pointer<Source> SourcePtr;

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
typedef Pointer<FILEExpr> FILEExprPtr;
typedef Pointer<LINEExpr> LINEExprPtr;
typedef Pointer<COLUMNExpr> COLUMNExprPtr;
typedef Pointer<ARGExpr> ARGExprPtr;
typedef Pointer<Tuple> TuplePtr;
typedef Pointer<Paren> ParenPtr;
typedef Pointer<Indexing> IndexingPtr;
typedef Pointer<Call> CallPtr;
typedef Pointer<FieldRef> FieldRefPtr;
typedef Pointer<StaticIndexing> StaticIndexingPtr;
typedef Pointer<VariadicOp> VariadicOpPtr;
typedef Pointer<And> AndPtr;
typedef Pointer<Or> OrPtr;
typedef Pointer<Lambda> LambdaPtr;
typedef Pointer<Unpack> UnpackPtr;
typedef Pointer<StaticExpr> StaticExprPtr;
typedef Pointer<DispatchExpr> DispatchExprPtr;
typedef Pointer<ForeignExpr> ForeignExprPtr;
typedef Pointer<ObjectExpr> ObjectExprPtr;
typedef Pointer<EvalExpr> EvalExprPtr;

typedef Pointer<ExprList> ExprListPtr;

typedef Pointer<Statement> StatementPtr;
typedef Pointer<Block> BlockPtr;
typedef Pointer<Label> LabelPtr;
typedef Pointer<Binding> BindingPtr;
typedef Pointer<Assignment> AssignmentPtr;
typedef Pointer<InitAssignment> InitAssignmentPtr;
typedef Pointer<VariadicAssignment> VariadicAssignmentPtr;
typedef Pointer<Goto> GotoPtr;
typedef Pointer<Switch> SwitchPtr;
typedef Pointer<CaseBlock> CaseBlockPtr;
typedef Pointer<Return> ReturnPtr;
typedef Pointer<If> IfPtr;
typedef Pointer<ExprStatement> ExprStatementPtr;
typedef Pointer<While> WhilePtr;
typedef Pointer<Break> BreakPtr;
typedef Pointer<Continue> ContinuePtr;
typedef Pointer<For> ForPtr;
typedef Pointer<ForeignStatement> ForeignStatementPtr;
typedef Pointer<Try> TryPtr;
typedef Pointer<Catch> CatchPtr;
typedef Pointer<Throw> ThrowPtr;
typedef Pointer<StaticFor> StaticForPtr;
typedef Pointer<Finally> FinallyPtr;
typedef Pointer<OnError> OnErrorPtr;
typedef Pointer<Unreachable> UnreachablePtr;
typedef Pointer<EvalStatement> EvalStatementPtr;
typedef Pointer<StaticAssertStatement> StaticAssertStatementPtr;

typedef Pointer<FormalArg> FormalArgPtr;
typedef Pointer<ReturnSpec> ReturnSpecPtr;
typedef Pointer<LLVMCode> LLVMCodePtr;
typedef Pointer<Code> CodePtr;

typedef Pointer<TopLevelItem> TopLevelItemPtr;
typedef Pointer<RecordDecl> RecordDeclPtr;
typedef Pointer<RecordBody> RecordBodyPtr;
typedef Pointer<RecordField> RecordFieldPtr;
typedef Pointer<VariantDecl> VariantDeclPtr;
typedef Pointer<InstanceDecl> InstanceDeclPtr;
typedef Pointer<NewTypeDecl> NewTypeDeclPtr;
typedef Pointer<Overload> OverloadPtr;
typedef Pointer<Procedure> ProcedurePtr;
typedef Pointer<IntrinsicSymbol> IntrinsicPtr;
typedef Pointer<EnumDecl> EnumDeclPtr;
typedef Pointer<EnumMember> EnumMemberPtr;
typedef Pointer<GlobalVariable> GlobalVariablePtr;
typedef Pointer<GVarInstance> GVarInstancePtr;
typedef Pointer<ExternalProcedure> ExternalProcedurePtr;
typedef Pointer<ExternalArg> ExternalArgPtr;
typedef Pointer<ExternalVariable> ExternalVariablePtr;
typedef Pointer<EvalTopLevel> EvalTopLevelPtr;
typedef Pointer<StaticAssertTopLevel> StaticAssertTopLevelPtr;
typedef Pointer<Documentation> DocumentationPtr;

typedef Pointer<GlobalAlias> GlobalAliasPtr;

typedef Pointer<Import> ImportPtr;
typedef Pointer<ImportModule> ImportModulePtr;
typedef Pointer<ImportStar> ImportStarPtr;
typedef Pointer<ImportMembers> ImportMembersPtr;
typedef Pointer<Module> ModulePtr;
typedef Pointer<ModuleDeclaration> ModuleDeclarationPtr;

typedef Pointer<Env> EnvPtr;

typedef Pointer<PrimOp> PrimOpPtr;

typedef Pointer<Type> TypePtr;
typedef Pointer<BoolType> BoolTypePtr;
typedef Pointer<IntegerType> IntegerTypePtr;
typedef Pointer<FloatType> FloatTypePtr;
typedef Pointer<ComplexType> ComplexTypePtr;
typedef Pointer<ArrayType> ArrayTypePtr;
typedef Pointer<VecType> VecTypePtr;
typedef Pointer<TupleType> TupleTypePtr;
typedef Pointer<UnionType> UnionTypePtr;
typedef Pointer<PointerType> PointerTypePtr;
typedef Pointer<CodePointerType> CodePointerTypePtr;
typedef Pointer<CCodePointerType> CCodePointerTypePtr;
typedef Pointer<RecordType> RecordTypePtr;
typedef Pointer<VariantType> VariantTypePtr;
typedef Pointer<StaticType> StaticTypePtr;
typedef Pointer<EnumType> EnumTypePtr;
typedef Pointer<NewType> NewTypePtr;

typedef Pointer<Pattern> PatternPtr;
typedef Pointer<PatternCell> PatternCellPtr;
typedef Pointer<PatternStruct> PatternStructPtr;
typedef Pointer<MultiPattern> MultiPatternPtr;
typedef Pointer<MultiPatternCell> MultiPatternCellPtr;
typedef Pointer<MultiPatternList> MultiPatternListPtr;

typedef Pointer<ValueHolder> ValueHolderPtr;
typedef Pointer<MultiStatic> MultiStaticPtr;

typedef Pointer<PValue> PValuePtr;
typedef Pointer<MultiPValue> MultiPValuePtr;

typedef Pointer<EValue> EValuePtr;
typedef Pointer<MultiEValue> MultiEValuePtr;

typedef Pointer<CValue> CValuePtr;
typedef Pointer<MultiCValue> MultiCValuePtr;

typedef Pointer<ObjectTable> ObjectTablePtr;

typedef Pointer<MatchResult> MatchResultPtr;



//
// Source, Location
//

struct Location;

struct Source : public Object {
    string fileName;
    llvm::OwningPtr<llvm::MemoryBuffer> buffer;
    llvm::TrackingVH<llvm::MDNode> debugInfo;

    Source(llvm::StringRef fileName);
    Source(llvm::StringRef lineOfCode, int dummy);

    Source(llvm::StringRef fileName, llvm::MemoryBuffer *buffer)
        : Object(SOURCE), fileName(fileName), buffer(buffer), debugInfo(NULL)
    { }

    const char *data() const { return buffer->getBufferStart(); }
    const char *endData() const { return buffer->getBufferEnd(); }
    size_t size() const { return buffer->getBufferSize(); }

    llvm::DIFile getDebugInfo() const { return llvm::DIFile(debugInfo); }
};

struct Location {
    SourcePtr source;
    unsigned offset;

    Location()
        : source(NULL), offset(0) {}
    Location(const SourcePtr &source, unsigned offset)
        : source(source), offset(offset) {}

    bool ok() const { return source != NULL; }
};



//
// error module
//

struct CompileContextEntry {
    vector<ObjectPtr> params;
    vector<unsigned> dispatchIndices;

    Location location;

    ObjectPtr callable;

    bool hasParams:1;

    CompileContextEntry(ObjectPtr callable)
        : callable(callable), hasParams(false) {}

    CompileContextEntry(ObjectPtr callable, llvm::ArrayRef<ObjectPtr> params)
        : params(params), callable(callable), hasParams(true) {}

    CompileContextEntry(ObjectPtr callable,
                        llvm::ArrayRef<ObjectPtr> params,
                        llvm::ArrayRef<unsigned> dispatchIndices)
        : params(params), dispatchIndices(dispatchIndices), callable(callable), hasParams(true) {}
};

void pushCompileContext(ObjectPtr obj);
void pushCompileContext(ObjectPtr obj, llvm::ArrayRef<ObjectPtr> params);
void pushCompileContext(ObjectPtr obj,
                        llvm::ArrayRef<ObjectPtr> params,
                        llvm::ArrayRef<unsigned> dispatchIndices);
void popCompileContext();
vector<CompileContextEntry> getCompileContext();
void setCompileContext(llvm::ArrayRef<CompileContextEntry> x);

struct PVData;

struct CompileContextPusher {
    CompileContextPusher(ObjectPtr obj) {
        pushCompileContext(obj);
    }
    CompileContextPusher(ObjectPtr obj, llvm::ArrayRef<ObjectPtr> params) {
        pushCompileContext(obj, params);
    }
    CompileContextPusher(
            ObjectPtr obj,
            llvm::ArrayRef<PVData> params,
            llvm::ArrayRef<unsigned> dispatchIndices = llvm::ArrayRef<unsigned>());
    ~CompileContextPusher() {
        popCompileContext();
    }
};

void pushLocation(Location const &location);
void popLocation();
Location topLocation();

struct LocationContext {
    Location loc;
    LocationContext(Location const &loc)
        : loc(loc) {
        if (loc.ok()) pushLocation(loc);
    }
    ~LocationContext() {
        if (loc.ok())
            popLocation();
    }
private :
    LocationContext(const LocationContext &) {}
    void operator=(const LocationContext &) {}
};

void getLineCol(Location const &location,
                unsigned &line,
                unsigned &column,
                unsigned &tabColumn);

llvm::DIFile getDebugLineCol(Location const &location, unsigned &line, unsigned &column);

void printFileLineCol(llvm::raw_ostream &out, Location const &location);

void invalidStaticObjectError(ObjectPtr obj);
void argumentInvalidStaticObjectError(unsigned index, ObjectPtr obj);

struct DebugPrinter {
    static int indent;
    const ObjectPtr obj;
    DebugPrinter(ObjectPtr obj);
    ~DebugPrinter();
};

extern "C" void displayCompileContext();


//
// AST
//

static llvm::BumpPtrAllocator *ANodeAllocator = new llvm::BumpPtrAllocator();

struct ANode : public Object {
    Location location;
    ANode(ObjectKind objKind)
        : Object(objKind) {}
    void *operator new(size_t num_bytes) {
        return ANodeAllocator->Allocate(num_bytes, llvm::AlignOf<ANode>::Alignment);
    }
    void operator delete(void* anode) {
        ANodeAllocator->Deallocate(anode);
    }
};

struct Identifier : public ANode {
    const llvm::SmallString<16> str;
    bool isOperator:1;
    
    Identifier(llvm::StringRef str)
        : ANode(IDENTIFIER), str(str), isOperator(false) {}
    Identifier(llvm::StringRef str, bool isOperator)
        : ANode(IDENTIFIER), str(str), isOperator(isOperator) {}

    static map<llvm::StringRef, IdentifierPtr> freeIdentifiers; // in parser.cpp
    
    static Identifier *get(llvm::StringRef str, bool isOperator = false) {
        map<llvm::StringRef, IdentifierPtr>::const_iterator iter = freeIdentifiers.find(str);
        if (iter == freeIdentifiers.end()) {
            Identifier *ident = new Identifier(str, isOperator);
            freeIdentifiers[ident->str] = ident;
            return ident;
        } else
            return iter->second.ptr();
    }
    
    static Identifier *get(llvm::StringRef str, Location const &location, bool isOperator = false) {
        Identifier *ident = new Identifier(str, isOperator);
        ident->location = location;
        return ident;
    }
};

struct DottedName : public ANode {
    llvm::SmallVector<IdentifierPtr, 2> parts;
    DottedName()
        : ANode(DOTTED_NAME) {}
    DottedName(llvm::ArrayRef<IdentifierPtr> parts)
        : ANode(DOTTED_NAME), parts(parts.begin(), parts.end()) {}

    string join() const;
};




//
// Expr
//

#define EXPR_KIND_MAP(XX) \
    XX(BOOL_LITERAL) \
    XX(INT_LITERAL) \
    XX(FLOAT_LITERAL) \
    XX(CHAR_LITERAL) \
    XX(STRING_LITERAL) \
\
    XX(FILE_EXPR) \
    XX(LINE_EXPR) \
    XX(COLUMN_EXPR) \
    XX(ARG_EXPR) \
\
    XX(NAME_REF) \
    XX(TUPLE) \
    XX(PAREN) \
    XX(INDEXING) \
    XX(CALL) \
    XX(FIELD_REF) \
    XX(STATIC_INDEXING) \
    XX(VARIADIC_OP) \
    XX(AND) \
    XX(OR) \
\
    XX(LAMBDA) \
    XX(UNPACK) \
    XX(STATIC_EXPR) \
    XX(DISPATCH_EXPR) \
\
    XX(FOREIGN_EXPR) \
    XX(OBJECT_EXPR) \
\
    XX(EVAL_EXPR) \

#define EXPR_KIND_GEN(e) e,
enum ExprKind {
    EXPR_KIND_MAP(EXPR_KIND_GEN)
};
#undef EXPR_KIND_GEN

llvm::raw_ostream &operator<<(llvm::raw_ostream &os, ExprKind);

struct Expr : public ANode {
    const ExprKind exprKind;
    Location startLocation;
    Location endLocation;

    MultiPValuePtr cachedAnalysis;

    Expr(ExprKind exprKind)
        : ANode(EXPRESSION), exprKind(exprKind) {}

    string asString() {
        if (!startLocation.ok() || !endLocation.ok())
            return "<generated expression>";
        assert(startLocation.source == endLocation.source);
        const char *data = startLocation.source->data();
        return string(data + startLocation.offset, data + endLocation.offset);
    }
};

struct BoolLiteral : public Expr {
    const bool value;
    BoolLiteral(bool value)
        : Expr(BOOL_LITERAL), value(value) {}
};

struct IntLiteral : public Expr {
    const string value;
    const string suffix;
    IntLiteral(llvm::StringRef value)
        : Expr(INT_LITERAL), value(value) {}
    IntLiteral(llvm::StringRef value, llvm::StringRef suffix)
        : Expr(INT_LITERAL), value(value), suffix(suffix) {}
};

struct FloatLiteral : public Expr {
    const string value;
    const string suffix;
    FloatLiteral(llvm::StringRef value)
        : Expr(FLOAT_LITERAL), value(value) {}
    FloatLiteral(llvm::StringRef value, llvm::StringRef suffix)
        : Expr(FLOAT_LITERAL), value(value), suffix(suffix) {}
};

struct CharLiteral : public Expr {
    ExprPtr desugared;
    const char value;
    CharLiteral(char value)
        : Expr(CHAR_LITERAL), value(value) {}
};

struct StringLiteral : public Expr {
    const IdentifierPtr value;
    StringLiteral(IdentifierPtr value)
        : Expr(STRING_LITERAL), value(value) {}
};

struct NameRef : public Expr {
    const IdentifierPtr name;
    NameRef(IdentifierPtr name)
        : Expr(NAME_REF), name(name) {}
};

// `__FILE__`
struct FILEExpr : public Expr {
    FILEExpr()
        : Expr(FILE_EXPR) {}
};

// `__LINE__`
struct LINEExpr : public Expr {
    LINEExpr()
        : Expr(LINE_EXPR) {}
};

// `__COLUMN__`
struct COLUMNExpr : public Expr {
    COLUMNExpr()
        : Expr(COLUMN_EXPR) {}
};

// `__ARG__`
struct ARGExpr : public Expr {
    IdentifierPtr name;
    ARGExpr(IdentifierPtr name)
        : Expr(ARG_EXPR), name(name) {}
};

// expression like `[foo, bar]`
struct Tuple : public Expr {
    // `foo, bar`
    ExprListPtr args;
    Tuple(ExprListPtr args)
        : Expr(TUPLE), args(args) {}
};

// parenthesized expression, like `(foo)`
struct Paren : public Expr {
    // `foo`
    const ExprListPtr args;
    Paren(ExprListPtr args)
        : Expr(PAREN), args(args) {}
};

// indexing is expression like `foo[bar, baz]`
struct Indexing : public Expr {
    // `foo`
    ExprPtr expr;
    // `bar, baz`
    const ExprListPtr args;
    Indexing(ExprPtr expr, ExprListPtr args)
        : Expr(INDEXING), expr(expr), args(args) {}
};

struct Call : public Expr {
    ExprPtr expr;
    const ExprListPtr parenArgs;

    ExprListPtr _allArgs;

    ExprListPtr allArgs();

    Call(ExprPtr expr, ExprListPtr parenArgs)
        : Expr(CALL), expr(expr), parenArgs(parenArgs),
          _allArgs(NULL) {}
};

struct FieldRef : public Expr {
    ExprPtr expr;
    const IdentifierPtr name;
    ExprPtr desugared;
    bool isDottedModuleName;
    FieldRef(ExprPtr expr, IdentifierPtr name)
        : Expr(FIELD_REF), expr(expr), name(name), isDottedModuleName(false) {}
};

// static indexing, like `foo.12`
struct StaticIndexing : public Expr {
    // `foo`
    ExprPtr expr;
    // `12`
    const size_t index;
    ExprPtr desugared;
    StaticIndexing(ExprPtr expr, size_t index)
        : Expr(STATIC_INDEXING), expr(expr), index(index) {}
};

#define VARIADIC_OP_KIND_MAP(XX) \
    XX(DEREFERENCE) \
    XX(ADDRESS_OF) \
    XX(NOT) \
    XX(PREFIX_OP) \
    XX(INFIX_OP) \
    XX(IF_EXPR) \

#define VARIADIC_OP_KIND_GEN(e) e,
enum VariadicOpKind {
    VARIADIC_OP_KIND_MAP(VARIADIC_OP_KIND_GEN)
};
#undef VARIADIC_OP_KIND_GEN

llvm::raw_ostream &operator<<(llvm::raw_ostream &os, VariadicOpKind);

struct VariadicOp : public Expr {
    const VariadicOpKind op;
    const ExprListPtr exprs;
    ExprPtr desugared;
    VariadicOp(VariadicOpKind op, ExprListPtr exprs )
        : Expr(VARIADIC_OP), op(op), exprs(exprs) {}
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

enum ProcedureMonoState {
    Procedure_NoOverloads,
    Procedure_MonoOverload,
    Procedure_PolyOverload
};

struct ProcedureMono {
    ProcedureMonoState monoState;
    vector<TypePtr> monoTypes;

    ProcedureMono() : monoState(Procedure_NoOverloads) {}
};

enum LambdaCapture {
    REF_CAPTURE,
    VALUE_CAPTURE,
    STATELESS
};

struct Lambda : public Expr {
    const LambdaCapture captureBy;
    const vector<FormalArgPtr> formalArgs;

    const StatementPtr body;

    ExprPtr converted;

    vector<string> freeVars;

    ProcedureMono mono;

    // if freevars are present
    RecordDeclPtr lambdaRecord;
    TypePtr lambdaType;

    // if freevars are absent
    ProcedurePtr lambdaProc;

    const bool hasVarArg:1;
    const bool hasAsConversion:1;
    bool initialized:1;

    Lambda(LambdaCapture captureBy,
           llvm::ArrayRef<FormalArgPtr> formalArgs,
           bool hasVarArg, bool hasAsConversion, StatementPtr body)
        : Expr(LAMBDA), captureBy(captureBy),
          formalArgs(formalArgs), body(body),
          hasVarArg(hasVarArg), hasAsConversion(hasAsConversion),
          initialized(false) {}
};

// unpack, like `..foo()`
struct Unpack : public Expr {
    ExprPtr expr;
    Unpack(ExprPtr expr) :
        Expr(UNPACK), expr(expr) {}
};

// static, like `#foo`
struct StaticExpr : public Expr {
    // `foo`
    ExprPtr expr;
    StaticExpr(ExprPtr expr) :
        Expr(STATIC_EXPR), expr(expr) {}
};

// variant dispatch, as `*foo` in `bar(*foo)`
struct DispatchExpr : public Expr {
    // `foo`
    ExprPtr expr;
    DispatchExpr(ExprPtr expr) :
        Expr(DISPATCH_EXPR), expr(expr) {}
};

// synthetic expression that is evaluated in specified env or module
struct ForeignExpr : public Expr {
    const string moduleName;
    EnvPtr foreignEnv;
    const ExprPtr expr;

    ForeignExpr(llvm::StringRef moduleName, ExprPtr expr)
        : Expr(FOREIGN_EXPR), moduleName(moduleName), expr(expr) {}
    ForeignExpr(EnvPtr foreignEnv, ExprPtr expr)
        : Expr(FOREIGN_EXPR), foreignEnv(foreignEnv), expr(expr) {}

    EnvPtr getEnv();
};

// synthetic expression that evaluates to specified object
struct ObjectExpr : public Expr {
    const ObjectPtr obj;
    ObjectExpr(ObjectPtr obj)
        : Expr(OBJECT_EXPR), obj(obj) {}
};

struct EvalExpr : public Expr {
    // expression that must evaluate to string
    ExprPtr args;
    // result of evaluation, lazy
    ExprListPtr value;

    EvalExpr(ExprPtr args)
        : Expr(EVAL_EXPR), args(args) {}
};



//
// ExprList
//

struct ExprList : public Object {
    vector<ExprPtr> exprs;

    MultiPValuePtr cachedAnalysis;

    ExprList()
        : Object(EXPR_LIST) {}
    ExprList(ExprPtr x)
        : Object(EXPR_LIST) {
        exprs.push_back(x);
    }
    ExprList(llvm::ArrayRef<ExprPtr> exprs)
        : Object(EXPR_LIST), exprs(exprs) {}
    size_t size() const { return exprs.size(); }
    void add(ExprPtr x) { exprs.push_back(x); }
    void add(ExprListPtr x) {
        exprs.insert(exprs.end(), x->exprs.begin(), x->exprs.end());
    }
    void insert(ExprPtr x) {
        exprs.insert(exprs.begin(), x);
    }
};

inline ExprListPtr Call::allArgs() {
    if (_allArgs == NULL) {
        _allArgs = new ExprList();
        _allArgs->add(parenArgs);
    }
    return _allArgs;
}


//
// Stmt
//

#define STATEMENT_KIND_MAP(XX) \
    XX(BLOCK) \
    XX(LABEL) \
    XX(BINDING) \
    XX(ASSIGNMENT) \
    XX(INIT_ASSIGNMENT) \
    XX(VARIADIC_ASSIGNMENT) \
    XX(GOTO) \
    XX(RETURN) \
    XX(IF) \
    XX(SWITCH) \
    XX(EXPR_STATEMENT) \
    XX(WHILE) \
    XX(BREAK) \
    XX(CONTINUE) \
    XX(FOR) \
    XX(FOREIGN_STATEMENT) \
    XX(TRY) \
    XX(THROW) \
    XX(STATIC_FOR) \
    XX(FINALLY) \
    XX(ONERROR) \
    XX(UNREACHABLE) \
    XX(EVAL_STATEMENT) \
    XX(STATIC_ASSERT_STATEMENT) \

#define STATEMENT_KIND_GEN(e) e,
enum StatementKind {
    STATEMENT_KIND_MAP(STATEMENT_KIND_GEN)
};
#undef STATEMENT_KIND_GEN

llvm::raw_ostream &operator<<(llvm::raw_ostream &os, StatementKind);

struct Statement : public ANode {
    const StatementKind stmtKind;
    Statement(StatementKind stmtKind)
        : ANode(STATEMENT), stmtKind(stmtKind) {}
};

struct Block : public Statement {
    vector<StatementPtr> statements;
    Block()
        : Statement(BLOCK) {}
    Block(llvm::ArrayRef<StatementPtr> statements)
        : Statement(BLOCK), statements(statements) {}
};

struct Label : public Statement {
    const IdentifierPtr name;
    Label(IdentifierPtr name)
        : Statement(LABEL), name(name) {}
};

#define BINDING_KIND_MAP(XX) \
    XX(VAR) \
    XX(REF) \
    XX(ALIAS) \
    XX(FORWARD) \

#define BINDING_KIND_GEN(e) e,
enum BindingKind {
    BINDING_KIND_MAP(BINDING_KIND_GEN)
};
#undef BINDING_KIND_GEN

llvm::raw_ostream &operator<<(llvm::raw_ostream &os, BindingKind);


struct PatternVar;

struct Binding : public Statement {
    const BindingKind bindingKind;
    vector<PatternVar> patternVars;
    vector<ObjectPtr> patternTypes;
    const ExprPtr predicate;
    vector<FormalArgPtr> args;
    const ExprListPtr values;
    const bool hasVarArg;
    Binding(BindingKind bindingKind,
        llvm::ArrayRef<FormalArgPtr> args,
        ExprListPtr values)
        : Statement(BINDING), bindingKind(bindingKind),
          args(args), values(values), hasVarArg(false) {}
    Binding(BindingKind bindingKind,
        llvm::ArrayRef<PatternVar> patternVars,
        llvm::ArrayRef<ObjectPtr> patternTypes,
        ExprPtr predicate,
        llvm::ArrayRef<FormalArgPtr> args,
        ExprListPtr values, bool hasVarArg)
        : Statement(BINDING), bindingKind(bindingKind),
          patternVars(patternVars),
          predicate(predicate),
          args(args), values(values), hasVarArg(hasVarArg) {}
};

struct Assignment : public Statement {
    const ExprListPtr left, right;
    Assignment(ExprListPtr left,
               ExprListPtr right)
        : Statement(ASSIGNMENT), left(left), right(right) {}
};

struct InitAssignment : public Statement {
    const ExprListPtr left, right;
    InitAssignment(ExprListPtr left,
                   ExprListPtr right)
        : Statement(INIT_ASSIGNMENT), left(left), right(right) {}
    InitAssignment(ExprPtr leftExpr, ExprPtr rightExpr)
        : Statement(INIT_ASSIGNMENT),
          left(new ExprList(leftExpr)),
          right(new ExprList(rightExpr)) {}
};

struct VariadicAssignment : public Statement {
    const ExprListPtr exprs;
    ExprPtr desugared;
    VariadicOpKind op;
    VariadicAssignment(VariadicOpKind op, ExprListPtr exprs )
        : Statement(VARIADIC_ASSIGNMENT), exprs(exprs), op(op) {}
};

struct Goto : public Statement {
    const IdentifierPtr labelName;
    Goto(IdentifierPtr labelName)
        : Statement(GOTO), labelName(labelName) {}
};

enum ReturnKind {
    RETURN_VALUE,
    RETURN_REF,
    RETURN_FORWARD
};

struct Return : public Statement {
    const ExprListPtr values;
    const ReturnKind returnKind:3;
    bool isExprReturn:1;
    bool isReturnSpecs:1;
    Return(ReturnKind returnKind, ExprListPtr values)
        : Statement(RETURN), values(values),
          returnKind(returnKind), isExprReturn(false), isReturnSpecs(false) {}
    Return(ReturnKind returnKind, ExprListPtr values, bool exprRet)
        : Statement(RETURN), values(values), 
          returnKind(returnKind), isExprReturn(exprRet), isReturnSpecs(false) {}
};

struct If : public Statement {
    vector<StatementPtr> conditionStatements;
    ExprPtr condition;

    StatementPtr thenPart, elsePart;

    If(ExprPtr condition, StatementPtr thenPart)
        : Statement(IF), condition(condition), thenPart(thenPart) {}
    If(ExprPtr condition, StatementPtr thenPart, StatementPtr elsePart)
        : Statement(IF), condition(condition), thenPart(thenPart),
          elsePart(elsePart) {}
    If(llvm::ArrayRef<StatementPtr> conditionStatements,
        ExprPtr condition, StatementPtr thenPart)
        : Statement(IF), conditionStatements(conditionStatements),
          condition(condition), thenPart(thenPart) {}
    If(llvm::ArrayRef<StatementPtr> conditionStatements,
        ExprPtr condition, StatementPtr thenPart, StatementPtr elsePart)
        : Statement(IF), conditionStatements(conditionStatements),
          condition(condition), thenPart(thenPart),
          elsePart(elsePart) {}
};

struct Switch : public Statement {
    vector<StatementPtr> exprStatements;
    ExprPtr expr;
    vector<CaseBlockPtr> caseBlocks;
    StatementPtr defaultCase;

    StatementPtr desugared;

    Switch(llvm::ArrayRef<StatementPtr> exprStatements,
           ExprPtr expr,
           llvm::ArrayRef<CaseBlockPtr> caseBlocks,
           StatementPtr defaultCase)
        : Statement(SWITCH), exprStatements(exprStatements),
          expr(expr), caseBlocks(caseBlocks),
          defaultCase(defaultCase) {}
    Switch(ExprPtr expr,
           llvm::ArrayRef<CaseBlockPtr> caseBlocks,
           StatementPtr defaultCase)
        : Statement(SWITCH), expr(expr), caseBlocks(caseBlocks),
          defaultCase(defaultCase) {}
};

struct CaseBlock : public ANode {
    const ExprListPtr caseLabels;
    const StatementPtr body;
    CaseBlock(ExprListPtr caseLabels, StatementPtr body)
        : ANode(CASE_BLOCK), caseLabels(caseLabels), body(body) {}
};

struct ExprStatement : public Statement {
    ExprPtr expr;
    ExprStatement(ExprPtr expr)
        : Statement(EXPR_STATEMENT), expr(expr) {}
};

struct While : public Statement {
    const vector<StatementPtr> conditionStatements;
    ExprPtr condition;

    const StatementPtr body;

    While(ExprPtr condition, StatementPtr body)
        : Statement(WHILE), condition(condition), body(body) {}
    While(llvm::ArrayRef<StatementPtr> conditionStatements,
        ExprPtr condition, StatementPtr body)
        : Statement(WHILE), conditionStatements(conditionStatements),
          condition(condition), body(body) {}
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
    const vector<IdentifierPtr> variables;
    ExprPtr expr;
    const StatementPtr body;
    StatementPtr desugared;
    For(llvm::ArrayRef<IdentifierPtr> variables,
        ExprPtr expr,
        StatementPtr body)
        : Statement(FOR), variables(variables), expr(expr), body(body) {}
};

struct ForeignStatement : public Statement {
    const string moduleName;
    EnvPtr foreignEnv;
    const StatementPtr statement;
    ForeignStatement(llvm::StringRef moduleName, StatementPtr statement)
        : Statement(FOREIGN_STATEMENT), moduleName(moduleName),
          statement(statement) {}
    ForeignStatement(EnvPtr foreignEnv, StatementPtr statement)
        : Statement(FOREIGN_STATEMENT), foreignEnv(foreignEnv),
          statement(statement) {}
    ForeignStatement(llvm::StringRef moduleName, EnvPtr foreignEnv,
                     StatementPtr statement)
        : Statement(FOREIGN_STATEMENT), moduleName(moduleName),
          foreignEnv(foreignEnv), statement(statement) {}

    EnvPtr getEnv();
};

struct Try : public Statement {
    const StatementPtr tryBlock;
    const vector<CatchPtr> catchBlocks;
    StatementPtr desugaredCatchBlock;

    Try(StatementPtr tryBlock,
        vector<CatchPtr> catchBlocks)
        : Statement(TRY), tryBlock(tryBlock),
          catchBlocks(catchBlocks) {}
};

struct Catch : public ANode {
    const IdentifierPtr exceptionVar;
    ExprPtr exceptionType; // optional
    const IdentifierPtr contextVar;
    const StatementPtr body;

    Catch(IdentifierPtr exceptionVar,
          ExprPtr exceptionType,
          IdentifierPtr contextVar,
          StatementPtr body)
        : ANode(CATCH), exceptionVar(exceptionVar),
          exceptionType(exceptionType), contextVar(contextVar), body(body) {}
};

struct Throw : public Statement {
    const ExprPtr expr;
    const ExprPtr context;
    ExprPtr desugaredExpr;
    ExprPtr desugaredContext;
    Throw(ExprPtr expr, ExprPtr context)
        : Statement(THROW), expr(expr), context(context) {}
};

struct StaticFor : public Statement {
    const IdentifierPtr variable;
    const ExprListPtr values;
    const StatementPtr body;

    bool clonesInitialized;
    vector<StatementPtr> clonedBodies;

    StaticFor(IdentifierPtr variable,
              ExprListPtr values,
              StatementPtr body)
        : Statement(STATIC_FOR), variable(variable), values(values),
          body(body), clonesInitialized(false) {}
};

struct Finally : public Statement {
    const StatementPtr body;

    explicit Finally(StatementPtr body) : Statement(FINALLY), body(body) {}
};

struct OnError : public Statement {
    const StatementPtr body;

    explicit OnError(StatementPtr body) : Statement(ONERROR), body(body) {}
};

struct Unreachable : public Statement {
    Unreachable()
        : Statement(UNREACHABLE) {}
};

struct EvalStatement : public Statement {
    ExprListPtr args;
    vector<StatementPtr> value;
    bool evaled:1;

    EvalStatement(ExprListPtr args)
        : Statement(EVAL_STATEMENT), args(args), evaled(false) {}
};

struct StaticAssertStatement : public Statement {
    const ExprPtr cond;
    const ExprListPtr message;

    StaticAssertStatement(ExprPtr cond, ExprListPtr message)
        : Statement(STATIC_ASSERT_STATEMENT), cond(cond), message(message)
    {}
};

//
// Code
//

enum ValueTempness {
    TEMPNESS_DONTCARE,
    TEMPNESS_LVALUE,
    TEMPNESS_RVALUE,
    TEMPNESS_FORWARD
};

struct FormalArg : public ANode {
    const IdentifierPtr name;
    ExprPtr type;
    ValueTempness tempness;
    ExprPtr asType;
    bool varArg:1;
    FormalArg(IdentifierPtr name, ExprPtr type)
        : ANode(FORMAL_ARG), name(name), type(type),
          tempness(TEMPNESS_DONTCARE), varArg(false) {}
    FormalArg(IdentifierPtr name, ExprPtr type, ValueTempness tempness)
        : ANode(FORMAL_ARG), name(name), type(type),
          tempness(tempness), varArg(false) {}
    FormalArg(IdentifierPtr name, ExprPtr type, ValueTempness tempness, bool varArg)
        : ANode(FORMAL_ARG), name(name), type(type),
          tempness(tempness), varArg(varArg) {}
    FormalArg(IdentifierPtr name, ExprPtr type, ValueTempness tempness, ExprPtr asType)
        : ANode(FORMAL_ARG), name(name), type(type),
          tempness(tempness), asType(asType), varArg(false) {}
    FormalArg(IdentifierPtr name, ExprPtr type, ValueTempness tempness, ExprPtr asType, bool varArg)
        : ANode(FORMAL_ARG), name(name), type(type),
          tempness(tempness), asType(asType), varArg(varArg) {}
};

struct ReturnSpec : public ANode {
    const ExprPtr type;
    const IdentifierPtr name;
    ReturnSpec(ExprPtr type)
        : ANode(RETURN_SPEC), type(type) {}
    ReturnSpec(ExprPtr type, IdentifierPtr name)
        : ANode(RETURN_SPEC), type(type), name(name) {}
};

struct PatternVar {
    IdentifierPtr name;
    bool isMulti:1;
    PatternVar(bool isMulti, IdentifierPtr name)
        : name(name), isMulti(isMulti) {}
    PatternVar()
        : isMulti(false) {}
};

struct LLVMCode : ANode {
    const string body;
    LLVMCode(llvm::StringRef body)
        : ANode(LLVM_CODE), body(body) {}
};

struct Code : public ANode {
    vector<PatternVar> patternVars;
    ExprPtr predicate;
    vector<FormalArgPtr> formalArgs;
    vector<ReturnSpecPtr> returnSpecs;
    ReturnSpecPtr varReturnSpec;
    StatementPtr body;
    LLVMCodePtr llvmBody;
    bool hasVarArg:1;
    bool returnSpecsDeclared:1;

    Code()
        : ANode(CODE),  hasVarArg(false), returnSpecsDeclared(false) {}
    Code(llvm::ArrayRef<PatternVar> patternVars,
         ExprPtr predicate,
         llvm::ArrayRef<FormalArgPtr> formalArgs,
         llvm::ArrayRef<ReturnSpecPtr> returnSpecs,
         ReturnSpecPtr varReturnSpec,
         StatementPtr body)
        : ANode(CODE), patternVars(patternVars), predicate(predicate),
          formalArgs(formalArgs),
          returnSpecs(returnSpecs), varReturnSpec(varReturnSpec),
          body(body),
          hasVarArg(false), returnSpecsDeclared(false)
          {}

    bool hasReturnSpecs() {
        return returnSpecsDeclared;
    }

    bool hasNamedReturns() {
        return hasReturnSpecs() && (
            (!returnSpecs.empty() && returnSpecs[0]->name != NULL)
            || (varReturnSpec != NULL && varReturnSpec->name != NULL));
    }

    bool isLLVMBody() {
        return llvmBody.ptr() != NULL;
    }
    bool hasBody() {
        return body.ptr() || isLLVMBody();
    }
};



//
// Visibility
//

enum Visibility {
    VISIBILITY_UNDEFINED,
    PUBLIC,
    PRIVATE
};



//
// TopLevelItem
//

struct TopLevelItem : public ANode {
    Module * const module;
    IdentifierPtr name; // for named top level items
    const Visibility visibility; // valid only if name != NULL

    EnvPtr env;

    TopLevelItem(ObjectKind objKind, Module *module, Visibility visibility = VISIBILITY_UNDEFINED)
        : ANode(objKind), module(module), visibility(visibility) {}
    TopLevelItem(ObjectKind objKind, Module *module, IdentifierPtr name, Visibility visibility)
        : ANode(objKind), module(module), name(name), visibility(visibility) {}
};

struct RecordDecl : public TopLevelItem {
    const vector<PatternVar> patternVars;
    const ExprPtr predicate;

    const vector<IdentifierPtr> params;

    const IdentifierPtr varParam;

    RecordBodyPtr body;

    vector<OverloadPtr> overloads;

    LambdaPtr lambda;

    bool builtinOverloadInitialized:1;

    RecordDecl(Module *module, Visibility visibility)
        : TopLevelItem(RECORD_DECL, module, visibility),
          builtinOverloadInitialized(false) {}
    RecordDecl(Module *module, Visibility visibility,
           llvm::ArrayRef<PatternVar> patternVars, ExprPtr predicate,
           llvm::ArrayRef<IdentifierPtr> params, IdentifierPtr varParam,
           RecordBodyPtr body)
        : TopLevelItem(RECORD_DECL, module, visibility),
          patternVars(patternVars),
          predicate(predicate),
          params(params),
          varParam(varParam),
          body(body),
          builtinOverloadInitialized(false) {}
    RecordDecl(Module *module, IdentifierPtr name,
           Visibility visibility,
           llvm::ArrayRef<PatternVar> patternVars,
           ExprPtr predicate,
           llvm::ArrayRef<IdentifierPtr> params,
           IdentifierPtr varParam,
           RecordBodyPtr body)
        : TopLevelItem(RECORD_DECL, module, name, visibility),
          patternVars(patternVars), predicate(predicate),
          params(params), varParam(varParam), body(body),
          builtinOverloadInitialized(false) {}
};

struct RecordBody : public ANode {
    ExprListPtr computed; // valid if isComputed == true
    const vector<RecordFieldPtr> fields; // valid if isComputed == false
    bool isComputed:1;
    const bool hasVarField:1;
    RecordBody(ExprListPtr computed)
        : ANode(RECORD_BODY), computed(computed), isComputed(true),
        hasVarField(false) {}
    RecordBody(llvm::ArrayRef<RecordFieldPtr> fields)
        : ANode(RECORD_BODY), fields(fields), isComputed(false), 
        hasVarField(false) {}
    RecordBody(llvm::ArrayRef<RecordFieldPtr> fields, bool hasVarField)
        : ANode(RECORD_BODY), fields(fields), isComputed(false), 
        hasVarField(hasVarField) {}
};

struct RecordField : public ANode {
    const IdentifierPtr name;
    const ExprPtr type;
    bool varField:1;
    RecordField(IdentifierPtr name, ExprPtr type)
        : ANode(RECORD_FIELD), name(name), type(type), varField(false) {}
};

struct VariantDecl : public TopLevelItem {
    const vector<PatternVar> patternVars;
    const ExprPtr predicate;

    const vector<IdentifierPtr> params;
    const IdentifierPtr varParam;

    const ExprListPtr defaultInstances;

    vector<InstanceDeclPtr> instances;

    vector<OverloadPtr> overloads;

    const bool open:1;

    VariantDecl(Module *module, IdentifierPtr name,
            Visibility visibility,
            llvm::ArrayRef<PatternVar> patternVars,
            ExprPtr predicate,
            llvm::ArrayRef<IdentifierPtr> params,
            IdentifierPtr varParam,
            bool open,
            ExprListPtr defaultInstances)
        : TopLevelItem(VARIANT_DECL, module, name, visibility),
          patternVars(patternVars), predicate(predicate),
          params(params), varParam(varParam),
          defaultInstances(defaultInstances),
          open(open) {}
};

struct InstanceDecl : public TopLevelItem {
    const vector<PatternVar> patternVars;
    const ExprPtr predicate;
    const ExprPtr target;
    const ExprListPtr members;

    InstanceDecl(Module *module, llvm::ArrayRef<PatternVar> patternVars,
             ExprPtr predicate,
             ExprPtr target,
             ExprListPtr members)
        : TopLevelItem(INSTANCE_DECL, module), patternVars(patternVars),
          predicate(predicate), target(target), members(members)
        {}
};

enum InlineAttribute {
    IGNORE,
    INLINE,
    FORCE_INLINE,
    NEVER_INLINE
};

struct Overload : public TopLevelItem {
    const ExprPtr target;
    const CodePtr code;

    // pre-computed patterns for matchInvoke
    vector<PatternCellPtr> cells;
    vector<MultiPatternCellPtr> multiCells;
    PatternPtr callablePattern;
    vector<PatternPtr> argPatterns;
    MultiPatternPtr varArgPattern;
    InlineAttribute isInline:3;
    int patternsInitializedState:2; // 0:notinit, -1:initing, +1:inited
    bool callByName:1;
    bool nameIsPattern:1;
    bool hasAsConversion:1;
    bool isDefault:1;

    Overload(Module *module, ExprPtr target,
             CodePtr code,
             bool callByName,
             InlineAttribute isInline)
        : TopLevelItem(OVERLOAD, module), target(target), code(code),
          isInline(isInline), patternsInitializedState(0),
          callByName(callByName), nameIsPattern(false), 
          hasAsConversion(false), isDefault(false) {}
    Overload(Module *module, ExprPtr target,
             CodePtr code,
             bool callByName,
             InlineAttribute isInline,
             bool hasAsConversion)
        : TopLevelItem(OVERLOAD, module), target(target), code(code),
          isInline(isInline), patternsInitializedState(0),
          callByName(callByName), nameIsPattern(false), 
          hasAsConversion(hasAsConversion), isDefault(false) {}
};

struct Procedure : public TopLevelItem {
    const OverloadPtr interface;

    OverloadPtr singleOverload;
    vector<OverloadPtr> overloads;
    ObjectTablePtr evaluatorCache; // HACK: used only for predicates
    ProcedureMono mono;
    LambdaPtr lambda;

    const bool privateOverload:1;

    Procedure(Module *module, IdentifierPtr name, Visibility visibility, bool privateOverload);
    Procedure(Module *module, IdentifierPtr name, Visibility visibility, bool privateOverload, OverloadPtr interface);
    ~Procedure();
};

struct IntrinsicInstance {
    llvm::Function *function;
    MultiPValuePtr outputTypes;
    std::string error;
};

struct IntrinsicSymbol : public TopLevelItem {
    llvm::Intrinsic::ID id;
    map<vector<TypePtr>, IntrinsicInstance> instances;
    
    IntrinsicSymbol(Module *module, IdentifierPtr name, llvm::Intrinsic::ID id)
        : TopLevelItem(INTRINSIC, module, name, PUBLIC), id(id) {}
};

struct NewTypeDecl : public TopLevelItem {
    const ExprPtr expr;
    NewTypePtr type;
    TypePtr baseType;
    bool initialized:1;
    NewTypeDecl(Module *module, IdentifierPtr name,
            Visibility visibility,
            ExprPtr expr)
        : TopLevelItem(NEW_TYPE_DECL, module, name, visibility), expr(expr),
        initialized(false) {}
    
    
};

struct EnumDecl : public TopLevelItem {
    const vector<PatternVar> patternVars;
    const ExprPtr predicate;

    vector<EnumMemberPtr> members;
    TypePtr type;
    EnumDecl(Module *module, IdentifierPtr name, Visibility visibility,
        llvm::ArrayRef<PatternVar> patternVars, ExprPtr predicate)
        : TopLevelItem(ENUM_DECL, module, name, visibility),
          patternVars(patternVars), predicate(predicate) {}
    EnumDecl(Module *module, IdentifierPtr name, Visibility visibility,
                llvm::ArrayRef<PatternVar> patternVars, ExprPtr predicate,
                llvm::ArrayRef<EnumMemberPtr> members)
        : TopLevelItem(ENUM_DECL, module, name, visibility),
          patternVars(patternVars), predicate(predicate),
          members(members) {}
};

struct EnumMember : public ANode {
    const IdentifierPtr name;
    TypePtr type;
    int index;
    EnumMember(IdentifierPtr name)
        : ANode(ENUM_MEMBER), name(name), index(-1) {}
};

struct GlobalVariable : public TopLevelItem {
    const vector<PatternVar> patternVars;
    const ExprPtr predicate;

    const vector<IdentifierPtr> params;
    const IdentifierPtr varParam;
    const ExprPtr expr;

    ObjectTablePtr instances;

    GlobalVariable(Module *module, IdentifierPtr name,
                   Visibility visibility,
                   llvm::ArrayRef<PatternVar> patternVars,
                   ExprPtr predicate,
                   llvm::ArrayRef<IdentifierPtr> params,
                   IdentifierPtr varParam,
                   ExprPtr expr);

    ~GlobalVariable();

    bool hasParams() const {
        return !params.empty() || (varParam.ptr() != NULL);
    }
};

struct GVarInstance : public RefCounted {
    const GlobalVariablePtr gvar;
    const vector<ObjectPtr> params;

    ExprPtr expr;
    EnvPtr env;
    MultiPValuePtr analysis;
    TypePtr type;
    ValueHolderPtr staticGlobal;
    llvm::GlobalVariable *llGlobal;
    llvm::TrackingVH<llvm::MDNode> debugInfo;

    bool analyzing:1;

    GVarInstance(GlobalVariablePtr gvar,
                 llvm::ArrayRef<ObjectPtr> params);

    ~GVarInstance();

    llvm::DIGlobalVariable getDebugInfo() { return llvm::DIGlobalVariable(debugInfo); }
};

#define CALLING_CONV_MAP(XX) \
    XX(CC_DEFAULT,  "AttributeCCall") \
    XX(CC_STDCALL,  "AttributeStdCall") \
    XX(CC_FASTCALL, "AttributeFastCall") \
    XX(CC_THISCALL, "AttributeThisCall") \
    XX(CC_LLVM,     "AttributeLLVMCall") \

#define CALLING_CONV_GEN(e, str) e,
enum CallingConv {
    CALLING_CONV_MAP(CALLING_CONV_GEN)
    CC_Count
};
#undef CALLING_CONV_GEN


struct ExternalProcedure : public TopLevelItem {
    vector<ExternalArgPtr> args;
    ExprPtr returnType;
    StatementPtr body;
    ExprListPtr attributes;

    llvm::SmallString<16> attrAsmLabel;

    TypePtr returnType2;
    TypePtr ptrType;

    llvm::Function *llvmFunc;
    llvm::TrackingVH<llvm::MDNode> debugInfo;

    CallingConv callingConv:4;
    bool hasVarArgs:1;
    bool attributesVerified:1;
    bool attrDLLImport:1;
    bool attrDLLExport:1;
    bool analyzed:1;
    bool bodyCodegenned:1;

    ExternalProcedure(Module *module, Visibility visibility)
        : TopLevelItem(EXTERNAL_PROCEDURE, module, visibility),
          attributes(new ExprList()),
          llvmFunc(NULL), debugInfo(NULL),
          hasVarArgs(false),
          attributesVerified(false),
          analyzed(false), bodyCodegenned(false)
        {}
    ExternalProcedure(Module *module, IdentifierPtr name,
                      Visibility visibility,
                      llvm::ArrayRef<ExternalArgPtr> args,
                      bool hasVarArgs,
                      ExprPtr returnType,
                      StatementPtr body,
                      ExprListPtr attributes)
        : TopLevelItem(EXTERNAL_PROCEDURE, module, name, visibility), args(args),
          returnType(returnType), body(body),
          attributes(attributes),
          llvmFunc(NULL), debugInfo(NULL),
          hasVarArgs(hasVarArgs),
          attributesVerified(false),
          analyzed(false), bodyCodegenned(false)
        {}

    llvm::DISubprogram getDebugInfo() { return llvm::DISubprogram(debugInfo); }
};

struct ExternalArg : public ANode {
    const IdentifierPtr name;
    const ExprPtr type;
    TypePtr type2;
    ExternalArg(IdentifierPtr name, ExprPtr type)
        : ANode(EXTERNAL_ARG), name(name), type(type) {}
};

struct ExternalVariable : public TopLevelItem {
    ExprPtr type;
    ExprListPtr attributes;

    TypePtr type2;
    llvm::GlobalVariable *llGlobal;
    llvm::TrackingVH<llvm::MDNode> debugInfo;

    bool attributesVerified:1;
    bool attrDLLImport:1;
    bool attrDLLExport:1;

    ExternalVariable(Module *module, Visibility visibility)
        : TopLevelItem(EXTERNAL_VARIABLE, module, visibility),
          attributes(new ExprList()),
          llGlobal(NULL), debugInfo(NULL),
          attributesVerified(false)
          {}
    ExternalVariable(Module *module, IdentifierPtr name,
                     Visibility visibility,
                     ExprPtr type,
                     ExprListPtr attributes)
        : TopLevelItem(EXTERNAL_VARIABLE, module, name, visibility),
          type(type), attributes(attributes),
          llGlobal(NULL), debugInfo(NULL),
          attributesVerified(false)
          {}

    llvm::DIGlobalVariable getDebugInfo() { return llvm::DIGlobalVariable(debugInfo); }
};

struct EvalTopLevel : public TopLevelItem {
    const ExprListPtr args;
    vector<TopLevelItemPtr> value;
    bool evaled:1;

    EvalTopLevel(Module *module, ExprListPtr args)
        : TopLevelItem(EVAL_TOPLEVEL, module), args(args), evaled(false)
        {}
};

struct StaticAssertTopLevel : public TopLevelItem {
    ExprPtr cond;
    ExprListPtr message;

    StaticAssertTopLevel(Module *module, ExprPtr cond, ExprListPtr message)
    : TopLevelItem(STATIC_ASSERT_TOP_LEVEL, module), cond(cond), message(message)
    {}
};


//
// documentation
//

enum DocumentationAnnotation
{
    SectionAnnotation,
    ModuleAnnotation,
    OverloadAnnotation,
    RecordAnnotion,
    InvalidAnnotation
};

struct Documentation : public TopLevelItem {

    const std::map<DocumentationAnnotation, string> annotation;
    const std::string text;
    Documentation(Module *module, const std::map<DocumentationAnnotation, string> &annotation, const std::string &text)
        : TopLevelItem(DOCUMENTATION, module), annotation(annotation), text(text)
        {}
};



//
// GlobalAlias
//

struct GlobalAlias : public TopLevelItem {
    const vector<PatternVar> patternVars;
    const ExprPtr predicate;

    const vector<IdentifierPtr> params;
    const IdentifierPtr varParam;
    const ExprPtr expr;

    vector<OverloadPtr> overloads;

    GlobalAlias(Module *module, IdentifierPtr name,
                Visibility visibility,
                llvm::ArrayRef<PatternVar> patternVars,
                ExprPtr predicate,
                llvm::ArrayRef<IdentifierPtr> params,
                IdentifierPtr varParam,
                ExprPtr expr)
        : TopLevelItem(GLOBAL_ALIAS, module, name, visibility),
          patternVars(patternVars), predicate(predicate),
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
    IMPORT_MEMBERS
};

struct Import : public ANode {
    const ImportKind importKind;
    const DottedNamePtr dottedName;
    Visibility visibility;
    ModulePtr module;
    Import(ImportKind importKind, DottedNamePtr dottedName)
        : ANode(IMPORT), importKind(importKind), dottedName(dottedName),
          visibility(PRIVATE) {}
};

struct ImportModule : public Import {
    const IdentifierPtr alias;
    ImportModule(DottedNamePtr dottedName,
                 IdentifierPtr alias)
        : Import(IMPORT_MODULE, dottedName), alias(alias) {}
};

struct ImportStar : public Import {
    ImportStar(DottedNamePtr dottedName)
        : Import(IMPORT_STAR, dottedName) {}
};

struct ImportedMember {
    Visibility visibility;
    IdentifierPtr name;
    IdentifierPtr alias;
    ImportedMember() {}
    ImportedMember(Visibility visibility, IdentifierPtr name, IdentifierPtr alias)
        : visibility(visibility), name(name), alias(alias) {}
};

struct ImportMembers : public Import {
    vector<ImportedMember> members;
    llvm::StringMap<IdentifierPtr> aliasMap;
    ImportMembers(DottedNamePtr dottedName)
        : Import(IMPORT_MEMBERS, dottedName) {}
};


//
// Module
//

struct ModuleDeclaration : public ANode {
    const DottedNamePtr name;
    const ExprListPtr attributes;

    ModuleDeclaration(DottedNamePtr name, ExprListPtr attributes)
        : ANode(MODULE_DECLARATION), name(name), attributes(attributes)
        {}
};

struct ImportSet {
    llvm::SmallVector<ObjectPtr,2> values;

    size_t size() const { return values.size(); }
    bool empty() const { return values.empty(); }
    ObjectPtr &operator[](size_t i) { return values[(unsigned)i]; }
    ObjectPtr const &operator[](size_t i) const { return values[(unsigned)i]; }
    ObjectPtr const *begin() const { return values.begin(); }
    ObjectPtr const *end() const { return values.end(); }

    void insert(ObjectPtr o) {
        ObjectPtr const *x = std::find(values.begin(), values.end(), o);
        if (x == values.end())
            values.push_back(o);
    }

    void clear() { values.clear(); }
};
    
struct ModuleLookup {
    ModulePtr module;
    llvm::StringMap<ModuleLookup> parents;
    
    ModuleLookup() : module(NULL), parents() {}
    ModuleLookup(Module *m) : module(m), parents() {}
};

struct Module : public ANode {
    enum InitState {
        BEFORE,
        RUNNING,
        DONE
    };

    SourcePtr source;
    string moduleName;
    vector<ImportPtr> imports;
    ModuleDeclarationPtr declaration;
    LLVMCodePtr topLevelLLVM;
    vector<TopLevelItemPtr> topLevelItems;

    llvm::StringMap<ObjectPtr> globals;
    llvm::StringMap<ObjectPtr> publicGlobals;
    
    llvm::StringMap<ModuleLookup> importedModuleNames;

    EnvPtr env;
    InitState initState;
    vector<llvm::SmallString<16> > attrBuildFlags;
    IntegerTypePtr attrDefaultIntegerType;
    FloatTypePtr attrDefaultFloatType;

    llvm::StringMap<ImportSet> publicSymbols;

    llvm::StringMap<ImportSet> allSymbols;

    set<string> importedNames;

    int publicSymbolsLoading; //:3;
    int allSymbolsLoading; //:3;
    bool attributesVerified:1;
    bool publicSymbolsLoaded:1;
    bool allSymbolsLoaded:1;
    bool topLevelLLVMGenerated:1;
    bool externalsGenerated:1;
    bool isIntrinsicsModule:1;

    llvm::TrackingVH<llvm::MDNode> debugInfo;

    Module(llvm::StringRef moduleName)
        : ANode(MODULE), moduleName(moduleName),
          initState(BEFORE),
          publicSymbolsLoading(0),
          allSymbolsLoading(0),
          attributesVerified(false),
          publicSymbolsLoaded(false),
          allSymbolsLoaded(false),
          topLevelLLVMGenerated(false),
          externalsGenerated(false),
          isIntrinsicsModule(false),
          debugInfo(NULL) {}
    Module(llvm::StringRef moduleName,
           llvm::ArrayRef<ImportPtr> imports,
           ModuleDeclarationPtr declaration,
           LLVMCodePtr topLevelLLVM,
           llvm::ArrayRef<TopLevelItemPtr> topLevelItems)
        : ANode(MODULE), moduleName(moduleName), imports(imports),
          declaration(declaration),
          topLevelLLVM(topLevelLLVM), topLevelItems(topLevelItems),
          initState(BEFORE),
          publicSymbolsLoading(0),
          allSymbolsLoading(0),
          attributesVerified(false),
          publicSymbolsLoaded(false),
          allSymbolsLoaded(false),
          topLevelLLVMGenerated(false),
          externalsGenerated(false),
          isIntrinsicsModule(false),
          debugInfo(NULL) {}

    llvm::DINameSpace getDebugInfo() { return llvm::DINameSpace(debugInfo); }
};


//
// analyzer
//

struct PVData {
    TypePtr type;
    bool isRValue:1;
    PVData() : type(NULL), isRValue(false) {}
    PVData(TypePtr type, bool isRValue)
        : type(type), isRValue(isRValue) {}

    bool ok() const { return type != NULL; }

    bool operator==(PVData const &x) const { return type == x.type && isRValue == x.isRValue; }
};

// propagation value
struct PValue : public Object {
    PVData data;
    PValue(TypePtr type, bool isTemp)
        : Object(PVALUE), data(type, isTemp) {}
    PValue(PVData data)
        : Object(PVALUE), data(data) {}
};

struct MultiPValue : public Object {
    llvm::SmallVector<PVData, 4> values;
    MultiPValue()
        : Object(MULTI_PVALUE) {}
    MultiPValue(PVData const &pv)
        : Object(MULTI_PVALUE) {
        values.push_back(pv);
    }
    MultiPValue(llvm::ArrayRef<PVData> values)
        : Object(MULTI_PVALUE), values(values.begin(), values.end()) {}
    size_t size() { return values.size(); }
    void add(PVData const &x) { values.push_back(x); }
    void add(MultiPValuePtr x) {
        values.insert(values.end(), x->values.begin(), x->values.end());
    }

    void toArgsKey(vector<TypePtr> *args)
    {
        for (PVData const *i = values.begin(), *end = values.end();
             i != end;
             ++i)
        {
            args->push_back(i->type);
        }
    }
};


//
// Env
//

struct Env : public Object {
    ObjectPtr parent;
    const bool exceptionAvailable;
    ExprPtr callByNameExprHead;
    llvm::StringMap<ObjectPtr> entries;
    Env()
        : Object(ENV), exceptionAvailable(false) {}
    Env(ModulePtr parent)
        : Object(ENV), parent(parent.ptr()), exceptionAvailable(false) {}
    Env(EnvPtr parent, bool exceptionAvailable = false)
        : Object(ENV), parent(parent.ptr()), exceptionAvailable(exceptionAvailable) {}
};



//
// interactive module
//

void runInteractive(llvm::Module *llvmModule, ModulePtr module);


//
// Types
//

#define TYPE_KIND_MAP(XX) \
    XX(BOOL_TYPE) \
    XX(INTEGER_TYPE) \
    XX(FLOAT_TYPE) \
    XX(COMPLEX_TYPE) \
    XX(POINTER_TYPE) \
    XX(CODE_POINTER_TYPE) \
    XX(CCODE_POINTER_TYPE) \
    XX(ARRAY_TYPE) \
    XX(VEC_TYPE) \
    XX(TUPLE_TYPE) \
    XX(UNION_TYPE) \
    XX(RECORD_TYPE) \
    XX(VARIANT_TYPE) \
    XX(STATIC_TYPE) \
    XX(ENUM_TYPE) \
    XX(NEW_TYPE) \

#define TYPE_KIND_GEN(e) e,
enum TypeKind {
    TYPE_KIND_MAP(TYPE_KIND_GEN)
};
#undef TYPE_KIND_GEN

llvm::raw_ostream &operator<<(llvm::raw_ostream &os, TypeKind);

struct Type : public Object {
    const TypeKind typeKind;
    llvm::Type *llType;
    llvm::TrackingVH<llvm::MDNode> debugInfo;
    size_t typeSize;
    size_t typeAlignment;

    vector<OverloadPtr> overloads;

    bool overloadsInitialized:1;
    bool defined:1;
    bool typeInfoInitialized:1;
    
    Type(TypeKind typeKind)
        : Object(TYPE), typeKind(typeKind),
          llType(NULL), debugInfo(NULL),
          overloadsInitialized(false),
          defined(false),
          typeInfoInitialized(false)
    {}

    void *operator new(size_t num_bytes) {
        return ANodeAllocator->Allocate(num_bytes, llvm::AlignOf<Type>::Alignment);
    }
    void operator delete(void* type) {
        ANodeAllocator->Deallocate(type);
    }
    llvm::DIType getDebugInfo() { return llvm::DIType(debugInfo); }
};

struct BoolType : public Type {
    BoolType() :
        Type(BOOL_TYPE) {}
};

struct IntegerType : public Type {
    const unsigned bits:15;
    const bool isSigned:1;
    IntegerType(unsigned bits, bool isSigned)
        : Type(INTEGER_TYPE), bits(bits), isSigned(isSigned) {}
};

struct FloatType : public Type {
    const unsigned bits:15;
    const bool isImaginary:1;
    FloatType(unsigned bits, bool isImaginary)
        : Type(FLOAT_TYPE), bits(bits), isImaginary(isImaginary){}
};

struct ComplexType : public Type {
    const llvm::StructLayout *layout;
    const unsigned bits:15;
    ComplexType(unsigned bits)
        : Type(COMPLEX_TYPE), layout(NULL), bits(bits) {}
};

struct PointerType : public Type {
    const TypePtr pointeeType;
    PointerType(TypePtr pointeeType)
        : Type(POINTER_TYPE), pointeeType(pointeeType) {}
};

struct CodePointerType : public Type {
    const vector<TypePtr> argTypes;

    const vector<uint8_t> returnIsRef;
    const vector<TypePtr> returnTypes;

    CodePointerType(llvm::ArrayRef<TypePtr> argTypes,
                    llvm::ArrayRef<uint8_t> returnIsRef,
                    llvm::ArrayRef<TypePtr> returnTypes)
        : Type(CODE_POINTER_TYPE), argTypes(argTypes),
          returnIsRef(returnIsRef), returnTypes(returnTypes) {}
};

struct CCodePointerType : public Type {
    vector<TypePtr> argTypes;
    TypePtr returnType; // NULL if void return

    llvm::Type *callType;

    llvm::Type *getCallType();

    const CallingConv callingConv:3;
    const bool hasVarArgs:1;

    CCodePointerType(CallingConv callingConv,
                     llvm::ArrayRef<TypePtr> argTypes,
                     bool hasVarArgs,
                     TypePtr returnType)
        : Type(CCODE_POINTER_TYPE),
          argTypes(argTypes),
          returnType(returnType), callType(NULL),
          callingConv(callingConv), hasVarArgs(hasVarArgs) {}
};

struct ArrayType : public Type {
    const TypePtr elementType;
    const unsigned size;
    ArrayType(TypePtr elementType, unsigned size)
        : Type(ARRAY_TYPE), elementType(elementType), size(size) {}
};

struct VecType : public Type {
    const TypePtr elementType;
    const unsigned size;
    VecType(TypePtr elementType, unsigned size)
        : Type(VEC_TYPE), elementType(elementType), size(size) {}
};

struct TupleType : public Type {
    vector<TypePtr> elementTypes;
    const llvm::StructLayout *layout;
    TupleType()
        : Type(TUPLE_TYPE), layout(NULL) {}
    TupleType(llvm::ArrayRef<TypePtr> elementTypes)
        : Type(TUPLE_TYPE), elementTypes(elementTypes),
          layout(NULL) {}
};

struct UnionType : public Type {
    vector<TypePtr> memberTypes;
    UnionType()
        : Type(UNION_TYPE) {}
    UnionType(llvm::ArrayRef<TypePtr> memberTypes)
        : Type(UNION_TYPE), memberTypes(memberTypes)
        {}
};

struct RecordType : public Type {
    const RecordDeclPtr record;

    const vector<ObjectPtr> params;

    vector<IdentifierPtr> fieldNames;
    vector<TypePtr> fieldTypes;
    
    llvm::StringMap<size_t> fieldIndexMap;

    const llvm::StructLayout *layout;

    unsigned varFieldPosition;
    bool fieldsInitialized:1;
    bool hasVarField:1;
    
    RecordType(RecordDeclPtr record, llvm::ArrayRef<ObjectPtr> params);

    size_t varFieldSize() {
        return fieldTypes.size() - fieldNames.size() + 1;
    }
    size_t fieldCount() {
        return fieldNames.size();
    }
};

struct VariantType : public Type {
    VariantDeclPtr variant;
    vector<ObjectPtr> params;

    vector<TypePtr> memberTypes;
    TypePtr reprType;

    bool initialized:1;

    VariantType(VariantDeclPtr variant)
        : Type(VARIANT_TYPE), variant(variant),
          initialized(false) {}
    VariantType(VariantDeclPtr variant, llvm::ArrayRef<ObjectPtr> params)
        : Type(VARIANT_TYPE), variant(variant), params(params),
          initialized(false) {}
};

struct StaticType : public Type {
    const ObjectPtr obj;
    StaticType(ObjectPtr obj)
        : Type(STATIC_TYPE), obj(obj) {}
};

struct EnumType : public Type {
    const EnumDeclPtr enumeration;
    bool initialized:1;

    EnumType(EnumDeclPtr enumeration)
        : Type(ENUM_TYPE), enumeration(enumeration), initialized(false) {}
};

struct NewType : public Type {
    const NewTypeDeclPtr newtype;
    NewType(NewTypeDeclPtr newtype)
        : Type(NEW_TYPE), newtype(newtype) {}
};


//
// Pattern
//

enum PatternKind {
    PATTERN_CELL,
    PATTERN_STRUCT
};

struct Pattern : public Object {
    const PatternKind kind;
    Pattern(PatternKind kind)
        : Object(PATTERN), kind(kind) {}

};

struct PatternCell : public Pattern {
    ObjectPtr obj;
    PatternCell(ObjectPtr obj)
        : Pattern(PATTERN_CELL), obj(obj) {}
};

struct PatternStruct : public Pattern {
    const ObjectPtr head;
    const MultiPatternPtr params;
    PatternStruct(ObjectPtr head, MultiPatternPtr params)
        : Pattern(PATTERN_STRUCT), head(head), params(params) {}
};


enum MultiPatternKind {
    MULTI_PATTERN_CELL,
    MULTI_PATTERN_LIST
};

struct MultiPattern : public Object {
    const MultiPatternKind kind;
    MultiPattern(MultiPatternKind kind)
        : Object(MULTI_PATTERN), kind(kind) {}
};

struct MultiPatternCell : public MultiPattern {
    MultiPatternPtr data;
    MultiPatternCell(MultiPatternPtr data)
        : MultiPattern(MULTI_PATTERN_CELL), data(data) {}
};

struct MultiPatternList : public MultiPattern {
    vector<PatternPtr> items;
    MultiPatternPtr tail;
    MultiPatternList(llvm::ArrayRef<PatternPtr> items,
                     MultiPatternPtr tail)
        : MultiPattern(MULTI_PATTERN_LIST), items(items), tail(tail) {}
    MultiPatternList(MultiPatternPtr tail)
        : MultiPattern(MULTI_PATTERN_LIST), tail(tail) {}
    MultiPatternList(PatternPtr x)
        : MultiPattern(MULTI_PATTERN_LIST) {
        items.push_back(x);
    }
    MultiPatternList()
        : MultiPattern(MULTI_PATTERN_LIST) {}
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
    MultiStatic(llvm::ArrayRef<ObjectPtr> values)
        : Object(MULTI_STATIC), values(values) {}
    size_t size() { return values.size(); }
    void add(ObjectPtr x) { values.push_back(x); }
    void add(MultiStaticPtr x) {
        values.insert(values.end(), x->values.begin(), x->values.end());
    }
};

}
