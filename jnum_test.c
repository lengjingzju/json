#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "jnum.h"
#if defined(_MSC_VER)
#include <windows.h>
#endif

extern int grisu2_dtoa(double num, char *buffer);
extern int dragonbox_dtoa(double num, char *buffer);

static unsigned int _system_ms_get(void)
{
#if !defined(_MSC_VER)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#else
    static LARGE_INTEGER freq = {0};
    static int freq_initialized = 0;
    if (!freq_initialized) {
        QueryPerformanceFrequency(&freq);
        freq_initialized = 1;
    }

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (unsigned int)((counter.QuadPart * 1000) / freq.QuadPart);
#endif
}

int main(int argc, char *argv[])
{
    char buf[64] = {0};

    if (argc != 2 && argc != 3 && argc != 4) {
        printf("Usage: %s <num>\n", argv[0]);
        printf("       %s <num> <cnt>\n", argv[0]);
        printf("       %s a <num>\n", argv[0]);
        printf("       %s a <num> <cnt>\n", argv[0]);
        printf("       %s <i/l/h/L/d> <num>\n", argv[0]);
        printf("       %s <i/l/h/L/d> <num>\n", argv[0]);
        printf("a: atod, i: itoa, l: ltoa, h: htoa, L: lhtoa, d or default: dtoa/grisu2/dragonbox/...\n");
        return -1;
    }

    switch (*argv[1]) {
    case 'i':
        {
            int32_t d = jnum_atoi(argv[2]);
            jnum_itoa(d, buf);
            printf("original  : %s\nprintf    : %d\njnum      : %s\n", argv[2], d, buf);
            break;
        }
    case 'l':
        {
            int64_t d = jnum_atol(argv[2]);
            jnum_ltoa(d, buf);
            printf("original  : %s\nprintf    : %lld\njnum      : %s\n", argv[2], (long long)d, buf);
            break;
        }
    case 'h':
        {
            uint32_t d = jnum_atoh(argv[2]);
            jnum_htoa(d, buf);
            printf("original  : %s\nprintf    : 0x%x\njnum      : %s\n", argv[2], d, buf);
            break;
        }
    case 'L':
        {
            uint64_t d = jnum_atolh(argv[2]);
            jnum_lhtoa(d, buf);
            printf("original  : %s\nprintf    : 0x%llx\njnum      : %s\n", argv[2], (unsigned long long)d, buf);
            break;
        }
    case 'd':
        {
            double d = jnum_atod(argv[2]);
            printf("original  : %s\nprintf    : %0.16g\n", argv[2], d);
            jnum_dtoa(d, buf);
            printf("ldouble   : %s\n", buf);
            grisu2_dtoa(d, buf);
            printf("grisu2    : %s\n", buf);
            dragonbox_dtoa(d, buf);
            printf("dragonbox : %s\n", buf);
            break;
        }

    case 'a':
        if (argc == 3) {
            volatile double d1, d2;
            d1 = strtod(argv[2], NULL);
            d2 = jnum_atod(argv[2]);
            if (d1 != d2)
                printf("----%s----\nstrtod:    %0.16g\njnum_atod: %0.16g\n", argv[2], d1, d2);
        } else {
            int i;
            int cnt = atoi(argv[3]);
            volatile double d1, d2;
            unsigned int ms1, ms2, ms3;
            char tmp[64] = {0};

            ms1 = _system_ms_get();
            for (i = 0; i < cnt; ++i) {
                d1 = strtod(argv[2], NULL);
            }
            ms2 = _system_ms_get();
            for (i = 0; i < cnt; ++i) {
                d2 = jnum_atod(argv[2]);
            }
            ms3 = _system_ms_get();

            jnum_dtoa(d1, buf);
            jnum_dtoa(d2, tmp);
            printf("original  : %s\nstrtod:    %s %ums\njnum_atod: %s %ums\t%.0lf%%\n", argv[2], buf, ms2 - ms1, tmp, ms3 - ms2,
                 ms3 - ms2 ? 100.0 * (ms2 - ms1) / (ms3 - ms2) : 0);
        }
        break;

    default:
        if (argc == 2) {
            double d = strtod(argv[1], NULL);
            char tmp[64] = {0};

            snprintf(tmp, sizeof(tmp), "%0.16g", d);
            jnum_dtoa(d, buf);
            if (strcmp(buf, tmp)) {
                printf("original  : %s\nprintf    : %s\n", argv[1], tmp);
                printf("ldouble   : %s\n", buf);
            }
        } else {
            int i;
            double d = strtod(argv[1], NULL);
            int cnt = atoi(argv[2]);
            unsigned int ms, ms1, ms2, ms3;

            ms1 = _system_ms_get();
            for (i = 0; i < cnt; ++i) {
                snprintf(buf, sizeof(buf), "%0.16g", d);
            }
            ms2 = _system_ms_get();
            ms = ms2 - ms1;
            printf("original  : %s\nprintf    : %0.16g\t%ums\n", argv[1], d, ms);

            ms1 = _system_ms_get();
            for (i = 0; i < cnt; ++i) {
                jnum_dtoa(d, buf);
            }
            ms2 = _system_ms_get();
            ms3 = ms2 - ms1;
            printf("ldouble   : %s\t%ums\t%.0lf%%\n", buf, ms3, ms3 ? 100.0 * ms / ms3 : 0);

            ms1 = _system_ms_get();
            for (i = 0; i < cnt; ++i) {
                grisu2_dtoa(d, buf);
            }
            ms2 = _system_ms_get();
            ms3 = ms2 - ms1;
            printf("grisu2    : %s\t%ums\t%.0lf%%\n", buf, ms3, ms3 ? 100.0 * ms / ms3 : 0);

            ms1 = _system_ms_get();
            for (i = 0; i < cnt; ++i) {
                dragonbox_dtoa(d, buf);
            }
            ms2 = _system_ms_get();
            ms3 = ms2 - ms1;
            printf("dragonbox : %s\t%ums\t%.0lf%%\n", buf, ms3, ms3 ? 100.0 * ms / ms3 : 0);
        }
        break;
    }

    return 0;
}
