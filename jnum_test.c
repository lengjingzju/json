#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

extern int jnum_dtoa(double num, char *buffer);
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
    if (argc != 2 && argc != 3) {
        printf("Usage: %s <num>\n", argv[0]);
        printf("Usage: %s <num> <cnt>\n", argv[0]);
        return -1;
    }

    char *tmp = NULL;
    double d = strtod(argv[1], &tmp);
    char buf[64] = {0};

    if (argc == 2) {
#if 1
        printf("original  : %s\nprintf    : %0.15g\n", argv[1], d);
        jnum_dtoa(d, buf);
        printf("ldouble   : %s\n", buf);
        grisu2_dtoa(d, buf);
        printf("grisu2    : %s\n", buf);
        dragonbox_dtoa(d, buf);
        printf("dragonbox : %s\n", buf);
#else
        char buf2[64] = {0};

        sprintf(buf2, "%0.15g", d);
        jnum_dtoa(d, buf);
        if (strcmp(buf, buf2)) {
            printf("original  : %s\nprintf    : %0.15g\n", argv[1], d);
            printf("ldouble   : %s\n", buf);
        }
#endif
    } else {
        int i;
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

    return 0;
}
