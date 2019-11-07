#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include "json.h"

#define CHECK_MEM_ALIGN     0
#define OBJECT_MEM_ID       0
#define STRING_MEM_ID       1

#define JsonErr(fmt, args...)  do {                         \
    printf("[JsonErr][%s:%d] ", __func__, __LINE__);        \
    printf(fmt, ##args);                                    \
} while(0)

// args of json_list_entry is different to list_entry of linux
#define json_list_entry(ptr, type)  ((type *)(ptr))

#define json_list_for_each_entry(pos, head, member)         \
for (pos = json_list_entry((head)->next, typeof(*pos));     \
    &pos->member != (head);                                 \
    pos = json_list_entry(pos->member.next, typeof(*pos)))

#define json_list_for_each_entry_safe(pos, n, head, member) \
for (pos = json_list_entry((head)->next, typeof(*pos)),     \
        n = json_list_entry(pos->member.next, typeof(*pos));\
    &pos->member != (head);                                 \
    pos = n, n = json_list_entry(n->member.next, typeof(*n)))

static inline void INIT_JSON_LIST_HEAD(struct json_list_head *list)
{
    list->next = list;
    list->prev = list;
}

static inline void __json_list_add(struct json_list_head *_new,
   struct json_list_head *prev, struct json_list_head *next)
{
    next->prev = _new;
    _new->next = next;
    _new->prev = prev;
    prev->next = _new;
}

static inline void json_list_add_tail(struct json_list_head *_new, struct json_list_head *head)
{
    __json_list_add(_new, head->prev, head);
}

static inline void json_list_add(struct json_list_head *_new, struct json_list_head *head)
{
    __json_list_add(_new, head, head->next);
}

static inline void json_list_del(struct json_list_head *entry)
{
    struct json_list_head *prev = entry->prev;
    struct json_list_head *next = entry->next;
    next->prev = prev;
    prev->next = next;
    entry->next = NULL;
    entry->prev = NULL;
}

typedef struct {
    struct json_list_head list;
    int id;
    size_t size;
    char *ptr;
    char *cur;
} block_mem_node_t;

static inline void _block_mem_del(struct json_list_head *head)
{
    block_mem_node_t *pos = NULL, *n = NULL;
    json_list_for_each_entry_safe(pos, n, head, list) {
        json_list_del(&pos->list);
        json_free(pos->ptr);
        json_free(pos);
    }
}

static inline void *_block_mem_new(int id, size_t size, struct json_list_head *head)
{
    block_mem_node_t *node = NULL;
    void *ptr = NULL;
    size_t tmp1 = 0, tmp2 = 0;

    tmp1 = size % JSON_PAGE_ALIGEN_BYTES;
    tmp2 = (tmp1 == 0 && size != 0) ? (size) : (size - tmp1 + JSON_PAGE_ALIGEN_BYTES);

    if ((ptr = json_malloc(tmp2)) == NULL) {
        JsonErr("malloc failed!\n");
        return NULL;
    }
    if ((node = json_malloc(sizeof(block_mem_node_t))) == NULL) {
        json_free(ptr);
        JsonErr("malloc failed!\n");
        return NULL;
    }

    node->id = id;
    node->size = tmp2;
    node->ptr = ptr;
    node->cur = ptr;
    json_list_add(&node->list, head);

    return ptr;
}

#if CHECK_MEM_ALIGN
static inline void *_block_mem_get(size_t len, int id, size_t size,
        struct json_list_head *head, block_mem_node_t **ret_node)
{
    block_mem_node_t *pos = NULL;
    void *ptr = NULL;
    size_t tmp1 = 0, tmp2 = 0, tmp3 = 0;

    tmp1 = len % JSON_DATA_ALIGEN_BYTES;
    tmp2 = (tmp1 == 0) ? (len) : (len - tmp1 + JSON_DATA_ALIGEN_BYTES);
    tmp3 = (tmp2 > size) ? tmp2 : size;

    do {
        json_list_for_each_entry(pos, head, list) {
            if ((pos->id == id) && (pos->cur + tmp2 <= pos->ptr + pos->size)) {
                ptr = pos->cur;
                pos->cur += tmp2;
                if (ret_node)
                    *ret_node = pos;
                return ptr;
            }
        }
    } while (_block_mem_new(id, tmp3, head));

    return NULL;
}

#else
static inline void *_block_mem_get(size_t len, int id, size_t size,
        struct json_list_head *head, block_mem_node_t **ret_node)
{
    block_mem_node_t *pos = NULL;
    void *ptr = NULL;

    do {
        json_list_for_each_entry(pos, head, list) {
            if ((pos->id == id) && (pos->cur + len <= pos->ptr + pos->size)) {
                ptr = pos->cur;
                pos->cur += len;
                if (ret_node)
                    *ret_node = pos;
                return ptr;
            }
        }
    } while (_block_mem_new(id, (len > size) ? len : size, head));

    return NULL;
}
#endif

void json_cache_memory_init(struct json_list_head *head)
{
    INIT_JSON_LIST_HEAD(head);
}

void json_cache_memory_free(struct json_list_head *head)
{
    _block_mem_del(head);
}

char *json_cache_new_string(size_t len, struct json_list_head *head, size_t cache_size)
{
    char *str = NULL;

    if (cache_size < JSON_PAGE_ALIGEN_BYTES)
        cache_size = JSON_PAGE_ALIGEN_BYTES;

    if ((str = _block_mem_get(len, STRING_MEM_ID,
            cache_size, head, NULL)) == NULL) {
        JsonErr("malloc failed!\n");
        return NULL;
    }

    return str;
}

int json_cache_change_key(json_object *json, const char *key, struct json_list_head *head, size_t cache_size)
{
    size_t len = 0;

    if (!key || (len = strlen(key)) == 0) {
        return -1;
    }
    if ((json->key = json_cache_new_string(len+1, head, cache_size)) == NULL) {
        JsonErr("malloc failed!\n");
        return -1;
    }
    memcpy(json->key, key, len);
    json->key[len] = 0;

    return 0;
}

int json_cache_change_string(json_object *json, const char *str, struct json_list_head *head, size_t cache_size)
{
    size_t len = 0;

    if (json->type != JSON_STRING) {
        return -1;
    }
    if (str && (len = strlen(str)) > 0) {
        if ((json->vstr = json_cache_new_string(len+1, head, cache_size)) == NULL) {
            JsonErr("malloc failed!\n");
            return -1;
        }
        memcpy(json->vstr, str, len);
        json->vstr[len] = 0;
    } else {
        json->vstr = NULL;
    }

    return 0;
}

json_object *json_cache_new_object(json_type_t type, struct json_list_head *head, size_t cache_size)
{
    json_object *json = NULL;

    if (cache_size < JSON_PAGE_ALIGEN_BYTES)
        cache_size = JSON_PAGE_ALIGEN_BYTES;

    if ((json = _block_mem_get(sizeof(json_object), OBJECT_MEM_ID,
            cache_size, head, NULL)) == NULL) {
        JsonErr("malloc failed!\n");
        return NULL;
    }
    memset(json, 0, sizeof(json_object));
    json->type = type;

    switch (json->type) {
        case JSON_ARRAY:
        case JSON_OBJECT:
            INIT_JSON_LIST_HEAD(&json->head);
            break;
        default:
            break;
    }

    return json;
}

json_object *json_cache_create_item(json_type_t type, void *value, struct json_list_head *head, size_t cache_size)
{
    json_object *json = NULL;

    if (cache_size < JSON_PAGE_ALIGEN_BYTES)
        cache_size = JSON_PAGE_ALIGEN_BYTES;

    if ((json = _block_mem_get(sizeof(json_object), OBJECT_MEM_ID,
            cache_size, head, NULL)) == NULL) {
        JsonErr("malloc failed!\n");
        return NULL;
    }
    memset(json, 0, sizeof(json_object));
    json->type = type;

    switch (json->type) {
        case JSON_NULL:
            break;
        case JSON_BOOL:
            json->vint = (*(json_bool_t *)value == JSON_FALSE) ? 0 : 1;
            break;
        case JSON_INT:
            json->vint = *(int *)value;
            break;
        case JSON_HEX:
            json->vhex = *(unsigned int *)value;
            break;
        case JSON_DOUBLE:
            json->vdbl = *(double *)value;
            break;
        case JSON_STRING:
            if (json_cache_change_string(json, *(char **)value, head, cache_size) < 0) {
                return NULL;
            }
            break;
        case JSON_ARRAY:
        case JSON_OBJECT:
            INIT_JSON_LIST_HEAD(&json->head);
            break;
        default:
            break;
    }

    return json;
}

json_object *json_cache_create_item_array(json_type_t item_type, void *values, int count,
    struct json_list_head *head, size_t cache_size)
{
    int i = 0;
    void *value = NULL;
    json_object *json = NULL, *node = NULL;

    if ((json = json_cache_create_array(head, cache_size)) == NULL) {
        JsonErr("create array failed!\n");
        return NULL;
    }

    for (i = 0; i < count; i++) {
        switch (item_type) {
            case JSON_BOOL:
                value = ((json_bool_t *)values) + i;;
                break;
            case JSON_INT:
                value = ((int *)values) + i;;
                break;
            case JSON_HEX:
                value = ((unsigned int *)values) + i;;
                break;
            case JSON_DOUBLE:
                value = ((double *)values) + i;;
                break;
            case JSON_STRING:
                value = ((char **)values) + i;;
                break;
            default:
                JsonErr("not support json type.\n");
                return NULL;
        }

        if ((node = json_cache_create_item(item_type, value, head, cache_size)) == NULL) {
            JsonErr("create item failed!\n");
            return NULL;
        }
        json_list_add_tail(&node->list, &json->head);
    }

    return json;
}

int json_cache_add_item_to_object(json_object *object, const char *key, json_object *item,
        struct json_list_head *head, size_t cache_size)
{
    if (object->type == JSON_OBJECT) {
        if (json_cache_change_key(item, key, head, cache_size) < 0) {
            JsonErr("add key failed!\n");
            return -1;
        }
        json_list_add_tail(&item->list, &object->head);
        return 0;
    }
    return -1;
}

int json_cache_add_new_item_to_object(json_object *object, json_type_t type, const char *key, void *value,
    struct json_list_head *head, size_t cache_size)
{
    json_object *item = NULL;

    if (object->type == JSON_OBJECT) {
        switch (type) {
            case JSON_NULL:
            case JSON_BOOL:
            case JSON_INT:
            case JSON_HEX:
            case JSON_DOUBLE:
            case JSON_STRING:
                if ((item = json_cache_create_item(type, value, head, cache_size)) == NULL) {
                    JsonErr("create item failed!\n");
                    return -1;
                }
                if (json_cache_change_key(item, key, head, cache_size) < 0) {
                    JsonErr("add key failed!\n");
                    return -1;
                }
                json_list_add_tail(&item->list, &object->head);
                return 0;
            default:
                JsonErr("not support json type.\n");
                return -1;
        }
    }
    return -1;
}

int json_item_total_get(json_object *json)
{
    int cnt = 0;
    json_object *item = NULL;

    if (!json)
        return 0;
    switch (json->type) {
        case JSON_ARRAY:
        case JSON_OBJECT:
            json_list_for_each_entry(item, &json->head, list) {
                cnt += json_item_total_get(item);
            }
            break;
        default:
            break;
    }
    ++cnt;

    return cnt;
}

void json_del_object(json_object *json)
{
    json_object *pos = NULL, *n = NULL;

    if (!json)
        return;
    if (json->key)
        json_free(json->key);
    switch (json->type) {
        case JSON_STRING:
            if (json->vstr)
                json_free(json->vstr);
            break;
        case JSON_ARRAY:
        case JSON_OBJECT:
            json_list_for_each_entry_safe(pos, n, &json->head, list) {
                json_list_del(&pos->list);
                json_del_object(pos);
            }
            break;
        default:
            break;
    }
    json_free(json);
}

json_object *json_new_object(json_type_t type)
{
    json_object *json = NULL;

    if ((json = json_malloc(sizeof(json_object))) == NULL) {
        JsonErr("malloc failed!\n");
        return NULL;
    }
    memset(json, 0, sizeof(json_object));
    json->type = type;

    switch (json->type) {
        case JSON_ARRAY:
        case JSON_OBJECT:
            INIT_JSON_LIST_HEAD(&json->head);
            break;
        default:
            break;
    }

    return json;
}

json_object *json_create_item(json_type_t type, void *value)
{
    json_object *json = NULL;

    if ((json = json_malloc(sizeof(json_object))) == NULL) {
        JsonErr("malloc failed!\n");
        return NULL;
    }
    memset(json, 0, sizeof(json_object));
    json->type = type;

    switch (type) {
        case JSON_NULL:
            break;
        case JSON_BOOL:
            json->vint = (*(json_bool_t *)value == JSON_FALSE) ? 0 : 1;
            break;
        case JSON_INT:
            json->vint = *(int *)value;
            break;
        case JSON_HEX:
            json->vhex = *(unsigned int *)value;
            break;
        case JSON_DOUBLE:
            json->vdbl = *(double *)value;
            break;
        case JSON_STRING:
        {
            char *str = *(char **)value;
            if (str) {
                if ((json->vstr = json_strdup(str)) == NULL) {
                    JsonErr("malloc failed!\n");
                    json_free(json);
                    return NULL;
                }
            } else {
                json->vstr = NULL;
            }
            break;
        }
        case JSON_ARRAY:
        case JSON_OBJECT:
            INIT_JSON_LIST_HEAD(&json->head);
            break;
        default:
            break;
    }

    return json;
}

json_object *json_create_item_array(json_type_t type, void *values, int count)
{
    int i = 0;
    void *value = NULL;
    json_object *json = NULL, *node = NULL;

    if ((json = json_create_array()) == NULL) {
        JsonErr("create array failed!\n");
        return NULL;
    }

    for (i = 0; i < count; i++) {
        switch (type) {
            case JSON_BOOL:
                value = ((json_bool_t *)values) + i;;
                break;
            case JSON_INT:
                value = ((int *)values) + i;;
                break;
            case JSON_HEX:
                value = ((unsigned int *)values) + i;;
                break;
            case JSON_DOUBLE:
                value = ((double *)values) + i;;
                break;
            case JSON_STRING:
                value = ((char **)values) + i;;
                break;
            default:
                JsonErr("not support json type.\n");
                goto err;
        }

        if ((node = json_create_item(type, value)) == NULL) {
            JsonErr("create item failed!\n");
            goto err;
        }
        json_list_add_tail(&node->list, &json->head);
    }

    return json;

err:
    json_del_object(json);
    return NULL;
}

int json_get_number_value(json_object *json,
        json_bool_t *vbool, int *vint, unsigned int *vhex, double *vdbl)
{
#define _get_number(json, val, etype) do {                          \
    switch (json->type) {                                           \
        case JSON_BOOL:   *val = (typeof(*val))json->vint; break;   \
        case JSON_INT:    *val = (typeof(*val))json->vint; break;   \
        case JSON_HEX:    *val = (typeof(*val))json->vhex; break;   \
        case JSON_DOUBLE: *val = (typeof(*val))json->vdbl; break;   \
        default:          *val = (typeof(*val))0;      return -1;   \
    }                                                               \
    return ((json->type == etype) ? 0 : 1);                         \
} while(0)

    if (vbool) {
        _get_number(json, vbool, JSON_BOOL);
    } else if (vint) {
        _get_number(json, vint, JSON_INT);
    } else if (vhex) {
        _get_number(json, vhex, JSON_HEX);
    } else if (vdbl) {
        _get_number(json, vdbl, JSON_DOUBLE);
    } else {
        return -1;
    }
    return 1;
}

int json_change_number_value(json_object *json,
        json_bool_t *vbool, int *vint, unsigned int *vhex, double *vdbl)
{
#define _CHANGE_NUMBER(json, val, etype) do {                       \
    switch (json->type) {                                           \
        case JSON_BOOL:   json->vint = (val==JSON_FALSE)?0:1;break; \
        case JSON_INT:    json->vint = (int)val;           break;   \
        case JSON_HEX:    json->vhex = (unsigned int)val;  break;   \
        case JSON_DOUBLE: json->vdbl = (double)val;        break;   \
        default:                                       return -1;   \
    }                                                               \
    return ((json->type == etype) ? 0 : 1);                         \
} while(0)

    if (vbool) {
        _CHANGE_NUMBER(json, *vbool, JSON_BOOL);
    } else if (vint) {
        _CHANGE_NUMBER(json, *vint, JSON_INT);
    } else if (vhex) {
        _CHANGE_NUMBER(json, *vhex, JSON_HEX);
    } else if (vdbl) {
        _CHANGE_NUMBER(json, *vdbl, JSON_DOUBLE);
    } else {
        return -1;
    }
    return 1;
}

int json_strict_change_number_value(json_object *json,
        json_bool_t *vbool, int *vint, unsigned int *vhex, double *vdbl)
{
    if (vbool) {
        if (json->type == JSON_BOOL) {
            json->vint = (*vbool == JSON_FALSE) ? 0 : 1;
            return 0;
        }
    } else if (vint) {
        if (json->type == JSON_INT) {
            json->vint = *vint;
            return 0;
        }
    } else if (vhex) {
        if (json->type == JSON_HEX) {
            json->vhex = *vhex;
            return 0;
        }
    } else if (vdbl) {
        if (json->type == JSON_DOUBLE) {
            json->vdbl = *vdbl;
            return 0;
        }
    } else {
        return -1;
    }

    return -1;
}

int json_change_key(json_object *json, const char *key)
{
    if (!key || strlen(key) == 0)
        return -1;
    if (json->key && strcmp(json->key, key) != 0) {
        json_free(json->key);
        json->key = NULL;
    }
    if (!json->key && (json->key = json_strdup(key)) == NULL) {
        JsonErr("malloc failed!\n");
        return -1;
    }

    return 0;
}

int json_change_string_value(json_object *json, const char *str)
{
    if (json->type == JSON_STRING) {
        if (str) {
            if (json->vstr && strcmp(json->vstr, str) != 0) {
                json_free(json->vstr);
                json->vstr = NULL;
            }
            if (!json->vstr && (json->vstr = json_strdup(str)) == NULL) {
                JsonErr("malloc failed!\n");
                return -1;
            }
        } else {
            if (json->vstr) {
                json_free(json->vstr);
                json->vstr = NULL;
            }
        }
        return 0;
    }

    return -1;
}

int json_get_array_size(json_object *json)
{
    int count = 0;
    json_object *pos = NULL;

    if (json->type == JSON_ARRAY) {
        json_list_for_each_entry(pos, &json->head, list) {
            ++count;
        }
    }

    return count;
}

json_object *json_get_array_item(json_object *json, int seq)
{
    int count = 0;
    json_object *pos = NULL;

    if (json->type == JSON_ARRAY) {
        json_list_for_each_entry(pos, &json->head, list) {
            if (count++ == seq)
                return pos;
        }
    }

    return NULL;
}

json_object *json_get_object_item(json_object *json, const char *key)
{
    json_object *pos = NULL;

    if (json->type == JSON_OBJECT) {
        json_list_for_each_entry(pos, &json->head, list) {
            if (strcmp(key, pos->key) == 0)
                return pos;
        }
    }

    return NULL;
}

json_object *json_detach_item_from_array(json_object *json, int seq)
{
    json_object *item = NULL;

    if ((item = json_get_array_item(json, seq)) == NULL)
        return NULL;
    json_list_del(&item->list);

    return item;
}

json_object *json_detach_item_from_object(json_object *json, const char *key)
{
    json_object *item = NULL;

    if ((item = json_get_object_item(json, key)) == NULL)
        return NULL;
    json_list_del(&item->list);

    return item;
}

int json_del_item_from_array(json_object *json, int seq)
{
    json_object *item = NULL;

    if ((item = json_detach_item_from_array(json, seq)) == NULL)
        return -1;
    json_del_object(item);

    return 0;
}

int json_del_item_from_object(json_object *json, const char *key)
{
    json_object *item = NULL;

    if ((item = json_detach_item_from_object(json, key)) == NULL)
        return -1;
    json_del_object(item);

    return 0;
}

int json_replace_item_in_array(json_object *array, int seq, json_object *new_item)
{
    json_object *item = NULL;

    if ((item = json_get_array_item(array, seq)) != NULL) {
        json_list_add_tail(&new_item->list, &item->list);
        json_list_del(&item->list);
        json_del_object(item);
    } else {
        json_list_add_tail(&new_item->list, &array->head);
    }

    return 0;
}

int json_replace_item_in_object(json_object *object, const char *key, json_object *new_item)
{
    json_object *item = NULL;

    if (json_change_key(new_item, key) < 0) {
        return -1;
    }
    if ((item = json_get_object_item(object, key)) != NULL) {
        json_list_add_tail(&new_item->list, &item->list);
        json_list_del(&item->list);
        json_del_object(item);
    } else {
        json_list_add_tail(&new_item->list, &object->head);
    }

    return 0;
}

int json_add_item_to_array(json_object *array, json_object *item)
{
    if (array->type == JSON_ARRAY) {
        json_list_add_tail(&item->list, &array->head);
        return 0;
    }
    return -1;
}

int json_add_item_to_object(json_object *object, const char *key, json_object *item)
{
    if (object->type == JSON_OBJECT) {
        if (json_change_key(item, key) < 0) {
            return -1;
        }
        json_list_add_tail(&item->list, &object->head);
        return 0;
    }
    return -1;
}

json_object *json_deepcopy(json_object *json)
{
    json_object *new_json = NULL;
    json_object *item = NULL, *node = NULL;

    switch (json->type) {
        case JSON_NULL:   new_json = json_create_null(); break;
        case JSON_BOOL:   new_json = json_create_bool((json_bool_t)json->vint); break;
        case JSON_INT:    new_json = json_create_int(json->vint); break;
        case JSON_HEX:    new_json = json_create_hex(json->vhex); break;
        case JSON_DOUBLE: new_json = json_create_double(json->vdbl); break;
        case JSON_STRING: new_json = json_create_string(json->vstr); break;
        case JSON_ARRAY:  new_json = json_create_array(); break;
        case JSON_OBJECT: new_json = json_create_object(); break;
        default: break;
    }

    if (new_json) {
        if (json->key && json_change_key(new_json, json->key) < 0) {
            JsonErr("add key failed!\n");
            json_del_object(new_json);
            return NULL;
        }
        if (json->type == JSON_ARRAY || json->type == JSON_OBJECT) {
            json_list_for_each_entry(item, &json->head, list) {
                if ((node = json_deepcopy(item)) == NULL) {
                    JsonErr("copy failed!\n");
                    json_del_object(new_json);
                    return NULL;
                }
                json_list_add_tail(&node->list, &new_json->head);
            }
        }
    }

    return new_json;
}

int json_copy_item_to_array(json_object *array, json_object *item)
{
    json_object *node = NULL;

    if (array->type == JSON_ARRAY) {
        if ((node = json_deepcopy(item)) == NULL) {
            JsonErr("copy failed!\n");
            return -1;
        }
        if (json_add_item_to_array(array, node) < 0) {
            JsonErr("add failed!\n");
            json_del_object(node);
            return -1;
        }
        return 0;
    }
    return -1;
}

int json_copy_item_to_object(json_object *object, const char *key, json_object *item)
{
    json_object *node = NULL;

    if (object->type == JSON_OBJECT) {
        if ((node = json_deepcopy(item)) == NULL) {
            JsonErr("copy failed!\n");
            return -1;
        }
        if (json_add_item_to_object(object, key, node) < 0) {
            JsonErr("add failed!\n");
            json_del_object(node);
            return -1;
        }
        return 0;
    }
    return -1;
}

int json_add_new_item_to_object(json_object *object, json_type_t type, const char *key, void *value)
{
    json_object *item = NULL;

    if (object->type == JSON_OBJECT) {
        switch (type) {
            case JSON_NULL:
            case JSON_BOOL:
            case JSON_INT:
            case JSON_HEX:
            case JSON_DOUBLE:
            case JSON_STRING:
                if ((item = json_create_item(type, value)) == NULL) {
                    JsonErr("create item failed!\n");
                    return -1;
                }
                if (json_change_key(item, key) < 0) {
                    JsonErr("add key failed!\n");
                    json_del_object(item);
                    return -1;
                }
                json_list_add_tail(&item->list, &object->head);
                return 0;

            default:
                JsonErr("not support json type.\n");
                return -1;
        }
    }

    return -1;
}

typedef struct {
    size_t full_size;
    size_t full_offset;
    size_t full_add_size;
    char *full_ptr;
#if JSON_DIRECT_FILE_SUPPORT
    FILE *fp;
#endif
    size_t temp_size;
    size_t temp_add_size;
    char *temp_ptr;

    size_t item_cell_size;
    int item_total;
    int item_count;
    json_bool_t format_flag;
    json_bool_t calculate_flag;
} json_print_t;

static inline int _print_temp_realloc(json_print_t *print_ptr, size_t len)
{
    if (print_ptr->temp_size < len) {
        while (print_ptr->temp_size < len) {
            print_ptr->temp_size += print_ptr->temp_add_size;
        }
        if ((print_ptr->temp_ptr = json_realloc(print_ptr->temp_ptr, print_ptr->temp_size)) == NULL) {
            JsonErr("malloc failed!\n");
            return -1;
        }
    }

    return 0;
}
#define _PRINT_TEMP_REALLOC(ptr, len) do { if (_print_temp_realloc(ptr, len) < 0) goto err; } while(0)

static int _print_full_strcat(json_print_t *print_ptr, const char *str)
{
    size_t len = 0;
    size_t tmp_len = 0;

    tmp_len = strlen(str);
    len = print_ptr->full_offset + tmp_len + 1;
#if JSON_DIRECT_FILE_SUPPORT
    if (!print_ptr->fp) {
#endif
        if (print_ptr->full_size < len) {
            if (print_ptr->calculate_flag) {
                while(print_ptr->item_total < print_ptr->item_count)
                    print_ptr->item_total += JSON_KEY_TOTAL_NUM_DEF;
                if (print_ptr->item_total - print_ptr->item_count > print_ptr->item_count) {
                    print_ptr->full_size += print_ptr->full_size;
                } else {
                    print_ptr->full_size += (long long)print_ptr->full_size * \
                                            (print_ptr->item_total - print_ptr->item_count) / print_ptr->item_count;
                }
            }
            while (print_ptr->full_size < len) {
                print_ptr->full_size += print_ptr->full_add_size;
            }

            if ((print_ptr->full_ptr = json_realloc(print_ptr->full_ptr, print_ptr->full_size)) == NULL) {
                JsonErr("malloc failed!\n");
                return -1;
            }
        }
#if JSON_DIRECT_FILE_SUPPORT
    } else {
        if (print_ptr->full_size < len) {
            if (print_ptr->full_offset > 0) {
                if (print_ptr->full_offset != fwrite(print_ptr->full_ptr, 1, print_ptr->full_offset, print_ptr->fp)) {
                    JsonErr("fwrite failed!\n");
                    return -1;;
                }
                print_ptr->full_offset = 0;
            }
            if (tmp_len != fwrite(str, 1, tmp_len, print_ptr->fp)) {
                JsonErr("fwrite failed!\n");
                return -1;;
            }
            return 0;
        }
    }
#endif

    memcpy(print_ptr->full_ptr + print_ptr->full_offset, str, tmp_len);
    print_ptr->full_offset += tmp_len;
    return 0;
}
#define _PRINT_FULL_STRCAT(ptr, str) do { if (_print_full_strcat(ptr, str) < 0) goto err; } while(0)

static int _print_full_update(json_print_t *print_ptr)
{
    if (_print_full_strcat(print_ptr, print_ptr->temp_ptr) < 0)
        return -1;
    if (print_ptr->temp_size > print_ptr->temp_add_size) {
        print_ptr->temp_size = print_ptr->temp_add_size;
        if ((print_ptr->temp_ptr = json_realloc(print_ptr->temp_ptr, print_ptr->temp_size)) == NULL)
            return -1;
    }

    return 0;
}
#define _PRINT_FULL_UPDATE(ptr) do { if (_print_full_update(ptr) < 0) goto err; } while(0)

static int _print_addi_format(json_print_t *print_ptr, int depth)
{
    size_t len = 0;
    int cnt = 0;

    if (print_ptr->format_flag && depth > 0) {
        len = depth + 2;
        _PRINT_TEMP_REALLOC(print_ptr, len);

        print_ptr->temp_ptr[cnt++] = '\n';
        while (depth-- > 0)
            print_ptr->temp_ptr[cnt++] = '\t';
        print_ptr->temp_ptr[cnt] = '\0';
        _PRINT_FULL_UPDATE(print_ptr);
    }

    return 0;
err:
    return -1;
}
#define _PRINT_ADDI_FORMAT(ptr, depth) do { if (_print_addi_format(ptr, depth) < 0) goto err; } while(0)

static int _json_print_string(json_print_t *print_ptr, const char *value)
{
#define CHNAGE_UTF16_SUPPORT 0

    size_t len = 0;
    size_t vlen = 0;
    size_t i = 0;
    int cnt = 0;

    if (value) {
        vlen = strlen(value);
        len += vlen;
        for (i = 0; i < vlen; i++) {
            if (strchr("\"\\\b\f\n\r\t\v", value[i])) {
                ++len;
#if CHNAGE_UTF16_SUPPORT
            } else if ((unsigned char)value[i] < ' ') {
                len += 5;
#endif
            }
        }
        len += 3;
        _PRINT_TEMP_REALLOC(print_ptr, len);

        print_ptr->temp_ptr[cnt++] = '\"';
        for (i = 0; i < vlen; i++) {
#if CHNAGE_UTF16_SUPPORT
            if ((unsigned char)value[i] >= ' ' && strchr("\"\\\b\f\n\r\t\v", value[i]) == NULL) {
#else
            if ((strchr("\"\\\b\f\n\r\t\v", value[i])) == NULL) {
#endif
                print_ptr->temp_ptr[cnt++] = value[i];
            } else {
                print_ptr->temp_ptr[cnt++] = '\\';
                switch (value[i]) {
                    case '\"': print_ptr->temp_ptr[cnt++] = '\"'; break;
                    case '\\': print_ptr->temp_ptr[cnt++] = '\\'; break;
                    case '\b': print_ptr->temp_ptr[cnt++] = 'b' ; break;
                    case '\f': print_ptr->temp_ptr[cnt++] = 'f' ; break;
                    case '\n': print_ptr->temp_ptr[cnt++] = 'n' ; break;
                    case '\r': print_ptr->temp_ptr[cnt++] = 'r' ; break;
                    case '\t': print_ptr->temp_ptr[cnt++] = 't' ; break;
                    case '\v': print_ptr->temp_ptr[cnt++] = 'v' ; break;
                    default:
#if CHNAGE_UTF16_SUPPORT
                        sprintf(print_ptr->temp_ptr + cnt,"u%04x", (unsigned char)value[i]);
                        cnt += 5;
#endif
                        break;
                }
            }
        }
        print_ptr->temp_ptr[cnt++] = '\"';
        print_ptr->temp_ptr[cnt] = '\0';
        _PRINT_FULL_UPDATE(print_ptr);
    } else {
        _PRINT_FULL_STRCAT(print_ptr, "\"\"");
    }

    return 0;
err:
    return -1;
}
#define _JSON_PRINT_STRING(ptr, val) do { if (_json_print_string(ptr, val) < 0) goto err; } while(0)

static inline int _json_print_int(json_print_t *print_ptr, int value)
{
    sprintf(print_ptr->temp_ptr, "%d", value);
    _PRINT_FULL_UPDATE(print_ptr);
    return 0;
err:
    return -1;
}
#define _JSON_PRINT_INT(ptr, val) do { if (_json_print_int(ptr, val) < 0) goto err; } while(0)

static inline int _json_print_hex(json_print_t *print_ptr, unsigned int value)
{
    sprintf(print_ptr->temp_ptr, "0x%x", value);
    _PRINT_FULL_UPDATE(print_ptr);
    return 0;
err:
    return -1;
}
#define _JSON_PRINT_HEX(ptr, val) do { if (_json_print_hex(ptr, val) < 0) goto err; } while(0)

static inline int _json_print_double(json_print_t *print_ptr, double value)
{
    if (value - (int)value) {
        sprintf(print_ptr->temp_ptr, "%lf", value);
    } else {
        sprintf(print_ptr->temp_ptr, "%.0lf", value);
    }
    _PRINT_FULL_UPDATE(print_ptr);
    return 0;
err:
    return -1;
}
#define _JSON_PRINT_DOUBLE(ptr, val) do { if (_json_print_double(ptr, val) < 0) goto err; } while(0)

static int _json_print(json_print_t *print_ptr, json_object *json, int depth, json_bool_t print_key)
{
    json_object *item = NULL;
    int new_depth = 0;

    if (print_key) {
        _PRINT_ADDI_FORMAT(print_ptr, depth);
        _JSON_PRINT_STRING(print_ptr, json->key);
        if (print_ptr->format_flag) {
            _PRINT_FULL_STRCAT(print_ptr, ":\t");
        } else {
            _PRINT_FULL_STRCAT(print_ptr, ":");
        }
    }

    switch (json->type) {
        case JSON_NULL:
            _PRINT_FULL_STRCAT(print_ptr, "null");
            break;
        case JSON_BOOL:
            if (json->vint) {
                _PRINT_FULL_STRCAT(print_ptr, "true");
            } else {
                _PRINT_FULL_STRCAT(print_ptr, "false");
            }
            break;
        case JSON_INT:
            _JSON_PRINT_INT(print_ptr, json->vint);
            break;
        case JSON_HEX:
            _JSON_PRINT_HEX(print_ptr, json->vhex);
            break;
        case JSON_DOUBLE:
            _JSON_PRINT_DOUBLE(print_ptr, json->vdbl);
            break;
        case JSON_STRING:
            _JSON_PRINT_STRING(print_ptr, json->vstr);
            break;
        case JSON_ARRAY:
            new_depth = depth + 1;
            _PRINT_FULL_STRCAT(print_ptr, "[");
            json_list_for_each_entry(item, &json->head, list) {
                if (print_ptr->format_flag && item->list.prev == &json->head) {
                    if (item->type == JSON_OBJECT || item->type == JSON_ARRAY)
                        _PRINT_ADDI_FORMAT(print_ptr, new_depth);
                }
                if (_json_print(print_ptr, item, new_depth, JSON_FALSE) < 0) {
                    return -1;
                }
                if (item->list.next != &json->head) {
                    _PRINT_FULL_STRCAT(print_ptr, ",");
                    if (print_ptr->format_flag)
                        _PRINT_FULL_STRCAT(print_ptr, " ");
                }
            }
            if (print_ptr->format_flag && json->head.prev != &json->head) {
                item = (json_object *)json->head.prev;
                if (item->type == JSON_OBJECT || item->type == JSON_ARRAY) {
                    if (depth) {
                        _PRINT_ADDI_FORMAT(print_ptr, depth);
                    } else {
                        _PRINT_FULL_STRCAT(print_ptr, "\n");
                    }
                }
            }
            _PRINT_FULL_STRCAT(print_ptr, "]");
            break;
        case JSON_OBJECT:
            new_depth = depth + 1;
            _PRINT_FULL_STRCAT(print_ptr, "{");
            json_list_for_each_entry(item, &json->head, list) {
                if (_json_print(print_ptr, item, new_depth, JSON_TRUE) < 0) {
                    return -1;
                }
                if (item->list.next != &json->head) {
                    _PRINT_FULL_STRCAT(print_ptr, ",");
                } else {
                    if (print_ptr->format_flag) {
                        if (depth == 0) {
                            _PRINT_FULL_STRCAT(print_ptr, "\n");
                        }
                    }
                }
            }
            if (json->head.next != &json->head)
                _PRINT_ADDI_FORMAT(print_ptr, depth);
            _PRINT_FULL_STRCAT(print_ptr, "}");
            break;
        default:
            break;
    }
    ++print_ptr->item_count;

    return 0;
err:
    return -1;
}

static int _print_val_release(json_print_t *print_ptr, json_bool_t free_all_flag)
{
#define _clear_free_ptr(ptr)    do { if (ptr) json_free(ptr); ptr = NULL; } while(0)
#define _clear_close_fp(fp)     do { if (fp) fclose(fp); fp = NULL; } while(0)
    int ret = 0;
#if JSON_DIRECT_FILE_SUPPORT
    if (!print_ptr->fp) {
#endif
        if (!free_all_flag) {
            print_ptr->full_ptr = json_realloc(print_ptr->full_ptr, print_ptr->full_offset+1);
            print_ptr->full_ptr[print_ptr->full_offset] = '\0';
        } else {
            _clear_free_ptr(print_ptr->full_ptr);
        }
        _clear_free_ptr(print_ptr->temp_ptr);

#if JSON_DIRECT_FILE_SUPPORT
    } else {
        if (!free_all_flag && print_ptr->full_offset > 0) {
            if (print_ptr->full_offset != fwrite(print_ptr->full_ptr, 1, print_ptr->full_offset, print_ptr->fp)) {
                JsonErr("fwrite failed!\n");
                ret = -1;
            }
            _clear_close_fp(print_ptr->fp);
            _clear_free_ptr(print_ptr->full_ptr);
            _clear_free_ptr(print_ptr->temp_ptr);
        }
    }
#endif

    return ret;
}

static int _print_val_init(json_print_t *print_ptr, json_print_choice_t *choice)
{
    size_t item_cell_size_def = 0;
    int item_total_size = 0;

    print_ptr->format_flag = choice->format_flag;
    print_ptr->calculate_flag = choice->calculate_flag;
    if (choice->full_add_size < JSON_FULL_ADD_SIZE_DEF) {
        print_ptr->full_add_size = JSON_FULL_ADD_SIZE_DEF;
    } else {
        print_ptr->full_add_size = choice->full_add_size;
    }
    if (choice->temp_add_size < JSON_TEMP_ADD_SIZE_DEF) {
        print_ptr->temp_add_size = JSON_TEMP_ADD_SIZE_DEF;
    } else {
        print_ptr->temp_add_size = choice->temp_add_size;
    }
    item_cell_size_def = (choice->format_flag) ? JSON_ITEM_FORMAT_CELL_SIZE_DEF : JSON_ITEM_UNFORMAT_CELL_SIZE_DEF;
    if (choice->item_cell_size < item_cell_size_def) {
        print_ptr->item_cell_size = item_cell_size_def;
    } else {
        print_ptr->item_cell_size = choice->item_cell_size;
    }

#if JSON_DIRECT_FILE_SUPPORT
    if (!choice->path) {
#endif
        if (print_ptr->calculate_flag) {
            item_total_size = choice->item_total * print_ptr->item_cell_size;
            if (item_total_size < JSON_FULL_ADD_SIZE_DEF) {
                item_total_size = JSON_FULL_ADD_SIZE_DEF;
            }
            print_ptr->full_size = item_total_size;
            print_ptr->item_total = choice->item_total;
        } else {
            print_ptr->full_size = print_ptr->full_add_size;
        }
#if JSON_DIRECT_FILE_SUPPORT
    } else {
        if ((print_ptr->fp = fopen(choice->path, "w+")) == NULL) {
            JsonErr("fopen(%s) failed!\n", choice->path);
            goto err;
        }
        print_ptr->full_size = print_ptr->full_add_size;
    }
#endif
    if ((print_ptr->full_ptr = json_malloc(print_ptr->full_size)) == NULL) {
        JsonErr("malloc failed!\n");
        goto err;
    }

    print_ptr->temp_size = print_ptr->temp_add_size;
    if ((print_ptr->temp_ptr = json_malloc(print_ptr->temp_size)) == NULL) {
        JsonErr("malloc failed!\n");
        goto err;
    }

    return 0;
err:
    _print_val_release(print_ptr, JSON_TRUE);
    return -1;
}

char *json_print_common(json_object *json, json_print_choice_t *choice)
{
    json_print_t print_val = {0};

    if (!json)
        return NULL;

    if (choice->calculate_flag)
        choice->item_total = json_item_total_get(json);

    if (_print_val_init(&print_val, choice) < 0)
        return NULL;

    if (_json_print(&print_val, json, 0, JSON_FALSE) < 0) {
        JsonErr("print failed!\n");
        goto err;
    }
    if (_print_val_release(&print_val, JSON_FALSE) < 0)
        return NULL;

#if JSON_DIRECT_FILE_SUPPORT
    if (!choice->path) {
#endif
        return print_val.full_ptr;
#if JSON_DIRECT_FILE_SUPPORT
    } else {
        return "ok";
    }
#endif
err:
    _print_val_release(&print_val, JSON_TRUE);
    return NULL;
}

void json_free_print_ptr(void *ptr)
{
    if (ptr) json_free(ptr);
}

#if JSON_SAX_APIS_SUPPORT
typedef struct {
    unsigned short type;
    unsigned short num;
} json_sax_print_depth_t;

typedef struct {
    int total;
    int count;
    json_sax_print_depth_t *array;
    json_print_t print_val;
    json_type_t last_type;
    json_bool_t error_flag;
} json_sax_print_t;

int json_sax_print_value(json_sax_print_handle handle, json_type_t type, const char *key, const void *value)
{
    json_sax_print_t *print_handle = handle;
    json_print_t *print_ptr = &print_handle->print_val;
    int cur_pos = print_handle->count-1;

    if (print_handle->error_flag) {
        return -1;
    }

    if (print_handle->count > 0) {
        if (!((type == JSON_ARRAY || type == JSON_OBJECT) && (*(json_sax_cmd_t*)value) == JSON_SAX_FINISH)) {
            // add ","
            if (print_handle->array[cur_pos].num > 0) {
                if (print_ptr->format_flag && print_handle->array[cur_pos].type == JSON_ARRAY) {
                    _PRINT_FULL_STRCAT(print_ptr, ", ");
                } else {
                    _PRINT_FULL_STRCAT(print_ptr, ",");
                }
            } else {
                if ((type == JSON_OBJECT || type == JSON_ARRAY) && print_handle->array[cur_pos].type == JSON_ARRAY) {
                    _PRINT_ADDI_FORMAT(print_ptr, print_handle->count);
                }
            }

            // add key
            if (print_handle->array[cur_pos].type == JSON_OBJECT) {
                if (!key || strlen(key) == 0) {
                    JsonErr("key is null!\n");
                    goto err;
                }
                _PRINT_ADDI_FORMAT(print_ptr, print_handle->count);
                _JSON_PRINT_STRING(print_ptr, key);
                if (print_ptr->format_flag) {
                    _PRINT_FULL_STRCAT(print_ptr, ":\t");
                } else {
                    _PRINT_FULL_STRCAT(print_ptr, ":");
                }
            }
        }
    }

    // add value
    switch (type) {
        case JSON_NULL:
            _PRINT_FULL_STRCAT(print_ptr, "null");
            break;
        case JSON_BOOL:
            if ((*(json_bool_t*)value)) {
                _PRINT_FULL_STRCAT(print_ptr, "true");
            } else {
                _PRINT_FULL_STRCAT(print_ptr, "false");
            }
            break;
        case JSON_INT:
            _JSON_PRINT_INT(print_ptr, (*(int*)value));
            break;
        case JSON_HEX:
            _JSON_PRINT_HEX(print_ptr, (*(unsigned int*)value));
            break;
        case JSON_DOUBLE:
            _JSON_PRINT_DOUBLE(print_ptr, (*(double*)value));
            break;
        case JSON_STRING:
            _JSON_PRINT_STRING(print_ptr, ((char*)value));
            break;

        case JSON_ARRAY:
            switch ((*(json_sax_cmd_t*)value)) {
                case JSON_SAX_START:
                    if (print_handle->count == print_handle->total) {
                        print_handle->total += JSON_KEY_TOTAL_NUM_DEF;
                        if ((print_handle->array = json_realloc(print_handle->array,
                                print_handle->total * sizeof(json_sax_print_depth_t))) == NULL) {
                            JsonErr("malloc failed!\n");
                            goto err;
                        }
                    }
                    if (print_handle->count > 0)
                        ++print_handle->array[cur_pos].num;
                    _PRINT_FULL_STRCAT(print_ptr, "[");
                    print_handle->array[print_handle->count].type = JSON_ARRAY;
                    print_handle->array[print_handle->count].num = 0;
                    ++print_handle->count;
                    break;

                case JSON_SAX_FINISH:
                    if (print_handle->count == 0 || print_handle->array[cur_pos].type != JSON_ARRAY) {
                        JsonErr("unexpected array finish!\n");
                        goto err;
                    }
                    if (print_ptr->format_flag && print_handle->array[cur_pos].num > 0
                            && (print_handle->last_type == JSON_OBJECT || print_handle->last_type == JSON_ARRAY)) {
                        if (print_handle->count > 1) {
                            _PRINT_ADDI_FORMAT(print_ptr, cur_pos);
                        } else {
                            _PRINT_FULL_STRCAT(print_ptr, "\n");
                        }
                    }
                    _PRINT_FULL_STRCAT(print_ptr, "]");
                    --print_handle->count;
                    print_handle->last_type = type;
                    return 0;

                default:
                    goto err;
            }

            break;

        case JSON_OBJECT:
            switch ((*(json_sax_cmd_t*)value)) {
                case JSON_SAX_START:
                    if (print_handle->count == print_handle->total) {
                        print_handle->total += JSON_KEY_TOTAL_NUM_DEF;
                        if ((print_handle->array = json_realloc(print_handle->array,
                                print_handle->total * sizeof(json_sax_print_depth_t))) == NULL) {
                            JsonErr("malloc failed!\n");
                            goto err;
                        }
                    }
                    if (print_handle->count > 0)
                        ++print_handle->array[cur_pos].num;
                    _PRINT_FULL_STRCAT(print_ptr, "{");
                    print_handle->array[print_handle->count].type = JSON_OBJECT;
                    print_handle->array[print_handle->count].num = 0;
                    ++print_handle->count;
                    break;

                case JSON_SAX_FINISH:
                    if (print_handle->count == 0 || print_handle->array[cur_pos].type != JSON_OBJECT) {
                        JsonErr("unexpected object finish!\n");
                        goto err;
                    }
                    if (print_handle->count > 1) {
                        if (print_handle->array[print_handle->count-1].num > 0) {
                            _PRINT_ADDI_FORMAT(print_ptr, cur_pos);
                        }
                    } else {
                        if (print_ptr->format_flag)
                            _PRINT_FULL_STRCAT(print_ptr, "\n");
                    }
                    _PRINT_FULL_STRCAT(print_ptr, "}");
                    --print_handle->count;
                    print_handle->last_type = type;
                    return 0;

                default:
                    goto err;
            }
            break;

        default:
            goto err;
    }

    print_handle->last_type = type;
    if (cur_pos >= 0)
        ++print_handle->array[cur_pos].num;
    ++print_ptr->item_count;

    return 0;
err:
    print_handle->error_flag = JSON_TRUE;
    return -1;
}

json_sax_print_handle json_sax_print_start(json_print_choice_t *choice)
{
    json_sax_print_t *print_handle = NULL;

    if ((print_handle = json_calloc(1, sizeof(json_sax_print_t))) == NULL) {
        JsonErr("malloc failed!\n");
        return NULL;
    }
    if (choice->item_total == 0)
        choice->item_total = JSON_KEY_TOTAL_NUM_DEF;
    if (_print_val_init(&print_handle->print_val, choice) < 0) {
        json_free(print_handle);
        return NULL;
    }

    print_handle->total = JSON_KEY_TOTAL_NUM_DEF;
    if ((print_handle->array = json_malloc(print_handle->total * sizeof(json_sax_print_depth_t))) == NULL) {
        _print_val_release(&print_handle->print_val, JSON_TRUE);
        json_free(print_handle);
        JsonErr("malloc failed!\n");
        return NULL;
    }

    return print_handle;
}

char *json_sax_print_finish(json_sax_print_handle handle)
{
    char *ret = NULL;

    json_sax_print_t *print_handle = handle;
    if (!print_handle)
        return NULL;
    if (print_handle->array)
        json_free(print_handle->array);
    if (print_handle->error_flag) {
        _print_val_release(&print_handle->print_val, JSON_TRUE);
        json_free(print_handle);
        return NULL;
    }

#if JSON_DIRECT_FILE_SUPPORT
    if (print_handle->print_val.fp) {
#endif
        ret = "ok";
#if JSON_DIRECT_FILE_SUPPORT
    } else {
        ret = print_handle->print_val.full_ptr;
    }
#endif

    if (_print_val_release(&print_handle->print_val, JSON_FALSE) < 0) {
        json_free(print_handle);
        return NULL;
    }
    json_free(print_handle);

    return ret;
}
#endif

typedef struct {
    size_t offset;
    const char *str;
#if JSON_DIRECT_FILE_SUPPORT
    FILE *fp;
    size_t file_size;
    size_t read_size;
    size_t read_offset;
    size_t read_count;
    char *read_ptr;
#endif
    // block memory, if head is not null, use it. sax parser not use it.
    json_bool_t repeat_key_check;
    char **key_array;
    size_t key_total;
    size_t key_count;
    size_t mem_size;
    struct json_list_head *head;
#if JSON_SAX_APIS_SUPPORT
    json_sax_parser parser;
    json_sax_parser_callback callback;
    json_sax_parse_ret parser_ret;
#endif
} json_parse_t;

static inline void *_parse_alloc(json_parse_t *parse_ptr, size_t size, int id, block_mem_node_t **ret_node)
{
    if (parse_ptr->head) {
        return _block_mem_get(size, id, parse_ptr->mem_size, parse_ptr->head, ret_node);
    } else {
        return json_malloc(size);
    }
}

#if JSON_DIRECT_FILE_SUPPORT
static inline int _realloc_read_ptr(json_parse_t *parse_ptr, int new_size)
{
    parse_ptr->read_size = new_size;
    if ((parse_ptr->read_ptr = json_realloc(parse_ptr->read_ptr, parse_ptr->read_size)) == NULL) {
        JsonErr("malloc failed!\n");
        return -1;
    }
    parse_ptr->read_offset = 0;
    parse_ptr->read_count = 0;
    return 0;
}
#endif

static inline int _get_parse_ptr(json_parse_t *parse_ptr, int add_offset, size_t read_size, char **sstr)
{
#if JSON_DIRECT_FILE_SUPPORT
    if (parse_ptr->str) {
#endif
        *sstr = (char*)(parse_ptr->str + parse_ptr->offset + add_offset);
        return read_size;
#if JSON_DIRECT_FILE_SUPPORT
    } else {
        size_t size = 0;
        size_t offset = 0;

        if (read_size == 0)
            read_size = JSON_PARSE_READ_SIZE_DEF;
        offset = parse_ptr->offset + add_offset;

        if ((offset >= parse_ptr->read_offset)
                && (offset + read_size <= parse_ptr->read_offset + parse_ptr->read_count)) {
            *sstr = parse_ptr->read_ptr + offset - parse_ptr->read_offset;
            size = parse_ptr->read_offset + parse_ptr->read_count - offset;
        } else {
            if (read_size > parse_ptr->read_size-1) {
                *sstr = "Z"; //Reduce the judgment pointer is NULL.
                if (_realloc_read_ptr(parse_ptr, read_size) < 0)
                    return 0;
            }
            fseek(parse_ptr->fp, offset, SEEK_SET);
            size = fread(parse_ptr->read_ptr, 1, parse_ptr->read_size-1, parse_ptr->fp);
            parse_ptr->read_ptr[size] = 0;
            parse_ptr->read_offset = offset;
            parse_ptr->read_count = size;
            *sstr = parse_ptr->read_ptr;
        }
        return size;
    }
#endif
}

static inline void _update_parse_offset(json_parse_t *parse_ptr, int num)
{
    parse_ptr->offset += num;
}

static inline void _skip_blank(json_parse_t *parse_ptr)
{
    char *str = NULL, *bak = NULL;

#if JSON_DIRECT_FILE_SUPPORT
    if (parse_ptr->str) {
#endif
        _get_parse_ptr(parse_ptr, 0, 0, &str);
        bak = str;
        while (*str && (unsigned char)(*str) <= ' ')
            str++;
        _update_parse_offset(parse_ptr, str-bak);
#if JSON_DIRECT_FILE_SUPPORT
    } else {
        int cnt = 0;
        while (_get_parse_ptr(parse_ptr, cnt, 32, &str) != 0) {
            while (*str) {
                if ((unsigned char)(*str) <= ' ') {
                    str++, ++cnt;
                } else {
                    goto end;
                }
            }
        }
end:
        _update_parse_offset(parse_ptr, cnt);
    }
#endif
}

static int _num_parse_unit(char **sstr, int *vint, unsigned int *vhex, double *vdbl, json_bool_t check_hex)
{
#define _HEX_CHECK(x)  ((x >= '0' && x <= '9') || (x >= 'a' && x <= 'f') || (x >= 'A' && x <= 'F'))
#define _HEX_CAL(y,x)  do {                             \
    if      (x >= '0' && x <= '9') y = x - '0';         \
    else if (x >= 'a' && x <= 'f') y = 10 + x - 'a';    \
    else if (x >= 'A' && x <= 'F') y = 10 + x - 'A';    \
    else                           y = 0;               \
} while (0)

    char *num = *sstr;
    if (*num == '0' && (*(num+1) == 'x' || *(num+1) == 'X') && _HEX_CHECK(*(num+2))) {
        unsigned int m = 0, n = 0;
        if (!check_hex)
            return -1;
        num += 2;
        do {
            _HEX_CAL(m, *num); n = (n << 4) + m; num++;
        } while (_HEX_CHECK(*num));

        *vhex = n;
        *sstr = num;
        return JSON_HEX;
    } else {
        double m = 0, n = 0;
        int sign = 1, scale = 0;

        if (*num == '-') num++, sign = -1;
        else if (*num == '+') num++;
        while (*num == '0') num++;

        if (*num >= '1' && *num <= '9') {
            do {
                m = (m * 10) + (*num++ - '0');
            } while (*num >= '0' && *num <= '9');
        }

        if (*num=='.' && *(num+1) >= '0' && *(num+1) <= '9') {
            num++;
            do {
                m = (m * 10) + (*num++ - '0');
                scale--;
            } while (*num >= '0' && *num <= '9');
            n = sign * m * pow (10.0, scale);

            *vdbl = n;
            *sstr = num;
            return JSON_DOUBLE;
        } else {
            n = sign * m;
            *sstr = num;
            if ((int)n == n) {
                *vint = (int)n;
                return JSON_INT;
            } else {
                *vdbl = n;
                return JSON_DOUBLE;
            }
        }
    }
    return -1;
}

static int _num_parse(char **sstr, int *vint, unsigned int *vhex, double *vdbl)
{
    int ret = -1;

    ret = _num_parse_unit(sstr, vint, vhex, vdbl, JSON_TRUE);
    switch (ret) {
        case JSON_HEX:
            return ret;
        case JSON_INT:
        case JSON_DOUBLE:
        {
            char val = **sstr;
            if (val != 'e' && val != 'E') {
                return ret;
            } else {
                int tret = 0;
                int tint = 0;
                unsigned int thex = 0;
                double tdbl = 0;
                double nbase = 0, nindex = 0;

                *sstr += 1;
                nbase = (ret == JSON_INT) ? (*vint) : (*vdbl);
                tret = _num_parse_unit(sstr, &tint, &thex, &tdbl, JSON_FALSE);
                switch (tret) {
                    case JSON_INT:
                    case JSON_DOUBLE:
                        nindex = (tret == JSON_INT) ? (tint) : (tdbl);
                        *vdbl = nbase * pow (10.0, nindex);
                        return JSON_DOUBLE;
                    default:
                        return -1;
                }
            }
        }
        default:
            return -1;
    }
}

static inline unsigned int _parse_hex4(const unsigned char *str)
{
    int i = 0;
    unsigned int val = 0;

    for (i = 0; i < 4; i++) {
        if ((str[i] >= '0') && (str[i] <= '9')) {
            val += str[i] - '0';
        } else if ((str[i] >= 'A') && (str[i] <= 'F')) {
            val += 10 + str[i] - 'A';
        } else if ((str[i] >= 'a') && (str[i] <= 'f')) {
            val += 10 + str[i] - 'a';
        } else {
            return 0;
        }
        if (i < 3) { /* shift left to make place for the next nibble */
            val = val << 4;
        }
    }

    return val;
}

static int utf16_literal_to_utf8(const unsigned char *start_str, const unsigned char *finish_str, unsigned char **pptr)
{ /* copy from cJSON */
    const unsigned char *str = start_str;
    unsigned char *ptr = *pptr;

    int seq_len = 0;
    int i = 0;
    unsigned long uc = 0;
    unsigned int  uc1 = 0, uc2 = 0;
    unsigned char len = 0;
    unsigned char first_byte_mark = 0;

    /* converts a UTF-16 literal to UTF-8, A literal can be one or two sequences of the form \uXXXX */
    if ((finish_str - str) < 6)                             /* input ends unexpectedly */
        goto fail;
    str += 2;
    uc1 = _parse_hex4(str);                                 /* get the first utf16 sequence */
    if (((uc1 >= 0xDC00) && (uc1 <= 0xDFFF)))               /* check first_code is valid */
        goto fail;
    if ((uc1 >= 0xD800) && (uc1 <= 0xDBFF)) {               /* UTF16 surrogate pair */
        str += 4;
        seq_len = 12;                                       /* \uXXXX\uXXXX */
        if ((finish_str - str) < 6)                         /* input ends unexpectedly */
            goto fail;
        if ((str[0] != '\\') || (str[1] != 'u'))            /* missing second half of the surrogate pair */
            goto fail;
        str += 2;
        uc2 = _parse_hex4(str);                             /* get the second utf16 sequence */
        if ((uc2 < 0xDC00) || (uc2 > 0xDFFF))               /* check second_code is valid */
            goto fail;
        uc = 0x10000 + (((uc1 & 0x3FF) << 10) | (uc2 & 0x3FF)); /* calculate the unicode uc from the surrogate pair */
    } else {
        seq_len = 6;                                        /* \uXXXX */
        uc = uc1;
    }

    /* encode as UTF-8 takes at maximum 4 bytes to encode: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
    if (uc < 0x80) {                                        /* normal ascii, encoding 0xxxxxxx */
        len = 1;
    } else if (uc < 0x800) {                                /* two bytes, encoding 110xxxxx 10xxxxxx */
        len = 2;
        first_byte_mark = 0xC0;                             /* 11000000 */
    } else if (uc < 0x10000) {                              /* three bytes, encoding 1110xxxx 10xxxxxx 10xxxxxx */
        len = 3;
        first_byte_mark = 0xE0;                             /* 11100000 */
    } else if (uc <= 0x10FFFF) {                            /* four bytes, encoding 1110xxxx 10xxxxxx 10xxxxxx 10xxxxxx */
        len = 4;
        first_byte_mark = 0xF0;                             /* 11110000 */
    }  else {                                               /* invalid unicode */
        goto fail;
    }
    for (i = len - 1; i > 0; i--) {                         /* encode as utf8 */
        ptr[i] = (unsigned char)((uc | 0x80) & 0xBF);       /* 10xxxxxx */
        uc >>= 6;
    }
    if (len > 1) {                                          /* encode first byte */
        ptr[0] = (unsigned char)((uc | first_byte_mark) & 0xFF);
    } else {
        ptr[0] = (unsigned char)(uc & 0x7F);
    }

    *pptr += len;
    return seq_len;

fail:
    return 0;
}

static char *_str_parse_transform(char *str, char *end_str, char *ptr)
{
    int seq_len = 0;

    while (str < end_str) {
        if (*str != '\\'){
            *ptr++ = *str++;
        } else {
            seq_len = 2;
            switch (str[1]) {
                case 'b':  *ptr++ = '\b'; break;
                case 'f':  *ptr++ = '\f'; break;
                case 'n':  *ptr++ = '\n'; break;
                case 'r':  *ptr++ = '\r'; break;
                case 't':  *ptr++ = '\t'; break;
                case 'v':  *ptr++ = '\v'; break;
                case '\"': *ptr++ = '\"'; break;
                case '\\': *ptr++ = '\\'; break;
                case '/':  *ptr++ = '/' ; break;
                case 'u':
                    if ((seq_len = utf16_literal_to_utf8((unsigned char*)str,
                                   (unsigned char*)end_str, (unsigned char**)&ptr)) == 0) {
                        JsonErr("invalid utf16 code!\n");
                        return NULL;
                    }
                    break;
                default :
                    return NULL;
            }
            str += seq_len;
        }
    }
    *ptr = 0;

    return ptr;
}

static int _str_parse_len_get(json_parse_t *parse_ptr, int *out_len, int *total_len)
{
    int len = 0;
    char *str = NULL, *bak = NULL;

#if JSON_DIRECT_FILE_SUPPORT
    if (parse_ptr->str) {
#endif
        _get_parse_ptr(parse_ptr, 0, 0, &str);
        if (*str != '\"')
            return -1;
        bak = str;

        str++;
        while (*str && *str != '\"') {
            if (*str++ == '\\')
                str++;
             ++len;
        }
        if (*str != '\"')
            return -1;
        str++;

        *out_len = len;
        *total_len = str - bak;
#if JSON_DIRECT_FILE_SUPPORT
    } else {
        int size = 0;
        int total = 0;
        int cnt = 0;

        _get_parse_ptr(parse_ptr, 0, 0, &str);
        if (*str != '\"')
            return -1;

        ++total;
        while ((size = _get_parse_ptr(parse_ptr, total, 0, &str)) != 0) {
            cnt = 0;
            while(*str && *str != '\"') {
                if (*str != '\\') {
                    str++, ++cnt, ++len;
                } else {
                    if (size-cnt >= 2) {
                        str += 2, cnt += 2, ++len;
                    } else {
                        if (parse_ptr->read_offset + parse_ptr->read_count < parse_ptr->file_size) {
                            break;
                        } else {
                            return -1;
                        }
                    }
                }
            }
            total += cnt;
            if (*str == '\"')
                break;
        }
        if (*str != '\"')
            return -1;
        ++total;

        *out_len = len;
        *total_len = total;
    }
#endif

    return 0;
}

static int _str_parse(json_parse_t *parse_ptr, char **pptr, json_bool_t key_flag)
{
    size_t i = 0;
    int len = 0;
    int total = 0;
    char *ptr = NULL, *ptr_bak = NULL;
    char *str = NULL, *end_str = NULL;
    block_mem_node_t *ret_node = NULL;
#if JSON_DIRECT_FILE_SUPPORT
    int use_size_bak = 0;
#endif

    *pptr = NULL;
    if (_str_parse_len_get(parse_ptr, &len, &total) < 0) {
        JsonErr("str format err!\n");
        return -1;
    }

#if JSON_DIRECT_FILE_SUPPORT
    if (parse_ptr->str) {
#endif
        _get_parse_ptr(parse_ptr, 0, 0, &str);
#if JSON_DIRECT_FILE_SUPPORT
    } else {
        use_size_bak = parse_ptr->read_size;
        if (_get_parse_ptr(parse_ptr, 0, total, &str) < total) {
            JsonErr("reread failed!\n");
            return -1;
        }
    }
#endif
    end_str = str + total - 1;
    str++;

    if ((ptr = _parse_alloc(parse_ptr, len+1, STRING_MEM_ID, &ret_node)) == NULL) {
        JsonErr("malloc failed!\n");
        return -1;
    }
    ptr_bak = ptr;
    if ((ptr = _str_parse_transform(str, end_str, ptr)) == NULL) {
        JsonErr("transform failed!\n");
        goto err;
    }

    _update_parse_offset(parse_ptr, total);
    if (key_flag) {
        if (ptr == ptr_bak) {
            goto err;
        }
        _skip_blank(parse_ptr);
        _get_parse_ptr(parse_ptr, 0, 1, &str);
        if (*str != ':') {
            goto err;
        }
        _update_parse_offset(parse_ptr, 1);

        if (parse_ptr->repeat_key_check) {
            for (i = 0; i < parse_ptr->key_count; i++) {
                if (strcmp(ptr_bak, parse_ptr->key_array[i]) == 0) {
                    *pptr = parse_ptr->key_array[i];
                    ret_node->cur = ptr_bak;
                    goto end;
                }
            }
            if (parse_ptr->key_count == parse_ptr->key_total) {
                parse_ptr->key_total += JSON_KEY_TOTAL_NUM_DEF;
                if ((parse_ptr->key_array = json_realloc(parse_ptr->key_array,
                                parse_ptr->key_total * sizeof(char*))) == NULL) {
                    JsonErr("malloc failed!\n");
                    goto err;
                }
            }
            parse_ptr->key_array[parse_ptr->key_count++] = ptr_bak;
        }
        *pptr = ptr_bak;
    } else {
        *pptr = ptr_bak;
    }

end:
#if JSON_DIRECT_FILE_SUPPORT
    if (!parse_ptr->str) {
        if (total > use_size_bak-1)
            _realloc_read_ptr(parse_ptr, use_size_bak);
    }
#endif
    return 0;

err:
    if (ptr_bak) {
        if (!parse_ptr->head) json_free(ptr_bak);
    }
    return -1;
}

static int _json_parse_value(json_parse_t *parse_ptr, json_object **item, json_object *parent, char *key)
{
    json_object *out_item = NULL;
    json_object *new_item = NULL;
    char *key_str = NULL;
    char *str = NULL;
    int nret = 0;
    int vint= 0;
    unsigned int vhex = 0;
    double vdbl = 0;
    char *vstr = NULL;

    if ((out_item = _parse_alloc(parse_ptr, sizeof(json_object), OBJECT_MEM_ID, NULL)) == NULL) {
        JsonErr("malloc failed!\n");
        goto err;
    }
    out_item->key = NULL;

    _skip_blank(parse_ptr);
    _get_parse_ptr(parse_ptr, 0, 8, &str);

    switch(*str) {
        case '\"':
        {
            out_item->type = JSON_STRING;
            if (_str_parse(parse_ptr, &vstr, JSON_FALSE) < 0) {
                if (!parse_ptr->head) json_free(out_item);
                goto err;
            }
            out_item->vstr = vstr;
            break;
        }

        case '-': case '+':
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        {
            char *str_bak = NULL;
            _get_parse_ptr(parse_ptr, 0, 128, &str);
            str_bak = str;
            nret = _num_parse(&str, &vint, &vhex, &vdbl);
            out_item->type = nret;
            switch (nret) {
                case JSON_INT:    out_item->vint = vint; break;
                case JSON_HEX:    out_item->vhex = vhex; break;
                case JSON_DOUBLE: out_item->vdbl = vdbl; break;
                default: if (!parse_ptr->head) json_free(out_item); goto err;
            }
            _update_parse_offset(parse_ptr, str-str_bak);
            break;
        }

        case '{':
        {
            out_item->type = JSON_OBJECT;
            INIT_JSON_LIST_HEAD(&out_item->head);
            _update_parse_offset(parse_ptr, 1);

            _skip_blank(parse_ptr);
            _get_parse_ptr(parse_ptr, 0, 1, &str);
            if (*str != '}') {
                while (1) {
                    _skip_blank(parse_ptr);
                    if (_str_parse(parse_ptr, &key_str, JSON_TRUE) < 0) {
                        if (!parse_ptr->head) json_del_object(out_item);
                        goto err;
                    }
                    if (_json_parse_value(parse_ptr, &new_item, out_item, key_str) < 0) {
                        if (!parse_ptr->head) json_del_object(out_item);
                        goto err;
                    }
                    _skip_blank(parse_ptr);
                    _get_parse_ptr(parse_ptr, 0, 1, &str);
                    switch(*str) {
                        case '}':
                            _update_parse_offset(parse_ptr, 1);
                            goto end;
                        case ',':
                            _update_parse_offset(parse_ptr, 1);
                            break;
                        default:
                            if (!parse_ptr->head) json_del_object(out_item);
                            JsonErr("invalid object!\n");
                            goto err;
                    }

                }
            } else {
                _update_parse_offset(parse_ptr, 1);
            }
            break;
        }

        case '[':
        {
            out_item->type = JSON_ARRAY;
            INIT_JSON_LIST_HEAD(&out_item->head);
            _update_parse_offset(parse_ptr, 1);

            _skip_blank(parse_ptr);
            _get_parse_ptr(parse_ptr, 0, 1, &str);
            if (*str != ']') {
                while (1) {
                    if (_json_parse_value(parse_ptr, &new_item, out_item, NULL) < 0) {
                        if (!parse_ptr->head) json_del_object(out_item);
                        goto err;
                    }
                    _skip_blank(parse_ptr);
                    _get_parse_ptr(parse_ptr, 0, 1, &str);
                    switch(*str) {
                        case ']':
                            _update_parse_offset(parse_ptr, 1);
                            goto end;
                        case ',':
                            _update_parse_offset(parse_ptr, 1);
                            break;
                        default:
                            if (!parse_ptr->head) json_del_object(out_item);
                            JsonErr("invalid object!\n");
                            goto err;
                    }
                }
            } else {
                _update_parse_offset(parse_ptr, 1);
            }
            break;
        }

        default:
        {
            if (strncmp(str, "false", 5) == 0) {
                out_item->type = JSON_BOOL;
                out_item->vint = 0;
                _update_parse_offset(parse_ptr, 5);
            } else if (strncmp(str, "true", 4) == 0) {
                out_item->type = JSON_BOOL;
                out_item->vint = 1;
                _update_parse_offset(parse_ptr, 4);
            } else if (strncmp(str, "null", 4) == 0) {
                out_item->type = JSON_NULL;
                _update_parse_offset(parse_ptr, 4);
            } else {
                if (!parse_ptr->head) json_free(out_item);
                JsonErr("invalid next ptr!\n");
                goto err;
            }
            break;
        }
    }

end:
    if (parent) {
        out_item->key = key; // parent object is array, key is NULL.
        json_list_add_tail(&out_item->list, &parent->head);
    }

    *item = out_item;
    return 0;
err:
    if (!parse_ptr->head) json_free(key);
    return -1;
}

json_object *json_parse_common(json_parse_choice_t *choice)
{
    json_object *json = NULL;
    json_parse_t parse_val = {0};
    size_t str_len = 0;

    parse_val.head = choice->head;
#if JSON_DIRECT_FILE_SUPPORT
    if (choice->str) {
#endif
        parse_val.str = choice->str;
        if (parse_val.head && choice->str_len == 0) {
            str_len = strlen(choice->str);
        } else {
            str_len = choice->str_len;
        }
#if JSON_DIRECT_FILE_SUPPORT
    } else {
        if ((parse_val.fp = fopen(choice->path, "r")) == NULL) {
            JsonErr("fopen(%s) failed!\n", choice->path);
            return NULL;
        }

        if (choice->read_size < JSON_PARSE_USE_SIZE_DEF) {
            parse_val.read_size = JSON_PARSE_USE_SIZE_DEF;
        } else {
            parse_val.read_size = choice->read_size;
        }
        if ((parse_val.read_ptr = json_malloc(parse_val.read_size)) == NULL) {
            JsonErr("malloc failed!\n");
            goto end;
        }

        fseek(parse_val.fp, 0, SEEK_END);
        str_len = ftell(parse_val.fp);
        fseek(parse_val.fp, 0, SEEK_SET);
        parse_val.file_size = str_len;
    }
#endif

    if (parse_val.head) {
        size_t mem_size = 0;
        mem_size = str_len / JSON_STR_MULTIPLE_NUM;
        if (mem_size < choice->mem_size)
            mem_size = choice->mem_size;
        parse_val.mem_size = mem_size - mem_size % JSON_PAGE_ALIGEN_BYTES + JSON_PAGE_ALIGEN_BYTES;
        parse_val.repeat_key_check = choice->repeat_key_check;
        if (parse_val.repeat_key_check) {
            parse_val.key_total = JSON_KEY_TOTAL_NUM_DEF;
            if ((parse_val.key_array = json_malloc(parse_val.key_total * sizeof(char*))) == NULL) {
                JsonErr("malloc failed!\n");
                goto end;
            }
        }
    }

    if (_json_parse_value(&parse_val, &json, NULL, NULL) < 0) {
        if (parse_val.head) {
            _block_mem_del(parse_val.head);
        } else {
            json_del_object(json);
        }
        json = NULL;
    }

end:
    if (parse_val.key_array)
        json_free(parse_val.key_array);
#if JSON_DIRECT_FILE_SUPPORT
    if (parse_val.read_ptr)
        json_free(parse_val.read_ptr);
    if (parse_val.fp)
        fclose(parse_val.fp);
#endif
    return json;
}

#if JSON_SAX_APIS_SUPPORT
static int _sax_str_parse(json_parse_t *parse_ptr, json_detail_str_t *data, json_bool_t key_flag)
{
    int len = 0;
    int total = 0;
    char *ptr = NULL, *ptr_bak = NULL;
    char *str = NULL, *end_str = NULL;
#if JSON_DIRECT_FILE_SUPPORT
    int use_size_bak = 0;
#endif

    if (_str_parse_len_get(parse_ptr, &len, &total) < 0) {
        JsonErr("str format err!\n");
        return -1;
    }
#if JSON_DIRECT_FILE_SUPPORT
    if (parse_ptr->str) {
#endif
        _get_parse_ptr(parse_ptr, 0, 0, &str);
#if JSON_DIRECT_FILE_SUPPORT
    } else {
        use_size_bak = parse_ptr->read_size;
        if (_get_parse_ptr(parse_ptr, 0, total, &str) < total) {
            JsonErr("reread failed!\n");
            return -1;
        }
    }
#endif
    end_str = str + total - 1;
    str++;

    if (len == total - 2
#if JSON_DIRECT_FILE_SUPPORT
            && !(parse_ptr->fp && key_flag)
#endif
                ) {
        ptr = str;
        ptr_bak = ptr;
        ptr += len;
        data->alloc = 0;
        data->str = ptr_bak;
    } else {
        if ((ptr = json_malloc(len+1)) == NULL) {
            JsonErr("malloc failed!\n");
            return -1;
        }
        ptr_bak = ptr;
        data->alloc = 1;
        data->str = ptr_bak;
        if ((ptr = _str_parse_transform(str, end_str, ptr)) == NULL) {
            JsonErr("transform failed!\n");
            goto err;
        }
    }

    _update_parse_offset(parse_ptr, total);
    if (key_flag) {
        if (ptr == ptr_bak) {
            goto err;
        }
        _skip_blank(parse_ptr);
        _get_parse_ptr(parse_ptr, 0, 1, &str);
        if (*str != ':') {
            goto err;
        }
        _update_parse_offset(parse_ptr, 1);
    }
    data->len = ptr-ptr_bak;

#if JSON_DIRECT_FILE_SUPPORT
    if (!parse_ptr->str) {
        if (total > use_size_bak-1)
            _realloc_read_ptr(parse_ptr, use_size_bak);
    }
#endif
    return 0;

err:
    if (data->alloc && data->str) {
        json_free(data->str);
    }
    memset(data, 0, sizeof(json_detail_str_t));
    return -1;
}

static inline json_bool_t _sax_json_parse_check_stop(json_parse_t *parse_ptr)
{
    if (parse_ptr->parser_ret == JSON_SAX_PARSE_STOP) {
        parse_ptr->parser.value.vcmd = JSON_SAX_FINISH;
        parse_ptr->callback(&parse_ptr->parser);
        return JSON_TRUE;
    }
    return JSON_FALSE;
}

static int _sax_json_parse_value(json_parse_t *parse_ptr, json_detail_str_t *key)
{
    int ret = -1;
    int i = 0;
    json_sax_parse_depth_t *tmp_array = NULL;
    int cur_depth = 0;
    int nret = 0;

    char *str = NULL;
    int vint= 0;
    unsigned int vhex = 0;
    double vdbl = 0;
    json_detail_str_t vstr = {0};
    json_detail_str_t key_str = {0};

    // increase depth
    if (parse_ptr->parser.count == parse_ptr->parser.total) {
        tmp_array = parse_ptr->parser.array;
        parse_ptr->parser.total += JSON_KEY_TOTAL_NUM_DEF;
        if ((tmp_array = json_realloc(parse_ptr->parser.array, parse_ptr->parser.total * sizeof(json_sax_parse_depth_t))) == NULL) {
            for (i = 0; i < parse_ptr->parser.count; i++) {
                if (parse_ptr->parser.array[i].key.alloc && parse_ptr->parser.array[i].key.str) {
                    json_free(parse_ptr->parser.array[i].key.str);
                }
            }
            parse_ptr->parser.array = NULL;
            if (key->alloc) {
                json_free(key->str);
            }
            JsonErr("malloc failed!\n");
            return -1;;
        }
        parse_ptr->parser.array = tmp_array;
    }
    cur_depth = parse_ptr->parser.count++;
    memcpy(&parse_ptr->parser.array[cur_depth].key, key, sizeof(json_detail_str_t));

    // parse value
    _skip_blank(parse_ptr);
    _get_parse_ptr(parse_ptr, 0, 8, &str);
    switch(*str) {
        case '\"':
        {
            parse_ptr->parser.array[cur_depth].type = JSON_STRING;
            if (_sax_str_parse(parse_ptr, &vstr, JSON_FALSE) < 0) {
                goto end;
            }
            memcpy(&parse_ptr->parser.value.vstr, &vstr, sizeof(json_detail_str_t));
            break;
        }

        case '-': case '+':
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        {
            char *str_bak = NULL;
            _get_parse_ptr(parse_ptr, 0, 128, &str);
            str_bak = str;
            nret = _num_parse(&str, &vint, &vhex, &vdbl);
            parse_ptr->parser.array[cur_depth].type = nret;
            switch (nret) {
                case JSON_INT:    parse_ptr->parser.value.vint = vint; break;
                case JSON_HEX:    parse_ptr->parser.value.vhex = vhex; break;
                case JSON_DOUBLE: parse_ptr->parser.value.vdbl = vdbl; break;
                default: goto end;
            }
            _update_parse_offset(parse_ptr, str-str_bak);
            break;
        }

        case '{':
        {
            parse_ptr->parser.array[cur_depth].type = JSON_OBJECT;
            parse_ptr->parser.value.vcmd = JSON_SAX_START;
            parse_ptr->parser_ret = parse_ptr->callback(&parse_ptr->parser);
            _update_parse_offset(parse_ptr, 1);
            if (_sax_json_parse_check_stop(parse_ptr)) {
                ret = 0;
                goto end;
            }

            _skip_blank(parse_ptr);
            _get_parse_ptr(parse_ptr, 0, 1, &str);
            if (*str != '}') {
                while (1) {
                    _skip_blank(parse_ptr);
                    if (_sax_str_parse(parse_ptr, &key_str, JSON_TRUE) < 0) {
                        goto end;
                    }
                    if (_sax_json_parse_value(parse_ptr, &key_str) < 0) {
                        goto end;
                    }
                    if (_sax_json_parse_check_stop(parse_ptr)) {
                        ret = 0;
                        goto end;
                    }

                    _skip_blank(parse_ptr);
                    _get_parse_ptr(parse_ptr, 0, 1, &str);
                    switch(*str) {
                        case '}':
                            parse_ptr->parser.value.vcmd = JSON_SAX_FINISH;
                            _update_parse_offset(parse_ptr, 1);
                            goto success;
                        case ',':
                            _update_parse_offset(parse_ptr, 1);
                            break;
                        default:
                            JsonErr("invalid object!\n");
                            goto end;
                    }

                }
            } else {
                parse_ptr->parser.value.vcmd = JSON_SAX_FINISH;
                _update_parse_offset(parse_ptr, 1);
            }
            break;
        }

        case '[':
        {
            parse_ptr->parser.array[cur_depth].type = JSON_ARRAY;
            parse_ptr->parser.value.vcmd = JSON_SAX_START;
            parse_ptr->parser_ret = parse_ptr->callback(&parse_ptr->parser);
            _update_parse_offset(parse_ptr, 1);
            if (_sax_json_parse_check_stop(parse_ptr)) {
                ret = 0;
                goto end;
            }

            _skip_blank(parse_ptr);
            _get_parse_ptr(parse_ptr, 0, 1, &str);
            if (*str != ']') {
                while (1) {
                    if (_sax_json_parse_value(parse_ptr, &key_str) < 0) {
                        goto end;
                    }
                    if (_sax_json_parse_check_stop(parse_ptr)) {
                        ret = 0;
                        goto end;
                    }

                    _skip_blank(parse_ptr);
                    _get_parse_ptr(parse_ptr, 0, 1, &str);
                    switch(*str) {
                        case ']':
                            parse_ptr->parser.value.vcmd = JSON_SAX_FINISH;
                            _update_parse_offset(parse_ptr, 1);
                            goto success;
                        case ',':
                            _update_parse_offset(parse_ptr, 1);
                            break;
                        default:
                            JsonErr("invalid object!\n");
                            goto end;
                    }
                }
            } else {
                parse_ptr->parser.value.vcmd = JSON_SAX_FINISH;
                _update_parse_offset(parse_ptr, 1);
            }
            break;
        }

        default:
        {
            if (strncmp(str, "false", 5) == 0) {
                parse_ptr->parser.array[cur_depth].type = JSON_BOOL;
                parse_ptr->parser.value.vint = 0;
                _update_parse_offset(parse_ptr, 5);
            } else if (strncmp(str, "true", 4) == 0) {
                parse_ptr->parser.array[cur_depth].type = JSON_BOOL;
                parse_ptr->parser.value.vint = 1;
                _update_parse_offset(parse_ptr, 4);
            } else if (strncmp(str, "null", 4) == 0) {
                parse_ptr->parser.array[cur_depth].type = JSON_NULL;
                _update_parse_offset(parse_ptr, 4);
            } else {
                JsonErr("invalid next ptr!\n");
                goto end;
            }
            break;
        }
    }

success:
    parse_ptr->parser_ret = parse_ptr->callback(&parse_ptr->parser);
    ret = 0;

end:
    if (parse_ptr->parser.array[cur_depth].type == JSON_STRING
            && parse_ptr->parser.value.vstr.alloc && parse_ptr->parser.value.vstr.str) {
        json_free(parse_ptr->parser.value.vstr.str);
    }
    memset(&parse_ptr->parser.value, 0, sizeof(parse_ptr->parser.value));

    if (parse_ptr->parser.array[cur_depth].key.alloc) {
        json_free(parse_ptr->parser.array[cur_depth].key.str);
    }
    --parse_ptr->parser.count;

    return ret;
}

int json_sax_parse_common(json_sax_parse_choice_t *choice)
{
    int ret = -1;
    int i = 0;
    json_parse_t parse_val = {0};
    json_detail_str_t key_str = {0};

#if JSON_DIRECT_FILE_SUPPORT
    if (choice->str) {
#endif
        parse_val.str = choice->str;
#if JSON_DIRECT_FILE_SUPPORT
    } else {
        if ((parse_val.fp = fopen(choice->path, "r")) == NULL) {
            JsonErr("fopen(%s) failed!\n", choice->path);
            return -1;
        }

        if (choice->read_size < JSON_PARSE_USE_SIZE_DEF) {
            parse_val.read_size = JSON_PARSE_USE_SIZE_DEF;
        } else {
            parse_val.read_size = choice->read_size;
        }
        if ((parse_val.read_ptr = json_malloc(parse_val.read_size)) == NULL) {
            JsonErr("malloc failed!\n");
            goto end;
        }

        fseek(parse_val.fp, 0, SEEK_END);
        parse_val.file_size = ftell(parse_val.fp);
        fseek(parse_val.fp, 0, SEEK_SET);
    }
#endif

    if ((parse_val.parser.array = json_malloc(JSON_KEY_TOTAL_NUM_DEF * sizeof(json_sax_parse_depth_t))) == NULL) {
        JsonErr("malloc failed!\n");
        goto end;
    }
    parse_val.parser.total = JSON_KEY_TOTAL_NUM_DEF;
    parse_val.callback = choice->callback;

    ret = _sax_json_parse_value(&parse_val, &key_str);
end:
    if (parse_val.parser.array) {
        for (i = 0; i < parse_val.parser.count; i++) {
            if (parse_val.parser.array[i].key.alloc && parse_val.parser.array[i].key.str) {
                json_free(parse_val.parser.array[i].key.str);
            }
        }
        json_free(parse_val.parser.array);
    }
#if JSON_DIRECT_FILE_SUPPORT
    if (parse_val.read_ptr)
        json_free(parse_val.read_ptr);
    if (parse_val.fp)
        fclose(parse_val.fp);
#endif
    return ret;
}
#endif

typedef struct {
    size_t offset;
    char *str;
    size_t mem_size;
    struct json_list_head *head;
} json_rapid_parse_t;

static inline void _rapid_skip_blank(json_rapid_parse_t *parse_ptr)
{
    char *str = NULL, *bak = NULL;

    str = parse_ptr->str + parse_ptr->offset;
    bak = str;
    while (*str && (unsigned char)(*str) <= ' ')
        str++;
    parse_ptr->offset += str-bak;
}

static int _str_rapid_parse_len_get(json_rapid_parse_t *parse_ptr, int *out_len, int *total_len)
{
    int len = 0;
    char *str = NULL, *bak = NULL;

    str = parse_ptr->str + parse_ptr->offset;
    if (*str != '\"')
        return -1;
    bak = str;

    str++;
    while (*str && *str != '\"') {
        if (*str++ == '\\')
            str++;
         ++len;
    }
    if (*str != '\"')
        return -1;
    str++;

    *out_len = len;
    *total_len = str - bak;

    return 0;
}

static int _str_rapid_parse(json_rapid_parse_t *parse_ptr, char **pptr, json_bool_t key_flag)
{
    int len = 0;
    int total = 0;
    char *ptr = NULL, *ptr_bak = NULL;
    char *str = NULL, *end_str = NULL, *tmp_str = NULL;

    *pptr = NULL;
    if (_str_rapid_parse_len_get(parse_ptr, &len, &total) < 0) {
        JsonErr("str format err!\n");
        goto err;
    }

    str = parse_ptr->str + parse_ptr->offset;
    end_str = str + total - 1;
    str++;
    ptr = str;
    ptr_bak = ptr;
    if (len == total - 2) {
        ptr += len;
        *ptr = 0;
    } else {
        tmp_str = str;
        str = strchr(tmp_str, '\\');
        ptr += str - tmp_str;
        if ((ptr = _str_parse_transform(str, end_str, ptr)) == NULL) {
            JsonErr("transform failed!\n");
            goto err;
        }
    }

    parse_ptr->offset += total;
    if (key_flag) {
        if (ptr == ptr_bak) {
            goto err;
        }
        _rapid_skip_blank(parse_ptr);
        str = parse_ptr->str + parse_ptr->offset;
        if (*str != ':') {
            goto err;
        }
        parse_ptr->offset += 1;
    }
    *pptr = ptr_bak;

    return 0;
err:
    return -1;
}

static int _json_rapid_parse_value(json_rapid_parse_t *parse_ptr, json_object **item, json_object *parent, char *key)
{
    json_object *out_item = NULL;
    json_object *new_item = NULL;
    char *key_str = NULL;
    char *str = NULL;
    int nret = 0;
    int vint= 0;
    unsigned int vhex = 0;
    double vdbl = 0;
    char *vstr = NULL;

    if ((out_item = _block_mem_get(sizeof(json_object), OBJECT_MEM_ID,
            parse_ptr->mem_size, parse_ptr->head, NULL)) == NULL) {
        JsonErr("malloc failed!\n");
        goto err;
    }
    out_item->key = NULL;

    _rapid_skip_blank(parse_ptr);
    str = parse_ptr->str + parse_ptr->offset;

    switch (*str) {
        case '\"':
        {
            out_item->type = JSON_STRING;
            if (_str_rapid_parse(parse_ptr, &vstr, JSON_FALSE) < 0) {
                goto err;
            }
            out_item->vstr = vstr;
            break;
        }

        case '-': case '+':
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        {
            char *str_bak = str;
            nret = _num_parse(&str, &vint, &vhex, &vdbl);
            out_item->type = nret;
            switch (nret) {
                case JSON_INT:    out_item->vint = vint; break;
                case JSON_HEX:    out_item->vhex = vhex; break;
                case JSON_DOUBLE: out_item->vdbl = vdbl; break;
                default: goto err;
            }
            parse_ptr->offset += str-str_bak;
            break;
        }

        case '{':
        {
            out_item->type = JSON_OBJECT;
            INIT_JSON_LIST_HEAD(&out_item->head);
            parse_ptr->offset += 1;

            _rapid_skip_blank(parse_ptr);
            str = parse_ptr->str + parse_ptr->offset;
            if (*str != '}') {
                while (1) {
                    _rapid_skip_blank(parse_ptr);
                    if (_str_rapid_parse(parse_ptr, &key_str, JSON_TRUE) < 0) {
                        goto err;
                    }
                    if (_json_rapid_parse_value(parse_ptr, &new_item, out_item, key_str) < 0) {
                        goto err;
                    }
                    _rapid_skip_blank(parse_ptr);
                    str = parse_ptr->str + parse_ptr->offset;
                    switch(*str) {
                        case '}':
                            parse_ptr->offset += 1;
                            goto end;
                        case ',':
                            parse_ptr->offset += 1;
                            break;
                        default:
                            JsonErr("invalid object!\n");
                            goto err;
                    }
                }
            } else {
                parse_ptr->offset += 1;
            }
            break;
        }

        case '[':
        {
            out_item->type = JSON_ARRAY;
            INIT_JSON_LIST_HEAD(&out_item->head);
            parse_ptr->offset += 1;

            _rapid_skip_blank(parse_ptr);
            str = parse_ptr->str + parse_ptr->offset;
            if (*str != ']') {
                while (1) {
                    if (_json_rapid_parse_value(parse_ptr, &new_item, out_item, NULL) < 0) {
                        goto err;
                    }
                    _rapid_skip_blank(parse_ptr);
                    str = parse_ptr->str + parse_ptr->offset;
                    switch(*str) {
                        case ']':
                            parse_ptr->offset += 1;
                            goto end;
                        case ',':
                            parse_ptr->offset += 1;
                            break;
                        default:
                            JsonErr("invalid object!\n");
                            goto err;
                    }
                }
            } else {
                parse_ptr->offset += 1;
            }
            break;
        }

        default:
        {
            if (strncmp(str, "false", 5) == 0) {
                out_item->type = JSON_BOOL;
                out_item->vint = 0;
                parse_ptr->offset += 5;
            } else if (strncmp(str, "true", 4) == 0) {
                out_item->type = JSON_BOOL;
                out_item->vint = 1;
                parse_ptr->offset += 4;
            } else if (strncmp(str, "null", 4) == 0) {
                out_item->type = JSON_NULL;
                parse_ptr->offset += 4;
            } else {
                JsonErr("invalid next ptr!\n");
                goto err;
            }
            break;
        }
    }

end:
    if (parent) {
        out_item->key = key; // parent object is array, key is NULL.
        json_list_add_tail(&out_item->list, &parent->head);
    }

    *item = out_item;
    return 0;
err:
    return -1;
}

json_object *json_rapid_parse_str(char *str, struct json_list_head *head, size_t str_len)
{
    json_object *json = NULL;
    json_rapid_parse_t parse_val = {0};
    size_t mem_size = 0;

    parse_val.str = str;
    parse_val.head = head;
    INIT_JSON_LIST_HEAD(parse_val.head);
    str_len = (str_len != 0) ? str_len : strlen(str);
    mem_size = str_len / JSON_STR_MULTIPLE_NUM;
    parse_val.mem_size = mem_size - mem_size % JSON_PAGE_ALIGEN_BYTES + JSON_PAGE_ALIGEN_BYTES;

    if (_json_rapid_parse_value(&parse_val, &json, NULL, NULL) < 0) {
        _block_mem_del(parse_val.head);
        json = NULL;
    }
    return json;
}

