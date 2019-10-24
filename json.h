#ifndef __JSON_H__
#define __JSON_H__

#define json_malloc         malloc
#define json_calloc         calloc
#define json_realloc        realloc
#define json_strdup         strdup
#define json_free           free

#define JSON_VERSION                        "v0.1.0"
#define JSON_DIRECT_FILE_SUPPORT            1
typedef int json_bool_t;
#define JSON_FALSE   0
#define JSON_TRUE    1

// print choice size
#define JSON_FULL_ADD_SIZE_DEF              8096
#define JSON_TEMP_ADD_SIZE_DEF              1024
#define JSON_ITEM_UNFORMAT_CELL_SIZE_DEF    24
#define JSON_ITEM_FORMAT_CELL_SIZE_DEF      32
// file parse choice size
#define JSON_PARSE_READ_SIZE_DEF            128
#define JSON_PARSE_USE_SIZE_DEF             8096
// block memmory alloc to parse
#define JSON_PAGE_ALIGEN_BYTES              8096
#define JSON_DATA_ALIGEN_BYTES              4
#define JSON_STR_MULTIPLE_NUM               8
#define JSON_KEY_TOTAL_NUM_DEF              100

struct json_list_head {
    struct json_list_head *next, *prev;
};

typedef enum {
    JSON_PRINT_REALLOC_ORIG = 0,
    JSON_PRINT_REALLOC_CAL,
    JSON_PRINT_DIRECT_FILE
} json_print_method_t;

typedef struct {
    size_t full_add_size;
    size_t temp_add_size;
    size_t item_cell_size;
#if JSON_DIRECT_FILE_SUPPORT
    const char *path;
#endif
    json_bool_t format_flag;
    json_print_method_t method_flag;
} json_print_choice_t;

typedef struct {
    const char *str;
#if JSON_DIRECT_FILE_SUPPORT
    const char *path;
    size_t read_size;
#endif
    size_t str_len;
    size_t mem_size;
    json_bool_t repeat_key_check;
    struct json_list_head *head;
} json_parse_choice_t;

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

typedef struct {
    struct json_list_head list;
    char *key;
    json_type_t type;
    union {
        int vint;
        unsigned int vhex;
        double vdbl;
        char *vstr;
        struct json_list_head head;
    };
} json_object;

/**** json with using cache apis ****/
// call json_cache_memory_init before use, call json_cache_memory_free after use.
void json_cache_memory_free(struct json_list_head *head);
void json_cache_memory_init(struct json_list_head *head);

json_object *json_cache_new_object(json_type_t type, struct json_list_head *head, size_t cache_size);
char *json_cache_new_string(size_t len, struct json_list_head *head, size_t cache_size);
int json_cache_change_key(json_object *json, const char *key, struct json_list_head *head, size_t cache_size);
int json_cache_change_string(json_object *json, const char *str, struct json_list_head *head, size_t cache_size);
json_object *json_cache_create_int(int value, struct json_list_head *head, size_t cache_size);
json_object *json_cache_create_string(const char *value, struct json_list_head *head, size_t cache_size);

// add_item_to_array can be used directly.
json_object *json_cache_create_bool_array(json_bool_t *values, int count,
    struct json_list_head *head, size_t cache_size);
json_object *json_cache_create_int_array(int *values, int count,
    struct json_list_head *head, size_t cache_size);
json_object *json_cache_create_hex_array(unsigned int *values, int count,
    struct json_list_head *head, size_t cache_size);
json_object *json_cache_create_double_array(double *values, int count,
    struct json_list_head *head, size_t cache_size);
json_object *json_cache_create_string_array(char **values, int count,
    struct json_list_head *head, size_t cache_size);

int json_cache_add_item_to_object(json_object *object, const char *key, json_object *item,
    struct json_list_head *head, size_t cache_size);
int json_cache_add_null_to_object(json_object *object, const char *key,
    struct json_list_head *head, size_t cache_size);
int json_cache_add_bool_to_object(json_object *object, const char *key, json_bool_t value,
    struct json_list_head *head, size_t cache_size);
int json_cache_add_int_to_object(json_object *object, const char *key, int value,
    struct json_list_head *head, size_t cache_size);
int json_cache_add_hex_to_object(json_object *object, const char *key, unsigned int value,
    struct json_list_head *head, size_t cache_size);
int json_cache_add_double_to_object(json_object *object, const char *key, double value,
    struct json_list_head *head, size_t cache_size);
int json_cache_add_string_to_object(json_object *object, const char *key, const char *value,
    struct json_list_head *head, size_t cache_size);

/**
 * The following APIs are only available to json with using cache.
 * json_fast_parse_str
 * json_fast_parse_file
 * json_rapid_parse_str
**/

/**
 * The following APIs are also available to json with using cache.
 * json_item_total_get
 * json_get_number_value
 * json_change_number_value
 * json_strict_change_number_value
 * json_get_array_size
 * json_get_array_item
 * json_get_object_item
 * json_detach_item_from_array
 * json_detach_item_from_object
 * json_add_item_to_array   // important
 * all "json print apis"
 * json_parse_common
 * all "inline number apis"
**/

/**** json common apis ****/
int json_item_total_get(json_object *json);
void json_delete_value(json_object *json);
void json_delete_json(json_object *json);
json_object *json_new_object(json_type_t type);
json_object *json_deepcopy(json_object *json);
json_object *json_create_bool_array(json_bool_t *values, int count);
json_object *json_create_int_array(int *values, int count);
json_object *json_create_hex_array(unsigned int *values, int count);
json_object *json_create_double_array(double *values, int count);
json_object *json_create_string_array(char **values, int count);

int json_change_key(json_object *json, const char *key);
int json_get_number_value(json_object *json, json_bool_t *vbool, int *vint, unsigned int *vhex, double *vdbl);
int json_change_number_value(json_object *json, json_bool_t *vbool, int *vint, unsigned int *vhex, double *vdbl);
int json_strict_change_number_value(json_object *json, json_bool_t *vbool, int *vint, unsigned int *vhex, double *vdbl);
int json_change_string_value(json_object *json, const char *str);

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
int json_add_null_to_object(json_object *object, const char *key);
int json_add_bool_to_object(json_object *object, const char *key, json_bool_t value);
int json_add_int_to_object(json_object *object, const char *key, int value);
int json_add_hex_to_object(json_object *object, const char *key, unsigned int value);
int json_add_double_to_object(json_object *object, const char *key, double value);
int json_add_string_to_object(json_object *object, const char *key, const char *value);
int json_copy_item_to_array(json_object *array, json_object *item);
int json_copy_item_to_object(json_object *object, const char *key, json_object *item);

/* json print apis */
char *json_print_common(json_object *json, json_print_choice_t *choice);
char *json_print_format(json_object *json);
char *json_print_unformat(json_object *json);
void json_free_print_ptr(void *ptr);
#if JSON_DIRECT_FILE_SUPPORT
char *json_fprint_format(json_object *json, const char *path);
char *json_fprint_unformat(json_object *json, const char *path);
#endif

/* json parse apis */
json_object *json_parse_common(json_parse_choice_t *choice);
json_object *json_parse_str(const char *str);
json_object *json_fast_parse_str(const char *str, struct json_list_head *head, size_t str_len);
#if JSON_DIRECT_FILE_SUPPORT
json_object *json_parse_file(const char *path);
json_object *json_fast_parse_file(const char *path, struct json_list_head *head);
#endif
json_object *json_rapid_parse_str(char *str, struct json_list_head *head, size_t str_len);

/* inline normal create apis */
static inline json_object *json_create_null(void)
{
    return json_new_object(JSON_NULL);
}

static inline json_object *json_create_bool(json_bool_t value)
{
    json_object *json = NULL;
    if ((json = json_new_object(JSON_BOOL)) == NULL) {
        return NULL;
    }
    json->vint = (value == JSON_FALSE) ? 0 : 1;
    return json;
}

static inline json_object *json_create_int(int value)
{
    json_object *json = NULL;
    if ((json = json_new_object(JSON_INT)) == NULL) {
        return NULL;
    }
    json->vint = value;
    return json;
}

static inline json_object *json_create_hex(unsigned int value)
{
    json_object *json = NULL;
    if ((json = json_new_object(JSON_HEX)) == NULL) {
        return NULL;
    }
    json->vhex = value;
    return json;
}

static inline json_object *json_create_double(double value)
{
    json_object *json = NULL;
    if ((json = json_new_object(JSON_DOUBLE)) == NULL) {
        return NULL;
    }
    json->vdbl = value;
    return json;
}

static inline json_object *json_create_string(const char *value)
{
    json_object *json = NULL;
    if ((json = json_new_object(JSON_STRING)) == NULL) {
        return NULL;
    }
    if (value) {
        if ((json->vstr = json_strdup(value)) == NULL) {
            json_free(json);
            return NULL;
        }
    } else {
        json->vstr = NULL;
    }
    return json;
}

static inline json_object *json_create_array(void)
{
    return json_new_object(JSON_ARRAY);
}

static inline json_object *json_create_object(void)
{
    return json_new_object(JSON_OBJECT);
}

/* inline number apis */
static inline json_bool_t json_get_bool_value(json_object *json)
{
    json_bool_t value = 0;
    json_get_number_value(json, &value, NULL, NULL, NULL);
    return value;
}
static inline int json_get_int_value(json_object *json)
{
    int value = 0;
    json_get_number_value(json, NULL, &value, NULL, NULL);
    return value;
}
static inline unsigned int json_get_hex_value(json_object *json)
{
    unsigned int value = 0;
    json_get_number_value(json, NULL, NULL, &value, NULL);
    return value;
}
static inline double json_get_double_value(json_object *json)
{
    double value = 0;
    json_get_number_value(json, NULL, NULL, NULL, &value);
    return value;
}

static inline int json_change_bool_value(json_object *json, json_bool_t value)
{
    return json_change_number_value(json, &value, NULL, NULL, NULL);
}
static inline int json_change_int_value(json_object *json, int value)
{
    return json_change_number_value(json, NULL, &value, NULL, NULL);
}
static inline int json_change_hex_value(json_object *json, unsigned int value)
{
    return json_change_number_value(json, NULL, NULL, &value, NULL);
}
static inline int json_change_double_value(json_object *json, double value)
{
    return json_change_number_value(json, NULL, NULL, NULL, &value);
}

#endif

