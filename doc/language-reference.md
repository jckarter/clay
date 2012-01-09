# Clay Language Reference 0.1

* [Conventions]
* [Tokenization]
* [Source file organization]
* [Type definitions]
* [Function definitions]
* [Global value definitions]
* [Statements]
* [Expressions]
* [Patterns and guards]

## Conventions

BNF grammar rules are provided in `monospace` blocks beginning with `# Grammar`. Concrete examples of syntax are provided in monospace blocks beginning with `// Example`. Regular expressions in grammar rules are delimited by `/` and use Perl `/x` syntax; that is, whitespace between the delimiting slashes is insignificant and used for readability. Literal strings in grammar rules are delimited by `"`.

## Tokenization

### Source encoding

Clay source code is stored as a stream of ASCII text. (Anything other than ASCII is currently passed through as an opaque stream of bytes by the compiler. This will change to proper UTF-8 encoding in the future.)

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

.

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

String literals represent a sequence of ASCII text. Syntactically, they consist of zero or more characters or escape codes as described under [Characters], delimited by either matching `"` characters or by matching `"""` sequences. Within `"`-delimited strings, the characters `"` and `\` must be escaped, whereas in `"""`-delimited strings, only `\` need be escaped. (The sequence `"""` may be escaped by escaping one or more of the `"` characters.) Whitespace within string literals is significant, including newlines.

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

## Source file organization

    # Grammar
    Module -> Import* ModuleDeclaration? TopLevelLLVM? TopLevelItem*

A clay module corresponds to a single source file. Source files are laid out in the following order:

* Zero or more [import declarations]
* An optional [module declaration]
* An optional [top-level LLVM] block
* The body of the module file, consisting of zero or more [top-level definitions]

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
        println(foo.bar.apple()); // Call member `apple` of module `foo.bar`
    }

Alternately, a local alias for the module may be provided with `as`, in which case module members are instead accessed through the given alias name:

    // Example
    import foo.bar as bar; // Import module `foo.bar` as `bar`

    main() {
        println(bar.apple()); // Call member `apple` of module `foo.bar` (alias `bar`)
    }

Individual members of a module may be imported directly into the current module's namespace, with or without aliases:

    // Example
    import foo.bar.(            // Import selected members of `foo.bar`:
        apple,                  //      public member `foo.bar.apple` as `apple`
        mandarin as clementine, //      public member `foo.bar.mandarin` as `clementine`
    );

    main() {
        println(apple());      // Call member `apple` of module `foo.bar`
        println(clementine()); // Call member `mandarin` of module `foo.bar` (alias `clementine`)
    }

Private members of a module can also be imported by explicitly prefixing the member name with `private`. (You should only do this if you know what you're doing.)

    // Example
    import foo.bar.(private banana);

    main() {
        println(banana());
    }

Finally, all of the public members of a module may be imported directly into the current module's namespace using `.*`. (You should also only do this if you know what you're doing.)

    // Example
    import foo.bar.*; // Import all public members of `foo.bar`

    main() {
        println(apple());
        println(mandarin());
    }

#### Conflict resolution

It is an error to import two modules or module members using the same name.

    // Example
    import foo;
    import bar as foo; // ERROR
    import bar.(foo); // ERROR
    import bar.(bar as foo); // ERROR

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
    ModuleDeclaration -> "in" DottedName ("(" ExprList ")") ";"

A module may optionally declare its name using the `in` keyword.

    // Example
    import foo.bar;

    in foo.bas;

Such a declaration must appear after any [import declarations] and before any [top-level LLVM] or other [top-level definitions]. The module declaration may optionally include a parenthesized list of module attributes. If no attribute list is provided, it is equivalent to the empty list.

    // Example
    in foo.bas ();
    in foo.bas (Float32);

The module attribute list can be an arbitrary multi-value expression, which can reference any imported symbols but not symbols defined within the module itself.

    // Example
    // foo.clay
    GraphicsModuleAttributes() = Float32, FastMath;

    // bar.clay
    import foo;

    in bar (..foo.GraphicsModuleAttributes());

### Top-level LLVM

    # Grammar
    TopLevelLLVM -> LLVMBlock
    LLVMBlock -> "__llvm__" "{" /.*/ "}"

A module may optionally contain a top-level block of LLVM assembly language. The given code will be emitted directly into the top level of the generated LLVM module, and may contain function declarations or other global declarations needed by `__llvm__` blocks in individual functions containing [inline LLVM]. The top-level LLVM block must appear after any [import declarations] or [module declaration] and before any [top-level definitions].

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

### Top-level definitions

    # Grammar
    TopLevelItem -> Record              # Types
                  | Variant
                  | Instance
                  | Enumeration
                  | Define              # Functions
                  | Overload
                  | Procedure
                  | ExternalProcedure
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

Most top-level definitions in Clay share common syntactic features.

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

The list of declared pattern variables may optionally be followed by a `|` character and predicate expression to constrain the domain of possible values for the variables in the following definition. The definition will only apply for values of the given pattern variables if the given predicate evaluates to true.

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

Visibility modifiers are not allowed on definitions that modify existing symbols instead of creating new ones. In forms that do admit a visibility modifier, it must appear after the pattern guard (if any) but before the subsequent definition.

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

More complex record types can be derived using a computed body. If the type name is followed by an `=` token, the following expression is evaluated to determine the layout of the record. The expression must evaluate to a tuple of pairs, each pair containing a static string field name and a field type.

    // Example
    // Equivalent to the non-computed Point[T] definition:
    record Point[T] = [[#"x", T], [#"y", T]];

    // Point with our own coordinate names
    // e.g. p:Point[Complex, #"zw"] would have Complex-typed fields p.z and p.w
    record PointWithCoordinates[T, xy] = [[xy.0, T], [xy.1, T]];

### Variants

    # Grammar
    Variant -> PatternGuard? Visibility? "variant" TypeDefinitionName ("(" ExprList ")")? ";"

Variant types provide a "discriminated union" type. A variant value can contain a value of any of the type's instance types. The variant value knows what type it contains, and the contained value can be given to an appropriately-typed function using [dispatch expressions]. The variant's instance types are defined after the variant name in a parenthesized list:

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

The variant instance list may be an arbitrary multiple-value expression. It is evaluated to derive the set of instances.

    // Example
    private RainbowTypes(Base) =
        Base[Red], Base[Orange], Base[Yellow], Base[Green],
        Base[Blue], Base[Indigo], Base[Violet];

    // Fruit will admit as instances Apple[Red], Banana[Violet], etc.
    variant Fruit (..RainbowTypes(Apple), ..RainbowTypes(Banana));

#### Extending variants

    # Grammar
    Instance -> PatternGuard? "instance" Pattern "(" ExprList ")" ";"

Variant types are open and can be extended using the `instance` keyword.

    // Example
    variant Exception (); // type Exception starts with no members

    // introduce RangeError and TypeError as instances of Exception
    record RangeError (lowerBound:Int, upperBound:Int, attemptedValue:Int);
    record TypeError (expectedTypeName:String, attemptedTypeName:String);
    instance Exception (RangeError, TypeError);

Parameterized variant types may be extended given particular parameter values or over
all or some parameter values with the use of pattern guards:

    // Example
    [C | Color?(C)]
    variant Fruit[C] ();

    // Extend Fruit[Yellow] to contain Banana
    instance Fruit[Yellow] (Banana);

    // Extend Fruit[Red] and Fruit[Green] to contain Apple
    [C | inValues?(C, Red, Green)]
    instance Fruit[C] (Apple);

    // Extend all Fruit[C] to contain Berry[C]
    [C]
    instance Fruit[C] (Berry[C]);

`instance` definitions bind to variant types by [pattern matching] the name in the instance definition to each variant type name. Unlike `variant` forms, the pattern guard is not optional when extending a variant generically; without a pattern guard, `instance Variant[T]` will attempt to extend only the type `Variant[T]` for the concrete parameter value `T` (which would need to be defined for the form to be valid) rather than for all values `T`.

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

Unlike record or variant types, enumeration types cannot currently be parameterized and do not allow pattern guards.

### Lambda types

Lambda types are record-like types that capture values from their enclosing scope. Unlike records, variants, or enumerations, they are not explicitly defined in top-level forms, but are implicitly created as needed when [lambda expressions] are used.

## Function definitions

    Define -> PatternGuard? "define" Identifier (Arguments ReturnSpec?)? ";"

    Overload -> PatternGuard? CodegenAttribute? "overload"
                Pattern Arguments ReturnSpec? Body

    Procedure -> PatternGuard? Visibility? CodegenAttribute?
                 Identifier Arguments ReturnSpec? Body

    CodegenAttribute -> "inline" | "alias"

### Arguments

    Arguments -> "(" ArgumentList ")"

    ArgumentList -> comma_list(Argument), ("," ".." Identifier)?
                  | (".." Identifier)?

    Argument -> ValueArgument
              | StaticArgument

    ValueArgument -> ("ref" | "rvalue" | "forward")? Identifier TypeSpec?
    StaticArgument -> "static" Expression

### Returns

    ReturnSpec -> ReturnTypeSpec
                | NamedReturnSpec

    ReturnTypeSpec -> ":" comma_list(Type)

    NamedReturnSpec -> "-->" comma_list(NamedReturn)

    NamedReturn -> ".."? Identifier ":" Type

    Body -> "=" ReturnExpression ";"
          | Block

    Type -> Expression

### External functions

    ExternalProcedure -> Visibility? "external" Identifier "(" ExternalArgs ")"
                         Type? (Body | ";")

    ExternalArgs -> comma_list(ExternalArg) ("," "..")?
                  | ("..")?

    ExternalArg -> Identifier TypeSpec

## Global value definitions

    GlobalVariable -> PatternGuard? Visibility? "var" Identifier PatternVars? "=" Expression ";"

    GlobalAlias -> PatternGuard? Visibility? "alias" Identifier PatternVars? "=" Expression ";"

    ExternalVariable -> Visibility? "external" Identifier TypeSpec ";"

## Statements

XXX everything below this is out of date

    Block -> "{" Statement+ "}"

    Statement -> Block
               | LabelDef
               | VarBinding
               | RefBinding
               | AliasBinding
               | Assignment
               | InitAssignment
               | UpdateAssignment
               | IfStatement
               | GotoStatement
               | SwitchStatement
               | ReturnStatement
               | ExpressionStatement
               | WhileStatement
               | BreakStatement
               | ContinueStatement
               | ForStatement
               | TryStatement
               | ThrowStatement
               | StaticForStatement
               | UnreachableStatement

    LabelDef -> Identifier ":"

    VarBinding -> "var" comma_list(Identifier) "=" Expression ";"
    RefBinding -> "ref" comma_list(Identifier) "=" Expression ";"
    AliasBinding -> "alias" Identifier "=" Expression ";"

    Assignment -> Expression "=" Expression ";"

    InitAssignment -> Expression "<--" Expression ";"

    UpdateAssignment -> Expression UpdateOp Expression ";"
    UpdateOp -> "+=" | "-=" | "*=" | "/=" | "%="

    IfStatement -> "if" "(" Expression ")" Statement
                   ("else" Statement)?

    GotoStatement -> "goto" Identifier ";"

    SwitchStatement -> "switch" "(" Expression ")" "{" CaseBlock* DefaultBlock? "}"
    CaseBlock -> CaseLabel+ CaseBody
    CaseLabel -> "case" Expression ":"
    DefaultBlock -> "default" ":" CaseBody
    CaseBody -> Statement+

    ReturnStatement -> "return" ReturnExpression? ";"
    ReturnExpression -> comma_list("ref"? Expression)

    ExpressionStatement -> Expression ";"

    WhileStatement -> "while" "(" Expression ")" Statement
    BreakStatement -> "break" ";"
    ContinueStatement -> "continue" ";"

    ForStatement -> "for" "(" Identifier "in" Expression ")" Statement

    TryStatement -> "try" Block "catch" Block
    ThrowStatement -> "throw" Expression? ";"

    StaticForStatement -> "static" "for" "(" Identifier "in" Expression ")"
                          Statement

    UnreachableStatement -> "unreachable" ";"

## Expressions

    Expression -> OrExpr
                | IfExpr
                | Lambda
                | Unpack
                | New
                | StaticExpr
                | PairExpr

    OrExpr -> AndExpr ("or" AndExpr)*
    AndExpr -> NotExpr ("and" NotExpr)*
    NotExpr -> "not"? CompareExpr

    EqualExpr -> CompareExpr (EqualOp CompareExpr)?
    EqualOp -> "==" | "!="

    CompareExpr -> AddSubExpr (CompareOp AddSubExpr)
    CompareOp -> "<" | "<=" | ">" | ">="

    AddSubExpr -> MulDivExpr (("+" | "-") MulDivExpr)*
    MulDivExpr -> PrefixExpr (("*" | "/" | "%") PrefixExpr)*

    PrefixExpr -> SignExpr
                | AddressOfExpr
                | DispatchExpr
                | SuffixExpr

    SignExpr -> ("+" | "-") SuffixExpr
    AddressOfExpr -> "&" SuffixExpr
    DispatchExpr -> "*" SuffixExpr
    SuffixExpr -> AtomicExpr Suffix*
    Suffix -> IndexSuffix
            | CallSuffix
            | FieldRefSuffix
            | TupleRefSuffix
            | DereferenceSuffix

    IndexSuffix -> "[" comma_list(Expression)? "]"
    CallSuffix -> "(" comma_list(Expression)? ")"
    FieldRefSuffix -> "." Identifier
    TupleRefSuffix -> "." IntLiteral
    DereferenceSuffix -> "^"

    AtomicExpr -> ArrayExpr
                | TupleExpr
                | NameRef
                | Literal

    ArrayExpr -> "[" comma_list(Expression) "]"
    TupleExpr -> "(" comma_list(Expression)? ")"

    NameRef -> DottedName

    Literal -> BoolLiteral
             | IntLiteral
             | CharLiteral
             | StringLiteral
             | IdentifierLiteral

    BoolLiteral -> "true" | "false"

    IdentifierLiteral -> "#" Identifier

    IfExpr -> "if" "(" Expression ")" Expression
              "else" Expression

    Lambda -> LambdaArgs "ref"? "=>" (Expression | Block)
    LambdaArgs -> Identifier
                | "(" LambdaArgs2 ")"
                | "(" ")"

    LambdaArgs2 -> "..." Identifier
                 | comma_list(Identifier) ("," "..." Identifier)?

    Unpack -> "..." Expression

    New -> "new" Expression

    StaticExpr -> "static" Expression

    PairExpr -> identifier ":" Expression

## Pattern matching

    Pattern -> AtomicPattern PatternSuffix?
    AtomicPattern -> IntLiteral
                   | NameRef
    PatternSuffix -> "[" comma_list(Pattern) "]"
