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

/* Using a custom division instead of u128 division. */
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

#if USING_U128_CALC
#if !USING_DIV_EXP
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

static inline uint64_t u128_div_1e19(u128 dividend, uint64_t *remainder)
{
    uint64_t ret = dividend / 10000000000000000000llu;
    *remainder = dividend - (u128)ret * 10000000000000000000llu;
    return ret;
}

#else // !USING_DIV_EXP

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

static inline uint64_t u128_div_1e17(u128 dividend, uint64_t *remainder)
{
    return u128_div_exp(dividend, remainder, 0xb877aa3236a4b44a, 0x000000b1a2bc2ec5, 103, 17);
}

static inline uint64_t u128_div_1e18(u128 dividend, uint64_t *remainder)
{
    return u128_div_exp(dividend, remainder, 0x9392ee8e921d5d08, 0x000003782dace9d9, 105, 18);
}

static inline uint64_t u128_div_1e19(u128 dividend, uint64_t *remainder)
{
    return u128_div_exp(dividend, remainder, 0xec1e4a7db69561a6, 0x00001158e460913d, 108, 19);
}
#endif // !USING_DIV_EXP

#else // USING_U128_CALC
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

#if defined(_MSC_VER) && defined(_M_AMD64)
static inline uint64_t u128_div_1e17(u64x2_t dividend, uint64_t *remainder)
{
    return _udiv128(dividend.hi, dividend.lo, 100000000000000000llu, remainder);
}

static inline uint64_t u128_div_1e18(u64x2_t dividend, uint64_t *remainder)
{
    return _udiv128(dividend.hi, dividend.lo, 1000000000000000000llu, remainder);
}

static inline uint64_t u128_div_1e19(u64x2_t dividend, uint64_t *remainder)
{
    return _udiv128(dividend.hi, dividend.lo, 10000000000000000000llu, remainder);
}

#else // defined(_MSC_VER) && defined(_M_AMD64)

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

static inline uint64_t u128_div_1e19(u64x2_t dividend, uint64_t *remainder)
{
    return u128_div_exp(dividend, remainder, 0xec1e4a7db69561a6, 0x00001158e460913d, 108, 19);
}
#endif // defined(_MSC_VER) && defined(_M_AMD64)
#endif // USING_U128_CALC

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

    uint8_t last_digit = 0;
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

        if (remainder > delta + f) {
            last_digit = remainder / 100000000;
            --v->e;
        } else {
            uint64_t tail = v->f % 10;
            if (remainder + cmp_lut[tail] <= delta + f) {
                v->f -= tail;
            }
        }

        if (v->f >= 100000000) {
            memcpy(buffer, "100000000", 9);
            trailing_zeros = 8;
            num_digits = 9;
        } else {
            num_digits = fill_1_8_digits(buffer, v->f, &trailing_zeros);
            if (last_digit) {
                buffer[num_digits++] = last_digit + '0';
                trailing_zeros = 0;
            }
        }
        *vnum_digits = num_digits - trailing_zeros;
        return num_digits;

    } else {
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
        if (remainder > delta + f) {
            last_digit = remainder / 10000000000000000llu;
            --v->e;
        } else {
            uint64_t tail = v->f % 10;
            if (remainder + cmp_lut[tail] <= delta + f) {
                v->f -= tail;
            }
        }

        if (v->f >= 10000000000000000llu) {
            memcpy(buffer, "10000000000000000", 17);
            trailing_zeros = 16;
            num_digits = 17;
        } else {
            num_digits = fill_1_16_digits(buffer, v->f, &trailing_zeros);
            if (last_digit) {
                buffer[num_digits++] = last_digit + '0';
                trailing_zeros = 0;
            }
        }
        *vnum_digits = num_digits - trailing_zeros;
        return num_digits;
    }
    return 0;
}

/*
# python to get lut
def print_pow_array(unit, index, end, positive):
    cmp_value = 1<<62 # ULONG_MAX = 18446744073709551615
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
        0x0de0b6b3a7640000, 0x3782dace9d900000, 0x16345785d8a00000, 0x08e1bc9bf0400000,
        0x2386f26fc1000000, 0x0e35fa931a000000, 0x38d7ea4c68000000, 0x16bcc41e90000000,
        0x09184e72a0000000, 0x246139ca80000000, 0x0e8d4a5100000000, 0x3a35294400000000,
        0x174876e800000000, 0x09502f9000000000, 0x2540be4000000000, 0x0ee6b28000000000,
        0x3b9aca0000000000, 0x17d7840000000000, 0x0989680000000000, 0x2625a00000000000,
        0x0f42400000000000, 0x3d09000000000000, 0x186a000000000000, 0x09c4000000000000,
        0x2710000000000000, 0x0fa0000000000000, 0x3e80000000000000, 0x1900000000000000,
        0x0a00000000000000, 0x2800000000000000, 0x1000000000000000, 0x0666666666666667,
        0x199999999999999a, 0x0a3d70a3d70a3d71, 0x28f5c28f5c28f5c3, 0x10624dd2f1a9fbe8,
        0x068db8bac710cb2a, 0x1a36e2eb1c432ca6, 0x0a7c5ac471b47843, 0x29f16b11c6d1e109,
        0x10c6f7a0b5ed8d37, 0x06b5fca6af2bd216, 0x1ad7f29abcaf4858, 0x0abcc77118461cf0,
        0x2af31dc4611873c0, 0x112e0be826d694b3, 0x06df37f675ef6eae, 0x1b7cdfd9d7bdbab8,
        0x0afebff0bcb24ab0, 0x2bfaffc2f2c92ac0, 0x119799812dea111a, 0x0709709a125da071,
        0x1c25c268497681c3, 0x0b424dc35095cd81, 0x2d09370d42573604, 0x1203af9ee756159c,
        0x0734aca5f6226f0b, 0x1cd2b297d889bc2c, 0x0b877aa3236a4b45, 0x2e1dea8c8da92d13,
        0x12725dd1d243aba1, 0x0760f253edb4ab0e, 0x1d83c94fb6d2ac35, 0x0bce5086492111af,
        0x2f394219248446bb, 0x12e3b40a0e9b4f7e, 0x078e480405d7b966, 0x1e392010175ee597,
        0x0c16d9a0095928a3, 0x305b66802564a28a, 0x1357c299a88ea76b, 0x07bcb43d769f762b,
        0x1ef2d0f5da7dd8ab, 0x0c612062576589de, 0x318481895d962777, 0x13ce9a36f23c0fca,
        0x07ec3daf94180651, 0x1fb0f6be50601942, 0x0cad2f7f5359a3b4, 0x32b4bdfd4d668ed0,
        0x14484bfeebc29f87, 0x081ceb32c4b43fd0, 0x2073accb12d0ff3e, 0x0cfb11ead453994c,
        0x33ec47ab514e652f, 0x14c4e977ba1f5bad, 0x084ec3c97da624ac, 0x213b0f25f69892ae,
        0x0d4ad2dbfc3d0779, 0x352b4b6ff0f41de2, 0x154484932d2e725b, 0x0881cea14545c758,
        0x22073a8515171d5e, 0x0d9c7dced53c7226, 0x3671f73b54f1c896, 0x15c72fb1552d836f,
        0x08b61313bbabce2d, 0x22d84c4eeeaf38b2, 0x0df01e85f912e37b, 0x37c07a17e44b8de9,
        0x164cfda3281e38c4, 0x08eb98a7a9a5b04f, 0x23ae629ea696c139, 0x0e45c10c42a2b3b1,
        0x391704310a8acec2, 0x16d601ad376ab91b, 0x09226712162ab071, 0x24899c4858aac1c4,
        0x0e9d71b689dde71b, 0x3a75c6da27779c6c, 0x17624f8a762fd82c, 0x095a8637627989ab,
        0x256a18dd89e626ac, 0x0ef73d256a5c0f78, 0x3bdcf495a9703de0, 0x17f1fb6f10934bf3,
        0x0993fe2c6d07b7fb, 0x264ff8b1b41edfeb, 0x0f53304714d9265e, 0x3d4cc11c53649978,
        0x18851a0b548ea3ca, 0x09ced737bb6c4184, 0x273b5cdeedb10610, 0x0fb158592be068d3,
        0x3ec56164af81a34c, 0x191bc08eac9a4152, 0x0a0b19d2ab70e6ee, 0x282c674aadc39bb6,
        0x1011c2eaabe7d7e3, 0x066d812aab29898e, 0x19b604aaaca62637, 0x0a48ceaaab75a8e3,
        0x29233aaaadd6a38b, 0x10747ddddf22a7d2, 0x0694ff258c744321, 0x1a53fc9631d10c82,
        0x0a87fea27a539e9b, 0x2a1ffa89e94e7a6a, 0x10d9976a5d52975e, 0x06bd6fc425543c8c,
        0x1af5bf109550f22f, 0x0ac8b2d36eed2dad, 0x2b22cb4dbbb4b6b2, 0x11411e1f17e1e2ae,
        0x06e6d8d93cc0c113, 0x1b9b6364f3030449, 0x0b0af48ec79ace84, 0x2c2bd23b1e6b3a0e,
        0x11ab20e472914a6c, 0x0711405b6106ea92, 0x1c45016d841baa47, 0x0b4ecd5f01a4aa83,
        0x2d3b357c0692aa0b, 0x1217aefe69077738, 0x073cac65c39c9617, 0x1cf2b1970e725859,
        0x0b94470938fa89bd, 0x2e511c24e3ea26f4, 0x1286d80ec190dc62, 0x076923391a39f1c1,
        0x1da48ce468e7c703, 0x0bdb6b8e905cb601, 0x2f6dae3a4172d804, 0x12f8ac174d612335,
        0x0796ab3c855a0e16, 0x1e5aacf215683855, 0x0c24452da229b022, 0x309114b688a6c087,
        0x136d3b7c36a919d0, 0x07c54afe7c43a3ed, 0x1f152bf9f10e8fb3, 0x0c6ede63fa05d315,
        0x31bb798fe8174c51, 0x13e497065cd61e87, 0x07f50935bebc0c36, 0x1fd424d6faf030d8,
        0x0cbb41ef979346bd, 0x32ed07be5e4d1af3, 0x145ecfe5bf520ac8, 0x0825ecc24c873783,
        0x2097b309321cde0c, 0x0d097ad07a71f26c, 0x3425eb41e9c7c9ad, 0x14dbf7b3f71cb712,
        0x0857fcae62d8493b, 0x215ff2b98b6124ea, 0x0d59944a37c0752b, 0x35665128df01d4a9,
        0x155c2076bf9a5511, 0x088b402f7fd7553a, 0x222d00bdff5d54e7, 0x0dab99e59958885d,
        0x36ae679665622172, 0x15df5ca28ef40d61, 0x08bfbea76c619ef4, 0x22fefa9db1867bce,
        0x0dff9772470297ec, 0x37fe5dc91c0a5fb0, 0x1665bf1d3e6a8cad, 0x08f57fa54c2a9eac,
        0x23d5fe9530aa7aae, 0x0e55990879ddcaac, 0x39566421e7772ab0, 0x16ef5b40c2fc777a,
        0x092c8ae6b464fc97, 0x24b22b9ad193f25c, 0x0eadab0aba3b2dbf, 0x3ab6ac2ae8ecb6fa,
        0x177c44ddf6c515fe, 0x0964e858c91ba266, 0x2593a163246e8996, 0x0f07da27a82c3709,
        0x3c1f689ea0b0dc23, 0x180c903f7379f1a8, 0x099ea0196163fa43, 0x267a8065858fe90c,
        0x0f64335bcf065d38, 0x3d90cd6f3c1974e0, 0x18a0522c7e709527, 0x09d9ba7832936edd,
        0x2766e9e0ca4dbb71, 0x0fc2c3f3841f17c7, 0x3f0b0fce107c5f1a, 0x19379fec0698260b,
        0x0a163ff802a3426b, 0x2858ffe00a8d09ab, 0x1023998cd1053711, 0x0674a3d1ed35493a,
        0x19d28f47b4d524e8, 0x0a54394fe1eedb90, 0x2950e53f87bb6e40, 0x1086c219697e2c1a,
        0x069c4da3c3cc11a4, 0x1a71368f0f304690, 0x0a93af6c6c79b5d3, 0x2a4ebdb1b1e6d74c,
        0x10ec4be0ad8f8952, 0x06c4eb26abd303bb, 0x1b13ac9aaf4c0ee9, 0x0ad4ab7112eb392a,
        0x2b52adc44bace4a8, 0x11544581b7dec1dd, 0x06ee8233e325e726, 0x1bba08cf8c979c95,
        0x0b1736b96b6fd83c, 0x2c5cdae5adbf60ed, 0x11bebdf578b2f392, 0x071918c896adfb08,
        0x1c6463225ab7ec1d, 0x0b5b5ada8aaff80c, 0x2d6d6b6a2abfe02f, 0x122bc490dde659ad,
        0x0744b506bf28f0ac, 0x1d12d41afca3c2ad, 0x0ba121a4650e4ddf, 0x2e8486919439377b,
        0x129b69070816e2fe, 0x07715d36033c5acc, 0x1dc574d80cf16b30, 0x0be8952338609147,
        0x2fa2548ce182451a, 0x130dbb6b8d674ed7, 0x079f17c49ef61f8a, 0x1e7c5f127bd87e25,
        0x0c31bfa0fe5698dc, 0x30c6fe83f95a636f, 0x1382cc34ca2427c6, 0x07cdeb4850db431c,
        0x1f37ad21436d0c70, 0x0c7caba6e7c5382d, 0x31f2ae9b9f14e0b3, 0x13faac3e3fa1f37b,
        0x07fdde7f4ca72e31, 0x1ff779fd329cb8c4, 0x0cc963fee10b7d1c, 0x33258ffb842df46d,
        0x14756ccb01abfb5f, 0x082ef85133de648d, 0x20bbe144cf799232, 0x0d17f3b51fca3a7b,
        0x345fced47f28e9e9, 0x14f31f8832dd2a5d, 0x08613fd014587759, 0x2184ff405161dd62,
        0x0d686619ba27255b, 0x35a19866e89c9569, 0x1573d68f903ea22a, 0x0894bc396ce5da78,
        0x2252f0e5b39769dd, 0x0dbac6c247d62a59, 0x36eb1b091f58a961, 0x15f7a46a0c89dd5a,
        0x08c974f738372558, 0x2325d3dce0dc955d, 0x0e0f218b8d25088c, 0x383c862e3494222f,
        0x167e9c127b6e7413, 0x08ff71a0fe2c2e6e, 0x23fdc683f8b0b9b8, 0x0e65829b3046b0b0,
        0x39960a6cc11ac2bf, 0x1708d0f84d3de780, 0x0936b9fcebb25c9a, 0x24dae7f3aec97266,
        0x0ebdf661791d60f6, 0x3af7d985e47583d6, 0x179657025b6234bc, 0x096f5600f15a7b7f,
        0x25bd5803c569edfa, 0x0f18899b1bc3f8cb, 0x3c62266c6f0fe329, 0x18274291c6065add,
        0x09a94dd3e8cf578c, 0x26a5374fa33d5e2f, 0x0f7549530e188c13, 0x3dd5254c3862304b,
        0x18bba884e35a79b8, 0x09e4a9cec15763e3, 0x2792a73b055d8f8c, 0x0fd442e4688bd305,
        0x3f510b91a22f4c13, 0x19539e3a40dfb808, 0x0a21727db38cb003, 0x2885c9f6ce32c00c,
        0x103583fc527ab338, 0x067bce64edcaae17, 0x19ef3993b72ab85a, 0x0a5fb0a17c777cf1,
        0x297ec285f1ddf3c3, 0x10991a9bfa58c7e8, 0x06a3a43e6423832a, 0x1a8e90f9908e0ca6,
        0x0a9f6d30a038d1dc, 0x2a7db4c280e34770, 0x10ff151a99f482fa, 0x06cc6ed770c83464,
        0x1b31bb5dc320d18f, 0x0ae0b158b4738706, 0x2b82c562d1ce1c18, 0x11678227871f3e70,
        0x06f6340fcfa618fa, 0x1bd8d03f3e9863e7, 0x0b23867fb2a35b29, 0x2c8e19feca8d6ca4,
        0x11d270cc51055ea8, 0x0720f9eb539bbf77, 0x1c83e7ad4e6efdda, 0x0b67f6455292cbf1,
        0x2d9fd9154a4b2fc3, 0x123ff06eea847981, 0x074cc692c434fd67, 0x1d331a4b10d3f59b,
        0x0bae0a846d219572, 0x2eb82a11b48655c5, 0x12b010d3e1cf5582, 0x0779a054c0b95568,
        0x1de6815302e5559d, 0x0bf5cd54678eef0c, 0x2fd735519e3bbc2e, 0x1322e220a5b17e79,
        0x07a78da6a8ad65ca, 0x1e9e369aa2b59728, 0x0c3f490aa77bd610, 0x30fd242a9def5840,
        0x139874ddd8c6234d, 0x07d6952589e8daec, 0x1f5a549627a36bae, 0x0c8a883c0fdaf7e0,
        0x322a20f03f6bdf7d, 0x1410d9f9b2f7f2ff, 0x0806bd9714632e00, 0x201af65c518cb7fe,
        0x0cd795be87051666, 0x335e56fa1c145996, 0x148c22ca71a1bd70, 0x08380dea93da4bc7,
        0x20e037aa4f692f19, 0x0d267caa862a12d7, 0x3499f2aa18a84b5a, 0x150a6110d6a9b7be,
        0x086a8d39ef77164c, 0x21aa34e7bddc5930, 0x0d77485cb25823ad, 0x35dd2172c9608eb2,
        0x158ba6fab6f36c48, 0x089e42caaf9491b7, 0x22790b2abe5246d9, 0x0dca04777f541c57,
        0x372811ddfd50715a, 0x16100725988693be, 0x08d3360f09cf6e4c, 0x234cd83c273db930,
        0x0e1ebce4dc7f16e0, 0x387af39371fc5b7f, 0x169794a160cb57cd, 0x09096ea6f3848985,
        0x2425ba9bce122614, 0x0e757dd7ec07426f, 0x39d5f75fb01d09ba, 0x172262f3133ed0b1,
        0x0940f4613ae5ed14, 0x2503d184eb97b44e, 0x0ece53cec4a314ec, 0x3b394f3b128c53b0,
        0x17b08617a104ee47, 0x0979cf3ca6cec5b6, 0x25e73cf29b3b16d7, 0x0f294b943e17a2bd,
        0x3ca52e50f85e8af2, 0x18421286c9bf6ac7, 0x09b407691d7fc450, 0x26d01da475ff113f,
        0x0f867241c8cc6d4d, 0x3e19c9072331b531, 0x18d71d360e13e214, 0x09efa548d26e5a6f,
        0x27be952349b969b9, 0x0fe5d54150b090b1, 0x3f97550542c242c1, 0x196fbb9bb44db44e,
        0x0a2cb1717b52481f, 0x28b2c5c5ed49207c, 0x1047824f2bb6d9cb, 0x068300ec77e2bd85,
        0x1a0c03b1df8af612, 0x0a6b34ad8c9dfc07, 0x29acd2b63277f01c, 0x10ab877c142ff9a5,
        0x06ab02fe6e79970f, 0x1aac0bf9b9e65c3b, 0x0aab37fd7d8f5818, 0x2aacdff5f63d605f,
        0x1111f32f2f4bc026, 0x06d3fadfac84b343, 0x1b4feb7eb212cd0a, 0x0aecc49914078537,
        0x2bb31264501e14dc, 0x117ad428200c0858, 0x06fdee7673380357, 0x1bf7b9d9cce00d5a,
        0x0b2fe3f0b8599ef1, 0x2cbf8fc2e1667bc2, 0x11e6398126f5cb1b, 0x0728e3cd42c8b7a5,
        0x1ca38f350b22de91, 0x0b749faed14125d4, 0x2dd27ebb4504974e, 0x125432b14ecea2ec,
        0x0754e113b91f745f, 0x1d53844ee47dd17a, 0x0bbb01b9283253cb, 0x2eec06e4a0c94f29,
        0x12c4cf8ea6b6ec77, 0x0781ec9f75e2c4fd, 0x1e07b27dd78b13f2, 0x0c0314325637a194,
        0x300c50c958de864f, 0x1338205089f29c20, 0x07b00ced03faa4da, 0x1ec033b40fea9366,
        0x0c4ce17b399107c3, 0x313385ece6441f09, 0x13ae3591f5b4d937, 0x07df48a0c8aebd4a,
        0x1f7d228322baf525, 0x0c987434744ac875, 0x3261d0d1d12b21d4, 0x14272053ed4473ef,
        0x080fa687f881c7f9, 0x203e9a1fe2071fe4, 0x0ce5d73ff402d98f, 0x33975cffd00b6639,
        0x14a2f1ffecd15c17, 0x08412d9991ed580a, 0x2104b66647b56025, 0x0d3515c2831559a9,
        0x34d4570a0c5566a1, 0x1521bc6a6b555c41, 0x0873e4f75e2224e7, 0x21cf93dd7888939b,
        0x0d863b256369d4a5, 0x3618ec958da75291, 0x15a391d56bdc876d, 0x08a7d3eef7f1cfc6,
        0x229f4fbbdfc73f15, 0x0dd95317f31c7fa2, 0x37654c5fcc71fe88
    };

    static const uint8_t positive_index_lut[487] = {
        18 , 19 , 19 , 19 , 20 , 20 , 21 , 21 , 21 , 22 , 22 , 23 , 23 , 23 , 24 , 24 , 25 , 25 , 25 , 26 ,
        26 , 27 , 27 , 27 , 28 , 28 , 29 , 29 , 29 , 30 , 30 , 30 , 31 , 31 , 32 , 32 , 32 , 33 , 33 , 34 ,
        34 , 34 , 35 , 35 , 36 , 36 , 36 , 37 , 37 , 38 , 38 , 38 , 39 , 39 , 40 , 40 , 40 , 41 , 41 , 42 ,
        42 , 42 , 43 , 43 , 44 , 44 , 44 , 45 , 45 , 46 , 46 , 46 , 47 , 47 , 48 , 48 , 48 , 49 , 49 , 50 ,
        50 , 50 , 51 , 51 , 52 , 52 , 52 , 53 , 53 , 54 , 54 , 54 , 55 , 55 , 56 , 56 , 56 , 57 , 57 , 58 ,
        58 , 58 , 59 , 59 , 60 , 60 , 60 , 61 , 61 , 62 , 62 , 62 , 63 , 63 , 64 , 64 , 64 , 65 , 65 , 66 ,
        66 , 66 , 67 , 67 , 68 , 68 , 68 , 69 , 69 , 69 , 70 , 70 , 71 , 71 , 71 , 72 , 72 , 73 , 73 , 73 ,
        74 , 74 , 75 , 75 , 75 , 76 , 76 , 77 , 77 , 77 , 78 , 78 , 79 , 79 , 79 , 80 , 80 , 81 , 81 , 81 ,
        82 , 82 , 83 , 83 , 83 , 84 , 84 , 85 , 85 , 85 , 86 , 86 , 87 , 87 , 87 , 88 , 88 , 89 , 89 , 89 ,
        90 , 90 , 91 , 91 , 91 , 92 , 92 , 93 , 93 , 93 , 94 , 94 , 95 , 95 , 95 , 96 , 96 , 97 , 97 , 97 ,
        98 , 98 , 99 , 99 , 99 , 100, 100, 101, 101, 101, 102, 102, 103, 103, 103, 104, 104, 105, 105, 105,
        106, 106, 107, 107, 107, 108, 108, 108, 109, 109, 110, 110, 110, 111, 111, 112, 112, 112, 113, 113,
        114, 114, 114, 115, 115, 116, 116, 116, 117, 117, 118, 118, 118, 119, 119, 120, 120, 120, 121, 121,
        122, 122, 122, 123, 123, 124, 124, 124, 125, 125, 126, 126, 126, 127, 127, 128, 128, 128, 129, 129,
        130, 130, 130, 131, 131, 132, 132, 132, 133, 133, 134, 134, 134, 135, 135, 136, 136, 136, 137, 137,
        138, 138, 138, 139, 139, 140, 140, 140, 141, 141, 142, 142, 142, 143, 143, 144, 144, 144, 145, 145,
        146, 146, 146, 147, 147, 147, 148, 148, 149, 149, 149, 150, 150, 151, 151, 151, 152, 152, 153, 153,
        153, 154, 154, 155, 155, 155, 156, 156, 157, 157, 157, 158, 158, 159, 159, 159, 160, 160, 161, 161,
        161, 162, 162, 163, 163, 163, 164, 164, 165, 165, 165, 166, 166, 167, 167, 167, 168, 168, 169, 169,
        169, 170, 170, 171, 171, 171, 172, 172, 173, 173, 173, 174, 174, 175, 175, 175, 176, 176, 177, 177,
        177, 178, 178, 179, 179, 179, 180, 180, 181, 181, 181, 182, 182, 183, 183, 183, 184, 184, 185, 185,
        185, 186, 186, 186, 187, 187, 188, 188, 188, 189, 189, 190, 190, 190, 191, 191, 192, 192, 192, 193,
        193, 194, 194, 194, 195, 195, 196, 196, 196, 197, 197, 198, 198, 198, 199, 199, 200, 200, 200, 201,
        201, 202, 202, 202, 203, 203, 204, 204, 204, 205, 205, 206, 206, 206, 207, 207, 208, 208, 208, 209,
        209, 210, 210, 210, 211, 211, 212
    };

    const diy_fp_t v = { .f = positive_base_lut[e], .e = positive_index_lut[e] };
    return v;
}

static inline diy_fp_t negative_diy_fp(int32_t e)
{
    static const uint64_t negative_base_lut[539] = {
        0x0de0b6b3a7640000, 0x22b1c8c1227a0000, 0x08ac7230489e8000, 0x15af1d78b58c4000,
        0x3635c9adc5dea000, 0x0d8d726b7177a800, 0x21e19e0c9bab2400, 0x0878678326eac900,
        0x152d02c7e14af680, 0x34f086f3b33b6840, 0x0d3c21bcecceda10, 0x2116545850052128,
        0x084595161401484a, 0x14adf4b7320334b9, 0x33b2e3c9fd0803cf, 0x0cecb8f27f4200f4,
        0x204fce5e3e250262, 0x0813f3978f894099, 0x1431e0fae6d7217d, 0x327cb2734119d3b8,
        0x0c9f2c9cd04674ee, 0x1f8def8808b02453, 0x07e37be2022c0915, 0x13b8b5b5056e16b4,
        0x314dc6448d9338c2, 0x0c5371912364ce31, 0x1ed09bead87c0379, 0x07b426fab61f00df,
        0x13426172c74d822c, 0x3025f39ef241c56d, 0x0c097ce7bc90715c, 0x1e17b84357691b65,
        0x0785ee10d5da46da, 0x12ced32a16a1b11f, 0x2f050fe938943acd, 0x0bc143fa4e250eb4,
        0x1d6329f1c35ca4c0, 0x0758ca7c70d72930, 0x125dfa371a19e6f8, 0x2deaf189c140c16c,
        0x0b7abc627050305b, 0x1cb2d6f618c878e4, 0x072cb5bd86321e39, 0x11efc659cf7d4b8e,
        0x2cd76fe086b93ce3, 0x0b35dbf821ae4f39, 0x1c06a5ec5433c60e, 0x0701a97b150cf184,
        0x118427b3b4a05bc9, 0x2bca63414390e576, 0x0af298d050e4395e, 0x1b5e7e08ca3a8f6a,
        0x06d79f82328ea3db, 0x111b0ec57e6499a2, 0x2ac3a4edbbfb8015, 0x0ab0e93b6efee006,
        0x1aba4714957d300e, 0x06ae91c5255f4c04, 0x10b46c6cdd6e3e09, 0x29c30f1029939b15,
        0x0a70c3c40a64e6c6, 0x1a19e96a19fc40ed, 0x06867a5a867f103c, 0x105031e2503da894,
        0x28c87cb5c89a2572, 0x0a321f2d7226895d, 0x197d4df19d605768, 0x3fb942dc0970da83,
        0x0fee50b7025c36a1, 0x27d3c9c985e68892, 0x09f4f2726179a225, 0x18e45e1df3b0155b,
        0x3e3aeb4ae1383563, 0x0f8ebad2b84e0d59, 0x26e4d30eccc3215e, 0x09b934c3b330c858,
        0x184f03e93ff9f4db, 0x3cc589c71ff0e423, 0x0f316271c7fc3909, 0x25fb761c73f68e96,
        0x097edd871cfda3a6, 0x17bd29d1c87a191e, 0x3b58e88c75313eca, 0x0ed63a231d4c4fb3,
        0x25179157c93ec73f, 0x0945e455f24fb1d0, 0x172ebad6ddc73c87, 0x39f4d3192a721752,
        0x0e7d34c64a9c85d5, 0x243903efba874e93, 0x090e40fbeea1d3a5, 0x16a3a275d494911c,
        0x3899162693736ac6, 0x0e264589a4dcdab2, 0x235fadd81c2822bc, 0x08d7eb76070a08af,
        0x161bcca7119915b6, 0x37457fa1abfeb645, 0x0dd15fe86affad92, 0x228b6fc50b7f31eb,
        0x08a2dbf142dfcc7b, 0x159725db272f7f33, 0x35f9dea3e1f6bdff, 0x0d7e77a8f87daf80,
        0x21bc2b266d3a36c0, 0x086f0ac99b4e8db0, 0x15159af804446238, 0x34b6036c0aaaf58b,
        0x0d2d80db02aabd63, 0x20f1c22386aad977, 0x083c7088e1aab65e, 0x14971956342ac7eb,
        0x3379bf57826af3ca, 0x0cde6fd5e09abcf3, 0x202c1796b182d85f, 0x080b05e5ac60b618,
        0x141b8ebe2ef1c73b, 0x3244e4db755c7214, 0x0c913936dd571c85, 0x1f6b0f092959c74c,
        0x07dac3c24a5671d3, 0x13a2e965b9d81c90, 0x3117477e509c4767, 0x0c45d1df942711da,
        0x1eae8caef261aca1, 0x07aba32bbc986b29, 0x132d17ed577d0be5, 0x2ff0bbd15ab89dbb,
        0x0bfc2ef456ae276f, 0x1df67562d8b36295, 0x077d9d58b62cd8a6, 0x12ba095dc7701d9d,
        0x2ed1176a72984a08, 0x0bb445da9ca61282, 0x1d42aea2879f2e45, 0x0750aba8a1e7cb92,
        0x1249ad2594c37cec, 0x2db830ddf3e8b84c, 0x0b6e0c377cfa2e13, 0x1c931e8ab8717330,
        0x0724c7a2ae1c5ccc, 0x11dbf316b346e7fe, 0x2ca5dfb8c03143fa, 0x0b2977ee300c50ff,
        0x1be7abd3781eca7d, 0x06f9eaf4de07b2a0, 0x1170cb642b133e8e, 0x2b99fc7a6bb01c62,
        0x0ae67f1e9aec0719, 0x1b403dcc834e11be, 0x06d00f7320d38470, 0x1108269fd210cb17,
        0x2a94608f8d29fbb8, 0x0aa51823e34a7eee, 0x1a9cbc59b83a3d53, 0x06a72f166e0e8f55,
        0x10a1f5b813246654, 0x2994e64c2fdaffd2, 0x0a6539930bf6bff5, 0x19fd0fef9de8dfe3,
        0x067f43fbe77a37f9, 0x103e29f5c2b18bee, 0x289b68e666bbddd3, 0x0a26da3999aef775,
        0x1961219000356aa4, 0x3f72d3e800858a99, 0x0fdcb4fa002162a7, 0x27a7c471005376a0,
        0x09e9f11c4014dda8, 0x18c8dac6a0342a24, 0x3df622f09082695a, 0x0f7d88bc24209a57,
        0x26b9d5d65a5181d8, 0x09ae757596946076, 0x183425a5f872f127, 0x3c825e1eed1f5ae2,
        0x0f209787bb47d6b9, 0x25d17ad3543398cd, 0x09745eb4d50ce634, 0x17a2ecc414a03f80,
        0x3b174fea33909ec0, 0x0ec5d3fa8ce427b0, 0x24ee91f2603a6338, 0x093ba47c980e98ce,
        0x17151b377c247e03, 0x39b4c40ab65b3b08, 0x0e6d3102ad96cec2, 0x2410fa86b1f904e5,
        0x09043ea1ac7e413a, 0x168a9c942f3ba30f, 0x385a8772761517a6, 0x0e16a1dc9d8545ea,
        0x233894a789cd2ec8, 0x08ce2529e2734bb2, 0x16035ce8b6203d3d, 0x37086845c7509918,
        0x0dc21a1171d42646, 0x2265412b9c925faf, 0x0899504ae72497ec, 0x157f48bb41db7bce,
        0x35be35d424a4b581, 0x0d6f8d7509292d61, 0x2196e1a496e6f171, 0x0865b86925b9bc5d,
        0x14fe4d06de5056e7, 0x347bc0912bc8d940, 0x0d1ef0244af23650, 0x20cd585abb5d87c8,
        0x08335616aed761f2, 0x14805738b51a74dd, 0x3340da0dc4c22429, 0x0cd036837130890b,
        0x200888489af9569a, 0x0802221226be55a7, 0x1405552d60dbd620, 0x320d54f172259750,
        0x0c83553c5c8965d4, 0x1f485516e7577e92, 0x07d21545b9d5dfa5, 0x138d352e5096af1b,
        0x30e104f3c978b5c4, 0x0c38413cf25e2d71, 0x1e8ca3185deb719b, 0x07a328c6177adc67,
        0x1317e5ef3ab32701, 0x2fbbbed612bfe181, 0x0beeefb584aff861, 0x1dd55745cbb7ecf1,
        0x077555d172edfb3d, 0x12a5568b9f52f417, 0x2e9d585d0e4f6238, 0x0ba756174393d88e,
        0x1d22573a28f19d63, 0x074895ce8a3c6759, 0x123576845997025e, 0x2d85a84adff985eb,
        0x0b616a12b7fe617b, 0x1c73892ecbfbf3b3, 0x071ce24bb2fefced, 0x11c835bd3f7d7850,
        0x2c7486591eb9acc8, 0x0b1d219647ae6b32, 0x1bc8d3f7b3340bfd, 0x06f234fdeccd0300,
        0x115d847ad000877e, 0x2b69cb33080152bb, 0x0ada72ccc20054af, 0x1b221effe500d3b5,
        0x06c887bff94034ee, 0x10f5535fef208451, 0x2a65506fd5d14acb, 0x0a99541bf57452b3,
        0x1a7f5245e5a2cebf, 0x069fd4917968b3b0, 0x108f936baf85c137, 0x2966f08d36ce630a,
        0x0a59bc234db398c3, 0x19e056584240fde6, 0x0678159610903f7a, 0x102c35f729689eb0,
        0x286e86e9e7858cb8, 0x0a1ba1ba79e1632e, 0x1945145230b377f3, 0x3f2cb2cd79c0abdf,
        0x0fcb2cb35e702af8, 0x277befc06c186b6b, 0x09defbf01b061adb, 0x18ad75d8438f4323,
        0x3db1a69ca8e627d7, 0x0f6c69a72a3989f6, 0x268f0821e98fd8e7, 0x09a3c2087a63f63a,
        0x1819651531f9e790, 0x3c3f7cb4fcf0c2e8, 0x0f0fdf2d3f3c30ba, 0x25a7adf11e1679d1,
        0x0969eb7c47859e75, 0x1788ccb6b2ce0c23, 0x3ad5ffc8bf031e57, 0x0eb57ff22fc0c796,
        0x24c5bfdd7761f2f7, 0x09316ff75dd87cbe, 0x16fb97ea6a9d37da, 0x3974fbca0a890ba1,
        0x0e5d3ef282a242e9, 0x23e91d5e4695a745, 0x08fa475791a569d2, 0x1671b25aec1d888b,
        0x381c3de34e49d55b, 0x0e070f78d3927557, 0x2311a6ae10ee2559, 0x08c469ab843b8957,
        0x15eb082cca94d758, 0x36cb946ffa741a5b, 0x0db2e51bfe9d0697, 0x223f3cc5fc889079,
        0x088fcf317f22241f, 0x156785fbbdd55a4c, 0x3582cef55a9561bd, 0x0d60b3bd56a55870,
        0x2171c159589d5d16, 0x085c705656275746, 0x14e718d7d7625a2e, 0x3441be1b9a75e172,
        0x0d106f86e69d785d, 0x20a916d14089ace8, 0x082a45b450226b3a, 0x1469ae42c8560c11,
        0x330833a6f4d71e2a, 0x0cc20ce9bd35c78b, 0x1fe52048590672da, 0x07f9481216419cb7,
        0x13ef342d37a407c9, 0x31d602710b1a1375, 0x0c75809c42c684de, 0x1f25c186a6f04c29,
        0x07c97061a9bc130b, 0x137798f428562f9a, 0x30aafe6264d77700, 0x0c2abf989935ddc0,
        0x1e6adefd7f06aa60, 0x079ab7bf5fc1aa98, 0x1302cb5e6f642a7c, 0x2f86fc6c167a6a36,
        0x0be1bf1b059e9a8e, 0x1db45dc38e0c8262, 0x076d1770e3832099, 0x1290ba9a38c7d17d,
        0x2e69d2818df38bb9, 0x0b9a74a0637ce2ef, 0x1d022390f8b83754, 0x074088e43e2e0dd5,
        0x1221563a9b732295, 0x2d535792849fd673, 0x0b54d5e4a127f59d, 0x1c5416bb92e3e608,
        0x071505aee4b8f982, 0x11b48e353bce6fc5, 0x2c4363851584176c, 0x0b10d8e1456105db,
        0x1baa1e332d728ea4, 0x06ea878ccb5ca3a9, 0x114a52dffc679926, 0x2b39cf2ff702fedf,
        0x0ace73cbfdc0bfb8, 0x1b04217dfa61df4c, 0x06c1085f7e9877d3, 0x10e294eebc7d2b90,
        0x2a367454d738ece6, 0x0a8d9d1535ce3b3a, 0x1a6208b506839410, 0x0698822d41a0e504,
        0x107d457124123c8a, 0x29392d9ada2d9759, 0x0a4e4b66b68b65d7, 0x19c3bc80c85c7e98,
        0x0670ef2032171fa6, 0x101a55d07d39cf1f, 0x2841d689391085cd, 0x0a1075a24e442174,
        0x19292615c3aa53a0, 0x3ee6df366929d110, 0x0fb9b7cd9a4a7444, 0x27504b8201ba22aa,
        0x09d412e0806e88ab, 0x18922f31411455aa, 0x3d6d75fb22b2d629, 0x0f5b5d7ec8acb58b,
        0x266469bcf5afc5da, 0x09991a6f3d6bf177, 0x17fec216198ddba8, 0x3bfce5373fe2a524,
        0x0eff394dcff8a949, 0x257e0f4287eda737, 0x095f83d0a1fb69ce, 0x176ec98994f48882,
        0x3a94f7d7f4635545, 0x0ea53df5fd18d552, 0x249d1ae6f8be154c, 0x092746b9be2f8553,
        0x16e230d05b76cd4f, 0x39357a08e4a90146, 0x0e4d5e82392a4052, 0x23c16c458ee9a0cc,
        0x08f05b1163ba6833, 0x1658e3ab79520480, 0x37de392caf4d0b3e, 0x0df78e4b2bd342d0,
        0x22eae3bbed902707, 0x08bab8eefb6409c2, 0x15d2ce55747a1865, 0x368f03d5a3313cfb,
        0x0da3c0f568cc4f3f, 0x2219626585fec61d, 0x08865899617fb188, 0x154fdd7f73bf3bd2,
        0x3547a9bea15e158d, 0x0d51ea6fa8578564, 0x214cca1724dacd78, 0x08533285c936b35e,
        0x14cffe4e7708c06b, 0x3407fbc42995e10c, 0x0d01fef10a657843, 0x2084fd5a99fdaca7,
        0x08213f56a67f6b2a, 0x14531e58a03e8be9, 0x32cfcbdd909c5dc5, 0x0cb3f2f764271772,
        0x1fc1df6a7a61ba9b, 0x07f077da9e986ea7, 0x13d92ba28c7d14a1, 0x319eed165f38b393,
        0x0c67bb4597ce2ce5, 0x1f03542dfb83703c, 0x07c0d50b7ee0dc0f, 0x1362149cbd322626,
        0x30753387d8fd5f5d, 0x0c1d4ce1f63f57d8, 0x1e494034e79e5b9a, 0x0792500d39e796e7,
        0x12edc82110c2f941, 0x2f527452a9e76f21, 0x0bd49d14aa79dbc9, 0x1d9388b3aa30a575,
        0x0764e22cea8c295e, 0x127c35704a5e6769, 0x2e368598b9ec0286, 0x0b8da1662e7b00a2,
        0x1ce2137f74338194, 0x073884dfdd0ce065, 0x120d4c2fa8a030fd, 0x2d213e7725907a77,
        0x0b484f9dc9641e9e, 0x1c34c70a777a4c8b, 0x070d31c29dde9323, 0x11a0fc668aac6fd7,
        0x2c1277005aaf1798, 0x0b049dc016abc5e6, 0x1b8b8a6038ad6ebf, 0x06e2e2980e2b5bb0,
        0x1137367c236c6538, 0x2b0a0836588efd0b, 0x0ac2820d9623bf43, 0x1ae64521f7595e27,
        0x06b991487dd6578a, 0x10cfeb353a97dad9, 0x2a07cc05127ba31d, 0x0a81f301449ee8c8,
        0x1a44df832b8d45f2, 0x069137e0cae3517d, 0x106b0bb1fb384bb7, 0x290b9d3cf40cbd4a,
        0x0a42e74f3d032f53, 0x19a742461887f64e, 0x0669d0918621fd94, 0x1008896bcf54f9f1,
        0x2815578d865470da, 0x0a0555e361951c37, 0x190d56b873f4c689, 0x3ea158cd21e3f055,
        0x0fa856334878fc16, 0x2724d780352e7635, 0x09c935e00d4b9d8e, 0x187706b0213d09e1,
        0x3d2990b8531898b3, 0x0f4a642e14c6262d, 0x2639fa7333ef5f70, 0x098e7e9cccfbd7dc,
        0x17e43c8800759ba6, 0x3bba97540126051f, 0x0eeea5d500498148, 0x25549e9480b7c333,
        0x095527a5202df0cd, 0x1754e31cd072da00, 0x3a5437c8091f2100, 0x0e950df20247c840,
        0x2474a2dd05b374a0, 0x091d28b7416cdd28, 0x16c8e5ca239028e4, 0x38f63e7958e8663a,
        0x0e3d8f9e563a198f, 0x2399e70bd7913fe4, 0x08e679c2f5e44ff9, 0x1640306766bac7ef,
        0x37a0790280d2f3d4, 0x0de81e40a034bcf5, 0x22c44ba19083d865, 0x08b112e86420f61a,
        0x15baaf44fa52673f, 0x3652b62c71ce021e, 0x0d94ad8b1c738088, 0x21f3b1dbc720c153,
        0x087cec76f1c83055, 0x15384f295c7478d4, 0x350cc5e767232e11, 0x0d433179d9c8cb85,
        0x2127fbb0a075fccb, 0x0849feec281d7f33, 0x14b8fd4e6449bdff, 0x33ce7943fab85afc,
        0x0cf39e50feae16bf, 0x20610bca7cb338de, 0x081842f29f2cce38, 0x143ca75e8df0038b,
        0x3297a26c62d808db, 0x0ca5e89b18b60237, 0x1f9ec583bdc70589, 0x07e7b160ef71c163,
        0x13c33b72569c6376, 0x3168149dd886f8a5, 0x0c5a05277621be2a, 0x1ee10ce2a7545b68,
        0x07b84338a9d516da, 0x134ca80da894b921, 0x303fa4222573ced2, 0x0c0fe908895cf3b5,
        0x1e27c69557686143, 0x0789f1a555da1851, 0x12d8dc1d56a13cca, 0x2f1e2649589317f9,
        0x0bc789925624c5ff, 0x1d72d7edd75beefc, 0x075cb5fb75d6fbbf, 0x1267c6f4a699755d,
        0x2e037163a07fa569, 0x0b80dc58e81fe95b, 0x1cc226de444fc762, 0x073089b79113f1d9,
        0x11f9584aeab1dc9d, 0x2cef5cbb4abca788, 0x0b3bd72ed2af29e2, 0x1c1599f50eb5e8b5,
        0x0705667d43ad7a2e, 0x118d80392931b172, 0x2be1c08ee6fc3b9b, 0x0af87023b9bf0ee7,
        0x1b6d1859505da541, 0x06db461654176951, 0x11242f37d23a8749
    };

    static const uint8_t negative_index_lut[539] = {
        18 , 18 , 17 , 17 , 17 , 16 , 16 , 15 , 15 , 15 , 14 , 14 , 13 , 13 , 13 , 12 , 12 , 11 , 11 , 11 ,
        10 , 10 , 9  , 9  , 9  , 8  , 8  , 7  , 7  , 7  , 6  , 6  , 5  , 5  , 5  , 4  , 4  , 3  , 3  , 3  ,
        2  , 2  , 1  , 1  , 1  , 0  , 0  , 1  , 1  , 1  , 2  , 2  , 3  , 3  , 3  , 4  , 4  , 5  , 5  , 5  ,
        6  , 6  , 7  , 7  , 7  , 8  , 8  , 8  , 9  , 9  , 10 , 10 , 10 , 11 , 11 , 12 , 12 , 12 , 13 , 13 ,
        14 , 14 , 14 , 15 , 15 , 16 , 16 , 16 , 17 , 17 , 18 , 18 , 18 , 19 , 19 , 20 , 20 , 20 , 21 , 21 ,
        22 , 22 , 22 , 23 , 23 , 24 , 24 , 24 , 25 , 25 , 26 , 26 , 26 , 27 , 27 , 28 , 28 , 28 , 29 , 29 ,
        30 , 30 , 30 , 31 , 31 , 32 , 32 , 32 , 33 , 33 , 34 , 34 , 34 , 35 , 35 , 36 , 36 , 36 , 37 , 37 ,
        38 , 38 , 38 , 39 , 39 , 40 , 40 , 40 , 41 , 41 , 42 , 42 , 42 , 43 , 43 , 44 , 44 , 44 , 45 , 45 ,
        46 , 46 , 46 , 47 , 47 , 47 , 48 , 48 , 49 , 49 , 49 , 50 , 50 , 51 , 51 , 51 , 52 , 52 , 53 , 53 ,
        53 , 54 , 54 , 55 , 55 , 55 , 56 , 56 , 57 , 57 , 57 , 58 , 58 , 59 , 59 , 59 , 60 , 60 , 61 , 61 ,
        61 , 62 , 62 , 63 , 63 , 63 , 64 , 64 , 65 , 65 , 65 , 66 , 66 , 67 , 67 , 67 , 68 , 68 , 69 , 69 ,
        69 , 70 , 70 , 71 , 71 , 71 , 72 , 72 , 73 , 73 , 73 , 74 , 74 , 75 , 75 , 75 , 76 , 76 , 77 , 77 ,
        77 , 78 , 78 , 79 , 79 , 79 , 80 , 80 , 81 , 81 , 81 , 82 , 82 , 83 , 83 , 83 , 84 , 84 , 85 , 85 ,
        85 , 86 , 86 , 86 , 87 , 87 , 88 , 88 , 88 , 89 , 89 , 90 , 90 , 90 , 91 , 91 , 92 , 92 , 92 , 93 ,
        93 , 94 , 94 , 94 , 95 , 95 , 96 , 96 , 96 , 97 , 97 , 98 , 98 , 98 , 99 , 99 , 100, 100, 100, 101,
        101, 102, 102, 102, 103, 103, 104, 104, 104, 105, 105, 106, 106, 106, 107, 107, 108, 108, 108, 109,
        109, 110, 110, 110, 111, 111, 112, 112, 112, 113, 113, 114, 114, 114, 115, 115, 116, 116, 116, 117,
        117, 118, 118, 118, 119, 119, 120, 120, 120, 121, 121, 122, 122, 122, 123, 123, 124, 124, 124, 125,
        125, 125, 126, 126, 127, 127, 127, 128, 128, 129, 129, 129, 130, 130, 131, 131, 131, 132, 132, 133,
        133, 133, 134, 134, 135, 135, 135, 136, 136, 137, 137, 137, 138, 138, 139, 139, 139, 140, 140, 141,
        141, 141, 142, 142, 143, 143, 143, 144, 144, 145, 145, 145, 146, 146, 147, 147, 147, 148, 148, 149,
        149, 149, 150, 150, 151, 151, 151, 152, 152, 153, 153, 153, 154, 154, 155, 155, 155, 156, 156, 157,
        157, 157, 158, 158, 159, 159, 159, 160, 160, 161, 161, 161, 162, 162, 163, 163, 163, 164, 164, 164,
        165, 165, 166, 166, 166, 167, 167, 168, 168, 168, 169, 169, 170, 170, 170, 171, 171, 172, 172, 172,
        173, 173, 174, 174, 174, 175, 175, 176, 176, 176, 177, 177, 178, 178, 178, 179, 179, 180, 180, 180,
        181, 181, 182, 182, 182, 183, 183, 184, 184, 184, 185, 185, 186, 186, 186, 187, 187, 188, 188, 188,
        189, 189, 190, 190, 190, 191, 191, 192, 192, 192, 193, 193, 194, 194, 194, 195, 195, 196, 196
    };

    const diy_fp_t v = { .f = negative_base_lut[e], .e = e < 47 ? negative_index_lut[e] : -negative_index_lut[e] };
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
    uint8_t last_digit = 0;

    int32_t s = v->e & 1;
    int32_t e = v->e >> 1;
    diy_fp_t x = e >= 0 ? positive_diy_fp(e) : negative_diy_fp(-e);
    uint64_t f = v->f << s;
    uint64_t delta = x.f << s;

#if USING_U128_CALC
    static const u128 cmp = (u128)10000000000000000llu * 1000000000000000000llu; // 1e34
    u128 m = (u128)f * x.f;
    m += delta >> 1;
    if (m < cmp) {
        v->e = e - x.e + 18;
        v->f = u128_div_1e18(m, &remainder);
        div   = 1000000000000000000llu;
        div10 = 100000000000000000llu;
    } else {
        v->e = e - x.e + 19;
        v->f = u128_div_1e19(m, &remainder);
        div   = 10000000000000000000llu;
        div10 = 1000000000000000000llu;
    }
    if (remainder > delta + f) {
        last_digit = remainder / div10;
        --v->e;
    } else {
        if ((v->f & 1) && ((u128)remainder + div <= delta + f)) {
            --v->f;
        }
    }
#else
    u64x2_t cmp = {.hi = 542101086242752llu, .lo = 4003012203950112768llu};
    u64x2_t m = u128_mul(f, x.f);
    m = u128_add(m, delta >> 1);
    if (u128_cmp(m, cmp) < 0) {
        v->e = e - x.e + 18;
        v->f = u128_div_1e18(m, &remainder);
        div   = 1000000000000000000llu;
        div10 = 100000000000000000llu;
    } else {
        v->e = e - x.e + 19;
        v->f = u128_div_1e19(m, &remainder);
        div   = 10000000000000000000llu;
        div10 = 1000000000000000000llu;
    }

    if (remainder > delta + f) {
        last_digit = remainder / div10;
        --v->e;
    } else {
        if (v->f & 1) {
            const uint64_t tmp = 0xFFFFFFFFFFFFFFFF - div;
            if (remainder > tmp || remainder + div < delta + f) {
                --v->f;
            }
        }
    }
#endif

    int32_t num_digits, trailing_zeros;
    if (v->f >= 10000000000000000llu) {
        memcpy(buffer, "10000000000000000", 17);
        trailing_zeros = 16;
        num_digits = 17;
    } else {
        num_digits = fill_significand(buffer, v->f, &trailing_zeros);
        if (last_digit) {
            buffer[num_digits++] = last_digit + '0';
            trailing_zeros = 0;
        }
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
