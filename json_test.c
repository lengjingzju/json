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

static unsigned _system_ms_get(void)
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

static char s_dst_json_path[256] = {0};
static void usage_print(const char *func)
{
#define ANY_JSON_NAME "x.json"
    printf("Usage: %s <%s> <num>\n", func, ANY_JSON_NAME);
    printf("\t%s %s 1 ==> test json_parse_str()\n", func, ANY_JSON_NAME);
    printf("\t%s %s 2 ==> test json_fast_parse_str()\n", func, ANY_JSON_NAME);
    printf("\t%s %s 3 ==> test json_reuse_parse_str()\n", func, ANY_JSON_NAME);
    printf("\t%s %s 4 ==> test json_parse_file()\n", func, ANY_JSON_NAME);
    printf("\t%s %s 5 ==> test json_fast_parse_file()\n", func, ANY_JSON_NAME);
    printf("\t%s %s 6 ==> test json_sax_parse_str()\n", func, ANY_JSON_NAME);
    printf("\t%s %s 7 ==> test json_sax_parse_file()\n", func, ANY_JSON_NAME);
    printf("Usage: %s 0 ==> test json_sax_print_xxxx()\n", func);
}

int test_json_sax_print(void)
{
    json_sax_print_hd handle = NULL;
    char *print_str = NULL;

    handle = json_sax_print_format_start(100);

    json_sax_print_object(handle, NULL, JSON_SAX_START);
    json_sax_print_string(handle, "Name", "LengJing");
    json_sax_print_int(handle, "Age", 30);
    json_sax_print_string(handle, "Phone", "18368887550");
    json_sax_print_array(handle, "Hobby", JSON_SAX_START);
    json_sax_print_string(handle, NULL, "Reading");
    json_sax_print_string(handle, NULL, "Walking");
    json_sax_print_string(handle, NULL, "Thinking");
    json_sax_print_array(handle, "Hobby", JSON_SAX_FINISH);
    json_sax_print_object(handle, NULL, JSON_SAX_FINISH);

    print_str = json_sax_print_finish(handle);
    printf("%s\n", print_str);
    json_free_print_ptr(print_str);

    return 0;
}

json_sax_ret_t _sax_parser_cb(json_sax_parser_t *parser)
{
#define KEY_BUF_LEN 64
#define STR_BUF_LEN 128
    static json_sax_print_hd handle = NULL;
    char *key = NULL;
    char *str = NULL;
    int key_alloc_flag = 0;
    int str_alloc_flag = 0;
    char key_buf[KEY_BUF_LEN] = {0};
    char str_buf[STR_BUF_LEN] = {0};
    json_sax_depth_t *depth = &parser->array[parser->count-1];
    if (depth->key.alloc == 0 && depth->key.str != NULL) {
        if (depth->key.len < KEY_BUF_LEN) {
            key = key_buf;
        } else {
            key = malloc(depth->key.len + 1);
            key_alloc_flag = 1;
        }
        memcpy(key, depth->key.str, depth->key.len);
        key[depth->key.len] = 0;
    } else {
        key = depth->key.str;
    }

    switch (parser->array[parser->count-1].type) {
        case JSON_NULL:
            json_sax_print_null(handle, key);
            break;
        case JSON_BOOL:
            json_sax_print_bool(handle, key, parser->value.vnum.vbool);
            break;
        case JSON_INT:
            json_sax_print_int(handle, key, parser->value.vnum.vint);
            break;
        case JSON_HEX:
            json_sax_print_hex(handle, key, parser->value.vnum.vhex);
            break;
        case JSON_DOUBLE:
            json_sax_print_double(handle, key, parser->value.vnum.vdbl);
            break;
        case JSON_STRING:
            if (parser->value.vstr.alloc == 0 && parser->value.vstr.str != NULL) {
                if (parser->value.vstr.len < STR_BUF_LEN) {
                    str = str_buf;
                } else {
                    str = malloc(parser->value.vstr.len + 1);
                    str_alloc_flag = 1;
                }
                memcpy(str, parser->value.vstr.str, parser->value.vstr.len);
                str[parser->value.vstr.len] = 0;
                json_sax_print_string(handle, key, str);
                if (str_alloc_flag)
                    free(str);
            } else {
                json_sax_print_string(handle, key, parser->value.vstr.str);
            }
            break;
        case JSON_ARRAY:
            if (parser->count == 1) {
                if (parser->value.vcmd == JSON_SAX_START) {
                    handle = json_sax_fprint_format_start(s_dst_json_path);
                    json_sax_print_array(handle, key, parser->value.vcmd);
                } else {
                    json_sax_print_array(handle, key, parser->value.vcmd);
                    json_sax_print_finish(handle);
                }
            } else {
                json_sax_print_array(handle, key, parser->value.vcmd);
            }
            break;
        case JSON_OBJECT:
            if (parser->count == 1) {
                if (parser->value.vcmd == JSON_SAX_START) {
                    handle = json_sax_fprint_format_start(s_dst_json_path);
                    json_sax_print_object(handle, key, parser->value.vcmd);
                } else {
                    json_sax_print_object(handle, key, parser->value.vcmd);
                    json_sax_print_finish(handle);
                }
            } else {
                json_sax_print_object(handle, key, parser->value.vcmd);
            }
           break;
        default:
           break;
    }

    if (key_alloc_flag)
        free(key);
    return JSON_SAX_PARSE_CONTINUE;
}
// gcc -o json json.c json_test.c -lm -O0 -g -W -Wall
// gcc -o json json.c json_test.c -lm -O2 -ffunction-sections -fdata-sections -W -Wall
int main(int argc, char *argv[])
{
    int choice = 0;
    char *file = NULL;
    char *orig_data = NULL;
    char *print_str = NULL;
    size_t orig_size = 0;
    json_mem_t mem;
    json_object *json = NULL;
    unsigned int ms1 = 0, ms2 = 0, ms3 = 0, ms4 = 0;
    int fast_flag = 0;

    pjson_memory_init(&mem);

    if (argc == 2) {
        choice = atoi(argv[1]);
        if (choice == 0) {
            test_json_sax_print();
            return 0;
        } else {
            usage_print(argv[0]);
            return -1;
        }
    }

    if (argc != 3) {
        usage_print(argv[0]);
        return -1;
    }

    file = argv[1];
    if (strlen(file) == 0 || access(file, F_OK) != 0) {
            printf("%s is not exist!\n", file);
        return -1;
    }

    ms1 = _system_ms_get();
    choice = atoi(argv[2]);
    switch(choice)
    {
        case 1:
        case 2:
        case 3:
        case 6:
            if (read_file_to_data(file, &orig_data, &orig_size) < 0) {
                printf("read file %s failed!\n", file);
                return -1;
            }
            break;
        case 4:
        case 5:
        case 7:
            break;
        default:
            usage_print(argv[0]);
            return -1;
    }

    switch(choice)
    {
        case 1:
            json = json_parse_str(orig_data);
            break;
        case 2:
            json = json_fast_parse_str(orig_data, &mem, orig_size);
            fast_flag = 1;
            break;
        case 3:
            json = json_reuse_parse_str(orig_data, &mem, orig_size);
            fast_flag = 1;
            break;
        case 4:
            json = json_parse_file(file);
            break;
        case 5:
            json = json_fast_parse_file(file, &mem);
            fast_flag = 1;
            break;
        case 6:
            if (json_sax_parse_str(orig_data, _sax_parser_cb) < 0) {
                printf("json_sax_parse_str failed!\n");
                read_file_data_free(&orig_data, &orig_size);
                return -1;
            }
            ms2 = _system_ms_get();
            goto end;
        case 7:
            if (json_sax_parse_file(file, _sax_parser_cb) < 0) {
                printf("json_sax_parse_file failed!\n");
                return -1;
            }
            ms2 = _system_ms_get();
            goto end;
        default:
            break;
    }

    if (json == NULL) {
        printf("json parse failed!\n");

        if (orig_data)
            read_file_data_free(&orig_data, &orig_size);
        return -1;
    }

    ms2 = _system_ms_get();
    snprintf(s_dst_json_path, sizeof(s_dst_json_path), "%s-%d.format.json", file, choice);
    switch(choice)
    {
        case 1:
            print_str = json_print_format(json);
            break;
        case 2:
            print_str = json_print_format(json);
            break;
        case 3:
            print_str = json_print_format(json);
            break;
        case 4:
            json_fprint_format(json, s_dst_json_path);
            break;
        case 5:
            json_fprint_format(json, s_dst_json_path);
            break;
        default:
            break;
    }
    if (print_str) {
        copy_data_to_file(print_str, strlen(print_str), s_dst_json_path);
        json_free_print_ptr(print_str);
        print_str = NULL;
    }


    ms3 = _system_ms_get();
    snprintf(s_dst_json_path, sizeof(s_dst_json_path), "%s-%d.unformat.json", file, choice);
    switch(choice)
    {
        case 1:
            print_str = json_print_unformat(json);
            break;
        case 2:
            print_str = json_print_unformat(json);
            break;
        case 3:
            print_str = json_print_unformat(json);
            break;
        case 4:
            json_fprint_unformat(json, s_dst_json_path);
            break;
        case 5:
            json_fprint_unformat(json, s_dst_json_path);
            break;
        default:
            break;
    }
    if (print_str) {
        copy_data_to_file(print_str, strlen(print_str), s_dst_json_path);
        json_free_print_ptr(print_str);
        print_str = NULL;
    }
    ms4 = _system_ms_get();

end:
    if (!fast_flag && json)
        json_del_object(json);
    pjson_memory_free(&mem);
    if (orig_data)
        read_file_data_free(&orig_data, &orig_size);

    if (choice < 6)
        printf("parse ms=%d, format print_ms=%d, unformat print_ms=%d\n", ms2-ms1, ms3-ms2, ms4-ms3);
    else
        printf("sax parse+print ms=%d\n", ms2-ms1);
    return 0;
}

