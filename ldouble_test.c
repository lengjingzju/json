#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

extern int ldouble_dtoa(double value, char* buffer);

static unsigned int _system_ms_get(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    //extern cyg_uint32 cyg_time_get_ms(void);
    //return cyg_time_get_ms(); //ecos
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
        ldouble_dtoa(d, buf);
#if 1
        printf("original: %s\nprintf  : %0.15g\nldouble : %s\n", argv[1], d, buf);
#else
        char buf2[64] = {0};
        sprintf(buf2, "%0.15g", d);
        if (strcmp(buf, buf2))
            printf("original: %s\nprintf  : %s\nldouble : %s\n", argv[1], buf2, buf);
#endif
    } else {
        int i;
        int cnt = atoi(argv[2]);
        unsigned int ms1, ms2, ms3;

        ms1 = _system_ms_get();
        for (i = 0; i < cnt; ++i) {
            sprintf(buf, "%0.15g", d);
        }
        ms2 = _system_ms_get();
        for (i = 0; i < cnt; ++i) {
            ldouble_dtoa(d, buf);
        }
        ms3 = _system_ms_get();
        printf("original: %s\nprintf  : %0.15g\t%ums\nldouble : %s\t%ums\t%.0lf%%\n", argv[1], d,
            ms2 - ms1, buf, ms3 - ms2, ms3 - ms2 ? 100.0 * (ms2 - ms1) / (ms3 - ms2) : 0);
    }

    return 0;
}
