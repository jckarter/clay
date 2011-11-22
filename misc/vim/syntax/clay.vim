" Vim syntax file
" Language: clay
" Maintainer: Joe Groff <joe@duriansoftware.com>
" Last Change: 2011 Nov 21

" Quit when a custom syntax file was already loaded
if exists("b:current_syntax")
    finish
endif

" Include ! and ? in keyword characters
setlocal iskeyword=33,48-57,63,65-90,95,97-122

syn keyword clayKeyword public private import record variant instance define overload external alias inline enum var if else goto return while switch break continue for try catch throw onerror finally

syn keyword clayLabelKeyword case default
syn keyword clayOperatorKeyword and or not static forward ref as in rvalue

syn keyword clayType Bool Int8 Int16 Int32 Int64 Int128 UInt8 UInt16 UInt32 UInt64 UInt128 Float32 Float64 Float80 Float128 Pointer CodePointer RefCodePointer CCodePointer StdCallCodePointer FastCallCodePointer RawPointer OpaquePointer Array Tuple Void Byte UByte Char Short UShort Int UInt Long ULong Float Double RawPointer SizeT PtrInt UPtrInt StringConstant Vec Union Static

syn keyword clayBoolean true false

syn keyword clayDebug observeTo observe observeCallTo observeCall

syn region clayString start=+"+ skip=+\\\\\|\\"+ end=+"+
syn region clayIdentifier start=+#"+ skip=+\\\\\|\\"+ end=+"+
syn region clayTripleString start=+"""+ skip=+\\\\\|\\"+ end=+""""\@!+
syn region clayTripleIdentifier start=+#"""+ skip=+\\\\\|\\"+ end=+""""\@!+

syn region clayComment start="/\*" end="\*/"
syn region clayComment start="//" end="$"

syn match clayDecimal /\.\@<![+\-]\?\<\([0-9][0-9_]*\)\([.][0-9_]*\)\?\([eE][+\-]\?[0-9][0-9_]*\)\?\(i\|i8\|u8\|i16\|u16\|i32\|u32\|i64\|i128\|u64\|u128\|f32\|f64\|f80\|f128\|u\|f\|fj\|j\|j32\|j64\|j80\|j128\)\?\w\@!/
syn match clayHex /\.\@<![+\-]\?\<0x[0-9A-Fa-f][0-9A-Fa-f_]*\(\([.][0-9A-Fa-f_]*\)\?[pP][+\-]\?[0-9][0-9_]*\)\?\(i\|i8\|u8\|i16\|u16\|i32\|u32\|i64\|u64\|f32\|f64\|f80\|f128\|u\|f\|fj\|j\|j32\|j64\|j80\|j128\)\?\>/
syn match claySimpleIdentifier /#[A-Za-z_?][A-Za-z0-9_?]*\>/
syn match clayChar /'\([^'\\]\|\\\(["'trnf0$\\]\|x[0-9a-fA-F]\{2}\)\)'/
syn match clayGotoLabel /^\s*[A-Za-z_?][A-Za-z0-9_?]*\(:\s*$\)\@=/

syn match clayMultiValue /\.\.\.\?/
syn match clayLambda /=>\|->/

hi def link clayKeyword     Statement
hi def link clayType        Type
hi def link clayBoolean     Boolean
hi def link clayComment     Comment
hi def link clayTripleString String
hi def link clayString      String
hi def link clayChar        Character
hi def link clayDecimal     Number
hi def link clayHex         Number
hi def link claySimpleIdentifier Constant
hi def link clayIdentifier Constant
hi def link clayTripleIdentifier Constant
hi def link clayOperatorKeyword Operator
hi def link clayLabelKeyword Label
hi def link clayGotoLabel    Label
hi def link clayMultiValue   Special
hi def link clayLambda       Special
hi def link clayDebug        Todo

let b:current_syntax = "clay"
