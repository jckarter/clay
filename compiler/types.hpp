#ifndef __TYPES_HPP
#define __TYPES_HPP

#include "clay.hpp"

namespace clay {

//
// types module
//

void initTypes(CompilerStatePtr cst);

TypePtr integerType(unsigned bits, bool isSigned, CompilerStatePtr cst);
TypePtr intType(unsigned bits, CompilerStatePtr cst);
TypePtr uintType(unsigned bits, CompilerStatePtr cst);
TypePtr floatType(unsigned bits, CompilerStatePtr cst);
TypePtr imagType(unsigned bits, CompilerStatePtr cst);
TypePtr complexType(unsigned bits, CompilerStatePtr cst);
TypePtr pointerType(TypePtr pointeeType);
TypePtr codePointerType(llvm::ArrayRef<TypePtr> argTypes,
                        llvm::ArrayRef<uint8_t> returnIsRef,
                        llvm::ArrayRef<TypePtr> returnTypes,
                        CompilerStatePtr cst);
TypePtr cCodePointerType(CallingConv callingConv,
                         llvm::ArrayRef<TypePtr> argTypes,
                         bool hasVarArgs,
                         TypePtr returnType,
                         CompilerStatePtr cst);
TypePtr arrayType(TypePtr elememtType, unsigned size);
TypePtr vecType(TypePtr elementType, unsigned size);
TypePtr tupleType(llvm::ArrayRef<TypePtr> elementTypes,
                  CompilerStatePtr cst);
TypePtr unionType(llvm::ArrayRef<TypePtr> memberTypes,
                  CompilerStatePtr cst);
TypePtr recordType(RecordDeclPtr record,
                   llvm::ArrayRef<ObjectPtr> params);
TypePtr variantType(VariantDeclPtr variant, llvm::ArrayRef<ObjectPtr> params);
TypePtr staticType(ObjectPtr obj, CompilerStatePtr cst);
TypePtr enumType(EnumDeclPtr enumeration, CompilerStatePtr cst);
TypePtr newType(NewTypeDeclPtr newtype);

bool isPrimitiveType(TypePtr t);
bool isPrimitiveAggregateType(TypePtr t);
bool isPrimitiveAggregateTooLarge(TypePtr t);
bool isPointerOrCodePointerType(TypePtr t);
bool isStaticOrTupleOfStatics(TypePtr t);

void initializeRecordFields(RecordTypePtr t, CompilerStatePtr cst);
llvm::ArrayRef<IdentifierPtr> recordFieldNames(RecordTypePtr t);
llvm::ArrayRef<TypePtr> recordFieldTypes(RecordTypePtr t);
const llvm::StringMap<size_t> &recordFieldIndexMap(RecordTypePtr t);

llvm::ArrayRef<TypePtr> variantMemberTypes(VariantTypePtr t);
TypePtr variantReprType(VariantTypePtr t);
unsigned dispatchTagCount(TypePtr t);
TypePtr newtypeReprType(NewTypePtr t);

void initializeEnumType(EnumTypePtr t);
void initializeNewType(NewTypePtr t);

const llvm::StructLayout *tupleTypeLayout(TupleType *t);
const llvm::StructLayout *complexTypeLayout(ComplexType *t);
const llvm::StructLayout *recordTypeLayout(RecordType *t);

llvm::Type *llvmIntType(unsigned bits);
llvm::Type *llvmFloatType(unsigned bits);
llvm::PointerType *llvmPointerType(llvm::Type *llType);
llvm::PointerType *llvmPointerType(TypePtr t);
llvm::Type *llvmArrayType(llvm::Type *llType, unsigned size);
llvm::Type *llvmArrayType(TypePtr type, unsigned size);
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

}

#endif // __TYPES_HPP
