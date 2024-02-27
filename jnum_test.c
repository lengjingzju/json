#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "jnum.h"

extern int grisu2_dtoa(double num, char *buffer);
extern int dragonbox_dtoa(double num, char *buffer);

static unsigned int _system_ms_get(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
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
            printf("original  : %s\nprintf    : %ld\njnum      : %s\n", argv[2], d, buf);
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
            printf("original  : %s\nprintf    : 0x%lx\njnum      : %s\n", argv[2], d, buf);
            break;
        }
    case 'd':
        {
            double d = jnum_atod(argv[2]);
            printf("original  : %s\nprintf    : %0.15g\n", argv[2], d);
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
            printf("strtod:    %0.15g\njnum_atod: %0.15g\n", d1, d2);
        } else {
            int i;
            int cnt = atoi(argv[3]);
            volatile double d1, d2;
            unsigned int ms1, ms2, ms3;

            ms1 = _system_ms_get();
            for (i = 0; i < cnt; ++i) {
                d1 = strtod(argv[2], NULL);
            }
            ms2 = _system_ms_get();
            for (i = 0; i < cnt; ++i) {
                d2 = jnum_atod(argv[2]);
            }
            ms3 = _system_ms_get();
            printf("strtod:    %0.15g %ums\njnum_atod: %0.15g %ums\t%.0lf%%\n", d1, ms2 - ms1, d2, ms3 - ms2,
                 ms3 - ms2 ? 100.0 * (ms2 - ms1) / (ms3 - ms2) : 0);
        }
        break;

    default:
        if (argc == 2) {
            double d = jnum_atod(argv[1]);
            char tmp[64] = {0};

            sprintf(tmp, "%0.15g", d);
            jnum_dtoa(d, buf);
            if (strcmp(buf, tmp)) {
                printf("original  : %s\nprintf    : %s\n", argv[1], tmp);
                printf("ldouble   : %s\n", buf);
            }
        } else {
            int i;
            double d = jnum_atod(argv[1]);
            int cnt = atoi(argv[2]);
            unsigned int ms, ms1, ms2, ms3;

            ms1 = _system_ms_get();
            for (i = 0; i < cnt; ++i) {
                sprintf(buf, "%0.15g", d);
            }
            ms2 = _system_ms_get();
            ms = ms2 - ms1;
            printf("original  : %s\nprintf    : %0.15g\t%ums\n", argv[1], d, ms);

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
