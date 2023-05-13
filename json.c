/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2019-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* URL: https://github.com/lengjingzju/json *
*******************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include "json.h"

/**************** debug ****************/

#define JSON_ERROR_PRINT_ENABLE         0

#if JSON_ERROR_PRINT_ENABLE
#define JsonErr(fmt, args...) do {                                  \
    printf("[JsonErr][%s:%d] ", __func__, __LINE__);                \
    printf(fmt, ##args);                                            \
} while(0)

#define JsonPareseErr(s)      do {                                  \
    if (parse_ptr->str) {                                           \
        JsonErr("%s\n%s\n", s, parse_ptr->str + parse_ptr->offset); \
    } else {                                                        \
        JsonErr("%s\n", s);                                         \
    }                                                               \
} while(0)

#else
#define JsonErr(fmt, args...) do {} while(0)
#define JsonPareseErr(s)      do {} while(0)
#endif

/* fix warning */
#if defined(__GNUC__)
#define UNUSED_ATTR                     __attribute__((unused))
#else
#define UNUSED_ATTR
#endif

/**************** definition ****************/

/* head apis */
#define json_malloc                     malloc
#define json_calloc                     calloc
#define json_realloc                    realloc
#define json_strdup                     strdup
#define json_free                       free

#define JSON_POOL_MEM_SIZE_DEF          8096
#define JSON_ITEM_NUM_PLUS_DEF          64

/* print choice size */
#define JSON_PRINT_UTF16_SUPPORT        0
#define JSON_PRINT_SIZE_PLUS_DEF        1024
#define JSON_FORMAT_ITEM_SIZE_DEF       32
#define JSON_UNFORMAT_ITEM_SIZE_DEF     24

/* file parse choice size */
#define JSON_PARSE_ERROR_STR            "Z"
#define JSON_PARSE_READ_SIZE_DEF        8096
#define JSON_PARSE_NUM_PLUS_DEF         8

/**************** json list ****************/

/* args of json_list_entry is different to list_entry of linux */
#define json_list_entry(ptr, type)  ((type *)(ptr))

#define json_list_for_each_entry(pos, head, member)             \
    for (pos = json_list_entry((head)->next, typeof(*pos));     \
        &pos->member != (head);                                 \
        pos = json_list_entry(pos->member.next, typeof(*pos)))

#define json_list_for_each_entry_safe(pos, n, head, member)     \
    for (pos = json_list_entry((head)->next, typeof(*pos)),     \
        n = json_list_entry(pos->member.next, typeof(*pos));    \
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

/**************** json normal apis ****************/

int json_item_total_get(json_object *json)
{
    int cnt = 0;
    json_object *item = NULL;

    if (!json)
        return 0;

    switch (json->type) {
    case JSON_ARRAY:
    case JSON_OBJECT:
        json_list_for_each_entry(item, &json->value.head, list) {
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
        if (json->value.vstr)
            json_free(json->value.vstr);
        break;
    case JSON_ARRAY:
    case JSON_OBJECT:
        json_list_for_each_entry_safe(pos, n, &json->value.head, list) {
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

    if ((json = json_calloc(1, sizeof(json_object))) == NULL) {
        JsonErr("malloc failed!\n");
        return NULL;
    }

    json->type = type;
    switch (json->type) {
    case JSON_ARRAY:
    case JSON_OBJECT:
        INIT_JSON_LIST_HEAD(&json->value.head);
        break;
    default:
        break;
    }

    return json;
}

json_object *json_create_item(json_type_t type, void *value)
{
    json_object *json = NULL;

    if ((json = json_calloc(1, sizeof(json_object))) == NULL) {
        JsonErr("malloc failed!\n");
        return NULL;
    }

    json->type = type;
    switch (type) {
    case JSON_NULL:
        break;
    case JSON_BOOL:   json->value.vnum.vbool = *(bool *)value;                  break;
    case JSON_INT:    json->value.vnum.vint  = *(int *)value;                   break;
    case JSON_HEX:    json->value.vnum.vhex  = *(unsigned int *)value;          break;
#if JSON_LONG_LONG_SUPPORT
    case JSON_LINT:   json->value.vnum.vlint = *(long long int *)value;         break;
    case JSON_LHEX:   json->value.vnum.vlhex = *(unsigned long long int *)value;break;
#endif
    case JSON_DOUBLE: json->value.vnum.vdbl  = *(double *)value;                break;
    case JSON_STRING:
        if (json_set_string_value(json, *(char **)value) < 0)
            return NULL;
        break;
    case JSON_ARRAY:
    case JSON_OBJECT:
        INIT_JSON_LIST_HEAD(&json->value.head);
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

    for (i = 0; i < count; ++i) {
        switch (type) {
        case JSON_BOOL:   value = ((bool *)values) + i;                  break;
        case JSON_INT:    value = ((int *)values) + i;                   break;
        case JSON_HEX:    value = ((unsigned int *)values) + i;          break;
#if JSON_LONG_LONG_SUPPORT
        case JSON_LINT:   value = ((long long int *)values) + i;         break;
        case JSON_LHEX:   value = ((unsigned long long int *)values) + i;break;
#endif
        case JSON_DOUBLE: value = ((double *)values) + i;                break;
        case JSON_STRING: value = ((char **)values) + i;                 break;
        default:          JsonErr("not support json type.\n");        goto err;
        }

        if ((node = json_create_item(type, value)) == NULL) {
            JsonErr("create item failed!\n");
            goto err;
        }
        json_list_add_tail(&node->list, &json->value.head);
    }

    return json;
err:
    json_del_object(json);
    return NULL;
}

int json_set_key(json_object *json, const char *key)
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

int json_set_string_value(json_object *json, const char *str)
{
    if (json->type == JSON_STRING) {
        if (str && strlen(str)) {
            if (json->value.vstr && strcmp(json->value.vstr, str) != 0) {
                json_free(json->value.vstr);
                json->value.vstr = NULL;
            }
            if (!json->value.vstr && (json->value.vstr = json_strdup(str)) == NULL) {
                JsonErr("malloc failed!\n");
                return -1;
            }
        } else {
            if (json->value.vstr) {
                json_free(json->value.vstr);
                json->value.vstr = NULL;
            }
        }
        return 0;
    }

    return -1;
}

int json_get_number_value(json_object *json, json_type_t type, void *value)
{
#if JSON_LONG_LONG_SUPPORT
#define _get_lnumber(json, etype, val)                                        \
    case JSON_LINT:   *(etype *)val = (etype)json->value.vnum.vlint;   break; \
    case JSON_LHEX:   *(etype *)val = (etype)json->value.vnum.vlhex;   break;
#else
#define _get_lnumber(json, etype, val)
#endif

#define _get_number(json, etype, val) do {                                    \
    switch (json->type) {                                                     \
    case JSON_BOOL:   *(etype *)val = (etype)json->value.vnum.vbool;   break; \
    case JSON_INT:    *(etype *)val = (etype)json->value.vnum.vint;    break; \
    case JSON_HEX:    *(etype *)val = (etype)json->value.vnum.vhex;    break; \
    _get_lnumber(json, etype, val)                                            \
    case JSON_DOUBLE: *(etype *)val = (etype)json->value.vnum.vdbl;    break; \
    default: *(etype *)val = (etype)0; JsonErr("wrong type!\n");   return -1; \
    }                                                                         \
} while(0)

    switch (type) {
    case JSON_BOOL:   _get_number(json, bool, value);                  break;
    case JSON_INT:    _get_number(json, int, value);                   break;
    case JSON_HEX:    _get_number(json, unsigned int, value);          break;
#if JSON_LONG_LONG_SUPPORT
    case JSON_LINT:   _get_number(json, long long int, value);         break;
    case JSON_LHEX:   _get_number(json, unsigned long long int, value);break;
#endif
    case JSON_DOUBLE: _get_number(json, double, value);                break;
    default:          JsonErr("wrong type!\n");                    return -1;
    }

    return json->type == type ? 0 : json->type;
}

int json_set_number_value(json_object *json, json_type_t type, void *value)
{
    int ret = 0;

    switch (json->type) {
    case JSON_BOOL:
    case JSON_INT:
    case JSON_HEX:
#if JSON_LONG_LONG_SUPPORT
    case JSON_LINT:
    case JSON_LHEX:
#endif
    case JSON_DOUBLE:
        break;
    default:
        JsonErr("wrong type!\n");
        return -1;
    }

    switch (type) {
    case JSON_BOOL:   json->value.vnum.vbool = *(bool *)value;                  break;
    case JSON_INT:    json->value.vnum.vint  = *(int *)value;                   break;
    case JSON_HEX:    json->value.vnum.vhex  = *(unsigned int *)value;          break;
#if JSON_LONG_LONG_SUPPORT
    case JSON_LINT:   json->value.vnum.vlint = *(long long int *)value;         break;
    case JSON_LHEX:   json->value.vnum.vlhex = *(unsigned long long int *)value;break;
#endif
    case JSON_DOUBLE: json->value.vnum.vdbl  = *(double *)value;                break;
    default:                                                                return -1;
    }

    if (json->type != type) {
        ret = json->type;
        json->type = type;
    }

    return ret;
}

int json_get_array_size(json_object *json)
{
    int count = 0;
    json_object *pos = NULL;

    if (json->type == JSON_ARRAY) {
        json_list_for_each_entry(pos, &json->value.head, list) {
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
        json_list_for_each_entry(pos, &json->value.head, list) {
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
        json_list_for_each_entry(pos, &json->value.head, list) {
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

    if (array->type == JSON_ARRAY) {
        if ((item = json_get_array_item(array, seq)) != NULL) {
            json_list_add_tail(&new_item->list, &item->list);
            json_list_del(&item->list);
            json_del_object(item);
        } else {
            json_list_add_tail(&new_item->list, &array->value.head);
        }

        return 0;
    }

    return -1;
}

int json_replace_item_in_object(json_object *object, const char *key, json_object *new_item)
{
    json_object *item = NULL;

    if (object->type == JSON_OBJECT) {
        if (json_set_key(new_item, key) < 0) {
            return -1;
        }
        if ((item = json_get_object_item(object, key)) != NULL) {
            json_list_add_tail(&new_item->list, &item->list);
            json_list_del(&item->list);
            json_del_object(item);
        } else {
            json_list_add_tail(&new_item->list, &object->value.head);
        }

        return 0;
    }

    return -1;
}

int json_add_item_to_array(json_object *array, json_object *item)
{
    if (array->type == JSON_ARRAY) {
        json_list_add_tail(&item->list, &array->value.head);
        return 0;
    }
    return -1;
}

int json_add_item_to_object(json_object *object, const char *key, json_object *item)
{
    if (object->type == JSON_OBJECT) {
        if (json_set_key(item, key) < 0) {
            return -1;
        }
        json_list_add_tail(&item->list, &object->value.head);
        return 0;
    }
    return -1;
}

json_object *json_deepcopy(json_object *json)
{
    json_object *new_json = NULL;
    json_object *item = NULL, *node = NULL;

    switch (json->type) {
    case JSON_NULL:   new_json = json_create_null();                        break;
    case JSON_BOOL:   new_json = json_create_bool(json->value.vnum.vbool);  break;
    case JSON_INT:    new_json = json_create_int(json->value.vnum.vint);    break;
    case JSON_HEX:    new_json = json_create_hex(json->value.vnum.vhex);    break;
#if JSON_LONG_LONG_SUPPORT
    case JSON_LINT:   new_json = json_create_lint(json->value.vnum.vlint);  break;
    case JSON_LHEX:   new_json = json_create_lhex(json->value.vnum.vlhex);  break;
#endif
    case JSON_DOUBLE: new_json = json_create_double(json->value.vnum.vdbl); break;
    case JSON_STRING: new_json = json_create_string(json->value.vstr);      break;
    case JSON_ARRAY:  new_json = json_create_array();                       break;
    case JSON_OBJECT: new_json = json_create_object();                      break;
    default:                                                                break;
    }

    if (new_json) {
        if (json->key && json_set_key(new_json, json->key) < 0) {
            JsonErr("add key failed!\n");
            json_del_object(new_json);
            return NULL;
        }
        if (json->type == JSON_ARRAY || json->type == JSON_OBJECT) {
            json_list_for_each_entry(item, &json->value.head, list) {
                if ((node = json_deepcopy(item)) == NULL) {
                    JsonErr("copy failed!\n");
                    json_del_object(new_json);
                    return NULL;
                }
                json_list_add_tail(&node->list, &new_json->value.head);
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
#if JSON_LONG_LONG_SUPPORT
        case JSON_LINT:
        case JSON_LHEX:
#endif
        case JSON_DOUBLE:
        case JSON_STRING:
            if ((item = json_create_item(type, value)) == NULL) {
                JsonErr("create item failed!\n");
                return -1;
            }
            if (json_set_key(item, key) < 0) {
                JsonErr("add key failed!\n");
                json_del_object(item);
                return -1;
            }
            json_list_add_tail(&item->list, &object->value.head);
            return 0;
        default:
            JsonErr("not support json type.\n");
            return -1;
        }
    }

    return -1;
}

/**************** json pool memory apis ****************/

static json_mem_node_t s_invalid_json_mem_node;
static json_mem_t s_invalid_json_mem;

static void _json_mem_init(json_mem_mgr_t *mgr)
{
    INIT_JSON_LIST_HEAD(&mgr->head);
    mgr->cur_node = &s_invalid_json_mem_node;
    mgr->mem_size = JSON_POOL_MEM_SIZE_DEF;
}

static void _json_mem_free(json_mem_mgr_t *mgr)
{
    json_mem_node_t *pos = NULL, *n = NULL;
    json_list_for_each_entry_safe(pos, n, &mgr->head, list) {
        json_list_del(&pos->list);
        json_free(pos->ptr);
        json_free(pos);
    }
    mgr->cur_node = &s_invalid_json_mem_node;
}

static void *_json_mem_new(size_t size, json_mem_mgr_t *mgr)
{
    json_mem_node_t *node = NULL;

    if ((node = json_malloc(sizeof(json_mem_node_t))) == NULL) {
        JsonErr("malloc failed!\n");
        return NULL;
    }
    node->size = size;
    if ((node->ptr = json_malloc(node->size)) == NULL) {
        JsonErr("malloc failed! %d\n", (int)node->size);
        json_free(node);
        return NULL;
    }
    node->cur = node->ptr;
    json_list_add(&node->list, &mgr->head);

    return node;
}

static void *pjson_memory_alloc(size_t size, json_mem_mgr_t *mgr)
{
    void *p = NULL;
    size_t data_size = 0, block_size = 0;

    data_size = size;
    if (mgr->cur_node->cur + data_size <= mgr->cur_node->ptr + mgr->cur_node->size) {
        goto end;
    }

    block_size = (data_size > mgr->mem_size) ? data_size : mgr->mem_size;
    if (_json_mem_new(block_size, mgr) != NULL) {
        mgr->cur_node = (json_mem_node_t *) (mgr->head.next);
        goto end;
    }

    return NULL;
end:
    p = mgr->cur_node->cur;
    mgr->cur_node->cur += data_size;
    return p;
}

void pjson_memory_init(json_mem_t *mem)
{
    _json_mem_init(&mem->obj_mgr);
    _json_mem_init(&mem->key_mgr);
    _json_mem_init(&mem->str_mgr);
}

void pjson_memory_free(json_mem_t *mem)
{
    _json_mem_free(&mem->obj_mgr);
    _json_mem_free(&mem->key_mgr);
    _json_mem_free(&mem->str_mgr);
}

json_object *pjson_new_object(json_type_t type, json_mem_t *mem)
{
    json_object *json = NULL;

    if ((json = pjson_memory_alloc(sizeof(json_object), &mem->obj_mgr)) == NULL) {
        JsonErr("malloc failed!\n");
        return NULL;
    }
    memset(json, 0, sizeof(json_object));
    json->type = type;

    switch (json->type) {
    case JSON_ARRAY:
    case JSON_OBJECT:
        INIT_JSON_LIST_HEAD(&json->value.head);
        break;
    default:
        break;
    }

    return json;
}

json_object *pjson_create_item(json_type_t type, void *value, json_mem_t *mem)
{
    json_object *json = NULL;

    if ((json = pjson_memory_alloc(sizeof(json_object), &mem->obj_mgr)) == NULL) {
        JsonErr("malloc failed!\n");
        return NULL;
    }
    memset(json, 0, sizeof(json_object));
    json->type = type;

    switch (json->type) {
    case JSON_NULL:
        break;
    case JSON_BOOL:   json->value.vnum.vbool = *(bool *)value;                  break;
    case JSON_INT:    json->value.vnum.vint  = *(int *)value;                   break;
    case JSON_HEX:    json->value.vnum.vhex  = *(unsigned int *)value;          break;
#if JSON_LONG_LONG_SUPPORT
    case JSON_LINT:   json->value.vnum.vlint = *(long long int *)value;         break;
    case JSON_LHEX:   json->value.vnum.vlhex = *(unsigned long long int *)value;break;
#endif
    case JSON_DOUBLE: json->value.vnum.vdbl  = *(double *)value;                break;
    case JSON_STRING:
        if (pjson_set_string_value(json, *(char **)value, mem) < 0)
            return NULL;
        break;
    case JSON_ARRAY:
    case JSON_OBJECT:
        INIT_JSON_LIST_HEAD(&json->value.head);
        break;
    default:
        break;
    }

    return json;
}

json_object *pjson_create_item_array(json_type_t item_type, void *values, int count, json_mem_t *mem)
{
    int i = 0;
    void *value = NULL;
    json_object *json = NULL, *node = NULL;

    if ((json = pjson_create_array(mem)) == NULL) {
        JsonErr("create array failed!\n");
        return NULL;
    }

    for (i = 0; i < count; ++i) {
        switch (item_type) {
        case JSON_BOOL:   value = ((bool *)values) + i;                   break;
        case JSON_INT:    value = ((int *)values) + i;                    break;
        case JSON_HEX:    value = ((unsigned int *)values) + i;           break;
#if JSON_LONG_LONG_SUPPORT
        case JSON_LINT:   value = ((long long int *)values) + i;          break;
        case JSON_LHEX:   value = ((unsigned long long int *)values) + i; break;
#endif
        case JSON_DOUBLE: value = ((double *)values) + i;                 break;
        case JSON_STRING: value = ((char **)values) + i;                  break;
        default: JsonErr("not support json type.\n");               return NULL;
        }

        if ((node = pjson_create_item(item_type, value, mem)) == NULL) {
            JsonErr("create item failed!\n");
            return NULL;
        }
        json_list_add_tail(&node->list, &json->value.head);
    }

    return json;
}

int pjson_set_key(json_object *json, const char *key, json_mem_t *mem)
{
    size_t len = 0;

    if (!key || (len = strlen(key)) == 0) {
        return -1;
    }
    if ((json->key = pjson_memory_alloc(len+1, &mem->key_mgr)) == NULL) {
        JsonErr("malloc failed!\n");
        return -1;
    }
    memcpy(json->key, key, len);
    json->key[len] = 0;

    return 0;
}

int pjson_set_string_value(json_object *json, const char *str, json_mem_t *mem)
{
    size_t len = 0;

    if (json->type != JSON_STRING) {
        return -1;
    }
    if (str && (len = strlen(str))) {
        if (!json->value.vstr || strcmp(json->value.vstr, str) != 0) {
            if ((json->value.vstr = pjson_memory_alloc(len+1, &mem->str_mgr)) == NULL) {
                JsonErr("malloc failed!\n");
                return -1;
            }
            memcpy(json->value.vstr, str, len);
            json->value.vstr[len] = 0;
        }
    } else {
        json->value.vstr = NULL;
    }

    return 0;
}

int pjson_add_item_to_object(json_object *object, const char *key, json_object *item, json_mem_t *mem)
{
    if (object->type == JSON_OBJECT) {
        if (pjson_set_key(item, key, mem) < 0) {
            JsonErr("add key failed!\n");
            return -1;
        }
        json_list_add_tail(&item->list, &object->value.head);
        return 0;
    }

    return -1;
}

int pjson_add_new_item_to_object(json_object *object, json_type_t type, const char *key, void *value, json_mem_t *mem)
{
    json_object *item = NULL;

    if (object->type == JSON_OBJECT) {
        switch (type) {
        case JSON_NULL:
        case JSON_BOOL:
        case JSON_INT:
        case JSON_HEX:
#if JSON_LONG_LONG_SUPPORT
        case JSON_LINT:
        case JSON_LHEX:
#endif
        case JSON_DOUBLE:
        case JSON_STRING:
            if ((item = pjson_create_item(type, value, mem)) == NULL) {
                JsonErr("create item failed!\n");
                return -1;
            }
            if (pjson_set_key(item, key, mem) < 0) {
                JsonErr("add key failed!\n");
                return -1;
            }
            json_list_add_tail(&item->list, &object->value.head);
            return 0;
        default:
            JsonErr("not support json type.\n");
            return -1;
        }
    }

    return -1;
}

/**************** json print apis ****************/

typedef struct _json_print_t {
    int fd;
    size_t size;
    size_t used;

    size_t plus_size;
    size_t item_size;
    int item_total;
    int item_count;
    bool format_flag;

    int (*realloc)(struct _json_print_t *print_ptr, size_t slen);
    char *ptr;
} json_print_t;

static const char hex_array[] = {
    '0', '1', '2', '3', '4',
    '5', '6', '7', '8', '9',
    'a', 'b', 'c', 'd', 'e', 'f'};

#define INT_TO_STRING_FUNC(fname, ftype)    \
static int fname(ftype n, char *c)          \
{                                           \
    char *s = c;                            \
    ftype m = n, t = 0;                     \
    int h = 0, i = 0, j = 0, k = 0, a = 0;  \
                                            \
    if (m == 0) {                           \
        *s++ = '0';                         \
        *s = '\0';                          \
        return 1;                           \
    }                                       \
                                            \
    if (n < 0) {                            \
        m = -n;                             \
        *s++ = '-';                         \
        a = 1;                              \
    }                                       \
                                            \
    while (m > 0) {                         \
        s[i++] = m % 10 + '0';              \
        m /= 10;                            \
    }                                       \
    s[i] = '\0';                            \
                                            \
    h = i >> 1;                             \
    while (j < h) {                         \
        k = i - j - 1;                      \
        t = s[k];                           \
        s[k] = s[j];                        \
        s[j++] = t;                         \
    }                                       \
                                            \
    return i + a;                           \
}

#define HEX_TO_STRING_FUNC(fname, ftype)    \
static int fname(ftype n, char *c)          \
{                                           \
    char *s = c;                            \
    ftype m = n, t = 0;                     \
    int h = 0, i = 0, j = 0, k = 0, a = 0;  \
                                            \
    *s++ = '0';                             \
    *s++ = 'x';                             \
    if (m == 0) {                           \
        *s++ = '0';                         \
        *s = '\0';                          \
        return 3;                           \
    }                                       \
    a = 2;                                  \
                                            \
    while (m > 0) {                         \
        s[i++] = hex_array[m & 0xf];        \
        m >>= 4;                            \
    }                                       \
    s[i] = '\0';                            \
                                            \
    h = i >> 1;                             \
    while (j < h)                           \
    {                                       \
        t = i - j - 1;                      \
        k = s[t];                           \
        s[t] = s[j];                        \
        s[j++] = k;                         \
    }                                       \
                                            \
    return i + a;                           \
}

INT_TO_STRING_FUNC(int_to_string, int)
INT_TO_STRING_FUNC(lint_to_string, long long int)
HEX_TO_STRING_FUNC(hex_to_string, unsigned int)
#if JSON_LONG_LONG_SUPPORT
HEX_TO_STRING_FUNC(lhex_to_string, unsigned long long int)
#endif

static int double_to_string(double n, char *c)
{
    char *s = c;
    long long int x = 0;
    double y = 0, z = n;
    int a = 0, i = 0;

    x = (long long int)z;
    y = z - x;

    if (x == 0) {
        *s++ = '0';
        if (y == 0) {
            *s++ = '\0';
            return 1;
        } else {
            *s++ = '.';
            a = 2;
            goto next;
        }
    } else {
        a = lint_to_string(x, s);
        if (y == 0) {
            return a;
        } else {
            s += a;
            *s++ = '.';
            a += 1;
            goto next;
        }
    }

next:
    if (n < 0) {
        z = -n;
        y = -y;
    }

    while (y) {
        z *= 10;
        x = (long long int)z;
        y = z - x;
        s[i++] = x % 10 + '0';
    }
    s[i] = '\0';

    return i + a;
}

static int _print_file_ptr_realloc(json_print_t *print_ptr, size_t slen)
{
    size_t len = print_ptr->used + slen + 1;

    if (print_ptr->size < len) {
        if (print_ptr->used > 0) {
            if (print_ptr->used !=(size_t)write(print_ptr->fd, print_ptr->ptr, print_ptr->used)) {
                JsonErr("write failed!\n");
                return -1;
            }
            print_ptr->used = 0;
        }

        len = slen + 1;
        if (print_ptr->size < len) {
            while (print_ptr->size < len)
                print_ptr->size += print_ptr->plus_size;
            if ((print_ptr->ptr = json_realloc(print_ptr->ptr, print_ptr->size)) == NULL) {
                JsonErr("malloc failed!\n");
                return -1;
            }
        }
    }

    return 0;
}

static int _print_str_ptr_realloc(json_print_t *print_ptr, size_t slen)
{
    size_t len = print_ptr->used + slen + 1;

    if (print_ptr->size < len) {
        while (print_ptr->item_total < print_ptr->item_count)
            print_ptr->item_total += JSON_ITEM_NUM_PLUS_DEF;
        if (print_ptr->item_total - print_ptr->item_count > print_ptr->item_count) {
            print_ptr->size += print_ptr->size;
        } else {
            print_ptr->size += (long long)print_ptr->size *
                (print_ptr->item_total - print_ptr->item_count) / print_ptr->item_count;
        }

        while (print_ptr->size < len)
            print_ptr->size += print_ptr->plus_size;
        if ((print_ptr->ptr = json_realloc(print_ptr->ptr, print_ptr->size)) == NULL) {
            JsonErr("malloc failed!\n");
            return -1;
        }
    }

    return 0;
}

static inline int _print_ptr_realloc(json_print_t *print_ptr, size_t slen)
{
    return print_ptr->realloc(print_ptr, slen);
}
#define _PRINT_PTR_REALLOC(ptr, len) do { if (_print_ptr_realloc(ptr, len) < 0) goto err; } while(0)

static inline int _print_ptr_strncat(json_print_t *print_ptr, const char *str, size_t slen)
{
    if (print_ptr->realloc(print_ptr, slen) < 0)
        return -1;

    memcpy(print_ptr->ptr + print_ptr->used, str, slen);
    print_ptr->used += slen;
    return 0;
}
#define _PRINT_PTR_STRNCAT(ptr, str, slen) do { if (_print_ptr_strncat(ptr, str, slen) < 0) goto err; } while(0)

static int _print_addi_format(json_print_t *print_ptr, int depth)
{
    size_t len = 0;

    if (print_ptr->format_flag && depth > 0) {
        len = depth + 2;
        _PRINT_PTR_REALLOC(print_ptr, len);

        print_ptr->ptr[print_ptr->used++] = '\n';
        while (depth-- > 0)
            print_ptr->ptr[print_ptr->used++] = '\t';
    }

    return 0;
err:
    return -1;
}
#define _PRINT_ADDI_FORMAT(ptr, depth) do { if (_print_addi_format(ptr, depth) < 0) goto err; } while(0)

static int _json_print_string(json_print_t *print_ptr, const char *value)
{
    size_t len = 0;
    size_t vlen = 0;
    size_t i = 0;

    if (value) {
        vlen = strlen(value);
        len += vlen;
        for (i = 0; i < vlen; ++i) {
            if (strchr("\"\\\b\f\n\r\t\v", value[i])) {
                ++len;
#if JSON_PRINT_UTF16_SUPPORT
            } else if ((unsigned char)value[i] < ' ') {
                len += 5;
#endif
            }
        }
        len += 3;
        _PRINT_PTR_REALLOC(print_ptr, len);

        print_ptr->ptr[print_ptr->used++] = '\"';
        for (i = 0; i < vlen; ++i) {
            if (
#if JSON_PRINT_UTF16_SUPPORT
                (unsigned char)value[i] >= ' ' &&
#endif
                strchr("\"\\\b\f\n\r\t\v", value[i]) == NULL) {
                print_ptr->ptr[print_ptr->used++] = value[i];
            } else {
                print_ptr->ptr[print_ptr->used++] = '\\';
                switch (value[i]) {
                case '\"': print_ptr->ptr[print_ptr->used++] = '\"'; break;
                case '\\': print_ptr->ptr[print_ptr->used++] = '\\'; break;
                case '\b': print_ptr->ptr[print_ptr->used++] = 'b' ; break;
                case '\f': print_ptr->ptr[print_ptr->used++] = 'f' ; break;
                case '\n': print_ptr->ptr[print_ptr->used++] = 'n' ; break;
                case '\r': print_ptr->ptr[print_ptr->used++] = 'r' ; break;
                case '\t': print_ptr->ptr[print_ptr->used++] = 't' ; break;
                case '\v': print_ptr->ptr[print_ptr->used++] = 'v' ; break;
                default:
#if JSON_PRINT_UTF16_SUPPORT
                    {
                        unsigned char uc = (unsigned char)value[i];
                        print_ptr->ptr[print_ptr->used++] = 'u';
                        print_ptr->ptr[print_ptr->used++] = '0';
                        print_ptr->ptr[print_ptr->used++] = '0';
                        print_ptr->ptr[print_ptr->used++] = hex_array[uc >> 4 & 0xf];
                        print_ptr->ptr[print_ptr->used++] = hex_array[uc & 0xf];
                    }
#endif
                    break;
                }
            }
        }
        print_ptr->ptr[print_ptr->used++] = '\"';
    } else {
        _PRINT_PTR_STRNCAT(print_ptr, "\"\"", 2);
    }

    return 0;
err:
    return -1;
}
#define _JSON_PRINT_STRING(ptr, val) do { if (_json_print_string(ptr, val) < 0) goto err; } while(0)

static int _json_print(json_print_t *print_ptr, json_object *json, int depth, bool print_key)
{
    json_object *item = NULL;
    char nbuf[64] = {0};
    int nlen = 0;
    int new_depth = 0;

    if (print_key) {
        _PRINT_ADDI_FORMAT(print_ptr, depth);
        _JSON_PRINT_STRING(print_ptr, json->key);
        if (print_ptr->format_flag) {
            _PRINT_PTR_STRNCAT(print_ptr, ":\t", 2);
        } else {
            _PRINT_PTR_STRNCAT(print_ptr, ":", 1);
        }
    }

    switch (json->type) {
    case JSON_NULL:
        _PRINT_PTR_STRNCAT(print_ptr, "null", 4);
        break;
    case JSON_BOOL:
        if (json->value.vnum.vbool) {
            _PRINT_PTR_STRNCAT(print_ptr, "true", 4);
        } else {
            _PRINT_PTR_STRNCAT(print_ptr, "false", 5);
        }
        break;
    case JSON_INT:
        nlen = int_to_string(json->value.vnum.vint, nbuf);
        _PRINT_PTR_STRNCAT(print_ptr, nbuf, nlen);
        break;
    case JSON_HEX:
        nlen = hex_to_string(json->value.vnum.vhex, nbuf);
        _PRINT_PTR_STRNCAT(print_ptr, nbuf, nlen);
        break;
#if JSON_LONG_LONG_SUPPORT
    case JSON_LINT:
        nlen = lint_to_string(json->value.vnum.vlint, nbuf);
        _PRINT_PTR_STRNCAT(print_ptr, nbuf, nlen);
        break;
    case JSON_LHEX:
        nlen = lhex_to_string(json->value.vnum.vlhex, nbuf);
        _PRINT_PTR_STRNCAT(print_ptr, nbuf, nlen);
        break;
#endif
    case JSON_DOUBLE:
        nlen = double_to_string(json->value.vnum.vdbl, nbuf);
        _PRINT_PTR_STRNCAT(print_ptr, nbuf, nlen);
        break;
    case JSON_STRING:
        _JSON_PRINT_STRING(print_ptr, json->value.vstr);
        break;
    case JSON_ARRAY:
        new_depth = depth + 1;
        _PRINT_PTR_STRNCAT(print_ptr, "[", 1);
        json_list_for_each_entry(item, &json->value.head, list) {
            if (print_ptr->format_flag && item->list.prev == &json->value.head) {
                if (item->type == JSON_OBJECT || item->type == JSON_ARRAY)
                    _PRINT_ADDI_FORMAT(print_ptr, new_depth);
            }
            if (_json_print(print_ptr, item, new_depth, false) < 0) {
                return -1;
            }
            if (item->list.next != &json->value.head) {
                _PRINT_PTR_STRNCAT(print_ptr, ",", 1);
                if (print_ptr->format_flag)
                    _PRINT_PTR_STRNCAT(print_ptr, " ", 1);
            }
        }
        if (print_ptr->format_flag && json->value.head.prev != &json->value.head) {
            item = (json_object *)json->value.head.prev;
            if (item->type == JSON_OBJECT || item->type == JSON_ARRAY) {
                if (depth) {
                    _PRINT_ADDI_FORMAT(print_ptr, depth);
                } else {
                    _PRINT_PTR_STRNCAT(print_ptr, "\n", 1);
                }
            }
        }
        _PRINT_PTR_STRNCAT(print_ptr, "]", 1);
        break;
    case JSON_OBJECT:
        new_depth = depth + 1;
        _PRINT_PTR_STRNCAT(print_ptr, "{", 1);
        json_list_for_each_entry(item, &json->value.head, list) {
            if (_json_print(print_ptr, item, new_depth, true) < 0) {
                return -1;
            }
            if (item->list.next != &json->value.head) {
                _PRINT_PTR_STRNCAT(print_ptr, ",", 1);
            } else {
                if (print_ptr->format_flag) {
                    if (depth == 0) {
                        _PRINT_PTR_STRNCAT(print_ptr, "\n", 1);
                    }
                }
            }
        }
        if (json->value.head.next != &json->value.head)
            _PRINT_ADDI_FORMAT(print_ptr, depth);
        _PRINT_PTR_STRNCAT(print_ptr, "}", 1);
        break;
    default:
        break;
    }
    ++print_ptr->item_count;

    return 0;
err:
    return -1;
}

static int _print_val_release(json_print_t *print_ptr, bool free_all_flag)
{
#define _clear_free_ptr(ptr)    do { if (ptr) json_free(ptr); ptr = NULL; } while(0)
#define _clear_close_fd(fd)     do { if (fd >= 0) close(fd); fd = -1; } while(0)
    int ret = 0;
    if (print_ptr->fd >= 0) {
        if (print_ptr->used > 0) {
            if (print_ptr->used != (size_t)write(print_ptr->fd, print_ptr->ptr, print_ptr->used)) {
                JsonErr("write failed!\n");
                ret = -1;
            }
        }
        _clear_close_fd(print_ptr->fd);
        _clear_free_ptr(print_ptr->ptr);

    } else {
        if (free_all_flag) {
            _clear_free_ptr(print_ptr->ptr);
        } else {
            print_ptr->ptr = json_realloc(print_ptr->ptr, print_ptr->used+1);
            print_ptr->ptr[print_ptr->used] = '\0';
        }
    }

    return ret;
}

static int _print_val_init(json_print_t *print_ptr, json_print_choice_t *choice)
{
    print_ptr->format_flag = choice->format_flag;
    print_ptr->plus_size = choice->plus_size ? choice->plus_size : JSON_PRINT_SIZE_PLUS_DEF;

    print_ptr->fd = -1;
    if (choice->path) {
        print_ptr->realloc = _print_file_ptr_realloc;
        if ((print_ptr->fd = open(choice->path, O_CREAT|O_TRUNC|O_WRONLY, 0666)) < 0) {
            JsonErr("open(%s) failed!\n", choice->path);
            goto err;
        }
        print_ptr->size = print_ptr->plus_size;
    } else {
        size_t item_size = 0;
        size_t total_size = 0;

        print_ptr->realloc = _print_str_ptr_realloc;
        item_size = (choice->format_flag) ? JSON_FORMAT_ITEM_SIZE_DEF : JSON_UNFORMAT_ITEM_SIZE_DEF;
        if (choice->item_size > item_size)
            item_size = choice->item_size;

        total_size = print_ptr->item_total * item_size;
        if (total_size < JSON_PRINT_SIZE_PLUS_DEF)
            total_size = JSON_PRINT_SIZE_PLUS_DEF;
        print_ptr->size = total_size;
    }

    if ((print_ptr->ptr = json_malloc(print_ptr->size)) == NULL) {
        JsonErr("malloc failed!\n");
        goto err;
    }

    return 0;
err:
    _print_val_release(print_ptr, true);
    return -1;
}

char *json_print_common(json_object *json, json_print_choice_t *choice)
{
    json_print_t print_val = {0};

    if (!json)
        return NULL;

    print_val.item_total = choice->item_total ? print_val.item_total : json_item_total_get(json);
    if (_print_val_init(&print_val, choice) < 0)
        return NULL;

    if (_json_print(&print_val, json, 0, false) < 0) {
        JsonErr("print failed!\n");
        goto err;
    }
    if (_print_val_release(&print_val, false) < 0)
        goto err;

    return choice->path ? "ok" : print_val.ptr;
err:
    _print_val_release(&print_val, true);
    return NULL;
}

void json_free_print_ptr(void *ptr)
{
    if (ptr) json_free(ptr);
}

#if JSON_SAX_APIS_SUPPORT
typedef struct {
    json_type_t type;
    int num;
} json_sax_print_depth_t;

typedef struct {
    int total;
    int count;
    json_sax_print_depth_t *array;

    json_print_t print_val;
    json_type_t last_type;
    bool error_flag;
} json_sax_print_t;

int json_sax_print_value(json_sax_print_hd handle, json_type_t type, const char *key, const void *value)
{
    json_sax_print_t *print_handle = handle;
    json_print_t *print_ptr = &print_handle->print_val;
    char nbuf[64] = {0};
    int nlen = 0;
    int cur_pos = print_handle->count - 1;

    if (print_handle->error_flag) {
        return -1;
    }

    if (print_handle->count > 0) {
        if (!((type == JSON_ARRAY || type == JSON_OBJECT) && (*(json_sax_cmd_t*)value) == JSON_SAX_FINISH)) {
            // add ","
            if (print_handle->array[cur_pos].num > 0) {
                if (print_ptr->format_flag && print_handle->array[cur_pos].type == JSON_ARRAY) {
                    _PRINT_PTR_STRNCAT(print_ptr, ", ", 2);
                } else {
                    _PRINT_PTR_STRNCAT(print_ptr, ",", 1);
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
                    _PRINT_PTR_STRNCAT(print_ptr, ":\t", 2);
                } else {
                    _PRINT_PTR_STRNCAT(print_ptr, ":", 1);
                }
            }
        }
    }

    // add value
    switch (type) {
    case JSON_NULL:
        _PRINT_PTR_STRNCAT(print_ptr, "null", 4);
        break;
    case JSON_BOOL:
        if ((*(bool*)value)) {
            _PRINT_PTR_STRNCAT(print_ptr, "true", 4);
        } else {
            _PRINT_PTR_STRNCAT(print_ptr, "false", 5);
        }
        break;

    case JSON_INT:
        nlen = int_to_string(*(int*)value, nbuf);
        _PRINT_PTR_STRNCAT(print_ptr, nbuf, nlen);
        break;
    case JSON_HEX:
        nlen = hex_to_string(*(unsigned int*)value, nbuf);
        _PRINT_PTR_STRNCAT(print_ptr, nbuf, nlen);
        break;
#if JSON_LONG_LONG_SUPPORT
    case JSON_LINT:
        nlen = lint_to_string(*(long long int*)value, nbuf);
        _PRINT_PTR_STRNCAT(print_ptr, nbuf, nlen);
        break;
    case JSON_LHEX:
        nlen = lhex_to_string(*(unsigned long long int*)value, nbuf);
        _PRINT_PTR_STRNCAT(print_ptr, nbuf, nlen);
        break;
#endif
    case JSON_DOUBLE:
        nlen = double_to_string(*(double*)value, nbuf);
        _PRINT_PTR_STRNCAT(print_ptr, nbuf, nlen);
        break;
    case JSON_STRING:
        _JSON_PRINT_STRING(print_ptr, ((char*)value));
        break;

    case JSON_ARRAY:
        switch ((*(json_sax_cmd_t*)value)) {
        case JSON_SAX_START:
            if (print_handle->count == print_handle->total) {
                print_handle->total += JSON_ITEM_NUM_PLUS_DEF;
                if ((print_handle->array = json_realloc(print_handle->array,
                            print_handle->total * sizeof(json_sax_print_depth_t))) == NULL) {
                    JsonErr("malloc failed!\n");
                    goto err;
                }
            }
            if (print_handle->count > 0)
                ++print_handle->array[cur_pos].num;
            _PRINT_PTR_STRNCAT(print_ptr, "[", 1);
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
                    _PRINT_PTR_STRNCAT(print_ptr, "\n", 1);
                }
            }
            _PRINT_PTR_STRNCAT(print_ptr, "]", 1);
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
                print_handle->total += JSON_ITEM_NUM_PLUS_DEF;
                if ((print_handle->array = json_realloc(print_handle->array,
                            print_handle->total * sizeof(json_sax_print_depth_t))) == NULL) {
                    JsonErr("malloc failed!\n");
                    goto err;
                }
            }
            if (print_handle->count > 0)
                ++print_handle->array[cur_pos].num;
            _PRINT_PTR_STRNCAT(print_ptr, "{", 1);
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
                    _PRINT_PTR_STRNCAT(print_ptr, "\n", 1);
            }
            _PRINT_PTR_STRNCAT(print_ptr, "}", 1);
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
    print_handle->error_flag = true;
    return -1;
}

json_sax_print_hd json_sax_print_start(json_print_choice_t *choice)
{
    json_sax_print_t *print_handle = NULL;

    if ((print_handle = json_calloc(1, sizeof(json_sax_print_t))) == NULL) {
        JsonErr("malloc failed!\n");
        return NULL;
    }
    if (choice->item_total == 0)
        choice->item_total = JSON_ITEM_NUM_PLUS_DEF;
    if (_print_val_init(&print_handle->print_val, choice) < 0) {
        json_free(print_handle);
        return NULL;
    }

    print_handle->total = JSON_ITEM_NUM_PLUS_DEF;
    if ((print_handle->array = json_malloc(print_handle->total * sizeof(json_sax_print_depth_t))) == NULL) {
        _print_val_release(&print_handle->print_val, true);
        json_free(print_handle);
        JsonErr("malloc failed!\n");
        return NULL;
    }

    return print_handle;
}

char *json_sax_print_finish(json_sax_print_hd handle)
{
    char *ret = NULL;

    json_sax_print_t *print_handle = handle;
    if (!print_handle)
        return NULL;
    if (print_handle->array)
        json_free(print_handle->array);
    if (print_handle->error_flag) {
        _print_val_release(&print_handle->print_val, true);
        json_free(print_handle);
        return NULL;
    }

    ret = (print_handle->print_val.fd >= 0) ? "ok" : print_handle->print_val.ptr;
    if (_print_val_release(&print_handle->print_val, false) < 0) {
        json_free(print_handle);
        return NULL;
    }
    json_free(print_handle);

    return ret;
}
#endif

typedef struct _json_parse_t {
    int fd;
    size_t size;
    size_t offset;
    size_t readed;
    size_t read_size;
    bool reuse_flag;

    char *str;
    json_mem_t *mem;
    void *(*alloc)(size_t size, json_mem_mgr_t *mgr);
    int (*getptr)(struct _json_parse_t *parse_ptr, int read_offset, size_t read_size, char **sstr);

#if JSON_SAX_APIS_SUPPORT
    json_sax_parser_t parser;
    json_sax_cb_t cb;
    json_sax_ret_t ret;
#endif
} json_parse_t;

static void *json_memory_alloc(size_t size, json_mem_mgr_t *mgr UNUSED_ATTR)
{
    return json_malloc(size);
}

static inline void *_parse_alloc(json_parse_t *parse_ptr, size_t size, json_mem_mgr_t *mgr)
{
    return parse_ptr->alloc(size, mgr);
}

static int _get_file_parse_ptr(json_parse_t *parse_ptr, int read_offset, size_t read_size, char **sstr)
{
    size_t offset = 0;
    ssize_t size = 0, rsize = 0;

    *sstr = JSON_PARSE_ERROR_STR; /* Reduce the judgment pointer is NULL. */
    offset = parse_ptr->offset + read_offset;
    size = parse_ptr->readed - offset;

    if (offset + read_size <= parse_ptr->readed) {
        *sstr = parse_ptr->str + offset;
    } else if (parse_ptr->read_size == 0) {
        if (size >= 0)
            *sstr = parse_ptr->str + offset;
        else
            size = 0;
    } else {
        size = parse_ptr->readed - parse_ptr->offset;
        if (size && parse_ptr->offset)
            memmove(parse_ptr->str, parse_ptr->str + parse_ptr->offset, size);
        parse_ptr->offset = 0;
        parse_ptr->readed = size;

        if (read_size > parse_ptr->size - parse_ptr->readed) {
            while (read_size > parse_ptr->size - parse_ptr->readed)
                parse_ptr->size += parse_ptr->read_size;
            if ((parse_ptr->str = json_realloc(parse_ptr->str, parse_ptr->size + 1)) == NULL) {
                JsonErr("malloc failed!\n");
                return 0;
            }
        }

        size = parse_ptr->size - parse_ptr->readed;
        if ((rsize = read(parse_ptr->fd, parse_ptr->str + parse_ptr->readed, size)) != size)
            parse_ptr->read_size = 0; /* finish readding file */
        parse_ptr->readed += rsize < 0 ? 0 : rsize;
        parse_ptr->str[parse_ptr->readed] = '\0';

        offset = parse_ptr->offset + read_offset;
        *sstr = parse_ptr->str + offset;
        size = parse_ptr->readed - offset;
    }

    return size;
}

static int _get_str_parse_ptr(json_parse_t *parse_ptr, int read_offset, size_t read_size UNUSED_ATTR, char **sstr)
{
    size_t offset = 0;
    ssize_t size = 0;

    *sstr = JSON_PARSE_ERROR_STR; /* Reduce the judgment pointer is NULL. */
    offset = parse_ptr->offset + read_offset;
    size = parse_ptr->size - offset;

    if (size >= 0)
        *sstr = parse_ptr->str + offset;
    else
        size = 0;

    return size;
}

static inline int _get_parse_ptr(json_parse_t *parse_ptr, int read_offset, size_t read_size, char **sstr)
{
    return parse_ptr->getptr(parse_ptr, read_offset, read_size, sstr);
}

static inline void _update_parse_offset(json_parse_t *parse_ptr, int num)
{
    parse_ptr->offset += num;
}

static inline void _skip_blank(json_parse_t *parse_ptr)
{
    unsigned char *str = NULL;
    unsigned char c = 0;
    int cnt = 0;

    while (_get_parse_ptr(parse_ptr, cnt, 64, (char **)&str) != 0) {
        while ((c = *str)) {
            if (c <= ' ') {
                str++, ++cnt;
            } else {
                _update_parse_offset(parse_ptr, cnt);
                return;
            }
        }
    }
}

static inline bool _hex_char_check(char c)
{
    bool ret = false;

    switch (c) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
        ret = true;
        break;
    default:
        break;
    }

    return ret;
}

static inline bool _dec_char_check(char c)
{
    bool ret = false;

    switch (c) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        ret = true;
        break;
    default:
        break;
    }

    return ret;
}

static inline unsigned int _hex_char_calculate(char c)
{
    unsigned int ret = 0;

    switch (c) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        ret = c - '0';
        break;
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
        ret = 10 + c - 'a';
        break;
   case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
        ret = 10 + c - 'A';
        break;
    default:
        break;
    }

    return ret;
}

static int _parse_num(char **sstr, json_number_t *vnum)
{
    int sign = 1;
    unsigned long long int k = 1, m = 0, n = 0;
    double d = 0;
    char *num = *sstr;

    if (*num == '0' && (*(num+1) == 'x' || *(num+1) == 'X')) {
        num += 2;
        while (_hex_char_check(*num))
            m = (m << 4) + _hex_char_calculate(*num++);

        *sstr = num;
        if (m > UINT_MAX) {
#if JSON_LONG_LONG_SUPPORT
            vnum->vlhex = m;
            return JSON_LHEX;
#else
            vnum->vdbl = m;
            return JSON_DOUBLE;
#endif
        } else {
            vnum->vhex = m;
            return JSON_HEX;
        }
    } else {
        switch (*num) {
        case '-': num++; sign = -1; break;
        case '+': num++; break;
        default: break;
        }

        while (*num == '0')
            num++;
        while (_dec_char_check(*num))
            m = (m << 3) + (m << 1) + (*num++ - '0');

        if (*num == '.') {
            num++;
            while (_dec_char_check(*num)) {
                n = (n << 3) + (n << 1) + (*num++ - '0');
                k = (k << 3) + (k << 1);
            }
            d = m + 1.0 * n / k;

            *sstr = num;
            vnum->vdbl = sign == 1 ? d : -d;
            return JSON_DOUBLE;
        } else {
            *sstr = num;
            if (m > INT_MAX) {
#if JSON_LONG_LONG_SUPPORT
                vnum->vlint = sign == 1 ? m : -m;
                return JSON_LINT;
#else
                vnum->vdbl = sign == 1 ? m : -m;
                return JSON_DOUBLE;
#endif
            } else {
                vnum->vint = sign == 1 ? m : -m;
                return JSON_INT;
            }
        }
    }

    return JSON_NULL;
}

static json_type_t _json_parse_number(char **sstr, json_number_t *vnum)
{
    json_type_t ret = JSON_NULL;
    json_type_t tret = JSON_NULL;
    json_number_t tnum;
    double nbase = 0, nindex = 0;

    ret = _parse_num(sstr, vnum);
    switch (ret) {
    case JSON_HEX:
#if JSON_LONG_LONG_SUPPORT
    case JSON_LHEX:
#endif
        return ret;
    case JSON_INT:
#if JSON_LONG_LONG_SUPPORT
    case JSON_LINT:
#endif
    case JSON_DOUBLE:
        switch (**sstr) {
        case 'e': case 'E':
            *sstr += 1;
            switch (ret) {
            case JSON_INT:     nbase = vnum->vint;  break;
#if JSON_LONG_LONG_SUPPORT
            case JSON_LINT:    nbase = vnum->vlint; break;
#endif
            case JSON_DOUBLE:  nbase = vnum->vdbl;  break;
            default: break;
            }

            tret = _parse_num(sstr, &tnum);
            switch (tret) {
            case JSON_INT:    nindex = tnum.vint;   break;
#if JSON_LONG_LONG_SUPPORT
            case JSON_LINT:   nindex = tnum.vlint;  break;
#endif
            case JSON_DOUBLE: nindex = tnum.vdbl;   break;
            default: return JSON_NULL;
            }
            vnum->vdbl = nbase * pow (10.0, nindex);
            return JSON_DOUBLE;
        default:
            return ret;
        }
        break;
    default:
        return JSON_NULL;
    }
}

static inline unsigned int _parse_hex4(const unsigned char *str)
{
    int i = 0;
    unsigned int val = 0;

    for (i = 0; i < 4; ++i) {
        switch (str[i]) {
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            val += str[i] - '0';
            break;
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
            val += 10 + str[i] - 'a';
            break;
       case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
            val += 10 + str[i] - 'A';
            break;
        default:
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

static char *_parse_transform(char *str, char *end_str, char *ptr)
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
                   JsonErr("invalid utf16 code(\\u%c)!\n", str[2]);
                   return NULL;
               }
               break;
            default :
                JsonErr("invalid escape character(\\%c)!\n", str[1]);
                return NULL;
            }
            str += seq_len;
        }
    }
    *ptr = '\0';

    return ptr;
}

static int _parse_strlen(json_parse_t *parse_ptr, int *string_len, int *total_len)
{
    char *str = NULL;
    int len = 0, total = 0, rsize = 0, cnt = 0;

    _get_parse_ptr(parse_ptr, 0, 1, &str);
    if (*str != '\"')
        goto err;
    ++total;

    while ((rsize = _get_parse_ptr(parse_ptr, total, 128, &str))) {
        cnt = 0;
        while(*str && *str != '\"') {
            if (*str != '\\') {
                ++str, ++cnt, ++len;
            } else {
                if (rsize - cnt >= 2) {
                    str += 2, cnt += 2, ++len;
                } else {
                    break;
                }
            }
        }
        total += cnt;

        if (*str == '\"') {
            ++total;
            *string_len = len;
            *total_len = total;
            return 0;
        }
    }

err:
    JsonPareseErr("str format err!");
    return -1;
}

static int _json_parse_string(json_parse_t *parse_ptr, char **pptr, bool key_flag, json_mem_mgr_t *mgr)
{
    char *ptr = NULL, *str = NULL, *end_str = NULL;
    int len = 0, total = 0;

    *pptr = NULL;
    if (_parse_strlen(parse_ptr, &len, &total) < 0) {
        return -1;
    }

    _get_parse_ptr(parse_ptr, 0, total, &str);
    end_str = str + total - 1;
    str++;

    if (parse_ptr->reuse_flag) {
        ptr = str;
        if (len == total - 2) {
            ptr[len] = '\0';
        } else {
            if (_parse_transform(str, end_str, ptr) == NULL) {
                JsonErr("transform failed!\n");
                goto err;
            }
        }
    } else {
        if ((ptr = _parse_alloc(parse_ptr, len+1, mgr)) == NULL) {
            JsonErr("malloc failed!\n");
            return -1;
        }
        if (len == total - 2) {
            memcpy(ptr, str, len);
            ptr[len] = '\0';
        } else {
            if (_parse_transform(str, end_str, ptr) == NULL) {
                JsonErr("transform failed!\n");
                goto err;
            }
        }
    }
    _update_parse_offset(parse_ptr, total);

    if (key_flag) {
        if (len == 0) {
            JsonErr("key is space!\n");
            goto err;
        }
        _skip_blank(parse_ptr);
        _get_parse_ptr(parse_ptr, 0, 1, &str);
        if (*str != ':') {
            JsonErr("key is not ended with ':' (%c)!\n", *str);
            goto err;
        }
        _update_parse_offset(parse_ptr, 1);
    }

    *pptr = ptr;
    return 0;

err:
    if (parse_ptr->mem == &s_invalid_json_mem)
        json_free(ptr);
    JsonPareseErr("parse string failed!");
    return -1;
}

static int _json_parse_value(json_parse_t *parse_ptr, json_object **item, json_object *parent, char *key)
{
    json_object *out_item = NULL;
    json_object *new_item = NULL;
    char *key_str = NULL;
    char *str = NULL;

    if ((out_item = _parse_alloc(parse_ptr, sizeof(json_object), &parse_ptr->mem->obj_mgr)) == NULL) {
        JsonErr("malloc failed!\n");
        goto err;
    }
    out_item->key = NULL;

    _skip_blank(parse_ptr);
    _get_parse_ptr(parse_ptr, 0, 8, &str);

    switch(*str) {
    case '\"':
        {
            char *vstr = NULL;
            out_item->type = JSON_STRING;
            if (_json_parse_string(parse_ptr, &vstr, false, &parse_ptr->mem->str_mgr) < 0) {
                goto err;
            }
            out_item->value.vstr = vstr;
            break;
        }

    case '-': case '+':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        {
            json_type_t nret;
            json_number_t vnum;
            char *str_bak = NULL;
            _get_parse_ptr(parse_ptr, 0, 128, &str);
            str_bak = str;
            nret = _json_parse_number(&str, &vnum);
            out_item->type = nret;
            switch (nret) {
            case JSON_INT:    out_item->value.vnum.vint  = vnum.vint;  break;
            case JSON_HEX:    out_item->value.vnum.vhex  = vnum.vhex;  break;
#if JSON_LONG_LONG_SUPPORT
            case JSON_LINT:   out_item->value.vnum.vlint = vnum.vlint; break;
            case JSON_LHEX:   out_item->value.vnum.vlhex = vnum.vlhex; break;
#endif
            case JSON_DOUBLE: out_item->value.vnum.vdbl  = vnum.vdbl;  break;
            default:          JsonPareseErr("Not number!");         goto err;
            }
            _update_parse_offset(parse_ptr, str-str_bak);
            break;
        }

    case '{':
        {
            out_item->type = JSON_OBJECT;
            INIT_JSON_LIST_HEAD(&out_item->value.head);
            _update_parse_offset(parse_ptr, 1);

            _skip_blank(parse_ptr);
            _get_parse_ptr(parse_ptr, 0, 1, &str);
            if (*str != '}') {
                while (1) {
                    _skip_blank(parse_ptr);
                    if (_json_parse_string(parse_ptr, &key_str, true, &parse_ptr->mem->key_mgr) < 0) {
                        goto err;
                    }
                    if (_json_parse_value(parse_ptr, &new_item, out_item, key_str) < 0) {
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
                        JsonPareseErr("invalid object!");
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
            INIT_JSON_LIST_HEAD(&out_item->value.head);
            _update_parse_offset(parse_ptr, 1);

            _skip_blank(parse_ptr);
            _get_parse_ptr(parse_ptr, 0, 1, &str);
            if (*str != ']') {
                while (1) {
                    if (_json_parse_value(parse_ptr, &new_item, out_item, NULL) < 0) {
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
                        JsonPareseErr("invalid array!");
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
                out_item->value.vnum.vbool = false;
                _update_parse_offset(parse_ptr, 5);
            } else if (strncmp(str, "true", 4) == 0) {
                out_item->type = JSON_BOOL;
                out_item->value.vnum.vbool = true;
                _update_parse_offset(parse_ptr, 4);
            } else if (strncmp(str, "null", 4) == 0) {
                out_item->type = JSON_NULL;
                _update_parse_offset(parse_ptr, 4);
            } else {
                JsonPareseErr("invalid next ptr!");
                goto err;
            }
            break;
        }
    }

end:
    if (parent) {
        out_item->key = key; // parent object is array, key is NULL.
        json_list_add_tail(&out_item->list, &parent->value.head);
    }

    *item = out_item;
    return 0;
err:
    if (parse_ptr->mem == &s_invalid_json_mem) {
        if (out_item)
            json_free(out_item);
        if (key)
            json_free(key);
    }
    return -1;
}

json_object *json_parse_common(json_parse_choice_t *choice)
{
    json_object *json = NULL;
    json_parse_t parse_val = {0};
    size_t mem_size = 0, total_size = 0;

    parse_val.read_size = parse_val.read_size ? parse_val.read_size : JSON_PARSE_READ_SIZE_DEF;
    parse_val.mem = choice->mem ? choice->mem : &s_invalid_json_mem;
    parse_val.fd = -1;
    if (choice->path) {
        parse_val.getptr = _get_file_parse_ptr;
        if ((parse_val.fd = open(choice->path, O_RDONLY)) < 0) {
            JsonErr("open(%s) failed!\n", choice->path);
            return NULL;
        }
        if (choice->str_len) {
            total_size = choice->str_len;
        } else {
            total_size = lseek(parse_val.fd, 0, SEEK_END);
            lseek(parse_val.fd, 0, SEEK_SET);
        }
    } else {
        parse_val.getptr = _get_str_parse_ptr;
        parse_val.str = choice->str;
        total_size = choice->str_len ? choice->str_len : strlen(choice->str);
        parse_val.size = total_size;
        if (choice->mem)
            parse_val.reuse_flag = choice->reuse_flag;
    }

    if (choice->mem) {
        parse_val.alloc = pjson_memory_alloc;
        pjson_memory_init(choice->mem);
        mem_size = total_size / JSON_PARSE_NUM_PLUS_DEF;
        if (mem_size < choice->mem_size)
            mem_size = choice->mem_size;
        choice->mem->obj_mgr.mem_size = mem_size;
        choice->mem->key_mgr.mem_size = mem_size;
        choice->mem->str_mgr.mem_size = mem_size;
    } else {
        parse_val.alloc = json_memory_alloc;
    }

    if (_json_parse_value(&parse_val, &json, NULL, NULL) < 0) {
        if (choice->mem) {
            pjson_memory_free(choice->mem);
        } else {
            json_del_object(json);
        }
        json = NULL;
    }

    if (parse_val.fd >= 0)
        close(parse_val.fd);
    return json;
}

#if JSON_SAX_APIS_SUPPORT
static inline bool _json_sax_parse_check_stop(json_parse_t *parse_ptr)
{
    if (parse_ptr->ret == JSON_SAX_PARSE_STOP) {
        parse_ptr->parser.value.vcmd = JSON_SAX_FINISH;
        parse_ptr->cb(&parse_ptr->parser);
        return true;
    }
    return false;
}

static inline int _json_sax_parse_string(json_parse_t *parse_ptr, json_sax_str_t *data, bool key_flag)
{
    char *ptr = NULL, *str = NULL, *end_str = NULL;
    int len = 0, total = 0;

    if (_parse_strlen(parse_ptr, &len, &total) < 0) {
        return -1;
    }

    _get_parse_ptr(parse_ptr, 0, total, &str);
    end_str = str + total - 1;
    str++;

    if (len == total - 2 && !(parse_ptr->fd >= 0 && key_flag)) {
        data->alloc = 0;
        data->str = str;
        data->len = len;
    } else {
        if ((ptr = json_malloc(len+1)) == NULL) {
            JsonErr("malloc failed!\n");
            return -1;
        }
        data->alloc = 1;
        data->str = ptr;
        data->len = len;
        if (_parse_transform(str, end_str, ptr) == NULL) {
            JsonErr("transform failed!\n");
            goto err;
        }
    }
    _update_parse_offset(parse_ptr, total);

    if (key_flag) {
        if (len == 0) {
            JsonErr("key is space!\n");
            goto err;
        }
        _skip_blank(parse_ptr);
        _get_parse_ptr(parse_ptr, 0, 1, &str);
        if (*str != ':') {
            JsonErr("key is not ended with ':' (%c)!\n", *str);
            goto err;
        }
        _update_parse_offset(parse_ptr, 1);
    }

    return 0;
err:
    if (data->alloc && data->str) {
        json_free(data->str);
    }
    memset(data, 0, sizeof(json_sax_str_t));

    JsonPareseErr("parse string failed!");
    return -1;
}

static int _json_sax_parse_value(json_parse_t *parse_ptr, json_sax_str_t *key)
{
    int ret = -1;
    int i = 0;
    json_sax_depth_t *tmp_array = NULL;
    int cur_depth = 0;

    char *str = NULL;
    json_sax_str_t vstr = {0};
    json_sax_str_t key_str = {0};

    // increase depth
    if (parse_ptr->parser.count == parse_ptr->parser.total) {
        parse_ptr->parser.total += JSON_PARSE_NUM_PLUS_DEF;
        if ((tmp_array = json_malloc(parse_ptr->parser.total * sizeof(json_sax_depth_t))) == NULL) {
            for (i = 0; i < parse_ptr->parser.count; ++i) {
                if (parse_ptr->parser.array[i].key.alloc == 1 && parse_ptr->parser.array[i].key.str) {
                    json_free(parse_ptr->parser.array[i].key.str);
                }
            }
            parse_ptr->parser.array = NULL;
            if (key->alloc == 1) {
                json_free(key->str);
            }
            JsonErr("malloc failed!\n");
            return -1;
        }
        memcpy(tmp_array, parse_ptr->parser.array, parse_ptr->parser.count * sizeof(json_sax_depth_t));
        json_free(parse_ptr->parser.array);
        parse_ptr->parser.array = tmp_array;
    }
    cur_depth = parse_ptr->parser.count++;
    memcpy(&parse_ptr->parser.array[cur_depth].key, key, sizeof(json_sax_str_t));

    // parse value
    _skip_blank(parse_ptr);
    _get_parse_ptr(parse_ptr, 0, 8, &str);
    switch(*str) {
    case '\"':
        {
            parse_ptr->parser.array[cur_depth].type = JSON_STRING;
            if (_json_sax_parse_string(parse_ptr, &vstr, false) < 0) {
                goto end;
            }
            memcpy(&parse_ptr->parser.value.vstr, &vstr, sizeof(json_sax_str_t));
            break;
        }

    case '-': case '+':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        {
            json_type_t nret;
            json_number_t vnum;
            char *str_bak = NULL;
            _get_parse_ptr(parse_ptr, 0, 128, &str);
            str_bak = str;
            nret = _json_parse_number(&str, &vnum);
            parse_ptr->parser.array[cur_depth].type = nret;
            switch (nret) {
            case JSON_INT:    parse_ptr->parser.value.vnum.vint  = vnum.vint;  break;
            case JSON_HEX:    parse_ptr->parser.value.vnum.vhex  = vnum.vhex;  break;
#if JSON_LONG_LONG_SUPPORT
            case JSON_LINT:   parse_ptr->parser.value.vnum.vlint = vnum.vlint; break;
            case JSON_LHEX:   parse_ptr->parser.value.vnum.vlhex = vnum.vlhex; break;
#endif
            case JSON_DOUBLE: parse_ptr->parser.value.vnum.vdbl  = vnum.vdbl;  break;
            default:          JsonPareseErr("Not number!");                 goto end;
            }
            _update_parse_offset(parse_ptr, str-str_bak);
            break;
        }

    case '{':
        {
            parse_ptr->parser.array[cur_depth].type = JSON_OBJECT;
            parse_ptr->parser.value.vcmd = JSON_SAX_START;
            parse_ptr->ret = parse_ptr->cb(&parse_ptr->parser);
            _update_parse_offset(parse_ptr, 1);
            if (_json_sax_parse_check_stop(parse_ptr)) {
                ret = 0;
                goto end;
            }

            _skip_blank(parse_ptr);
            _get_parse_ptr(parse_ptr, 0, 1, &str);
            if (*str != '}') {
                while (1) {
                    _skip_blank(parse_ptr);
                    if (_json_sax_parse_string(parse_ptr, &key_str, true) < 0) {
                        goto end;
                    }
                    if (_json_sax_parse_value(parse_ptr, &key_str) < 0) {
                        goto end;
                    }
                    if (_json_sax_parse_check_stop(parse_ptr)) {
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
                        JsonPareseErr("invalid object!");
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
            parse_ptr->ret = parse_ptr->cb(&parse_ptr->parser);
            _update_parse_offset(parse_ptr, 1);
            if (_json_sax_parse_check_stop(parse_ptr)) {
                ret = 0;
                goto end;
            }

            _skip_blank(parse_ptr);
            _get_parse_ptr(parse_ptr, 0, 1, &str);
            if (*str != ']') {
                while (1) {
                    if (_json_sax_parse_value(parse_ptr, &key_str) < 0) {
                        goto end;
                    }
                    if (_json_sax_parse_check_stop(parse_ptr)) {
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
                        JsonPareseErr("invalid array!");
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
                parse_ptr->parser.value.vnum.vbool = false;
                _update_parse_offset(parse_ptr, 5);
            } else if (strncmp(str, "true", 4) == 0) {
                parse_ptr->parser.array[cur_depth].type = JSON_BOOL;
                parse_ptr->parser.value.vnum.vbool = true;
                _update_parse_offset(parse_ptr, 4);
            } else if (strncmp(str, "null", 4) == 0) {
                parse_ptr->parser.array[cur_depth].type = JSON_NULL;
                _update_parse_offset(parse_ptr, 4);
            } else {
                JsonPareseErr("invalid next ptr!");
                goto end;
            }
            break;
        }
    }

success:
    parse_ptr->ret = parse_ptr->cb(&parse_ptr->parser);
    ret = 0;

end:
    if (parse_ptr->parser.array[cur_depth].type == JSON_STRING
        && parse_ptr->parser.value.vstr.alloc == 1 && parse_ptr->parser.value.vstr.str) {
        json_free(parse_ptr->parser.value.vstr.str);
    }
    memset(&parse_ptr->parser.value, 0, sizeof(parse_ptr->parser.value));

    if (parse_ptr->parser.array[cur_depth].key.alloc == 1) {
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
    json_sax_str_t key_str = {0};

    parse_val.read_size = parse_val.read_size ? parse_val.read_size : JSON_PARSE_READ_SIZE_DEF;
    parse_val.mem = NULL;
    parse_val.alloc = json_memory_alloc;
    parse_val.fd = -1;
    if (choice->path) {
        parse_val.getptr = _get_file_parse_ptr;
        if ((parse_val.fd = open(choice->path, O_RDONLY)) < 0) {
            JsonErr("open(%s) failed!\n", choice->path);
            return -1;
        }
    } else {
        parse_val.getptr = _get_str_parse_ptr;
        parse_val.str = choice->str;
        parse_val.size = choice->str_len ? choice->str_len : strlen(choice->str);
    }

    if ((parse_val.parser.array = json_malloc(JSON_PARSE_NUM_PLUS_DEF * sizeof(json_sax_depth_t))) == NULL) {
        JsonErr("malloc failed!\n");
        goto end;
    }
    parse_val.parser.total = JSON_PARSE_NUM_PLUS_DEF;
    parse_val.cb = choice->cb;

    ret = _json_sax_parse_value(&parse_val, &key_str);

end:
    if (parse_val.parser.array) {
        for (i = 0; i < parse_val.parser.count; ++i) {
            if (parse_val.parser.array[i].key.alloc == 1 && parse_val.parser.array[i].key.str) {
                json_free(parse_val.parser.array[i].key.str);
            }
        }
        json_free(parse_val.parser.array);
    }
    if (parse_val.fd >= 0)
        close(parse_val.fd);

    return ret;
}
#endif
