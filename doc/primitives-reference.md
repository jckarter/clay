# <a name="clayprogramminglanguagereference"></a>The Clay Programming Language
## Primitives Reference, version 0.1

This document describes the contents of the `__primitives__` module synthesized by the Clay compiler and implicitly available to all Clay programs. This module defines primitive types, fundamental operations, and tools for compile-time introspection of programs. For information about the Clay language itself, consult the [Language Reference](language-reference.md).

* [Primitive types]
    * `Pointer`
    * `CodePointer`
    * `CCodePointer`
    * `LLVMCodePointer`
    * `VarArgsCCodePointer`
    * `StdCallCodePointer`
    * `FastCallCodePointer`
    * `ThisCallCodePointer`
    * `Array`
    * `Vec`
    * `Tuple`
    * `Union`
    * `Static`

* [Data access operations]
    * `primitiveCopy`
    * `arrayRef`
    * `arrayElements`
    * `tupleRef`
    * `tupleElements`
    * `recordFieldRef`
    * `recordFieldRefByName`
    * `recordFields`
    * `enumToInt`
    * `intToEnum`
* [Numeric operations]
    * `boolNot`
    * `numericEquals?`
    * `numericLesser?`
    * `numericAdd`
    * `numericSubtract`
    * `numericMultiply`
    * `numericDivide`
    * `numericNegate`
    * `integerRemainder`
    * `integerShiftLeft`
    * `integerShiftRight`
    * `integerBitwiseAnd`
    * `integerBitwiseOr`
    * `integerBitwiseXor`
    * `integerBitwiseNot`
    * `numericConvert`
    * `integerAddChecked`
    * `integerSubtractChecked`
    * `integerMultiplyChecked`
    * `integerDivideChecked`
    * `integerRemainderChecked`
    * `integerNegateChecked`
    * `integerShiftLeftChecked`
    * `integerConvertChecked`
* [Checked integer operations] 

* [Pointer operations]
    * `addressOf`
    * `pointerDereference`
    * `pointerEquals?`
    * `pointerLesser?`
    * `pointerOffset`
    * `pointerToInt`
    * `intToPointer`
    * `pointerCast`
* [Function pointer operations]
    * `makeCodePointer`
    * `makeCCodePointer`
    * `callCCodePointer`
* [Atomic memory access operations]
    * `atomicFence`
    * `atomicRMW`
    * `atomicLoad`
    * `atomicStore`
    * `atomicCompareExchange`

    * `OrderUnordered`
    * `OrderMonotonic`
    * `OrderAcquire`
    * `OrderRelease`
    * `OrderAcqRel`
    * `OrderSeqCst`

    * `RMWXchg`
    * `RMWAdd`
    * `RMWSubtract`
    * `RMWAnd`
    * `RMWNAnd`
    * `RMWOr`
    * `RMWXor`
    * `RMWMin`
    * `RMWMax`
    * `RMWUMin`
    * `RMWUMax`

* [Exception handling]
    PRIM_activeException
* [Symbol and function introspection]
    * `Type?`
    * `CallDefined?`
    * `ModuleName`
    * `StaticName`
    * `staticFieldRef`
* [Static string manipulation]
    * `Identifier?`
    * `IdentifierSize`
    * `IdentifierConcat`
    * `IdentifierSlice`
    * `IdentifierModuleName`
    * `IdentifierStaticName`
* [Type introspection]
    * `TypeSize`
    * `TypeAlignment`
    * `CCodePointer?`
    * `TupleElementCount`
    * `UnionMemberCount`
    * `Record?`
    * `RecordFieldCount`
    * `RecordFieldName`
    * `RecordWithField?`
    * `Variant?`
    * `VariantMemberIndex`
    * `VariantMemberCount`
    * `Enum?`
    * `EnumMemberCount`
    * `EnumMemberName`
* [External function attributes]
    * `AttributeStdCall`
    * `AttributeFastCall`
    * `AttributeThisCall`
    * `AttributeCCall`
    * `AttributeLLVMCall`
    * `AttributeDLLImport`
    * `AttributeDLLExport`
* [Compiler flags]
    * `Flag?`
    * `Flag`
* [Miscellaneous]
    * `staticIntegers`
