" Vim syntax file
" Language: clay
" Maintainer: Shivram Khandeparker <shivram@tachyontech.net>
" Last Change: 2010 June 3

" Quit when a custom syntax file was already loaded
if exists("b:current_syntax")
    finish
endif

syn keyword clayKeyword public private import as record overloadable overload external static callbyname lvalue rvalue enum var ref and or not if else goto return returnref while switch case default break continue for in try catch throw alias finally procedure

syn keyword clayType Bool Int8 Int16 Int32 Int64 UInt8 UInt16 UInt32 UInt64 Float32 Float64 Pointer CodePointer RefCodePointer CCodePointer Array Tuple Void Byte CChar CUChar CString Short UShort Int UInt Long ULong Float Double RawPointer SizeT PtrInt UPtrInt CLong CULong Vector String

syn keyword clayConstant true false

syn region clayString start=+L\="+ skip=+\\\\\|\\"+ end=+"+ contains=@Spell

syntax region clayComment start="/\*" end="\*/"
syntax region clayComment start="//" end="$"

hi def link clayKeyword     Statement
hi def link clayType        Type
hi def link clayConstant    Constant
hi def link clayComment     Comment
hi def link clayString      Number

let b:current_syntax = "clay"
