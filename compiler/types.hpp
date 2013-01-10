#ifndef __TYPES_HPP
#define __TYPES_HPP

#include "clay.hpp"

namespace clay {

//
// types module
//

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

TypePtr integerType(unsigned int bits, bool isSigned);
TypePtr intType(unsigned int bits);
TypePtr uintType(unsigned int bits);
TypePtr floatType(unsigned int bits);
TypePtr imagType(unsigned int bits);
TypePtr complexType(unsigned int bits);
TypePtr pointerType(TypePtr pointeeType);
TypePtr codePointerType(llvm::ArrayRef<TypePtr> argTypes,
                        llvm::ArrayRef<uint8_t> returnIsRef,
                        llvm::ArrayRef<TypePtr> returnTypes);
TypePtr cCodePointerType(CallingConv callingConv,
                         llvm::ArrayRef<TypePtr> argTypes,
                         bool hasVarArgs,
                         TypePtr returnType);
TypePtr arrayType(TypePtr elememtType, unsigned int size);
TypePtr vecType(TypePtr elementType, unsigned int size);
TypePtr tupleType(llvm::ArrayRef<TypePtr> elementTypes);
TypePtr unionType(llvm::ArrayRef<TypePtr> memberTypes);
TypePtr recordType(RecordDeclPtr record, llvm::ArrayRef<ObjectPtr> params);
TypePtr variantType(VariantDeclPtr variant, llvm::ArrayRef<ObjectPtr> params);
TypePtr staticType(ObjectPtr obj);
TypePtr enumType(EnumDeclPtr enumeration);
TypePtr newType(NewTypeDeclPtr newtype);

bool isPrimitiveType(TypePtr t);
bool isPrimitiveAggregateType(TypePtr t);
bool isPrimitiveAggregateTooLarge(TypePtr t);
bool isPointerOrCodePointerType(TypePtr t);
bool isStaticOrTupleOfStatics(TypePtr t);

void initializeRecordFields(RecordTypePtr t);
llvm::ArrayRef<IdentifierPtr> recordFieldNames(RecordTypePtr t);
llvm::ArrayRef<TypePtr> recordFieldTypes(RecordTypePtr t);
const llvm::StringMap<size_t> &recordFieldIndexMap(RecordTypePtr t);

llvm::ArrayRef<TypePtr> variantMemberTypes(VariantTypePtr t);
TypePtr variantReprType(VariantTypePtr t);
int dispatchTagCount(TypePtr t);
TypePtr newtypeReprType(NewTypePtr t);

void initializeEnumType(EnumTypePtr t);
void initializeNewType(NewTypePtr t);

const llvm::StructLayout *tupleTypeLayout(TupleType *t);
const llvm::StructLayout *complexTypeLayout(ComplexType *t);
const llvm::StructLayout *recordTypeLayout(RecordType *t);

llvm::Type *llvmIntType(unsigned int bits);
llvm::Type *llvmFloatType(unsigned int bits);
llvm::PointerType *llvmPointerType(llvm::Type *llType);
llvm::PointerType *llvmPointerType(TypePtr t);
llvm::Type *llvmArrayType(llvm::Type *llType, unsigned int size);
llvm::Type *llvmArrayType(TypePtr type, unsigned int size);
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
