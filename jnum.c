/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2019-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* URL: https://github.com/lengjingzju/json *
*******************************************/
#include <string.h>
#include <stdlib.h>
#include "jnum.h"

#define DIY_SIGNIFICAND_SIZE        64                  /* Symbol: 1 bit, Exponent, 11 bits, Mantissa, 52 bits */
#define DP_SIGNIFICAND_SIZE         52                  /* Mantissa, 52 bits */
#define DP_EXPONENT_OFFSET          0x3FF               /* Exponent offset is 0x3FF */
#define DP_EXPONENT_MAX             0x7FF               /* Max Exponent value */
#define DP_EXPONENT_MASK            0x7FF0000000000000  /* Exponent Mask, 11 bits */
#define DP_SIGNIFICAND_MASK         0x000FFFFFFFFFFFFF  /* Mantissa Mask, 52 bits */
#define DP_HIDDEN_BIT               0x0010000000000000  /* Integer bit for Mantissa */

#ifndef LSHIFT_RESERVED_BIT
#define LSHIFT_RESERVED_BIT         11                  /* The value should be: 2 << x <= 11 */
#endif

#ifndef APPROX_TAIL_CMP_VAL
#define APPROX_TAIL_CMP_VAL         2                   /* The value should be less than or equal to 4 */
#endif
static const uint32_t s_tail_cmp = (APPROX_TAIL_CMP_VAL << 1) + 1;

typedef struct {
    uint64_t f;
    int32_t e;
} diy_fp_t;

typedef struct {
    uint32_t hi;
    uint32_t lo;
} u32x2_t;

typedef struct {
    uint32_t hi;
    uint32_t lo;
    int32_t e;
} pow9x2_t;

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

    s += fill_1_20_digits(buffer, n, &tz);

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

    print('static const u32x2_t %s_base_lut[%d] = {' % ('positive' if positive else 'negative', end + 1), end='')
    for i in range(end + 1):
        if i % 4 == 0:
            print()
            print('    ', end='')
        hi = base_lut[i] // 1000000000;
        lo = base_lut[i] - hi * 1000000000;
        print('{%9d,%9d}' % (hi, lo), end='')
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

static inline pow9x2_t positive_diy_fp(int32_t e)
{
    static const u32x2_t positive_base_lut[244] = {
        {100000000,        0}, {160000000,        0}, {256000000,        0}, {409600000,        0},
        {655360000,        0}, {104857600,        0}, {167772160,        0}, {268435456,        0},
        {429496729,600000000}, {687194767,360000000}, {109951162,777600000}, {175921860,444160000},
        {281474976,710656000}, {450359962,737049600}, {720575940,379279360}, {115292150,460684698},
        {184467440,737095517}, {295147905,179352826}, {472236648,286964522}, {755578637,259143235},
        {120892581,961462918}, {193428131,138340668}, {309485009,821345069}, {495176015,714152110},
        {792281625,142643376}, {126765060, 22822941}, {202824096, 36516705}, {324518553,658426727},
        {519229685,853482763}, {830767497,365572421}, {132922799,578491588}, {212676479,325586540},
        {340282366,920938464}, {544451787, 73501542}, {871122859,317602467}, {139379657,490816395},
        {223007451,985306232}, {356811923,176489971}, {570899077, 82383953}, {913438523,331814324},
        {146150163,733090292}, {233840261,972944467}, {374144419,156711148}, {598631070,650737836},
        {957809713, 41180537}, {153249554, 86588886}, {245199286,538542218}, {392318858,461667548},
        {627710173,538668077}, {100433627,766186893}, {160693804,425899028}, {257110087, 81438445},
        {411376139,330301511}, {658201822,928482417}, {105312291,668557187}, {168499666,669691499},
        {269599466,671506398}, {431359146,674410237}, {690174634,679056379}, {110427941,548649021},
        {176684706,477838433}, {282695530,364541493}, {452312848,583266389}, {723700557,733226222},
        {115792089,237316196}, {185267342,779705913}, {296427748,447529461}, {474284397,516047137},
        {758855036, 25675419}, {121416805,764108067}, {194266889,222572908}, {310827022,756116652},
        {497323236,409786643}, {795717178,255658628}, {127314748,520905381}, {203703597,633448609},
        {325925756,213517774}, {521481209,941628439}, {834369935,906605501}, {133499189,745056881},
        {213598703,592091009}, {341757925,747345614}, {546812681,195752982}, {874900289,913204770},
        {139984046,386112764}, {223974474,217780422}, {358359158,748448674}, {573374653,997517878},
        {917399446,396028605}, {146783911,423364577}, {234854258,277383323}, {375766813,243813317},
        {601226901,190101307}, {961963041,904162091}, {153914086,704665935}, {246262538,727465496},
        {394020061,963944793}, {630432099,142311668}, {100869135,862769867}, {161390617,380431787},
        {258224987,808690859}, {413159980,493905375}, {661055968,790248599}, {105768955,  6439776},
        {169230328, 10303642}, {270768524,816485827}, {433229639,706377322}, {693167423,530203715},
        {110906787,764832595}, {177450860,423732152}, {283921376,677971442}, {454274202,684754307},
        {726838724,295606891}, {116294195,887297103}, {186070713,419675364}, {297713141,471480583},
        {476341026,354368932}, {762145642,166990291}, {121943302,746718447}, {195109284,394749515},
        {312174855, 31599224}, {499479768, 50558758}, {799167628,880894012}, {127866820,620943042},
        {204586912,993508867}, {327339060,789614188}, {523742497,263382700}, {837987995,621412319},
        {134078079,299425971}, {214524926,879081554}, {343239883,  6530486}, {549183812,810448778},
        {878694100,496718044}, {140591056, 79474887}, {224945689,727159820}, {359913103,563455711},
        {575860965,701529137}, {921377545,122446620}, {147420407,219591460}, {235872651,551346335},
        {377396242,482154136}, {603833987,971446617}, {966134380,754314587}, {154581500,920690334},
        {247330401,473104535}, {395728642,356967255}, {633165827,771147608}, {101306532,443383618},
        {162090451,909413788}, {259344723, 55062060}, {414951556,888099296}, {663922491, 20958874},
        {106227598,563353420}, {169964157,701365472}, {271942652,322184755}, {435108243,715495608},
        {696173189,944792972}, {111387710,391166876}, {178220336,625867001}, {285152538,601387202},
        {456244061,762219522}, {729990498,819551235}, {116798479,811128198}, {186877567,697805117},
        {299004108,316488186}, {478406573,306381098}, {765450517,290209756}, {122472082,766433561},
        {195955332,426293698}, {313528531,882069916}, {501645651, 11311866}, {802633041,618098985},
        {128421286,658895838}, {205474058,654233341}, {328758493,846773345}, {526013590,154837351},
        {841621744,247739762}, {134659479, 79638362}, {215455166,527421379}, {344728266,443874207},
        {551565226,310198730}, {882504362, 96317968}, {141200697,935410875}, {225921116,696657400},
        {361473786,714651840}, {578358058,743442944}, {925372893,989508710}, {148059663, 38321394},
        {236895460,861314230}, {379032737,378102768}, {606452379,804964428}, {970323807,687943085},
        {155251809,230070894}, {248402894,768113430}, {397444631,628981488}, {635911410,606370380},
        {101745825,697019261}, {162793321,115230818}, {260469313,784369308}, {416750902, 54990893},
        {666801443,287985428}, {106688230,926077669}, {170701169,481724270}, {273121871,170758832},
        {436994993,873214130}, {699191990,197142608}, {111870718,431542818}, {178993149,490468508},
        {286389039,184749613}, {458222462,695599380}, {733155940,312959007}, {117304950,450073442},
        {187687920,720117506}, {300300673,152188010}, {480481077, 43500815}, {768769723,269601304},
        {123003155,723136209}, {196805049,157017934}, {314888078,651228694}, {503820925,841965911},
        {806113481,347145457}, {128978157, 15543274}, {206365051,224869237}, {330184081,959790779},
        {528294531,135665247}, {845271249,817064395}, {135243399,970730304}, {216389439,953168485},
        {346223103,925069576}, {553956966,280111322}, {886331146, 48178115}, {141812983,367708499},
        {226900773,388333598}, {363041237,421333756}, {580865979,874134009}, {929385567,798614415},
        {148701690,847778307}, {237922705,356445291}, {380676328,570312465}, {609082125,712499943},
        {974531401,139999909}, {155925024,182399986}, {249480038,691839977}, {399168061,906943963}
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

    const pow9x2_t v = { .hi = positive_base_lut[e].hi, .lo = positive_base_lut[e].lo, .e = positive_index_lut[e] };
    return v;
}

static inline pow9x2_t negative_diy_fp(int32_t e)
{
    static const u32x2_t negative_base_lut[286] = {
        {100000000,        0}, {625000000,        0}, {390625000,        0}, {244140625,        0},
        {152587890,625000000}, {953674316,406250000}, {596046447,753906250}, {372529029,846191407},
        {232830643,653869629}, {145519152,283668519}, {909494701,772928238}, {568434188,608080149},
        {355271367,880050093}, {222044604,925031309}, {138777878, 78144568}, {867361737,988403548},
        {542101086,242752218}, {338813178,901720136}, {211758236,813575085}, {132348898,  8484428},
        {827180612,553027675}, {516987882,845642297}, {323117426,778526436}, {201948391,736579023},
        {126217744,835361889}, {788860905,221011806}, {493038065,763132379}, {308148791,101957737},
        {192592994,438723586}, {120370621,524202241}, {752316384,526264006}, {470197740,328915004},
        {293873587,705571877}, {183670992,315982424}, {114794370,197489015}, {717464813,734306341},
        {448415508,583941463}, {280259692,864963415}, {175162308, 40602134}, {109476442,525376334},
        {684227765,783602086}, {427642353,614751304}, {267276471,  9219565}, {167047794,380762228},
        {104404871,487976393}, {652530446,799852453}, {407831529,249907783}, {254894705,781192365},
        {159309191,113245228}, {995682444,457782674}, {622301527,786114171}, {388938454,866321357},
        {243086534,291450848}, {151929083,932156780}, {949556774,575979875}, {593472984,109987422},
        {370920615, 68742139}, {231825384,417963837}, {144890865,261227398}, {905567907,882671237},
        {565979942,426669523}, {353737464, 16668452}, {221085915, 10417783}, {138178696,881511115},
        {863616855,509444463}, {539760534,693402790}, {337350334,183376744}, {210843958,864610465},
        {131777474,290381541}, {823609214,314884627}, {514755758,946802892}, {321722349,341751808},
        {201076468,338594880}, {125672792,711621800}, {785454954,447636249}, {490909346,529772656},
        {306818341,581107910}, {191761463,488192444}, {119850914,680120278}, {749068216,750751733},
        {468167635,469219833}, {292604772,168262396}, {182877982,605163998}, {114298739,128227499},
        {714367119,551421864}, {446479449,719638665}, {279049656, 74774166}, {174406035, 46733854},
        {109003771,904208659}, {681273574,401304116}, {425795984,   815072}, {266122490,   509420},
        {166326556,250318388}, {103954097,656448993}, {649713110,352806202}, {406070693,970503876},
        {253794183,731564923}, {158621364,832228077}, {991383530,201425478}, {619614706,375890924},
        {387259191,484931828}, {242036994,678082393}, {151273121,673801496}, {945457010,461259344},
        {590910631,538287090}, {369319144,711429432}, {230824465,444643395}, {144265290,902902122},
        {901658068,143138260}, {563536292,589461413}, {352210182,868413383}, {220131364,292758365},
        {137582102,682973978}, {859888141,768587361}, {537430088,605367101}, {335893805,378354438},
        {209933628,361471524}, {131208517,725919703}, {820053235,786998139}, {512533272,366873837},
        {320333295,229296148}, {200208309,518310093}, {125130193,448943808}, {782063709, 55898799},
        {488789818,159936750}, {305493636,349960469}, {190933522,718725293}, {119333451,699203308},
        {745834073,120020675}, {466146295,700012922}, {291341434,812508076}, {182088396,757817548},
        {113805247,973635968}, {711282799,835224795}, {444551749,897015497}, {277844843,685634686},
        {173653027,303521679}, {108533142, 64701049}, {678332137,904381557}, {423957586,190238473},
        {264973491,368899046}, {165608432,105561904}, {103505270, 65976190}, {646907937,912351186},
        {404317461,195219491}, {252698413,247012182}, {157936508,279382614}, {987103176,746141335},
        {616939485,466338335}, {385587178,416461459}, {240991986,510288412}, {150619991,568930258},
        {941374947,305814109}, {588359342, 66133818}, {367724588,791333637}, {229827867,994583523},
        {143642417,496614702}, {897765109,353841886}, {561103193,346151179}, {350689495,841344487},
        {219180934,900840304}, {136988084,313025190}, {856175526,956407438}, {535109704,347754649},
        {334443565,217346656}, {209027228,260841660}, {130642017,663026038}, {816512610,393912733},
        {510320381,496195458}, {318950238,435122162}, {199343899, 21951351}, {124589936,888719595},
        {778687105,554497464}, {486679440,971560915}, {304174650,607225572}, {190109156,629515983},
        {118818222,893447489}, {742613893, 84046807}, {464133683,177529254}, {290083551,985955784},
        {181302219,991222365}, {113313887,494513978}, {708211796,840712363}, {442632373, 25445227},
        {276645233,140903267}, {172903270,713064542}, {108064544,195665339}, {675403401,222908366},
        {422127125,764317729}, {263829453,602698581}, {164893408,501686613}, {103058380,313554133},
        {644114876,959713331}, {402571798, 99820832}, {251607373,812388020}, {157254608,632742513},
        {982841303,954640703}, {614275814,971650440}, {383922384,357281525}, {239951490,223300953},
        {149969681,389563096}, {937310508,684769347}, {585819067,927980842}, {366136917,454988027},
        {228835573,409367517}, {143022233,380854698}, {893888958,630341861}, {558680599,143963663},
        {349175374,464977290}, {218234609, 40610806}, {136396630,650381754}, {852478941,564885961},
        {532799338,478053726}, {332999586,548783579}, {208124741,592989737}, {130077963,495618586},
        {812987271,847616158}, {508117044,904760099}, {317573153, 65475062}, {198483220,665921914},
        {124052012,916201196}, {775325080,726257475}, {484578175,453910922}, {302861359,658694327},
        {189288349,786683954}, {118305218,616677472}, {739407616,354234195}, {462129760,221396372},
        {288831100,138372733}, {180519437,586482958}, {112824648,491551849}, {705154053, 72199054},
        {440721283,170124409}, {275450801,981327756}, {172156751,238329847}, {107597969,523956155},
        {672487309,524725965}, {420304568,452953728}, {262690355,283096080}, {164181472, 51935050},
        {102613420, 32459407}, {641333875,202871289}, {400833672,  1794556}, {250521045,  1121598},
        {156575653,125700999}, {978597832, 35631240}, {611623645, 22269525}, {382264778,138918453},
        {238915486,336824034}, {149322178,960515021}, {933263618,503218879}, {583289761,564511800},
        {364556100,977819875}, {227847563,111137422}, {142404726,944460889}, {890029543,402880554},
        {556268464,626800346}, {347667790,391750217}, {217292368,994843886}, {135807730,621777429},
        {848798316,386108927}, {530498947,741318079}, {331561842,338323800}, {207226151,461452375},
        {129516344,663407735}, {809477154,146298338}, {505923221,341436462}, {316202013,338397789},
        {197626258,336498618}, {123516411,460311637}, {771977571,626947726}, {482485982,266842329},
        {301553738,916776456}, {188471086,822985285}, {117794429,264365803}, {736215182,902286268},
        {460134489,313928918}, {287584055,821205574}, {179740034,888253484}, {112337521,805158428},
        {702109511,282240170}, {438818444,551400106}, {274261527,844625067}, {171413454,902890667},
        {107133409,314306667}, {669583808,214416666}
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

    const pow9x2_t v = { .hi = negative_base_lut[e].hi, .lo = negative_base_lut[e].lo, .e = negative_index_lut[e] };
    return v;
}

static inline void ldouble_convert(diy_fp_t *v)
{
    uint64_t f = v->f;
    int32_t e = v->e, t = v->e;
    pow9x2_t x;
    uint64_t hi, lo;

    e >>= 2;
    t -= e << 2;
    if (t) {
        ++e;
        f >>= 4 - t;
    }
    x = e >= 0 ? positive_diy_fp(e) : negative_diy_fp(-e);

    hi = f / 1000000000;
    lo = f - hi * 1000000000;
    if (x.lo && lo)
        f = x.hi * hi + (x.hi * lo + x.lo * hi + (x.lo * lo) / 1000000000) / 1000000000;
    else
        f = x.hi * hi + (x.hi * lo + x.lo * hi) / 1000000000;

#if LSHIFT_RESERVED_BIT > 10
    v->f = f;
    v->e = e - x.e + 18;
#elif LSHIFT_RESERVED_BIT > 7
    v->f = f / 10;
    v->e = e - x.e + 19;
#elif LSHIFT_RESERVED_BIT > 4
    v->f = f / 100;
    v->e = e - x.e + 20;
#else
    v->f = f / 1000;
    v->e = e - x.e + 21;
#endif
}

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

static inline int32_t fill_a_16_digits(char *buffer, uint64_t digits, int32_t *ptz)
{
    char *s = buffer;

    if (digits < 100000000llu) {
        memset(s, '0', 8);
        fill_a_8_digits(s + 8, digits, ptz);
    } else {
        uint32_t q = (uint32_t)(digits / 100000000);
        uint32_t r = (uint32_t)(digits - (uint64_t)q * 100000000);

        fill_t_8_digits(s, q, ptz);
        if (r < s_tail_cmp) {
            memset(s + 8, '0', 8);
            *ptz += 8;
        } else {
            fill_a_8_digits(s + 8, r, ptz);
        }
    }

    return 16;
}

static inline int32_t fill_significand(char *buffer, uint64_t digits, int32_t *ptz)
{
    char *s = buffer;

    digits += APPROX_TAIL_CMP_VAL;
    if (digits < 10000000000000000llu) {
        uint32_t q = (uint32_t)(digits / 100000000);
        uint32_t r = (uint32_t)(digits - (uint64_t)q * 100000000);

        s += fill_1_8_digits(s, q, ptz);
        if (r < s_tail_cmp) {
            *ptz += 8;
            memset(s, '0', 8);
            s += 8;
        } else {
            s += fill_a_8_digits(s, r, ptz);
        }
    } else {
        uint32_t q = (uint32_t)(digits / 10000000000000000llu);
        uint64_t r = (digits - (uint64_t)q * 10000000000000000llu);

        s += fill_1_4_digits(s, q, ptz);
        if (r < s_tail_cmp) {
            memset(s, '0', 16);
            s += 16;
            *ptz += 16;
        } else {
            s += fill_a_16_digits(s, r, ptz);
        }
    }

    return s - buffer;
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

static inline char* ldouble_format(char *buffer, uint64_t digits, int32_t decimal_exponent)
{
    int32_t num_digits, trailing_zeros, vnum_digits, decimal_point;

    num_digits = fill_significand(buffer + 1, digits, &trailing_zeros);
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

int jnum_parse_hex(const char *str, jnum_type_t *type, jnum_value_t *value)
{
    const char *s = str;
    char c;
    uint64_t m = 0;

    s += 2;
    while (1) {
        switch ((c = *s)) {
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            c -= '0';
            break;
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
            c -= 'a';
            break;
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
            c -= 'A';
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

int jnum_parse_num(const char *str, jnum_type_t *type, jnum_value_t *value)
{
    static const double div10_lut[20] = {
        1    , 1e-1 , 1e-2 , 1e-3 , 1e-4 , 1e-5 , 1e-6 , 1e-7 , 1e-8 , 1e-9 ,
        1e-10, 1e-11, 1e-12, 1e-13, 1e-14, 1e-15, 1e-16, 1e-17, 1e-18, 1e-19,
    };

    const char *s = str;
    int32_t sign = 1, k = 0;
    uint64_t m = 0, n = 0;
    double d = 0;

    switch (*s) {
    case '-': ++s; sign = -1; break;
    case '+': ++s; break;
    default: break;
    }

    while (*s == '0')
        ++s;

    while (1) {
        switch (*s) {
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            break;
        case 'e': case 'E':
            goto end;
        case '.':
            goto next2;
        default:
            goto next1;
        }
        m = (m << 3) + (m << 1) + (*s++ - '0');
        ++k;
    }

next1:
    if (k >= 20)
        goto end;

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
            goto end;
        }
    }
    return s - str;

next2:
    if (k >= 20)
        goto end;

    ++s;
    k = 0;
    while (1) {
        switch (*s) {
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            break;
        default:
            goto next3;
        }
        n = (n << 3) + (n << 1) + (*s++ - '0');
        ++k;
    }

next3:
    if (k >= 20)
        goto end;

    d = m + n * div10_lut[k];
    *type = JNUM_DOUBLE;
    value->vdbl = sign == 1 ? d : -d;
    return s - str;

end:
    *type = JNUM_DOUBLE;
    value->vdbl = strtod(str, (char **)&s);
    return s - str;
}

#define jnum_to_func(rtype, fname)                      \
rtype fname(const char *str)                            \
{                                                       \
    jnum_type_t type;                                   \
    jnum_value_t value;                                 \
    rtype val = 0;                                      \
    unsigned char c = *(unsigned char *)str;            \
                                                        \
    while (c <= ' ' && c) c = *(unsigned char *)++str;  \
    jnum_parse(str, &type, &value);                     \
    switch (type) {                                     \
    case JNUM_BOOL:   val = (rtype)value.vbool;break;   \
    case JNUM_INT:    val = (rtype)value.vint; break;   \
    case JNUM_HEX:    val = (rtype)value.vhex; break;   \
    case JNUM_LINT:   val = (rtype)value.vlint;break;   \
    case JNUM_LHEX:   val = (rtype)value.vlhex;break;   \
    case JNUM_DOUBLE: val = (rtype)value.vdbl; break;   \
    default: break;                                     \
    }                                                   \
    return val;                                         \
}

jnum_to_func(int32_t, jnum_atoi)
jnum_to_func(int64_t, jnum_atol)
jnum_to_func(uint32_t, jnum_atoh)
jnum_to_func(uint64_t, jnum_atolh)
jnum_to_func(double, jnum_atod)