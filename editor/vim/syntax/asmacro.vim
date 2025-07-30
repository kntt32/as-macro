" ~/.vim/syntax.asmacro.vim

syntax match asmacroComment "//.*$"
syntax region asmacroComment start="/\*" end="\*/"
syntax region asmacroString start=+"+ end=+"+ skip=+\\"+
syntax region asmacroChar start=+'+ end=+'+ skip=+\\'+
syntax match asmacroNumber "\<\(0[xX][0-9a-fA-F]\+\|[0-9]\+\|0[bB][01]\+\)\>"
syntax keyword asmacroBool true false
syntax keyword asmacroNull null
syntax keyword asmacroKeyword as if else return fn pub const static let import in out inout optional while for return extern template impl defer sizeof alignof
syntax keyword asmacroRegister rax rcx rdx rbx rdi rsi rsp rbp r8 r9 r10 r11 r12 r13 r14 r15 rip xmm0 xmm1 xmm2 xmm3 xmm4 xmm5 xmm6 xmm7 xmm8 xmm9 xmm10 xmm11 xmm12 xmm13 xmm14 xmm15 auto
syntax keyword asmacroStorage stack imm mem reg xmm
syntax match asmacroOperator "[\.@:]"
syntax match asmacroDivider "[,;]"
syntax keyword asmacroUserType struct enum union type
syntax keyword asmacroPrimitiveType bool char i8 i16 i32 i64 b8 b16 b32 b64 bin fn

highlight link asmacroComment Comment
highlight link asmacroString String
highlight link asmacroChar Character
highlight link asmacroNumber Number
highlight link asmacroBool Boolean
highlight link asmacroNull Keyword
highlight link asmacroKeyword Keyword
highlight link asmacroRegister StorageClass
highlight link asmacroStorage StorageClass
highlight link asmacroOperator Operator
highlight link asmacroDivider Delimiter
highlight link asmacroUserType Type
highlight link asmacroPrimitiveType Type

