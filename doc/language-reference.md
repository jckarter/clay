# Clay Programming Language Reference 0.1

* [Conventions]
* [Tokenization]
* [Modules]
* [Source file organization]
* [Type definitions]
* [Function definitions]
* [Global value definitions]
* [Statements]
* [Expressions]
* [Pattern matching]

## Conventions

BNF grammar rules are provided in `monospace` blocks beginning with `# Grammar`. Examples of Clay code are provided in monospace blocks beginning with `// Example`. Regular expressions in grammar rules are delimited by `/slashes/` and use Perl `/x` syntax; that is, whitespace between the delimiting slashes is insignificant and used for readability. Literal strings in grammar rules are delimited by `"quotation marks"`.

## Tokenization

### Source encoding

Clay source code is evaluated as a stream of ASCII text. (Anything other than ASCII in string and character literals is currently passed through as an opaque stream of bytes by the compiler. This will change to proper UTF-8 encoding in the future.)

### Whitespace

    # Grammar
    ws -> /[ \t\r\n\f]+/

ASCII space, tab, carriage return, newline, and form feed are treated as whitespace. Whitespace is used by the lexer to delimit tokens and is otherwise insignificant and eliminated during tokenization.

### Comments

    # Grammar
    Comment -> "/*" /.*?/ "*/"
             | "//" /.*$/

Comments in Clay come in two forms: block comments, delimited by non-nesting `/*` and `*/`, and line comments, begun with `//` and extending to the end of the current source line. Comments are treated grammatically as equivalent to a whitespace character.

### Identifiers and keywords

    # Grammar
    Identifier -> !Keyword, /[A-Za-z_?][A-Za-z_0-9?]*/

    Keyword -> "__ARG__"
             | "__COLUMN__"
             | "__FILE__"
             | "__LINE__"
             | "__llvm__"
             | "alias"
             | "and"
             | "as"
             | "break"
             | "case"
             | "catch"
             | "continue"
             | "define"
             | "else"
             | "enum"
             | "eval"
             | "external"
             | "false"
             | "finally"
             | "for"
             | "forward"
             | "goto"
             | "if"
             | "import"
             | "in"
             | "inline"
             | "instance"
             | "not"
             | "onerror"
             | "or"
             | "overload"
             | "private"
             | "public"
             | "record"
             | "ref"
             | "return"
             | "rvalue"
             | "static"
             | "switch"
             | "throw"
             | "true"
             | "try"
             | "var"
             | "variant"
             | "while"

Clay identifiers begin with an ASCII letter character, underscore (`_`), or question mark (`?`), and contain zero or more subsequent characters, which may be letters, underscores, question marks, or ASCII digits. A reserved keyword may not be used as an identifier.

    // Examples of valid identifiers
    a a1 a_1 abc123 a? ?a ?

### Integer literals

    # Grammar
    IntToken -> "0x" HexDigits
              | DecimalDigits

    HexDigits -> /([0-9A-Fa-f]_*)+/
    DecimalDigits -> /([0-9]_*)+/

Integer literals may be expressed in decimal, or in hexadecimal prefixed by `0x`. Either form may include any number of underscores after any digit for human readability purposes (excluding the leading `0` in the `0x` prefix). Underscores have no effect on the value of the literal.

    // Examples of integer literals
    0 1 23 0x45abc 1_000_000 1_000_000_ 0xFFFF_FFFF

### Floating-point literals

    # Grammar
    FloatToken -> "0x" HexDigits ("." HexDigits?)? /[pP] [+-]?/ DecimalDigits
                | DecimalDigits ("." DecimalDigits?)? (/[eE] [+-]?/ DecimalDigits)?

Like integers, floating-point literals also come in decimal and hexadecimal forms. A floating-point decimal literal is differentiated from an integer literal by including either a decimal point `.` followed by zero or more decimal digits, an exponential marker `e` or `E` followed by an optional sign and a required decimal value indicating the decimal exponent, or both. Floating-point hexadecimal literals likewise contain an optional hexadecimal point (also `.`) followed by zero or more hex digits, but require an exponential marker `p` or `P` followed by an optional sign and required decimal value indicating the binary exponent. Like integer literals, floating-point literals may also include underscores after any digit in the integer, mantissa, or exponent for human readability.

    // Examples of decimal floating-point literals
    1. 1.0 1e0 1e-2 0.000_001 1e1_000
    // Examples of hexadecimal floating-point literals
    0x1p0 0x1.0p0 0x1.0000_0000_0000_1p1_023

### Character literals

    # Grammar
    CharToken -> "'" CharChar "'"

    CharChar -> /[^\\']/
              | EscapeCode

    EscapeCode -> /\\ ([nrtf\\'"0] | x [0-9A-Fa-f]{2})/

Character literals represent a single ASCII character. Syntactically, they consist of either a single character (excluding `\` and `'`) or a backslash followed by an escape code character, delimited by matching `'` characters. The provided escape codes are a subset of those traditionally provided by C-like languages:

* `\0` encodes the ASCII null character.
* `\t` encodes the ASCII tab character.
* `\n` encodes the ASCII newline character.
* `\f` encodes the ASCII form feed character.
* `\r` encodes the ASCII carriage return character.
* `\"` encodes the character `"`.
* `\'` encodes the character `'`.
* `\\` encodes the character `\`.
* `\x` followed by two hexadecimal digits encodes an arbitrary ASCII code point. For example `'\x20'` is the space character (code `0x20`).

 

    // Examples of character literals
    'x'  ' '  '\n'  '\''  '\x7F'

### String literals

    # Grammar
    StringToken -> "\"" StringChar* "\""
                 | "\"\"\"" TripleStringChar* "\"\"\""

    StringChar -> /[^\\"]/
                | EscapeCode

    TripleStringChar -> /(?!=""" ([^"]|$)) [^\\]/
                      | EscapeCode

String literals represent a sequence of ASCII text. Syntactically, they consist of zero or more characters or escape codes as described for [character literals], delimited by either matching `"` characters or by matching `"""` sequences. Within `"`-delimited strings, the characters `"` and `\` must be escaped, whereas in `"""`-delimited strings, only `\` need be escaped. (The sequence `"""` may be escaped by escaping one or more of the `"` characters.) Whitespace within string literals is significant, including newlines.

    // Examples of string literals
    "hello world"  "\"hello world\""
    """the string "hello world""""
    "apples\0bananas\0oranges\0\0"  "\x1B[31mALERT\x1B[0m"

    """
    "But not with you, Derek, this star nonsense."
    "Yes, yes."
    "Which is it then?"
    "I'm not sure."
    """

## Modules

Clay programs are organized into modules. Modules correspond one-to-one with Clay source files. Modules are named in Clay hierarchially using dotted names; these correspond to paths in the filesystem. The name `foo.bar` corresponds to (in search order) `foo/bar.clay` or `foo/bar/bar.clay` under one of the compiler's search paths. Hierarchical names are only used for source organization, and no semantic relationship is implied among modules with hierarchically related names.

Modules form the basis of encapsulation and namespacing in Clay. Every module has an independent namespace comprising its imported modules and symbols, selected via [import declarations], and its own defined [symbols], created by [top-level definitions] in the module source code. Modules can control access to their namespace from other importing modules by marking symbols as public or private.

### Special modules

A few modules have special significance:

* The `__primitives__` module is synthesized by the compiler; it is always present and does not correspond to a source file. It contains fundamental types such as `Int`, `Pointer[T]`, and `Bool`; functions providing basic operations on those types; and compile-time introspection functions. The [primitives reference](primitives-reference.md) documentation describes its contents in detail.
* The `prelude` module is loaded automatically and implicitly imported into every other module. This module is also the one searched for the special functions used to desugar [operators](#operatorfunctions).
* If the entry point module does not declare its name in a [module declaration], it is given the default name `__main__`. Regardless of its name, this module is searched for a `main` function, which if found will be used as the entry point for a standalone executable generated from the given module.

### Operator functions

#### Value semantics

## Compilation strategy

XXX

### Compile-time evaluation

### Pattern matching

    # Grammar
    Pattern -> AtomicPattern PatternSuffix?

    AtomicPattern -> Literal
                   | PatternNameRef

    PatternNameRef -> DottedName

    PatternSuffix -> "[" comma_list(Pattern) "]"

## Source file organization

    # Grammar
    Module -> Import* ModuleDeclaration? TopLevelLLVM? TopLevelItem*

A clay module corresponds to a single source file. Source files are laid out in the following order:

* Zero or more [import declarations]
* An optional [module declaration]
* An optional [top-level LLVM] block
* The body of the module file, consisting of zero or more [top-level definitions] that define the module's [symbols]

#### List syntactic forms

    # Grammar
    comma_list(Rule) -> (Rule ("," Rule)* ","?)?

Comma-delimited lists are a common feature in Clay's grammar. In most contexts, Clay allows a comma-delimited list to optionally end with a trailing comma.

    // Examples
    record US_Address (
        name:String,
        street:String,
        city:String,
        state:String,
        zip:String,
    );

    foo(a,b,c) { return a+b, b+c; }

In [pattern matching] contexts, a variation of the comma-delimited list is used that allows an optional tail syntax form, representing a variadic item that greedily matches the rest of the values being matched.

    # Grammar
    variadic_list(Rule, LastRule) -> Rule ("," Rule)* ("," (LastRule)?)?
                                   | LastRule
                                   | nil

    // Examples
    define sum(..xs);
    overload sum() = 0;
    overload sum(a, ..b) = a + sum(..b);

### Import declarations

    # Grammar
    Import -> Visibility? "import" DottedName ImportSpec? ";"

    ImportSpec -> "as" Identifier
                | "." "(" comma_list(ImportedItem) ")"
                | "." "*"

    DottedName -> Identifier ("." Identifier)*

    ImportedItem -> Visibility? Identifier ("as" Identifier)?

Import declarations connect the module to other modules by making those other modules' definitions visible in the current namespace.

All import declaration forms start with an optional `public` or `private` visibility modifier, followed by the `import` keyword and the name of the module. Module names consist of one or more identifiers separated by the `.` character. With the `private` modifier, the effects of the import are private to the module—the name or names imported by the declaration are not available to be imported by modules that in turn import the current module. By contrast, `public import`s become visible through the current module to other modules. Without an explicit visibility, the import defaults to `private`.

    // Examples
    public import foo.bar;
    private import foo.bar;
    import foo.bar; // equivalent to `private import`

The module name is followed by an import spec, and the import declaration is ended with a semicolon. The import spec comes in one of four general forms. If no import spec is provided (that is, the semicolon immediately follows the module name), the module's name is imported into the namespace as-is. Public members of the module may then be accessed by applying the `.` operator to the module name.

    // Example
    import foo.bar; // Import module `foo.bar` as `foo.bar`

    main() {
        foo.bar.apple(); // Call member `apple` of module `foo.bar`
    }

Alternately, a local alias for the module may be provided with `as`, in which case module members are instead accessed through the given alias name:

    // Example
    import foo.bar as bar; // Import module `foo.bar` as `bar`

    main() {
        bar.apple(); // Call member `apple` of module `foo.bar` (alias `bar`)
    }

Individual members of a module may be imported directly into the current module's namespace, with or without aliases:

    // Example
    import foo.bar.(            // Import selected members of `foo.bar`:
        apple,                  //      public member `foo.bar.apple` as `apple`
        mandarin as clementine, //      public member `foo.bar.mandarin` as `clementine`
    );

    main() {
        apple();      // Call member `apple` of module `foo.bar`
        clementine(); // Call member `mandarin` of module `foo.bar` (alias `clementine`)
    }

Private members of a module can also be imported by explicitly prefixing the member name with `private`. (You should only do this if you know what you're doing.)

    // Example
    import foo.bar.(private banana);

    main() {
        banana();
    }

Finally, all of the public members of a module may be imported directly into the current module's namespace using `.*`. (You should also only do this if you know what you're doing.)

    // Example
    import foo.bar.*; // Import all public members of `foo.bar`

    main() {
        apple();
        mandarin();
    }

#### Conflict resolution

It is an error to import two modules or module members using the same name.

    // Example
    import malkevich;
    import bar as malkevich; // ERROR
    import bar.(malkevich); // ERROR
    import bar.(bar as malkevich); // ERROR

However, it is allowed to import multiple modules with `.*` even if they contain members with conflicting names, so long as no ambiguous names are actually referenced.

    // Example
    // foo.clay
    a() { }
    b() { }

    // bar.clay
    b() { }
    c() { }

    // main.clay
    import foo.*; // ok
    import bar.*; // ok

    main() {
        a(); // ok, only in foo
        c(); // ok, only in bar
        b(); // ERROR, ambiguous
    }

Such ambiguities can be resolved by explicitly importing the ambiguous name from one of the imported modules.

    // Example
    // main.clay
    import foo.*;
    import bar.*;
    import bar.(b);

    main() {
        a(); // ok, only in foo
        c(); // ok, only in bar
        b(); // ok, explicitly disambiguated to bar.b
    }

A name defined in the current module may also shadow a name imported by `.*`, in which case the current module's definition takes precedence.

    // Example
    // main.clay
    import foo.*;
    import bar.*;

    b() { }

    main() {
        a(); // ok, only in foo
        c(); // ok, only in bar
        b(); // ok, resolves to locally-defined main.b
    }

Such ambiguities may of course also be avoided by using `as` to alias the conflicting names.

### Module declaration

    # Grammar
    ModuleDeclaration -> "in" DottedName AttributeList? ";"

    AttributeList -> "(" ExprList ")"

A module may optionally declare its name using the `in` keyword.

    // Example
    import foo.bar;

    in foo.bas;

Such a declaration must appear after any [import declarations] and before any [top-level LLVM] or other [top-level definitions]. The module declaration may optionally include a parenthesized list of module attributes. If no attribute list is provided, it is equivalent to the empty list.

    // Example
    in foo.bas ();
    in foo.bas (Float32);

The module attribute list can be an arbitrary [multiple value expression], which can reference any imported symbols but not symbols defined within the module itself.

    // Example
    // foo.clay
    GraphicsModuleAttributes() = Float32, Int32;

    // bar.clay
    import foo;

    in bar (..foo.GraphicsModuleAttributes());

XXX supported attributes?

### Top-level LLVM

    # Grammar
    TopLevelLLVM -> LLVMBlock
    LLVMBlock -> "__llvm__" "{" /.*/ "}"

A module may optionally contain a top-level block of LLVM assembly language. The given code will be emitted directly into the top level of the generated LLVM module, and may contain function declarations, metadata nodes, or other global definitions needed by `__llvm__` blocks in individual [inline LLVM functions]. The top-level LLVM block must appear after any [import declarations] or [module declaration] and before any [top-level definitions].

    // Example
    in traps;
    // Declare the llvm.trap intrinsic for use by the trap() function.
    __llvm__ {
    declare void @llvm.trap()
    }

    trap() __llvm__ {
        call void @llvm.trap()
        ret i8* null
    }

Clay static values can be interpolated into LLVM code using LLVM interpolation as described for [inline LLVM functions].

### Top-level definitions

    # Grammar
    TopLevelItem -> Record              # Types
                  | Variant
                  | Instance
                  | Enumeration
                  | Define              # Functions
                  | Overload
                  | Function
                  | ExternalFunction
                  | GlobalVariable      # Global values
                  | GlobalAlias
                  | ExternalVariable

Top-level definitions make up the meat of Clay source code and come in three general flavors:

* [Type definitions]
* [Function definitions]
* [Global value definitions]

Clay uses a two-pass loading mechanism. Modules are imported and their namespaces populated in one pass before any definitions are evaluated. Forward and circular references are thus possible without requiring forward declarations.

    // Example
    // Mutually recursive functions
    hurd() { hird(); }
    hird() { hurd(); }

    // Mutually recursive record types
    record Ping (pong:Pointer[Pong]);
    record Pong (ping:Pointer[Ping]);

Most top-level definitions in Clay share common syntactic features:

#### Pattern guards

    # Grammar
    PatternGuard -> "[" comma_list(PatternVar) ("|" Expression)? "]"
    PatternVar -> Identifier | ".." Identifier

Most definition forms in Clay can be genericized. Pattern guards provide the means for declaring and controlling generic behavior. A simple pattern guard declares zero or more pattern variable names enclosed in brackets, which can be used as generic types or as type parameters in the subsequent definition.

    // Example
    // Define printTwice for all types T
    [T]
    printTwice(file:File, x:T) {
        printTo(file, x);
        printTo(file, x);
    }

    // Define the record type Point[T] for all types T
    [T]
    record Point[T] (x:T, y:T);

    // Define printPoint for all Stream types, and Point[T] for all T
    [Stream, T]
    printPoint(s:Stream, p:Point[T]) {
        printTo(s, "(", p.x, ", ", p.y, ")");
    }

Variadic pattern variables can be declared in the pattern guard as a name prefixed with `..`.

    // Example
    // Define printlnTwice for any bag of types ..TT
    [..TT]
    printlnTwice(file:File, ..x:TT) {
        printlnTo(file, ..x);
        printlnTo(file, ..x);
    }

The list of declared pattern variables may optionally be followed by a `|` character and predicate expression. The predicate expression constrains the domain of valid values for the declared variables to those for which the given predicate evaluates to true.

    // Example
    // Define the record type Point[T] for all types T where `Numeric?(T)` is true
    [T | Numeric?(T)]
    record Point[T] (x:T, y:T);

A predicate may also appear in a pattern guard without any pattern variables, in order to conditionalize the following definition on compiler flags or platform attributes.

    // Example
    platformCheck() {}

    [| TypeSize(Pointer[Int]) < 4]
    overload platformCheck() { error("Time for a new computer"); }

#### Visibility modifiers

    # Grammar
    Visibility -> "public" | "private"

Every form that creates a new symbol name may be prefixed with a `public` or `private` visibility modifier. `public` symbols are available to be imported by other modules, whereas `private` modules normally are not (though they can be force-imported using special [import declarations]). Definitions without an explicit visibility default to `public`.

Visibility modifiers are not valid for definitions that modify existing symbols instead of creating new ones, such as variant `instance`s or function `overload`s. In forms that do admit a visibility modifier, the modifier must appear after the pattern guard, if any, but before the subsequent definition.

### Symbols

Top-level forms work by creating or modifying symbols, which are module-level global names representing types or functions. Symbol names are used in [overloads] and [pattern matching]. Symbols may also be used as singleton types; in an expression, a `symbol` is the only value of the primitive type `Static[symbol]`.

XXX

## Type definitions

Clay provides four different kinds of user-defined types:

* [Records]
* [Variants]
* [Enumerations]
* [Lambda types]

### Records

    # Grammar
    Record -> PatternGuard? Visibility? "record" TypeDefinitionName RecordBody

    TypeDefinitionName -> Identifier PatternVars?
    PatternVars -> "[" comma_list(PatternVar) "]"

    RecordBody -> NormalRecordBody
                | ComputedRecordBody

    NormalRecordBody -> "(" comma_list(RecordField) ")" ";"
    ComputedRecordBody -> "=" comma_list(Expression) ";"

    RecordField -> Identifier TypeSpec
    TypeSpec -> ":" Pattern

Record types are general-purpose aggregates. In memory, records are laid out in a manner compatible with the equivalent `struct` definition in C. In the simplest form, a record definition consists of the keyword `record` followed by the record's name, and a list of `fieldName: Type` pairs, separated by commas and enclosed in parens.

    // Example
    record Point (x:Int, y:Int);

A record type may also be parameterized with one or more pattern variables:

    // Example
    [T]
    record Point[T] (x:T, y:T);

    [T | Float?(T)]
    record FloatPoint[T] (x:T, y:T);

If no predicate expression is needed, the pattern guard is optional; the named parameters are assumed to be unbounded variables rather than existing identifiers.

    // Example
    record Point[T] (x:T, y:T); // [T] guard is assumed

A record type's layout can also be evaluated from an expression. If the type name is followed by an `=` token, the following expression is evaluated to determine the layout of the record. The expression must evaluate to a tuple of pairs, each pair containing a [static string](#staticstrings) field name and a field type [symbol](#symbols).

    // Example
    // Equivalent to the non-computed Point[T] definition:
    record Point[T] = [[#"x", T], [#"y", T]];

    // Point with our own coordinate names
    // e.g. p:Point[Complex, #"zw"] would have Complex-typed fields p.z and p.w
    record PointWithCoordinates[T, xy] = [[xy.0, T], [xy.1, T]];

Record definitions currently do not directly allow for template specialization. This can be worked around using an overloaded function as a computed record body.

    // Example
    record Vec3D[T] = Vec3DBody(T);

    private define Vec3DBody;
    [T | T != Double]
    overload Vec3DBody(static T) = [[#"coords", Array[T, 3]]];
    // Use a SIMD vector for Float
    overload Vec3DBody(static Float) = [[#"coords", Vec[Float, 4]]];

### Variants

    # Grammar
    Variant -> PatternGuard? Visibility? "variant" TypeDefinitionName ("(" ExprList ")")? ";"

Variant type definitions create a discriminated union type. A variant value can contain a value of any of the variant's instance types. The variant value knows what type it contains, and the contained value can be dispatched to an appropriately-typed function using the [dispatch operator].

Syntactically, the variant's instance types are defined after the variant name in a parenthesized list.

    // Example
    variant Fruit (Apple, Orange, Banana);

Like record types, variant types may be parameterized.

    // Example
    [C | Color?(C)]
    variant Fruit[C] (Apple[C], Orange[C], Banana[C]);

Also like record types, the pattern guard is optional if no predicate is needed; the specified parameters are taken as unbounded pattern variables rather than existing identifiers.

    // Example
    variant Maybe[T] (Nothing, T); // [T] pattern guard implied
    variant Either[T, U] (T, U); // [T, U] pattern guard implied

The variant instance list may be an arbitrary [multiple value expression]. It is evaluatedat compile time to derive the set of instances.

    // Example
    private RainbowTypes(Base) =
        Base[Red], Base[Orange], Base[Yellow], Base[Green],
        Base[Blue], Base[Indigo], Base[Violet];

    // Fruit will admit as instances Apple[Red], Banana[Violet], etc.
    variant Fruit (..RainbowTypes(Apple), ..RainbowTypes(Banana));

#### Extending variants

    # Grammar
    Instance -> PatternGuard? "instance" Pattern "(" ExprList ")" ";"

Variant types are open. Instance types can be added to an already-defined variant using the `instance` keyword.

    // Example
    variant Exception (); // type Exception starts with no members

    // introduce RangeError and TypeError as instances of Exception
    record RangeError (lowerBound:Int, upperBound:Int, attemptedValue:Int);
    record TypeError (expectedTypeName:String, attemptedTypeName:String);
    instance Exception (RangeError, TypeError);

`instance` definitions bind to variant types by [pattern matching] the name in the instance definition to each variant type name. Parameterized variant types may be extended for concrete parameter values, over all parameter values, or over some parameter variables with the use of pattern guards:

    // Example
    [C | Color?(C)]
    variant Fruit[C] ();

    // Extend Fruit[Yellow] to contain Banana
    instance Fruit[Yellow] (Banana);

    // Extend Fruit[Red] and Fruit[Green] to contain Apple
    [C | C == Red or C == Green]
    instance Fruit[C] (Apple);

    // Extend all Fruit[C] to contain Berry[C]
    [C]
    instance Fruit[C] (Berry[C]);

Unlike `variant` forms, the pattern guard is not optional when extending a variant generically; without a pattern guard, `instance Variant[T]` will attempt to extend only the type `Variant[T]` for the concrete parameter value `T` (which would need to be defined for the form to be valid) rather than for all values `T`.

### Enumerations

    # Grammar
    Enumeration -> Visibility? "enum" Identifier "(" comma_list(Identifier) ")" ";"

Enumerations define a new type, values of which may contain one of a given set of symbolic values. In addition to the type name, the symbol names are also defined in the current module as constants of the newly-created type. The symbol names share visibility with the parent type.

    // Example
    enum ThreatLevel (Green, Blue, Yellow, Orange, Red, Midnight);

    // SecurityLevel and all of its values are private
    private enum SecurityLevel (
        Angel_0A, Archangel_1B,
        Principal_2C, Power_3D,
        Virtue_4E, Domination_5F,
        Throne_6G, Cherubic_7H,
        Seraphic_8X,
    );

Unlike record or variant types, enumeration types cannot currently be parameterized, and their definitions do not allow pattern guards.

### Lambda types

Lambda types are record-like types that capture values from their enclosing scope. Unlike records, variants, or enumerations, they are not explicitly defined in top-level forms, but are implicitly created as needed when [lambda expressions] are used.

## Function definitions

Function definitions control most of Clay's compile-time and run-time behavior. Clay functions are inherently generic. They can be parameterized to provide equivalent behavior over a variety of types or compile-time values, and they can be overloaded to provide divergent implementations of a common interface. Runtime function calls are resolved at compile time for every set of input types with which they are invoked.

* [Simple function definitions]
* [Overloaded function definitions]
* [Arguments]
* [Return types]
* [Function body]
* [Inline and alias qualifiers]
* [External functions]

### Simple function definitions

    # Grammar
    Function -> PatternGuard? Visibility? CodegenAttribute?
                Identifier Arguments ReturnSpec? FunctionBody

The simplest form of function definition creates a new function symbol with a single overload. These definitions consist of the new function's name, followed by a list of [arguments], an optional list of [return types], and the [function body]. If the return types are omitted, they are inferred from the function body. Function definitions may also use [visibility modifiers] and/or [pattern guards].

    // Examples
    hello() { println(helloString()); }
    private helloString() = "Hello World";

    squareInt(x:Int) : Int = x*x;

    [T]
    square(x:T) : T = x*x;

    [T | Float?(T)]
    quadraticRoots(a:T, b:T, c:T) : T, T {
        var q = -0.5*(b+signum(b)*sqrt(b*b - 4.*a*c));
        return q/a, c/q;
    }

A simple function definition always defines a new symbol name; it is an error if a symbol with the same name already exists.

    // Example
    abs(x:Int) = if (x < 0) -x else x;
    // ERROR: abs is already defined
    abs(x:Float) = if (x < 0.) -x else if (x == 0.) 0. else x;

Overloads are necessary to extend a function with multiple implementations.

### Overloaded function definitions

    # Grammar
    Define -> PatternGuard? "define" Identifier (Arguments ReturnSpec?)? ";"

    Overload -> PatternGuard? CodegenAttribute? "overload"
                Pattern Arguments ReturnSpec? FunctionBody

Simple function definitions define a symbol and attach a function implementation to the symbol in lockstep, but the two steps can also be performed independently. The `define` keyword defines a symbol without any initial overloads. The `overload` keyword extends an already-defined symbol with new implementations.

    // Example
    define abs;
    overload abs(x:Int) = if (x < 0) -x else x;
    overload abs(x:Float) = if (x < 0.) -x else if (x == 0.) 0. else x;

A `define` may also define an interface constraint for the symbol by following the symbol name with a list of [arguments] and optional [return types]. All overloads attached to the symbol must conform to a subset of the specified interface. If the `define` form does not specify arguments or return types, then the argument types and/or return types of the created symbol's overloads are unconstrained.

    // Example
    [T | Numeric?(T)]
    define abs(x:T) : T;

    [T | Integer?(T)]
    overload abs(x:T) = if (x < 0) -x else x;
    [T | Float?(T)]
    overload abs(x:T) = if (x < 0.) -x else if (x == 0.) 0. else x;

    // Not valid because Numeric?(String) is false
    overload abs(x:String) {
        if (x[0] == "-")
            return sliceFrom(x, 1);
        else
            return x;
    }

Overloads can extend not only symbols created by `define`, but type name symbols as well. Overloading type names is used idiomatically to implement constructor functions for types.

    // Example
    record LatLong (latitude:Float64, longitude:Float64);
    record Address (street:String, city:String, state:String, zip:String);

    overload Address(coords:LatLong) = geocode(coords);

Overloads bind to symbols by [pattern matching]. Overloads may thus attach to parameterized type names for all or some of their possible parameters.

    // Example
    record Point[T] (x:T, y:T);

    // default-initialize points to a known-bad value
    [T | Float?(T)]
    overload Point[T]() = Point[T](nan(T), nan(T));

    overload Point[Int]() = Point[Int](-0x8000_0000, 0x7FFF_FFFF);

The unparameterized base name of a type also may be overloaded independently of its parameterized instances.

    // Example
    // if no T given, give them a Point[Float]
    overload Point() = Point[Float]();

Within a module, overloads are matched to a call site's symbol name and argument types in reverse definition order. (If you think this is stupid, you are correct.) The first overload that matches gets instantiated for the call site.

    // Example
    define foo(x);

    // oops, the second overload gets visited first and always wins
    overload foo(x:Int) { println("Hey, over here!"); }

    overload foo(x) {
        println("Pay no attention to that overload behind the curtain.");
    }

Match order among overloads from different modules is done walking the import graph depth-first. (If you think is this _really_ stupid, you are correct.) Resolution order among circularly-dependent modules is undefined.

    // Example
    // a.clay
    define foo(x);
    // visited last
    overload foo(x:Int) {}

    // b.clay
    import a;
    // visited second
    overload a.foo(x:Int) {}

    // c.clay
    import b;
    import a;
    // visited first
    overload a.foo(x:Int) {}

Symbols created by [simple function definitions] may also be overloaded; in fact, a simple function definition is simply shorthand for an unconstrained `define` followed by an `overload`.

    // Example
    double(x) = x+x;
    // is equivalent to:
    define double;
    overload double(x) = x+x;

Universal overloads are supported as a special case, in which the overloaded symbol name is itself a pattern variable.

    // Example
    record MyInt (value:Int);

    // delegate any function called with a MyInt to be called on its Int value
    [F]
    overload F(x:MyInt) = ..F(x.value);

    // Implement a default constructor for any Numeric? type
    [T | Numeric?(T)]
    overload T() = T(0);

Such overloads are matched against all call sites that aren't matched first by a more specific overload. In other words, universal overloads are ordered after all specific overloads.

Note that if a call site does not use a [symbol](#symbols) in the function position, it is desugared into a call to the `call` [operator function](#operatorfunctions).

    // Example
    record MyCallable ();

    overload call(f:MyCallable, x:Int, y:Int) : Int = x + y;

    main() {
        var f = MyCallable();
        println(f(1, 2)); // really call(f, 1, 2)
    }

### Arguments

    # Grammar
    Arguments -> "(" ArgumentList ")"

    ArgumentList -> variadic_list(Argument, VarArgument)

    Argument -> NamedArgument
              | StaticArgument

    NamedArgument -> ReferenceQualifier? Identifier TypeSpec?

Arguments are declared in a parenthesized list of names, each name optionally followed by a type specifier. The type specifiers are matched to the types of a call site's input values using [pattern matching]. If an argument is declared without a type specifier, it has an implicit unbounded pattern variable as its type specifier.

    // Example
    // Define double(x) for any type of x, with an explicit pattern var
    [T]
    double(x:T) = x+x;

    // Define double(x) for any type of x, with less typing
    double(x) = x+x;

Arguments are passed by reference.

    // Example
    inc(x:Int) {
        x += 1;
    }

    main() {
        var x = 2;
        println(x); // prints 2
        inc(x);
        println(x); // prints 3
    }

#### Variadic arguments

    # Grammar
    VarArgument -> ReferenceQualifier? ".." Identifier TypeSpec?

The final argument of a function may be declared as variadic by being prefixed with a `..` token, in which case it will match all remaining arguments after the previously-matched arguments. The argument name will be bound as a [multiple value variable](#multiplevalueexpressions).

    // Example
    // Print ..stuff n times
    printlnTimes(n:Int, ..stuff) {
        for (i in range(n))
            println(..stuff);
    }

    main(args) {
        printlnTimes(3, "She loves you ", "yeah yeah yeah");
    }

The types of the variadic argument's values may be bound to a variadic pattern variable. In the argument specification, the multi-type variable's name does not require a second `..` token.

    // Example
    // Print ..stuff n times, for only String? ..stuff
    [..TT | allValues?(String?, ..TT)]
    printlnTimes(n:Int, ..stuff:TT) {
        for (i in range(n))
            println(..stuff);
    }

    // Call a CodePointer object with input values matching its input types
    [..In, ..Out]
    overload call(f:CodePointer[[..In], [..Out]], ..in:In) : ..Out {
        return ..f(..in);
    }

#### Reference qualifiers

    # Grammar
    ReferenceQualifier -> "ref" | "rvalue" | "forward"

Argument declarations may be prefixed with an optional reference qualifier. Clay distinguishes "lvalues", which are values bound to a variable, referenced through a pointer, or otherwise with externally-referenceable identities, from "rvalues", which are unnamed temporary values that will exist only for the extent of a single function call. (The terms come from an lvalue being a valid expression on the **L**eft side of an assignment statement, whereas an rvalue is only valid on the **R**ight side.) Local and global variables are lvalues, as are any expressions that [return by reference](#returnstatements); the results of return-by-value expressions are rvalues.

    // Example
    // The result of `2+2` in this example is bound to `x` and is thus an lvalue
    lv() {
        var x = 2 + 2;
        f(x);
    }

    // The result of `2+2` in this example is temporary and is thus an rvalue
    rv() {
        f(2 + 2);
    }

Since rvalues are only referenceable for the duration of a single function call, functions can take advantage of this fact to perform move optimization. They can reclaim resources from their rvalue arguments instead of allocating new resources. In an argument list, an argument name may be annotated with the `rvalue` keyword, in which case it will only match rvalues. Rvalues will be deleted using the `destroy` [operator function](#operatorfunctions) at the end of the statement for which they were allocated.

    // Example
    foo(rvalue x:String) {
        // Use `move` to steal x's buffer, then append " world" to the end
        return move(x) + " world";
    }

Conversely, an argument name may be annotated with the `ref` keyword, in which case it will only match lvalues.

    // Example
    bar(ref x:String) {
        // Return a slice referencing the first five characters of x;
        // a bad idea if x is an rvalue and will be deallocated soon after
        // we return
        return sliced(x, 0, 5);
    }

Without a `ref` or `rvalue` qualifier, the argument will match either lvalues or rvalues of a matching type. Within the function body, the argument's name will be bound as an lvalue.

    // Example
    foo(rvalue x:Int) {}

    bar(x:Int) {
        // ERROR: our name `x` is an lvalue, even if bound to an rvalue from the caller
        foo(x);
    }

    main() {
        bar(2+2);
    }

If a function needs to carry the `ref`- or `rvalue`-ness of an arbitrary argument through to other functions, it may qualify that argument with the `forward` keyword.

    // Example
    foo(rvalue x:Int) {}

    bar(forward x:Int) {
        foo(x); // ok: we forward the rvalue-ness of our input from main()
    }

    main() {
        bar(2+2);
    }

The `ref`, `rvalue`, and `forward` qualifiers may be applied to a variadic argument name, in which case the qualifier applies to all of the values the name matches. `forward` variadic arguments are especially useful for providing "perfect forwarding" through wrapper functions that should not alter the behavior of the functions they wrap.

    // Example
    trace(f, forward ..args) {
        println("enter ", f);
        finally println("exit  ", f);

        return forward ..f(..args);
    }

#### Static arguments

    # Grammar
    StaticArgument -> "static" Pattern

Functions may match against values computed at compile-time using `static` arguments. A `static` argument matches against the result of a corresponding [static expression](#staticexpressions) at the call site using [pattern matching].

    // Example
    define beetlejuice;

    [n]
    overload beetlejuice(static n) {
        for (i in range(n))
            println("Beetlejuice!");
    }

    // Unroll the common case
    overload beetlejuice(static 3) {
        println("Beetlejuice!");
        println("Beetlejuice!");
        println("Beetlejuice!");
    }

    main() {
        beetlejuice(static 3);
    }

In expressions, [symbols] and [static strings] are implicitly static and will also match `static` arguments.

    // Example
    define defaultValue;

    [T]
    defaultValue(static T) = T(0);

    [T | Float?(T)]
    defaultValue(static T) = nan(T);

    main() {
        println(defaultValue(Int)); // 0
        println(defaultValue(Float64)); // nan
    }

`static` arguments are implemented as syntactic sugar for an unnamed argument of primitive type `Static[T]`.

    // Example
    [n] foo(static n) { }
    // is equivalent to (aside from binding the name 'x'):
    [n] foo(x:Static[n]) { }

### Return types

    # Grammar
    ReturnSpec -> ReturnTypeSpec
                | NamedReturnSpec

    ReturnTypeSpec -> ":" ExprList

A function definition may declare its return types by following the argument list with a `:` and a [multiple value expression] that evaluates to the return types. The expression may refer to any pattern variables bound by the argument list. If a [return statement] in the function body returns values of types that do not match the declared types, it is an error.

    // Example
    double(x:Int) : Int = x + x;

    [T]
    diagonal(x:T) : Point[T] = Point[T](x, x);

    [T | Integer?(T)]
    safeDouble(x:T) : NextLargerInt(T) {
        alias NextT = NextLargerInt(T);
        return NextT(x) + NextT(x);
    }

If the function does not declare its return types, they will be inferred from the function body. To declare that a function returns no values, an empty return declaration (or a declaration that evaluates to no values) may be used.

    // Example
    foo() { } // infers no return values from the body

    foo() : { } // explicitly declares no return values

    foo() : () { } // also explicitly declares no return values

#### Named return values

    NamedReturnSpec -> "-->" comma_list(NamedReturn)

    NamedReturn -> ".."? Identifier ":" Expression

In special cases where constructing a return value as a whole is inadequate or inefficient, a function may bind names directly referencing its uninitialized return values. The return values may then be constructed piecemeal using [initialization statements]. Named returns are declared by following the argument list with a `-->` token. Similar to [arguments], each subsequent named return value is declared with a name followed by a type specifier. Unlike arguments, the type specifier is required and is evaluated as an expression rather than matched as a pattern. A variadic named return may also be declared prefixed with a `..` token, in which case the type expression is evaluated as a [multiple value expression](#multiplevalueexpressions).

Note that named return values are inherently unsafe (hence the intentionally awkward syntax). They start out uninitialized and thus must be initialized with [initialization statements] \(`<--`) rather than [simple assignment statements] \(`=`); any operation other than initialization will have undefined behavior before the value is initialized. Special care must be taken with named returns and exception safety; since named return values are not implicitly destroyed during unwinding, even if partially or fully initialized, explicit [catch blocks] or `onerror` [scope guard statements] must be used to release resources in case an exception terminates the function.

    // Example
    record SOAPoint (xs:Vector[Float], ys:Vector[Float]);

    overload SOAPoint(size:SizeT) --> returned:SOAPoint
    {
        returned.xs <-- Vector[Float]();
        onerror destroy(returned.xs);
        resize(returned.xs, size);

        returned.ys <-- Vector[Float]();
        onerror destroy(returned.ys);
        resize(returned.ys, size);
    }

### Function body

    # Grammar
    FunctionBody -> Block
                  | "=" ReturnExpression ";"
                  | LLVMBlock

The implementation of a function is contained in its body. The most common body form is a [block](#blocks) containing a series of [statements].

    // Example
    demBones(a, b) {
        println(a, " bone's connected to the ", b, " bone");
    }

    main() {
        var bones = array("arm", "shoulder", "thigh", "hip", "leg");
        for (a in bones)
            for (b in bones)
                demBones(a, b);
    }

If the function body consists of a single `return` statement, a shorthand form is provided. In lieu of the block and `return` keyword, `=` can be used after the argument and return type declarations. The `=` form is equivalent to the block-and-`return` form.

    // Example
    square(x) = x*x;
    // is exactly the same as
    square(x) {
        return x*x;
    }

    overload index(a:PitchedArray[T], i) = ref a.arr[i*a.pitch];
    // is exactly the same as
    overload index(a:PitchedArray[T], i) {
        return ref a.arr[i*a.pitch];
    }

#### Inline LLVM functions

    # Grammar
    LLVMBlock -> "__llvm__" "{" /.*/ "}"

A function may also be implemented directly in LLVM assembly language by specifying the body as an `__llvm__` block. The contents of the block will be emitted literally into the function's body.

LLVM blocks currently leak an unfortunate amount of implementation detail. To return values into Clay, named returns must be used. Arguments and named return values are bound as LLVM pointers of the corresponding LLVM type; for instance, an argument `x:Int32` will be available as the `i32*`-typed value `%x` in the LLVM code. Clay functions internally return a value of type `i8*` which should normally be null; thus, all LLVM basic blocks that exit the function must end with `ret i8* null`.

    // Example
    // Who needs `__primitives__.numericAdd`?
    overload add(x:Int32, y:Int32) --> sum:Int32 __llvm__ {
        %xv = load i32* %x
        %yv = load i32* %y
        %sumv = add i32 %xv, %yv
        store i32 %sumv, i32* %sum
        ret i8* null
    }

Static values can be interpolated into the LLVM code using the forms `$Identifier` or `${Expression}`. [Symbols] are interpolated as their underlying LLVM type; [static strings] are interpolated literally; and static integer, floating-point, and boolean values are interpolated as the equivalent LLVM numeric literals.

    // Example
    // A generic form of the above that works for all integer types

    alias NORMAL_RETURN = #"ret i8* null";

    [T | Integer?(T)]
    overload add(x:T, y:T) --> sum:T __llvm__ {
        %xv = load $T* %x
        %yv = load $T* %y
        %sumv = add $T %xv, %yv
        store $T %sumv, $T* %sum
        $NORMAL_RETURN
    }

If an inline LLVM block references intrinsics, metadata nodes, or other global LLVM symbols, those symbols must be declared in a [top-level LLVM] block.

Inline LLVM function bodies currently cannot be evaluated at compile time.

### Inline and alias qualifiers

    # Grammar
    CodegenAttribute -> "inline" | "alias"

Any simple function or overload definition may be modified by an optional `inline` or `alias` attribute:

* `inline` functions are compiled directly into their caller after their arguments are evaluated. If inlining is impossible (for instance, if the function is recursive), a compile-time error is raised. `inline` function code is also rendered invisible to debuggers or other source analysis tools. Clay's `inline` is intended primarily for trivial operator definitions rather than as the weaker code generation/linkage hint provided by C99 or C++'s `inline` modifiers.
* `alias` functions evaluate their arguments using call-by-name semantics. In short, alias functions behave like C preprocessor macros without the hygiene or precedence issues. In detail: The caller, instead of evaluating the alias function's arguments and passing the values to the function, will pass the argument expressions themselves into the function as closure-like entities. Unlike actual [lambda expressions], references into the caller's scope from alias arguments are statically resolved. Argument expressions are evaluated inside the alias function every time they are referenced. Alias functions are implicitly specialized on their source location and thus can use [compilation context operators] to query their source location and other compilation context information.

 

    // Example
    // An "assert()" function that only evaluates its argument if Debug?() is true

    Debug?() = false;

    define assert(x:Bool);

    [| not Debug?()]
    alias overload assert(x:Bool) { }

    [| Debug?()]
    alias overload assert(x:Bool) {
        if (not x) {
            printlnTo(stderr, __FILE__, ":", __LINE__, ": assertion failed!");
            abort();
        }
    }

### External functions

    # Grammar
    ExternalFunction -> Visibility? "external" AttributeList?
                        Identifier "(" ExternalArgs ")"
                        ":" Type? (FunctionBody | ";")

    ExternalArgs -> variadic_list(ExternalArg, "..")

    ExternalArg -> Identifier TypeSpec

Normal Clay functions are generated with internal linkage, and are compiled and linked together into an executable or object file as a single compilation unit. External function definitions must be used to define the interface with code from outside the unit, whether it be Clay code calling out to existing C or C++ libraries, or C, C++, or Clay code wanting to call into a precompiled Clay library.

An external definition without a function body declares an external entry point for access by Clay code. Variadic C functions may also be declared by including a trailing `..` token in the declared argument list.

    // Example
    // Call out to `puts` and `printf` from libc
    external puts(s:Pointer[Int8]) : Int;
    external printf(fmt:Pointer[Int8], ..) : Int;

    main() {
        puts(cstring("Hello world!"));
        printf(cstring("1 + 1 = %d"), 1 + 1);
    }

An external definition with a body defines a Clay function with external linkage. The function will use C's symbol naming and calling convention (including passed-by-value arguments), in order to be linkable from C or other C-compatible language code.

    // Example
    // square.clay:
    // Implement an function in Clay and make it available to C.
    external square(x:Float64) : Float64 = x*x;

    /*
     * square.c:
     */
    #include <stdio.h>

    double square(double x);

    int main() {
        printf("%g\n", square(2.0));
    }

Compared to internal Clay functions, external functions have several limitations:

* External functions cannot be generic. Their argument and return types must be fully specified. Return types cannot be inferred, and named return bindings cannot be used. The definition cannot include a pattern guard. External functions cannot be overloaded.
* External functions may return only zero or one values.
* `<stdarg.h>`-compatible variadic C functions may be declared and called from Clay, but implementing C variadic functions is currently unsupported.
* Clay exceptions cannot currently be propagated across an external boundary. A Clay exception that is unhandled in an external function will be passed to the `unhandledExceptionInExternal` [operator] function.
* Clay types with nontrivial [value semantics] may not be passed by value to external functions. They must be passed by pointer instead.

Although they define a top-level name, external function names are not true [symbols]. An external function's name instead evaluates directly to a value of the primitive `CCodePointer[[..InTypes], [..OutTypes]]` type representing the external's function pointer.

External functions cannot currently be called by the compile-time evaluator.

#### External attributes

A parenthesized [multiple value expression] list may be provided after the `external` keyword in order to set attributes on the external function. A string value in the attribute list controls the function's external linkage name.

    // Example
    // Bypass crt1 and provide our own entry point
    external write(fildes:Int, buf:Pointer[Int8], nbyte:SizeT) : SSizeT;

    alias STDOUT_FILENO = 1;

    external ("_start") start() {
        var greeting = "hello world";
        write(STDOUT_FILENO, cstring(greeting), size(greeting));
    }

In the `__primitives__` module, symbols are provided that, when used as external attributes, set the calling convention used by the function:

* `AttributeCCall` corresponds to the default C calling convention.
* `AttributeLLVMCall` uses the native LLVM calling convention. This can be used to bind to LLVM intrinsics.
* `AttributeStdCall`, `AttributeFastCall`, and `AttributeThisCall` correspond to legacy x86 calling conventions on Windows.

## Global value definitions

* [Global aliases]
* [Global variables]
* [External variables]

Like many old-fashioned languages, Clay supports global state.

### Global aliases

    # Grammar
    GlobalAlias -> PatternGuard? Visibility?
                   "alias" Identifier PatternVars? "=" Expression ";"

Analogous to [alias functions](#inlineandaliasqualifiers), global aliases define a global expression that is evaluated on a call-by-name basis. This is useful for defining symbolic constants without allocating a true global variable.

    // Example
    alias PI = 3.14159265358979323846264338327950288;

    degreesToRadians(deg:Double) : Double = (PI/180.)*deg;

Global aliases may be parameterized with a pattern guard. If no predicate is necessary, the pattern guard is optional; the given parameters will be taken as unbounded pattern variables.

    // Example
    [T | Float?(T)]
    alias PI[T] = T(3.14159265358979323846264338327950288);

    alias ZERO[T] = T(0); // [T] pattern guard implied

Global alias definitions do not define true [symbols]. The alias name expands directly into the bound expression.

### Global variables

    # Grammar
    GlobalVariable -> PatternGuard? Visibility?
                      "var" Identifier PatternVars? "=" Expression ";"

Global variable definitions instantiate a globally-visible mutable variable. The bound expression is evaluated at runtime, before the `main()` entry point is called, to initialize the global variable. The variable's type is inferred from the type of the expression.

    // Example
    var msg = noisyString();

    // This happens before main()
    noisyString() {
        println("The following message is not endorsed by this global variable.");
        return String();
    }

    a() { push(msg, "Hello "); }
    b() { push(msg, "world!"); }

    main() {
        a();
        b();
        println(msg);
    }

Global variables may be parameterized with a pattern guard. If no predicate is necessary, the pattern guard is optional; the given parameters will be taken as unbounded pattern variables.

    // Example
    // XXX

Global variables are instantiated on an as-needed basis. If a global variable is never referenced by runtime-executed code, it will not be instantiated. Global initializers should not be counted on for effects other than initializing the variable.

    // Example
    var rodney = f();

    // rodney will not be instantiated, and this will not be executed
    f() {
        println("I get no respect, I tell ya");
        return 0;
    }

    main() { }

Global variable initializer expressions are evaluated in dependency order, and it is an error if there is a circular dependency. The order of initialization among independently-initialized global variables is undefined.

    // Example
    var a = c + 1; // runs second
    var b = a + c; // runs third
    var c = 0;     // runs first

    abc() = a + b + c;
    var d = abc(); // runs fourth

    // ERROR: circularity
    var x = y;
    var y = x;

    // ERROR: circularity
    getY() = y;
    var x = getY();
    var y = x;

Global variables are deleted in reverse initialization order using the `destroy` [operator function] after the program's `main` entry point has finished executing.

Since global variable initializers are executed at runtime, global variables are currently poorly supported by compile-time evaluation. The pointer value and type of global variables may be safely derived at compile time; however, the initial value of a global variable at compile time is currently undefined, and though a global variable may be initialized and mutated during compile time, the compile-time value of the global is not propagated in any form to runtime.

Global variable names are not true [symbols]. A global variable name evaluates to a reference to the global variable's value.

Runtime global variable access is subject to the memory model standardized in C11 and C++11. The `__primitives__` module includes primitive atomic operations for synchronized, uninterruptible value access; see the [primitive modules reference](primitives-reference.md) for details.

### External variables

    # Grammar
    ExternalVariable -> Visibility? "external" AttributeList? Identifier TypeSpec ";"

C `extern` variables can be linked to with `external` variable definitions. An external variable definition consists of the variable name followed by its type. Like external functions, external variables cannot be parameterized.

    // Example
    // The year is 1979. Errno is still a global variable...
    external errno : Int;

    main() {
        if (null?(fopen(cstring("hello.txt"), cstring("r"))))
            println("error code ", errno);
    }

Like [external functions], external variable definitions may include an optional attribute list after the `external` keyword. Currently, the only kind of attribute supported is a string, which if provided, specifies the linkage name for the variable.

    // Example
    external ("____errno$OBSCURECOMPATIBILITYTAG") errno : Int;

Externally-linkable global variables defined in Clay are currently unsupported. External variables cannot currently be evaluated by the compile-time evaluator.

## Statements

    Statement -> Block
               | Assignment
               | IfStatement
               | SwitchStatement
               | WhileStatement
               | ForStatement
               | MultiValueForStatement
               | ReturnStatement
               | GotoStatement
               | BreakStatement
               | ContinueStatement
               | ThrowStatement
               | TryStatement
               | ScopeGuardStatement
               | EvalStatement
               | ExprStatement

Statements form the basic unit of control flow within function definitions.

* [Blocks]
* [Expression statements]
* [Return statements]
* [Local variable bindings]
* [Assignment statements]
* [Conditional statements]
* [Loop statements]
* [Branch statements]
* [Exception handling statements]
* [Eval statements]

### Blocks

    # Grammar
    Block -> "{" (Statement | Binding | LabelDef)* "}"

Blocks group sequences of statements and provide scopes for [local variable bindings]. Within a block, statements are executed sequentially except as modified by control flow statements.

    // Example
    main() {
        println("VENI");
        println("VIDI");
        println("VICI");
    }

#### Labels

    # Grammar
    LabelDef -> Identifier ":"

In addition to statements, blocks may also contain [labels], which provide targets for [goto statements]. A label is declared as an identifier followed by a `:` token. Label names are lexically scoped to their parent block.

    // Example
    main() {
    second_verse:
        println("I'm Henry VIII I am");
        println("I'm Henry VIII I am I am");
        goto second_verse;
    }

### Expression statements

    # Grammar
    ExprStatement -> CallWithTrailingBlock
                   | Expression ";"
    CallWithTrailingBlock -> AtomicExpr CallSuffix ":" (Lambda "::")*
                             ArgumentList LambdaArrow Block

The simplest form of statement is a single expression followed by a `;` token. The expression is evaluated, and its return values, if any, are immediately deleted using the `destroy` [operator function](#operatorfunctions).

    // Example
    main() {
        1 + 2;
        println("Hi");
    }

As a special case, if a [call expression](#callexpressions) with a block [lambda expression](#lambdaexpressions) as its final argument is used as a statement, the semicolon after the final lambda block may be omitted.

    // Example
    enableMode(maybeMode:Maybe[Mode]) {
        maybe(maybeMode): mode -> {
            println(mode.name, " mode selected");
        } :: () -> {
            println("Please select a mode");
        }
        // no semicolon
    }

### Return statements

    # Grammar
    ReturnStatement -> "return" ReturnExpression? ";"
    ReturnExpression -> ReturnKind ExprList ";"
    ReturnKind -> "ref" | "forward"

Return statements end execution of the current function and provide the return value or values to which the function evaluates.

    // Example
    foo(x, y) {
        return x + y;
    }

    main() {
        println(foo(1, 2)); // prints 3
    }

The [function body] `=` shorthand syntax is sugar for a [block](#blocks) containing a single return statement.

    // Example
    // shorthand for the above
    foo(x, y) = x + y;

Functions with branching control flow may have multiple return statements. The return types of each return statement within a function must match. If the function declares its return types, return statement values must also match the types declared.

    // Example
    [T | Float?(T)]
    quadraticRoots(a:T, b:T, c:T) : T, T {
        if (b == 0.) {
            var r = sqrt(-c/a);
            return r, -r;
        }

        var q = -0.5*(b+signum(b)*sqrt(b*b - 4.*a*c));
        return q/a, c/q;
    }

If control flow leaves code unreachable after all return statements, the compiler will raise an error.

    // Example
    populateArk() {
        rescueAmphibians();
        rescueReptiles();
        rescueBirds();
        rescueMammals();
        return;

        // ERROR: unreachable code
        rescueDinosaurs();
    }

Functions may return by reference with `return ref`. The return values must all be lvalues, and the function will itself evaluate to an lvalue or lvalues. If there are multiple return statements, their `ref`-ness in addition to their return types must match.

    // Example
    record PitchedVector[T] (vec:Vector[T], pitch:SizeT);

    [T]
    overload index(pv:PitchedVector[T], n) {
        return ref pv.vec[n*pv.pitch];
    }

    record ZippedSequence[A,B] (a:A, b:B)

    [A,B]
    overload index(ab:ZippedSequence[A,B], n) {
        return ref ab.a[n], ab.b[n];
    }

A return statement may also generically forward the `ref`-ness of its values with `return forward`. For each returned value, if the value is an lvalue, it is returned by reference; otherwise, it is returned by value.

    // Example
    // A more generic version of above that works for lvalue or rvalue sequences
    [A,B]
    overload index(ab:ZippedSequence[A,B], n) {
        return forward ab.a[n], ab.b[n];
    }

### Local variable bindings

    Binding -> BindingKind comma_list(Identifier) "=" ExprList ";"
    BindingKind -> "var" | "ref" | "forward" | "alias"

Local variables are introduced by binding statements. Local variables come in a few different flavors:

* `var` bindings create new, independent local values. The right-hand expression is evaluated to initialize the value as if by an [initialization statement](#initializationstatements).

        // Example
        main() {
            var x = 1;
            var y = 2;
            println("x = ", x, "; y = ", y); // x = 1; y = 2
            y = 3;
            println("x = ", x, "; y = ", y); // x = 1; y = 3
        }

    `var`s have deterministic lifetimes. They will be deleted by being passed to the `destroy` [operator function](#operatorfunctions) at the end of their containing block.

        // Example
        record Noisy (x:Int);

        overload Noisy() { println("created"); return Noisy(0); }
        overload destroy(x:Noisy) { println("destroyed"); }

        main() {
            var x = Noisy(); // prints "created"
            println("hello world"); // prints "hello world"
        } // prints "destroyed"

    Deletion of `var`s happens when their scope is exited for any reason, whether by normal control flow, by `return`, `break`, `continue`, or `goto` statements, or by unwinding caused by exceptions.

* `ref` bindings create named references to existing lvalues. (See [Reference qualifiers] for a discussion of lvalues and rvalues.)

        // Example
        main() {
            var x = 1;
            ref y = x;
            println("x = ", x, "; y = ", y); // x = 1; y = 1
            y = 3;
            println("x = ", x, "; y = ", y); // x = 3; y = 3
        }

    A `ref` binding only binds a new name to an existing value and does not affect the lifetime of the value. If the `ref` references a value in dynamically-allocated memory, care must be taken that the `ref` is not referenced after the underlying value is freed.

        // Example
        external malloc(size:SizeT) : Pointer[Int8];
        external free(p:Pointer[Int8]) :;

        main() {
            var p = Pointer[Int](malloc(TypeSize(Int)));
            ref x = p^;
            free(Pointer[Int8](p));
            // oops, the ref x is still in scope, but its underlying value was freed
            x = 0;
        }

* `forward` bindings can be used if a binding needs to be generic over lvalues or rvalues. If the bound value is an lvalue, the bound variable behaves like a `ref` and references the existing value; otherwise, it behaves like `var` and moves the temporary value into a new local value.

        // Example

        main() {
            var xs = range(4); // 0 to 3 as a lazy sequence
            var ys = array(0, 1, 2, 3); // 0 to 3 as an in-memory sequence

            // xs[2] is an rvalue, so x2 will store it in a new `var`
            // ys[2] is an lvalue, so y2 will be a `ref` to ys[2]
            forward x2, y2 = xs[2], ys[2];
        }

* `alias` bindings have call-by-name semantics, like [alias functions](#inlineandaliasqualifiers) or [global aliases]. The alias's name expands to the bound expression (evaluated in its original lexical context) at every call site.

Multiple variable bindings can be assigned to the values of a [multiple value expression]. Each value from the right-hand side is bound to the corresponding variable name on the left-hand side. It is an error if the number of values does not correspond to the number of variables.

    // Example
    twoAndThree() = 2, 3;

    main() {
        // x = 1; y = 2
        var x, y = 1, 2;

        // a = 1; b = 2; c = 3; d = 4
        var a, b, c, d = 1, ..twoAndThree(), 4;
    }

If multiple variables are being bound, the right-hand side is implicitly evaluated in [multiple value context]; in other words, a single expression that evaluates to multiple values may omit the `..` prefix.

    // Example
    oneAndTwo() = 1, 2;

    main() {
        // x = 1; y = 2
        var x, y = oneAndTwo();
    }

Variable names are scoped from after the binding statement to the end of their enclosing block. New binding names may shadow module-global names, variable names from outer scopes, or previous bindings from the same block. Variable names are bound after the right-hand expression is evaluated, so the right-hand expression may refer to outer bindings before they are shadowed.

    // Example
    var x = 1;

    main() {
        println(x); // prints 1
        var x = x + 1;
        println(x); // prints 2
        {
            var x = x + 1;
            println(x); // prints 3
            var x = x + 1;
            println(x); // prints 4
        }
        println(x); // prints 2
    }

Binding statements must appear in a block. A binding statement cannot be used as a single-statement body of an `if`, `while` or other compound statement.

    // Example
    foo() {
        if (true)
            var x = 1; // ERROR
    }

### Assignment statements

    # Grammar
    Assignment -> ExprList AssignmentOp ExprList ";"
    AssignmentOp -> "="
                  | "+=" | "-=" | "*=" | "/=" | "%="
                  | "<--"

Assignment statements update variables with new values.

#### Simple assignment statements

The most common form of assignment uses the `=` token. The [multiple value expression](#multiplevalueexpressions) on the right side of the `=` is evaluated, followed by the left side. It is an error if the number of values on the left and right sides do not match. If both sides evaluate to a single value, the `assign` [operator function](#operatorfunctions) is called with the left and right values.

    // Example
    main() {
        var x = 1;
        x = 2;
        // is equivalent to
        assign(x, 2);
    }

If both sides evaluate to multiple values, the right-hand values are evaluated into temporary values, and each temporary value is then `move`d and assigned to the corresponding left-hand value using `assign`. This allows multiple-value assignment to safely shuffle values.

    // Example
    main() {
        var x = 1;
        var y = 2;
        x, y = y, x;

        // is like

        var tmp1 = y;
        var tmp2 = x;
        assign(x, move(tmp1));
        assign(y, move(tmp2));
    }

    shellGame(shellA, shellB, shellC) {
        shellB, shellC, shellA = shellA, shellB, shellC;
        shellC, shellB, shellA = shellA, shellB, shellC;
        shellA, shellC, shellB = shellA, shellB, shellC;
        shellB, shellA, shellC = shellA, shellB, shellC;
        shellC, shellA, shellB = shellA, shellB, shellC;
    }

The left-hand expression should generally evaluate to an lvalue or lvalues; however, this is not strictly enforced. `assign` may also be overloaded for non-lvalues in order to support temporary proxy-assignment objects. Typical `assign` overloads should qualify their left-hand argument as `ref`.

If the left-hand expression is a single [index](#indexexpressions), [static index](#staticindexexpressions), or [field reference](#fieldreferenceexpressions) expression, special property assignment operator functions are used instead of `assign`.

* `a[..b] = c;` desugars into `indexAssign(a, ..b, c);`
* `a.0 = c;` desugars into `staticIndexAssign(a, static 0, c);`
* `a.field = c;` desugars into `fieldRefAssign(a, #"field", c);`

Property assignment is currently only supported for single assignment. Multiple-value assignment will always be evaluated, `move`d, and `assign`ed as described above.

Assignment is a statement in Clay. Attempting to use assignment in an expression is a syntax error.

    // Example
    foo(x) {
        // ERROR
        if (x = 0)
            zero();
    }

#### Update assignment statements

Like other C-like languages, Clay provides special assignment syntax for updating a value using one of the [additive operators] or [multiplicative operators].

    // Example
    main() {
        var x = 1;
        println(x);
        x += 1;
        println(x);
    }

The assignment tokens `+=`, `-=`, `*=`, `/=`, and `%=` desugar into calls to the `updateAssign` [operator function](#operatorfunctions), which is passed three arguments: the `add`, `subtract`, `multiply`, `divide`, or `remainder` operator symbol corresponding to the update operation; the left-hand value; and the right-hand value. Update assignment is currently only supported for single values.

    // Example
    main() {
        var x = 1;

        x += 1;
        x -= 2;
        x *= 4;
        x /= 8;
        x %= 16;

        // equivalent to

        updateAssign(add, x, 1);
        updateAssign(subtract, x, 2);
        updateAssign(multiply, x, 4);
        updateAssign(divide, x, 8);
        updateAssign(remainder, x, 16);
    }

Like single-value simple assignment, update assignment also supports special property update operators for when the left-hand expression is an [index](#indexexpressions), [static index](#staticindexexpressions), or [field reference](#fieldreferenceexpressions) expression.

* `a[..b] += c;` desugars into `indexUpdateAssign(add, a, ..b, c);`
* `a.0 += c;` desugars into `staticIndexUpdateAssign(add, a, static 0, c);`
* `a.field += c;` desugars into `fieldRefUpdateAssign(add, a, #"field", c);`

#### Initialization statements

The above assignment operators assume that the values being assigned have already been initialized. If the prior value owns resources, the `assign` implementation needs to adjust or release those resources to accommodate the new value. However, if the value being updated is uninitialized, such as when it comes from a primitive memory allocator like `malloc` or is a [named return value](#namedreturnvalues), this cannot be done safely, and indeed, there are no already-existing resources to manage. In these cases, initialization must be performed instead of assignment.

    // Example
    main() {
        var p = allocateRawMemory(Foo);
        finally freeRawMemory(p);

        p^ <-- Foo();
    }

Unlike the other assignment forms, initialization is handled as a primitive. The left-hand expression must evaluate to an lvalue or lvalues. If the right-hand expression returns by value, then a reference to the left-hand side is implicitly passed to the outermost function invocation on the right-hand side. Return statements in the called function will write their return values directly into the left-hand value to initialize it.

    // Example
    foo(p:Pointer[Int]) {
        p^ <-- bar();
    }

    bar() {
        return 2; // The value 2 is directly written into p^
    }

If the right-hand side is an lvalue, the `copy` [operator function](#operatorfunctions), which must return by value, is called to copy the value into the left-hand side.

    // Example
    foo(p:Pointer[Int]) {
        var x = 1;
        p^ <-- x; // really p^ <-- copy(x);
    }

As a special case, if the right-hand side is a `forward` argument bound to an rvalue, the `move` operator function is applied instead of `copy`.

    // Example
    foo(p:Pointer[Int], forward x:Int) {
        p^ <-- x;
    }

    bar(p:Pointer[Int]) {
        var x = 2;
        foo(p, 2); // p^ <-- move(2);
        foo(p, x); // p^ <-- copy(x);
    }

`var` [binding statements] behave identically to initialization statements when initializing their newly-bound variables.

Note that, just as assigning uninitialized values with `=` is undefined, so is initializing already-initialized values with `<--`. The `<--` operator should not generally be used except when necessary.

### Conditional statements

    # Grammar
    IfStatement -> "if" "(" Expression ")" Statement
                   ("else" Statement)?

    SwitchStatement -> "switch" "(" Expression ")"
                       ("case" "(" ExprList ")" Statement)*
                       ("else" Statement)?

Conditional statements alter control flow based on the execution state. `if` statements conditionally execute a statement if an expression, which must evaluate to a value of the primitive `Bool` type, is `true`. An `else` clause may immediately follow the `if` statement, in which case the `else` statement will be executed only if the condition is `false`. Without an `else` clause, execution for a `false` condition resumes after the `if` statement.

    // Example
    explainJoke(asFoghornLeghorn?:Bool) {
        print("That");

        if (asFoghornLeghorn?)
            print(", I say, that");

        print(" was a joke");

        if (asFoghornLeghorn?)
            println(" son");
        else
            println();
    }

Switch statements dispatch to one of several branches based on the value of an expression. The `switch` expression is evaluated, and its value is compared against each subsequent `case` clause until one is found for which the `case?` [operator function](#operatorfunctions) returns true. If no case clause matches, the `else` clause is executed if present; if there is no `else` clause, then execution resumes after the switch statement. After the chosen `case` or `else` clause is executed, execution continues after the switch statement.

    // Example
    enum Suit (Spades, Hearts, Clubs, Diamonds);
    record Card (rank:Int, suit:Suit);

    overload printTo(stream, card:Card) {
        switch (card.rank)
        case (1) // evaluates case?(card.rank, 1)
            printTo(stream, "Ace");
        case (2) // evaluates case?(card.rank, 2)
            printTo(stream, "Deuce");
        case (3, 4, 5, 6, 7, 8, 9, 10) // evaluates case?(card.rank, 3, 4, ...)
            printTo(stream, card.rank);
        case (11)
            printTo(stream, "Jack");
        case (12)
            printTo(stream, "Queen");
        case (13)
            printTo(stream, "King");
        else
            assert(false);

        printTo(stream, " of ", card.suit);
    }

Unlike other languages with `switch` forms, Clay does not allow "fall-through" between cases.

### Loop statements

Loop statements repeatedly execute a statement while a condition holds.

#### While loops

    # Grammar
    WhileStatement -> "while" "(" Expression ")" Statement

A `while` loop evaluates an expression, which must evaluate to a value of the `Bool` primitive type, and if the expression is `true`, it executes the associated body statement. The expression is then reevaluated and the body executed in a loop until the expression evaluates to `false`. When the expression evaluates to `false`, execution resumes after the `while` statement.

    // Example
    main() {
        // Print 0 through 9, the hard way
        var x = 0;
        while (x < 10) {
            println(x);
            x += 1;
        }
    }

#### For loops

    # Grammar
    ForStatement -> "for" "(" comma_list(Identifier) "in" Expression ")" Statement

A `for` loop provides a higher-level looping mechanism than `while` to loop over sequences. It binds values yielded from a iterator object to variables in the scope of its body statement, repeatedly executing the body for each value until the iterator is exhausted.

    // Example
    main() {
        // Print 0 through 9, the easier way
        for (x in range(10))
            println(x);
    }

`for` loops are implemented in terms of `while` loops using the `iterator`, `hasNext?`, and `next` [operator functions]. Before the loop is entered, `iterator(expr)` is called to derive an iterator object for the sequence. Within the loop, on each iteration, `hasNext?(iter)` is called to test whether the iterator has more values, and if true, `next(iter)` is called to fetch the next result from the iterator. The return values from `next` are bound to the loop variables, and the loop body is then executed.

    // Example
    main() {
        // The above example, desugared
        {
            forward _iter = iterator(range(10));
            while (hasNext?(_iter)) {
                forward x = next(_iter);
                println(x);
            }
        }
    }

#### Multiple-value for loops

    # Grammar
    MultiValueForStatement -> ".." "for" "(" Identifier "in" ExprList ")" Statement

A special syntactic form is provided to apply an operation over each individual value of a [multiple value expression]. The iterated expression is evaluated in an implicit [multiple value context].

    // Example
    // implement printTo(stream, ..xs) in terms of individual printTo(stream, x)
    // overloads
    [..TT | countValues(..TT) != 1]
    overload printTo(stream, ..xs:TT) {
        ..for (x in xs)
            printTo(stream, x);
    }

Unlike `while` or `for`, `..for` is not a proper loop. It is unrolled; that is, the body is instantiated repeatedly for each value. The bound variable is locally rebound for each instantiation; thus, unlike a normal `for` loop's bindings, the `..for` binding's type may change for each instantiation.

    // Example
    foo() {
        ..for (x in 1, '2', "three")
            bar(x);

        // desugars to

        {
            forward x = 1;
            bar(x);
        }
        {
            forward x = '2';
            bar(x);
        }
        {
            forward x = "three";
            bar(x);
        }
    }

### Branch statements

Branch statements provide nonlocal control flow within a function.

#### Break and continue statements

    BreakStatement -> "break" ";"
    ContinueStatement -> "continue" ";"

`break` and `continue` prematurely halt execution of a `while`, `for`, or `..for` loop. `continue` resumes execution at the next iteration of the loop, and `break` resumes execution after the loop. `break` and `continue` apply to the innermost loop in which they lexically appear; they are invalid outside of a loop.

    // Example
    // XXX

#### Goto statements

    # Grammar
    GotoStatement -> "goto" Identifier ";"

`goto` statements jump to (almost) arbitrary [labels] within the function. There are some restrictions: goto statements may not jump into a `var` binding's scope from outside (and thereby skip the variable's initialization). Jumping from an outer block into a label defined in an inner block is also currently unsupported.

    // Example
    // XXX

### Exception handling statements

Clay optionally supports exception handling. The `ExceptionsEnabled?` function from the `__primitives__` module will return true when exceptions are enabled for the current compilation unit. Regardless of whether exception handling is enabled for runtime, the compile-time evaluator does not currently support exception handling; the compile-time evaluator always behaves as if exceptions are disabled.

#### Throw statements

    ThrowStatement -> "throw" Expression ";"

The `throw` statement throws an exception, to be caught by a [try block](#tryblocks) from an enclosing scope. Execution jumps from the throw to the innermost matching catch clause in the current dynamic scope, unwinding any intervening stack frames and destroying their local variables along the way.

    // Example
    // XXX

The exception is thrown by calling the `throwValue` [operator function](#operatorfunctions) with the value of the given expression as an input. The call is assumed not to locally return; even if exceptions are disabled, `throwValue` must otherwise arrange for execution to terminate (such as by calling `abort` from libc).

#### Try blocks

    TryStatement -> "try" Block
                    ("catch" "(" (Identifier (":" Type)?) ")" Block)+

Try statements execute their associated block normally. If an exception occurs during the dynamic extent of the try statement, the exception is caught and tested against the try block's associated catch clauses, and if a matching catch clause is found, execution resumes inside the matching clause. Catch clauses are matched by declared type, and the thrown exception value is bound as the specified type. A catch-all clause can also be declared without a type; the clause will match any exception not caught by a previous catch clause. If no catch clause matches the exception, it is rethrown to be caught by an outer scope.

    // Example
    // XXX

A series of catch clauses desugars into the try block's exception handler as a series of `if` statements constructed from the `exceptionIs?`, `exceptionAs`, `exceptionAsAny`, and `continueException` [operator functions], as well as the `activeException` primitive function from the `__primitives__` module, which returns a pointer to the exception that instigated the current unwinding. Clay does not yet provide a facility for fully manually coding a handler.

    // Example
    foo(x) {
        try {
            throw x;
        }
        catch (a:A) {
            handleA(a);
        }
        catch (b:B) {
            handleB(b);
        }
        catch (any) {
            handleAny(any);
        }
    }

    // foo's handler will look like this:
    fooHandler() {
        var exp = activeException();
        if (exceptionIs?(A, exp)) {
            forward a = exceptionAs(A, exp);
            handleA(a);
        }
        else if (exceptionIs?(B, exp)) {
            forward b = exceptionAs(B, exp);
            handleB(b);
        }
        else {
            forward any = exceptionAsAny(exp);
            handleAny(any);
        }
        // Without a catch-all, the final branch would continue unwinding:
        /*
        else
            continueException();
        */
    }

Catch clauses may rethrow the current exception by reusing the given exception object in a [throw statement](#throwstatements) inside the catch clause.

If exceptions are disabled, a try statement's body is compiled as a normal block, and its catch clauses are ignored.

#### Scope guard statements

    ScopeGuardStatement -> ScopeGuardKind Statement
    ScopeGuardKind -> "finally" | "onerror"

Scope guard statements provide a convenient shorthand for deterministically and safely performing required cleanup in the face of arbitrary nonlocal control flow. A `finally` statement causes the associated statement to be executed when the enclosing block is exited for any reason, much like a `var`'s destructor but without the associated `var`.

    // Example
    // XXX

An `onerror` statement causes the associated statement to be executed only when the enclosing block is unwound by an exception. If the block is exited by normal control flow, or by `break`, `continue`, `goto`, or `return`, the associated statement will not be executed.

    // Example
    // XXX

If exceptions are disabled, `onerror` scope guards are ignored. `finally` scope guards behave normally and will still execute when their associated scope is exited by non-exceptional means.

### Eval statements

    EvalStatement -> "eval" ExprList ";"

Eval statements provide compile-time access to the Clay parser. The associated [multiple value expression] is evaluated at compile time and concatenated into a [static string](#staticstrings), which is then parsed and expanded into one or more statements. Those statements then take the eval statement's place in execution. Eval statements may synthesize any statement, including new variable bindings.

    // Example
    // XXX

The result of `eval` must be parsable as a complete statement or statements; it cannot synthesize half-open blocks, partial statements, or other incomplete constructs. Labels currently cannot be generated by `eval`.

    // Example
    foo() {
        // ERROR: can't create a half-block
        eval #"{"; }

        // ERROR: can't create a half-statement
        eval #"var x ="; 1;
    }

[Eval expressions] are also supported for generating expressions.

## Expressions

    # Grammar
    Expression -> PairExpr
                | IfExpr
                | Unpack
                | StaticExpr
                | Lambda
                | OrExpr

Expressions describe how values flow among functions in a computation. Clay provides a hierarchy of operators with which to construct expressions. Many operators are syntactic sugar for overloadable [operator functions]. The precedence hierarchy for Clay is summarized as follows, from highest to lowest precedence, along with sample syntax and associated operator functions where appropriate.

* Atomic expressions
    * [Name references]
    * [Literal expressions]
    * [Parentheses] — `(a, b, c)`
    * [Tuple expressions] — `[a, b, c]` — `tupleLiteral`
    * [Compilation context operators] — `__FILE__` etc.
    * [Eval expressions] — `eval a`
* Suffix operators
    * [Call expressions] — `a(b,c)` — `call`
    * [Index expressions] — `a[b,c]` — `index`
    * [Static index expressions] — `a.0` — `staticIndex`
    * [Field reference expressions] — `a.field` — `fieldRef`
    * [Dereference expressions] — `a^` — `dereference`
* Prefix operators
    * [Unary plus] — `+a` — `plus`
    * [Unary minus] — `-a` — `minus`
    * [Address operator] — `&a`
    * [Dispatch operator] — `*a`
* [Multiplicative operators]
    * `a * b` — `multiply`
    * `a / b` — `divide`
    * `a % b` — `remainder`
* [Additive operators]
    * `a + b` — `add`
    * `a - b` — `subtract`
* [Ordered comparison operators]
    * `a <= b` — `lesserEquals?`
    * `a < b` — `lesser?`
    * `a > b` — `greater?`
    * `a >= b` — `greaterEquals?`
* [Equality comparison operators]
    * `a == b` — `equals?`
    * `a != b` — `notEquals?`
* [Boolean not] — `not a`
* [Boolean and] — `a and b`
* [Boolean or] — `a or b`
* Low-precedence prefix operators
    * [If expressions] — `if (a) b else c`
    * [Keyword pair expressions] — `name: a`
    * [Static expressions] — `static a`
    * [Unpack operator] — `..a`
    * [Lambda expressions] — `(a, b) -> c`
* [Multiple value expressions] — `a, b, c`

### Atomic expressions

    # Grammar
    AtomicExpr -> NameRef
                | Literal
                | ParenExpr
                | TupleExpr
                | Literal
                | EvalExpr
                | ContextOp

Atomic expressions form the basic units of expressions.

#### Name references

    # Grammar
    NameRef -> Identifier

A name reference consists simply of an identifier. It evaluates to the named local or global entity within the current scope. An error is raised if no matching entity is found for the given name.

#### Literal expressions

    # Grammar
    Literal -> BoolLiteral
             | IntLiteral
             | FloatLiteral
             | CharLiteral
             | StringLiteral
             | StaticStringLiteral
    BoolLiteral -> "true" | "false"

    IntLiteral -> Sign? IntToken NumericTypeSuffix?
    FloatLiteral -> Sign? FloatToken NumericTypeSuffix?

    Sign -> "+" | "-"
    NumericTypeSuffix -> Identifier

    CharLiteral -> CharToken
    StringLiteral -> StringToken
    StaticStringLiteral -> "#" StringToken
                         | "#" Identifier

Literals evaluate to primitive constant values.

* The boolean literals `true` and `false` evaluate to the two possible values of the primitive `Bool` type.
* Integer literals evaluate to constant integer values. They consist of an [integer literal token](#integerliterals), optionally prefixed by a sign `+` or `-` and/or suffixed by a type specifier. Without a type specifier suffix, the literal is of the primitive `Int32` type, unless the module declares a different default integer type in its [module attributes]. To create a literal of another type, the following suffixes are allowed, with the corresponding primitive types:
    * `ss` — `Int8` ("short short")
    * `s` — `Int16` ("short")
    * `i` — `Int32` ("int")
    * `l` — `Int64` ("long")
    * `ll` — `Int128` ("long long")
    * `uss` — `UInt8`
    * `us` — `UInt16`
    * `u` — `UInt32`
    * `ul` — `UInt64`
    * `ull` — `Int128`

    One of the floating-point type suffixes described below may also be applied to an integer literal to create a floating-point literal with an integer value. If the integer literal represents a value that is not within the valid range of the literal's type, it is an error.

        // Example
        // XXX

* Floating-point literals evaluate to constant floating-point real or imaginary values. They consist of a [floating-point literal token](#floatingpointliterals), and like an integer can be modified by an optional sign prefix and/or type specifier suffix. Without a suffix, the literal is of the primitive `Float64` type, unless the module declares a different default floating-point type in its [module attributes]. To create a literal of another type, the following suffixes are allowed:
    * `f` — `Float32` ("single")
    * `ff` — `Float64` ("double")
    * `fl` — `Float80`
    * `fj` — `Imag32`
    * `j` or `ffj` — `Imag64`
    * `flj` — `Imag80`

    Floating-point literals may not use integer suffixes.

        // Example
        // XXX

* Character literals consist of a [character literal token](#characterliterals). They are evaluated by passing the ASCII code of the represented character to the `Char` [operator function](#operatorfunctions).

        // Example
        // XXX

* String literals consist of a [string literal token](#stringliterals). The constant string is emitted into the string table, and pointers of the primitive `Pointer[Int8]` type to the first character and after the last character are passed as arguments to the `StringConstant` [operator function](#operatorfunctions).

        // Example
        // XXX

* Static string literals consist of a string literal token prefixed with a `#` token. They exist entirely as compile-time entities, and thus evaluate to values of the stateless primitive `Static[#"identifier"]` type. If the contents of the static string are a valid identifier, it may also be specified as a bare, unquoted identifier prefixed with a `#` token.

        // Example
        // XXX

#### Parentheses

    # Grammar
    ParenExpr -> "(" ExprList ")"

Parentheses override precedence order and have no effect on evaluation themselves.

#### Tuple expressions

    # Grammar
    TupleExpr -> "[" ExprList "]"

Tuple expressions are used to construct tuple objects. They are evaluated by passing the bracketed expression list to the `tupleLiteral` [operator function](#operatorfunctions).

#### Compilation context operators

    # Grammar
    ContextOp -> "__FILE__"
               | "__LINE__"
               | "__COLUMN__"
               | "__ARG__" Identifier

[Alias functions](#inlineandaliasqualifiers) have magic powers to access the source location of their invocation and string representations of their arguments. The `__FILE__` operator evaluates to a [static string](#staticstring) containing the file from which the function was called. `__LINE__` and `__COLUMN__` evaluate to `Int32` values representing the source line and column. `__ARG__` returns a static string representation of the argument named after the `__ARG__` token. `__ARG__` does not evaluate the named argument. These four operators are only valid inside alias functions, and `__ARG__` is only valid applied to an alias function argument.

    // Example
    // An assert function that reports the condition and source location with the failure
    alias assert(cond:Bool) {
        if (cond) {
            println(stderr, "Assertion \"", __ARG__ cond, "\" failed at ",
                __FILE__, ":", __LINE__, ":", __COLUMN__);
            flush(stderr);
            abort();
        }
    }

#### Eval expressions

    # Grammar
    EvalExpr -> "eval" Expression

Like [eval statements], eval expressions provide access to the Clay parser at compile time, but in expression context. The given expression is evaluated at compile time into a [static string](#staticstrings), and the result is parsed as an expression and evaluated in place of the `eval` form.

    // Example
    // XXX

The generated static string must parse as a complete expression; open brackets or other partial expressions cannot be generated.

### Multiple value expressions

    # Grammar
    ExprList -> comma_list(Expression)

Clay functions, and thereby most expression forms, can return multiple values. The comma operator concatenates values into a multiple value list.

    // Example

However, for sanity's sake, expressions are normally constrained to single values, and it is an error to use a multiple-value expression in a single value context. (Zero values is considered "multiple values" by this rule.)

    // Example

Where multiple value expressions are allowed, a multiple value context may be introduced using the [unpack operator] `..`. The multiple values of the `..` expression are unpacked directly into the surrounding multiple value list.

    // Example

Certain syntactic forms provide implicit multiple value context:

* [Expression statements] are evaluated in multiple value context; the expression's values, if any, are discarded.
* [Local variable bindings] with multiple variables evaluate their right-hand expression in multiple value context.
* [Assignment statements] with multiple left-hand values evaluate their right-hand expression in multiple value context.
* [Multiple-value for loops] evaluate their value list in multiple value context.

In these contexts, a lone multiple value expression may be used without an explicit unpack. However, within a multiple value expression that concatenates multiple expressions, multiple-value subexpressions still require an explicit unpack. (In other words, the `..` operator has higher precedence than the `,` operator, and implicit multiple value context only applies to the outermost precedence level.)

    // Example
    // XXX

    # Grammar
    PairExpr -> Identifier ":" Expression
    IfExpr -> "if" "(" Expression ")" Expression "else" Expression
    Unpack -> ".." Expression
    StaticExpr -> "static" Expression

    OrExpr -> AndExpr ("or" OrExpr)?

    AndExpr -> NotExpr ("and" AndExpr)?

    NotExpr -> "not" EqualExpr
             | EqualExpr

    EqualExpr -> (EqualExpr EqualOp)? CompareExpr
    EqualOp -> "==" | "!="

    CompareExpr -> (CompareExpr CompareOp)? AddExpr
    CompareOp -> "<=" | "<" | ">" | ">="

    AddExpr -> (AddExpr AddOp)? MulExpr
    AddOp -> "+" | "-"

    MulExpr -> (MulExpr MulOp)? PrefixExpr
    MulOp -> "*" | "/" | "%"

    PrefixExpr -> PrefixOp SuffixExpr
                | SuffixExpr
    PrefixOp -> "+" | "-" | "&" | "*"

    SuffixExpr -> AtomicExpr SuffixOp*
    SuffixOp -> CallSuffix
              | IndexSuffix
              | FieldRefSuffix
              | StaticIndexSuffix
              | DereferenceSuffix

    CallSuffix -> "(" ExprList ")" CallLambdaList?
    CallLambdaList -> ":" Lambda ("::" Lambda)*

    IndexSuffix -> "[" ExprList "]"
    FieldRefSuffix -> "." Identifier
    StaticIndexSuffix -> "." IntToken
    DereferenceSuffix -> "^"

    Lambda -> Arguments LambdaArrow LambdaBody
    LambdaArrow -> "=>" | "->"
    LambdaBody -> Block
                | ReturnExpression


