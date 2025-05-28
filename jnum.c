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
/*
 * When set to 1, use one more precision when using large number multiplying,
 * and it will slow down the speed.
 * When set to 0, use a lookup table with only 1/4 of the original lookup table size,
 * and it only has precision of 15bits.
 */
#ifndef USING_HIGH_RES
#define USING_HIGH_RES              1
#endif

/* Using floating-point multiplication, only for low-precision (15bits), it is valid when USING_HIGH_RES is 0. */
#ifndef USING_FLOAT_MUL
#define USING_FLOAT_MUL             0
#endif

/* Using a custom division instead of u128 division, it is valid when USING_HIGH_RES is 1. */
#ifndef USING_DIV_EXP
#define USING_DIV_EXP               0
#endif

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

static inline int32_t u64_pz_get(uint64_t f)
{
#if defined(_MSC_VER)
    unsigned long index;
    _BitScanReverse64(&index, f);
    return 63 - index;
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_clzll(f);
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

#if USING_HIGH_RES
#if !USING_U128_CALC
static inline int u128_cmp(u64x2_t v1, u64x2_t v2)
{
    if (v1.hi > v2.hi)
        return 1;
    if (v1.hi < v2.hi)
        return -1;
    if (v1.lo > v2.lo)
        return 1;
    if (v1.lo < v2.lo)
        return -1;
    return 0;
}

static inline u64x2_t u128_add(u64x2_t v1, uint64_t v2)
{
    u64x2_t ret = {0, 0};
    uint64_t diff = 0xFFFFFFFFFFFFFFFF - v1.lo;

    if (v2 > diff) {
        ret.lo = v2 - diff - 1;
        ret.hi = v1.hi + 1;
    } else {
        ret.lo = v1.lo + v2;
        ret.hi = v1.hi;
    }
    return ret;
}

static inline u64x2_t u128_sub(u64x2_t v1, u64x2_t v2)
{
    u64x2_t ret = {0, 0};

    if (v1.lo < v2.lo) {
        uint64_t diff = 0xFFFFFFFFFFFFFFFF - v2.lo;
        ret.lo = v1.lo + diff + 1;
        ret.hi = v1.hi - v2.hi - 1;
    } else {
        ret.lo = v1.lo - v2.lo;
        ret.hi = v1.hi - v2.hi;
    }

    return ret;
}

/*
# python to get lut

# ret = dividend / divisor
#     = dividend / (10 ^ E)
#     = dividend / ((2 * 5) ^ E)
#     = dividend / ((2 ^ E) * (5 ^ E))
#     = (dividend >> E) / (5 ^ E)
#     ≈ ((dividend >> E) * ((1 << K) / (5 ^ E))) >> K
#
# "10 ** maxE" should be less than "1 << 64", so maxE is 19

# python to get lut

mul_lut = []
shift_lut = []
def print_lut():
    maxE = 19
    for E in range(1, maxE + 1):
        minN = (10 ** E) >> E
        maxN = (((1 << 64) - 1) * (10 ** E)) >> E
        V = 5 ** E
        M = 0
        for K in range(128, 0, -1):
            M = ((1 << K) // V) + 1
            if M < (1 << 64):
                break
        mul_lut.append(M)
        shift_lut.append(K)

    print('static const uint64_t mul_lut[%d] = {' % (maxE), end='')
    for i in range(maxE):
        if i % 4 == 0:
            print()
            print('    ', end='')
        print('0x%016x' % (mul_lut[i]), end='')
        if i != maxE - 1:
            print(', ', end='');
        else:
            print()
            print('};')

    print()
    print('static const uint64_t pow5_lut[%d] = {' % (maxE), end='')
    for i in range(maxE):
        if i % 4 == 0:
            print()
            print('    ', end='')
        print('0x%016x' % (5 ** (i + 1)), end='')
        if i != maxE - 1:
            print(', ', end='');
        else:
            print()
            print('};')

    print()
    print('static const uint8_t shift_lut[%d] = {' % (maxE), end='')
    for i in range(maxE):
        if i % 20 == 0:
            print()
            print('    ', end='')
        print('%-2d' % (shift_lut[i]), end='')
        if i != maxE - 1:
            print(', ', end='');
        else:
            print()
            print('};')

print_lut()
*/

static inline uint64_t u128_div_exp(u64x2_t dividend, uint64_t *remainder, uint64_t M, uint64_t P, int32_t K, int32_t E)
{
    const u64x2_t n = {
        .hi = dividend.hi >> E,
        .lo = dividend.hi << (64 - E) | dividend.lo >> E
    };
    int32_t zeros = u64_pz_get(n.hi);

    uint64_t hi = (n.hi << zeros | n.lo >> (64 - zeros)) + 1;
    u64x2_t r = u128_mul(hi, M);
    int32_t shift = (K + zeros) - 128;
    uint64_t ret = r.hi >> shift;

    u64x2_t m = u128_mul(ret, P);
    if (u128_cmp(m, n) > 0) {
        --ret;
        *remainder = u128_sub(u128_add(n, P), m).lo << E;
    } else {
        *remainder = u128_sub(n, m).lo << E;
    }

    *remainder += dividend.lo & (((uint64_t)1 << E) - 1);
    return ret;
}

static inline uint64_t u128_div_1e17(u64x2_t dividend, uint64_t *remainder)
{
    return u128_div_exp(dividend, remainder, 0xb877aa3236a4b44a, 0x000000b1a2bc2ec5, 103, 17);
}

static inline uint64_t u128_div_1e18(u64x2_t dividend, uint64_t *remainder)
{
    return u128_div_exp(dividend, remainder, 0x9392ee8e921d5d08, 0x000003782dace9d9, 105, 18);
}

#else // !USING_U128_CALC

#if USING_DIV_EXP
static inline uint64_t u128_div_exp(u128 dividend, uint64_t *remainder, uint64_t M, uint64_t P, int32_t K, int32_t E)
{
    const u128 n = dividend >> E;
    uint64_t hi = (uint64_t)(n >> 64);
    uint64_t lo = (uint64_t)n;
    int32_t zeros = u64_pz_get(hi);

    hi = (hi << zeros | lo >> (64 - zeros)) + 1;
    uint64_t ret = (((u128)hi * M) >> (K + zeros - 64));

    u128 m = (u128)ret * P;
    if (m > n) {
        --ret;
        m -= P;
    }

    *remainder = (n - m) << E;
    *remainder += (uint64_t)dividend & (((uint64_t)1 << E) - 1);
    return ret;
}

/*
static const uint64_t mul_lut[19] = {
    0xcccccccccccccccd, 0xa3d70a3d70a3d70b, 0x83126e978d4fdf3c, 0xd1b71758e219652c,
    0xa7c5ac471b478424, 0x8637bd05af6c69b6, 0xd6bf94d5e57a42bd, 0xabcc77118461cefd,
    0x89705f4136b4a598, 0xdbe6fecebdedd5bf, 0xafebff0bcb24aaff, 0x8cbccc096f5088cc,
    0xe12e13424bb40e14, 0xb424dc35095cd810, 0x901d7cf73ab0acda, 0xe69594bec44de15c,
    0xb877aa3236a4b44a, 0x9392ee8e921d5d08, 0xec1e4a7db69561a6
};

static const uint64_t pow5_lut[19] = {
    0x0000000000000005, 0x0000000000000019, 0x000000000000007d, 0x0000000000000271,
    0x0000000000000c35, 0x0000000000003d09, 0x000000000001312d, 0x000000000005f5e1,
    0x00000000001dcd65, 0x00000000009502f9, 0x0000000002e90edd, 0x000000000e8d4a51,
    0x0000000048c27395, 0x000000016bcc41e9, 0x000000071afd498d, 0x0000002386f26fc1,
    0x000000b1a2bc2ec5, 0x000003782dace9d9, 0x00001158e460913d
};

static const uint8_t shift_lut[19] = {
    66, 68, 70, 73, 75, 77, 80, 82, 84, 87, 89, 91, 94, 96, 98, 101, 103, 105, 108
};
*/

static inline uint64_t u128_div_1e17(u128 dividend, uint64_t *remainder)
{
    return u128_div_exp(dividend, remainder, 0xb877aa3236a4b44a, 0x000000b1a2bc2ec5, 103, 17);
}

static inline uint64_t u128_div_1e18(u128 dividend, uint64_t *remainder)
{
    return u128_div_exp(dividend, remainder, 0x9392ee8e921d5d08, 0x000003782dace9d9, 105, 18);
}

#else // USING_DIV_EXP

static inline uint64_t u128_div_1e17(u128 dividend, uint64_t *remainder)
{
    uint64_t ret = dividend / 100000000000000000llu;
    *remainder = dividend - (u128)ret * 100000000000000000llu;
    return ret;
}

static inline uint64_t u128_div_1e18(u128 dividend, uint64_t *remainder)
{
    uint64_t ret = dividend / 1000000000000000000llu;
    *remainder = dividend - (u128)ret * 1000000000000000000llu;
    return ret;
}
#endif // USING_DIV_EXP
#endif // !USING_U128_CALC
#endif // USING_HIGH_RES

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
    'a', 'b', 'c', 'd', 'e', 'f'};

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

def print_lut():
    result_digits_keys = {}
    for i in range(1, 129):
        min_digits = len(str(1<<(i-1)))
        digits = len(str(1<<i))

        if min_digits == digits:
            if digits not in result_digits_keys.keys():
                result_digits_keys[digits] = []
            result_digits_keys[digits].append(i)
    #print(result_digits_keys)

    reserved_digits_list = []
    for i in range(1, 53):
        v = 1 << i
        s = str(v)
        digits = len(s)
        reserved_digits_list.append(digits)
    #print(reserved_digits_list)

    base_lut = []
    index_lut = []
    shift_lut = []
    for i in range(1, 53):
        if i <= 26:
            digits = reserved_digits_list[i-1] + 9  # using dividend 1000000000
        else:
            digits = reserved_digits_list[i-1] + 17 # using dividend 100000000000000000

        bits = [v - i for v in result_digits_keys[digits]]

        base_exp = -1074

        for j in range(0, 13): # max left shift bit is 12
            exp = j - base_exp
            val = 5 ** exp

            cmp_value = 1 << 64 # ULONG_MAX = 18446744073709551615
            digit = val
            k = 0
            bit = 64

            while digit >= cmp_value:
                k += 1
                digit //= 10
            while True:
                while (1 << bit) > digit:
                    bit -= 1
                bit += 1

                if bit > bits[-1]:
                    k += 1
                    digit //= 10
                else:
                    break

            if bit in bits:
                digit += 1
                base_lut.append(digit)
                index_lut.append(k - exp)
                shift_lut.append(j)
                #print("bit[%2d]: shift=%d, exp=%d digit=%d" % (i, j, k - exp, digit))
                break

    print('static const uint64_t n_base_lut[52] = {', end='')
    for i in range(52):
        if i % 5 == 0:
            print()
            print('    ', end='')
        print('0x%016x' % (base_lut[i]), end='')
        if i != 51:
            print(', ', end='');
        else:
            print()
            print('};')

    print()
    print('static const uint8_t n_index_lut[52] = {', end='')
    for i in range(52):
        if i % 20 == 0:
            print()
            print('    ', end='')
        print('%-2d' % (index_lut[i] + 400), end='')
        if i != 51:
            print(', ', end='');
        else:
            print()
            print('};')

    print()
    print('static const uint8_t n_shift_lut[52] = {', end='')
    for i in range(52):
        if i % 20 == 0:
            print()
            print('    ', end='')
        print('%-2d' % (shift_lut[i]), end='')
        if i != 51:
            print(', ', end='');
        else:
            print()
            print('};')

print_lut()
*/

static int32_t ldouble_convert_n(diy_fp_t *v, char *buffer, int32_t *vnum_digits)
{
    static const uint64_t n_base_lut[52] = {
        0x00000000933e37a6, 0x000000001d72d7ee, 0x000000001d72d7ee, 0x00000000933e37a6, 0x00000000499f1bd3,
        0x000000001d72d7ee, 0x00000000933e37a6, 0x00000000499f1bd3, 0x000000001d72d7ee, 0x00000001267c6f4b,
        0x00000000933e37a6, 0x000000001d72d7ee, 0x000000001d72d7ee, 0x00000000933e37a6, 0x00000000499f1bd3,
        0x000000001d72d7ee, 0x00000000933e37a6, 0x00000000499f1bd3, 0x000000001d72d7ee, 0x00000001267c6f4b,
        0x00000000933e37a6, 0x000000001d72d7ee, 0x000000001d72d7ee, 0x00000000933e37a6, 0x00000000499f1bd3,
        0x000000001d72d7ee, 0x06db461654176951, 0x036da30b2a0bb4a9, 0x00af87023b9bf0ef, 0x06db461654176951,
        0x036da30b2a0bb4a9, 0x00af87023b9bf0ef, 0x00af87023b9bf0ef, 0x06db461654176951, 0x00af87023b9bf0ef,
        0x00af87023b9bf0ef, 0x06db461654176951, 0x036da30b2a0bb4a9, 0x00af87023b9bf0ef, 0x06db461654176951,
        0x036da30b2a0bb4a9, 0x00af87023b9bf0ef, 0x00af87023b9bf0ef, 0x036da30b2a0bb4a9, 0x00af87023b9bf0ef,
        0x00af87023b9bf0ef, 0x06db461654176951, 0x00af87023b9bf0ef, 0x00af87023b9bf0ef, 0x06db461654176951,
        0x036da30b2a0bb4a9, 0x00af87023b9bf0ef
    };

    static const uint8_t n_index_lut[52] = {
        67, 68, 68, 67, 67, 68, 67, 67, 68, 67, 67, 68, 68, 67, 67, 68, 67, 67, 68, 67,
        67, 68, 68, 67, 67, 68, 59, 59, 60, 59, 59, 60, 60, 59, 60, 60, 59, 59, 60, 59,
        59, 60, 60, 59, 60, 60, 59, 60, 60, 59, 59, 60
    };

    static const uint8_t n_shift_lut[52] = {
        1 , 0 , 0 , 1 , 2 , 0 , 1 , 2 , 0 , 0 , 1 , 0 , 0 , 1 , 2 , 0 , 1 , 2 , 0 , 0 ,
        1 , 0 , 0 , 1 , 2 , 0 , 0 , 1 , 0 , 0 , 1 , 0 , 0 , 0 , 0 , 0 , 0 , 1 , 0 , 0 ,
        1 , 0 , 0 , 1 , 0 , 0 , 0 , 0 , 0 , 0 , 1 , 0
    };

    int32_t num_digits, trailing_zeros;
    int32_t i = 63 - u64_pz_get(v->f);
    int32_t e = (int)n_index_lut[i] - 400;
    int32_t s = n_shift_lut[i];

    if (i < 26) {
        static const uint64_t cmp_lut[10] = {
            0            , 1000000000llu, 2000000000llu, 3000000000llu, 4000000000llu,
            5000000000llu, 6000000000llu, 7000000000llu, 8000000000llu, 9000000000llu
        };

        uint32_t remainder;
        uint64_t t = n_base_lut[i];
        uint64_t f = v->f << s;
        uint64_t delta = t << s;
        uint64_t m = f * t;

        m += delta >> 1;
        v->f = m / 1000000000;
        remainder = m % 1000000000;
        v->e = e + 9;

        if (v->f < 10) {
            *buffer = '0' + v->f;
            *vnum_digits = 1;
            return 1;
        }

        uint64_t tail = v->f % 10;
        if (remainder + cmp_lut[tail] <= delta + 100000000) {
            v->f -= tail;
        }

        if (v->f >= 100000000) {
            memcpy(buffer, "100000000", 9);
            trailing_zeros = 8;
            num_digits = 9;
        } else {
            num_digits = fill_1_8_digits(buffer, v->f, &trailing_zeros);
        }
        *vnum_digits = num_digits - trailing_zeros;
        return num_digits;

    } else {
#if USING_HIGH_RES
        static const uint64_t cmp_lut[10] = { 0,
            100000000000000000llu, 200000000000000000llu, 300000000000000000llu,
            400000000000000000llu, 500000000000000000llu, 600000000000000000llu,
            700000000000000000llu, 800000000000000000llu, 900000000000000000llu
        };

        uint64_t remainder;
        uint64_t t = n_base_lut[i];
        uint64_t f = v->f << s;
        uint64_t delta = t << s;

#if USING_U128_CALC
        u128 m = (u128)f * t;
        m += delta >> 1;
        v->f = u128_div_1e17(m, &remainder);
        v->e = e + 17;
#else
        u64x2_t m = u128_mul(f, t);
        m = u128_add(m, delta >> 1);
        v->f = u128_div_1e17(m, &remainder);
        v->e = e + 17;
#endif
        uint64_t tail = v->f % 10;
        if (remainder + cmp_lut[tail] <= delta + 10000000000000000llu) {
            v->f -= tail;
        }

#else // USING_HIGH_RES

        uint64_t t = n_base_lut[i];
        uint64_t f = v->f << s;
        double d = (double)f * t;
        v->f = (uint64_t)((d + 5e18) * 1e-19);
        v->e = e + 19;
#endif
        if (v->f >= 10000000000000000llu) {
            memcpy(buffer, "10000000000000000", 17);
            trailing_zeros = 16;
            num_digits = 17;
        } else {
            num_digits = fill_1_16_digits(buffer, v->f, &trailing_zeros);
        }
        *vnum_digits = num_digits - trailing_zeros;
        return num_digits;
    }
    return 0;
}

#if USING_HIGH_RES
/*
# python to get lut
def print_pow_array(unit, index, end, positive):
    cmp_value = 1<<59 # ULONG_MAX = 18446744073709551615
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
    pos = 0;
    print('static const uint8_t %s_index_lut[%d] = {' % ('positive' if positive else 'negative', end + 1), end='')
    for i in range(end + 1):
        if i % 20 == 0:
            print()
            print('    ', end='')
        if index_lut[i] >= 0:
            print('%-3d' % (index_lut[i]), end='')
        else:
            if pos == 0:
                pos = i
            print('%-3d' % (-index_lut[i]), end='')

        if i != end:
            print(', ', end='');
        else:
            print()
            print('};')
    if pos:
        print('From index %d, the real value is "-index_lut[i]"' % (pos))

def print_positive_array():
    # 1 * (2 ** 2) = 0.4 * (10 ** 1)
    # 4 = 0.4 * (10 ** 1)
    # 486 = (1023 - 52) / 2 + 1
    print_pow_array(4, 1, 486, True)

def print_negative_array():
    # 1 * (2 ** -2) = 2.5 * (10 ** -1)
    # 25 = 2.5 * (10 ** 1)
    # 538 = (1022 + 52) / 2 + 1
    print_pow_array(25, 1, 538, False)

print_positive_array()
print()
print_negative_array()
*/

static inline diy_fp_t positive_diy_fp(int32_t e)
{
    static const uint64_t positive_base_lut[487] = {
        0x016345785d8a0000, 0x058d15e176280000, 0x02386f26fc100000, 0x00e35fa931a00000,
        0x038d7ea4c6800000, 0x016bcc41e9000000, 0x05af3107a4000000, 0x0246139ca8000000,
        0x00e8d4a510000000, 0x03a3529440000000, 0x0174876e80000000, 0x05d21dba00000000,
        0x02540be400000000, 0x00ee6b2800000000, 0x03b9aca000000000, 0x017d784000000000,
        0x05f5e10000000000, 0x02625a0000000000, 0x00f4240000000000, 0x03d0900000000000,
        0x0186a00000000000, 0x061a800000000000, 0x0271000000000000, 0x00fa000000000000,
        0x03e8000000000000, 0x0190000000000000, 0x0640000000000000, 0x0280000000000000,
        0x0100000000000000, 0x0400000000000000, 0x019999999999999a, 0x0666666666666667,
        0x028f5c28f5c28f5d, 0x010624dd2f1a9fbf, 0x04189374bc6a7efa, 0x01a36e2eb1c432cb,
        0x068db8bac710cb2a, 0x029f16b11c6d1e11, 0x010c6f7a0b5ed8d4, 0x0431bde82d7b634e,
        0x01ad7f29abcaf486, 0x06b5fca6af2bd216, 0x02af31dc4611873c, 0x0112e0be826d694c,
        0x044b82fa09b5a52d, 0x01b7cdfd9d7bdbac, 0x06df37f675ef6eae, 0x02bfaffc2f2c92ac,
        0x0119799812dea112, 0x0465e6604b7a8447, 0x01c25c268497681d, 0x0709709a125da071,
        0x02d09370d4257361, 0x01203af9ee75615a, 0x0480ebe7b9d58567, 0x01cd2b297d889bc3,
        0x0734aca5f6226f0b, 0x02e1dea8c8da92d2, 0x012725dd1d243abb, 0x049c97747490eae9,
        0x01d83c94fb6d2ac4, 0x0760f253edb4ab0e, 0x02f394219248446c, 0x012e3b40a0e9b4f8,
        0x04b8ed0283a6d3e0, 0x01e392010175ee5a, 0x078e480405d7b966, 0x0305b66802564a29,
        0x01357c299a88ea77, 0x04d5f0a66a23a9db, 0x01ef2d0f5da7dd8b, 0x07bcb43d769f762b,
        0x0318481895d96278, 0x013ce9a36f23c0fd, 0x04f3a68dbc8f03f3, 0x01fb0f6be5060195,
        0x07ec3daf94180651, 0x032b4bdfd4d668ed, 0x014484bfeebc29f9, 0x051212ffbaf0a7e2,
        0x02073accb12d0ff4, 0x00cfb11ead453995, 0x033ec47ab514e653, 0x014c4e977ba1f5bb,
        0x05313a5dee87d6ec, 0x0213b0f25f69892b, 0x00d4ad2dbfc3d078, 0x0352b4b6ff0f41df,
        0x0154484932d2e726, 0x05512124cb4b9c97, 0x022073a8515171d6, 0x00d9c7dced53c723,
        0x03671f73b54f1c8a, 0x015c72fb1552d837, 0x0571cbec554b60dc, 0x022d84c4eeeaf38c,
        0x00df01e85f912e38, 0x037c07a17e44b8df, 0x0164cfda3281e38d, 0x05933f68ca078e31,
        0x023ae629ea696c14, 0x00e45c10c42a2b3c, 0x0391704310a8aced, 0x016d601ad376ab92,
        0x05b5806b4ddaae47, 0x024899c4858aac1d, 0x00e9d71b689dde72, 0x03a75c6da27779c7,
        0x017624f8a762fd83, 0x05d893e29d8bf60b, 0x0256a18dd89e626b, 0x00ef73d256a5c0f8,
        0x03bdcf495a9703de, 0x017f1fb6f10934c0, 0x05fc7edbc424d2fd, 0x0264ff8b1b41edff,
        0x00f53304714d9266, 0x03d4cc11c5364998, 0x018851a0b548ea3d, 0x06214682d523a8f3,
        0x0273b5cdeedb1061, 0x00fb158592be068e, 0x03ec56164af81a35, 0x0191bc08eac9a416,
        0x0646f023ab269055, 0x0282c674aadc39bc, 0x01011c2eaabe7d7f, 0x040470baaaf9f5f9,
        0x019b604aaaca6264, 0x066d812aab29898e, 0x029233aaaadd6a39, 0x010747ddddf22a7e,
        0x041d1f7777c8a9f5, 0x01a53fc9631d10c9, 0x0694ff258c744321, 0x02a1ffa89e94e7a7,
        0x010d9976a5d52976, 0x043665da9754a5d8, 0x01af5bf109550f23, 0x06bd6fc425543c8c,
        0x02b22cb4dbbb4b6c, 0x011411e1f17e1e2b, 0x04504787c5f878ac, 0x01b9b6364f303045,
        0x06e6d8d93cc0c113, 0x02c2bd23b1e6b3a1, 0x011ab20e472914a7, 0x046ac8391ca4529b,
        0x01c45016d841baa5, 0x0711405b6106ea92, 0x02d3b357c0692aa1, 0x01217aefe6907774,
        0x0485ebbf9a41ddce, 0x01cf2b1970e72586, 0x073cac65c39c9617, 0x02e511c24e3ea270,
        0x01286d80ec190dc7, 0x04a1b603b0643719, 0x01da48ce468e7c71, 0x076923391a39f1c1,
        0x02f6dae3a4172d81, 0x012f8ac174d61234, 0x04be2b05d35848ce, 0x01e5aacf21568386,
        0x0796ab3c855a0e16, 0x0309114b688a6c09, 0x0136d3b7c36a919d, 0x04db4edf0daa4674,
        0x01f152bf9f10e8fc, 0x07c54afe7c43a3ed, 0x031bb798fe8174c6, 0x013e497065cd61e9,
        0x04f925c1973587a2, 0x01fd424d6faf030e, 0x07f50935bebc0c36, 0x032ed07be5e4d1b0,
        0x0145ecfe5bf520ad, 0x0517b3f96fd482b2, 0x02097b309321cde1, 0x00d097ad07a71f27,
        0x03425eb41e9c7c9b, 0x014dbf7b3f71cb72, 0x0536fdecfdc72dc5, 0x0215ff2b98b6124f,
        0x00d59944a37c0753, 0x035665128df01d4b, 0x0155c2076bf9a552, 0x0557081dafe69545,
        0x0222d00bdff5d54f, 0x00dab99e59958886, 0x036ae67966562218, 0x015df5ca28ef40d7,
        0x0577d728a3bd0359, 0x022fefa9db1867bd, 0x00dff9772470297f, 0x037fe5dc91c0a5fb,
        0x01665bf1d3e6a8cb, 0x05996fc74f9aa32c, 0x023d5fe9530aa7ab, 0x00e55990879ddcab,
        0x039566421e7772ab, 0x016ef5b40c2fc778, 0x05bbd6d030bf1ddf, 0x024b22b9ad193f26,
        0x00eadab0aba3b2dc, 0x03ab6ac2ae8ecb70, 0x0177c44ddf6c5160, 0x05df11377db14580,
        0x02593a163246e89a, 0x00f07da27a82c371, 0x03c1f689ea0b0dc3, 0x0180c903f7379f1b,
        0x0603240fdcde7c6a, 0x0267a8065858fe91, 0x00f64335bcf065d4, 0x03d90cd6f3c1974e,
        0x018a0522c7e70953, 0x0628148b1f9c254a, 0x02766e9e0ca4dbb8, 0x00fc2c3f3841f17d,
        0x03f0b0fce107c5f2, 0x019379fec0698261, 0x064de7fb01a60983, 0x02858ffe00a8d09b,
        0x01023998cd105372, 0x0408e66334414dc5, 0x019d28f47b4d524f, 0x0674a3d1ed35493a,
        0x02950e53f87bb6e4, 0x01086c219697e2c2, 0x0421b0865a5f8b07, 0x01a71368f0f30469,
        0x069c4da3c3cc11a4, 0x02a4ebdb1b1e6d75, 0x010ec4be0ad8f896, 0x043b12f82b63e255,
        0x01b13ac9aaf4c0ef, 0x06c4eb26abd303bb, 0x02b52adc44bace4b, 0x011544581b7dec1e,
        0x045511606df7b078, 0x01bba08cf8c979ca, 0x06ee8233e325e726, 0x02c5cdae5adbf60f,
        0x011bebdf578b2f3a, 0x046faf7d5e2cbce5, 0x01c6463225ab7ec2, 0x071918c896adfb08,
        0x02d6d6b6a2abfe03, 0x0122bc490dde659b, 0x048af1243779966c, 0x01d12d41afca3c2b,
        0x0744b506bf28f0ac, 0x02e8486919439378, 0x0129b69070816e30, 0x04a6da41c205b8c0,
        0x01dc574d80cf16b3, 0x07715d36033c5acc, 0x02fa2548ce182452, 0x0130dbb6b8d674ee,
        0x04c36edae359d3b6, 0x01e7c5f127bd87e3, 0x079f17c49ef61f8a, 0x030c6fe83f95a637,
        0x01382cc34ca2427d, 0x04e0b30d328909f2, 0x01f37ad21436d0c7, 0x07cdeb4850db431c,
        0x031f2ae9b9f14e0c, 0x013faac3e3fa1f38, 0x04feab0f8fe87cdf, 0x01ff779fd329cb8d,
        0x07fdde7f4ca72e31, 0x033258ffb842df47, 0x014756ccb01abfb6, 0x051d5b32c06afed8,
        0x020bbe144cf79924, 0x00d17f3b51fca3a8, 0x0345fced47f28e9f, 0x014f31f8832dd2a6,
        0x053cc7e20cb74a98, 0x02184ff405161dd7, 0x00d686619ba27256, 0x035a19866e89c957,
        0x01573d68f903ea23, 0x055cf5a3e40fa88b, 0x02252f0e5b39769e, 0x00dbac6c247d62a6,
        0x036eb1b091f58a97, 0x015f7a46a0c89dd6, 0x057de91a83227757, 0x02325d3dce0dc956,
        0x00e0f218b8d25089, 0x0383c862e3494223, 0x0167e9c127b6e742, 0x059fa7049edb9d05,
        0x023fdc683f8b0b9c, 0x00e65829b3046b0b, 0x039960a6cc11ac2c, 0x01708d0f84d3de78,
        0x05c2343e134f79e0, 0x024dae7f3aec9727, 0x00ebdf661791d610, 0x03af7d985e47583e,
        0x0179657025b6234c, 0x05e595c096d88d2f, 0x025bd5803c569ee0, 0x00f18899b1bc3f8d,
        0x03c62266c6f0fe33, 0x018274291c6065ae, 0x0609d0a4718196b8, 0x026a5374fa33d5e3,
        0x00f7549530e188c2, 0x03dd5254c3862305, 0x018bba884e35a79c, 0x062eea2138d69e6e,
        0x02792a73b055d8f9, 0x00fd442e4688bd31, 0x03f510b91a22f4c2, 0x019539e3a40dfb81,
        0x0654e78e9037ee02, 0x02885c9f6ce32c01, 0x0103583fc527ab34, 0x040d60ff149eacce,
        0x019ef3993b72ab86, 0x067bce64edcaae17, 0x0297ec285f1ddf3d, 0x010991a9bfa58c7f,
        0x042646a6fe9631fa, 0x01a8e90f9908e0cb, 0x06a3a43e6423832a, 0x02a7db4c280e3477,
        0x010ff151a99f4830, 0x043fc546a67d20bf, 0x01b31bb5dc320d19, 0x06cc6ed770c83464,
        0x02b82c562d1ce1c2, 0x011678227871f3e7, 0x0459e089e1c7cf9c, 0x01bd8d03f3e9863f,
        0x06f6340fcfa618fa, 0x02c8e19feca8d6cb, 0x011d270cc51055eb, 0x04749c33144157aa,
        0x01c83e7ad4e6efde, 0x0720f9eb539bbf77, 0x02d9fd9154a4b2fd, 0x0123ff06eea84799,
        0x048ffc1bbaa11e61, 0x01d331a4b10d3f5a, 0x074cc692c434fd67, 0x02eb82a11b48655d,
        0x012b010d3e1cf559, 0x04ac0434f873d561, 0x01de6815302e555a, 0x0779a054c0b95568,
        0x02fd735519e3bbc3, 0x01322e220a5b17e8, 0x04c8b888296c5f9f, 0x01e9e369aa2b5973,
        0x07a78da6a8ad65ca, 0x030fd242a9def584, 0x0139874ddd8c6235, 0x04e61d37763188d4,
        0x01f5a549627a36bb, 0x07d6952589e8daec, 0x0322a20f03f6bdf8, 0x01410d9f9b2f7f30,
        0x0504367e6cbdfcc0, 0x0201af65c518cb80, 0x00cd795be8705167, 0x0335e56fa1c1459a,
        0x0148c22ca71a1bd7, 0x052308b29c686f5c, 0x020e037aa4f692f2, 0x00d267caa862a12e,
        0x03499f2aa18a84b6, 0x0150a6110d6a9b7c, 0x0542984435aa6df0, 0x021aa34e7bddc593,
        0x00d77485cb25823b, 0x035dd2172c9608ec, 0x0158ba6fab6f36c5, 0x0562e9beadbcdb12,
        0x022790b2abe5246e, 0x00dca04777f541c6, 0x0372811ddfd50716, 0x016100725988693c,
        0x058401c96621a4f0, 0x0234cd83c273db93, 0x00e1ebce4dc7f16e, 0x0387af39371fc5b8,
        0x0169794a160cb57d, 0x05a5e5285832d5f4, 0x02425ba9bce12262, 0x00e757dd7ec07427,
        0x039d5f75fb01d09c, 0x0172262f3133ed0c, 0x05c898bcc4cfb42d, 0x02503d184eb97b45,
        0x00ece53cec4a314f, 0x03b394f3b128c53b, 0x017b08617a104ee5, 0x05ec2185e8413b92,
        0x025e73cf29b3b16e, 0x00f294b943e17a2c, 0x03ca52e50f85e8b0, 0x018421286c9bf6ad,
        0x061084a1b26fdab2, 0x026d01da475ff114, 0x00f867241c8cc6d5, 0x03e19c9072331b54,
        0x018d71d360e13e22, 0x0635c74d8384f885, 0x027be952349b969c, 0x00fe5d54150b090c,
        0x03f97550542c242d, 0x0196fbb9bb44db45, 0x065beee6ed136d14, 0x028b2c5c5ed49208,
        0x01047824f2bb6d9d, 0x0411e093caedb673, 0x01a0c03b1df8af62, 0x068300ec77e2bd85,
        0x029acd2b63277f02, 0x010ab877c142ff9b, 0x042ae1df050bfe6a, 0x01aac0bf9b9e65c4,
        0x06ab02fe6e79970f, 0x02aacdff5f63d606, 0x01111f32f2f4bc03, 0x04447ccbcbd2f00a,
        0x01b4feb7eb212cd1, 0x06d3fadfac84b343, 0x02bb31264501e14e, 0x0117ad428200c086,
        0x045eb50a08030216, 0x01bf7b9d9cce00d6, 0x06fdee7673380357, 0x02cbf8fc2e1667bd,
        0x011e6398126f5cb2, 0x04798e6049bd72c7, 0x01ca38f350b22dea, 0x0728e3cd42c8b7a5,
        0x02dd27ebb4504975, 0x0125432b14ecea2f, 0x04950cac53b3a8bb, 0x01d53844ee47dd18,
        0x0754e113b91f745f, 0x02eec06e4a0c94f3, 0x012c4cf8ea6b6ec8, 0x04b133e3a9adbb1e,
        0x01e07b27dd78b140, 0x0781ec9f75e2c4fd, 0x0300c50c958de865, 0x01338205089f29c2,
        0x04ce0814227ca708, 0x01ec033b40fea937, 0x07b00ced03faa4da, 0x0313385ece6441f1,
        0x013ae3591f5b4d94, 0x04eb8d647d6d364e, 0x01f7d228322baf53, 0x07df48a0c8aebd4a,
        0x03261d0d1d12b21e, 0x014272053ed4473f, 0x0509c814fb511cfc, 0x0203e9a1fe2071ff,
        0x00ce5d73ff402d99, 0x033975cffd00b664, 0x014a2f1ffecd15c2, 0x0528bc7ffb345706,
        0x02104b66647b5603, 0x00d3515c2831559b, 0x034d4570a0c5566b, 0x01521bc6a6b555c5,
        0x05486f1a9ad55711, 0x021cf93dd788893a, 0x00d863b256369d4b, 0x03618ec958da752a,
        0x015a391d56bdc877, 0x0568e4755af721dc, 0x0229f4fbbdfc73f2, 0x00dd95317f31c7fb,
        0x037654c5fcc71fe9, 0x0162884f31e93ff7, 0x058a213cc7a4ffdb
    };

    static const uint8_t positive_index_lut[487] = {
        17 , 18 , 18 , 18 , 19 , 19 , 20 , 20 , 20 , 21 , 21 , 22 , 22 , 22 , 23 , 23 , 24 , 24 , 24 , 25 ,
        25 , 26 , 26 , 26 , 27 , 27 , 28 , 28 , 28 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 32 , 32 , 32 , 33 ,
        33 , 34 , 34 , 34 , 35 , 35 , 36 , 36 , 36 , 37 , 37 , 38 , 38 , 38 , 39 , 39 , 40 , 40 , 40 , 41 ,
        41 , 42 , 42 , 42 , 43 , 43 , 44 , 44 , 44 , 45 , 45 , 46 , 46 , 46 , 47 , 47 , 48 , 48 , 48 , 49 ,
        49 , 49 , 50 , 50 , 51 , 51 , 51 , 52 , 52 , 53 , 53 , 53 , 54 , 54 , 55 , 55 , 55 , 56 , 56 , 57 ,
        57 , 57 , 58 , 58 , 59 , 59 , 59 , 60 , 60 , 61 , 61 , 61 , 62 , 62 , 63 , 63 , 63 , 64 , 64 , 65 ,
        65 , 65 , 66 , 66 , 67 , 67 , 67 , 68 , 68 , 69 , 69 , 69 , 70 , 70 , 71 , 71 , 71 , 72 , 72 , 73 ,
        73 , 73 , 74 , 74 , 75 , 75 , 75 , 76 , 76 , 77 , 77 , 77 , 78 , 78 , 79 , 79 , 79 , 80 , 80 , 81 ,
        81 , 81 , 82 , 82 , 83 , 83 , 83 , 84 , 84 , 85 , 85 , 85 , 86 , 86 , 87 , 87 , 87 , 88 , 88 , 88 ,
        89 , 89 , 90 , 90 , 90 , 91 , 91 , 92 , 92 , 92 , 93 , 93 , 94 , 94 , 94 , 95 , 95 , 96 , 96 , 96 ,
        97 , 97 , 98 , 98 , 98 , 99 , 99 , 100, 100, 100, 101, 101, 102, 102, 102, 103, 103, 104, 104, 104,
        105, 105, 106, 106, 106, 107, 107, 108, 108, 108, 109, 109, 110, 110, 110, 111, 111, 112, 112, 112,
        113, 113, 114, 114, 114, 115, 115, 116, 116, 116, 117, 117, 118, 118, 118, 119, 119, 120, 120, 120,
        121, 121, 122, 122, 122, 123, 123, 124, 124, 124, 125, 125, 126, 126, 126, 127, 127, 127, 128, 128,
        129, 129, 129, 130, 130, 131, 131, 131, 132, 132, 133, 133, 133, 134, 134, 135, 135, 135, 136, 136,
        137, 137, 137, 138, 138, 139, 139, 139, 140, 140, 141, 141, 141, 142, 142, 143, 143, 143, 144, 144,
        145, 145, 145, 146, 146, 147, 147, 147, 148, 148, 149, 149, 149, 150, 150, 151, 151, 151, 152, 152,
        153, 153, 153, 154, 154, 155, 155, 155, 156, 156, 157, 157, 157, 158, 158, 159, 159, 159, 160, 160,
        161, 161, 161, 162, 162, 163, 163, 163, 164, 164, 164, 165, 165, 166, 166, 166, 167, 167, 168, 168,
        168, 169, 169, 170, 170, 170, 171, 171, 172, 172, 172, 173, 173, 174, 174, 174, 175, 175, 176, 176,
        176, 177, 177, 178, 178, 178, 179, 179, 180, 180, 180, 181, 181, 182, 182, 182, 183, 183, 184, 184,
        184, 185, 185, 186, 186, 186, 187, 187, 188, 188, 188, 189, 189, 190, 190, 190, 191, 191, 192, 192,
        192, 193, 193, 194, 194, 194, 195, 195, 196, 196, 196, 197, 197, 198, 198, 198, 199, 199, 200, 200,
        200, 201, 201, 202, 202, 202, 203, 203, 203, 204, 204, 205, 205, 205, 206, 206, 207, 207, 207, 208,
        208, 209, 209, 209, 210, 210, 211
    };
    const diy_fp_t v = { .f = positive_base_lut[e], .e = positive_index_lut[e] };
    return v;
}

static inline diy_fp_t negative_diy_fp(int32_t e)
{
    static const uint64_t negative_base_lut[539] = {
        0x016345785d8a0000, 0x03782dace9d90000, 0x00de0b6b3a764000, 0x022b1c8c1227a000,
        0x056bc75e2d631000, 0x015af1d78b58c400, 0x03635c9adc5dea00, 0x00d8d726b7177a80,
        0x021e19e0c9bab240, 0x054b40b1f852bda0, 0x0152d02c7e14af68, 0x034f086f3b33b684,
        0x00d3c21bcecceda1, 0x0211654585005213, 0x052b7d2dcc80cd2f, 0x014adf4b7320334c,
        0x033b2e3c9fd0803d, 0x00cecb8f27f42010, 0x0204fce5e3e25027, 0x050c783eb9b5c860,
        0x01431e0fae6d7218, 0x0327cb2734119d3c, 0x07e37be2022c0915, 0x01f8def8808b0246,
        0x04ee2d6d415b85ad, 0x013b8b5b5056e16c, 0x0314dc6448d9338d, 0x07b426fab61f00df,
        0x01ed09bead87c038, 0x04d0985cb1d3608b, 0x013426172c74d823, 0x03025f39ef241c57,
        0x0785ee10d5da46da, 0x01e17b84357691b7, 0x04b3b4ca85a86c48, 0x012ced32a16a1b12,
        0x02f050fe938943ad, 0x0758ca7c70d72930, 0x01d6329f1c35ca4c, 0x04977e8dc68679be,
        0x0125dfa371a19e70, 0x02deaf189c140c17, 0x072cb5bd86321e39, 0x01cb2d6f618c878f,
        0x047bf19673df52e4, 0x011efc659cf7d4b9, 0x02cd76fe086b93cf, 0x0701a97b150cf184,
        0x01c06a5ec5433c61, 0x046109eced2816f3, 0x0118427b3b4a05bd, 0x02bca63414390e58,
        0x06d79f82328ea3db, 0x01b5e7e08ca3a8f7, 0x0446c3b15f992669, 0x0111b0ec57e6499b,
        0x02ac3a4edbbfb802, 0x06ae91c5255f4c04, 0x01aba4714957d301, 0x042d1b1b375b8f83,
        0x010b46c6cdd6e3e1, 0x029c30f1029939b2, 0x06867a5a867f103c, 0x01a19e96a19fc40f,
        0x04140c78940f6a25, 0x0105031e2503da8a, 0x028c87cb5c89a258, 0x065f537c675815da,
        0x0197d4df19d60577, 0x03fb942dc0970da9, 0x00fee50b7025c36b, 0x027d3c9c985e688a,
        0x063917877cec0557, 0x018e45e1df3b0156, 0x03e3aeb4ae138357, 0x00f8ebad2b84e0d6,
        0x026e4d30eccc3216, 0x0613c0fa4ffe7d37, 0x0184f03e93ff9f4e, 0x03cc589c71ff0e43,
        0x00f316271c7fc391, 0x025fb761c73f68ea, 0x05ef4a74721e8648, 0x017bd29d1c87a192,
        0x03b58e88c75313ed, 0x00ed63a231d4c4fc, 0x025179157c93ec74, 0x05cbaeb5b771cf22,
        0x0172ebad6ddc73c9, 0x039f4d3192a72176, 0x00e7d34c64a9c85e, 0x0243903efba874ea,
        0x05a8e89d75252447, 0x016a3a275d494912, 0x03899162693736ad, 0x00e264589a4dcdac,
        0x0235fadd81c2822c, 0x0586f329c466456e, 0x0161bcca7119915c, 0x037457fa1abfeb65,
        0x00dd15fe86affada, 0x0228b6fc50b7f31f, 0x0565c976c9cbdfcd, 0x0159725db272f7f4,
        0x035f9dea3e1f6be0, 0x00d7e77a8f87daf8, 0x021bc2b266d3a36c, 0x054566be0111188e,
        0x015159af80444624, 0x034b6036c0aaaf59, 0x00d2d80db02aabd7, 0x020f1c22386aad98,
        0x0525c6558d0ab1fb, 0x014971956342ac7f, 0x03379bf57826af3d, 0x00cde6fd5e09abd0,
        0x0202c1796b182d86, 0x0506e3af8bbc71cf, 0x0141b8ebe2ef1c74, 0x03244e4db755c722,
        0x07dac3c24a5671d3, 0x01f6b0f092959c75, 0x04e8ba596e760724, 0x013a2e965b9d81c9,
        0x03117477e509c477, 0x07aba32bbc986b29, 0x01eae8caef261acb, 0x04cb45fb55df42fa,
        0x0132d17ed577d0bf, 0x02ff0bbd15ab89dc, 0x077d9d58b62cd8a6, 0x01df67562d8b362a,
        0x04ae825771dc0768, 0x012ba095dc7701da, 0x02ed1176a72984a1, 0x0750aba8a1e7cb92,
        0x01d42aea2879f2e5, 0x04926b496530df3b, 0x01249ad2594c37cf, 0x02db830ddf3e8b85,
        0x0724c7a2ae1c5ccc, 0x01c931e8ab871733, 0x0476fcc5acd1ba00, 0x011dbf316b346e80,
        0x02ca5dfb8c031440, 0x06f9eaf4de07b2a0, 0x01be7abd3781eca8, 0x045c32d90ac4cfa4,
        0x01170cb642b133e9, 0x02b99fc7a6bb01c7, 0x06d00f7320d38470, 0x01b403dcc834e11c,
        0x044209a7f48432c6, 0x01108269fd210cb2, 0x02a94608f8d29fbc, 0x06a72f166e0e8f55,
        0x01a9cbc59b83a3d6, 0x04287d6e04c91995, 0x010a1f5b81324666, 0x02994e64c2fdaffe,
        0x067f43fbe77a37f9, 0x019fd0fef9de8dff, 0x040f8a7d70ac62fc, 0x0103e29f5c2b18bf,
        0x0289b68e666bbdde, 0x06584864000d5aa9, 0x01961219000356ab, 0x03f72d3e800858aa,
        0x00fdcb4fa002162b, 0x027a7c471005376a, 0x063236b1a80d0a89, 0x018c8dac6a0342a3,
        0x03df622f09082696, 0x00f7d88bc24209a6, 0x026b9d5d65a5181e, 0x060d09697e1cbc4a,
        0x0183425a5f872f13, 0x03c825e1eed1f5af, 0x00f209787bb47d6c, 0x025d17ad3543398d,
        0x05e8bb3105280fe0, 0x017a2ecc414a03f8, 0x03b174fea33909ec, 0x00ec5d3fa8ce427b,
        0x024ee91f2603a634, 0x05c546cddf091f81, 0x017151b377c247e1, 0x039b4c40ab65b3b1,
        0x00e6d3102ad96ced, 0x02410fa86b1f904f, 0x05a2a7250bcee8c4, 0x0168a9c942f3ba31,
        0x0385a8772761517b, 0x00e16a1dc9d8545f, 0x0233894a789cd2ed, 0x0580d73a2d880f50,
        0x016035ce8b6203d4, 0x037086845c750992, 0x00dc21a1171d4265, 0x02265412b9c925fb,
        0x055fd22ed076def4, 0x0157f48bb41db7bd, 0x035be35d424a4b59, 0x00d6f8d7509292d7,
        0x02196e1a496e6f18, 0x053f9341b79415ba, 0x014fe4d06de5056f, 0x0347bc0912bc8d94,
        0x00d1ef0244af2365, 0x020cd585abb5d87d, 0x052015ce2d469d38, 0x014805738b51a74e,
        0x03340da0dc4c2243, 0x00cd036837130891, 0x0200888489af956a, 0x0501554b5836f588,
        0x01405552d60dbd62, 0x0320d54f17225975, 0x07d21545b9d5dfa5, 0x01f485516e7577ea,
        0x04e34d4b9425abc7, 0x0138d352e5096af2, 0x030e104f3c978b5d, 0x07a328c6177adc67,
        0x01e8ca3185deb71a, 0x04c5f97bceacc9c1, 0x01317e5ef3ab3271, 0x02fbbbed612bfe19,
        0x077555d172edfb3d, 0x01dd55745cbb7ed0, 0x04a955a2e7d4bd06, 0x012a5568b9f52f42,
        0x02e9d585d0e4f624, 0x074895ce8a3c6759, 0x01d22573a28f19d7, 0x048d5da11665c098,
        0x0123576845997026, 0x02d85a84adff985f, 0x071ce24bb2fefced, 0x01c73892ecbfbf3c,
        0x04720d6f4fdf5e14, 0x011c835bd3f7d785, 0x02c7486591eb9acd, 0x06f234fdeccd0300,
        0x01bc8d3f7b3340c0, 0x0457611eb40021e0, 0x0115d847ad000878, 0x02b69cb33080152c,
        0x06c887bff94034ee, 0x01b221effe500d3c, 0x043d54d7fbc82115, 0x010f5535fef20846,
        0x02a65506fd5d14ad, 0x069fd4917968b3b0, 0x01a7f5245e5a2cec, 0x0423e4daebe1704e,
        0x0108f936baf85c14, 0x02966f08d36ce631, 0x0678159610903f7a, 0x019e056584240fdf,
        0x040b0d7dca5a27ac, 0x0102c35f729689eb, 0x0286e86e9e7858cc, 0x065145148c2cddfd,
        0x01945145230b3780, 0x03f2cb2cd79c0abe, 0x00fcb2cb35e702b0, 0x0277befc06c186b7,
        0x062b5d7610e3d0c9, 0x018ad75d8438f433, 0x03db1a69ca8e627e, 0x00f6c69a72a398a0,
        0x0268f0821e98fd8f, 0x060659454c7e79e4, 0x01819651531f9e79, 0x03c3f7cb4fcf0c2f,
        0x00f0fdf2d3f3c30c, 0x025a7adf11e1679e, 0x05e2332dacb38309, 0x01788ccb6b2ce0c3,
        0x03ad5ffc8bf031e6, 0x00eb57ff22fc0c7a, 0x024c5bfdd7761f30, 0x05bee5fa9aa74df7,
        0x016fb97ea6a9d37e, 0x03974fbca0a890bb, 0x00e5d3ef282a242f, 0x023e91d5e4695a75,
        0x059c6c96bb076223, 0x01671b25aec1d889, 0x0381c3de34e49d56, 0x00e070f78d392756,
        0x02311a6ae10ee256, 0x057ac20b32a535d6, 0x015eb082cca94d76, 0x036cb946ffa741a6,
        0x00db2e51bfe9d06a, 0x0223f3cc5fc88908, 0x0559e17eef755693, 0x0156785fbbdd55a5,
        0x03582cef55a9561c, 0x00d60b3bd56a5587, 0x02171c159589d5d2, 0x0539c635f5d8968c,
        0x014e718d7d7625a3, 0x03441be1b9a75e18, 0x00d106f86e69d786, 0x020a916d14089acf,
        0x051a6b90b2158305, 0x01469ae42c8560c2, 0x0330833a6f4d71e3, 0x07f9481216419cb7,
        0x01fe52048590672e, 0x04fbcd0b4de901f3, 0x013ef342d37a407d, 0x031d602710b1a138,
        0x07c97061a9bc130b, 0x01f25c186a6f04c3, 0x04dde63d0a158be7, 0x0137798f428562fa,
        0x030aafe6264d7770, 0x079ab7bf5fc1aa98, 0x01e6adefd7f06aa6, 0x04c0b2d79bd90a9f,
        0x01302cb5e6f642a8, 0x02f86fc6c167a6a4, 0x076d1770e3832099, 0x01db45dc38e0c827,
        0x04a42ea68e31f460, 0x01290ba9a38c7d18, 0x02e69d2818df38bc, 0x074088e43e2e0dd5,
        0x01d022390f8b8376, 0x0488558ea6dcc8a6, 0x01221563a9b7322a, 0x02d535792849fd68,
        0x071505aee4b8f982, 0x01c5416bb92e3e61, 0x046d238d4ef39bf2, 0x011b48e353bce6fd,
        0x02c4363851584177, 0x06ea878ccb5ca3a9, 0x01baa1e332d728eb, 0x045294b7ff19e64a,
        0x0114a52dffc67993, 0x02b39cf2ff702fee, 0x06c1085f7e9877d3, 0x01b04217dfa61df5,
        0x0438a53baf1f4ae4, 0x010e294eebc7d2b9, 0x02a367454d738ecf, 0x0698822d41a0e504,
        0x01a6208b50683941, 0x041f515c49048f23, 0x0107d457124123c9, 0x029392d9ada2d976,
        0x0670ef2032171fa6, 0x019c3bc80c85c7ea, 0x040695741f4e73c8, 0x0101a55d07d39cf2,
        0x02841d689391085d, 0x064a498570ea94e8, 0x019292615c3aa53a, 0x03ee6df366929d11,
        0x00fb9b7cd9a4a745, 0x027504b8201ba22b, 0x06248bcc5045156b, 0x018922f31411455b,
        0x03d6d75fb22b2d63, 0x00f5b5d7ec8acb59, 0x0266469bcf5afc5e, 0x05ffb085866376ea,
        0x017fec216198ddbb, 0x03bfce5373fe2a53, 0x00eff394dcff8a95, 0x0257e0f4287eda74,
        0x05dbb262653d2221, 0x0176ec98994f4889, 0x03a94f7d7f463555, 0x00ea53df5fd18d56,
        0x0249d1ae6f8be155, 0x05b88c3416ddb354, 0x016e230d05b76cd5, 0x039357a08e4a9015,
        0x00e4d5e82392a406, 0x023c16c458ee9a0d, 0x059638eade548120, 0x01658e3ab7952048,
        0x037de392caf4d0b4, 0x00df78e4b2bd342d, 0x022eae3bbed90271, 0x0574b3955d1e861a,
        0x015d2ce55747a187, 0x0368f03d5a3313d0, 0x00da3c0f568cc4f4, 0x02219626585fec62,
        0x0553f75fdcefcef5, 0x0154fdd7f73bf3be, 0x03547a9bea15e159, 0x00d51ea6fa857857,
        0x0214cca1724dacd8, 0x0533ff939dc2301b, 0x014cffe4e7708c07, 0x03407fbc42995e11,
        0x00d01fef10a65785, 0x02084fd5a99fdacb, 0x0514c796280fa2fb, 0x014531e58a03e8bf,
        0x032cfcbdd909c5dd, 0x07f077da9e986ea7, 0x01fc1df6a7a61baa, 0x04f64ae8a31f4529,
        0x013d92ba28c7d14b, 0x0319eed165f38b3a, 0x07c0d50b7ee0dc0f, 0x01f03542dfb83704,
        0x04d885272f4c898a, 0x01362149cbd32263, 0x030753387d8fd5f6, 0x0792500d39e796e7,
        0x01e494034e79e5ba, 0x04bb72084430be51, 0x012edc82110c2f95, 0x02f527452a9e76f3,
        0x0764e22cea8c295e, 0x01d9388b3aa30a58, 0x049f0d5c129799db, 0x0127c35704a5e677,
        0x02e368598b9ec029, 0x073884dfdd0ce065, 0x01ce2137f743381a, 0x0483530bea280c40,
        0x0120d4c2fa8a0310, 0x02d213e7725907a8, 0x070d31c29dde9323, 0x01c34c70a777a4c9,
        0x04683f19a2ab1bf6, 0x011a0fc668aac6fe, 0x02c1277005aaf17a, 0x06e2e2980e2b5bb0,
        0x01b8b8a6038ad6ec, 0x044dcd9f08db194e, 0x01137367c236c654, 0x02b0a0836588efd1,
        0x06b991487dd6578a, 0x01ae64521f7595e3, 0x0433facd4ea5f6b7, 0x010cfeb353a97dae,
        0x02a07cc05127ba32, 0x069137e0cae3517d, 0x01a44df832b8d460, 0x041ac2ec7ece12ee,
        0x0106b0bb1fb384bc, 0x0290b9d3cf40cbd5, 0x0669d0918621fd94, 0x019a742461887f65,
        0x0402225af3d53e7d, 0x01008896bcf54fa0, 0x02815578d865470e, 0x064355ae1cfd31a3,
        0x0190d56b873f4c69, 0x03ea158cd21e3f06, 0x00fa856334878fc2, 0x02724d780352e764,
        0x061dc1ac084f4279, 0x0187706b0213d09f, 0x03d2990b8531898c, 0x00f4a642e14c6263,
        0x02639fa7333ef5f7, 0x05f90f22001d66ea, 0x017e43c8800759bb, 0x03bba97540126052,
        0x00eeea5d50049815, 0x025549e9480b7c34, 0x05d538c7341cb680, 0x01754e31cd072da0,
        0x03a5437c8091f210, 0x00e950df20247c84, 0x02474a2dd05b374a, 0x05b2397288e40a39,
        0x016c8e5ca239028f, 0x038f63e7958e8664, 0x00e3d8f9e563a199, 0x02399e70bd7913ff,
        0x05900c19d9aeb1fc, 0x01640306766bac7f, 0x037a0790280d2f3e, 0x00de81e40a034bd0,
        0x022c44ba19083d87, 0x056eabd13e9499d0, 0x015baaf44fa52674, 0x03652b62c71ce022,
        0x00d94ad8b1c73809, 0x021f3b1dbc720c16, 0x054e13ca571d1e35, 0x015384f295c7478e,
        0x0350cc5e767232e2, 0x00d433179d9c8cb9, 0x02127fbb0a075fcd, 0x052e3f5399126f80,
        0x014b8fd4e6449be0, 0x033ce7943fab85b0, 0x00cf39e50feae16c, 0x020610bca7cb338e,
        0x050f29d7a37c00e3, 0x0143ca75e8df0039, 0x03297a26c62d808e, 0x07e7b160ef71c163,
        0x01f9ec583bdc7059, 0x04f0cedc95a718de, 0x013c33b72569c638, 0x03168149dd886f8b,
        0x07b84338a9d516da, 0x01ee10ce2a7545b7, 0x04d32a036a252e49, 0x0134ca80da894b93,
        0x0303fa4222573cee, 0x0789f1a555da1851, 0x01e27c6955768615, 0x04b6370755a84f33,
        0x012d8dc1d56a13cd, 0x02f1e26495893180, 0x075cb5fb75d6fbbf, 0x01d72d7edd75bef0,
        0x0499f1bd29a65d58, 0x01267c6f4a699756, 0x02e037163a07fa57, 0x073089b79113f1d9,
        0x01cc226de444fc77, 0x047e5612baac7728, 0x011f9584aeab1dca, 0x02cef5cbb4abca79,
        0x0705667d43ad7a2e, 0x01c1599f50eb5e8c, 0x0463600e4a4c6c5d, 0x0118d80392931b18,
        0x02be1c08ee6fc3ba, 0x06db461654176951, 0x01b6d1859505da55
    };

    static const uint8_t negative_index_lut[539] = {
        17 , 17 , 16 , 16 , 16 , 15 , 15 , 14 , 14 , 14 , 13 , 13 , 12 , 12 , 12 , 11 , 11 , 10 , 10 , 10 ,
        9  , 9  , 9  , 8  , 8  , 7  , 7  , 7  , 6  , 6  , 5  , 5  , 5  , 4  , 4  , 3  , 3  , 3  , 2  , 2  ,
        1  , 1  , 1  , 0  , 0  , 1  , 1  , 1  , 2  , 2  , 3  , 3  , 3  , 4  , 4  , 5  , 5  , 5  , 6  , 6  ,
        7  , 7  , 7  , 8  , 8  , 9  , 9  , 9  , 10 , 10 , 11 , 11 , 11 , 12 , 12 , 13 , 13 , 13 , 14 , 14 ,
        15 , 15 , 15 , 16 , 16 , 17 , 17 , 17 , 18 , 18 , 19 , 19 , 19 , 20 , 20 , 21 , 21 , 21 , 22 , 22 ,
        23 , 23 , 23 , 24 , 24 , 25 , 25 , 25 , 26 , 26 , 27 , 27 , 27 , 28 , 28 , 29 , 29 , 29 , 30 , 30 ,
        30 , 31 , 31 , 32 , 32 , 32 , 33 , 33 , 34 , 34 , 34 , 35 , 35 , 36 , 36 , 36 , 37 , 37 , 38 , 38 ,
        38 , 39 , 39 , 40 , 40 , 40 , 41 , 41 , 42 , 42 , 42 , 43 , 43 , 44 , 44 , 44 , 45 , 45 , 46 , 46 ,
        46 , 47 , 47 , 48 , 48 , 48 , 49 , 49 , 50 , 50 , 50 , 51 , 51 , 52 , 52 , 52 , 53 , 53 , 54 , 54 ,
        54 , 55 , 55 , 56 , 56 , 56 , 57 , 57 , 58 , 58 , 58 , 59 , 59 , 60 , 60 , 60 , 61 , 61 , 62 , 62 ,
        62 , 63 , 63 , 64 , 64 , 64 , 65 , 65 , 66 , 66 , 66 , 67 , 67 , 68 , 68 , 68 , 69 , 69 , 69 , 70 ,
        70 , 71 , 71 , 71 , 72 , 72 , 73 , 73 , 73 , 74 , 74 , 75 , 75 , 75 , 76 , 76 , 77 , 77 , 77 , 78 ,
        78 , 79 , 79 , 79 , 80 , 80 , 81 , 81 , 81 , 82 , 82 , 83 , 83 , 83 , 84 , 84 , 85 , 85 , 85 , 86 ,
        86 , 87 , 87 , 87 , 88 , 88 , 89 , 89 , 89 , 90 , 90 , 91 , 91 , 91 , 92 , 92 , 93 , 93 , 93 , 94 ,
        94 , 95 , 95 , 95 , 96 , 96 , 97 , 97 , 97 , 98 , 98 , 99 , 99 , 99 , 100, 100, 101, 101, 101, 102,
        102, 103, 103, 103, 104, 104, 105, 105, 105, 106, 106, 106, 107, 107, 108, 108, 108, 109, 109, 110,
        110, 110, 111, 111, 112, 112, 112, 113, 113, 114, 114, 114, 115, 115, 116, 116, 116, 117, 117, 118,
        118, 118, 119, 119, 120, 120, 120, 121, 121, 122, 122, 122, 123, 123, 124, 124, 124, 125, 125, 126,
        126, 126, 127, 127, 128, 128, 128, 129, 129, 130, 130, 130, 131, 131, 132, 132, 132, 133, 133, 134,
        134, 134, 135, 135, 136, 136, 136, 137, 137, 138, 138, 138, 139, 139, 140, 140, 140, 141, 141, 142,
        142, 142, 143, 143, 144, 144, 144, 145, 145, 145, 146, 146, 147, 147, 147, 148, 148, 149, 149, 149,
        150, 150, 151, 151, 151, 152, 152, 153, 153, 153, 154, 154, 155, 155, 155, 156, 156, 157, 157, 157,
        158, 158, 159, 159, 159, 160, 160, 161, 161, 161, 162, 162, 163, 163, 163, 164, 164, 165, 165, 165,
        166, 166, 167, 167, 167, 168, 168, 169, 169, 169, 170, 170, 171, 171, 171, 172, 172, 173, 173, 173,
        174, 174, 175, 175, 175, 176, 176, 177, 177, 177, 178, 178, 179, 179, 179, 180, 180, 181, 181, 181,
        182, 182, 183, 183, 183, 184, 184, 184, 185, 185, 186, 186, 186, 187, 187, 188, 188, 188, 189, 189,
        190, 190, 190, 191, 191, 192, 192, 192, 193, 193, 194, 194, 194, 195, 195, 196, 196, 196, 197
    };
    const diy_fp_t v = { .f = negative_base_lut[e], .e = e < 45 ? negative_index_lut[e] : -negative_index_lut[e] };
    return v;
}

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
    uint64_t remainder, div, div10;

    int32_t s = v->e & 1;
    int32_t e = v->e >> 1;
    diy_fp_t x = e >= 0 ? positive_diy_fp(e) : negative_diy_fp(-e);
    uint64_t f = v->f << s;
    uint64_t delta = x.f << s;

#if USING_U128_CALC
    static const u128 cmp = (u128)10000000000000000llu * 100000000000000000llu; // 1e33
    u128 m = (u128)f * x.f;
    m += delta >> 1;
    if (m < cmp) {
        v->e = e - x.e + 17;
        v->f = u128_div_1e17(m, &remainder);
        div   = 100000000000000000llu;
        div10 = 10000000000000000llu;
    } else {
        v->e = e - x.e + 18;
        v->f = u128_div_1e18(m, &remainder);
        div   = 1000000000000000000llu;
        div10 = 100000000000000000llu;
    }
#else
    u64x2_t cmp = {.hi = 54210108624275llu, .lo = 4089650035136921600llu};
    u64x2_t m = u128_mul(f, x.f);
    m = u128_add(m, delta >> 1);
    if (u128_cmp(m, cmp) < 0) {
        v->e = e - x.e + 17;
        v->f = u128_div_1e17(m, &remainder);
        div   = 100000000000000000llu;
        div10 = 10000000000000000llu;
    } else {
        v->e = e - x.e + 18;
        v->f = u128_div_1e18(m, &remainder);
        div   = 1000000000000000000llu;
        div10 = 100000000000000000llu;
    }
#endif

    if ((v->f & 1) && (remainder + div <= delta + div10)) {
        --v->f;
    }

    int32_t num_digits, trailing_zeros;
    if (v->f >= 10000000000000000llu) {
        memcpy(buffer, "10000000000000000", 17);
        trailing_zeros = 16;
        num_digits = 17;
    } else {
        num_digits = fill_significand(buffer, v->f, &trailing_zeros);
    }
    *vnum_digits = num_digits - trailing_zeros;
    return num_digits;
}

#else

/*
# python to get lut

def print_pow_array(unit, index, end, positive):
    cmp_value = 1<<64 # ULONG_MAX = 18446744073709551615
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
        print('0x%x' % (base_lut[i]), end='')
        if i != end:
            print(', ', end='');
        else:
            print()
            print('};')

    print()
    print('static const uint8_t %s_index_lut[%d] = {' % ('positive' if positive else 'negative', end + 1), end='')
    for i in range(end + 1):
        if i % 20 == 0:
            print()
            print('    ', end='')

        if positive:
            print('%-3d' % (index_lut[i] + 200), end='')
        else:
            print('%-3d' % (index_lut[i]), end='')

        if i != end:
            print(', ', end='');
        else:
            print()
            print('};')

def print_positive_array():
    # 1 * (2 ** 8) = 25.6 * (10 ** 1)
    # 256 = 25.6 * (10 ** 1)
    # 122 = (1023 - 52) / 8 + 1
    print_pow_array(256, 1, 122, True)

def print_negative_array():
    # 1 * (2 ** -8) = 0.0390625 * (10 ** -1)
    # 390625 = 0.0390625 * (10 ** 7)
    # 135 = (1022 + 52) / 8 + 1
    print_pow_array(390625, 7, 135, False)

print_positive_array()
print()
print_negative_array()
*/

static inline diy_fp_t positive_diy_fp(int32_t e)
{
    static const uint64_t positive_base_lut[123] = {
        0x8ac7230489e80000, 0x2386f26fc1000000, 0x5af3107a40000000, 0xe8d4a51000000000,
        0x3b9aca0000000000, 0x9896800000000000, 0x2710000000000000, 0x6400000000000000,
        0x199999999999999a, 0x4189374bc6a7ef9e, 0xa7c5ac471b478424, 0x2af31dc4611873c0,
        0x6df37f675ef6eae0, 0x1c25c268497681c3, 0x480ebe7b9d58566d, 0xb877aa3236a4b44a,
        0x2f394219248446bb, 0x78e480405d7b9659, 0x1ef2d0f5da7dd8ab, 0x4f3a68dbc8f03f25,
        0xcad2f7f5359a3b3f, 0x33ec47ab514e652f, 0x84ec3c97da624ab5, 0x22073a8515171d5e,
        0x571cbec554b60dbc, 0xdf01e85f912e37a4, 0x391704310a8acec2, 0x9226712162ab070e,
        0x256a18dd89e626ac, 0x5fc7edbc424d2fcc, 0xf53304714d9265e0, 0x3ec56164af81a34c,
        0xa0b19d2ab70e6ed7, 0x29233aaaadd6a38b, 0x694ff258c7443208, 0x1af5bf109550f22f,
        0x4504787c5f878ab6, 0xb0af48ec79ace838, 0x2d3b357c0692aa0b, 0x73cac65c39c96162,
        0x1da48ce468e7c703, 0x4be2b05d35848cd3, 0xc24452da229b021c, 0x31bb798fe8174c51,
        0x7f50935bebc0c35f, 0x2097b309321cde0c, 0x536fdecfdc72dc48, 0xd59944a37c0752a3,
        0x36ae679665622172, 0x8bfbea76c619ef37, 0x23d5fe9530aa7aae, 0x5bbd6d030bf1dde6,
        0xeadab0aba3b2dbe6, 0x3c1f689ea0b0dc23, 0x99ea0196163fa42f, 0x2766e9e0ca4dbb71,
        0x64de7fb01a60982a, 0x19d28f47b4d524e8, 0x421b0865a5f8b066, 0xa93af6c6c79b5d2e,
        0x2b52adc44bace4a8, 0x6ee8233e325e7251, 0x1c6463225ab7ec1d, 0x48af1243779966b1,
        0xba121a4650e4ddec, 0x2fa2548ce182451a, 0x79f17c49ef61f894, 0x1f37ad21436d0c70,
        0x4feab0f8fe87cdea, 0xcc963fee10b7d1b4, 0x345fced47f28e9e9, 0x8613fd0145877586,
        0x2252f0e5b39769dd, 0x57de91a832277568, 0xe0f218b8d25088b9, 0x39960a6cc11ac2bf,
        0x936b9fcebb25c996, 0x25bd5803c569edfa, 0x609d0a4718196b74, 0xf7549530e188c129,
        0x3f510b91a22f4c13, 0xa21727db38cb0030, 0x297ec285f1ddf3c3, 0x6a3a43e642383296,
        0x1b31bb5dc320d18f, 0x459e089e1c7cf9c0, 0xb23867fb2a35b28e, 0x2d9fd9154a4b2fc3,
        0x74cc692c434fd66c, 0x1de6815302e5559d, 0x4c8b888296c5f9e3, 0xc3f490aa77bd60fd,
        0x322a20f03f6bdf7d, 0x806bd9714632dff7, 0x20e037aa4f692f19, 0x542984435aa6def6,
        0xd77485cb25823ac8, 0x372811ddfd50715a, 0x8d3360f09cf6e4be, 0x2425ba9bce122614,
        0x5c898bcc4cfb42c3, 0xece53cec4a314ebe, 0x3ca52e50f85e8af2, 0x9b407691d7fc44f9,
        0x27be952349b969b9, 0x65beee6ed136d135, 0x1a0c03b1df8af612, 0x42ae1df050bfe694,
        0xaab37fd7d8f58179, 0x2bb31264501e14dc, 0x6fdee76733803565, 0x1ca38f350b22de91,
        0x4950cac53b3a8bb0, 0xbbb01b9283253ca3, 0x300c50c958de864f, 0x7b00ced03faa4d96,
        0x1f7d228322baf525, 0x509c814fb511cfba, 0xce5d73ff402d98e4, 0x34d4570a0c5566a1,
        0x873e4f75e2224e69, 0x229f4fbbdfc73f15, 0x58a213cc7a4ffda6
    };

    static const uint8_t positive_index_lut[123] = {
        219, 217, 216, 215, 213, 212, 210, 209, 207, 206, 205, 203, 202, 200, 199, 198, 196, 195, 193, 192,
        191, 189, 188, 186, 185, 184, 182, 181, 179, 178, 177, 175, 174, 172, 171, 169, 168, 167, 165, 164,
        162, 161, 160, 158, 157, 155, 154, 153, 151, 150, 148, 147, 146, 144, 143, 141, 140, 138, 137, 136,
        134, 133, 131, 130, 129, 127, 126, 124, 123, 122, 120, 119, 117, 116, 115, 113, 112, 110, 109, 108,
        106, 105, 103, 102, 100, 99 , 98 , 96 , 95 , 93 , 92 , 91 , 89 , 88 , 86 , 85 , 84 , 82 , 81 , 79 ,
        78 , 77 , 75 , 74 , 72 , 71 , 69 , 68 , 67 , 65 , 64 , 62 , 61 , 60 , 58 , 57 , 55 , 54 , 53 , 51 ,
        50 , 48 , 47
    };

    const diy_fp_t v = { .f = positive_base_lut[e], .e = positive_index_lut[e] - 200 };
    return v;
}

static inline diy_fp_t negative_diy_fp(int32_t e)
{
    static const uint64_t negative_base_lut[136] = {
        0x8ac7230489e80000, 0x3635c9adc5dea000, 0xd3c21bcecceda100, 0x52b7d2dcc80cd2e4,
        0x204fce5e3e250262, 0x7e37be2022c0914c, 0x314dc6448d9338c2, 0xc097ce7bc90715b4,
        0x4b3b4ca85a86c47b, 0x1d6329f1c35ca4c0, 0x72cb5bd86321e38d, 0x2cd76fe086b93ce3,
        0xaf298d050e4395d7, 0x446c3b15f9926688, 0x1aba4714957d300e, 0x6867a5a867f103b3,
        0x28c87cb5c89a2572, 0x9f4f2726179a2246, 0x3e3aeb4ae1383563, 0xf316271c7fc3908b,
        0x5ef4a74721e86477, 0x25179157c93ec73f, 0x90e40fbeea1d3a4b, 0x3899162693736ac6,
        0xdd15fe86affad913, 0x565c976c9cbdfccc, 0x21bc2b266d3a36c0, 0x83c7088e1aab65dc,
        0x3379bf57826af3ca, 0xc913936dd571c84d, 0x4e8ba596e760723e, 0x1eae8caef261aca1,
        0x77d9d58b62cd8a52, 0x2ed1176a72984a08, 0xb6e0c377cfa2e12f, 0x476fcc5acd1b9ff7,
        0x1be7abd3781eca7d, 0x6d00f7320d3846f5, 0x2a94608f8d29fbb8, 0xa6539930bf6bff46,
        0x40f8a7d70ac62fb8, 0xfdcb4fa002162a64, 0x63236b1a80d0a88f, 0x26b9d5d65a5181d8,
        0x9745eb4d50ce6333, 0x3b174fea33909ec0, 0xe6d3102ad96cec1e, 0x5a2a7250bcee8c3c,
        0x233894a789cd2ec8, 0x899504ae72497ebb, 0x35be35d424a4b581, 0xd1ef0244af236500,
        0x52015ce2d469d374, 0x200888489af9569a, 0x7d21545b9d5dfa47, 0x30e104f3c978b5c4,
        0xbeeefb584aff8604, 0x4a955a2e7d4bd05a, 0x1d22573a28f19d63, 0x71ce24bb2fefcecb,
        0x2c7486591eb9acc8, 0xada72ccc20054aea, 0x43d54d7fbc821144, 0x1a7f5245e5a2cebf,
        0x678159610903f798, 0x286e86e9e7858cb8, 0x9defbf01b061adac, 0x3db1a69ca8e627d7,
        0xf0fdf2d3f3c30ba0, 0x5e2332dacb38308b, 0x24c5bfdd7761f2f7, 0x8fa475791a569d11,
        0x381c3de34e49d55b, 0xdb2e51bfe9d0696b, 0x559e17eef755692e, 0x2171c159589d5d16,
        0x82a45b450226b39d, 0x330833a6f4d71e2a, 0xc75809c42c684dd2, 0x4dde63d0a158be66,
        0x1e6adefd7f06aa60, 0x76d1770e38320987, 0x2e69d2818df38bb9, 0xb54d5e4a127f59c9,
        0x46d238d4ef39bf13, 0x1baa1e332d728ea4, 0x6c1085f7e9877d2e, 0x2a367454d738ece6,
        0xa4e4b66b68b65d61, 0x40695741f4e73c7a, 0xfb9b7cd9a4a7443d, 0x6248bcc5045156a8,
        0x266469bcf5afc5da, 0x95f83d0a1fb69cda, 0x3a94f7d7f4635545, 0xe4d5e82392a40516,
        0x59638eade54811fd, 0x22eae3bbed902707, 0x8865899617fb1872, 0x3547a9bea15e158d,
        0xd01fef10a657842d, 0x514c796280fa2fa2, 0x1fc1df6a7a61ba9b, 0x7c0d50b7ee0dc0ee,
        0x30753387d8fd5f5d, 0xbd49d14aa79dbc83, 0x49f0d5c129799da3, 0x1ce2137f74338194,
        0x70d31c29dde93229, 0x2c1277005aaf1798, 0xac2820d9623bf42a, 0x433facd4ea5f6b61,
        0x1a44df832b8d45f2, 0x669d0918621fd938, 0x2815578d865470da, 0x9c935e00d4b9d8d3,
        0x3d2990b8531898b3, 0xeeea5d5004981479, 0x5d538c7341cb67ff, 0x2474a2dd05b374a0,
        0x8e679c2f5e44ff90, 0x37a0790280d2f3d4, 0xd94ad8b1c7380875, 0x54e13ca571d1e34e,
        0x2127fbb0a075fccb, 0x81842f29f2cce376, 0x3297a26c62d808db, 0xc5a05277621be294,
        0x4d32a036a252e482, 0x1e27c69557686143, 0x75cb5fb75d6fbbed, 0x2e037163a07fa569,
        0xb3bd72ed2af29e20, 0x463600e4a4c6c5c5, 0x1b6d1859505da541, 0x6b22271ce1edcd85
    };

    static const uint8_t negative_index_lut[136] = {
        19 , 20 , 22 , 23 , 24 , 26 , 27 , 29 , 30 , 31 , 33 , 34 , 36 , 37 , 38 , 40 , 41 , 43 , 44 , 46 ,
        47 , 48 , 50 , 51 , 53 , 54 , 55 , 57 , 58 , 60 , 61 , 62 , 64 , 65 , 67 , 68 , 69 , 71 , 72 , 74 ,
        75 , 77 , 78 , 79 , 81 , 82 , 84 , 85 , 86 , 88 , 89 , 91 , 92 , 93 , 95 , 96 , 98 , 99 , 100, 102,
        103, 105, 106, 107, 109, 110, 112, 113, 115, 116, 117, 119, 120, 122, 123, 124, 126, 127, 129, 130,
        131, 133, 134, 136, 137, 138, 140, 141, 143, 144, 146, 147, 148, 150, 151, 153, 154, 155, 157, 158,
        160, 161, 162, 164, 165, 167, 168, 169, 171, 172, 174, 175, 176, 178, 179, 181, 182, 184, 185, 186,
        188, 189, 191, 192, 193, 195, 196, 198, 199, 200, 202, 203, 205, 206, 207, 209
    };

    const diy_fp_t v = { .f = negative_base_lut[e], .e = negative_index_lut[e] };
    return v;
}

static inline int32_t fill_significand(char *buffer, uint64_t digits, int32_t *ptz)
{
    char *s = buffer;

    uint32_t q = (uint32_t)(digits / 100000000);
    uint32_t r = (uint32_t)(digits - (uint64_t)q * 100000000);

    uint32_t q1 = FAST_DIV10000(q);
    uint32_t r1 = q - q1 * 10000;

    uint32_t q2 = FAST_DIV100(q1);
    uint32_t r2 = q1 - q2 * 100;

    if (q2 >= 10) {
        memcpy(s, &ch_100_lut[q2<<1], 2);
        s += 2;
        *ptz = tz_100_lut[q2];
    } else {
        *s++ = q2 + '0';
        *ptz = 0;
    }

    if (!r2) {
        memset(s, '0', 2);
        s += 2;
        *ptz += 2;
    } else {
        memcpy(s, &ch_100_lut[r2<<1], 2);
        s += 2;
        *ptz = tz_100_lut[r2];
    }

    if (!r1) {
        memset(s, '0', 4);
        s += 4;
        *ptz += 4;
    } else {
        s += fill_t_4_digits(s, r1, ptz);
    }

    if (!r) {
        memset(s, '0', 8);
        s += 8;
        *ptz += 8;
    } else {
        s += fill_t_8_digits(s, r, ptz);
    }

    return s - buffer;
}

static inline int32_t ldouble_convert(diy_fp_t *v, char *buffer, int32_t *vnum_digits)
{
    uint64_t f = v->f << (v->e & 7);
    int32_t e = v->e >> 3;
    diy_fp_t x = e >= 0 ? positive_diy_fp(e) : negative_diy_fp(-e);

#if USING_U128_CALC && !USING_FLOAT_MUL
    const u128 m = (u128)f * x.f;

    static const u128 cmp1 = (u128)99999999999999950llu   * 1000000000000000000llu;
    static const u128 cmp2 = (u128)999999999999999500llu  * 1000000000000000000llu;
    static const u128 cmp3 = (u128)9999999999999995000llu * 1000000000000000000llu;

    if (m < cmp1) {         // 1e35 - 5e19
        static const u128 addn = (u128)10 * 5000000000000000000llu;
        static const u128 divn = (u128)10 * 10000000000000000000llu;
        v->f = (uint64_t)((m + addn) / divn);
        v->e = e - x.e + 20;
    } if (m < cmp2) {       // 1e36 - 5e20
        static const u128 addn = (u128)100 * 5000000000000000000llu;
        static const u128 divn = (u128)100 * 10000000000000000000llu;
        v->f = (uint64_t)((m + addn) / divn);
        v->e = e - x.e + 21;
    } else if (m < cmp3) {  // 1e37 - 5e21
        static const u128 addn = (u128)1000 * 5000000000000000000llu;
        static const u128 divn = (u128)1000 * 10000000000000000000llu;
        v->f = (uint64_t)((m + addn) / divn);
        v->e = e - x.e + 22;
    } else {                // 1e38 - 5e22
        static const u128 addn = (u128)10000 * 5000000000000000000llu;
        static const u128 divn = (u128)10000 * 10000000000000000000llu;
        v->f = (uint64_t)((m + addn) / divn);
        v->e = e - x.e + 23;
    }

#else

    double d = (double)f * x.f;
    if (d + 5e19 < 1e35) {
        v->f = (uint64_t)((d + 5e19) * 1e-20);
        v->e = e - x.e + 20;
    } else if (d + 5e20 < 1e36) {
        v->f = (uint64_t)((d + 5e20) * 1e-21);
        v->e = e - x.e + 21;
    } else if (d + 5e21 < 1e37) {
        v->f = (uint64_t)((d + 5e21) * 1e-22);
        v->e = e - x.e + 22;
    } else {
        v->f = (uint64_t)((d + 5e22) * 1e-23);
        v->e = e - x.e + 23;
    }
#endif

    int32_t num_digits, trailing_zeros;
    num_digits = fill_significand(buffer, v->f, &trailing_zeros);
    *vnum_digits = num_digits - trailing_zeros;
    return num_digits;
}
#endif

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

        if (0 <= -v.e && -v.e <= DP_SIGNIFICAND_SIZE && ((v.f & (((uint64_t)1 << -v.e) - 1)) == 0)) {
            /* small integer. */
            int32_t tz, n;
            n = fill_1_20_digits(s, v.f >> -v.e, &tz);
            memcpy(s + n, ".0", 3);
            return n + 2 + signbit;
        } else {
            /* (-1022 - 52) <= e <= (1023 - 52) */
            num_digits = ldouble_convert(&v, s + 1, &vnum_digits);
        }
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
            double d = m * exponent_lut[i - MIN_NEG_EXP];
            *v |= neg << 63;
            return d;
        }
    }

    bm = 64 - u64_pz_get(m);
    i -= MIN_NEG_EXP;
    if (i < 0) {
        if (bm - 1 + i < 0) {
            *v = neg << 63;
            return d;
        } else {
            m >>= -i;
            i = 0;
            bm += i;
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
    m >>= bits;
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
