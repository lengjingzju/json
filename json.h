#ifndef __JSON_H__
#define __JSON_H__

#define JSON_VERSION                        "v0.2.0"
#define JSON_DIRECT_FILE_SUPPORT            1
#define JSON_SAX_APIS_SUPPORT               1

/* head apis */
#define json_malloc                         malloc
#define json_calloc                         calloc
#define json_realloc                        realloc
#define json_strdup                         strdup
#define json_free                           free

/* print choice size */
#define JSON_FULL_ADD_SIZE_DEF              8096
#define JSON_TEMP_ADD_SIZE_DEF              1024
#define JSON_ITEM_UNFORMAT_CELL_SIZE_DEF    24
#define JSON_ITEM_FORMAT_CELL_SIZE_DEF      32
/* file parse choice size */
#define JSON_PARSE_READ_SIZE_DEF            128
#define JSON_PARSE_USE_SIZE_DEF             8096
/* block memmory alloc to parse */
#define JSON_PAGE_ALIGEN_BYTES              8096
#define JSON_DATA_ALIGEN_BYTES              4
#define JSON_STR_MULTIPLE_NUM               8
#define JSON_KEY_TOTAL_NUM_DEF              100

/* json node */
typedef int json_bool_t;
#define JSON_FALSE   0
#define JSON_TRUE    1

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

/* DOM parse/print */
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

typedef struct {
    int item_total;
    size_t full_add_size;
    size_t temp_add_size;
    size_t item_cell_size;
#if JSON_DIRECT_FILE_SUPPORT
    const char *path;
#endif
    json_bool_t format_flag;
    json_bool_t calculate_flag;
} json_print_choice_t;

#if JSON_SAX_APIS_SUPPORT
/* SAX parse/print */
typedef enum {
    JSON_SAX_START = 0,
    JSON_SAX_FINISH
} json_sax_cmd_t;

typedef struct {
    unsigned int alloc:1;
    unsigned int len:31;
    char *str;
} json_detail_str_t;

typedef struct {
    json_type_t type;
    json_detail_str_t key;
} json_sax_parse_depth_t;

typedef struct {
    int total;
    int count;
    json_sax_parse_depth_t *array;
    union {
        int vint;                   // bool, int
        unsigned int vhex;          // hex
        double vdbl;                // double
        json_detail_str_t vstr;     // string
        json_sax_cmd_t vcmd;        // array, object
    } value;
} json_sax_parser;

typedef enum {
    JSON_SAX_PARSE_CONTINUE = 0,
    JSON_SAX_PARSE_STOP
} json_sax_parse_ret;

typedef json_sax_parse_ret (*json_sax_parser_callback)(json_sax_parser *parser);

typedef struct {
    const char *str;
#if JSON_DIRECT_FILE_SUPPORT
    const char *path;
    size_t read_size;
#endif
    json_sax_parser_callback callback;
} json_sax_parse_choice_t;

typedef void* json_sax_print_handle;
#endif

/**** json with using cache apis ****/

/* some json apis */
// call json_cache_memory_init before use, call json_cache_memory_free after use.
void json_cache_memory_free(struct json_list_head *head);
void json_cache_memory_init(struct json_list_head *head);
char *json_cache_new_string(size_t len, struct json_list_head *head, size_t cache_size);
int json_cache_change_key(json_object *json, const char *key, struct json_list_head *head, size_t cache_size);
int json_cache_change_string(json_object *json, const char *str, struct json_list_head *head, size_t cache_size);

/* create json apis */
json_object *json_cache_new_object(json_type_t type, struct json_list_head *head, size_t cache_size);
json_object *json_cache_create_item(json_type_t type, void *value, struct json_list_head *head, size_t cache_size);

static inline json_object *json_cache_create_null(struct json_list_head *head, size_t cache_size)
{
    return json_cache_new_object(JSON_NULL, head, cache_size);
}

static inline json_object *json_cache_create_bool(json_bool_t value, struct json_list_head *head, size_t cache_size)
{
    return json_cache_create_item(JSON_BOOL, &value, head, cache_size);
}

static inline json_object *json_cache_create_int(int value, struct json_list_head *head, size_t cache_size)
{
    return json_cache_create_item(JSON_INT, &value, head, cache_size);
}

static inline json_object *json_cache_create_hex(unsigned int value, struct json_list_head *head, size_t cache_size)
{
    return json_cache_create_item(JSON_HEX, &value, head, cache_size);
}

static inline json_object *json_cache_create_double(double value, struct json_list_head *head, size_t cache_size)
{
    return json_cache_create_item(JSON_DOUBLE, &value, head, cache_size);
}

static inline json_object *json_cache_create_string(const char *value, struct json_list_head *head, size_t cache_size)
{
    return json_cache_create_item(JSON_STRING, &value, head, cache_size);
}

static inline json_object *json_cache_create_array(struct json_list_head *head, size_t cache_size)
{
    return json_cache_new_object(JSON_ARRAY, head, cache_size);
}

static inline json_object *json_cache_create_object(struct json_list_head *head, size_t cache_size)
{
    return json_cache_new_object(JSON_OBJECT, head, cache_size);
}

/* create json array apis */
json_object *json_cache_create_item_array(json_type_t item_type, void *values, int count,
    struct json_list_head *head, size_t cache_size);

static inline json_object *json_cache_create_bool_array(json_bool_t *values, int count,
    struct json_list_head *head, size_t cache_size)
{
    return json_cache_create_item_array(JSON_BOOL, values, count, head, cache_size);
}

static inline json_object *json_cache_create_int_array(int *values, int count,
    struct json_list_head *head, size_t cache_size)
{
    return json_cache_create_item_array(JSON_INT, values, count, head, cache_size);
}

static inline json_object *json_cache_create_hex_array(unsigned int *values, int count,
    struct json_list_head *head, size_t cache_size)
{
    return json_cache_create_item_array(JSON_HEX, values, count, head, cache_size);
}

static inline json_object *json_cache_create_double_array(double *values, int count,
    struct json_list_head *head, size_t cache_size)
{
    return json_cache_create_item_array(JSON_DOUBLE, values, count, head, cache_size);
}

static inline json_object *json_cache_create_string_array(char **values, int count,
    struct json_list_head *head, size_t cache_size)
{
    return json_cache_create_item_array(JSON_STRING, values, count, head, cache_size);
}

/* some array/object apis */
// add_item_to_array() is also available to json with using cache..
int json_cache_add_item_to_object(json_object *object, const char *key, json_object *item,
    struct json_list_head *head, size_t cache_size);

/* create new item and add it to object apis */
int json_cache_add_new_item_to_object(json_object *object, json_type_t type, const char *key, void *value,
    struct json_list_head *head, size_t cache_size);

static inline int json_cache_add_null_to_object(json_object *object, const char *key,
    struct json_list_head *head, size_t cache_size)
{
    return json_cache_add_new_item_to_object(object, JSON_NULL, key, NULL, head, cache_size);
}

static inline int json_cache_add_bool_to_object(json_object *object, const char *key, json_bool_t value,
    struct json_list_head *head, size_t cache_size)
{
    return json_cache_add_new_item_to_object(object, JSON_BOOL, key, &value, head, cache_size);
}

static inline int json_cache_add_int_to_object(json_object *object, const char *key, int value,
    struct json_list_head *head, size_t cache_size)
{
    return json_cache_add_new_item_to_object(object, JSON_INT, key, &value, head, cache_size);
}

static inline int json_cache_add_hex_to_object(json_object *object, const char *key, unsigned int value,
    struct json_list_head *head, size_t cache_size)
{
    return json_cache_add_new_item_to_object(object, JSON_HEX, key, &value, head, cache_size);
}

static inline int json_cache_add_double_to_object(json_object *object, const char *key, double value,
    struct json_list_head *head, size_t cache_size)
{
    return json_cache_add_new_item_to_object(object, JSON_DOUBLE, key, &value, head, cache_size);
}

static inline int json_cache_add_string_to_object(json_object *object, const char *key, const char *value,
    struct json_list_head *head, size_t cache_size)
{
    return json_cache_add_new_item_to_object(object, JSON_STRING, key, &value, head, cache_size);
}

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
 * json_add_item_to_array
 * all "json dom print apis"
 * json_parse_common
 * all "get number value apis"
 * all "change number value apis"
**/

/**** json common apis ****/

/* some json apis */
int json_item_total_get(json_object *json);
int json_change_key(json_object *json, const char *key);
int json_change_string_value(json_object *json, const char *str);

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

/* get number value apis */
int json_get_number_value(json_object *json, json_bool_t *vbool, int *vint, unsigned int *vhex, double *vdbl);
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

/* change number value apis */
int json_change_number_value(json_object *json, json_bool_t *vbool, int *vint, unsigned int *vhex, double *vdbl);
int json_strict_change_number_value(json_object *json, json_bool_t *vbool, int *vint, unsigned int *vhex, double *vdbl);

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

/* some array/object apis */
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

/**** json dom print apis ****/
char *json_print_common(json_object *json, json_print_choice_t *choice);
void json_free_print_ptr(void *ptr);

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

#if JSON_DIRECT_FILE_SUPPORT
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
#endif

/**** json dom parse apis ****/
json_object *json_rapid_parse_str(char *str, struct json_list_head *head, size_t str_len); // will change incoming str
json_object *json_parse_common(json_parse_choice_t *choice);

static inline json_object *json_parse_str(const char *str)
{
    json_parse_choice_t choice = {0};
    choice.str = str;
    return json_parse_common(&choice);
}

static inline json_object *json_fast_parse_str(const char *str, struct json_list_head *head, size_t str_len)
{
    json_parse_choice_t choice = {0};
    choice.str = str;
    choice.head = head;
    choice.str_len = str_len;
    //choice.repeat_key_check = JSON_TRUE;
    return json_parse_common(&choice);
}

#if JSON_DIRECT_FILE_SUPPORT
static inline json_object *json_parse_file(const char *path)
{
    json_parse_choice_t choice = {0};
    choice.path = path;
    return json_parse_common(&choice);
}

static inline json_object *json_fast_parse_file(const char *path, struct json_list_head *head)
{
    json_parse_choice_t choice = {0};
    choice.path = path;
    choice.head = head;
    //choice.repeat_key_check = JSON_TRUE;
    return json_parse_common(&choice);
}
#endif

#if JSON_SAX_APIS_SUPPORT
/**** json sax print apis ****/
int json_sax_print_value(json_sax_print_handle handle, json_type_t type, const char *key, const void *value);
json_sax_print_handle json_sax_print_start(json_print_choice_t *choice);
char *json_sax_print_finish(json_sax_print_handle handle);

static inline json_sax_print_handle json_sax_print_format_start(int item_total)
{
    json_print_choice_t choice = {0};

    choice.format_flag = JSON_TRUE;
    choice.calculate_flag = JSON_TRUE;
    choice.item_total = item_total;
    choice.calculate_flag = JSON_TRUE;
    return json_sax_print_start(&choice);
}

static inline json_sax_print_handle json_sax_print_unformat_start(int item_total)
{
    json_print_choice_t choice = {0};

    choice.format_flag = JSON_FALSE;
    choice.calculate_flag = JSON_TRUE;
    choice.item_total = item_total;
    choice.calculate_flag = JSON_TRUE;
    return json_sax_print_start(&choice);
}

#if JSON_DIRECT_FILE_SUPPORT
static inline json_sax_print_handle json_sax_fprint_format_start(const char *path)
{
    json_print_choice_t choice = {0};

    choice.format_flag = JSON_TRUE;
    choice.path = path;
    return json_sax_print_start(&choice);
}

static inline json_sax_print_handle json_sax_fprint_unformat_start(const char *path)
{
    json_print_choice_t choice = {0};

    choice.format_flag = JSON_FALSE;
    choice.path = path;
    return json_sax_print_start(&choice);
}
#endif

static inline int json_sax_print_null(json_sax_print_handle handle, const char *key)
{
    return json_sax_print_value(handle, JSON_NULL, key, NULL);
}

static inline int json_sax_print_bool(json_sax_print_handle handle, const char *key, json_bool_t value)
{
    return json_sax_print_value(handle, JSON_BOOL, key, &value);
}

static inline int json_sax_print_int(json_sax_print_handle handle, const char *key, int value)
{
    return json_sax_print_value(handle, JSON_INT, key, &value);
}

static inline int json_sax_print_hex(json_sax_print_handle handle, const char *key, unsigned int value)
{
    return json_sax_print_value(handle, JSON_HEX, key, &value);
}

static inline int json_sax_print_double(json_sax_print_handle handle, const char *key, double value)
{
    return json_sax_print_value(handle, JSON_DOUBLE, key, &value);
}

static inline int json_sax_print_string(json_sax_print_handle handle, const char *key, const char *value)
{
    return json_sax_print_value(handle, JSON_STRING, key, value);
}

static inline int json_sax_print_array(json_sax_print_handle handle, const char *key, json_sax_cmd_t value)
{
    return json_sax_print_value(handle, JSON_ARRAY, key, &value);
}

static inline int json_sax_print_object(json_sax_print_handle handle, const char *key, json_sax_cmd_t value)
{
    return json_sax_print_value(handle, JSON_OBJECT, key, &value);
}

/**** json sax parse apis ****/
int json_sax_parse_common(json_sax_parse_choice_t *choice);

static inline int json_sax_parse_str(const char *str, json_sax_parser_callback callback)
{
    json_sax_parse_choice_t choice = {0};
    choice.str = str;
    choice.callback = callback;
    return json_sax_parse_common(&choice);
}

#if JSON_DIRECT_FILE_SUPPORT
static inline int json_sax_parse_file(const char *path, json_sax_parser_callback callback)
{
    json_sax_parse_choice_t choice = {0};
    choice.path = path;
    choice.callback = callback;
    return json_sax_parse_common(&choice);
}
#endif

#endif

#endif

