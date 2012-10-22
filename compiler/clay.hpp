#ifndef __CLAY_HPP
#define __CLAY_HPP

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

#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/Triple.h>
#include <llvm/Analysis/DebugInfo.h>
#include <llvm/Analysis/DIBuilder.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/Assembly/Writer.h>
#include <llvm/Assembly/Parser.h>
#include <llvm/Assembly/PrintModulePass.h>
#include <llvm/BasicBlock.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/CodeGen/LinkAllAsmWriterComponents.h>
#include <llvm/CodeGen/LinkAllCodegenComponents.h>
#include <llvm/DerivedTypes.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/Function.h>
#include <llvm/Intrinsics.h>
#include <llvm/LinkAllVMCore.h>
#include <llvm/Linker.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/PassManager.h>
#include <llvm/Support/Allocator.h>
#include <llvm/Support/Dwarf.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/PathV2.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetData.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Vectorize.h>
#include <llvm/Type.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace clay {

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
// int128 type
//

#if defined(__GNUC__) && defined(_INT128_DEFINED)
typedef __int128 clay_int128;
typedef unsigned __int128 clay_uint128;
//#elif (defined(__clang__))
//typedef __int128_t clay_int128;
//typedef __uint128_t clay_uint128;
#else
// fake it by doing 64-bit math in a 128-bit padded container
struct uint128_holder;

struct int128_holder {
    ptrdiff64_t lowValue;
    ptrdiff64_t highPad; // not used in static math

    int128_holder() {}
    explicit int128_holder(ptrdiff64_t low) : lowValue(low), highPad(low < 0 ? -1 : 0) {}
    int128_holder(ptrdiff64_t low, ptrdiff64_t high)
        : lowValue(low), highPad(high) {}
    explicit int128_holder(uint128_holder y);

    int128_holder &operator=(ptrdiff64_t low) {
        new ((void*)this) int128_holder(low);
        return *this;
    }

    int128_holder operator-() const { return int128_holder(-lowValue); }
    int128_holder operator~() const { return int128_holder(~lowValue); }

    bool operator==(int128_holder y) const { return lowValue == y.lowValue; }
    bool operator< (int128_holder y) const { return lowValue <  y.lowValue; }

    int128_holder operator+ (int128_holder y) const { return int128_holder(lowValue +  y.lowValue); }
    int128_holder operator- (int128_holder y) const { return int128_holder(lowValue -  y.lowValue); }
    int128_holder operator* (int128_holder y) const { return int128_holder(lowValue *  y.lowValue); }
    int128_holder operator/ (int128_holder y) const { return int128_holder(lowValue /  y.lowValue); }
    int128_holder operator% (int128_holder y) const { return int128_holder(lowValue %  y.lowValue); }
    int128_holder operator<<(int128_holder y) const { return int128_holder(lowValue << y.lowValue); }
    int128_holder operator>>(int128_holder y) const { return int128_holder(lowValue >> y.lowValue); }
    int128_holder operator& (int128_holder y) const { return int128_holder(lowValue &  y.lowValue); }
    int128_holder operator| (int128_holder y) const { return int128_holder(lowValue |  y.lowValue); }
    int128_holder operator^ (int128_holder y) const { return int128_holder(lowValue ^  y.lowValue); }

    operator ptrdiff64_t() const { return lowValue; }
} CLAY_ALIGN(16);

struct uint128_holder {
    size64_t lowValue;
    size64_t highPad; // not used in static math

    uint128_holder() {}
    explicit uint128_holder(size64_t low) : lowValue(low), highPad(0) {}
    uint128_holder(size64_t low, size64_t high) : lowValue(low), highPad(high) {}
    explicit uint128_holder(int128_holder y) : lowValue(y.lowValue), highPad(y.highPad) {}

    uint128_holder &operator=(size64_t low) {
        new ((void*)this) uint128_holder(low);
        return *this;
    }

    uint128_holder operator-() const { return uint128_holder((size64_t)(-(ptrdiff64_t)lowValue)); }
    uint128_holder operator~() const { return uint128_holder(~lowValue); }

    bool operator==(uint128_holder y) const { return lowValue == y.lowValue; }
    bool operator< (uint128_holder y) const { return lowValue <  y.lowValue; }

    uint128_holder operator+ (uint128_holder y) const { return uint128_holder(lowValue +  y.lowValue); }
    uint128_holder operator- (uint128_holder y) const { return uint128_holder(lowValue -  y.lowValue); }
    uint128_holder operator* (uint128_holder y) const { return uint128_holder(lowValue *  y.lowValue); }
    uint128_holder operator/ (uint128_holder y) const { return uint128_holder(lowValue /  y.lowValue); }
    uint128_holder operator% (uint128_holder y) const { return uint128_holder(lowValue %  y.lowValue); }
    uint128_holder operator<<(uint128_holder y) const { return uint128_holder(lowValue << y.lowValue); }
    uint128_holder operator>>(uint128_holder y) const { return uint128_holder(lowValue >> y.lowValue); }
    uint128_holder operator& (uint128_holder y) const { return uint128_holder(lowValue &  y.lowValue); }
    uint128_holder operator| (uint128_holder y) const { return uint128_holder(lowValue |  y.lowValue); }
    uint128_holder operator^ (uint128_holder y) const { return uint128_holder(lowValue ^  y.lowValue); }

    operator size64_t() const { return lowValue; }
} CLAY_ALIGN(16);

}

namespace std {

template<>
class numeric_limits<clay::int128_holder> {
public:
    static clay::int128_holder min() throw() {
        return clay::int128_holder(0, std::numeric_limits<clay::ptrdiff64_t>::min());
    }
    static clay::int128_holder max() throw() {
        return clay::int128_holder(-1, std::numeric_limits<clay::ptrdiff64_t>::max());
    }
};

template<>
class numeric_limits<clay::uint128_holder> {
public:
    static clay::uint128_holder min() throw() {
        return clay::uint128_holder(0, 0);
    }
    static clay::uint128_holder max() throw() {
        return clay::uint128_holder(
            std::numeric_limits<clay::size64_t>::max(),
            std::numeric_limits<clay::size64_t>::max()
        );
    }
};

}

namespace clay {

inline int128_holder::int128_holder(uint128_holder y)
    : lowValue(y.lowValue), highPad(y.highPad) {}

typedef int128_holder clay_int128;
typedef uint128_holder clay_uint128;
#endif

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const clay_int128 &x) {
    return os << ptrdiff64_t(x);
}
inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const clay_uint128 &x) {
    return os << size64_t(x);
}


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
// ObjectKind
//

enum ObjectKind {
    SOURCE,       // -- 0
    LOCATION,

    IDENTIFIER,
    DOTTED_NAME,

    EXPRESSION,
    EXPR_LIST,    // -- 5
    STATEMENT,
    CASE_BLOCK,
    CATCH,

    FORMAL_ARG,
    RETURN_SPEC, // -- 10
    LLVM_CODE,
    CODE,

    RECORD_DECL,
    RECORD_BODY,
    RECORD_FIELD, // -- 15
    VARIANT_DECL,
    INSTANCE_DECL,
    NEW_TYPE_DECL,
    OVERLOAD,
    PROCEDURE,   // -- 20
    ENUM_DECL,   
    ENUM_MEMBER,
    GLOBAL_VARIABLE,
    EXTERNAL_PROCEDURE,
    EXTERNAL_ARG, // -- 25
    EXTERNAL_VARIABLE,  
    EVAL_TOPLEVEL,

    GLOBAL_ALIAS,

    IMPORT,
    MODULE_DECLARATION,  // -- 30
    MODULE,

    ENV,

    PRIM_OP,

    TYPE, // -- 35

    PATTERN,    
    MULTI_PATTERN,

    VALUE_HOLDER,
    MULTI_STATIC,

    PVALUE,
    MULTI_PVALUE,

    EVALUE,
    MULTI_EVALUE,

    CVALUE,
    MULTI_CVALUE,

    DOCUMENTATION,

    STATIC_ASSERT_TOP_LEVEL,

    DONT_CARE
};


//
// Object
//

struct Object {
    int refCount;
    ObjectKind objKind;
    Object(ObjectKind objKind)
        : refCount(0), objKind(objKind) {}
    void incRef() { ++refCount; }
    void decRef() {
        if (--refCount == 0)
            dealloc();
    }
    void operator delete(void*) {}
    virtual void dealloc() { ::operator delete(this); }
    virtual ~Object() { dealloc(); }
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
struct WithStatement;
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
typedef Pointer<WithStatement> WithStatementPtr;
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

void error(llvm::Twine const &msg);
void error(Location const& location, llvm::Twine const &msg);

struct Source : public Object {
    string fileName;
    llvm::OwningPtr<llvm::MemoryBuffer> buffer;
    llvm::TrackingVH<llvm::MDNode> debugInfo;
    Source(llvm::StringRef fileName)
        : Object(SOURCE), fileName(fileName), debugInfo(NULL)
    {
        if (llvm::error_code ec = llvm::MemoryBuffer::getFileOrSTDIN(fileName, buffer))
            error("unable to open file " + fileName + ": " + ec.message());
    }

    Source(llvm::StringRef lineOfCode, int dummy)
        : Object(SOURCE), debugInfo(NULL)
    {
                buffer.reset(llvm::MemoryBuffer::getMemBufferCopy(lineOfCode));
    }

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
    size_t offset;

    Location()
        : source(NULL), offset(0) {}
    Location(const SourcePtr &source, size_t offset)
        : source(source), offset(offset) {}

    bool ok() const { return source != NULL; }
};



//
// error module
//

struct CompileContextEntry {
    ObjectPtr callable;
    bool hasParams;
    vector<ObjectPtr> params;
    vector<unsigned> dispatchIndices;

    Location location;

    CompileContextEntry(ObjectPtr callable)
        : callable(callable), hasParams(false) {}

    CompileContextEntry(ObjectPtr callable, llvm::ArrayRef<ObjectPtr> params)
        : callable(callable), hasParams(true), params(params) {}

    CompileContextEntry(ObjectPtr callable,
                        llvm::ArrayRef<ObjectPtr> params,
                        llvm::ArrayRef<unsigned> dispatchIndices)
        : callable(callable), hasParams(true), params(params), dispatchIndices(dispatchIndices) {}
};

void pushCompileContext(ObjectPtr obj);
void pushCompileContext(ObjectPtr obj, llvm::ArrayRef<ObjectPtr> params);
void pushCompileContext(ObjectPtr obj,
                        llvm::ArrayRef<ObjectPtr> params,
                        llvm::ArrayRef<unsigned> dispatchIndices);
void popCompileContext();
vector<CompileContextEntry> getCompileContext();
void setCompileContext(llvm::ArrayRef<CompileContextEntry> x);

struct CompileContextPusher {
    CompileContextPusher(ObjectPtr obj) {
        pushCompileContext(obj);
    }
    CompileContextPusher(ObjectPtr obj, llvm::ArrayRef<ObjectPtr> params) {
        pushCompileContext(obj, params);
    }
    CompileContextPusher(ObjectPtr obj, llvm::ArrayRef<TypePtr> params) {
        vector<ObjectPtr> params2;
        for (unsigned i = 0; i < params.size(); ++i)
            params2.push_back((Object *)(params[i].ptr()));
        pushCompileContext(obj, params2);
    }
    CompileContextPusher(ObjectPtr obj,
                         llvm::ArrayRef<TypePtr> params,
                         llvm::ArrayRef<unsigned> dispatchIndices) {
        vector<ObjectPtr> params2;
        for (unsigned i = 0; i < params.size(); ++i)
            params2.push_back((Object *)(params[i].ptr()));
        pushCompileContext(obj, params2, dispatchIndices);
    }
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

void warning(llvm::Twine const &msg);

void fmtError(const char *fmt, ...);

template <class T>
inline void error(Pointer<T> context, llvm::Twine const &msg)
{
    if (context->location.ok())
        pushLocation(context->location);
    error(msg);
}

template <class T>
inline void error(T const *context, llvm::Twine const &msg)
{
    error(context->location, msg);
}

void argumentError(unsigned int index, llvm::StringRef msg);

void arityError(int expected, int received);
void arityError2(int minExpected, int received);

template <class T>
inline void arityError(Pointer<T> context, int expected, int received)
{
    if (context->location.ok())
        pushLocation(context->location);
    arityError(expected, received);
}

template <class T>
inline void arityError2(Pointer<T> context, int minExpected, int received)
{
    if (context->location.ok())
        pushLocation(context->location);
    arityError2(minExpected, received);
}

void ensureArity(MultiStaticPtr args, size_t size);
void ensureArity(MultiEValuePtr args, size_t size);
void ensureArity(MultiPValuePtr args, size_t size);
void ensureArity(MultiCValuePtr args, size_t size);

template <class T>
inline void ensureArity(T const &args, size_t size)
{
    if (args.size() != size)
        arityError(size, args.size());
}
    
template <class T>
inline void ensureArity2(T const &args, size_t size, bool hasVarArgs)
{
    if (!hasVarArgs)
        ensureArity(args, size);
    else if ((int)args.size() < size)
        arityError2(size, args.size());
}

void arityMismatchError(int leftArity, int rightArity, bool hasVarArg);

void typeError(llvm::StringRef expected, TypePtr receivedType);
void typeError(TypePtr expectedType, TypePtr receivedType);

void argumentTypeError(unsigned int index,
                       llvm::StringRef expected,
                       TypePtr receivedType);
void argumentTypeError(unsigned int index,
                       TypePtr expectedType,
                       TypePtr receivedType);

void indexRangeError(llvm::StringRef kind,
                     size_t value,
                     size_t maxValue);

void argumentIndexRangeError(unsigned int index,
                             llvm::StringRef kind,
                             size_t value,
                             size_t maxValue);

void getLineCol(Location const &location,
                int &line,
                int &column,
                int &tabColumn);

llvm::DIFile getDebugLineCol(Location const &location, int &line, int &column);

void printFileLineCol(llvm::raw_ostream &out, Location const &location);

void invalidStaticObjectError(ObjectPtr obj);
void argumentInvalidStaticObjectError(unsigned int index, ObjectPtr obj);

struct DebugPrinter {
    static int indent;
    ObjectPtr obj;
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
    virtual void dealloc() { ANodeAllocator->Deallocate(this); }
};

struct Identifier : public ANode {
    const llvm::SmallString<16> str;
    Identifier(llvm::StringRef str)
        : ANode(IDENTIFIER), str(str) {}

    static map<llvm::StringRef, IdentifierPtr> freeIdentifiers; // in parser.cpp

    static Identifier *get(llvm::StringRef str) {
        map<llvm::StringRef, IdentifierPtr>::const_iterator iter = freeIdentifiers.find(str);
        if (iter == freeIdentifiers.end()) {
            Identifier *ident = new Identifier(str);
            freeIdentifiers[ident->str] = ident;
            return ident;
        } else
            return iter->second.ptr();
    }

    static Identifier *get(llvm::StringRef str, Location const &location) {
        Identifier *ident = new Identifier(str);
        ident->location = location;
        return ident;
    }
};

struct DottedName : public ANode {
    vector<IdentifierPtr> parts;
    DottedName()
        : ANode(DOTTED_NAME) {}
    DottedName(llvm::ArrayRef<IdentifierPtr> parts)
        : ANode(DOTTED_NAME), parts(parts.begin(), parts.end()) {}

    string join() const;
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

    FILE_EXPR,
    LINE_EXPR,
    COLUMN_EXPR,
    ARG_EXPR,

    NAME_REF,
    TUPLE,
    PAREN,
    INDEXING,
    CALL,
    FIELD_REF,
    STATIC_INDEXING,
    VARIADIC_OP,
    AND,
    OR,

    LAMBDA,
    UNPACK,
    STATIC_EXPR,
    DISPATCH_EXPR,

    FOREIGN_EXPR,
    OBJECT_EXPR,

    EVAL_EXPR
};

struct Expr : public ANode {
    ExprKind exprKind;
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
    bool value;
    BoolLiteral(bool value)
        : Expr(BOOL_LITERAL), value(value) {}
};

struct IntLiteral : public Expr {
    string value;
    string suffix;
    IntLiteral(llvm::StringRef value)
        : Expr(INT_LITERAL), value(value) {}
    IntLiteral(llvm::StringRef value, llvm::StringRef suffix)
        : Expr(INT_LITERAL), value(value), suffix(suffix) {}
};

struct FloatLiteral : public Expr {
    string value;
    string suffix;
    FloatLiteral(llvm::StringRef value)
        : Expr(FLOAT_LITERAL), value(value) {}
    FloatLiteral(llvm::StringRef value, llvm::StringRef suffix)
        : Expr(FLOAT_LITERAL), value(value), suffix(suffix) {}
};

struct CharLiteral : public Expr {
    char value;
    ExprPtr desugared;
    CharLiteral(char value)
        : Expr(CHAR_LITERAL), value(value) {}
};

struct StringLiteral : public Expr {
    IdentifierPtr value;
    StringLiteral(IdentifierPtr value)
        : Expr(STRING_LITERAL), value(value) {}
};

struct NameRef : public Expr {
    IdentifierPtr name;
    NameRef(IdentifierPtr name)
        : Expr(NAME_REF), name(name) {}
};

struct FILEExpr : public Expr {
    FILEExpr()
        : Expr(FILE_EXPR) {}
};

struct LINEExpr : public Expr {
    LINEExpr()
        : Expr(LINE_EXPR) {}
};

struct COLUMNExpr : public Expr {
    COLUMNExpr()
        : Expr(COLUMN_EXPR) {}
};

struct ARGExpr : public Expr {
    IdentifierPtr name;
    ARGExpr(IdentifierPtr name)
        : Expr(ARG_EXPR), name(name) {}
};

struct Tuple : public Expr {
    ExprListPtr args;
    Tuple(ExprListPtr args)
        : Expr(TUPLE), args(args) {}
};

struct Paren : public Expr {
    ExprListPtr args;
    Paren(ExprListPtr args)
        : Expr(PAREN), args(args) {}
};

struct Indexing : public Expr {
    ExprPtr expr;
    ExprListPtr args;
    Indexing(ExprPtr expr, ExprListPtr args)
        : Expr(INDEXING), expr(expr), args(args) {}
};

struct Call : public Expr {
    ExprPtr expr;
    ExprListPtr parenArgs;
    ExprListPtr lambdaArgs;

    ExprListPtr _allArgs;

    ExprListPtr allArgs();

    Call(ExprPtr expr, ExprListPtr parenArgs, ExprListPtr lambdaArgs)
        : Expr(CALL), expr(expr), parenArgs(parenArgs), lambdaArgs(lambdaArgs),
          _allArgs(NULL) {}
    Call(ExprPtr expr, ExprListPtr parenArgs);
};

struct FieldRef : public Expr {
    ExprPtr expr;
    IdentifierPtr name;
    ExprPtr desugared;
    bool isDottedModuleName;
    FieldRef(ExprPtr expr, IdentifierPtr name)
        : Expr(FIELD_REF), expr(expr), name(name), isDottedModuleName(false) {}
};

struct StaticIndexing : public Expr {
    ExprPtr expr;
    size_t index;
    ExprPtr desugared;
    StaticIndexing(ExprPtr expr, size_t index)
        : Expr(STATIC_INDEXING), expr(expr), index(index) {}
};

enum VariadicOpKind {
    DEREFERENCE,
    ADDRESS_OF,
    NOT,
    PREFIX_OP,
    INFIX_OP,
    IF_EXPR
};

struct VariadicOp : public Expr {
    VariadicOpKind op;
    ExprListPtr exprs;
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
    LambdaCapture captureBy;
    vector<FormalArgPtr> formalArgs;
    bool hasVarArg;

    StatementPtr body;

    ExprPtr converted;

    bool initialized;
    vector<string> freeVars;

    ProcedureMono mono;

    // if freevars are present
    RecordDeclPtr lambdaRecord;
    TypePtr lambdaType;

    // if freevars are absent
    ProcedurePtr lambdaProc;

    Lambda(LambdaCapture captureBy) :
        Expr(LAMBDA), captureBy(captureBy),
        hasVarArg(false), initialized(false) {}
    Lambda(LambdaCapture captureBy,
           llvm::ArrayRef<FormalArgPtr> formalArgs,
           bool hasVarArg, StatementPtr body)
        : Expr(LAMBDA), captureBy(captureBy),
          formalArgs(formalArgs), hasVarArg(hasVarArg),
          body(body), initialized(false) {}
};

struct Unpack : public Expr {
    ExprPtr expr;
    Unpack(ExprPtr expr) :
        Expr(UNPACK), expr(expr) {}
};

struct StaticExpr : public Expr {
    ExprPtr expr;
    StaticExpr(ExprPtr expr) :
        Expr(STATIC_EXPR), expr(expr) {}
};

struct DispatchExpr : public Expr {
    ExprPtr expr;
    DispatchExpr(ExprPtr expr) :
        Expr(DISPATCH_EXPR), expr(expr) {}
};

struct ForeignExpr : public Expr {
    string moduleName;
    EnvPtr foreignEnv;
    ExprPtr expr;

    ForeignExpr(llvm::StringRef moduleName, ExprPtr expr)
        : Expr(FOREIGN_EXPR), moduleName(moduleName), expr(expr) {}
    ForeignExpr(EnvPtr foreignEnv, ExprPtr expr)
        : Expr(FOREIGN_EXPR), foreignEnv(foreignEnv), expr(expr) {}

    ForeignExpr(llvm::StringRef moduleName, EnvPtr foreignEnv, ExprPtr expr)
        : Expr(FOREIGN_EXPR), moduleName(moduleName),
          foreignEnv(foreignEnv), expr(expr) {}

    EnvPtr getEnv();
};

struct ObjectExpr : public Expr {
    ObjectPtr obj;
    ObjectExpr(ObjectPtr obj)
        : Expr(OBJECT_EXPR), obj(obj) {}
};

struct EvalExpr : public Expr {
    ExprPtr args;
    bool evaled;
    ExprListPtr value;

    EvalExpr(ExprPtr args)
        : Expr(EVAL_EXPR), args(args), evaled(false) {}
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
        _allArgs->add(lambdaArgs);
    }
    return _allArgs;
}

inline Call::Call(ExprPtr expr, ExprListPtr parenArgs)
        : Expr(CALL), expr(expr), parenArgs(parenArgs), lambdaArgs(new ExprList()),
          _allArgs(NULL) {}

//
// Stmt
//

enum StatementKind {
    BLOCK,
    LABEL,
    BINDING,
    ASSIGNMENT,
    INIT_ASSIGNMENT,
    VARIADIC_ASSIGNMENT,
    GOTO,
    RETURN,
    IF,
    SWITCH,
    EXPR_STATEMENT,
    WHILE,
    BREAK,
    CONTINUE,
    FOR,
    FOREIGN_STATEMENT,
    TRY,
    THROW,
    STATIC_FOR,
    FINALLY,
    ONERROR,
    UNREACHABLE,
    EVAL_STATEMENT,
    WITH,
    STATIC_ASSERT_STATEMENT
};

struct Statement : public ANode {
    StatementKind stmtKind;
    Statement(StatementKind stmtKind)
        : ANode(STATEMENT), stmtKind(stmtKind) {}
};

struct Block : public Statement {
    BlockPtr desugared;
    vector<StatementPtr> statements;
    Block()
        : Statement(BLOCK) {}
    Block(llvm::ArrayRef<StatementPtr> statements)
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
    ALIAS,
    FORWARD
};

struct PatternVar;

struct Binding : public Statement {
    BindingKind bindingKind;
    vector<PatternVar> patternVars;
    vector<ObjectPtr> patternTypes;
    ExprPtr predicate;
    vector<FormalArgPtr> args;
    ExprListPtr values;
    bool hasVarArg;
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
    ExprListPtr left, right;
    Assignment(ExprListPtr left,
               ExprListPtr right)
        : Statement(ASSIGNMENT), left(left), right(right) {}
};

struct InitAssignment : public Statement {
    ExprListPtr left, right;
    InitAssignment(ExprListPtr left,
                   ExprListPtr right)
        : Statement(INIT_ASSIGNMENT), left(left), right(right) {}
    InitAssignment(ExprPtr leftExpr, ExprPtr rightExpr)
        : Statement(INIT_ASSIGNMENT),
          left(new ExprList(leftExpr)),
          right(new ExprList(rightExpr)) {}
};

struct VariadicAssignment : public Statement {
    int op;
    ExprListPtr exprs;
    ExprPtr desugared;
    VariadicAssignment(int op, ExprListPtr exprs )
        : Statement(VARIADIC_ASSIGNMENT), op(op), exprs(exprs) {}
};

struct Goto : public Statement {
    IdentifierPtr labelName;
    Goto(IdentifierPtr labelName)
        : Statement(GOTO), labelName(labelName) {}
};

enum ReturnKind {
    RETURN_VALUE,
    RETURN_REF,
    RETURN_FORWARD
};

struct Return : public Statement {
    ReturnKind returnKind;
    ExprListPtr values;
    bool isExprReturn;
    bool isReturnSpecs;
    Return(ReturnKind returnKind, ExprListPtr values)
        : Statement(RETURN), returnKind(returnKind), values(values), 
          isExprReturn(false), isReturnSpecs(false) {}
    Return(ReturnKind returnKind, ExprListPtr values, bool exprRet)
        : Statement(RETURN), returnKind(returnKind), values(values), 
          isExprReturn(exprRet), isReturnSpecs(false) {}
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
    ExprListPtr caseLabels;
    StatementPtr body;
    CaseBlock(ExprListPtr caseLabels, StatementPtr body)
        : ANode(CASE_BLOCK), caseLabels(caseLabels), body(body) {}
};

struct ExprStatement : public Statement {
    ExprPtr expr;
    ExprStatement(ExprPtr expr)
        : Statement(EXPR_STATEMENT), expr(expr) {}
};

struct While : public Statement {
    vector<StatementPtr> conditionStatements;
    ExprPtr condition;

    StatementPtr body;

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
    vector<IdentifierPtr> variables;
    ExprPtr expr;
    StatementPtr body;
    StatementPtr desugared;
    For(llvm::ArrayRef<IdentifierPtr> variables,
        ExprPtr expr,
        StatementPtr body)
        : Statement(FOR), variables(variables), expr(expr), body(body) {}
};

struct ForeignStatement : public Statement {
    string moduleName;
    EnvPtr foreignEnv;
    StatementPtr statement;
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
    StatementPtr tryBlock;
    vector<CatchPtr> catchBlocks;
    StatementPtr desugaredCatchBlock;

    Try(StatementPtr tryBlock,
        vector<CatchPtr> catchBlocks)
        : Statement(TRY), tryBlock(tryBlock),
          catchBlocks(catchBlocks) {}
};

struct Catch : public ANode {
    IdentifierPtr exceptionVar;
    ExprPtr exceptionType; // optional
    IdentifierPtr contextVar;
    StatementPtr body;

    Catch(IdentifierPtr exceptionVar,
          ExprPtr exceptionType,
          IdentifierPtr contextVar,
          StatementPtr body)
        : ANode(CATCH), exceptionVar(exceptionVar),
          exceptionType(exceptionType), contextVar(contextVar), body(body) {}
};

struct Throw : public Statement {
    ExprPtr expr;
    Throw(ExprPtr expr)
        : Statement(THROW), expr(expr) {}
};

struct StaticFor : public Statement {
    IdentifierPtr variable;
    ExprListPtr values;
    StatementPtr body;

    bool clonesInitialized;
    vector<StatementPtr> clonedBodies;

    StaticFor(IdentifierPtr variable,
              ExprListPtr values,
              StatementPtr body)
        : Statement(STATIC_FOR), variable(variable), values(values),
          body(body), clonesInitialized(false) {}
};

struct Finally : public Statement {
    StatementPtr body;

    explicit Finally(StatementPtr body) : Statement(FINALLY), body(body) {}
};

struct OnError : public Statement {
    StatementPtr body;

    explicit OnError(StatementPtr body) : Statement(ONERROR), body(body) {}
};

struct Unreachable : public Statement {
    Unreachable()
        : Statement(UNREACHABLE) {}
};

struct EvalStatement : public Statement {
    ExprListPtr args;
    bool evaled;
    vector<StatementPtr> value;

    EvalStatement(ExprListPtr args)
        : Statement(EVAL_STATEMENT), args(args), evaled(false) {}
};


struct WithStatement : public Statement {
    vector<IdentifierPtr> lhs;
    ExprPtr rhs;
    Location withLocation;
    WithStatement( vector<IdentifierPtr> i, ExprPtr r, Location const &l)
        : Statement(WITH), lhs(i), rhs(r), withLocation(l) {}
};


struct StaticAssertStatement : public Statement {
    ExprPtr cond;
    ExprListPtr message;

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
    IdentifierPtr name;
    ExprPtr type;
    ValueTempness tempness;
    bool varArg;
    FormalArg(IdentifierPtr name, ExprPtr type)
        : ANode(FORMAL_ARG), name(name), type(type),
          tempness(TEMPNESS_DONTCARE), varArg(false) {}
    FormalArg(IdentifierPtr name, ExprPtr type, ValueTempness tempness)
        : ANode(FORMAL_ARG), name(name), type(type),
          tempness(tempness), varArg(false) {}
    FormalArg(IdentifierPtr name, ExprPtr type, ValueTempness tempness, bool varArg)
        : ANode(FORMAL_ARG), name(name), type(type),
          tempness(tempness), varArg(varArg) {}
};

struct ReturnSpec : public ANode {
    ExprPtr type;
    IdentifierPtr name;
    ReturnSpec(ExprPtr type)
        : ANode(RETURN_SPEC), type(type) {}
    ReturnSpec(ExprPtr type, IdentifierPtr name)
        : ANode(RETURN_SPEC), type(type), name(name) {}
};

struct PatternVar {
    bool isMulti;
    IdentifierPtr name;
    PatternVar(bool isMulti, IdentifierPtr name)
        : isMulti(isMulti), name(name) {}
    PatternVar()
        : isMulti(false) {}
};

struct LLVMCode : ANode {
    string body;
    LLVMCode(llvm::StringRef body)
        : ANode(LLVM_CODE), body(body) {}
};

struct Code : public ANode {
    vector<PatternVar> patternVars;
    ExprPtr predicate;
    vector<FormalArgPtr> formalArgs;
    bool hasVarArg;
    bool returnSpecsDeclared;
    vector<ReturnSpecPtr> returnSpecs;
    ReturnSpecPtr varReturnSpec;
    StatementPtr body;
    LLVMCodePtr llvmBody;

    Code()
        : ANode(CODE),  hasVarArg(false), returnSpecsDeclared(false) {}
    Code(llvm::ArrayRef<PatternVar> patternVars,
         ExprPtr predicate,
         llvm::ArrayRef<FormalArgPtr> formalArgs,
         llvm::ArrayRef<ReturnSpecPtr> returnSpecs,
         ReturnSpecPtr varReturnSpec,
         StatementPtr body)
        : ANode(CODE), patternVars(patternVars), predicate(predicate),
          formalArgs(formalArgs), hasVarArg(false), returnSpecsDeclared(false),
          returnSpecs(returnSpecs), varReturnSpec(varReturnSpec),
          body(body) {}

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
    PUBLIC,
    PRIVATE
};



//
// TopLevelItem
//

struct TopLevelItem : public ANode {
    EnvPtr env;
    IdentifierPtr name; // for named top level items
    Visibility visibility; // valid only if name != NULL
    TopLevelItem(ObjectKind objKind)
        : ANode(objKind) {}
    TopLevelItem(ObjectKind objKind, Visibility visibility)
        : ANode(objKind), visibility(visibility) {}
    TopLevelItem(ObjectKind objKind, IdentifierPtr name, Visibility visibility)
        : ANode(objKind), name(name), visibility(visibility) {}
};

struct RecordDecl : public TopLevelItem {
    vector<PatternVar> patternVars;
    ExprPtr predicate;

    vector<IdentifierPtr> params;
    IdentifierPtr varParam;

    RecordBodyPtr body;

    vector<OverloadPtr> overloads;
    bool builtinOverloadInitialized;

    LambdaPtr lambda;

    RecordDecl(Visibility visibility)
        : TopLevelItem(RECORD_DECL, visibility),
          builtinOverloadInitialized(false) {}
    RecordDecl(Visibility visibility,
           llvm::ArrayRef<PatternVar> patternVars, ExprPtr predicate)
        : TopLevelItem(RECORD_DECL, visibility),
          patternVars(patternVars),
          predicate(predicate),
          builtinOverloadInitialized(false) {}
    RecordDecl(IdentifierPtr name,
           Visibility visibility,
           llvm::ArrayRef<PatternVar> patternVars,
           ExprPtr predicate,
           llvm::ArrayRef<IdentifierPtr> params,
           IdentifierPtr varParam,
           RecordBodyPtr body)
        : TopLevelItem(RECORD_DECL, name, visibility),
          patternVars(patternVars), predicate(predicate),
          params(params), varParam(varParam), body(body),
          builtinOverloadInitialized(false) {}
};

struct RecordBody : public ANode {
    bool isComputed;
    ExprListPtr computed; // valid if isComputed == true
    vector<RecordFieldPtr> fields; // valid if isComputed == false
    RecordBody(ExprListPtr computed)
        : ANode(RECORD_BODY), isComputed(true), computed(computed) {}
    RecordBody(llvm::ArrayRef<RecordFieldPtr> fields)
        : ANode(RECORD_BODY), isComputed(false), fields(fields) {}
};

struct RecordField : public ANode {
    IdentifierPtr name;
    ExprPtr type;
    RecordField(IdentifierPtr name, ExprPtr type)
        : ANode(RECORD_FIELD), name(name), type(type) {}
};

struct VariantDecl : public TopLevelItem {
    vector<PatternVar> patternVars;
    ExprPtr predicate;

    vector<IdentifierPtr> params;
    IdentifierPtr varParam;

    bool open;
    ExprListPtr defaultInstances;

    vector<InstanceDeclPtr> instances;

    vector<OverloadPtr> overloads;

    VariantDecl(IdentifierPtr name,
            Visibility visibility,
            llvm::ArrayRef<PatternVar> patternVars,
            ExprPtr predicate,
            llvm::ArrayRef<IdentifierPtr> params,
            IdentifierPtr varParam,
            bool open,
            ExprListPtr defaultInstances)
        : TopLevelItem(VARIANT_DECL, name, visibility),
          patternVars(patternVars), predicate(predicate),
          params(params), varParam(varParam),
          open(open), defaultInstances(defaultInstances) {}
};

struct InstanceDecl : public TopLevelItem {
    vector<PatternVar> patternVars;
    ExprPtr predicate;
    ExprPtr target;
    ExprListPtr members;

    InstanceDecl(llvm::ArrayRef<PatternVar> patternVars,
             ExprPtr predicate,
             ExprPtr target,
             ExprListPtr members)
        : TopLevelItem(INSTANCE_DECL), patternVars(patternVars),
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
    ExprPtr target;
    CodePtr code;
    bool callByName;
    InlineAttribute isInline;

    // pre-computed patterns for matchInvoke
    int patternsInitializedState; // 0:notinit, -1:initing, +1:inited
    bool nameIsPattern;
    vector<PatternCellPtr> cells;
    vector<MultiPatternCellPtr> multiCells;
    PatternPtr callablePattern;
    vector<PatternPtr> argPatterns;
    MultiPatternPtr varArgPattern;
    
    Overload(ExprPtr target,
             CodePtr code,
             bool callByName,
             InlineAttribute isInline)
        : TopLevelItem(OVERLOAD), target(target), code(code),
          callByName(callByName), isInline(isInline),
          patternsInitializedState(0), nameIsPattern(false) {}
};

struct Procedure : public TopLevelItem {
    OverloadPtr interface;

    OverloadPtr singleOverload;
    vector<OverloadPtr> overloads;
    ObjectTablePtr evaluatorCache; // HACK: used only for predicates
    ProcedureMono mono;
    LambdaPtr lambda;

    Procedure(IdentifierPtr name, Visibility visibility)
        : TopLevelItem(PROCEDURE, name, visibility) {}

    Procedure(IdentifierPtr name, Visibility visibility, OverloadPtr interface)
        : TopLevelItem(PROCEDURE, name, visibility), interface(interface) {}
};


struct NewTypeDecl : public TopLevelItem {
    ExprPtr expr;
    TypePtr type;
    TypePtr baseType;
    bool initialized;
    NewTypeDecl(IdentifierPtr name,
            Visibility visibility,
            ExprPtr expr)
        : TopLevelItem(NEW_TYPE_DECL, name, visibility), expr(expr),
        initialized(false) {}
    
    
};

struct EnumDecl : public TopLevelItem {
    vector<PatternVar> patternVars;
    ExprPtr predicate;

    vector<EnumMemberPtr> members;
    TypePtr type;
    EnumDecl(IdentifierPtr name, Visibility visibility,
        llvm::ArrayRef<PatternVar> patternVars, ExprPtr predicate)
        : TopLevelItem(ENUM_DECL, name, visibility),
          patternVars(patternVars), predicate(predicate) {}
    EnumDecl(IdentifierPtr name, Visibility visibility,
                llvm::ArrayRef<PatternVar> patternVars, ExprPtr predicate,
                llvm::ArrayRef<EnumMemberPtr> members)
        : TopLevelItem(ENUM_DECL, name, visibility),
          patternVars(patternVars), predicate(predicate),
          members(members) {}
};

struct EnumMember : public ANode {
    IdentifierPtr name;
    int index;
    TypePtr type;
    EnumMember(IdentifierPtr name)
        : ANode(ENUM_MEMBER), name(name), index(-1) {}
};

struct GlobalVariable : public TopLevelItem {
    vector<PatternVar> patternVars;
    ExprPtr predicate;

    vector<IdentifierPtr> params;
    IdentifierPtr varParam;
    ExprPtr expr;

    ObjectTablePtr instances;

    GlobalVariable(IdentifierPtr name,
                   Visibility visibility,
                   llvm::ArrayRef<PatternVar> patternVars,
                   ExprPtr predicate,
                   llvm::ArrayRef<IdentifierPtr> params,
                   IdentifierPtr varParam,
                   ExprPtr expr)
        : TopLevelItem(GLOBAL_VARIABLE, name, visibility),
          patternVars(patternVars), predicate(predicate),
          params(params), varParam(varParam), expr(expr) {}

    bool hasParams() const {
        return !params.empty() || (varParam.ptr() != NULL);
    }
};

struct GVarInstance : public Object {
    GlobalVariablePtr gvar;
    vector<ObjectPtr> params;

    bool analyzing;
    ExprPtr expr;
    EnvPtr env;
    MultiPValuePtr analysis;
    TypePtr type;
    ValueHolderPtr staticGlobal;
    llvm::GlobalVariable *llGlobal;
    llvm::TrackingVH<llvm::MDNode> debugInfo;

    GVarInstance(GlobalVariablePtr gvar,
                 llvm::ArrayRef<ObjectPtr> params)
        : Object(DONT_CARE), gvar(gvar), params(params),
          analyzing(false), llGlobal(NULL), debugInfo(NULL) {}

    llvm::DIGlobalVariable getDebugInfo() { return llvm::DIGlobalVariable(debugInfo); }
};

enum CallingConv {
    CC_DEFAULT,
    CC_STDCALL,
    CC_FASTCALL,
    CC_THISCALL,
    CC_LLVM,
    CC_Count
};

struct ExternalProcedure : public TopLevelItem {
    vector<ExternalArgPtr> args;
    bool hasVarArgs;
    ExprPtr returnType;
    StatementPtr body;
    ExprListPtr attributes;

    bool attributesVerified;
    bool attrDLLImport;
    bool attrDLLExport;
    CallingConv callingConv;
    llvm::SmallString<16> attrAsmLabel;

    bool analyzed;
    bool bodyCodegenned;
    TypePtr returnType2;
    TypePtr ptrType;

    llvm::Function *llvmFunc;
    llvm::TrackingVH<llvm::MDNode> debugInfo;

    ExternalProcedure(Visibility visibility)
        : TopLevelItem(EXTERNAL_PROCEDURE, visibility), hasVarArgs(false),
          attributes(new ExprList()), attributesVerified(false),
          analyzed(false), bodyCodegenned(false), llvmFunc(NULL), debugInfo(NULL) {}
    ExternalProcedure(IdentifierPtr name,
                      Visibility visibility,
                      llvm::ArrayRef<ExternalArgPtr> args,
                      bool hasVarArgs,
                      ExprPtr returnType,
                      StatementPtr body,
                      ExprListPtr attributes)
        : TopLevelItem(EXTERNAL_PROCEDURE, name, visibility), args(args),
          hasVarArgs(hasVarArgs), returnType(returnType), body(body),
          attributes(attributes), attributesVerified(false),
          analyzed(false), bodyCodegenned(false), llvmFunc(NULL), debugInfo(NULL) {}

    llvm::DISubprogram getDebugInfo() { return llvm::DISubprogram(debugInfo); }
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
    ExprListPtr attributes;

    bool attributesVerified;
    bool attrDLLImport;
    bool attrDLLExport;

    TypePtr type2;
    llvm::GlobalVariable *llGlobal;
    llvm::TrackingVH<llvm::MDNode> debugInfo;

    ExternalVariable(Visibility visibility)
        : TopLevelItem(EXTERNAL_VARIABLE, visibility),
          attributes(new ExprList()), attributesVerified(false),
          llGlobal(NULL), debugInfo(NULL) {}
    ExternalVariable(IdentifierPtr name,
                     Visibility visibility,
                     ExprPtr type,
                     ExprListPtr attributes)
        : TopLevelItem(EXTERNAL_VARIABLE, name, visibility),
          type(type), attributes(attributes),
          attributesVerified(false), llGlobal(NULL), debugInfo(NULL) {}

    llvm::DIGlobalVariable getDebugInfo() { return llvm::DIGlobalVariable(debugInfo); }
};

struct EvalTopLevel : public TopLevelItem {
    ExprListPtr args;
    bool evaled;
    vector<TopLevelItemPtr> value;

    EvalTopLevel(ExprListPtr args)
        : TopLevelItem(EVAL_TOPLEVEL), args(args), evaled(false)
        {}
};

struct StaticAssertTopLevel : public TopLevelItem {
    ExprPtr cond;
    ExprListPtr message;

    StaticAssertTopLevel(ExprPtr cond, ExprListPtr message)
    : TopLevelItem(STATIC_ASSERT_TOP_LEVEL), cond(cond), message(message)
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
    RecordAnnotion
};

struct Documentation : public TopLevelItem {

    std::map<DocumentationAnnotation, string> annotation;
    std::string text;
    Documentation(const std::map<DocumentationAnnotation, string> &annotation, const std::string &text)
        : TopLevelItem(DOCUMENTATION), annotation(annotation), text(text)
        {}
};



//
// GlobalAlias
//

struct GlobalAlias : public TopLevelItem {
    vector<PatternVar> patternVars;
    ExprPtr predicate;

    vector<IdentifierPtr> params;
    IdentifierPtr varParam;
    ExprPtr expr;

    vector<OverloadPtr> overloads;

    GlobalAlias(IdentifierPtr name,
                Visibility visibility,
                llvm::ArrayRef<PatternVar> patternVars,
                ExprPtr predicate,
                llvm::ArrayRef<IdentifierPtr> params,
                IdentifierPtr varParam,
                ExprPtr expr)
        : TopLevelItem(GLOBAL_ALIAS, name, visibility),
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
    ImportKind importKind;
    DottedNamePtr dottedName;
    Visibility visibility;
    ModulePtr module;
    Import(ImportKind importKind, DottedNamePtr dottedName, Visibility visibility)
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
    ImportMembers(DottedNamePtr dottedName, Visibility visibility)
        : Import(IMPORT_MEMBERS, dottedName, visibility) {}
};


//
// Module
//

struct ModuleDeclaration : public ANode {
    DottedNamePtr name;
    ExprListPtr attributes;

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
    bool attributesVerified;
    vector<llvm::SmallString<16> > attrBuildFlags;
    IntegerTypePtr attrDefaultIntegerType;
    FloatTypePtr attrDefaultFloatType;

    llvm::StringMap<ImportSet> publicSymbols;
    bool publicSymbolsLoaded;
    int publicSymbolsLoading;

    llvm::StringMap<ImportSet> allSymbols;
    bool allSymbolsLoaded;
    int allSymbolsLoading;

    set<string> importedNames;

    bool topLevelLLVMGenerated;
    bool externalsGenerated;

    llvm::TrackingVH<llvm::MDNode> debugInfo;

    Module(llvm::StringRef moduleName)
        : ANode(MODULE), moduleName(moduleName),
          initState(BEFORE),
          attributesVerified(false),
          publicSymbolsLoaded(false), publicSymbolsLoading(0),
          allSymbolsLoaded(false), allSymbolsLoading(0),
          topLevelLLVMGenerated(false),
          externalsGenerated(false),
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
          attributesVerified(false),
          publicSymbolsLoaded(false), publicSymbolsLoading(0),
          allSymbolsLoaded(false), allSymbolsLoading(0),
          topLevelLLVMGenerated(false),
          externalsGenerated(false),
          debugInfo(NULL) {}

    llvm::DINameSpace getDebugInfo() { return llvm::DINameSpace(debugInfo); }
};


//
// analyzer
//

struct PVData {
    TypePtr type;
    bool isTemp;
    PVData() : type(NULL), isTemp(NULL) {}
    PVData(TypePtr type, bool isTemp)
        : type(type), isTemp(isTemp) {}

    bool ok() const { return type != NULL; }

    bool operator==(PVData const &x) const { return type == x.type && isTemp == x.isTemp; }
};

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
};


//
// parser module
//


enum ParserFlags
{
    NoParserFlags = 0,
    ParserKeepDocumentation = 1
};

ModulePtr parse(llvm::StringRef moduleName, SourcePtr source, ParserFlags flags = NoParserFlags);
ExprPtr parseExpr(SourcePtr source, int offset, int length);
ExprListPtr parseExprList(SourcePtr source, int offset, int length);
void parseStatements(SourcePtr source, int offset, int length,
    vector<StatementPtr> &statements);
void parseTopLevelItems(SourcePtr source, int offset, int length,
    vector<TopLevelItemPtr> &topLevels);
void parseInteractive(SourcePtr source, int offset, int length,
                      vector<TopLevelItemPtr>& toplevels,
                      vector<ImportPtr>& imports,
                      vector<StatementPtr>& stmts);


//
// printer module
//

llvm::raw_ostream &operator<<(llvm::raw_ostream &out, const Object &obj);

llvm::raw_ostream &operator<<(llvm::raw_ostream &out, const Object *obj);

template <class T>
llvm::raw_ostream &operator<<(llvm::raw_ostream &out, const Pointer<T> &p)
{
    out << *p;
    return out;
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &out, const PatternVar &pvar);

void enableSafePrintName();
void disableSafePrintName();

struct SafePrintNameEnabler {
    SafePrintNameEnabler() { enableSafePrintName(); }
    ~SafePrintNameEnabler() { disableSafePrintName(); }
};

void printNameList(llvm::raw_ostream &out, llvm::ArrayRef<ObjectPtr> x);
void printNameList(llvm::raw_ostream &out, llvm::ArrayRef<ObjectPtr> x, llvm::ArrayRef<unsigned> dispatchIndices);
void printNameList(llvm::raw_ostream &out, llvm::ArrayRef<TypePtr> x);
void printStaticName(llvm::raw_ostream &out, ObjectPtr x);
void printName(llvm::raw_ostream &out, ObjectPtr x);
void printTypeAndValue(llvm::raw_ostream &out, EValuePtr ev);
void printValue(llvm::raw_ostream &out, EValuePtr ev);

string shortString(llvm::StringRef in);


//
// clone ast
//

CodePtr clone(CodePtr x);
void clone(llvm::ArrayRef<PatternVar> x, vector<PatternVar> &out);
void clone(llvm::ArrayRef<IdentifierPtr> x, vector<IdentifierPtr> &out);
ExprPtr clone(ExprPtr x);
ExprPtr cloneOpt(ExprPtr x);
ExprListPtr clone(ExprListPtr x);
void clone(llvm::ArrayRef<FormalArgPtr> x, vector<FormalArgPtr> &out);
FormalArgPtr clone(FormalArgPtr x);
FormalArgPtr cloneOpt(FormalArgPtr x);
void clone(llvm::ArrayRef<ReturnSpecPtr> x, vector<ReturnSpecPtr> &out);
ReturnSpecPtr clone(ReturnSpecPtr x);
ReturnSpecPtr cloneOpt(ReturnSpecPtr x);
StatementPtr clone(StatementPtr x);
StatementPtr cloneOpt(StatementPtr x);
void clone(llvm::ArrayRef<StatementPtr> x, vector<StatementPtr> &out);
CaseBlockPtr clone(CaseBlockPtr x);
void clone(llvm::ArrayRef<CaseBlockPtr> x, vector<CaseBlockPtr> &out);
CatchPtr clone(CatchPtr x);
void clone(llvm::ArrayRef<CatchPtr> x, vector<CatchPtr> &out);



//
// Env
//

struct Env : public Object {
    ObjectPtr parent;
    ExprPtr callByNameExprHead;
    llvm::StringMap<ObjectPtr> entries;
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
ObjectPtr lookupPrivate(ModulePtr module, IdentifierPtr name);
ObjectPtr lookupPublic(ModulePtr module, IdentifierPtr name);
ObjectPtr safeLookupPublic(ModulePtr module, IdentifierPtr name);

void addLocal(EnvPtr env, IdentifierPtr name, ObjectPtr value);
ObjectPtr lookupEnv(EnvPtr env, IdentifierPtr name);
ObjectPtr safeLookupEnv(EnvPtr env, IdentifierPtr name);
ModulePtr safeLookupModule(EnvPtr env);
llvm::DINameSpace lookupModuleDebugInfo(EnvPtr env);

ObjectPtr lookupEnvEx(EnvPtr env, IdentifierPtr name,
                      EnvPtr nonLocalEnv, bool &isNonLocal,
                      bool &isGlobal);

ExprPtr foreignExpr(EnvPtr env, ExprPtr expr);

ExprPtr lookupCallByNameExprHead(EnvPtr env);
Location safeLookupCallByNameLocation(EnvPtr env);

//
// interactive module
//

void runInteractive(llvm::Module *llvmModule, ModulePtr module);


//
// objects
//

struct ValueHolder : public Object {
    TypePtr type;
    char *buf;
    ValueHolder(TypePtr type);
    ~ValueHolder();

    template<typename T>
    T &as() { return *(T*)buf; }

    template<typename T>
    T const &as() const { return *(T const *)buf; }
};


bool _objectValueEquals(ObjectPtr a, ObjectPtr b);
int objectHash(ObjectPtr a);

inline bool objectEquals(ObjectPtr a, ObjectPtr b) {
    if (a == b)
        return true;
    return _objectValueEquals(a,b);
}

template <typename ObjectVectorA, typename ObjectVectorB>
inline bool objectVectorEquals(ObjectVectorA const &a,
                        ObjectVectorB const &b) {
    if (a.size() != b.size()) return false;
    for (unsigned i = 0; i < a.size(); ++i) {
        if (!objectEquals(a[i].ptr(), b[i].ptr()))
            return false;
    }
    return true;
}

template <typename ObjectVector>
inline int objectVectorHash(ObjectVector const &a) {
    int h = 0;
    for (unsigned i = 0; i < a.size(); ++i)
        h += objectHash(a[i].ptr());
    return h;
}

struct ObjectTableNode {
    vector<ObjectPtr> key;
    ObjectPtr value;
    ObjectTableNode(llvm::ArrayRef<ObjectPtr> key,
                    ObjectPtr value)
        : key(key), value(value) {}
};

struct ObjectTable : public Object {
    vector< vector<ObjectTableNode> > buckets;
    unsigned size;
public :
    ObjectTable() : Object(DONT_CARE), size(0) {}
    ObjectPtr &lookup(llvm::ArrayRef<ObjectPtr> key);
private :
    void rehash();
};


//
// Types
//

enum TypeKind {
    BOOL_TYPE,
    INTEGER_TYPE,
    FLOAT_TYPE,
    COMPLEX_TYPE,
    POINTER_TYPE,
    CODE_POINTER_TYPE,
    CCODE_POINTER_TYPE,
    ARRAY_TYPE,
    VEC_TYPE,
    TUPLE_TYPE,
    UNION_TYPE,
    RECORD_TYPE,
    VARIANT_TYPE,
    STATIC_TYPE,
    ENUM_TYPE,
    NEW_TYPE
};

struct Type : public Object {
    TypeKind typeKind;
    llvm::Type *llType;
    llvm::TrackingVH<llvm::MDNode> debugInfo;
    bool defined;

    bool typeInfoInitialized;
    size_t typeSize;
    size_t typeAlignment;

    bool overloadsInitialized;
    vector<OverloadPtr> overloads;

    Type(TypeKind typeKind)
        : Object(TYPE), typeKind(typeKind),
          llType(NULL), debugInfo(NULL), defined(false),
          typeInfoInitialized(false), overloadsInitialized(false) {}

    llvm::DIType getDebugInfo() { return llvm::DIType(debugInfo); }
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
    bool isImaginary;
    FloatType(int bits, bool isImaginary)
        : Type(FLOAT_TYPE), bits(bits), isImaginary(isImaginary){}
};

struct ComplexType : public Type {
    int bits;
    const llvm::StructLayout *layout;
    ComplexType(int bits)
        : Type(COMPLEX_TYPE), bits(bits), layout(NULL) {}
};

struct PointerType : public Type {
    TypePtr pointeeType;
    PointerType(TypePtr pointeeType)
        : Type(POINTER_TYPE), pointeeType(pointeeType) {}
};

struct CodePointerType : public Type {
    vector<TypePtr> argTypes;

    vector<uint8_t> returnIsRef;
    vector<TypePtr> returnTypes;

    CodePointerType(llvm::ArrayRef<TypePtr> argTypes,
                    llvm::ArrayRef<uint8_t> returnIsRef,
                    llvm::ArrayRef<TypePtr> returnTypes)
        : Type(CODE_POINTER_TYPE), argTypes(argTypes),
          returnIsRef(returnIsRef), returnTypes(returnTypes) {}
};

struct CCodePointerType : public Type {
    CallingConv callingConv;
    vector<TypePtr> argTypes;
    bool hasVarArgs;
    TypePtr returnType; // NULL if void return

    llvm::Type *callType;

    llvm::Type *getCallType();

    CCodePointerType(CallingConv callingConv,
                     llvm::ArrayRef<TypePtr> argTypes,
                     bool hasVarArgs,
                     TypePtr returnType)
        : Type(CCODE_POINTER_TYPE), callingConv(callingConv),
          argTypes(argTypes), hasVarArgs(hasVarArgs),
          returnType(returnType), callType(NULL) {}
};

struct ArrayType : public Type {
    TypePtr elementType;
    int size;
    ArrayType(TypePtr elementType, int size)
        : Type(ARRAY_TYPE), elementType(elementType), size(size) {}
};

struct VecType : public Type {
    TypePtr elementType;
    int size;
    VecType(TypePtr elementType, int size)
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
    RecordDeclPtr record;
    vector<ObjectPtr> params;

    bool fieldsInitialized;
    vector<IdentifierPtr> fieldNames;
    vector<TypePtr> fieldTypes;
    llvm::StringMap<size_t> fieldIndexMap;

    const llvm::StructLayout *layout;

    RecordType(RecordDeclPtr record)
        : Type(RECORD_TYPE), record(record), fieldsInitialized(false),
          layout(NULL) {}
    RecordType(RecordDeclPtr record, llvm::ArrayRef<ObjectPtr> params)
        : Type(RECORD_TYPE), record(record), params(params),
          fieldsInitialized(false), layout(NULL) {}
};

struct VariantType : public Type {
    VariantDeclPtr variant;
    vector<ObjectPtr> params;

    bool initialized;
    vector<TypePtr> memberTypes;
    TypePtr reprType;

    VariantType(VariantDeclPtr variant)
        : Type(VARIANT_TYPE), variant(variant),
          initialized(false) {}
    VariantType(VariantDeclPtr variant, llvm::ArrayRef<ObjectPtr> params)
        : Type(VARIANT_TYPE), variant(variant), params(params),
          initialized(false) {}
};

struct StaticType : public Type {
    ObjectPtr obj;
    StaticType(ObjectPtr obj)
        : Type(STATIC_TYPE), obj(obj) {}
};

struct EnumType : public Type {
    EnumDeclPtr enumeration;
    bool initialized;

    EnumType(EnumDeclPtr enumeration)
        : Type(ENUM_TYPE), enumeration(enumeration), initialized(false) {}
};

struct NewType : public Type {
    NewTypeDeclPtr newtype;
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
    PatternKind kind;
    Pattern(PatternKind kind)
        : Object(PATTERN), kind(kind) {}
};

struct PatternCell : public Pattern {
    ObjectPtr obj;
    PatternCell(ObjectPtr obj)
        : Pattern(PATTERN_CELL), obj(obj) {}
};

struct PatternStruct : public Pattern {
    ObjectPtr head;
    MultiPatternPtr params;
    PatternStruct(ObjectPtr head, MultiPatternPtr params)
        : Pattern(PATTERN_STRUCT), head(head), params(params) {}
};


enum MultiPatternKind {
    MULTI_PATTERN_CELL,
    MULTI_PATTERN_LIST
};

struct MultiPattern : public Object {
    MultiPatternKind kind;
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


//
// parachute
//

int parachute(int (*mainfn)(int, char **, char const* const*),
    int argc, char **argv, char const* const* envp);

}

#endif
