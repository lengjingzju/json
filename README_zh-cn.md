# LJSON 说明

[English Edition](./README.md)

LJSON 是一个远远快于 cJSON、大幅度快于 RapidJSON 的 C 实现的 JSON 库，他是目前最快的通用 JSON 库。
LJSON 支持 JSON 的解析、打印、编辑，提供 DOM 和 SAX 接口，I/O 支持字符串和文件，且完全支持 nativejson-benchmark 的测试用例。
LJSON 默认使用个人开发的 ldouble 算法打印浮点数，和标准库对比可能只有第15位小数的区别，是目前最快的浮点数转字符串算法；也可选择个人优化过的 grisu2 算法或 dragonbox 算法。


## 功能特点

* 更快：打印和解析速度比 cJSON 和 RapidJSON 都要快，速度最高可比 CJSON 快19倍，比 Rapid JSON 快1倍，见测试结果
* 更省：提供多种省内存的手段，例如内存池、文件边读边解析、边打印边写文件、SAX方式的接口，可做到内存占用是个常数
* 更强：支持DOM和SAX风格的API，提供普通模式和内存池模式JSON的接口，支持字符串和文件作为输入输出(可扩展支持其它流)，扩展支持长长整形和十六进制数字
* 更友好：C语言实现，不依赖任何库，不含平台相关代码，只有一个头文件和库文件，和cJSON对应一致的接口，代码逻辑比任何JSON库都更清晰

## 编译运行

### 编译方法

* 直接编译

```sh
gcc -o ljson json.c json_test.c -O2 -ffunction-sections -fdata-sections -W -Wall
```

* [IMAKE](https://github.com/lengjingzju/cbuild-ng) 编译

```sh
make O=<编译输出目录> && make O=<编译输出目录> DESTDIR=<安装目录>
```

* 交叉编译

```sh
make O=<编译输出目录> CROSS_COMPILE=<交叉编译器前缀> && make O=<编译输出目录> DESTDIR=<安装目录>
```

* 选择浮点数转字符串算法 `gcc -DJSON_DTOA_ALGORITHM=n`， n可能为 0 / 1 / 2 / 3 
    * 0: 个人实现的 ldouble 算法: 比谷歌的 grisu2 的默认实现快 **117%** ，比腾讯优化的 grisu2 实现快 **30%** ，比 sprintf 快 **13.3** 倍
    * 1: C标准库的 sprintf
    * 2: 个人优化的 grisu2 算法: 谷歌的 grisu2 的默认实现比 sprintf 快 **5.7** 倍，腾讯优化的 grisu2 实现比 sprintf 快 **9.1** 倍，LJSON 的优化实现比 sprintf 快 **11.4** 倍
    * 3: 个人优化的 dragonbox 算法: 性能和 ldouble 算法基本相差无几

### 运行方法

```sh
./json <json文件名> <测试序号0-7>
```

### 调试方法

* 设置 json.c 中的变量 `JSON_ERROR_PRINT_ENABLE` 的值为 `1` 后重新编译

### 错误检测

* 设置 json.c 中的变量 `JSON_STRICT_PARSE_MODE` 的值为 `0` / `1` / `2` 后重新编译
    * 0: 关闭不是常见的错误检测，例如解析完成后还剩尾后字符
    * 1: 检测更多的错误，且允许 key 为空字符串
    * 2: 除去 1 开启的错误检测之外，还关闭某些不是标准的特性，例如十六进制数字，第一个json对象不是array或object
        * 设置为2时 100% 符合 [nativejson-benchmark](https://github.com/miloyip/nativejson-benchmark) 的测试用例

## 性能测试

注：主要是测试速度，`O2` 优化等级且默认选项编译，测试文件来自 [nativejson-benchmark](https://github.com/miloyip/nativejson-benchmark) 项目

> 测试平台: Ambarella CV25M Board | CPU: ARM CortexA53 | OS: Linux-5.15<br>
> 测试结果: LJSON 比cJSON 解析最快可达 475%，打印最快可达 2225%，LJSON 比 RapidJSON 解析最快可达 131%，打印最快可达 137%

![AARCH64-Linux测试结果](test_result/test_for_aarch64.png)

> 测试平台: PC | CPU: Intel i7-10700 | OS: Ubuntu 18.04 (VirtualBox)<br>
> 测试结果: :LJSON 比cJSON 解析最快可达 560%，打印最快可达 2894%，LJSON 比 RapidJSON 解析最快可达 75%，打印最快可达 124%

![x86_64-Linux测试结果](test_result/test_for_x86_64.png)

![ldouble-x86_64测试结果](test_result/ldb_for_x86_64.png)

> 测试平台: Nationalchip STB | CPU: CSKY | DDR3: 128MB, 533MHz | OS: ECOS<br>
> 注: 老版本测试结果，新版本删除了临时buffer，且解析速度提升了两倍

![ECOS测试结果](test_result/test_for_csky.png)

## json对象结构

使用 long long 类型支持，编译时需要设置 json.h 中的 `JSON_LONG_LONG_SUPPORT` 值为 1

```c
struct json_list {
    struct json_list *next;
};                                      // 单向链表

struct json_list_head {
    struct json_list *next, *prev;
};                                      // 链表头，分别指向链表的第一个元素和最后一个元素

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
} json_type_t;                          // json对象类型

typedef struct {
    unsigned int type:4;                // json_type_t，json_string_t作为key时才有type
    unsigned int escaped:1;             // str是否包含需要转义的字符
    unsigned int alloced:1;             // str是否是malloc的，只用于SAX APIs
    unsigned int reserved:2;
    unsigned int len:24;                // str的长度
    char *str;
} json_string_t;                        // json string 对象或 type+key

typedef union {
    bool vbool;
    int vint;
    unsigned int vhex;
#if JSON_LONG_LONG_SUPPORT
    long long int vlint;
    unsigned long long int vlhex;
#endif
    double vdbl;
} json_number_t;                        // json数字对象值

#if JSON_SAX_APIS_SUPPORT
typedef enum {
    JSON_SAX_START = 0,
    JSON_SAX_FINISH
} json_sax_cmd_t;                       // 只用于SAX APIs，JSON_ARRAY或JSON_OBJECT有开始和结束
#endif

typedef union {
    json_number_t vnum;                 // json数字对象的值
    json_string_t vstr;                 // json字符串对象的值
#if JSON_SAX_APIS_SUPPORT
    json_sax_cmd_t vcmd;
#endif
    struct json_list_head head;         // json结构体/数组对象的值
} json_value_t;                         // json对象值

typedef struct {
    struct json_list list;              // json链表节点
    json_string_t jkey;                 // json对象的type和key
    json_value_t value;                 // json对象的值
} json_object;                          // json对象

typedef struct {
    unsigned int hash;                  // json key的hash，只有JSON_OBJECT的子项才有key
    json_object *json;                  // json对象的指针
} json_item_t;

typedef struct {
    unsigned int conflicted:1;          // key的hash是否有冲突
    unsigned int reserved:31;
    unsigned int total;                 // items分配的内存数目
    unsigned int count;                 // items中子项的个数
    json_item_t *items;                 // 存储子项的数组
} json_items_t;                         // 存储JSON_ARRAY或JSON_OBJECT的所有子项
```

* 使用单向链表管理json节点树

## 经典编辑模式接口

```c
void json_memory_free(void *ptr);
```
* json_item_total_get: 释放经典编辑模式申请的内存或打印到字符串返回的指针

```c
int json_item_total_get(json_object *json);
```

* json_item_total_get: 获取节点总数

```c
void json_del_object(json_object *json);
json_object *json_new_object(json_type_t type);
json_object *json_create_item(json_type_t type, void *value);
json_object *json_create_item_array(json_type_t type, void *values, int count);

static inline json_object *json_create_null(void);
static inline json_object *json_create_bool(bool value);
static inline json_object *json_create_int(int value);
static inline json_object *json_create_hex(unsigned int value);
#if JSON_LONG_LONG_SUPPORT
static inline json_object *json_create_lint(long long int value);
static inline json_object *json_create_lhex(unsigned long long int value);
#endif
static inline json_object *json_create_double(double value);
static inline json_object *json_create_string(json_string_t *value);
static inline json_object *json_create_array(void);
static inline json_object *json_create_object(void);

static inline json_object *json_create_bool_array(bool *values, int count);
static inline json_object *json_create_int_array(int *values, int count);
static inline json_object *json_create_hex_array(unsigned int *values, int count);
#if JSON_LONG_LONG_SUPPORT
static inline json_object *json_create_lint_array(long long int *values, int count);
static inline json_object *json_create_lhex_array(unsigned long long int *values, int count);
#endif
static inline json_object *json_create_double_array(double *values, int count);
static inline json_object *json_create_string_array(json_string_t *values, int count);
```

* json_del_object: 删除节点(并递归删除子节点)
* json_new_object: 创建指定类型的空节点
* json_create_item: 创建指定类型的有值节点
* json_create_item_array: 快速创建指定类型的数组节点，使用要点同上
* 要点：创建的节点使用完后需要使用`json_del_object`删除，但是如果把该节点加入了array或object，该节点无需再删除

```c
void json_string_info_update(json_string_t *jstr);
unsigned int json_string_hash_code(json_string_t *jstr);
int json_string_strdup(json_string_t *src, json_string_t *dst);
static inline int json_set_key(json_object *json, json_string_t *jkey);
static inline int json_set_string_value(json_object *json, json_string_t *jstr);
```

* json_string_info_update: 更新jstr的len和escaped，如果传入的len大于0，则什么都不做
* json_string_hash_code: 获取字符串的hash值
* json_string_strdup: 修改LJSON中的字符串
* json_set_key: 修改节点的key(JSON_OBJECT类型下的子节点才有key)
* json_set_string_value: 修改string类型节点的value

```c
int json_get_number_value(json_object *json, json_type_t type, void *value);
int json_set_number_value(json_object *json, json_type_t type, void *value);

static inline bool json_get_bool_value(json_object *json);
static inline int json_get_int_value(json_object *json);
static inline unsigned int json_get_hex_value(json_object *json);
#if JSON_LONG_LONG_SUPPORT
static inline long long int json_get_lint_value(json_object *json);
static inline unsigned long long int json_get_lhex_value(json_object *json);
#endif
static inline double json_get_double_value(json_object *json);

static inline int json_set_bool_value(json_object *json, bool value);
static inline int json_set_int_value(json_object *json, int value);
static inline int json_set_hex_value(json_object *json, unsigned int value);
#if JSON_LONG_LONG_SUPPORT
static inline int json_set_lint_value(json_object *json, long long int value);
static inline int json_set_lhex_value(json_object *json, unsigned long long int value);
#endif
static inline int json_set_double_value(json_object *json, double value);
```

* json_get_number_value: 获取number类型节点的value，返回值: 正值(原有类型枚举值)表示成功有强制转换，0表示成功且类型对应，-1表示失败不是number类型
* json_set_number_value: 修改number类型节点的value，返回值说明同上

```c
int json_get_array_size(json_object *json);
int json_get_object_size(json_object *json);
json_object *json_get_array_item(json_object *json, int seq, json_object **prev);
json_object *json_get_object_item(json_object *json, const char *key, json_object **prev);
```

* json_get_array_size: 获取array类型节点的大小(有多少个一级子节点)
* json_get_object_size: 获取object类型节点的大小(有多少个一级子节点)
* json_get_array_item: 获取array类型节点的的第seq个子节点
* json_get_object_item: 获取object类型节点的指定key的子节点


```c
json_object *json_search_object_item(json_items_t *items, json_string_t *jkey, unsigned int hash);
void json_free_items(json_items_t *items);
int json_get_items(json_object *json, json_items_t *items);
```

* json_search_object_item: 二分法查找items中的指定key的json对象
* json_free_items: 释放items中分配的内存
* json_get_items: 获取JSON_ARRAY或JSON_OBJECT中的所有一级子节点，加速访问

```c
int json_add_item_to_array(json_object *array, json_object *item);
int json_add_item_to_object(json_object *object, json_object *item);
```

* 将节点加入到array或object，加入object需要先设置item的key
* 经典模式如果该节点加入成功，`无需`再调用`json_del_object`删除该节点

```c
json_object *json_detach_item_from_array(json_object *json, int seq);
json_object *json_detach_item_from_object(json_object *json, const char *key);
```

* 将指定的子节点从array或object取下并返回
* 使用完成后`需要`使用`json_del_object`删除返回的子节点
* 注：使用内存cache的json`不需要`调用`json_del_object`删除返回的子节点

```c
int json_del_item_from_array(json_object *json, int seq);
int json_del_item_from_object(json_object *json, const char *key);
```

* 将指定的子节点从array或object删除

```c
int json_replace_item_in_array(json_object *array, int seq, json_object *new_item);
int json_replace_item_in_object(json_object *object, json_object *new_item);
```

* 将array或object指定的子节点替换成new_item
* 如果原来的子节点不存在就直接新增new_item

```c
json_object *json_deepcopy(json_object *json);
int json_copy_item_to_array(json_object *array, json_object *item);
int json_copy_item_to_object(json_object *object, json_object *item);
```

* json_deepcopy: 节点深度复制
* json_copy_item_to_xxxx: 将节点复制并加入到array或object
* 如果该节点加入成功，`还需要`再调用`json_del_object`删除原来传入的节点

```c
json_object *json_add_new_item_to_array(json_object *array, json_type_t type, void* value);
json_object *json_add_new_item_to_object(json_object *object, json_type_t type, json_string_t *jkey, void* value);

static inline json_object *json_add_null_to_array(json_object *array);
static inline json_object *json_add_bool_to_array(json_object *array, bool value);
static inline json_object *json_add_int_to_array(json_object *array, int value);
static inline json_object *json_add_hex_to_array(json_object *array, unsigned int value);
#if JSON_LONG_LONG_SUPPORT
static inline json_object *json_add_lint_to_array(json_object *array, long long int value);
static inline json_object *json_add_lhex_to_array(json_object *array, unsigned long long int value);
#endif
static inline json_object *json_add_double_to_array(json_object *array, double value);
static inline json_object *json_add_string_to_array(json_object *array, json_string_t *value);
static inline json_object *json_add_array_to_array(json_object *array);
static inline json_object *json_add_object_to_array(json_object *array);

static inline json_object *json_add_null_to_object(json_object *object, json_string_t *jkey);
static inline json_object *json_add_bool_to_object(json_object *object, json_string_t *jkey, bool value);
static inline json_object *json_add_int_to_object(json_object *object, json_string_t *jkey, int value);
static inline json_object *json_add_hex_to_object(json_object *object, json_string_t *jkey, unsigned int value);
#if JSON_LONG_LONG_SUPPORT
static inline json_object *json_add_lint_to_object(json_object *object, json_string_t *jkey, long long int value);
static inline json_object *json_add_lhex_to_object(json_object *object, json_string_t *jkey, unsigned long long int value);
#endif
static inline json_object *json_add_double_to_object(json_object *object, json_string_t *jkey, double value);
static inline json_object *json_add_string_to_object(json_object *object, json_string_t *jkey, json_string_t *value);
static inline json_object *json_add_array_to_object(json_object *object, json_string_t *jkey);
static inline json_object *json_add_object_to_object(json_object *object, json_string_t *jkey);
```

* json_add_new_item_to_array: 新建指定类型的节点，并将该节点加入array
* json_add_new_item_to_object: 新建指定类型的节点，并将该节点加入object

```c
/*
 * The below APIs are also available to pool json:
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
```

* `编辑(一般模式)`的一些API(内部没有调用malloc/free)也可以用于内存池
* 注意: pool模式时，json_detach_item_from_array/object返回的json节点不能使用`json_del_object`删除

## 内存池结构

```c
typedef struct {
    struct json_list list;              // 链表节点
    size_t size;                        // 内存大小
    char *ptr;                          // 首部地址
    char *cur;                          // 当前地址
} json_mem_node_t;

typedef struct {
    struct json_list_head head;         // json_mem_node_t挂载节点
    size_t mem_size;                    // 默认分配块内存大小
    json_mem_node_t *cur_node;          // 当前使用的内存节点
} json_mem_mgr_t;

typedef struct {
    json_mem_mgr_t obj_mgr;             // 对象节点的内存管理
    json_mem_mgr_t key_mgr;             // 字符串key的内存管理
    json_mem_mgr_t str_mgr;             // 字符串value的内存管理
} json_mem_t;
```

* 内存池原理是先分配一个大内存，然后从大内存中分配小内存
* 内存池只能统一释放申请

## 内存池编辑模式接口

```c
void pjson_memory_free(json_mem_t *mem);
void pjson_memory_init(json_mem_t *mem);
+int pjson_memory_statistics(json_mem_mgr_t *mgr);
```

* pjson_memory_free: 释放json内存池管理的所有内存
* pjson_memory_init: 初始化json内存池管理结构
* pjson_memory_statistics: 统计内存池分配的内存
* 注：编辑模式初始化内存池后可修改mem_size
* 注：使用内存池前需要使用pjson_memory_init初始化内存池入口，全部使用完成后使用pjson_memory_free释放
* 注：绝对不要调用存在malloc, free之类的api，例如`json_new_object`和`json_del_object`等

```c
json_object *pjson_new_object(json_type_t type, json_mem_t *mem);
json_object *pjson_create_item(json_type_t type, void *value, json_mem_t *mem);
json_object *pjson_create_item_array(json_type_t item_type, void *values, int count, json_mem_t *mem);

static inline json_object *pjson_create_null(json_mem_t *mem);
static inline json_object *pjson_create_bool(bool value, json_mem_t *mem);
static inline json_object *pjson_create_int(int value, json_mem_t *mem);
static inline json_object *pjson_create_hex(unsigned int value, json_mem_t *mem);
#if JSON_LONG_LONG_SUPPORT
static inline json_object *pjson_create_lint(long long int value, json_mem_t *mem);
static inline json_object *pjson_create_lhex(unsigned long long int value, json_mem_t *mem);
#endif
static inline json_object *pjson_create_double(double value, json_mem_t *mem);
static inline json_object *pjson_create_string(json_string_t *value, json_mem_t *mem);
static inline json_object *pjson_create_array(json_mem_t *mem);
static inline json_object *pjson_create_object(json_mem_t *mem);

static inline json_object *pjson_create_bool_array(bool *values, int count, json_mem_t *mem);
static inline json_object *pjson_create_int_array(int *values, int count, json_mem_t *mem);
static inline json_object *pjson_create_hex_array(unsigned int *values, int count, json_mem_t *mem);
#if JSON_LONG_LONG_SUPPORT
static inline json_object *pjson_create_lint_array(long long int *values, int count, json_mem_t *mem);
static inline json_object *pjson_create_lhex_array(unsigned long long int *values, int count, json_mem_t *mem);
#endif
static inline json_object *pjson_create_double_array(double *values, int count, json_mem_t *mem);
static inline json_object *pjson_create_string_array(json_string_t *values, int count, json_mem_t *mem);
```

* pjson_new_object: 在内存池中创建指定类型的空节点
* pjson_create_item: 在内存池中创建指定类型的有值节点
* pjson_create_item_array: 在内存池中创建(子节点指定类型)的array节点

```c
int pjson_string_strdup(json_string_t *src, json_string_t *dst, json_mem_mgr_t *mgr);
static inline int pjson_set_key(json_object *json, json_string_t *jkey, json_mem_t *mem);
static inline int pjson_set_string_value(json_object *json, json_string_t *jstr, json_mem_t *mem);
```

* pjson_string_strdup: 修改JSON中的字符串，该字符串在内存池中分配
* pjson_set_key: 修改json节点的key，该key在内存池中分配
* pjson_set_string_value: 修改 JSON_STRING 类型json节点的值，该值在内存池中分配

```c
int pjson_replace_item_in_array(json_object *array, int seq, json_object *new_item);
int pjson_replace_item_in_object(json_object *object, json_object *new_item);
```

* 将array或object指定的子节点替换成new_item
* 如果原来的子节点不存在就直接新增new_item

```c
json_object *pjson_deepcopy(json_object *json, json_mem_t *mem);
int pjson_copy_item_to_array(json_object *array, json_object *item, json_mem_t *mem);
int pjson_copy_item_to_object(json_object *object, json_object *item, json_mem_t *mem);
```

* pjson_deepcopy: 节点深度复制
* pjson_copy_item_to_xxxx: 将节点复制并加入到array或object

```c
json_object *pjson_add_new_item_to_array(json_object *array, json_type_t type, void *value, json_mem_t *mem);
json_object *pjson_add_new_item_to_object(json_object *object, json_type_t type, json_string_t *jkey, void *value, json_mem_t *mem);

static inline json_object *pjson_add_null_to_array(json_object *array, json_mem_t *mem);
static inline json_object *pjson_add_bool_to_array(json_object *array, bool value, json_mem_t *mem);
static inline json_object *pjson_add_int_to_array(json_object *array, int value, json_mem_t *mem);
static inline json_object *pjson_add_hex_to_array(json_object *array, unsigned int value, json_mem_t *mem);
#if JSON_LONG_LONG_SUPPORT
static inline json_object *pjson_add_lint_to_array(json_object *array, long long int value, json_mem_t *mem);
static inline json_object *pjson_add_lhex_to_array(json_object *array, unsigned long long int value, json_mem_t *mem);
#endif
static inline json_object *pjson_add_double_to_array(json_object *array, double value, json_mem_t *mem);
static inline json_object *pjson_add_string_to_array(json_object *array, json_string_t *value, json_mem_t *mem);
static inline json_object *pjson_add_array_to_array(json_object *array, json_mem_t *mem);
static inline json_object *pjson_add_object_to_array(json_object *array, json_mem_t *mem);

static inline json_object *pjson_add_null_to_object(json_object *object, json_string_t *jkey, json_mem_t *mem);
static inline json_object *pjson_add_bool_to_object(json_object *object, json_string_t *jkey, bool value, json_mem_t *mem);
static inline json_object *pjson_add_int_to_object(json_object *object, json_string_t *jkey, int value, json_mem_t *mem);
static inline json_object *pjson_add_hex_to_object(json_object *object, json_string_t *jkey, unsigned int value, json_mem_t *mem);
#if JSON_LONG_LONG_SUPPORT
static inline json_object *pjson_add_lint_to_object(json_object *object, json_string_t *jkey, long long int value, json_mem_t *mem);
static inline json_object *pjson_add_lhex_to_object(json_object *object, json_string_t *jkey, unsigned long long int value, json_mem_t *mem);
#endif
static inline json_object *pjson_add_double_to_object(json_object *object, json_string_t *jkey, double value, json_mem_t *mem);
static inline json_object *pjson_add_string_to_object(json_object *object, json_string_t *jkey, json_string_t *value, json_mem_t *mem);
static inline json_object *pjson_add_array_to_object(json_object *object, json_string_t *jkey, json_mem_t *mem);
static inline json_object *pjson_add_object_to_object(json_object *object, json_string_t *jkey, json_mem_t *mem);
```

* pjson_add_new_item_to_array: 在内存池中创建指定类型的子节点，并加入到array
* pjson_add_new_item_to_object: 在内存池中创建指定类型的子节点，并加入到object

## DOM打印/DOM解析

```c
typedef struct {
    size_t str_len;                     // 打印到字符串时返回生成的字符串长度(strlen)
    size_t plus_size;                   // 打印生成的字符串的realloc的增量大小 / write buffer的缓冲区大小
    size_t item_size;                   // 每个json对象生成字符串的预估的平均长度
    int item_total;                     // json对象节点的总数
    bool format_flag;                   // 字符串是否进行格式化
    const char *path;                   // 文件保存路径
} json_print_choice_t;
```

* plus_size: 经典模式下打印字符串realloc的增量，或write buffer的缓冲区大小，最小值/默认值为 JSON_PRINT_SIZE_PLUS_DEF
* item_size: 每个json对象生成字符串的预估的平均长度，最小值/默认值为 JSON_UNFORMAT_ITEM_SIZE_DEF 和 JSON_FORMAT_ITEM_SIZE_DEF
* item_total: json对象节点的总数如果此值未设置，将自动计算总数；否则取默认值JSON_PRINT_NUM_PLUS_DEF
* format_flag: 格式化打印选项，`false: 压缩打印；true: 格式化打印`
* path: 如果path不为空，将直接边打印边输出到文件；否则是打印到一个大的完整字符串

```c
char *json_print_common(json_object *json, json_print_choice_t *choice);

static inline char *json_print_format(json_object *json, int item_total, size_t *length);
static inline char *json_print_unformat(json_object *json, int item_total, size_t *length);
static inline char *json_fprint_format(json_object *json, int item_total, const char *path);
static inline char *json_fprint_unformat(json_object *json, int item_total, const char *path);
```

* json_print_common: 打印通用接口
* json_print_format: 格式化打印成字符串的简写接口，`需要` `json_memory_free`释放返回的字符串
* json_print_unformat: 类似json_print_format，只是非格式化打印
* json_fprint_format: 格式化直接边打印边输出到文件的简写接口，成功返回"ok"字符串，`不需要` `json_memory_free`释放返回的字符串
* json_fprint_unformat: 类似json_fprint_format，只是非格式化打印

```c
typedef struct {
    size_t mem_size;                    // 内存池每个内存块的大小
    size_t read_size;                   // json读缓冲的初始大小
    size_t str_len;                     // 要解析的字符串长度
    bool reuse_flag;                    // 是否复用原始json字符串，原始json字符串会被修改
    json_mem_t *mem;                    // 内存池管理结构
    const char *path;                   // 要解析的json文件的路径
    char *str;                          // 要解析的json字符串的指针
} json_parse_choice_t;
```

* mem_size: 内存池每个内存块的大小，最小值为 (str_len / JSON_PARSE_NUM_DIV_DEF) 的值
* read_size: json读缓冲的初始大小，最小值 JSON_PARSE_READ_SIZE_DEF
* str_len: 要解析的字符串长度 strlen(str)，使用内存池时该参数有效，如果为0，json_parse_common会自己计算一次
* path: 要解析的json文件，str 和 path 有且只有一个有值
* str: 要解析的json字符串，str 和 path 有且只有一个有值

```c
json_object *json_parse_common(json_parse_choice_t *choice);
static inline json_object *json_parse_str(char *str, size_t str_len);
static inline json_object *json_fast_parse_str(char *str, size_t str_len, json_mem_t *mem);
static inline json_object *json_reuse_parse_str(char *str, size_t str_len, json_mem_t *mem);
static inline json_object *json_parse_file(const char *path);
static inline json_object *json_fast_parse_file(const char *path, json_mem_t *mem);
```

* json_parse_common: 解析通用接口
* json_parse_str: 类似cJSON的经典字符串解析的简写接口，用完后需要`json_del_object`释放返回的管理结构
* json_fast_parse_str: 使用内存池的字符串解析的简写接口，使用前必须使用`pjson_memory_init`初始化mem，用完后需要`pjson_memory_free`释放
* json_reuse_parse_str: 使用内存池极速解析并复用原始字符串，`会修改传入的字符串，使用过程中不要释放原始的str` , `速度最快，占用内存最少`
* json_parse_file: 类似json_parse_str，只是从文件边读边解析
* json_fast_parse_file: 类似json_parse_str， 只是边读文件边解析

## SAX打印/SAX解析

使用 SAX APIs 编译时需要设置 json.h 中的 `JSON_SAX_APIS_SUPPORT` 值为 1

```c
typedef void* json_sax_print_hd;
```

* json_sax_print_hd: 实际是json_sax_print_t指针

```c
json_sax_print_hd json_sax_print_start(json_print_choice_t *choice);
static inline json_sax_print_hd json_sax_print_format_start(int item_total);
static inline json_sax_print_hd json_sax_print_unformat_start(int item_total);
static inline json_sax_print_hd json_sax_fprint_format_start(int item_total, const char *path);
static inline json_sax_print_hd json_sax_fprint_unformat_start(int item_total, const char *path);
```

* json_sax_print_start: sax打印时必须先调用此函数，进行资源初始化并获取句柄 json_sax_print_hd
  * 如果打印到字符串，最好给一个item_total值，用于计算生成字符串的增量

```c
int json_sax_print_value(json_sax_print_hd handle, json_type_t type, json_string_t *jkey, const void *value);
static inline int json_sax_print_null(json_sax_print_hd handle, json_string_t *jkey);
static inline int json_sax_print_bool(json_sax_print_hd handle, json_string_t *jkey, bool value);
static inline int json_sax_print_int(json_sax_print_hd handle, json_string_t *jkey, int value);
static inline int json_sax_print_hex(json_sax_print_hd handle, json_string_t *jkey, unsigned int value);
#if JSON_LONG_LONG_SUPPORT
static inline int json_sax_print_lint(json_sax_print_hd handle, json_string_t *jkey, long long int value);
static inline int json_sax_print_lhex(json_sax_print_hd handle, json_string_t *jkey, unsigned long long int value);
#endif
static inline int json_sax_print_double(json_sax_print_hd handle, json_string_t *jkey, double value);
static inline int json_sax_print_string(json_sax_print_hd handle, json_string_t *jkey, json_string_t *value);
static inline int json_sax_print_array(json_sax_print_hd handle, json_string_t *jkey, json_sax_cmd_t value);
static inline int json_sax_print_object(json_sax_print_hd handle, json_string_t *jkey, json_sax_cmd_t value);
```

* json_sax_print_value: sax 条目通用打印接口，如果要打印节点的父节点是object，key必须有值；其它情况下key填不填值均可
  * array 和 object 要打印两次，一次值是 `JSON_SAX_START` 表示开始，一次值是 `JSON_SAX_FINISH` 表示完成
  * 传入key时可以先不用`json_saxstr_update` 计算长度

```c
char *json_sax_print_finish(json_sax_print_hd handle, size_t *length);
```

* json_sax_print_finish: sax打印完成必须调用此函数，释放中间资源并返回字符串
  * 打印成字符串时，该函数返回打印的字符串， `需要` `json_memory_free`释放返回的字符串
  * 直接边打印边输出到文件时，成功返回"ok"字符串，`不需要` `json_memory_free`释放返回的字符串

```c
typedef enum {
    JSON_SAX_PARSE_CONTINUE = 0,
    JSON_SAX_PARSE_STOP
} json_sax_ret_t;

typedef struct {
    int total;
    int index;
    json_string_t *array;
    json_value_t value;
} json_sax_parser_t;
typedef json_sax_ret_t (*json_sax_cb_t)(json_sax_parser_t *parser);

typedef struct {
    char *str;
    const char *path;
    size_t read_size;
    json_sax_cb_t cb;
} json_sax_parse_choice_t;
```

* json_sax_ret_t: JSON_SAX_PARSE_CONTINUE 表示SAX解析器继续解析，JSON_SAX_PARSE_STOP 表示中断解析
* json_sax_cb_t: 调用者自己填写的回调函数，必须有值，返回 `JSON_SAX_PARSE_STOP` 表示中断解析并返回
* json_sax_parser_t: 传给回调函数的值
  * array 是 array/object `类型+key` 的层次结构，total表示当前分配了多少层次，index表示当前用了多少层次，即当前层为 `array[index]`
  * value 是当前层的值
* json_sax_parse_choice_t: 参考 `json_parse_choice_t` 说明

```c
int json_sax_parse_common(json_sax_parse_choice_t *choice);
static inline int json_sax_parse_str(char *str, size_t str_len, json_sax_cb_t cb);
static inline int json_sax_parse_file(const char *path, json_sax_cb_t cb);
```

* json_sax_parse_common: sax解析通用接口，因为不需要保存json树结构，所以不需要使用内存池
* json_sax_parse_str: 从字符串解析的快捷接口
* json_sax_parse_file: 从文件边读边解析的快捷接口

## 参考代码

* 参考了[cJSON](https://github.com/DaveGamble/cJSON)的实现，其中utf16_literal_to_utf8函数完全来自cJSON

## 联系方式

* Phone: +86 18368887550
* wx/qq: 1083936981
* Email: lengjingzju@163.com 3090101217@zju.edu.cn
