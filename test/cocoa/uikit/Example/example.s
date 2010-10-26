	.section	__TEXT,__text,regular,pure_instructions
	.section	__TEXT,__textcoal_nt,coalesced,pure_instructions
	.section	__TEXT,__const_coal,coalesced
	.section	__TEXT,__picsymbolstub4,symbol_stubs,none,16
	.section	__TEXT,__StaticInit,regular,pure_instructions
	.syntax unified
	.section	__TEXT,__text,regular,pure_instructions
	.align	2
_clayglobals_init:                      @ @clayglobals_init
Leh_func_begin0:
@ BB#0:                                 @ %init
	stmdb	sp!, {r4, r5, r6, r7, lr}
	add	r7, sp, #12
	stmdb	sp!, {r8, r11}
	sub	sp, sp, #103, 30	@ 412
	bic	sp, sp, #7
	ldr	r0, LCPI0_157
	ldr	r1, LCPI0_158
	mov	r5, #0
	
LPC0_1:
	add	r0, pc, r0
	
LPC0_0:
	ldr	r1, [pc, r1]
	ldr	r2, [r1]
	str	r5, [r0, #128]
	stmia	r0, {r2, r5}
	ldr	r0, LCPI0_159
	
LPC0_2:
	add	r0, pc, r0
	bl	_objc_getClass
	cmp	r0, #0
	bne	LBB0_11
@ BB#1:                                 @ %ifTrue.i
	ldr	r5, LCPI0_160
	
LPC0_3:
	add	r5, pc, r5
	ldr	r5, [r5, #128]
	str	r5, [sp, #16]
	cmp	r5, #0
	beq	LBB0_4
@ BB#2:                                 @ %ifTrue.i
	cmp	r5, #1
	bne	LBB0_5
@ BB#3:                                 @ %ifTrue13.i4.i.i
	ldr	r5, LCPI0_161
	
LPC0_4:
	add	r5, pc, r5
	ldr	r0, [r5, #140]
	bl	_free
LBB0_4:                                 @ %clay_destroy(Exception).exit.i.i
	ldr	r5, LCPI0_162
	mov	r0, #2
	ldr	r1, LCPI0_163
	
LPC0_5:
	add	r5, pc, r5
	str	r0, [r5, #128]
	
LPC0_6:
	add	r0, pc, r1
	str	r0, [r5, #132]
	add	r0, r0, #8
	str	r0, [r5, #136]
	b	LBB0_10
LBB0_5:                                 @ %ifMerge20.i10.i.i
	sub	r0, r5, #2
	cmp	r0, #2
	blo	LBB0_4
@ BB#6:                                 @ %ifMerge20.i10.i.i
	cmp	r5, #4
	cmpne	r5, #5
	beq	LBB0_4
@ BB#7:                                 @ %ifMerge20.i10.i.i
	cmp	r5, #6
	cmpne	r5, #7
	beq	LBB0_4
@ BB#8:                                 @ %clay_destroy(Exception).exit25.i.i
	add	r0, sp, #16
LBB0_9:                                 @ %clay_destroy(Exception).exit25.i.i
	bl	"_clay_error(StringConstant, Int32)"
LBB0_10:                                @ %exception
                                        @ =>This Inner Loop Header: Depth=1
	ldr	r5, LCPI0_164
	
LPC0_7:
	ldr	r3, [pc, r5]
	mov	r1, #36
	mov	r2, #1
	ldr	r5, LCPI0_165
	
LPC0_8:
	add	r0, pc, r5
	bl	_fwrite
	bl	_abort
	b	LBB0_10
LBB0_11:                                @ %normal2
	ldr	r1, LCPI0_166
	
LPC0_9:
	add	r4, pc, r1
	str	r0, [r4, #8]
	ldr	r1, LCPI0_167
	
LPC0_10:
	add	r0, pc, r1
	bl	_sel_registerName
	str	r0, [r4, #12]
	str	r5, [r4, #16]
	str	r5, [r4, #20]
	ldr	r0, LCPI0_168
	
LPC0_11:
	add	r0, pc, r0
	bl	_sel_registerName
	str	r0, [r4, #24]
	ldr	r0, LCPI0_169
	
LPC0_12:
	add	r0, pc, r0
	bl	_sel_registerName
	str	r0, [r4, #28]
	ldr	r0, LCPI0_170
	
LPC0_13:
	add	r0, pc, r0
	bl	_sel_registerName
	str	r0, [r4, #32]
	ldr	r0, LCPI0_171
	
LPC0_14:
	add	r0, pc, r0
	bl	_sel_registerName
	str	r0, [r4, #36]
	ldr	r0, LCPI0_172
	
LPC0_15:
	add	r0, pc, r0
	bl	_sel_registerName
	str	r0, [r4, #40]
	ldr	r1, LCPI0_173
	ldr	r0, [r4, #8]
	mov	r2, #0
	
LPC0_16:
	add	r1, pc, r1
	bl	_objc_allocateClassPair
	cmp	r0, #0
	bne	LBB0_20
@ BB#12:                                @ %ifTrue.i221
	ldr	r0, LCPI0_174
	
LPC0_17:
	add	r0, pc, r0
	ldr	r0, [r0, #128]
	str	r0, [sp, #160]
	cmp	r0, #0
	beq	LBB0_15
@ BB#13:                                @ %ifTrue.i221
	cmp	r0, #1
	bne	LBB0_16
@ BB#14:                                @ %ifTrue13.i4.i11.i
	ldr	r0, LCPI0_175
	
LPC0_18:
	add	r0, pc, r0
	ldr	r0, [r0, #140]
	bl	_free
LBB0_15:                                @ %clay_destroy(Exception).exit.i19.i
	ldr	r0, LCPI0_176
	mov	r1, #3
	ldr	r2, LCPI0_177
	
LPC0_19:
	add	r0, pc, r0
	str	r1, [r0, #128]
	
LPC0_20:
	add	r1, pc, r2
	str	r1, [r0, #132]
	add	r1, r1, #8
	str	r1, [r0, #136]
	ldr	r1, LCPI0_178
	
LPC0_21:
	add	r1, pc, r1
	str	r1, [r0, #140]
	add	r1, r1, #18
	str	r1, [r0, #144]
	b	LBB0_10
LBB0_16:                                @ %ifMerge20.i10.i17.i
	sub	r1, r0, #2
	cmp	r1, #2
	blo	LBB0_15
@ BB#17:                                @ %ifMerge20.i10.i17.i
	cmp	r0, #4
	cmpne	r0, #5
	beq	LBB0_15
@ BB#18:                                @ %ifMerge20.i10.i17.i
	cmp	r0, #6
	cmpne	r0, #7
	beq	LBB0_15
@ BB#19:                                @ %clay_destroy(Exception).exit25.i18.i
	add	r0, sp, #160
	b	LBB0_9
LBB0_20:                                @ %ifMerge.i223
	mov	r4, r0
	mov	r0, r4
	bl	_object_getClass
	ldr	r0, LCPI0_179
	mov	r2, #4
	mov	r3, #2
	
LPC0_23:
	add	r0, pc, r0
	str	r0, [sp]
	ldr	r0, LCPI0_180
	
LPC0_22:
	add	r1, pc, r0
	mov	r0, r4
	bl	_class_addIvar
	tst	r0, #255
	beq	LBB0_26
@ BB#21:                                @ %whileBody.i48.preheader.i
	ldr	r0, LCPI0_181
	mov	r2, #4
	mov	r3, #2
	
LPC0_25:
	add	r0, pc, r0
	str	r0, [sp]
	ldr	r0, LCPI0_182
	
LPC0_24:
	add	r1, pc, r0
	mov	r0, r4
	bl	_class_addIvar
	tst	r0, #255
	bne	LBB0_38
@ BB#22:                                @ %ifTrue100.i
	ldr	r4, LCPI0_183
	
LPC0_32:
	add	r4, pc, r4
	ldr	r4, [r4, #128]
	str	r4, [sp, #312]
	cmp	r4, #0
	beq	LBB0_25
@ BB#23:                                @ %ifTrue100.i
	cmp	r4, #1
	bne	LBB0_34
@ BB#24:                                @ %ifTrue13.i4.i66.i
	ldr	r4, LCPI0_184
	
LPC0_33:
	add	r4, pc, r4
	ldr	r0, [r4, #140]
	bl	_free
LBB0_25:                                @ %clay_destroy(Exception).exit.i74.i
	ldr	r4, LCPI0_185
	mov	r0, #4
	ldr	r1, LCPI0_186
	
LPC0_34:
	add	r4, pc, r4
	str	r0, [r4, #128]
	
LPC0_35:
	add	r0, pc, r1
	str	r0, [r4, #132]
	add	r0, r0, #8
	str	r0, [r4, #136]
	ldr	r0, LCPI0_187
	
LPC0_36:
	add	r0, pc, r0
	str	r0, [r4, #140]
	add	r0, r0, #18
	str	r0, [r4, #144]
	ldr	r0, LCPI0_188
	
LPC0_37:
	add	r0, pc, r0
	str	r0, [r4, #148]
	add	r0, r0, #14
	str	r0, [r4, #152]
	b	LBB0_10
LBB0_26:                                @ %ifTrue57.i224
	ldr	r4, LCPI0_189
	
LPC0_26:
	add	r4, pc, r4
	ldr	r4, [r4, #128]
	str	r4, [sp, #216]
	cmp	r4, #0
	beq	LBB0_29
@ BB#27:                                @ %ifTrue57.i224
	cmp	r4, #1
	bne	LBB0_30
@ BB#28:                                @ %ifTrue13.i4.i38.i
	ldr	r4, LCPI0_190
	
LPC0_27:
	add	r4, pc, r4
	ldr	r0, [r4, #140]
	bl	_free
LBB0_29:                                @ %clay_destroy(Exception).exit.i46.i
	ldr	r4, LCPI0_191
	mov	r0, #4
	ldr	r1, LCPI0_192
	
LPC0_28:
	add	r4, pc, r4
	str	r0, [r4, #128]
	
LPC0_29:
	add	r0, pc, r1
	str	r0, [r4, #132]
	add	r0, r0, #8
	str	r0, [r4, #136]
	ldr	r0, LCPI0_193
	
LPC0_30:
	add	r0, pc, r0
	str	r0, [r4, #140]
	add	r0, r0, #18
	str	r0, [r4, #144]
	ldr	r0, LCPI0_194
	
LPC0_31:
	add	r0, pc, r0
	str	r0, [r4, #148]
	add	r0, r0, #6
	str	r0, [r4, #152]
	b	LBB0_10
LBB0_30:                                @ %ifMerge20.i10.i44.i
	sub	r0, r4, #2
	cmp	r0, #2
	blo	LBB0_29
@ BB#31:                                @ %ifMerge20.i10.i44.i
	cmp	r4, #4
	cmpne	r4, #5
	beq	LBB0_29
@ BB#32:                                @ %ifMerge20.i10.i44.i
	cmp	r4, #6
	cmpne	r4, #7
	beq	LBB0_29
@ BB#33:                                @ %clay_destroy(Exception).exit25.i45.i
	add	r0, sp, #216
	b	LBB0_9
LBB0_34:                                @ %ifMerge20.i10.i72.i
	sub	r0, r4, #2
	cmp	r0, #2
	blo	LBB0_25
@ BB#35:                                @ %ifMerge20.i10.i72.i
	cmp	r4, #4
	cmpne	r4, #5
	beq	LBB0_25
@ BB#36:                                @ %ifMerge20.i10.i72.i
	cmp	r4, #6
	cmpne	r4, #7
	beq	LBB0_25
@ BB#37:                                @ %clay_destroy(Exception).exit25.i73.i
	add	r0, sp, #78, 30	@ 312
	b	LBB0_9
LBB0_38:                                @ %ifMerge115.i
	ldr	r0, LCPI0_38
	ldr	r2, LCPI0_40
	
LPC0_38:
	add	r0, pc, r0
	str	r0, [sp, #304]
	
LPC0_40:
	add	r2, pc, r2
	str	r2, [sp, #288]
	add	r0, r0, #1
	str	r0, [sp, #308]
	add	r2, r2, #1
	str	r2, [sp, #292]
	ldr	r0, LCPI0_39
	add	r2, sp, #232
	
LPC0_39:
	add	r0, pc, r0
	add	r1, r0, #1
	str	r0, [sp, #296]
	str	r0, [sp, #280]
	str	r0, [sp, #224]
	str	r1, [sp, #300]
	str	r1, [sp, #284]
	str	r1, [sp, #228]
	add	r0, sp, #70, 30	@ 280
	add	r1, sp, #224
	bl	"_clay_add(StringConstant, StringConstant)"
	cmp	r0, #0
	bne	LBB0_10
@ BB#39:                                @ %normal26.i.i.i
	add	r0, sp, #18, 28	@ 288
	add	r1, sp, #232
	add	r2, sp, #248
	bl	"_clay_add(StringConstant, Vector[Char])"
	mov	r5, r0
	ldr	r0, [sp, #240]
	bl	_free
	cmp	r5, #0
	bne	LBB0_10
@ BB#40:                                @ %normal28.i.i.i
	add	r0, sp, #74, 30	@ 296
	add	r1, sp, #248
	add	r2, sp, #66, 30	@ 264
	bl	"_clay_add(StringConstant, Vector[Char])"
	mov	r5, r0
	ldr	r0, [sp, #256]
	bl	_free
	cmp	r5, #0
	bne	LBB0_10
@ BB#41:                                @ %clay_SelectorTypeEncoding(Static[ExampleAppDelegate], Static[#applicationc58didFinishLaunchingWithOptionsc58]).exit.i
	add	r0, sp, #19, 28	@ 304
	add	r1, sp, #66, 30	@ 264
	add	r2, sp, #144
	bl	"_clay_add(StringConstant, Vector[Char])"
	mov	r5, r0
	ldr	r0, [sp, #272]
	bl	_free
	cmp	r5, #0
	bne	LBB0_10
@ BB#42:                                @ %normal130.i
	ldr	r0, [sp, #144]
	add	r1, r0, #1
	ldr	r2, [sp, #148]
	cmp	r2, r1
	bhs	LBB0_45
@ BB#43:                                @ %clay_reserve(Vector[Char], UInt32).exit.i52.i
	str	r1, [sp, #220]
	add	r0, sp, #144
	add	r1, sp, #220
	bl	"_clay_vectorSetCapacity(Vector[Char], UInt32)"
	cmp	r0, #0
	bne	LBB0_209
@ BB#44:                                @ %clay_reserve(Vector[Char], UInt32).exit.return12_crit_edge.i54.i
	ldr	r0, [sp, #144]
LBB0_45:                                @ %normal132.i
	mov	r1, #0
	ldr	r5, [sp, #152]
	strb	r1, [r5, r0]
	mov	r3, r5
	ldr	r1, LCPI0_41
	
LPC0_41:
	add	r0, pc, r1
	ldr	r1, [r0, #12]
	ldr	r0, LCPI0_42
	
LPC0_42:
	add	r2, pc, r0
	mov	r0, r4
	bl	_class_addMethod
	mov	r6, r0
	mov	r0, r5
	bl	_free
	tst	r6, #255
	bne	LBB0_54
@ BB#46:                                @ %ifTrue136.i
	ldr	r4, LCPI0_43
	
LPC0_43:
	add	r4, pc, r4
	ldr	r4, [r4, #128]
	str	r4, [sp, #212]
	cmp	r4, #0
	beq	LBB0_49
@ BB#47:                                @ %ifTrue136.i
	cmp	r4, #1
	bne	LBB0_50
@ BB#48:                                @ %ifTrue13.i4.i24.i
	ldr	r4, LCPI0_44
	
LPC0_44:
	add	r4, pc, r4
	ldr	r0, [r4, #140]
	bl	_free
LBB0_49:                                @ %clay_destroy(Exception).exit.i32.i
	ldr	r4, LCPI0_45
	mov	r0, #6
	ldr	r1, LCPI0_46
	
LPC0_45:
	add	r4, pc, r4
	str	r0, [r4, #128]
	
LPC0_46:
	add	r0, pc, r1
	str	r0, [r4, #132]
	add	r0, r0, #8
	str	r0, [r4, #136]
	ldr	r0, LCPI0_47
	
LPC0_47:
	add	r0, pc, r0
	str	r0, [r4, #140]
	add	r0, r0, #18
	str	r0, [r4, #144]
	ldr	r0, LCPI0_48
	
LPC0_48:
	add	r0, pc, r0
	str	r0, [r4, #148]
	add	r0, r0, #42
	str	r0, [r4, #152]
	b	LBB0_10
LBB0_50:                                @ %ifMerge20.i10.i30.i
	sub	r0, r4, #2
	cmp	r0, #2
	blo	LBB0_49
@ BB#51:                                @ %ifMerge20.i10.i30.i
	cmp	r4, #4
	cmpne	r4, #5
	beq	LBB0_49
@ BB#52:                                @ %ifMerge20.i10.i30.i
	cmp	r4, #6
	cmpne	r4, #7
	beq	LBB0_49
@ BB#53:                                @ %clay_destroy(Exception).exit25.i31.i
	add	r0, sp, #212
	b	LBB0_9
LBB0_54:                                @ %return156.i
	ldr	r0, LCPI0_49
	add	r1, sp, #168
	add	r2, sp, #176
	
LPC0_49:
	add	r0, pc, r0
	str	r0, [sp, #200]
	add	r0, r0, #1
	str	r0, [sp, #204]
	ldr	r0, LCPI0_50
	
LPC0_50:
	add	r0, pc, r0
	str	r0, [sp, #192]
	add	r0, r0, #1
	str	r0, [sp, #196]
	ldr	r0, LCPI0_51
	
LPC0_51:
	add	r0, pc, r0
	str	r0, [sp, #168]
	add	r0, r0, #1
	str	r0, [sp, #172]
	add	r0, sp, #192
	bl	"_clay_add(StringConstant, StringConstant)"
	cmp	r0, #0
	bne	LBB0_10
@ BB#55:                                @ %clay_SelectorTypeEncoding(Static[ExampleAppDelegate], Static[#dealloc]).exit.i
	add	r0, sp, #200
	add	r1, sp, #176
	add	r2, sp, #128
	bl	"_clay_add(StringConstant, Vector[Char])"
	mov	r5, r0
	ldr	r0, [sp, #184]
	bl	_free
	cmp	r5, #0
	bne	LBB0_10
@ BB#56:                                @ %normal165.i
	ldr	r0, [sp, #128]
	add	r1, r0, #1
	ldr	r2, [sp, #132]
	cmp	r2, r1
	bhs	LBB0_59
@ BB#57:                                @ %clay_reserve(Vector[Char], UInt32).exit.i.i233
	str	r1, [sp, #164]
	add	r0, sp, #128
	add	r1, sp, #164
	bl	"_clay_vectorSetCapacity(Vector[Char], UInt32)"
	cmp	r0, #0
	bne	LBB0_210
@ BB#58:                                @ %clay_reserve(Vector[Char], UInt32).exit.return12_crit_edge.i.i235
	ldr	r0, [sp, #128]
LBB0_59:                                @ %normal167.i
	mov	r1, #0
	ldr	r5, [sp, #136]
	strb	r1, [r5, r0]
	mov	r3, r5
	ldr	r1, LCPI0_52
	
LPC0_52:
	add	r0, pc, r1
	ldr	r1, [r0, #36]
	ldr	r0, LCPI0_53
	
LPC0_53:
	add	r2, pc, r0
	mov	r0, r4
	bl	_class_addMethod
	mov	r6, r0
	mov	r0, r5
	bl	_free
	tst	r6, #255
	bne	LBB0_68
@ BB#60:                                @ %ifTrue171.i
	ldr	r4, LCPI0_54
	
LPC0_54:
	add	r4, pc, r4
	ldr	r4, [r4, #128]
	str	r4, [sp, #156]
	cmp	r4, #0
	beq	LBB0_63
@ BB#61:                                @ %ifTrue171.i
	cmp	r4, #1
	bne	LBB0_64
@ BB#62:                                @ %ifTrue13.i4.i.i237
	ldr	r4, LCPI0_55
	
LPC0_55:
	add	r4, pc, r4
	ldr	r0, [r4, #140]
	bl	_free
LBB0_63:                                @ %clay_destroy(Exception).exit.i.i245
	ldr	r4, LCPI0_56
	mov	r0, #6
	ldr	r1, LCPI0_57
	
LPC0_56:
	add	r4, pc, r4
	str	r0, [r4, #128]
	
LPC0_57:
	add	r0, pc, r1
	str	r0, [r4, #132]
	add	r0, r0, #8
	str	r0, [r4, #136]
	ldr	r0, LCPI0_58
	
LPC0_58:
	add	r0, pc, r0
	str	r0, [r4, #140]
	add	r0, r0, #18
	str	r0, [r4, #144]
	ldr	r0, LCPI0_59
	
LPC0_59:
	add	r0, pc, r0
	str	r0, [r4, #148]
	add	r0, r0, #7
	str	r0, [r4, #152]
	b	LBB0_10
LBB0_64:                                @ %ifMerge20.i10.i.i243
	sub	r0, r4, #2
	cmp	r0, #2
	blo	LBB0_63
@ BB#65:                                @ %ifMerge20.i10.i.i243
	cmp	r4, #4
	cmpne	r4, #5
	beq	LBB0_63
@ BB#66:                                @ %ifMerge20.i10.i.i243
	cmp	r4, #6
	cmpne	r4, #7
	beq	LBB0_63
@ BB#67:                                @ %clay_destroy(Exception).exit25.i.i244
	add	r0, sp, #156
	b	LBB0_9
LBB0_68:                                @ %normal66
	mov	r0, r4
	bl	_objc_registerClassPair
	ldr	r0, LCPI0_60
	
LPC0_60:
	add	r1, pc, r0
	mov	r0, r4
	bl	_class_getInstanceVariable
	bl	_ivar_getOffset
	ldr	r1, LCPI0_61
	
LPC0_61:
	add	r5, pc, r1
	str	r0, [r5, #16]
	ldr	r0, LCPI0_62
	
LPC0_62:
	add	r1, pc, r0
	mov	r0, r4
	bl	_class_getInstanceVariable
	bl	_ivar_getOffset
	str	r0, [r5, #20]
	ldr	r0, LCPI0_63
	
LPC0_63:
	add	r0, pc, r0
	bl	_objc_getClass
	cmp	r0, #0
	bne	LBB0_77
@ BB#69:                                @ %ifTrue.i247
	ldr	r0, LCPI0_64
	
LPC0_64:
	add	r0, pc, r0
	ldr	r0, [r0, #128]
	str	r0, [sp, #316]
	cmp	r0, #0
	beq	LBB0_72
@ BB#70:                                @ %ifTrue.i247
	cmp	r0, #1
	bne	LBB0_73
@ BB#71:                                @ %ifTrue13.i4.i.i249
	ldr	r0, LCPI0_65
	
LPC0_65:
	add	r0, pc, r0
	ldr	r0, [r0, #140]
	bl	_free
LBB0_72:                                @ %clay_destroy(Exception).exit.i.i257
	ldr	r0, LCPI0_66
	mov	r1, #2
	ldr	r2, LCPI0_67
	
LPC0_66:
	add	r0, pc, r0
	str	r1, [r0, #128]
	
LPC0_67:
	add	r1, pc, r2
	str	r1, [r0, #132]
	add	r1, r1, #16
	str	r1, [r0, #136]
	b	LBB0_10
LBB0_73:                                @ %ifMerge20.i10.i.i255
	sub	r1, r0, #2
	cmp	r1, #2
	blo	LBB0_72
@ BB#74:                                @ %ifMerge20.i10.i.i255
	cmp	r0, #4
	cmpne	r0, #5
	beq	LBB0_72
@ BB#75:                                @ %ifMerge20.i10.i.i255
	cmp	r0, #6
	cmpne	r0, #7
	beq	LBB0_72
@ BB#76:                                @ %clay_destroy(Exception).exit25.i.i256
	add	r0, sp, #79, 30	@ 316
	b	LBB0_9
LBB0_77:                                @ %normal68
	ldr	r1, LCPI0_68
	
LPC0_68:
	add	r4, pc, r1
	str	r0, [r4, #44]
	ldr	r1, LCPI0_69
	
LPC0_69:
	add	r0, pc, r1
	bl	_sel_registerName
	str	r0, [r4, #48]
	ldr	r1, LCPI0_70
	ldr	r0, [r4, #44]
	mov	r2, #0
	
LPC0_70:
	add	r1, pc, r1
	bl	_objc_allocateClassPair
	cmp	r0, #0
	bne	LBB0_86
@ BB#78:                                @ %ifTrue.i260
	ldr	r0, LCPI0_71
	
LPC0_71:
	add	r0, pc, r0
	ldr	r0, [r0, #128]
	str	r0, [sp, #336]
	cmp	r0, #0
	beq	LBB0_81
@ BB#79:                                @ %ifTrue.i260
	cmp	r0, #1
	bne	LBB0_82
@ BB#80:                                @ %ifTrue13.i4.i6.i
	ldr	r0, LCPI0_72
	
LPC0_72:
	add	r0, pc, r0
	ldr	r0, [r0, #140]
	bl	_free
LBB0_81:                                @ %clay_destroy(Exception).exit.i14.i
	ldr	r0, LCPI0_73
	mov	r1, #3
	ldr	r2, LCPI0_74
	
LPC0_73:
	add	r0, pc, r0
	str	r1, [r0, #128]
	
LPC0_74:
	add	r1, pc, r2
	str	r1, [r0, #132]
	add	r1, r1, #8
	str	r1, [r0, #136]
	ldr	r1, LCPI0_75
	
LPC0_75:
	add	r1, pc, r1
	str	r1, [r0, #140]
	add	r1, r1, #21
	str	r1, [r0, #144]
	b	LBB0_10
LBB0_82:                                @ %ifMerge20.i10.i12.i
	sub	r1, r0, #2
	cmp	r1, #2
	blo	LBB0_81
@ BB#83:                                @ %ifMerge20.i10.i12.i
	cmp	r0, #4
	cmpne	r0, #5
	beq	LBB0_81
@ BB#84:                                @ %ifMerge20.i10.i12.i
	cmp	r0, #6
	cmpne	r0, #7
	beq	LBB0_81
@ BB#85:                                @ %clay_destroy(Exception).exit25.i13.i
	add	r0, sp, #21, 28	@ 336
	b	LBB0_9
LBB0_86:                                @ %ifMerge.i262
	mov	r4, r0
	mov	r0, r4
	bl	_object_getClass
	ldr	r0, LCPI0_76
	add	r1, sp, #86, 30	@ 344
	add	r2, sp, #22, 28	@ 352
	
LPC0_76:
	add	r0, pc, r0
	str	r0, [sp, #400]
	add	r0, r0, #1
	str	r0, [sp, #404]
	ldr	r0, LCPI0_77
	
LPC0_77:
	add	r0, pc, r0
	str	r0, [sp, #392]
	add	r0, r0, #1
	str	r0, [sp, #396]
	ldr	r0, LCPI0_78
	
LPC0_78:
	add	r0, pc, r0
	str	r0, [sp, #384]
	add	r0, r0, #1
	str	r0, [sp, #388]
	ldr	r0, LCPI0_79
	
LPC0_79:
	add	r0, pc, r0
	str	r0, [sp, #344]
	add	r0, r0, #1
	str	r0, [sp, #348]
	add	r0, sp, #6, 26	@ 384
	bl	"_clay_add(StringConstant, StringConstant)"
	cmp	r0, #0
	bne	LBB0_10
@ BB#87:                                @ %normal22.i.i.i265
	add	r0, sp, #98, 30	@ 392
	add	r1, sp, #22, 28	@ 352
	add	r2, sp, #23, 28	@ 368
	bl	"_clay_add(StringConstant, Vector[Char])"
	mov	r5, r0
	ldr	r0, [sp, #360]
	bl	_free
	cmp	r5, #0
	bne	LBB0_10
@ BB#88:                                @ %clay_SelectorTypeEncoding(Static[ExampleViewController], Static[#shouldAutorotateToInterfaceOrientationc58]).exit.i
	add	r0, sp, #25, 28	@ 400
	add	r1, sp, #23, 28	@ 368
	add	r2, sp, #5, 26	@ 320
	bl	"_clay_add(StringConstant, Vector[Char])"
	mov	r5, r0
	ldr	r0, [sp, #376]
	bl	_free
	cmp	r5, #0
	bne	LBB0_10
@ BB#89:                                @ %normal45.i
	ldr	r0, [sp, #320]
	add	r1, r0, #1
	ldr	r2, [sp, #324]
	cmp	r2, r1
	bhs	LBB0_92
@ BB#90:                                @ %clay_reserve(Vector[Char], UInt32).exit.i.i270
	str	r1, [sp, #340]
	add	r0, sp, #5, 26	@ 320
	add	r1, sp, #85, 30	@ 340
	bl	"_clay_vectorSetCapacity(Vector[Char], UInt32)"
	cmp	r0, #0
	bne	LBB0_211
@ BB#91:                                @ %clay_reserve(Vector[Char], UInt32).exit.return12_crit_edge.i.i272
	ldr	r0, [sp, #320]
LBB0_92:                                @ %normal47.i
	ldr	r5, [sp, #328]
	mov	r11, #0
	strb	r11, [r5, r0]
	ldr	r1, LCPI0_80
	
LPC0_80:
	add	r0, pc, r1
	mov	r3, r5
	ldr	r1, [r0, #48]
	ldr	r0, LCPI0_81
	
LPC0_81:
	add	r2, pc, r0
	mov	r0, r4
	bl	_class_addMethod
	mov	r6, r0
	mov	r0, r5
	bl	_free
	tst	r6, #255
	bne	LBB0_101
@ BB#93:                                @ %ifTrue51.i
	ldr	r4, LCPI0_82
	
LPC0_82:
	add	r4, pc, r4
	ldr	r4, [r4, #128]
	str	r4, [sp, #332]
	cmp	r4, #0
	beq	LBB0_96
@ BB#94:                                @ %ifTrue51.i
	cmp	r4, #1
	bne	LBB0_97
@ BB#95:                                @ %ifTrue13.i4.i.i276
	ldr	r4, LCPI0_83
	
LPC0_83:
	add	r4, pc, r4
	ldr	r0, [r4, #140]
	bl	_free
LBB0_96:                                @ %clay_destroy(Exception).exit.i.i284
	ldr	r4, LCPI0_84
	mov	r11, #6
	ldr	r0, LCPI0_85
	
LPC0_84:
	add	r4, pc, r4
	str	r11, [r4, #128]
	
LPC0_85:
	add	r11, pc, r0
	str	r11, [r4, #132]
	add	r11, r11, #8
	str	r11, [r4, #136]
	ldr	r11, LCPI0_86
	
LPC0_86:
	add	r11, pc, r11
	str	r11, [r4, #140]
	add	r11, r11, #21
	str	r11, [r4, #144]
	ldr	r11, LCPI0_87
	
LPC0_87:
	add	r11, pc, r11
	str	r11, [r4, #148]
	add	r11, r11, #39
	str	r11, [r4, #152]
	b	LBB0_10
LBB0_97:                                @ %ifMerge20.i10.i.i282
	sub	r11, r4, #2
	cmp	r11, #2
	blo	LBB0_96
@ BB#98:                                @ %ifMerge20.i10.i.i282
	cmp	r4, #4
	cmpne	r4, #5
	beq	LBB0_96
@ BB#99:                                @ %ifMerge20.i10.i.i282
	cmp	r4, #6
	cmpne	r4, #7
	beq	LBB0_96
@ BB#100:                               @ %clay_destroy(Exception).exit25.i.i283
	add	r0, sp, #83, 30	@ 332
	b	LBB0_9
LBB0_101:                               @ %normal79
	mov	r0, r4
	bl	_objc_registerClassPair
	ldr	r0, LCPI0_88
	
LPC0_89:
	add	r4, pc, r0
	ldr	r0, LCPI0_89
	
LPC0_88:
	add	r0, pc, r0
	bl	_sel_registerName
	str	r0, [r4, #52]
	ldr	r0, LCPI0_90
	
LPC0_90:
	add	r0, pc, r0
	bl	_sel_registerName
	str	r0, [r4, #56]
	str	r11, [r4, #60]
	ldr	r0, LCPI0_91
	
LPC0_91:
	add	r0, pc, r0
	bl	_sel_registerName
	str	r0, [r4, #64]
	ldr	r0, LCPI0_92
	
LPC0_92:
	add	r0, pc, r0
	bl	_sel_registerName
	str	r0, [r4, #68]
	str	r11, [r4, #72]
	str	r11, [r4, #76]
	ldr	r0, LCPI0_93
	
LPC0_93:
	ldr	r0, [pc, r0]
	str	r0, [r4, #112]
	mov	r0, #242, 30	@ 968
	orr	r0, r0, #1, 22	@ 1024
	str	r0, [r4, #116]
	ldr	r0, LCPI0_94
	
LPC0_94:
	add	r0, pc, r0
	str	r0, [r4, #120]
	mov	r0, #2
	str	r0, [r4, #124]
	ldr	r0, LCPI0_95
	
LPC0_95:
	add	r0, pc, r0
	bl	_objc_getClass
	cmp	r0, #0
	bne	LBB0_110
@ BB#102:                               @ %ifTrue.i286
	ldr	r0, LCPI0_96
	
LPC0_96:
	add	r0, pc, r0
	ldr	r0, [r0, #128]
	str	r0, [sp, #408]
	cmp	r0, #0
	beq	LBB0_105
@ BB#103:                               @ %ifTrue.i286
	cmp	r0, #1
	bne	LBB0_106
@ BB#104:                               @ %ifTrue13.i4.i.i288
	ldr	r0, LCPI0_97
	
LPC0_97:
	add	r0, pc, r0
	ldr	r0, [r0, #140]
	bl	_free
LBB0_105:                               @ %clay_destroy(Exception).exit.i.i296
	ldr	r0, LCPI0_98
	mov	r1, #2
	ldr	r2, LCPI0_99
	
LPC0_98:
	add	r0, pc, r0
	str	r1, [r0, #128]
	
LPC0_99:
	add	r1, pc, r2
	str	r1, [r0, #132]
	add	r1, r1, #8
	str	r1, [r0, #136]
	b	LBB0_10
LBB0_106:                               @ %ifMerge20.i10.i.i294
	sub	r1, r0, #2
	cmp	r1, #2
	blo	LBB0_105
@ BB#107:                               @ %ifMerge20.i10.i.i294
	cmp	r0, #4
	cmpne	r0, #5
	beq	LBB0_105
@ BB#108:                               @ %ifMerge20.i10.i.i294
	cmp	r0, #6
	cmpne	r0, #7
	beq	LBB0_105
@ BB#109:                               @ %clay_destroy(Exception).exit25.i.i295
	add	r0, sp, #102, 30	@ 408
	b	LBB0_9
LBB0_110:                               @ %normal134
	ldr	r1, LCPI0_100
	
LPC0_100:
	add	r4, pc, r1
	str	r0, [r4, #80]
	ldr	r1, LCPI0_101
	
LPC0_101:
	add	r0, pc, r1
	bl	_sel_registerName
	str	r0, [r4, #84]
	ldr	r0, LCPI0_102
	
LPC0_102:
	add	r0, pc, r0
	bl	_sel_registerName
	str	r0, [r4, #88]
	ldr	r1, LCPI0_103
	ldr	r0, [r4, #8]
	mov	r2, #0
	
LPC0_103:
	add	r1, pc, r1
	bl	_objc_allocateClassPair
	cmp	r0, #0
	bne	LBB0_119
@ BB#111:                               @ %ifTrue.i208
	ldr	r0, LCPI0_104
	
LPC0_104:
	add	r0, pc, r0
	ldr	r0, [r0, #128]
	str	r0, [sp, #40]
	cmp	r0, #0
	beq	LBB0_114
@ BB#112:                               @ %ifTrue.i208
	cmp	r0, #1
	bne	LBB0_115
@ BB#113:                               @ %ifTrue13.i4.i8.i
	ldr	r0, LCPI0_105
	
LPC0_105:
	add	r0, pc, r0
	ldr	r0, [r0, #140]
	bl	_free
LBB0_114:                               @ %clay_destroy(Exception).exit.i16.i
	ldr	r0, LCPI0_106
	mov	r1, #3
	ldr	r2, LCPI0_107
	
LPC0_106:
	add	r0, pc, r0
	str	r1, [r0, #128]
	
LPC0_107:
	add	r1, pc, r2
	str	r1, [r0, #132]
	add	r1, r1, #8
	str	r1, [r0, #136]
	ldr	r1, LCPI0_108
	
LPC0_108:
	add	r1, pc, r1
	str	r1, [r0, #140]
	add	r1, r1, #17
	str	r1, [r0, #144]
	b	LBB0_10
LBB0_115:                               @ %ifMerge20.i10.i14.i
	sub	r1, r0, #2
	cmp	r1, #2
	blo	LBB0_114
@ BB#116:                               @ %ifMerge20.i10.i14.i
	cmp	r0, #4
	cmpne	r0, #5
	beq	LBB0_114
@ BB#117:                               @ %ifMerge20.i10.i14.i
	cmp	r0, #6
	cmpne	r0, #7
	beq	LBB0_114
@ BB#118:                               @ %clay_destroy(Exception).exit25.i15.i
	add	r0, sp, #40
	b	LBB0_9
LBB0_119:                               @ %ifMerge.i209
	mov	r4, r0
	bl	_object_getClass
	ldr	r0, LCPI0_109
	mov	r2, #4
	mov	r3, #2
	
LPC0_110:
	add	r0, pc, r0
	str	r0, [sp]
	ldr	r0, LCPI0_110
	
LPC0_109:
	add	r1, pc, r0
	mov	r0, r4
	bl	_class_addIvar
	tst	r0, #255
	beq	LBB0_126
@ BB#120:                               @ %whileBody.i45.preheader.i
	ldr	r0, LCPI0_111
	mov	r2, #4
	mov	r3, #2
	
LPC0_112:
	add	r0, pc, r0
	str	r0, [sp]
	ldr	r0, LCPI0_112
	
LPC0_111:
	add	r1, pc, r0
	mov	r0, r4
	bl	_class_addIvar
	tst	r0, #255
	beq	LBB0_134
@ BB#121:                               @ %whileBody.i49.preheader.i
	ldr	r0, LCPI0_119
	mov	r2, #4
	mov	r3, #2
	
LPC0_120:
	add	r0, pc, r0
	str	r0, [sp]
	ldr	r0, LCPI0_120
	
LPC0_119:
	add	r1, pc, r0
	mov	r0, r4
	bl	_class_addIvar
	tst	r0, #255
	bne	LBB0_158
@ BB#122:                               @ %ifTrue144.i
	ldr	r4, LCPI0_127
	
LPC0_127:
	add	r4, pc, r4
	ldr	r4, [r4, #128]
	str	r4, [sp, #120]
	cmp	r4, #0
	beq	LBB0_125
@ BB#123:                               @ %ifTrue144.i
	cmp	r4, #1
	bne	LBB0_142
@ BB#124:                               @ %ifTrue13.i4.i34.i
	ldr	r4, LCPI0_128
	
LPC0_128:
	add	r4, pc, r4
	ldr	r0, [r4, #140]
	bl	_free
LBB0_125:                               @ %clay_destroy(Exception).exit.i42.i
	ldr	r4, LCPI0_129
	mov	r0, #4
	ldr	r1, LCPI0_130
	
LPC0_129:
	add	r4, pc, r4
	str	r0, [r4, #128]
	
LPC0_130:
	add	r0, pc, r1
	str	r0, [r4, #132]
	add	r0, r0, #8
	str	r0, [r4, #136]
	ldr	r0, LCPI0_131
	
LPC0_131:
	add	r0, pc, r0
	str	r0, [r4, #140]
	add	r0, r0, #17
	str	r0, [r4, #144]
	ldr	r0, LCPI0_132
	
LPC0_132:
	add	r0, pc, r0
	str	r0, [r4, #148]
	add	r0, r0, #11
	str	r0, [r4, #152]
	b	LBB0_10
LBB0_126:                               @ %ifTrue57.i
	ldr	r4, LCPI0_113
	
LPC0_113:
	add	r4, pc, r4
	ldr	r4, [r4, #128]
	str	r4, [sp, #116]
	cmp	r4, #0
	beq	LBB0_129
@ BB#127:                               @ %ifTrue57.i
	cmp	r4, #1
	bne	LBB0_130
@ BB#128:                               @ %ifTrue13.i4.i21.i
	ldr	r4, LCPI0_114
	
LPC0_114:
	add	r4, pc, r4
	ldr	r0, [r4, #140]
	bl	_free
LBB0_129:                               @ %clay_destroy(Exception).exit.i29.i
	ldr	r4, LCPI0_115
	mov	r0, #4
	ldr	r1, LCPI0_116
	
LPC0_115:
	add	r4, pc, r4
	str	r0, [r4, #128]
	
LPC0_116:
	add	r0, pc, r1
	str	r0, [r4, #132]
	add	r0, r0, #8
	str	r0, [r4, #136]
	ldr	r0, LCPI0_117
	
LPC0_117:
	add	r0, pc, r0
	str	r0, [r4, #140]
	add	r0, r0, #17
	str	r0, [r4, #144]
	ldr	r0, LCPI0_118
	
LPC0_118:
	add	r0, pc, r0
	str	r0, [r4, #148]
	add	r0, r0, #9
	str	r0, [r4, #152]
	b	LBB0_10
LBB0_130:                               @ %ifMerge20.i10.i27.i
	sub	r0, r4, #2
	cmp	r0, #2
	blo	LBB0_129
@ BB#131:                               @ %ifMerge20.i10.i27.i
	cmp	r4, #4
	cmpne	r4, #5
	beq	LBB0_129
@ BB#132:                               @ %ifMerge20.i10.i27.i
	cmp	r4, #6
	cmpne	r4, #7
	beq	LBB0_129
@ BB#133:                               @ %clay_destroy(Exception).exit25.i28.i
	add	r0, sp, #116
	b	LBB0_9
LBB0_134:                               @ %ifTrue101.i
	ldr	r4, LCPI0_121
	
LPC0_121:
	add	r4, pc, r4
	ldr	r4, [r4, #128]
	str	r4, [sp, #124]
	cmp	r4, #0
	beq	LBB0_137
@ BB#135:                               @ %ifTrue101.i
	cmp	r4, #1
	bne	LBB0_138
@ BB#136:                               @ %ifTrue13.i4.i56.i
	ldr	r4, LCPI0_122
	
LPC0_122:
	add	r4, pc, r4
	ldr	r0, [r4, #140]
	bl	_free
LBB0_137:                               @ %clay_destroy(Exception).exit.i64.i
	ldr	r4, LCPI0_123
	mov	r0, #4
	ldr	r1, LCPI0_124
	
LPC0_123:
	add	r4, pc, r4
	str	r0, [r4, #128]
	
LPC0_124:
	add	r0, pc, r1
	str	r0, [r4, #132]
	add	r0, r0, #8
	str	r0, [r4, #136]
	ldr	r0, LCPI0_125
	
LPC0_125:
	add	r0, pc, r0
	str	r0, [r4, #140]
	add	r0, r0, #17
	str	r0, [r4, #144]
	ldr	r0, LCPI0_126
	
LPC0_126:
	add	r0, pc, r0
	str	r0, [r4, #148]
	add	r0, r0, #10
	str	r0, [r4, #152]
	b	LBB0_10
LBB0_138:                               @ %ifMerge20.i10.i62.i
	sub	r0, r4, #2
	cmp	r0, #2
	blo	LBB0_137
@ BB#139:                               @ %ifMerge20.i10.i62.i
	cmp	r4, #4
	cmpne	r4, #5
	beq	LBB0_137
@ BB#140:                               @ %ifMerge20.i10.i62.i
	cmp	r4, #6
	cmpne	r4, #7
	beq	LBB0_137
@ BB#141:                               @ %clay_destroy(Exception).exit25.i63.i
	add	r0, sp, #124
	b	LBB0_9
LBB0_142:                               @ %ifMerge20.i10.i40.i
	sub	r0, r4, #2
	cmp	r0, #2
	blo	LBB0_125
@ BB#143:                               @ %ifMerge20.i10.i40.i
	cmp	r4, #4
	cmpne	r4, #5
	beq	LBB0_125
@ BB#144:                               @ %ifMerge20.i10.i40.i
	cmp	r4, #6
	cmpne	r4, #7
	beq	LBB0_125
@ BB#145:                               @ %clay_destroy(Exception).exit25.i41.i
	add	r0, sp, #120
	b	LBB0_9
@ BB#146:
	.align	2
LCPI0_157:
	.long	_merged-(LPC0_1+8)
	 
@ BB#147:
	.align	2
LCPI0_158:
	.long	L___stderrp$non_lazy_ptr-(LPC0_0+8)
	 
@ BB#148:
	.align	2
LCPI0_159:
	.long	L_clayliteral_str4-(LPC0_2+8)
	 
@ BB#149:
	.align	2
LCPI0_160:
	.long	_merged-(LPC0_3+8)
	 
@ BB#150:
	.align	2
LCPI0_161:
	.long	_merged-(LPC0_4+8)
	 
@ BB#151:
	.align	2
LCPI0_162:
	.long	_merged-(LPC0_5+8)
	 
@ BB#152:
	.align	2
LCPI0_163:
	.long	L_clayliteral_str4-(LPC0_6+8)
	 
@ BB#153:
	.align	2
LCPI0_164:
	.long	_merged-(LPC0_7+8)
	 
@ BB#154:
	.align	2
LCPI0_165:
	.long	L_clayliteral_str-(LPC0_8+8)
	 
@ BB#155:
	.align	2
LCPI0_166:
	.long	_merged-(LPC0_9+8)
	 
@ BB#156:
	.align	2
LCPI0_167:
	.long	L_clayliteral_str19-(LPC0_10+8)
	 
@ BB#157:
	.align	2
LCPI0_168:
	.long	L_clayliteral_str22-(LPC0_11+8)
	 
LBB0_158:                               @ %ifMerge159.i
	ldr	r0, LCPI0_133
	ldr	r2, LCPI0_135
	
LPC0_133:
	add	r0, pc, r0
	str	r0, [sp, #104]
	
LPC0_135:
	add	r2, pc, r2
	str	r2, [sp, #88]
	add	r0, r0, #1
	str	r0, [sp, #108]
	add	r2, r2, #1
	str	r2, [sp, #92]
	ldr	r0, LCPI0_134
	add	r2, sp, #56
	
LPC0_134:
	add	r0, pc, r0
	add	r1, r0, #1
	str	r0, [sp, #96]
	str	r0, [sp, #48]
	str	r1, [sp, #100]
	str	r1, [sp, #52]
	add	r0, sp, #88
	add	r1, sp, #48
	bl	"_clay_add(StringConstant, StringConstant)"
	cmp	r0, #0
	bne	LBB0_10
@ BB#159:                               @ %normal22.i.i.i
	add	r0, sp, #96
	add	r1, sp, #56
	add	r2, sp, #72
	bl	"_clay_add(StringConstant, Vector[Char])"
	mov	r5, r0
	ldr	r0, [sp, #64]
	bl	_free
	cmp	r5, #0
	bne	LBB0_10
@ BB#160:                               @ %clay_SelectorTypeEncoding(Static[ExampleController], Static[#performMathc58]).exit.i
	add	r0, sp, #104
	add	r1, sp, #72
	add	r2, sp, #24
	bl	"_clay_add(StringConstant, Vector[Char])"
	mov	r5, r0
	ldr	r0, [sp, #80]
	bl	_free
	cmp	r5, #0
	bne	LBB0_10
@ BB#161:                               @ %normal174.i
	ldr	r0, [sp, #24]
	add	r1, r0, #1
	ldr	r2, [sp, #28]
	cmp	r2, r1
	bhs	LBB0_164
@ BB#162:                               @ %clay_reserve(Vector[Char], UInt32).exit.i.i
	str	r1, [sp, #44]
	add	r0, sp, #24
	add	r1, sp, #44
	bl	"_clay_vectorSetCapacity(Vector[Char], UInt32)"
	cmp	r0, #0
	bne	LBB0_212
@ BB#163:                               @ %clay_reserve(Vector[Char], UInt32).exit.return12_crit_edge.i.i
	ldr	r0, [sp, #24]
LBB0_164:                               @ %normal176.i
	mov	r1, #0
	ldr	r5, [sp, #32]
	strb	r1, [r5, r0]
	mov	r3, r5
	ldr	r1, LCPI0_136
	
LPC0_136:
	add	r0, pc, r1
	ldr	r1, [r0, #52]
	ldr	r0, LCPI0_137
	
LPC0_137:
	add	r2, pc, r0
	mov	r0, r4
	bl	_class_addMethod
	mov	r6, r0
	mov	r0, r5
	bl	_free
	tst	r6, #255
	bne	LBB0_185
@ BB#165:                               @ %ifTrue180.i
	ldr	r4, LCPI0_138
	
LPC0_138:
	add	r4, pc, r4
	ldr	r4, [r4, #128]
	str	r4, [sp, #36]
	cmp	r4, #0
	beq	LBB0_168
@ BB#166:                               @ %ifTrue180.i
	cmp	r4, #1
	bne	LBB0_173
@ BB#167:                               @ %ifTrue13.i4.i.i211
	ldr	r4, LCPI0_139
	
LPC0_139:
	add	r4, pc, r4
	ldr	r0, [r4, #140]
	bl	_free
LBB0_168:                               @ %clay_destroy(Exception).exit.i.i219
	ldr	r4, LCPI0_140
	mov	r0, #6
	ldr	r1, LCPI0_141
	
LPC0_140:
	add	r4, pc, r4
	str	r0, [r4, #128]
	
LPC0_141:
	add	r0, pc, r1
	str	r0, [r4, #132]
	add	r0, r0, #8
	str	r0, [r4, #136]
	ldr	r0, LCPI0_142
	
LPC0_142:
	add	r0, pc, r0
	str	r0, [r4, #140]
	add	r0, r0, #17
	str	r0, [r4, #144]
	ldr	r0, LCPI0_143
	
LPC0_143:
	add	r0, pc, r0
	str	r0, [r4, #148]
	add	r0, r0, #12
	str	r0, [r4, #152]
	b	LBB0_10
@ BB#169:
	.align	2
LCPI0_169:
	.long	L_clayliteral_str28-(LPC0_12+8)
	 
@ BB#170:
	.align	2
LCPI0_170:
	.long	L_clayliteral_str40-(LPC0_13+8)
	 
@ BB#171:
	.align	2
LCPI0_171:
	.long	L_clayliteral_str51-(LPC0_14+8)
	 
@ BB#172:
	.align	2
LCPI0_172:
	.long	L_clayliteral_str60-(LPC0_15+8)
	 
LBB0_173:                               @ %ifMerge20.i10.i.i217
	sub	r0, r4, #2
	cmp	r0, #2
	blo	LBB0_168
@ BB#174:                               @ %ifMerge20.i10.i.i217
	cmp	r4, #4
	cmpne	r4, #5
	beq	LBB0_168
@ BB#175:                               @ %ifMerge20.i10.i.i217
	cmp	r4, #6
	cmpne	r4, #7
	beq	LBB0_168
@ BB#176:                               @ %clay_destroy(Exception).exit25.i.i218
	add	r0, sp, #36
	b	LBB0_9
@ BB#177:
	.align	2
LCPI0_173:
	.long	L_clayliteral_str2-(LPC0_16+8)
	 
@ BB#178:
	.align	2
LCPI0_174:
	.long	_merged-(LPC0_17+8)
	 
@ BB#179:
	.align	2
LCPI0_175:
	.long	_merged-(LPC0_18+8)
	 
@ BB#180:
	.align	2
LCPI0_176:
	.long	_merged-(LPC0_19+8)
	 
@ BB#181:
	.align	2
LCPI0_177:
	.long	L_clayliteral_str9-(LPC0_20+8)
	 
@ BB#182:
	.align	2
LCPI0_178:
	.long	L_clayliteral_str2-(LPC0_21+8)
	 
@ BB#183:
	.align	2
LCPI0_179:
	.long	L_clayliteral_str12-(LPC0_23+8)
	 
@ BB#184:
	.align	2
LCPI0_180:
	.long	L_clayliteral_str11-(LPC0_22+8)
	 
LBB0_185:                               @ %normal160
	mov	r0, r4
	bl	_objc_registerClassPair
	ldr	r0, LCPI0_144
	
LPC0_144:
	add	r1, pc, r0
	mov	r0, r4
	bl	_class_getInstanceVariable
	bl	_ivar_getOffset
	ldr	r1, LCPI0_145
	
LPC0_145:
	add	r5, pc, r1
	str	r0, [r5, #60]
	ldr	r0, LCPI0_146
	
LPC0_146:
	add	r1, pc, r0
	mov	r0, r4
	bl	_class_getInstanceVariable
	bl	_ivar_getOffset
	str	r0, [r5, #72]
	ldr	r0, LCPI0_147
	
LPC0_147:
	add	r1, pc, r0
	mov	r0, r4
	bl	_class_getInstanceVariable
	bl	_ivar_getOffset
	str	r0, [r5, #76]
	ldr	r0, LCPI0_148
	
LPC0_148:
	add	r0, pc, r0
	bl	_objc_getClass
	cmp	r0, #0
	bne	LBB0_208
@ BB#186:                               @ %ifTrue.i195
	ldr	r0, LCPI0_149
	
LPC0_149:
	add	r0, pc, r0
	ldr	r0, [r0, #128]
	str	r0, [sp, #20]
	cmp	r0, #0
	beq	LBB0_189
@ BB#187:                               @ %ifTrue.i195
	cmp	r0, #1
	bne	LBB0_192
@ BB#188:                               @ %ifTrue13.i4.i.i197
	ldr	r0, LCPI0_150
	
LPC0_150:
	add	r0, pc, r0
	ldr	r0, [r0, #140]
	bl	_free
LBB0_189:                               @ %clay_destroy(Exception).exit.i.i205
	ldr	r0, LCPI0_151
	mov	r1, #2
	ldr	r2, LCPI0_152
	
LPC0_151:
	add	r0, pc, r0
	str	r1, [r0, #128]
	
LPC0_152:
	add	r1, pc, r2
	str	r1, [r0, #132]
	add	r1, r1, #17
	str	r1, [r0, #136]
	b	LBB0_10
@ BB#190:
	.align	2
LCPI0_181:
	.long	L_clayliteral_str12-(LPC0_25+8)
	 
@ BB#191:
	.align	2
LCPI0_182:
	.long	L_clayliteral_str15-(LPC0_24+8)
	 
LBB0_192:                               @ %ifMerge20.i10.i.i203
	sub	r1, r0, #2
	cmp	r1, #2
	blo	LBB0_189
@ BB#193:                               @ %ifMerge20.i10.i.i203
	cmp	r0, #4
	cmpne	r0, #5
	beq	LBB0_189
@ BB#194:                               @ %ifMerge20.i10.i.i203
	cmp	r0, #6
	cmpne	r0, #7
	beq	LBB0_189
@ BB#195:                               @ %clay_destroy(Exception).exit25.i.i204
	add	r0, sp, #20
	b	LBB0_9
@ BB#196:
	.align	2
LCPI0_183:
	.long	_merged-(LPC0_32+8)
	 
@ BB#197:
	.align	2
LCPI0_184:
	.long	_merged-(LPC0_33+8)
	 
@ BB#198:
	.align	2
LCPI0_185:
	.long	_merged-(LPC0_34+8)
	 
@ BB#199:
	.align	2
LCPI0_186:
	.long	L_clayliteral_str9-(LPC0_35+8)
	 
@ BB#200:
	.align	2
LCPI0_187:
	.long	L_clayliteral_str2-(LPC0_36+8)
	 
@ BB#201:
	.align	2
LCPI0_188:
	.long	L_clayliteral_str15-(LPC0_37+8)
	 
@ BB#202:
	.align	2
LCPI0_189:
	.long	_merged-(LPC0_26+8)
	 
@ BB#203:
	.align	2
LCPI0_190:
	.long	_merged-(LPC0_27+8)
	 
@ BB#204:
	.align	2
LCPI0_191:
	.long	_merged-(LPC0_28+8)
	 
@ BB#205:
	.align	2
LCPI0_192:
	.long	L_clayliteral_str9-(LPC0_29+8)
	 
@ BB#206:
	.align	2
LCPI0_193:
	.long	L_clayliteral_str2-(LPC0_30+8)
	 
@ BB#207:
	.align	2
LCPI0_194:
	.long	L_clayliteral_str11-(LPC0_31+8)
	 
LBB0_208:                               @ %normal162
	ldr	r1, LCPI0_153
	
LPC0_153:
	add	r4, pc, r1
	str	r0, [r4, #92]
	mov	r6, #2
	ldr	r1, LCPI0_154
	
LPC0_154:
	add	r0, pc, r1
	bl	_sel_registerName
	str	r0, [r4, #96]
	ldr	r0, LCPI0_155
	
LPC0_155:
	add	r0, pc, r0
	bl	_sel_registerName
	str	r0, [r4, #100]
	ldr	r0, LCPI0_156
	
LPC0_156:
	add	r0, pc, r0
	bl	_sel_registerName
	str	r0, [r4, #104]
	mov	r0, #28
	orr	r0, r0, #2, 20	@ 8192
	bl	_malloc
	mov	r5, r0
	mov	r0, #1
	str	r0, [r5]
	mov	r0, #2
	bl	_isatty
	str	r6, [r5, #4]
	mvn	r2, #0
	str	r2, [sp, #8]
	mov	r1, #0
	cmp	r0, #0
	strb	r1, [r5, #8]
	mov	r0, #20
	orr	r0, r0, #1, 20	@ 4096
	strb	r1, [sp, #12]
	mov	r2, #12
	orr	r2, r2, #1, 20	@ 4096
	str	r1, [r5, r2]
	mov	r2, #16
	orr	r2, r2, #1, 20	@ 4096
	str	r1, [r5, r2]
	mov	r2, #24
	orr	r2, r2, #2, 20	@ 8192
	str	r1, [r5, r2]
	movne	r1, #1
	strb	r1, [r5, r0]
	add	r0, sp, #8
	bl	"_clay_destroy(RawFile)"
	str	r5, [r4, #108]
	sub	sp, r7, #20
	ldmia	sp!, {r8, r11}
	ldmia	sp!, {r4, r5, r6, r7, pc}
LBB0_209:                               @ %landing131.i
	ldr	r0, [sp, #152]
	bl	_free
	b	LBB0_10
LBB0_210:                               @ %landing166.i
	ldr	r0, [sp, #136]
	bl	_free
	b	LBB0_10
LBB0_211:                               @ %landing46.i
	ldr	r0, [sp, #328]
	bl	_free
	b	LBB0_10
LBB0_212:                               @ %landing175.i
	ldr	r0, [sp, #32]
	bl	_free
	b	LBB0_10
@ BB#213:
	.align	2
LCPI0_38:
	.long	L_clayliteral_str41-(LPC0_38+8)
	 
	.align	2
LCPI0_39:
	.long	L_clayliteral_str12-(LPC0_39+8)
	 
	.align	2
LCPI0_40:
	.long	L_clayliteral_str43-(LPC0_40+8)
	 
	.align	2
LCPI0_41:
	.long	_merged-(LPC0_41+8)
	 
	.align	2
LCPI0_42:
	.long	"_clay_cwrapper_LambdaProcedure(Pointer[ObjcObject], Pointer[ObjcMethod], Pointer[ObjcObject], Pointer[ObjcObject])"-(LPC0_42+8)
	 
	.align	2
LCPI0_43:
	.long	_merged-(LPC0_43+8)
	 
	.align	2
LCPI0_44:
	.long	_merged-(LPC0_44+8)
	 
	.align	2
LCPI0_45:
	.long	_merged-(LPC0_45+8)
	 
	.align	2
LCPI0_46:
	.long	L_clayliteral_str9-(LPC0_46+8)
	 
	.align	2
LCPI0_47:
	.long	L_clayliteral_str2-(LPC0_47+8)
	 
	.align	2
LCPI0_48:
	.long	L_clayliteral_str19-(LPC0_48+8)
	 
	.align	2
LCPI0_49:
	.long	L_clayliteral_str68-(LPC0_49+8)
	 
	.align	2
LCPI0_50:
	.long	L_clayliteral_str12-(LPC0_50+8)
	 
	.align	2
LCPI0_51:
	.long	L_clayliteral_str43-(LPC0_51+8)
	 
	.align	2
LCPI0_52:
	.long	_merged-(LPC0_52+8)
	 
	.align	2
LCPI0_53:
	.long	"_clay_cwrapper_LambdaProcedure(Pointer[ObjcObject], Pointer[ObjcMethod])"-(LPC0_53+8)
	 
	.align	2
LCPI0_54:
	.long	_merged-(LPC0_54+8)
	 
	.align	2
LCPI0_55:
	.long	_merged-(LPC0_55+8)
	 
	.align	2
LCPI0_56:
	.long	_merged-(LPC0_56+8)
	 
	.align	2
LCPI0_57:
	.long	L_clayliteral_str9-(LPC0_57+8)
	 
	.align	2
LCPI0_58:
	.long	L_clayliteral_str2-(LPC0_58+8)
	 
	.align	2
LCPI0_59:
	.long	L_clayliteral_str51-(LPC0_59+8)
	 
	.align	2
LCPI0_60:
	.long	L_clayliteral_str11-(LPC0_60+8)
	 
	.align	2
LCPI0_61:
	.long	_merged-(LPC0_61+8)
	 
	.align	2
LCPI0_62:
	.long	L_clayliteral_str15-(LPC0_62+8)
	 
	.align	2
LCPI0_63:
	.long	L_clayliteral_str78-(LPC0_63+8)
	 
	.align	2
LCPI0_64:
	.long	_merged-(LPC0_64+8)
	 
	.align	2
LCPI0_65:
	.long	_merged-(LPC0_65+8)
	 
	.align	2
LCPI0_66:
	.long	_merged-(LPC0_66+8)
	 
	.align	2
LCPI0_67:
	.long	L_clayliteral_str78-(LPC0_67+8)
	 
	.align	2
LCPI0_68:
	.long	_merged-(LPC0_68+8)
	 
	.align	2
LCPI0_69:
	.long	L_clayliteral_str82-(LPC0_69+8)
	 
	.align	2
LCPI0_70:
	.long	L_clayliteral_str76-(LPC0_70+8)
	 
	.align	2
LCPI0_71:
	.long	_merged-(LPC0_71+8)
	 
	.align	2
LCPI0_72:
	.long	_merged-(LPC0_72+8)
	 
	.align	2
LCPI0_73:
	.long	_merged-(LPC0_73+8)
	 
	.align	2
LCPI0_74:
	.long	L_clayliteral_str9-(LPC0_74+8)
	 
	.align	2
LCPI0_75:
	.long	L_clayliteral_str76-(LPC0_75+8)
	 
	.align	2
LCPI0_76:
	.long	L_clayliteral_str41-(LPC0_76+8)
	 
	.align	2
LCPI0_77:
	.long	L_clayliteral_str12-(LPC0_77+8)
	 
	.align	2
LCPI0_78:
	.long	L_clayliteral_str43-(LPC0_78+8)
	 
	.align	2
LCPI0_79:
	.long	L_clayliteral_str83-(LPC0_79+8)
	 
	.align	2
LCPI0_80:
	.long	_merged-(LPC0_80+8)
	 
	.align	2
LCPI0_81:
	.long	"_clay_cwrapper_LambdaProcedure(Pointer[ObjcObject], Pointer[ObjcMethod], Int32)"-(LPC0_81+8)
	 
	.align	2
LCPI0_82:
	.long	_merged-(LPC0_82+8)
	 
	.align	2
LCPI0_83:
	.long	_merged-(LPC0_83+8)
	 
	.align	2
LCPI0_84:
	.long	_merged-(LPC0_84+8)
	 
	.align	2
LCPI0_85:
	.long	L_clayliteral_str9-(LPC0_85+8)
	 
	.align	2
LCPI0_86:
	.long	L_clayliteral_str76-(LPC0_86+8)
	 
	.align	2
LCPI0_87:
	.long	L_clayliteral_str82-(LPC0_87+8)
	 
	.align	2
LCPI0_88:
	.long	_merged-(LPC0_89+8)
	 
	.align	2
LCPI0_89:
	.long	L_clayliteral_str103-(LPC0_88+8)
	 
	.align	2
LCPI0_90:
	.long	L_clayliteral_str122-(LPC0_90+8)
	 
	.align	2
LCPI0_91:
	.long	L_clayliteral_str130-(LPC0_91+8)
	 
	.align	2
LCPI0_92:
	.long	L_clayliteral_str141-(LPC0_92+8)
	 
	.align	2
LCPI0_93:
	.long	L___CFConstantStringClassReference$non_lazy_ptr-(LPC0_93+8)
	 
	.align	2
LCPI0_94:
	.long	L_clayliteral_str8-(LPC0_94+8)
	 
	.align	2
LCPI0_95:
	.long	L_clayliteral_str163-(LPC0_95+8)
	 
	.align	2
LCPI0_96:
	.long	_merged-(LPC0_96+8)
	 
	.align	2
LCPI0_97:
	.long	_merged-(LPC0_97+8)
	 
	.align	2
LCPI0_98:
	.long	_merged-(LPC0_98+8)
	 
	.align	2
LCPI0_99:
	.long	L_clayliteral_str163-(LPC0_99+8)
	 
	.align	2
LCPI0_100:
	.long	_merged-(LPC0_100+8)
	 
	.align	2
LCPI0_101:
	.long	L_clayliteral_str165-(LPC0_101+8)
	 
	.align	2
LCPI0_102:
	.long	L_clayliteral_str175-(LPC0_102+8)
	 
	.align	2
LCPI0_103:
	.long	L_clayliteral_str88-(LPC0_103+8)
	 
	.align	2
LCPI0_104:
	.long	_merged-(LPC0_104+8)
	 
	.align	2
LCPI0_105:
	.long	_merged-(LPC0_105+8)
	 
	.align	2
LCPI0_106:
	.long	_merged-(LPC0_106+8)
	 
	.align	2
LCPI0_107:
	.long	L_clayliteral_str9-(LPC0_107+8)
	 
	.align	2
LCPI0_108:
	.long	L_clayliteral_str88-(LPC0_108+8)
	 
	.align	2
LCPI0_109:
	.long	L_clayliteral_str12-(LPC0_110+8)
	 
	.align	2
LCPI0_110:
	.long	L_clayliteral_str91-(LPC0_109+8)
	 
	.align	2
LCPI0_111:
	.long	L_clayliteral_str12-(LPC0_112+8)
	 
	.align	2
LCPI0_112:
	.long	L_clayliteral_str95-(LPC0_111+8)
	 
	.align	2
LCPI0_113:
	.long	_merged-(LPC0_113+8)
	 
	.align	2
LCPI0_114:
	.long	_merged-(LPC0_114+8)
	 
	.align	2
LCPI0_115:
	.long	_merged-(LPC0_115+8)
	 
	.align	2
LCPI0_116:
	.long	L_clayliteral_str9-(LPC0_116+8)
	 
	.align	2
LCPI0_117:
	.long	L_clayliteral_str88-(LPC0_117+8)
	 
	.align	2
LCPI0_118:
	.long	L_clayliteral_str91-(LPC0_118+8)
	 
	.align	2
LCPI0_119:
	.long	L_clayliteral_str12-(LPC0_120+8)
	 
	.align	2
LCPI0_120:
	.long	L_clayliteral_str98-(LPC0_119+8)
	 
	.align	2
LCPI0_121:
	.long	_merged-(LPC0_121+8)
	 
	.align	2
LCPI0_122:
	.long	_merged-(LPC0_122+8)
	 
	.align	2
LCPI0_123:
	.long	_merged-(LPC0_123+8)
	 
	.align	2
LCPI0_124:
	.long	L_clayliteral_str9-(LPC0_124+8)
	 
	.align	2
LCPI0_125:
	.long	L_clayliteral_str88-(LPC0_125+8)
	 
	.align	2
LCPI0_126:
	.long	L_clayliteral_str95-(LPC0_126+8)
	 
	.align	2
LCPI0_127:
	.long	_merged-(LPC0_127+8)
	 
	.align	2
LCPI0_128:
	.long	_merged-(LPC0_128+8)
	 
	.align	2
LCPI0_129:
	.long	_merged-(LPC0_129+8)
	 
	.align	2
LCPI0_130:
	.long	L_clayliteral_str9-(LPC0_130+8)
	 
	.align	2
LCPI0_131:
	.long	L_clayliteral_str88-(LPC0_131+8)
	 
	.align	2
LCPI0_132:
	.long	L_clayliteral_str98-(LPC0_132+8)
	 
	.align	2
LCPI0_133:
	.long	L_clayliteral_str68-(LPC0_133+8)
	 
	.align	2
LCPI0_134:
	.long	L_clayliteral_str12-(LPC0_134+8)
	 
	.align	2
LCPI0_135:
	.long	L_clayliteral_str43-(LPC0_135+8)
	 
	.align	2
LCPI0_136:
	.long	_merged-(LPC0_136+8)
	 
	.align	2
LCPI0_137:
	.long	"_clay_cwrapper_LambdaProcedure(Pointer[ObjcObject], Pointer[ObjcMethod], Pointer[ObjcObject])"-(LPC0_137+8)
	 
	.align	2
LCPI0_138:
	.long	_merged-(LPC0_138+8)
	 
	.align	2
LCPI0_139:
	.long	_merged-(LPC0_139+8)
	 
	.align	2
LCPI0_140:
	.long	_merged-(LPC0_140+8)
	 
	.align	2
LCPI0_141:
	.long	L_clayliteral_str9-(LPC0_141+8)
	 
	.align	2
LCPI0_142:
	.long	L_clayliteral_str88-(LPC0_142+8)
	 
	.align	2
LCPI0_143:
	.long	L_clayliteral_str103-(LPC0_143+8)
	 
	.align	2
LCPI0_144:
	.long	L_clayliteral_str91-(LPC0_144+8)
	 
	.align	2
LCPI0_145:
	.long	_merged-(LPC0_145+8)
	 
	.align	2
LCPI0_146:
	.long	L_clayliteral_str95-(LPC0_146+8)
	 
	.align	2
LCPI0_147:
	.long	L_clayliteral_str98-(LPC0_147+8)
	 
	.align	2
LCPI0_148:
	.long	L_clayliteral_str191-(LPC0_148+8)
	 
	.align	2
LCPI0_149:
	.long	_merged-(LPC0_149+8)
	 
	.align	2
LCPI0_150:
	.long	_merged-(LPC0_150+8)
	 
	.align	2
LCPI0_151:
	.long	_merged-(LPC0_151+8)
	 
	.align	2
LCPI0_152:
	.long	L_clayliteral_str191-(LPC0_152+8)
	 
	.align	2
LCPI0_153:
	.long	_merged-(LPC0_153+8)
	 
	.align	2
LCPI0_154:
	.long	L_clayliteral_str193-(LPC0_154+8)
	 
	.align	2
LCPI0_155:
	.long	L_clayliteral_str200-(LPC0_155+8)
	 
	.align	2
LCPI0_156:
	.long	L_clayliteral_str208-(LPC0_156+8)
	 
Leh_func_end0:

	.align	2
_clayglobals_destroy:                   @ @clayglobals_destroy
Leh_func_begin1:
@ BB#0:                                 @ %init
	stmdb	sp!, {r4, r5, r7, lr}
	add	r7, sp, #8
	sub	sp, sp, #4
	ldr	r0, LCPI1_0
	
LPC1_0:
	add	r0, pc, r0
	ldr	r4, [r0, #108]
	cmp	r4, #0
	beq	LBB1_7
@ BB#1:                                 @ %return10.i
	ldr	r0, [r4]
	subs	r0, r0, #1
	str	r0, [r4]
	bne	LBB1_7
@ BB#2:                                 @ %return26.i
	mov	r5, r4
	mov	r0, #24
	orr	r0, r0, #2, 20	@ 8192
	ldr	r2, [r5, r0]!
	cmp	r2, #0
	beq	LBB1_5
@ BB#3:                                 @ %return10.i.i
	add	r1, r4, #21
	ldr	r0, [r4, #4]
	add	r1, r1, #1, 20	@ 4096
	bl	"_clay_write(RawFile, Pointer[UInt8], UInt32)"
	cmp	r0, #0
	bne	LBB1_7
@ BB#4:                                 @ %return18.i.i
	mov	r0, #0
	str	r0, [r5]
LBB1_5:                                 @ %clay_destroy(FileData).exit.i
	add	r0, r4, #4
	bl	"_clay_destroy(RawFile)"
	cmp	r0, #0
	bne	LBB1_7
@ BB#6:                                 @ %return31.i
	ldr	r0, LCPI1_1
	
LPC1_1:
	add	r0, pc, r0
	ldr	r0, [r0, #108]
	bl	_free
LBB1_7:                                 @ %clay_destroy(File).exit
	ldr	r0, LCPI1_2
	
LPC1_2:
	add	r0, pc, r0
	ldr	r0, [r0, #128]
	str	r0, [sp]
	cmp	r0, #0
	beq	LBB1_13
@ BB#8:                                 @ %clay_destroy(File).exit
	cmp	r0, #1
	beq	LBB1_14
@ BB#9:                                 @ %ifMerge20.i
	sub	r1, r0, #2
	cmp	r1, #2
	blo	LBB1_13
@ BB#10:                                @ %ifMerge20.i
	cmp	r0, #4
	beq	LBB1_13
@ BB#11:                                @ %ifMerge20.i
	cmp	r0, #5
	cmpne	r0, #6
	beq	LBB1_13
@ BB#12:                                @ %ifMerge20.i
	cmp	r0, #7
	movne	r0, sp
	blne	"_clay_error(StringConstant, Int32)"
LBB1_13:                                @ %clay_destroy(Exception).exit
	sub	sp, r7, #8
	ldmia	sp!, {r4, r5, r7, pc}
LBB1_14:                                @ %ifTrue13.i
	ldr	r0, LCPI1_3
	
LPC1_3:
	add	r0, pc, r0
	ldr	r0, [r0, #140]
	bl	_free
	sub	sp, r7, #8
	ldmia	sp!, {r4, r5, r7, pc}
@ BB#15:
	.align	2
LCPI1_0:
	.long	_merged-(LPC1_0+8)
	 
	.align	2
LCPI1_1:
	.long	_merged-(LPC1_1+8)
	 
	.align	2
LCPI1_2:
	.long	_merged-(LPC1_2+8)
	 
	.align	2
LCPI1_3:
	.long	_merged-(LPC1_3+8)
	 
Leh_func_end1:

	.globl	_main
	.align	2
_main:                                  @ @main
Leh_func_begin2:
@ BB#0:                                 @ %init
	stmdb	sp!, {r4, r5, r6, r7, lr}
	ldr	r2, LCPI2_0
	
LPC2_0:
	add	r5, pc, r2
	mov	r4, r1
	str	r0, [r5, #4]
	ldrd	r0, [r5, #92]
	add	r7, sp, #12
	bl	_objc_msgSend
	ldr	r1, [r5, #100]
	bl	_objc_msgSend
	mov	r6, r0
	mov	r2, #0
	ldr	r0, [r5, #4]
	mov	r1, r4
	mov	r3, r2
	bl	_UIApplicationMain
	mov	r4, r0
	ldr	r1, [r5, #104]
	mov	r0, r6
	bl	_objc_msgSend
	mov	r0, r4
	ldmia	sp!, {r4, r5, r6, r7, pc}
@ BB#1:
	.align	2
LCPI2_0:
	.long	_merged-(LPC2_0+8)
	 
Leh_func_end2:

	.align	2
"_clay_error(StringConstant, Int32)":   @ @"clay_error(StringConstant, Int32)"
@ BB#0:                                 @ %clay_reserve(Vector[Char], UInt32).exit.i.i
	stmdb	sp!, {r4, r5, r6, r7, lr}
	add	r7, sp, #12
	stmdb	sp!, {r8, r10, r11}
	sub	sp, sp, #208
	bic	sp, sp, #7
	str	r0, [sp, #4]
	mov	r1, #0
	mov	r0, #23
	str	r1, [sp, #8]
	str	r1, [sp, #12]
	str	r1, [sp, #16]
	str	r0, [sp, #60]
	add	r0, sp, #8
	add	r1, sp, #60
	bl	"_clay_vectorSetCapacity(Vector[Char], UInt32)"
	cmp	r0, #0
	bne	LBB3_11
@ BB#1:                                 @ %clay_reserve(Vector[Char], UInt32).exit.i.i.whileBegin.i.i_crit_edge
	mvn	r4, #22
	add	r5, sp, #8
	add	r6, sp, #76
	add	r8, sp, #72
	add	r10, sp, #68
LBB3_2:                                 @ %whileBegin.i.i
                                        @ =>This Inner Loop Header: Depth=1
	cmp	r4, #0
	beq	LBB3_5
@ BB#3:                                 @ %whileBody.i.i
                                        @   in Loop: Header=BB3_2 Depth=1
	ldr	r0, LCPI3_0
	add	r1, sp, #64
	str	r1, [sp]
	mov	r2, r8
	mov	r3, r10
	
LPC3_0:
	add	r0, pc, r0
	mov	r1, r6
	add	r0, r0, r4
	ldrb	r11, [r0, #23]
	ldr	r0, [sp, #8]
	str	r0, [sp, #76]
	mov	r0, #1
	str	r0, [sp, #72]
	mov	r0, r5
	bl	"_clay_insertLocationsUnsafe(Vector[Char], UInt32, UInt32)"
	cmp	r0, #0
	bne	LBB3_11
@ BB#4:                                 @ %normal16.i.i
                                        @   in Loop: Header=BB3_2 Depth=1
	ldr	r0, [sp, #68]
	add	r4, r4, #1
	strb	r11, [r0]
	b	LBB3_2
LBB3_5:                                 @ %clay_printTo(Vector[Char], StringConstant, Int32).exit
	ldr	r0, [sp, #4]
	ldr	r2, [r0]
	add	r4, sp, #80
	ldr	r0, LCPI3_1
	
LPC3_1:
	add	r1, pc, r0
	mov	r0, r4
	bl	_sprintf
	mov	r0, r4
	bl	_strlen
	mov	r5, r0
	ldr	r0, [sp, #8]
	ldr	r1, [sp, #12]
	add	r0, r0, r5
	cmp	r1, r0
	blo	LBB3_10
LBB3_6:                                 @ %clay_printTo(Vector[Char], StringConstant, Int32).exit.whileBegin.i.i6_crit_edge
	add	r6, sp, #196
	add	r8, sp, #192
	add	r10, sp, #188
LBB3_7:                                 @ %whileBegin.i.i6
                                        @ =>This Inner Loop Header: Depth=1
	cmp	r5, #0
	beq	LBB3_13
@ BB#8:                                 @ %whileBody.i.i8
                                        @   in Loop: Header=BB3_7 Depth=1
	ldr	r0, [sp, #8]
	ldrb	r11, [r4]
	str	r0, [sp, #196]
	add	r1, sp, #184
	str	r1, [sp]
	mov	r2, r8
	mov	r3, r10
	mov	r0, #1
	str	r0, [sp, #192]
	mov	r1, r6
	add	r0, sp, #8
	bl	"_clay_insertLocationsUnsafe(Vector[Char], UInt32, UInt32)"
	cmp	r0, #0
	bne	LBB3_11
@ BB#9:                                 @ %normal16.i.i10
                                        @   in Loop: Header=BB3_7 Depth=1
	ldr	r0, [sp, #188]
	strb	r11, [r0]
	sub	r5, r5, #1
	add	r4, r4, #1
	b	LBB3_7
LBB3_10:                                @ %clay_reserve(Vector[Char], UInt32).exit.i.i4
	str	r0, [sp, #180]
	add	r0, sp, #8
	add	r1, sp, #180
	bl	"_clay_vectorSetCapacity(Vector[Char], UInt32)"
	cmp	r0, #0
	beq	LBB3_6
LBB3_11:                                @ %landing1
	ldr	r0, [sp, #16]
LBB3_12:                                @ %landing1
	bl	_free
	b	LBB3_29
LBB3_13:                                @ %normal2
	mov	r2, #0
	ldr	r4, [sp, #16]
	ldr	r0, [sp, #12]
	ldr	r1, [sp, #8]
	str	r2, [sp, #8]
	str	r2, [sp, #12]
	str	r2, [sp, #16]
	mov	r2, #1
	str	r2, [sp, #24]
	str	r1, [sp, #28]
	str	r0, [sp, #32]
	str	r4, [sp, #36]
	ldr	r2, LCPI3_2
	
LPC3_2:
	add	r0, pc, r2
	ldr	r0, [r0, #128]
	str	r0, [sp, #56]
	cmp	r0, #0
	beq	LBB3_16
@ BB#14:                                @ %normal2
	cmp	r0, #1
	bne	LBB3_20
@ BB#15:                                @ %ifTrue13.i
	ldr	r0, LCPI3_3
	
LPC3_3:
	add	r0, pc, r0
	ldr	r0, [r0, #140]
	bl	_free
LBB3_16:                                @ %normal16.i
	add	r0, sp, #24
	bl	"_clay_moveUnsafe(Exception)"
	cmp	r0, #0
	beq	LBB3_28
@ BB#17:                                @ %landing18.i
	ldr	r0, [sp, #24]
	str	r0, [sp, #204]
	cmp	r0, #0
	beq	LBB3_29
@ BB#18:                                @ %landing18.i
	cmp	r0, #1
	bne	LBB3_24
@ BB#19:                                @ %ifTrue13.i19
	ldr	r0, [sp, #36]
	b	LBB3_12
LBB3_20:                                @ %ifMerge20.i
	sub	r1, r0, #2
	cmp	r1, #2
	blo	LBB3_16
@ BB#21:                                @ %ifMerge20.i
	cmp	r0, #4
	cmpne	r0, #5
	beq	LBB3_16
@ BB#22:                                @ %ifMerge20.i
	cmp	r0, #6
	cmpne	r0, #7
	beq	LBB3_16
@ BB#23:                                @ %ifTrue13.i13
	add	r0, sp, #56
	bl	"_clay_error(StringConstant, Int32)"
	mov	r0, #1
	str	r0, [sp, #200]
	mov	r0, r4
	b	LBB3_12
LBB3_24:                                @ %ifMerge20.i25
	sub	r1, r0, #2
	cmp	r1, #2
	blo	LBB3_29
@ BB#25:                                @ %ifMerge20.i25
	cmp	r0, #4
	cmpne	r0, #5
	beq	LBB3_29
@ BB#26:                                @ %ifMerge20.i25
	cmp	r0, #6
	cmpne	r0, #7
	beq	LBB3_29
@ BB#27:                                @ %ifMerge80.i26
	add	r0, sp, #204
	bl	"_clay_error(StringConstant, Int32)"
	b	LBB3_29
LBB3_28:                                @ %clay_destroy(Exception).exit40
	mov	r0, #0
	str	r0, [sp, #24]
LBB3_29:                                @ %clay_throwValue(Error).exit
	sub	sp, r7, #24
	ldmia	sp!, {r8, r10, r11}
	ldmia	sp!, {r4, r5, r6, r7, pc}
@ BB#30:
	.align	2
LCPI3_0:
	.long	L_clayliteral_str5-(LPC3_0+8)
	 
	.align	2
LCPI3_1:
	.long	L_clayliteral_str8-(LPC3_1+8)
	 
	.align	2
LCPI3_2:
	.long	_merged-(LPC3_2+8)
	 
	.align	2
LCPI3_3:
	.long	_merged-(LPC3_3+8)
	 

	.align	2
"_clay_vectorSetCapacity(Vector[Char], UInt32)": @ @"clay_vectorSetCapacity(Vector[Char], UInt32)"
@ BB#0:                                 @ %init
	stmdb	sp!, {r4, r5, r6, r7, lr}
	add	r7, sp, #12
	stmdb	sp!, {r8, r10, r11}
	sub	sp, sp, #88
	bic	sp, sp, #7
	mov	r5, r0
	mov	r4, r1
	ldr	r6, [r5]
	ldr	r0, [r4]
	cmp	r0, r6
	bhs	LBB4_26
@ BB#1:                                 @ %clay_reserve(Vector[Char], UInt32).exit.i.i
	mov	r4, #0
	add	r0, sp, #8
	add	r1, sp, #60
	str	r4, [sp, #8]
	str	r4, [sp, #12]
	str	r4, [sp, #16]
	mov	r4, #16
	str	r4, [sp, #60]
	bl	"_clay_vectorSetCapacity(Vector[Char], UInt32)"
	cmp	r0, #0
	bne	LBB4_6
@ BB#2:                                 @ %clay_reserve(Vector[Char], UInt32).exit.i.i.whileBegin.i.i_crit_edge
	mvn	r4, #15
	add	r5, sp, #8
	add	r6, sp, #76
	add	r8, sp, #72
	add	r10, sp, #68
LBB4_3:                                 @ %whileBegin.i.i
                                        @ =>This Inner Loop Header: Depth=1
	cmp	r4, #0
	beq	LBB4_8
@ BB#4:                                 @ %whileBody.i.i
                                        @   in Loop: Header=BB4_3 Depth=1
	ldr	r0, LCPI4_0
	add	r1, sp, #64
	str	r1, [sp]
	mov	r2, r8
	mov	r3, r10
	
LPC4_0:
	add	r0, pc, r0
	mov	r1, r6
	add	r0, r0, r4
	ldrb	r11, [r0, #16]
	ldr	r0, [sp, #8]
	str	r0, [sp, #76]
	mov	r0, #1
	str	r0, [sp, #72]
	mov	r0, r5
	bl	"_clay_insertLocationsUnsafe(Vector[Char], UInt32, UInt32)"
	cmp	r0, #0
	bne	LBB4_6
@ BB#5:                                 @ %normal16.i.i
                                        @   in Loop: Header=BB4_3 Depth=1
	ldr	r0, [sp, #68]
	add	r4, r4, #1
	strb	r11, [r0]
	b	LBB4_3
LBB4_6:                                 @ %landing1.i
	ldr	r0, [sp, #16]
LBB4_7:                                 @ %landing1.i
	bl	_free
	b	LBB4_24
LBB4_8:                                 @ %normal2.i
	mov	r0, #0
	ldr	r4, [sp, #16]
	ldr	r5, [sp, #12]
	ldr	r6, [sp, #8]
	str	r0, [sp, #8]
	str	r0, [sp, #12]
	str	r0, [sp, #16]
	mov	r0, #1
	str	r0, [sp, #24]
	str	r6, [sp, #28]
	str	r5, [sp, #32]
	str	r4, [sp, #36]
	ldr	r0, LCPI4_1
	
LPC4_1:
	add	r5, pc, r0
	ldr	r5, [r5, #128]
	str	r5, [sp, #56]
	cmp	r5, #0
	beq	LBB4_11
@ BB#9:                                 @ %normal2.i
	cmp	r5, #1
	bne	LBB4_15
@ BB#10:                                @ %ifTrue13.i
	ldr	r4, LCPI4_2
	
LPC4_2:
	add	r4, pc, r4
	ldr	r0, [r4, #140]
	bl	_free
LBB4_11:                                @ %normal16.i
	add	r0, sp, #24
	bl	"_clay_moveUnsafe(Exception)"
	cmp	r0, #0
	beq	LBB4_23
@ BB#12:                                @ %landing18.i
	ldr	r4, [sp, #24]
	str	r4, [sp, #84]
	cmp	r4, #0
	beq	LBB4_24
@ BB#13:                                @ %landing18.i
	cmp	r4, #1
	bne	LBB4_19
@ BB#14:                                @ %ifTrue13.i122
	ldr	r0, [sp, #36]
	b	LBB4_7
LBB4_15:                                @ %ifMerge20.i
	sub	r6, r5, #2
	cmp	r6, #2
	blo	LBB4_11
@ BB#16:                                @ %ifMerge20.i
	cmp	r5, #4
	cmpne	r5, #5
	beq	LBB4_11
@ BB#17:                                @ %ifMerge20.i
	cmp	r5, #6
	cmpne	r5, #7
	beq	LBB4_11
@ BB#18:                                @ %ifTrue13.i116
	add	r0, sp, #56
	mov	r5, #1
	bl	"_clay_error(StringConstant, Int32)"
	str	r5, [sp, #80]
	mov	r0, r4
	b	LBB4_7
LBB4_19:                                @ %ifMerge20.i128
	sub	r5, r4, #2
	cmp	r5, #2
	blo	LBB4_24
@ BB#20:                                @ %ifMerge20.i128
	cmp	r4, #4
	cmpne	r4, #5
	beq	LBB4_24
@ BB#21:                                @ %ifMerge20.i128
	cmp	r4, #6
	cmpne	r4, #7
	beq	LBB4_24
@ BB#22:                                @ %ifMerge80.i129
	add	r0, sp, #84
	bl	"_clay_error(StringConstant, Int32)"
	b	LBB4_24
LBB4_23:                                @ %clay_destroy(Exception).exit143
	mov	r4, #0
	str	r4, [sp, #24]
LBB4_24:                                @ %clay_throwValue(Error).exit
	mov	r0, #1
LBB4_25:                                @ %clay_throwValue(Error).exit
	sub	sp, r7, #24
	ldmia	sp!, {r8, r10, r11}
	ldmia	sp!, {r4, r5, r6, r7, pc}
LBB4_26:                                @ %ifMerge
	bl	_malloc
	mov	r8, r0
	ldr	r1, [r5, #8]
	cmp	r6, #0
	beq	LBB4_29
@ BB#27:                                @ %ifMerge.return46_crit_edge
	mov	r0, r8
	mov	r2, r1
LBB4_28:                                @ %return46
                                        @ =>This Inner Loop Header: Depth=1
	ldrb	r3, [r2], #1
	strb	r3, [r0], #1
	subs	r6, r6, #1
	bne	LBB4_28
LBB4_29:                                @ %whileEnd
	mov	r0, r1
	bl	_free
	str	r8, [r5, #8]
	ldr	r0, [r4]
	str	r0, [r5, #4]
	mov	r0, #0
	b	LBB4_25
@ BB#30:
	.align	2
LCPI4_0:
	.long	L_clayliteral_str6-(LPC4_0+8)
	 
	.align	2
LCPI4_1:
	.long	_merged-(LPC4_1+8)
	 
	.align	2
LCPI4_2:
	.long	_merged-(LPC4_2+8)
	 

	.align	2
"_clay_moveUnsafe(Exception)":          @ @"clay_moveUnsafe(Exception)"
@ BB#0:                                 @ %init
	stmdb	sp!, {r7, lr}
	mov	r7, sp
	sub	sp, sp, #4
	ldr	r2, LCPI5_0
	ldr	r1, [r0]
	str	r1, [sp]
	cmp	r1, #7
	
LPC5_0:
	add	r2, pc, r2
	str	r1, [r2, #128]
	bhi	LBB5_11
@ BB#1:                                 @ %init
	adr	r2, #LJTI5_0_0
	ldr	r1, [r2, r1, lsl #2]
	add	pc, r1, r2
LJTI5_0_0:
	.set	L5_0_0_set_10,LBB5_10-LJTI5_0_0
	.long	 L5_0_0_set_10
	.set	L5_0_0_set_2,LBB5_2-LJTI5_0_0
	.long	 L5_0_0_set_2
	.set	L5_0_0_set_3,LBB5_3-LJTI5_0_0
	.long	 L5_0_0_set_3
	.set	L5_0_0_set_4,LBB5_4-LJTI5_0_0
	.long	 L5_0_0_set_4
	.set	L5_0_0_set_5,LBB5_5-LJTI5_0_0
	.long	 L5_0_0_set_5
	.set	L5_0_0_set_7,LBB5_7-LJTI5_0_0
	.long	 L5_0_0_set_7
	.set	L5_0_0_set_8,LBB5_8-LJTI5_0_0
	.long	 L5_0_0_set_8
	.set	L5_0_0_set_9,LBB5_9-LJTI5_0_0
	.long	 L5_0_0_set_9
LBB5_2:                                 @ %ifTrue21
	ldr	r2, LCPI5_1
	ldr	r1, [r0, #4]
	
LPC5_1:
	add	r2, pc, r2
	str	r1, [r2, #132]
	ldr	r1, [r0, #8]
	str	r1, [r2, #136]
	ldr	r0, [r0, #12]
	str	r0, [r2, #140]
	b	LBB5_10
LBB5_3:                                 @ %ifTrue34
	ldr	r2, LCPI5_2
	ldr	r1, [r0, #4]
	
LPC5_2:
	add	r2, pc, r2
	str	r1, [r2, #132]
	ldr	r0, [r0, #8]
	str	r0, [r2, #136]
	b	LBB5_10
LBB5_4:                                 @ %ifTrue47
	ldr	r2, LCPI5_3
	ldr	r1, [r0, #4]
	
LPC5_3:
	add	r2, pc, r2
	str	r1, [r2, #132]
	ldr	r1, [r0, #8]
	str	r1, [r2, #136]
	ldr	r1, [r0, #12]
	str	r1, [r2, #140]
	ldr	r0, [r0, #16]
	str	r0, [r2, #144]
	b	LBB5_10
LBB5_5:                                 @ %ifTrue60
	ldr	r2, LCPI5_4
	ldr	r1, [r0, #4]
	
LPC5_4:
	add	r2, pc, r2
LBB5_6:                                 @ %ifTrue60
	str	r1, [r2, #132]
	ldr	r1, [r0, #8]
	str	r1, [r2, #136]
	ldr	r1, [r0, #12]
	str	r1, [r2, #140]
	ldr	r1, [r0, #16]
	str	r1, [r2, #144]
	ldr	r1, [r0, #20]
	str	r1, [r2, #148]
	ldr	r0, [r0, #24]
	str	r0, [r2, #152]
	b	LBB5_10
LBB5_7:                                 @ %ifTrue73
	ldr	r2, LCPI5_5
	ldr	r1, [r0, #4]
	
LPC5_5:
	add	r2, pc, r2
	b	LBB5_6
LBB5_8:                                 @ %ifTrue86
	ldr	r2, LCPI5_6
	ldr	r1, [r0, #4]
	
LPC5_6:
	add	r2, pc, r2
	b	LBB5_6
LBB5_9:                                 @ %ifTrue99
	ldr	r1, LCPI5_7
	ldr	r0, [r0, #4]
	
LPC5_7:
	add	r1, pc, r1
	str	r0, [r1, #132]
LBB5_10:                                @ %ifTrue
	mov	r0, #0
	mov	sp, r7
	ldmia	sp!, {r7, pc}
LBB5_11:                                @ %ifMerge109
	mov	r0, sp
	bl	"_clay_error(StringConstant, Int32)"
	mov	r0, #1
	mov	sp, r7
	ldmia	sp!, {r7, pc}
@ BB#12:
	.align	2
LCPI5_0:
	.long	_merged-(LPC5_0+8)
	 
	.align	2
LCPI5_1:
	.long	_merged-(LPC5_1+8)
	 
	.align	2
LCPI5_2:
	.long	_merged-(LPC5_2+8)
	 
	.align	2
LCPI5_3:
	.long	_merged-(LPC5_3+8)
	 
	.align	2
LCPI5_4:
	.long	_merged-(LPC5_4+8)
	 
	.align	2
LCPI5_5:
	.long	_merged-(LPC5_5+8)
	 
	.align	2
LCPI5_6:
	.long	_merged-(LPC5_6+8)
	 
	.align	2
LCPI5_7:
	.long	_merged-(LPC5_7+8)
	 

	.align	2
"_clay_insertLocationsUnsafe(Vector[Char], UInt32, UInt32)": @ @"clay_insertLocationsUnsafe(Vector[Char], UInt32, UInt32)"
@ BB#0:                                 @ %init
	stmdb	sp!, {r4, r5, r6, r7, lr}
	add	r7, sp, #12
	stmdb	sp!, {r8, r10, r11}
	sub	sp, sp, #4
	ldr	r12, [r0]
	ldr	r5, [r0, #4]
	ldr	lr, [r2]
	add	r4, lr, r12
	mov	r6, r3
	mov	r8, r2
	mov	r10, r1
	mov	r11, r0
	cmp	r5, r4
	bhs	LBB6_3
@ BB#1:                                 @ %ifTrue
	mov	r12, r4, lsl #1
	mov	r1, sp
	mov	r0, r11
	str	r12, [sp]
	bl	"_clay_vectorSetCapacity(Vector[Char], UInt32)"
	cmp	r0, #0
	bne	LBB6_8
@ BB#2:                                 @ %ifTrue.ifMerge_crit_edge
	ldr	r12, [r11]
	ldr	lr, [r8]
LBB6_3:                                 @ %ifMerge
	ldr	r0, [r10]
	ldr	r1, [r11, #8]
	add	r2, r1, r0
	cmp	r12, r0
	beq	LBB6_7
@ BB#4:                                 @ %bb.nph.i.i
	add	r3, lr, r12
	add	r4, lr, r0
	sub	r5, r3, r4
	add	r12, r12, lr
	add	r3, r1, r3
	add	r12, r12, r1
	add	r3, r3, r0
	sub	r12, r12, #1
	sub	r3, r3, #1
	sub	r3, r3, r4
LBB6_5:                                 @ %whileBody.i.i
                                        @ =>This Inner Loop Header: Depth=1
	ldrb	lr, [r3], #-1
	strb	lr, [r12], #-1
	subs	r5, r5, #1
	bne	LBB6_5
@ BB#6:                                 @ %clay_moveMemoryBackwardsUnsafe(Pointer[Char], Pointer[Char], Pointer[Char]).exit.loopexit
	ldr	lr, [r8]
	ldr	r12, [r11]
LBB6_7:                                 @ %clay_moveMemoryBackwardsUnsafe(Pointer[Char], Pointer[Char], Pointer[Char]).exit
	add	r3, lr, r12
	str	r3, [r11]
	str	r2, [r6]
	ldr	r2, [r8]
	add	r0, r2, r0
	add	r0, r1, r0
	ldr	r1, [r7, #8]
	str	r0, [r1]
	mov	r0, #0
	b	LBB6_9
LBB6_8:                                 @ %landing
	mov	r0, #1
LBB6_9:                                 @ %landing
	sub	sp, r7, #24
	ldmia	sp!, {r8, r10, r11}
	ldmia	sp!, {r4, r5, r6, r7, pc}

	.align	2
"_clay_cwrapper_LambdaProcedure(Pointer[ObjcObject], Pointer[ObjcMethod], Pointer[ObjcObject], Pointer[ObjcObject])": @ @"clay_cwrapper_LambdaProcedure(Pointer[ObjcObject], Pointer[ObjcMethod], Pointer[ObjcObject], Pointer[ObjcObject])"
Leh_func_begin7:
@ BB#0:                                 @ %body
	stmdb	sp!, {r4, r5, r6, r7, lr}
	ldr	r1, LCPI7_0
	
LPC7_0:
	add	r5, pc, r1
	mov	r4, r0
	ldr	r0, [r5, #20]
	ldr	r6, [r5, #16]
	add	r7, sp, #12
	ldr	r1, [r5, #24]
	ldr	r0, [r4, r0]
	bl	_objc_msgSend
	mov	r2, r0
	ldr	r0, [r4, r6]
	ldr	r1, [r5, #28]
	bl	_objc_msgSend
	ldr	r0, [r5, #16]
	ldr	r1, [r5, #32]
	ldr	r0, [r4, r0]
	bl	_objc_msgSend
	ldmia	sp!, {r4, r5, r6, r7, pc}
@ BB#1:
	.align	2
LCPI7_0:
	.long	_merged-(LPC7_0+8)
	 
Leh_func_end7:

	.align	2
"_clay_add(StringConstant, StringConstant)": @ @"clay_add(StringConstant, StringConstant)"
@ BB#0:                                 @ %init
	stmdb	sp!, {r4, r5, r7, lr}
	add	r7, sp, #8
	sub	sp, sp, #32
	bic	sp, sp, #7
	mov	r4, r2
	mov	r5, r1
	mov	r1, r0
	mov	r2, #0
	add	r0, sp, #16
	str	r2, [sp, #16]
	str	r2, [sp, #20]
	str	r2, [sp, #24]
	bl	"_clay_push(Vector[Char], StringConstant)"
	cmp	r0, #0
	beq	LBB8_3
@ BB#1:                                 @ %landing
	ldr	r0, [sp, #24]
LBB8_2:                                 @ %landing
	bl	_free
	mov	r0, #1
	sub	sp, r7, #8
	ldmia	sp!, {r4, r5, r7, pc}
LBB8_3:                                 @ %normal
	ldr	r0, [sp, #16]
	str	r0, [sp]
	mov	r1, r5
	ldr	r0, [sp, #20]
	str	r0, [sp, #4]
	ldr	r0, [sp, #24]
	str	r0, [sp, #8]
	mov	r0, sp
	bl	"_clay_push(Vector[Char], StringConstant)"
	cmp	r0, #0
	ldreq	r0, [sp]
	streq	r0, [r4]
	ldreq	r0, [sp, #4]
	streq	r0, [r4, #4]
	ldreq	r0, [sp, #8]
	streq	r0, [r4, #8]
	moveq	r0, #0
	subeq	sp, r7, #8
	ldmiaeq	sp!, {r4, r5, r7, pc}
	ldr	r0, [sp, #8]
	b	LBB8_2

	.align	2
"_clay_push(Vector[Char], StringConstant)": @ @"clay_push(Vector[Char], StringConstant)"
@ BB#0:                                 @ %init
	stmdb	sp!, {r4, r5, r6, r7, lr}
	add	r7, sp, #12
	stmdb	sp!, {r8, r10, r11}
	sub	sp, sp, #8
	ldrd	r4, [r1]
	ldrd	r8, [r0]
	sub	r6, r5, r4
	add	r10, r6, r8
	str	r1, [sp]
	mov	r11, r0
	cmp	r9, r10
	blo	LBB9_2
@ BB#1:                                 @ %init.ifMerge.i.i_crit_edge
	mov	r0, r8
	b	LBB9_6
LBB9_2:                                 @ %ifTrue.i.i
	mov	r0, r10, lsl #1
	str	r0, [sp, #4]
	add	r1, sp, #4
	mov	r0, r11
	bl	"_clay_vectorSetCapacity(Vector[Char], UInt32)"
	cmp	r0, #0
	beq	LBB9_5
@ BB#3:                                 @ %ifTrue.i.i.clay_insert(Vector[Char], UInt32, StringConstant).exit_crit_edge
	mov	r0, #1
LBB9_4:                                 @ %clay_insert(Vector[Char], UInt32, StringConstant).exit
	sub	sp, r7, #24
	ldmia	sp!, {r8, r10, r11}
	ldmia	sp!, {r4, r5, r6, r7, pc}
LBB9_5:                                 @ %ifTrue.ifMerge_crit_edge.i.i
	ldr	r0, [r11]
LBB9_6:                                 @ %ifMerge.i.i
	add	r1, r0, r6
	ldr	r2, [r11, #8]
	add	r3, r2, r8
	cmp	r1, r10
	beq	LBB9_10
@ BB#7:                                 @ %bb.nph.i.i.i.i
	add	r0, r0, r5
	sub	r12, r1, r10
	add	r1, r2, r1
	add	r1, r1, r8
	sub	r0, r0, #1
	sub	r1, r1, #1
	sub	r0, r0, r4
	sub	r1, r1, r10
	add	r0, r2, r0
LBB9_8:                                 @ %whileBody.i.i.i.i
                                        @ =>This Inner Loop Header: Depth=1
	ldrb	lr, [r1], #-1
	strb	lr, [r0], #-1
	subs	r12, r12, #1
	bne	LBB9_8
@ BB#9:                                 @ %clay_moveMemoryBackwardsUnsafe(Pointer[Char], Pointer[Char], Pointer[Char]).exit.loopexit.i.i
	ldr	r0, [r11]
LBB9_10:                                @ %normal.i
	add	r0, r0, r6
	str	r0, [r11]
	cmp	r8, r10
	bne	LBB9_12
LBB9_11:                                @ %normal.i.clay_insert(Vector[Char], UInt32, StringConstant).exit_crit_edge
	mov	r0, #0
	b	LBB9_4
LBB9_12:                                @ %bb.nph.i.i.i
	ldr	r1, [sp]
	ldr	r0, [r1]
	sub	r1, r10, r3
	add	r1, r2, r1
LBB9_13:                                @ %whileBody.i.i.i
                                        @ =>This Inner Loop Header: Depth=1
	ldrb	r2, [r0], #1
	strb	r2, [r3], #1
	subs	r1, r1, #1
	bne	LBB9_13
	b	LBB9_11

	.align	2
"_clay_add(StringConstant, Vector[Char])": @ @"clay_add(StringConstant, Vector[Char])"
@ BB#0:                                 @ %init
	stmdb	sp!, {r4, r5, r6, r7, lr}
	add	r7, sp, #12
	stmdb	sp!, {r8, r10}
	sub	sp, sp, #36
	bic	sp, sp, #7
	mov	r4, r2
	mov	r5, r1
	mov	r1, r0
	mov	r2, #0
	add	r0, sp, #16
	str	r2, [sp, #16]
	str	r2, [sp, #20]
	str	r2, [sp, #24]
	bl	"_clay_push(Vector[Char], StringConstant)"
	cmp	r0, #0
	beq	LBB10_3
@ BB#1:                                 @ %landing
	ldr	r0, [sp, #24]
LBB10_2:                                @ %landing
	bl	_free
	mov	r0, #1
	b	LBB10_14
LBB10_3:                                @ %normal
	ldr	r6, [sp, #16]
	str	r6, [sp]
	ldr	r0, [sp, #20]
	str	r0, [sp, #4]
	ldr	r1, [sp, #24]
	str	r1, [sp, #8]
	ldr	r8, [r5]
	add	r10, r8, r6
	cmp	r0, r10
	blo	LBB10_5
@ BB#4:                                 @ %normal.ifMerge.i.i.i_crit_edge
	mov	r0, r6
	b	LBB10_7
LBB10_5:                                @ %ifTrue.i.i.i
	mov	r0, r10, lsl #1
	str	r0, [sp, #32]
	add	r1, sp, #32
	mov	r0, sp
	bl	"_clay_vectorSetCapacity(Vector[Char], UInt32)"
	cmp	r0, #0
	bne	LBB10_15
@ BB#6:                                 @ %ifTrue.ifMerge_crit_edge.i.i.i
	ldr	r1, [sp, #8]
	ldr	r0, [sp]
LBB10_7:                                @ %ifMerge.i.i.i
	add	r0, r0, r8
	cmp	r0, r10
	beq	LBB10_10
@ BB#8:                                 @ %bb.nph.i.i.i.i.i
	add	r3, r1, r0
	add	r12, r0, r1
	sub	r2, r0, r10
	add	r3, r3, r6
	sub	r12, r12, #1
	sub	r3, r3, #1
	sub	r3, r3, r10
LBB10_9:                                @ %whileBody.i.i.i.i.i
                                        @ =>This Inner Loop Header: Depth=1
	ldrb	lr, [r3], #-1
	strb	lr, [r12], #-1
	subs	r2, r2, #1
	bne	LBB10_9
LBB10_10:                               @ %normal.i.i
	str	r0, [sp]
	cmp	r6, r10
	beq	LBB10_13
@ BB#11:                                @ %bb.nph.i.i.i.i
	add	r2, r1, r6
	sub	r12, r10, r2
	ldr	r3, [r5, #8]
	add	r12, r1, r12
LBB10_12:                               @ %whileBody.i.i.i.i
                                        @ =>This Inner Loop Header: Depth=1
	ldrb	lr, [r3], #1
	strb	lr, [r2], #1
	subs	r12, r12, #1
	bne	LBB10_12
LBB10_13:                               @ %normal2
	str	r0, [r4]
	ldr	r0, [sp, #4]
	stmib	r4, {r0, r1}
	mov	r0, #0
LBB10_14:                               @ %normal2
	sub	sp, r7, #20
	ldmia	sp!, {r8, r10}
	ldmia	sp!, {r4, r5, r6, r7, pc}
LBB10_15:                               @ %landing1
	ldr	r0, [sp, #8]
	b	LBB10_2

	.align	2
"_clay_cwrapper_LambdaProcedure(Pointer[ObjcObject], Pointer[ObjcMethod])": @ @"clay_cwrapper_LambdaProcedure(Pointer[ObjcObject], Pointer[ObjcMethod])"
Leh_func_begin11:
@ BB#0:                                 @ %body
	stmdb	sp!, {r4, r5, r7, lr}
	add	r7, sp, #8
	sub	sp, sp, #8
	bic	sp, sp, #7
	ldr	r1, LCPI11_0
	
LPC11_0:
	add	r5, pc, r1
	mov	r4, r0
	ldr	r0, [r5, #20]
	ldr	r0, [r4, r0]
	ldr	r1, [r5, #40]
	bl	_objc_msgSend
	ldr	r0, [r5, #16]
	ldr	r1, [r5, #40]
	ldr	r0, [r4, r0]
	bl	_objc_msgSend
	str	r4, [sp]
	ldr	r0, [r5, #8]
	str	r0, [sp, #4]
	ldr	r1, [r5, #36]
	mov	r0, sp
	bl	_objc_msgSendSuper
	sub	sp, r7, #8
	ldmia	sp!, {r4, r5, r7, pc}
@ BB#1:
	.align	2
LCPI11_0:
	.long	_merged-(LPC11_0+8)
	 
Leh_func_end11:

	.align	2
"_clay_cwrapper_LambdaProcedure(Pointer[ObjcObject], Pointer[ObjcMethod], Int32)": @ @"clay_cwrapper_LambdaProcedure(Pointer[ObjcObject], Pointer[ObjcMethod], Int32)"
@ BB#0:                                 @ %body
	mov	r0, #1
	bx	lr

	.align	2
"_clay_cwrapper_LambdaProcedure(Pointer[ObjcObject], Pointer[ObjcMethod], Pointer[ObjcObject])": @ @"clay_cwrapper_LambdaProcedure(Pointer[ObjcObject], Pointer[ObjcMethod], Pointer[ObjcObject])"
Leh_func_begin13:
@ BB#0:                                 @ %body
	stmdb	sp!, {r4, r5, r6, r7, lr}
	mov	r4, r0
	ldr	r1, LCPI13_0
	
LPC13_0:
	add	r0, pc, r1
	add	r7, sp, #12
	ldr	r1, [r0, #56]
	mov	r0, r2
	bl	_objc_msgSend
	cmp	r0, #3
	ldmiahi	sp!, {r4, r5, r6, r7, pc}
	adr	r1, #LJTI13_0_0
	ldr	r0, [r1, r0, lsl #2]
	add	pc, r0, r1
LJTI13_0_0:
	.set	L13_0_0_set_1,LBB13_1-LJTI13_0_0
	.long	 L13_0_0_set_1
	.set	L13_0_0_set_3,LBB13_3-LJTI13_0_0
	.long	 L13_0_0_set_3
	.set	L13_0_0_set_4,LBB13_4-LJTI13_0_0
	.long	 L13_0_0_set_4
	.set	L13_0_0_set_5,LBB13_5-LJTI13_0_0
	.long	 L13_0_0_set_5
LBB13_1:                                @ %ifTrue.i.i
	ldr	r0, LCPI13_1
	
LPC13_1:
	add	r5, pc, r0
	ldrd	r0, [r5, #60]
	ldr	r0, [r4, r0]
	bl	_objc_msgSend
	ldr	r1, [r5, #68]
	bl	_objc_msgSend
	mov	r6, r0
	ldr	r0, [r5, #72]
	ldr	r1, [r5, #64]
	ldr	r0, [r4, r0]
	bl	_objc_msgSend
	ldr	r1, [r5, #68]
	bl	_objc_msgSend
	add	r3, r0, r6
LBB13_2:                                @ %ifTrue.i.i
	ldr	r0, [r5, #80]
	ldr	r1, [r5, #84]
	add	r2, r5, #112
	ldr	r6, [r5, #76]
	bl	_objc_msgSend
	mov	r2, r0
	ldr	r0, [r4, r6]
	ldr	r1, [r5, #88]
	bl	_objc_msgSend
	ldmia	sp!, {r4, r5, r6, r7, pc}
LBB13_3:                                @ %ifTrue7.i.i
	ldr	r0, LCPI13_2
	
LPC13_2:
	add	r5, pc, r0
	ldrd	r0, [r5, #60]
	ldr	r0, [r4, r0]
	bl	_objc_msgSend
	ldr	r1, [r5, #68]
	bl	_objc_msgSend
	mov	r6, r0
	ldr	r0, [r5, #72]
	ldr	r1, [r5, #64]
	ldr	r0, [r4, r0]
	bl	_objc_msgSend
	ldr	r1, [r5, #68]
	bl	_objc_msgSend
	sub	r3, r6, r0
	b	LBB13_2
LBB13_4:                                @ %ifTrue12.i.i
	ldr	r0, LCPI13_3
	
LPC13_3:
	add	r5, pc, r0
	ldrd	r0, [r5, #60]
	ldr	r0, [r4, r0]
	bl	_objc_msgSend
	ldr	r1, [r5, #68]
	bl	_objc_msgSend
	mov	r6, r0
	ldr	r0, [r5, #72]
	ldr	r1, [r5, #64]
	ldr	r0, [r4, r0]
	bl	_objc_msgSend
	ldr	r1, [r5, #68]
	bl	_objc_msgSend
	mul	r3, r0, r6
	b	LBB13_2
LBB13_5:                                @ %ifTrue17.i.i
	ldr	r0, LCPI13_4
	
LPC13_4:
	add	r5, pc, r0
	ldrd	r0, [r5, #60]
	ldr	r0, [r4, r0]
	bl	_objc_msgSend
	ldr	r1, [r5, #68]
	bl	_objc_msgSend
	mov	r6, r0
	ldr	r0, [r5, #72]
	ldr	r1, [r5, #64]
	ldr	r0, [r4, r0]
	bl	_objc_msgSend
	ldr	r1, [r5, #68]
	bl	_objc_msgSend
	mov	r1, r0
	mov	r0, r6
	bl	___divsi3
	mov	r3, r0
	b	LBB13_2
@ BB#6:
	.align	2
LCPI13_0:
	.long	_merged-(LPC13_0+8)
	 
	.align	2
LCPI13_1:
	.long	_merged-(LPC13_1+8)
	 
	.align	2
LCPI13_2:
	.long	_merged-(LPC13_2+8)
	 
	.align	2
LCPI13_3:
	.long	_merged-(LPC13_3+8)
	 
	.align	2
LCPI13_4:
	.long	_merged-(LPC13_4+8)
	 
Leh_func_end13:

	.align	2
"_clay_destroy(RawFile)":               @ @"clay_destroy(RawFile)"
Leh_func_begin14:
@ BB#0:                                 @ %init
	stmdb	sp!, {r4, r7, lr}
	add	r7, sp, #4
	sub	sp, sp, #4
	bic	sp, sp, #7
	ldrb	r1, [r0, #4]
	cmp	r1, #0
	beq	LBB14_7
@ BB#1:                                 @ %ifTrue
	ldr	r0, [r0]
	bl	_close
	cmn	r0, #1
	bne	LBB14_7
@ BB#2:                                 @ %ifTrue9
	bl	___error
	ldr	r0, [r0]
	bl	_strerror
	mov	r4, r0
	mov	r0, #10
	strb	r0, [sp]
	mov	r1, #7
	ldr	r0, LCPI14_0
	
LPC14_0:
	add	r0, pc, r0
	bl	"_clay_write(File, Pointer[UInt8], UInt32)"
	cmp	r0, #0
	beq	LBB14_4
LBB14_3:                                @ %ifTrue9.clay_errorNoThrow(CStringRef).exit_crit_edge
	mov	r4, #0
	b	LBB14_6
LBB14_4:                                @ %normal.i.i.i
	mov	r0, r4
	bl	_strlen
	mov	r1, r0
	mov	r0, r4
	bl	"_clay_write(File, Pointer[UInt8], UInt32)"
	cmp	r0, #0
	bne	LBB14_3
@ BB#5:                                 @ %normal2.i.i.i
	mov	r0, sp
	mov	r1, #1
	mov	r4, #0
	bl	"_clay_write(File, Pointer[UInt8], UInt32)"
	cmp	r0, #0
	moveq	r4, #1
LBB14_6:                                @ %clay_errorNoThrow(CStringRef).exit
	and	r0, r4, #1
	eor	r0, r0, #1
	sub	sp, r7, #4
	ldmia	sp!, {r4, r7, pc}
LBB14_7:                                @ %ifMerge
	mov	r0, #0
	sub	sp, r7, #4
	ldmia	sp!, {r4, r7, pc}
@ BB#8:
	.align	2
LCPI14_0:
	.long	L_clayliteral_str209-(LPC14_0+8)
	 
Leh_func_end14:

	.align	2
"_clay_write(File, Pointer[UInt8], UInt32)": @ @"clay_write(File, Pointer[UInt8], UInt32)"
Leh_func_begin15:
@ BB#0:                                 @ %init
	stmdb	sp!, {r4, r5, r6, r7, lr}
	add	r7, sp, #12
	stmdb	sp!, {r8, r10, r11}
	ldr	r2, LCPI15_0
	mov	r3, #24
	orr	r3, r3, #2, 20	@ 8192
	mov	r10, r0
	mov	r11, r1
	mov	r0, #0
	
LPC15_0:
	add	r2, pc, r2
	ldr	r4, [r2, #108]
	mov	r5, r4
	ldr	r3, [r5, r3]!
	add	r2, r4, #21
	add	r6, r2, #1, 20	@ 4096
	add	r8, r2, #2, 20	@ 8192
	add	r3, r6, r3
LBB15_1:                                @ %whileBegin
                                        @ =>This Inner Loop Header: Depth=1
	cmp	r11, #0
	beq	LBB15_11
@ BB#2:                                 @ %whileBody
                                        @   in Loop: Header=BB15_1 Depth=1
	cmp	r3, r8
	bne	LBB15_7
@ BB#3:                                 @ %return27
                                        @   in Loop: Header=BB15_1 Depth=1
	sub	r2, r3, r6
	str	r2, [r5]
	cmp	r3, r6
	bne	LBB15_5
@ BB#4:                                 @ %return27.ifMerge_crit_edge
                                        @   in Loop: Header=BB15_1 Depth=1
	mov	r0, #0
	b	LBB15_6
LBB15_5:                                @ %return10.i
                                        @   in Loop: Header=BB15_1 Depth=1
	ldr	r0, [r4, #4]
	mov	r1, r6
	bl	"_clay_write(RawFile, Pointer[UInt8], UInt32)"
	cmp	r0, #0
	movne	r0, #1
	ldmiane	sp!, {r8, r10, r11}
	ldmiane	sp!, {r4, r5, r6, r7, pc}
	mov	r0, #0
	str	r0, [r5]
LBB15_6:                                @ %return18.i
                                        @   in Loop: Header=BB15_1 Depth=1
	mov	r3, r6
LBB15_7:                                @ %ifMerge
                                        @   in Loop: Header=BB15_1 Depth=1
	ldrb	r1, [r10], #1
	strb	r1, [r3], #1
	cmp	r1, #10
	sub	r11, r11, #1
	mov	r1, #0
	moveq	r1, #1
	add	r0, r1, r0
	b	LBB15_1
LBB15_8:                                @ %ifTrue98
	cmp	r3, r6
	bne	LBB15_10
@ BB#9:                                 @ %ifTrue98.clay_flushData(FileData).exit7_crit_edge
	mov	r0, #0
	ldmia	sp!, {r8, r10, r11}
	ldmia	sp!, {r4, r5, r6, r7, pc}
LBB15_10:                               @ %return10.i3
	ldr	r0, [r4, #4]
	mov	r1, r6
	bl	"_clay_write(RawFile, Pointer[UInt8], UInt32)"
	cmp	r0, #0
	moveq	r0, #0
	streq	r0, [r5]
	movne	r0, #1
	ldmia	sp!, {r8, r10, r11}
	ldmia	sp!, {r4, r5, r6, r7, pc}
LBB15_11:                               @ %return77
	cmp	r0, #0
	sub	r2, r3, r6
	str	r2, [r5]
	movne	r0, #20
	orrne	r0, r0, #1, 20	@ 4096
	ldrbne	r0, [r4, r0]
	tstne	r0, #255
	bne	LBB15_8
@ BB#12:                                @ %ifMerge102
	mov	r0, #0
	ldmia	sp!, {r8, r10, r11}
	ldmia	sp!, {r4, r5, r6, r7, pc}
@ BB#13:
	.align	2
LCPI15_0:
	.long	_merged-(LPC15_0+8)
	 
Leh_func_end15:

	.align	2
"_clay_write(RawFile, Pointer[UInt8], UInt32)": @ @"clay_write(RawFile, Pointer[UInt8], UInt32)"
Leh_func_begin16:
@ BB#0:                                 @ %init
	stmdb	sp!, {r4, r5, r6, r7, lr}
	add	r7, sp, #12
	stmdb	sp!, {r8, r10, r11}
	sub	sp, sp, #24
	bic	sp, sp, #7
	bl	_write
	cmn	r0, #1
	bne	LBB16_23
@ BB#1:                                 @ %ifTrue
	bl	___error
	ldr	r0, [r0]
	bl	_strerror
	mov	r4, r0
	mov	r1, #0
	str	r1, [sp]
	str	r1, [sp, #4]
	str	r1, [sp, #8]
	bl	_strlen
	mov	r5, r0
	cmp	r0, #0
	bne	LBB16_12
LBB16_2:                                @ %ifTrue.whileBegin.i.i.i_crit_edge
	add	r6, sp, #16
LBB16_3:                                @ %whileBegin.i.i.i
                                        @ =>This Loop Header: Depth=1
                                        @     Child Loop BB16_10 Depth 2
	cmp	r5, #0
	beq	LBB16_15
@ BB#4:                                 @ %whileBody.i.i.i
                                        @   in Loop: Header=BB16_3 Depth=1
	ldr	r8, [sp]
	add	r10, r8, #1
	ldr	r0, [sp, #4]
	ldrb	r11, [r4]
	cmp	r0, r10
	blo	LBB16_6
@ BB#5:                                 @ %whileBody.i.i.i.ifMerge.i.i.i_crit_edge
                                        @   in Loop: Header=BB16_3 Depth=1
	mov	r0, r8
	b	LBB16_8
LBB16_6:                                @ %ifTrue.i.i.i
                                        @   in Loop: Header=BB16_3 Depth=1
	mov	r0, r10, lsl #1
	str	r0, [sp, #16]
	mov	r1, r6
	mov	r0, sp
	bl	"_clay_vectorSetCapacity(Vector[Char], UInt32)"
	cmp	r0, #0
	bne	LBB16_13
@ BB#7:                                 @ %ifTrue.ifMerge_crit_edge.i.i.i
                                        @   in Loop: Header=BB16_3 Depth=1
	ldr	r0, [sp]
LBB16_8:                                @ %ifMerge.i.i.i
                                        @   in Loop: Header=BB16_3 Depth=1
	add	r1, r0, #1
	ldr	r2, [sp, #8]
	cmp	r1, r10
	beq	LBB16_11
@ BB#9:                                 @ %bb.nph.i.i.i.i.i
                                        @   in Loop: Header=BB16_3 Depth=1
	add	r12, r2, r1
	add	r0, r2, r0
	sub	r3, r1, r10
	add	r12, r12, r8
	sub	r12, r12, r10
	sub	r12, r12, #1
LBB16_10:                               @ %whileBody.i.i.i.i.i
                                        @   Parent Loop BB16_3 Depth=1
                                        @ =>  This Inner Loop Header: Depth=2
	ldrb	lr, [r12], #-1
	strb	lr, [r0], #-1
	subs	r3, r3, #1
	bne	LBB16_10
LBB16_11:                               @ %normal16.i.i.i
                                        @   in Loop: Header=BB16_3 Depth=1
	str	r1, [sp]
	strb	r11, [r2, r8]
	sub	r5, r5, #1
	add	r4, r4, #1
	b	LBB16_3
LBB16_12:                               @ %clay_reserve(Vector[Char], UInt32).exit.i.i.i
	mov	r0, sp
	add	r1, sp, #12
	str	r5, [sp, #12]
	bl	"_clay_vectorSetCapacity(Vector[Char], UInt32)"
	cmp	r0, #0
	beq	LBB16_2
LBB16_13:                               @ %landing1.i
	ldr	r0, [sp, #8]
LBB16_14:                               @ %landing1.i
	bl	_free
	mov	r0, #1
	b	LBB16_24
LBB16_15:                               @ %normal2.i
	mov	r0, #0
	ldr	r6, [sp]
	ldr	r4, [sp, #8]
	ldr	r5, [sp, #4]
	str	r0, [sp]
	str	r0, [sp, #4]
	str	r0, [sp, #8]
	ldr	r0, LCPI16_0
	
LPC16_0:
	add	r0, pc, r0
	ldr	r0, [r0, #128]
	str	r0, [sp, #20]
	cmp	r0, #0
	beq	LBB16_18
@ BB#16:                                @ %normal2.i
	cmp	r0, #1
	bne	LBB16_19
@ BB#17:                                @ %ifTrue13.i.i.i
	ldr	r0, LCPI16_1
	
LPC16_1:
	add	r0, pc, r0
	ldr	r0, [r0, #140]
	bl	_free
LBB16_18:                               @ %clay_destroy(Exception).exit17.i
	ldr	r0, LCPI16_2
	
LPC16_2:
	add	r1, pc, r0
	mov	r0, #1
	str	r0, [r1, #128]
	str	r6, [r1, #132]
	str	r5, [r1, #136]
	str	r4, [r1, #140]
	b	LBB16_24
LBB16_19:                               @ %ifMerge20.i.i.i
	sub	r1, r0, #2
	cmp	r1, #2
	blo	LBB16_18
@ BB#20:                                @ %ifMerge20.i.i.i
	cmp	r0, #4
	cmpne	r0, #5
	beq	LBB16_18
@ BB#21:                                @ %ifMerge20.i.i.i
	cmp	r0, #6
	cmpne	r0, #7
	beq	LBB16_18
@ BB#22:                                @ %clay_destroy(Exception).exit30.i
	add	r0, sp, #20
	bl	"_clay_error(StringConstant, Int32)"
	mov	r0, r4
	b	LBB16_14
LBB16_23:                               @ %ifMerge
	mov	r0, #0
LBB16_24:                               @ %ifMerge
	sub	sp, r7, #24
	ldmia	sp!, {r8, r10, r11}
	ldmia	sp!, {r4, r5, r6, r7, pc}
@ BB#25:
	.align	2
LCPI16_0:
	.long	_merged-(LPC16_0+8)
	 
	.align	2
LCPI16_1:
	.long	_merged-(LPC16_1+8)
	 
	.align	2
LCPI16_2:
	.long	_merged-(LPC16_2+8)
	 
Leh_func_end16:

	.section	__TEXT,__cstring,cstring_literals
	.align	4                       @ @clayliteral_str
L_clayliteral_str:
	.asciz	 "exception when initializing globals\n"

	.section	__DATA,__mod_init_func,mod_init_funcs
	.align	2
	.long	_clayglobals_init
	.section	__DATA,__mod_term_func,mod_term_funcs
	.align	2
	.long	_clayglobals_destroy
	.section	__TEXT,__cstring,cstring_literals
	.align	4                       @ @clayliteral_str2
L_clayliteral_str2:
	.asciz	 "ExampleAppDelegate"

L_clayliteral_str4:                     @ @clayliteral_str4
	.asciz	 "NSObject"

	.align	4                       @ @clayliteral_str5
L_clayliteral_str5:
	.asciz	 "invalid variant, tag = "

	.align	4                       @ @clayliteral_str6
L_clayliteral_str6:
	.asciz	 "assertion failed"

L_clayliteral_str8:                     @ @clayliteral_str8
	.asciz	 "%d"

L_clayliteral_str9:                     @ @clayliteral_str9
	.asciz	 "__main__"

L_clayliteral_str11:                    @ @clayliteral_str11
	.asciz	 "window"

L_clayliteral_str12:                    @ @clayliteral_str12
	.asciz	 "@"

L_clayliteral_str15:                    @ @clayliteral_str15
	.asciz	 "viewController"

	.align	4                       @ @clayliteral_str19
L_clayliteral_str19:
	.asciz	 "application:didFinishLaunchingWithOptions:"

L_clayliteral_str22:                    @ @clayliteral_str22
	.asciz	 "view"

L_clayliteral_str28:                    @ @clayliteral_str28
	.asciz	 "addSubview:"

	.align	4                       @ @clayliteral_str40
L_clayliteral_str40:
	.asciz	 "makeKeyAndVisible"

L_clayliteral_str41:                    @ @clayliteral_str41
	.asciz	 "c"

L_clayliteral_str43:                    @ @clayliteral_str43
	.asciz	 ":"

L_clayliteral_str51:                    @ @clayliteral_str51
	.asciz	 "dealloc"

L_clayliteral_str60:                    @ @clayliteral_str60
	.asciz	 "release"

L_clayliteral_str68:                    @ @clayliteral_str68
	.asciz	 "v"

	.align	4                       @ @clayliteral_str76
L_clayliteral_str76:
	.asciz	 "ExampleViewController"

	.align	4                       @ @clayliteral_str78
L_clayliteral_str78:
	.asciz	 "UIViewController"

	.align	4                       @ @clayliteral_str82
L_clayliteral_str82:
	.asciz	 "shouldAutorotateToInterfaceOrientation:"

L_clayliteral_str83:                    @ @clayliteral_str83
	.asciz	 "i"

	.align	4                       @ @clayliteral_str88
L_clayliteral_str88:
	.asciz	 "ExampleController"

L_clayliteral_str91:                    @ @clayliteral_str91
	.asciz	 "leftField"

L_clayliteral_str95:                    @ @clayliteral_str95
	.asciz	 "rightField"

L_clayliteral_str98:                    @ @clayliteral_str98
	.asciz	 "resultLabel"

L_clayliteral_str103:                   @ @clayliteral_str103
	.asciz	 "performMath:"

	.align	4                       @ @clayliteral_str122
L_clayliteral_str122:
	.asciz	 "selectedSegmentIndex"

L_clayliteral_str130:                   @ @clayliteral_str130
	.asciz	 "text"

L_clayliteral_str141:                   @ @clayliteral_str141
	.asciz	 "intValue"

L_clayliteral_str163:                   @ @clayliteral_str163
	.asciz	 "NSString"

	.align	4                       @ @clayliteral_str165
L_clayliteral_str165:
	.asciz	 "stringWithFormat:"

L_clayliteral_str175:                   @ @clayliteral_str175
	.asciz	 "setText:"

	.align	4                       @ @clayliteral_str191
L_clayliteral_str191:
	.asciz	 "NSAutoreleasePool"

L_clayliteral_str193:                   @ @clayliteral_str193
	.asciz	 "alloc"

L_clayliteral_str200:                   @ @clayliteral_str200
	.asciz	 "init"

L_clayliteral_str208:                   @ @clayliteral_str208
	.asciz	 "drain"

L_clayliteral_str209:                   @ @clayliteral_str209
	.asciz	 "error: "

.zerofill __DATA,__bss,_merged,156,4    @ @merged
                                        @ @merged

	.section	__DATA,__nl_symbol_ptr,non_lazy_symbol_pointers
	.align	2
L___CFConstantStringClassReference$non_lazy_ptr:
	.indirect_symbol	___CFConstantStringClassReference
	.long	0
L___stderrp$non_lazy_ptr:
	.indirect_symbol	___stderrp
	.long	0

.subsections_via_symbols
