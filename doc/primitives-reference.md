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
    * `integerAddChecked`, `integerSubtractChecked`, `integerMultiplyChecked`, `integerDivideChecked`, `integerRemainderChecked`, `integerNegateChecked`, `integerShiftLeftChecked`, `integerConvertChecked`

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
* `StdCallCodePointer`, `FastCallCodePointer`, and `ThisCallCodePointer` point to functions using legacy x86 calling conventions on Windows. They correspond to C function pointer types decorated with `__stdcall`, `__fastcall`, or `__thiscall` in Microsoft dialect, or with `__attribute__((stdcall))` etc. in GCC dialect.

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

These functions provide fundamental data manipulation operations. Unlike normal symbols, they may not be overloaded.

#### `primitiveCopy`

    [T]
    primitiveCopy(dest:T, src:T) :;

`primitiveCopy` performs a bitwise copy of `src` into `dst`. `TypeSize(T)` bytes are copied. The function corresponds to an LLVM `load` instruction followed by a `store`.

#### `arrayRef`

    [T,n,I | Integer?(I)]
    arrayRef(array:Array[T,n], i:I) : ref T;

`arrayRef` returns a reference to element `i` of `array`. `i` is zero-based and is not bounds checked. The function corresponds to an LLVM `getelementptr` instruction.

#### `arrayElements`

    [T,n]
    arrayElements(array:Array[T,n]) : ref ..repeatValue(static n, T);

`arrayElements` returns a reference to each element of `array` in order as a multiple value list.

#### `tupleRef`

    [..T, n | n >= 0 and n < countValues(..T)]
    tupleRef(tuple:Tuple[..T], static n) : ref nthValue(static n, ..T);

`tupleRef` returns a reference to the `n`th element of `tuple`. `n` is zero-based, and must be greater than or equal to zero, and less than the number of elements in `tuple`. The function corresponds to an LLVM `getelementptr` instruction.

#### `tupleElements`

    [..T]
    tupleElements(tuple:Tuple[..T]) : ref ..T;

`tupleElements` returns a reference to each element of `tuple` in order as a multiple value list.

#### `recordFieldRef`

    [R, n | Record?(R) and n >= 0 and n < RecordFieldCount(R)]
    recordFieldRef(rec:R, static n) : ref RecordFieldType(R, static N);

`recordFieldRef` returns a reference to the `n`th field of `rec`, which must be of a record type. `n` is zero-based, and must be greater than or equal to zero, and less than the number of fields in the record type `R`. The function corresponds to an LLVM `getelementptr` instruction.

#### `recordFieldRefByName`

    [R, name | Record?(R) and Identifier?(name) and RecordWithField?(R, name)]
    recordFieldRefByName(rec:R, static name) : ref RecordFieldTypeByName(R, name);

`recordFieldRefByName` returns a reference to the field named by the static string `name` in `rec`, which must be of a record type with a field of that name. The function corresponds to an LLVM `getelementptr` instruction.

#### `recordFields`

    [R | Record?(R)]
    recordFields(rec:R) : ref ..RecordFieldTypes(R);

`recordFields` returns a reference to each field of `rec` in order as a multiple value list. `rec` must be of a record type.

#### `enumToInt`

    [E | Enum?(E)]
    enumToInt(en:E) : Int32;

`enumToInt` returns the ordinal value of `en` as an `Int32`. `en` must be of an enum type.

#### `intToEnum`

    [E | Enum?(E)]
    intToEnum(static E, n:Int32) : E;

`intToEnum` returns a value of the enum type `E` with the ordinal value `n`. `n` is not bounds checked against the defined values of `E`.

### Numeric operations

These functions provide fundamental arithmetic operations for the primitive [`Bool`], [integer](#integertypes), [floating-point](#floatingpointtypes), and [imaginary](#imaginaryandcomplextypes) types. Unlike normal symbols, they may not be overloaded. Binary numeric primitives generally require inputs of matching types. Conversion rules for heterogeneous types are left to be implemented by the library. These functions also do not operate on complex types; complex math is also left to the library.

#### `boolNot`

    boolNot(x:Bool) : Bool;

`boolNot` returns the complement of `x`, which must be a `Bool`. It is equivalent to the `not` operator.

#### `numericEquals?`

    [T | Numeric?(T)]
    numericEquals?(a:T, b:T) : Bool;

`numericEquals?` returns `true` if `a` and `b` are numerically equal, `false` otherwise. For integer types, the function corresponds to an LLVM `icmp eq` instruction, and for floating-point types, the function corresponds to an LLVM `fcmp ueq` instruction. Floating-point comparison follows IEEE 754 unordered comparison rules; `+0.0` is equal to `-0.0` and comparisons involving NaN are always false and non-signaling.

#### `numericLesser?`

    [T | Numeric?(T)]
    numericLesser?(a:T, b:T) : Bool;

`numericLesser?` returns `true` if `a` is numerically less than `b` are numerically equal, `false` otherwise. For signed integer types, the function corresponds to an LLVM `icmp slt` instruction; for unsigned types, `icmp ult`; and for floating-point types, `fcmp ult`. Floating point comparison follows IEEE 754 unordered comparison rules; `-0.0` is not less than `+0.0` and comparisons involving NaN are always false and non-signaling.

(`__primitives__` does not currently provide the full set of floating-point comparison operators. The Clay library currently implements floating-point ordered and unordered comparison using inline LLVM functions. For floating-point, these primitives are used only for compile-time evaluation, which does not support inline LLVM.)

#### `numericAdd`

    [T | Numeric?(T)]
    numericAdd(a:T, b:T) : T;

`numericAdd` returns the sum of `a` and `b`. Integer signed or unsigned addition overflows following two's-complement arithmetic rules. For integer types, the function corresponds to the LLVM `add` instruction, and for floating-point types, the `fadd` instruction.

#### `numericSubtract`

    [T | Numeric?(T)]
    numericSubtract(a:T, b:T) : T;

`numericSubtract` returns the difference of `a` and `b`. Integer signed or unsigned subtraction overflows following two's-complement arithmetic rules. For integer types, the function corresponds to the LLVM `sub` instruction, and for floating-point types, the `fsub` instruction.

#### `numericMultiply`

    [T | Numeric?(T)]
    numericMultiply(a:T, b:T) : T;

`numericMultiply` returns the product of `a` and `b`. Integer signed or unsigned multiplication overflows following two's-complement arithmetic rules. For integer types, the function corresponds to the LLVM `mul` instruction, and for floating-point types, the `fmul` instruction.


#### `numericDivide`

    [T | Numeric?(T)]
    numericDivide(a:T, b:T) : T;

`numericDivide` returns the quotient of `a` and `b`. Integer division is truncated towards zero. Integer division by zero is undefined, as is signed division overflow (for example, the result of `-0x8000_0000/-1`). Floating-point division by zero, underflow, and overflow follow IEEE 754 rules. For signed integer types, the function corresponds to the LLVM `sdiv` instruction; for unsigned types, `udiv`; and for floating point types, `fdiv`.

#### `numericNegate`

    [T | Numeric?(T)]
    numericNegate(a:T) : T;

`numericNegate` returns the negation of `a`. Integer negation behaves like two's-complement subtraction from zero (and in fact, LLVM represents integer negation as `sub %T 0, %a`). Unsigned negation thus gives the two's complement, and signed negation overflow (for example, the result of negating `-0x8000_0000`) gives back the original value. Floating-point negation corresponds to `fsub %T -0.0, %a` in LLVM; negating a zero gives the other zero, and negating a NaN gives an unspecified other NaN value.

#### `integerRemainder`

    [T | Integer?(T)]
    integerRemainder(a:T, b:T) : T;

`integerRemainder` returns the remainder from dividing `a` and `b`. For signed remainder operations, a nonzero remainder has the same sign as `a`. Integer division by zero is undefined, as is signed division overflow (for example, the result of `-0x8000_0000/-1`). (Although the result of the remainder operation itself does not overflow, LLVM still specifies that the remainder of an overflowing division operation is undefined.) For signed integer types, the function corresponds to the LLVM `srem` instruction, and for unsigned types, `urem`.

#### `integerShiftLeft`

    [T | Integer?(T)]
    integerShiftLeft(a:T, b:T) : T;

`integerShiftLeft` returns the result of bitshifting `a` left by `b` bits. The result is undefined is `b` is either negative or greater than or equal to the bit size of `T`. Overflowed bits are discarded; the value is still defined. The function corresponds to the LLVM `shl` instruction.

#### `integerShiftRight`

    [T | Integer?(T)]
    integerShiftRight(a:T, b:T) : T;

`integerShiftRight` returns the result of bitshifting `a` right by `b` bits. The result is undefined is `b` is either negative or greater than or equal to the bit size of `T`. For signed integer types, an "arithmetic" shift is performed; the sign bit is propagated rightward, and the operation corresponds to the LLVM `ashr` instruction. For unsigned integer types, a "logical" shift is performed; the high bits of the result are set to zero, and the operation corresponds to the LLVM `lshr` instruction.

#### `integerBitwiseAnd`

    [T | Integer?(T)]
    integerBitwiseAnd(a:T, b:T) : T;

`integerBitwiseAnd` returns the bitwise-and of `a` and `b`. It corresponds to the LLVM `and` instruction.

#### `integerBitwiseOr`

    [T | Integer?(T)]
    integerBitwiseOr(a:T, b:T) : T;

`integerBitwiseOr` returns the bitwise-or of `a` and `b`. It corresponds to the LLVM `or` instruction.

#### `integerBitwiseXor`

    [T | Integer?(T)]
    integerBitwiseXor(a:T, b:T) : T;

`integerBitwiseXor` returns the bitwise-xor of `a` and `b`. It corresponds to the LLVM `xor` instruction.

#### `integerBitwiseNot`

    [T | Integer?(T)]
    integerBitwiseNot(a:T) : T;

`integerBitwiseNot` returns the bitwise-not of `a`. LLVM represents bitwise-not as an `xor %T %a, -1` instruction.

#### `numericConvert`

    [T, U | Numeric?(T) and Numeric?(U)]
    numericConvert(static T, a:U) : T;

`numericConvert` returns `a` converted to type `T` while preserving its numeric value. If `T` and `U` are the same type, the value is simply copied.

##### Integer to integer conversion

Conversion from an integer type to a smaller integer type is done by bitwise truncation, corresponding to LLVM's `trunc` instruction. Conversion from an integer type to a larger signed type is done by sign extension, corresponding to LLVM's `sext` instruction, and conversion to a larger unsigned type is done by zero extension, corresponding to `zext`. Conversion to an equal-sized signed or unsigned type is done by bitwise casting, corresponding to the `bitcast` instruction.

##### Floating-point to floating-point conversion

Conversion among floating-point types is done by precision truncation or extension, corresponding to the LLVM `fptrunc` and `fpext` instructions. Overflowing truncation is undefined.

##### Integer to floating-point conversion

Conversion from integer to floating-point types is done corresponding to the LLVM instructions `sitofp` for signed types, and `uitofp` for unsigned types. Overflowing conversion is undefined.

##### Integer to floating-point conversion

Conversion from floating-point to integer types is done corresponding to the LLVM instructions `fptosi` for signed types, and `fptoui` for unsigned types. Overflowing conversion is undefined.

### Checked integer operations

These operations provide integer arithmetic operations checked for overflow. Unlike the previously-described numeric primitives, the results of these operations are always undefined if they overflow; however, an additional boolean value is also returned that will be true if the operation overflowed. For non-overflowing cases, these functions behave like their non-checked counterparts and return false as their second value. Unlike normal symbols, they may not be overloaded.

    [T | Integer?(T)]
    integerAddChecked(a:T, b:T) : T, Bool;
    [T | Integer?(T)]
    integerSubtractChecked(a:T, b:T) : T, Bool;
    [T | Integer?(T)]
    integerMultiplyChecked(a:T, b:T) : T, Bool;
    [T | Integer?(T)]
    integerDivideChecked(a:T, b:T) : T, Bool;
    [T | Integer?(T)]
    integerNegateChecked(a:T) : T, Bool;
    [T | Integer?(T)]
    integerRemainderChecked(a:T, b:T) : T, Bool;
    [T | Integer?(T)]
    integerShiftLeftChecked(a:T, b:T) : T, Bool;
    [T, U | Integer?(T) and Integer?(U)]
    integerConvertChecked(static T, a:U) : T, Bool;

### Pointer operations

These operations create, dereference, compare, and convert pointers. Unlike normal symbols, they may not be overloaded.

#### `addressOf`

    [T]
    addressOf(x:T) : Pointer[T];

`addressOf` returns the address of `x`. It is equivalent to the prefix `&` operator.

#### `pointerDereference`

    [T]
    pointerDereference(p:Pointer[T]) : ref T;

`pointerDereference` returns a reference to the object pointed to by `p`. (It is effectively a no-op at the LLVM level, because references are themselves represented as pointers.)

#### `pointerEquals?`

    [T, U]
    pointerEquals?(p:Pointer[T], q:Pointer[U]) : Bool;

`pointerEquals?` returns true if `p` and `q` have the same address value. It corresponds to the LLVM `icmp eq` instruction.

#### `pointerLesser?`

    [T, U]
    pointerLesser?(p:Pointer[T], q:Pointer[U]) : Bool;

`pointerLesser?` returns true if the address of `p` is numerically less than that of `q`. It corresponds to the LLVM `icmp lt` instruction.

#### `pointerOffset`

    [T, I | Integer?(I)]
    pointerOffset(p:Pointer[T], i:I) : Pointer[T];

`pointerOffset` returns a new pointer value, offset from the address of `p` by `i * TypeSize(T)` bytes. It corresponds to the LLVM `getelementptr` instruction.

#### `pointerToInt`

    [T, I | Integer?(I)]
    pointerToInt(static I, p:Pointer[T]) : I;

`pointerToInt` converts the address value of `p` to an integer type `I`. If `I` is larger than a pointer, the address is zero-extended; if smaller, the address is truncated. The function corresponds to the LLVM `ptrtoint` instruction.

#### `intToPointer`

    [T, I | Integer?(I)]
    intToPointer(static T, address:I) : Pointer[T];

`pointerToInt` converts the integer value `address` into a pointer of type `T`. If `I` is larger than a pointer, its value is truncated; if smaller, the value is zero-extended. The function corresponds to the LLVM `inttoptr` instruction.

#### `pointerCast`

    [P1, P2 | Pointer?(P1) and Pointer?(P2)]
    pointerCast(static P1, p:P2) : P1;

`pointerCast` converts a pointer `p` to a value of another pointer type with the same address value. It corresponds to an LLVM `bitcast` instruction. In addition to casting data pointers (such as `Pointer[T]` to `Pointer[U]`), it may also cast among `CodePointer`, `CCodePointer`, etc. types, and cast data to code pointers and back.

### Function pointer operations

These operations create and invoke pointers to instances of Clay functions. Unlike normal symbols, they may not be overloaded.

#### `makeCodePointer`

    [F, ..T]
    makeCodePointer(static F, static ..T) : CodePointer[[..T], [..CallType(F, ..T)]];

`makeCodePointer` looks up an overload for `F` matching the input types `..T`. If a matching overload is found, the overload is instantiated for the given input types, and a pointer to the function instance is returned as a [`CodePointer`]. The output types parameter of the `CodePointer` is deduced from the matched overload's return types. `F` must be a symbol or a lambda that does not capture its scope (which is equivalent to a symbol). An error is raised if `F` is not a symbol, or no matching overload for `F` is found.

`makeCodePointer` matches overloads as if all input types are lvalues. Taking `CodePointer`s to rvalue functions is currently unsupported.

#### `makeCCodePointer`

    [F, ..T]
    makeCCodePointer(static F, static ..T) : CCodePointer[[..T], [..CallType(F, ..T)]];

`makeCCodePointer` looks up an overload for `F` matching the input types `..T`. If a matching overload is found and is determined to be C-compatible, the overload is instantiated for the given input types, along with a thunk function adapting the overload to the C calling convention. A pointer to the thunk is returned as a [`CCodePointer`]. The output types parameter of the `CCodePointer` is deduced from the matched overload's return types. `F` must be a symbol or a lambda that does not capture its scope (which is equivalent to a symbol). An error is raised if `F` is not a symbol, no matching overload for `F` is found, or the matching overload is not C-compatible.

The matched overload must meet the following criteria to be deemed C-compatible:

* It must return zero or one values.
* It must not have any arguments of types with nontrivial `copy`, `move`, or `destroy` operations.

Like an external function, if a Clay exception escapes from the pointed-to overload, the `unhandledExceptionInExternal` operator function is called.

#### `callCCodePointer`

    define callCCodePointer;
    [..In, ..Out]
    overload callCCodePointer(f:CCodePointer[[..In], [..Out]], ..args:In) : ..Out;
    [..In, ..Out]
    overload callCCodePointer(f:VarArgsCCodePointer[[..In], [..Out]], ..args:In, ..varArgs) : ..Out;
    [..In, ..Out]
    overload callCCodePointer(f:LLVMCodePointer[[..In], [..Out]], ..args:In) : ..Out;
    // and so on for StdCallCodePointer, etc.

`callCCodePointer` invokes an external function, `CCodePointer`, or other external function pointer type.

### Atomic memory operations

The following functions provide uninterruptible, lock-free memory access and synchronization. Unlike normal symbols, they may not be overloaded. Atomic operations are currently not supported by the compile-time evaluator; an error will be raised if an atomic operation is evaluated at compile time.

#### Memory order symbols

    define OrderUnordered;
    define OrderMonotonic;
    define OrderAcquire;
    define OrderRelease;
    define OrderAcqRel;
    define OrderSeqCst;

A set of symbols are defined and used as parameters by every atomic operation to specify the memory ordering constraints for that operation. These correspond to LLVM's memory ordering constraints, which in turn are a superset of those specified by the C11 and C++11 memory models. See the [LLVM Atomic Instructions and Concurrency Guide](http://llvm.org/docs/Atomics.html) for details.

* `OrderUnordered` corresponds to the LLVM `unordered` memory ordering.
* `OrderMonotonic` corresponds to the LLVM `monotonic` and C++11 `memory_order_relaxed` orderings.
* `OrderAcquire` corresponds to the LLVM `acquire` and C++11 `memory_order_acquire` orderings.
* `OrderRelease` corresponds to the LLVM `release` and C++11 `memory_order_release` orderings.
* `OrderAcqRel` corresponds to the LLVM `acq_rel` and C++11 `memory_order_acq_rel` orderings.
* `OrderSeqCst` corresponds to the LLVM `seq_cst` and C++11 `memory_order_seq_cst` orderings.

#### `atomicLoad`

    [Order, T | Order?(Order)]
    atomicLoad(static Order, p:Pointer[T]) : T;

`atomicLoad` performs an atomic load of the value pointed to by `p`. If atomic loads of type `T` are not supported by the target platform, an error is raised. The value is bitwise-copied. The load is subject to the memory ordering constraints specified by `Order`. The function corresponds to the LLVM `load atomic` instruction.

#### `atomicStore`

    [Order, T | Order?(Order)]
    atomicStore(static Order, p:Pointer[T], value:T) :;

`atomicStore` performs an atomic store of `T` to the address of `p`. If atomic stores of type `T` are not supported by the target platform, an error is raised. `value` is bitwise-copied. The store is subject to the memory ordering constraints specified by `Order`. The function corresponds to the LLVM `store atomic` instruction.

#### `atomicRMW`

    define RMWXchg;
    define RMWAdd;
    define RMWSubtract;
    define RMWAnd;
    define RMWNAnd;
    define RMWOr;
    define RMWXor;
    define RMWMin;
    define RMWMax;
    define RMWUMin;
    define RMWUMax;

    [Order, Op, T | Order?(Order) and RMWOp?(Op)]
    atomicRMW(static Order, static Op, p:Pointer[T], operand:T) : T;

`atomicRMW` performs an atomic read-modify-write update operation to the value pointed to by `p`. The value pointed to by `p` prior to the operation is returned. The `Op` parameter determines the operation used to update the value pointed to by `p`:

* `RMWXchg` causes `operand` to be written to `p^`. The value is bitwise copied.
* `RMWAdd` causes `operand` to be added to `p^`. `T` must be an integer type.
* `RMWSubtract` causes `operand` to be subtracted from `p^`. `T` must be an integer type.
* `RMWAnd` causes `operand` to be bitwise-anded with `p^`.
* `RMWNAnd` causes `operand` to be bitwise-anded with `p^`, and the bitwise-not of the result is written to `p`.
* `RMWOr` causes `operand` to be bitwise-ored with `p^`.
* `RMWXor` causes `operand` to be bitwise-xored with `p^`.
* `RMWMin` causes the signed minimum of `operand` and `p^` to be written to `p`.
* `RMWMax` causes the signed maximum of `operand` and `p^` to be written to `p`.
* `RMWUMin` causes the unsigned minimum of `operand` and `p^` to be written to `p`.
* `RMWUMax` causes the unsigned maximum of `operand` and `p^` to be written to `p`.

The update is subject to the memory ordering constraints specified by `Order`. The function corresponds to the LLVM `atomicrmw` instruction. An error is raised if the target platform does not atomically support the specified operation for `T`.

#### `atomicCompareExchange`

    [Order, T | Order?(Order)]
    atomicCompareExchange(static Order, p:Pointer[T], old:T, new:T) : T;

`atomicCompareExchange` performs an atomic compare-and-swap update of the value pointed to by `p`. If `p^` is bitwise equal to `old`, `new` is bitwise-written to `p`, and `old` is returned; otherwise, `p^` is unchanged, and its current value is returned. An error is raised if the target platform does not support atomic compare-and-swap for type `T`. The operation is subject to the memory ordering constraints specified by `Order`. The function corresponds to the LLVM `cmpxchg` instruction.

#### `atomicFence`

    [Order | Order?(Order)]
    atomicFence(static Order);

`atomicFence` introduces a happens-before edge with `OrderAcquire`, `OrderRelease`, `OrderAcqRel`, or `OrderSeqCst` semantics without an associated memory operation. It corresponds to an LLVM `fence` instruction.

### Exception handling

The following functions support the exception handling implementation. Unlike normal symbols, they may not be overloaded.

#### `activeException`

    activeException() : Pointer[Int8];

`activeException()` returns a pointer to the exception that is currently instigating unwinding. It is only valid during unwinding itself, specifically not during catch clauses. This primitive is an implementation detail and should not normally be used.

### Symbol and function introspection

