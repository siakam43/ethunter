/*
 * et_bench fixture: fnptr-global-array/example_6
 *
 * Scenario: ZFS RAID-Z Reed-Solomon GF(2^8) multiplication dispatch.
 * A global array gf_x1_mul_fns[256] maps byte values to GF multiplication
 * functions. raidz_rec_pqr_abd() indexes gf_x1_mul_fns[c] and calls through.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* --- Types --- */

typedef void (*mul_fn_ptr_t)(void);

/* --- Target functions: 256 mul_x1_N functions --- */

static void mul_x1_0(void) {}
static void mul_x1_1(void) {}
static void mul_x1_2(void) {}
static void mul_x1_3(void) {}
static void mul_x1_4(void) {}
static void mul_x1_5(void) {}
static void mul_x1_6(void) {}
static void mul_x1_7(void) {}
static void mul_x1_8(void) {}
static void mul_x1_9(void) {}
static void mul_x1_10(void) {}
static void mul_x1_11(void) {}
static void mul_x1_12(void) {}
static void mul_x1_13(void) {}
static void mul_x1_14(void) {}
static void mul_x1_15(void) {}
static void mul_x1_16(void) {}
static void mul_x1_17(void) {}
static void mul_x1_18(void) {}
static void mul_x1_19(void) {}
static void mul_x1_20(void) {}
static void mul_x1_21(void) {}
static void mul_x1_22(void) {}
static void mul_x1_23(void) {}
static void mul_x1_24(void) {}
static void mul_x1_25(void) {}
static void mul_x1_26(void) {}
static void mul_x1_27(void) {}
static void mul_x1_28(void) {}
static void mul_x1_29(void) {}
static void mul_x1_30(void) {}
static void mul_x1_31(void) {}
static void mul_x1_32(void) {}
static void mul_x1_33(void) {}
static void mul_x1_34(void) {}
static void mul_x1_35(void) {}
static void mul_x1_36(void) {}
static void mul_x1_37(void) {}
static void mul_x1_38(void) {}
static void mul_x1_39(void) {}
static void mul_x1_40(void) {}
static void mul_x1_41(void) {}
static void mul_x1_42(void) {}
static void mul_x1_43(void) {}
static void mul_x1_44(void) {}
static void mul_x1_45(void) {}
static void mul_x1_46(void) {}
static void mul_x1_47(void) {}
static void mul_x1_48(void) {}
static void mul_x1_49(void) {}
static void mul_x1_50(void) {}
static void mul_x1_51(void) {}
static void mul_x1_52(void) {}
static void mul_x1_53(void) {}
static void mul_x1_54(void) {}
static void mul_x1_55(void) {}
static void mul_x1_56(void) {}
static void mul_x1_57(void) {}
static void mul_x1_58(void) {}
static void mul_x1_59(void) {}
static void mul_x1_60(void) {}
static void mul_x1_61(void) {}
static void mul_x1_62(void) {}
static void mul_x1_63(void) {}
static void mul_x1_64(void) {}
static void mul_x1_65(void) {}
static void mul_x1_66(void) {}
static void mul_x1_67(void) {}
static void mul_x1_68(void) {}
static void mul_x1_69(void) {}
static void mul_x1_70(void) {}
static void mul_x1_71(void) {}
static void mul_x1_72(void) {}
static void mul_x1_73(void) {}
static void mul_x1_74(void) {}
static void mul_x1_75(void) {}
static void mul_x1_76(void) {}
static void mul_x1_77(void) {}
static void mul_x1_78(void) {}
static void mul_x1_79(void) {}
static void mul_x1_80(void) {}
static void mul_x1_81(void) {}
static void mul_x1_82(void) {}
static void mul_x1_83(void) {}
static void mul_x1_84(void) {}
static void mul_x1_85(void) {}
static void mul_x1_86(void) {}
static void mul_x1_87(void) {}
static void mul_x1_88(void) {}
static void mul_x1_89(void) {}
static void mul_x1_90(void) {}
static void mul_x1_91(void) {}
static void mul_x1_92(void) {}
static void mul_x1_93(void) {}
static void mul_x1_94(void) {}
static void mul_x1_95(void) {}
static void mul_x1_96(void) {}
static void mul_x1_97(void) {}
static void mul_x1_98(void) {}
static void mul_x1_99(void) {}
static void mul_x1_100(void) {}
static void mul_x1_101(void) {}
static void mul_x1_102(void) {}
static void mul_x1_103(void) {}
static void mul_x1_104(void) {}
static void mul_x1_105(void) {}
static void mul_x1_106(void) {}
static void mul_x1_107(void) {}
static void mul_x1_108(void) {}
static void mul_x1_109(void) {}
static void mul_x1_110(void) {}
static void mul_x1_111(void) {}
static void mul_x1_112(void) {}
static void mul_x1_113(void) {}
static void mul_x1_114(void) {}
static void mul_x1_115(void) {}
static void mul_x1_116(void) {}
static void mul_x1_117(void) {}
static void mul_x1_118(void) {}
static void mul_x1_119(void) {}
static void mul_x1_120(void) {}
static void mul_x1_121(void) {}
static void mul_x1_122(void) {}
static void mul_x1_123(void) {}
static void mul_x1_124(void) {}
static void mul_x1_125(void) {}
static void mul_x1_126(void) {}
static void mul_x1_127(void) {}
static void mul_x1_128(void) {}
static void mul_x1_129(void) {}
static void mul_x1_130(void) {}
static void mul_x1_131(void) {}
static void mul_x1_132(void) {}
static void mul_x1_133(void) {}
static void mul_x1_134(void) {}
static void mul_x1_135(void) {}
static void mul_x1_136(void) {}
static void mul_x1_137(void) {}
static void mul_x1_138(void) {}
static void mul_x1_139(void) {}
static void mul_x1_140(void) {}
static void mul_x1_141(void) {}
static void mul_x1_142(void) {}
static void mul_x1_143(void) {}
static void mul_x1_144(void) {}
static void mul_x1_145(void) {}
static void mul_x1_146(void) {}
static void mul_x1_147(void) {}
static void mul_x1_148(void) {}
static void mul_x1_149(void) {}
static void mul_x1_150(void) {}
static void mul_x1_151(void) {}
static void mul_x1_152(void) {}
static void mul_x1_153(void) {}
static void mul_x1_154(void) {}
static void mul_x1_155(void) {}
static void mul_x1_156(void) {}
static void mul_x1_157(void) {}
static void mul_x1_158(void) {}
static void mul_x1_159(void) {}
static void mul_x1_160(void) {}
static void mul_x1_161(void) {}
static void mul_x1_162(void) {}
static void mul_x1_163(void) {}
static void mul_x1_164(void) {}
static void mul_x1_165(void) {}
static void mul_x1_166(void) {}
static void mul_x1_167(void) {}
static void mul_x1_168(void) {}
static void mul_x1_169(void) {}
static void mul_x1_170(void) {}
static void mul_x1_171(void) {}
static void mul_x1_172(void) {}
static void mul_x1_173(void) {}
static void mul_x1_174(void) {}
static void mul_x1_175(void) {}
static void mul_x1_176(void) {}
static void mul_x1_177(void) {}
static void mul_x1_178(void) {}
static void mul_x1_179(void) {}
static void mul_x1_180(void) {}
static void mul_x1_181(void) {}
static void mul_x1_182(void) {}
static void mul_x1_183(void) {}
static void mul_x1_184(void) {}
static void mul_x1_185(void) {}
static void mul_x1_186(void) {}
static void mul_x1_187(void) {}
static void mul_x1_188(void) {}
static void mul_x1_189(void) {}
static void mul_x1_190(void) {}
static void mul_x1_191(void) {}
static void mul_x1_192(void) {}
static void mul_x1_193(void) {}
static void mul_x1_194(void) {}
static void mul_x1_195(void) {}
static void mul_x1_196(void) {}
static void mul_x1_197(void) {}
static void mul_x1_198(void) {}
static void mul_x1_199(void) {}
static void mul_x1_200(void) {}
static void mul_x1_201(void) {}
static void mul_x1_202(void) {}
static void mul_x1_203(void) {}
static void mul_x1_204(void) {}
static void mul_x1_205(void) {}
static void mul_x1_206(void) {}
static void mul_x1_207(void) {}
static void mul_x1_208(void) {}
static void mul_x1_209(void) {}
static void mul_x1_210(void) {}
static void mul_x1_211(void) {}
static void mul_x1_212(void) {}
static void mul_x1_213(void) {}
static void mul_x1_214(void) {}
static void mul_x1_215(void) {}
static void mul_x1_216(void) {}
static void mul_x1_217(void) {}
static void mul_x1_218(void) {}
static void mul_x1_219(void) {}
static void mul_x1_220(void) {}
static void mul_x1_221(void) {}
static void mul_x1_222(void) {}
static void mul_x1_223(void) {}
static void mul_x1_224(void) {}
static void mul_x1_225(void) {}
static void mul_x1_226(void) {}
static void mul_x1_227(void) {}
static void mul_x1_228(void) {}
static void mul_x1_229(void) {}
static void mul_x1_230(void) {}
static void mul_x1_231(void) {}
static void mul_x1_232(void) {}
static void mul_x1_233(void) {}
static void mul_x1_234(void) {}
static void mul_x1_235(void) {}
static void mul_x1_236(void) {}
static void mul_x1_237(void) {}
static void mul_x1_238(void) {}
static void mul_x1_239(void) {}
static void mul_x1_240(void) {}
static void mul_x1_241(void) {}
static void mul_x1_242(void) {}
static void mul_x1_243(void) {}
static void mul_x1_244(void) {}
static void mul_x1_245(void) {}
static void mul_x1_246(void) {}
static void mul_x1_247(void) {}
static void mul_x1_248(void) {}
static void mul_x1_249(void) {}
static void mul_x1_250(void) {}
static void mul_x1_251(void) {}
static void mul_x1_252(void) {}
static void mul_x1_253(void) {}
static void mul_x1_254(void) {}
static void mul_x1_255(void) {}

/* --- Global function pointer array (256 entries) --- */

static const mul_fn_ptr_t gf_x1_mul_fns[256] = {
    mul_x1_0,   mul_x1_1,   mul_x1_2,   mul_x1_3,   mul_x1_4,   mul_x1_5,
    mul_x1_6,   mul_x1_7,   mul_x1_8,   mul_x1_9,   mul_x1_10,  mul_x1_11,
    mul_x1_12,  mul_x1_13,  mul_x1_14,  mul_x1_15,  mul_x1_16,  mul_x1_17,
    mul_x1_18,  mul_x1_19,  mul_x1_20,  mul_x1_21,  mul_x1_22,  mul_x1_23,
    mul_x1_24,  mul_x1_25,  mul_x1_26,  mul_x1_27,  mul_x1_28,  mul_x1_29,
    mul_x1_30,  mul_x1_31,  mul_x1_32,  mul_x1_33,  mul_x1_34,  mul_x1_35,
    mul_x1_36,  mul_x1_37,  mul_x1_38,  mul_x1_39,  mul_x1_40,  mul_x1_41,
    mul_x1_42,  mul_x1_43,  mul_x1_44,  mul_x1_45,  mul_x1_46,  mul_x1_47,
    mul_x1_48,  mul_x1_49,  mul_x1_50,  mul_x1_51,  mul_x1_52,  mul_x1_53,
    mul_x1_54,  mul_x1_55,  mul_x1_56,  mul_x1_57,  mul_x1_58,  mul_x1_59,
    mul_x1_60,  mul_x1_61,  mul_x1_62,  mul_x1_63,  mul_x1_64,  mul_x1_65,
    mul_x1_66,  mul_x1_67,  mul_x1_68,  mul_x1_69,  mul_x1_70,  mul_x1_71,
    mul_x1_72,  mul_x1_73,  mul_x1_74,  mul_x1_75,  mul_x1_76,  mul_x1_77,
    mul_x1_78,  mul_x1_79,  mul_x1_80,  mul_x1_81,  mul_x1_82,  mul_x1_83,
    mul_x1_84,  mul_x1_85,  mul_x1_86,  mul_x1_87,  mul_x1_88,  mul_x1_89,
    mul_x1_90,  mul_x1_91,  mul_x1_92,  mul_x1_93,  mul_x1_94,  mul_x1_95,
    mul_x1_96,  mul_x1_97,  mul_x1_98,  mul_x1_99,  mul_x1_100, mul_x1_101,
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

/* --- Constants --- */

enum {
    REC_PQR_STRIDE = 16,
    REC_PQR_X = 0,
    REC_PQR_Y = 1,
    REC_PQR_Z = 2,
    REC_PQR_XS = 3,
    REC_PQR_YS = 4,
    MUL_PQR_XP = 0,
    MUL_PQR_XQ = 1,
};

/* --- Caller: indexes gf_x1_mul_fns[c] directly and calls through --- */

static void
raidz_rec_pqr_abd(void **t, const size_t tsize, void **c,
                  const unsigned *const mul)
{
    size_t x = 0, xend = tsize;
    size_t y = 0, z = 0, p = 0, q = 0, r = 0;
    (void)t; (void)tsize; (void)c;

    for (; x < xend; x += REC_PQR_STRIDE, y += REC_PQR_STRIDE,
         z += REC_PQR_STRIDE, p += REC_PQR_STRIDE, q += REC_PQR_STRIDE,
         r += REC_PQR_STRIDE) {
        /* XOR accumulations: P += X, Q += Y, R += Z */
        ((uint8_t *)t[p])[0] ^= ((uint8_t *)t[x])[0];
        ((uint8_t *)t[q])[0] ^= ((uint8_t *)t[y])[0];
        ((uint8_t *)t[r])[0] ^= ((uint8_t *)t[z])[0];

        /* Save Pxyz and Qxyz */
        ((uint8_t *)t[REC_PQR_XS])[0] = ((uint8_t *)t[x])[0];
        ((uint8_t *)t[REC_PQR_YS])[0] = ((uint8_t *)t[y])[0];

        /* Calc X - indexes gf_x1_mul_fns directly */
        gf_x1_mul_fns[mul[MUL_PQR_XP]]();	/* Xp = Pxyz * xp   */
        gf_x1_mul_fns[mul[MUL_PQR_XQ]]();	/* Xq = Qxyz * xq   */
    }
}
