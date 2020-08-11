#ifndef __JSON_H__
#define __JSON_H__
#include <ctype.h>

/*
 * original URL:
 * https://gitee.com/lengjingzju/json
 * https://github.com/lengjingzju/json
 */

#define JSON_SAX_APIS_SUPPORT           1

/* json node */
typedef int json_bool_t;
#define JSON_FALSE                      0
#define JSON_TRUE                       1

/******** json object structure ********/

struct json_list_head {
    struct json_list_head *next, *prev;
};

typedef enum {
    JSON_NULL = 0,
    JSON_BOOL,
    JSON_INT,
    JSON_HEX,
    JSON_DOUBLE,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} json_type_t;

typedef union {
    json_bool_t vbool;
    int vint;
    unsigned int vhex;
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
int json_change_key(json_object *json, const char *key);
int json_change_string(json_object *json, const char *str);

/* create/del json apis */
void json_del_object(json_object *json);
json_object *json_new_object(json_type_t type);
json_object *json_create_item(json_type_t type, void *value);

static inline json_object *json_create_null(void)
{
    return json_new_object(JSON_NULL);
}

static inline json_object *json_create_bool(json_bool_t value)
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

static inline json_object *json_create_bool_array(json_bool_t *values, int count)
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

static inline json_object *json_create_double_array(double *values, int count)
{
    return json_create_item_array(JSON_DOUBLE, values, count);
}

static inline json_object *json_create_string_array(char **values, int count)
{
    return json_create_item_array(JSON_STRING, values, count);
}

/* number value apis */
int json_get_number_value(json_object *json, json_type_t type, void *value);
int json_change_number_value(json_object *json, json_type_t type, void *value);
int json_strict_change_number_value(json_object *json, json_type_t type, void *value);

static inline json_bool_t json_get_bool_value(json_object *json)
{
    json_bool_t value = 0;
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

static inline double json_get_double_value(json_object *json)
{
    double value = 0;
    json_get_number_value(json, JSON_DOUBLE, &value);
    return value;
}

static inline int json_change_bool_value(json_object *json, json_bool_t value)
{
    return json_change_number_value(json, JSON_BOOL, &value);
}

static inline int json_change_int_value(json_object *json, int value)
{
    return json_change_number_value(json, JSON_INT, &value);
}

static inline int json_change_hex_value(json_object *json, unsigned int value)
{
    return json_change_number_value(json, JSON_HEX, &value);
}

static inline int json_change_double_value(json_object *json, double value)
{
    return json_change_number_value(json, JSON_DOUBLE, &value);
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

static inline int json_add_bool_to_object(json_object *object, const char *key, json_bool_t value)
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
    size_t def_size;
    size_t align_byte;
    int fast_alloc;
    json_mem_node_t *cur_node;
} json_mem_mgr_t;

typedef struct {
    json_mem_mgr_t obj_mgr;
    json_mem_mgr_t key_mgr;
    json_mem_mgr_t str_mgr;
} json_mem_t;

/******** json memory pool editing mode APIs ********/

/* memmory pool head init/free */
void pjson_memory_head_init(struct json_list_head *head);
void pjson_memory_head_move(struct json_list_head *old, struct json_list_head *_new);
void pjson_memory_head_free(struct json_list_head *head);

// call pjson_memory_init before use, call pjson_memory_free after use.
void pjson_memory_free(json_mem_t *mem);
void pjson_memory_init(json_mem_t *mem);
int pjson_change_key(json_object *json, const char *key, json_mem_t *mem);
int pjson_change_string(json_object *json, const char *str, json_mem_t *mem);

/* create json apis */
json_object *pjson_new_object(json_type_t type, json_mem_t *mem);
json_object *pjson_create_item(json_type_t type, void *value, json_mem_t *mem);

static inline json_object *pjson_create_null(json_mem_t *mem)
{
    return pjson_new_object(JSON_NULL, mem);
}

static inline json_object *pjson_create_bool(json_bool_t value, json_mem_t *mem)
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

static inline json_object *pjson_create_bool_array(json_bool_t *values, int count, json_mem_t *mem)
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

static inline int pjson_add_bool_to_object(json_object *object, const char *key, json_bool_t value, json_mem_t *mem)
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
    const char *path;
    size_t addsize;
    size_t temp_addsize;
    int item_total;
    size_t item_cellsize;
    json_bool_t format_flag;
    json_bool_t calculate_flag;
} json_print_choice_t;

typedef struct {
    const char *path;
    char *str;
    size_t read_size;
    size_t str_len;
    size_t mem_size;
    json_mem_t *mem;
    json_bool_t reuse_flag;
} json_parse_choice_t;

char *json_print_common(json_object *json, json_print_choice_t *choice);
void json_free_print_ptr(void *ptr);
json_object *json_parse_common(json_parse_choice_t *choice);

static inline char *json_print_format(json_object *json)
{
    json_print_choice_t choice = {0};

    choice.format_flag = JSON_TRUE;
    choice.calculate_flag = JSON_TRUE;
    return json_print_common(json, &choice);
}

static inline char *json_print_unformat(json_object *json)
{
    json_print_choice_t choice = {0};

    choice.format_flag = JSON_FALSE;
    choice.calculate_flag = JSON_TRUE;
    return json_print_common(json, &choice);
}

static inline char *json_fprint_format(json_object *json, const char *path)
{
    json_print_choice_t choice = {0};

    choice.format_flag = JSON_TRUE;
    choice.path = path;
    return json_print_common(json, &choice);
}

static inline char *json_fprint_unformat(json_object *json, const char *path)
{
    json_print_choice_t choice = {0};

    choice.format_flag = JSON_FALSE;
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

static inline json_object *json_resuse_parse_str(char *str, json_mem_t *mem, size_t str_len)
{
    json_parse_choice_t choice = {0};
    choice.str = str;
    choice.mem = mem;
    choice.str_len = str_len;
    choice.reuse_flag = JSON_TRUE;
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
} json_detail_str_t;

typedef enum {
    JSON_SAX_START = 0,
    JSON_SAX_FINISH
} json_sax_cmd_t;

typedef union {
    json_number_t vnum;
    json_detail_str_t vstr;
    json_sax_cmd_t vcmd;            // array, object
} json_sax_value_t;

typedef struct {
    json_type_t type;
    json_detail_str_t key;
} json_sax_parse_depth_t;

typedef struct {
    int total;
    int count;
    json_sax_parse_depth_t *array;
    json_sax_value_t value;
} json_sax_parser;

typedef enum {
    JSON_SAX_PARSE_CONTINUE = 0,
    JSON_SAX_PARSE_STOP
} json_sax_parse_ret;

typedef json_sax_parse_ret (*json_sax_parser_callback)(json_sax_parser *parser);

typedef struct {
    char *str;
    const char *path;
    size_t read_size;
    json_sax_parser_callback callback;
} json_sax_parse_choice_t;

typedef void* json_sax_phdl;

int json_sax_print_value(json_sax_phdl handle, json_type_t type, const char *key, const void *value);
json_sax_phdl json_sax_print_start(json_print_choice_t *choice);
char *json_sax_print_finish(json_sax_phdl handle);
int json_sax_parse_common(json_sax_parse_choice_t *choice);

static inline int json_sax_print_null(json_sax_phdl handle, const char *key)
{
    return json_sax_print_value(handle, JSON_NULL, key, NULL);
}

static inline int json_sax_print_bool(json_sax_phdl handle, const char *key, json_bool_t value)
{
    return json_sax_print_value(handle, JSON_BOOL, key, &value);
}

static inline int json_sax_print_int(json_sax_phdl handle, const char *key, int value)
{
    return json_sax_print_value(handle, JSON_INT, key, &value);
}

static inline int json_sax_print_hex(json_sax_phdl handle, const char *key, unsigned int value)
{
    return json_sax_print_value(handle, JSON_HEX, key, &value);
}

static inline int json_sax_print_double(json_sax_phdl handle, const char *key, double value)
{
    return json_sax_print_value(handle, JSON_DOUBLE, key, &value);
}

static inline int json_sax_print_string(json_sax_phdl handle, const char *key, const char *value)
{
    return json_sax_print_value(handle, JSON_STRING, key, value);
}

static inline int json_sax_print_array(json_sax_phdl handle, const char *key, json_sax_cmd_t value)
{
    return json_sax_print_value(handle, JSON_ARRAY, key, &value);
}

static inline int json_sax_print_object(json_sax_phdl handle, const char *key, json_sax_cmd_t value)
{
    return json_sax_print_value(handle, JSON_OBJECT, key, &value);
}

static inline json_sax_phdl json_sax_print_format_start(int item_total)
{
    json_print_choice_t choice = {0};

    choice.format_flag = JSON_TRUE;
    choice.calculate_flag = JSON_TRUE;
    choice.item_total = item_total;
    choice.calculate_flag = JSON_TRUE;
    return json_sax_print_start(&choice);
}

static inline json_sax_phdl json_sax_print_unformat_start(int item_total)
{
    json_print_choice_t choice = {0};

    choice.format_flag = JSON_FALSE;
    choice.calculate_flag = JSON_TRUE;
    choice.item_total = item_total;
    choice.calculate_flag = JSON_TRUE;
    return json_sax_print_start(&choice);
}

static inline json_sax_phdl json_sax_fprint_format_start(const char *path)
{
    json_print_choice_t choice = {0};

    choice.format_flag = JSON_TRUE;
    choice.path = path;
    return json_sax_print_start(&choice);
}

static inline json_sax_phdl json_sax_fprint_unformat_start(const char *path)
{
    json_print_choice_t choice = {0};

    choice.format_flag = JSON_FALSE;
    choice.path = path;
    return json_sax_print_start(&choice);
}

static inline int json_sax_parse_str(char *str, json_sax_parser_callback callback)
{
    json_sax_parse_choice_t choice = {0};
    choice.str = str;
    choice.callback = callback;
    return json_sax_parse_common(&choice);
}

static inline int json_sax_parse_file(const char *path, json_sax_parser_callback callback)
{
    json_sax_parse_choice_t choice = {0};
    choice.path = path;
    choice.callback = callback;
    return json_sax_parse_common(&choice);
}

#endif
#endif

