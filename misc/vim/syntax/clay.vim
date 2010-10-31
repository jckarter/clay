" Vim syntax file
" Language: clay
" Maintainer: Joe Groff <joe@duriansoftware.com>
" Last Change: 2010 Oct 31

" Quit when a custom syntax file was already loaded
if exists("b:current_syntax")
    finish
endif

syn keyword clayKeyword public private import record variant instance procedure overload external alias callbyname lvalue rvalue inline enum var if else goto return while switch case default break continue for try catch throw

syn keyword clayOperatorKeyword and or not new static forward ref as in

syn keyword clayType Bool Int8 Int16 Int32 Int64 UInt8 UInt16 UInt32 UInt64 Float32 Float64 Pointer CodePointer RefCodePointer CCodePointer StdCallCodePointer FastCallCodePointer Array Tuple Void Byte UByte Char Short UShort Int UInt Long ULong Float Double RawPointer SizeT PtrInt UPtrInt StringConstant Vec Union Static

syn keyword clayBoolean true false

syn region clayQuotedIdentifier start=+#"+ skip=+\\\\\|\\"+ end=+"+
syn region clayString start=+"+ skip=+\\\\\|\\"+ end=+"+
syn region clayChar start=+'+ skip=+\\\\\|\\'+ end=+'+

syn region clayComment start="/\*" end="\*/"
syn region clayComment start="//" end="$"

syn match clayDecimal /[+\-]\?\<\(0\|[1-9][0-9_]*\)\([.][0-9_]*\)\?\([eE][+\-]\?[0-9][0-9_]*\)\?\(i8\|u8\|i16\|u16\|i32\|u32\|i64\|u64\|f32\|f64\|u\|f\)\?\W\@=/
syn match clayHexInt /[+\-]\?\<0x[0-9A-Fa-f][0-9A-Fa-f_]*\(i8\|u8\|i16\|u16\|i32\|u32\|i64\|u64\|f32\|f64\|u\|f\)\?\>/
syn match claySimpleIdentifier /#[A-Za-z_][A-Za-z0-9_?]*\>/

hi def link clayKeyword     Statement
hi def link clayType        Type
hi def link clayBoolean     Boolean
hi def link clayComment     Comment
hi def link clayString      String
hi def link clayChar        Character
hi def link clayDecimal     Number
hi def link clayHexInt      Number
hi def link claySimpleIdentifier Constant
hi def link clayQuotedIdentifier Constant
hi def link clayOperatorKeyword Operator

let b:current_syntax = "clay"
