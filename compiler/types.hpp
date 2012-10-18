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
TypePtr recordType(RecordDeclPtr record, const vector<ObjectPtr> &params);
TypePtr variantType(VariantDeclPtr variant, const vector<ObjectPtr> &params);
TypePtr staticType(ObjectPtr obj);
TypePtr enumType(EnumDeclPtr enumeration);
TypePtr newType(NewTypeDeclPtr newtype);

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
TypePtr newtypeReprType(NewTypePtr t);

void initializeEnumType(EnumTypePtr t);
void initializeNewType(NewTypePtr t);

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

}

#endif // __TYPES_HPP
