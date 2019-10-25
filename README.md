# JSON使用和接口说明

## 功能特点
* 比cJSON更快，更省内存
* 支持边读文件边解析成json控制结构
* 支持json控制结构边打印边输出到文件
* 接口和cJSON一样友好，代码逻辑比cJSON更清晰，代码大小比cJSON稍大一些
* json解析和打印接口是弹性的，可以设置参数，实现特定类型的json解析效率的提高
* 除类似cJSON的经典接口外，另外提供一套使用内存池的接口

## 性能测试
* 解析json字符串成json控制结构，解析速度最多快`250%`，耗用内存可省`100%`以上
* json控制结构打印成json字符串，打印速度最多快`150%`
* 边读文件边解析成json控制结构(无需全部读完文件再解析)，速度基本无损失
* json控制结构边打印边输出到文件(无需全部打印完再写文件)，速度基本无损失
* 注：测试平台(Nationalchip STB | CPU: CSKY | DDR3: 128MB, 533MHz | OS:  ECOS)

## 接口说明

### 节点
```C
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
        int vint;                   // bool, int
        unsigned int vhex;          // hex, 0xaaaaaaaa
        double vdbl;                // double
        char *vstr;                 // string
        struct json_list_head head; // array, object
    };
} json_object;
```
* 使用双向链表管理节点，类似linux内核的lish.h

### 打印
```C
// print choice size
#define JSON_FULL_ADD_SIZE_DEF              8096
#define JSON_TEMP_ADD_SIZE_DEF              1024
#define JSON_ITEM_UNFORMAT_CELL_SIZE_DEF    24
#define JSON_ITEM_FORMAT_CELL_SIZE_DEF      32

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

/* json print apis */
char *json_print_common(json_object *json, json_print_choice_t *choice);
char *json_print_format(json_object *json);
char *json_print_unformat(json_object *json);
void json_free_print_ptr(void *ptr);
#if JSON_DIRECT_FILE_SUPPORT
char *json_fprint_format(json_object *json, const char *path);
char *json_fprint_unformat(json_object *json, const char *path);
#endif

```
* full_add_size: 经典模式下打印字符串realloc的增量，或write buffer的缓冲区大小，最小值/默认值为 JSON_FULL_ADD_SIZE_DEF
* temp_add_size: 临时转换buffer的realloc的增量的最小值，如果增大，用完后代码会自动realloc到最小值，最小值/默认值为 JSON_TEMP_ADD_SIZE_DEF
* item_cell_size: 预估的一个节点打印成字符串后的长度，最小值/默认值为 JSON_ITEM_UNFORMAT_CELL_SIZE_DEF 和 JSON_ITEM_FORMAT_CELL_SIZE_DEF
* path: 如果path不为空并设置JSON_PRINT_DIRECT_FILE，将直接边打印边输出到文件
* format_flag: 格式化打印选项
* method_flag: JSON_PRINT_REALLOC_ORIG, 经典模式和cJSON类似；`JSON_PRINT_REALLOC_CAL`，算法预估 realloc大小，`速度更快`；JSON_PRINT_DIRECT_FILE，文件打印
* json_print_common: 打印通用接口
* json_print_format, json_print_unformat: 打印成字符串的简写接口，`需要` `json_free_print_ptr`释放返回的字符串
* json_fprint_format, json_fprint_unformat: 直接边打印边输出到文件的简写接口，成功返回 path，`不需要` `json_free_print_ptr`释放返回的字符串

### 解析
```C
// file parse choice size
#define JSON_PARSE_READ_SIZE_DEF            128
#define JSON_PARSE_USE_SIZE_DEF             8096
#define JSON_STR_MULTIPLE_NUM               8

#define JSON_PAGE_ALIGEN_BYTES              8096
#define JSON_DATA_ALIGEN_BYTES              4
#define JSON_KEY_TOTAL_NUM_DEF              100

typedef struct {
    const char *str;
#if JSON_DIRECT_FILE_SUPPORT
    const char *path;
    size_t read_size;
#endif
    // only for json with using cache
    size_t str_len;
    size_t mem_size;
    json_bool_t repeat_key_check;
    struct json_list_head *head;
} json_parse_choice_t;

/* json parse apis */
json_object *json_parse_common(json_parse_choice_t *choice);
json_object *json_parse_str(const char *str);
json_object *json_fast_parse_str(const char *str, struct json_list_head *head, size_t str_len);
#if JSON_DIRECT_FILE_SUPPORT
json_object *json_parse_file(const char *path);
json_object *json_fast_parse_file(const char *path, struct json_list_head *head);
#endif
json_object *json_rapid_parse_str(char *str, struct json_list_head *head, size_t str_len);
```
* str: 要解析的字符串， str 和 path 有且只有一个有值
* path: 要解析的文件
* read_size: 每次读文件的读取大小，最小值 JSON_PARSE_READ_SIZE_DEF
* 要解析的字符串长度 strlen(str)，如果为0，json_parse_common会自己计算一次
* mem_size: 每个内存池的总大小，最小值为 (str_len / JSON_STR_MULTIPLE_NUM) 向上取JSON_PAGE_ALIGEN_BYTES的整
* repeat_key_check: 是否检查重复key，设为真时会降低速度，但是会少占用内存，key指针记录数组的realloc的增量是JSON_KEY_TOTAL_NUM_DEF
* head: 内存池入口节点，使用前必须使用`json_cache_memory_init`初始化，`使用内存池解析速度更快，占用内存更少`
* 注：JSON_PAGE_ALIGEN_BYTES 是内存池的默认大小和对齐大小
* 注：JSON_DATA_ALIGEN_BYTES 有些体系结构的指针需要`4字节对齐`，不然会死机，如果OBJECT_MEM_ID和STRING_MEM_ID相同，必须打开CHECK_MEM_ALIGN和设置对齐字节
* json_parse_common: 解析通用接口
* json_parse_str: 类似cJSON的经典字符串解析的简写接口，用完后需要`json_delete_json`释放返回的管理结构
* json_parse_file: 同上，只是从文件边读边解析
* json_fast_parse_str: 使用内存池的字符串解析的简写接口，使用前必须使用`json_cache_memory_init`初始化，用完后需要`json_cache_memory_free`释放内存
* json_fast_parse_file: 同上，只是边读文件边解析
* json_rapid_parse_str: 不调用json_parse_common，使用内存池极速解析，`会修改传入的字符串，使用过程中不要释放原始的str`,`速度最快，占用内存最少`

### 编辑(一般模式)

```C
int json_item_total_get(json_object *json);
```
* 获取节点总数

```C
void json_delete_value(json_object *json);
```
* 删除节点的value

```C
void json_delete_json(json_object *json);
```
* 删除节点

```C
json_object *json_new_object(json_type_t type);
static inline json_object *json_create_null(void);
static inline json_object *json_create_bool(json_bool_t value);
static inline json_object *json_create_int(int value);
static inline json_object *json_create_hex(unsigned int value);
static inline json_object *json_create_double(double value);
static inline json_object *json_create_string(const char *value);
static inline json_object *json_create_array(void);
static inline json_object *json_create_object(void);
```
* 新建节点，使用完后需要使用`json_delete_json`删除节点
* 如果把该节点加入了array或object，该节点无需再删除
* 后面的static inline xxxx() 函数是快速模式

```C
json_object *json_create_bool_array(json_bool_t *values, int count);
json_object *json_create_int_array(int *values, int count);
json_object *json_create_hex_array(unsigned int *values, int count);
json_object *json_create_double_array(double *values, int count);
json_object *json_create_string_array(char **values, int count);
```
* 快速创建数组节点，使用要点同上

```C
int json_change_key(json_object *json, const char *key);
```
* 修改节点的key，object类型下的子节点必须要求key


```C
int json_get_number_value(json_object *json, json_bool_t *vbool, int *vint, unsigned int *vhex, double *vdbl);
int json_change_number_value(json_object *json, json_bool_t *vbool, int *vint, unsigned int *vhex, double *vdbl);
int json_strict_change_number_value(json_object *json, json_bool_t *vbool, int *vint, unsigned int *vhex, double *vdbl);
static inline json_bool_t json_get_bool_value(json_object *json);
static inline int json_get_int_value(json_object *json);
static inline unsigned int json_get_hex_value(json_object *json);
static inline double json_get_double_value(json_object *json);
static inline int json_change_bool_value(json_object *json, json_bool_t value);
static inline int json_change_int_value(json_object *json, int value);
static inline int json_change_hex_value(json_object *json, unsigned int value);
static inline int json_change_double_value(json_object *json, double value);
```
* 获取或修改number类型节点的value，传入的vbool vint vhex vdbl `有且只有一个有值`，返回1表明有强制转换，返回0表明类型对应，返回-1表明不是number类型
* 后面的static inline xxxx() 函数是快速模式

```C
int json_change_string_value(json_object *json, const char *str);
```
* 修改string类型节点的value

```C
int json_get_array_size(json_object *json);
```
* 获取array类型节点的大小(有多少个一级子节点)

```C
json_object *json_get_array_item(json_object *json, int seq);
```
* 获取array类型节点的的第seq个子节点

```C
json_object *json_get_object_item(json_object *json, const char *key);
```
* 获取object类型节点的同样key的子节点

```C
json_object *json_detach_item_from_array(json_object *json, int seq);
json_object *json_detach_item_from_object(json_object *json, const char *key);
```
* 将指定子节点从array或object取下并返回，使用完成后`需要`使用`json_delete_json`删除返回的子节点
* 注：使用内存cache的json`不需要`调用`json_delete_json`删除返回的子节点

```C
int json_del_item_from_array(json_object *json, int seq);
int json_del_item_from_object(json_object *json, const char *key);
```
* 将子节点从array或object删除

```C
int json_replace_item_in_array(json_object *array, int seq, json_object *new_item);
int json_replace_item_in_object(json_object *object, const char *key, json_object *new_item);
```
* 将array或object指定的子节点替换成new_item，如果原来的子节点不存在就直接新增new_item

```C
int json_add_item_to_array(json_object *array, json_object *item);
int json_add_item_to_object(json_object *object, const char *key, json_object *item);
int json_add_null_to_object(json_object *object, const char *key);
int json_add_bool_to_object(json_object *object, const char *key, json_bool_t value);
int json_add_int_to_object(json_object *object, const char *key, int value);
int json_add_hex_to_object(json_object *object, const char *key, unsigned int value);
int json_add_double_to_object(json_object *object, const char *key, double value);
int json_add_string_to_object(json_object *object, const char *key, const char *value);
```
* 将节点加入到array或object，如果该节点加入成功，`无需`再调用`json_delete_json`删除该节点

```C
json_object *json_deepcopy(json_object *json);
int json_copy_item_to_array(json_object *array, json_object *item);
int json_copy_item_to_object(json_object *object, const char *key, json_object *item);
```
* 节点深度复制
* 将节点复制并加入到array或object，如果该节点加入成功，`还需要`再调用`json_delete_json`删除原来传入的节点


### 编辑(使用内存池)

```C
void json_cache_memory_init(struct json_list_head *head);
void json_cache_memory_free(struct json_list_head *head);
```
* 使用内存池前需要使用json_cache_memory_init初始化内存池入口
* 使用内存池完成后需要使用json_cache_memory_free释放内存池
* 绝对不要调用存在malloc, free之类的api，例如`json_new_object`和`json_delete_json`等

```C
json_object *json_cache_new_object(json_type_t type, struct json_list_head *head, size_t cache_size);
json_object *json_cache_create_int(int value, struct json_list_head *head, size_t cache_size);
json_object *json_cache_create_string(const char *value, struct json_list_head *head, size_t cache_size);
```
* 在内存池中分配一个指定类型的节点

```C
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
```
* 在内存池中创建指定类型的array节点

```C
char *json_cache_new_string(size_t len, struct json_list_head *head, size_t cache_size);
```
* 在内存池中分配一个字符串

```C
int json_cache_change_key(json_object *json, const char *key, struct json_list_head *head, size_t cache_size);
```
* 修改节点的key，该key在内存池中分配

```C
int json_cache_change_string(json_object *json, const char *str, struct json_list_head *head, size_t cache_size);
```
* 修改string类型节点的value，该value在内存池中分配

```C
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
```
* 在内存池中的object加入子节点

```C
/**
 * The following APIs are also available to json with using cache.
 * json_item_total_get
 * json_get_number_value
 * json_change_number_value
 * all "inline number apis"
 * json_strict_change_number_value
 * json_get_array_size
 * json_get_array_item
 * json_get_object_item
 * json_detach_item_from_array
 * json_detach_item_from_object
 * json_add_item_to_array
**/
```
* `编辑(一般模式)`的一些api(内部没有调用malloc/free)也可以用于内存池

## 参考代码
* 参考了[cJSON](https://github.com/DaveGamble/cJSON)的实现，其中utf16_literal_to_utf8函数完全来自cJSON

## 联系方式
* 邮箱：lengjingzju@163.com
* 微博：lengjingzju@sina.com
* wx/qq：1083936981
* 支付宝：lengjingzju@163.com
