# <a name="clayprimitivesreference"></a>The Clay Programming Language
## Primitives Reference, version 0.1

This document describes the contents of the `__primitives__` module synthesized by the Clay compiler and implicitly available to all Clay programs. This module provides primitive types, fundamental operations, and functions for compile-time introspection of programs. For information about the Clay language itself, consult the [Language Reference](language-reference.md).

* [Primitive types]
    * [`Bool`]
    * [Integer types]
        * `Int8`, `Int16`, `Int32`, `Int64`, `Int128`, `UInt8`, `UInt16`, `UInt32`, `UInt64`, `UInt128`
    * [Floating-point types]
        * `Float32`, `Float64`, `Float80`
    * [Imaginary and complex types]
        * `Imag32`, `Imag64`, `Imag80`, `Complex32`, `Complex64`, `Complex80`
    * [`Pointer`]
    * [`CodePointer`]
    * [External code pointer types]
        * `CCodePointer`, `VarArgsCCodePointer`, `LLVMCodePointer`, `StdCallCodePointer`, `FastCallCodePointer`, `ThisCallCodePointer`
    * [`Array`]
    * [`Vec`]
    * [`Tuple`]
    * [`Union`]
    * [`Static`]

* [Data access operations]
    * [`primitiveCopy`]
    * [`arrayRef`]
    * [`arrayElements`]
    * [`tupleRef`]
    * [`tupleElements`]
    * [`recordFieldRef`]
    * [`recordFieldRefByName`]
    * [`recordFields`]
    * [`enumToInt`]
    * [`intToEnum`]

* [Numeric operations]
    * [`boolNot`]
    * [`numericEquals?`]
    * [`numericLesser?`]
    * [`numericAdd`]
    * [`numericSubtract`]
    * [`numericMultiply`]
    * [`numericDivide`]
    * [`numericNegate`]
    * [`integerRemainder`]
    * [`integerShiftLeft`]
    * [`integerShiftRight`]
    * [`integerBitwiseAnd`]
    * [`integerBitwiseOr`]
    * [`integerBitwiseXor`]
    * [`integerBitwiseNot`]
    * [`numericConvert`]

* [Checked integer operations]
    * [`integerAddChecked`]
    * [`integerSubtractChecked`]
    * [`integerMultiplyChecked`]
    * [`integerDivideChecked`]
    * [`integerRemainderChecked`]
    * [`integerNegateChecked`]
    * [`integerShiftLeftChecked`]
    * [`integerConvertChecked`]

* [Pointer operations]
    * [`addressOf`]
    * [`pointerDereference`]
    * [`pointerEquals?`]
    * [`pointerLesser?`]
    * [`pointerOffset`]
    * [`pointerToInt`]
    * [`intToPointer`]
    * [`pointerCast`]

* [Function pointer operations]
    * [`makeCodePointer`]
    * [`makeCCodePointer`]
    * [`callCCodePointer`]

* [Atomic memory operations]
    * [Memory order symbols]
        * `OrderUnordered`, `OrderMonotonic`, `OrderAcquire`, `OrderRelease`, `OrderAcqRel`, `OrderSeqCst`
    * [`atomicLoad`]
    * [`atomicStore`]
    * [`atomicRMW`]
        * `RMWXchg`, `RMWAdd`, `RMWSubtract`, `RMWAnd`, `RMWNAnd`, `RMWOr`, `RMWXor`, `RMWMin`, `RMWMax`, `RMWUMin`, `RMWUMax`
    * [`atomicCompareExchange`]
    * [`atomicFence`]

* [Exception handling]
    * [`activeException`]

* [Symbol and function introspection]
    * [`Type?`]
    * [`CallDefined?`]
    * [`ModuleName`]
    * [`StaticName`]
    * [`staticFieldRef`]

* [Static string manipulation]
    * [`Identifier?`]
    * [`IdentifierSize`]
    * [`IdentifierConcat`]
    * [`IdentifierSlice`]
    * [`IdentifierModuleName`]
    * [`IdentifierStaticName`]

* [Type introspection]
    * [`TypeSize`]
    * [`TypeAlignment`]
    * [`CCodePointer?`]
    * [`TupleElementCount`]
    * [`UnionMemberCount`]
    * [`Record?`]
    * [`RecordFieldCount`]
    * [`RecordFieldName`]
    * [`RecordWithField?`]
    * [`Variant?`]
    * [`VariantMemberIndex`]
    * [`VariantMemberCount`]
    * [`Enum?`]
    * [`EnumMemberCount`]
    * [`EnumMemberName`]

* [Compiler flags]
    * [`ExceptionsEnabled?`]
    * [`Flag?`]
    * [`Flag`]

* [External function attributes]
    * [Calling convention attributes]
        * `AttributeCCall`, `AttributeStdCall`, `AttributeFastCall`, `AttributeThisCall`, `AttributeLLVMCall`
    * [Linkage attributes]
        * `AttributeDLLImport`, `AttributeDLLExport`

* [Miscellaneous]
    * [`staticIntegers`]

### Primitive types

Primitive types correspond more or less to the basic LLVM types, with some occasional added higher-level semantics. The primitive types are defined as overloadable symbols with no initial overloads; library code may overload primitive type symbols in order to define constructor functions for the types. Corresponding LLVM and C types for use with external function linkage is provided for each type.

#### `Bool`

The `Bool` type represents boolean values. Values of the type may only have the values `true` or `false`. This type corresponds to the LLVM `i1` type, the C99 `_Bool` type, and the C++ `bool` type. `false` in LLVM is `i1 0` and `true` is `i1 1`.

#### Integer types

Signed and unsigned integer types are provided for sizes of 8, 16, 32, 64, and 128 bits. The signed types are named `Int8`, `Int16`, etc. and the unsigned types are named `UInt8` etc. Both sets of types correspond to the LLVM `i8`, `i16`, etc. types. (LLVM does not differentiate signed from unsigned types.) They also correspond to the C99 `<stdint.h>` `int8_t` etc. and `uint8_t` etc. typedefs.

#### Floating-point types

`Float32` and `Float64` floating-point types are provided, corresponding to IEEE-754 single and double precision floating point numbers. For Unix x86 targets, the platform-specific `Float80` type is also provided. These types correspond respectively to the LLVM `float`, `double`, and `x86_fp80` types, or the C `float`, `double`, and `long double` types (the latter implemented as an extension by many x86-targeting compilers).

#### Imaginary and complex types

Imaginary and complex types are provided for each floating-point type. `Imag32` etc. correspond to the same LLVM and C types as `Float32` etc., but semantically represent imaginary values in Clay. `Complex32` etc. correspond to LLVM `{float,float}` etc. types and C99 `_Complex float` etc. types.

#### `Pointer`

`Pointer[T]` is the type of a pointer to a value of type `T`, for any type `T`. It corresponds to the `%T*` type in LLVM (where `%T` is the LLVM type of `T`), or to the `T*` type in C. Pointers are created by the prefix `&` operator, or by the equivalent [`addressOf`] primitive function.

#### `CodePointer`

`CodePointer[[..In], [..Out]]` is the type of a pointer to a Clay function, instantiated for the argument types `..In` and return types `..Out`. These pointers can be created using the [`makeCodePointer`] primitive function. The Clay calling convention is unspecified, so `CodePointer`s have no specific corresponding LLVM or C type beyond being a function pointer of some kind.

#### External code pointer types

`CCodePointer[[..In], [..Out]]` is the type of a pointer to a C function with argument types `..In` and return types `..Out`. `CCodePointer[[A,B,C],[]]` corresponds to the C `void (*)(A,B,C)` type, and `CCodePointer[[A,B,C],[D]]` to the `D (*)(A,B,C)` type. The LLVM type depends on the target platform's C calling convention.

`CCodePointer`s are called using the C calling convention when passed to the [`callCCodePointer`] primitive function. Different symbols are provided for function pointers with different calling conventions.

* `LLVMCodePointer[[A,B,C],[D]]` corresponds directly to an LLVM `D (A,B,C) *` function pointer type.
* `VarArgsCCodePointer[[A,B,C], [D]]` points to a variadic C function; it corresponds to the `D (*)(A,B,C,...)` type in C.
* `StdCallCodePointer`, `FastCallCodePointer`, and `ThisCallCodePointer` point to functions using legacy x86 calling conventions on Windows. They correspond to C function pointer types decorated with `__stdcall`, `__fastcall`, or `__thiscall` in Microsoft dialect, or `__attribute__((stdcall))` etc. in GCC dialect.

`CCodePointer`s, along with the other external code pointer types, may be obtained by evaluating external function names, or as return values from C functions. They may also be derived directly automatically from some Clay functions using the [`makeCCodePointer`] primitive function.

    // Example
    // XXX

#### `Array`

`Array[T,n]` is the type of a fixed-size, locally-allocated array, containing `n` elements of size `T`. The `n` parameter must currently be an `Int32` (regardless of the target word size). It corresponds to the LLVM `[n x %T]` type, or the C `T [n]` type; however, unlike C arrays, Clay arrays never implicitly devolve to pointers. The [`arrayRef`] and [`arrayElements`] primitive functions provide access to array elements.

#### `Vec`

`Vec[T,n]` is the type of a SIMD vector value, containing `n` elements of size `T`. The `n` parameter must currently be an `Int32` (regardless of the target word size). It corresponds to the LLVM `<n x %T>` type, or the GCC extension `T __attribute__((vector_size(n*sizeof(T))))` type.

No primitives are currently provided for manipulating `Vec` types; they are primarily intended to be used directly with LLVM's vector intrinsics.

#### `Tuple`

`Tuple[..T]` is an anonymous aggregate of the specified types. Tuples are laid out equivalently to naturally-aligned C `struct`s containing the same corresponding types in the same order. `Tuple[A,B,C]` corresponds to an LLVM `{%A, %B, %C}` struct type.

#### `Union`

`Union[..T]` is an anonymous, nondiscriminated union of the specified types. It is laid out equivalently to a naturally-aligned C `union` containing the corresponding types in the same order. LLVM does not represent unions in its type system; an LLVM type is chosen in order to have the proper size and alignment for the member types of the union.

#### `Static`

`Static[x]` is a stateless type used to represent compile-time values. Clay symbols, static strings, and `static` expressions evaluate to values of instances of this type.

(`Static` values are actually not quite completely stateless yet; they are emitted as `i8 undef` values at the LLVM level. They thus still unfortunately take up space inside tuples and record types.)

### Data access operations
