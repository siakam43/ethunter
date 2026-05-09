/* CG-Bench fixture: fnptr-global-array/example_6 */
/* fnptr: gf_x1_mul_fns[c], targets: mul_x1_0, mul_x1_1, mul_x1_2, mul_x1_3, mul_x1_4, mul_x1_5, mul_x1_6, mul_x1_7, mul_x1_8, mul_x1_9, mul_x1_10, mul_x1_11, mul_x1_12, mul_x1_13, mul_x1_14, mul_x1_15, mul_x1_16, mul_x1_17, mul_x1_18, mul_x1_19, mul_x1_20, mul_x1_21, mul_x1_22, mul_x1_23, mul_x1_24, mul_x1_25, mul_x1_26, mul_x1_27, mul_x1_28, mul_x1_29, mul_x1_30, mul_x1_31, mul_x1_32, mul_x1_33, mul_x1_34, mul_x1_35, mul_x1_36, mul_x1_37, mul_x1_38, mul_x1_39, mul_x1_40, mul_x1_41, mul_x1_42, mul_x1_43, mul_x1_44, mul_x1_45, mul_x1_46, mul_x1_47, mul_x1_48, mul_x1_49, mul_x1_50, mul_x1_51, mul_x1_52, mul_x1_53, mul_x1_54, mul_x1_55, mul_x1_56, mul_x1_57, mul_x1_58, mul_x1_59, mul_x1_60, mul_x1_61, mul_x1_62, mul_x1_63, mul_x1_64, mul_x1_65, mul_x1_66, mul_x1_67, mul_x1_68, mul_x1_69, mul_x1_70, mul_x1_71, mul_x1_72, mul_x1_73, mul_x1_74, mul_x1_75, mul_x1_76, mul_x1_77, mul_x1_78, mul_x1_79, mul_x1_80, mul_x1_81, mul_x1_82, mul_x1_83, mul_x1_84, mul_x1_85, mul_x1_86, mul_x1_87, mul_x1_88, mul_x1_89, mul_x1_90, mul_x1_91, mul_x1_92, mul_x1_93, mul_x1_94, mul_x1_95, mul_x1_96, mul_x1_97, mul_x1_98, mul_x1_99, mul_x1_100, mul_x1_101, mul_x1_102, mul_x1_103, mul_x1_104, mul_x1_105, mul_x1_106, mul_x1_107, mul_x1_108, mul_x1_109, mul_x1_110, mul_x1_111, mul_x1_112, mul_x1_113, mul_x1_114, mul_x1_115, mul_x1_116, mul_x1_117, mul_x1_118, mul_x1_119, mul_x1_120, mul_x1_121, mul_x1_122, mul_x1_123, mul_x1_124, mul_x1_125, mul_x1_126, mul_x1_127, mul_x1_128, mul_x1_129, mul_x1_130, mul_x1_131, mul_x1_132, mul_x1_133, mul_x1_134, mul_x1_135, mul_x1_136, mul_x1_137, mul_x1_138, mul_x1_139, mul_x1_140, mul_x1_141, mul_x1_142, mul_x1_143, mul_x1_144, mul_x1_145, mul_x1_146, mul_x1_147, mul_x1_148, mul_x1_149, mul_x1_150, mul_x1_151, mul_x1_152, mul_x1_153, mul_x1_154, mul_x1_155, mul_x1_156, mul_x1_157, mul_x1_158, mul_x1_159, mul_x1_160, mul_x1_161, mul_x1_162, mul_x1_163, mul_x1_164, mul_x1_165, mul_x1_166, mul_x1_167, mul_x1_168, mul_x1_169, mul_x1_170, mul_x1_171, mul_x1_172, mul_x1_173, mul_x1_174, mul_x1_175, mul_x1_176, mul_x1_177, mul_x1_178, mul_x1_179, mul_x1_180, mul_x1_181, mul_x1_182, mul_x1_183, mul_x1_184, mul_x1_185, mul_x1_186, mul_x1_187, mul_x1_188, mul_x1_189, mul_x1_190, mul_x1_191, mul_x1_192, mul_x1_193, mul_x1_194, mul_x1_195, mul_x1_196, mul_x1_197, mul_x1_198, mul_x1_199, mul_x1_200, mul_x1_201, mul_x1_202, mul_x1_203, mul_x1_204, mul_x1_205, mul_x1_206, mul_x1_207, mul_x1_208, mul_x1_209, mul_x1_210, mul_x1_211, mul_x1_212, mul_x1_213, mul_x1_214, mul_x1_215, mul_x1_216, mul_x1_217, mul_x1_218, mul_x1_219, mul_x1_220, mul_x1_221, mul_x1_222, mul_x1_223, mul_x1_224, mul_x1_225, mul_x1_226, mul_x1_227, mul_x1_228, mul_x1_229, mul_x1_230, mul_x1_231, mul_x1_232, mul_x1_233, mul_x1_234, mul_x1_235, mul_x1_236, mul_x1_237, mul_x1_238, mul_x1_239, mul_x1_240, mul_x1_241, mul_x1_242, mul_x1_243, mul_x1_244, mul_x1_245, mul_x1_246, mul_x1_247, mul_x1_248, mul_x1_249, mul_x1_250, mul_x1_251, mul_x1_252, mul_x1_253, mul_x1_254, mul_x1_255 */

static void
raidz_rec_pqr_abd(void **t, const size_t tsize, void **c,
    const unsigned * const mul)
{
	...

	for (; x < xend; x += REC_PQR_STRIDE, y += REC_PQR_STRIDE,
	    z += REC_PQR_STRIDE, p += REC_PQR_STRIDE, q += REC_PQR_STRIDE,
	    r += REC_PQR_STRIDE) {
		LOAD(x, REC_PQR_X);
		LOAD(y, REC_PQR_Y);
		LOAD(z, REC_PQR_Z);

		XOR_ACC(p, REC_PQR_X);
		XOR_ACC(q, REC_PQR_Y);
		XOR_ACC(r, REC_PQR_Z);

		/* Save Pxyz and Qxyz */
		COPY(REC_PQR_X, REC_PQR_XS);
		COPY(REC_PQR_Y, REC_PQR_YS);

		/* Calc X */
		MUL(mul[MUL_PQR_XP], REC_PQR_X);	/* Xp = Pxyz * xp   */
		MUL(mul[MUL_PQR_XQ], REC_PQR_Y);	/* Xq = Qxyz * xq   */
		...
        }
    ...
}

static const mul_fn_ptr_t __attribute__((aligned(256)))
gf_x1_mul_fns[256] = {
	mul_x1_0, mul_x1_1, mul_x1_2, mul_x1_3, mul_x1_4, mul_x1_5,
	mul_x1_6, mul_x1_7, mul_x1_8, mul_x1_9, mul_x1_10, mul_x1_11,
	mul_x1_12, mul_x1_13, mul_x1_14, mul_x1_15, mul_x1_16, mul_x1_17,
	mul_x1_18, mul_x1_19, mul_x1_20, mul_x1_21, mul_x1_22, mul_x1_23,
	mul_x1_24, mul_x1_25, mul_x1_26, mul_x1_27, mul_x1_28, mul_x1_29,
	mul_x1_30, mul_x1_31, mul_x1_32, mul_x1_33, mul_x1_34, mul_x1_35,
	mul_x1_36, mul_x1_37, mul_x1_38, mul_x1_39, mul_x1_40, mul_x1_41,
	mul_x1_42, mul_x1_43, mul_x1_44, mul_x1_45, mul_x1_46, mul_x1_47,
	mul_x1_48, mul_x1_49, mul_x1_50, mul_x1_51, mul_x1_52, mul_x1_53,
	mul_x1_54, mul_x1_55, mul_x1_56, mul_x1_57, mul_x1_58, mul_x1_59,
	mul_x1_60, mul_x1_61, mul_x1_62, mul_x1_63, mul_x1_64, mul_x1_65,
	mul_x1_66, mul_x1_67, mul_x1_68, mul_x1_69, mul_x1_70, mul_x1_71,
	mul_x1_72, mul_x1_73, mul_x1_74, mul_x1_75, mul_x1_76, mul_x1_77,
	mul_x1_78, mul_x1_79, mul_x1_80, mul_x1_81, mul_x1_82, mul_x1_83,
	mul_x1_84, mul_x1_85, mul_x1_86, mul_x1_87, mul_x1_88, mul_x1_89,
	mul_x1_90, mul_x1_91, mul_x1_92, mul_x1_93, mul_x1_94, mul_x1_95,
	mul_x1_96, mul_x1_97, mul_x1_98, mul_x1_99, mul_x1_100, mul_x1_101,
	mul_x1_102, mul_x1_103, mul_x1_104, mul_x1_105, mul_x1_106, mul_x1_107,
	mul_x1_108, mul_x1_109, mul_x1_110, mul_x1_111, mul_x1_112, mul_x1_113,
	mul_x1_114, mul_x1_115, mul_x1_116, mul_x1_117, mul_x1_118, mul_x1_119,
	mul_x1_120, mul_x1_121, mul_x1_122, mul_x1_123, mul_x1_124, mul_x1_125,
	mul_x1_126, mul_x1_127, mul_x1_128, mul_x1_129, mul_x1_130, mul_x1_131,
	mul_x1_132, mul_x1_133, mul_x1_134, mul_x1_135, mul_x1_136, mul_x1_137,
	mul_x1_138, mul_x1_139, mul_x1_140, mul_x1_141, mul_x1_142, mul_x1_143,
	mul_x1_144, mul_x1_145, mul_x1_146, mul_x1_147, mul_x1_148, mul_x1_149,
	mul_x1_150, mul_x1_151, mul_x1_152, mul_x1_153, mul_x1_154, mul_x1_155,
	mul_x1_156, mul_x1_157, mul_x1_158, mul_x1_159, mul_x1_160, mul_x1_161,
	mul_x1_162, mul_x1_163, mul_x1_164, mul_x1_165, mul_x1_166, mul_x1_167,
	mul_x1_168, mul_x1_169, mul_x1_170, mul_x1_171, mul_x1_172, mul_x1_173,
	mul_x1_174, mul_x1_175, mul_x1_176, mul_x1_177, mul_x1_178, mul_x1_179,
	mul_x1_180, mul_x1_181, mul_x1_182, mul_x1_183, mul_x1_184, mul_x1_185,
	mul_x1_186, mul_x1_187, mul_x1_188, mul_x1_189, mul_x1_190, mul_x1_191,
	mul_x1_192, mul_x1_193, mul_x1_194, mul_x1_195, mul_x1_196, mul_x1_197,
	mul_x1_198, mul_x1_199, mul_x1_200, mul_x1_201, mul_x1_202, mul_x1_203,
	mul_x1_204, mul_x1_205, mul_x1_206, mul_x1_207, mul_x1_208, mul_x1_209,
	mul_x1_210, mul_x1_211, mul_x1_212, mul_x1_213, mul_x1_214, mul_x1_215,
	mul_x1_216, mul_x1_217, mul_x1_218, mul_x1_219, mul_x1_220, mul_x1_221,
	mul_x1_222, mul_x1_223, mul_x1_224, mul_x1_225, mul_x1_226, mul_x1_227,
	mul_x1_228, mul_x1_229, mul_x1_230, mul_x1_231, mul_x1_232, mul_x1_233,
	mul_x1_234, mul_x1_235, mul_x1_236, mul_x1_237, mul_x1_238, mul_x1_239,
	mul_x1_240, mul_x1_241, mul_x1_242, mul_x1_243, mul_x1_244, mul_x1_245,
	mul_x1_246, mul_x1_247, mul_x1_248, mul_x1_249, mul_x1_250, mul_x1_251,
	mul_x1_252, mul_x1_253, mul_x1_254, mul_x1_255
};

#define	MUL(c, r...) 							\
{									\
	switch (REG_CNT(r)) {						\
	case 2:								\
		COPY(r, _mul_x2_in);					\
		gf_x2_mul_fns[c]();					\
		COPY(_mul_x2_acc, r);					\
		break;							\
	case 1:								\
		COPY(r, _mul_x1_in);					\
		gf_x1_mul_fns[c]();					\
		COPY(_mul_x1_acc, r);					\
		break;							\
	default:							\
		VERIFY(0);						\
	}								\
}




/* Stub implementation for mul_x1_0 */
void mul_x1_0(void) {}



/* Stub implementation for mul_x1_1 */
void mul_x1_1(void) {}



/* Stub implementation for mul_x1_2 */
void mul_x1_2(void) {}



/* Stub implementation for mul_x1_3 */
void mul_x1_3(void) {}



/* Stub implementation for mul_x1_4 */
void mul_x1_4(void) {}



/* Stub implementation for mul_x1_5 */
void mul_x1_5(void) {}



/* Stub implementation for mul_x1_6 */
void mul_x1_6(void) {}



/* Stub implementation for mul_x1_7 */
void mul_x1_7(void) {}



/* Stub implementation for mul_x1_8 */
void mul_x1_8(void) {}



/* Stub implementation for mul_x1_9 */
void mul_x1_9(void) {}



/* Stub implementation for mul_x1_10 */
void mul_x1_10(void) {}



/* Stub implementation for mul_x1_11 */
void mul_x1_11(void) {}



/* Stub implementation for mul_x1_12 */
void mul_x1_12(void) {}



/* Stub implementation for mul_x1_13 */
void mul_x1_13(void) {}



/* Stub implementation for mul_x1_14 */
void mul_x1_14(void) {}



/* Stub implementation for mul_x1_15 */
void mul_x1_15(void) {}



/* Stub implementation for mul_x1_16 */
void mul_x1_16(void) {}



/* Stub implementation for mul_x1_17 */
void mul_x1_17(void) {}



/* Stub implementation for mul_x1_18 */
void mul_x1_18(void) {}



/* Stub implementation for mul_x1_19 */
void mul_x1_19(void) {}



/* Stub implementation for mul_x1_20 */
void mul_x1_20(void) {}



/* Stub implementation for mul_x1_21 */
void mul_x1_21(void) {}



/* Stub implementation for mul_x1_22 */
void mul_x1_22(void) {}



/* Stub implementation for mul_x1_23 */
void mul_x1_23(void) {}



/* Stub implementation for mul_x1_24 */
void mul_x1_24(void) {}



/* Stub implementation for mul_x1_25 */
void mul_x1_25(void) {}



/* Stub implementation for mul_x1_26 */
void mul_x1_26(void) {}



/* Stub implementation for mul_x1_27 */
void mul_x1_27(void) {}



/* Stub implementation for mul_x1_28 */
void mul_x1_28(void) {}



/* Stub implementation for mul_x1_29 */
void mul_x1_29(void) {}



/* Stub implementation for mul_x1_30 */
void mul_x1_30(void) {}



/* Stub implementation for mul_x1_31 */
void mul_x1_31(void) {}



/* Stub implementation for mul_x1_32 */
void mul_x1_32(void) {}



/* Stub implementation for mul_x1_33 */
void mul_x1_33(void) {}



/* Stub implementation for mul_x1_34 */
void mul_x1_34(void) {}



/* Stub implementation for mul_x1_35 */
void mul_x1_35(void) {}



/* Stub implementation for mul_x1_36 */
void mul_x1_36(void) {}



/* Stub implementation for mul_x1_37 */
void mul_x1_37(void) {}



/* Stub implementation for mul_x1_38 */
void mul_x1_38(void) {}



/* Stub implementation for mul_x1_39 */
void mul_x1_39(void) {}



/* Stub implementation for mul_x1_40 */
void mul_x1_40(void) {}



/* Stub implementation for mul_x1_41 */
void mul_x1_41(void) {}



/* Stub implementation for mul_x1_42 */
void mul_x1_42(void) {}



/* Stub implementation for mul_x1_43 */
void mul_x1_43(void) {}



/* Stub implementation for mul_x1_44 */
void mul_x1_44(void) {}



/* Stub implementation for mul_x1_45 */
void mul_x1_45(void) {}



/* Stub implementation for mul_x1_46 */
void mul_x1_46(void) {}



/* Stub implementation for mul_x1_47 */
void mul_x1_47(void) {}



/* Stub implementation for mul_x1_48 */
void mul_x1_48(void) {}



/* Stub implementation for mul_x1_49 */
void mul_x1_49(void) {}



/* Stub implementation for mul_x1_50 */
void mul_x1_50(void) {}



/* Stub implementation for mul_x1_51 */
void mul_x1_51(void) {}



/* Stub implementation for mul_x1_52 */
void mul_x1_52(void) {}



/* Stub implementation for mul_x1_53 */
void mul_x1_53(void) {}



/* Stub implementation for mul_x1_54 */
void mul_x1_54(void) {}



/* Stub implementation for mul_x1_55 */
void mul_x1_55(void) {}



/* Stub implementation for mul_x1_56 */
void mul_x1_56(void) {}



/* Stub implementation for mul_x1_57 */
void mul_x1_57(void) {}



/* Stub implementation for mul_x1_58 */
void mul_x1_58(void) {}



/* Stub implementation for mul_x1_59 */
void mul_x1_59(void) {}



/* Stub implementation for mul_x1_60 */
void mul_x1_60(void) {}



/* Stub implementation for mul_x1_61 */
void mul_x1_61(void) {}



/* Stub implementation for mul_x1_62 */
void mul_x1_62(void) {}



/* Stub implementation for mul_x1_63 */
void mul_x1_63(void) {}



/* Stub implementation for mul_x1_64 */
void mul_x1_64(void) {}



/* Stub implementation for mul_x1_65 */
void mul_x1_65(void) {}



/* Stub implementation for mul_x1_66 */
void mul_x1_66(void) {}



/* Stub implementation for mul_x1_67 */
void mul_x1_67(void) {}



/* Stub implementation for mul_x1_68 */
void mul_x1_68(void) {}



/* Stub implementation for mul_x1_69 */
void mul_x1_69(void) {}



/* Stub implementation for mul_x1_70 */
void mul_x1_70(void) {}



/* Stub implementation for mul_x1_71 */
void mul_x1_71(void) {}



/* Stub implementation for mul_x1_72 */
void mul_x1_72(void) {}



/* Stub implementation for mul_x1_73 */
void mul_x1_73(void) {}



/* Stub implementation for mul_x1_74 */
void mul_x1_74(void) {}



/* Stub implementation for mul_x1_75 */
void mul_x1_75(void) {}



/* Stub implementation for mul_x1_76 */
void mul_x1_76(void) {}



/* Stub implementation for mul_x1_77 */
void mul_x1_77(void) {}



/* Stub implementation for mul_x1_78 */
void mul_x1_78(void) {}



/* Stub implementation for mul_x1_79 */
void mul_x1_79(void) {}



/* Stub implementation for mul_x1_80 */
void mul_x1_80(void) {}



/* Stub implementation for mul_x1_81 */
void mul_x1_81(void) {}



/* Stub implementation for mul_x1_82 */
void mul_x1_82(void) {}



/* Stub implementation for mul_x1_83 */
void mul_x1_83(void) {}



/* Stub implementation for mul_x1_84 */
void mul_x1_84(void) {}



/* Stub implementation for mul_x1_85 */
void mul_x1_85(void) {}



/* Stub implementation for mul_x1_86 */
void mul_x1_86(void) {}



/* Stub implementation for mul_x1_87 */
void mul_x1_87(void) {}



/* Stub implementation for mul_x1_88 */
void mul_x1_88(void) {}



/* Stub implementation for mul_x1_89 */
void mul_x1_89(void) {}



/* Stub implementation for mul_x1_90 */
void mul_x1_90(void) {}



/* Stub implementation for mul_x1_91 */
void mul_x1_91(void) {}



/* Stub implementation for mul_x1_92 */
void mul_x1_92(void) {}



/* Stub implementation for mul_x1_93 */
void mul_x1_93(void) {}



/* Stub implementation for mul_x1_94 */
void mul_x1_94(void) {}



/* Stub implementation for mul_x1_95 */
void mul_x1_95(void) {}



/* Stub implementation for mul_x1_96 */
void mul_x1_96(void) {}



/* Stub implementation for mul_x1_97 */
void mul_x1_97(void) {}



/* Stub implementation for mul_x1_98 */
void mul_x1_98(void) {}



/* Stub implementation for mul_x1_99 */
void mul_x1_99(void) {}



/* Stub implementation for mul_x1_100 */
void mul_x1_100(void) {}



/* Stub implementation for mul_x1_101 */
void mul_x1_101(void) {}



/* Stub implementation for mul_x1_102 */
void mul_x1_102(void) {}



/* Stub implementation for mul_x1_103 */
void mul_x1_103(void) {}



/* Stub implementation for mul_x1_104 */
void mul_x1_104(void) {}



/* Stub implementation for mul_x1_105 */
void mul_x1_105(void) {}



/* Stub implementation for mul_x1_106 */
void mul_x1_106(void) {}



/* Stub implementation for mul_x1_107 */
void mul_x1_107(void) {}



/* Stub implementation for mul_x1_108 */
void mul_x1_108(void) {}



/* Stub implementation for mul_x1_109 */
void mul_x1_109(void) {}



/* Stub implementation for mul_x1_110 */
void mul_x1_110(void) {}



/* Stub implementation for mul_x1_111 */
void mul_x1_111(void) {}



/* Stub implementation for mul_x1_112 */
void mul_x1_112(void) {}



/* Stub implementation for mul_x1_113 */
void mul_x1_113(void) {}



/* Stub implementation for mul_x1_114 */
void mul_x1_114(void) {}



/* Stub implementation for mul_x1_115 */
void mul_x1_115(void) {}



/* Stub implementation for mul_x1_116 */
void mul_x1_116(void) {}



/* Stub implementation for mul_x1_117 */
void mul_x1_117(void) {}



/* Stub implementation for mul_x1_118 */
void mul_x1_118(void) {}



/* Stub implementation for mul_x1_119 */
void mul_x1_119(void) {}



/* Stub implementation for mul_x1_120 */
void mul_x1_120(void) {}



/* Stub implementation for mul_x1_121 */
void mul_x1_121(void) {}



/* Stub implementation for mul_x1_122 */
void mul_x1_122(void) {}



/* Stub implementation for mul_x1_123 */
void mul_x1_123(void) {}



/* Stub implementation for mul_x1_124 */
void mul_x1_124(void) {}



/* Stub implementation for mul_x1_125 */
void mul_x1_125(void) {}



/* Stub implementation for mul_x1_126 */
void mul_x1_126(void) {}



/* Stub implementation for mul_x1_127 */
void mul_x1_127(void) {}



/* Stub implementation for mul_x1_128 */
void mul_x1_128(void) {}



/* Stub implementation for mul_x1_129 */
void mul_x1_129(void) {}



/* Stub implementation for mul_x1_130 */
void mul_x1_130(void) {}



/* Stub implementation for mul_x1_131 */
void mul_x1_131(void) {}



/* Stub implementation for mul_x1_132 */
void mul_x1_132(void) {}



/* Stub implementation for mul_x1_133 */
void mul_x1_133(void) {}



/* Stub implementation for mul_x1_134 */
void mul_x1_134(void) {}



/* Stub implementation for mul_x1_135 */
void mul_x1_135(void) {}



/* Stub implementation for mul_x1_136 */
void mul_x1_136(void) {}



/* Stub implementation for mul_x1_137 */
void mul_x1_137(void) {}



/* Stub implementation for mul_x1_138 */
void mul_x1_138(void) {}



/* Stub implementation for mul_x1_139 */
void mul_x1_139(void) {}



/* Stub implementation for mul_x1_140 */
void mul_x1_140(void) {}



/* Stub implementation for mul_x1_141 */
void mul_x1_141(void) {}



/* Stub implementation for mul_x1_142 */
void mul_x1_142(void) {}



/* Stub implementation for mul_x1_143 */
void mul_x1_143(void) {}



/* Stub implementation for mul_x1_144 */
void mul_x1_144(void) {}



/* Stub implementation for mul_x1_145 */
void mul_x1_145(void) {}



/* Stub implementation for mul_x1_146 */
void mul_x1_146(void) {}



/* Stub implementation for mul_x1_147 */
void mul_x1_147(void) {}



/* Stub implementation for mul_x1_148 */
void mul_x1_148(void) {}



/* Stub implementation for mul_x1_149 */
void mul_x1_149(void) {}



/* Stub implementation for mul_x1_150 */
void mul_x1_150(void) {}



/* Stub implementation for mul_x1_151 */
void mul_x1_151(void) {}



/* Stub implementation for mul_x1_152 */
void mul_x1_152(void) {}



/* Stub implementation for mul_x1_153 */
void mul_x1_153(void) {}



/* Stub implementation for mul_x1_154 */
void mul_x1_154(void) {}



/* Stub implementation for mul_x1_155 */
void mul_x1_155(void) {}



/* Stub implementation for mul_x1_156 */
void mul_x1_156(void) {}



/* Stub implementation for mul_x1_157 */
void mul_x1_157(void) {}



/* Stub implementation for mul_x1_158 */
void mul_x1_158(void) {}



/* Stub implementation for mul_x1_159 */
void mul_x1_159(void) {}



/* Stub implementation for mul_x1_160 */
void mul_x1_160(void) {}



/* Stub implementation for mul_x1_161 */
void mul_x1_161(void) {}



/* Stub implementation for mul_x1_162 */
void mul_x1_162(void) {}



/* Stub implementation for mul_x1_163 */
void mul_x1_163(void) {}



/* Stub implementation for mul_x1_164 */
void mul_x1_164(void) {}



/* Stub implementation for mul_x1_165 */
void mul_x1_165(void) {}



/* Stub implementation for mul_x1_166 */
void mul_x1_166(void) {}



/* Stub implementation for mul_x1_167 */
void mul_x1_167(void) {}



/* Stub implementation for mul_x1_168 */
void mul_x1_168(void) {}



/* Stub implementation for mul_x1_169 */
void mul_x1_169(void) {}



/* Stub implementation for mul_x1_170 */
void mul_x1_170(void) {}



/* Stub implementation for mul_x1_171 */
void mul_x1_171(void) {}



/* Stub implementation for mul_x1_172 */
void mul_x1_172(void) {}



/* Stub implementation for mul_x1_173 */
void mul_x1_173(void) {}



/* Stub implementation for mul_x1_174 */
void mul_x1_174(void) {}



/* Stub implementation for mul_x1_175 */
void mul_x1_175(void) {}



/* Stub implementation for mul_x1_176 */
void mul_x1_176(void) {}



/* Stub implementation for mul_x1_177 */
void mul_x1_177(void) {}



/* Stub implementation for mul_x1_178 */
void mul_x1_178(void) {}



/* Stub implementation for mul_x1_179 */
void mul_x1_179(void) {}



/* Stub implementation for mul_x1_180 */
void mul_x1_180(void) {}



/* Stub implementation for mul_x1_181 */
void mul_x1_181(void) {}



/* Stub implementation for mul_x1_182 */
void mul_x1_182(void) {}



/* Stub implementation for mul_x1_183 */
void mul_x1_183(void) {}



/* Stub implementation for mul_x1_184 */
void mul_x1_184(void) {}



/* Stub implementation for mul_x1_185 */
void mul_x1_185(void) {}



/* Stub implementation for mul_x1_186 */
void mul_x1_186(void) {}



/* Stub implementation for mul_x1_187 */
void mul_x1_187(void) {}



/* Stub implementation for mul_x1_188 */
void mul_x1_188(void) {}



/* Stub implementation for mul_x1_189 */
void mul_x1_189(void) {}



/* Stub implementation for mul_x1_190 */
void mul_x1_190(void) {}



/* Stub implementation for mul_x1_191 */
void mul_x1_191(void) {}



/* Stub implementation for mul_x1_192 */
void mul_x1_192(void) {}



/* Stub implementation for mul_x1_193 */
void mul_x1_193(void) {}



/* Stub implementation for mul_x1_194 */
void mul_x1_194(void) {}



/* Stub implementation for mul_x1_195 */
void mul_x1_195(void) {}



/* Stub implementation for mul_x1_196 */
void mul_x1_196(void) {}



/* Stub implementation for mul_x1_197 */
void mul_x1_197(void) {}



/* Stub implementation for mul_x1_198 */
void mul_x1_198(void) {}



/* Stub implementation for mul_x1_199 */
void mul_x1_199(void) {}



/* Stub implementation for mul_x1_200 */
void mul_x1_200(void) {}



/* Stub implementation for mul_x1_201 */
void mul_x1_201(void) {}



/* Stub implementation for mul_x1_202 */
void mul_x1_202(void) {}



/* Stub implementation for mul_x1_203 */
void mul_x1_203(void) {}



/* Stub implementation for mul_x1_204 */
void mul_x1_204(void) {}



/* Stub implementation for mul_x1_205 */
void mul_x1_205(void) {}



/* Stub implementation for mul_x1_206 */
void mul_x1_206(void) {}



/* Stub implementation for mul_x1_207 */
void mul_x1_207(void) {}



/* Stub implementation for mul_x1_208 */
void mul_x1_208(void) {}



/* Stub implementation for mul_x1_209 */
void mul_x1_209(void) {}



/* Stub implementation for mul_x1_210 */
void mul_x1_210(void) {}



/* Stub implementation for mul_x1_211 */
void mul_x1_211(void) {}



/* Stub implementation for mul_x1_212 */
void mul_x1_212(void) {}



/* Stub implementation for mul_x1_213 */
void mul_x1_213(void) {}



/* Stub implementation for mul_x1_214 */
void mul_x1_214(void) {}



/* Stub implementation for mul_x1_215 */
void mul_x1_215(void) {}



/* Stub implementation for mul_x1_216 */
void mul_x1_216(void) {}



/* Stub implementation for mul_x1_217 */
void mul_x1_217(void) {}



/* Stub implementation for mul_x1_218 */
void mul_x1_218(void) {}



/* Stub implementation for mul_x1_219 */
void mul_x1_219(void) {}



/* Stub implementation for mul_x1_220 */
void mul_x1_220(void) {}



/* Stub implementation for mul_x1_221 */
void mul_x1_221(void) {}



/* Stub implementation for mul_x1_222 */
void mul_x1_222(void) {}



/* Stub implementation for mul_x1_223 */
void mul_x1_223(void) {}



/* Stub implementation for mul_x1_224 */
void mul_x1_224(void) {}



/* Stub implementation for mul_x1_225 */
void mul_x1_225(void) {}



/* Stub implementation for mul_x1_226 */
void mul_x1_226(void) {}



/* Stub implementation for mul_x1_227 */
void mul_x1_227(void) {}



/* Stub implementation for mul_x1_228 */
void mul_x1_228(void) {}



/* Stub implementation for mul_x1_229 */
void mul_x1_229(void) {}



/* Stub implementation for mul_x1_230 */
void mul_x1_230(void) {}



/* Stub implementation for mul_x1_231 */
void mul_x1_231(void) {}



/* Stub implementation for mul_x1_232 */
void mul_x1_232(void) {}



/* Stub implementation for mul_x1_233 */
void mul_x1_233(void) {}



/* Stub implementation for mul_x1_234 */
void mul_x1_234(void) {}



/* Stub implementation for mul_x1_235 */
void mul_x1_235(void) {}



/* Stub implementation for mul_x1_236 */
void mul_x1_236(void) {}



/* Stub implementation for mul_x1_237 */
void mul_x1_237(void) {}



/* Stub implementation for mul_x1_238 */
void mul_x1_238(void) {}



/* Stub implementation for mul_x1_239 */
void mul_x1_239(void) {}



/* Stub implementation for mul_x1_240 */
void mul_x1_240(void) {}



/* Stub implementation for mul_x1_241 */
void mul_x1_241(void) {}



/* Stub implementation for mul_x1_242 */
void mul_x1_242(void) {}



/* Stub implementation for mul_x1_243 */
void mul_x1_243(void) {}



/* Stub implementation for mul_x1_244 */
void mul_x1_244(void) {}



/* Stub implementation for mul_x1_245 */
void mul_x1_245(void) {}



/* Stub implementation for mul_x1_246 */
void mul_x1_246(void) {}



/* Stub implementation for mul_x1_247 */
void mul_x1_247(void) {}



/* Stub implementation for mul_x1_248 */
void mul_x1_248(void) {}



/* Stub implementation for mul_x1_249 */
void mul_x1_249(void) {}



/* Stub implementation for mul_x1_250 */
void mul_x1_250(void) {}



/* Stub implementation for mul_x1_251 */
void mul_x1_251(void) {}



/* Stub implementation for mul_x1_252 */
void mul_x1_252(void) {}



/* Stub implementation for mul_x1_253 */
void mul_x1_253(void) {}



/* Stub implementation for mul_x1_254 */
void mul_x1_254(void) {}



/* Stub implementation for mul_x1_255 */
void mul_x1_255(void) {}
