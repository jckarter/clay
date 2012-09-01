#ifndef __CLAY_HPP
#define __CLAY_HPP

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS
#endif

#include <string>
#include <vector>
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
// Object
//

struct Object {
    int refCount;
    int objKind;
    Object(int objKind)
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

    RECORD,
    RECORD_BODY,
    RECORD_FIELD, // -- 15
    VARIANT,
    INSTANCE,
    OVERLOAD,
    PROCEDURE,
    ENUMERATION,    // -- 20
    ENUM_MEMBER,
    GLOBAL_VARIABLE,
    EXTERNAL_PROCEDURE,
    EXTERNAL_ARG,
    EXTERNAL_VARIABLE,  // -- 25
    EVAL_TOPLEVEL,

    GLOBAL_ALIAS,

    IMPORT,
    MODULE_DECLARATION,
    MODULE_HOLDER,  // -- 30
    MODULE,

    ENV,

    PRIM_OP,

    TYPE,

    PATTERN,    // -- 35
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

    DONT_CARE
};



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

struct FormalArg;
struct ReturnSpec;
struct LLVMCode;
struct Code;

struct TopLevelItem;
struct Record;
struct RecordBody;
struct RecordField;
struct Variant;
struct Instance;
struct Overload;
struct Procedure;
struct Enumeration;
struct EnumMember;
struct GlobalVariable;
struct GVarInstance;
struct ExternalProcedure;
struct ExternalArg;
struct ExternalVariable;
struct EvalTopLevel;
struct Documentation;

struct GlobalAlias;

struct Import;
struct ImportModule;
struct ImportStar;
struct ImportMembers;
struct ModuleHolder;
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

typedef Pointer<FormalArg> FormalArgPtr;
typedef Pointer<ReturnSpec> ReturnSpecPtr;
typedef Pointer<LLVMCode> LLVMCodePtr;
typedef Pointer<Code> CodePtr;

typedef Pointer<TopLevelItem> TopLevelItemPtr;
typedef Pointer<Record> RecordPtr;
typedef Pointer<RecordBody> RecordBodyPtr;
typedef Pointer<RecordField> RecordFieldPtr;
typedef Pointer<Variant> VariantPtr;
typedef Pointer<Instance> InstancePtr;
typedef Pointer<Overload> OverloadPtr;
typedef Pointer<Procedure> ProcedurePtr;
typedef Pointer<Enumeration> EnumerationPtr;
typedef Pointer<EnumMember> EnumMemberPtr;
typedef Pointer<GlobalVariable> GlobalVariablePtr;
typedef Pointer<GVarInstance> GVarInstancePtr;
typedef Pointer<ExternalProcedure> ExternalProcedurePtr;
typedef Pointer<ExternalArg> ExternalArgPtr;
typedef Pointer<ExternalVariable> ExternalVariablePtr;
typedef Pointer<EvalTopLevel> EvalTopLevelPtr;
typedef Pointer<Documentation> DocumentationPtr;

typedef Pointer<GlobalAlias> GlobalAliasPtr;

typedef Pointer<Import> ImportPtr;
typedef Pointer<ImportModule> ImportModulePtr;
typedef Pointer<ImportStar> ImportStarPtr;
typedef Pointer<ImportMembers> ImportMembersPtr;
typedef Pointer<ModuleHolder> ModuleHolderPtr;
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



//
// Source, Location
//

void error(llvm::Twine const &msg);

struct Source : public Object {
    string fileName;
    llvm::OwningPtr<llvm::MemoryBuffer> buffer;
    llvm::TrackingVH<llvm::MDNode> debugInfo;
    Source(llvm::StringRef fileName)
        : Object(SOURCE), fileName(fileName), debugInfo(NULL)
    {
        if (llvm::error_code ec = llvm::MemoryBuffer::getFileOrSTDIN(fileName, buffer))
            error("unable to open file: " + fileName);
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

extern bool shouldPrintFullMatchErrors;
extern set<pair<string,string> > logMatchSymbols;

struct CompileContextEntry {
    ObjectPtr callable;
    bool hasParams;
    vector<ObjectPtr> params;
    vector<unsigned> dispatchIndices;

    Location location;

    CompileContextEntry(ObjectPtr callable)
        : callable(callable), hasParams(false) {}

    CompileContextEntry(ObjectPtr callable, const vector<ObjectPtr> &params)
        : callable(callable), hasParams(true), params(params) {}

    CompileContextEntry(ObjectPtr callable,
                        const vector<ObjectPtr> &params,
                        const vector<unsigned> &dispatchIndices)
        : callable(callable), hasParams(true), params(params), dispatchIndices(dispatchIndices) {}
};

void pushCompileContext(ObjectPtr obj);
void pushCompileContext(ObjectPtr obj, const vector<ObjectPtr> &params);
void pushCompileContext(ObjectPtr obj,
                        const vector<ObjectPtr> &params,
                        const vector<unsigned> &dispatchIndices);
void popCompileContext();
vector<CompileContextEntry> getCompileContext();
void setCompileContext(const vector<CompileContextEntry> &x);

struct CompileContextPusher {
    CompileContextPusher(ObjectPtr obj) {
        pushCompileContext(obj);
    }
    CompileContextPusher(ObjectPtr obj, const vector<ObjectPtr> &params) {
        pushCompileContext(obj, params);
    }
    CompileContextPusher(ObjectPtr obj, const vector<TypePtr> &params) {
        vector<ObjectPtr> params2;
        for (unsigned i = 0; i < params.size(); ++i)
            params2.push_back((Object *)(params[i].ptr()));
        pushCompileContext(obj, params2);
    }
    CompileContextPusher(ObjectPtr obj,
                         const vector<TypePtr> &params,
                         const vector<unsigned> &dispatchIndices) {
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

void setAbortOnError(bool flag);
void warning(llvm::Twine const &msg);

void fmtError(const char *fmt, ...);

template <class T>
void error(Pointer<T> context, llvm::Twine const &msg)
{
    if (context->location.ok())
        pushLocation(context->location);
    error(msg);
}

template <class T>
void error(T const *context, llvm::Twine const &msg)
{
    if (context->location.ok())
        pushLocation(context->location);
    error(msg);
}

void argumentError(unsigned int index, llvm::StringRef msg);

void arityError(int expected, int received);
void arityError2(int minExpected, int received);

template <class T>
void arityError(Pointer<T> context, int expected, int received)
{
    if (context->location.ok())
        pushLocation(context->location);
    arityError(expected, received);
}

template <class T>
void arityError2(Pointer<T> context, int minExpected, int received)
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
void ensureArity(const vector<T> &args, size_t size)
{
    if ((int)args.size() != size)
        arityError(size, args.size());
}

template <class T>
void ensureArity2(const vector<T> &args, size_t size, bool hasVarArgs)
{
    if (!hasVarArgs)
        ensureArity(args, size);
    else if ((int)args.size() < size)
        arityError2(size, args.size());
}

void arityMismatchError(int leftArity, int rightArity);

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
// Token
//

enum TokenKind {
    T_NONE,
    T_UOPSTRING,
    T_OPSTRING,
    T_SYMBOL,
    T_KEYWORD,
    T_IDENTIFIER,
    T_STRING_LITERAL,
    T_CHAR_LITERAL,
    T_INT_LITERAL,
    T_FLOAT_LITERAL,
    T_STATIC_INDEX,
    T_SPACE,
    T_LINE_COMMENT,
    T_BLOCK_COMMENT,
    T_LLVM,
    T_DOC_START,
    T_DOC_PROPERTY,
    T_DOC_TEXT,
    T_DOC_END,
};

struct Token {
    Location location;
    llvm::SmallString<16> str;
    int tokenKind;
    Token() : tokenKind(T_NONE) {}
    explicit Token(int tokenKind) : tokenKind(tokenKind) {}
    explicit Token(int tokenKind, llvm::StringRef str) : tokenKind(tokenKind), str(str) {}
};



//
// lexer module
//

void tokenize(SourcePtr source, vector<Token> &tokens);

void tokenize(SourcePtr source, size_t offset, size_t length,
              vector<Token> &tokens);



//
// AST
//

static llvm::BumpPtrAllocator *ANodeAllocator = new llvm::BumpPtrAllocator();

struct ANode : public Object {
    Location location;
    ANode(int objKind)
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
    FieldRef(ExprPtr expr, IdentifierPtr name)
        : Expr(FIELD_REF), expr(expr), name(name) {}
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
    IF_EXPR,
};

struct VariadicOp : public Expr {
    int op;
    ExprListPtr exprs;
    ExprPtr desugared;
    VariadicOp(int op, ExprListPtr exprs )
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
    Procedure_PolyOverload,
};

struct ProcedureMono {
    ProcedureMonoState monoState;
    vector<TypePtr> monoTypes;

    ProcedureMono() : monoState(Procedure_NoOverloads) {}
};

struct Lambda : public Expr {
    bool captureByRef;
    vector<FormalArgPtr> formalArgs;
    bool hasVarArg;

    StatementPtr body;

    ExprPtr converted;

    bool initialized;
    vector<string> freeVars;

    ProcedureMono mono;

    // if freevars are present
    RecordPtr lambdaRecord;
    TypePtr lambdaType;

    // if freevars are absent
    ProcedurePtr lambdaProc;

    Lambda(bool captureByRef) :
        Expr(LAMBDA), captureByRef(captureByRef),
        initialized(false), hasVarArg(false) {}
    Lambda(bool captureByRef,
           const vector<FormalArgPtr> &formalArgs,
           bool hasVarArg, StatementPtr body)
        : Expr(LAMBDA), captureByRef(captureByRef),
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
    ExprList(const vector<ExprPtr> &exprs)
        : Object(EXPR_LIST), exprs(exprs) {}
    size_t size() { return exprs.size(); }
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
    WITH
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
    ALIAS,
    FORWARD
};

struct PatternVar;

struct Binding : public Statement {
    int bindingKind;
    vector<PatternVar> patternVars;
    vector<ObjectPtr> patternTypes;
    ExprPtr predicate;
    vector<FormalArgPtr> args;
    ExprListPtr values;
    bool hasVarArg;
    Binding(int bindingKind,
        const vector<FormalArgPtr> &args,
        ExprListPtr values)
        : Statement(BINDING), bindingKind(bindingKind),
          args(args), values(values), hasVarArg(false) {}
    Binding(int bindingKind,
        const vector<PatternVar> &patternVars,
        const vector<ObjectPtr> &patternTypes,
        ExprPtr predicate,
        const vector<FormalArgPtr> &args,
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
    If(const vector<StatementPtr> &conditionStatements,
        ExprPtr condition, StatementPtr thenPart)
        : Statement(IF), conditionStatements(conditionStatements),
          condition(condition), thenPart(thenPart) {}
    If(const vector<StatementPtr> &conditionStatements,
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

    Switch(const vector<StatementPtr> &exprStatements,
           ExprPtr expr,
           const vector<CaseBlockPtr> &caseBlocks,
           StatementPtr defaultCase)
        : Statement(SWITCH), exprStatements(exprStatements),
          expr(expr), caseBlocks(caseBlocks),
          defaultCase(defaultCase) {}
    Switch(ExprPtr expr,
           const vector<CaseBlockPtr> &caseBlocks,
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
    While(const vector<StatementPtr> &conditionStatements,
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
    For(const vector<IdentifierPtr> &variables,
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
    StatementPtr body;

    Catch(IdentifierPtr exceptionVar,
          ExprPtr exceptionType,
          StatementPtr body)
        : ANode(CATCH), exceptionVar(exceptionVar),
          exceptionType(exceptionType), body(body) {}
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
    Code(const vector<PatternVar> &patternVars,
         ExprPtr predicate,
         const vector<FormalArgPtr> &formalArgs,
         const vector<ReturnSpecPtr> &returnSpecs,
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
    TopLevelItem(int objKind)
        : ANode(objKind) {}
    TopLevelItem(int objKind, Visibility visibility)
        : ANode(objKind), visibility(visibility) {}
    TopLevelItem(int objKind, IdentifierPtr name, Visibility visibility)
        : ANode(objKind), name(name), visibility(visibility) {}
};

struct Record : public TopLevelItem {
    vector<PatternVar> patternVars;
    ExprPtr predicate;

    vector<IdentifierPtr> params;
    IdentifierPtr varParam;

    RecordBodyPtr body;

    vector<OverloadPtr> overloads;
    bool builtinOverloadInitialized;

    LambdaPtr lambda;

    Record(Visibility visibility)
        : TopLevelItem(RECORD, visibility),
          builtinOverloadInitialized(false) {}
    Record(Visibility visibility,
           const vector<PatternVar> &patternVars, ExprPtr predicate)
        : TopLevelItem(RECORD, visibility),
          patternVars(patternVars),
          predicate(predicate),
          builtinOverloadInitialized(false) {}
    Record(IdentifierPtr name,
           Visibility visibility,
           const vector<PatternVar> &patternVars,
           ExprPtr predicate,
           const vector<IdentifierPtr> &params,
           IdentifierPtr varParam,
           RecordBodyPtr body)
        : TopLevelItem(RECORD, name, visibility),
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
    RecordBody(const vector<RecordFieldPtr> &fields)
        : ANode(RECORD_BODY), isComputed(false), fields(fields) {}
};

struct RecordField : public ANode {
    IdentifierPtr name;
    ExprPtr type;
    RecordField(IdentifierPtr name, ExprPtr type)
        : ANode(RECORD_FIELD), name(name), type(type) {}
};

struct Variant : public TopLevelItem {
    vector<PatternVar> patternVars;
    ExprPtr predicate;

    vector<IdentifierPtr> params;
    IdentifierPtr varParam;

    ExprListPtr defaultInstances;

    vector<InstancePtr> instances;

    vector<OverloadPtr> overloads;

    Variant(IdentifierPtr name,
            Visibility visibility,
            const vector<PatternVar> &patternVars,
            ExprPtr predicate,
            const vector<IdentifierPtr> &params,
            IdentifierPtr varParam,
            ExprListPtr defaultInstances)
        : TopLevelItem(VARIANT, name, visibility),
          patternVars(patternVars), predicate(predicate),
          params(params), varParam(varParam),
          defaultInstances(defaultInstances) {}
};

struct Instance : public TopLevelItem {
    vector<PatternVar> patternVars;
    ExprPtr predicate;
    ExprPtr target;
    ExprListPtr members;

    Instance(const vector<PatternVar> &patternVars,
             ExprPtr predicate,
             ExprPtr target,
             ExprListPtr members)
        : TopLevelItem(INSTANCE), patternVars(patternVars),
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
    vector<OverloadPtr> overloads;
    ObjectTablePtr evaluatorCache; // HACK: used only for predicates
    ProcedureMono mono;
    LambdaPtr lambda;

    Procedure(IdentifierPtr name, Visibility visibility)
        : TopLevelItem(PROCEDURE, name, visibility) {}

    Procedure(IdentifierPtr name, Visibility visibility, OverloadPtr interface)
        : TopLevelItem(PROCEDURE, name, visibility), interface(interface) {}
};

struct Enumeration : public TopLevelItem {
    vector<PatternVar> patternVars;
    ExprPtr predicate;

    vector<EnumMemberPtr> members;
    TypePtr type;
    Enumeration(IdentifierPtr name, Visibility visibility,
        const vector<PatternVar> &patternVars, ExprPtr predicate)
        : TopLevelItem(ENUMERATION, name, visibility),
          patternVars(patternVars), predicate(predicate) {}
    Enumeration(IdentifierPtr name, Visibility visibility,
                const vector<PatternVar> &patternVars, ExprPtr predicate,
                const vector<EnumMemberPtr> &members)
        : TopLevelItem(ENUMERATION, name, visibility),
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
                   const vector<PatternVar> &patternVars,
                   ExprPtr predicate,
                   const vector<IdentifierPtr> &params,
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
                 const vector<ObjectPtr> &params)
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
                      const vector<ExternalArgPtr> &args,
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
                const vector<PatternVar> &patternVars,
                ExprPtr predicate,
                const vector<IdentifierPtr> &params,
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

struct ModuleHolder : public Object {
    Module *module;
    llvm::StringMap<ModuleHolderPtr> children;
    ModuleHolder()
        : Object(MODULE_HOLDER), module(NULL) {}

    static ModuleHolderPtr get(ModulePtr module);
};

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

struct Module : public ANode {
    SourcePtr source;
    string moduleName;
    vector<ImportPtr> imports;
    ModuleDeclarationPtr declaration;
    LLVMCodePtr topLevelLLVM;
    vector<TopLevelItemPtr> topLevelItems;

    ModuleHolderPtr rootHolder;
    llvm::StringMap<ObjectPtr> globals;

    ModuleHolderPtr publicRootHolder;
    llvm::StringMap<ObjectPtr> publicGlobals;

    ModuleHolderPtr selfHolder;

    EnvPtr env;
    bool initialized;
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

    bool topLevelLLVMGenerated;
    bool externalsGenerated;

    llvm::TrackingVH<llvm::MDNode> debugInfo;

    Module(llvm::StringRef moduleName)
        : ANode(MODULE), moduleName(moduleName),
          initialized(false),
          attributesVerified(false),
          publicSymbolsLoaded(false), publicSymbolsLoading(0),
          allSymbolsLoaded(false), allSymbolsLoading(0),
          topLevelLLVMGenerated(false),
          externalsGenerated(false),
          debugInfo(NULL) {}
    Module(llvm::StringRef moduleName,
           const vector<ImportPtr> &imports,
           ModuleDeclarationPtr declaration,
           LLVMCodePtr topLevelLLVM,
           const vector<TopLevelItemPtr> &topLevelItems)
        : ANode(MODULE), moduleName(moduleName), imports(imports),
          declaration(declaration),
          topLevelLLVM(topLevelLLVM), topLevelItems(topLevelItems),
          initialized(false),
          attributesVerified(false),
          publicSymbolsLoaded(false), publicSymbolsLoading(0),
          allSymbolsLoaded(false), allSymbolsLoading(0),
          topLevelLLVMGenerated(false),
          externalsGenerated(false),
          debugInfo(NULL) {}

    llvm::DINameSpace getDebugInfo() { return llvm::DINameSpace(debugInfo); }
};

inline ModuleHolderPtr ModuleHolder::get(ModulePtr module)
{
    if (module->selfHolder == NULL) {
        module->selfHolder = new ModuleHolder();
        module->selfHolder->module = module.ptr();
    }
    return module->selfHolder;
}


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

void printNameList(llvm::raw_ostream &out, const vector<ObjectPtr> &x);
void printNameList(llvm::raw_ostream &out, const vector<ObjectPtr> &x, const vector<unsigned> &dispatchIndices);
void printNameList(llvm::raw_ostream &out, const vector<TypePtr> &x);
void printStaticName(llvm::raw_ostream &out, ObjectPtr x);
void printName(llvm::raw_ostream &out, ObjectPtr x);
void printTypeAndValue(llvm::raw_ostream &out, EValuePtr ev);
void printValue(llvm::raw_ostream &out, EValuePtr ev);

string shortString(llvm::StringRef in);


//
// clone ast
//

CodePtr clone(CodePtr x);
void clone(const vector<PatternVar> &x, vector<PatternVar> &out);
void clone(const vector<IdentifierPtr> &x, vector<IdentifierPtr> &out);
ExprPtr clone(ExprPtr x);
ExprPtr cloneOpt(ExprPtr x);
ExprListPtr clone(ExprListPtr x);
void clone(const vector<FormalArgPtr> &x, vector<FormalArgPtr> &out);
FormalArgPtr clone(FormalArgPtr x);
FormalArgPtr cloneOpt(FormalArgPtr x);
void clone(const vector<ReturnSpecPtr> &x, vector<ReturnSpecPtr> &out);
ReturnSpecPtr clone(ReturnSpecPtr x);
ReturnSpecPtr cloneOpt(ReturnSpecPtr x);
StatementPtr clone(StatementPtr x);
StatementPtr cloneOpt(StatementPtr x);
void clone(const vector<StatementPtr> &x, vector<StatementPtr> &out);
CaseBlockPtr clone(CaseBlockPtr x);
void clone(const vector<CaseBlockPtr> &x, vector<CaseBlockPtr> &out);
CatchPtr clone(CatchPtr x);
void clone(const vector<CatchPtr> &x, vector<CatchPtr> &out);



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
ObjectPtr lookupModuleHolder(ModuleHolderPtr mh, IdentifierPtr name);
ObjectPtr safeLookupModuleHolder(ModuleHolderPtr mh, IdentifierPtr name);
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
// loader module
//

extern llvm::StringMap<ModulePtr> globalModules;
extern llvm::StringMap<string> globalFlags;
extern ModulePtr globalMainModule;

void addProcedureOverload(ProcedurePtr proc, EnvPtr Env, OverloadPtr x);
void getProcedureMonoTypes(ProcedureMono &mono, EnvPtr env,
    vector<FormalArgPtr> const &formalArgs, bool hasVarArg);

void addSearchPath(llvm::StringRef path);
ModulePtr loadProgram(llvm::StringRef fileName, vector<string> *sourceFiles);
ModulePtr loadProgramSource(llvm::StringRef name, llvm::StringRef source);
ModulePtr loadedModule(llvm::StringRef module);
llvm::StringRef primOpName(PrimOpPtr x);
ModulePtr preludeModule();
ModulePtr primitivesModule();
ModulePtr operatorsModule();
ModulePtr staticModule(ObjectPtr x);



//
// PrimOp
//

enum PrimOpCode {
    PRIM_TypeP,
    PRIM_TypeSize,
    PRIM_TypeAlignment,

    PRIM_SymbolP,
    
    PRIM_StaticCallDefinedP,
    PRIM_StaticCallOutputTypes,

    PRIM_StaticMonoP,
    PRIM_StaticMonoInputTypes,

    PRIM_bitcopy,
    PRIM_bitcast,

    PRIM_boolNot,

    PRIM_numericAdd,
    PRIM_numericSubtract,
    PRIM_numericMultiply,
    PRIM_floatDivide,
    PRIM_numericNegate,

    PRIM_integerQuotient,
    PRIM_integerRemainder,
    PRIM_integerShiftLeft,
    PRIM_integerShiftRight,
    PRIM_integerBitwiseAnd,
    PRIM_integerBitwiseOr,
    PRIM_integerBitwiseXor,
    PRIM_integerBitwiseNot,
    PRIM_integerEqualsP,
    PRIM_integerLesserP,

    PRIM_numericConvert,

    PRIM_integerAddChecked,
    PRIM_integerSubtractChecked,
    PRIM_integerMultiplyChecked,
    PRIM_integerQuotientChecked,
    PRIM_integerRemainderChecked,
    PRIM_integerNegateChecked,
    PRIM_integerShiftLeftChecked,
    PRIM_integerConvertChecked,

    PRIM_floatOrderedEqualsP,
    PRIM_floatOrderedLesserP,
    PRIM_floatOrderedLesserEqualsP,
    PRIM_floatOrderedGreaterP,
    PRIM_floatOrderedGreaterEqualsP,
    PRIM_floatOrderedNotEqualsP,
    PRIM_floatOrderedP,
    PRIM_floatUnorderedEqualsP,
    PRIM_floatUnorderedLesserP,
    PRIM_floatUnorderedLesserEqualsP,
    PRIM_floatUnorderedGreaterP,
    PRIM_floatUnorderedGreaterEqualsP,
    PRIM_floatUnorderedNotEqualsP,
    PRIM_floatUnorderedP,

    PRIM_Pointer,

    PRIM_addressOf,
    PRIM_pointerDereference,
    PRIM_pointerOffset,
    PRIM_pointerToInt,
    PRIM_intToPointer,
    PRIM_nullPointer,

    PRIM_CodePointer,
    PRIM_makeCodePointer,

    PRIM_AttributeStdCall,
    PRIM_AttributeFastCall,
    PRIM_AttributeThisCall,
    PRIM_AttributeCCall,
    PRIM_AttributeLLVMCall,
    PRIM_AttributeDLLImport,
    PRIM_AttributeDLLExport,

    PRIM_ExternalCodePointer,
    PRIM_makeExternalCodePointer,
    PRIM_callExternalCodePointer,

    PRIM_Array,
    PRIM_arrayRef,
    PRIM_arrayElements,

    PRIM_Vec,

    PRIM_Tuple,
    PRIM_TupleElementCount,
    PRIM_tupleRef,
    PRIM_tupleElements,

    PRIM_Union,
    PRIM_UnionMemberCount,

    PRIM_RecordP,
    PRIM_RecordFieldCount,
    PRIM_RecordFieldName,
    PRIM_RecordWithFieldP,
    PRIM_recordFieldRef,
    PRIM_recordFieldRefByName,
    PRIM_recordFields,

    PRIM_VariantP,
    PRIM_VariantMemberIndex,
    PRIM_VariantMemberCount,
    PRIM_VariantMembers,
    PRIM_variantRepr,

    PRIM_Static,
    PRIM_StaticName,
    PRIM_staticIntegers,
    PRIM_integers,
    PRIM_staticFieldRef,

    PRIM_MainModule,
    PRIM_StaticModule,
    PRIM_ModuleName,
    PRIM_ModuleMemberNames,

    PRIM_EnumP,
    PRIM_EnumMemberCount,
    PRIM_EnumMemberName,
    PRIM_enumToInt,
    PRIM_intToEnum,

    PRIM_StringLiteralP,
    PRIM_stringLiteralByteIndex,
    PRIM_stringLiteralBytes,
    PRIM_stringLiteralByteSize,
    PRIM_stringLiteralByteSlice,
    PRIM_stringLiteralConcat,
    PRIM_stringLiteralFromBytes,

    PRIM_stringTableConstant,

    PRIM_FlagP,
    PRIM_Flag,

    PRIM_atomicFence,
    PRIM_atomicRMW,
    PRIM_atomicLoad,
    PRIM_atomicStore,
    PRIM_atomicCompareExchange,

    PRIM_OrderUnordered,
    PRIM_OrderMonotonic,
    PRIM_OrderAcquire,
    PRIM_OrderRelease,
    PRIM_OrderAcqRel,
    PRIM_OrderSeqCst,

    PRIM_RMWXchg,
    PRIM_RMWAdd,
    PRIM_RMWSubtract,
    PRIM_RMWAnd,
    PRIM_RMWNAnd,
    PRIM_RMWOr,
    PRIM_RMWXor,
    PRIM_RMWMin,
    PRIM_RMWMax,
    PRIM_RMWUMin,
    PRIM_RMWUMax,

    PRIM_ByRef,
    PRIM_RecordWithProperties,

    PRIM_activeException,

    PRIM_memcpy,
    PRIM_memmove,

    PRIM_countValues,
    PRIM_nthValue,
    PRIM_withoutNthValue,
    PRIM_takeValues,
    PRIM_dropValues,

    PRIM_LambdaRecordP,
    PRIM_LambdaSymbolP,
    PRIM_LambdaMonoP,
    PRIM_LambdaMonoInputTypes,

    PRIM_GetOverload
};

struct PrimOp : public Object {
    int primOpCode;
    PrimOp(int primOpCode)
        : Object(PRIM_OP), primOpCode(primOpCode) {}
};



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

struct ObjectTableNode {
    vector<ObjectPtr> key;
    ObjectPtr value;
    ObjectTableNode(const vector<ObjectPtr> &key,
                    ObjectPtr value)
        : key(key), value(value) {}
};

struct ObjectTable : public Object {
    vector< vector<ObjectTableNode> > buckets;
    unsigned size;
public :
    ObjectTable() : Object(DONT_CARE), size(0) {}
    ObjectPtr &lookup(const vector<ObjectPtr> &key);
private :
    void rehash();
};



//
// types module
//

struct Type : public Object {
    int typeKind;
    llvm::Type *llType;
    llvm::TrackingVH<llvm::MDNode> debugInfo;
    bool defined;

    bool typeInfoInitialized;
    size_t typeSize;
    size_t typeAlignment;

    bool overloadsInitialized;
    vector<OverloadPtr> overloads;

    Type(int typeKind)
        : Object(TYPE), typeKind(typeKind),
          llType(NULL), debugInfo(NULL), defined(false),
          typeInfoInitialized(false), overloadsInitialized(false) {}

    llvm::DIType getDebugInfo() { return llvm::DIType(debugInfo); }
};

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
    ENUM_TYPE
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

    vector<bool> returnIsRef;
    vector<TypePtr> returnTypes;

    CodePointerType(const vector<TypePtr> &argTypes,
                    const vector<bool> &returnIsRef,
                    const vector<TypePtr> &returnTypes)
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
                     const vector<TypePtr> &argTypes,
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
    TupleType(const vector<TypePtr> &elementTypes)
        : Type(TUPLE_TYPE), elementTypes(elementTypes),
          layout(NULL) {}
};

struct UnionType : public Type {
    vector<TypePtr> memberTypes;
    UnionType()
        : Type(UNION_TYPE) {}
    UnionType(const vector<TypePtr> &memberTypes)
        : Type(UNION_TYPE), memberTypes(memberTypes)
        {}
};

struct RecordType : public Type {
    RecordPtr record;
    vector<ObjectPtr> params;

    bool fieldsInitialized;
    vector<IdentifierPtr> fieldNames;
    vector<TypePtr> fieldTypes;
    llvm::StringMap<size_t> fieldIndexMap;

    const llvm::StructLayout *layout;

    RecordType(RecordPtr record)
        : Type(RECORD_TYPE), record(record), fieldsInitialized(false),
          layout(NULL) {}
    RecordType(RecordPtr record, const vector<ObjectPtr> &params)
        : Type(RECORD_TYPE), record(record), params(params),
          fieldsInitialized(false), layout(NULL) {}
};

struct VariantType : public Type {
    VariantPtr variant;
    vector<ObjectPtr> params;

    bool initialized;
    vector<TypePtr> memberTypes;
    TypePtr reprType;

    VariantType(VariantPtr variant)
        : Type(VARIANT_TYPE), variant(variant),
          initialized(false) {}
    VariantType(VariantPtr variant, const vector<ObjectPtr> &params)
        : Type(VARIANT_TYPE), variant(variant), params(params),
          initialized(false) {}
};

struct StaticType : public Type {
    ObjectPtr obj;
    StaticType(ObjectPtr obj)
        : Type(STATIC_TYPE), obj(obj) {}
};

struct EnumType : public Type {
    EnumerationPtr enumeration;
    bool initialized;

    EnumType(EnumerationPtr enumeration)
        : Type(ENUM_TYPE), enumeration(enumeration), initialized(false) {}
};


extern TypePtr boolType;
extern TypePtr int8Type;
extern TypePtr int16Type;
extern TypePtr int32Type;
extern TypePtr int64Type;
extern TypePtr int128Type;
extern TypePtr uint8Type;
extern TypePtr uint16Type;
extern TypePtr uint32Type;
extern TypePtr uint64Type;
extern TypePtr uint128Type;
extern TypePtr float32Type;
extern TypePtr float64Type;
extern TypePtr float80Type;
extern TypePtr imag32Type;
extern TypePtr imag64Type;
extern TypePtr imag80Type;
extern TypePtr complex32Type;
extern TypePtr complex64Type;
extern TypePtr complex80Type;

// aliases
extern TypePtr cIntType;
extern TypePtr cSizeTType;
extern TypePtr cPtrDiffTType;

void initTypes();

TypePtr integerType(int bits, bool isSigned);
TypePtr intType(int bits);
TypePtr uintType(int bits);
TypePtr floatType(int bits);
TypePtr imagType(int bits);
TypePtr complexType(int bits);
TypePtr pointerType(TypePtr pointeeType);
TypePtr codePointerType(const vector<TypePtr> &argTypes,
                        const vector<bool> &returnIsRef,
                        const vector<TypePtr> &returnTypes);
TypePtr cCodePointerType(CallingConv callingConv,
                         const vector<TypePtr> &argTypes,
                         bool hasVarArgs,
                         TypePtr returnType);
TypePtr arrayType(TypePtr elememtType, int size);
TypePtr vecType(TypePtr elementType, int size);
TypePtr tupleType(const vector<TypePtr> &elementTypes);
TypePtr unionType(const vector<TypePtr> &memberTypes);
TypePtr recordType(RecordPtr record, const vector<ObjectPtr> &params);
TypePtr variantType(VariantPtr variant, const vector<ObjectPtr> &params);
TypePtr staticType(ObjectPtr obj);
TypePtr enumType(EnumerationPtr enumeration);

bool isPrimitiveType(TypePtr t);
bool isPrimitiveAggregateType(TypePtr t);
bool isPrimitiveAggregateTooLarge(TypePtr t);
bool isPointerOrCodePointerType(TypePtr t);
bool isStaticOrTupleOfStatics(TypePtr t);

void initializeRecordFields(RecordTypePtr t);
const vector<IdentifierPtr> &recordFieldNames(RecordTypePtr t);
const vector<TypePtr> &recordFieldTypes(RecordTypePtr t);
const llvm::StringMap<size_t> &recordFieldIndexMap(RecordTypePtr t);

const vector<TypePtr> &variantMemberTypes(VariantTypePtr t);
TypePtr variantReprType(VariantTypePtr t);
int dispatchTagCount(TypePtr t);

void initializeEnumType(EnumTypePtr t);

const llvm::StructLayout *tupleTypeLayout(TupleType *t);
const llvm::StructLayout *complexTypeLayout(ComplexType *t);
const llvm::StructLayout *recordTypeLayout(RecordType *t);

llvm::Type *llvmIntType(int bits);
llvm::Type *llvmFloatType(int bits);
llvm::PointerType *llvmPointerType(llvm::Type *llType);
llvm::PointerType *llvmPointerType(TypePtr t);
llvm::Type *llvmArrayType(llvm::Type *llType, int size);
llvm::Type *llvmArrayType(TypePtr type, int size);
llvm::Type *llvmVoidType();

llvm::Type *llvmType(TypePtr t);
llvm::DIType llvmTypeDebugInfo(TypePtr t);
llvm::DIType llvmVoidTypeDebugInfo();

size_t typeSize(TypePtr t);
size_t typeAlignment(TypePtr t);
void typePrint(llvm::raw_ostream &out, TypePtr t);
string typeName(TypePtr t);

inline size_t alignedUpTo(size_t offset, size_t align) {
    return (offset+align-1)/align*align;
}

inline size_t alignedUpTo(size_t offset, TypePtr type) {
    return alignedUpTo(offset, typeAlignment(type));
}


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
    MultiPatternList(const vector<PatternPtr> &items,
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
    MultiStatic(const vector<ObjectPtr> &values)
        : Object(MULTI_STATIC), values(values) {}
    size_t size() { return values.size(); }
    void add(ObjectPtr x) { values.push_back(x); }
    void add(MultiStaticPtr x) {
        values.insert(values.end(), x->values.begin(), x->values.end());
    }
};



//
// desugar
//

ExprPtr desugarCharLiteral(char c);
ExprPtr desugarFieldRef(FieldRefPtr x);
ExprPtr desugarStaticIndexing(StaticIndexingPtr x);
ExprPtr desugarVariadicOp(VariadicOpPtr x);
ExprListPtr desugarVariadicAssignmentRight(VariadicAssignment *x);
StatementPtr desugarForStatement(ForPtr x);
StatementPtr desugarCatchBlocks(const vector<CatchPtr> &catchBlocks);
StatementPtr desugarSwitchStatement(SwitchPtr x);
BlockPtr desugarBlock(BlockPtr x);

ExprListPtr desugarEvalExpr(EvalExprPtr eval, EnvPtr env);
vector<StatementPtr> const &desugarEvalStatement(EvalStatementPtr eval, EnvPtr env);
vector<TopLevelItemPtr> const &desugarEvalTopLevel(EvalTopLevelPtr eval, EnvPtr env);


//
// patterns
//

ObjectPtr derefDeep(PatternPtr x);
MultiStaticPtr derefDeep(MultiPatternPtr x);

bool unifyObjObj(ObjectPtr a, ObjectPtr b);
bool unifyObjPattern(ObjectPtr a, PatternPtr b);
bool unifyPatternObj(PatternPtr a, ObjectPtr b);
bool unify(PatternPtr a, PatternPtr b);
bool unifyMulti(MultiPatternPtr a, MultiStaticPtr b);
bool unifyMulti(MultiPatternPtr a, MultiPatternPtr b);
bool unifyMulti(MultiPatternListPtr a, unsigned indexA,
                MultiPatternPtr b);
bool unifyMulti(MultiPatternPtr a,
                MultiPatternListPtr b, unsigned indexB);
bool unifyMulti(MultiPatternListPtr a, unsigned indexA,
                MultiPatternListPtr b, unsigned indexB);
bool unifyEmpty(MultiPatternListPtr x, unsigned index);
bool unifyEmpty(MultiPatternPtr x);

PatternPtr evaluateOnePattern(ExprPtr expr, EnvPtr env);
PatternPtr evaluateAliasPattern(GlobalAliasPtr x, MultiPatternPtr params);
MultiPatternPtr evaluateMultiPattern(ExprListPtr exprs, EnvPtr env);

void patternPrint(llvm::raw_ostream &out, PatternPtr x);
void patternPrint(llvm::raw_ostream &out, MultiPatternPtr x);



//
// lambdas
//

void initializeLambda(LambdaPtr x, EnvPtr env);


//
// match invoke
//

enum MatchCode {
    MATCH_SUCCESS,
    MATCH_CALLABLE_ERROR,
    MATCH_ARITY_ERROR,
    MATCH_ARGUMENT_ERROR,
    MATCH_MULTI_ARGUMENT_ERROR,
    MATCH_PREDICATE_ERROR,
    MATCH_BINDING_ERROR,
    MATCH_MULTI_BINDING_ERROR
};

struct MatchResult : public Object {
    int matchCode;
    MatchResult(int matchCode)
        : Object(DONT_CARE), matchCode(matchCode) {}
};
typedef Pointer<MatchResult> MatchResultPtr;

struct MatchSuccess : public MatchResult {
    bool callByName;
    InlineAttribute isInline;
    CodePtr code;
    EnvPtr env;

    ObjectPtr callable;
    vector<TypePtr> argsKey;

    vector<TypePtr> fixedArgTypes;
    vector<IdentifierPtr> fixedArgNames;
    IdentifierPtr varArgName;
    vector<TypePtr> varArgTypes;
    unsigned varArgPosition;

    MatchSuccess(bool callByName, InlineAttribute isInline, CodePtr code, EnvPtr env,
                 ObjectPtr callable, const vector<TypePtr> &argsKey)
        : MatchResult(MATCH_SUCCESS), callByName(callByName),
          isInline(isInline), code(code), env(env), callable(callable),
          argsKey(argsKey), varArgPosition(0) {}
};
typedef Pointer<MatchSuccess> MatchSuccessPtr;

struct MatchCallableError : public MatchResult {
    ExprPtr patternExpr;
    ObjectPtr callable;
    MatchCallableError(ExprPtr patternExpr, ObjectPtr callable)
        : MatchResult(MATCH_CALLABLE_ERROR), patternExpr(patternExpr), callable(callable) {}
};

struct MatchArityError : public MatchResult {
    unsigned expectedArgs;
    unsigned gotArgs;
    bool variadic;
    MatchArityError(unsigned expectedArgs, unsigned gotArgs, bool variadic)
        : MatchResult(MATCH_ARITY_ERROR),
          expectedArgs(expectedArgs),
          gotArgs(gotArgs),
          variadic(variadic) {}
};

struct MatchArgumentError : public MatchResult {
    unsigned argIndex;
    TypePtr type;
    FormalArgPtr arg;
    MatchArgumentError(unsigned argIndex, TypePtr type, FormalArgPtr arg)
        : MatchResult(MATCH_ARGUMENT_ERROR), argIndex(argIndex),
            type(type), arg(arg) {}
};

struct MatchMultiArgumentError : public MatchResult {
    unsigned argIndex;
    MultiStaticPtr types;
    FormalArgPtr varArg;
    MatchMultiArgumentError(unsigned argIndex, MultiStaticPtr types, FormalArgPtr varArg)
        : MatchResult(MATCH_MULTI_ARGUMENT_ERROR),
            argIndex(argIndex), types(types), varArg(varArg) {}
};

struct MatchPredicateError : public MatchResult {
    ExprPtr predicateExpr;
    MatchPredicateError(ExprPtr predicateExpr)
        : MatchResult(MATCH_PREDICATE_ERROR), predicateExpr(predicateExpr) {}
};

struct MatchBindingError : public MatchResult {
    unsigned argIndex;
    TypePtr type;
    FormalArgPtr arg;
    MatchBindingError(unsigned argIndex, TypePtr type, FormalArgPtr arg)
        : MatchResult(MATCH_BINDING_ERROR), argIndex(argIndex),
            type(type), arg(arg) {}
};

struct MatchMultiBindingError : public MatchResult {
    unsigned argIndex;
    MultiStaticPtr types;
    FormalArgPtr varArg;
    MatchMultiBindingError(unsigned argIndex, MultiStaticPtr types, FormalArgPtr varArg)
        : MatchResult(MATCH_MULTI_BINDING_ERROR),
            argIndex(argIndex), types(types), varArg(varArg) {}
};

void initializePatternEnv(EnvPtr patternEnv, const vector<PatternVar> &pvars, vector<PatternCellPtr> &cells, vector<MultiPatternCellPtr> &multiCells);
    
MatchResultPtr matchInvoke(OverloadPtr overload,
                           ObjectPtr callable,
                           const vector<TypePtr> &argsKey);

void printMatchError(llvm::raw_ostream &os, MatchResultPtr result);



//
// invoke tables
//

const vector<OverloadPtr> &callableOverloads(ObjectPtr x);

struct InvokeSet;

struct InvokeEntry {
    InvokeSet *parent;
    ObjectPtr callable;
    vector<TypePtr> argsKey;
    vector<bool> forwardedRValueFlags;

    bool analyzed;
    bool analyzing;

    CodePtr origCode;
    CodePtr code;
    EnvPtr env;
    EnvPtr interfaceEnv;

    vector<TypePtr> fixedArgTypes;
    vector<IdentifierPtr> fixedArgNames;
    IdentifierPtr varArgName;
    vector<TypePtr> varArgTypes;
    unsigned varArgPosition;

    bool callByName; // if callByName the rest of InvokeEntry is not set
    InlineAttribute isInline;

    ObjectPtr analysis;
    vector<bool> returnIsRef;
    vector<TypePtr> returnTypes;

    llvm::Function *llvmFunc;
    llvm::Function *llvmCWrappers[CC_Count];

    llvm::TrackingVH<llvm::MDNode> debugInfo;

    InvokeEntry(InvokeSet *parent,
                ObjectPtr callable,
                const vector<TypePtr> &argsKey)
        : parent(parent), varArgPosition(0),
          callable(callable), argsKey(argsKey),
          analyzed(false), analyzing(false), callByName(false),
          isInline(IGNORE), llvmFunc(NULL), debugInfo(NULL)
    {
        for (size_t i = 0; i < CC_Count; ++i)
            llvmCWrappers[i] = NULL;
    }

    llvm::DISubprogram getDebugInfo() { return llvm::DISubprogram(debugInfo); }
};

extern vector<OverloadPtr> patternOverloads;

struct InvokeSet {
    ObjectPtr callable;
    vector<TypePtr> argsKey;
    OverloadPtr interface;
    vector<OverloadPtr> overloads;

    vector<MatchSuccessPtr> matches;
    unsigned nextOverloadIndex;

    bool shouldLog;

    map<vector<ValueTempness>, InvokeEntry*> tempnessMap;
    map<vector<ValueTempness>, InvokeEntry*> tempnessMap2;

    InvokeSet(ObjectPtr callable,
              const vector<TypePtr> &argsKey,
              OverloadPtr symbolInterface,
              const vector<OverloadPtr> &symbolOverloads)
        : callable(callable), argsKey(argsKey),
          interface(symbolInterface),
          overloads(symbolOverloads), nextOverloadIndex(0),
          shouldLog(false)
    {
        overloads.insert(overloads.end(), patternOverloads.begin(), patternOverloads.end());
    }
};

typedef vector< pair<OverloadPtr, MatchResultPtr> > MatchFailureVector;

struct MatchFailureError {
    MatchFailureVector failures;
    bool failedInterface;

    MatchFailureError() : failedInterface(false) {}
};

InvokeSet *lookupInvokeSet(ObjectPtr callable,
                           const vector<TypePtr> &argsKey);
InvokeEntry* lookupInvokeEntry(ObjectPtr callable,
                               const vector<TypePtr> &argsKey,
                               const vector<ValueTempness> &argsTempness,
                               MatchFailureError &failures);

void matchBindingError(MatchResultPtr result);
void matchFailureLog(MatchFailureError const &err);
void matchFailureError(MatchFailureError const &err);



//
// constructors
//

bool isOverloadablePrimOp(ObjectPtr x);
vector<OverloadPtr> &primOpOverloads(PrimOpPtr x);
void addPrimOpOverload(PrimOpPtr x, OverloadPtr overload);
void addPatternOverload(OverloadPtr x);
void initTypeOverloads(TypePtr t);
void initBuiltinConstructor(RecordPtr x);



//
// literals
//

ValueHolderPtr parseIntLiteral(ModulePtr module, IntLiteral *x);
ValueHolderPtr parseFloatLiteral(ModulePtr module, FloatLiteral *x);


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

void disableAnalysisCaching();
void enableAnalysisCaching();

struct AnalysisCachingDisabler {
    AnalysisCachingDisabler() { disableAnalysisCaching(); }
    ~AnalysisCachingDisabler() { enableAnalysisCaching(); }
};


PVData safeAnalyzeOne(ExprPtr expr, EnvPtr env);
MultiPValuePtr safeAnalyzeMulti(ExprListPtr exprs, EnvPtr env, unsigned wantCount);
MultiPValuePtr safeAnalyzeExpr(ExprPtr expr, EnvPtr env);
MultiPValuePtr safeAnalyzeIndexingExpr(ExprPtr indexable,
                                       ExprListPtr args,
                                       EnvPtr env);
MultiPValuePtr safeAnalyzeMultiArgs(ExprListPtr exprs,
                                    EnvPtr env,
                                    vector<unsigned> &dispatchIndices);
InvokeEntry* safeAnalyzeCallable(ObjectPtr x,
                                 const vector<TypePtr> &argsKey,
                                 const vector<ValueTempness> &argsTempness);
MultiPValuePtr safeAnalyzeCallByName(InvokeEntry* entry,
                                     ExprPtr callable,
                                     ExprListPtr args,
                                     EnvPtr env);
MultiPValuePtr safeAnalyzeGVarInstance(GVarInstancePtr x);

MultiPValuePtr analyzeMulti(ExprListPtr exprs, EnvPtr env, unsigned wantCount);
PVData analyzeOne(ExprPtr expr, EnvPtr env);

MultiPValuePtr analyzeMultiArgs(ExprListPtr exprs,
                                EnvPtr env,
                                vector<unsigned> &dispatchIndices);
PVData analyzeOneArg(ExprPtr x,
                        EnvPtr env,
                        unsigned startIndex,
                        vector<unsigned> &dispatchIndices);
MultiPValuePtr analyzeArgExpr(ExprPtr x,
                              EnvPtr env,
                              unsigned startIndex,
                              vector<unsigned> &dispatchIndices);

MultiPValuePtr analyzeExpr(ExprPtr expr, EnvPtr env);
MultiPValuePtr analyzeStaticObject(ObjectPtr x);

GVarInstancePtr lookupGVarInstance(GlobalVariablePtr x,
                                   const vector<ObjectPtr> &params);
GVarInstancePtr defaultGVarInstance(GlobalVariablePtr x);
GVarInstancePtr analyzeGVarIndexing(GlobalVariablePtr x,
                                    ExprListPtr args,
                                    EnvPtr env);
MultiPValuePtr analyzeGVarInstance(GVarInstancePtr x);

PVData analyzeExternalVariable(ExternalVariablePtr x);
void analyzeExternalProcedure(ExternalProcedurePtr x);
void verifyAttributes(ExternalProcedurePtr x);
void verifyAttributes(ExternalVariablePtr x);
void verifyAttributes(ModulePtr x);
MultiPValuePtr analyzeIndexingExpr(ExprPtr indexable,
                                   ExprListPtr args,
                                   EnvPtr env);
bool unwrapByRef(TypePtr &t);
TypePtr constructType(ObjectPtr constructor, MultiStaticPtr args);
PVData analyzeTypeConstructor(ObjectPtr obj, MultiStaticPtr args);
MultiPValuePtr analyzeAliasIndexing(GlobalAliasPtr x,
                                    ExprListPtr args,
                                    EnvPtr env);
void computeArgsKey(MultiPValuePtr args,
                    vector<TypePtr> &argsKey,
                    vector<ValueTempness> &argsTempness);
MultiPValuePtr analyzeReturn(const vector<bool> &returnIsRef,
                             const vector<TypePtr> &returnTypes);
MultiPValuePtr analyzeCallExpr(ExprPtr callable,
                               ExprListPtr args,
                               EnvPtr env);
PVData analyzeDispatchIndex(PVData const &pv, int tag);
MultiPValuePtr analyzeDispatch(ObjectPtr obj,
                               MultiPValuePtr args,
                               const vector<unsigned> &dispatchIndices);
MultiPValuePtr analyzeCallValue(PVData const &callable,
                                MultiPValuePtr args);
MultiPValuePtr analyzeCallPointer(PVData const &x,
                                  MultiPValuePtr args);
bool analyzeIsDefined(ObjectPtr x,
                      const vector<TypePtr> &argsKey,
                      const vector<ValueTempness> &argsTempness);
InvokeEntry* analyzeCallable(ObjectPtr x,
                             const vector<TypePtr> &argsKey,
                             const vector<ValueTempness> &argsTempness);

MultiPValuePtr analyzeCallByName(InvokeEntry* entry,
                                 ExprPtr callable,
                                 ExprListPtr args,
                                 EnvPtr env);

void analyzeCodeBody(InvokeEntry* entry);

struct AnalysisContext {
    bool hasRecursivePropagation;
    bool returnInitialized;
    vector<bool> returnIsRef;
    vector<TypePtr> returnTypes;
    AnalysisContext()
        : hasRecursivePropagation(false),
          returnInitialized(false) {}
};

enum StatementAnalysis {
    SA_FALLTHROUGH,
    SA_RECURSIVE,
    SA_TERMINATED
};

StatementAnalysis analyzeStatement(StatementPtr stmt, EnvPtr env, AnalysisContext *ctx);
void initializeStaticForClones(StaticForPtr x, unsigned count);
EnvPtr analyzeBinding(BindingPtr x, EnvPtr env);
bool returnKindToByRef(ReturnKind returnKind, PVData const &pv);

MultiPValuePtr analyzePrimOp(PrimOpPtr x, MultiPValuePtr args);

ObjectPtr unwrapStaticType(TypePtr t);

bool staticToBool(ObjectPtr x, bool &out, TypePtr &type);
bool staticToBool(MultiStaticPtr x, unsigned index);
bool staticToCallingConv(ObjectPtr x, CallingConv &out);
CallingConv staticToCallingConv(MultiStaticPtr x, unsigned index);


//
// evaluator
//

bool staticToType(ObjectPtr x, TypePtr &out);
TypePtr staticToType(MultiStaticPtr x, unsigned index);

void evaluateReturnSpecs(const vector<ReturnSpecPtr> &returnSpecs,
                         ReturnSpecPtr varReturnSpec,
                         EnvPtr env,
                         vector<bool> &isRef,
                         vector<TypePtr> &types);

MultiStaticPtr evaluateExprStatic(ExprPtr expr, EnvPtr env);
ObjectPtr evaluateOneStatic(ExprPtr expr, EnvPtr env);
MultiStaticPtr evaluateMultiStatic(ExprListPtr exprs, EnvPtr env);

TypePtr evaluateType(ExprPtr expr, EnvPtr env);
void evaluateMultiType(ExprListPtr exprs, EnvPtr env, vector<TypePtr> &out);
IdentifierPtr evaluateIdentifier(ExprPtr expr, EnvPtr env);
bool evaluateBool(ExprPtr expr, EnvPtr env);
void evaluatePredicate(const vector<PatternVar> &patternVars,
    ExprPtr expr, EnvPtr env);

ValueHolderPtr intToValueHolder(int x);
ValueHolderPtr sizeTToValueHolder(size_t x);
ValueHolderPtr ptrDiffTToValueHolder(ptrdiff_t x);
ValueHolderPtr boolToValueHolder(bool x);

size_t valueHolderToSizeT(ValueHolderPtr vh);

ObjectPtr makeTupleValue(const vector<ObjectPtr> &elements);
void extractTupleElements(EValuePtr ev,
                          vector<ObjectPtr> &elements);
ObjectPtr evalueToStatic(EValuePtr ev);

struct EValue : public Object {
    TypePtr type;
    char *addr;
    bool forwardedRValue;
    EValue(TypePtr type, char *addr)
        : Object(EVALUE), type(type), addr(addr),
          forwardedRValue(false) {}

    template<typename T>
    T &as() { return *(T*)addr; }

    template<typename T>
    T const &as() const { return *(T const *)addr; }
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
    size_t size() { return values.size(); }
    void add(EValuePtr x) { values.push_back(x); }
    void add(MultiEValuePtr x) {
        values.insert(values.end(), x->values.begin(), x->values.end());
    }
};

void evalValueInit(EValuePtr dest);
void evalValueDestroy(EValuePtr dest);
void evalValueCopy(EValuePtr dest, EValuePtr src);
void evalValueMove(EValuePtr dest, EValuePtr src);
void evalValueAssign(EValuePtr dest, EValuePtr src);
void evalValueMoveAssign(EValuePtr dest, EValuePtr src);
bool evalToBoolFlag(EValuePtr a);

int evalMarkStack();
void evalDestroyStack(int marker);
void evalPopStack(int marker);
void evalDestroyAndPopStack(int marker);
EValuePtr evalAllocValue(TypePtr t);

EValuePtr evalOneAsRef(ExprPtr expr, EnvPtr env);


//
// codegen
//

static const unsigned short DW_LANG_user_CLAY = 0xC1A4;

extern llvm::Module *llvmModule;
extern llvm::DIBuilder *llvmDIBuilder;
extern const llvm::TargetData *llvmTargetData;

llvm::PointerType *exceptionReturnType();
llvm::Value *noExceptionReturnValue();

llvm::TargetMachine *initLLVM(llvm::StringRef targetTriple,
    llvm::StringRef name,
    llvm::StringRef flags,
    bool relocPic,
    bool debug,
    bool optimized);

bool inlineEnabled();
void setInlineEnabled(bool enabled);
bool exceptionsEnabled();
void setExceptionsEnabled(bool enabled);

struct CValue : public Object {
    TypePtr type;
    llvm::Value *llValue;
    bool forwardedRValue;
    CValue(TypePtr type, llvm::Value *llValue)
        : Object(CVALUE), type(type), llValue(llValue),
          forwardedRValue(false)
    {
        llvmType(type); // force full definition of type
    }
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
    size_t size() { return values.size(); }
    void add(CValuePtr x) { values.push_back(x); }
    void add(MultiCValuePtr x) {
        values.insert(values.end(), x->values.begin(), x->values.end());
    }
};

struct JumpTarget {
    llvm::BasicBlock *block;
    int stackMarker;
    int useCount;
    JumpTarget() : block(NULL), stackMarker(-1), useCount(0) {}
    JumpTarget(llvm::BasicBlock *block, int stackMarker)
        : block(block), stackMarker(stackMarker), useCount(0) {}
};

struct CReturn {
    bool byRef;
    TypePtr type;
    CValuePtr value;
    CReturn(bool byRef, TypePtr type, CValuePtr value)
        : byRef(byRef), type(type), value(value) {}
};

struct StackSlot {
    llvm::Type *llType;
    llvm::Value *llValue;
    StackSlot(llvm::Type *llType, llvm::Value *llValue)
        : llType(llType), llValue(llValue) {}
};

enum ValueStackEntryType {
    LOCAL_VALUE,
    FINALLY_STATEMENT,
    ONERROR_STATEMENT,
};

struct ValueStackEntry {
    ValueStackEntryType type;
    CValuePtr value;
    EnvPtr statementEnv;
    StatementPtr statement;

    explicit ValueStackEntry(CValuePtr value)
        : type(LOCAL_VALUE), value(value), statementEnv(NULL), statement(NULL) {}
    ValueStackEntry(ValueStackEntryType type,
        EnvPtr statementEnv,
        StatementPtr statement)
        : type(type), value(NULL), statementEnv(statementEnv), statement(statement)
    {
        assert(type != LOCAL_VALUE);
    }
};

struct CodegenContext {
    llvm::Function *llvmFunc;
    vector<llvm::TrackingVH<llvm::MDNode> > debugScope;
    llvm::IRBuilder<> *initBuilder;
    llvm::IRBuilder<> *builder;

    vector<ValueStackEntry> valueStack;
    llvm::Value *valueForStatics;

    vector<StackSlot> allocatedSlots;
    vector<StackSlot> discardedSlots;

    vector< vector<CReturn> > returnLists;
    vector<JumpTarget> returnTargets;
    llvm::StringMap<JumpTarget> labels;
    vector<JumpTarget> breaks;
    vector<JumpTarget> continues;
    vector<JumpTarget> exceptionTargets;
    bool checkExceptions;
    llvm::Value *exceptionValue;
    int inlineDepth;

    CodegenContext()
        : llvmFunc(NULL),
          initBuilder(NULL),
          builder(NULL),
          valueForStatics(NULL),
          checkExceptions(true),
          exceptionValue(NULL),
          inlineDepth(0)
    {
    }

    CodegenContext(llvm::Function *llvmFunc)
        : llvmFunc(llvmFunc),
          initBuilder(NULL),
          builder(NULL),
          valueForStatics(NULL),
          checkExceptions(true),
          exceptionValue(NULL),
          inlineDepth(0)
    {
    }

    ~CodegenContext() {
        delete builder;
        delete initBuilder;
    }

    llvm::DILexicalBlock getDebugScope() {
        if (debugScope.empty())
            return llvm::DILexicalBlock(NULL);
        else
            return llvm::DILexicalBlock(debugScope.back());
    }

    void pushDebugScope(llvm::DILexicalBlock scope) {
        debugScope.push_back((llvm::MDNode*)scope);
    }
    void popDebugScope() {
        debugScope.pop_back();
    }
};

struct DebugLocationContext {
    Location loc;
    CodegenContext* ctx;
    DebugLocationContext(Location const &loc, CodegenContext* ctx)
        : loc(loc), ctx(ctx)
    {
        if (loc.ok()) {
            pushLocation(loc);
            if (llvmDIBuilder != NULL && ctx->inlineDepth == 0) {
                int line, column;
                getDebugLineCol(loc, line, column);
                llvm::DebugLoc debugLoc = llvm::DebugLoc::get(line, column, ctx->getDebugScope());
                ctx->builder->SetCurrentDebugLocation(debugLoc);
            }
        }
    }
    ~DebugLocationContext() {
        if (loc.ok()) {
            popLocation();
        }
    }
private :
    DebugLocationContext(const DebugLocationContext &) {}
    void operator=(const DebugLocationContext &) {}
};

void codegenGVarInstance(GVarInstancePtr x);
void codegenExternalVariable(ExternalVariablePtr x);
void codegenExternalProcedure(ExternalProcedurePtr x, bool codegenBody);

InvokeEntry* codegenCallable(ObjectPtr x,
                             const vector<TypePtr> &argsKey,
                             const vector<ValueTempness> &argsTempness);
void codegenCodeBody(InvokeEntry* entry);
void codegenCWrapper(InvokeEntry* entry);

void codegenEntryPoints(ModulePtr module, bool importedExternals);
void codegenMain(ModulePtr module);

static ExprPtr implicitUnpackExpr(unsigned wantCount, ExprListPtr exprs) {
    if (wantCount >= 1 && exprs->size() == 1 && exprs->exprs[0]->exprKind != UNPACK)
        return exprs->exprs[0];
    else
        return NULL;
}

// externals

struct ExternalTarget : public Object {
    ExternalTarget() : Object(DONT_CARE) {}
    virtual ~ExternalTarget() {}

    virtual llvm::CallingConv::ID callingConvention(CallingConv conv) = 0;

    virtual llvm::Type *typeReturnsAsBitcastType(CallingConv conv, TypePtr type) = 0;
    virtual llvm::Type *typePassesAsBitcastType(CallingConv conv, TypePtr type, bool varArg) = 0;
    virtual bool typeReturnsBySretPointer(CallingConv conv, TypePtr type) = 0;
    virtual bool typePassesByByvalPointer(CallingConv conv, TypePtr type, bool varArg) = 0;

    // for generating C function declarations
    llvm::Type *pushReturnType(CallingConv conv,
                               TypePtr type,
                               vector<llvm::Type *> &llArgTypes,
                               vector< pair<unsigned, llvm::Attributes> > &llAttributes);
    void pushArgumentType(CallingConv conv,
                          TypePtr type,
                          vector<llvm::Type *> &llArgTypes,
                          vector< pair<unsigned, llvm::Attributes> > &llAttributes);

    // for generating C function definitions
    void allocReturnValue(CallingConv conv,
                          TypePtr type,
                          llvm::Function::arg_iterator &ai,
                          vector<CReturn> &returns,
                          CodegenContext* ctx);
    CValuePtr allocArgumentValue(CallingConv conv,
                                 TypePtr type,
                                 llvm::StringRef name,
                                 llvm::Function::arg_iterator &ai,
                                 CodegenContext* ctx);
    void returnStatement(CallingConv conv,
                         TypePtr type,
                         vector<CReturn> &returns,
                         CodegenContext* ctx);

    // for calling C functions
    void loadStructRetArgument(CallingConv conv,
                               TypePtr type,
                               vector<llvm::Value *> &llArgs,
                               vector< pair<unsigned, llvm::Attributes> > &llAttributes,
                               CodegenContext* ctx,
                               MultiCValuePtr out);
    void loadArgument(CallingConv conv,
                      CValuePtr cv,
                      vector<llvm::Value *> &llArgs,
                      vector< pair<unsigned, llvm::Attributes> > &llAttributes,
                      CodegenContext* ctx);
    void loadVarArgument(CallingConv conv,
                         CValuePtr cv,
                         vector<llvm::Value *> &llArgs,
                         vector< pair<unsigned, llvm::Attributes> > &llAttributes,
                         CodegenContext* ctx);
    void storeReturnValue(CallingConv conv,
                          llvm::Value *callReturnValue,
                          TypePtr returnType,
                          CodegenContext* ctx,
                          MultiCValuePtr out);
};

typedef Pointer<ExternalTarget> ExternalTargetPtr;

void initExternalTarget(string target);
ExternalTargetPtr getExternalTarget();

//
// parachute
//

int parachute(int (*mainfn)(int, char **, char const* const*),
    int argc, char **argv, char const* const* envp);

}

#include "operators.hpp"
#include "hirestimer.hpp"

#endif
