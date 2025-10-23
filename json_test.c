#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "json.h"

#if !defined(_MSC_VER)
#include <unistd.h>
#else
#include <windows.h>
#include <io.h>
#define access                          _access
#define unlink                          _unlink
#define F_OK                            0
#pragma warning(disable: 4996)
#endif

#define _fmalloc       malloc
#define _ffree         free
#define _fclose_fp(fp) do {if (fp) fclose(fp); fp = NULL; } while(0)
#define _free_ptr(ptr) do {if (ptr) _ffree(ptr); ptr = NULL; } while(0)

static char s_dst_json_path[256] = {0};
static char *s_print_str = NULL;
static size_t s_print_size = 0;

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

static unsigned long long _system_ns_get(void)
{
#if !defined(_MSC_VER)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000 + ts.tv_nsec;
#else
    static LARGE_INTEGER freq = {0};
    static int freq_initialized = 0;
    if (!freq_initialized) {
        QueryPerformanceFrequency(&freq);
        freq_initialized = 1;
    }

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (unsigned long long)((counter.QuadPart * 1000000000ULL) / freq.QuadPart);
#endif
}

int copy_data_to_file(char *data, size_t size, const char *dst)
{
    int ret = -1;
    FILE *wfp = NULL;

    if (!data || !dst)
        return -1;
    if ((wfp = fopen(dst, "wb+")) == NULL)
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

    if ((rfp = fopen(src, "rb")) == NULL)
        return -1;
    fseek(rfp, 0, SEEK_END);
    *size = ftell(rfp);
    fseek(rfp, 0, SEEK_SET);
    if (*size == 0)
        goto err;

    if ((*data = (char *)_fmalloc(*size + 1)) == NULL)
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

static void usage_print(const char *func)
{
#define ANY_JSON_NAME "x.json"
    printf("Usage:  %s <%s> <num>\n", func, ANY_JSON_NAME);
    printf("\t%s %s  1 ==> test json_parse_str()\n", func, ANY_JSON_NAME);
    printf("\t%s %s  2 ==> test json_fast_parse_str()\n", func, ANY_JSON_NAME);
    printf("\t%s %s  3 ==> test json_reuse_parse_str()\n", func, ANY_JSON_NAME);
    printf("\t%s %s  4 ==> test json_parse_file()\n", func, ANY_JSON_NAME);
    printf("\t%s %s  5 ==> test json_fast_parse_file()\n", func, ANY_JSON_NAME);
#if JSON_SAX_APIS_SUPPORT
    printf("\t%s %s  6 ==> test json_sax_parse_str()\n", func, ANY_JSON_NAME);
    printf("\t%s %s  7 ==> test json_sax_parse_file()\n", func, ANY_JSON_NAME);
#endif
    printf("\t%s %s >7 ==> test json_reuse_parse_str() <num> times\n", func, ANY_JSON_NAME);
#if JSON_SAX_APIS_SUPPORT
    printf("Usage:  %s 0 ==> test json_sax_print_xxxx()\n", func);
#endif
}

#if JSON_SAX_APIS_SUPPORT
int test_json_sax_print(void)
{
    json_sax_print_hd handle = NULL;
    json_string_t jkey = {0}, jstr = {0};
    json_binary_t jbin = {0};
    char narray[8] = {1, 2, 3, 4, 5, 6, 7, 8};

    handle = json_sax_print_format_start(10, NULL);
    json_sax_print_object_start(handle, NULL);

    jkey.str = (char *)"Name", jkey.info.len = 4;
    jstr.str = (char *)"LengJing", jstr.info.len = 8;
    json_sax_print_object_item(handle, &jkey, &jstr);

    jkey.str = (char *)"Age", jkey.info.len = 3;
    json_sax_print_object_item(handle, &jkey, 30);

    jkey.str = (char *)"Phone", jkey.info.len = 5;
    jstr.str = (char *)"18368887550", jstr.info.len = 11;
    json_sax_print_object_item(handle, &jkey, &jstr);

    jkey.str = (char *)"Binary", jkey.info.len = 6;
    jbin.bin = (void *)narray, jbin.info.len = 8;
    json_sax_print_object_item(handle, &jkey, &jbin);

    jkey.str = (char *)"Hobby", jkey.info.len = 5;
    json_sax_print_array_start(handle, &jkey);
    jstr.str = (char *)"Reading", jstr.info.len = 7;
    json_sax_print_array_item(handle, &jstr);
    jstr.str = (char *)"Walking", jstr.info.len = 7;
    json_sax_print_array_item(handle, &jstr);
    jstr.str = (char *)"Thinking", jstr.info.len = 8;
    json_sax_print_array_item(handle, &jstr);
    json_sax_print_array_finish(handle);

    json_sax_print_object_finish(handle);
    s_print_str = json_sax_print_finish(handle, NULL, NULL);
    printf("%s\n", s_print_str);
    json_memory_free(s_print_str);

    return 0;
}

static bool s_sax_for_file = false;
static int s_sax_item_num = 0;
json_sax_ret_t _sax_parser_cb(json_sax_parser_t *parser)
{
    static json_sax_print_hd handle = NULL;
    json_string_t *jkey = &parser->array[parser->index];

    if (parser->index == 0) {
        switch (parser->array[parser->index].info.type) {
        case JSON_ARRAY:
        case JSON_OBJECT:
            if (parser->value.vcmd == JSON_SAX_START) {
                if (s_sax_for_file)
                    handle = json_sax_fprint_format_start(s_sax_item_num, s_dst_json_path, NULL);
                else
                    handle = json_sax_print_format_start(s_sax_item_num, NULL);
            }
            break;
        default:
            if (s_sax_for_file)
                handle = json_sax_fprint_format_start(s_sax_item_num, s_dst_json_path, NULL);
            else
                handle = json_sax_print_format_start(s_sax_item_num, NULL);
            break;
        }
    }

    switch (parser->array[parser->index].info.type) {
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
    case JSON_LINT:
        json_sax_print_lint(handle, jkey, parser->value.vnum.vlint);
        break;
    case JSON_LHEX:
        json_sax_print_lhex(handle, jkey, parser->value.vnum.vlhex);
        break;
    case JSON_DOUBLE:
        json_sax_print_double(handle, jkey, parser->value.vnum.vdbl);
        break;
    case JSON_STRING:
        json_sax_print_string(handle, jkey, &parser->value.vstr);
        break;
    case JSON_BINARY:
        json_sax_print_binary(handle, jkey, &parser->value.vbin);
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
        switch (parser->array[parser->index].info.type) {
        case JSON_ARRAY:
        case JSON_OBJECT:
            if (parser->value.vcmd == JSON_SAX_FINISH) {
                if (s_sax_for_file) {
                    json_sax_print_finish(handle, NULL, NULL);
                } else {
                    s_print_str = json_sax_print_finish(handle, &s_print_size, NULL);
                }
            }
            break;
        default:
            if (s_sax_for_file) {
                json_sax_print_finish(handle, NULL, NULL);
            } else {
                s_print_str = json_sax_print_finish(handle, &s_print_size, NULL);
            }
            break;
        }
    }

    return JSON_SAX_PARSE_CONTINUE;
}
#endif

int test_reuse_parse_repeat(const char *file, int times)
{
    int i = 0;
    size_t size = 0;
    char *data = NULL, *bak = NULL;
    json_object *json = NULL;
    json_mem_t mem = {0};
    unsigned long long total_ns = 0, last_ns = 0;

    pjson_memory_init(&mem);

    if (read_file_to_data(file, &data, &size) < 0) {
        printf("read file %s failed!\n", file);
        return -1;
    }

    bak = (char *)malloc(size + 1);
    if (!bak) {
        printf("malloc (%u) failed!\n", (unsigned int)(size + 1));
        read_file_data_free(&data, &size);
        return -1;
    }

    /*
     * obj_mgr needs to be set to a suitable value so that one memory block
     * can store all objects.  If the original string is not reused, the other
     * two mgrs also need to be set.
     * */
    mem.valid = 1;
    mem.obj_mgr.mem_size = size;

    for (i = 0; i < times; ++i) {
        memcpy(bak, data, size + 1);
        last_ns = _system_ns_get();
        json = json_reuse_parse_str(bak, size, &mem);
        (void)json; // prevent warning
        pjson_memory_refresh(&mem);
        total_ns += _system_ns_get() - last_ns;
    }

    printf("Repeat %d times, LJSON reuse parse costs %llu ns\n", times, total_ns);
    free(bak);
    pjson_memory_free(&mem);
    read_file_data_free(&data, &size);

    return 0;
}

int main(int argc, char *argv[])
{
    int ret = 0;
    size_t size = 0;
    int nitem = 0, nflag = 0;
    int choice = 0;
    char *file = NULL;
    char *data = NULL;
    json_object *json = NULL;
    json_mem_t mem = {0};
    unsigned int ms[8] = {0};

    pjson_memory_init(&mem);

#if JSON_SAX_APIS_SUPPORT
    if (argc == 2) {
        choice = atoi(argv[1]);
        if (choice == 0) {
            test_json_sax_print();
            return 0;
        } else {
            goto err;
        }
    }
#endif
    if (argc != 3)
        goto err;

    file = argv[1];
    if (strlen(file) == 0 || access(file, F_OK) != 0) {
        printf("%s is not exist!\n", file);
        goto err;
    }
    choice = atoi(argv[2]);
    if (choice < 1)
        goto err;

    if (choice > 7) {
        return test_reuse_parse_repeat(file, choice);
    }

    ms[0] = _system_ms_get();
    switch(choice)
    {
    case 1: case 2: case 3:
#if JSON_SAX_APIS_SUPPORT
    case 6:
#endif
        if (read_file_to_data(file, &data, &size) < 0) {
            printf("read file %s failed!\n", file);
            goto err;
        }
        break;
    default:
        break;
    }

    ms[1] = _system_ms_get();
    switch(choice)
    {
    case 1: json = json_parse_str(data, size); break;
    case 2: json = json_fast_parse_str(data, size, &mem); nflag = 1; break;
    case 3: json = json_reuse_parse_str(data, size, &mem); nflag = 1; break;
    case 4: json = json_parse_file(file); break;
    case 5: json = json_fast_parse_file(file, &mem); nflag = 1; break;

#if JSON_SAX_APIS_SUPPORT
    case 6:
        s_sax_item_num = (int)(size >> 5);
        if (json_sax_parse_str(data, size, _sax_parser_cb) < 0) {
            printf("json_sax_parse_str failed!\n");
            ret = -1;
            goto end;
        }

        ms[2] = _system_ms_get();
        if (s_print_str) {
            snprintf(s_dst_json_path, sizeof(s_dst_json_path), "%s-%d.format.json", file, choice);
            copy_data_to_file(s_print_str, s_print_size, s_dst_json_path);
            json_memory_free(s_print_str);
            s_print_str = NULL;
        }

        ms[3] = _system_ms_get();
        printf("[sax] %d. read=%-5d parse+format=%-5d       write=%-5d\n", choice, ms[1]-ms[0], ms[2]-ms[1], ms[3]-ms[2]);
        goto end;
    case 7:
        s_sax_for_file = true;
        snprintf(s_dst_json_path, sizeof(s_dst_json_path), "%s-%d.format.json", file, choice);
        if (json_sax_parse_file(file, _sax_parser_cb) < 0) {
            printf("json_sax_parse_file failed!\n");
            ret = -1;
            goto end;
        }

        ms[2] = _system_ms_get();
        printf("[sax] %d. read+parse+format+write=%-5d\n", choice, ms[2]-ms[1]);
        goto end;
#endif
    default:
        break;
    }

    if (!json) {
        printf("json parse failed!\n");
        ret = -1;
        goto end;
    }

    ms[2] = _system_ms_get();
    snprintf(s_dst_json_path, sizeof(s_dst_json_path), "%s-%d.format.json", file, choice);
    if (nflag)
        nitem = pjson_memory_statistics(&mem.obj_mgr) / sizeof(json_object);
    else
        nitem = (int)(size >> 5);
    switch(choice)
    {
    case 1: case 2: case 3:
        s_print_str = json_print_format(json, nitem, &s_print_size, NULL);
        break;
    case 4: case 5:
        json_fprint_format(json, nitem, s_dst_json_path, NULL);
        break;
    default:
        break;
    }

    ms[3] = _system_ms_get();
    if (s_print_str) {
        copy_data_to_file(s_print_str, s_print_size, s_dst_json_path);
        json_memory_free(s_print_str);
        s_print_str = NULL;
    }
    ms[4] = _system_ms_get();

    //sleep(1);

    ms[5] = _system_ms_get();
    snprintf(s_dst_json_path, sizeof(s_dst_json_path), "%s-%d.unformat.json", file, choice);
    switch(choice)
    {
    case 1: case 2: case 3:
        s_print_str = json_print_unformat(json, nitem, &s_print_size, NULL);
        break;
    case 4: case 5:
        json_fprint_unformat(json, nitem, s_dst_json_path, NULL);
        break;
    default:
        break;
    }

    ms[6] = _system_ms_get();
    if (s_print_str) {
        copy_data_to_file(s_print_str, s_print_size, s_dst_json_path);
        json_memory_free(s_print_str);
        s_print_str = NULL;
    }
    ms[7] = _system_ms_get();

    switch(choice)
    {
    case 1: case 2: case 3:
        printf("[dom] %d. read=%-5d parse=%-5d format=%-5d write=%-5d unformat=%-5d write=%-5d\n",
            choice, ms[1]-ms[0], ms[2]-ms[1], ms[3]-ms[2], ms[4]-ms[3], ms[6]-ms[5], ms[7]-ms[6]);
        break;
    case 4: case 5:
        printf("[dom] %d. read+parse=%-5d       format+write=%-5d       unformat+write=%-5d\n",
            choice, ms[2]-ms[1], ms[3]-ms[2], ms[6]-ms[5]);
        break;
    default:
        break;
    }

    if (!nflag && json)
        json_del_object(json);
    pjson_memory_free(&mem);
end:
    if (data)
        read_file_data_free(&data, &size);
    return ret;

err:
    usage_print(argv[0]);
    return -1;
}
