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

#define DIY_SIGNIFICAND_SIZE        64                  /* Symbol: 1 bit, Exponent, 11 bits, Mantissa, 52 bits */
#define DP_SIGNIFICAND_SIZE         52                  /* Mantissa, 52 bits */
#define DP_EXPONENT_OFFSET          0x3FF               /* Exponent offset is 0x3FF */
#define DP_EXPONENT_MAX             0x7FF               /* Max Exponent value */
#define DP_EXPONENT_MASK            0x7FF0000000000000  /* Exponent Mask, 11 bits */
#define DP_SIGNIFICAND_MASK         0x000FFFFFFFFFFFFF  /* Mantissa Mask, 52 bits */
#define DP_HIDDEN_BIT               0x0010000000000000  /* Integer bit for Mantissa */
#define LSHIFT_RESERVED_BIT         11                  /* Significand: 53 bits */

#ifndef USING_FLOAT_MUL
#define USING_FLOAT_MUL             0                   /* Using floating-point multiplication */
#endif

/* When set to 1, use __int128 multiplication instead of floating-point multiplication. */
#if (USING_FLOAT_MUL == 0) && (__WORDSIZE == 64) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6) || __clang_major__ >= 9)
#define USING_U128_MUL              1
#else
#define USING_U128_MUL              0
#endif

/* When set to 1, use one more precision when using large number multiplying, and it will slows down the speed. */
#ifndef USING_HIGH_RESOLUTION
#define USING_HIGH_RESOLUTION       1
#endif

typedef struct {
    uint64_t f;
    int32_t e;
} diy_fp_t;

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
    cmp_value = 1000000000000000000 # ULONG_MAX = 18446744073709551615
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
        print('%18d' % (base_lut[i]), end='')
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
    # 243 = (1023 - 52 - (11 - x)) / 4 + 1
    print_pow_array(16, 1, 243, True)

def print_negative_array():
    # 1 * (2 ** -4) = 0.625 * (10 ** -1)
    # 625 = 0.625 * (10 ** 3)
    # 285 = (1022 + 52 + (63 - (11 - x))) / 4 + 1
    print_pow_array(625, 3, 285, False)

print_positive_array()
print()
print_negative_array()
*/

static inline diy_fp_t positive_diy_fp(int32_t e)
{
    static const uint64_t positive_base_lut[244] = {
        100000000000000000, 160000000000000000, 256000000000000000, 409600000000000000,
        655360000000000000, 104857600000000000, 167772160000000000, 268435456000000000,
        429496729600000000, 687194767360000000, 109951162777600000, 175921860444160000,
        281474976710656000, 450359962737049600, 720575940379279360, 115292150460684698,
        184467440737095517, 295147905179352826, 472236648286964522, 755578637259143235,
        120892581961462918, 193428131138340668, 309485009821345069, 495176015714152110,
        792281625142643376, 126765060022822941, 202824096036516705, 324518553658426727,
        519229685853482763, 830767497365572421, 132922799578491588, 212676479325586540,
        340282366920938464, 544451787073501542, 871122859317602467, 139379657490816395,
        223007451985306232, 356811923176489971, 570899077082383953, 913438523331814324,
        146150163733090292, 233840261972944467, 374144419156711148, 598631070650737836,
        957809713041180537, 153249554086588886, 245199286538542218, 392318858461667548,
        627710173538668077, 100433627766186893, 160693804425899028, 257110087081438445,
        411376139330301511, 658201822928482417, 105312291668557187, 168499666669691499,
        269599466671506398, 431359146674410237, 690174634679056379, 110427941548649021,
        176684706477838433, 282695530364541493, 452312848583266389, 723700557733226222,
        115792089237316196, 185267342779705913, 296427748447529461, 474284397516047137,
        758855036025675419, 121416805764108067, 194266889222572908, 310827022756116652,
        497323236409786643, 795717178255658628, 127314748520905381, 203703597633448609,
        325925756213517774, 521481209941628439, 834369935906605501, 133499189745056881,
        213598703592091009, 341757925747345614, 546812681195752982, 874900289913204770,
        139984046386112764, 223974474217780422, 358359158748448674, 573374653997517878,
        917399446396028605, 146783911423364577, 234854258277383323, 375766813243813317,
        601226901190101307, 961963041904162091, 153914086704665935, 246262538727465496,
        394020061963944793, 630432099142311668, 100869135862769867, 161390617380431787,
        258224987808690859, 413159980493905375, 661055968790248599, 105768955006439776,
        169230328010303642, 270768524816485827, 433229639706377322, 693167423530203715,
        110906787764832595, 177450860423732152, 283921376677971442, 454274202684754307,
        726838724295606891, 116294195887297103, 186070713419675364, 297713141471480583,
        476341026354368932, 762145642166990291, 121943302746718447, 195109284394749515,
        312174855031599224, 499479768050558758, 799167628880894012, 127866820620943042,
        204586912993508867, 327339060789614188, 523742497263382700, 837987995621412319,
        134078079299425971, 214524926879081554, 343239883006530486, 549183812810448778,
        878694100496718044, 140591056079474887, 224945689727159820, 359913103563455711,
        575860965701529137, 921377545122446620, 147420407219591460, 235872651551346335,
        377396242482154136, 603833987971446617, 966134380754314587, 154581500920690334,
        247330401473104535, 395728642356967255, 633165827771147608, 101306532443383618,
        162090451909413788, 259344723055062060, 414951556888099296, 663922491020958874,
        106227598563353420, 169964157701365472, 271942652322184755, 435108243715495608,
        696173189944792972, 111387710391166876, 178220336625867001, 285152538601387202,
        456244061762219522, 729990498819551235, 116798479811128198, 186877567697805117,
        299004108316488186, 478406573306381098, 765450517290209756, 122472082766433561,
        195955332426293698, 313528531882069916, 501645651011311866, 802633041618098985,
        128421286658895838, 205474058654233341, 328758493846773345, 526013590154837351,
        841621744247739762, 134659479079638362, 215455166527421379, 344728266443874207,
        551565226310198730, 882504362096317968, 141200697935410875, 225921116696657400,
        361473786714651840, 578358058743442944, 925372893989508710, 148059663038321394,
        236895460861314230, 379032737378102768, 606452379804964428, 970323807687943085,
        155251809230070894, 248402894768113430, 397444631628981488, 635911410606370380,
        101745825697019261, 162793321115230818, 260469313784369308, 416750902054990893,
        666801443287985428, 106688230926077669, 170701169481724270, 273121871170758832,
        436994993873214130, 699191990197142608, 111870718431542818, 178993149490468508,
        286389039184749613, 458222462695599380, 733155940312959007, 117304950450073442,
        187687920720117506, 300300673152188010, 480481077043500815, 768769723269601304,
        123003155723136209, 196805049157017934, 314888078651228694, 503820925841965911,
        806113481347145457, 128978157015543274, 206365051224869237, 330184081959790779,
        528294531135665247, 845271249817064395, 135243399970730304, 216389439953168485,
        346223103925069576, 553956966280111322, 886331146048178115, 141812983367708499,
        226900773388333598, 363041237421333756, 580865979874134009, 929385567798614415,
        148701690847778307, 237922705356445291, 380676328570312465, 609082125712499943,
        974531401139999909, 155925024182399986, 249480038691839977, 399168061906943963
    };

    static const int8_t positive_index_lut[244] = {
        17 , 17 , 17 , 17 , 17 , 16 , 16 , 16 , 16 , 16 , 15 , 15 , 15 , 15 , 15 , 14 , 14 , 14 , 14 , 14 ,
        13 , 13 , 13 , 13 , 13 , 12 , 12 , 12 , 12 , 12 , 11 , 11 , 11 , 11 , 11 , 10 , 10 , 10 , 10 , 10 ,
        9  , 9  , 9  , 9  , 9  , 8  , 8  , 8  , 8  , 7  , 7  , 7  , 7  , 7  , 6  , 6  , 6  , 6  , 6  , 5  ,
        5  , 5  , 5  , 5  , 4  , 4  , 4  , 4  , 4  , 3  , 3  , 3  , 3  , 3  , 2  , 2  , 2  , 2  , 2  , 1  ,
        1  , 1  , 1  , 1  , 0  , 0  , 0  , 0  , 0  , -1 , -1 , -1 , -1 , -1 , -2 , -2 , -2 , -2 , -3 , -3 ,
        -3 , -3 , -3 , -4 , -4 , -4 , -4 , -4 , -5 , -5 , -5 , -5 , -5 , -6 , -6 , -6 , -6 , -6 , -7 , -7 ,
        -7 , -7 , -7 , -8 , -8 , -8 , -8 , -8 , -9 , -9 , -9 , -9 , -9 , -10, -10, -10, -10, -10, -11, -11,
        -11, -11, -11, -12, -12, -12, -12, -13, -13, -13, -13, -13, -14, -14, -14, -14, -14, -15, -15, -15,
        -15, -15, -16, -16, -16, -16, -16, -17, -17, -17, -17, -17, -18, -18, -18, -18, -18, -19, -19, -19,
        -19, -19, -20, -20, -20, -20, -20, -21, -21, -21, -21, -21, -22, -22, -22, -22, -23, -23, -23, -23,
        -23, -24, -24, -24, -24, -24, -25, -25, -25, -25, -25, -26, -26, -26, -26, -26, -27, -27, -27, -27,
        -27, -28, -28, -28, -28, -28, -29, -29, -29, -29, -29, -30, -30, -30, -30, -30, -31, -31, -31, -31,
        -31, -32, -32, -32
    };

    const diy_fp_t v = { .f = positive_base_lut[e], .e = positive_index_lut[e] };
    return v;
}

static inline diy_fp_t negative_diy_fp(int32_t e)
{
    static const uint64_t negative_base_lut[286] = {
        100000000000000000, 625000000000000000, 390625000000000000, 244140625000000000,
        152587890625000000, 953674316406250000, 596046447753906250, 372529029846191407,
        232830643653869629, 145519152283668519, 909494701772928238, 568434188608080149,
        355271367880050093, 222044604925031309, 138777878078144568, 867361737988403548,
        542101086242752218, 338813178901720136, 211758236813575085, 132348898008484428,
        827180612553027675, 516987882845642297, 323117426778526436, 201948391736579023,
        126217744835361889, 788860905221011806, 493038065763132379, 308148791101957737,
        192592994438723586, 120370621524202241, 752316384526264006, 470197740328915004,
        293873587705571877, 183670992315982424, 114794370197489015, 717464813734306341,
        448415508583941463, 280259692864963415, 175162308040602134, 109476442525376334,
        684227765783602086, 427642353614751304, 267276471009219565, 167047794380762228,
        104404871487976393, 652530446799852453, 407831529249907783, 254894705781192365,
        159309191113245228, 995682444457782674, 622301527786114171, 388938454866321357,
        243086534291450848, 151929083932156780, 949556774575979875, 593472984109987422,
        370920615068742139, 231825384417963837, 144890865261227398, 905567907882671237,
        565979942426669523, 353737464016668452, 221085915010417783, 138178696881511115,
        863616855509444463, 539760534693402790, 337350334183376744, 210843958864610465,
        131777474290381541, 823609214314884627, 514755758946802892, 321722349341751808,
        201076468338594880, 125672792711621800, 785454954447636249, 490909346529772656,
        306818341581107910, 191761463488192444, 119850914680120278, 749068216750751733,
        468167635469219833, 292604772168262396, 182877982605163998, 114298739128227499,
        714367119551421864, 446479449719638665, 279049656074774166, 174406035046733854,
        109003771904208659, 681273574401304116, 425795984000815072, 266122490000509420,
        166326556250318388, 103954097656448993, 649713110352806202, 406070693970503876,
        253794183731564923, 158621364832228077, 991383530201425478, 619614706375890924,
        387259191484931828, 242036994678082393, 151273121673801496, 945457010461259344,
        590910631538287090, 369319144711429432, 230824465444643395, 144265290902902122,
        901658068143138260, 563536292589461413, 352210182868413383, 220131364292758365,
        137582102682973978, 859888141768587361, 537430088605367101, 335893805378354438,
        209933628361471524, 131208517725919703, 820053235786998139, 512533272366873837,
        320333295229296148, 200208309518310093, 125130193448943808, 782063709055898799,
        488789818159936750, 305493636349960469, 190933522718725293, 119333451699203308,
        745834073120020675, 466146295700012922, 291341434812508076, 182088396757817548,
        113805247973635968, 711282799835224795, 444551749897015497, 277844843685634686,
        173653027303521679, 108533142064701049, 678332137904381557, 423957586190238473,
        264973491368899046, 165608432105561904, 103505270065976190, 646907937912351186,
        404317461195219491, 252698413247012182, 157936508279382614, 987103176746141335,
        616939485466338335, 385587178416461459, 240991986510288412, 150619991568930258,
        941374947305814109, 588359342066133818, 367724588791333637, 229827867994583523,
        143642417496614702, 897765109353841886, 561103193346151179, 350689495841344487,
        219180934900840304, 136988084313025190, 856175526956407438, 535109704347754649,
        334443565217346656, 209027228260841660, 130642017663026038, 816512610393912733,
        510320381496195458, 318950238435122162, 199343899021951351, 124589936888719595,
        778687105554497464, 486679440971560915, 304174650607225572, 190109156629515983,
        118818222893447489, 742613893084046807, 464133683177529254, 290083551985955784,
        181302219991222365, 113313887494513978, 708211796840712363, 442632373025445227,
        276645233140903267, 172903270713064542, 108064544195665339, 675403401222908366,
        422127125764317729, 263829453602698581, 164893408501686613, 103058380313554133,
        644114876959713331, 402571798099820832, 251607373812388020, 157254608632742513,
        982841303954640703, 614275814971650440, 383922384357281525, 239951490223300953,
        149969681389563096, 937310508684769347, 585819067927980842, 366136917454988027,
        228835573409367517, 143022233380854698, 893888958630341861, 558680599143963663,
        349175374464977290, 218234609040610806, 136396630650381754, 852478941564885961,
        532799338478053726, 332999586548783579, 208124741592989737, 130077963495618586,
        812987271847616158, 508117044904760099, 317573153065475062, 198483220665921914,
        124052012916201196, 775325080726257475, 484578175453910922, 302861359658694327,
        189288349786683954, 118305218616677472, 739407616354234195, 462129760221396372,
        288831100138372733, 180519437586482958, 112824648491551849, 705154053072199054,
        440721283170124409, 275450801981327756, 172156751238329847, 107597969523956155,
        672487309524725965, 420304568452953728, 262690355283096080, 164181472051935050,
        102613420032459407, 641333875202871289, 400833672001794556, 250521045001121598,
        156575653125700999, 978597832035631240, 611623645022269525, 382264778138918453,
        238915486336824034, 149322178960515021, 933263618503218879, 583289761564511800,
        364556100977819875, 227847563111137422, 142404726944460889, 890029543402880554,
        556268464626800346, 347667790391750217, 217292368994843886, 135807730621777429,
        848798316386108927, 530498947741318079, 331561842338323800, 207226151461452375,
        129516344663407735, 809477154146298338, 505923221341436462, 316202013338397789,
        197626258336498618, 123516411460311637, 771977571626947726, 482485982266842329,
        301553738916776456, 188471086822985285, 117794429264365803, 736215182902286268,
        460134489313928918, 287584055821205574, 179740034888253484, 112337521805158428,
        702109511282240170, 438818444551400106, 274261527844625067, 171413454902890667,
        107133409314306667, 669583808214416666
    };

    static const int8_t negative_index_lut[286] = {
        17 , 18 , 18 , 18 , 18 , 19 , 19 , 19 , 19 , 19 , 20 , 20 , 20 , 20 , 20 , 21 , 21 , 21 , 21 , 21 ,
        22 , 22 , 22 , 22 , 22 , 23 , 23 , 23 , 23 , 23 , 24 , 24 , 24 , 24 , 24 , 25 , 25 , 25 , 25 , 25 ,
        26 , 26 , 26 , 26 , 26 , 27 , 27 , 27 , 27 , 28 , 28 , 28 , 28 , 28 , 29 , 29 , 29 , 29 , 29 , 30 ,
        30 , 30 , 30 , 30 , 31 , 31 , 31 , 31 , 31 , 32 , 32 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 33 , 34 ,
        34 , 34 , 34 , 34 , 35 , 35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 36 , 37 , 37 , 37 , 37 , 38 , 38 ,
        38 , 38 , 38 , 39 , 39 , 39 , 39 , 39 , 40 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 41 , 42 , 42 ,
        42 , 42 , 42 , 43 , 43 , 43 , 43 , 43 , 44 , 44 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 ,
        46 , 46 , 46 , 47 , 47 , 47 , 47 , 48 , 48 , 48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 ,
        50 , 50 , 51 , 51 , 51 , 51 , 51 , 52 , 52 , 52 , 52 , 52 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 ,
        54 , 54 , 55 , 55 , 55 , 55 , 55 , 56 , 56 , 56 , 56 , 56 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 ,
        58 , 59 , 59 , 59 , 59 , 59 , 60 , 60 , 60 , 60 , 60 , 61 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 ,
        62 , 63 , 63 , 63 , 63 , 63 , 64 , 64 , 64 , 64 , 64 , 65 , 65 , 65 , 65 , 65 , 66 , 66 , 66 , 66 ,
        66 , 67 , 67 , 67 , 67 , 68 , 68 , 68 , 68 , 68 , 69 , 69 , 69 , 69 , 69 , 70 , 70 , 70 , 70 , 70 ,
        71 , 71 , 71 , 71 , 71 , 72 , 72 , 72 , 72 , 72 , 73 , 73 , 73 , 73 , 73 , 74 , 74 , 74 , 74 , 74 ,
        75 , 75 , 75 , 75 , 75 , 76
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
        f <<= t;
    }
    x = e >= 0 ? positive_diy_fp(e) : negative_diy_fp(-e);

#if USING_U128_MUL
    __extension__ typedef unsigned __int128 u128;
    const u128 m = (u128)f * x.f;
    uint64_t ho = (uint64_t)(m >> 64);

#if !USING_HIGH_RESOLUTION
    static const u128 magich = (u128)10 * 5000000000000000000llu;
    static const u128 magicn = (u128)10 * 10000000000000000000llu;
    if (ho < 54210108624275) { // (1e33 - 5e17) >> 64
        v->f = (uint64_t)((m + 500000000000000000llu) / 1000000000000000000llu);
        v->e = e - x.e + 18;
    } else if (ho < 542101086242751) { // (1e34 - 5e18) >> 64
        v->f = (uint64_t)((m + 5000000000000000000llu) / 10000000000000000000llu);
        v->e = e - x.e + 19;
    } else {
        v->f = (uint64_t)((m + magich) / magicn);
        v->e = e - x.e + 20;
    }
#else
    if (ho < 54210108624275) { // (1e33 - 5e16) >> 64
        v->f = (uint64_t)((m + 50000000000000000llu) / 100000000000000000llu);
        v->e = e - x.e + 17;
    } else if (ho < 542101086242752) { // (1e34 - 5e17) >> 64
        v->f = (uint64_t)((m + 500000000000000000llu) / 1000000000000000000llu);
        v->e = e - x.e + 18;
    } else {
        v->f = (uint64_t)((m + 5000000000000000000llu) / 10000000000000000000llu);
        v->e = e - x.e + 19;
    }
#endif

#else

#if !USING_HIGH_RESOLUTION
    double d = (double)f * x.f;
    if (d + 5e17 < 1e33) {
        v->f = (uint64_t)((d + 5e17) * 1e-18);
        v->e = e - x.e + 18;
    } else if (d + 5e18 < 1e34) {
        v->f = (uint64_t)((d + 5e18) * 1e-19);
        v->e = e - x.e + 19;
    } else {
        v->f = (uint64_t)((d + 5e19) * 1e-20);
        v->e = e - x.e + 20;
    }
#else
    double d = (double)f * x.f;
    if (d + 5e16 < 1e33) {
        v->f = (uint64_t)((d + 5e16) * 1e-17);
        v->e = e - x.e + 17;
    } else if (d + 5e17 < 1e34) {
        v->f = (uint64_t)((d + 5e17) * 1e-18);
        v->e = e - x.e + 18;
    } else {
        v->f = (uint64_t)((d + 5e18) * 1e-19);
        v->e = e - x.e + 19;
    }
#endif
#endif
}

#if !USING_HIGH_RESOLUTION
static inline int32_t fill_significand(char *buffer, uint64_t digits, int32_t *ptz, int32_t *fixed)
{
    char *s = buffer;
    uint32_t q, r, q1, r1, q2, r2;

    *fixed = 0;
    q = (uint32_t)(digits / 100000000);
    r = (uint32_t)(digits - (uint64_t)q * 100000000);
    q1 = FAST_DIV10000(q);
    r1 = q - q1 * 10000;

    if (q1 >= 100) {
        q2 = FAST_DIV100(q1);
        r2 = q1 - q2 * 100;
        if (q2 >= 10) {
            *ptz = tz_100_lut[q2];
            memcpy(s, &ch_100_lut[q2<<1], 2);
            s += 2;
        } else {
            *ptz = 0;
            *s++ = q2 + '0';
        }
        if (!r2) {
            *ptz += 2;
            memset(s, '0', 2);
            s += 2;
        } else {
            *ptz = tz_100_lut[r2];
            memcpy(s, &ch_100_lut[r2<<1], 2);
            s += 2;
        }
    } else {
        r2 = q1;
        *ptz = tz_100_lut[r2];
        memcpy(s, &ch_100_lut[r2<<1], 2);
        s += 2;
    }

    if (!r1) {
        *ptz += 4;
        memset(s, '0', 4);
        s += 4;
    } else {
        s += fill_t_4_digits(s, r1, ptz);
    }

    if (!r) {
        memset(s + 8, '0', 8);
        *ptz += 8;
        s += 8;
    } else {
        s += fill_t_8_digits(s, r, ptz);
    }

    return s - buffer;
}

#else

#if USING_U128_MUL
#define APPROX_TAIL_CMP_VAL         1                   /* The value should be less than or equal to 4 */
#else
#define APPROX_TAIL_CMP_VAL         4
#endif
static const uint32_t s_tail_cmp = (APPROX_TAIL_CMP_VAL << 1) + 1;

static inline int32_t fill_a_4_digits(char *buffer, uint32_t digits, int32_t *ptz)
{
    char *s = buffer;
    uint32_t q = FAST_DIV100(digits);
    uint32_t r = digits - q * 100;

    memcpy(s, &ch_100_lut[q<<1], 2);
    memcpy(s + 2, &ch_100_lut[r<<1], 2);

    if (r < s_tail_cmp) {
        s[3] = '0';
        *ptz = tz_100_lut[q] + 2;
    } else {
        if (s[3] < (char)s_tail_cmp + '0') {
            s[3] = '0';
            *ptz = 1;
        } else {
            s[3] -= APPROX_TAIL_CMP_VAL;
            *ptz = 0;
        }
    }

    return 4;
}

static inline int32_t fill_a_8_digits(char *buffer, uint32_t digits, int32_t *ptz)
{
    char *s = buffer;

    if (digits < 10000) {
        memset(s, '0', 4);
        fill_a_4_digits(s + 4, digits, ptz);
    } else {
        uint32_t q = FAST_DIV10000(digits);
        uint32_t r = digits - q * 10000;

        fill_t_4_digits(s, q, ptz);
        if (r < s_tail_cmp) {
            memset(s + 4, '0', 4);
            *ptz += 4;
        } else {
            fill_a_4_digits(s + 4, r, ptz);
        }
    }

    return 8;
}

static inline int32_t fill_d_4_digits(char *buffer, uint32_t digits, int32_t *ptz)
{
    char *s = buffer;
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

    return s - buffer;
}

static inline int32_t fill_d_8_digits(char *buffer, uint32_t digits, int32_t *ptz)
{
    char *s = buffer;

    uint32_t q = FAST_DIV10000(digits);
    uint32_t r = digits - q * 10000;

    s += fill_d_4_digits(s, q, ptz);
    if (!r) {
        *ptz += 4;
        memset(s, '0', 4);
        s += 4;
    } else {
        s += fill_t_4_digits(s, r, ptz);
    }

    return s - buffer;
}

static inline int32_t fill_significand(char *buffer, uint64_t digits, int32_t *ptz, int32_t *fixed)
{
    char *s = buffer;

    digits += APPROX_TAIL_CMP_VAL;
    if (digits < 10000000000000000llu) {
        *fixed = 0;

        uint32_t q = (uint32_t)(digits / 100000000);
        uint32_t r = (uint32_t)(digits - (uint64_t)q * 100000000);

        s += fill_d_8_digits(s, q, ptz);
        if (r < s_tail_cmp) {
            *ptz += 8;
            memset(s, '0', 8);
            s += 8;
        } else {
            s += fill_a_8_digits(s, r, ptz);
        }

        if (*(s-1) - '0' >= (int)s_tail_cmp)
            *(s-1) -= APPROX_TAIL_CMP_VAL;
    } else {
        digits /= 10;
        *fixed = 1;

        uint32_t q = (uint32_t)(digits / 100000000);
        uint32_t r = (uint32_t)(digits - (uint64_t)q * 100000000);

        *ptz = 0;
        fill_t_8_digits(s, q, ptz);
        if (!r) {
            memset(s + 8, '0', 8);
            *ptz += 8;
        } else {
            fill_t_8_digits(s + 8, r, ptz);
        }
        s += 16;
    }

    return s - buffer;
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

static inline char* ldouble_format(char *buffer, uint64_t digits, int32_t decimal_exponent)
{
    int32_t num_digits, trailing_zeros, vnum_digits, decimal_point, fixed_exponent;

    num_digits = fill_significand(buffer + 1, digits, &trailing_zeros, &fixed_exponent);
    vnum_digits = num_digits - trailing_zeros;
    decimal_exponent += fixed_exponent;
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
    int32_t lshiftbit = 11 - LSHIFT_RESERVED_BIT;
    union {double d; uint64_t n;} u = {.d = num};
    int32_t signbit = u.n >> (DIY_SIGNIFICAND_SIZE - 1);
    int32_t exponent = (u.n & DP_EXPONENT_MASK) >> DP_SIGNIFICAND_SIZE; /* Exponent */
    uint64_t significand = u.n & DP_SIGNIFICAND_MASK; /* Mantissa */

    if (signbit) {
        *s++ = '-';
    }

    switch (exponent) {
    case DP_EXPONENT_MAX:
        if (significand) {
            memcpy(buffer, "NaN", 4);
            return 3;
        } else {
            memcpy(s, "Infinity", 9);
            return signbit + 8;
        }
        break;

    case 0:
        if (significand) {
            /* no-normalized double */
            v.f = significand; /* Non-normalized double doesn't have a extra integer bit for Mantissa */
            v.e = 1 - DP_EXPONENT_OFFSET - DP_SIGNIFICAND_SIZE; /* Fixed Exponent: -1022, divisor of Mantissa: pow(2,52) */

            lshiftbit = u64_pz_get(v.f) - LSHIFT_RESERVED_BIT;
            v.f <<= lshiftbit;
            v.e -= lshiftbit; /* The smallest e is (-1022 - 52 - (63 - LSHIFT_RESERVED_BIT)) */
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
            v.f <<= lshiftbit;
            v.e -= lshiftbit; /* The largest e is (1023 - 52 - (11 - LSHIFT_RESERVED_BIT)) */
        }
        break;
    }

    ldouble_convert(&v);
    s = ldouble_format(s, v.f, v.e);

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

static int jnum_parse_num(const char *str, jnum_type_t *type, jnum_value_t *value)
{
#define IS_DIGIT(c)      ((c) >= '0' && (c) <= '9')
    static const double mul10_lut[20] = {
        1   , 1e1 , 1e2 , 1e3 , 1e4 , 1e5 , 1e6 , 1e7 , 1e8 , 1e9 ,
        1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19,
    };
    static const double div10_lut[20] = {
        1    , 1e-1 , 1e-2 , 1e-3 , 1e-4 , 1e-5 , 1e-6 , 1e-7 , 1e-8 , 1e-9 ,
        1e-10, 1e-11, 1e-12, 1e-13, 1e-14, 1e-15, 1e-16, 1e-17, 1e-18, 1e-19,
    };

    const char *s = str;
    int32_t sign = 1, k = 0;
    uint64_t m = 0;
    double d = 0;

    switch (*s) {
    case '-': ++s; sign = -1; break;
    case '+': ++s; break;
    default: break;
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
            d = m;
            goto next2;
        default:
            goto next1;
        }
    } else {
        s -= k;
        while (1) {
            m = 0;
            k = 0;
            while (IS_DIGIT(*s)) {
                m = (m << 3) + (m << 1) + (*s++ - '0');
                ++k;
                if (k == 19)
                    break;
            }
            d = d * mul10_lut[k] + m;

            switch (*s) {
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                break;
            case '.':
                goto next2;
            default:
                goto next3;
            }
        }
    }

next1:
    if (m <= 2147483647U /*INT_MAX*/) {
        *type = JNUM_INT;
        value->vint = sign == 1 ? (int32_t)m : -((int32_t)m);
    } else if (m <= 9223372036854775807U /*LLONG_MAX*/) {
        *type = JNUM_LINT;
        value->vlint = sign == 1 ? (int64_t)m : -((int64_t)m);
    } else {
        if (m == 9223372036854775808U && sign == -1) {
            *type = JNUM_LINT;
            value->vlint = -m;
        } else {
            *type = JNUM_DOUBLE;
            value->vdbl = sign == 1 ? (double)m : -((double)m);
        }
    }
    return s - str;

next2:
    ++s;
    m = 0;
    k = 0;
    while (IS_DIGIT(*s)) {
        m = (m << 3) + (m << 1) + (*s++ - '0');
        ++k;
    }
    if (k < 20) {
        d += m * div10_lut[k];
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
        d += m * div10_lut[k];
        while (IS_DIGIT(*s))
            ++s;
    }

next3:
    *type = JNUM_DOUBLE;
    value->vdbl = sign == 1 ? d : -d;
    return s - str;
}

int jnum_parse(const char *str, jnum_type_t *type, jnum_value_t *value)
{
    const char *s = str;
    int len = 0, len2 = 0;
    jnum_type_t t;
    jnum_value_t v;

    while (1) {
        switch (*s) {
        case '\b': case '\f': case '\n': case '\r': case '\t': case '\v': case ' ':
            ++s;
            break;
        default:
            goto next;
        }
    }

next:
    len = s - str;
    if (*s == '0' && (*(s + 1) == 'x' || *(s + 1) == 'X')) {
        len2 = jnum_parse_hex(s, type, value);
        if (len2 == 2) {
            *type = JNUM_INT;
            value->vint = 0;
            return len + 1;
        }
        return len + len2;
    }

    len += jnum_parse_num(s, type, value);
    if (*type == JNUM_NULL)
        return 0;

    switch (*(str + len)) {
    case 'e': case 'E':
        len2 = jnum_parse_num(str + len + 1, &t, &v);
        if (t == JNUM_NULL)
            return len;

        switch (*type) {
        case JNUM_INT: value->vdbl = value->vint; break;
        case JNUM_LINT: value->vdbl = value->vlint; break;
        default: break;
        }

        *type = JNUM_DOUBLE;
        len += len2 + 1;

        switch (t) {
        case JNUM_INT: value->vdbl *= pow(10, v.vint); break;
        case JNUM_LINT: value->vdbl *= pow(10, v.vlint); break;
        case JNUM_DOUBLE: value->vdbl *= pow(10, v.vdbl); break;
        default: break;
        }
        break;

    default:
        break;
    }

    return len;
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
