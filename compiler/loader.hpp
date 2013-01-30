#ifndef __LOADER_HPP
#define __LOADER_HPP

#include "clay.hpp"

namespace clay {


void addOverload(vector<OverloadPtr> &overloads, OverloadPtr &x);
void addProcedureOverload(ProcedurePtr proc, EnvPtr Env, OverloadPtr x);
void getProcedureMonoTypes(ProcedureMono &mono, EnvPtr env,
    llvm::ArrayRef<FormalArgPtr> formalArgs, bool hasVarArg);

void initLoader(CompilerState* cst);
void setSearchPath(const llvm::ArrayRef<PathString> path,
                   CompilerState* cst);
ModulePtr loadProgram(llvm::StringRef fileName, vector<string> *sourceFiles,
                      bool verbose, bool repl, CompilerState* cst);
ModulePtr loadProgramSource(llvm::StringRef name, llvm::StringRef source, 
                            bool verbose, bool repl, CompilerState* cst);
ModulePtr loadedModule(llvm::StringRef module, CompilerState* cst);
ModulePtr preludeModule(CompilerState* cst);
ModulePtr primitivesModule(CompilerState* cst);
ModulePtr operatorsModule(CompilerState* cst);
ModulePtr intrinsicsModule(CompilerState* cst);
ModulePtr staticModule(ObjectPtr x, CompilerState* cst);

void addGlobals(ModulePtr m, llvm::ArrayRef<TopLevelItemPtr> toplevels);
void loadDependent(ModulePtr m, vector<string> *sourceFiles,
                   ImportPtr dependent, bool verbose);
void initModule(ModulePtr m);



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
    PRIM_recordVariadicField,

    PRIM_VariantP,
    PRIM_VariantMemberIndex,
    PRIM_VariantMemberCount,
    PRIM_VariantMembers,
    PRIM_variantRepr,

    PRIM_BaseType,

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

    PRIM_GetOverload,

    PRIM_usuallyEquals
};

struct PrimOp : public Object {
    int primOpCode;
    CompilerState* cst;
    PrimOp(int primOpCode, CompilerState* cst)
        : Object(PRIM_OP), primOpCode(primOpCode), cst(cst) {}
};

llvm::StringRef primOpName(const PrimOpPtr& x);

}

#endif // __LOADER_HPP
