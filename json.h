/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2019-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* URL: https://github.com/lengjingzju/json *
*******************************************/
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JSON_VERSION                    0x030307
#define JSON_SAX_APIS_SUPPORT           1

/**************** json object structure / json对象结构 ****************/

/*
 * struct json_list - the value of json list
 * @next: the next list
 * @description: LJSON uses it to manage json objects and memory blocks.
 */
/*
 * struct json_list - 链表节点
 * @next: 下一个对象的链表节点
 * @description: LJSON使用它管理JSON对象和内存块的节点
 */
struct json_list {
    struct json_list *next;
};

/*
 * struct json_list_head - the head of json list
 * @next: the next list
 * @prev: the last list
 * @description: LJSON uses it to manage json objects and memory blocks
 */
/*
 * struct json_list_head - 挂载链表的头
 * @next: 指向头对象的链表节点
 * @prev: 指向尾对象的链表节点
 * @description: LJSON使用它管理JSON对象和内存块的节点
 */
struct json_list_head {
    struct json_list *next, *prev;
};

/*
 * json_type_t - the json object type
 * @description: LJSON supports not only standard types, but also extended types (JSON HEX...).
 */
/*
 * json_type_t - json对象的类型
 * @description: LJSON支持的对象比cJSON多，例如长整数，十六机制的数
 */
typedef enum {
    JSON_NULL = 0,              /* It doesn't has value variable: null */
    JSON_BOOL,                  /* Its value variable is vbool: true, false */
    JSON_INT,                   /* Its value variable is vint */
    JSON_HEX,                   /* Its value variable is vhex */
    JSON_LINT,                  /* Its value variable is vlint */
    JSON_LHEX,                  /* Its value variable is vlhex */
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
/*
 * json_string_t - json对象的键或字符串类型的值
 * @type: json对象的类型，只在作为json对象的键时才有效
 * @escaped: 是否含转义字符
 * @alloced: 是否是在堆中分配，只在SAX接口中有效
 * @len: 字符串长度
 * @str: 字符串数据
 * @description: LJSON使用此结构就知道了字符串长度，可以加快数据处理
 */
typedef struct {
    uint32_t type:4;
    uint32_t escaped:1;
    uint32_t alloced:1;
    uint32_t reserved:2;
    uint32_t len:24;
    char *str;
} json_string_t;

/*
 * json_number_t - the json number object value
 * @description: LJSON supports decimal and hexadecimal, integer and long long integer.
 */
/*
 * json_number_t - json数字类型的值
 * @description: LJSON支持长整数和十六进制数
 */
typedef union {
    bool vbool;
    int32_t vint;
    uint32_t vhex;
    int64_t vlint;
    uint64_t vlhex;
    double vdbl;
} json_number_t;

#if JSON_SAX_APIS_SUPPORT
/*
 * json_sax_cmd_t - the beginning and end of JSON_ARRAY or JSON_OBJECT object
 * @description: We know that parentheses have two sides, `JSON_SAX_START` indicates left side,
 *   and `JSON_SAX_FINISH` indicates right side.
 */
/*
 * json_sax_cmd_t - SAX APIs中指示JSON_ARRAY或JSON_OBJECT的开始和结束
 * @description: 集合类型是括号包起来的，JSON_SAX_START表示左边括号, JSON_SAX_FINISH指示右边括号
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
/*
 * json_value_t - json对象的值
 * @vnum: 数字类型的值
 * @vstr: 字符串类型的值
 * @vcmd: SAX APIs指示集合对象的开始和结束
 * @head: 集合对象的子节点挂载的链表头
 * @description: LJSON使用union管理对象的值从而节省内存空间
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
/*
 * json_object - json对象
 * @list: 链表节点，指向下一个对象或父对象的链表头
 * @jkey: json对象的类型和键值，只有JSON_OBJECT的子对象才有键值
 * @value: json对象的值
 * @description: LJSON使用更紧凑的内存结构以节省内存
 */
typedef struct {
    struct json_list list;
    json_string_t jkey;
    json_value_t value;
} json_object;

/*
 * json_item_t - the json item which contains json object and hash code
 * @hash: the hash code of key, only child json objects of JSON_OBJECT has the value
 * @json: the json object
 * @description: LJSON uses hash code to accelerate access to member of JSON_OBJECT.
 */
/*
 * json_item_t - 包含json对象和hash值的结构
 * @hash: 只有JSON_OBJECT才有key值，才有key的hash值
 * @json: json对象
 * @description: LJSON使用hash值来加快获取JSON_OBJECT下的子对象
 */
typedef struct {
    uint32_t hash;
    json_object *json;
} json_item_t;

/*
 * json_items_t - the json items array
 * @conflicted: whether the key hashes are conflicted in items, only for JSON_OBJECT parent
 * @total: the total size of items
 * @count: the total number of child json objects of JSON_ARRAY or JSON_OBJECT
 * @json: the array to store child json objects
 * @description: LJSON uses it to store all sub-objects of JSON_ARRAY or JSON_OBJECT.
 */
/*
 * json_items_t - json_item_t 的管理结构
 * @conflicted: JSON_OBJECT下的子对象的key的hash值是否有冲突
 * @total: 数组的容量
 * @count: 数组的当前数量
 * @json: 存储json对象的数组
 * @description: LJSON使用它存储JSON_ARRAY或JSON_OBJECT下的所有子对象，使用json_get_items接口获取
 */
typedef struct {
    uint32_t conflicted:1;
    uint32_t reserved:31;
    uint32_t total;
    uint32_t count;
    json_item_t *items;
} json_items_t;

/**************** json classic editor / json常规编辑接口 ****************/

/*
 * json_memory_free - Free the ptr alloced by LJSON
 * @ptr: IN, the alloced ptr
 * @return: None
 * @description: the alloced ptr may be:
 *   1. the returned string by json_print_common or json_sax_print_finish when printing
 *      to string and json_print_ptr_t is not passed in
 *   2. the p of json_print_ptr_t which stores the string returned when printing to a string,
 *      or the internal buffer when printing to a file
 *   3. the LJSON style string alloced by LJSON classic(not pool) APIS
 */
/*
 * json_memory_free - 释放内存
 * @ptr: IN, 内存指针
 * @return: 无返回值
 * @description: 要释放的内存可能是:
 *   1. json_print_common 或 json_sax_print_finish 打印到字符串时返回的字符串，且没有传入 json_print_ptr_t
 *   2. json_print_ptr_t的p成员，它存储了打印到字符串时返回的字符串，或打印到文件时的内部缓冲
 *   3. 普通接口(非内存池接口)分配的字符串
 */
void json_memory_free(void *ptr);

/*
 * json_item_total_get - Get the total number of json tree (recursive)
 * @json: IN, the json object
 * @return: the total number
 */
/*
 * json_item_total_get - 递归获取json对象的总数
 * @json: IN, json对象
 * @return: 对象总数
 */
int json_item_total_get(json_object *json);

/*
 * json_del_object - Delete the json object, includes the child objects (recursive)
 * @json: IN, the json object
 * @return: None
 */
/*
 * json_del_object - 递归删除json对象(即挂载在它上面的子对象)
 * @json: IN, json对象
 * @return: 无返回值
 */
void json_del_object(json_object *json);

/*
 * json_new_object - Create a json object without value
 * @type: IN, the json object type
 * @return: NULL on failure, a pointer on success
 */
/*
 * json_new_object - 新建一个指定类型的无值对象
 * @type: IN, 对象类型
 * @return: 失败返回NULL；成功返回指针
 */
json_object *json_new_object(json_type_t type);

/*
 * json_create_item - Create a json object with value
 * @type: IN, the json object type
 * @value: IN, the json object value
 * @return: NULL on failure, a pointer on success
 */
/*
 * json_create_item - 新建一个指定类型的有值对象
 * @type: IN, 对象类型
 * @value: IN, 对象值
 * @return: 失败返回NULL；成功返回指针
 */
json_object *json_create_item(json_type_t type, void *value);
static inline json_object *json_create_null(void) { return json_new_object(JSON_NULL); }
static inline json_object *json_create_bool(bool value) { return json_create_item(JSON_BOOL, &value); }
static inline json_object *json_create_int(int32_t value) { return json_create_item(JSON_INT, &value); }
static inline json_object *json_create_hex(uint32_t value) { return json_create_item(JSON_HEX, &value); }
static inline json_object *json_create_lint(int64_t value) { return json_create_item(JSON_LINT, &value); }
static inline json_object *json_create_lhex(uint64_t value) { return json_create_item(JSON_LHEX, &value); }
static inline json_object *json_create_double(double value) { return json_create_item(JSON_DOUBLE, &value); }
static inline json_object *json_create_string(json_string_t *value) { return json_create_item(JSON_STRING, value); }
static inline json_object *json_create_array(void) { return json_new_object(JSON_ARRAY); }
static inline json_object *json_create_object(void) { return json_new_object(JSON_OBJECT); }

#ifdef __cplusplus
}
static inline json_object *json_create_unit(bool value) { return json_create_item(JSON_BOOL, &value); }
static inline json_object *json_create_unit(int32_t value) { return json_create_item(JSON_INT, &value); }
static inline json_object *json_create_unit(uint32_t value) { return json_create_item(JSON_HEX, &value); }
static inline json_object *json_create_unit(int64_t value) { return json_create_item(JSON_LINT, &value); }
static inline json_object *json_create_unit(uint64_t value) { return json_create_item(JSON_LHEX, &value); }
static inline json_object *json_create_unit(double value) { return json_create_item(JSON_DOUBLE, &value); }
static inline json_object *json_create_unit(json_string_t *value) { return json_create_item(JSON_STRING, value); }
extern "C" {
#else
/* C11泛型选择(Generic selection) */
#define json_create_unit(value)  _Generic((value), \
bool            : json_create_bool               , \
int32_t         : json_create_int                , \
uint32_t        : json_create_hex                , \
int64_t         : json_create_lint               , \
uint64_t        : json_create_lhex               , \
double          : json_create_double             , \
json_string_t*  : json_create_string)(value)
#endif

/*
 * json_create_item_array - Create a JSON_ARRAY object with a group of child json objects
 * @type: IN, the type of child json objects
 * @values: IN, the values of child json objects
 * @count: IN, the total number of child json objects
 * @return: NULL on failure, a pointer on success
 */
/*
 * json_create_item_array - 新建一个 JSON_ARRAY 的有子节点的对象
 * @type: IN, 子对象类型
 * @values: IN, 子对象值数组
 * @count: IN, 子对象数量
 * @return: 失败返回NULL；成功返回指针
 */
json_object *json_create_item_array(json_type_t type, void *values, int count);
static inline json_object *json_create_bool_array(bool *values, int count) { return json_create_item_array(JSON_BOOL, values, count); }
static inline json_object *json_create_int_array(int32_t *values, int count) { return json_create_item_array(JSON_INT, values, count); }
static inline json_object *json_create_hex_array(uint32_t *values, int count) { return json_create_item_array(JSON_HEX, values, count); }
static inline json_object *json_create_lint_array(int64_t *values, int count) { return json_create_item_array(JSON_LINT, values, count); }
static inline json_object *json_create_lhex_array(uint64_t *values, int count) { return json_create_item_array(JSON_LHEX, values, count); }
static inline json_object *json_create_double_array(double *values, int count) { return json_create_item_array(JSON_DOUBLE, values, count); }
static inline json_object *json_create_string_array(json_string_t *values, int count) { return json_create_item_array(JSON_STRING, values, count); }

#ifdef __cplusplus
}
static inline json_object *json_create_units(bool *values, int count) { return json_create_item_array(JSON_BOOL, values, count); }
static inline json_object *json_create_units(int32_t *values, int count) { return json_create_item_array(JSON_INT, values, count); }
static inline json_object *json_create_units(uint32_t *values, int count) { return json_create_item_array(JSON_HEX, values, count); }
static inline json_object *json_create_units(int64_t *values, int count) { return json_create_item_array(JSON_LINT, values, count); }
static inline json_object *json_create_units(uint64_t *values, int count) { return json_create_item_array(JSON_LHEX, values, count); }
static inline json_object *json_create_units(double *values, int count) { return json_create_item_array(JSON_DOUBLE, values, count); }
static inline json_object *json_create_units(json_string_t *values, int count) { return json_create_item_array(JSON_STRING, values, count); }
extern "C" {
#else
/* C11泛型选择(Generic selection) */
#define json_create_units(values, count)  _Generic((values), \
bool*           : json_create_bool_array                   , \
int32_t*        : json_create_int_array                    , \
uint32_t*       : json_create_hex_array                    , \
int64_t*        : json_create_lint_array                   , \
uint64_t*       : json_create_lhex_array                   , \
double*         : json_create_double_array                 , \
json_string_t*  : json_create_string_array)(values, count)
#endif

/*
 * json_string_info_update - Update the parameters for json string
 * @jstr: INOUT, the LJSON string
 * @return: None
 * @description: If jstr->len is not equal to 0, the parameters will not be updated
 *   if jstr->str contains `"  \  \b  \f  \n  \r  \t  \v`, v->escaped will be set
 */
/*
 * json_string_info_update - 更新json_string_t的信息
 * @jstr: INOUT, 要处理的对象
 * @return: 无返回值
 * @description: jstr->len是0时数据才会更新
 *   含有 `"  \  \b  \f  \n  \r  \t  \v` 字符时会设置v->escaped
 */
void json_string_info_update(json_string_t *jstr);

/*
 * json_string_hash_code - Calculate the hash code of json string
 * @jstr: IN, the LJSON string
 * @return: the hash value
 */
/*
 * json_string_hash_code - 计算字符串的hash值
 * @jstr: IN, 字符串数据
 * @return: hash值
 */
uint32_t json_string_hash_code(json_string_t *jstr);

/*
 * json_string_strdup - Strdup the LJSON string src to dst
 * @src: IN, the source string
 * @dst: OUT, the destination string
 * @return: -1 on failure, 0 on success
 */
/*
 * json_string_strdup - 复制字符串数据
 * @src: IN, 源
 * @dst: OUT, 目标
 * @return: 失败返回-1；成功返回0
 * @description: 如果dst足够容纳src的数据，将不会进行内存分配
 */
int json_string_strdup(json_string_t *src, json_string_t *dst);

/*
 * json_set_key - Set the key of json object
 * @json: IN, the json object to be set
 * @jkey: IN, the LJSON string key, allow length not to be set first by json_string_info_update
 * @return: -1 on failure, 0 on success
 */
/*
 * json_set_key - 设置json的key
 * @json: IN, 被设置的json对象
 * @jkey: IN, 要设置的key，可以预先填好jkey的len/escaped成员接口加快速度
 * @return: 失败返回-1；成功返回0
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
/*
 * json_set_string_value - 设置JSON_STRING类型的json对象的值
 * @json: IN, 被设置的json对象
 * @jstr: IN, 要设置的值, 可以预先填好jkey的len/escaped成员接口加快速度
 * @return: 失败返回-1；成功返回0
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
/*
 * json_get_number_value - 获取数字对象的值
 * @json: IN, json对象
 * @type: IN, 获取值value的类型
 * @value: OUT, 回写获取到的值
 * @return: 失败返回-1；成功且类型对应返回0；成功且类型强转返回正数
 */
int json_get_number_value(json_object *json, json_type_t type, void *value);
static inline bool json_get_bool_value(json_object *json) { bool value = 0; json_get_number_value(json, JSON_BOOL, &value); return value; }
static inline int32_t json_get_int_value(json_object *json) { int32_t value = 0; json_get_number_value(json, JSON_INT, &value); return value; }
static inline uint32_t json_get_hex_value(json_object *json) { uint32_t value = 0; json_get_number_value(json, JSON_HEX, &value); return value; }
static inline int64_t json_get_lint_value(json_object *json) { int64_t value = 0; json_get_number_value(json, JSON_LINT, &value); return value; }
static inline uint64_t json_get_lhex_value(json_object *json) { uint64_t value = 0; json_get_number_value(json, JSON_LHEX, &value); return value; }
static inline double json_get_double_value(json_object *json) { double value = 0; json_get_number_value(json, JSON_DOUBLE, &value); return value; }

/*
 * json_set_number_value - Set the value of numerical object
 * @json: IN, the json object
 * @type: IN, the json object type
 * @value: IN, the json object value
 * @return: -1 on failure, 0 on success, >0(original type) on success with type cast
 */
/*
 * json_set_number_value - 设置数字对象的值
 * @json: IN, json对象
 * @type: IN, 设置的值value的类型
 * @value: IN, 设置的值
 * @return: 失败返回-1；成功且类型对应返回0；成功且类型强转返回原先的类型
 */
int json_set_number_value(json_object *json, json_type_t type, void *value);
static inline int json_set_bool_value(json_object *json, bool value) { return json_set_number_value(json, JSON_BOOL, &value); }
static inline int json_set_int_value(json_object *json, int32_t value) { return json_set_number_value(json, JSON_INT, &value); }
static inline int json_set_hex_value(json_object *json, uint32_t value) { return json_set_number_value(json, JSON_HEX, &value); }
static inline int json_set_lint_value(json_object *json, int64_t value) { return json_set_number_value(json, JSON_LINT, &value); }
static inline int json_set_lhex_value(json_object *json, uint64_t value) { return json_set_number_value(json, JSON_LHEX, &value); }
static inline int json_set_double_value(json_object *json, double value) { return json_set_number_value(json, JSON_DOUBLE, &value); }

#ifdef __cplusplus
}
static inline int json_set_number(json_object *json, bool value) { return json_set_number_value(json, JSON_BOOL, &value); }
static inline int json_set_number(json_object *json, int32_t value) { return json_set_number_value(json, JSON_INT, &value); }
static inline int json_set_number(json_object *json, uint32_t value) { return json_set_number_value(json, JSON_HEX, &value); }
static inline int json_set_number(json_object *json, int64_t value) { return json_set_number_value(json, JSON_LINT, &value); }
static inline int json_set_number(json_object *json, uint64_t value) { return json_set_number_value(json, JSON_LHEX, &value); }
static inline int json_set_number(json_object *json, double value) { return json_set_number_value(json, JSON_DOUBLE, &value); }
extern "C" {
#else
/* C11泛型选择(Generic selection) */
#define json_set_number(json, value)  _Generic((value), \
bool            : json_set_bool_value                 , \
int32_t         : json_set_int_value                  , \
uint32_t        : json_set_hex_value                  , \
int64_t         : json_set_lint_value                 , \
uint64_t        : json_set_lhex_value                 , \
double          : json_set_double_value)(json, value)
#endif

/*
 * json_get_array_size - Get the total child objects of JSON_ARRAY object (not recursive)
 * @json: IN, the JSON_ARRAY object
 * @return: the total child objects
 */
/*
 * json_get_array_size - 获取JSON_ARRAY类型对象的子对象个数(不递归)
 * @json: IN, json对象
 * @return: 子对象数量
 */
int json_get_array_size(json_object *json);

/*
 * json_get_object_size - Get the total child objects of JSON_OBJECT object (not recursive)
 * @json: IN, the JSON_OBJECT object
 * @return: the total child objects
 */
/*
 * json_get_object_size - 获取JSON_OBJECT类型对象的子对象个数(不递归)
 * @json: IN, json对象
 * @return: 子对象数量
 */
int json_get_object_size(json_object *json);

/*
 * json_get_array_item - Get the specified object in JSON_ARRAY object
 * @json: IN, the JSON_ARRAY object
 * @seq: IN, the sequence number
 * &prev: OUT, to store the previous JSON object, it can be NULL
 * @return: NULL on failure, a pointer on success
 */
/*
 * json_get_array_item - 获取JSON_ARRAY类型对象的指定序号的子对象
 * @json: IN, json对象
 * @seq: IN, 序号
 * &prev: OUT, 存储前一个对象，主要是删除会用到，不是删除时可以设为NULL
 * @return: 失败返回NULL；成功返回指针
 */
json_object *json_get_array_item(json_object *json, int seq, json_object **prev);

/*
 * json_get_object_item - Get the specified object in JSON_OBJECT object
 * @json: IN, the JSON_OBJECT object
 * @key: IN, the specified key
 * &prev: OUT, to store the previous JSON object, it can be NULL
 * @return: NULL on failure, a pointer on success
 */
/*
 * json_get_object_item - 获取JSON_ARRAY类型对象的指定key的子对象
 * @json: IN, json对象
 * @seq: key, 键值
 * &prev: OUT, 存储前一个对象，主要是删除会用到，不是删除时可以设为NULL
 * @return: 失败返回NULL；成功返回指针
 */
json_object *json_get_object_item(json_object *json, const char *key, json_object **prev);

/*
 * json_search_object_item - Search the specified object in chiild items of JSON_OBJECT object
 * @items: IN, all child json objects of JSON_OBJECT
 * @jkey: IN, the searched LJSON string key, allow length not to be set first by json_string_info_update
 * @hash: IN, the hash value for searched key, if it is zero, the func will calculate actual hash
 * @return: NULL on failure, a pointer on success
 * @description: LJSON uses dichotomy to search the specific json object
 */
/*
 * json_search_object_item - 从items存储的数组中找到指定key的对象
 * @items: IN, 存储json对象数组的管理结构
 * @jkey: IN, 要查找的key，可以预先填好jkey的len/escaped成员接口加快速度
 * @hash: IN, 要查找的hash值
 * @return: 失败返回NULL；成功返回指针
 * @description: LJSON使用二分法和hash值加快查找速度
 */
json_object *json_search_object_item(json_items_t *items, json_string_t *jkey, uint32_t hash);

/*
 * json_free_items - Free alloced memory in json_items_t
 * @items: INOUT, the json items
 * @return: none
 */
/*
 * json_free_items - 释放items的内存
 * @items: IN, 存储json对象数组的管理结构
 * @return: 无返回值
 */
void json_free_items(json_items_t *items);

/*
 * json_get_items - Get all child json objects of JSON_ARRAY or JSON_OBJECT
 * @json: IN, the JSON_ARRAY or JSON_OBJECT object to get
 * @items: INOUT, the json items
 * @return: -1 on failure, 0 on success
 * @description: LJSON uses it to accelerate access to member of JSON_ARRAY or JSON_OBJECT
 */
/*
 * json_get_items - 获取JSON_ARRAY或JSON_OBJECT类型的对象下的所有子对象
 * @json: IN, json对象
 * @items: INOUT, 回写获取到的所有子对象
 * @return: 失败返回-1；成功返回0
 * @description: LJSON将子对象由链表改存储为数组，可以显著加快获取速度
 */
int json_get_items(json_object *json, json_items_t *items);

/*
 * json_add_item_to_array - Add the specified object to the JSON_ARRAY object
 * @array: IN, the JSON_ARRAY object
 * @item: IN, the json object to add
 * @return: -1 on failure, 0 on success
 * @description: The item will be added to the end, once successfully added,
 *   it is no longer necessary to manually delete item.
 */
/*
 * json_add_item_to_array - 将指定的json对象添加到JSON_ARRAY对象中
 * @array: IN, 目标JSON_ARRAY对象
 * @item: IN, 要添加的json对象
 * @return: 失败返回-1；成功返回0
 * @description: 该对象将被添加到数组的末尾，一旦成功添加，无需手动删除该对象。
 */
int json_add_item_to_array(json_object *array, json_object *item);

/*
 * json_add_item_to_object - Add the specified object to the JSON_OBJECT object
 * @object: IN, the JSON_OBJECT object
 * @item: IN, the json object to add, users should set the key of item first
 * @return: -1 on failure, 0 on success
 * @description: The item will be added to the end, once successfully added,
 *   it is no longer necessary to manually delete item.
 */
/*
 * json_add_item_to_object - 将指定的json对象添加到JSON_OBJECT对象中
 * @object: IN, 目标JSON_OBJECT对象
 * @item: IN, 要添加的json对象，用户应首先设置该对象的键
 * @return: 失败返回-1；成功返回0
 * @description: 该对象将被添加到对象的末尾，一旦成功添加，无需手动删除该对象。
 */
int json_add_item_to_object(json_object *object, json_object *item);

/*
 * json_detach_item_from_array - Detach the specified object in JSON_ARRAY object
 * @json: IN, the JSON_ARRAY object
 * @seq: IN, the sequence number
 * @return: NULL on failure, a pointer (the detached object) on success
 * @description: After use, users need to delete the returned object in classic jsons,
 *   don't delete it in pool jsons.
 */
/*
 * json_detach_item_from_array - 从JSON_ARRAY对象中分离指定的对象
 * @json: IN, 目标JSON_ARRAY对象
 * @seq: IN, 要分离的对象的序号
 * @return: 失败返回NULL；成功时返回分离的对象指针
 * @description: 使用后，用户需要手动删除返回的对象(普通JSON），在Pool JSON中不要删除
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
/*
 * json_detach_item_from_object - 从JSON_OBJECT对象中分离指定的对象
 * @json: IN, 目标JSON_OBJECT对象
 * @key: IN, 要分离的对象的键
 * @return: 失败返回NULL；成功时返回分离的对象指针
 * @description: 使用后，用户需要手动删除返回的对象(普通JSON），在Pool JSON中不要删除
 */
json_object *json_detach_item_from_object(json_object *json, const char *key);

/*
 * json_del_item_from_array - Delete the specified object in JSON_ARRAY object
 * @json: IN, the JSON_ARRAY object
 * @seq: IN, the sequence number
 * @return: -1 on not found, 0 on found and deleted
 */
/*
 * json_del_item_from_array - 从JSON_ARRAY对象中删除指定的对象
 * @json: IN, 目标JSON_ARRAY对象
 * @seq: IN, 要删除的对象的序号
 * @return: 失败返回-1(未找到)；成功返回0(找到并删除)
 */
int json_del_item_from_array(json_object *json, int seq);

/*
 * json_del_item_from_object - Delete the specified object in JSON_OBJECT object
 * @json: IN, the JSON_OBJECT object
 * @key: IN, the specified key
 * @return: -1 on not found, 0 on found and deleted
 */
/*
 * json_del_item_from_object - 从JSON_OBJECT对象中删除指定的对象
 * @json: IN, 目标JSON_OBJECT对象
 * @key: IN, 要删除的对象的键
 * @return: 失败返回-1(未找到)；成功返回0(找到并删除)
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
/*
 * json_replace_item_in_array - 用新的json对象替换JSON_ARRAY对象中的指定对象
 * @array: IN, 目标JSON_ARRAY对象
 * @seq: IN, 要替换的对象的序号
 * @new_item: IN, 新的json对象
 * @return: 失败返回-1；成功返回0
 * @description: 如果序号无效，new_item将被添加到数组的末尾
 */
int json_replace_item_in_array(json_object *array, int seq, json_object *new_item);

/*
 * json_replace_item_in_object - Replace the specified object in JSON_OBJECT object with new json object
 * @object: IN, the JSON_OBJECT object
 * @new_item: IN, the new json object, users should set the key of new_item first
 * @return: -1 on failure, 0 on success
 * @description: If key is not satisfied, new_item will be added to the end.
 */
/*
 * json_replace_item_in_object - 用新的json对象替换JSON_OBJECT对象中的指定对象
 * @object: IN, 目标JSON_OBJECT对象
 * @new_item: IN, 新的json对象，用户应首先设置该对象的键
 * @return: 失败返回-1；成功返回0
 * @description: 如果键无效，new_item将被添加到对象的末尾
 */
int json_replace_item_in_object(json_object *object, json_object *new_item);

/*
 * json_deepcopy - Deep copy the json object (recursive)
 * @json: IN, the source json object
 * @return: NULL on failure, a pointer on success
 */
/*
 * json_deepcopy - 深度递归复制json对象
 * @json: IN, 源json对象
 * @return: 失败返回NULL；成功时返回复制的对象指针
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
/*
 * json_copy_item_to_array - 复制指定的json对象并将其添加到JSON_ARRAY对象中
 * @array: IN, 目标JSON_ARRAY对象
 * @item: IN, 要复制并添加的json对象
 * @return: 失败返回-1；成功返回0
 * @description: 该对象将被添加到数组的末尾，成功添加后，仍需手动删除该对象
 */
int json_copy_item_to_array(json_object *array, json_object *item);

/*
 * json_copy_item_to_object - Copy the specified object, the add it to the JSON_OBJECT object
 * @object: IN, the JSON_OBJECT object
 * @item: IN, the json object to copy and add, users should set the key of item first
 * @return: -1 on failure, 0 on success
 * @description: The item will be added to the end, once successfully added,
 *   it is also necessary to manually delete item.
 */
/*
 * json_copy_item_to_object - 复制指定的json对象并将其添加到JSON_OBJECT对象中
 * @object: IN, 目标JSON_OBJECT对象
 * @item: IN, 要复制并添加的json对象，用户应首先设置该对象的键
 * @return: 失败返回-1；成功返回0
 * @description: 该对象将被添加到对象的末尾，成功添加后，仍需手动删除该对象
 */
int json_copy_item_to_object(json_object *object, json_object *item);

/*
 * json_add_new_item_to_array - Create a new object, the add it to the JSON_ARRAY object
 * @array: IN, the JSON_ARRAY object
 * @type: IN, the json object type
 * @value: IN, the json object value
 * @return: NULL on failure, a pointer on success
 * @description: the pointer points to the created json object
 */
/*
 * json_add_new_item_to_array - 创建一个新的json对象并将其添加到JSON_ARRAY对象中
 * @array: IN, 目标JSON_ARRAY对象
 * @type: IN, json对象的类型
 * @value: IN, json对象的值
 * @return: 失败返回NULL；成功时返回创建的json对象指针
 */
json_object *json_add_new_item_to_array(json_object *array, json_type_t type, void* value);
static inline json_object *json_add_null_to_array(json_object *array) { return json_add_new_item_to_array(array, JSON_NULL, NULL); }
static inline json_object *json_add_bool_to_array(json_object *array, bool value) { return json_add_new_item_to_array(array, JSON_BOOL, &value); }
static inline json_object *json_add_int_to_array(json_object *array, int32_t value) { return json_add_new_item_to_array(array, JSON_INT, &value); }
static inline json_object *json_add_hex_to_array(json_object *array, uint32_t value) { return json_add_new_item_to_array(array, JSON_HEX, &value); }
static inline json_object *json_add_lint_to_array(json_object *array, int64_t value) { return json_add_new_item_to_array(array, JSON_LINT, &value); }
static inline json_object *json_add_lhex_to_array(json_object *array, uint64_t value) { return json_add_new_item_to_array(array, JSON_LHEX, &value); }
static inline json_object *json_add_double_to_array(json_object *array, double value) { return json_add_new_item_to_array(array, JSON_DOUBLE, &value); }
static inline json_object *json_add_string_to_array(json_object *array, json_string_t *value) { return json_add_new_item_to_array(array, JSON_STRING, value); }
static inline json_object *json_add_array_to_array(json_object *array) { return json_add_new_item_to_array(array, JSON_ARRAY, NULL); }
static inline json_object *json_add_object_to_array(json_object *array) { return json_add_new_item_to_array(array, JSON_OBJECT, NULL); }

#ifdef __cplusplus
}
static inline json_object *json_array_append(json_object *array, bool value) { return json_add_new_item_to_array(array, JSON_BOOL, &value); }
static inline json_object *json_array_append(json_object *array, int32_t value) { return json_add_new_item_to_array(array, JSON_INT, &value); }
static inline json_object *json_array_append(json_object *array, uint32_t value) { return json_add_new_item_to_array(array, JSON_HEX, &value); }
static inline json_object *json_array_append(json_object *array, int64_t value) { return json_add_new_item_to_array(array, JSON_LINT, &value); }
static inline json_object *json_array_append(json_object *array, uint64_t value) { return json_add_new_item_to_array(array, JSON_LHEX, &value); }
static inline json_object *json_array_append(json_object *array, double value) { return json_add_new_item_to_array(array, JSON_DOUBLE, &value); }
static inline json_object *json_array_append(json_object *array, json_string_t *value) { return json_add_new_item_to_array(array, JSON_STRING, value); }
extern "C" {
#else
/* C11泛型选择(Generic selection) */
#define json_array_append(array, value)  _Generic((value), \
bool            : json_add_bool_to_array                 , \
int32_t         : json_add_int_to_array                  , \
uint32_t        : json_add_hex_to_array                  , \
int64_t         : json_add_lint_to_array                 , \
uint64_t        : json_add_lhex_to_array                 , \
double          : json_add_double_to_array               , \
json_string_t*  : json_add_string_to_array)(array, value)
#endif

#define json_array_append_null(array)   json_add_null_to_array  (array)
#define json_array_append_array(array)  json_add_array_to_array (array)
#define json_array_append_object(array) json_add_object_to_array(array)

/*
 * json_add_new_item_to_object - Create a new object, the add it to the JSON_OBJECT object
 * @object: IN, the JSON_OBJECT object
 * @type: the json object type
 * @jkey: IN, the LJSON string key, allow length not to be set first by json_string_info_update
 * @value: IN, the json object value
 * @return: NULL on failure, a pointer on success
 * @description: the pointer points to the created json object
 */
/*
 * json_add_new_item_to_object - 创建一个新的json对象并将其添加到JSON_OBJECT对象中
 * @object: IN, 目标JSON_OBJECT对象
 * @type: IN, json对象的类型
 * @jkey: IN, json对象的键，允许长度未预先设置
 * @value: IN, json对象的值
 * @return: 失败返回NULL；成功时返回创建的json对象指针
 */
json_object *json_add_new_item_to_object(json_object *object, json_type_t type, json_string_t *jkey, void* value);
static inline json_object *json_add_null_to_object(json_object *object, json_string_t *jkey) { return json_add_new_item_to_object(object, JSON_NULL, jkey, NULL); }
static inline json_object *json_add_bool_to_object(json_object *object, json_string_t *jkey, bool value) { return json_add_new_item_to_object(object, JSON_BOOL, jkey, &value); }
static inline json_object *json_add_int_to_object(json_object *object, json_string_t *jkey, int32_t value) { return json_add_new_item_to_object(object, JSON_INT, jkey, &value); }
static inline json_object *json_add_hex_to_object(json_object *object, json_string_t *jkey, uint32_t value) { return json_add_new_item_to_object(object, JSON_HEX, jkey, &value); }
static inline json_object *json_add_lint_to_object(json_object *object, json_string_t *jkey, int64_t value) { return json_add_new_item_to_object(object, JSON_LINT, jkey, &value); }
static inline json_object *json_add_lhex_to_object(json_object *object, json_string_t *jkey, uint64_t value) { return json_add_new_item_to_object(object, JSON_LHEX, jkey, &value); }
static inline json_object *json_add_double_to_object(json_object *object, json_string_t *jkey, double value) { return json_add_new_item_to_object(object, JSON_DOUBLE, jkey, &value); }
static inline json_object *json_add_string_to_object(json_object *object, json_string_t *jkey, json_string_t *value) { return json_add_new_item_to_object(object, JSON_STRING, jkey, value); }
static inline json_object *json_add_array_to_object(json_object *object, json_string_t *jkey) { return json_add_new_item_to_object(object, JSON_ARRAY, jkey, NULL); }
static inline json_object *json_add_object_to_object(json_object *object, json_string_t *jkey) { return json_add_new_item_to_object(object, JSON_OBJECT, jkey, NULL); }

#ifdef __cplusplus
}
static inline json_object *json_object_append(json_object *object, json_string_t *jkey, bool value) { return json_add_new_item_to_object(object, JSON_BOOL, jkey, &value); }
static inline json_object *json_object_append(json_object *object, json_string_t *jkey, int32_t value) { return json_add_new_item_to_object(object, JSON_INT, jkey, &value); }
static inline json_object *json_object_append(json_object *object, json_string_t *jkey, uint32_t value) { return json_add_new_item_to_object(object, JSON_HEX, jkey, &value); }
static inline json_object *json_object_append(json_object *object, json_string_t *jkey, int64_t value) { return json_add_new_item_to_object(object, JSON_LINT, jkey, &value); }
static inline json_object *json_object_append(json_object *object, json_string_t *jkey, uint64_t value) { return json_add_new_item_to_object(object, JSON_LHEX, jkey, &value); }
static inline json_object *json_object_append(json_object *object, json_string_t *jkey, double value) { return json_add_new_item_to_object(object, JSON_DOUBLE, jkey, &value); }
static inline json_object *json_object_append(json_object *object, json_string_t *jkey, json_string_t *value) { return json_add_new_item_to_object(object, JSON_STRING, jkey, value); }
extern "C" {
#else
/* C11泛型选择(Generic selection) */
#define json_object_append(object, jkey, value)  _Generic((value), \
bool            : json_add_bool_to_object,                         \
int32_t         : json_add_int_to_object,                          \
uint32_t        : json_add_hex_to_object,                          \
int64_t         : json_add_lint_to_object,                         \
uint64_t        : json_add_lhex_to_object,                         \
double          : json_add_double_to_object,                       \
json_string_t*  : json_add_string_to_object)(object, jkey, value)
#endif

#define json_object_append_null(object, jkey)   json_add_null_to_object  (object, jkey)
#define json_object_append_array(object, jkey)  json_add_array_to_object (object, jkey)
#define json_object_append_object(object, jkey) json_add_object_to_object(object, jkey)

/*
 * The below APIs are also available to pool json / 下面的接口也可用于pjson:
 * json_item_total_get
 * json_string_info_update
 * json_get_number_value / ...
 * json_set_number_value / ...
 * json_get_array_size
 * json_get_object_size
 * json_get_array_item
 * json_get_object_item
 * json_search_object_item
 * json_free_items
 * json_get_items
 * json_add_item_to_array
 * json_add_item_to_object
 * json_detach_item_from_array
 * json_detach_item_from_object
 */

/**************** json pool editor / 内存块json的编辑接口 ****************/

/*
 * json_mem_node_t - the block memory node
 * @list: the list value, LJSON associates `list` to the `head` of json_mem_mgr_t
 *   or the `list` of brother json_mem_node_t
 * @size: the memory size
 * @ptr: the memory pointer
 * @cur: the current memory pointer
 * @description: LJSON can use the block memory to accelerate memory allocation and save memory space.
 */
/*
 * json_mem_node_t - 块内存节点
 * @list: 链表节点，LJSON将 `list` 关联到 `json_mem_mgr_t` 的 `head` 或兄弟 `json_mem_node_t` 的 `list`
 * @size: 内存大小
 * @ptr: 内存指针
 * @cur: 当前内存指针
 * @description: LJSON使用块内存来加速内存分配并节省内存空间，内存块只能统一申请统一释放，无法单独释放某个对象
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
 * @mem_size: the default memory size to allocate, its default value is JSON_POOL_MEM_SIZE_DEF(8192)
 * @cur_node: the current block memory node
 * @description: the manage node manages a group of block memory nodes.
 */
/*
 * json_mem_mgr_t - 管理块内存节点的结构
 * @head: 链表头，LJSON通过链表头管理块内存节点
 * @mem_size: 默认分配的内存大小，默认值为 `JSON_POOL_MEM_SIZE_DEF`(8192)
 * @cur_node: 当前块内存节点
 * @description: 该管理节点管理一组块内存节点
 */
typedef struct {
    struct json_list_head head;
    size_t mem_size;
    json_mem_node_t *cur_node;
} json_mem_mgr_t;

/*
 * json_mem_t - the head to manage all types of block memory
 * @valid: IN, is there already memory allocation available to speed up
 *   frequent parsing of small JSON files
 * @obj_mgr: the node to manage json_object
 * @key_mgr: the node to manage the value of key
 * @str_mgr: the node to manage the value of JSON_STRING object
 * @description: The reason for dividing into multiple management nodes is that
 * there is a memory address alignment requirement for json_object.
 */
/*
 * json_mem_t - 管理所有类型块内存的结构
 * @valid: IN, 是否已经有内存分配可用于加速频繁解析小型JSON文件
 * @obj_mgr: 管理 `json_object` 的节点
 * @key_mgr: 管理键值的节点
 * @str_mgr: 管理 `JSON_STRING` 对象值的节点
 * @description: 划分为多个管理节点的原因是 `json_object` 有内存地址对齐要求
 *   valid设为false时，JSON解析函数内部会重新初始化mem，所以此时mem一定不要有挂载内存节点
 */
typedef struct {
    bool valid;
    json_mem_mgr_t obj_mgr;
    json_mem_mgr_t key_mgr;
    json_mem_mgr_t str_mgr;
} json_mem_t;

/*
 * pjson_memory_free - Free all block memory
 * @mem: IN, the block memory manager
 * @description: Users need to call it to delete all json objects, not one.
 */
/*
 * pjson_memory_free - 释放所有块内存
 * @mem: IN, 块内存管理器
 * @description: 用户需要调用此接口来删除所有json对象，无法单独释放某个json对象的内存
 */
void pjson_memory_free(json_mem_t *mem);

/*
 * pjson_memory_init - Initializate the block memory manager
 * @mem: IN, the block memory manager to be initializated
 * @return: None
 * @description: Users need to call it before using block memory apis.
 *   User can re-set `mem_size` after calling it.
 */
/*
 * pjson_memory_init - 初始化块内存管理器
 * @mem: IN, 要初始化的块内存管理器
 * @return: 无返回值
 * @description: 用户在使用块内存API之前需要调用此接口；调用后，用户可以重新设置 `mem_size`
 */
void pjson_memory_init(json_mem_t *mem);

/*
 * pjson_memory_refresh - Refresh all block memory
 * @mem: IN, the block memory manager
 * @description: It will retain the first memory node and release the other memory nodes.
 *   The retained nodes will be updated to an unused state and can be used to speed up
 *   frequent parsing of small JSON files with reusing memory.
 */
/*
 * pjson_memory_refresh - 刷新所有块内存
 * @mem: IN, 块内存管理器
 * @description: 此接口会保留第一个内存节点并释放其他内存节点，保留的节点将被更新为未使用状态，
 *   可以用于加速频繁解析小型JSON文件时的内存复用
 */
void pjson_memory_refresh(json_mem_t *mem);

/*
 * pjson_memory_statistics - Count the alloced memory of the memory blocks
 * @mgr: IN, the block memory manager node
 * @return: the alloced memory size
 */
/*
 * pjson_memory_statistics - 统计块内存管理器节点已分配的内存大小
 * @mgr: IN, 块内存管理器节点
 * @return: 已分配的内存大小
 */
int pjson_memory_statistics(json_mem_mgr_t *mgr);

/*
 * pjson_new_object - Create a pool json object without value
 * @type: IN, the json object type
 * @mem: IN, the block memory manager
 * @return: NULL on failure, a pointer on success
 */
/*
 * pjson_new_object - 创建一个无值的池json对象
 * @type: IN, json对象类型
 * @mem: IN, 块内存管理器
 * @return: 失败返回 NULL；成功返回指针
 */
json_object *pjson_new_object(json_type_t type, json_mem_t *mem);

/*
 * pjson_create_item - Create a pool json object with value
 * @type: IN, the json object type
 * @value: IN, the json object value
 * @mem: IN, the block memory manager
 * @return: NULL on failure, a pointer on success
 */
/*
 * pjson_create_item - 创建一个有值的池json对象
 * @type: IN, json对象类型
 * @value: IN, json对象值
 * @mem: IN, 块内存管理器
 * @return: 失败返回 NULL；成功返回指针
 */
json_object *pjson_create_item(json_type_t type, void *value, json_mem_t *mem);
static inline json_object *pjson_create_null(json_mem_t *mem) { return pjson_new_object(JSON_NULL, mem); }
static inline json_object *pjson_create_bool(bool value, json_mem_t *mem) { return pjson_create_item(JSON_BOOL, &value, mem); }
static inline json_object *pjson_create_int(int32_t value, json_mem_t *mem) { return pjson_create_item(JSON_INT, &value, mem); }
static inline json_object *pjson_create_hex(uint32_t value, json_mem_t *mem) { return pjson_create_item(JSON_HEX, &value, mem); }
static inline json_object *pjson_create_lint(int64_t value, json_mem_t *mem) { return pjson_create_item(JSON_LINT, &value, mem); }
static inline json_object *pjson_create_lhex(uint64_t value, json_mem_t *mem) { return pjson_create_item(JSON_LHEX, &value, mem); }
static inline json_object *pjson_create_double(double value, json_mem_t *mem) { return pjson_create_item(JSON_DOUBLE, &value, mem); }
static inline json_object *pjson_create_string(json_string_t *value, json_mem_t *mem) { return pjson_create_item(JSON_STRING, value, mem); }
static inline json_object *pjson_create_array(json_mem_t *mem) { return pjson_new_object(JSON_ARRAY, mem); }
static inline json_object *pjson_create_object(json_mem_t *mem) { return pjson_new_object(JSON_OBJECT, mem); }

#ifdef __cplusplus
}
static inline json_object *pjson_create_unit(bool value, json_mem_t *mem) { return pjson_create_item(JSON_BOOL, &value, mem); }
static inline json_object *pjson_create_unit(int32_t value, json_mem_t *mem) { return pjson_create_item(JSON_INT, &value, mem); }
static inline json_object *pjson_create_unit(uint32_t value, json_mem_t *mem) { return pjson_create_item(JSON_HEX, &value, mem); }
static inline json_object *pjson_create_unit(int64_t value, json_mem_t *mem) { return pjson_create_item(JSON_LINT, &value, mem); }
static inline json_object *pjson_create_unit(uint64_t value, json_mem_t *mem) { return pjson_create_item(JSON_LHEX, &value, mem); }
static inline json_object *pjson_create_unit(double value, json_mem_t *mem) { return pjson_create_item(JSON_DOUBLE, &value, mem); }
static inline json_object *pjson_create_unit(json_string_t *value, json_mem_t *mem) { return pjson_create_item(JSON_STRING, value, mem); }
extern "C" {
#else
/* C11泛型选择(Generic selection) */
#define pjson_create_unit(value, mem)  _Generic((value), \
bool            : pjson_create_bool                    , \
int32_t         : pjson_create_int                     , \
uint32_t        : pjson_create_hex                     , \
int64_t         : pjson_create_lint                    , \
uint64_t        : pjson_create_lhex                    , \
double          : pjson_create_double                  , \
json_string_t*  : pjson_create_string)(value, mem)
#endif

/*
 * pjson_create_item_array - Create a pool JSON_ARRAY object with a group of child json objects
 * @type: IN, the type of child json objects
 * @values: IN, the values of child json objects
 * @count: IN, the total number of child json objects
 * @mem: IN, the block memory manager
 * @return: NULL on failure, a pointer on success
 */
/*
 * pjson_create_item_array - 创建一个包含一组子json对象的池JSON_ARRAY对象
 * @item_type: IN, 子json对象的类型
 * @values: IN, 子json对象的值数组
 * @count: IN, 子json对象的总数
 * @mem: IN, 块内存管理器
 * @return: 失败返回 NULL；成功返回指针
 */
json_object *pjson_create_item_array(json_type_t item_type, void *values, int count, json_mem_t *mem);
static inline json_object *pjson_create_bool_array(bool *values, int count, json_mem_t *mem) { return pjson_create_item_array(JSON_BOOL, values, count, mem); }
static inline json_object *pjson_create_int_array(int32_t *values, int count, json_mem_t *mem) { return pjson_create_item_array(JSON_INT, values, count, mem); }
static inline json_object *pjson_create_hex_array(uint32_t *values, int count, json_mem_t *mem) { return pjson_create_item_array(JSON_HEX, values, count, mem); }
static inline json_object *pjson_create_lint_array(int64_t *values, int count, json_mem_t *mem) { return pjson_create_item_array(JSON_LINT, values, count, mem); }
static inline json_object *pjson_create_lhex_array(uint64_t *values, int count, json_mem_t *mem) { return pjson_create_item_array(JSON_LHEX, values, count, mem); }
static inline json_object *pjson_create_double_array(double *values, int count, json_mem_t *mem) { return pjson_create_item_array(JSON_DOUBLE, values, count, mem); }
static inline json_object *pjson_create_string_array(json_string_t *values, int count, json_mem_t *mem) { return pjson_create_item_array(JSON_STRING, values, count, mem); }

#ifdef __cplusplus
}
static inline json_object *pjson_create_units(bool *values, int count, json_mem_t *mem) { return pjson_create_item_array(JSON_BOOL, values, count, mem); }
static inline json_object *pjson_create_units(int32_t *values, int count, json_mem_t *mem) { return pjson_create_item_array(JSON_INT, values, count, mem); }
static inline json_object *pjson_create_units(uint32_t *values, int count, json_mem_t *mem) { return pjson_create_item_array(JSON_HEX, values, count, mem); }
static inline json_object *pjson_create_units(int64_t *values, int count, json_mem_t *mem) { return pjson_create_item_array(JSON_LINT, values, count, mem); }
static inline json_object *pjson_create_units(uint64_t *values, int count, json_mem_t *mem) { return pjson_create_item_array(JSON_LHEX, values, count, mem); }
static inline json_object *pjson_create_units(double *values, int count, json_mem_t *mem) { return pjson_create_item_array(JSON_DOUBLE, values, count, mem); }
static inline json_object *pjson_create_units(json_string_t *values, int count, json_mem_t *mem) { return pjson_create_item_array(JSON_STRING, values, count, mem); }
extern "C" {
#else
/* C11泛型选择(Generic selection) */
#define pjson_create_units(values, count, mem)  _Generic((values), \
bool*           : pjson_create_bool_array                        , \
int32_t*        : pjson_create_int_array                         , \
uint32_t*       : pjson_create_hex_array                         , \
int64_t*        : pjson_create_lint_array                        , \
uint64_t*       : pjson_create_lhex_array                        , \
double*         : pjson_create_double_array                      , \
json_string_t*  : pjson_create_string_array)(values, count, mem)
#endif

/*
 * pjson_string_strdup - Strdup the LJSON string src to dst
 * @src: IN, the source string
 * @dst: OUT, the destination string
 * @return: -1 on failure, 0 on success
 */
/*
 * pjson_string_strdup - 复制LJSON字符串src到dst
 * @src: IN, 源字符串
 * @dst: OUT, 目标字符串
 * @mgr: IN, 块内存管理器
 * @return: 失败返回 -1；成功返回 0
 */
int pjson_string_strdup(json_string_t *src, json_string_t *dst, json_mem_mgr_t *mgr);

/*
 * pjson_set_key - Set the key of json object
 * @json: IN, the json object to be set
 * @jkey: IN, the LJSON string key, allow length not to be set first by json_string_info_update
 * @mem: IN, the block memory manager
 * @return: -1 on failure, 0 on success
 */
/*
 * pjson_set_key - 设置json对象的键
 * @json: IN, 要设置的json对象
 * @jkey: IN, LJSON字符串键，允许长度未预先通过 json_string_info_update 设置
 * @mem: IN, 块内存管理器
 * @return: 失败返回 -1；成功返回 0
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
/*
 * pjson_set_string_value - 设置 JSON_STRING 对象的字符串值
 * @json: IN, 要设置的json对象
 * @jstr: IN, LJSON字符串值，允许长度未预先通过 json_string_info_update 设置
 * @mem: IN, 块内存管理器
 * @return: 失败返回 -1；成功返回 0
 */
static inline int pjson_set_string_value(json_object *json, json_string_t *jstr, json_mem_t *mem)
{
    if (json->jkey.type == JSON_STRING) {
        return pjson_string_strdup(jstr, &json->value.vstr, &mem->str_mgr);
    }
    return -1;
}

/*
 * pjson_replace_item_in_array - Replace the specified object in JSON_ARRAY object with new json object
 * @array: IN, the JSON_ARRAY object
 * @seq: IN, the sequence number
 * @new_item: IN, the new json object
 * @return: -1 on failure, 0 on success
 * @description: If seq is not satisfied, new_item will be added to the end.
 */
/*
 * pjson_replace_item_in_array - 替换JSON_ARRAY对象中指定序号的json对象
 * @array: IN,JSON_ARRAY对象
 * @seq: IN, 序号
 * @new_item: IN, 新的json对象
 * @return: 失败返回 -1；成功返回 0
 * @description: 如果序号不满足，new_item 将被添加到末尾
 */
int pjson_replace_item_in_array(json_object *array, int seq, json_object *new_item);

/*
 * pjson_replace_item_in_object - Replace the specified object in JSON_OBJECT object with new json object
 * @object: IN, the JSON_OBJECT object
 * @new_item: IN, the new json object, users should set the key of new_item first
 * @return: -1 on failure, 0 on success
 * @description: If key is not satisfied, new_item will be added to the end.
 */
/*
 * pjson_replace_item_in_object - 替换JSON_OBJECT对象中指定键的json对象
 * @object: IN,JSON_OBJECT对象
 * @new_item: IN, 新的json对象，用户应预先设置 new_item 的键
 * @return: 失败返回 -1；成功返回 0
 * @description: 如果键不满足，new_item 将被添加到末尾
 */
int pjson_replace_item_in_object(json_object *object, json_object *new_item);

/*
 * pjson_deepcopy - Deep copy the json object (recursive)
 * @json: IN, the source json object
 * @mem: IN, the block memory manager
 * @return: NULL on failure, a pointer on success
 */
/*
 * pjson_deepcopy - 递归深拷贝json对象
 * @json: IN, 源json对象
 * @mem: IN, 块内存管理器
 * @return: 失败返回 NULL；成功返回指针
 */
json_object *pjson_deepcopy(json_object *json, json_mem_t *mem);

/*
 * pjson_copy_item_to_array - Copy the specified object, the add it to the JSON_ARRAY object
 * @array: IN, the JSON_ARRAY object
 * @item: IN, the json object to copy and add
 * @mem: IN, the block memory manager
 * @return: -1 on failure, 0 on success
 * @description: The item will be added to the end
 */
/*
 * pjson_copy_item_to_array - 复制指定的json对象并将其添加到JSON_ARRAY对象中
 * @array: IN,JSON_ARRAY对象
 * @item: IN, 要复制并添加的json对象
 * @mem: IN, 块内存管理器
 * @return: 失败返回 -1；成功返回 0
 * @description: 对象将被添加到末尾
 */
int pjson_copy_item_to_array(json_object *array, json_object *item, json_mem_t *mem);

/*
 * pjson_copy_item_to_object - Copy the specified object, the add it to the JSON_OBJECT object
 * @object: IN, the JSON_OBJECT object
 * @item: IN, the json object to copy and add, users should set the key of item first
 * @mem: IN, the block memory manager
 * @return: -1 on failure, 0 on success
 * @description: The item will be added to the end
 */
/*
 * pjson_copy_item_to_object - 复制指定的json对象并将其添加到JSON_OBJECT对象中
 * @object: IN,JSON_OBJECT对象
 * @item: IN, 要复制并添加的json对象，用户应预先设置 item 的键
 * @mem: IN, 块内存管理器
 * @return: 失败返回 -1；成功返回 0
 * @description: 对象将被添加到末尾。
 */
int pjson_copy_item_to_object(json_object *object, json_object *item, json_mem_t *mem);

/*
 * pjson_add_new_item_to_array - Create a new object, the add it to the pool JSON_ARRAY object
 * @object: IN, the JSON_ARRAY object
 * @type: the json object type
 * @value: IN, the json object value
 * @mem: IN, the block memory manager
 * @return: NULL on failure, a pointer on success
 * @description: the pointer points to the created json object
 */
/*
 * pjson_add_new_item_to_array - 创建一个新的json对象并将其添加到池JSON_ARRAY对象中
 * @array: IN,JSON_ARRAY对象
 * @type: IN, json对象类型
 * @value: IN, json对象值
 * @mem: IN, 块内存管理器
 * @return: 失败返回 NULL；成功返回指针
 * @description: 指针指向创建的json对象
 */
json_object *pjson_add_new_item_to_array(json_object *array, json_type_t type, void *value, json_mem_t *mem);
static inline json_object *pjson_add_null_to_array(json_object *array, json_mem_t *mem) { return pjson_add_new_item_to_array(array, JSON_NULL, NULL, mem); }
static inline json_object *pjson_add_bool_to_array(json_object *array, bool value, json_mem_t *mem) { return pjson_add_new_item_to_array(array, JSON_BOOL, &value, mem); }
static inline json_object *pjson_add_int_to_array(json_object *array, int32_t value, json_mem_t *mem) { return pjson_add_new_item_to_array(array, JSON_INT, &value, mem); }
static inline json_object *pjson_add_hex_to_array(json_object *array, uint32_t value, json_mem_t *mem) { return pjson_add_new_item_to_array(array, JSON_HEX, &value, mem); }
static inline json_object *pjson_add_lint_to_array(json_object *array, int64_t value, json_mem_t *mem) { return pjson_add_new_item_to_array(array, JSON_LINT, &value, mem); }
static inline json_object *pjson_add_lhex_to_array(json_object *array, uint64_t value, json_mem_t *mem) { return pjson_add_new_item_to_array(array, JSON_LHEX, &value, mem); }
static inline json_object *pjson_add_double_to_array(json_object *array, double value, json_mem_t *mem) { return pjson_add_new_item_to_array(array, JSON_DOUBLE, &value, mem); }
static inline json_object *pjson_add_string_to_array(json_object *array, json_string_t *value, json_mem_t *mem) { return pjson_add_new_item_to_array(array, JSON_STRING, value, mem); }
static inline json_object *pjson_add_array_to_array(json_object *array, json_mem_t *mem) { return pjson_add_new_item_to_array(array, JSON_ARRAY, NULL, mem); }
static inline json_object *pjson_add_object_to_array(json_object *array, json_mem_t *mem) { return pjson_add_new_item_to_array(array, JSON_OBJECT, NULL, mem); }

#ifdef __cplusplus
}
static inline json_object *pjson_array_append(json_object *array, bool value, json_mem_t *mem) { return pjson_add_new_item_to_array(array, JSON_BOOL, &value, mem); }
static inline json_object *pjson_array_append(json_object *array, int32_t value, json_mem_t *mem) { return pjson_add_new_item_to_array(array, JSON_INT, &value, mem); }
static inline json_object *pjson_array_append(json_object *array, uint32_t value, json_mem_t *mem) { return pjson_add_new_item_to_array(array, JSON_HEX, &value, mem); }
static inline json_object *pjson_array_append(json_object *array, int64_t value, json_mem_t *mem) { return pjson_add_new_item_to_array(array, JSON_LINT, &value, mem); }
static inline json_object *pjson_array_append(json_object *array, uint64_t value, json_mem_t *mem) { return pjson_add_new_item_to_array(array, JSON_LHEX, &value, mem); }
static inline json_object *pjson_array_append(json_object *array, double value, json_mem_t *mem) { return pjson_add_new_item_to_array(array, JSON_DOUBLE, &value, mem); }
static inline json_object *pjson_array_append(json_object *array, json_string_t *value, json_mem_t *mem) { return pjson_add_new_item_to_array(array, JSON_STRING, value, mem); }
extern "C" {
#else
/* C11泛型选择(Generic selection) */
#define pjson_array_append(array, value, mem)  _Generic((value), \
bool            : pjson_add_bool_to_array                      , \
int32_t         : pjson_add_int_to_array                       , \
uint32_t        : pjson_add_hex_to_array                       , \
int64_t         : pjson_add_lint_to_array                      , \
uint64_t        : pjson_add_lhex_to_array                      , \
double          : pjson_add_double_to_array                    , \
json_string_t*  : pjson_add_string_to_array)(array, value, mem)
#endif

#define pjson_array_append_null(array, mem)   pjson_add_null_to_array  (array, mem)
#define pjson_array_append_array(array, mem)  pjson_add_array_to_array (array, mem)
#define pjson_array_append_object(array, mem) pjson_add_object_to_array(array, mem)

/*
 * pjson_add_new_item_to_object - Create a new object, the add it to the pool JSON_OBJECT object
 * @object: IN, the JSON_OBJECT object
 * @type: the json object type
 * @jkey: IN, the LJSON string key, allow length not to be set first by json_string_info_update
 * @value: IN, the json object value
 * @mem: IN, the block memory manager
 * @return: NULL on failure, a pointer on success
 * @description: the pointer points to the created json object
 */
/*
 * pjson_add_new_item_to_object - 创建一个新的json对象并将其添加到池JSON_OBJECT对象中
 * @object: IN,JSON_OBJECT对象
 * @type: IN, json对象类型
 * @jkey: IN, LJSON字符串键，允许长度未预先通过 json_string_info_update 设置
 * @value: IN, json对象值
 * @mem: IN, 块内存管理器
 * @return: 失败返回 NULL；成功返回指针
 * @description: 指针指向创建的json对象
 */
json_object *pjson_add_new_item_to_object(json_object *object, json_type_t type, json_string_t *jkey, void *value, json_mem_t *mem);
static inline json_object *pjson_add_null_to_object(json_object *object, json_string_t *jkey, json_mem_t *mem) { return pjson_add_new_item_to_object(object, JSON_NULL, jkey, NULL, mem); }
static inline json_object *pjson_add_bool_to_object(json_object *object, json_string_t *jkey, bool value, json_mem_t *mem) { return pjson_add_new_item_to_object(object, JSON_BOOL, jkey, &value, mem); }
static inline json_object *pjson_add_int_to_object(json_object *object, json_string_t *jkey, int32_t value, json_mem_t *mem) { return pjson_add_new_item_to_object(object, JSON_INT, jkey, &value, mem); }
static inline json_object *pjson_add_hex_to_object(json_object *object, json_string_t *jkey, uint32_t value, json_mem_t *mem) { return pjson_add_new_item_to_object(object, JSON_HEX, jkey, &value, mem); }
static inline json_object *pjson_add_lint_to_object(json_object *object, json_string_t *jkey, int64_t value, json_mem_t *mem) { return pjson_add_new_item_to_object(object, JSON_LINT, jkey, &value, mem); }
static inline json_object *pjson_add_lhex_to_object(json_object *object, json_string_t *jkey, uint64_t value, json_mem_t *mem) { return pjson_add_new_item_to_object(object, JSON_LHEX, jkey, &value, mem); }
static inline json_object *pjson_add_double_to_object(json_object *object, json_string_t *jkey, double value, json_mem_t *mem) { return pjson_add_new_item_to_object(object, JSON_DOUBLE, jkey, &value, mem); }
static inline json_object *pjson_add_string_to_object(json_object *object, json_string_t *jkey, json_string_t *value, json_mem_t *mem) { return pjson_add_new_item_to_object(object, JSON_STRING, jkey, value, mem); }
static inline json_object *pjson_add_array_to_object(json_object *object, json_string_t *jkey, json_mem_t *mem) { return pjson_add_new_item_to_object(object, JSON_ARRAY, jkey, NULL, mem); }
static inline json_object *pjson_add_object_to_object(json_object *object, json_string_t *jkey, json_mem_t *mem) { return pjson_add_new_item_to_object(object, JSON_OBJECT, jkey, NULL, mem); }

#ifdef __cplusplus
}
static inline json_object *pjson_object_append(json_object *object, json_string_t *jkey, bool value, json_mem_t *mem) { return pjson_add_new_item_to_object(object, JSON_BOOL, jkey, &value, mem); }
static inline json_object *pjson_object_append(json_object *object, json_string_t *jkey, int32_t value, json_mem_t *mem) { return pjson_add_new_item_to_object(object, JSON_INT, jkey, &value, mem); }
static inline json_object *pjson_object_append(json_object *object, json_string_t *jkey, uint32_t value, json_mem_t *mem) { return pjson_add_new_item_to_object(object, JSON_HEX, jkey, &value, mem); }
static inline json_object *pjson_object_append(json_object *object, json_string_t *jkey, int64_t value, json_mem_t *mem) { return pjson_add_new_item_to_object(object, JSON_LINT, jkey, &value, mem); }
static inline json_object *pjson_object_append(json_object *object, json_string_t *jkey, uint64_t value, json_mem_t *mem) { return pjson_add_new_item_to_object(object, JSON_LHEX, jkey, &value, mem); }
static inline json_object *pjson_object_append(json_object *object, json_string_t *jkey, double value, json_mem_t *mem) { return pjson_add_new_item_to_object(object, JSON_DOUBLE, jkey, &value, mem); }
static inline json_object *pjson_object_append(json_object *object, json_string_t *jkey, json_string_t *value, json_mem_t *mem) { return pjson_add_new_item_to_object(object, JSON_STRING, jkey, value, mem); }
extern "C" {
#else
/* C11泛型选择(Generic selection) */
#define pjson_object_append(object, jkey, value, mem)  _Generic((value), \
bool            : pjson_add_bool_to_object                             , \
int32_t         : pjson_add_int_to_object                              , \
uint32_t        : pjson_add_hex_to_object                              , \
int64_t         : pjson_add_lint_to_object                             , \
uint64_t        : pjson_add_lhex_to_object                             , \
double          : pjson_add_double_to_object                           , \
json_string_t*  : pjson_add_string_to_object)(object, jkey, value, mem)
#endif

#define pjson_object_append_null(object, jkey, mem)   pjson_add_null_to_object  (object, jkey, mem)
#define pjson_object_append_array(object, jkey, mem)  pjson_add_array_to_object (object, jkey, mem)
#define pjson_object_append_object(object, jkey, mem) pjson_add_object_to_object(object, jkey, mem)

/**************** json DOM printer / DOM打印器 ****************/

/*
 * json_print_choice_t - the choice to print
 * @size: INOUT, the length of data
 * @p: INOUT, the data, it can be NULL while size is not 0
 * @description: it is used for reusing cache to accelerate printing speed
 */
/*
 * json_print_ptr_t - 打印缓冲
 * @size: INOUT, 数据的长度
 * @p: INOUT, 数据指针，当size不为0时可以为 NULL
 * @description: 用于复用缓存以加速打印速度，此时LJSON内部可能都不会进行内存分配。
 *    打印到字符串时，开始时传入json_print_ptr_t，print结束时也传入json_print_ptr_t，此时不用释放
 *    打印返回的字符串，会一直复用json_print_ptr_t的p成员，最后不再使用时释放p成员一次即可。
 */
typedef struct {
    size_t size;
    char *p;
} json_print_ptr_t;

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
 * @ptr: INOUT, pre-allocated memory for printing
 * @path: IN, when the path is set, it prints to file while printing,
 *   otherwise it directly print to string
 */
/*
 * json_print_choice_t - 打印参数设置
 * @str_len: OUT, 打印到字符串时返回的字符串长度
 * @plus_size: IN, 打印到字符串时重新分配内存时增加的内存大小，或打印到文件时的写入缓冲区大小，默认值为 `JSON_PRINT_SIZE_PLUS_DEF`(1024)
 * @item_size: IN, 将 JSON 对象转换为字符串时的大小，默认值为 `JSON_FORMAT_ITEM_SIZE_DEF`(32)（当 format_flag 为 true 时）或 `JSON_UNFORMAT_ITEM_SIZE_DEF`(24)（当 format_flag 为 false 时）
 * @item_total: IN, JSON 对象的总数，DOM 打印时会自动计算，SAX 打印时最好由用户设置
 * @format_flag: IN, 设置格式化打印（true）或压缩打印（false）
 * @ptr: INOUT, 预分配的内存用于打印
 * @path: IN, 当 path 设置时，打印到文件，否则直接打印到字符串
 * @description: 如果设置了ptr，外部缓冲进入打印内部函数后，将缓冲交给内部接口，自身置空；当打印完成后：
 *   如果打印到字符串，ptr被置为了返回的字符串的值，最后使用json_memory_free释放ptr->p或返回的字符串之一；
 *   如果打印到文件，ptr被置为了内部读写缓冲的值，最后使用json_memory_free释放ptr->p
 */
typedef struct {
    size_t str_len; /* return string length if it is printed to string. */
    size_t plus_size;
    size_t item_size;
    int item_total;
    bool format_flag;
    json_print_ptr_t *ptr;
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
/*
 * json_print_common - 通用的DOM打印器
 * @json: IN, 要打印的json对象
 * @choice: INOUT, 打印选项
 * @return: 失败返回NULL；成功返回指针
 * @description: 当打印到文件时，成功返回的指针是 `"ok"`，不要释放它；
 *   当打印到字符串时，成功返回的指针是打印的字符串，使用 `json_memory_free` 释放它或ptr->p
 */
char *json_print_common(json_object *json, json_print_choice_t *choice);

/*
 * json_print_format - Formatted DOM printer to string
 * @json: IN, the json object to be printed
 * @item_total: IN, the total json objects, it is better to set the value by users
 * @length: OUT, the length of returned print string
 * @ptr: INOUT, pre-allocated memory for printing
 * @return: NULL on failure, a pointer on success
 */
/*
 * json_print_format - 格式化 DOM 打印器，输出到字符串
 * @json: IN, 要打印的json对象
 * @item_total: IN, json对象的总数，最好由用户设置
 * @length: OUT, 返回的打印字符串的长度
 * @ptr: INOUT, 预分配的内存用于打印
 * @return: 失败返回NULL；成功返回指针
 */
static inline char *json_print_format(json_object *json, int item_total, size_t *length, json_print_ptr_t *ptr)
{
    json_print_choice_t choice;
    char *str = NULL;

    memset(&choice, 0, sizeof(choice));
    choice.format_flag = true;
    choice.item_total = item_total;
    choice.ptr = ptr;
    str = json_print_common(json, &choice);
    if (length)
        *length = choice.str_len;
    return str;
}

/*
 * json_print_unformat - Compressed DOM printer to string
 * @json: IN, the json object to be printed
 * @item_total: IN, the total json objects, it is better to set the value by users
 * @length: OUT, the length of returned print string
 * @ptr: INOUT, pre-allocated memory for printing
 * @return: NULL on failure, a pointer on success
 */
/*
 * json_print_unformat - 压缩 DOM 打印器，输出到字符串
 * @json: IN, 要打印的json对象
 * @item_total: IN, json对象的总数，最好由用户设置
 * @length: OUT, 返回的打印字符串的长度
 * @ptr: INOUT, 预分配的内存用于打印
 * @return: 失败返回NULL；成功返回指针
 */
static inline char *json_print_unformat(json_object *json, int item_total, size_t *length, json_print_ptr_t *ptr)
{
    json_print_choice_t choice;
    char *str = NULL;

    memset(&choice, 0, sizeof(choice));
    choice.format_flag = false;
    choice.item_total = item_total;
    choice.ptr = ptr;
    str = json_print_common(json, &choice);
    if (length)
        *length = choice.str_len;
    return str;
}

/*
 * json_fprint_format - Formatted DOM printer to file
 * @json: IN, the json object to be printed
 * @item_total: IN, the total json objects, it is better to set the value by users
 * @path: IN, the file to store the printed string
 * @ptr: INOUT, pre-allocated memory for printing
 * @return: NULL on failure, a pointer on success
 */
/*
 * json_fprint_format - 格式化 DOM 打印器，输出到文件
 * @json: IN, 要打印的json对象
 * @item_total: IN, json对象的总数，最好由用户设置
 * @path: IN, 存储打印字符串的文件路径
 * @ptr: INOUT, 预分配的内存用于打印
 * @return: 失败返回NULL；成功返回指针
 */
static inline char *json_fprint_format(json_object *json, int item_total, const char *path, json_print_ptr_t *ptr)
{
    json_print_choice_t choice;

    memset(&choice, 0, sizeof(choice));
    choice.format_flag = true;
    choice.item_total = item_total;
    choice.path = path;
    choice.ptr = ptr;
    return json_print_common(json, &choice);
}

/*
 * json_fprint_unformat - Compressed DOM printer to file
 * @json: IN, the json object to be printed
 * @item_total: IN, the total json objects, it is better to set the value by users
 * @path: IN, the file to store the printed string
 * @ptr: INOUT, pre-allocated memory for printing
 * @return: NULL on failure, a pointer on success
 */
/*
 * json_fprint_unformat - 压缩 DOM 打印器，输出到文件
 * @json: IN, 要打印的json对象
 * @item_total: IN, json对象的总数，最好由用户设置
 * @path: IN, 存储打印字符串的文件路径
 * @ptr: INOUT, 预分配的内存用于打印
 * @return: 失败返回NULL；成功返回指针
 */
static inline char *json_fprint_unformat(json_object *json, int item_total, const char *path, json_print_ptr_t *ptr)
{
    json_print_choice_t choice;

    memset(&choice, 0, sizeof(choice));
    choice.format_flag = false;
    choice.item_total = item_total;
    choice.path = path;
    choice.ptr = ptr;
    return json_print_common(json, &choice);
}

/**************** json DOM parser / DOM解析器  ****************/

/*
 * json_parse_choice_t - the choice to parse json
 * @mem_size: the default block memory size to allocate, its smallest value is
 *   (str_len / `JSON_STR_MULTIPLE_NUM`(8))
 * @read_size: IN, the read buffer size when parsing from file,
 *   its default value is `JSON_PARSE_READ_SIZE_DEF`(8192)
 * @str_len: IN, the size of string to be parsed when parsing from string,
 *   it's better to set it when parsing from string
 * @reuse_flag: IN, whether to use the `str` directly as the value of JSON_STRING object and key
 * @mem: IN, the block memory manager, users needn't to Initializate it first
 * @path: IN, the file to be parsed, when the path is set, it parses the data while reading,
 *   otherwise it directly parses from the string
 * @str: IN, the string to be parsed, only one of `path` and `str` has value
 */
/*
 * json_parse_choice_t - 解析配置
 * @mem_size: 默认分配的块内存大小，最小值为 (str_len / `JSON_STR_MULTIPLE_NUM`(8))
 * @read_size: IN, 从文件解析时的读取缓冲区大小，默认值为 `JSON_PARSE_READ_SIZE_DEF`(8192)
 * @str_len: IN, 从字符串解析时字符串的大小，最好在从字符串解析时设置
 * @reuse_flag: IN, 是否直接使用 `str` 作为 `JSON_STRING` 对象和键的值
 * @mem: IN, 块内存管理器，用户无需预先初始化
 * @path: IN, 要解析的文件，当 path 设置时，边读取边解析数据，否则直接从字符串解析
 * @str: IN, 要解析的字符串，`path` 和 `str` 只能有一个有值
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
/*
 * json_parse_common - 通用的 DOM 解析器
 * @choice: IN, 解析选项
 * @return: 失败返回NULL；成功返回指针
 */
json_object *json_parse_common(json_parse_choice_t *choice);

/*
 * json_parse_str - The ordinary DOM parser from string
 * @str: IN, the string to be parsed
 * @str_len: IN, the length of str
 * @return: NULL on failure, a pointer on success
 * @description: Use `malloc` to allocate memory
 */
/*
 * json_parse_str - 普通的 DOM 解析器，从字符串解析
 * @str: IN, 要解析的字符串
 * @str_len: IN, 字符串的长度
 * @return: 失败返回NULL；成功返回指针
 * @description: 使用 `malloc` 分配内存
 */
static inline json_object *json_parse_str(char *str, size_t str_len)
{
    json_parse_choice_t choice;

    memset(&choice, 0, sizeof(choice));
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
/*
 * json_fast_parse_str - 快速的 DOM 解析器，从字符串解析
 * @str: IN, 要解析的字符串
 * @str_len: IN, 字符串的长度
 * @mem: IN, 块内存管理器
 * @return: 失败返回NULL；成功返回指针
 * @description: 使用 `pjson_memory_alloc` 分配内存，速度更快。
 */
static inline json_object *json_fast_parse_str(char *str, size_t str_len, json_mem_t *mem)
{
    json_parse_choice_t choice;

    memset(&choice, 0, sizeof(choice));
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
/*
 * json_reuse_parse_str - 可复用的 DOM 解析器，从字符串解析
 * @str: IN, 要解析的字符串
 * @str_len: IN, 字符串的长度
 * @mem: IN, 块内存管理器
 * @return: 失败返回NULL；成功返回指针
 * @description: 使用 `pjson_memory_alloc` 分配内存，速度更快，
 *   并且直接使用解析的 `str` 作为 `JSON_STRING` 对象和键的值，
 *   这意味着它会修改原始字符串并节省内存。
 */
static inline json_object *json_reuse_parse_str(char *str, size_t str_len, json_mem_t *mem)
{
    json_parse_choice_t choice;

    memset(&choice, 0, sizeof(choice));
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
/*
 * json_parse_file - 普通的 DOM 解析器，从文件解析
 * @path: IN, 要解析的文件路径
 * @return: 失败返回NULL；成功返回指针
 * @description: 边读取边解析数据，无需一次性读取所有数据。
 */
static inline json_object *json_parse_file(const char *path)
{
    json_parse_choice_t choice;

    memset(&choice, 0, sizeof(choice));
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
/*
 * json_fast_parse_file - 快速的 DOM 解析器，从文件解析
 * @path: IN, 要解析的文件路径
 * @mem: IN, 块内存管理器
 * @return: 失败返回NULL；成功返回指针
 * @description: 边读取边解析数据，无需一次性读取所有数据。
 */
static inline json_object *json_fast_parse_file(const char *path, json_mem_t *mem)
{
    json_parse_choice_t choice;

    memset(&choice, 0, sizeof(choice));
    choice.path = path;
    choice.mem = mem;
    return json_parse_common(&choice);
}

/**************** json SAX printer / SAX解析器 ****************/

#if JSON_SAX_APIS_SUPPORT

/*
 * json_sax_print_hd - the handle of SAX printer
 * @description: It is a pointer of `json_sax_print_t`
 */
/*
 * json_sax_parser_t - SAX打印句柄
 * @description: 实际就是 `json_sax_print_t` 的指针
 */
typedef void* json_sax_print_hd;

/*
 * json_sax_print_start - Start the SAX printer
 * @choice: INOUT, the print choice
 * @return: NULL on failure, a pointer (the handle of SAX print) on success
 */
/*
 * json_sax_print_start - 启动SAX打印器
 * @choice: INOUT, 打印选项
 * @return: 失败返回NULL；成功返回指针（SAX打印器的句柄）
 */
json_sax_print_hd json_sax_print_start(json_print_choice_t *choice);

/*
 * json_sax_print_format_start - Start the formatted SAX printer to string
 * @item_total: IN, the total json objects, it is better to set the value by users
 * @ptr: IN, pre-allocated memory for printing
 * @return: NULL on failure, a pointer (the handle of SAX print) on success
 */
/*
 * json_sax_print_format_start - 启动格式化的SAX打印器，输出到字符串
 * @item_total: IN, json对象的总数，最好由用户设置
 * @ptr: IN, 预分配的内存用于打印
 * @return: 失败返回NULL；成功返回指针（SAX打印器的句柄）
 */
static inline json_sax_print_hd json_sax_print_format_start(int item_total, json_print_ptr_t *ptr)
{
    json_print_choice_t choice;

    memset(&choice, 0, sizeof(choice));
    choice.format_flag = true;
    choice.item_total = item_total;
    choice.ptr = ptr;
    return json_sax_print_start(&choice);
}

/*
 * json_sax_print_unformat_start - Start the compressed SAX printer to string
 * @item_total: IN, the total json objects, it is better to set the value by users
 * @ptr: IN, pre-allocated memory for printing
 * @return: NULL on failure, a pointer (the handle of SAX print) on success
 */
/*
 * json_sax_print_unformat_start - 启动压缩的SAX打印器，输出到字符串
 * @item_total: IN, json对象的总数，最好由用户设置
 * @ptr: IN, 预分配的内存用于打印
 * @return: 失败返回NULL；成功返回指针（SAX打印器的句柄）
 */
static inline json_sax_print_hd json_sax_print_unformat_start(int item_total, json_print_ptr_t *ptr)
{
    json_print_choice_t choice;

    memset(&choice, 0, sizeof(choice));
    choice.format_flag = false;
    choice.item_total = item_total;
    choice.ptr = ptr;
    return json_sax_print_start(&choice);
}

/*
 * json_sax_fprint_format_start - Start the formatted SAX printer to file
 * @item_total: IN, the total json objects, it is better to set the value by users
 * @path: IN, the file to store the printed string
 * @ptr: IN, pre-allocated memory for printing
 * @return: NULL on failure, a pointer (the handle of SAX print) on success
 */
/*
 * json_sax_fprint_format_start - 启动格式化的SAX打印器，输出到文件
 * @item_total: IN, json对象的总数，最好由用户设置
 * @path: IN, 存储打印字符串的文件路径
 * @ptr: IN, 预分配的内存用于打印
 * @return: 失败返回NULL；成功返回指针（SAX打印器的句柄）
 */
static inline json_sax_print_hd json_sax_fprint_format_start(int item_total, const char *path, json_print_ptr_t *ptr)
{
    json_print_choice_t choice;

    memset(&choice, 0, sizeof(choice));
    choice.format_flag = true;
    choice.item_total = item_total;
    choice.path = path;
    choice.ptr = ptr;
    return json_sax_print_start(&choice);
}

/*
 * json_sax_fprint_unformat_start - Start the compressed SAX printer to file
 * @item_total: IN, the total json objects, it is better to set the value by users
 * @path: IN, the file to store the printed string
 * @ptr: IN, pre-allocated memory for printing
 * @return: NULL on failure, a pointer (the handle of SAX print) on success
 */
/*
 * json_sax_fprint_unformat_start - 启动压缩的SAX打印器，输出到文件
 * @item_total: IN, json对象的总数，最好由用户设置
 * @path: IN, 存储打印字符串的文件路径
 * @ptr: IN, 预分配的内存用于打印
 * @return: 失败返回NULL；成功返回指针（SAX打印器的句柄）
 */
static inline json_sax_print_hd json_sax_fprint_unformat_start(int item_total, const char *path, json_print_ptr_t *ptr)
{
    json_print_choice_t choice;

    memset(&choice, 0, sizeof(choice));
    choice.format_flag = false;
    choice.item_total = item_total;
    choice.path = path;
    choice.ptr = ptr;
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
/*
 * json_sax_print_value - SAX打印json对象
 * @handle: IN, SAX打印器的句柄
 * @type: IN, json对象的类型
 * @jkey: IN, json对象的键，允许长度未预先设置
 * @value: IN, json对象的值
 * @return: 失败返回-1；成功返回0
 * @description: 如果要打印的节点的父节点是对象，则键字符串必须有值；其他情况下，键字符串可以填写或不填写
 *  JSON_ARRAY和JSON_OBJECT需要打印两次，一次值为 `JSON_SAX_START` 表示开始，一次值为 `JSON_SAX_FINISH` 表示完成
 */
int json_sax_print_value(json_sax_print_hd handle, json_type_t type, json_string_t *jkey, const void *value);
static inline int json_sax_print_null(json_sax_print_hd handle, json_string_t *jkey) { return json_sax_print_value(handle, JSON_NULL, jkey, NULL); }
static inline int json_sax_print_bool(json_sax_print_hd handle, json_string_t *jkey, bool value) { return json_sax_print_value(handle, JSON_BOOL, jkey, &value); }
static inline int json_sax_print_int(json_sax_print_hd handle, json_string_t *jkey, int32_t value) { return json_sax_print_value(handle, JSON_INT, jkey, &value); }
static inline int json_sax_print_hex(json_sax_print_hd handle, json_string_t *jkey, uint32_t value) { return json_sax_print_value(handle, JSON_HEX, jkey, &value); }
static inline int json_sax_print_lint(json_sax_print_hd handle, json_string_t *jkey, int64_t value) { return json_sax_print_value(handle, JSON_LINT, jkey, &value); }
static inline int json_sax_print_lhex(json_sax_print_hd handle, json_string_t *jkey, uint64_t value) { return json_sax_print_value(handle, JSON_LHEX, jkey, &value); }
static inline int json_sax_print_double(json_sax_print_hd handle, json_string_t *jkey, double value) { return json_sax_print_value(handle, JSON_DOUBLE, jkey, &value); }
static inline int json_sax_print_string(json_sax_print_hd handle, json_string_t *jkey, json_string_t *value) { return json_sax_print_value(handle, JSON_STRING, jkey, value); }
static inline int json_sax_print_array(json_sax_print_hd handle, json_string_t *jkey, json_sax_cmd_t value) { return json_sax_print_value(handle, JSON_ARRAY, jkey, &value); }
static inline int json_sax_print_object(json_sax_print_hd handle, json_string_t *jkey, json_sax_cmd_t value) { return json_sax_print_value(handle, JSON_OBJECT, jkey, &value); }

#ifdef __cplusplus
}
static inline int json_sax_print_array_item(json_sax_print_hd handle, bool value) { return json_sax_print_value(handle, JSON_BOOL, NULL, &value); }
static inline int json_sax_print_array_item(json_sax_print_hd handle, int32_t value) { return json_sax_print_value(handle, JSON_INT, NULL, &value); }
static inline int json_sax_print_array_item(json_sax_print_hd handle, uint32_t value) { return json_sax_print_value(handle, JSON_HEX, NULL, &value); }
static inline int json_sax_print_array_item(json_sax_print_hd handle, int64_t value) { return json_sax_print_value(handle, JSON_LINT, NULL, &value); }
static inline int json_sax_print_array_item(json_sax_print_hd handle, uint64_t value) { return json_sax_print_value(handle, JSON_LHEX, NULL, &value); }
static inline int json_sax_print_array_item(json_sax_print_hd handle, double value) { return json_sax_print_value(handle, JSON_DOUBLE, NULL, &value); }
static inline int json_sax_print_array_item(json_sax_print_hd handle, json_string_t *value) { return json_sax_print_value(handle, JSON_STRING, NULL, value); }
extern "C" {
#else
/* C11泛型选择(Generic selection) */
#define json_sax_print_array_item(handle, value)  _Generic((value), \
bool            : json_sax_print_bool                             , \
int32_t         : json_sax_print_int                              , \
uint32_t        : json_sax_print_hex                              , \
int64_t         : json_sax_print_lint                             , \
uint64_t        : json_sax_print_lhex                             , \
double          : json_sax_print_double                           , \
json_string_t*  : json_sax_print_string)(handle, NULL, value)
#endif

#define json_sax_print_array_null(handle)         json_sax_print_null  (handle, NULL, NULL)
#define json_sax_print_array_start(handle, jkey)  json_sax_print_array (handle, jkey, JSON_SAX_START)
#define json_sax_print_array_finish(handle)       json_sax_print_array (handle, NULL, JSON_SAX_FINISH)

#ifdef __cplusplus
}
static inline int json_sax_print_object_item(json_sax_print_hd handle, json_string_t *jkey, bool value) { return json_sax_print_value(handle, JSON_BOOL, jkey, &value); }
static inline int json_sax_print_object_item(json_sax_print_hd handle, json_string_t *jkey, int32_t value) { return json_sax_print_value(handle, JSON_INT, jkey, &value); }
static inline int json_sax_print_object_item(json_sax_print_hd handle, json_string_t *jkey, uint32_t value) { return json_sax_print_value(handle, JSON_HEX, jkey, &value); }
static inline int json_sax_print_object_item(json_sax_print_hd handle, json_string_t *jkey, int64_t value) { return json_sax_print_value(handle, JSON_LINT, jkey, &value); }
static inline int json_sax_print_object_item(json_sax_print_hd handle, json_string_t *jkey, uint64_t value) { return json_sax_print_value(handle, JSON_LHEX, jkey, &value); }
static inline int json_sax_print_object_item(json_sax_print_hd handle, json_string_t *jkey, double value) { return json_sax_print_value(handle, JSON_DOUBLE, jkey, &value); }
static inline int json_sax_print_object_item(json_sax_print_hd handle, json_string_t *jkey, json_string_t *value) { return json_sax_print_value(handle, JSON_STRING, jkey, value); }
extern "C" {
#else
/* C11泛型选择(Generic selection) */
#define json_sax_print_object_item(handle, jkey, value)  _Generic((value), \
bool            : json_sax_print_bool                             , \
int32_t         : json_sax_print_int                              , \
uint32_t        : json_sax_print_hex                              , \
int64_t         : json_sax_print_lint                             , \
uint64_t        : json_sax_print_lhex                             , \
double          : json_sax_print_double                           , \
json_string_t*  : json_sax_print_string)(handle, jkey, value)
#endif

#define json_sax_print_object_null(handle, jkey)  json_sax_print_null  (handle, jkey, NULL)
#define json_sax_print_object_start(handle, jkey) json_sax_print_object(handle, jkey, JSON_SAX_START)
#define json_sax_print_object_finish(handle)      json_sax_print_object(handle, NULL, JSON_SAX_FINISH)

/*
 * json_sax_print_finish - Finish the SAX printer
 * @handle: IN, the handle of SAX printer
 * @length: OUT, the length of returned printed string
 * @ptr: OUT, export internal allocated memory for printing
 * @return: NULL on failure, a pointer on success
 * @description: When printing to file, the pointer is `"ok"` on success, don't free it,
 *   when printing to string, the pointer is the printed string, use `json_memory_free` to free it or ptr->p.
 */
/*
 * json_sax_print_finish - 结束SAX打印器
 * @handle: IN, SAX 打印器的句柄
 * @length: OUT, 返回的打印字符串的长度
 * @ptr: OUT, 导出内部分配的内存用于打印
 * @return: 失败返回NULL；成功时返回指针
 * @description: 当打印到文件时，成功时返回 "ok"，不要释放它；当打印到字符串时，返回打印的字符串，使用 `json_memory_free` 释放它或ptr->p
 */
char *json_sax_print_finish(json_sax_print_hd handle, size_t *length, json_print_ptr_t *ptr);

/**************** json SAX parser / SAX解析器 ****************/

/*
 * json_sax_parser_t - the description passed by SAX parser to the callback function
 * @total: the size of depth array
 * @index: the current index of JSON type and key
 * @array: the json depth information, which stores json object type and key
 * @value: the json object value
 * @description: LJSON SAX parsing will maintain a depth information used for state machine.
 */
/*
 * json_sax_parse_choice_t - SAX解析选项
 * @read_size: IN, 从文件解析时的读取缓冲区大小，默认值为 `JSON_PARSE_READ_SIZE_DEF`(8192)
 * @str_len: IN, 从字符串解析时字符串的大小，最好在从字符串解析时设置
 * @str: IN, 要解析的字符串，`path` 和 `str` 只能有一个有值
 * @path: IN, 要解析的文件，当 path 设置时，边读取边解析数据，否则直接从字符串解析
 * @cb: IN, 处理 SAX 解析器传递结果的回调函数
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
/*
 * json_sax_parse_choice_t - SAX的回调函数
 * @description: 用户使用SAX接口时需要设置回调函数, 返回 `JSON_SAX_PARSE_CONTINUE` 表示继续解析；
 *   返回 `JSON_SAX_PARSE_STOP` 表示停止解析
 */
typedef enum {
    JSON_SAX_PARSE_CONTINUE = 0,
    JSON_SAX_PARSE_STOP
} json_sax_ret_t;
typedef json_sax_ret_t (*json_sax_cb_t)(json_sax_parser_t *parser);

/*
 * json_sax_parse_choice_t - the choice to parse
 * @read_size: IN, the read buffer size when parsing from file,
 *   its default value is `JSON_PARSE_READ_SIZE_DEF`(8192)
 * @str_len: IN, the size of string to be parsed when parsing from string,
 *   it's better to set it when parsing from string
 * @str: IN, the string to be parsed, only one of `path` and `str` has value
 * @path: IN, the file to be parsed, when the path is set, it parses the data while reading,
 *   otherwise it directly parses from the string
 * @cb: IN, the callback to process result passed by the SAX parser
 */
/*
 * json_sax_parse_choice_t - 解析选项
 * @read_size: IN, 从文件解析时的读取缓冲区大小，默认值为 `JSON_PARSE_READ_SIZE_DEF`(8192)
 * @str_len: IN, 从字符串解析时的字符串大小，建议在从字符串解析时设置
 * @str: IN, 要解析的字符串，`path` 和 `str` 只能有一个有值
 * @path: IN, 要解析的文件，当设置了路径时，解析器会在读取时解析数据，否则直接从字符串解析
 * @cb: IN, 处理 SAX 解析器传递结果的回调函数
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
/*
 * json_sax_parse_common - 通用的SAX解析器
 * @choice: IN, 解析选项
 * @return: 失败返回-1；成功返回0
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
/*
 * json_sax_parse_str - 从字符串进行SAX解析
 * @str: IN, 要解析的字符串
 * @str_len: IN, 字符串的长度
 * @cb: IN, 处理SAX解析器传递结果的回调函数
 * @return: 失败返回-1；成功返回0
 * @description: LJSON直接从字符串解析数据
 */
static inline int json_sax_parse_str(char *str, size_t str_len, json_sax_cb_t cb)
{
    json_sax_parse_choice_t choice;

    memset(&choice, 0, sizeof(choice));
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
/*
 * json_sax_parse_file - 从文件进行SAX解析
 * @path: IN, 要解析的文件
 * @cb: IN, 处理SAX解析器传递结果的回调函数
 * @return: 失败返回-1；成功返回0
 * @description: LJSON边读边解析数据，不会读完文件到内存再解析，降低内存峰值使用
 */
static inline int json_sax_parse_file(const char *path, json_sax_cb_t cb)
{
    json_sax_parse_choice_t choice;

    memset(&choice, 0, sizeof(choice));
    choice.path = path;
    choice.cb = cb;
    return json_sax_parse_common(&choice);
}

#endif
#ifdef __cplusplus
}
#endif
