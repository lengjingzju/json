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

#define JSON_VERSION                    0x010202
#define JSON_SAX_APIS_SUPPORT           1
#define JSON_LONG_LONG_SUPPORT          1

/**************** json object structure ****************/

/*
 * struct json_list - the value of json list
 * @next: the next list
 * @description: LJSON uses it to manage json objects and memory blocks.
 */
struct json_list {
    struct json_list *next;
};

/*
 * struct json_list_head - the head of json list
 * @next: the next list
 * @prev: the last list
 * @description: LJSON uses it to manage json objects and memory blocks.
 */
struct json_list_head {
    struct json_list *next, *prev;
};

/*
 * json_type_t - the json object type
 * @description: LJSON supports not only standard types, but also extended types (JSON HEX...).
 */
typedef enum {
    JSON_NULL = 0,              /* It doesn't has value variable: null */
    JSON_BOOL,                  /* Its value variable is vbool: true, false */
    JSON_INT,                   /* Its value variable is vint */
    JSON_HEX,                   /* Its value variable is vhex */
#if JSON_LONG_LONG_SUPPORT
    JSON_LINT,                  /* Its value variable is vlint */
    JSON_LHEX,                  /* Its value variable is vlhex */
#endif
    JSON_DOUBLE,                /* Its value variable is vdbl */
    JSON_STRING,                /* Its value variable is vstr */
    JSON_ARRAY,                 /* Its value variable is head */
    JSON_OBJECT                 /* Its value variable is head */
} json_type_t;

/*
 * json_string_t - the json string object value or type-key value
 * @type: the json object type (json_type_t), it is only valid when used as a key
 * @escaped: whether the string contains characters that need to be escaped
 * @alloced: whether the string is alloced, it is only valid for SAX APIs
 * @len: the length of string
 * @str: the value of string
 * @description: LJSON uses string with information to accelerate printing.
 */
typedef struct {
    unsigned int type:4;
    unsigned int escaped:1;
    unsigned int alloced:1;
    unsigned int reserved:2;
    unsigned int len:24;
    char *str;
} json_string_t;

/*
 * json_number_t - the json number object value
 * @description: LJSON supports decimal and hexadecimal, integer and long long integer.
 */
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

#if JSON_SAX_APIS_SUPPORT
/*
 * json_sax_cmd_t - the beginning and end of JSON_ARRAY or JSON_OBJECT object
 * @description: We know that parentheses have two sides, `JSON_SAX_START` indicates left side,
 *   and `JSON_SAX_FINISH` indicates right side.
 */
typedef enum {
    JSON_SAX_START = 0,
    JSON_SAX_FINISH
} json_sax_cmd_t;
#endif

/*
 * json_value_t - the json object value
 * @vnum: the numerical value
 * @vstr: the string value
 * @vcmd: the SAX array or object value, only for SAX APIs
 * @head: the DOM array or object value, LJSON manages child json objects through the list head
 * @description: LJSON uses union to manage the value of different objects to save memory.
 */
typedef union {
    json_number_t vnum;
    json_string_t vstr;
#if JSON_SAX_APIS_SUPPORT
    json_sax_cmd_t vcmd;
#endif
    struct json_list_head head;
} json_value_t;

/*
 * json_object - the json object
 * @list: the list value, LJSON associates `list` to the `head` of parent json object
 *   or the `list` of brother json objects
 * @jkey: the json object type and key, only the child objects of JSON_OBJECT have key
 * @value: the json object value
 * @description: LJSON uses union to manage the value of different objects to save memory.
 */
typedef struct {
    struct json_list list;
    json_string_t jkey;
    json_value_t value;
} json_object;

/**************** json classic editor ****************/

/*
 * json_memory_free - Free the ptr alloced by LJSON
 * @ptr: IN, the alloced ptr
 * @description: the alloced ptr may be:
 *   1. the returned string by json_print_common or json_sax_print_finish when printing to string
 *   2. the LJSON style string alloced by LJSON classic(not pool) APIS
 */
void json_memory_free(void *ptr);

/*
 * json_item_total_get - Get the total number of json tree (recursive)
 * @json: IN, the json object
 * @return: the total number
 */
int json_item_total_get(json_object *json);

/*
 * json_del_object - Delete the json object, includes the child objects (recursive)
 * @json: IN, the json object
 * @return: None
 */
void json_del_object(json_object *json);

/*
 * json_new_object - Create a json object without value
 * @type: IN, the json object type
 * @return: NULL on failure, a pointer on success
 */
json_object *json_new_object(json_type_t type);

/*
 * json_create_item - Create a json object with value
 * @type: IN, the json object type
 * @value: IN, the json object value
 * @return: NULL on failure, a pointer on success
 */
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

static inline json_object *json_create_string(json_string_t *value)
{
    return json_create_item(JSON_STRING, value);
}

static inline json_object *json_create_array(void)
{
    return json_new_object(JSON_ARRAY);
}

static inline json_object *json_create_object(void)
{
    return json_new_object(JSON_OBJECT);
}

/*
 * json_create_item_array - Create a JSON_ARRAY object with a group of child json objects
 * @type: IN, the type of child json objects
 * @values: IN, the values of child json objects
 * @count: IN, the total number of child json objects
 * @return: NULL on failure, a pointer on success
 */
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

static inline json_object *json_create_string_array(json_string_t *values, int count)
{
    return json_create_item_array(JSON_STRING, values, count);
}

/*
 * json_string_info_update - Update the parameters for json string
 * @jstr: INOUT, the LJSON string
 * @return: None
 * @description: If jstr->len is not equal to 0, the parameters will not be updated.
 */
void json_string_info_update(json_string_t *jstr);

/*
 * json_string_strdup - Strdup the LJSON string src to dst
 * @src: IN, the source string
 * @dst: OUT, the destination string
 * @return: -1 on failure, 0 on success
 */
int json_string_strdup(json_string_t *src, json_string_t *dst);

/*
 * json_set_key - Set the key of json object
 * @json: IN, the json object to be set
 * @jkey: IN, the LJSON string key, allow length not to be set first by json_string_info_update
 * @return: -1 on failure, 0 on success
 */
static inline int json_set_key(json_object *json, json_string_t *jkey)
{
    return json_string_strdup(jkey, &json->jkey);
}

/*
 * json_set_string_value - Set the string of JSON_STRING object
 * @json: IN, the json object to be set
 * @jstr: IN, the LJSON string value, allow length not to be set first by json_string_info_update
 * @return: -1 on failure, 0 on success
 */
static inline int json_set_string_value(json_object *json, json_string_t *jstr)
{
    if (json->jkey.type == JSON_STRING) {
        return json_string_strdup(jstr, &json->value.vstr);
    }
    return -1;
}

/*
 * json_get_number_value - Get the value of numerical object
 * @json: IN, the json object
 * @type: IN, the json object type
 * @value: OUT, the json object value
 * @return: -1 on failure, 0 on success, >0(original type) on success with type cast
 */
int json_get_number_value(json_object *json, json_type_t type, void *value);

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

/*
 * json_set_number_value - Set the value of numerical object
 * @json: IN, the json object
 * @type: IN, the json object type
 * @value: IN, the json object value
 * @return: -1 on failure, 0 on success, >0(original type) on success with type cast
 */
int json_set_number_value(json_object *json, json_type_t type, void *value);

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

/*
 * json_get_array_size - Get the total child objects of JSON_ARRAY object (not recursive)
 * @json: IN, the JSON_ARRAY object
 * @return: the total child objects
 */
int json_get_array_size(json_object *json);

/*
 * json_get_array_item - Get the specified object in JSON_ARRAY object
 * @json: IN, the JSON_ARRAY object
 * @seq: IN, the sequence number
 * &prev: OUT, to store the previous JSON object, it can be NULL
 * @return: NULL on failure, a pointer on success
 */
json_object *json_get_array_item(json_object *json, int seq, json_object **prev);

/*
 * json_get_object_item - Get the specified object in JSON_OBJECT object
 * @json: IN, the JSON_OBJECT object
 * @key: IN, the specified key
 * &prev: OUT, to store the previous JSON object, it can be NULL
 * @return: NULL on failure, a pointer on success
 */
json_object *json_get_object_item(json_object *json, const char *key, json_object **prev);

/*
 * json_detach_item_from_array - Detach the specified object in JSON_ARRAY object
 * @json: IN, the JSON_ARRAY object
 * @seq: IN, the sequence number
 * @return: NULL on failure, a pointer (the detached object) on success
 * @description: After use, users need to delete the returned object in classic jsons,
 *   don't delete it in pool jsons.
 */
json_object *json_detach_item_from_array(json_object *json, int seq);

/*
 * json_detach_item_from_object - Detach the specified object in JSON_OBJECT object
 * @json: IN, the JSON_ARRAY object
 * @key: IN, the specified key
 * @return: NULL on failure, a pointer (the detached object) on success
 * @description: After use, users need to delete the returned object in classic jsons,
 *   don't delete it in pool jsons.
 */
json_object *json_detach_item_from_object(json_object *json, const char *key);

/*
 * json_del_item_from_array - Delete the specified object in JSON_ARRAY object
 * @json: IN, the JSON_ARRAY object
 * @seq: IN, the sequence number
 * @return: -1 on not found, 0 on found and deleted
 */
int json_del_item_from_array(json_object *json, int seq);

/*
 * json_del_item_from_object - Delete the specified object in JSON_OBJECT object
 * @json: IN, the JSON_OBJECT object
 * @key: IN, the specified key
 * @return: -1 on not found, 0 on found and deleted
 */
int json_del_item_from_object(json_object *json, const char *key);

/*
 * json_replace_item_in_array - Replace the specified object in JSON_ARRAY object with new json object
 * @array: IN, the JSON_ARRAY object
 * @seq: IN, the sequence number
 * @new_item: IN, the new json object
 * @return: -1 on failure, 0 on success
 * @description: If seq is not satisfied, new_item will be added to the end.
 */
int json_replace_item_in_array(json_object *array, int seq, json_object *new_item);

/*
 * json_replace_item_in_object - Replace the specified object in JSON_OBJECT object with new json object
 * @object: IN, the JSON_OBJECT object
 * @jkey: IN, the LJSON string key, allow length not to be set first by json_string_info_update
 * @new_item: IN, the new json object
 * @return: -1 on failure, 0 on success
 * @description: If key is not satisfied, new_item will be added to the end.
 */
int json_replace_item_in_object(json_object *object, json_string_t *jkey, json_object *new_item);

/*
 * json_add_item_to_array - Add the specified object to the JSON_ARRAY object
 * @array: IN, the JSON_ARRAY object
 * @item: IN, the json object to add
 * @return: -1 on failure, 0 on success
 * @description: The item will be added to the end, once successfully added,
 *   it is no longer necessary to manually delete item.
 */
int json_add_item_to_array(json_object *array, json_object *item);

/*
 * json_add_item_to_object - Add the specified object to the JSON_OBJECT object
 * @object: IN, the JSON_OBJECT object
 * @jkey: IN, the LJSON string key, allow length not to be set first by json_string_info_update
 * @item: IN, the json object to add
 * @return: -1 on failure, 0 on success
 * @description: The item will be added to the end, once successfully added,
 *   it is no longer necessary to manually delete item.
 */
int json_add_item_to_object(json_object *object, json_string_t *jkey, json_object *item);

/*
 * json_deepcopy - Deep copy the json object (recursive)
 * @json: IN, the source json object
 * @return: NULL on failure, a pointer on success
 */
json_object *json_deepcopy(json_object *json);

/*
 * json_copy_item_to_array - Copy the specified object, the add it to the JSON_ARRAY object
 * @array: IN, the JSON_ARRAY object
 * @item: IN, the json object to copy and add
 * @return: -1 on failure, 0 on success
 * @description: The item will be added to the end, once successfully added,
 *   it is also necessary to manually delete item.
 */
int json_copy_item_to_array(json_object *array, json_object *item);

/*
 * json_copy_item_to_object - Copy the specified object, the add it to the JSON_OBJECT object
 * @object: IN, the JSON_OBJECT object
 * @jkey: IN, the LJSON string key, allow length not to be set first by json_string_info_update
 * @item: IN, the json object to copy and add
 * @return: -1 on failure, 0 on success
 * @description: The item will be added to the end, once successfully added,
 *   it is also necessary to manually delete item.
 */
int json_copy_item_to_object(json_object *object, json_string_t *jkey, json_object *item);

/*
 * json_add_new_item_to_object - Create a new object, the add it to the JSON_OBJECT object
 * @object: IN, the JSON_OBJECT object
 * @type: the json object type
 * @jkey: IN, the LJSON string key, allow length not to be set first by json_string_info_update
 * @value: IN, the json object value
 * @return: -1 on failure, 0 on success
 */
int json_add_new_item_to_object(json_object *object, json_type_t type, json_string_t *jkey, void* value);

static inline int json_add_null_to_object(json_object *object, json_string_t *jkey)
{
    return json_add_new_item_to_object(object, JSON_NULL, jkey, NULL);
}

static inline int json_add_bool_to_object(json_object *object, json_string_t *jkey, bool value)
{
    return json_add_new_item_to_object(object, JSON_BOOL, jkey, &value);
}

static inline int json_add_int_to_object(json_object *object, json_string_t *jkey, int value)
{
    return json_add_new_item_to_object(object, JSON_INT, jkey, &value);
}

static inline int json_add_hex_to_object(json_object *object, json_string_t *jkey, unsigned int value)
{
    return json_add_new_item_to_object(object, JSON_HEX, jkey, &value);
}

#if JSON_LONG_LONG_SUPPORT
static inline int json_add_lint_to_object(json_object *object, json_string_t *jkey, long long int value)
{
    return json_add_new_item_to_object(object, JSON_LINT, jkey, &value);
}

static inline int json_add_lhex_to_object(json_object *object, json_string_t *jkey, unsigned long long int value)
{
    return json_add_new_item_to_object(object, JSON_LHEX, jkey, &value);
}
#endif

static inline int json_add_double_to_object(json_object *object, json_string_t *jkey, double value)
{
    return json_add_new_item_to_object(object, JSON_DOUBLE, jkey, &value);
}

static inline int json_add_string_to_object(json_object *object, json_string_t *jkey, json_string_t *value)
{
    return json_add_new_item_to_object(object, JSON_STRING, jkey, &value);
}

/*
 * The below APIs are also available to pool json:
 * json_item_total_get
 * json_string_info_update
 * json_get_number_value / ...
 * json_set_number_value / ...
 * json_get_array_size
 * json_get_array_item
 * json_get_object_item
 * json_detach_item_from_array
 * json_detach_item_from_object
 * json_add_item_to_array
 */

/**************** json pool editor ****************/

/*
 * json_mem_node_t - the block memory node
 * @list: the list value, LJSON associates `list` to the `head` of json_mem_mgr_t
 *   or the `list` of brother json_mem_node_t
 * @size: the memory size
 * @ptr: the memory pointer
 * @cur: the current memory pointer
 * @description: LJSON can use the block memory to accelerate memory allocation and save memory space.
 */
typedef struct {
    struct json_list list;
    size_t size;
    char *ptr;
    char *cur;
} json_mem_node_t;

/*
 * json_mem_mgr_t - the node to manage block memory node
 * @head: the list head, LJSON manages block memory nodes through the list head
 * @mem_size: the default memory size to allocate, its default value is JSON_POOL_MEM_SIZE_DEF(8096)
 * @cur_node: the current block memory node
 * @description: the manage node manages a group of block memory nodes.
 */
typedef struct {
    struct json_list_head head;
    size_t mem_size;
    json_mem_node_t *cur_node;
} json_mem_mgr_t;

/*
 * json_mem_t - the head to manage all types of block memory
 * @obj_mgr: the node to manage json_object
 * @key_mgr: the node to manage the value of key
 * @str_mgr: the node to manage the value of JSON_STRING object
 * @description: The reason for dividing into multiple management nodes is that
 * there is a memory address alignment requirement for json_object.
 */
typedef struct {
    json_mem_mgr_t obj_mgr;
    json_mem_mgr_t key_mgr;
    json_mem_mgr_t str_mgr;
} json_mem_t;

/*
 * pjson_memory_free - Free all block memory
 * @mem: IN, the block memory manager
 * @description: Users need to call it to delete all json objects, not one.
 */
void pjson_memory_free(json_mem_t *mem);

/*
 * pjson_memory_init - Initializate the block memory manager
 * @mem: IN, the block memory manager to be initializated
 * @return: None
 * @description: Users need to call it before using block memory apis.
 *   User can re-set `mem_size` after calling it.
 */
void pjson_memory_init(json_mem_t *mem);

/*
 * pjson_new_object - Create a pool json object without value
 * @type: IN, the json object type
 * @mem: IN, the block memory manager
 * @return: NULL on failure, a pointer on success
 */
json_object *pjson_new_object(json_type_t type, json_mem_t *mem);

/*
 * pjson_create_item - Create a pool json object with value
 * @type: IN, the json object type
 * @value: IN, the json object value
 * @mem: IN, the block memory manager
 * @return: NULL on failure, a pointer on success
 */
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

static inline json_object *pjson_create_string(json_string_t *value, json_mem_t *mem)
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

/*
 * pjson_create_item_array - Create a pool JSON_ARRAY object with a group of child json objects
 * @type: IN, the type of child json objects
 * @values: IN, the values of child json objects
 * @count: IN, the total number of child json objects
 * @mem: IN, the block memory manager
 * @return: NULL on failure, a pointer on success
 */
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

static inline json_object *pjson_create_string_array(json_string_t *values, int count, json_mem_t *mem)
{
    return pjson_create_item_array(JSON_STRING, values, count, mem);
}

/*
 * pjson_string_strdup - Strdup the LJSON string src to dst
 * @src: IN, the source string
 * @dst: OUT, the destination string
 * @return: -1 on failure, 0 on success
 */
int pjson_string_strdup(json_string_t *src, json_string_t *dst, json_mem_mgr_t *mgr);

/*
 * pjson_set_key - Set the key of json object
 * @json: IN, the json object to be set
 * @jkey: IN, the LJSON string key, allow length not to be set first by json_string_info_update
 * @mem: IN, the block memory manager
 * @return: -1 on failure, 0 on success
 */
static inline int pjson_set_key(json_object *json, json_string_t *jkey, json_mem_t *mem)
{
    return pjson_string_strdup(jkey, &json->jkey, &mem->key_mgr);
}

/*
 * pjson_set_string_value - Set the string of JSON_STRING object
 * @json: IN, the json object to be set
 * @jstr: IN, the LJSON string value, allow length not to be set first by json_string_info_update
 * @mem: IN, the block memory manager
 * @return: -1 on failure, 0 on success
 */
static inline int pjson_set_string_value(json_object *json, json_string_t *jstr, json_mem_t *mem)
{
    if (json->jkey.type == JSON_STRING) {
        return pjson_string_strdup(jstr, &json->value.vstr, &mem->str_mgr);
    }
    return -1;
}

/*
 * pjson_add_item_to_object - Add the specified object to the pool JSON_OBJECT object
 * @object: IN, the JSON_OBJECT object
 * @jkey: IN, the LJSON string key, allow length not to be set first by json_string_info_update
 * @item: IN, the json object to add
 * @mem: IN, the block memory manager
 * @return: -1 on failure, 0 on success
 */
int pjson_add_item_to_object(json_object *object, json_string_t *jkey, json_object *item, json_mem_t *mem);

/*
 * pjson_add_new_item_to_object - Create a new object, the add it to the pool JSON_OBJECT object
 * @object: IN, the JSON_OBJECT object
 * @type: the json object type
 * @jkey: IN, the LJSON string key, allow length not to be set first by json_string_info_update
 * @value: IN, the json object value
 * @mem: IN, the block memory manager
 * @return: -1 on failure, 0 on success
 */
int pjson_add_new_item_to_object(json_object *object, json_type_t type, json_string_t *jkey, void *value, json_mem_t *mem);

static inline int pjson_add_null_to_object(json_object *object, json_string_t *jkey, json_mem_t *mem)
{
    return pjson_add_new_item_to_object(object, JSON_NULL, jkey, NULL, mem); }

static inline int pjson_add_bool_to_object(json_object *object, json_string_t *jkey, bool value, json_mem_t *mem)
{
    return pjson_add_new_item_to_object(object, JSON_BOOL, jkey, &value, mem);
}

static inline int pjson_add_int_to_object(json_object *object, json_string_t *jkey, int value, json_mem_t *mem)
{
    return pjson_add_new_item_to_object(object, JSON_INT, jkey, &value, mem);
}

static inline int pjson_add_hex_to_object(json_object *object, json_string_t *jkey, unsigned int value, json_mem_t *mem)
{
    return pjson_add_new_item_to_object(object, JSON_HEX, jkey, &value, mem);
}

#if JSON_LONG_LONG_SUPPORT
static inline int pjson_add_lint_to_object(json_object *object, json_string_t *jkey, long long int value, json_mem_t *mem)
{
    return pjson_add_new_item_to_object(object, JSON_LINT, jkey, &value, mem);
}

static inline int pjson_add_lhex_to_object(json_object *object, json_string_t *jkey, unsigned long long int value, json_mem_t *mem)
{
    return pjson_add_new_item_to_object(object, JSON_LHEX, jkey, &value, mem);
}
#endif

static inline int pjson_add_double_to_object(json_object *object, json_string_t *jkey, double value, json_mem_t *mem)
{
    return pjson_add_new_item_to_object(object, JSON_DOUBLE, jkey, &value, mem);
}

static inline int pjson_add_string_to_object(json_object *object, json_string_t *jkey, json_string_t *value, json_mem_t *mem)
{
    return pjson_add_new_item_to_object(object, JSON_STRING, jkey, value, mem);
}

/**************** json DOM printer ****************/

/*
 * json_print_choice_t - the choice to print
 * @str_len: OUT, the length of returned printed string when printing to string
 * @plus_size: IN, increased memory size during reallocation when printing to string,
 *   or the write buffer size when printing to file,
 *   its default value is `JSON_PRINT_SIZE_PLUS_DEF`(1024)
 * @item_size: IN, the json object size when transfering to a string,
 *   its default value is `JSON_FORMAT_ITEM_SIZE_DEF`(32) when format_flag is true,
 *   or `JSON_UNFORMAT_ITEM_SIZE_DEF`(24) when format_flag is false
 * @item_total: IN, the total json objects, it will be calculated automatically in DOM print,
 *   it is better to set the value by users in SAX print
 * @format_flag: IN, set formatted printing (true) or compressed printing(false)
 * @path: IN, when the path is set, it prints to file while printing,
 *   otherwise it directly print to string
 */
typedef struct {
    size_t str_len; /* return string length if it is printed to string. */
    size_t plus_size;
    size_t item_size;
    int item_total;
    bool format_flag;
    const char *path;
} json_print_choice_t;

/*
 * json_print_common - The common DOM printer
 * @json: IN, the json object to be printed
 * @choice: INOUT, the print choice
 * @return: NULL on failure, a pointer on success
 * @description: When printing to file, the pointer is `"ok"` on success, don't free it,
 *   when printing to string, the pointer is the printed string, use `json_memory_free` to free it.
 */
char *json_print_common(json_object *json, json_print_choice_t *choice);

/*
 * json_print_format - Formatted DOM printer to string
 * @json: IN, the json object to be printed
 * @length: OUT, the length of returned print string
 * @return: NULL on failure, a pointer on success
 */
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

/*
 * json_print_unformat - Compressed DOM printer to string
 * @json: IN, the json object to be printed
 * @length: OUT, the length of returned print string
 * @return: NULL on failure, a pointer on success
 */
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

/*
 * json_fprint_format - Formatted DOM printer to file
 * @json: IN, the json object to be printed
 * @path: IN, the file to store the printed string
 * @return: NULL on failure, a pointer on success
 */
static inline char *json_fprint_format(json_object *json, const char *path)
{
    json_print_choice_t choice = {0};

    choice.format_flag = true;
    choice.path = path;
    return json_print_common(json, &choice);
}

/*
 * json_fprint_unformat - Compressed DOM printer to file
 * @json: IN, the json object to be printed
 * @path: IN, the file to store the printed string
 * @return: NULL on failure, a pointer on success
 */
static inline char *json_fprint_unformat(json_object *json, const char *path)
{
    json_print_choice_t choice = {0};

    choice.format_flag = false;
    choice.path = path;
    return json_print_common(json, &choice);
}

/**************** json DOM parser ****************/

/*
 * json_parse_choice_t - the choice to parse json
 * @mem_size: the default block memory size to allocate, its smallest value is
 *   (str_len / `JSON_STR_MULTIPLE_NUM`(8))
 * @read_size: IN, the read buffer size when parsing from file,
 *   its default value is `JSON_PARSE_READ_SIZE_DEF`(8096)
 * @str_len: IN, the size of string to be parsed when parsing from string,
 *   it's better to set it when parsing from string
 * @reuse_flag: IN, whether to use the `str` directly as the value of JSON_STRING object and key
 * @mem: IN, the block memory manager, users needn't to Initializate it first
 * @path: IN, the file to be parsed, when the path is set, it parses the data while reading,
 *   otherwise it directly parses from the string
 * @str: IN, the string to be parsed, only one of `path` and `str` has value
 */
typedef struct {
    size_t mem_size;
    size_t read_size;
    size_t str_len;
    bool reuse_flag;
    json_mem_t *mem;
    const char *path;
    char *str;
} json_parse_choice_t;

/*
 * json_parse_common - The common DOM parser
 * @choice: IN, the parse choice
 * @return: NULL on failure, a pointer on success
 */
json_object *json_parse_common(json_parse_choice_t *choice);

/*
 * json_parse_str - The ordinary DOM parser from string
 * @str: IN, the string to be parsed
 * @str_len: IN, the length of str
 * @return: NULL on failure, a pointer on success
 * @description: Use `malloc` to allocate memory
 */
static inline json_object *json_parse_str(char *str, size_t str_len)
{
    json_parse_choice_t choice = {0};
    choice.str = str;
    choice.str_len = str_len;
    return json_parse_common(&choice);
}

/*
 * json_fast_parse_str - The fast DOM parser from string
 * @str: IN, the string to be parsed
 * @str_len: IN, the length of str
 * @mem: IN, the block memory manager
 * @return: NULL on failure, a pointer on success
 * @description: Use `pjson_memory_alloc` to allocate memory, it is faster.
 */
static inline json_object *json_fast_parse_str(char *str, size_t str_len, json_mem_t *mem)
{
    json_parse_choice_t choice = {0};
    choice.str = str;
    choice.mem = mem;
    choice.str_len = str_len;
    return json_parse_common(&choice);
}

/*
 * json_reuse_parse_str - The reused DOM parser from string
 * @str: IN, the string to be parsed
 * @str_len: IN, the length of str
 * @mem: IN, the block memory manager
 * @return: NULL on failure, a pointer on success
 * @description: Use `pjson_memory_alloc` to allocate memory for json object, it is faster,
 *  and it uses the parsed `str` directly as the value of JSON_STRING object and key,
 *  this means that it modifies the original string and saves memory.
 */
static inline json_object *json_reuse_parse_str(char *str, size_t str_len, json_mem_t *mem)
{
    json_parse_choice_t choice = {0};
    choice.str = str;
    choice.mem = mem;
    choice.str_len = str_len;
    choice.reuse_flag = true;
    return json_parse_common(&choice);
}

/*
 * json_parse_file - The ordinary DOM parser from file
 * @path: IN, the file to be parsed
 * @return: NULL on failure, a pointer on success
 * @description: It parses the data while reading, no need to read data all at once.
 */
static inline json_object *json_parse_file(const char *path)
{
    json_parse_choice_t choice = {0};
    choice.path = path;
    return json_parse_common(&choice);
}

/*
 * json_fast_parse_file - The fast DOM parser from file
 * @path: IN, the file to be parsed
 * @mem: IN, the block memory manager
 * @return: NULL on failure, a pointer on success
 * @description: It parses the data while reading, no need to read data all at once.
 */
static inline json_object *json_fast_parse_file(const char *path, json_mem_t *mem)
{
    json_parse_choice_t choice = {0};
    choice.path = path;
    choice.mem = mem;
    return json_parse_common(&choice);
}

/**************** json SAX printer ****************/

#if JSON_SAX_APIS_SUPPORT

/*
 * json_sax_print_hd - the handle of SAX printer
 * @description: It is a pointer of `json_sax_print_t`.
 */
typedef void* json_sax_print_hd;

/*
 * json_sax_print_start - Start the SAX printer
 * @choice: INOUT, the print choice
 * @return: NULL on failure, a pointer (the handle of SAX print) on success
 */
json_sax_print_hd json_sax_print_start(json_print_choice_t *choice);

/*
 * json_sax_print_format_start - Start the formatted SAX printer to string
 * @item_total: IN, the total json objects, it is better to set the value by users
 * @return: NULL on failure, a pointer (the handle of SAX print) on success
 */
static inline json_sax_print_hd json_sax_print_format_start(int item_total)
{
    json_print_choice_t choice = {0};

    choice.format_flag = true;
    choice.item_total = item_total;
    return json_sax_print_start(&choice);
}

/*
 * json_sax_print_unformat_start - Start the compressed SAX printer to string
 * @item_total: IN, the total json objects, it is better to set the value by users
 * @return: NULL on failure, a pointer (the handle of SAX print) on success
 */
static inline json_sax_print_hd json_sax_print_unformat_start(int item_total)
{
    json_print_choice_t choice = {0};

    choice.format_flag = false;
    choice.item_total = item_total;
    return json_sax_print_start(&choice);
}

/*
 * json_sax_fprint_format_start - Start the formatted SAX printer to file
 * @item_total: IN, the total json objects, it is better to set the value by users
 * @path: IN, the file to store the printed string
 * @return: NULL on failure, a pointer (the handle of SAX print) on success
 */
static inline json_sax_print_hd json_sax_fprint_format_start(int item_total, const char *path)
{
    json_print_choice_t choice = {0};

    choice.format_flag = true;
    choice.item_total = item_total;
    choice.path = path;
    return json_sax_print_start(&choice);
}

/*
 * json_sax_fprint_unformat_start - Start the compressed SAX printer to file
 * @item_total: IN, the total json objects, it is better to set the value by users
 * @path: IN, the file to store the printed string
 * @return: NULL on failure, a pointer (the handle of SAX print) on success
 */
static inline json_sax_print_hd json_sax_fprint_unformat_start(int item_total, const char *path)
{
    json_print_choice_t choice = {0};

    choice.format_flag = false;
    choice.item_total = item_total;
    choice.path = path;
    return json_sax_print_start(&choice);
}

/*
 * json_sax_print_value - SAX print the json object
 * @handle: IN, the handle of SAX printer
 * @type: the json object type
 * @jkey: IN, the LJSON key, allow length not to be set first by json_string_info_update
 * @value: IN, the json object value
 * @return: -1 on failure, 0 on success
 * @description: If the parent node of the node to be printed is an object, the key string must have a value;
 *   in other cases, the key string can be filled in or not.
 *   The JSON_ARRAY and JSON_OBJECT are printed twice, once the value is `JSON_SAX_START` to start,
 *   and once the value is `JSON_SAX_FINISH` to complete.
 */
int json_sax_print_value(json_sax_print_hd handle, json_type_t type, json_string_t *jkey, const void *value);

static inline int json_sax_print_null(json_sax_print_hd handle, json_string_t *jkey)
{
    return json_sax_print_value(handle, JSON_NULL, jkey, NULL);
}

static inline int json_sax_print_bool(json_sax_print_hd handle, json_string_t *jkey, bool value)
{
    return json_sax_print_value(handle, JSON_BOOL, jkey, &value);
}

static inline int json_sax_print_int(json_sax_print_hd handle, json_string_t *jkey, int value)
{
    return json_sax_print_value(handle, JSON_INT, jkey, &value);
}

static inline int json_sax_print_hex(json_sax_print_hd handle, json_string_t *jkey, unsigned int value)
{
    return json_sax_print_value(handle, JSON_HEX, jkey, &value);
}

#if JSON_LONG_LONG_SUPPORT
static inline int json_sax_print_lint(json_sax_print_hd handle, json_string_t *jkey, long long int value)
{
    return json_sax_print_value(handle, JSON_LINT, jkey, &value);
}

static inline int json_sax_print_lhex(json_sax_print_hd handle, json_string_t *jkey, unsigned long long int value)
{
    return json_sax_print_value(handle, JSON_LHEX, jkey, &value);
}
#endif

static inline int json_sax_print_double(json_sax_print_hd handle, json_string_t *jkey, double value)
{
    return json_sax_print_value(handle, JSON_DOUBLE, jkey, &value);
}

static inline int json_sax_print_string(json_sax_print_hd handle, json_string_t *jkey, json_string_t *value)
{
    return json_sax_print_value(handle, JSON_STRING, jkey, value);
}

static inline int json_sax_print_array(json_sax_print_hd handle, json_string_t *jkey, json_sax_cmd_t value)
{
    return json_sax_print_value(handle, JSON_ARRAY, jkey, &value);
}

static inline int json_sax_print_object(json_sax_print_hd handle, json_string_t *jkey, json_sax_cmd_t value)
{
    return json_sax_print_value(handle, JSON_OBJECT, jkey, &value);
}

/*
 * json_sax_print_finish - Finish the SAX printer
 * @handle: IN, the handle of SAX printer
 * @length: OUT, the length of returned printed string
 * @return: NULL on failure, a pointer on success
 * @description: When printing to file, the pointer is `"ok"` on success, don't free it,
 *   when printing to string, the pointer is the printed string, use `json_memory_free` to free it.
 */
char *json_sax_print_finish(json_sax_print_hd handle, size_t *length);

/**************** json SAX parser ****************/

/*
 * json_sax_parser_t - the description passed by SAX parser to the callback function
 * @total: the size of depth array
 * @index: the current index of JSON type and key
 * @array: the json depth information, which stores json object type and key
 * @value: the json object value
 * @description: LJSON SAX parsing will maintain a depth information used for state machine.
 */
typedef struct {
    int total;
    int index;
    json_string_t *array;
    json_value_t value;
} json_sax_parser_t;

/*
 * json_sax_cb_t - the callback function for SAX parsing
 * @description: Users need to fill the callback function, returning `JSON_SAX_PARSE_CONTINUE`
 * indicates continuing parsing, returning `JSON_SAX_PARSE_STOP` indicates stoping parsing
 */
typedef enum {
    JSON_SAX_PARSE_CONTINUE = 0,
    JSON_SAX_PARSE_STOP
} json_sax_ret_t;
typedef json_sax_ret_t (*json_sax_cb_t)(json_sax_parser_t *parser);

/*
 * json_sax_parse_choice_t - the choice to parse
 * @read_size: IN, the read buffer size when parsing from file,
 *   its default value is `JSON_PARSE_READ_SIZE_DEF`(8096)
 * @str_len: IN, the size of string to be parsed when parsing from string,
 *   it's better to set it when parsing from string
 * @str: IN, the string to be parsed, only one of `path` and `str` has value
 * @path: IN, the file to be parsed, when the path is set, it parses the data while reading,
 *   otherwise it directly parses from the string
 * @cb: IN, the callback to process result passed by the SAX parser
 */
typedef struct {
    size_t read_size;
    size_t str_len;
    char *str;
    const char *path;
    json_sax_cb_t cb;
} json_sax_parse_choice_t;

/*
 * json_sax_parse_common - The common SAX parser
 * @choice: IN, the parse choice
 * @return: -1 on failure, 0 on success
 */
int json_sax_parse_common(json_sax_parse_choice_t *choice);

/*
 * json_sax_parse_str - The SAX parser from string
 * @str: IN, the string to be parsed
 * @str_len: IN, the length of str
 * @cb: IN, the callback to process result passed by the SAX parser
 * @return: -1 on failure, 0 on success
 * description: LJSON directly parses the data from the string
 */
static inline int json_sax_parse_str(char *str, size_t str_len, json_sax_cb_t cb)
{
    json_sax_parse_choice_t choice = {0};
    choice.str = str;
    choice.cb = cb;
    choice.str_len = str_len;
    return json_sax_parse_common(&choice);
}

/*
 * json_sax_parse_file - The SAX parser from file
 * @path: IN, the file to be parsed
 * @cb: IN, the callback function to process result passed by the SAX parser
 * @return: -1 on failure, 0 on success
 * description: LJSON parses the data while reading
 */
static inline int json_sax_parse_file(const char *path, json_sax_cb_t cb)
{
    json_sax_parse_choice_t choice = {0};
    choice.path = path;
    choice.cb = cb;
    return json_sax_parse_common(&choice);
}

#endif
#endif
