#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "json.h"

/*
 * Compile Method:
 * gcc -o ljson json.c json_test.c -O0 -g -W -Wall
 * gcc -o ljson json.c json_test.c -O2 -ffunction-sections -fdata-sections -W -Wall
 */

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
#if JSON_SAX_APIS_SUPPORT
    printf("\t%s %s 6 ==> test json_sax_parse_str()\n", func, ANY_JSON_NAME);
    printf("\t%s %s 7 ==> test json_sax_parse_file()\n", func, ANY_JSON_NAME);
    printf("Usage: %s 0 ==> test json_sax_print_xxxx()\n", func);
#endif
}

#if JSON_SAX_APIS_SUPPORT
int test_json_sax_print(void)
{
    json_sax_print_hd handle = NULL;
    char *print_str = NULL;
    json_string_t jkey = {0}, jstr = {0};

    handle = json_sax_print_format_start(10);
    json_sax_print_object(handle, NULL, JSON_SAX_START);
    jkey.str = "Name", jkey.len = 4;
    jstr.str = "LengJing", jstr.len = 8;
    json_sax_print_string(handle, &jkey, &jstr);
    jkey.str = "Age", jkey.len = 3;
    json_sax_print_int(handle, &jkey, 30);
    jkey.str = "Phone", jkey.len = 5;
    jstr.str = "18368887550", jstr.len = 11;
    json_sax_print_string(handle, &jkey, &jstr);
    jkey.str = "Hobby", jkey.len = 5;
    json_sax_print_array(handle, &jkey, JSON_SAX_START);
    jstr.str = "Reading", jstr.len = 7;
    json_sax_print_string(handle, NULL, &jstr);
    jstr.str = "Walking", jstr.len = 7;
    json_sax_print_string(handle, NULL, &jstr);
    jstr.str = "Thinking", jstr.len = 8;
    json_sax_print_string(handle, NULL, &jstr);
    json_sax_print_array(handle, NULL, JSON_SAX_FINISH);
    json_sax_print_object(handle, NULL, JSON_SAX_FINISH);

    print_str = json_sax_print_finish(handle, NULL);
    printf("%s\n", print_str);
    json_memory_free(print_str);

    return 0;
}

static bool s_sax_for_file = false;
json_sax_ret_t _sax_parser_cb(json_sax_parser_t *parser)
{
    static json_sax_print_hd handle = NULL;
    char *print_str = NULL;
    size_t print_size = 0;
    json_string_t *jkey = &parser->array[parser->index];

    if (parser->index == 0) {
        switch (parser->array[parser->index].type) {
        case JSON_ARRAY:
        case JSON_OBJECT:
            if (parser->value.vcmd == JSON_SAX_START) {
                if (s_sax_for_file)
                    handle = json_sax_fprint_format_start(0, s_dst_json_path);
                else
                    handle = json_sax_print_format_start(0);
            }
            break;
        default:
            if (s_sax_for_file)
                handle = json_sax_fprint_format_start(0, s_dst_json_path);
            else
                handle = json_sax_print_format_start(0);
            break;
        }
    }

    switch (parser->array[parser->index].type) {
    case JSON_NULL:
        json_sax_print_null(handle, jkey);
        break;
    case JSON_BOOL:
        json_sax_print_bool(handle, jkey, parser->value.vnum.vbool);
        break;
    case JSON_INT:
        json_sax_print_int(handle, jkey, parser->value.vnum.vint);
        break;
    case JSON_HEX:
        json_sax_print_hex(handle, jkey, parser->value.vnum.vhex);
        break;
#if JSON_LONG_LONG_SUPPORT
    case JSON_LINT:
        json_sax_print_lint(handle, jkey, parser->value.vnum.vlint);
        break;
    case JSON_LHEX:
        json_sax_print_lhex(handle, jkey, parser->value.vnum.vlhex);
        break;
#endif
    case JSON_DOUBLE:
        json_sax_print_double(handle, jkey, parser->value.vnum.vdbl);
        break;
    case JSON_STRING:
        json_sax_print_string(handle, jkey, &parser->value.vstr);
        break;
    case JSON_ARRAY:
        json_sax_print_array(handle, jkey, parser->value.vcmd);
        break;
    case JSON_OBJECT:
        json_sax_print_object(handle, jkey, parser->value.vcmd);
        break;
    default:
        break;
    }

    if (parser->index == 0) {
        switch (parser->array[parser->index].type) {
        case JSON_ARRAY:
        case JSON_OBJECT:
            if (parser->value.vcmd == JSON_SAX_FINISH) {
                if (s_sax_for_file) {
                    json_sax_print_finish(handle, NULL);
                } else {
                    print_str = json_sax_print_finish(handle, &print_size);
                    if (print_str) {
                        copy_data_to_file(print_str, print_size, s_dst_json_path);
                        json_memory_free(print_str);
                        print_str = NULL;
                    }
                }
            }
            break;
        default:
            if (s_sax_for_file) {
                json_sax_print_finish(handle, NULL);
            } else {
                print_str = json_sax_print_finish(handle, &print_size);
                if (print_str) {
                    copy_data_to_file(print_str, print_size, s_dst_json_path);
                    json_memory_free(print_str);
                    print_str = NULL;
                }
            }
            break;
        }
    }

    return JSON_SAX_PARSE_CONTINUE;
}
#endif

int main(int argc, char *argv[])
{
    int choice = 0;
    char *file = NULL;
    char *orig_data = NULL;
    char *print_str = NULL;
    size_t orig_size = 0, print_size = 0;
    json_mem_t mem;
    json_object *json = NULL;
    unsigned int ms1 = 0, ms2 = 0, ms3 = 0, ms4 = 0, ms5 = 0;
    int fast_flag = 0;
    int item_total = 0;

    pjson_memory_init(&mem);

#if JSON_SAX_APIS_SUPPORT
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
#endif

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
#if JSON_SAX_APIS_SUPPORT
    case 6:
#endif
        if (read_file_to_data(file, &orig_data, &orig_size) < 0) {
            printf("read file %s failed!\n", file);
            return -1;
        }
        break;
    case 4:
    case 5:
#if JSON_SAX_APIS_SUPPORT
    case 7:
#endif
        break;
    default:
        usage_print(argv[0]);
        return -1;
    }

    switch(choice)
    {
    case 1:
        json = json_parse_str(orig_data, orig_size);
        break;
    case 2:
        json = json_fast_parse_str(orig_data, orig_size, &mem);
        fast_flag = 1;
        break;
    case 3:
        json = json_reuse_parse_str(orig_data, orig_size, &mem);
        fast_flag = 1;
        break;
    case 4:
        json = json_parse_file(file);
        break;
    case 5:
        json = json_fast_parse_file(file, &mem);
        fast_flag = 1;
        break;
#if JSON_SAX_APIS_SUPPORT
    case 6:
        snprintf(s_dst_json_path, sizeof(s_dst_json_path), "%s-%d.format.json", file, choice);
        if (json_sax_parse_str(orig_data, orig_size, _sax_parser_cb) < 0) {
            printf("json_sax_parse_str failed!\n");
            read_file_data_free(&orig_data, &orig_size);
            return -1;
        }
        ms2 = _system_ms_get();
        goto end;
    case 7:
        s_sax_for_file = true;
        snprintf(s_dst_json_path, sizeof(s_dst_json_path), "%s-%d.format.json", file, choice);
        if (json_sax_parse_file(file, _sax_parser_cb) < 0) {
            printf("json_sax_parse_file failed!\n");
            return -1;
        }
        ms2 = _system_ms_get();
        goto end;
#endif
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
    switch(choice)
    {
    case 2:
    case 3:
    case 5:
        item_total = pjson_memory_statistics(&mem.obj_mgr) >> 6;
        break;
    default:
        break;
    }

    snprintf(s_dst_json_path, sizeof(s_dst_json_path), "%s-%d.format.json", file, choice);
    switch(choice)
    {
    case 1:
        print_str = json_print_format(json, item_total, &print_size);
        break;
    case 2:
        print_str = json_print_format(json, item_total, &print_size);
        break;
    case 3:
        print_str = json_print_format(json, item_total, &print_size);
        break;
    case 4:
        json_fprint_format(json, item_total, s_dst_json_path);
        break;
    case 5:
        json_fprint_format(json, item_total, s_dst_json_path);
        break;
    default:
        break;
    }
    if (print_str) {
        copy_data_to_file(print_str, print_size, s_dst_json_path);
        json_memory_free(print_str);
        print_str = NULL;
    }
    ms3 = _system_ms_get();

    sleep(1);
    ms4 = _system_ms_get();
    snprintf(s_dst_json_path, sizeof(s_dst_json_path), "%s-%d.unformat.json", file, choice);
    switch(choice)
    {
    case 1:
        print_str = json_print_unformat(json, item_total, &print_size);
        break;
    case 2:
        print_str = json_print_unformat(json, item_total, &print_size);
        break;
    case 3:
        print_str = json_print_unformat(json, item_total, &print_size);
        break;
    case 4:
        json_fprint_unformat(json, item_total, s_dst_json_path);
        break;
    case 5:
        json_fprint_unformat(json, item_total, s_dst_json_path);
        break;
    default:
        break;
    }
    if (print_str) {
        copy_data_to_file(print_str, print_size, s_dst_json_path);
        json_memory_free(print_str);
        print_str = NULL;
    }
    ms5 = _system_ms_get();

#if JSON_SAX_APIS_SUPPORT
end:
#endif
    if (!fast_flag && json)
        json_del_object(json);
    pjson_memory_free(&mem);
    if (orig_data)
        read_file_data_free(&orig_data, &orig_size);

    if (choice < 6)
        printf("parse ms=%d, format print_ms=%d, unformat print_ms=%d\n", ms2-ms1, ms3-ms2, ms5-ms4);
    else
        printf("sax parse+print ms=%d\n", ms2-ms1);
    return 0;
}
