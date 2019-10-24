#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include "json.h"

#define _fmalloc       malloc
#define _ffree         free
#define _fclose_fp(fp) do {if (fp) fclose(fp); fp = NULL; } while(0)
#define _free_ptr(ptr) do {if (ptr) _ffree(ptr); ptr = NULL; } while(0)

static time_t _system_ms_get(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    //extern cyg_uint32 cyg_time_get_ms(void);
    //return cyg_time_get_ms(); //ecos
}

int copy_data_to_file(char *data, size_t size, const char *dst)
{
    int ret = -1;
    FILE *wfp = NULL;

    if (!data || !dst)
        return -1;
    if ((wfp = fopen(dst, "w+")) == NULL)
        return -1;
    if (size == fwrite(data, 1, size, wfp))
        ret = 0;
    _fclose_fp(wfp);
    if (ret < 0 && access(dst, F_OK) == 0)
        unlink(dst);

    return ret;
}

int read_file_to_data(const char *src, char **data, size_t *size)
{
    FILE *rfp = NULL;
    size_t total = 0;

    if (!src || !data)
        return -1;

    if (!size)
        size = &total;
    *data = NULL, *size = 0;

    if ((rfp = fopen(src, "r")) == NULL)
        return -1;
    fseek(rfp, 0, SEEK_END);
    *size = ftell(rfp);
    fseek(rfp, 0, SEEK_SET);
    if (*size == 0)
        goto err;

    if ((*data = _fmalloc(*size + 1)) == NULL)
        goto err;
    if (*size != fread(*data, 1, *size, rfp))
        goto err;

    (*data)[*size] = 0;
    _fclose_fp(rfp);
    return 0;
err:
    _fclose_fp(rfp);
    _free_ptr(*data);
    *size = 0;
    return -1;
}

int read_file_data_free(char **data, size_t *size)
{
    if (data)
        _free_ptr(*data);
    if (size)
        *size = 0;
    return 0;
}

int main(int argc, char *argv[])
{
#define _FAST_PARSE   1
    // gcc -o json json.c json_test.c -lm -O0 -g
    // gcc -o json json.c json_test.c -lm -O2 -ffunction-sections -fdata-sections -W -Wall
    int ret = -1;
    json_object *json = NULL;
    struct json_list_head head;
    char *data = NULL;
    char *new_data = NULL;
    size_t size = 0;
    int src_len = 0;
    const char *src = NULL;
    char *dst = NULL;
    time_t ms1, ms2, ms3, ms4;

    if (argc != 3) {
        printf("Usage: %s xxx.json f/s/r\n", argv[0]);
        printf("f: one read one parse; s: read all then parse; r: reuse original str\n");
        return -1;
    }
    if (!argv[1] || strlen(argv[1]) == 0)
        return -1;

    json_cache_memory_init(&head);
    src = argv[1];
    src_len = strlen(src);
    if ((dst = _fmalloc(src_len + 1 + strlen(".unformat.json"))) == NULL)
    {
        printf("malloc dst err!\n");
        goto err;
    }

    ms1 = _system_ms_get();
#if _FAST_PARSE

#if JSON_DIRECT_FILE_SUPPORT
    if (strncmp(argv[2], "f", 1) == 0)
    {
        if ((json = json_fast_parse_file(src, &head)) == NULL)
        {
            printf("json_parse err!\n");
            goto err;
        }
    }
    else
#endif
    {
        if (read_file_to_data(src, &data, &size) < 0)
        {
            printf("read err!\n");
            goto err;
        }
        if (strncmp(argv[2], "r", 1) == 0)
        {
            if ((json = json_rapid_parse_str(data, &head, size)) == NULL)
            {
                read_file_data_free(&data, &size);
                printf("json_parse err!\n");
                goto err;
            }
            printf("parse ms=%ld\n", _system_ms_get()-ms1);

            ms1 = _system_ms_get();
            sprintf(dst, "%s.rapid.json", src);
            if ((new_data = json_print_format(json)) == NULL)
            {
                read_file_data_free(&data, &size);
                printf("json_print err!\n");
                goto err;
            }
            if (copy_data_to_file(new_data, strlen(new_data), dst) < 0)
            {
                read_file_data_free(&data, &size);
                printf("write err!\n");
                goto err;
            }
            printf("print ms=%ld\n", _system_ms_get()-ms1);
            read_file_data_free(&data, &size);
            goto end;
        }
        else
        {
            if ((json = json_fast_parse_str(data, &head, size)) == NULL)
            {
                read_file_data_free(&data, &size);
                printf("json_parse err!\n");
                goto err;
            }
            read_file_data_free(&data, &size);
        }
    }

#else

#if JSON_DIRECT_FILE_SUPPORT
    if (strncmp(argv[2], "f", 1) == 0)
    {
        if ((json = json_parse_file(src)) == NULL)
        {
            printf("json_parse err!\n");
            goto err;
        }
    }
    else
#endif
    {
        if (read_file_to_data(src, &data, &size) < 0)
        {
            printf("read err!\n");
            goto err;
        }
        if ((json = json_parse_str(data)) == NULL)
        {
            read_file_data_free(&data, &size);
            printf("json_parse err!\n");
            goto err;
        }
        read_file_data_free(&data, &size);
    }


#endif

    ms2 = _system_ms_get();
#if JSON_DIRECT_FILE_SUPPORT
    if (strncmp(argv[2], "f", 1) == 0)
    {
        sprintf(dst, "%s.unformat.json", src);
        if (json_fprint_unformat(json, dst) == NULL)
        {
            printf("json_print err!\n");
            goto err;
        }
    }
    else
#endif
    {
        sprintf(dst, "%s.unformat.json", src);
        if ((new_data = json_print_unformat(json)) == NULL)
        {
            printf("json_print err!\n");
            goto err;
        }
        if (copy_data_to_file(new_data, strlen(new_data), dst) < 0)
        {
            printf("write err!\n");
            goto err;
        }
        json_free_print_ptr(new_data);
        new_data = NULL;
    }

    ms3 = _system_ms_get();
#if JSON_DIRECT_FILE_SUPPORT
    if (strncmp(argv[2], "f", 1) == 0)
    {
        sprintf(dst, "%s.format.json", src);
        if (json_fprint_format(json, dst) == NULL)
        {
            printf("json_print err!\n");
            goto err;
        }
    }
    else
#endif
    {
        sprintf(dst, "%s.format.json", src);
        if ((new_data = json_print_format(json)) == NULL)
        {
            printf("json_print err!\n");
            goto err;
        }
        if (copy_data_to_file(new_data, strlen(new_data), dst) < 0)
        {
            printf("write err!\n");
            goto err;
        }
        json_free_print_ptr(new_data);
        new_data = NULL;
    }


    ms4 = _system_ms_get();
    printf("parse ms=%ld, unformat_print_ms=%ld, format_print_ms=%ld\n", ms2-ms1, ms3-ms2, ms4-ms3);

end:
    ret = 0;
err:
    _free_ptr(dst);
#if _FAST_PARSE
    json_cache_memory_free(&head);
#else
    if (json)
        json_delete_json(json);
#endif
    if (new_data)
        json_free_print_ptr(new_data);

    return ret;
}

