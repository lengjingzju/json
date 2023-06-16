/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2019-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* URL: https://github.com/lengjingzju/json *
*******************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include "json.h"

#define type_member                     jkey.type
#define key_member                      jkey.str
#define str_member                      vstr.str

/*
 * Using grisu2 to convert double to string
 */
#ifndef JSON_ENABLE_GRISU2_DTOA
#define JSON_ENABLE_GRISU2_DTOA         1
#endif

/*
 * Using table lookup methods to accelerate division, etc
 */
#ifndef GRISU2_USING_LUT_ACCELERATE
#define GRISU2_USING_LUT_ACCELERATE     1
#endif

/*
 * JSON_STRICT_PARSE_MODE can be: 0 / 1 / 2
 * 0: not strict mode
 * 1: will enable more checks
 * 2: further turns off some features that are not standard, such as hexadecimal
 */
#define JSON_STRICT_PARSE_MODE          1

/**************** debug ****************/

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

#if defined(__GNUC__) || defined(__clang__)
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

#define JSON_ITEM_NUM_PLUS_DEF          16
#define JSON_POOL_MEM_SIZE_DEF          8096

/* print choice size */
#define JSON_PRINT_UTF16_SUPPORT        0
#define JSON_PRINT_NUM_PLUS_DEF         64
#define JSON_PRINT_SIZE_PLUS_DEF        1024
#define JSON_FORMAT_ITEM_SIZE_DEF       32
#define JSON_UNFORMAT_ITEM_SIZE_DEF     24

/* file parse choice size */
#define JSON_PARSE_ERROR_STR            "Z"
#define JSON_PARSE_READ_SIZE_DEF        8096
#define JSON_PARSE_NUM_DIV_DEF          8

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

unsigned int json_string_hash_code(json_string_t *jstr)
{
    unsigned int i = 0, hash = 0;

    json_string_info_update(jstr);
    if (!jstr->len)
        return 0;

    for (i = 0; i < jstr->len; ++i)
        hash = (hash << 5) - hash + jstr->str[i];
    return hash;
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

int json_get_object_size(json_object *json)
{
    int count = 0;
    json_object *pos = NULL;

    if (json->type_member == JSON_OBJECT) {
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

json_object *json_search_object_item(json_items_t *items, json_string_t *jkey, unsigned int hash)
{
    int left = 0, right = 0, middle = 0, i = 0, count = 0;
    json_object *json = NULL;
    if (!hash)
        hash = json_string_hash_code(jkey);

    count = items->count;
    if (!count)
        return NULL;

    right = count - 1;
    while (left <= right) {
        middle = (left + right) >> 1;
        if (hash == items->items[middle].hash) {
            if (!items->conflicted)
                return items->items[middle].json;

            if (jkey->len) {
                for (i = middle; i < count; ++i) {
                    if (hash != items->items[i].hash)
                        break;
                    json = items->items[i].json;
                    if (jkey->len == json->jkey.len && memcmp(jkey->str, json->key_member, jkey->len) == 0)
                        return json;
                }
                for (i = middle - 1; i >= 0; --i) {
                    if (hash != items->items[i].hash)
                        break;
                    json = items->items[i].json;
                    if (jkey->len == json->jkey.len && memcmp(jkey->str, json->key_member, jkey->len) == 0)
                        return json;
                }
            } else {
                for (i = middle; i < count; ++i) {
                    if (hash != items->items[i].hash)
                        break;
                    json = items->items[i].json;
                    if (!json->jkey.len)
                        return json;
                }
                for (i = middle - 1; i >= 0; --i) {
                    if (hash != items->items[i].hash)
                        break;
                    json = items->items[i].json;
                    if (!json->jkey.len)
                        return json;
                }
            }
            return NULL;
        }

        if (hash > items->items[middle].hash)
            left = middle + 1;
        else
            right = middle - 1;
    }

    return NULL;
}

static int _json_hash_cmp(const void *a, const void *b)
{
    unsigned int ha = ((const json_item_t *)a)->hash;
    unsigned int hb = ((const json_item_t *)b)->hash;
    return (int)(ha - hb);
}

static inline void json_object_items_sort(json_items_t *items)
{
    items->conflicted = 0;
    if (items->count > 1) {
        unsigned int i = 0;
        qsort(items->items, items->count, sizeof(json_item_t), _json_hash_cmp);
        for (i = 1; i < items->count; ++i) {
            if (items->items[i-1].hash == items->items[i].hash) {
                items->conflicted = 1;
                break;
            }
        }
    }
}

void json_free_items(json_items_t *items)
{
    if (items->items)
        json_free(items->items);
    items->items = NULL;
    items->conflicted = 0;
    items->total = 0;
    items->count = 0;
}

int json_get_items(json_object *json, json_items_t *items)
{
    json_object *pos = NULL;

    items->count = 0;
    if (json->type_member != JSON_ARRAY && json->type_member != JSON_OBJECT)
        return -1;

    json_list_for_each_entry(pos, &json->value.head, list) {
        if (items->count == items->total) {
            items->total += JSON_ITEM_NUM_PLUS_DEF;
            if ((items->items = json_realloc(items->items, items->total * sizeof(json_item_t))) == NULL) {
                JsonErr("malloc failed!\n");
                goto err;
            }
        }

        if (json->type_member == JSON_OBJECT)
            items->items[items->count].hash = json_string_hash_code(&pos->jkey);
        items->items[items->count++].json = pos;
    }

    if (json->type_member == JSON_OBJECT)
        json_object_items_sort(items);
    return 0;
err:
    json_free_items(items);
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

int json_add_item_to_object(json_object *object, json_object *item)
{
    if (object->type_member == JSON_OBJECT) {
        json_list_add_tail(&item->list, &object->value.head);
        return 0;
    }
    return -1;
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

int json_replace_item_in_object(json_object *object, json_object *new_item)
{
    json_object *item = NULL, *prev = NULL;

    if (object->type_member == JSON_OBJECT) {
        if ((item = json_get_object_item(object, new_item->key_member, &prev)) != NULL) {
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
        json_list_add_tail(&node->list, &array->value.head);
        return 0;
    }

    return -1;
}

int json_copy_item_to_object(json_object *object, json_object *item)
{
    json_object *node = NULL;

    if (object->type_member == JSON_OBJECT) {
        if ((node = json_deepcopy(item)) == NULL) {
            JsonErr("copy failed!\n");
            return -1;
        }
        json_list_add_tail(&node->list, &object->value.head);
        return 0;
    }

    return -1;
}

json_object *json_add_new_item_to_array(json_object *array, json_type_t type, void *value)
{
    json_object *item = NULL;

    if (array->type_member == JSON_ARRAY) {
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
        case JSON_ARRAY:
        case JSON_OBJECT:
            if ((item = json_create_item(type, value)) == NULL) {
                JsonErr("create item failed!\n");
                return NULL;
            }
            json_list_add_tail(&item->list, &array->value.head);
            return item;
        default:
            JsonErr("not support json type.\n");
            return NULL;
        }
    }

    return NULL;
}

json_object *json_add_new_item_to_object(json_object *object, json_type_t type, json_string_t *jkey, void *value)
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
        case JSON_ARRAY:
        case JSON_OBJECT:
            if ((item = json_create_item(type, value)) == NULL) {
                JsonErr("create item failed!\n");
                return NULL;
            }
            if (json_set_key(item, jkey) < 0) {
                JsonErr("add key failed!\n");
                json_del_object(item);
                return NULL;
            }
            json_list_add_tail(&item->list, &object->value.head);
            return item;
        default:
            JsonErr("not support json type.\n");
            return NULL;
        }
    }

    return NULL;
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

int pjson_replace_item_in_array(json_object *array, int seq, json_object *new_item)
{
    json_object *item = NULL, *prev = NULL;

    if (array->type_member == JSON_ARRAY) {
        if ((item = json_get_array_item(array, seq, &prev)) != NULL) {
            json_list_del(&item->list, &prev->list, &array->value.head);
            json_list_add(&new_item->list, &prev->list, &array->value.head);
        } else {
            json_list_add_tail(&new_item->list, &array->value.head);
        }

        return 0;
    }

    return -1;
}

int pjson_replace_item_in_object(json_object *object, json_object *new_item)
{
    json_object *item = NULL, *prev = NULL;

    if (object->type_member == JSON_OBJECT) {
        if ((item = json_get_object_item(object, new_item->key_member, &prev)) != NULL) {
            json_list_del(&item->list, &prev->list, &object->value.head);
            json_list_add(&new_item->list, &prev->list, &object->value.head);
        } else {
            json_list_add_tail(&new_item->list, &object->value.head);
        }

        return 0;
    }

    return -1;
}

json_object *pjson_deepcopy(json_object *json, json_mem_t *mem)
{
    json_object *new_json = NULL;
    json_object *item = NULL, *node = NULL;

    switch (json->type_member) {
    case JSON_NULL:   new_json = pjson_create_null(mem);                          break;
    case JSON_BOOL:   new_json = pjson_create_bool(json->value.vnum.vbool, mem);  break;
    case JSON_INT:    new_json = pjson_create_int(json->value.vnum.vint, mem);    break;
    case JSON_HEX:    new_json = pjson_create_hex(json->value.vnum.vhex, mem);    break;
#if JSON_LONG_LONG_SUPPORT
    case JSON_LINT:   new_json = pjson_create_lint(json->value.vnum.vlint, mem);  break;
    case JSON_LHEX:   new_json = pjson_create_lhex(json->value.vnum.vlhex, mem);  break;
#endif
    case JSON_DOUBLE: new_json = pjson_create_double(json->value.vnum.vdbl, mem); break;
    case JSON_STRING: new_json = pjson_create_string(&json->value.vstr, mem);     break;
    case JSON_ARRAY:  new_json = pjson_create_array(mem);                         break;
    case JSON_OBJECT: new_json = pjson_create_object(mem);                        break;
    default:                                                                      break;
    }

    if (new_json) {
        if (pjson_string_strdup(&json->jkey, &new_json->jkey, &mem->key_mgr) < 0) {
            JsonErr("add key failed!\n");
            return NULL;
        }
        if (json->type_member == JSON_ARRAY || json->type_member == JSON_OBJECT) {
            json_list_for_each_entry(item, &json->value.head, list) {
                if ((node = pjson_deepcopy(item, mem)) == NULL) {
                    JsonErr("copy failed!\n");
                    return NULL;
                }
                json_list_add_tail(&node->list, &new_json->value.head);
            }
        }
    }

    return new_json;
}

int pjson_copy_item_to_array(json_object *array, json_object *item, json_mem_t *mem)
{
    json_object *node = NULL;

    if (array->type_member == JSON_ARRAY) {
        if ((node = pjson_deepcopy(item, mem)) == NULL) {
            JsonErr("copy failed!\n");
            return -1;
        }
        json_list_add_tail(&node->list, &array->value.head);
        return 0;
    }

    return -1;
}

int pjson_copy_item_to_object(json_object *object, json_object *item, json_mem_t *mem)
{
    json_object *node = NULL;

    if (object->type_member == JSON_OBJECT) {
        if ((node = pjson_deepcopy(item, mem)) == NULL) {
            JsonErr("copy failed!\n");
            return -1;
        }
        json_list_add_tail(&node->list, &object->value.head);
        return 0;
    }

    return -1;
}

json_object *pjson_add_new_item_to_array(json_object *array, json_type_t type, void *value, json_mem_t *mem)
{
    json_object *item = NULL;

    if (array->type_member == JSON_ARRAY) {
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
        case JSON_ARRAY:
        case JSON_OBJECT:
            if ((item = pjson_create_item(type, value, mem)) == NULL) {
                JsonErr("create item failed!\n");
                return NULL;
            }
            json_list_add_tail(&item->list, &array->value.head);
            return item;
        default:
            JsonErr("not support json type.\n");
            return NULL;
        }
    }

    return NULL;
}

json_object *pjson_add_new_item_to_object(json_object *object, json_type_t type, json_string_t *jkey, void *value, json_mem_t *mem)
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
        case JSON_ARRAY:
        case JSON_OBJECT:
            if ((item = pjson_create_item(type, value, mem)) == NULL) {
                JsonErr("create item failed!\n");
                return NULL;
            }
            if (pjson_set_key(item, jkey, mem) < 0) {
                JsonErr("add key failed!\n");
                return NULL;
            }
            json_list_add_tail(&item->list, &object->value.head);
            return item;
        default:
            JsonErr("not support json type.\n");
            return NULL;
        }
    }

    return NULL;
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
    while (j < h) {                         \
        k = i - j - 1;                      \
        t = s[k];                           \
        s[k] = s[j];                        \
        s[j++] = t;                         \
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

#if JSON_ENABLE_GRISU2_DTOA

/*
 * ********** REFERENCE **********
 * URL: https://github.com/miloyip/dtoa-benchmark
 * Section: src/grisu, src/milo
 */

#define DIY_SIGNIFICAND_SIZE        64                  /* Symbol: 1 bit, Exponent, 11 bits, Mantissa, 52 bits */
#define DP_SIGNIFICAND_SIZE         52                  /* Mantissa, 52 bits */
#define DP_EXPONENT_OFFSET          0x3FF               /* Exponent offset is 0x3FF */
#define DP_EXPONENT_MASK            0x7FF0000000000000  /* Exponent Mask, 11 bits */
#define DP_SIGNIFICAND_MASK         0x000FFFFFFFFFFFFF  /* Mantissa Mask, 52 bits */
#define DP_HIDDEN_BIT               0x0010000000000000  /* Integer bit for Mantissa */
#define D_1_LOG2_10                 0.30102999566398114 /* 1/lg(10) */

typedef struct diy_fp_t {
    uint64_t f;
    int e;
} diy_fp_t;

#if 0
static void print_powers_ten_i(void)
{
    /*
     * min value of e is -1022 - 52 - 63 = -1137;
     * max value of e is  1023 - 52 - 0 = 971;
     * Range of (Exponent_Value - Exponent_Offset) is (-1022, 1023)
     * Divisor of Mantissa is pow(2,52)
     * Range of u64 right shift is (0, 63)
     */
    int e = 0, k = 0, index = 0;
    int min = -1137, max = 971, cnt = 0;
    double dk = 0;

    printf("static const uint8_t powers_ten_i[] = {");
    for (e = min; e <= max; ++e) {
        if (cnt++ % 20 == 0)
            printf("\n    ");
        dk = (-61 - e) * D_1_LOG2_10 + 347;
        k = (int)dk;
        if (dk - k > 0.0)
            ++k;
        index = (unsigned int)((k >> 3) + 1);
        printf("%-2d", index);
        if (e != max)
            printf(", ");
    }
    printf("\n};\n\n");
}

static void print_number_lut(void)
{
    int i = 0, index = 0, max = 0;

    max = (100 >> 1) - 1;
    printf("static const uint8_t num_10_lut[] = {");
    for (i = 0; i <= max; ++i) {
        if (i % 25 == 0)
            printf("\n    ");
        index = i / 5;
        printf("%d", index);
        if (i != max)
            printf(", ");
    }
    printf("\n};\n\n");

    max = (1000 >> 2) - 1;
    printf("static const uint8_t num_100_lut[] = {");
    for (i = 0; i <= max; ++i) {
        if (i % 25 == 0)
            printf("\n    ");
        index = i / 25;
        printf("%d", index);
        if (i != max)
            printf(", ");
    }
    printf("\n};\n\n");

    max = (10000 >> 3) - 1;
    printf("static const uint8_t num_1000_lut[] = {");
    for (i = 0; i <= max; ++i) {
        if (i % 25 == 0)
            printf("\n    ");
        index = i / 125;
        printf("%d", index);
        if (i != max)
            printf(", ");
    }
    printf("\n};\n\n");
}
#endif

#define get_10_multiple(v)          (((v)<<1) + ((v)<<3))
#define get_100_multiple(v)         (((v)<<2) + ((v)<<5) + ((v)<<6))
#define get_1000_multiple(v)        (((v)<<3) + ((v)<<5) + ((v)<<6) + ((v)<<7) + ((v)<<8) + ((v)<<9))

#if GRISU2_USING_LUT_ACCELERATE
static const uint8_t num_10_lut[] = {
    0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9
};
#define get_10_quotient(v)          num_10_lut[(v)>>1]

static const uint8_t num_100_lut[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9
};
#define get_100_quotient(v)         num_100_lut[(v)>>2]

static const uint8_t num_1000_lut[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9
};
#define get_1000_quotient(v)        num_1000_lut[(v)>>3]

#else

#define get_10_quotient(v)          ((v) / 10)
#define get_100_quotient(v)         ((v) / 100)
#define get_1000_quotient(v)        ((v) / 1000)
#endif

static inline diy_fp_t cached_power(int e, int* K)
{
    // 10^-348, 10^-340, ..., 10^340
    static const uint64_t powers_ten[] = {
        0xfa8fd5a0081c0288, 0xbaaee17fa23ebf76, 0x8b16fb203055ac76, 0xcf42894a5dce35ea,
        0x9a6bb0aa55653b2d, 0xe61acf033d1a45df, 0xab70fe17c79ac6ca, 0xff77b1fcbebcdc4f,
        0xbe5691ef416bd60c, 0x8dd01fad907ffc3c, 0xd3515c2831559a83, 0x9d71ac8fada6c9b5,
        0xea9c227723ee8bcb, 0xaecc49914078536d, 0x823c12795db6ce57, 0xc21094364dfb5637,
        0x9096ea6f3848984f, 0xd77485cb25823ac7, 0xa086cfcd97bf97f4, 0xef340a98172aace5,
        0xb23867fb2a35b28e, 0x84c8d4dfd2c63f3b, 0xc5dd44271ad3cdba, 0x936b9fcebb25c996,
        0xdbac6c247d62a584, 0xa3ab66580d5fdaf6, 0xf3e2f893dec3f126, 0xb5b5ada8aaff80b8,
        0x87625f056c7c4a8b, 0xc9bcff6034c13053, 0x964e858c91ba2655, 0xdff9772470297ebd,
        0xa6dfbd9fb8e5b88f, 0xf8a95fcf88747d94, 0xb94470938fa89bcf, 0x8a08f0f8bf0f156b,
        0xcdb02555653131b6, 0x993fe2c6d07b7fac, 0xe45c10c42a2b3b06, 0xaa242499697392d3,
        0xfd87b5f28300ca0e, 0xbce5086492111aeb, 0x8cbccc096f5088cc, 0xd1b71758e219652c,
        0x9c40000000000000, 0xe8d4a51000000000, 0xad78ebc5ac620000, 0x813f3978f8940984,
        0xc097ce7bc90715b3, 0x8f7e32ce7bea5c70, 0xd5d238a4abe98068, 0x9f4f2726179a2245,
        0xed63a231d4c4fb27, 0xb0de65388cc8ada8, 0x83c7088e1aab65db, 0xc45d1df942711d9a,
        0x924d692ca61be758, 0xda01ee641a708dea, 0xa26da3999aef774a, 0xf209787bb47d6b85,
        0xb454e4a179dd1877, 0x865b86925b9bc5c2, 0xc83553c5c8965d3d, 0x952ab45cfa97a0b3,
        0xde469fbd99a05fe3, 0xa59bc234db398c25, 0xf6c69a72a3989f5c, 0xb7dcbf5354e9bece,
        0x88fcf317f22241e2, 0xcc20ce9bd35c78a5, 0x98165af37b2153df, 0xe2a0b5dc971f303a,
        0xa8d9d1535ce3b396, 0xfb9b7cd9a4a7443c, 0xbb764c4ca7a44410, 0x8bab8eefb6409c1a,
        0xd01fef10a657842c, 0x9b10a4e5e9913129, 0xe7109bfba19c0c9d, 0xac2820d9623bf429,
        0x80444b5e7aa7cf85, 0xbf21e44003acdd2d, 0x8e679c2f5e44ff8f, 0xd433179d9c8cb841,
        0x9e19db92b4e31ba9, 0xeb96bf6ebadf77d9, 0xaf87023b9bf0ee6b
    };

    static const int16_t powers_ten_e[] = {
        -1220, -1193, -1166, -1140, -1113, -1087, -1060, -1034, -1007,  -980,
         -954,  -927,  -901,  -874,  -847,  -821,  -794,  -768,  -741,  -715,
         -688,  -661,  -635,  -608,  -582,  -555,  -529,  -502,  -475,  -449,
         -422,  -396,  -369,  -343,  -316,  -289,  -263,  -236,  -210,  -183,
         -157,  -130,  -103,   -77,   -50,   -24,     3,    30,    56,    83,
          109,   136,   162,   189,   216,   242,   269,   295,   322,   348,
          375,   402,   428,   455,   481,   508,   534,   561,   588,   614,
          641,   667,   694,   720,   747,   774,   800,   827,   853,   880,
          907,   933,   960,   986,  1013,  1039,  1066
    };

#if GRISU2_USING_LUT_ACCELERATE
    /* powers_ten_i is got from print_powers_ten_i */
    static const uint8_t powers_ten_i[] = {
        84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84, 84,
        84, 84, 84, 84, 84, 84, 84, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83,
        83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 83, 82, 82, 82, 82, 82, 82, 82,
        82, 82, 82, 82, 82, 82, 82, 82, 82, 82, 82, 82, 82, 82, 82, 82, 82, 82, 82, 82,
        81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81,
        81, 81, 81, 81, 81, 81, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
        80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 79, 79, 79, 79, 79, 79, 79,
        79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79, 79,
        78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78,
        78, 78, 78, 78, 78, 78, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
        77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 76, 76, 76, 76, 76, 76, 76,
        76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 76, 75,
        75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75, 75,
        75, 75, 75, 75, 75, 75, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
        74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 73, 73, 73, 73, 73, 73, 73,
        73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 73, 72,
        72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
        72, 72, 72, 72, 72, 72, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71,
        71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 70, 70, 70, 70, 70, 70, 70, 70,
        70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 69,
        69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69, 69,
        69, 69, 69, 69, 69, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68,
        68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 67, 67, 67, 67, 67, 67, 67, 67,
        67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 66,
        66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
        66, 66, 66, 66, 66, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
        65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 63, 63,
        63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
        63, 63, 63, 63, 63, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62,
        62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 62, 61, 61, 61, 61, 61, 61, 61, 61, 61,
        61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 60, 60,
        60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60,
        60, 60, 60, 60, 60, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59,
        59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 59, 58, 58, 58, 58, 58, 58, 58, 58, 58,
        58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58, 57, 57,
        57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
        57, 57, 57, 57, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 55, 55, 55, 55, 55, 55, 55, 55, 55,
        55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 55, 54, 54, 54,
        54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54,
        54, 54, 54, 54, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53,
        53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 52, 52, 52, 52, 52, 52, 52, 52, 52,
        52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 52, 51, 51, 51,
        51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
        51, 51, 51, 51, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
        50, 50, 50, 50, 50, 50, 50, 50, 50, 50, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
        49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 48, 48, 48,
        48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
        48, 48, 48, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 47,
        47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46,
        46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 45, 45, 45,
        45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
        45, 45, 45, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 44,
        44, 44, 44, 44, 44, 44, 44, 44, 44, 44, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43,
        43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 43, 42, 42, 42, 42,
        42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42,
        42, 42, 42, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
        41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
        40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 39, 39, 39, 39,
        39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
        39, 39, 39, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
        38, 38, 38, 38, 38, 38, 38, 38, 38, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
        37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 37, 36, 36, 36, 36,
        36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
        36, 36, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
        35, 35, 35, 35, 35, 35, 35, 35, 35, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34,
        34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 33, 33, 33, 33,
        33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
        33, 33, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
        32, 32, 32, 32, 32, 32, 32, 32, 32, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31,
        31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 30, 30, 30, 30, 30,
        30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
        30, 30, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
        29, 29, 29, 29, 29, 29, 29, 29, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
        28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 27, 27, 27, 27, 27,
        27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
        27, 27, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
        26, 26, 26, 26, 26, 26, 26, 26, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
        25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 24, 24, 24, 24, 24,
        24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
        24, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
        23, 23, 23, 23, 23, 23, 23, 23, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22,
        22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 21, 21, 21, 21, 21, 21,
        21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
        21, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
        20, 20, 20, 20, 20, 20, 20, 20, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
        19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 18, 18, 18, 18, 18, 18,
        18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
        18, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
        17, 17, 17, 17, 17, 17, 17, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
        14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
        14, 14, 14, 14, 14, 14, 14, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
        13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 12, 12, 12, 12, 12, 12,
        12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
        11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
        11, 11, 11, 11, 11, 11, 11, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,  9,  9,  9,  9,  9,  9,  9,
         9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
         8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
         8,  8,  8,  8,  8,  8,  8,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
         7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  6,  6,  6,  6,  6,  6,  6,
         6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
         5,  5,  5,  5,  5,  5,  5,  5,  5
    };

    const unsigned int index = powers_ten_i[e + 1137];
#else
    const double dk = (-61 - e) * D_1_LOG2_10 + 347;
    int k = (int)dk; /* dk must be positive, so can do ceiling in positive */
    if (dk - k > 0.0)
        ++k;
    const unsigned int index = (unsigned int)((k >> 3) + 1);
#endif

    diy_fp_t res;
    res.f = powers_ten[index];
    res.e = powers_ten_e[index];
    *K = -(-348 + (int)(index << 3)); /* decimal exponent no need lookup table */
    return res;
}

static inline int get_u64_prefix0(uint64_t f)
{
#if defined(_MSC_VER) && defined(_M_AMD64)
    unsigned long index;
    _BitScanReverse64(&index, f);
    return (63 - index);
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_clzll(f);
#else
    int index = DP_SIGNIFICAND_SIZE + 1; /* max value of diy_fp_t.f is smaller than pow(2, 54) */
    while (!(f & ((uint64_t)1 << index)))
        --index;
    return (63 - index);
#endif
}

static diy_fp_t diy_fp_multiply(diy_fp_t x, diy_fp_t y)
{
#if defined(_MSC_VER) && defined(_M_AMD64)
    uint64_t h;
    const uint64_t l = _umul128(x.f, y.f, &h);
    if (l & ((uint64_t)1 << 63)) /* rounding */
        ++h;
#elif (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6) || __clang_major__ >= 9) && (__WORDSIZE == 64)
    __extension__ typedef unsigned __int128 uint128;
    const uint128 p = (uint128)x.f * (uint128)y.f;
    uint64_t h = (uint64_t)(p >> 64);
    if ((uint64_t)p & ((uint64_t)1 << 63)) /* rounding */
        ++h;
#else
    const uint64_t M32 = 0XFFFFFFFF;
    const uint64_t a = x.f >> 32;
    const uint64_t b = x.f & M32;
    const uint64_t c = y.f >> 32;
    const uint64_t d = y.f & M32;

    const uint64_t ac = a * c;
    const uint64_t bc = b * c;
    const uint64_t ad = a * d;
    const uint64_t bd = b * d;

    uint64_t tmp = (bd >> 32) + (ad & M32) + (bc & M32);
    tmp += 1U << 31; /* mult_round */
    const uint64_t h = ac + (ad >> 32) + (bc >> 32) + (tmp >> 32);
#endif

    diy_fp_t r;
    r.f = h;
    r.e = x.e + y.e + 64;
    return r;
}

static inline void grisu_round(char* buffer, int len,
    uint64_t delta, uint64_t rest, uint64_t ten_kappa, uint64_t W_pv)
{
    uint64_t t = rest + ten_kappa;
    while (rest < W_pv && delta >= t && (W_pv - rest > t - W_pv || t < W_pv)) {
        --buffer[len - 1];
        rest += ten_kappa;
        t += ten_kappa;
    }
}

static inline void digit_gen(diy_fp_t Wv, diy_fp_t Wp, uint64_t delta, char* buffer, int* len, int* K)
{
    static const uint32_t divs[] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};
    uint32_t p1;
    uint64_t p2, p3;
    int d, kappa;
    diy_fp_t one, W_pv;

    one.f = ((uint64_t)1) << -Wp.e;
    one.e = Wp.e;

    W_pv.f = Wp.f - Wv.f;
    W_pv.e = Wp.e;

    p1 = Wp.f >> -one.e; /* Mp_cut */
    p2 = Wp.f & (one.f - 1);

    /* count decimal digit 32bit */
    if (p1 >= 100000) {
        if      (p1 < 1000000)    kappa = 6;
        else if (p1 < 10000000)   kappa = 7;
        else if (p1 < 100000000)  kappa = 8;
        else if (p1 < 1000000000) kappa = 9;
        else                      kappa = 10;
    } else {
        if      (p1 >= 10000)     kappa = 5;
        else if (p1 >= 1000)      kappa = 4;
        else if (p1 >= 100)       kappa = 3;
        else if (p1 >= 10)        kappa = 2;
        else                      kappa = 1;
    }

    *len = 0;
    while (kappa > 0) {
        switch (kappa) {
        case 10: d = p1 / 1000000000      ; p1 -= d * 1000000000      ; break;
        case  9: d = p1 /  100000000      ; p1 -= d *  100000000      ; break;
        case  8: d = p1 /   10000000      ; p1 -= d *   10000000      ; break;
        case  7: d = p1 /    1000000      ; p1 -= d *    1000000      ; break;
        case  6: d = p1 /     100000      ; p1 -= d *     100000      ; break;
        case  5: d = p1 /      10000      ; p1 -= d *      10000      ; break;
        case  4: d = get_1000_quotient(p1); p1 -= get_1000_multiple(d); break;
        case  3: d = get_100_quotient(p1) ; p1 -= get_100_multiple(d) ; break;
        case  2: d = get_10_quotient(p1)  ; p1 -= get_10_multiple(d)  ; break;
        case  1: d = p1                   ; p1  =              0      ; break;
        default:
#if defined(_MSC_VER)
            __assume(0);
#elif __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
            __builtin_unreachable();
#else
            d = 0;
#endif
            break;
        }

        buffer[(*len)++] = '0' + d; /* Mp_inv1 */
        --kappa;
        p3 = (((uint64_t)p1) << -one.e) + p2;
        if (p3 <= delta) { /* Mp_delta */
            *K += kappa;
            grisu_round(buffer, *len, delta, p3, ((uint64_t)divs[kappa]) << -one.e, W_pv.f);
            return;
        }
    }

    while (1) {
        p2 = (p2 << 1) + (p2 << 3);
        delta = (delta << 1) + (delta << 3);
        d = p2 >> -one.e;

        buffer[(*len)++] = '0' + d;
        --kappa;
        p2 &= one.f - 1;
        if (p2 < delta) {
            *K += kappa;
            grisu_round(buffer, *len, delta, p2, one.f, W_pv.f * divs[-kappa]);
            return;
        }
    }
}

static inline void grisu2(double value, char* buffer, int* length, int* K)
{
    diy_fp_t v, w_v, w_m, w_p, c_mk, Wv, Wm, Wp;
    int z_v, z_p;

    /* convert double to diy_fp */
    union {double d; uint64_t n;} u = {.d = value};
    int biased_e = (u.n & DP_EXPONENT_MASK) >> DP_SIGNIFICAND_SIZE; /* Exponent */
    uint64_t significand = u.n & DP_SIGNIFICAND_MASK; /* Mantissa */

    if (biased_e != 0) { /* normalized double */
        v.f = significand + DP_HIDDEN_BIT; /* Normalized double has a extra integer bit for Mantissa */
        v.e = biased_e - DP_EXPONENT_OFFSET - DP_SIGNIFICAND_SIZE; /* Exponent offset: -1023, divisor of Mantissa: pow(2,52) */
    } else { /* no-normalized double */
        v.f = significand; /* Non-normalized double doesn't have a extra integer bit for Mantissa */
        v.e = 1 - DP_EXPONENT_OFFSET - DP_SIGNIFICAND_SIZE; /* Fixed Exponent: -1022, divisor of Mantissa: pow(2,52) */
    }

    /* normalize v and boundaries */
    z_v = get_u64_prefix0(v.f);
    w_v.f = v.f << z_v;
    w_v.e = v.e - z_v;

    w_p.f = (v.f << 1) + 1;
    w_p.e = v.e - 1;
    z_p = get_u64_prefix0(w_p.f);
    w_p.f <<= z_p;
    w_p.e -= z_p;

    if (v.f == DP_HIDDEN_BIT) {
        w_m.f = (v.f << 2) - 1;
        w_m.e = v.e - 2;
    } else {
        w_m.f = (v.f << 1) - 1;
        w_m.e = v.e - 1;
    }
    w_m.f <<= w_m.e - w_p.e;
    w_m.e = w_p.e;

    c_mk = cached_power(w_p.e, K);
    Wv   = diy_fp_multiply(w_v, c_mk);
    Wp   = diy_fp_multiply(w_p, c_mk);
    Wm   = diy_fp_multiply(w_m, c_mk);

    ++Wm.f;
    --Wp.f;
    digit_gen(Wv, Wp, Wp.f - Wm.f, buffer, length, K);
}

static inline int fill_exponent(int K, char* buffer)
{
    int i = 0, k = 0;

    if (K < 0) {
        buffer[i++] = '-';
        K = -K;
    }

    if (K < 10) {
        buffer[i++] = '0' + K;
        buffer[i] = '\0';
        return i;
    }

    if (K >= 100) {
        k = get_100_quotient(K);
        K -= get_100_multiple(k);
        buffer[i++] = '0' + k;
    }

    k = get_10_quotient(K);
    K -= get_10_multiple(k);
    buffer[i++] = '0' + k;
    buffer[i++] = '0' + K;
    buffer[i] = '\0';

    return i;
}

static inline int prettify_string(char* buffer, int length, int K)
{
    /*
     * v = buffer * 10^k
     * kk is such that 10^(kk-1) <= v < 10^kk
     * this way kk gives the position of the comma.
     */
    const int kk = length + K;
    int offset;

    if (kk <= 21) {
        if (length <= kk) {
            /*
             * 1234e7 -> 12340000000
             * the first digits are already in. Add some 0s and call it a day.
             * the 21 is a personal choice. Only 16 digits could possibly be relevant.
             * Basically we want to print 12340000000 rather than 1234.0e7 or 1.234e10
             */
            memset(&buffer[length], '0', K);
            buffer[kk] = '.';
            buffer[kk + 1] = '0';
            buffer[kk + 2] = '\0';
            return (kk + 2);
        }

        if (kk > 0) {
            /*
             * 1234e-2 -> 12.34
             * comma number. Just insert a '.' at the correct location.
             */
            memmove(&buffer[kk + 1], &buffer[kk], length - kk);
            buffer[kk] = '.';
            buffer[length + 1] = '\0';
            return length + 1;
        }

        if (kk > -6) {
            /*
             * 1234e-6 -> 0.001234
             * something like 0.000abcde.
             * add '0.' and some '0's
             */
            offset = 2 - kk;
            memmove(&buffer[offset], &buffer[0], length);
            buffer[0] = '0';
            buffer[1] = '.';
            memset(&buffer[2], '0', offset - 2);
            buffer[length + offset] = '\0';
            return length + offset;
        }
    }

    if (length == 1) {
        /*
         * 1e30
         * just add 'e...'
         * fill_positive_fixnum will terminate the string
         */
        buffer[1] = 'e';
        return (2 + fill_exponent(kk - 1, &buffer[2]));
    }

    /*
     * 1234e30 -> 1.234e33
     * leave the first digit. then add a '.' and at the end 'e...'
     * fill_fixnum will terminate the string.
     */
    memmove(&buffer[2], &buffer[1], length - 1);
    buffer[1] = '.';
    buffer[length + 1] = 'e';
    return (length + 2 + fill_exponent(kk - 1, &buffer[length + 2]));
}

static int grisu2_dtoa(double value, char* buffer)
{
    int length, K, pre = 0;

    if (value == 0) {
        memcpy(buffer, "0.0", 4);
        return 3;
    }

    if (value < 0) {
        *buffer++ = '-';
        value = -value;
        pre = 1;
    }

    grisu2(value, buffer, &length, &K);
    return (pre + prettify_string(buffer, length, K));
}

#define double_to_string grisu2_dtoa

#else
static int double_to_string(double n, char *c)
{
    char *s = c;
    long long int x = 0;
    double y = 0;
    double z = n < 0 ? -n : n;
    int a = 0, i = 0;

    /* 16 is equal to MB_LEN_MAX */
    if (z > LLONG_MAX || z <= 1e-16) {
        return sprintf(c, "%1.15g", n);
    }

    x = (long long int)z;
    y = z - x;
    if (x) {
        a = lint_to_string(n < 0 ? -x : x, s);
        s += a;
    } else {
        *s++ = '0';
        a = 1;
    }

    *s++ = '.';
    ++a;
    if (y == 0) {
        *s++ = '0';
        *s = '\0';
        return 1 + a;
    }

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
#endif

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
        print_ptr->item_total += JSON_PRINT_NUM_PLUS_DEF;
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

#define _PRINT_PTR_STRNCAT(str, slen)      do {     \
    _PRINT_PTR_REALLOC((slen + 1));                 \
    memcpy(print_ptr->cur, str, slen);              \
    print_ptr->cur += slen;                         \
} while(0)

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

static int _json_print_value(json_print_t *print_ptr, json_object *json)
{
    json_object *parent = NULL, *tmp = NULL;
    json_object **item_array = NULL;
    int item_depth = -1, item_total = 0;

    goto next3;
next1:
    if (unlikely(item_depth >= item_total - 1)) {
        item_total += JSON_ITEM_NUM_PLUS_DEF;
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

next2a:
    if (print_ptr->format_flag) {
        _PRINT_ADDI_FORMAT(print_ptr, item_depth + 1);
        if (unlikely(!json->jkey.len)) {
#if !JSON_STRICT_PARSE_MODE
            JsonErr("key is null!\n");
            goto err;
#else
            _PRINT_PTR_STRNCAT("\"\":\t", 4);
#endif
        } else {
            _JSON_PRINT_STRING(print_ptr, &json->jkey);
            _PRINT_PTR_STRNCAT(":\t", 2);
        }
    } else {
        if (unlikely(!json->jkey.len)) {
#if !JSON_STRICT_PARSE_MODE
            JsonErr("key is null!\n");
            goto err;
#else
            _PRINT_PTR_STRNCAT("\"\":", 3);
#endif
        } else {
            _JSON_PRINT_STRING(print_ptr, &json->jkey);
            _PRINT_PTR_STRNCAT(":", 1);
        }
    }
    goto next3;

next2b:
    if (print_ptr->format_flag) {
        if (json->type_member == JSON_ARRAY)
            _PRINT_ADDI_FORMAT(print_ptr, item_depth + 1);
        else
            _PRINT_PTR_STRNCAT(" ", 1);
    }

next3:
    switch (json->type_member) {
    case JSON_NULL:
        _PRINT_PTR_STRNCAT("null", 4);
        break;
    case JSON_BOOL:
        if (json->value.vnum.vbool) {
            _PRINT_PTR_STRNCAT("true", 4);
        } else {
            _PRINT_PTR_STRNCAT("false", 5);
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
            _PRINT_PTR_STRNCAT("\"\"", 2);
        } else {
            _JSON_PRINT_STRING(print_ptr, &json->value.vstr);
        }
        break;
    case JSON_OBJECT:
        if (unlikely(json->value.head.prev == (struct json_list *)&json->value.head)) {
            _PRINT_PTR_STRNCAT("{}", 2);
            break;
        }
        _PRINT_PTR_STRNCAT("{", 1);
        goto next1;
    case JSON_ARRAY:
        if (unlikely(json->value.head.prev == (struct json_list *)&json->value.head)) {
            _PRINT_PTR_STRNCAT("[]", 2);
            break;
        }
        _PRINT_PTR_STRNCAT("[", 1);
        goto next1;
    default:
        break;
    }
    ++print_ptr->item_count;

next4:
    if (likely(item_depth >= 0)) {
        tmp = (json_object*)(json->list.next);
        if (likely(&tmp->list != (struct json_list *)&parent->value.head)) {
            _PRINT_PTR_STRNCAT(",", 1);
            json = tmp;
            if (parent->type_member == JSON_OBJECT)
                goto next2a;
            else
                goto next2b;
        } else {
            if (parent->type_member == JSON_OBJECT) {
                if (print_ptr->format_flag) {
                    if (item_depth > 0) {
                        _PRINT_ADDI_FORMAT(print_ptr, item_depth);
                    } else {
                        _PRINT_PTR_STRNCAT("\n", 1);
                    }
                }
                _PRINT_PTR_STRNCAT("}", 1);
            } else {
                if (print_ptr->format_flag) {
                    if (json->type_member == JSON_OBJECT || json->type_member == JSON_ARRAY) {
                        if (likely(item_depth > 0)) {
                            _PRINT_ADDI_FORMAT(print_ptr, item_depth);
                        } else {
                            _PRINT_PTR_STRNCAT("\n", 1);
                        }
                    }
                }
                _PRINT_PTR_STRNCAT("]", 1);
            }

            ++print_ptr->item_count;
            if (likely(item_depth > 0)) {
                json = parent;
                parent = item_array[--item_depth];
                goto next4;
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

    if (_json_print_value(&print_val, json) < 0) {
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

    if (likely(print_handle->count > 0
        && !((type == JSON_ARRAY || type == JSON_OBJECT) && (*(json_sax_cmd_t*)value) == JSON_SAX_FINISH))) {
        // add ","
        if (print_handle->array[cur_pos].num > 0)
            _PRINT_PTR_STRNCAT(",", 1);

        // add key
        if (print_handle->array[cur_pos].type == JSON_OBJECT) {
            if (print_ptr->format_flag) {
                if (unlikely(!jkey || !jkey->str || !jkey->str[0])) {
#if !JSON_STRICT_PARSE_MODE
                    JsonErr("key is null!\n");
                    goto err;
#else
                    _PRINT_ADDI_FORMAT(print_ptr, print_handle->count);
                    _PRINT_PTR_STRNCAT("\"\":\t", 4);
#endif
                } else {
                    _PRINT_ADDI_FORMAT(print_ptr, print_handle->count);
                    json_string_info_update(jkey);
                    _JSON_PRINT_STRING(print_ptr, jkey);
                    _PRINT_PTR_STRNCAT(":\t", 2);
                }
            } else {
                if (unlikely(!jkey || !jkey->str || !jkey->str[0])) {
#if !JSON_STRICT_PARSE_MODE
                    JsonErr("key is null!\n");
                    goto err;
#else
                    _PRINT_PTR_STRNCAT("\"\":", 3);
#endif
                } else {
                    json_string_info_update(jkey);
                    _JSON_PRINT_STRING(print_ptr, jkey);
                    _PRINT_PTR_STRNCAT(":", 1);
                }
            }
        } else {
            if (print_ptr->format_flag) {
                if (type == JSON_ARRAY) {
                    _PRINT_ADDI_FORMAT(print_ptr, print_handle->count);
                } else {
                    if (print_handle->array[cur_pos].num > 0) {
                        _PRINT_PTR_STRNCAT(" ", 1);
                    } else {
                        if (type == JSON_OBJECT)
                            _PRINT_ADDI_FORMAT(print_ptr, print_handle->count);
                    }
                }
            }
        }
    }

    // add value
    switch (type) {
    case JSON_NULL:
        _PRINT_PTR_STRNCAT("null", 4);
        break;
    case JSON_BOOL:
        if ((*(bool*)value)) {
            _PRINT_PTR_STRNCAT("true", 4);
        } else {
            _PRINT_PTR_STRNCAT("false", 5);
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
            _PRINT_PTR_STRNCAT("\"\"", 2);
        } else {
            json_string_info_update(jstr);
            _JSON_PRINT_STRING(print_ptr, jstr);
        }
        break;

    case JSON_ARRAY:
    case JSON_OBJECT:
        switch ((*(json_sax_cmd_t*)value)) {
        case JSON_SAX_START:
            if (unlikely(print_handle->count == print_handle->total)) {
                print_handle->total += JSON_PRINT_NUM_PLUS_DEF;
                if (unlikely((print_handle->array = json_realloc(print_handle->array,
                            print_handle->total * sizeof(json_sax_print_depth_t))) == NULL)) {
                    JsonErr("malloc failed!\n");
                    goto err;
                }
            }
            if (type == JSON_OBJECT) {
                _PRINT_PTR_STRNCAT("{", 1);
            } else {
                _PRINT_PTR_STRNCAT("[", 1);
            }
            if (print_handle->count > 0)
                ++print_handle->array[cur_pos].num;
            print_handle->array[print_handle->count].type = type;
            print_handle->array[print_handle->count].num = 0;
            ++print_handle->count;
            break;

        case JSON_SAX_FINISH:
            if (unlikely(print_handle->count == 0 || print_handle->array[cur_pos].type != type)) {
                JsonErr("unexpected array or object finish!\n");
                goto err;
            }
            if (print_ptr->format_flag) {
                if (print_handle->count > 1) {
                    if (print_handle->array[print_handle->count-1].num > 0) {
                        if (type == JSON_OBJECT) {
                            _PRINT_ADDI_FORMAT(print_ptr, cur_pos);
                        } else {
                            if (print_handle->last_type == JSON_OBJECT || print_handle->last_type == JSON_ARRAY)
                                _PRINT_ADDI_FORMAT(print_ptr, cur_pos);
                        }
                    }
                } else {
                    _PRINT_PTR_STRNCAT("\n", 1);
                }
            }
            if (type == JSON_OBJECT) {
                _PRINT_PTR_STRNCAT("}", 1);
            } else {
                _PRINT_PTR_STRNCAT("]", 1);
            }
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
        choice->item_total = JSON_PRINT_NUM_PLUS_DEF;
    if (_print_val_init(&print_handle->print_val, choice) < 0) {
        json_free(print_handle);
        return NULL;
    }

    print_handle->total = JSON_PRINT_NUM_PLUS_DEF;
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
        item_total += JSON_ITEM_NUM_PLUS_DEF;
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
        if (likely(parse_ptr->size - parse_ptr->offset >= 5 && memcmp("false", str, 5) == 0)) {
            item->type_member = JSON_BOOL;
            item->value.vnum.vbool = false;
            _UPDATE_PARSE_OFFSET(5);
        } else {
            JsonPareseErr("invalid next ptr!");
            goto err;
        }
        break;
    case 't':
        if (likely(parse_ptr->size - parse_ptr->offset >= 4 && memcmp("true", str, 4) == 0)) {
            item->type_member = JSON_BOOL;
            item->value.vnum.vbool = true;
            _UPDATE_PARSE_OFFSET(4);
        } else {
            JsonPareseErr("invalid next ptr!");
            goto err;
        }
        break;

    case 'n':
        if (likely(parse_ptr->size - parse_ptr->offset >= 4 && memcmp("null", str, 4) == 0)) {
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
        item_total += JSON_ITEM_NUM_PLUS_DEF;
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
        if (likely(parse_ptr->size - parse_ptr->offset >= 5 && memcmp("false", str, 5) == 0)) {
            item->type_member = JSON_BOOL;
            item->value.vnum.vbool = false;
            _UPDATE_PARSE_OFFSET(5);
        } else {
            JsonPareseErr("invalid next ptr!");
            goto err;
        }
        break;
    case 't':
        if (likely(parse_ptr->size - parse_ptr->offset >= 4 && memcmp("true", str, 4) == 0)) {
            item->type_member = JSON_BOOL;
            item->value.vnum.vbool = true;
            _UPDATE_PARSE_OFFSET(4);
        } else {
            JsonPareseErr("invalid next ptr!");
            goto err;
        }
        break;

    case 'n':
        if (likely(parse_ptr->size - parse_ptr->offset >= 4 && memcmp("null", str, 4) == 0)) {
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
        mem_size = total_size / JSON_PARSE_NUM_DIV_DEF;
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
    parse_ptr->parser.total += JSON_ITEM_NUM_PLUS_DEF;
    if (unlikely((parse_ptr->parser.array = json_malloc(sizeof(json_string_t) * parse_ptr->parser.total)) == NULL)) {
        JsonErr("malloc failed!\n");
        return -1;
    }
    memset(parse_ptr->parser.array, 0, sizeof(json_string_t));
    goto next3;

next1:
    if (unlikely(parse_ptr->parser.index >= parse_ptr->parser.total - 1)) {
        parse_ptr->parser.total += JSON_ITEM_NUM_PLUS_DEF;
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
        if (likely(parse_ptr->size - parse_ptr->offset >= 5 && memcmp("false", str, 5) == 0)) {
            jkey->type = JSON_BOOL;
            value->vnum.vbool = false;
            _UPDATE_PARSE_OFFSET(5);
        } else {
            JsonPareseErr("invalid next ptr!");
            goto err;
        }
        break;
    case 't':
        if (likely(parse_ptr->size - parse_ptr->offset >= 4 && memcmp("true", str, 4) == 0)) {
            jkey->type = JSON_BOOL;
            value->vnum.vbool = true;
            _UPDATE_PARSE_OFFSET(4);
        } else {
            JsonPareseErr("invalid next ptr!");
            goto err;
        }
        break;

    case 'n':
        if (likely(parse_ptr->size - parse_ptr->offset >= 4 && memcmp("null", str, 4) == 0)) {
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
