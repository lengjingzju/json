/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2019-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* URL: https://github.com/lengjingzju/json *
*******************************************/
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "jnum.h"
#if defined(_MSC_VER)
#include <intrin.h>
#endif

#if defined(__GNUC__) || defined(__clang__)
#define FALLTHROUGH_ATTR            __attribute__((fallthrough))
#else
#define FALLTHROUGH_ATTR
#endif

#define DIY_SIGNIFICAND_SIZE        64                  /* Symbol: 1 bit, Exponent, 11 bits, Mantissa, 52 bits */
#define DP_SIGNIFICAND_SIZE         52                  /* Mantissa, 52 bits */
#define DP_EXPONENT_OFFSET          0x3FF               /* Exponent offset is 0x3FF */
#define DP_EXPONENT_MAX             0x7FF               /* Max Exponent value */
#define DP_EXPONENT_MASK            0x7FF0000000000000  /* Exponent Mask, 11 bits */
#define DP_SIGNIFICAND_MASK         0x000FFFFFFFFFFFFF  /* Mantissa Mask, 52 bits */
#define DP_HIDDEN_BIT               0x0010000000000000  /* Integer bit for Mantissa */

#if (__WORDSIZE == 64) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6) || __clang_major__ >= 9)
#define USING_U128_CALC             1
#else
#define USING_U128_CALC             0
#endif
#if USING_U128_CALC
__extension__ typedef unsigned __int128 u128;
#endif

typedef struct {
    uint64_t f;
    int32_t e;
} diy_fp_t;

typedef struct {
    uint64_t hi;
    uint64_t lo;
} u64x2_t;

#define FAST_DIV100(n)      (((n) * 5243) >> 19)                            /* 0 <= n < 10000 */
#define FAST_DIV10000(n)    ((uint32_t)(((uint64_t)(n) * 109951163) >> 40)) /* 0 <= n < 100000000 */

static inline int32_t u64_pz_get(uint64_t n)
{
#if defined(_MSC_VER)
    unsigned long index;
    _BitScanReverse64(&index, n);
    return 63 - index;
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_clzll(n);
#else
    int c = 0;
    if (n & 0xFFFFFFFF00000000) n >>= 32; else c += 32;
    if (n & 0xFFFF0000)         n >>= 16; else c += 16;
    if (n & 0xFF00)             n >>= 8 ; else c += 8;
    if (n & 0xF0)               n >>= 4 ; else c += 4;
    if (n & 0xC)                n >>= 2 ; else c += 2;
    return c + (n & 0x1);
#endif
}

static inline u64x2_t u128_mul(uint64_t x, uint64_t y)
{
    u64x2_t ret;
#if defined(_MSC_VER)
    ret.lo = _umul128(x, y, &ret.hi);
#elif USING_U128_CALC
    const u128 p = (u128)x * (u128)y;
    ret.hi = (uint64_t)(p >> 64);
    ret.lo = (uint64_t)p;
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

    ret.hi = ac + (mid1 >> 32) + (mid2 >> 32);
    ret.lo = (bd & M32) | (mid2 & M32) << 32;
#endif
    return ret;
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
            *ptz += 16;
        } else {
            s += fill_t_16_digits(s, r, ptz);
        }
    }

    return s - buffer;
}

int jnum_itoa(int32_t num, char *buffer)
{
    char *s = buffer;
    uint32_t n = 0;
    int32_t tz = 0;

    if (num == 0) {
        memcpy(s, "0", 2);
        return 1;
    }

    if (num < 0) {
        n = -num;
        *s++ = '-';
    } else {
        n = num;
    }

    if (n < 100000000) {
        s += fill_1_8_digits(s, n, &tz);
    } else {
        uint32_t q = (uint32_t)(((uint64_t)n * 1441151881) >> 57); /* n / 100000000 */
        uint32_t r = n - q * 100000000;

        if (q >= 10) {
            tz = tz_100_lut[q];
            memcpy(s, &ch_100_lut[q<<1], 2);
            s += 2;
        } else {
            tz = 0;
            *s++ = q + '0';
        }

        if (!r) {
            tz += 8;
            memset(s, '0', 8);
            s += 8;
        } else {
            s += fill_t_8_digits(s, r, &tz);
        }
    }

    *s = '\0';
    return s - buffer;
}

int jnum_ltoa(int64_t num, char *buffer)
{
    char *s = buffer;
    uint64_t n = 0;
    int32_t tz = 0;

    if (num == 0) {
        memcpy(s, "0", 2);
        return 1;
    }

    if (num < 0) {
        n = -num;
        *s++ = '-';
    } else {
        n = num;
    }

    s += fill_1_20_digits(s, n, &tz);

    *s = '\0';
    return s - buffer;
}

static const char hex_char_lut[] = {
    '0', '1', '2', '3', '4',
    '5', '6', '7', '8', '9',
    'a', 'b', 'c', 'd', 'e', 'f'
};

static inline int fill_t_2_hexs(char *buffer, uint32_t num)
{
    char *s = buffer;
    *s++ = hex_char_lut[num >> 4];
    *s++ = hex_char_lut[num & 0xf];
    return 2;
}

static inline int fill_t_4_hexs(char *buffer, uint32_t num)
{
    char *s = buffer;

    fill_t_2_hexs(s, num >> 8);
    fill_t_2_hexs(s + 2, num & 0xff);
    return 4;
}

static inline int fill_t_8_hexs(char *buffer, uint32_t num)
{
    char *s = buffer;

    fill_t_4_hexs(s, num >> 16);
    fill_t_4_hexs(s + 4, num & 0xffff);
    return 8;
}

static inline int fill_1_2_hexs(char *buffer, uint32_t num)
{
    char *s = buffer;
    uint32_t q = num >> 4;
    uint32_t r = num & 0xf;

    if (q)
        *s++ = hex_char_lut[q];
    *s++ = hex_char_lut[r];
    return s - buffer;
}

static inline int fill_1_4_hexs(char *buffer, uint32_t num)
{
    char *s = buffer;
    uint32_t q = num >> 8;
    uint32_t r = num & 0xff;

    if (!q) {
        return fill_1_2_hexs(s, num);
    } else {
        s += fill_1_2_hexs(s, q);
        s += fill_t_2_hexs(s, r);
    }
    return s - buffer;
}

static inline int fill_1_8_hexs(char *buffer, uint32_t num)
{
    char *s = buffer;
    uint32_t q = num >> 16;
    uint32_t r = num & 0xffff;

    if (!q) {
        return fill_1_4_hexs(s, num);
    } else {
        s += fill_1_4_hexs(s, q);
        s += fill_t_4_hexs(s, r);
    }
    return s - buffer;
}

int jnum_htoa(uint32_t num, char *buffer)
{
    char *s = buffer;

    *s++ = '0';
    *s++ = 'x';
    s += fill_1_8_hexs(s, num);

    *s = '\0';
    return s - buffer;
}

int jnum_lhtoa(uint64_t num, char *buffer)
{
    char *s = buffer;
    uint32_t q = (uint32_t)(num >> 32);
    uint32_t r = (uint32_t)(num & 0xffffffff);

    *s++ = '0';
    *s++ = 'x';
    if (!q) {
        s += fill_1_8_hexs(s, r);
    } else {
        s += fill_1_8_hexs(s, q);
        s += fill_t_8_hexs(s, r);
    }

    *s = '\0';
    return s - buffer;
}

/*
# python to get lut

def print_pow10(step):
    minv = 0
    nunit = 0
    nexpm = 0

    maxv = 0
    punit = 0
    pexpm = 0

    if step == 8:
        # compute negative array
        # 1 * (2 ** -8) = 0.0390625 * (10 ** -1)
        # 390625 = 0.0390625 * (10 ** 7)
        # 136 = (1022 + 52 + 11) / 8 + 1
        minv = -136
        nunit = 390625
        nexpm = 7
        # compute positive array
        # 1 * (2 ** 8) = 25.6 * (10 ** 1)
        # 256 = 25.6 * (10 ** 1)
        # 122 = (1023 - 52) / 8 + 1
        maxv = 122
        punit = 256
        pexpm = 1
    elif step == 4:
        # compute negative array
        # 1 * (2 ** -4) = 0.625 * (10 ** -1)
        # 625 = 0.625 * (10 ** 3)
        # 272 = (1022 + 52 + 11) / 4 + 1
        minv = -272
        nunit = 625
        nexpm = 3
        # compute positive array
        # 1 * (2 ** 4) = 1.6 * (10 ** 1)
        # 16 = 1.6 * (10 ** 1)
        # 243 = (1023 - 52) / 4 + 1
        maxv = 243
        punit = 16
        pexpm = 1
    elif step == 2:
        # compute negative array
        # 1 * (2 ** -2) = 2.5 * (10 ** -1)
        # 25 = 2.5 * (10 ** 1)
        # 543 = (1022 + 52 + 11) / 2 + 1
        minv = -543
        nunit = 25
        nexpm = 1
        # compute positive array
        # 1 * (2 ** 2) = 0.4 * (10 ** 1)
        # 4 = 0.4 * (10 ** 1)
        # 486 = (1023 - 52) / 2 + 1
        maxv = 486
        punit = 4
        pexpm = 1
    else:
        print('Only step 8 or 4 or 2 are supported.')
        return -1

    mul_lut = []
    exp_lut = []
    pos = 0
    for i in range(minv, maxv + 1):
        if i < 0:
            j = -i
            unit = nunit
            expm = nexpm
        else:
            j = i
            unit = punit
            expm = pexpm

        val = unit ** j
        bit = len(str(val))
        mul = (1 << 64) * val // (10 ** bit)
        rem = (1 << 64) * val % (10 ** bit)
        if rem:
            mul += 1
        exp = j * expm - bit

        mul_lut.append(mul)
        if step == 8:
            if exp < 0:
                exp_lut.append(-exp)
                if pos == 0:
                    pos = i - minv
                    print('#define POW10_EXP_NUT_POS    %d // From the pos, the value of pow10_exp_lut is negitive.' % (pos))
            else:
                exp_lut.append(exp)

        elif step == 4:
            exp_lut.append(exp)
        elif step == 2:
            if exp < 0:
                exp_lut.append(-exp)
            else:
                exp_lut.append(exp)
                if pos == 0:
                    pos = i - minv
                    print('#define POW10_EXP_NUT_POS    %d // From the pos, the value of pow10_exp_lut is positive.' % (pos))

    print('#define POW10_LUT_MIN_IDX    %d' % (minv))
    print('#define POW10_LUT_MAX_IDX    %d' % (maxv))
    last_idx = maxv - minv

    print('static const uint64_t pow10_mul_lut[POW10_LUT_MAX_IDX - POW10_LUT_MIN_IDX + 1] = {', end='')
    for i in range(last_idx + 1):
        if i % 5 == 0:
            print()
            print('    ', end='')
        print('0x%016x' % (mul_lut[i]), end='')
        if i != last_idx:
            print(', ', end='');
        else:
            print()
            print('};')

    print()
    print('static const %s pow10_exp_lut[POW10_LUT_MAX_IDX - POW10_LUT_MIN_IDX + 1] = {' % ('int8_t' if step == 4 else 'uint8_t'), end='')
    for i in range(last_idx + 1):
        if i % 20 == 0:
            print()
            print('    ', end='')
        print('%-3d' % (exp_lut[i]), end='')

        if i != last_idx:
            print(', ', end='');
        else:
            print()
            print('};')

print_pow10(4)
*/

#define POW10_LUT_MIN_IDX    -272
#define POW10_LUT_MAX_IDX    243
static const uint64_t pow10_mul_lut[POW10_LUT_MAX_IDX - POW10_LUT_MIN_IDX + 1] = {
    0x4d32a036a252e482, 0x7b84338a9d516d9d, 0xc5a05277621be294, 0x1f9ec583bdc70589, 0x3297a26c62d808db,
    0x50f29d7a37c00e2a, 0x81842f29f2cce376, 0xcf39e50feae16bf0, 0x2127fbb0a075fccb, 0x350cc5e767232e11,
    0x54e13ca571d1e34e, 0x87cec76f1c830549, 0xd94ad8b1c7380875, 0x22c44ba19083d865, 0x37a0790280d2f3d4,
    0x5900c19d9aeb1fba, 0x8e679c2f5e44ff90, 0xe3d8f9e563a198e6, 0x2474a2dd05b374a0, 0x3a5437c8091f2100,
    0x5d538c7341cb67ff, 0x95527a5202df0ccc, 0xeeea5d5004981479, 0x2639fa7333ef5f70, 0x3d2990b8531898b3,
    0x61dc1ac084f42784, 0x9c935e00d4b9d8d3, 0xfa856334878fc151, 0x2815578d865470da, 0x402225af3d53e7c3,
    0x669d0918621fd938, 0xa42e74f3d032f526, 0x1a44df832b8d45f2, 0x2a07cc05127ba31d, 0x433facd4ea5f6b61,
    0x6b991487dd65789a, 0xac2820d9623bf42a, 0x1b8b8a6038ad6ebf, 0x2c1277005aaf1798, 0x4683f19a2ab1bf5a,
    0x70d31c29dde93229, 0xb484f9dc9641e9db, 0x1ce2137f74338194, 0x2e368598b9ec0286, 0x49f0d5c129799da3,
    0x764e22cea8c295d2, 0xbd49d14aa79dbc83, 0x1e494034e79e5b9a, 0x30753387d8fd5f5d, 0x4d885272f4c89895,
    0x7c0d50b7ee0dc0ee, 0xc67bb4597ce2ce49, 0x1fc1df6a7a61ba9b, 0x32cfcbdd909c5dc5, 0x514c796280fa2fa2,
    0x8213f56a67f6b29c, 0xd01fef10a657842d, 0x214cca1724dacd78, 0x3547a9bea15e158d, 0x553f75fdcefcef47,
    0x8865899617fb1872, 0xda3c0f568cc4f3e9, 0x22eae3bbed902707, 0x37de392caf4d0b3e, 0x59638eade54811fd,
    0x8f05b1163ba6832e, 0xe4d5e82392a40516, 0x249d1ae6f8be154c, 0x3a94f7d7f4635545, 0x5dbb262653d22208,
    0x95f83d0a1fb69cda, 0xeff394dcff8a948f, 0x266469bcf5afc5da, 0x3d6d75fb22b2d629, 0x6248bcc5045156a8,
    0x9d412e0806e88aa6, 0xfb9b7cd9a4a7443d, 0x2841d689391085cd, 0x40695741f4e73c7a, 0x670ef2032171fa5d,
    0xa4e4b66b68b65d61, 0x1a6208b506839410, 0x2a367454d738ece6, 0x438a53baf1f4ae3d, 0x6c1085f7e9877d2e,
    0xace73cbfdc0bfb7c, 0x1baa1e332d728ea4, 0x2c4363851584176c, 0x46d238d4ef39bf13, 0x71505aee4b8f981e,
    0xb54d5e4a127f59c9, 0x1d022390f8b83754, 0x2e69d2818df38bb9, 0x4a42ea68e31f45f4, 0x76d1770e38320987,
    0xbe1bf1b059e9a8d7, 0x1e6adefd7f06aa60, 0x30aafe6264d77700, 0x4dde63d0a158be66, 0x7c97061a9bc130a3,
    0xc75809c42c684dd2, 0x1fe52048590672da, 0x330833a6f4d71e2a, 0x51a6b90b21583043, 0x82a45b450226b39d,
    0xd106f86e69d785c8, 0x2171c159589d5d16, 0x3582cef55a9561bd, 0x559e17eef755692e, 0x88fcf317f22241e3,
    0xdb2e51bfe9d0696b, 0x2311a6ae10ee2559, 0x381c3de34e49d55b, 0x59c6c96bb076222b, 0x8fa475791a569d11,
    0xe5d3ef282a242e82, 0x24c5bfdd7761f2f7, 0x3ad5ffc8bf031e57, 0x5e2332dacb38308b, 0x969eb7c47859e744,
    0xf0fdf2d3f3c30ba0, 0x268f0821e98fd8e7, 0x3db1a69ca8e627d7, 0x62b5d7610e3d0c8c, 0x9defbf01b061adac,
    0xfcb2cb35e702af79, 0x286e86e9e7858cb8, 0x40b0d7dca5a27abf, 0x678159610903f798, 0xa59bc234db398c26,
    0x1a7f5245e5a2cebf, 0x2a65506fd5d14acb, 0x43d54d7fbc821144, 0x6c887bff94034ed3, 0xada72ccc20054aea,
    0x1bc8d3f7b3340bfd, 0x2c7486591eb9acc8, 0x4720d6f4fdf5e13f, 0x71ce24bb2fefcecb, 0xb616a12b7fe617ab,
    0x1d22573a28f19d63, 0x2e9d585d0e4f6238, 0x4a955a2e7d4bd05a, 0x77555d172edfb3c3, 0xbeeefb584aff8604,
    0x1e8ca3185deb719b, 0x30e104f3c978b5c4, 0x4e34d4b9425abc6c, 0x7d21545b9d5dfa47, 0xc83553c5c8965d3e,
    0x200888489af9569a, 0x3340da0dc4c22429, 0x52015ce2d469d374, 0x8335616aed761f20, 0xd1ef0244af236500,
    0x2196e1a496e6f171, 0x35be35d424a4b581, 0x55fd22ed076def35, 0x899504ae72497ebb, 0xdc21a1171d42645e,
    0x233894a789cd2ec8, 0x385a8772761517a6, 0x5a2a7250bcee8c3c, 0x9043ea1ac7e41393, 0xe6d3102ad96cec1e,
    0x24ee91f2603a6338, 0x3b174fea33909ec0, 0x5e8bb3105280fe00, 0x9745eb4d50ce6333, 0xf209787bb47d6b85,
    0x26b9d5d65a5181d8, 0x3df622f09082695a, 0x63236b1a80d0a88f, 0x9e9f11c4014dda7f, 0xfdcb4fa002162a64,
    0x289b68e666bbddd3, 0x40f8a7d70ac62fb8, 0x67f43fbe77a37f8c, 0xa6539930bf6bff46, 0x1a9cbc59b83a3d53,
    0x2a94608f8d29fbb8, 0x44209a7f48432c5a, 0x6d00f7320d3846f5, 0xae67f1e9aec07188, 0x1be7abd3781eca7d,
    0x2ca5dfb8c03143fa, 0x476fcc5acd1b9ff7, 0x724c7a2ae1c5ccbe, 0xb6e0c377cfa2e12f, 0x1d42aea2879f2e45,
    0x2ed1176a72984a08, 0x4ae825771dc07673, 0x77d9d58b62cd8a52, 0xbfc2ef456ae276e9, 0x1eae8caef261aca1,
    0x3117477e509c4767, 0x4e8ba596e760723e, 0x7dac3c24a5671d30, 0xc913936dd571c84d, 0x202c1796b182d85f,
    0x3379bf57826af3ca, 0x525c6558d0ab1faa, 0x83c7088e1aab65dc, 0xd2d80db02aabd62c, 0x21bc2b266d3a36c0,
    0x35f9dea3e1f6bdff, 0x565c976c9cbdfccc, 0x8a2dbf142dfcc7ac, 0xdd15fe86affad913, 0x235fadd81c2822bc,
    0x3899162693736ac6, 0x5a8e89d75252446f, 0x90e40fbeea1d3a4b, 0xe7d34c64a9c85d45, 0x25179157c93ec73f,
    0x3b58e88c75313eca, 0x5ef4a74721e86477, 0x97edd871cfda3a57, 0xf316271c7fc3908b, 0x26e4d30eccc3215e,
    0x3e3aeb4ae1383563, 0x63917877cec0556c, 0x9f4f2726179a2246, 0xfee50b7025c36a09, 0x28c87cb5c89a2572,
    0x4140c78940f6a250, 0x6867a5a867f103b3, 0xa70c3c40a64e6c52, 0x1aba4714957d300e, 0x2ac3a4edbbfb8015,
    0x446c3b15f9926688, 0x6d79f82328ea3da7, 0xaf298d050e4395d7, 0x1c06a5ec5433c60e, 0x2cd76fe086b93ce3,
    0x47bf19673df52e38, 0x72cb5bd86321e38d, 0xb7abc627050305ae, 0x1d6329f1c35ca4c0, 0x2f050fe938943acd,
    0x4b3b4ca85a86c47b, 0x785ee10d5da46d91, 0xc097ce7bc90715b4, 0x1ed09bead87c0379, 0x314dc6448d9338c2,
    0x4ee2d6d415b85acf, 0x7e37be2022c0914c, 0xc9f2c9cd04674edf, 0x204fce5e3e250262, 0x33b2e3c9fd0803cf,
    0x52b7d2dcc80cd2e4, 0x84595161401484a0, 0xd3c21bcecceda100, 0x21e19e0c9bab2400, 0x3635c9adc5dea000,
    0x56bc75e2d6310000, 0x8ac7230489e80000, 0xde0b6b3a76400000, 0x2386f26fc1000000, 0x38d7ea4c68000000,
    0x5af3107a40000000, 0x9184e72a00000000, 0xe8d4a51000000000, 0x2540be4000000000, 0x3b9aca0000000000,
    0x5f5e100000000000, 0x9896800000000000, 0xf424000000000000, 0x2710000000000000, 0x3e80000000000000,
    0x6400000000000000, 0xa000000000000000, 0x199999999999999a, 0x28f5c28f5c28f5c3, 0x4189374bc6a7ef9e,
    0x68db8bac710cb296, 0xa7c5ac471b478424, 0x1ad7f29abcaf4858, 0x2af31dc4611873c0, 0x44b82fa09b5a52cc,
    0x6df37f675ef6eae0, 0xafebff0bcb24aaff, 0x1c25c268497681c3, 0x2d09370d42573604, 0x480ebe7b9d58566d,
    0x734aca5f6226f0ae, 0xb877aa3236a4b44a, 0x1d83c94fb6d2ac35, 0x2f394219248446bb, 0x4b8ed0283a6d3df8,
    0x78e480405d7b9659, 0xc16d9a0095928a28, 0x1ef2d0f5da7dd8ab, 0x318481895d962777, 0x4f3a68dbc8f03f25,
    0x7ec3daf941806507, 0xcad2f7f5359a3b3f, 0x2073accb12d0ff3e, 0x33ec47ab514e652f, 0x5313a5dee87d6eb1,
    0x84ec3c97da624ab5, 0xd4ad2dbfc3d07788, 0x22073a8515171d5e, 0x3671f73b54f1c896, 0x571cbec554b60dbc,
    0x8b61313bbabce2c7, 0xdf01e85f912e37a4, 0x23ae629ea696c139, 0x391704310a8acec2, 0x5b5806b4ddaae469,
    0x9226712162ab070e, 0xe9d71b689dde71b0, 0x256a18dd89e626ac, 0x3bdcf495a9703de0, 0x5fc7edbc424d2fcc,
    0x993fe2c6d07b7fac, 0xf53304714d9265e0, 0x273b5cdeedb10610, 0x3ec56164af81a34c, 0x646f023ab2690546,
    0xa0b19d2ab70e6ed7, 0x19b604aaaca62637, 0x29233aaaadd6a38b, 0x41d1f7777c8a9f45, 0x694ff258c7443208,
    0xa87fea27a539e9a6, 0x1af5bf109550f22f, 0x2b22cb4dbbb4b6b2, 0x4504787c5f878ab6, 0x6e6d8d93cc0c1123,
    0xb0af48ec79ace838, 0x1c45016d841baa47, 0x2d3b357c0692aa0b, 0x485ebbf9a41ddcdd, 0x73cac65c39c96162,
    0xb94470938fa89bcf, 0x1da48ce468e7c703, 0x2f6dae3a4172d804, 0x4be2b05d35848cd3, 0x796ab3c855a0e152,
    0xc24452da229b021c, 0x1f152bf9f10e8fb3, 0x31bb798fe8174c51, 0x4f925c1973587a1c, 0x7f50935bebc0c35f,
    0xcbb41ef979346bcb, 0x2097b309321cde0c, 0x3425eb41e9c7c9ad, 0x536fdecfdc72dc48, 0x857fcae62d8493a6,
    0xd59944a37c0752a3, 0x222d00bdff5d54e7, 0x36ae679665622172, 0x577d728a3bd03582, 0x8bfbea76c619ef37,
    0xdff9772470297ebe, 0x23d5fe9530aa7aae, 0x39566421e7772ab0, 0x5bbd6d030bf1dde6, 0x92c8ae6b464fc970,
    0xeadab0aba3b2dbe6, 0x2593a163246e8996, 0x3c1f689ea0b0dc23, 0x603240fdcde7c69d, 0x99ea0196163fa42f,
    0xf64335bcf065d37e, 0x2766e9e0ca4dbb71, 0x3f0b0fce107c5f1a, 0x64de7fb01a60982a, 0xa163ff802a3426a9,
    0x19d28f47b4d524e8, 0x2950e53f87bb6e40, 0x421b0865a5f8b066, 0x69c4da3c3cc11a3d, 0xa93af6c6c79b5d2e,
    0x1b13ac9aaf4c0ee9, 0x2b52adc44bace4a8, 0x45511606df7b0773, 0x6ee8233e325e7251, 0xb1736b96b6fd83b4,
    0x1c6463225ab7ec1d, 0x2d6d6b6a2abfe02f, 0x48af1243779966b1, 0x744b506bf28f0ab4, 0xba121a4650e4ddec,
    0x1dc574d80cf16b30, 0x2fa2548ce182451a, 0x4c36edae359d3b5c, 0x79f17c49ef61f894, 0xc31bfa0fe5698db9,
    0x1f37ad21436d0c70, 0x31f2ae9b9f14e0b3, 0x4feab0f8fe87cdea, 0x7fdde7f4ca72e310, 0xcc963fee10b7d1b4,
    0x20bbe144cf799232, 0x345fced47f28e9e9, 0x53cc7e20cb74a974, 0x8613fd0145877586, 0xd686619ba27255a3,
    0x2252f0e5b39769dd, 0x36eb1b091f58a961, 0x57de91a832277568, 0x8c974f7383725574, 0xe0f218b8d25088b9,
    0x23fdc683f8b0b9b8, 0x39960a6cc11ac2bf, 0x5c2343e134f79dfe, 0x936b9fcebb25c996, 0xebdf661791d60f57,
    0x25bd5803c569edfa, 0x3c62266c6f0fe329, 0x609d0a4718196b74, 0x9a94dd3e8cf578ba, 0xf7549530e188c129,
    0x2792a73b055d8f8c, 0x3f510b91a22f4c13, 0x654e78e9037ee01e, 0xa21727db38cb0030, 0x19ef3993b72ab85a,
    0x297ec285f1ddf3c3, 0x42646a6fe9631f9e, 0x6a3a43e642383296, 0xa9f6d30a038d1dbd, 0x1b31bb5dc320d18f,
    0x2b82c562d1ce1c18, 0x459e089e1c7cf9c0, 0x6f6340fcfa618f99, 0xb23867fb2a35b28e, 0x1c83e7ad4e6efdda,
    0x2d9fd9154a4b2fc3, 0x48ffc1bbaa11e604, 0x74cc692c434fd66c, 0xbae0a846d2195713, 0x1de6815302e5559d,
    0x2fd735519e3bbc2e, 0x4c8b888296c5f9e3, 0x7a78da6a8ad65c9e, 0xc3f490aa77bd60fd, 0x1f5a549627a36bae,
    0x322a20f03f6bdf7d, 0x504367e6cbdfcbfa, 0x806bd9714632dff7, 0xcd795be870516657, 0x20e037aa4f692f19,
    0x3499f2aa18a84b5a, 0x542984435aa6def6, 0x86a8d39ef77164bd, 0xd77485cb25823ac8, 0x22790b2abe5246d9,
    0x372811ddfd50715a, 0x58401c96621a4ef7, 0x8d3360f09cf6e4be, 0xe1ebce4dc7f16dfc, 0x2425ba9bce122614,
    0x39d5f75fb01d09ba, 0x5c898bcc4cfb42c3, 0x940f4613ae5ed137, 0xece53cec4a314ebe, 0x25e73cf29b3b16d7,
    0x3ca52e50f85e8af2, 0x61084a1b26fdab1c, 0x9b407691d7fc44f9, 0xf867241c8cc6d4c1, 0x27be952349b969b9,
    0x3f97550542c242c1, 0x65beee6ed136d135, 0xa2cb1717b52481ee, 0x1a0c03b1df8af612, 0x29acd2b63277f01c,
    0x42ae1df050bfe694, 0x6ab02fe6e79970ec, 0xaab37fd7d8f58179, 0x1b4feb7eb212cd0a, 0x2bb31264501e14dc,
    0x45eb50a08030215f, 0x6fdee76733803565, 0xb2fe3f0b8599ef08, 0x1ca38f350b22de91, 0x2dd27ebb4504974e,
    0x4950cac53b3a8bb0, 0x754e113b91f745e6, 0xbbb01b9283253ca3, 0x1e07b27dd78b13f2, 0x300c50c958de864f,
    0x4ce0814227ca707e, 0x7b00ced03faa4d96, 0xc4ce17b399107c23, 0x1f7d228322baf525, 0x3261d0d1d12b21d4,
    0x509c814fb511cfba, 0x80fa687f881c7f8f, 0xce5d73ff402d98e4, 0x2104b66647b56025, 0x34d4570a0c5566a1,
    0x5486f1a9ad557102, 0x873e4f75e2224e69, 0xd863b256369d4a41, 0x229f4fbbdfc73f15, 0x37654c5fcc71fe88,
    0x58a213cc7a4ffda6, 0x8dd01fad907ffc3c, 0xe2e69915b3fff9fa, 0x244ddb0db6666570, 0x3a162b4923d708b3,
    0x5cf04541d2f1a784, 0x94b3a202eb1c3f3a, 0xedec366b11c6cb90, 0x261150630d159136, 0x3ce8809e7b55b523,
    0x617400fd9222bb6b, 0x9becce62836ac578, 0xf97ae3d0d2446f26, 0x27eab3cf7dcd826d, 0x3fddec7f2faf3714,
    0x662fe0cb7f7ebe87
};

static const int8_t pow10_exp_lut[POW10_LUT_MAX_IDX - POW10_LUT_MIN_IDX + 1] = {
    55 , 55 , 55 , 54 , 54 , 54 , 54 , 54 , 53 , 53 , 53 , 53 , 53 , 52 , 52 , 52 , 52 , 52 , 51 , 51 ,
    51 , 51 , 51 , 50 , 50 , 50 , 50 , 50 , 49 , 49 , 49 , 49 , 48 , 48 , 48 , 48 , 48 , 47 , 47 , 47 ,
    47 , 47 , 46 , 46 , 46 , 46 , 46 , 45 , 45 , 45 , 45 , 45 , 44 , 44 , 44 , 44 , 44 , 43 , 43 , 43 ,
    43 , 43 , 42 , 42 , 42 , 42 , 42 , 41 , 41 , 41 , 41 , 41 , 40 , 40 , 40 , 40 , 40 , 39 , 39 , 39 ,
    39 , 38 , 38 , 38 , 38 , 38 , 37 , 37 , 37 , 37 , 37 , 36 , 36 , 36 , 36 , 36 , 35 , 35 , 35 , 35 ,
    35 , 34 , 34 , 34 , 34 , 34 , 33 , 33 , 33 , 33 , 33 , 32 , 32 , 32 , 32 , 32 , 31 , 31 , 31 , 31 ,
    31 , 30 , 30 , 30 , 30 , 30 , 29 , 29 , 29 , 29 , 28 , 28 , 28 , 28 , 28 , 27 , 27 , 27 , 27 , 27 ,
    26 , 26 , 26 , 26 , 26 , 25 , 25 , 25 , 25 , 25 , 24 , 24 , 24 , 24 , 24 , 23 , 23 , 23 , 23 , 23 ,
    22 , 22 , 22 , 22 , 22 , 21 , 21 , 21 , 21 , 21 , 20 , 20 , 20 , 20 , 20 , 19 , 19 , 19 , 19 , 18 ,
    18 , 18 , 18 , 18 , 17 , 17 , 17 , 17 , 17 , 16 , 16 , 16 , 16 , 16 , 15 , 15 , 15 , 15 , 15 , 14 ,
    14 , 14 , 14 , 14 , 13 , 13 , 13 , 13 , 13 , 12 , 12 , 12 , 12 , 12 , 11 , 11 , 11 , 11 , 11 , 10 ,
    10 , 10 , 10 , 10 , 9  , 9  , 9  , 9  , 8  , 8  , 8  , 8  , 8  , 7  , 7  , 7  , 7  , 7  , 6  , 6  ,
    6  , 6  , 6  , 5  , 5  , 5  , 5  , 5  , 4  , 4  , 4  , 4  , 4  , 3  , 3  , 3  , 3  , 3  , 2  , 2  ,
    2  , 2  , 2  , 1  , 1  , 1  , 1  , 1  , 0  , 0  , 0  , 0  , -1 , -1 , -1 , -1 , -1 , -2 , -2 , -2 ,
    -2 , -2 , -3 , -3 , -3 , -3 , -3 , -4 , -4 , -4 , -4 , -4 , -5 , -5 , -5 , -5 , -5 , -6 , -6 , -6 ,
    -6 , -6 , -7 , -7 , -7 , -7 , -7 , -8 , -8 , -8 , -8 , -8 , -9 , -9 , -9 , -9 , -9 , -10, -10, -10,
    -10, -11, -11, -11, -11, -11, -12, -12, -12, -12, -12, -13, -13, -13, -13, -13, -14, -14, -14, -14,
    -14, -15, -15, -15, -15, -15, -16, -16, -16, -16, -16, -17, -17, -17, -17, -17, -18, -18, -18, -18,
    -18, -19, -19, -19, -19, -19, -20, -20, -20, -20, -21, -21, -21, -21, -21, -22, -22, -22, -22, -22,
    -23, -23, -23, -23, -23, -24, -24, -24, -24, -24, -25, -25, -25, -25, -25, -26, -26, -26, -26, -26,
    -27, -27, -27, -27, -27, -28, -28, -28, -28, -28, -29, -29, -29, -29, -29, -30, -30, -30, -30, -31,
    -31, -31, -31, -31, -32, -32, -32, -32, -32, -33, -33, -33, -33, -33, -34, -34, -34, -34, -34, -35,
    -35, -35, -35, -35, -36, -36, -36, -36, -36, -37, -37, -37, -37, -37, -38, -38, -38, -38, -38, -39,
    -39, -39, -39, -39, -40, -40, -40, -40, -41, -41, -41, -41, -41, -42, -42, -42, -42, -42, -43, -43,
    -43, -43, -43, -44, -44, -44, -44, -44, -45, -45, -45, -45, -45, -46, -46, -46, -46, -46, -47, -47,
    -47, -47, -47, -48, -48, -48, -48, -48, -49, -49, -49, -49, -49, -50, -50, -50
};

static inline int32_t fill_significand(char *buffer, uint64_t digits, int32_t *ptz)
{
    char *s = buffer;

    uint32_t q = (uint32_t)(digits / 100000000);
    uint32_t r = (uint32_t)(digits - (uint64_t)q * 100000000);

    uint32_t q1 = FAST_DIV10000(q);
    uint32_t r1 = q - q1 * 10000;

    uint32_t q2 = FAST_DIV100(q1);
    uint32_t r2 = q1 - q2 * 100;

    memcpy(s, &ch_100_lut[q2<<1], 2);
    memcpy(s + 2, &ch_100_lut[r2<<1], 2);
    if (!r2) {
        *ptz = tz_100_lut[q2] + 2;
    } else {
        *ptz = tz_100_lut[r2];
    }

    if (!r1) {
        memset(s + 4, '0', 4);
        *ptz += 4;
    } else {
        fill_t_4_digits(s + 4, r1, ptz);
    }

    if (!r) {
        memset(s + 8, '0', 8);
        *ptz += 8;
    } else {
        fill_t_8_digits(s + 8, r, ptz);
    }

    return 16;
}

static inline int32_t ldouble_convert(diy_fp_t *v, char *buffer, int32_t *vnum_digits)
{
    const uint8_t s_lut[4] = {8, 9, 6, 7};
    int32_t s = s_lut[v->e & 0x3];
    v->f = (v->f << s) + (1 << (s - 1));
    v->e -= s;
    v->e >>= 2;

    uint32_t index = v->e - POW10_LUT_MIN_IDX;
    uint64_t pow10 = pow10_mul_lut[index];

    v->e -= pow10_exp_lut[index];
    v->f = u128_mul(pow10, v->f).hi;
    uint32_t delta = pow10 >> (64 - s);
    uint32_t remainder = 0;
    uint64_t orig = v->f;

    if (orig < 100000000000000000llu) {
        v->f = orig / 10;
        remainder = orig - v->f * 10;
        v->e += 1;
        if (delta < 20) {
            if ((v->f & 1) && (10 + remainder <= delta)) {
                v->f -= 1;
            }
        } else {
            uint16_t t = v->f % 10;
            if (t * 10 + remainder <= delta) {
                v->f -= t;
            }
        }
    } else if (orig < 1000000000000000000llu) {
        v->f = orig / 100;
        remainder = orig - v->f * 100;
        v->e += 2;
        if (delta < 200) {
            if ((v->f & 1) && (100 + remainder <= delta)) {
                v->f -= 1;
            }
        } else {
            uint16_t t = v->f % 10;
            if (t * 100 + remainder <= delta) {
                v->f -= t;
            }
        }
    } else {
        v->f = orig / 1000;
        remainder = orig - v->f * 1000;
        v->e += 3;
        if ((v->f & 1) && (1000 + remainder <= delta)) {
            v->f -= 1;
        }
    }

    int32_t num_digits, trailing_zeros;
    num_digits = fill_significand(buffer, v->f, &trailing_zeros);
    *vnum_digits = num_digits - trailing_zeros;
    return num_digits;
}

/*
# python to get define

def print_define():
    base_shift = 6
    base_exp = -1074 - base_shift

    val = 5 ** (-base_exp)
    bit = len(str(val))

    smul = (1 << 32) * val // (10 ** bit)
    srem = (1 << 32) * val % (10 ** bit)
    if srem:
        smul += 1

    lmul = (1 << 64) * val // (10 ** bit)
    lrem = (1 << 64) * val % (10 ** bit)
    if lrem:
        lmul += 1

    delta = (1 << base_shift) * smul >> 32
    exp = base_exp + bit
    split = 64 - base_shift - (len(bin(smul)) - 2)

    print('#define MIM_POW10_EXP      %d'   % (exp))
    print('#define MIM_POW10_DELTA    %d'   % (delta))
    print('#define MIM_POW10_SPLIT    %d'   % (split))
    print('#define MIM_POW10_SHIFT    %d'   % (base_shift))
    print('#define MIM_POW10_SMALL_M  0x%x' % (smul))
    print('#define MIM_POW10_LARGE_M  0x%x' % (lmul))

print_define()
*/

static inline int32_t ldouble_convert_n(diy_fp_t *v, char *buffer, int32_t *vnum_digits)
{
#define MIM_POW10_EXP      -325
#define MIM_POW10_DELTA    49
#define MIM_POW10_SPLIT    26
#define MIM_POW10_SHIFT    6
#define MIM_POW10_SMALL_M  0xc5a05278
#define MIM_POW10_LARGE_M  0xc5a05277621be294

    v->f = (v->f << MIM_POW10_SHIFT) + (1 << (MIM_POW10_SHIFT - 1));
    v->e = MIM_POW10_EXP;

    if (v->f <= (1 << MIM_POW10_SPLIT)) {
        v->f = v->f * MIM_POW10_SMALL_M >> 32;
    } else {
        v->f = u128_mul(v->f, MIM_POW10_LARGE_M).hi;
    }

    uint64_t orig = v->f;
    uint64_t remainder;
    v->f = orig / 100;
    remainder = orig - v->f * 100;

    if (v->f == 0) {
        v->e += 1;
        remainder = ch_100_lut[remainder << 1] - '0';
    } else {
        if (remainder <= MIM_POW10_DELTA) {
            v->e += 2;
            remainder = 0;
        } else {
            v->e += 1;
            remainder = ch_100_lut[remainder << 1] - '0';
        }
    }

    int32_t num_digits = 0, trailing_zeros = 0;
    if (v->f) {
        num_digits = fill_1_16_digits(buffer, v->f, &trailing_zeros);
    }
    if (remainder) {
        buffer[num_digits++] = remainder + '0';
        trailing_zeros = 0;
    }
    *vnum_digits = num_digits - trailing_zeros;
    return num_digits;
}

static inline int32_t fill_exponent(int32_t K, char *buffer)
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

static inline char* ldouble_format(char *buffer, int32_t num_digits, int32_t vnum_digits, int32_t decimal_point)
{
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
            if (decimal_point > num_digits) {
                memmove(buffer, buffer + 1, num_digits);
                memset(buffer + num_digits, '0', decimal_point - num_digits);
            } else {
                memmove(buffer, buffer + 1, decimal_point);
            }
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

int jnum_dtoa(double num, char *buffer)
{
    diy_fp_t v;
    char *s = buffer;
    union {double d; uint64_t n;} u = {.d = num};
    int32_t signbit = u.n >> (DIY_SIGNIFICAND_SIZE - 1);
    int32_t exponent = (u.n & DP_EXPONENT_MASK) >> DP_SIGNIFICAND_SIZE; /* Exponent */
    uint64_t significand = u.n & DP_SIGNIFICAND_MASK; /* Mantissa */
    int32_t num_digits, vnum_digits;

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

    case 0:
        if (significand) {
            /* no-normalized double */
            v.f = significand; /* Non-normalized double doesn't have a extra integer bit for Mantissa */
            v.e = 1 - DP_EXPONENT_OFFSET - DP_SIGNIFICAND_SIZE; /* Fixed Exponent: -1022, divisor of Mantissa: pow(2,52) */
            num_digits = ldouble_convert_n(&v, s + 1, &vnum_digits);
        } else {
            memcpy(s, "0.0", 4);
            return signbit + 3;
        }
        break;

    default:
        /* normalized double */
        v.f = significand + DP_HIDDEN_BIT; /* Normalized double has a extra integer bit for Mantissa */
        v.e = exponent - DP_EXPONENT_OFFSET - DP_SIGNIFICAND_SIZE; /* Exponent offset: -1023, divisor of Mantissa: pow(2,52) */

        if (v.e >= -DP_SIGNIFICAND_SIZE && v.e <= 0 \
            && (v.f & (((uint64_t)1 << -v.e) - 1)) == 0) {
            /* small integer. */
            int32_t tz, n;
            n = fill_1_16_digits(s, v.f >> -v.e, &tz);
            memcpy(s + n, ".0", 3);
            return n + 2 + signbit;
        }
        if (v.e > 0 && v.e <= (63 - DP_SIGNIFICAND_SIZE)) {
            /* big integer. */
            int32_t tz, n;
            n = fill_1_20_digits(s, v.f << v.e, &tz);
            memcpy(s + n, ".0", 3);
            return n + 2 + signbit;
        }
        /* (-1022 - 52) <= e <= (1023 - 52) */
        num_digits = ldouble_convert(&v, s + 1, &vnum_digits);
        break;
    }

    s = ldouble_format(s, num_digits, vnum_digits, num_digits + v.e);
    *s = '\0';
    return s - buffer;
}

static int jnum_parse_hex(const char *str, jnum_type_t *type, jnum_value_t *value)
{
    const char *s = str;
    char c;
    uint64_t m = 0;

    s += 2;
    while ((s - str) < 18) {
        switch ((c = *s)) {
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            c -= '0';
            break;
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
            c -= 'a' - 10;
            break;
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
            c -= 'A' - 10;
            break;
        default:
            goto end;
        }
        m = (m << 4) + c;
        ++s;
    }

end:
    if (m > 4294967295U /*UINT_MAX*/) {
        *type = JNUM_LHEX;
        value->vlhex = m;
    } else {
        *type = JNUM_HEX;
        value->vhex = (uint32_t)m;
    }

    return s - str;
}

/*
# python to get lut
maxn = 323 + 309
print('static const double exponent_lut[%d] = {' % (maxn), end='')
for i in range(-323, 309):
    j = i + 323
    if j % 10 == 0:
        print()
        print('    ', end='')
    if i == 0:
        print('1     ', end='')
    else:
        print('1e%+-4d' % (i), end='')
    if j != maxn - 1:
        print(', ', end='');
    else:
        print()
        print('};')
*/

#define MIN_NEG_EXP     -323
#define EXP_LUT_NUM     632
static double jnum_convert_double(uint64_t neg, uint64_t m, int32_t k, int32_t i)
{
    static const double exponent_lut[EXP_LUT_NUM] = {
        1e-323, 1e-322, 1e-321, 1e-320, 1e-319, 1e-318, 1e-317, 1e-316, 1e-315, 1e-314,
        1e-313, 1e-312, 1e-311, 1e-310, 1e-309, 1e-308, 1e-307, 1e-306, 1e-305, 1e-304,
        1e-303, 1e-302, 1e-301, 1e-300, 1e-299, 1e-298, 1e-297, 1e-296, 1e-295, 1e-294,
        1e-293, 1e-292, 1e-291, 1e-290, 1e-289, 1e-288, 1e-287, 1e-286, 1e-285, 1e-284,
        1e-283, 1e-282, 1e-281, 1e-280, 1e-279, 1e-278, 1e-277, 1e-276, 1e-275, 1e-274,
        1e-273, 1e-272, 1e-271, 1e-270, 1e-269, 1e-268, 1e-267, 1e-266, 1e-265, 1e-264,
        1e-263, 1e-262, 1e-261, 1e-260, 1e-259, 1e-258, 1e-257, 1e-256, 1e-255, 1e-254,
        1e-253, 1e-252, 1e-251, 1e-250, 1e-249, 1e-248, 1e-247, 1e-246, 1e-245, 1e-244,
        1e-243, 1e-242, 1e-241, 1e-240, 1e-239, 1e-238, 1e-237, 1e-236, 1e-235, 1e-234,
        1e-233, 1e-232, 1e-231, 1e-230, 1e-229, 1e-228, 1e-227, 1e-226, 1e-225, 1e-224,
        1e-223, 1e-222, 1e-221, 1e-220, 1e-219, 1e-218, 1e-217, 1e-216, 1e-215, 1e-214,
        1e-213, 1e-212, 1e-211, 1e-210, 1e-209, 1e-208, 1e-207, 1e-206, 1e-205, 1e-204,
        1e-203, 1e-202, 1e-201, 1e-200, 1e-199, 1e-198, 1e-197, 1e-196, 1e-195, 1e-194,
        1e-193, 1e-192, 1e-191, 1e-190, 1e-189, 1e-188, 1e-187, 1e-186, 1e-185, 1e-184,
        1e-183, 1e-182, 1e-181, 1e-180, 1e-179, 1e-178, 1e-177, 1e-176, 1e-175, 1e-174,
        1e-173, 1e-172, 1e-171, 1e-170, 1e-169, 1e-168, 1e-167, 1e-166, 1e-165, 1e-164,
        1e-163, 1e-162, 1e-161, 1e-160, 1e-159, 1e-158, 1e-157, 1e-156, 1e-155, 1e-154,
        1e-153, 1e-152, 1e-151, 1e-150, 1e-149, 1e-148, 1e-147, 1e-146, 1e-145, 1e-144,
        1e-143, 1e-142, 1e-141, 1e-140, 1e-139, 1e-138, 1e-137, 1e-136, 1e-135, 1e-134,
        1e-133, 1e-132, 1e-131, 1e-130, 1e-129, 1e-128, 1e-127, 1e-126, 1e-125, 1e-124,
        1e-123, 1e-122, 1e-121, 1e-120, 1e-119, 1e-118, 1e-117, 1e-116, 1e-115, 1e-114,
        1e-113, 1e-112, 1e-111, 1e-110, 1e-109, 1e-108, 1e-107, 1e-106, 1e-105, 1e-104,
        1e-103, 1e-102, 1e-101, 1e-100, 1e-99 , 1e-98 , 1e-97 , 1e-96 , 1e-95 , 1e-94 ,
        1e-93 , 1e-92 , 1e-91 , 1e-90 , 1e-89 , 1e-88 , 1e-87 , 1e-86 , 1e-85 , 1e-84 ,
        1e-83 , 1e-82 , 1e-81 , 1e-80 , 1e-79 , 1e-78 , 1e-77 , 1e-76 , 1e-75 , 1e-74 ,
        1e-73 , 1e-72 , 1e-71 , 1e-70 , 1e-69 , 1e-68 , 1e-67 , 1e-66 , 1e-65 , 1e-64 ,
        1e-63 , 1e-62 , 1e-61 , 1e-60 , 1e-59 , 1e-58 , 1e-57 , 1e-56 , 1e-55 , 1e-54 ,
        1e-53 , 1e-52 , 1e-51 , 1e-50 , 1e-49 , 1e-48 , 1e-47 , 1e-46 , 1e-45 , 1e-44 ,
        1e-43 , 1e-42 , 1e-41 , 1e-40 , 1e-39 , 1e-38 , 1e-37 , 1e-36 , 1e-35 , 1e-34 ,
        1e-33 , 1e-32 , 1e-31 , 1e-30 , 1e-29 , 1e-28 , 1e-27 , 1e-26 , 1e-25 , 1e-24 ,
        1e-23 , 1e-22 , 1e-21 , 1e-20 , 1e-19 , 1e-18 , 1e-17 , 1e-16 , 1e-15 , 1e-14 ,
        1e-13 , 1e-12 , 1e-11 , 1e-10 , 1e-9  , 1e-8  , 1e-7  , 1e-6  , 1e-5  , 1e-4  ,
        1e-3  , 1e-2  , 1e-1  , 1     , 1e+1  , 1e+2  , 1e+3  , 1e+4  , 1e+5  , 1e+6  ,
        1e+7  , 1e+8  , 1e+9  , 1e+10 , 1e+11 , 1e+12 , 1e+13 , 1e+14 , 1e+15 , 1e+16 ,
        1e+17 , 1e+18 , 1e+19 , 1e+20 , 1e+21 , 1e+22 , 1e+23 , 1e+24 , 1e+25 , 1e+26 ,
        1e+27 , 1e+28 , 1e+29 , 1e+30 , 1e+31 , 1e+32 , 1e+33 , 1e+34 , 1e+35 , 1e+36 ,
        1e+37 , 1e+38 , 1e+39 , 1e+40 , 1e+41 , 1e+42 , 1e+43 , 1e+44 , 1e+45 , 1e+46 ,
        1e+47 , 1e+48 , 1e+49 , 1e+50 , 1e+51 , 1e+52 , 1e+53 , 1e+54 , 1e+55 , 1e+56 ,
        1e+57 , 1e+58 , 1e+59 , 1e+60 , 1e+61 , 1e+62 , 1e+63 , 1e+64 , 1e+65 , 1e+66 ,
        1e+67 , 1e+68 , 1e+69 , 1e+70 , 1e+71 , 1e+72 , 1e+73 , 1e+74 , 1e+75 , 1e+76 ,
        1e+77 , 1e+78 , 1e+79 , 1e+80 , 1e+81 , 1e+82 , 1e+83 , 1e+84 , 1e+85 , 1e+86 ,
        1e+87 , 1e+88 , 1e+89 , 1e+90 , 1e+91 , 1e+92 , 1e+93 , 1e+94 , 1e+95 , 1e+96 ,
        1e+97 , 1e+98 , 1e+99 , 1e+100, 1e+101, 1e+102, 1e+103, 1e+104, 1e+105, 1e+106,
        1e+107, 1e+108, 1e+109, 1e+110, 1e+111, 1e+112, 1e+113, 1e+114, 1e+115, 1e+116,
        1e+117, 1e+118, 1e+119, 1e+120, 1e+121, 1e+122, 1e+123, 1e+124, 1e+125, 1e+126,
        1e+127, 1e+128, 1e+129, 1e+130, 1e+131, 1e+132, 1e+133, 1e+134, 1e+135, 1e+136,
        1e+137, 1e+138, 1e+139, 1e+140, 1e+141, 1e+142, 1e+143, 1e+144, 1e+145, 1e+146,
        1e+147, 1e+148, 1e+149, 1e+150, 1e+151, 1e+152, 1e+153, 1e+154, 1e+155, 1e+156,
        1e+157, 1e+158, 1e+159, 1e+160, 1e+161, 1e+162, 1e+163, 1e+164, 1e+165, 1e+166,
        1e+167, 1e+168, 1e+169, 1e+170, 1e+171, 1e+172, 1e+173, 1e+174, 1e+175, 1e+176,
        1e+177, 1e+178, 1e+179, 1e+180, 1e+181, 1e+182, 1e+183, 1e+184, 1e+185, 1e+186,
        1e+187, 1e+188, 1e+189, 1e+190, 1e+191, 1e+192, 1e+193, 1e+194, 1e+195, 1e+196,
        1e+197, 1e+198, 1e+199, 1e+200, 1e+201, 1e+202, 1e+203, 1e+204, 1e+205, 1e+206,
        1e+207, 1e+208, 1e+209, 1e+210, 1e+211, 1e+212, 1e+213, 1e+214, 1e+215, 1e+216,
        1e+217, 1e+218, 1e+219, 1e+220, 1e+221, 1e+222, 1e+223, 1e+224, 1e+225, 1e+226,
        1e+227, 1e+228, 1e+229, 1e+230, 1e+231, 1e+232, 1e+233, 1e+234, 1e+235, 1e+236,
        1e+237, 1e+238, 1e+239, 1e+240, 1e+241, 1e+242, 1e+243, 1e+244, 1e+245, 1e+246,
        1e+247, 1e+248, 1e+249, 1e+250, 1e+251, 1e+252, 1e+253, 1e+254, 1e+255, 1e+256,
        1e+257, 1e+258, 1e+259, 1e+260, 1e+261, 1e+262, 1e+263, 1e+264, 1e+265, 1e+266,
        1e+267, 1e+268, 1e+269, 1e+270, 1e+271, 1e+272, 1e+273, 1e+274, 1e+275, 1e+276,
        1e+277, 1e+278, 1e+279, 1e+280, 1e+281, 1e+282, 1e+283, 1e+284, 1e+285, 1e+286,
        1e+287, 1e+288, 1e+289, 1e+290, 1e+291, 1e+292, 1e+293, 1e+294, 1e+295, 1e+296,
        1e+297, 1e+298, 1e+299, 1e+300, 1e+301, 1e+302, 1e+303, 1e+304, 1e+305, 1e+306,
        1e+307, 1e+308
    };
    const uint64_t *lut = (const uint64_t *)exponent_lut;
    double d = 0;
    uint64_t *v = (uint64_t *)&d;

    uint64_t significand, f;
    int32_t exponent;
    int32_t bm, bf, bits = 0;
    u64x2_t x;

    if (i < 0) {
        if (i > -20) {
            d = m / exponent_lut[-i - MIN_NEG_EXP];
            *v |= neg << 63;
            return d;
        }
    } else {
        if (i + k < 20) {
            d = m * exponent_lut[i - MIN_NEG_EXP];
            *v |= neg << 63;
            return d;
        }
    }

    i -= MIN_NEG_EXP;
    if (i < 0) {
        if (k - 1 + i < 0) {
            *v = neg << 63;
            return d;
        } else {
            static uint64_t pow10_lut[19] = {
                10llu, 100llu, 1000llu, 10000llu, 100000llu, 1000000llu, 10000000llu, 100000000llu,
                1000000000llu, 10000000000llu, 100000000000llu, 1000000000000llu, 10000000000000llu,
                100000000000000llu, 1000000000000000llu, 10000000000000000llu, 100000000000000000llu,
                1000000000000000000llu, 10000000000000000000llu
            };
            k += i;
            m /= pow10_lut[-i - 1];
            i = 0;
        }
    } else if (i >= EXP_LUT_NUM) {
        *v = DP_EXPONENT_MASK | (neg << 63);
        return d;
    }

    f = lut[i];
    significand = f & DP_SIGNIFICAND_MASK;
    exponent = (f & DP_EXPONENT_MASK) >> DP_SIGNIFICAND_SIZE;
    if (exponent == 0) {
        exponent = 1 - DP_EXPONENT_OFFSET - DP_SIGNIFICAND_SIZE;
        bf = 64 - u64_pz_get(significand);
    } else {
        significand += DP_HIDDEN_BIT;
        exponent -= DP_EXPONENT_OFFSET + DP_SIGNIFICAND_SIZE;
        bf = DP_SIGNIFICAND_SIZE + 1;
    }

    bm = 64 - u64_pz_get(m);
    if (bm + bf <= 64) {
        m *= significand;
        bits = u64_pz_get(m);
        m <<= bits;
        exponent -= bits;
    } else {
        x = u128_mul(m, significand);
        if (x.hi) {
            bits = u64_pz_get(x.hi);
            m = (x.hi << bits) | (x.lo >> (64 - bits));
            exponent += 64 - bits;
        } else {
            bits = u64_pz_get(x.lo);
            m = x.lo << bits;
            exponent -= bits;
        }
    }

    bits = 64 - (DP_SIGNIFICAND_SIZE + 1);
    if (m & ((uint64_t)1 << (bits - 1))) {
        m >>= bits;
        m += 1;
        if (m == ((uint64_t)1 << (DP_SIGNIFICAND_SIZE + 1))) {
            m >>= 1;
            exponent += 1;
        }
    } else {
        m >>= bits;
    }
    exponent += bits;

    exponent += DP_EXPONENT_OFFSET + DP_SIGNIFICAND_SIZE;
    if (exponent > 1) {
        if (exponent < DP_EXPONENT_MAX) {
            *v = (m & DP_SIGNIFICAND_MASK) | ((uint64_t)exponent << DP_SIGNIFICAND_SIZE) | (neg << 63);
        } else {
            *v = DP_EXPONENT_MASK | (neg << 63);
        }
    } else {
        if (exponent > -DP_SIGNIFICAND_SIZE) {
            exponent -= 1;
            *v = (m >> -exponent) | (neg << 63);
        } else {
            *v = neg << 63;
        }
    }
    return d;
}

int jnum_parse_num(const char *str, jnum_type_t *type, jnum_value_t *value)
{
#define IS_DIGIT(c)     ((c) >= '0' && (c) <= '9')
    const char *s = str;
    int32_t eneg = 0, e = 0, k = 0, ek = 0, b = 0, i = 0, z = 0;
    uint64_t neg = 0, m = 0, n = 0;

    switch (*s) {
    case '-':
        neg = 1;
        FALLTHROUGH_ATTR;
    case '+':
        ++s;
        break;
    case '0':
        if ((*(s + 1) == 'x' || *(s + 1) == 'X')) {
            int len = jnum_parse_hex(s, type, value);
            if (len == 2) {
                *type = JNUM_INT;
                value->vint = 0;
                return 1;
            }
            return len;
        }
        break;
    default:
        break;
    }

    switch (*s) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    case '.':
        break;
    default:
        *type = JNUM_NULL;
        value->vint = 0;
        return 0;
    }

    while (*s == '0')
        ++s;

    while (IS_DIGIT(*s)) {
        m = (m << 3) + (m << 1) + (*s++ - '0');
        ++k;
    }

    if (k < 20) {
        switch (*s) {
        case '.':
            ++s;
            break;
        case 'e': case 'E':
            switch (*(s + 1)) {
            case '-':
                eneg = 1;
                FALLTHROUGH_ATTR;
            case '+':
                if (IS_DIGIT(*(s + 2))) {
                    s += 2;
                    goto end3;
                }
                goto end1;
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                ++s;
                goto end3;
            default:
                goto end1;
            }
        default:
            goto end1;
        }
    } else {
        s -= k;
        m = 0;
        k = 0;
        while (IS_DIGIT(*s)) {
            m = (m << 3) + (m << 1) + (*s++ - '0');
            ++k;
            if (k == 19)
                break;
        }

        while (1) {
            switch (*s) {
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                ++s;
                ++i;
                break;
            case '.':
                ++s;
                while (IS_DIGIT(*s))
                    ++s;

                if (*s == 'e' || *s == 'E') {
                    switch (*(s + 1)) {
                    case '-':
                        eneg = 1;
                        FALLTHROUGH_ATTR;
                    case '+':
                        if (IS_DIGIT(*(s + 2))) {
                            s += 2;
                            goto end3;
                        }
                        goto end2;
                    case '0': case '1': case '2': case '3': case '4':
                    case '5': case '6': case '7': case '8': case '9':
                        ++s;
                        goto end3;
                    default:
                        goto end2;
                    }
                }
                goto end2;
            case 'e': case 'E':
                switch (*(s + 1)) {
                case '-':
                    eneg = 1;
                    FALLTHROUGH_ATTR;
                case '+':
                    if (IS_DIGIT(*(s + 2))) {
                        s += 2;
                        goto end3;
                    }
                    goto end2;
                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                    ++s;
                    goto end3;
                default:
                    goto end2;
                }
            default:
                goto end2;
            }
        }
    }

    if (m == 0) {
        while (*s == '0') {
            ++s;
            --z;
        }
    } else {
        n = m;
        b = k;
    }

    while (IS_DIGIT(*s)) {
        m = (m << 3) + (m << 1) + (*s++ - '0');
        ++k;
        --i;
    }

    if (k < 20) {
        switch (*s) {
        case 'e': case 'E':
            switch (*(s + 1)) {
            case '-':
                eneg = 1;
                FALLTHROUGH_ATTR;
            case '+':
                if (IS_DIGIT(*(s + 2))) {
                    s += 2;
                    goto end3;
                }
                goto end2;
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                ++s;
                goto end3;
            default:
                goto end2;
            }
        default:
            goto end2;
        }
    } else {
        s -= k - b;
        m = n;
        k = b;
        i = 0;

        while (IS_DIGIT(*s)) {
            m = (m << 3) + (m << 1) + (*s++ - '0');
            ++k;
            --i;
            if (k == 19)
                break;
        }
        while (IS_DIGIT(*s))
            ++s;

        if (*s == 'e' || *s == 'E') {
            switch (*(s + 1)) {
            case '-':
                eneg = 1;
                FALLTHROUGH_ATTR;
            case '+':
                if (IS_DIGIT(*(s + 2))) {
                    s += 2;
                    goto end3;
                }
                goto end2;
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                ++s;
                goto end3;
            default:
                goto end2;
            }
        }
        goto end2;
    }

end3:
    while (*s == '0')
        ++s;

    ek = 0;
    while (IS_DIGIT(*s)) {
        e = (e << 3) + (e << 1) + (*s++ - '0');
        ++ek;
        if (ek == 4)
            break;
    }
    while (IS_DIGIT(*s))
        ++s;
    i += eneg == 0 ? e : -e;

end2:
    *type = JNUM_DOUBLE;
    if (m == 0) {
        value->vlhex = neg << 63;
    } else {
        value->vdbl = jnum_convert_double(neg, m, k, i + z);
    }
    return s - str;

end1:
    if (m <= 2147483647U /*INT_MAX*/) {
        *type = JNUM_INT;
        value->vint = neg == 0 ? (int32_t)m : -((int32_t)m);
    } else if (m <= 9223372036854775807U /*LLONG_MAX*/) {
        *type = JNUM_LINT;
        value->vlint = neg == 0 ? (int64_t)m : -((int64_t)m);
    } else {
        if (m == 9223372036854775808U && neg == 1) {
            *type = JNUM_LINT;
            value->vlint = -m;
        } else {
            *type = JNUM_DOUBLE;
            value->vdbl = neg == 0 ? (double)m : -((double)m);
        }
    }
    return s - str;
}

#define jnum_to_func(rtype, fname)                      \
rtype fname(const char *str)                            \
{                                                       \
    jnum_type_t type;                                   \
    jnum_value_t value;                                 \
    rtype val = 0;                                      \
    jnum_parse(str, &type, &value);                     \
    switch (type) {                                     \
    case JNUM_BOOL:   val = (rtype)value.vbool;break;   \
    case JNUM_INT:    val = (rtype)value.vint; break;   \
    case JNUM_HEX:    val = (rtype)value.vhex; break;   \
    case JNUM_LINT:   val = (rtype)value.vlint;break;   \
    case JNUM_LHEX:   val = (rtype)value.vlhex;break;   \
    case JNUM_DOUBLE: val = (rtype)value.vdbl; break;   \
    default:          val = 0;                 break;   \
    }                                                   \
    return val;                                         \
}

jnum_to_func(int32_t, jnum_atoi)
jnum_to_func(int64_t, jnum_atol)
jnum_to_func(uint32_t, jnum_atoh)
jnum_to_func(uint64_t, jnum_atolh)
jnum_to_func(double, jnum_atod)
