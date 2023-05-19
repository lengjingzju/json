/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2019-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* URL: https://github.com/lengjingzju/json *
*******************************************/

#ifndef __JSON_H__
#define __JSON_H__
#include <stdbool.h>
#include <stdlib.h>

#define JSON_VERSION                    0x010007
#define JSON_SAX_APIS_SUPPORT           1
#define JSON_LONG_LONG_SUPPORT          1

/******** json object structure ********/

struct json_list_head {
    struct json_list_head *next, *prev;
};

typedef enum {
    JSON_NULL = 0,
    JSON_BOOL,
    JSON_INT,
    JSON_HEX,
#if JSON_LONG_LONG_SUPPORT
    JSON_LINT,
    JSON_LHEX,
#endif
    JSON_DOUBLE,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} json_type_t;

typedef union {
    bool vbool;
    int vint;
    unsigned int vhex;
#if JSON_LONG_LONG_SUPPORT
    long long int vlint;
    unsigned long long int vlhex;
#endif
    double vdbl;
} json_number_t;

typedef union {
    json_number_t vnum;
    char *vstr;
    struct json_list_head head;
} json_value_t;

typedef struct {
    struct json_list_head list;
    char *key;
    json_type_t type;
    json_value_t value;
} json_object;

/******** json classic editing mode APIs ********/

/* basic json apis */
int json_item_total_get(json_object *json);

/* create apis */
void json_del_object(json_object *json);
json_object *json_new_object(json_type_t type);
json_object *json_create_item(json_type_t type, void *value);

static inline json_object *json_create_null(void)
{
    return json_new_object(JSON_NULL);
}

static inline json_object *json_create_bool(bool value)
{
    return json_create_item(JSON_BOOL, &value);
}

static inline json_object *json_create_int(int value)
{
    return json_create_item(JSON_INT, &value);
}

static inline json_object *json_create_hex(unsigned int value)
{
    return json_create_item(JSON_HEX, &value);
}

#if JSON_LONG_LONG_SUPPORT
static inline json_object *json_create_lint(long long int value)
{
    return json_create_item(JSON_LINT, &value);
}

static inline json_object *json_create_lhex(unsigned long long int value)
{
    return json_create_item(JSON_LHEX, &value);
}
#endif

static inline json_object *json_create_double(double value)
{
    return json_create_item(JSON_DOUBLE, &value);
}

static inline json_object *json_create_string(const char *value)
{
    return json_create_item(JSON_STRING, &value);
}

static inline json_object *json_create_array(void)
{
    return json_new_object(JSON_ARRAY);
}

static inline json_object *json_create_object(void)
{
    return json_new_object(JSON_OBJECT);
}

/* create json array apis */
json_object *json_create_item_array(json_type_t type, void *values, int count);

static inline json_object *json_create_bool_array(bool *values, int count)
{
    return json_create_item_array(JSON_BOOL, values, count);
}

static inline json_object *json_create_int_array(int *values, int count)
{
    return json_create_item_array(JSON_INT, values, count);
}

static inline json_object *json_create_hex_array(unsigned int *values, int count)
{
    return json_create_item_array(JSON_HEX, values, count);
}

#if JSON_LONG_LONG_SUPPORT
static inline json_object *json_create_lint_array(long long int *values, int count)
{
    return json_create_item_array(JSON_LINT, values, count);
}

static inline json_object *json_create_lhex_array(unsigned long long int *values, int count)
{
    return json_create_item_array(JSON_LHEX, values, count);
}
#endif

static inline json_object *json_create_double_array(double *values, int count)
{
    return json_create_item_array(JSON_DOUBLE, values, count);
}

static inline json_object *json_create_string_array(char **values, int count)
{
    return json_create_item_array(JSON_STRING, values, count);
}

/* setting apis */
int json_set_key(json_object *json, const char *key);
int json_set_string_value(json_object *json, const char *str);
int json_get_number_value(json_object *json, json_type_t type, void *value);
int json_set_number_value(json_object *json, json_type_t type, void *value);

static inline bool json_get_bool_value(json_object *json)
{
    bool value = 0;
    json_get_number_value(json, JSON_BOOL, &value);
    return value;
}

static inline int json_get_int_value(json_object *json)
{
    int value = 0;
    json_get_number_value(json, JSON_INT, &value);
    return value;
}

static inline unsigned int json_get_hex_value(json_object *json)
{
    unsigned int value = 0;
    json_get_number_value(json, JSON_HEX, &value);
    return value;
}

#if JSON_LONG_LONG_SUPPORT
static inline long long int json_get_lint_value(json_object *json)
{
    long long int value = 0;
    json_get_number_value(json, JSON_LINT, &value);
    return value;
}

static inline unsigned long long int json_get_lhex_value(json_object *json)
{
    unsigned long long int value = 0;
    json_get_number_value(json, JSON_LHEX, &value);
    return value;
}
#endif

static inline double json_get_double_value(json_object *json)
{
    double value = 0;
    json_get_number_value(json, JSON_DOUBLE, &value);
    return value;
}

static inline int json_set_bool_value(json_object *json, bool value)
{
    return json_set_number_value(json, JSON_BOOL, &value);
}

static inline int json_set_int_value(json_object *json, int value)
{
    return json_set_number_value(json, JSON_INT, &value);
}

static inline int json_set_hex_value(json_object *json, unsigned int value)
{
    return json_set_number_value(json, JSON_HEX, &value);
}

#if JSON_LONG_LONG_SUPPORT
static inline int json_set_lint_value(json_object *json, long long int value)
{
    return json_set_number_value(json, JSON_LINT, &value);
}

static inline int json_set_lhex_value(json_object *json, unsigned long long int value)
{
    return json_set_number_value(json, JSON_LHEX, &value);
}
#endif

static inline int json_set_double_value(json_object *json, double value)
{
    return json_set_number_value(json, JSON_DOUBLE, &value);
}

/* array/object apis */
int json_get_array_size(json_object *json);
json_object *json_get_array_item(json_object *json, int seq);
json_object *json_get_object_item(json_object *json, const char *key);

json_object *json_detach_item_from_array(json_object *json, int seq);
json_object *json_detach_item_from_object(json_object *json, const char *key);

int json_del_item_from_array(json_object *json, int seq);
int json_del_item_from_object(json_object *json, const char *key);

int json_replace_item_in_array(json_object *array, int seq, json_object *new_item);
int json_replace_item_in_object(json_object *object, const char *key, json_object *new_item);

int json_add_item_to_array(json_object *array, json_object *item);
int json_add_item_to_object(json_object *object, const char *key, json_object *item);

json_object *json_deepcopy(json_object *json);
int json_copy_item_to_array(json_object *array, json_object *item);
int json_copy_item_to_object(json_object *object, const char *key, json_object *item);

/* create new item and add it to object apis */
int json_add_new_item_to_object(json_object *object, json_type_t type, const char *key, void* value);

static inline int json_add_null_to_object(json_object *object, const char *key)
{
    return json_add_new_item_to_object(object, JSON_NULL, key, NULL);
}

static inline int json_add_bool_to_object(json_object *object, const char *key, bool value)
{
    return json_add_new_item_to_object(object, JSON_BOOL, key, &value);
}

static inline int json_add_int_to_object(json_object *object, const char *key, int value)
{
    return json_add_new_item_to_object(object, JSON_INT, key, &value);
}

static inline int json_add_hex_to_object(json_object *object, const char *key, unsigned int value)
{
    return json_add_new_item_to_object(object, JSON_HEX, key, &value);
}

#if JSON_LONG_LONG_SUPPORT
static inline int json_add_lint_to_object(json_object *object, const char *key, long long int value)
{
    return json_add_new_item_to_object(object, JSON_LINT, key, &value);
}

static inline int json_add_lhex_to_object(json_object *object, const char *key, unsigned long long int value)
{
    return json_add_new_item_to_object(object, JSON_LHEX, key, &value);
}
#endif

static inline int json_add_double_to_object(json_object *object, const char *key, double value)
{
    return json_add_new_item_to_object(object, JSON_DOUBLE, key, &value);
}

static inline int json_add_string_to_object(json_object *object, const char *key, const char *value)
{
    return json_add_new_item_to_object(object, JSON_STRING, key, &value);
}

/******** json memory pool structure ********/

typedef struct {
    struct json_list_head list;
    size_t size;
    char *ptr;
    char *cur;
} json_mem_node_t;

typedef struct {
    struct json_list_head head;
    size_t mem_size;
    json_mem_node_t *cur_node;
} json_mem_mgr_t;

typedef struct {
    json_mem_mgr_t obj_mgr;
    json_mem_mgr_t key_mgr;
    json_mem_mgr_t str_mgr;
} json_mem_t;

/******** json memory pool editing mode APIs ********/

// call pjson_memory_init before use, call pjson_memory_free after use.
void pjson_memory_free(json_mem_t *mem);
void pjson_memory_init(json_mem_t *mem);

int pjson_set_key(json_object *json, const char *key, json_mem_t *mem);
int pjson_set_string_value(json_object *json, const char *str, json_mem_t *mem);

/* create json apis */
json_object *pjson_new_object(json_type_t type, json_mem_t *mem);
json_object *pjson_create_item(json_type_t type, void *value, json_mem_t *mem);

static inline json_object *pjson_create_null(json_mem_t *mem)
{
    return pjson_new_object(JSON_NULL, mem);
}

static inline json_object *pjson_create_bool(bool value, json_mem_t *mem)
{
    return pjson_create_item(JSON_BOOL, &value, mem);
}

static inline json_object *pjson_create_int(int value, json_mem_t *mem)
{
    return pjson_create_item(JSON_INT, &value, mem);
}

static inline json_object *pjson_create_hex(unsigned int value, json_mem_t *mem)
{
    return pjson_create_item(JSON_HEX, &value, mem);
}

#if JSON_LONG_LONG_SUPPORT
static inline json_object *pjson_create_lint(long long int value, json_mem_t *mem)
{
    return pjson_create_item(JSON_LINT, &value, mem);
}

static inline json_object *pjson_create_lhex(unsigned long long int value, json_mem_t *mem)
{
    return pjson_create_item(JSON_LHEX, &value, mem);
}
#endif

static inline json_object *pjson_create_double(double value, json_mem_t *mem)
{
    return pjson_create_item(JSON_DOUBLE, &value, mem);
}

static inline json_object *pjson_create_string(const char *value, json_mem_t *mem)
{
    return pjson_create_item(JSON_STRING, &value, mem);
}

static inline json_object *pjson_create_array(json_mem_t *mem)
{
    return pjson_new_object(JSON_ARRAY, mem);
}

static inline json_object *pjson_create_object(json_mem_t *mem)
{
    return pjson_new_object(JSON_OBJECT, mem);
}

/* create json array apis */
json_object *pjson_create_item_array(json_type_t item_type, void *values, int count, json_mem_t *mem);

static inline json_object *pjson_create_bool_array(bool *values, int count, json_mem_t *mem)
{
    return pjson_create_item_array(JSON_BOOL, values, count, mem);
}

static inline json_object *pjson_create_int_array(int *values, int count, json_mem_t *mem)
{
    return pjson_create_item_array(JSON_INT, values, count, mem);
}

static inline json_object *pjson_create_hex_array(unsigned int *values, int count, json_mem_t *mem)
{
    return pjson_create_item_array(JSON_HEX, values, count, mem);
}

#if JSON_LONG_LONG_SUPPORT
static inline json_object *pjson_create_lint_array(long long int *values, int count, json_mem_t *mem)
{
    return pjson_create_item_array(JSON_LINT, values, count, mem);
}

static inline json_object *pjson_create_lhex_array(unsigned long long int *values, int count, json_mem_t *mem)
{
    return pjson_create_item_array(JSON_LHEX, values, count, mem);
}
#endif

static inline json_object *pjson_create_double_array(double *values, int count, json_mem_t *mem)
{
    return pjson_create_item_array(JSON_DOUBLE, values, count, mem);
}

static inline json_object *pjson_create_string_array(char **values, int count, json_mem_t *mem)
{
    return pjson_create_item_array(JSON_STRING, values, count, mem);
}

/* some array/object apis */

int pjson_add_item_to_object(json_object *object, const char *key, json_object *item, json_mem_t *mem);
int pjson_add_new_item_to_object(json_object *object, json_type_t type, const char *key, void *value, json_mem_t *mem);

static inline int pjson_add_null_to_object(json_object *object, const char *key, json_mem_t *mem)
{
    return pjson_add_new_item_to_object(object, JSON_NULL, key, NULL, mem); }

static inline int pjson_add_bool_to_object(json_object *object, const char *key, bool value, json_mem_t *mem)
{
    return pjson_add_new_item_to_object(object, JSON_BOOL, key, &value, mem);
}

static inline int pjson_add_int_to_object(json_object *object, const char *key, int value, json_mem_t *mem)
{
    return pjson_add_new_item_to_object(object, JSON_INT, key, &value, mem);
}

static inline int pjson_add_hex_to_object(json_object *object, const char *key, unsigned int value, json_mem_t *mem)
{
    return pjson_add_new_item_to_object(object, JSON_HEX, key, &value, mem);
}

#if JSON_LONG_LONG_SUPPORT
static inline int pjson_add_lint_to_object(json_object *object, const char *key, long long int value, json_mem_t *mem)
{
    return pjson_add_new_item_to_object(object, JSON_LINT, key, &value, mem);
}

static inline int pjson_add_lhex_to_object(json_object *object, const char *key, unsigned long long int value, json_mem_t *mem)
{
    return pjson_add_new_item_to_object(object, JSON_LHEX, key, &value, mem);
}
#endif

static inline int pjson_add_double_to_object(json_object *object, const char *key, double value, json_mem_t *mem)
{
    return pjson_add_new_item_to_object(object, JSON_DOUBLE, key, &value, mem);
}

static inline int pjson_add_string_to_object(json_object *object, const char *key, const char *value, json_mem_t *mem)
{
    return pjson_add_new_item_to_object(object, JSON_STRING, key, &value, mem);
}

/******** json dom print/parse apis ********/

typedef struct {
    size_t str_len; /* return string length if it is printed to string. */
    size_t plus_size;
    size_t item_size;
    int item_total;
    bool format_flag;
    const char *path;
} json_print_choice_t;

typedef struct {
    size_t mem_size;
    size_t read_size;
    size_t str_len;
    bool reuse_flag;
    json_mem_t *mem;
    const char *path;
    char *str;
} json_parse_choice_t;

char *json_print_common(json_object *json, json_print_choice_t *choice);
void json_free_print_ptr(void *ptr);
json_object *json_parse_common(json_parse_choice_t *choice);

static inline char *json_print_format(json_object *json, size_t *length)
{
    json_print_choice_t choice = {0};
    char *str = NULL;

    choice.format_flag = true;
    str = json_print_common(json, &choice);
    if (length)
        *length = choice.str_len;
    return str;
}

static inline char *json_print_unformat(json_object *json, size_t *length)
{
    json_print_choice_t choice = {0};
    char *str = NULL;

    choice.format_flag = false;
    str = json_print_common(json, &choice);
    if (length)
        *length = choice.str_len;
    return str;
}

static inline char *json_fprint_format(json_object *json, const char *path)
{
    json_print_choice_t choice = {0};

    choice.format_flag = true;
    choice.path = path;
    return json_print_common(json, &choice);
}

static inline char *json_fprint_unformat(json_object *json, const char *path)
{
    json_print_choice_t choice = {0};

    choice.format_flag = false;
    choice.path = path;
    return json_print_common(json, &choice);
}

static inline json_object *json_parse_str(char *str)
{
    json_parse_choice_t choice = {0};
    choice.str = str;
    return json_parse_common(&choice);
}

static inline json_object *json_fast_parse_str(char *str, json_mem_t *mem, size_t str_len)
{
    json_parse_choice_t choice = {0};
    choice.str = str;
    choice.mem = mem;
    choice.str_len = str_len;
    return json_parse_common(&choice);
}

static inline json_object *json_reuse_parse_str(char *str, json_mem_t *mem, size_t str_len)
{
    json_parse_choice_t choice = {0};
    choice.str = str;
    choice.mem = mem;
    choice.str_len = str_len;
    choice.reuse_flag = true;
    return json_parse_common(&choice);
}

static inline json_object *json_parse_file(const char *path)
{
    json_parse_choice_t choice = {0};
    choice.path = path;
    return json_parse_common(&choice);
}

static inline json_object *json_fast_parse_file(const char *path, json_mem_t *mem)
{
    json_parse_choice_t choice = {0};
    choice.path = path;
    choice.mem = mem;
    return json_parse_common(&choice);
}

/**** json sax print/parse apis ****/

#if JSON_SAX_APIS_SUPPORT

typedef struct {
    int alloc;
    size_t len;
    char *str;
} json_sax_str_t;

typedef enum {
    JSON_SAX_START = 0,
    JSON_SAX_FINISH
} json_sax_cmd_t;

typedef union {
    json_number_t vnum;
    json_sax_str_t vstr;
    json_sax_cmd_t vcmd;            /* for array and object */
} json_sax_value_t;

typedef struct {
    json_type_t type;
    json_sax_str_t key;
} json_sax_depth_t;

typedef enum {
    JSON_SAX_PARSE_CONTINUE = 0,
    JSON_SAX_PARSE_STOP
} json_sax_ret_t;

typedef struct {
    int total;
    int count;
    json_sax_depth_t *array;
    json_sax_value_t value;
} json_sax_parser_t;

typedef json_sax_ret_t (*json_sax_cb_t)(json_sax_parser_t *parser);

typedef struct {
    size_t read_size;
    size_t str_len;
    char *str;
    const char *path;
    json_sax_cb_t cb;
} json_sax_parse_choice_t;

typedef void* json_sax_print_hd;
int json_sax_print_value(json_sax_print_hd handle, json_type_t type, const char *key, const void *value);
json_sax_print_hd json_sax_print_start(json_print_choice_t *choice);
char *json_sax_print_finish(json_sax_print_hd handle, size_t *length);
int json_sax_parse_common(json_sax_parse_choice_t *choice);

static inline int json_sax_print_null(json_sax_print_hd handle, const char *key)
{
    return json_sax_print_value(handle, JSON_NULL, key, NULL);
}

static inline int json_sax_print_bool(json_sax_print_hd handle, const char *key, bool value)
{
    return json_sax_print_value(handle, JSON_BOOL, key, &value);
}

static inline int json_sax_print_int(json_sax_print_hd handle, const char *key, int value)
{
    return json_sax_print_value(handle, JSON_INT, key, &value);
}

static inline int json_sax_print_hex(json_sax_print_hd handle, const char *key, unsigned int value)
{
    return json_sax_print_value(handle, JSON_HEX, key, &value);
}

#if JSON_LONG_LONG_SUPPORT
static inline int json_sax_print_lint(json_sax_print_hd handle, const char *key, long long int value)
{
    return json_sax_print_value(handle, JSON_LINT, key, &value);
}

static inline int json_sax_print_lhex(json_sax_print_hd handle, const char *key, unsigned long long int value)
{
    return json_sax_print_value(handle, JSON_LHEX, key, &value);
}
#endif

static inline int json_sax_print_double(json_sax_print_hd handle, const char *key, double value)
{
    return json_sax_print_value(handle, JSON_DOUBLE, key, &value);
}

static inline int json_sax_print_string(json_sax_print_hd handle, const char *key, const char *value)
{
    return json_sax_print_value(handle, JSON_STRING, key, value);
}

static inline int json_sax_print_array(json_sax_print_hd handle, const char *key, json_sax_cmd_t value)
{
    return json_sax_print_value(handle, JSON_ARRAY, key, &value);
}

static inline int json_sax_print_object(json_sax_print_hd handle, const char *key, json_sax_cmd_t value)
{
    return json_sax_print_value(handle, JSON_OBJECT, key, &value);
}

static inline json_sax_print_hd json_sax_print_format_start(int item_total)
{
    json_print_choice_t choice = {0};

    choice.format_flag = true;
    choice.item_total = item_total;
    return json_sax_print_start(&choice);
}

static inline json_sax_print_hd json_sax_print_unformat_start(int item_total)
{
    json_print_choice_t choice = {0};

    choice.format_flag = false;
    choice.item_total = item_total;
    return json_sax_print_start(&choice);
}

static inline json_sax_print_hd json_sax_fprint_format_start(int item_total, const char *path)
{
    json_print_choice_t choice = {0};

    choice.format_flag = true;
    choice.item_total = item_total;
    choice.path = path;
    return json_sax_print_start(&choice);
}

static inline json_sax_print_hd json_sax_fprint_unformat_start(int item_total, const char *path)
{
    json_print_choice_t choice = {0};

    choice.format_flag = false;
    choice.item_total = item_total;
    choice.path = path;
    return json_sax_print_start(&choice);
}

static inline int json_sax_parse_str(char *str, json_sax_cb_t cb)
{
    json_sax_parse_choice_t choice = {0};
    choice.str = str;
    choice.cb = cb;
    return json_sax_parse_common(&choice);
}

static inline int json_sax_parse_file(const char *path, json_sax_cb_t cb)
{
    json_sax_parse_choice_t choice = {0};
    choice.path = path;
    choice.cb = cb;
    return json_sax_parse_common(&choice);
}

#endif
#endif
