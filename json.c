/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2019-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* URL: https://github.com/lengjingzju/json *
*******************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include "json.h"

#define type_member                     jkey.type
#define key_member                      jkey.str
#define str_member                      vstr.str

/**************** debug ****************/

/*
 * JSON_STRICT_PARSE_MODE can be: 0 / 1 / 2
 * 0: not strict mode
 * 1: will enable more checks
 * 2: further turns off some features that are not standard, such as hexadecimal
 */
#define JSON_STRICT_PARSE_MODE          1

/* error print */
#define JSON_ERROR_PRINT_ENABLE         0

#if JSON_ERROR_PRINT_ENABLE
#define JsonErr(fmt, args...) do {                                  \
    printf("[JsonErr][%s:%d] ", __func__, __LINE__);                \
    printf(fmt, ##args);                                            \
} while(0)

#define JsonPareseErr(s)      do {                                  \
    if (parse_ptr->str) {                                           \
        char ptmp[32] = {0};                                        \
        strncpy(ptmp, parse_ptr->str + parse_ptr->offset, 31);      \
        JsonErr("%s::%s\n", s, ptmp);                               \
    } else {                                                        \
        JsonErr("%s\n", s);                                         \
    }                                                               \
} while(0)

#else
#define JsonErr(fmt, args...) do {} while(0)
#define JsonPareseErr(s)      do {} while(0)
#endif

/**************** gcc builtin ****************/

#if defined(__GNUC__)
#define UNUSED_ATTR                     __attribute__((unused))
#define likely(cond)                    __builtin_expect(!!(cond), 1)
#define unlikely(cond)                  __builtin_expect(!!(cond), 0)
#else
#define UNUSED_ATTR
#define likely(cond)                    (cond)
#define unlikely(cond)                  (cond)
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
        &pos->member != (struct json_list *)(head);             \
        pos = json_list_entry(pos->member.next, typeof(*pos)))

#define json_list_for_each_entry_safe(p, pos, n, head, member)  \
    for (p = json_list_entry((head), typeof(*pos)),             \
        pos = json_list_entry((head)->next, typeof(*pos)),      \
        n = json_list_entry(pos->member.next, typeof(*pos));    \
        &pos->member != (struct json_list *)(head);             \
        p = pos, pos = n, n = json_list_entry(n->member.next, typeof(*n)))


static inline void INIT_JSON_LIST_HEAD(struct json_list_head *head)
{
    head->next = (struct json_list *)head;
    head->prev = (struct json_list *)head;
}

static inline void json_list_add_head(struct json_list *list, struct json_list_head *head)
{
    if (head->next == (struct json_list *)head) {
        head->prev = list;
    }
    list->next = head->next;
    head->next = list;
}

static inline void json_list_add_tail(struct json_list *list, struct json_list_head *head)
{
    list->next = head->prev->next;
    head->prev->next = list;
    head->prev = list;
}

static inline void json_list_add(struct json_list *list, struct json_list *prev, struct json_list_head *head)
{
    if (prev->next == (struct json_list *)head) {
        head->prev = list;
    }
    list->next = prev->next;
    prev->next = list;
}

static inline void json_list_del(struct json_list *list, struct json_list *prev, struct json_list_head *head)
{
    if (list->next == (struct json_list *)head) {
        head->prev = prev;
    }
    prev->next = list->next;
    list->next = NULL;
}

/**************** json normal apis ****************/

void json_memory_free(void *ptr)
{
    if (ptr)
        json_free(ptr);
}

int json_item_total_get(json_object *json)
{
    int cnt = 0;
    json_object *item = NULL;

    if (!json)
        return 0;

    switch (json->type_member) {
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
    json_object *p = NULL, *pos = NULL, *n = NULL;

    if (!json)
        return;

    if (json->key_member)
        json_free(json->key_member);

    switch (json->type_member) {
    case JSON_STRING:
        if (json->value.str_member)
            json_free(json->value.str_member);
        break;
    case JSON_ARRAY:
    case JSON_OBJECT:
        json_list_for_each_entry_safe(p, pos, n, &json->value.head, list) {
            json_list_del(&pos->list, &p->list, &json->value.head);
            json_del_object(pos);
            pos = p;
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

    json->type_member = type;
    switch (type) {
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

    json->type_member = type;
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
        if (json_set_string_value(json, (json_string_t *)value) < 0)
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
        case JSON_STRING: value = ((json_string_t *)values) + i;         break;
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

void json_string_info_update(json_string_t *jstr)
{
    const char *str = jstr->str;
    int i = 0;
    int escaped = 0;

    if (jstr->len)
        return;

    if (str) {
        for (i = 0; str[i]; ++i) {
            switch (str[i]) {
            case '\"': case '\\': case '\b': case '\f': case '\n': case '\r': case '\t': case '\v':
                escaped = 1;
                break;
            default:
#if JSON_PRINT_UTF16_SUPPORT
                {
                    if ((unsigned char)str[i] < ' ')
                        escaped = 1;
                }
#endif
                break;
            }
        }
    }

    jstr->escaped = escaped;
    jstr->len = i;
}

int json_string_strdup(json_string_t *src, json_string_t *dst)
{
    json_string_info_update(src);
    json_string_info_update(dst);
    if (src->len) {
        if (dst->len < src->len) {
            if ((dst->str = json_realloc(dst->str, src->len + 1)) == NULL) {
                JsonErr("malloc failed!\n");
                dst->len = 0;
                return -1;
            }
        }
        dst->escaped = src->escaped;
        dst->len = src->len;
        memcpy(dst->str, src->str, src->len);
        dst->str[src->len] = '\0';
    } else {
        dst->escaped = 0;
        dst->len = 0;
        if (dst->str) {
            json_free(dst->str);
            dst->str = NULL;
        }
    }

    return 0;
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
    switch (json->type_member) {                                                \
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

    return json->type_member == type ? 0 : json->type_member;
}

int json_set_number_value(json_object *json, json_type_t type, void *value)
{
    int ret = 0;

    switch (json->type_member) {
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

    if (json->type_member != type) {
        ret = json->type_member;
        json->type_member = type;
    }

    return ret;
}

int json_get_array_size(json_object *json)
{
    int count = 0;
    json_object *pos = NULL;

    if (json->type_member == JSON_ARRAY) {
        json_list_for_each_entry(pos, &json->value.head, list) {
            ++count;
        }
    }

    return count;
}

json_object *json_get_array_item(json_object *json, int seq, json_object **prev)
{
    int count = 0;
    json_object *p = NULL, *pos = NULL, *n = NULL;

    if (json->type_member == JSON_ARRAY) {
        json_list_for_each_entry_safe(p, pos, n, &json->value.head, list) {
            if (count++ == seq) {
                if (prev)
                    *prev = p;
                return pos;
            }
        }
    }

    return NULL;
}

json_object *json_get_object_item(json_object *json, const char *key, json_object **prev)
{
    json_object *p = NULL, *pos = NULL, *n = NULL;

    if (json->type_member == JSON_OBJECT) {
        if (key && key[0]) {
            json_list_for_each_entry_safe(p, pos, n, &json->value.head, list) {
                if (pos->key_member && strcmp(key, pos->key_member) == 0) {
                    if (prev)
                        *prev = p;
                    return pos;
                }
            }
        } else {
            json_list_for_each_entry_safe(p, pos, n, &json->value.head, list) {
                if (!pos->jkey.len) {
                    if (prev)
                        *prev = p;
                    return pos;
                }
            }
        }
    }

    return NULL;
}

json_object *json_detach_item_from_array(json_object *json, int seq)
{
    json_object *item = NULL, *prev = NULL;

    if ((item = json_get_array_item(json, seq, &prev)) == NULL)
        return NULL;
    json_list_del(&item->list, &prev->list, &json->value.head);

    return item;
}

json_object *json_detach_item_from_object(json_object *json, const char *key)
{
    json_object *item = NULL, *prev = NULL;

    if ((item = json_get_object_item(json, key, &prev)) == NULL)
        return NULL;
    json_list_del(&item->list, &prev->list, &json->value.head);

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
    json_object *item = NULL, *prev = NULL;

    if (array->type_member == JSON_ARRAY) {
        if ((item = json_get_array_item(array, seq, &prev)) != NULL) {
            json_list_del(&item->list, &prev->list, &array->value.head);
            json_del_object(item);
            json_list_add(&new_item->list, &prev->list, &array->value.head);
        } else {
            json_list_add_tail(&new_item->list, &array->value.head);
        }

        return 0;
    }

    return -1;
}

int json_replace_item_in_object(json_object *object, json_string_t *jkey, json_object *new_item)
{
    json_object *item = NULL, *prev = NULL;

    if (object->type_member == JSON_OBJECT) {
        if (json_set_key(new_item, jkey) < 0) {
            return -1;
        }
        if ((item = json_get_object_item(object, jkey->str, &prev)) != NULL) {
            json_list_del(&item->list, &prev->list, &object->value.head);
            json_del_object(item);
            json_list_add(&new_item->list, &prev->list, &object->value.head);
        } else {
            json_list_add_tail(&new_item->list, &object->value.head);
        }

        return 0;
    }

    return -1;
}

int json_add_item_to_array(json_object *array, json_object *item)
{
    if (array->type_member == JSON_ARRAY) {
        json_list_add_tail(&item->list, &array->value.head);
        return 0;
    }
    return -1;
}

int json_add_item_to_object(json_object *object, json_string_t *jkey, json_object *item)
{
    if (object->type_member == JSON_OBJECT) {
        if (json_set_key(item, jkey) < 0) {
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

    switch (json->type_member) {
    case JSON_NULL:   new_json = json_create_null();                        break;
    case JSON_BOOL:   new_json = json_create_bool(json->value.vnum.vbool);  break;
    case JSON_INT:    new_json = json_create_int(json->value.vnum.vint);    break;
    case JSON_HEX:    new_json = json_create_hex(json->value.vnum.vhex);    break;
#if JSON_LONG_LONG_SUPPORT
    case JSON_LINT:   new_json = json_create_lint(json->value.vnum.vlint);  break;
    case JSON_LHEX:   new_json = json_create_lhex(json->value.vnum.vlhex);  break;
#endif
    case JSON_DOUBLE: new_json = json_create_double(json->value.vnum.vdbl); break;
    case JSON_STRING: new_json = json_create_string(&json->value.vstr);     break;
    case JSON_ARRAY:  new_json = json_create_array();                       break;
    case JSON_OBJECT: new_json = json_create_object();                      break;
    default:                                                                break;
    }

    if (new_json) {
        if (json_string_strdup(&json->jkey, &new_json->jkey) < 0) {
            JsonErr("add key failed!\n");
            json_del_object(new_json);
            return NULL;
        }
        if (json->type_member == JSON_ARRAY || json->type_member == JSON_OBJECT) {
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

    if (array->type_member == JSON_ARRAY) {
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

int json_copy_item_to_object(json_object *object, json_string_t *jkey, json_object *item)
{
    json_object *node = NULL;

    if (object->type_member == JSON_OBJECT) {
        if ((node = json_deepcopy(item)) == NULL) {
            JsonErr("copy failed!\n");
            return -1;
        }
        if (json_add_item_to_object(object, jkey, node) < 0) {
            JsonErr("add failed!\n");
            json_del_object(node);
            return -1;
        }
        return 0;
    }

    return -1;
}

int json_add_new_item_to_object(json_object *object, json_type_t type, json_string_t *jkey, void *value)
{
    json_object *item = NULL;

    if (object->type_member == JSON_OBJECT) {
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
            if (json_set_key(item, jkey) < 0) {
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
    json_mem_node_t *p = NULL, *pos = NULL, *n = NULL;
    json_list_for_each_entry_safe(p, pos, n, &mgr->head, list) {
        json_list_del(&pos->list, &p->list, &mgr->head);
        json_free(pos->ptr);
        json_free(pos);
        pos = p;
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
    json_list_add_head(&node->list, &mgr->head);

    return node;
}

static inline void *pjson_memory_alloc(size_t size, json_mem_mgr_t *mgr)
{
    void *p = NULL;

    if ((mgr->cur_node->cur - mgr->cur_node->ptr) + size > mgr->cur_node->size) {
        if ((_json_mem_new((mgr->mem_size >= size) ? mgr->mem_size : size, mgr) == NULL)) {
            return NULL;
        }
        mgr->cur_node = (json_mem_node_t *)(mgr->head.next);
    }

    p = mgr->cur_node->cur;
    mgr->cur_node->cur += size;
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
    json->type_member = type;

    switch (type) {
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
    json->type_member = type;

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
        if (pjson_set_string_value(json, (json_string_t *)value, mem) < 0)
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
        case JSON_STRING: value = ((json_string_t *)values) + i;          break;
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

int pjson_string_strdup(json_string_t *src, json_string_t *dst, json_mem_mgr_t *mgr)
{
    json_string_info_update(src);
    json_string_info_update(dst);
    if (src->len) {
        if (dst->len < src->len) {
            if ((dst->str = pjson_memory_alloc(src->len + 1, mgr)) == NULL) {
                JsonErr("malloc failed!\n");
                dst->len = 0;
                return -1;
            }
        }
        dst->escaped = src->escaped;
        dst->len = src->len;
        memcpy(dst->str, src->str, src->len);
        dst->str[src->len] = '\0';
    } else {
        dst->escaped = 0;
        dst->len = 0;
        dst->str = NULL;
    }

    return 0;
}

int pjson_add_item_to_object(json_object *object, json_string_t *jkey, json_object *item, json_mem_t *mem)
{
    if (object->type_member == JSON_OBJECT) {
        if (pjson_set_key(item, jkey, mem) < 0) {
            JsonErr("add key failed!\n");
            return -1;
        }
        json_list_add_tail(&item->list, &object->value.head);
        return 0;
    }

    return -1;
}

int pjson_add_new_item_to_object(json_object *object, json_type_t type, json_string_t *jkey, void *value, json_mem_t *mem)
{
    json_object *item = NULL;

    if (object->type_member == JSON_OBJECT) {
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
            if (pjson_set_key(item, jkey, mem) < 0) {
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
    char *ptr;
    char *cur;
    int (*realloc)(struct _json_print_t *print_ptr, size_t slen);
    size_t size;

    size_t plus_size;
    size_t item_size;
    int item_total;
    int item_count;
    bool format_flag;
} json_print_t;

#define GET_BUF_USED_SIZE(bp) ((bp)->cur - (bp)->ptr)
#define GET_BUF_FREE_SIZE(bp) ((bp)->size - ((bp)->cur - (bp)->ptr))

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
        t = m;                              \
        m /= 10;                            \
        t -= (m << 3) + (m << 1);           \
        s[i++] = t + '0';                   \
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

    if (n < 0) {
        z = -n;
        y = -y;
    }

    /* 16 is equal to MB_LEN_MAX */
    if (z > LLONG_MAX || z <= 1e-16) {
        return sprintf(c, "%1.15g", n);
    }

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
    while (y) {
        z *= 10;
        x = (long long int)z;
        y = z - x;
        s[i++] = x % 10 + '0';
        if (unlikely(i >= MB_LEN_MAX)) {
            return sprintf(c, "%1.15g", n);
        }
    }
    s[i] = '\0';

    return i + a;
}

static int _print_file_ptr_realloc(json_print_t *print_ptr, size_t slen)
{
    size_t len = GET_BUF_USED_SIZE(print_ptr);

    if (len > 0) {
        if (len != (size_t)write(print_ptr->fd, print_ptr->ptr, len)) {
            JsonErr("write failed!\n");
            return -1;
        }
        print_ptr->cur = print_ptr->ptr;
    }

    len = slen + 1;
    if (print_ptr->size < len) {
        while (print_ptr->size < len)
            print_ptr->size += print_ptr->plus_size;
        if ((print_ptr->ptr = json_realloc(print_ptr->ptr, print_ptr->size)) == NULL) {
            JsonErr("malloc failed!\n");
            print_ptr->cur = print_ptr->ptr;
            return -1;
        }
        print_ptr->cur = print_ptr->ptr;
    }

    return 0;
}

static int _print_str_ptr_realloc(json_print_t *print_ptr, size_t slen)
{
    size_t used = GET_BUF_USED_SIZE(print_ptr);
    size_t len = used + slen + 1;

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
        print_ptr->cur = print_ptr->ptr;
        return -1;
    }
    print_ptr->cur = print_ptr->ptr + used;

    return 0;
}

#define _PRINT_PTR_REALLOC(nz) do {                 \
    if (unlikely(GET_BUF_FREE_SIZE(print_ptr) < nz  \
        && print_ptr->realloc(print_ptr, nz) < 0))  \
        goto err;                                   \
} while(0)

#define _PRINT_PTR_NUMBER(fname, num) do {          \
    _PRINT_PTR_REALLOC(129);                        \
    print_ptr->cur += fname(num, print_ptr->cur);   \
} while(0)

static inline int _print_ptr_strncat(json_print_t *print_ptr, const char *str, size_t slen)
{
    _PRINT_PTR_REALLOC((slen + 1));
    memcpy(print_ptr->cur, str, slen);
    print_ptr->cur += slen;

    return 0;
err:
    return -1;
}
#define _PRINT_PTR_STRNCAT(ptr, str, slen) do { if (unlikely(_print_ptr_strncat(ptr, str, slen) < 0)) goto err; } while(0)

static inline int _print_addi_format(json_print_t *print_ptr, size_t depth)
{
    _PRINT_PTR_REALLOC((depth + 2));
    *print_ptr->cur++ = '\n';
    memset(print_ptr->cur, '\t', depth);
    print_ptr->cur += depth;

    return 0;
err:
    return -1;
}
#define _PRINT_ADDI_FORMAT(ptr, depth) do { if (unlikely(_print_addi_format(ptr, depth) < 0)) goto err; } while(0)

static int _json_print_string(json_print_t *print_ptr, json_string_t *jstr)
{
#define _JSON_PRINT_SEGMENT()     do {  \
    size = str - bak - 1;               \
    memcpy(print_ptr->cur, bak, size);  \
    print_ptr->cur += size;             \
    bak = str;                          \
} while(0)

    char ch = '\0';
    size_t len = 0, size = 0, alloced = 0;
    const char *str = NULL, *bak = NULL;

    str = jstr->str;
    len = jstr->len;
    if (likely(!jstr->escaped)) {
        alloced = len + 3;
        _PRINT_PTR_REALLOC(alloced);
        *print_ptr->cur++ = '\"';
        memcpy(print_ptr->cur, str, len);
        print_ptr->cur += len;
        *print_ptr->cur++ = '\"';
        return 0;
    }

#if JSON_PRINT_UTF16_SUPPORT
    alloced = (len << 1) + 3;
#else
    alloced = (len << 2) + (len << 1) + 3;
#endif
    _PRINT_PTR_REALLOC(alloced);
    *print_ptr->cur++ = '\"';

    bak = str;
    while (1) {
        switch ((*str++)) {
        case '\"': ch = '\"'; break;
        case '\\': ch = '\\'; break;
        case '\b': ch = 'b' ; break;
        case '\f': ch = 'f' ; break;
        case '\n': ch = 'n' ; break;
        case '\r': ch = 'r' ; break;
        case '\t': ch = 't' ; break;
        case '\v': ch = 'v' ; break;
        case '\0':
            _JSON_PRINT_SEGMENT();
            *print_ptr->cur++ = '\"';
            return 0;
        default:
#if JSON_PRINT_UTF16_SUPPORT
            {
                unsigned char uc = (unsigned char)(*(str-1));
                if (uc < ' ') {
                    _JSON_PRINT_SEGMENT();
                    *print_ptr->cur++ = '\\';
                    *print_ptr->cur++ = 'u';
                    *print_ptr->cur++ = '0';
                    *print_ptr->cur++ = '0';
                    *print_ptr->cur++ = hex_array[uc >> 4 & 0xf];
                    *print_ptr->cur++ = hex_array[uc & 0xf];
                }
            }
#endif
            continue;
        }

        _JSON_PRINT_SEGMENT();
        *print_ptr->cur++ = '\\';
        *print_ptr->cur++ = ch;
    }

err:
    JsonErr("malloc failed!\n");
    return -1;
}
#define _JSON_PRINT_STRING(ptr, val) do { if (unlikely(_json_print_string(ptr, val) < 0)) goto err; } while(0)

static int _json_print(json_print_t *print_ptr, json_object *json)
{
    json_object *parent = NULL, *tmp = NULL;
    json_object **item_array = NULL;
    int item_depth = -1, item_total = 0;

    goto next3;
next1:
    if (unlikely(item_depth >= item_total - 1)) {
        item_total += 16;
        if (unlikely((item_array = json_realloc(item_array, sizeof(json_object *) * item_total)) == NULL)) {
            JsonErr("malloc failed!\n");
            goto err;
        }
    }
    item_array[++item_depth] = json;
    parent = json;
    json = (json_object *)json->value.head.next;
    if (parent->type_member == JSON_ARRAY) {
        if (print_ptr->format_flag) {
            if (json->type_member == JSON_OBJECT || json->type_member == JSON_ARRAY)
                _PRINT_ADDI_FORMAT(print_ptr, item_depth + 1);
        }
        goto next3;
    }

next2:
    if (print_ptr->format_flag) {
        _PRINT_ADDI_FORMAT(print_ptr, item_depth + 1);
        if (unlikely(!json->jkey.len)) {
#if !JSON_STRICT_PARSE_MODE
            JsonErr("key is null!\n");
            goto err;
#else
            _PRINT_PTR_STRNCAT(print_ptr, "\"\":\t", 4);
#endif
        } else {
            _JSON_PRINT_STRING(print_ptr, &json->jkey);
            _PRINT_PTR_STRNCAT(print_ptr, ":\t", 2);
        }
    } else {
        if (unlikely(!json->jkey.len)) {
#if !JSON_STRICT_PARSE_MODE
            JsonErr("key is null!\n");
            goto err;
#else
            _PRINT_PTR_STRNCAT(print_ptr, "\"\":", 3);
#endif
        } else {
            _JSON_PRINT_STRING(print_ptr, &json->jkey);
            _PRINT_PTR_STRNCAT(print_ptr, ":", 1);
        }
    }

next3:
    switch (json->type_member) {
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
        _PRINT_PTR_NUMBER(int_to_string, json->value.vnum.vint);
        break;
    case JSON_HEX:
        _PRINT_PTR_NUMBER(hex_to_string, json->value.vnum.vhex);
        break;
#if JSON_LONG_LONG_SUPPORT
    case JSON_LINT:
        _PRINT_PTR_NUMBER(lint_to_string, json->value.vnum.vlint);
        break;
    case JSON_LHEX:
        _PRINT_PTR_NUMBER(lhex_to_string, json->value.vnum.vlhex);
        break;
#endif
    case JSON_DOUBLE:
        _PRINT_PTR_NUMBER(double_to_string, json->value.vnum.vdbl);
        break;
    case JSON_STRING:
        if (unlikely(!json->value.vstr.len)) {
            _PRINT_PTR_STRNCAT(print_ptr, "\"\"", 2);
        } else {
            _JSON_PRINT_STRING(print_ptr, &json->value.vstr);
        }
        break;
    case JSON_OBJECT:
        if (unlikely(json->value.head.prev == (struct json_list *)&json->value.head)) {
            _PRINT_PTR_STRNCAT(print_ptr, "{}", 2);
            break;
        }
        _PRINT_PTR_STRNCAT(print_ptr, "{", 1);
        goto next1;
    case JSON_ARRAY:
        if (unlikely(json->value.head.prev == (struct json_list *)&json->value.head)) {
            _PRINT_PTR_STRNCAT(print_ptr, "[]", 2);
            break;
        }
        _PRINT_PTR_STRNCAT(print_ptr, "[", 1);
        goto next1;
    default:
        break;
    }
    ++print_ptr->item_count;

next4:
    if (likely(item_depth >= 0)) {
        tmp = (json_object*)(json->list.next);
        if (parent->type_member == JSON_OBJECT) {
            if (likely(&tmp->list != (struct json_list *)&parent->value.head)) {
                _PRINT_PTR_STRNCAT(print_ptr, ",", 1);
                json = tmp;
                goto next2;
            } else {
                if (print_ptr->format_flag) {
                    if (item_depth > 0) {
                        _PRINT_ADDI_FORMAT(print_ptr, item_depth);
                    } else {
                        _PRINT_PTR_STRNCAT(print_ptr, "\n", 1);
                    }
                }
                _PRINT_PTR_STRNCAT(print_ptr, "}", 1);
                ++print_ptr->item_count;
                if (likely(item_depth > 0)) {
                    json = parent;
                    parent = item_array[--item_depth];
                    goto next4;
                }
            }
        } else {
            if (likely(&tmp->list != (struct json_list *)&parent->value.head)) {
                if (print_ptr->format_flag) {
                    _PRINT_PTR_STRNCAT(print_ptr, ", ", 2);
                } else {
                    _PRINT_PTR_STRNCAT(print_ptr, ",", 1);
                }
                json = tmp;
                goto next3;
            } else {
                if (print_ptr->format_flag) {
                    if (json->type_member == JSON_OBJECT || json->type_member == JSON_ARRAY) {
                        if (likely(item_depth > 0)) {
                            _PRINT_ADDI_FORMAT(print_ptr, item_depth);
                        } else {
                            _PRINT_PTR_STRNCAT(print_ptr, "\n", 1);
                        }
                    }
                }
                _PRINT_PTR_STRNCAT(print_ptr, "]", 1);
                ++print_ptr->item_count;
                if (likely(item_depth > 0)) {
                    json = parent;
                    parent = item_array[--item_depth];
                    goto next4;
                }
            }
        }
    }

    if (item_array) {
        json_free(item_array);
    }
    return 0;
err:
    if (item_array) {
        json_free(item_array);
    }
    return -1;
}

static int _print_val_release(json_print_t *print_ptr, bool free_all_flag, size_t *length)
{
#define _clear_free_ptr(ptr)    do { if (ptr) json_free(ptr); ptr = NULL; } while(0)
#define _clear_close_fd(fd)     do { if (fd >= 0) close(fd); fd = -1; } while(0)
    int ret = 0;
    size_t used = GET_BUF_USED_SIZE(print_ptr);

    if (print_ptr->fd >= 0) {
        if (!free_all_flag && used > 0) {
            if (used != (size_t)write(print_ptr->fd, print_ptr->ptr, used)) {
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
            if (length)
                *length = print_ptr->cur - print_ptr->ptr;
            *print_ptr->cur = '\0';
            print_ptr->ptr = json_realloc(print_ptr->ptr, used + 1);
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
    print_ptr->cur = print_ptr->ptr;

    return 0;
err:
    _print_val_release(print_ptr, true, NULL);
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

    if (_json_print(&print_val, json) < 0) {
        JsonErr("print failed!\n");
        goto err;
    }
    if (_print_val_release(&print_val, false, &choice->str_len) < 0)
        goto err;

    return choice->path ? "ok" : print_val.ptr;
err:
    _print_val_release(&print_val, true, NULL);
    return NULL;
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

int json_sax_print_value(json_sax_print_hd handle, json_type_t type, json_string_t *jkey, const void *value)
{
    json_sax_print_t *print_handle = handle;
    json_print_t *print_ptr = &print_handle->print_val;
    json_string_t *jstr = NULL;
    int cur_pos = print_handle->count - 1;

    if (unlikely(print_handle->error_flag)) {
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
                if (print_ptr->format_flag
                    && (type == JSON_OBJECT || type == JSON_ARRAY)
                    && print_handle->array[cur_pos].type == JSON_ARRAY) {
                    _PRINT_ADDI_FORMAT(print_ptr, print_handle->count);
                }
            }

            // add key
            if (print_handle->array[cur_pos].type == JSON_OBJECT) {
                if (print_ptr->format_flag) {
                    if (unlikely(!jkey || !jkey->str || !jkey->str[0])) {
#if !JSON_STRICT_PARSE_MODE
                        JsonErr("key is null!\n");
                        goto err;
#else
                        if (print_handle->count > 0)
                            _PRINT_ADDI_FORMAT(print_ptr, print_handle->count);
                        _PRINT_PTR_STRNCAT(print_ptr, "\"\":\t", 4);
#endif
                    } else {
                        if (print_handle->count > 0)
                            _PRINT_ADDI_FORMAT(print_ptr, print_handle->count);
                        json_string_info_update(jkey);
                        _JSON_PRINT_STRING(print_ptr, jkey);
                        _PRINT_PTR_STRNCAT(print_ptr, ":\t", 2);
                    }
                } else {
                    if (unlikely(!jkey || !jkey->str || !jkey->str[0])) {
#if !JSON_STRICT_PARSE_MODE
                        JsonErr("key is null!\n");
                        goto err;
#else
                        _PRINT_PTR_STRNCAT(print_ptr, "\"\":", 3);
#endif
                    } else {
                        json_string_info_update(jkey);
                        _JSON_PRINT_STRING(print_ptr, jkey);
                        _PRINT_PTR_STRNCAT(print_ptr, ":", 1);
                    }
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
        _PRINT_PTR_NUMBER(int_to_string, *(int*)value);
        break;
    case JSON_HEX:
        _PRINT_PTR_NUMBER(hex_to_string, *(unsigned int*)value);
        break;
#if JSON_LONG_LONG_SUPPORT
    case JSON_LINT:
        _PRINT_PTR_NUMBER(lint_to_string, *(long long int*)value);
        break;
    case JSON_LHEX:
        _PRINT_PTR_NUMBER(lhex_to_string, *(unsigned long long int*)value);
        break;
#endif
    case JSON_DOUBLE:
        _PRINT_PTR_NUMBER(double_to_string, *(double*)value);
        break;
    case JSON_STRING:
        jstr = (json_string_t*)value;
        if (unlikely(!jstr || !jstr->str || !jstr->str[0])) {
            _PRINT_PTR_STRNCAT(print_ptr, "\"\"", 2);
        } else {
            json_string_info_update(jstr);
            _JSON_PRINT_STRING(print_ptr, jstr);
        }
        break;

    case JSON_ARRAY:
        switch ((*(json_sax_cmd_t*)value)) {
        case JSON_SAX_START:
            if (print_handle->count == print_handle->total) {
                print_handle->total += JSON_ITEM_NUM_PLUS_DEF;
                if (unlikely((print_handle->array = json_realloc(print_handle->array,
                            print_handle->total * sizeof(json_sax_print_depth_t))) == NULL)) {
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
            if (unlikely(print_handle->count == 0 || print_handle->array[cur_pos].type != JSON_ARRAY)) {
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
            if (unlikely(print_handle->count == print_handle->total)) {
                print_handle->total += JSON_ITEM_NUM_PLUS_DEF;
                if (unlikely((print_handle->array = json_realloc(print_handle->array,
                            print_handle->total * sizeof(json_sax_print_depth_t))) == NULL)) {
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
            if (unlikely(print_handle->count == 0 || print_handle->array[cur_pos].type != JSON_OBJECT)) {
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
        _print_val_release(&print_handle->print_val, true, NULL);
        json_free(print_handle);
        JsonErr("malloc failed!\n");
        return NULL;
    }

    return print_handle;
}

char *json_sax_print_finish(json_sax_print_hd handle, size_t *length)
{
    char *ret = NULL;

    json_sax_print_t *print_handle = handle;
    if (!print_handle)
        return NULL;
    if (print_handle->array)
        json_free(print_handle->array);
    if (print_handle->error_flag) {
        _print_val_release(&print_handle->print_val, true, NULL);
        json_free(print_handle);
        return NULL;
    }

    ret = (print_handle->print_val.fd >= 0) ? "ok" : print_handle->print_val.ptr;
    if (_print_val_release(&print_handle->print_val, false, length) < 0) {
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

    /* The two func ptrs are only for DOM parser */
    int (*parse_string)(struct _json_parse_t *parse_ptr, json_string_t *jstr, json_mem_mgr_t *mgr);
    int (*parse_value)(struct _json_parse_t *parse_ptr, json_object **root);

#if JSON_SAX_APIS_SUPPORT
    json_sax_parser_t parser;
    json_sax_cb_t cb;
    json_sax_ret_t ret;
#endif
} json_parse_t;

static inline void *_parse_alloc(json_parse_t *parse_ptr, size_t size, json_mem_mgr_t *mgr)
{
    if (parse_ptr->mem == &s_invalid_json_mem)
        return json_malloc(size);
    return pjson_memory_alloc(size, mgr);
}

static int _get_file_parse_ptr(json_parse_t *parse_ptr, int read_offset, size_t read_size, char **sstr)
{
    size_t offset = 0;
    ssize_t size = 0, rsize = 0;

    offset = parse_ptr->offset + read_offset;
    size = parse_ptr->readed - offset;

    if (likely(offset + read_size <= parse_ptr->readed)) {
        *sstr = parse_ptr->str + offset;
    } else if (parse_ptr->read_size == 0) {
        if (size >= 0) {
            *sstr = parse_ptr->str + offset;
        } else {
            *sstr = JSON_PARSE_ERROR_STR; /* Reduce the judgment pointer is NULL. */
            size = 0;
        }
    } else {
        size = parse_ptr->readed - parse_ptr->offset;
        if (size && parse_ptr->offset)
            memmove(parse_ptr->str, parse_ptr->str + parse_ptr->offset, size);
        parse_ptr->offset = 0;
        parse_ptr->readed = size;

        if (read_size > parse_ptr->size) {
            while (read_size > parse_ptr->size)
                parse_ptr->size += parse_ptr->read_size;
            if ((parse_ptr->str = json_realloc(parse_ptr->str, parse_ptr->size + 1)) == NULL) {
                JsonErr("malloc failed!\n");
                *sstr = JSON_PARSE_ERROR_STR; /* Reduce the judgment pointer is NULL. */
                return 0;
            }
        }

        size = parse_ptr->size - parse_ptr->readed;
        if ((rsize = read(parse_ptr->fd, parse_ptr->str + parse_ptr->readed, size)) != size)
            parse_ptr->read_size = 0; /* finish readding file */
        parse_ptr->readed += rsize < 0 ? 0 : rsize;
        parse_ptr->str[parse_ptr->readed] = '\0';

        *sstr = parse_ptr->str + read_offset;
        size = parse_ptr->readed - read_offset;
    }

    return size;
}

static inline int _get_str_parse_ptr(json_parse_t *parse_ptr, int read_offset, size_t read_size UNUSED_ATTR, char **sstr)
{
    size_t offset = parse_ptr->offset + read_offset;
    *sstr = parse_ptr->str + offset;
    return (parse_ptr->size - offset);
}

static inline int _get_parse_ptr(json_parse_t *parse_ptr, int read_offset, size_t read_size, char **sstr)
{
    if (parse_ptr->fd >= 0)
        return _get_file_parse_ptr(parse_ptr, read_offset, read_size, sstr);
    return _get_str_parse_ptr(parse_ptr, read_offset, read_size, sstr);
}
#define _UPDATE_PARSE_OFFSET(add_offset)    parse_ptr->offset += add_offset

static inline bool _hex_char_check(char c)
{
    switch (c) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
        return true;
    default:
        return false;
    }
}

static inline bool _dec_char_check(char c)
{
    switch (c) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        return true;
    default:
        return false;
    }
}

static inline unsigned int _hex_char_calculate(char c)
{
    switch (c) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        return c - '0';
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
        return 10 + c - 'a';
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
        return 10 + c - 'A';
    default:
        return 0;
    }
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
        case '-': ++num; sign = -1; break;
        case '+': ++num; break;
        default: break;
        }

        while (*num == '0')
            ++num;
        while (_dec_char_check(*num))
            m = (m << 3) + (m << 1) + (*num++ - '0');

        if (*num == '.') {
            ++num;
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
    const char *num = *sstr;

#if JSON_STRICT_PARSE_MODE == 2
    if (*num == '0' && (*(num+1) == 'x' || *(num+1) == 'X' || _dec_char_check(*(num+1)))) {
        JsonErr("leading zero can't be parsed in strict mode!\n");
        return JSON_NULL;
    }
#endif

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
            vnum->vdbl = strtod(num, sstr);
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
            val = (val << 4) + (str[i] - '0');
            break;
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
            val = (val << 4) + 10 + (str[i] - 'a');
            break;
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
            val = (val << 4) + 10 + (str[i] - 'A');
            break;
        default:
            return 0;
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
    for (i = len - 1; i > 0; --i) {                         /* encode as utf8 */
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

static inline void _skip_blank(json_parse_t *parse_ptr)
{
    unsigned char *str = NULL;
    int cnt = 0;

    while (_get_parse_ptr(parse_ptr, cnt, 64, (char **)&str)) {
        while (*str <= ' ') {
            if (likely(*str)) {
                ++str;
                ++cnt;
            } else {
                goto next;
            }
        }
        break;
next:
        continue;
    }
    _UPDATE_PARSE_OFFSET(cnt);
}

static int _parse_strcpy(char *ptr, const char *str, int nsize)
{
    const char *bak = ptr, *last = str, *end = str + nsize;
    char ch = '\0';
    int size = 0, seq_len = 0;

    while (1) {
        switch ((*str++)) {
        case '\"':
            size = str - last - 1;
            memcpy(ptr, last, size);
            ptr += size;
            *ptr = '\0';
            return (ptr - bak);
        case '\0':
            return -1;
        case '\\':
            switch ((*str++)) {
            case 'b':  ch = '\b'; break;
            case 'f':  ch = '\f'; break;
            case 'n':  ch = '\n'; break;
            case 'r':  ch = '\r'; break;
            case 't':  ch = '\t'; break;
            case 'v':  ch = '\v'; break;
            case '\"': ch = '\"'; break;
            case '\\': ch = '\\'; break;
            case '/' : ch = '/' ; break;
            case 'u':
                str -= 2;
                size = str - last;
                memcpy(ptr, last, size);
                ptr += size;
                if (unlikely((seq_len = utf16_literal_to_utf8((unsigned char*)str,
                    (unsigned char*)end, (unsigned char**)&ptr)) == 0)) {
                    JsonErr("invalid utf16 code(\\u%c)!\n", str[2]);
                    return -1;
                }
                str += seq_len;
                last = str;
                continue;
            default :
                JsonErr("invalid escape character(\\%c)!\n", str[1]);
                return -1;
            }

            size = str - last - 2;
            memcpy(ptr, last, size);
            ptr += size;
            *ptr++ = ch;
            last = str;
            break;
        default:
            break;
        }
    }

    return -1;
}

static int _parse_strlen(json_parse_t *parse_ptr, bool *escaped)
{
#define PARSE_READ_SIZE    128
    char *str = NULL, *bak = NULL;
    int total = 0, rsize = 0;
    char c = '\0';

    _get_parse_ptr(parse_ptr, 0, PARSE_READ_SIZE, &str);
    bak = str++;

    while (1) {
        switch ((c = *str++)) {
        case '\"':
            total += str - bak;
            return total;
        case '\\':
            if (likely(rsize != (str - bak))) {
                ++str;
                *escaped = true;
            } else {
                --str;
                total += str - bak;
                if (unlikely((rsize = _get_parse_ptr(parse_ptr, total, PARSE_READ_SIZE, &str)) < 2)) {
                    JsonErr("last char is slash!\n");
                    goto err;
                }
                bak = str;
            }
            break;
        case '\0':
            --str;
            total += str - bak;
            if (unlikely((rsize = _get_parse_ptr(parse_ptr, total, PARSE_READ_SIZE, &str)) < 1)) {
                JsonErr("No more string!\n");
                goto err;
            }
            bak = str;
            break;
#if JSON_STRICT_PARSE_MODE == 2
        case '\b': case '\f': case '\n': case '\r': case '\t': case '\v':
            JsonErr("tab and linebreak can't be existed in string in strict mode!\n");
            goto err;
#endif
        default:
            break;
        }
    }

err:
    JsonPareseErr("str format err!");
    return -1;
}

static int _json_parse_string(json_parse_t *parse_ptr, json_string_t *jstr, json_mem_mgr_t *mgr)
{
    char *ptr = NULL, *str = NULL;
    bool escaped = false;
    int len = 0, total = 0;

    memset(jstr, 0, sizeof(json_string_t));
    if (unlikely((total = _parse_strlen(parse_ptr, &escaped)) < 0)) {
        return -1;
    }
    len = total - 2;
    _get_parse_ptr(parse_ptr, 0, total, &str);
    ++str;

    if (unlikely((ptr = _parse_alloc(parse_ptr, len + 1, mgr)) == NULL)) {
        JsonErr("malloc failed!\n");
        return -1;
    }

    if (likely(!escaped)) {
        memcpy(ptr, str, len);
        ptr[len] = '\0';
    } else {
        if (unlikely((len = _parse_strcpy(ptr, str, len)) < 0)) {
            JsonErr("_parse_strcpy failed!\n");
            if (parse_ptr->mem == &s_invalid_json_mem)
                json_free(ptr);
            goto err;
        }
    }
    _UPDATE_PARSE_OFFSET(total);

    jstr->escaped = escaped;
    jstr->len = len;
    jstr->str = ptr;
    return len;

err:
    JsonPareseErr("parse string failed!");
    return -1;
}

static int _json_parse_value(json_parse_t *parse_ptr, json_object **root)
{
    json_string_t jkey = {0};
    char *str = NULL, *bak = NULL;
    json_object *item = NULL, *parent = NULL;
    json_object **item_array = NULL;
    int item_depth = -1, item_total = 0;

    goto next3;

next1:
    if (unlikely(item_depth >= item_total - 1)) {
        item_total += 16;
        if (unlikely((item_array = json_realloc(item_array, sizeof(json_object *) * item_total)) == NULL)) {
            JsonErr("malloc failed!\n");
            goto err;
        }
    }
    item_array[++item_depth] = item;
    parent = item;
    *root = *item_array;
    if (parent->type_member == JSON_ARRAY)
        goto next3;

next2:
    _skip_blank(parse_ptr);
    _get_parse_ptr(parse_ptr, 0, 2, &str);
    if (unlikely(str[0] != '\"')) {
        JsonPareseErr("key is not started with quote!");
        goto err;
    }

    if (unlikely(str[1] == '\"')) {
#if !JSON_STRICT_PARSE_MODE
        JsonPareseErr("key is space!");
        goto err;
#else
        _UPDATE_PARSE_OFFSET(2);
#endif
    } else {
        if (unlikely(parse_ptr->parse_string(parse_ptr, &jkey, &parse_ptr->mem->key_mgr) < 0)) {
            goto err;
        }
    }

    _skip_blank(parse_ptr);
    _get_parse_ptr(parse_ptr, 0, 1, &str);
    if (unlikely(*str != ':')) {
        JsonPareseErr("key is not before ':'");
        goto err;
    }
    _UPDATE_PARSE_OFFSET(1);

next3:
    if (unlikely((item = _parse_alloc(parse_ptr, sizeof(json_object), &parse_ptr->mem->obj_mgr)) == NULL)) {
        JsonErr("malloc failed!\n");
        goto err;
    }
    memcpy(&item->jkey, &jkey, sizeof(json_string_t));
    memset(&jkey, 0, sizeof(json_string_t));

    _skip_blank(parse_ptr);
    _get_parse_ptr(parse_ptr, 0, 128, &str);

    switch (*str) {
    case '\"':
        item->type_member = JSON_STRING;
        if (unlikely(str[1] == '\"')) {
            memset(&item->value.vstr, 0, sizeof(json_string_t));
            _UPDATE_PARSE_OFFSET(2);
        } else {
            if (unlikely(parse_ptr->parse_string(parse_ptr, &item->value.vstr, &parse_ptr->mem->str_mgr) < 0)) {
                goto err;
            }
        }
        break;

    case '-': case '+':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        bak = str;
        if (unlikely((item->type_member = _json_parse_number(&str, &item->value.vnum)) == JSON_NULL)) {
            JsonPareseErr("Not number!");
            goto err;
        }
        _UPDATE_PARSE_OFFSET(str - bak);
        break;

    case '{':
        item->type_member = JSON_OBJECT;
        INIT_JSON_LIST_HEAD(&item->value.head);
        _UPDATE_PARSE_OFFSET(1);

        _skip_blank(parse_ptr);
        _get_parse_ptr(parse_ptr, 0, 1, &str);
        if (likely(*str != '}')) {
            if (parent)
                json_list_add_tail(&item->list, &parent->value.head);
            goto next1;
        } else {
            _UPDATE_PARSE_OFFSET(1);
        }
        break;

    case '[':
        item->type_member = JSON_ARRAY;
        INIT_JSON_LIST_HEAD(&item->value.head);
        _UPDATE_PARSE_OFFSET(1);

        _skip_blank(parse_ptr);
        _get_parse_ptr(parse_ptr, 0, 1, &str);
        if (likely(*str != ']')) {
            if (parent)
                json_list_add_tail(&item->list, &parent->value.head);
            goto next1;
        } else {
            _UPDATE_PARSE_OFFSET(1);
        }
        break;

    case 'f':
        if (strncmp(str, "false", 5) == 0) {
            item->type_member = JSON_BOOL;
            item->value.vnum.vbool = false;
            _UPDATE_PARSE_OFFSET(5);
        } else {
            JsonPareseErr("invalid next ptr!");
            goto err;
        }
        break;
    case 't':
        if (strncmp(str, "true", 4) == 0) {
            item->type_member = JSON_BOOL;
            item->value.vnum.vbool = true;
            _UPDATE_PARSE_OFFSET(4);
        } else {
            JsonPareseErr("invalid next ptr!");
            goto err;
        }
        break;

    case 'n':
        if (strncmp(str, "null", 4) == 0) {
            item->type_member = JSON_NULL;
            _UPDATE_PARSE_OFFSET(4);
        } else {
            JsonPareseErr("invalid next ptr!");
            goto err;
        }
        break;
    default:
        JsonPareseErr("invalid next ptr!");
        goto err;
    }

    if (parent) {
        json_list_add_tail(&item->list, &parent->value.head);
        item = NULL;
    }

next4:
    if (likely(item_depth >= 0)) {
        _skip_blank(parse_ptr);
        _get_parse_ptr(parse_ptr, 0, 1, &str);
        if (parent->type_member == JSON_OBJECT) {
            switch (*str) {
            case ',':
                _UPDATE_PARSE_OFFSET(1);
                goto next2;
            case '}':
                _UPDATE_PARSE_OFFSET(1);
                if (likely(item_depth > 0)) {
                    parent = item_array[--item_depth];
                    goto next4;
                }
                break;
            default:
                JsonPareseErr("invalid object!");
                goto err;
            }
        } else {
            switch (*str) {
            case ',':
                _UPDATE_PARSE_OFFSET(1);
                goto next3;
            case ']':
                _UPDATE_PARSE_OFFSET(1);
                if (likely(item_depth > 0)) {
                    parent = item_array[--item_depth];
                    goto next4;
                }
                break;
            default:
                JsonPareseErr("invalid array!");
                goto err;
            }
        }
    }

    if (item_array) {
        *root = *item_array;
        json_free(item_array);
    } else {
        *root = item;
    }

    return 0;

err:
    if (item_array) {
        *root = *item_array;
        json_free(item_array);
    } else {
        if (!(*root)) {
            *root = item;
            item = NULL;
        }
    }

    if (parse_ptr->mem == &s_invalid_json_mem) {
        if (item)
            json_free(item);
        if (jkey.str)
            json_free(jkey.str);
    }
    return -1;
}

static inline void _skip_blank_rapid(json_parse_t *parse_ptr)
{
    unsigned char *str = (unsigned char *)(parse_ptr->str + parse_ptr->offset);
    int cnt = 0;

    while (*str <= ' ' && *str) {
        ++str;
        ++cnt;
    }
    _UPDATE_PARSE_OFFSET(cnt);
}

static int _json_parse_string_reuse(json_parse_t *parse_ptr, json_string_t *jstr, json_mem_mgr_t *mgr UNUSED_ATTR)
{
    char *str = NULL, *end = NULL, *last = NULL, *bak = NULL, *ptr= NULL;
    int len = 0, seq_len = 0, size = 0;
    char ch = '\0';

    memset(jstr, 0, sizeof(json_string_t));
    _UPDATE_PARSE_OFFSET(1);
    end = parse_ptr->str + parse_ptr->size;
    str = parse_ptr->str + parse_ptr->offset;
    bak = str;
    ptr = str;
    jstr->str = str;

    while (1) {
        switch ((*str++)) {
        case '\"':
            len = str - bak;
            _UPDATE_PARSE_OFFSET(len);
            --len;
            ptr[len] = '\0';
            jstr->len = len;
            return len;
        case '\0':
            goto err;
        case '\\':
            --str;
            last = str;
            ptr += str - bak;
            goto next;
#if JSON_STRICT_PARSE_MODE == 2
        case '\b': case '\f': case '\n': case '\r': case '\t': case '\v':
            JsonErr("tab and linebreak can't be existed in string in strict mode!\n");
            goto err;
#endif
        default:
            break;
        }
    }

next:
    while (1) {
        switch ((*str++)) {
        case '\"':
            size = str - last - 1;
            memmove(ptr, last, size);
            ptr += size;
            *ptr = '\0';

            len = str - bak;
            _UPDATE_PARSE_OFFSET(len);
            jstr->escaped = 1;
            jstr->len = ptr - bak;
            return jstr->len;
        case '\0':
            goto err;
        case '\\':
            switch ((*str++)) {
            case 'b':  ch = '\b'; break;
            case 'f':  ch = '\f'; break;
            case 'n':  ch = '\n'; break;
            case 'r':  ch = '\r'; break;
            case 't':  ch = '\t'; break;
            case 'v':  ch = '\v'; break;
            case '\"': ch = '\"'; break;
            case '\\': ch = '\\'; break;
            case '/' : ch = '/' ; break;
            case 'u':
                str -= 2;
                size = str - last;
                memmove(ptr, last, size);
                ptr += size;
                if (unlikely((seq_len = utf16_literal_to_utf8((unsigned char*)str,
                    (unsigned char*)end, (unsigned char**)&ptr)) == 0)) {
                    JsonErr("invalid utf16 code(\\u%c)!\n", str[2]);
                    goto err;
                }
                str += seq_len;
                last = str;
                continue;
            default :
                JsonErr("invalid escape character(\\%c)!\n", str[1]);
                goto err;
            }

            size = str - last - 2;
            memmove(ptr, last, size);
            ptr += size;
            *ptr++ = ch;
            last = str;
            break;
        default:
            break;
        }
    }

err:
    memset(jstr, 0, sizeof(json_string_t));
    JsonPareseErr("parse string failed!");
    return -1;
}

static int _json_parse_string_rapid(json_parse_t *parse_ptr, json_string_t *jstr, json_mem_mgr_t *mgr)
{
    char *str = NULL, *bak = NULL, *ptr= NULL;
    int len = 0, total = 0;
    bool escaped = false;

    memset(jstr, 0, sizeof(json_string_t));
    _UPDATE_PARSE_OFFSET(1);
    str = parse_ptr->str + parse_ptr->offset;
    bak = str;

    while (1) {
        switch ((*str++)) {
        case '\"':
            goto next;
        case '\0':
            goto err;
        case '\\':
            ++str;
            escaped = true;
            break;
#if JSON_STRICT_PARSE_MODE == 2
        case '\b': case '\f': case '\n': case '\r': case '\t': case '\v':
            JsonErr("tab and linebreak can't be existed in string in strict mode!\n");
            goto err;
#endif
        default:
            break;
        }
    }

next:
    total = str - bak;
    len = total - 1;
    if (unlikely((ptr = _parse_alloc(parse_ptr, len + 1, mgr)) == NULL)) {
        JsonErr("malloc failed!\n");
        goto err;
    }

    if (likely(!escaped)) {
        memcpy(ptr, bak, len);
        ptr[len] = '\0';
    } else {
        if (unlikely((len = _parse_strcpy(ptr, bak, len)) < 0)) {
            JsonErr("_parse_strcpy failed!\n");
            if (parse_ptr->mem == &s_invalid_json_mem)
                json_free(ptr);
            goto err;
        }
    }
    _UPDATE_PARSE_OFFSET(total);

    jstr->escaped = escaped;
    jstr->len = len;
    jstr->str = ptr;
    return len;
err:
    JsonPareseErr("parse string failed!");
    return -1;
}

static int _json_parse_value_rapid(json_parse_t *parse_ptr, json_object **root)
{
    json_string_t jkey = {0};
    char *str = NULL, *bak = NULL;
    json_object *item = NULL, *parent = NULL;
    json_object **item_array = NULL;
    int item_depth = -1, item_total = 0;

    goto next3;

next1:
    if (unlikely(item_depth >= item_total - 1)) {
        item_total += 16;
        if (unlikely((item_array = json_realloc(item_array, sizeof(json_object *) * item_total)) == NULL)) {
            JsonErr("malloc failed!\n");
            goto err;
        }
    }
    item_array[++item_depth] = item;
    parent = item;
    *root = *item_array;
    if (parent->type_member == JSON_ARRAY)
        goto next3;

next2:
    _skip_blank_rapid(parse_ptr);
    str = parse_ptr->str + parse_ptr->offset;
    if (unlikely(str[0] != '\"')) {
        JsonPareseErr("key is not started with quote!");
        goto err;
    }

    if (unlikely(str[1] == '\"')) {
#if !JSON_STRICT_PARSE_MODE
        JsonPareseErr("key is space!");
        goto err;
#else
        _UPDATE_PARSE_OFFSET(2);
#endif
    } else {
        if (unlikely(parse_ptr->parse_string(parse_ptr, &jkey, &parse_ptr->mem->key_mgr) < 0)) {
            goto err;
        }
    }

    _skip_blank_rapid(parse_ptr);
    str = parse_ptr->str + parse_ptr->offset;
    if (unlikely(*str != ':')) {
        JsonPareseErr("key is not before ':'");
        goto err;
    }
    _UPDATE_PARSE_OFFSET(1);

next3:
    if (unlikely((item = _parse_alloc(parse_ptr, sizeof(json_object), &parse_ptr->mem->obj_mgr)) == NULL)) {
        JsonErr("malloc failed!\n");
        goto err;
    }
    memcpy(&item->jkey, &jkey, sizeof(json_string_t));
    memset(&jkey, 0, sizeof(json_string_t));

    _skip_blank_rapid(parse_ptr);
    str = parse_ptr->str + parse_ptr->offset;

    switch (*str) {
    case '\"':
        item->type_member = JSON_STRING;
        if (unlikely(str[1] == '\"')) {
            memset(&item->value.vstr, 0, sizeof(json_string_t));
            _UPDATE_PARSE_OFFSET(2);
        } else {
            if (unlikely(parse_ptr->parse_string(parse_ptr, &item->value.vstr, &parse_ptr->mem->str_mgr) < 0)) {
                goto err;
            }
        }
        break;

    case '-': case '+':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        bak = str;
        if (unlikely((item->type_member = _json_parse_number(&str, &item->value.vnum)) == JSON_NULL)) {
            JsonPareseErr("Not number!");
            goto err;
        }
        _UPDATE_PARSE_OFFSET(str - bak);
        break;

    case '{':
        item->type_member = JSON_OBJECT;
        INIT_JSON_LIST_HEAD(&item->value.head);
        _UPDATE_PARSE_OFFSET(1);

        _skip_blank_rapid(parse_ptr);
        str = parse_ptr->str + parse_ptr->offset;
        if (likely(*str != '}')) {
            if (parent)
                json_list_add_tail(&item->list, &parent->value.head);
            goto next1;
        } else {
            _UPDATE_PARSE_OFFSET(1);
        }
        break;

    case '[':
        item->type_member = JSON_ARRAY;
        INIT_JSON_LIST_HEAD(&item->value.head);
        _UPDATE_PARSE_OFFSET(1);

        _skip_blank_rapid(parse_ptr);
        str = parse_ptr->str + parse_ptr->offset;
        if (likely(*str != ']')) {
            if (parent)
                json_list_add_tail(&item->list, &parent->value.head);
            goto next1;
        } else {
            _UPDATE_PARSE_OFFSET(1);
        }
        break;

    case 'f':
        if (strncmp(str, "false", 5) == 0) {
            item->type_member = JSON_BOOL;
            item->value.vnum.vbool = false;
            _UPDATE_PARSE_OFFSET(5);
        } else {
            JsonPareseErr("invalid next ptr!");
            goto err;
        }
        break;
    case 't':
        if (strncmp(str, "true", 4) == 0) {
            item->type_member = JSON_BOOL;
            item->value.vnum.vbool = true;
            _UPDATE_PARSE_OFFSET(4);
        } else {
            JsonPareseErr("invalid next ptr!");
            goto err;
        }
        break;

    case 'n':
        if (strncmp(str, "null", 4) == 0) {
            item->type_member = JSON_NULL;
            _UPDATE_PARSE_OFFSET(4);
        } else {
            JsonPareseErr("invalid next ptr!");
            goto err;
        }
        break;
    default:
        JsonPareseErr("invalid next ptr!");
        goto err;
    }

    if (parent) {
        json_list_add_tail(&item->list, &parent->value.head);
        item = NULL;
    }

next4:
    if (likely(item_depth >= 0)) {
        _skip_blank_rapid(parse_ptr);
        str = parse_ptr->str + parse_ptr->offset;
        if (parent->type_member == JSON_OBJECT) {
            switch (*str) {
            case ',':
                _UPDATE_PARSE_OFFSET(1);
                goto next2;
            case '}':
                _UPDATE_PARSE_OFFSET(1);
                if (likely(item_depth > 0)) {
                    parent = item_array[--item_depth];
                    goto next4;
                }
                break;
            default:
                JsonPareseErr("invalid object!");
                goto err;
            }
        } else {
            switch (*str) {
            case ',':
                _UPDATE_PARSE_OFFSET(1);
                goto next3;
            case ']':
                _UPDATE_PARSE_OFFSET(1);
                if (likely(item_depth > 0)) {
                    parent = item_array[--item_depth];
                    goto next4;
                }
                break;
            default:
                JsonPareseErr("invalid array!");
                goto err;
            }
        }
    }

    if (item_array) {
        *root = *item_array;
        json_free(item_array);
    } else {
        *root = item;
    }

    return 0;

err:
    if (item_array) {
        *root = *item_array;
        json_free(item_array);
    } else {
        if (!(*root)) {
            *root = item;
            item = NULL;
        }
    }

    if (parse_ptr->mem == &s_invalid_json_mem) {
        if (item)
            json_free(item);
        if (jkey.str)
            json_free(jkey.str);
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
        parse_val.parse_string = _json_parse_string;
        parse_val.parse_value = _json_parse_value;
    } else {
        parse_val.str = choice->str;
        total_size = choice->str_len ? choice->str_len : strlen(choice->str);
        parse_val.size = total_size;
        if (choice->mem) {
            parse_val.reuse_flag = choice->reuse_flag;
        }
        if (parse_val.reuse_flag) {
            parse_val.parse_string = _json_parse_string_reuse;
        } else {
            parse_val.parse_string = _json_parse_string_rapid;
        }
        parse_val.parse_value = _json_parse_value_rapid;
    }

    if (choice->mem) {
        pjson_memory_init(choice->mem);
        mem_size = total_size / 8;
        if (mem_size < choice->mem_size)
            mem_size = choice->mem_size;
        choice->mem->obj_mgr.mem_size = mem_size;
        choice->mem->key_mgr.mem_size = mem_size;
        choice->mem->str_mgr.mem_size = mem_size;
    }

#if JSON_STRICT_PARSE_MODE == 2
    _skip_blank(&parse_val);
    if (parse_val.str[parse_val.offset] != '{' && parse_val.str[parse_val.offset] != '[') {
        JsonErr("The first object isn't object or array\n");
        goto end;
    }
#endif

    if (parse_val.parse_value(&parse_val, &json) < 0) {
        if (choice->mem) {
            pjson_memory_free(choice->mem);
        } else {
            json_del_object(json);
        }
        json = NULL;
        goto end;
    }

#if JSON_STRICT_PARSE_MODE
    _skip_blank(&parse_val);
    if (parse_val.str[parse_val.offset]) {
        JsonErr("Extra trailing characters!\n%s\n", parse_val.str + parse_val.offset);
        if (choice->mem) {
            pjson_memory_free(choice->mem);
        } else {
            json_del_object(json);
        }
        json = NULL;
        goto end;
    }
#endif

end:
    if (choice->path) {
        if (parse_val.str)
            json_free(parse_val.str);
        if (parse_val.fd >= 0)
            close(parse_val.fd);
    }
    return json;
}

#if JSON_SAX_APIS_SUPPORT
static inline int _json_sax_parse_string(json_parse_t *parse_ptr, json_string_t *jstr, bool key_flag)
{
    char *ptr = NULL, *str = NULL;
    int len = 0, total = 0;
    bool escaped = false;

    memset(jstr, 0, sizeof(json_string_t));
    if (unlikely((total = _parse_strlen(parse_ptr, &escaped)) < 0)) {
        return -1;
    }
    len = total - 2;
    _get_parse_ptr(parse_ptr, 0, total, &str);
    ++str;

    if (likely(!escaped)) {
        if (!(parse_ptr->fd >= 0 && key_flag)) {
            jstr->escaped = escaped;
            jstr->alloced = 0;
            jstr->len = len;
            jstr->str = str;
        } else {
            if (unlikely((ptr = json_malloc(len+1)) == NULL)) {
                JsonErr("malloc failed!\n");
                return -1;
            }
            memcpy(ptr, str, len);
            ptr[len] = '\0';

            jstr->escaped = escaped;
            jstr->alloced = 1;
            jstr->len = len;
            jstr->str = ptr;
        }
    } else {
        if (unlikely((ptr = json_malloc(len+1)) == NULL)) {
            JsonErr("malloc failed!\n");
            return -1;
        }
        if (unlikely((len = _parse_strcpy(ptr, str, len)) < 0)) {
            JsonErr("_parse_strcpy failed!\n");
            json_free(ptr);
            goto err;
        }

        jstr->escaped = escaped;
        jstr->alloced = 1;
        jstr->len = len;
        jstr->str = ptr;
    }
    _UPDATE_PARSE_OFFSET(total);

    return len;
err:
    JsonPareseErr("parse string failed!");
    return -1;
}

static int _json_sax_parse_value(json_parse_t *parse_ptr)
{
    char *str = NULL, *bak = NULL;
    json_string_t *jkey = NULL, *parent = NULL;
    json_value_t *value = &parse_ptr->parser.value;
    json_string_t *tarray = NULL;
    int i = 0;

    memset(value, 0, sizeof(*value));
    parse_ptr->parser.total += 16;
    if (unlikely((parse_ptr->parser.array = json_malloc(sizeof(json_string_t) * parse_ptr->parser.total)) == NULL)) {
        JsonErr("malloc failed!\n");
        return -1;
    }
    memset(parse_ptr->parser.array, 0, sizeof(json_string_t));
    goto next3;

next1:
    if (unlikely(parse_ptr->parser.index >= parse_ptr->parser.total - 1)) {
        parse_ptr->parser.total += 16;
        if (unlikely((tarray = json_malloc(sizeof(json_string_t) * parse_ptr->parser.total)) == NULL)) {
            JsonErr("malloc failed!\n");
            goto err;
        }

        memcpy(tarray, parse_ptr->parser.array, sizeof(json_string_t) * (parse_ptr->parser.index + 1));
        json_free(parse_ptr->parser.array);
        parse_ptr->parser.array = tarray;
    }

    parent = parse_ptr->parser.array + parse_ptr->parser.index;
    ++parse_ptr->parser.index;
    memset(parse_ptr->parser.array + parse_ptr->parser.index, 0, sizeof(json_string_t));
    if (parent->type == JSON_ARRAY)
        goto next3;

next2:
    jkey = parse_ptr->parser.array + parse_ptr->parser.index;
    _skip_blank(parse_ptr);
    _get_parse_ptr(parse_ptr, 0, 2, &str);
    if (unlikely(str[0] != '\"')) {
        JsonPareseErr("key is not started with quote!");
        goto err;
    }

    if (unlikely(str[1] == '\"')) {
#if !JSON_STRICT_PARSE_MODE
        JsonPareseErr("key is space!");
        goto err;
#else
        _UPDATE_PARSE_OFFSET(2);
#endif
    } else {
        if (unlikely(_json_sax_parse_string(parse_ptr, jkey, true) < 0)) {
            goto err;
        }
    }

    _skip_blank(parse_ptr);
    _get_parse_ptr(parse_ptr, 0, 1, &str);
    if (unlikely(*str != ':')) {
        JsonPareseErr("key is not before ':'");
        goto err;
    }
    _UPDATE_PARSE_OFFSET(1);

next3:
    jkey = parse_ptr->parser.array + parse_ptr->parser.index;
    _skip_blank(parse_ptr);
    _get_parse_ptr(parse_ptr, 0, 128, &str);

    switch (*str) {
    case '\"':
        jkey->type = JSON_STRING;
        if (unlikely(str[1] == '\"')) {
            memset(value, 0, sizeof(*value));
            _UPDATE_PARSE_OFFSET(2);
        } else {
            if (unlikely(_json_sax_parse_string(parse_ptr, &value->vstr, false) < 0)) {
                goto err;
            }
        }
        break;

    case '-': case '+':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        bak = str;
        if (unlikely((jkey->type = _json_parse_number(&str, &value->vnum)) == JSON_NULL)) {
            JsonPareseErr("Not number!");
            goto err;
        }
        _UPDATE_PARSE_OFFSET(str - bak);
        break;

    case '{':
        jkey->type = JSON_OBJECT;
        value->vcmd = JSON_SAX_START;
        _UPDATE_PARSE_OFFSET(1);
        parse_ptr->ret = parse_ptr->cb(&parse_ptr->parser);
        if (unlikely(parse_ptr->ret == JSON_SAX_PARSE_STOP)) {
            goto end;
        }

        _skip_blank(parse_ptr);
        _get_parse_ptr(parse_ptr, 0, 1, &str);
        if (likely(*str != '}')) {
            goto next1;
        } else {
            jkey->type = JSON_OBJECT;
            value->vcmd = JSON_SAX_FINISH;
            _UPDATE_PARSE_OFFSET(1);
        }
        break;

    case '[':
        jkey->type = JSON_ARRAY;
        value->vcmd = JSON_SAX_START;
        _UPDATE_PARSE_OFFSET(1);
        parse_ptr->ret = parse_ptr->cb(&parse_ptr->parser);
        if (unlikely(parse_ptr->ret == JSON_SAX_PARSE_STOP)) {
            goto end;
        }

        _skip_blank(parse_ptr);
        _get_parse_ptr(parse_ptr, 0, 1, &str);
        if (likely(*str != ']')) {
            goto next1;
        } else {
            jkey->type = JSON_ARRAY;
            value->vcmd = JSON_SAX_FINISH;
            _UPDATE_PARSE_OFFSET(1);
        }
        break;

    case 'f':
        if (strncmp(str, "false", 5) == 0) {
            jkey->type = JSON_BOOL;
            value->vnum.vbool = false;
            _UPDATE_PARSE_OFFSET(5);
        } else {
            JsonPareseErr("invalid next ptr!");
            goto err;
        }
        break;
    case 't':
        if (strncmp(str, "true", 4) == 0) {
            jkey->type = JSON_BOOL;
            value->vnum.vbool = true;
            _UPDATE_PARSE_OFFSET(4);
        } else {
            JsonPareseErr("invalid next ptr!");
            goto err;
        }
        break;

    case 'n':
        if (strncmp(str, "null", 4) == 0) {
            jkey->type = JSON_NULL;
            _UPDATE_PARSE_OFFSET(4);
        } else {
            JsonPareseErr("invalid next ptr!");
            goto err;
        }
        break;
    default:
        JsonPareseErr("invalid next ptr!");
        goto err;
    }

    parse_ptr->ret = parse_ptr->cb(&parse_ptr->parser);
    if (jkey->type == JSON_STRING && value->vstr.alloced) {
        json_free(value->vstr.str);
    }
    memset(value, 0, sizeof(*value));
    if (jkey->alloced) {
        json_free(jkey->str);
    }
    memset(jkey, 0, sizeof(*jkey));
    if (unlikely(parse_ptr->ret == JSON_SAX_PARSE_STOP)) {
        --parse_ptr->parser.index;
        goto end;
    }

next4:
    if (likely(parse_ptr->parser.index > 0)) {
        _skip_blank(parse_ptr);
        _get_parse_ptr(parse_ptr, 0, 1, &str);
        if (parent->type == JSON_OBJECT) {
            switch (*str) {
            case ',':
                _UPDATE_PARSE_OFFSET(1);
                goto next2;
            case '}':
                _UPDATE_PARSE_OFFSET(1);
                --parse_ptr->parser.index;
                jkey = parse_ptr->parser.array + parse_ptr->parser.index;
                value->vcmd = JSON_SAX_FINISH;
                parse_ptr->ret = parse_ptr->cb(&parse_ptr->parser);
                memset(value, 0, sizeof(*value));
                if (jkey->alloced) {
                    json_free(jkey->str);
                }
                memset(jkey, 0, sizeof(*jkey));
                if (unlikely(parse_ptr->ret == JSON_SAX_PARSE_STOP)) {
                    --parse_ptr->parser.index;
                    goto end;
                }

                if (likely(parse_ptr->parser.index > 0)) {
                    parent = parse_ptr->parser.array + parse_ptr->parser.index - 1;
                    goto next4;
                }
                break;
            default:
                JsonPareseErr("invalid object!");
                goto err;
            }
        } else {
            switch (*str) {
            case ',':
                _UPDATE_PARSE_OFFSET(1);
                goto next3;
            case ']':
                _UPDATE_PARSE_OFFSET(1);
                --parse_ptr->parser.index;
                jkey = parse_ptr->parser.array + parse_ptr->parser.index;
                value->vcmd = JSON_SAX_FINISH;
                parse_ptr->ret = parse_ptr->cb(&parse_ptr->parser);
                memset(value, 0, sizeof(*value));
                if (jkey->alloced) {
                    json_free(jkey->str);
                }
                memset(jkey, 0, sizeof(*jkey));
                if (unlikely(parse_ptr->ret == JSON_SAX_PARSE_STOP)) {
                    --parse_ptr->parser.index;
                    goto end;
                }

                if (likely(parse_ptr->parser.index > 0)) {
                    parent = parse_ptr->parser.array + parse_ptr->parser.index - 1;
                    goto next4;
                }
                break;
            default:
                JsonPareseErr("invalid array!");
                goto err;
            }
        }
    }

    parse_ptr->parser.index = -1;
end:
    value->vcmd = JSON_SAX_FINISH;
    for (i =parse_ptr->parser.index; i >= 0; --i) {
        parse_ptr->ret = parse_ptr->cb(&parse_ptr->parser);
        if (parse_ptr->parser.array[i].alloced) {
            json_free(parse_ptr->parser.array[i].str);
        }
    }
    json_free(parse_ptr->parser.array);
    memset(&parse_ptr->parser, 0, sizeof(parse_ptr->parser));

    return 0;
err:
    if (parse_ptr->parser.array) {
        for (i = 0; i < parse_ptr->parser.index; ++i) {
            if (parse_ptr->parser.array[i].alloced) {
                json_free(parse_ptr->parser.array[i].str);
            }
        }
        json_free(parse_ptr->parser.array);
    }
    memset(&parse_ptr->parser, 0, sizeof(parse_ptr->parser));
    return -1;
}

int json_sax_parse_common(json_sax_parse_choice_t *choice)
{
    int ret = -1;
    json_parse_t parse_val = {0};

    parse_val.read_size = parse_val.read_size ? parse_val.read_size : JSON_PARSE_READ_SIZE_DEF;
    parse_val.mem = NULL;
    parse_val.fd = -1;
    if (choice->path) {
        if ((parse_val.fd = open(choice->path, O_RDONLY)) < 0) {
            JsonErr("open(%s) failed!\n", choice->path);
            return -1;
        }
    } else {
        parse_val.str = choice->str;
        parse_val.size = choice->str_len ? choice->str_len : strlen(choice->str);
    }
    parse_val.cb = choice->cb;

#if JSON_STRICT_PARSE_MODE == 2
    _skip_blank(&parse_val);
    if (parse_val.str[parse_val.offset] != '{' && parse_val.str[parse_val.offset] != '[') {
        JsonErr("The first object isn't object or array!\n");
        goto end;
    }
#endif

    ret = _json_sax_parse_value(&parse_val);
#if JSON_STRICT_PARSE_MODE
    if (ret == 0) {
        _skip_blank(&parse_val);
        if (parse_val.str[parse_val.offset]) {
            JsonErr("Extra trailing characters!\n%s\n", parse_val.str + parse_val.offset);
            ret = -1;
        }
    }
#endif

#if JSON_STRICT_PARSE_MODE == 2
end:
#endif
    if (choice->path) {
        if (parse_val.str)
            json_free(parse_val.str);
        if (parse_val.fd >= 0)
            close(parse_val.fd);
    }

    return ret;
}
#endif
