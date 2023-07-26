#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#define DIY_SIGNIFICAND_SIZE        64                  /* Symbol: 1 bit, Exponent, 11 bits, Mantissa, 52 bits */
#define DP_SIGNIFICAND_SIZE         52                  /* Mantissa, 52 bits */
#define DP_EXPONENT_OFFSET          0x3FF               /* Exponent offset is 0x3FF */
#define DP_EXPONENT_MAX             0x7FF               /* Max Exponent value */
#define DP_EXPONENT_MASK            0x7FF0000000000000  /* Exponent Mask, 11 bits */
#define DP_SIGNIFICAND_MASK         0x000FFFFFFFFFFFFF  /* Mantissa Mask, 52 bits */
#define DP_HIDDEN_BIT               0x0010000000000000  /* Integer bit for Mantissa */
#define LSHIFT_RESERVED_BIT         11

typedef struct {
    uint64_t f;
    int32_t e;
} diy_fp_t;

static inline uint64_t u128_calc(uint64_t x, uint64_t y)
{
    const uint64_t div = 10000000000000000000llu;
    uint64_t hi, lo, ret;
    double val;

#if defined(_MSC_VER) && defined(_M_AMD64)
    lo = _umul128(x, y, &hi);
#elif (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6) || __clang_major__ >= 9) && (__WORDSIZE == 64)
    __extension__ typedef unsigned __int128 uint128;
    const uint128 p = (uint128)x * (uint128)y;
    hi = (uint64_t)(p >> 64);
    lo = (uint64_t)p;
#else
    const uint64_t M32 = 0XFFFFFFFF;
    const uint64_t a = x >> 32;
    const uint64_t b = x & M32;
    const uint64_t c = y >> 32;
    const uint64_t d = y & M32;

    const uint64_t ac = a * c;
    const uint64_t bc = b * c;
    const uint64_t ad = a * d;
    const uint64_t bd = b * d;

    const uint64_t mid1 = ad + (bd >> 32);
    const uint64_t mid2 = bc + (mid1 & M32);

    hi = ac + (mid1 >> 32) + (mid2 >> 32);
    lo = (bd & M32) | (mid2 & M32) << 32;
#endif

    val = hi * 1.8446744073709551616; /* 1<<64 = 18446744073709551616 */
    ret = (uint64_t)val + 1;
    if (lo >= div)
        ++ret;

    return ret;
}

static inline int32_t u64_pz_get(uint64_t f)
{
#if defined(_MSC_VER) && defined(_M_AMD64)
    unsigned long index;
    _BitScanReverse64(&index, f);
    return 63 - index;
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_clzll(f);
#else
    int32_t index = DP_SIGNIFICAND_SIZE; /* max value of f is smaller than pow(2, 53) */
    while (!(f & ((uint64_t)1 << index)))
        --index;
    return 63 - index;
#endif
}

/*
# python to get lut

def print_pow_array(unit, index, end, positive):
    cmp_value = 10000000000000000000 # ULONG_MAX = 18446744073709551615
    base_lut = []
    index_lut = []
    for i in range(end + 1):
        a = unit ** i
        b = a
        j = 0
        if a < cmp_value:
            while b < cmp_value:
                j += 1
                b = a * (10 ** j)
            j -= 1
            b = a * (10 ** j)
            j = i * index + j

        else:
            while b >= cmp_value:
                j += 1
                b = a // (10 ** j)
            b = a // (10 ** j)
            if b != cmp_value:
                b += 1;
            j = i * index - j

        #print('%-3d: %d 0x%016x %d' % (i, j, b, b))
        base_lut.append(b)
        index_lut.append(j)

    print('static const uint64_t %s_base_lut[%d] = {' % ('positive' if positive else 'negative', end + 1), end='')
    for i in range(end + 1):
        if i % 4 == 0:
            print()
            print('    ', end='')
        print('0x%016x' % (base_lut[i]), end='')
        if i != end:
            print(', ', end='');
        else:
            print()
            print('};')

    print()
    print('static const int8_t %s_index_lut[%d] = {' % ('positive' if positive else 'negative', end + 1), end='')
    for i in range(end + 1):
        if i % 20 == 0:
            print()
            print('    ', end='')
        print('%-3d' % (index_lut[i]), end='')
        if i != end:
            print(', ', end='');
        else:
            print()
            print('};')

def print_positive_array():
    # 1 * (2 ** 4) = 1.6 * (10 ** 1)
    # 16 = 1.6 * (10 ** 1)
    # 243 = (1023 - 52) / 4 + 1
    print_pow_array(16, 1, 243, True)

def print_negative_array():
    # 1 * (2 ** -4) = 0.625 * (10 ** -1)
    # 625 = 0.625 * (10 ** 3)
    # 282 = (1022 + 52 + (63 - 11)) / 4 + 1
    print_pow_array(625, 3, 282, False)

print_positive_array()
print()
print_negative_array()
*/

static inline diy_fp_t positive_diy_fp(int32_t e)
{
    static const uint64_t positive_base_lut[244] = {
        0x0de0b6b3a7640000, 0x16345785d8a00000, 0x2386f26fc1000000, 0x38d7ea4c68000000,
        0x5af3107a40000000, 0x0e8d4a5100000000, 0x174876e800000000, 0x2540be4000000000,
        0x3b9aca0000000000, 0x5f5e100000000000, 0x0f42400000000000, 0x186a000000000000,
        0x2710000000000000, 0x3e80000000000000, 0x6400000000000000, 0x1000000000000000,
        0x199999999999999a, 0x28f5c28f5c28f5c3, 0x4189374bc6a7ef9e, 0x68db8bac710cb296,
        0x10c6f7a0b5ed8d37, 0x1ad7f29abcaf4858, 0x2af31dc4611873c0, 0x44b82fa09b5a52cc,
        0x6df37f675ef6eae0, 0x119799812dea111a, 0x1c25c268497681c3, 0x2d09370d42573604,
        0x480ebe7b9d58566d, 0x734aca5f6226f0ae, 0x12725dd1d243aba1, 0x1d83c94fb6d2ac35,
        0x2f394219248446bb, 0x4b8ed0283a6d3df8, 0x78e480405d7b9659, 0x1357c299a88ea76b,
        0x1ef2d0f5da7dd8ab, 0x318481895d962777, 0x4f3a68dbc8f03f25, 0x7ec3daf941806507,
        0x14484bfeebc29f87, 0x2073accb12d0ff3e, 0x33ec47ab514e652f, 0x5313a5dee87d6eb1,
        0x84ec3c97da624ab5, 0x154484932d2e725b, 0x22073a8515171d5e, 0x3671f73b54f1c896,
        0x571cbec554b60dbc, 0x0df01e85f912e37b, 0x164cfda3281e38c4, 0x23ae629ea696c139,
        0x391704310a8acec2, 0x5b5806b4ddaae469, 0x0e9d71b689dde71b, 0x17624f8a762fd82c,
        0x256a18dd89e626ac, 0x3bdcf495a9703de0, 0x5fc7edbc424d2fcc, 0x0f53304714d9265e,
        0x18851a0b548ea3ca, 0x273b5cdeedb10610, 0x3ec56164af81a34c, 0x646f023ab2690546,
        0x1011c2eaabe7d7e3, 0x19b604aaaca62637, 0x29233aaaadd6a38b, 0x41d1f7777c8a9f45,
        0x694ff258c7443208, 0x10d9976a5d52975e, 0x1af5bf109550f22f, 0x2b22cb4dbbb4b6b2,
        0x4504787c5f878ab6, 0x6e6d8d93cc0c1123, 0x11ab20e472914a6c, 0x1c45016d841baa47,
        0x2d3b357c0692aa0b, 0x485ebbf9a41ddcdd, 0x73cac65c39c96162, 0x1286d80ec190dc62,
        0x1da48ce468e7c703, 0x2f6dae3a4172d804, 0x4be2b05d35848cd3, 0x796ab3c855a0e152,
        0x136d3b7c36a919d0, 0x1f152bf9f10e8fb3, 0x31bb798fe8174c51, 0x4f925c1973587a1c,
        0x7f50935bebc0c35f, 0x145ecfe5bf520ac8, 0x2097b309321cde0c, 0x3425eb41e9c7c9ad,
        0x536fdecfdc72dc48, 0x857fcae62d8493a6, 0x155c2076bf9a5511, 0x222d00bdff5d54e7,
        0x36ae679665622172, 0x577d728a3bd03582, 0x0dff9772470297ec, 0x1665bf1d3e6a8cad,
        0x23d5fe9530aa7aae, 0x39566421e7772ab0, 0x5bbd6d030bf1dde6, 0x0eadab0aba3b2dbf,
        0x177c44ddf6c515fe, 0x2593a163246e8996, 0x3c1f689ea0b0dc23, 0x603240fdcde7c69d,
        0x0f64335bcf065d38, 0x18a0522c7e709527, 0x2766e9e0ca4dbb71, 0x3f0b0fce107c5f1a,
        0x64de7fb01a60982a, 0x1023998cd1053711, 0x19d28f47b4d524e8, 0x2950e53f87bb6e40,
        0x421b0865a5f8b066, 0x69c4da3c3cc11a3d, 0x10ec4be0ad8f8952, 0x1b13ac9aaf4c0ee9,
        0x2b52adc44bace4a8, 0x45511606df7b0773, 0x6ee8233e325e7251, 0x11bebdf578b2f392,
        0x1c6463225ab7ec1d, 0x2d6d6b6a2abfe02f, 0x48af1243779966b1, 0x744b506bf28f0ab4,
        0x129b69070816e2fe, 0x1dc574d80cf16b30, 0x2fa2548ce182451a, 0x4c36edae359d3b5c,
        0x79f17c49ef61f894, 0x1382cc34ca2427c6, 0x1f37ad21436d0c70, 0x31f2ae9b9f14e0b3,
        0x4feab0f8fe87cdea, 0x7fdde7f4ca72e310, 0x14756ccb01abfb5f, 0x20bbe144cf799232,
        0x345fced47f28e9e9, 0x53cc7e20cb74a974, 0x8613fd0145877586, 0x1573d68f903ea22a,
        0x2252f0e5b39769dd, 0x36eb1b091f58a961, 0x57de91a832277568, 0x0e0f218b8d25088c,
        0x167e9c127b6e7413, 0x23fdc683f8b0b9b8, 0x39960a6cc11ac2bf, 0x5c2343e134f79dfe,
        0x0ebdf661791d60f6, 0x179657025b6234bc, 0x25bd5803c569edfa, 0x3c62266c6f0fe329,
        0x609d0a4718196b74, 0x0f7549530e188c13, 0x18bba884e35a79b8, 0x2792a73b055d8f8c,
        0x3f510b91a22f4c13, 0x654e78e9037ee01e, 0x103583fc527ab338, 0x19ef3993b72ab85a,
        0x297ec285f1ddf3c3, 0x42646a6fe9631f9e, 0x6a3a43e642383296, 0x10ff151a99f482fa,
        0x1b31bb5dc320d18f, 0x2b82c562d1ce1c18, 0x459e089e1c7cf9c0, 0x6f6340fcfa618f99,
        0x11d270cc51055ea8, 0x1c83e7ad4e6efdda, 0x2d9fd9154a4b2fc3, 0x48ffc1bbaa11e604,
        0x74cc692c434fd66c, 0x12b010d3e1cf5582, 0x1de6815302e5559d, 0x2fd735519e3bbc2e,
        0x4c8b888296c5f9e3, 0x7a78da6a8ad65c9e, 0x139874ddd8c6234d, 0x1f5a549627a36bae,
        0x322a20f03f6bdf7d, 0x504367e6cbdfcbfa, 0x806bd9714632dff7, 0x148c22ca71a1bd70,
        0x20e037aa4f692f19, 0x3499f2aa18a84b5a, 0x542984435aa6def6, 0x86a8d39ef77164bd,
        0x158ba6fab6f36c48, 0x22790b2abe5246d9, 0x372811ddfd50715a, 0x58401c96621a4ef7,
        0x0e1ebce4dc7f16e0, 0x169794a160cb57cd, 0x2425ba9bce122614, 0x39d5f75fb01d09ba,
        0x5c898bcc4cfb42c3, 0x0ece53cec4a314ec, 0x17b08617a104ee47, 0x25e73cf29b3b16d7,
        0x3ca52e50f85e8af2, 0x61084a1b26fdab1c, 0x0f867241c8cc6d4d, 0x18d71d360e13e214,
        0x27be952349b969b9, 0x3f97550542c242c1, 0x65beee6ed136d135, 0x1047824f2bb6d9cb,
        0x1a0c03b1df8af612, 0x29acd2b63277f01c, 0x42ae1df050bfe694, 0x6ab02fe6e79970ec,
        0x1111f32f2f4bc026, 0x1b4feb7eb212cd0a, 0x2bb31264501e14dc, 0x45eb50a08030215f,
        0x6fdee76733803565, 0x11e6398126f5cb1b, 0x1ca38f350b22de91, 0x2dd27ebb4504974e,
        0x4950cac53b3a8bb0, 0x754e113b91f745e6, 0x12c4cf8ea6b6ec77, 0x1e07b27dd78b13f2,
        0x300c50c958de864f, 0x4ce0814227ca707e, 0x7b00ced03faa4d96, 0x13ae3591f5b4d937,
        0x1f7d228322baf525, 0x3261d0d1d12b21d4, 0x509c814fb511cfba, 0x80fa687f881c7f8f,
        0x14a2f1ffecd15c17, 0x2104b66647b56025, 0x34d4570a0c5566a1, 0x5486f1a9ad557102,
        0x873e4f75e2224e69, 0x15a391d56bdc876d, 0x229f4fbbdfc73f15, 0x37654c5fcc71fe88
    };

    static const int8_t positive_index_lut[244] = {
        18 , 18 , 18 , 18 , 18 , 17 , 17 , 17 , 17 , 17 , 16 , 16 , 16 , 16 , 16 , 15 , 15 , 15 , 15 , 15 ,
        14 , 14 , 14 , 14 , 14 , 13 , 13 , 13 , 13 , 13 , 12 , 12 , 12 , 12 , 12 , 11 , 11 , 11 , 11 , 11 ,
        10 , 10 , 10 , 10 , 10 , 9  , 9  , 9  , 9  , 8  , 8  , 8  , 8  , 8  , 7  , 7  , 7  , 7  , 7  , 6  ,
        6  , 6  , 6  , 6  , 5  , 5  , 5  , 5  , 5  , 4  , 4  , 4  , 4  , 4  , 3  , 3  , 3  , 3  , 3  , 2  ,
        2  , 2  , 2  , 2  , 1  , 1  , 1  , 1  , 1  , 0  , 0  , 0  , 0  , 0  , -1 , -1 , -1 , -1 , -2 , -2 ,
        -2 , -2 , -2 , -3 , -3 , -3 , -3 , -3 , -4 , -4 , -4 , -4 , -4 , -5 , -5 , -5 , -5 , -5 , -6 , -6 ,
        -6 , -6 , -6 , -7 , -7 , -7 , -7 , -7 , -8 , -8 , -8 , -8 , -8 , -9 , -9 , -9 , -9 , -9 , -10, -10,
        -10, -10, -10, -11, -11, -11, -11, -12, -12, -12, -12, -12, -13, -13, -13, -13, -13, -14, -14, -14,
        -14, -14, -15, -15, -15, -15, -15, -16, -16, -16, -16, -16, -17, -17, -17, -17, -17, -18, -18, -18,
        -18, -18, -19, -19, -19, -19, -19, -20, -20, -20, -20, -20, -21, -21, -21, -21, -22, -22, -22, -22,
        -22, -23, -23, -23, -23, -23, -24, -24, -24, -24, -24, -25, -25, -25, -25, -25, -26, -26, -26, -26,
        -26, -27, -27, -27, -27, -27, -28, -28, -28, -28, -28, -29, -29, -29, -29, -29, -30, -30, -30, -30,
        -30, -31, -31, -31
    };

    const diy_fp_t v = { .f = positive_base_lut[e], .e = positive_index_lut[e] };
    return v;
}

static inline diy_fp_t negative_diy_fp(int32_t e)
{
    static const uint64_t negative_base_lut[283] = {
        0x0de0b6b3a7640000, 0x56bc75e2d6310000, 0x3635c9adc5dea000, 0x21e19e0c9bab2400,
        0x152d02c7e14af680, 0x84595161401484a0, 0x52b7d2dcc80cd2e4, 0x33b2e3c9fd0803cf,
        0x204fce5e3e250262, 0x1431e0fae6d7217d, 0x7e37be2022c0914c, 0x4ee2d6d415b85acf,
        0x314dc6448d9338c2, 0x1ed09bead87c0379, 0x13426172c74d822c, 0x785ee10d5da46d91,
        0x4b3b4ca85a86c47b, 0x2f050fe938943acd, 0x1d6329f1c35ca4c0, 0x125dfa371a19e6f8,
        0x72cb5bd86321e38d, 0x47bf19673df52e38, 0x2cd76fe086b93ce3, 0x1c06a5ec5433c60e,
        0x118427b3b4a05bc9, 0x6d79f82328ea3da7, 0x446c3b15f9926688, 0x2ac3a4edbbfb8015,
        0x1aba4714957d300e, 0x10b46c6cdd6e3e09, 0x6867a5a867f103b3, 0x4140c78940f6a250,
        0x28c87cb5c89a2572, 0x197d4df19d605768, 0x0fee50b7025c36a1, 0x63917877cec0556c,
        0x3e3aeb4ae1383563, 0x26e4d30eccc3215e, 0x184f03e93ff9f4db, 0x0f316271c7fc3909,
        0x5ef4a74721e86477, 0x3b58e88c75313eca, 0x25179157c93ec73f, 0x172ebad6ddc73c87,
        0x0e7d34c64a9c85d5, 0x5a8e89d75252446f, 0x3899162693736ac6, 0x235fadd81c2822bc,
        0x161bcca7119915b6, 0x8a2dbf142dfcc7ac, 0x565c976c9cbdfccc, 0x35f9dea3e1f6bdff,
        0x21bc2b266d3a36c0, 0x15159af804446238, 0x83c7088e1aab65dc, 0x525c6558d0ab1faa,
        0x3379bf57826af3ca, 0x202c1796b182d85f, 0x141b8ebe2ef1c73b, 0x7dac3c24a5671d30,
        0x4e8ba596e760723e, 0x3117477e509c4767, 0x1eae8caef261aca1, 0x132d17ed577d0be5,
        0x77d9d58b62cd8a52, 0x4ae825771dc07673, 0x2ed1176a72984a08, 0x1d42aea2879f2e45,
        0x1249ad2594c37cec, 0x724c7a2ae1c5ccbe, 0x476fcc5acd1b9ff7, 0x2ca5dfb8c03143fa,
        0x1be7abd3781eca7d, 0x1170cb642b133e8e, 0x6d00f7320d3846f5, 0x44209a7f48432c5a,
        0x2a94608f8d29fbb8, 0x1a9cbc59b83a3d53, 0x10a1f5b813246654, 0x67f43fbe77a37f8c,
        0x40f8a7d70ac62fb8, 0x289b68e666bbddd3, 0x1961219000356aa4, 0x0fdcb4fa002162a7,
        0x63236b1a80d0a88f, 0x3df622f09082695a, 0x26b9d5d65a5181d8, 0x183425a5f872f127,
        0x0f209787bb47d6b9, 0x5e8bb3105280fe00, 0x3b174fea33909ec0, 0x24ee91f2603a6338,
        0x17151b377c247e03, 0x0e6d3102ad96cec2, 0x5a2a7250bcee8c3c, 0x385a8772761517a6,
        0x233894a789cd2ec8, 0x16035ce8b6203d3d, 0x899504ae72497ebb, 0x55fd22ed076def35,
        0x35be35d424a4b581, 0x2196e1a496e6f171, 0x14fe4d06de5056e7, 0x8335616aed761f20,
        0x52015ce2d469d374, 0x3340da0dc4c22429, 0x200888489af9569a, 0x1405552d60dbd620,
        0x7d21545b9d5dfa47, 0x4e34d4b9425abc6c, 0x30e104f3c978b5c4, 0x1e8ca3185deb719b,
        0x1317e5ef3ab32701, 0x77555d172edfb3c3, 0x4a955a2e7d4bd05a, 0x2e9d585d0e4f6238,
        0x1d22573a28f19d63, 0x123576845997025e, 0x71ce24bb2fefcecb, 0x4720d6f4fdf5e13f,
        0x2c7486591eb9acc8, 0x1bc8d3f7b3340bfd, 0x115d847ad000877e, 0x6c887bff94034ed3,
        0x43d54d7fbc821144, 0x2a65506fd5d14acb, 0x1a7f5245e5a2cebf, 0x108f936baf85c137,
        0x678159610903f798, 0x40b0d7dca5a27abf, 0x286e86e9e7858cb8, 0x1945145230b377f3,
        0x0fcb2cb35e702af8, 0x62b5d7610e3d0c8c, 0x3db1a69ca8e627d7, 0x268f0821e98fd8e7,
        0x1819651531f9e790, 0x0f0fdf2d3f3c30ba, 0x5e2332dacb38308b, 0x3ad5ffc8bf031e57,
        0x24c5bfdd7761f2f7, 0x16fb97ea6a9d37da, 0x0e5d3ef282a242e9, 0x59c6c96bb076222b,
        0x381c3de34e49d55b, 0x2311a6ae10ee2559, 0x15eb082cca94d758, 0x88fcf317f22241e3,
        0x559e17eef755692e, 0x3582cef55a9561bd, 0x2171c159589d5d16, 0x14e718d7d7625a2e,
        0x82a45b450226b39d, 0x51a6b90b21583043, 0x330833a6f4d71e2a, 0x1fe52048590672da,
        0x13ef342d37a407c9, 0x7c97061a9bc130a3, 0x4dde63d0a158be66, 0x30aafe6264d77700,
        0x1e6adefd7f06aa60, 0x1302cb5e6f642a7c, 0x76d1770e38320987, 0x4a42ea68e31f45f4,
        0x2e69d2818df38bb9, 0x1d022390f8b83754, 0x1221563a9b732295, 0x71505aee4b8f981e,
        0x46d238d4ef39bf13, 0x2c4363851584176c, 0x1baa1e332d728ea4, 0x114a52dffc679926,
        0x6c1085f7e9877d2e, 0x438a53baf1f4ae3d, 0x2a367454d738ece6, 0x1a6208b506839410,
        0x107d457124123c8a, 0x670ef2032171fa5d, 0x40695741f4e73c7a, 0x2841d689391085cd,
        0x19292615c3aa53a0, 0x0fb9b7cd9a4a7444, 0x6248bcc5045156a8, 0x3d6d75fb22b2d629,
        0x266469bcf5afc5da, 0x17fec216198ddba8, 0x0eff394dcff8a949, 0x5dbb262653d22208,
        0x3a94f7d7f4635545, 0x249d1ae6f8be154c, 0x16e230d05b76cd4f, 0x0e4d5e82392a4052,
        0x59638eade54811fd, 0x37de392caf4d0b3e, 0x22eae3bbed902707, 0x15d2ce55747a1865,
        0x8865899617fb1872, 0x553f75fdcefcef47, 0x3547a9bea15e158d, 0x214cca1724dacd78,
        0x14cffe4e7708c06b, 0x8213f56a67f6b29c, 0x514c796280fa2fa2, 0x32cfcbdd909c5dc5,
        0x1fc1df6a7a61ba9b, 0x13d92ba28c7d14a1, 0x7c0d50b7ee0dc0ee, 0x4d885272f4c89895,
        0x30753387d8fd5f5d, 0x1e494034e79e5b9a, 0x12edc82110c2f941, 0x764e22cea8c295d2,
        0x49f0d5c129799da3, 0x2e368598b9ec0286, 0x1ce2137f74338194, 0x120d4c2fa8a030fd,
        0x70d31c29dde93229, 0x4683f19a2ab1bf5a, 0x2c1277005aaf1798, 0x1b8b8a6038ad6ebf,
        0x1137367c236c6538, 0x6b991487dd65789a, 0x433facd4ea5f6b61, 0x2a07cc05127ba31d,
        0x1a44df832b8d45f2, 0x106b0bb1fb384bb7, 0x669d0918621fd938, 0x402225af3d53e7c3,
        0x2815578d865470da, 0x190d56b873f4c689, 0x0fa856334878fc16, 0x61dc1ac084f42784,
        0x3d2990b8531898b3, 0x2639fa7333ef5f70, 0x17e43c8800759ba6, 0x0eeea5d500498148,
        0x5d538c7341cb67ff, 0x3a5437c8091f2100, 0x2474a2dd05b374a0, 0x16c8e5ca239028e4,
        0x0e3d8f9e563a198f, 0x5900c19d9aeb1fba, 0x37a0790280d2f3d4, 0x22c44ba19083d865,
        0x15baaf44fa52673f, 0x87cec76f1c830549, 0x54e13ca571d1e34e, 0x350cc5e767232e11,
        0x2127fbb0a075fccb, 0x14b8fd4e6449bdff, 0x81842f29f2cce376, 0x50f29d7a37c00e2a,
        0x3297a26c62d808db, 0x1f9ec583bdc70589, 0x13c33b72569c6376, 0x7b84338a9d516d9d,
        0x4d32a036a252e482, 0x303fa4222573ced2, 0x1e27c69557686143, 0x12d8dc1d56a13cca,
        0x75cb5fb75d6fbbed, 0x499f1bd29a65d574, 0x2e037163a07fa569, 0x1cc226de444fc762,
        0x11f9584aeab1dc9d, 0x705667d43ad7a2d4, 0x463600e4a4c6c5c5, 0x2be1c08ee6fc3b9b,
        0x1b6d1859505da541, 0x11242f37d23a8749, 0x6b22271ce1edcd85, 0x42f558720d34a073,
        0x29d957474840e448, 0x1a27d68c8d288ead, 0x1058e617d839592d, 0x662b9e1507666d54,
        0x3fdb42cd24a00455, 0x27e909c036e402b5, 0x18f1a618224e81b1, 0x0f9707cf1571110f,
        0x616ff0ce4602aa9b, 0x3ce5f680ebc1aaa1, 0x260fba1093590aa5
    };

    static const int8_t negative_index_lut[283] = {
        18 , 19 , 19 , 19 , 19 , 20 , 20 , 20 , 20 , 20 , 21 , 21 , 21 , 21 , 21 , 22 , 22 , 22 , 22 , 22 ,
        23 , 23 , 23 , 23 , 23 , 24 , 24 , 24 , 24 , 24 , 25 , 25 , 25 , 25 , 25 , 26 , 26 , 26 , 26 , 26 ,
        27 , 27 , 27 , 27 , 27 , 28 , 28 , 28 , 28 , 29 , 29 , 29 , 29 , 29 , 30 , 30 , 30 , 30 , 30 , 31 ,
        31 , 31 , 31 , 31 , 32 , 32 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 34 , 35 ,
        35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 36 , 37 , 37 , 37 , 37 , 37 , 38 , 38 , 38 , 38 , 39 , 39 ,
        39 , 39 , 39 , 40 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 42 , 43 , 43 ,
        43 , 43 , 43 , 44 , 44 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 47 , 47 ,
        47 , 47 , 47 , 48 , 48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 ,
        51 , 51 , 52 , 52 , 52 , 52 , 52 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 ,
        55 , 55 , 56 , 56 , 56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 ,
        59 , 60 , 60 , 60 , 60 , 60 , 61 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 ,
        63 , 64 , 64 , 64 , 64 , 64 , 65 , 65 , 65 , 65 , 65 , 66 , 66 , 66 , 66 , 66 , 67 , 67 , 67 , 67 ,
        67 , 68 , 68 , 68 , 68 , 69 , 69 , 69 , 69 , 69 , 70 , 70 , 70 , 70 , 70 , 71 , 71 , 71 , 71 , 71 ,
        72 , 72 , 72 , 72 , 72 , 73 , 73 , 73 , 73 , 73 , 74 , 74 , 74 , 74 , 74 , 75 , 75 , 75 , 75 , 75 ,
        76 , 76 , 76
    };

    const diy_fp_t v = { .f = negative_base_lut[e], .e = negative_index_lut[e] };
    return v;
}

static inline void ldouble_convert(diy_fp_t *v)
{
    uint64_t f = v->f;
    int32_t e = v->e, t = v->e;
    diy_fp_t x;

    e >>= 2;
    t -= e << 2;
    if (t) {
        ++e;
        f >>= 4 - t;
    }
    if (e >= 0) {
        x = positive_diy_fp(e);
    } else {
        x = negative_diy_fp(-e);
    }

    v->f = u128_calc(x.f, f);
    v->e = e - x.e + 19;
}

static const char ch_100_lut[200] = {
    '0','0','0','1','0','2','0','3','0','4','0','5','0','6','0','7','0','8','0','9',
    '1','0','1','1','1','2','1','3','1','4','1','5','1','6','1','7','1','8','1','9',
    '2','0','2','1','2','2','2','3','2','4','2','5','2','6','2','7','2','8','2','9',
    '3','0','3','1','3','2','3','3','3','4','3','5','3','6','3','7','3','8','3','9',
    '4','0','4','1','4','2','4','3','4','4','4','5','4','6','4','7','4','8','4','9',
    '5','0','5','1','5','2','5','3','5','4','5','5','5','6','5','7','5','8','5','9',
    '6','0','6','1','6','2','6','3','6','4','6','5','6','6','6','7','6','8','6','9',
    '7','0','7','1','7','2','7','3','7','4','7','5','7','6','7','7','7','8','7','9',
    '8','0','8','1','8','2','8','3','8','4','8','5','8','6','8','7','8','8','8','9',
    '9','0','9','1','9','2','9','3','9','4','9','5','9','6','9','7','9','8','9','9',
};

static const uint8_t tz_100_lut[100] = {
    2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

#define FAST_DIV10(n)       (ch_100_lut[(n) << 1] - '0')                    /* 0 <= n < 100 */
#define FAST_DIV100(n)      (((n) * 5243) >> 19)                            /* 0 <= n < 10000 */
#define FAST_DIV10000(n)    ((uint32_t)(((uint64_t)(n) * 109951163) >> 40)) /* 0 <= n < 100000000 */

static inline int32_t fill_1_4_digits(char *buffer, uint32_t digits, int32_t *ptz)
{
    char *s = buffer;

    if (digits < 100) {
        if (digits >= 10) {
            *ptz = tz_100_lut[digits];
            memcpy(s, &ch_100_lut[digits<<1], 2);
            s += 2;
        } else {
            *ptz = 0;
            *s++ = digits + '0';
        }
    } else {
        uint32_t q = FAST_DIV100(digits);
        uint32_t r = digits - q * 100;

        if (q >= 10) {
            *ptz = tz_100_lut[q];
            memcpy(s, &ch_100_lut[q<<1], 2);
            s += 2;
        } else {
            *ptz = 0;
            *s++ = q + '0';
        }

        if (!r) {
            *ptz += 2;
            memset(s, '0', 2);
            s += 2;
        } else {
            *ptz = tz_100_lut[r];
            memcpy(s, &ch_100_lut[r<<1], 2);
            s += 2;
        }
    }

    return s - buffer;
}

static inline int32_t fill_t_4_digits(char *buffer, uint32_t digits, int32_t *ptz)
{
    char *s = buffer;
    uint32_t q = FAST_DIV100(digits);
    uint32_t r = digits - q * 100;

    memcpy(s, &ch_100_lut[q<<1], 2);
    memcpy(s + 2, &ch_100_lut[r<<1], 2);

    if (!r) {
        *ptz = tz_100_lut[q] + 2;
    } else {
        *ptz = tz_100_lut[r];
    }

    return 4;
}

static inline int32_t fill_1_8_digits(char *buffer, uint32_t digits, int32_t *ptz)
{
    char *s = buffer;

    if (digits < 10000) {
        return fill_1_4_digits(s, digits, ptz);
    } else {
        uint32_t q = FAST_DIV10000(digits);
        uint32_t r = digits - q * 10000;

        s += fill_1_4_digits(s, q, ptz);
        if (!r) {
            *ptz += 4;
            memset(s, '0', 4);
            s += 4;
        } else {
            s += fill_t_4_digits(s, r, ptz);
        }
    }

    return s - buffer;
}

static inline int32_t fill_t_8_digits(char *buffer, uint32_t digits, int32_t *ptz)
{
    char *s = buffer;

    if (digits < 10000) {
        memset(s, '0', 4);
        fill_t_4_digits(s + 4, digits, ptz);
    } else {
        uint32_t q = FAST_DIV10000(digits);
        uint32_t r = digits - q * 10000;

        fill_t_4_digits(s, q, ptz);
        if (!r) {
            memset(s + 4, '0', 4);
            *ptz += 4;
        } else {
            fill_t_4_digits(s + 4, r, ptz);
        }
    }

    return 8;
}

static inline int32_t fill_1_16_digits(char *buffer, uint64_t digits, int32_t *ptz)
{
    char *s = buffer;

    if (digits < 100000000llu) {
        return fill_1_8_digits(s, (uint32_t)digits, ptz);
    } else {
        uint32_t q = (uint32_t)(digits / 100000000);
        uint32_t r = (uint32_t)(digits - (uint64_t)q * 100000000);

        s += fill_1_8_digits(s, q, ptz);
        if (!r) {
            *ptz += 8;
            memset(s, '0', 8);
            s += 8;
        } else {
            s += fill_t_8_digits(s, r, ptz);
        }
    }

    return s - buffer;
}

static inline int32_t fill_t_16_digits(char *buffer, uint64_t digits, int32_t *ptz)
{
    char *s = buffer;

    if (digits < 100000000llu) {
        memset(s, '0', 8);
        fill_t_8_digits(s + 8, digits, ptz);
    } else {
        uint32_t q = (uint32_t)(digits / 100000000);
        uint32_t r = (uint32_t)(digits - (uint64_t)q * 100000000);

        fill_t_8_digits(s, q, ptz);
        if (!r) {
            memset(s + 8, '0', 8);
            *ptz += 8;
        } else {
            fill_t_8_digits(s + 8, r, ptz);
        }
    }

    return 16;
}

static inline int32_t fill_1_20_digits(char *buffer, uint64_t digits, int32_t *ptz)
{
    char *s = buffer;

    if (digits < 10000000000000000llu) {
        return fill_1_16_digits(s, digits, ptz);
    } else {
        uint32_t q = (uint32_t)(digits / 10000000000000000llu);
        uint64_t r = (digits - (uint64_t)q * 10000000000000000llu);

        s += fill_1_4_digits(s, q, ptz);
        if (!r) {
            memset(s, '0', 16);
            s += 16;
            *ptz = 16;
        } else {
            s += fill_t_16_digits(s, r, ptz);
        }
    }

    return s - buffer;
}

static inline int32_t fill_significand(char *buffer, uint64_t digits, int32_t *ptz)
{
   return fill_1_20_digits(buffer, digits, ptz);
}

static inline int32_t fill_exponent(int32_t K, char* buffer)
{
    int32_t i = 0, k = 0;

    if (K < 0) {
        buffer[i++] = '-';
        K = -K;
    } else {
        buffer[i++] = '+';
    }

    if (K < 100) {
        if (K < 10) {
            buffer[i++] = '0' + K;
        } else {
            memcpy(&buffer[i], &ch_100_lut[K<<1], 2);
            i += 2;
        }
    } else {
        k = FAST_DIV100(K);
        K -= k * 100;
        buffer[i++] = '0' + k;
        memcpy(&buffer[i], &ch_100_lut[K<<1], 2);
        i += 2;
    }

    return i;
}

static inline char* ldouble_format(char* buffer, uint64_t digits, int32_t decimal_exponent)
{
    int32_t num_digits, trailing_zeros, vnum_digits, decimal_point;

    num_digits = fill_significand(buffer + 1, digits + 2, &trailing_zeros);
    if (buffer[num_digits] >= '5') {
        buffer[num_digits] -= 2;
    } else {
        buffer[num_digits] = '0';
        for (trailing_zeros = 1; trailing_zeros < num_digits; ++trailing_zeros) {
            if (buffer[num_digits - trailing_zeros] != '0')
                break;
        }
    }
    vnum_digits = num_digits - trailing_zeros;
    decimal_point = num_digits + decimal_exponent;

    switch (decimal_point) {
    case -6: case -5: case -4: case -3: case -2: case -1: case 0:
         /* 0.[000]digits */
        memmove(buffer + 2 - decimal_point, buffer + 1, vnum_digits);
        memset(buffer, '0', 2 - decimal_point);
        buffer[1] = '.';
        buffer += 2 - decimal_point + vnum_digits;
        break;

    case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9:
    case 10: case 11: case 12: case 13: case 14: case 15: case 16: case 17:
        if (decimal_point < vnum_digits) {
            /* dig.its */
            memmove(buffer, buffer + 1, decimal_point);
            buffer[decimal_point] = '.';
            buffer += vnum_digits + 1;
        } else {
            /* digits[000] */
            memmove(buffer, buffer + 1, decimal_point);
            buffer += decimal_point;
            memcpy(buffer, ".0", 2);
            buffer += 2;
        }
        break;

    default:
        buffer[0] = buffer[1];
        ++buffer;
        if (vnum_digits != 1) {
            /* d.igitsE+123 */
            *buffer = '.';
            buffer += vnum_digits;
        } else {
            /* dE+123 */
        }
        *buffer++ = 'e';
        buffer += fill_exponent(decimal_point - 1, buffer);
        break;
    }

    *buffer = '\0';
    return buffer;
}

int ldouble_dtoa(double value, char* buffer)
{
    diy_fp_t v;
    char *s = buffer;
    union {double d; uint64_t n;} u = {.d = value};
    int32_t signbit = u.n >> (DIY_SIGNIFICAND_SIZE - 1);
    int32_t exponent = (u.n & DP_EXPONENT_MASK) >> DP_SIGNIFICAND_SIZE; /* Exponent */
    uint64_t significand = u.n & DP_SIGNIFICAND_MASK; /* Mantissa */

    if (signbit) {
        *s++ = '-';
    }

    switch (exponent) {
    case DP_EXPONENT_MAX:
        if (significand) {
            memcpy(buffer, "nan", 4);
            return 3;
        } else {
            memcpy(s, "inf", 4);
            return signbit + 3;
        }
        break;

    case 0:
        if (!significand) {
            /* no-normalized double */
            v.f = significand; /* Non-normalized double doesn't have a extra integer bit for Mantissa */
            v.e = 1 - DP_EXPONENT_OFFSET - DP_SIGNIFICAND_SIZE; /* Fixed Exponent: -1022, divisor of Mantissa: pow(2,52) */

            /* The smallest e is (-1022 - 52 - (63 - 11)) = -1126 */
            int32_t lshiftbit = u64_pz_get(v.f) - LSHIFT_RESERVED_BIT;
            v.f <<= lshiftbit;
            v.e -= lshiftbit; /* The smallest e is (-1022 - 52 - (63 - 11)) = -1126 */
        } else {
            memcpy(s, "0.0", 4);
            return signbit + 3;
        }
        break;

    default:
        /* normalized double */
        v.f = significand + DP_HIDDEN_BIT; /* Normalized double has a extra integer bit for Mantissa */
        v.e = exponent - DP_EXPONENT_OFFSET - DP_SIGNIFICAND_SIZE; /* Exponent offset: -1023, divisor of Mantissa: pow(2,52) */

        if (0 <= -v.e && -v.e <= DP_SIGNIFICAND_SIZE && ((v.f & (((uint64_t)1 << -v.e) - 1)) == 0)) {
            /* small integer. */
            int32_t tz, n;
            n = fill_significand(s, v.f >> -v.e, &tz);
            memcpy(s + n, ".0", 3);
            return n + 2 + signbit;
        } else {
            /* The largest e is (1023 - 52) = 971 */
        }
        break;
    }

    ldouble_convert(&v);
    s = ldouble_format(s, v.f, v.e);
    return s - buffer;
}
