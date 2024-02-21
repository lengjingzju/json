/*
 * ********** REFERENCE **********
 * URL: https://github.com/miloyip/dtoa-benchmark
 * Section: src/grisu, src/milo
 */

/*
 * Using table lookup methods to accelerate division, etc
 */
#ifndef GRISU2_USING_LUT_ACCELERATE
#define GRISU2_USING_LUT_ACCELERATE 1
#endif

#define DIY_SIGNIFICAND_SIZE        64                  /* Symbol: 1 bit, Exponent, 11 bits, Mantissa, 52 bits */
#define DP_SIGNIFICAND_SIZE         52                  /* Mantissa, 52 bits */
#define DP_EXPONENT_OFFSET          0x3FF               /* Exponent offset is 0x3FF */
#define DP_EXPONENT_MASK            0x7FF0000000000000  /* Exponent Mask, 11 bits */
#define DP_SIGNIFICAND_MASK         0x000FFFFFFFFFFFFF  /* Mantissa Mask, 52 bits */
#define DP_HIDDEN_BIT               0x0010000000000000  /* Integer bit for Mantissa */
#define D_1_LOG2_10                 0.30102999566398114 /* 1/lg(10) */

typedef struct {
    uint64_t f;
    int32_t e;
} diy_fp_t;

#if 0
static void print_powers_ten_i(void)
{
    /*
     * min value of e is -1022 - 52 - 63 = -1137;
     * max value of e is  1023 - 52 - 0 = 971;
     * Range of (Exponent_Value - Exponent_Offset) is (-1022, 1023)
     * Divisor of Mantissa is pow(2,52)
     * Range of u64 right shift is (0, 63)
     */
    int e = 0, k = 0, index = 0;
    int min = -1137, max = 971, cnt = 0;
    double dk = 0;

    printf("static const uint8_t powers_ten_i[] = {");
    for (e = min; e <= max; ++e) {
        if (cnt++ % 20 == 0)
            printf("\n    ");
        dk = (-61 - e) * D_1_LOG2_10 + 347;
        k = (int)dk;
        if (dk - k > 0.0)
            ++k;
        index = (unsigned int)((k >> 3) + 1);
        printf("%-2d", index);
        if (e != max)
            printf(", ");
    }
    printf("\n};\n\n");
}

static void print_number_lut(void)
{
    int i = 0, index = 0, max = 0;

    max = (100 >> 1) - 1;
    printf("static const uint8_t num_10_lut[] = {");
    for (i = 0; i <= max; ++i) {
        if (i % 25 == 0)
            printf("\n    ");
        index = i / 5;
        printf("%d", index);
        if (i != max)
            printf(", ");
    }
    printf("\n};\n\n");

    max = (1000 >> 2) - 1;
    printf("static const uint8_t num_100_lut[] = {");
    for (i = 0; i <= max; ++i) {
        if (i % 25 == 0)
            printf("\n    ");
        index = i / 25;
        printf("%d", index);
        if (i != max)
            printf(", ");
    }
    printf("\n};\n\n");

    max = (10000 >> 3) - 1;
    printf("static const uint8_t num_1000_lut[] = {");
    for (i = 0; i <= max; ++i) {
        if (i % 25 == 0)
            printf("\n    ");
        index = i / 125;
        printf("%d", index);
        if (i != max)
            printf(", ");
    }
    printf("\n};\n\n");
}
#endif

#define get_10_multiple(v)          (((v)<<1) + ((v)<<3))
#define get_100_multiple(v)         (((v)<<2) + ((v)<<5) + ((v)<<6))
#define get_1000_multiple(v)        (((v)<<3) + ((v)<<5) + ((v)<<6) + ((v)<<7) + ((v)<<8) + ((v)<<9))

#if GRISU2_USING_LUT_ACCELERATE
static const uint8_t num_10_lut[] = {
    0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9
};
#define get_10_quotient(v)          num_10_lut[(v)>>1]

static const uint8_t num_100_lut[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9
};
#define get_100_quotient(v)         num_100_lut[(v)>>2]

static const uint8_t num_1000_lut[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9
};
#define get_1000_quotient(v)        num_1000_lut[(v)>>3]

#else

#define get_10_quotient(v)          ((v) / 10)
#define get_100_quotient(v)         ((v) / 100)
#define get_1000_quotient(v)        ((v) / 1000)
#endif

static inline diy_fp_t cached_power(int e, int* K)
{
    // 10^-348, 10^-340, ..., 10^340
    static const uint64_t powers_ten[] = {
        0xfa8fd5a0081c0288, 0xbaaee17fa23ebf76, 0x8b16fb203055ac76, 0xcf42894a5dce35ea,
        0x9a6bb0aa55653b2d, 0xe61acf033d1a45df, 0xab70fe17c79ac6ca, 0xff77b1fcbebcdc4f,
        0xbe5691ef416bd60c, 0x8dd01fad907ffc3c, 0xd3515c2831559a83, 0x9d71ac8fada6c9b5,
        0xea9c227723ee8bcb, 0xaecc49914078536d, 0x823c12795db6ce57, 0xc21094364dfb5637,
        0x9096ea6f3848984f, 0xd77485cb25823ac7, 0xa086cfcd97bf97f4, 0xef340a98172aace5,
        0xb23867fb2a35b28e, 0x84c8d4dfd2c63f3b, 0xc5dd44271ad3cdba, 0x936b9fcebb25c996,
        0xdbac6c247d62a584, 0xa3ab66580d5fdaf6, 0xf3e2f893dec3f126, 0xb5b5ada8aaff80b8,
        0x87625f056c7c4a8b, 0xc9bcff6034c13053, 0x964e858c91ba2655, 0xdff9772470297ebd,
        0xa6dfbd9fb8e5b88f, 0xf8a95fcf88747d94, 0xb94470938fa89bcf, 0x8a08f0f8bf0f156b,
        0xcdb02555653131b6, 0x993fe2c6d07b7fac, 0xe45c10c42a2b3b06, 0xaa242499697392d3,
        0xfd87b5f28300ca0e, 0xbce5086492111aeb, 0x8cbccc096f5088cc, 0xd1b71758e219652c,
        0x9c40000000000000, 0xe8d4a51000000000, 0xad78ebc5ac620000, 0x813f3978f8940984,
        0xc097ce7bc90715b3, 0x8f7e32ce7bea5c70, 0xd5d238a4abe98068, 0x9f4f2726179a2245,
        0xed63a231d4c4fb27, 0xb0de65388cc8ada8, 0x83c7088e1aab65db, 0xc45d1df942711d9a,
        0x924d692ca61be758, 0xda01ee641a708dea, 0xa26da3999aef774a, 0xf209787bb47d6b85,
        0xb454e4a179dd1877, 0x865b86925b9bc5c2, 0xc83553c5c8965d3d, 0x952ab45cfa97a0b3,
        0xde469fbd99a05fe3, 0xa59bc234db398c25, 0xf6c69a72a3989f5c, 0xb7dcbf5354e9bece,
        0x88fcf317f22241e2, 0xcc20ce9bd35c78a5, 0x98165af37b2153df, 0xe2a0b5dc971f303a,
        0xa8d9d1535ce3b396, 0xfb9b7cd9a4a7443c, 0xbb764c4ca7a44410, 0x8bab8eefb6409c1a,
        0xd01fef10a657842c, 0x9b10a4e5e9913129, 0xe7109bfba19c0c9d, 0xac2820d9623bf429,
        0x80444b5e7aa7cf85, 0xbf21e44003acdd2d, 0x8e679c2f5e44ff8f, 0xd433179d9c8cb841,
        0x9e19db92b4e31ba9, 0xeb96bf6ebadf77d9, 0xaf87023b9bf0ee6b
    };

    static const int16_t powers_ten_e[] = {
        -1220, -1193, -1166, -1140, -1113, -1087, -1060, -1034, -1007,  -980,
         -954,  -927,  -901,  -874,  -847,  -821,  -794,  -768,  -741,  -715,
         -688,  -661,  -635,  -608,  -582,  -555,  -529,  -502,  -475,  -449,
         -422,  -396,  -369,  -343,  -316,  -289,  -263,  -236,  -210,  -183,
         -157,  -130,  -103,   -77,   -50,   -24,     3,    30,    56,    83,
          109,   136,   162,   189,   216,   242,   269,   295,   322,   348,
          375,   402,   428,   455,   481,   508,   534,   561,   588,   614,
          641,   667,   694,   720,   747,   774,   800,   827,   853,   880,
          907,   933,   960,   986,  1013,  1039,  1066
    };

#if GRISU2_USING_LUT_ACCELERATE
    /* powers_ten_i is got from print_powers_ten_i */
    static const uint8_t powers_ten_i[] = {
        84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84,
        84, 84, 84, 84, 84, 84, 84, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
        83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 82, 82, 82, 82, 82, 82, 82,
        82, 82, 82, 82, 82, 82, 82, 82, 82, 82, 82, 82, 82, 82, 82, 82, 82, 82, 82, 82,
        81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81,
        81, 81, 81, 81, 81, 81, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
        80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 79, 79, 79, 79, 79, 79, 79,
        79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79,
        78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78,
        78, 78, 78, 78, 78, 78, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
        77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 76, 76, 76, 76, 76, 76, 76,
        76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 75,
        75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75,
        75, 75, 75, 75, 75, 75, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
        74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 73, 73, 73, 73, 73, 73, 73,
        73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 72,
        72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
        72, 72, 72, 72, 72, 72, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71,
        71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 70, 70, 70, 70, 70, 70, 70, 70,
        70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 69,
        69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69,
        69, 69, 69, 69, 69, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68,
        68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 67, 67, 67, 67, 67, 67, 67, 67,
        67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 66,
        66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
        66, 66, 66, 66, 66, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
        65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 63, 63,
        63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
        63, 63, 63, 63, 63, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62,
        62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 61, 61, 61, 61, 61, 61, 61, 61, 61,
        61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 60, 60,
        60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60,
        60, 60, 60, 60, 60, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59,
        59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 58, 58, 58, 58, 58, 58, 58, 58, 58,
        58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 57, 57,
        57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
        57, 57, 57, 57, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 55, 55, 55, 55, 55, 55, 55, 55, 55,
        55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 54, 54, 54,
        54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54,
        54, 54, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
        53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 52, 52, 52, 52, 52, 52, 52, 52, 52,
        52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 51, 51, 51,
        51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
        51, 51, 51, 51, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
        50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
        49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 48, 48, 48,
        48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
        48, 48, 48, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47,
        47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46,
        46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 45, 45, 45,
        45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
        45, 45, 45, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44,
        44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43,
        43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 42, 42, 42, 42,
        42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42,
        42, 42, 42, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
        41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
        40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 39, 39, 39, 39,
        39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
        39, 39, 39, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
        38, 38, 38, 38, 38, 38, 38, 38, 38, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
        37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 36, 36, 36, 36,
        36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
        36, 36, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
        35, 35, 35, 35, 35, 35, 35, 35, 35, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34,
        34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 33, 33, 33, 33,
        33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
        33, 33, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
        32, 32, 32, 32, 32, 32, 32, 32, 32, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31,
        31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 30, 30, 30, 30, 30,
        30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
        30, 30, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
        29, 29, 29, 29, 29, 29, 29, 29, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
        28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 27, 27, 27, 27, 27,
        27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
        27, 27, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
        26, 26, 26, 26, 26, 26, 26, 26, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
        25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
        24, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
        23, 23, 23, 23, 23, 23, 23, 23, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22,
        22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 21, 21, 21, 21, 21, 21,
        21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
        21, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
        20, 20, 20, 20, 20, 20, 20, 20, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
        19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 18, 18, 18, 18, 18, 18,
        18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
        18, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
        17, 17, 17, 17, 17, 17, 17, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
        14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
        14, 14, 14, 14, 14, 14, 14, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
        13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 12, 12, 12, 12, 12, 12,
        12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
        11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
        11, 11, 11, 11, 11, 11, 11, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,  9,  9,  9,  9,  9,  9,  9,
         9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
         8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
         8,  8,  8,  8,  8,  8,  8,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
         7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  6,  6,  6,  6,  6,  6,  6,
         6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
         5,  5,  5,  5,  5,  5,  5,  5,  5
    };

    const unsigned int index = powers_ten_i[e + 1137];
#else
    const double dk = (-61 - e) * D_1_LOG2_10 + 347;
    int k = (int)dk; /* dk must be positive, so can do ceiling in positive */
    if (dk - k > 0.0)
        ++k;
    const unsigned int index = (unsigned int)((k >> 3) + 1);
#endif

    diy_fp_t res;
    res.f = powers_ten[index];
    res.e = powers_ten_e[index];
    *K = -(-348 + (int)(index << 3)); /* decimal exponent no need lookup table */
    return res;
}

static inline int get_u64_prefix0(uint64_t f)
{
#if defined(_MSC_VER) && defined(_M_AMD64)
    unsigned long index;
    _BitScanReverse64(&index, f);
    return (63 - index);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_clzll(f);
#else
    int index = DP_SIGNIFICAND_SIZE + 1; /* max value of diy_fp_t.f is smaller than pow(2, 54) */
    while (!(f & ((uint64_t)1 << index)))
        --index;
    return (63 - index);
#endif
}

static diy_fp_t diy_fp_multiply(diy_fp_t x, diy_fp_t y)
{
#if defined(_MSC_VER) && defined(_M_AMD64)
    uint64_t h;
    const uint64_t l = _umul128(x.f, y.f, &h);
    if (l & ((uint64_t)1 << 63)) /* rounding */
        ++h;
#elif (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6) || __clang_major__ >= 9) && (__WORDSIZE == 64)
    __extension__ typedef unsigned __int128 uint128;
    const uint128 p = (uint128)x.f * (uint128)y.f;
    uint64_t h = (uint64_t)(p >> 64);
    if ((uint64_t)p & ((uint64_t)1 << 63)) /* rounding */
        ++h;
#else
    const uint64_t M32 = 0XFFFFFFFF;
    const uint64_t a = x.f >> 32;
    const uint64_t b = x.f & M32;
    const uint64_t c = y.f >> 32;
    const uint64_t d = y.f & M32;

    const uint64_t ac = a * c;
    const uint64_t bc = b * c;
    const uint64_t ad = a * d;
    const uint64_t bd = b * d;

    uint64_t tmp = (bd >> 32) + (ad & M32) + (bc & M32);
    tmp += 1U << 31; /* mult_round */
    const uint64_t h = ac + (ad >> 32) + (bc >> 32) + (tmp >> 32);
#endif

    diy_fp_t r;
    r.f = h;
    r.e = x.e + y.e + 64;
    return r;
}

static inline void grisu_round(char* buffer, int len,
    uint64_t delta, uint64_t rest, uint64_t ten_kappa, uint64_t W_pv)
{
    uint64_t t = rest + ten_kappa;
    while (rest < W_pv && delta >= t && (W_pv - rest > t - W_pv || t < W_pv)) {
        --buffer[len - 1];
        rest += ten_kappa;
        t += ten_kappa;
    }
}

static inline void digit_gen(diy_fp_t Wv, diy_fp_t Wp, uint64_t delta, char* buffer, int* len, int* K)
{
    static const uint32_t divs[] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};
    uint32_t p1;
    uint64_t p2, p3;
    int d, kappa;
    diy_fp_t one, W_pv;

    one.f = ((uint64_t)1) << -Wp.e;
    one.e = Wp.e;

    W_pv.f = Wp.f - Wv.f;
    W_pv.e = Wp.e;

    p1 = Wp.f >> -one.e; /* Mp_cut */
    p2 = Wp.f & (one.f - 1);

    /* count decimal digit 32bit */
    if (p1 >= 100000) {
        if      (p1 < 1000000)    kappa = 6;
        else if (p1 < 10000000)   kappa = 7;
        else if (p1 < 100000000)  kappa = 8;
        else if (p1 < 1000000000) kappa = 9;
        else                      kappa = 10;
    } else {
        if      (p1 >= 10000)     kappa = 5;
        else if (p1 >= 1000)      kappa = 4;
        else if (p1 >= 100)       kappa = 3;
        else if (p1 >= 10)        kappa = 2;
        else                      kappa = 1;
    }

    *len = 0;
    while (kappa > 0) {
        switch (kappa) {
        case 10: d = p1 / 1000000000      ; p1 -= d * 1000000000      ; break;
        case  9: d = p1 /  100000000      ; p1 -= d *  100000000      ; break;
        case  8: d = p1 /   10000000      ; p1 -= d *   10000000      ; break;
        case  7: d = p1 /    1000000      ; p1 -= d *    1000000      ; break;
        case  6: d = p1 /     100000      ; p1 -= d *     100000      ; break;
        case  5: d = p1 /      10000      ; p1 -= d *      10000      ; break;
        case  4: d = get_1000_quotient(p1); p1 -= get_1000_multiple(d); break;
        case  3: d = get_100_quotient(p1) ; p1 -= get_100_multiple(d) ; break;
        case  2: d = get_10_quotient(p1)  ; p1 -= get_10_multiple(d)  ; break;
        case  1: d = p1                   ; p1  =              0      ; break;
        default:
#if defined(_MSC_VER)
            __assume(0);
#elif __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
            __builtin_unreachable();
#else
            d = 0;
#endif
            break;
        }

        buffer[(*len)++] = '0' + d; /* Mp_inv1 */
        --kappa;
        p3 = (((uint64_t)p1) << -one.e) + p2;
        if (p3 <= delta) { /* Mp_delta */
            *K += kappa;
            grisu_round(buffer, *len, delta, p3, ((uint64_t)divs[kappa]) << -one.e, W_pv.f);
            return;
        }
    }

    while (1) {
        p2 = (p2 << 1) + (p2 << 3);
        delta = (delta << 1) + (delta << 3);
        d = p2 >> -one.e;

        buffer[(*len)++] = '0' + d;
        --kappa;
        p2 &= one.f - 1;
        if (p2 < delta) {
            *K += kappa;
            grisu_round(buffer, *len, delta, p2, one.f, W_pv.f * divs[-kappa]);
            return;
        }
    }
}

static inline void grisu2(double value, char* buffer, int* length, int* K)
{
    diy_fp_t v, w_v, w_m, w_p, c_mk, Wv, Wm, Wp;
    int z_v, z_p;

    /* convert double to diy_fp */
    union {double d; uint64_t n;} u = {.d = value};
    int biased_e = (u.n & DP_EXPONENT_MASK) >> DP_SIGNIFICAND_SIZE; /* Exponent */
    uint64_t significand = u.n & DP_SIGNIFICAND_MASK; /* Mantissa */

    if (biased_e != 0) { /* normalized double */
        v.f = significand + DP_HIDDEN_BIT; /* Normalized double has a extra integer bit for Mantissa */
        v.e = biased_e - DP_EXPONENT_OFFSET - DP_SIGNIFICAND_SIZE; /* Exponent offset: -1023, divisor of Mantissa: pow(2,52) */
    } else { /* no-normalized double */
        v.f = significand; /* Non-normalized double doesn't have a extra integer bit for Mantissa */
        v.e = 1 - DP_EXPONENT_OFFSET - DP_SIGNIFICAND_SIZE; /* Fixed Exponent: -1022, divisor of Mantissa: pow(2,52) */
    }

    /* normalize v and boundaries */
    z_v = get_u64_prefix0(v.f);
    w_v.f = v.f << z_v;
    w_v.e = v.e - z_v;

    w_p.f = (v.f << 1) + 1;
    w_p.e = v.e - 1;
    z_p = get_u64_prefix0(w_p.f);
    w_p.f <<= z_p;
    w_p.e -= z_p;

    if (v.f == DP_HIDDEN_BIT) {
        w_m.f = (v.f << 2) - 1;
        w_m.e = v.e - 2;
    } else {
        w_m.f = (v.f << 1) - 1;
        w_m.e = v.e - 1;
    }
    w_m.f <<= w_m.e - w_p.e;
    w_m.e = w_p.e;

    c_mk = cached_power(w_p.e, K);
    Wv   = diy_fp_multiply(w_v, c_mk);
    Wp   = diy_fp_multiply(w_p, c_mk);
    Wm   = diy_fp_multiply(w_m, c_mk);

    ++Wm.f;
    --Wp.f;
    digit_gen(Wv, Wp, Wp.f - Wm.f, buffer, length, K);
}

static inline int fill_exponent(int K, char* buffer)
{
    int i = 0, k = 0;

    if (K < 0) {
        buffer[i++] = '-';
        K = -K;
    }

    if (K < 10) {
        buffer[i++] = '0' + K;
        buffer[i] = '\0';
        return i;
    }

    if (K >= 100) {
        k = get_100_quotient(K);
        K -= get_100_multiple(k);
        buffer[i++] = '0' + k;
    }

    k = get_10_quotient(K);
    K -= get_10_multiple(k);
    buffer[i++] = '0' + k;
    buffer[i++] = '0' + K;
    buffer[i] = '\0';

    return i;
}

static inline int prettify_string(char* buffer, int length, int K)
{
    /*
     * v = buffer * 10^k
     * kk is such that 10^(kk-1) <= v < 10^kk
     * this way kk gives the position of the comma.
     */
    const int kk = length + K;
    int offset;

    if (kk <= 21) {
        if (length <= kk) {
            /*
             * 1234e7 -> 12340000000
             * the first digits are already in. Add some 0s and call it a day.
             * the 21 is a personal choice. Only 16 digits could possibly be relevant.
             * Basically we want to print 12340000000 rather than 1234.0e7 or 1.234e10
             */
            memset(&buffer[length], '0', K);
            buffer[kk] = '.';
            buffer[kk + 1] = '0';
            buffer[kk + 2] = '\0';
            return (kk + 2);
        }

        if (kk > 0) {
            /*
             * 1234e-2 -> 12.34
             * comma number. Just insert a '.' at the correct location.
             */
            memmove(&buffer[kk + 1], &buffer[kk], length - kk);
            buffer[kk] = '.';
            buffer[length + 1] = '\0';
            return length + 1;
        }

        if (kk > -6) {
            /*
             * 1234e-6 -> 0.001234
             * something like 0.000abcde.
             * add '0.' and some '0's
             */
            offset = 2 - kk;
            memmove(&buffer[offset], &buffer[0], length);
            buffer[0] = '0';
            buffer[1] = '.';
            memset(&buffer[2], '0', offset - 2);
            buffer[length + offset] = '\0';
            return length + offset;
        }
    }

    if (length == 1) {
        /*
         * 1e30
         * just add 'e...'
         * fill_positive_fixnum will terminate the string
         */
        buffer[1] = 'e';
        return (2 + fill_exponent(kk - 1, &buffer[2]));
    }

    /*
     * 1234e30 -> 1.234e33
     * leave the first digit. then add a '.' and at the end 'e...'
     * fill_fixnum will terminate the string.
     */
    memmove(&buffer[2], &buffer[1], length - 1);
    buffer[1] = '.';
    buffer[length + 1] = 'e';
    return (length + 2 + fill_exponent(kk - 1, &buffer[length + 2]));
}

static int grisu2_dtoa(double value, char* buffer)
{
    int length, K, pre = 0;

    if (value == 0) {
        memcpy(buffer, "0.0", 4);
        return 3;
    }

    if (value < 0) {
        *buffer++ = '-';
        value = -value;
        pre = 1;
    }

    grisu2(value, buffer, &length, &K);
    return (pre + prettify_string(buffer, length, K));
}
