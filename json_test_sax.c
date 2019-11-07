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

int test_case1(void)
{
    json_sax_print_handle handle = NULL;
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

json_sax_parse_ret json_sax_parser_callback_test2(json_sax_parser *parser)
{
    static json_sax_print_handle handle = NULL;
    char *key = NULL;
    int alloc_flag = 0;
    json_sax_parse_depth_t *depth = &parser->array[parser->count-1];
    if (depth->key.alloc == 0 && depth->key.str != NULL) {
        key = malloc(depth->key.len + 1);
        memcpy(key, depth->key.str, depth->key.len);
        key[depth->key.len] = 0;
        alloc_flag = 1;
    } else {
        key = depth->key.str;
    }

    switch (parser->array[parser->count-1].type) {
        case JSON_NULL:
            json_sax_print_null(handle, key);
            break;
        case JSON_BOOL:
            json_sax_print_bool(handle, key, parser->value.vint);
            break;
        case JSON_INT:
            json_sax_print_int(handle, key, parser->value.vint);
            break;
        case JSON_HEX:
            json_sax_print_hex(handle, key, parser->value.vhex);
            break;
        case JSON_DOUBLE:
            json_sax_print_double(handle, key, parser->value.vdbl);
            break;
        case JSON_STRING:
            if (parser->value.vstr.alloc == 0 && parser->value.vstr.str != NULL) {
                char *str = NULL;
                str = malloc(parser->value.vstr.len + 1);
                memcpy(str, parser->value.vstr.str, parser->value.vstr.len);
                str[parser->value.vstr.len] = 0;
                json_sax_print_string(handle, key, str);
                free(str);
            } else {
                json_sax_print_string(handle, key, parser->value.vstr.str);
            }
            break;
        case JSON_ARRAY:
            if (parser->count == 1) {
                if (parser->value.vcmd == JSON_SAX_START) {
                    handle = json_sax_fprint_format_start("sax_out.json");
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
                    handle = json_sax_fprint_format_start("sax_out.json");
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

    if (alloc_flag)
        free(key);
    return JSON_SAX_PARSE_CONTINUE;
}

int test_case2(const char *src)
{
    return json_sax_parse_file(src, json_sax_parser_callback_test2);
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("usage: %s filename\n", argv[0]);
    }
    test_case2(argv[1]);
    return 0;
}

