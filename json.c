/*******************************************
* SPDX-License-Identifier: MIT             *
* Copyright (C) 2019-.... Jing Leng        *
* Contact: Jing Leng <lengjingzju@163.com> *
* URL: https://github.com/lengjingzju/json *
*******************************************/
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include "jnum.h"
#include "json.h"

#if !defined(_MSC_VER)
#include <unistd.h>
#if !defined(_WIN32)
#define O_BINARY                        0
#else
#define O_BINARY                        0x8000 // MinGW-gcc
#endif
#else
#include <io.h>
#define open                            _open
#define close                           _close
#define read                            _read
#define write                           _write
#define ssize_t                         int
#pragma warning(disable: 4996)
#endif

#define type_member                     jkey.type
#define key_member                      jkey.str
#define str_member                      vstr.str

/**************** configuration ****************/

/*
 * Algorithm to convert double to string
 * 0: ljson dtoa developed by Jing Leng, fastest and best
 * 1: sprintf of C standard library, slowest
 * 2: grisu2 dtoa developed by Google
 * 3: dragonbox dtoa developed by Raffaello Giulietti, faster than grisu2
 */
#ifndef JSON_DTOA_ALGORITHM
#define JSON_DTOA_ALGORITHM             0
#endif

#if JSON_DTOA_ALGORITHM == 1
static inline int json_dtoa(double num, char *buffer)
{
    return sprintf(buffer, "%1.16g", num);
}
#elif JSON_DTOA_ALGORITHM == 2
extern int grisu2_dtoa(double num, char *buffer);
#define json_dtoa                       grisu2_dtoa
#elif JSON_DTOA_ALGORITHM == 3
extern int dragonbox_dtoa(double num, char *buffer);
#define json_dtoa                       dragonbox_dtoa
#else
#define json_dtoa                       jnum_dtoa
#endif

/* Whether to allow C-like single-line comments and multi-line comments */
#ifndef JSON_PARSE_SKIP_COMMENT
#define JSON_PARSE_SKIP_COMMENT         0
#endif

/* Whether to allow comma in last element of array or object */
#ifndef JSON_PARSE_LAST_COMMA
#define JSON_PARSE_LAST_COMMA           1
#endif

/* Whether to allow empty key */
#ifndef JSON_PARSE_EMPTY_KEY
#define JSON_PARSE_EMPTY_KEY            0
#endif

/* Whether to allow special characters such as newline in the string */
#ifndef JSON_PARSE_SPECIAL_CHAR
#define JSON_PARSE_SPECIAL_CHAR         1
#endif

/* Whether to allow single quoted string/key and unquoted key */
#ifndef JSON_PARSE_SPECIAL_QUOTES
#define JSON_PARSE_SPECIAL_QUOTES       0
#endif

/* Whether to allow HEX number */
#ifndef JSON_PARSE_HEX_NUM
#define JSON_PARSE_HEX_NUM              1
#endif

/* Whether to allow special number such as starting with '.', '+', '0' */
#ifndef JSON_PARSE_SPECIAL_NUM
#define JSON_PARSE_SPECIAL_NUM          1
#endif

/* Whether to allow special double such as nan, inf, -inf */
#ifndef JSON_PARSE_SPECIAL_DOUBLE
#define JSON_PARSE_SPECIAL_DOUBLE       1
#endif

/* Whether to allow json starting with non-array and non-object */
#ifndef JSON_PARSE_SINGLE_VALUE
#define JSON_PARSE_SINGLE_VALUE         1
#endif

/* Whether to allow characters other than spaces after finishing parsing */
#ifndef JSON_PARSE_FINISHED_CHAR
#define JSON_PARSE_FINISHED_CHAR        0
#endif

/**************** debug ****************/

/* error print */
#define JSON_ERROR_PRINT_ENABLE         1

#if JSON_ERROR_PRINT_ENABLE
#define JsonErr(fmt, ...) do {                                      \
    printf("[JsonErr][%s:%d] ", __func__, __LINE__);                \
    printf(fmt, ##__VA_ARGS__);                                     \
} while(0)

#define JsonPareseErr(s)      do {                                  \
    if (parse_ptr->str) {                                           \
        char ptmp[32] = {0};                                        \
        strncpy(ptmp, parse_ptr->str + parse_ptr->offset, 31);      \
        JsonErr("====%s====\n%s\n", s, ptmp);                       \
    } else {                                                        \
        JsonErr("%s\n", s);                                         \
    }                                                               \
} while(0)

#else
#define JsonErr(fmt, ...)     do {} while(0)
#define JsonPareseErr(s)      do {} while(0)
#endif

/**************** gcc builtin ****************/

#if defined(__GNUC__) || defined(__clang__)
#define UNUSED_ATTR                     __attribute__((unused))
#define FALLTHROUGH_ATTR                __attribute__((fallthrough))
#define likely(cond)                    __builtin_expect(!!(cond), 1)
#define unlikely(cond)                  __builtin_expect(!!(cond), 0)
#else
#define UNUSED_ATTR
#define FALLTHROUGH_ATTR
#define likely(cond)                    (cond)
#define unlikely(cond)                  (cond)
#endif

#if JSON_PARSE_SPECIAL_QUOTES
#define UNUSED_END_CH
#else
#define UNUSED_END_CH                   UNUSED_ATTR
#endif

/**************** definition ****************/

/* head apis */
#define json_malloc                     malloc
#define json_calloc                     calloc
#define json_realloc                    realloc
#define json_strdup                     strdup
#define json_free                       free

#define JSON_ITEM_NUM_PLUS_DEF          16
#define JSON_POOL_MEM_SIZE_DEF          8192

/* print choice size */
#define JSON_PRINT_UTF16_SUPPORT        0
#define JSON_PRINT_NUM_INIT_DEF         1024
#define JSON_PRINT_NUM_PLUS_DEF         64
#define JSON_PRINT_DEPTH_DEF            16
#define JSON_PRINT_SIZE_PLUS_DEF        1024
#define JSON_FORMAT_ITEM_SIZE_DEF       32
#define JSON_UNFORMAT_ITEM_SIZE_DEF     24

/* file parse choice size */
#define JSON_PARSE_ERROR_STR            "Z"
#define JSON_PARSE_READ_SIZE_DEF        8192
#define JSON_PARSE_NUM_DIV_DEF          8

/**************** json list ****************/

/* args of json_list_entry is different to list_entry of linux */
#define json_list_entry(ptr, type)  ((type *)(ptr))

#define json_list_for_each_entry(pos, head, member, type)           \
    for (pos = json_list_entry((head)->next, type);                 \
        &pos->member != (struct json_list *)(head);                 \
        pos = json_list_entry(pos->member.next, type))

#define json_list_for_each_entry_safe(p, pos, n, head, member, type)\
    for (p = json_list_entry((head), type),                         \
        pos = json_list_entry((head)->next, type),                  \
        n = json_list_entry(pos->member.next, type);                \
        &pos->member != (struct json_list *)(head);                 \
        p = pos, pos = n, n = json_list_entry(n->member.next, type))

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

static inline int _item_total_get(json_object *json)
{
    int cnt = 0;
    json_object *item = NULL;

    switch (json->type_member) {
    case JSON_ARRAY:
    case JSON_OBJECT:
        json_list_for_each_entry(item, &json->value.head, list, json_object) {
            cnt += _item_total_get(item);
        }
        break;
    default:
        break;
    }
    ++cnt;

    return cnt;
}

int json_item_total_get(json_object *json)
{
    if (!json)
        return 0;
    return _item_total_get(json);
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
        json_list_for_each_entry_safe(p, pos, n, &json->value.head, list, json_object) {
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

    if ((json = (json_object *)json_calloc(1, sizeof(json_object))) == NULL) {
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

    if ((json = (json_object *)json_calloc(1, sizeof(json_object))) == NULL) {
        JsonErr("malloc failed!\n");
        return NULL;
    }

    json->type_member = type;
    switch (type) {
    case JSON_NULL:
        break;
    case JSON_BOOL:   json->value.vnum.vbool = *(bool *)value;      break;
    case JSON_INT:    json->value.vnum.vint  = *(int32_t *)value;   break;
    case JSON_HEX:    json->value.vnum.vhex  = *(uint32_t *)value;  break;
    case JSON_LINT:   json->value.vnum.vlint = *(int64_t *)value;   break;
    case JSON_LHEX:   json->value.vnum.vlhex = *(uint64_t *)value;  break;
    case JSON_DOUBLE: json->value.vnum.vdbl  = *(double *)value;    break;
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
        case JSON_BOOL:   value = ((bool *)values) + i;         break;
        case JSON_INT:    value = ((int32_t *)values) + i;      break;
        case JSON_HEX:    value = ((uint32_t *)values) + i;     break;
        case JSON_LINT:   value = ((int64_t *)values) + i;      break;
        case JSON_LHEX:   value = ((uint64_t *)values) + i;     break;
        case JSON_DOUBLE: value = ((double *)values) + i;       break;
        case JSON_STRING: value = ((json_string_t *)values) + i;break;
        default:          JsonErr("not support json type.\n");  goto err;
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

uint32_t json_string_hash_code(json_string_t *jstr)
{
    uint32_t i = 0, hash = 0;

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
            char *new_str = (char *)json_realloc(dst->str, src->len + 1);
            if (new_str) {
                dst->str = new_str;
            } else {
                JsonErr("malloc failed!\n");
                json_free(dst->str);
                dst->str = NULL;
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
#define _get_number(json, etype, val) do {                                    \
    switch (json->type_member) {                                              \
    case JSON_BOOL:   *(etype *)val = (etype)json->value.vnum.vbool;   break; \
    case JSON_INT:    *(etype *)val = (etype)json->value.vnum.vint;    break; \
    case JSON_HEX:    *(etype *)val = (etype)json->value.vnum.vhex;    break; \
    case JSON_LINT:   *(etype *)val = (etype)json->value.vnum.vlint;   break; \
    case JSON_LHEX:   *(etype *)val = (etype)json->value.vnum.vlhex;   break; \
    case JSON_DOUBLE: *(etype *)val = (etype)json->value.vnum.vdbl;    break; \
    default: *(etype *)val = (etype)0; JsonErr("wrong type!\n");   return -1; \
    }                                                                         \
} while(0)

    switch (type) {
    case JSON_BOOL:   _get_number(json, bool, value);       break;
    case JSON_INT:    _get_number(json, int32_t, value);    break;
    case JSON_HEX:    _get_number(json, uint32_t, value);   break;
    case JSON_LINT:   _get_number(json, int64_t, value);    break;
    case JSON_LHEX:   _get_number(json, uint64_t, value);   break;
    case JSON_DOUBLE: _get_number(json, double, value);     break;
    default:          JsonErr("wrong type!\n");         return -1;
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
    case JSON_LINT:
    case JSON_LHEX:
    case JSON_DOUBLE:
        break;
    default:
        JsonErr("wrong type!\n");
        return -1;
    }

    switch (type) {
    case JSON_BOOL:   json->value.vnum.vbool = *(bool *)value;      break;
    case JSON_INT:    json->value.vnum.vint  = *(int32_t *)value;   break;
    case JSON_HEX:    json->value.vnum.vhex  = *(uint32_t *)value;  break;
    case JSON_LINT:   json->value.vnum.vlint = *(int64_t *)value;   break;
    case JSON_LHEX:   json->value.vnum.vlhex = *(uint64_t *)value;  break;
    case JSON_DOUBLE: json->value.vnum.vdbl  = *(double *)value;    break;
    default:                                                    return -1;
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
        json_list_for_each_entry(pos, &json->value.head, list, json_object) {
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
        json_list_for_each_entry(pos, &json->value.head, list, json_object) {
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
        json_list_for_each_entry_safe(p, pos, n, &json->value.head, list, json_object) {
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
            json_list_for_each_entry_safe(p, pos, n, &json->value.head, list, json_object) {
                if (pos->key_member && strcmp(key, pos->key_member) == 0) {
                    if (prev)
                        *prev = p;
                    return pos;
                }
            }
        } else {
            json_list_for_each_entry_safe(p, pos, n, &json->value.head, list, json_object) {
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

json_object *json_search_object_item(json_items_t *items, json_string_t *jkey, uint32_t hash)
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
    uint32_t ha = ((const json_item_t *)a)->hash;
    uint32_t hb = ((const json_item_t *)b)->hash;
    return (int)(ha - hb);
}

static inline void json_object_items_sort(json_items_t *items)
{
    items->conflicted = 0;
    if (items->count > 1) {
        uint32_t i = 0;
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
    if (json->type_member == JSON_ARRAY) {
        json_list_for_each_entry(pos, &json->value.head, list, json_object) {
            if (items->count == items->total) {
                items->total += JSON_ITEM_NUM_PLUS_DEF;
                json_item_t *new_items = (json_item_t *)json_realloc(items->items, items->total * sizeof(json_item_t));
                if (new_items) {
                    items->items = new_items;
                } else {
                    JsonErr("malloc failed!\n");
                    json_free(items->items);
                    items->items = NULL;
                    goto err;
                }
            }
            items->items[items->count++].json = pos;
        }
        return 0;
    }

    if (json->type_member == JSON_OBJECT) {
        json_list_for_each_entry(pos, &json->value.head, list, json_object) {
            if (items->count == items->total) {
                items->total += JSON_ITEM_NUM_PLUS_DEF;
                json_item_t *new_items = (json_item_t *)json_realloc(items->items, items->total * sizeof(json_item_t));
                if (new_items) {
                    items->items = new_items;
                } else {
                    JsonErr("malloc failed!\n");
                    json_free(items->items);
                    items->items = NULL;
                    goto err;
                }
            }
            items->items[items->count].hash = json_string_hash_code(&pos->jkey);
            items->items[items->count++].json = pos;
        }
        json_object_items_sort(items);
        return 0;
    }

    return -1;
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
    case JSON_LINT:   new_json = json_create_lint(json->value.vnum.vlint);  break;
    case JSON_LHEX:   new_json = json_create_lhex(json->value.vnum.vlhex);  break;
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
            json_list_for_each_entry(item, &json->value.head, list, json_object) {
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
        case JSON_LINT:
        case JSON_LHEX:
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
        case JSON_LINT:
        case JSON_LHEX:
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
    json_list_for_each_entry_safe(p, pos, n, &mgr->head, list, json_mem_node_t) {
        json_list_del(&pos->list, &p->list, &mgr->head);
        json_free(pos->ptr);
        json_free(pos);
        pos = p;
    }
    mgr->cur_node = &s_invalid_json_mem_node;
}

static void _json_mem_refresh(json_mem_mgr_t *mgr)
{
    json_mem_node_t *p = NULL, *pos = NULL, *n = NULL;
    int i = 0;
    json_list_for_each_entry_safe(p, pos, n, &mgr->head, list, json_mem_node_t) {
        if (++i == 1) {
            pos->cur = pos->ptr;
            mgr->cur_node = pos;
        } else {
            json_list_del(&pos->list, &p->list, &mgr->head);
            json_free(pos->ptr);
            json_free(pos);
            pos = p;
        }
    }
}

static void *_json_mem_new(size_t size, json_mem_mgr_t *mgr)
{
    json_mem_node_t *node = NULL;

    if ((node = (json_mem_node_t *)json_malloc(sizeof(json_mem_node_t))) == NULL) {
        JsonErr("malloc failed!\n");
        return NULL;
    }
    node->size = size;
    if ((node->ptr = (char *)json_malloc(node->size)) == NULL) {
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

void pjson_memory_refresh(json_mem_t *mem)
{
    _json_mem_refresh(&mem->obj_mgr);
    _json_mem_refresh(&mem->key_mgr);
    _json_mem_refresh(&mem->str_mgr);
}

int pjson_memory_statistics(json_mem_mgr_t *mgr)
{
    json_mem_node_t *pos = NULL;
    int size = 0;

    json_list_for_each_entry(pos, &mgr->head, list, json_mem_node_t) {
        size += (int)(pos->size);
    }
    return size;
}

json_object *pjson_new_object(json_type_t type, json_mem_t *mem)
{
    json_object *json = NULL;

    if ((json = (json_object *)pjson_memory_alloc(sizeof(json_object), &mem->obj_mgr)) == NULL) {
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

    if ((json = (json_object *)pjson_memory_alloc(sizeof(json_object), &mem->obj_mgr)) == NULL) {
        JsonErr("malloc failed!\n");
        return NULL;
    }
    memset(json, 0, sizeof(json_object));
    json->type_member = type;

    switch (type) {
    case JSON_NULL:
        break;
    case JSON_BOOL:   json->value.vnum.vbool = *(bool *)value;      break;
    case JSON_INT:    json->value.vnum.vint  = *(int32_t *)value;   break;
    case JSON_HEX:    json->value.vnum.vhex  = *(uint32_t *)value;  break;
    case JSON_LINT:   json->value.vnum.vlint = *(int64_t *)value;   break;
    case JSON_LHEX:   json->value.vnum.vlhex = *(uint64_t *)value;  break;
    case JSON_DOUBLE: json->value.vnum.vdbl  = *(double *)value;    break;
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
        case JSON_BOOL:   value = ((bool *)values) + i;         break;
        case JSON_INT:    value = ((int32_t *)values) + i;      break;
        case JSON_HEX:    value = ((uint32_t *)values) + i;     break;
        case JSON_LINT:   value = ((int64_t *)values) + i;      break;
        case JSON_LHEX:   value = ((uint64_t *)values) + i;     break;
        case JSON_DOUBLE: value = ((double *)values) + i;       break;
        case JSON_STRING: value = ((json_string_t *)values) + i;break;
        default: JsonErr("not support json type.\n");     return NULL;
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
            if ((dst->str = (char *)pjson_memory_alloc(src->len + 1, mgr)) == NULL) {
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
    case JSON_LINT:   new_json = pjson_create_lint(json->value.vnum.vlint, mem);  break;
    case JSON_LHEX:   new_json = pjson_create_lhex(json->value.vnum.vlhex, mem);  break;
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
            json_list_for_each_entry(item, &json->value.head, list, json_object) {
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
        case JSON_LINT:
        case JSON_LHEX:
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
        case JSON_LINT:
        case JSON_LHEX:
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
        char *new_str = (char *)json_realloc(print_ptr->ptr, print_ptr->size);
        if (new_str) {
            print_ptr->ptr = new_str;
            print_ptr->cur = print_ptr->ptr;
        } else {
            JsonErr("malloc failed!\n");
            json_free(print_ptr->ptr);
            print_ptr->ptr = NULL;
            print_ptr->cur = print_ptr->ptr;
            return -1;
        }
    }

    return 0;
}

static int _print_str_ptr_realloc(json_print_t *print_ptr, size_t slen)
{
    size_t used = GET_BUF_USED_SIZE(print_ptr);
    size_t len = used + slen + 1;

    while (print_ptr->item_total < print_ptr->item_count) {
        print_ptr->item_total += JSON_PRINT_NUM_PLUS_DEF;
        print_ptr->size += print_ptr->plus_size >> 2;
    }
    if (print_ptr->item_total - print_ptr->item_count > print_ptr->item_count) {
        print_ptr->size += print_ptr->size;
    } else {
        print_ptr->size += (uint64_t)print_ptr->size *
            (print_ptr->item_total - print_ptr->item_count) / print_ptr->item_count;
    }

    while (print_ptr->size < len)
        print_ptr->size += print_ptr->plus_size;

    char *new_str = (char *)json_realloc(print_ptr->ptr, print_ptr->size);
    if (new_str) {
        print_ptr->ptr = new_str;
        print_ptr->cur = print_ptr->ptr + used;
    } else {
        JsonErr("malloc failed!\n");
        json_free(print_ptr->ptr);
        print_ptr->ptr = NULL;
        print_ptr->cur = print_ptr->ptr;
        return -1;
    }

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
    const char *str = NULL, *bak = NULL, *end = NULL;

    str = jstr->str;
    len = jstr->len;
    end = str + len;
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
    alloced = (len << 2) + (len << 1) + 3;
#else
    alloced = (len << 1) + 3;
#endif
    _PRINT_PTR_REALLOC(alloced);
    *print_ptr->cur++ = '\"';

    bak = str;
    while (str < end) {
        switch ((*str++)) {
        case '\"': ch = '\"'; break;
        case '\\': ch = '\\'; break;
        case '\b': ch = 'b' ; break;
        case '\f': ch = 'f' ; break;
        case '\n': ch = 'n' ; break;
        case '\r': ch = 'r' ; break;
        case '\t': ch = 't' ; break;
        case '\v': ch = 'v' ; break;
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

    ++str;
    _JSON_PRINT_SEGMENT();
    *print_ptr->cur++ = '\"';
    return 0;
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
        json_object **new_array = (json_object **)json_realloc(item_array, sizeof(json_object *) * item_total);
        if (new_array) {
            item_array = new_array;
        } else {
            JsonErr("malloc failed!\n");
            json_free(item_array);
            item_array = NULL;
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
#if !JSON_PARSE_EMPTY_KEY
            JsonErr("key is empty!\n");
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
#if !JSON_PARSE_EMPTY_KEY
            JsonErr("key is empty!\n");
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
        _PRINT_PTR_NUMBER(jnum_itoa, json->value.vnum.vint);
        break;
    case JSON_HEX:
        _PRINT_PTR_NUMBER(jnum_htoa, json->value.vnum.vhex);
        break;
    case JSON_LINT:
        _PRINT_PTR_NUMBER(jnum_ltoa, json->value.vnum.vlint);
        break;
    case JSON_LHEX:
        _PRINT_PTR_NUMBER(jnum_lhtoa, json->value.vnum.vlhex);
        break;
    case JSON_DOUBLE:
        _PRINT_PTR_NUMBER(json_dtoa, json->value.vnum.vdbl);
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

static int _print_val_release(json_print_t *print_ptr, bool free_all_flag, size_t *length, json_print_ptr_t *ptr)
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
        if (ptr) {
            ptr->size = print_ptr->size;
            ptr->p = print_ptr->ptr;
        } else {
            _clear_free_ptr(print_ptr->ptr);
        }
    } else {
        if (free_all_flag) {
            _clear_free_ptr(print_ptr->ptr);
        } else {
            if (length)
                *length = print_ptr->cur - print_ptr->ptr;
            *print_ptr->cur = '\0';


            if (ptr) {
                ptr->size = print_ptr->size;
                ptr->p = print_ptr->ptr;
            } else {
                /* Reduce size, never fail */
                print_ptr->ptr = (char *)json_realloc(print_ptr->ptr, used + 1);
            }
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
        if ((print_ptr->fd = open(choice->path, O_CREAT|O_TRUNC|O_WRONLY|O_BINARY, 0666)) < 0) {
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

    if (choice->ptr && choice->ptr->p) {
        print_ptr->size = choice->ptr->size;
        print_ptr->ptr = choice->ptr->p;
        choice->ptr->p = NULL;
    } else {
        if ((print_ptr->ptr = (char *)json_malloc(print_ptr->size)) == NULL) {
            JsonErr("malloc failed!\n");
            goto err;
        }
    }
    print_ptr->cur = print_ptr->ptr;

    return 0;
err:
    _print_val_release(print_ptr, true, NULL, NULL);
    return -1;
}

char *json_print_common(json_object *json, json_print_choice_t *choice)
{
    json_print_t print_val = {0};

    if (!json)
        return NULL;

    print_val.item_total = choice->item_total ? choice->item_total : JSON_PRINT_NUM_INIT_DEF;
    if (_print_val_init(&print_val, choice) < 0)
        return NULL;

    if (_json_print_value(&print_val, json) < 0) {
        JsonErr("print failed!\n");
        goto err;
    }
    if (_print_val_release(&print_val, false, &choice->str_len, choice->ptr) < 0)
        goto err;

    return choice->path ? (char *)"ok" : print_val.ptr;
err:
    _print_val_release(&print_val, true, NULL, NULL);
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
    json_sax_print_t *print_handle = (json_sax_print_t *)handle;
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
#if !JSON_PARSE_EMPTY_KEY
                    JsonErr("key is empty!\n");
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
#if !JSON_PARSE_EMPTY_KEY
                    JsonErr("key is empty!\n");
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
        _PRINT_PTR_NUMBER(jnum_itoa, *(int32_t*)value);
        break;
    case JSON_HEX:
        _PRINT_PTR_NUMBER(jnum_htoa, *(uint32_t*)value);
        break;
    case JSON_LINT:
        _PRINT_PTR_NUMBER(jnum_ltoa, *(int64_t*)value);
        break;
    case JSON_LHEX:
        _PRINT_PTR_NUMBER(jnum_lhtoa, *(uint64_t*)value);
        break;
    case JSON_DOUBLE:
        _PRINT_PTR_NUMBER(json_dtoa, *(double*)value);
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
                print_handle->total += JSON_PRINT_DEPTH_DEF;
                json_sax_print_depth_t *new_array = (json_sax_print_depth_t *)json_realloc(
                    print_handle->array, print_handle->total * sizeof(json_sax_print_depth_t));
                if (new_array) {
                    print_handle->array = new_array;
                } else {
                    JsonErr("malloc failed!\n");
                    json_free(print_handle->array);
                    print_handle->array = NULL;
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

    if ((print_handle = (json_sax_print_t *)json_calloc(1, sizeof(json_sax_print_t))) == NULL) {
        JsonErr("malloc failed!\n");
        return NULL;
    }
    print_handle->print_val.item_total = choice->item_total ? choice->item_total : JSON_PRINT_NUM_INIT_DEF;
    if (_print_val_init(&print_handle->print_val, choice) < 0) {
        json_free(print_handle);
        return NULL;
    }

    print_handle->total = JSON_PRINT_DEPTH_DEF;
    if ((print_handle->array = (json_sax_print_depth_t *)json_malloc(print_handle->total * sizeof(json_sax_print_depth_t))) == NULL) {
        _print_val_release(&print_handle->print_val, true, NULL, NULL);
        json_free(print_handle);
        JsonErr("malloc failed!\n");
        return NULL;
    }

    return print_handle;
}

char *json_sax_print_finish(json_sax_print_hd handle, size_t *length, json_print_ptr_t *ptr)
{
    char *ret = NULL;

    json_sax_print_t *print_handle = (json_sax_print_t *)handle;
    if (!print_handle)
        return NULL;
    if (print_handle->array)
        json_free(print_handle->array);
    if (print_handle->error_flag) {
        _print_val_release(&print_handle->print_val, true, NULL, NULL);
        json_free(print_handle);
        return NULL;
    }

    ret = (print_handle->print_val.fd >= 0) ? (char *)"ok" : print_handle->print_val.ptr;
    if (_print_val_release(&print_handle->print_val, false, length, ptr) < 0) {
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

    void (*skip_blank)(struct _json_parse_t *parse_ptr);
    int (*parse_string)(struct _json_parse_t *parse_ptr, char end_ch, json_string_t *jstr, json_mem_mgr_t *mgr);
    int (*parse_value)(struct _json_parse_t *parse_ptr, json_object **root);

#if JSON_SAX_APIS_SUPPORT
    json_sax_parser_t parser;
    json_sax_cb_t cb;
    json_sax_ret_t ret;
#endif
} json_parse_t;

#define IS_BLANK(c)      ((((c) + 0xdf) & 0xff) > 0xdf)
#define IS_DIGIT(c)      ((c) >= '0' && (c) <= '9')

/*
// To get char_type_lut
void print_char_type_lut(void)
{
    int i = 0;
    for (i = 0; i < 256; ++i) {
        switch (i) {
        case '\0': printf("%-3d, ", 1 << 0); break;
        case '\"': printf("%-3d, ", 1 << 1); break;
        case '\'': printf("%-3d, ", 1 << 2); break;
        case ':' : printf("%-3d, ", 1 << 3); break;
        case '\\': printf("%-3d, ", 1 << 4); break;
        case ' ' : printf("%-3d, ", 1 << 5); break;
        case '\b': case '\f': case '\n': case '\r': case '\t': case '\v':
                   printf("%-3d, ", 1 << 6); break;
        default  : printf("%-3d, ", 0); break;
        }
        if ((i & 0xF) == 0xF)
            printf("\n");
    }
    return 0;
}
 */

static const uint8_t char_type_lut[256] = {
    1  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 64 , 64 , 64 , 64 , 64 , 64 , 0  , 0  ,
    0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
    32 , 0  , 2  , 0  , 0  , 0  , 0  , 4  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
    0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 8  , 0  , 0  , 0  , 0  , 0  ,
    0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
    0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 16 , 0  , 0  , 0  ,
    0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
    0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
    0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
    0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
    0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
    0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
    0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
    0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
    0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
    0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0
};

static inline bool _is_special_char(uint8_t val)
{
#if JSON_PARSE_SPECIAL_QUOTES
#if JSON_PARSE_SPECIAL_CHAR
    static uint8_t flag = ~((1 << 5) | (1 << 6));
#else
    static uint8_t flag = ~((1 << 5));
#endif
#else
#if JSON_PARSE_SPECIAL_CHAR
    static uint8_t flag = ~((1 << 2) | (1 << 3) | (1 << 5) | (1 << 6));
#else
    static uint8_t flag = ~((1 << 2) | (1 << 3) | (1 << 5));
#endif
#endif
    return (char_type_lut[val] & flag);
}

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
    size = (ssize_t)(parse_ptr->readed - offset);

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
        size = (ssize_t)(parse_ptr->readed - parse_ptr->offset);
        if (size && parse_ptr->offset)
            memmove(parse_ptr->str, parse_ptr->str + parse_ptr->offset, size);
        parse_ptr->offset = 0;
        parse_ptr->readed = size;

        if (read_size > parse_ptr->size) {
            while (read_size > parse_ptr->size)
                parse_ptr->size += parse_ptr->read_size;

            char *new_str = (char *)json_realloc(parse_ptr->str, parse_ptr->size + 1);
            if (new_str) {
                parse_ptr->str = new_str;
            } else {
                JsonErr("malloc failed!\n");
                json_free(parse_ptr->str);
                parse_ptr->str = NULL;
                *sstr = JSON_PARSE_ERROR_STR; /* Reduce the judgment pointer is NULL. */
                return 0;
            }
        }

        size = (ssize_t)(parse_ptr->size - parse_ptr->readed);
        if ((rsize = read(parse_ptr->fd, parse_ptr->str + parse_ptr->readed, size)) != size)
            parse_ptr->read_size = 0; /* finish readding file */
        parse_ptr->readed += rsize < 0 ? 0 : rsize;
        parse_ptr->str[parse_ptr->readed] = '\0';

        *sstr = parse_ptr->str + read_offset;
        size = (ssize_t)(parse_ptr->readed - read_offset);
    }

    return size;
}

static inline int _get_str_parse_ptr(json_parse_t *parse_ptr, int read_offset, size_t read_size UNUSED_ATTR, char **sstr)
{
    size_t offset = parse_ptr->offset + read_offset;
    *sstr = parse_ptr->str + offset;
    return (int)(parse_ptr->size - offset);
}

static inline int _get_parse_ptr(json_parse_t *parse_ptr, int read_offset, size_t read_size, char **sstr)
{
    if (parse_ptr->fd >= 0)
        return _get_file_parse_ptr(parse_ptr, read_offset, read_size, sstr);
    return _get_str_parse_ptr(parse_ptr, read_offset, read_size, sstr);
}
#define _UPDATE_PARSE_OFFSET(add_offset)    parse_ptr->offset += add_offset

static inline json_type_t _json_parse_number(const char **sstr, json_number_t *vnum)
{
    const char *s = *sstr;

#if !JSON_PARSE_HEX_NUM
    if (unlikely(*s == '0' && (*(s+1) == 'x' || *(s+1) == 'X'))) {
        JsonErr("HEX can't be parsed in standard json!\n");
        return JSON_NULL;
    }
#endif

    json_type_t type;
    *sstr += jnum_parse_num(s, (jnum_type_t *)&type, (jnum_value_t *)(vnum));
    return type;
}

static inline uint32_t _parse_hex4(const unsigned char *str)
{
    int i = 0;
    uint32_t val = 0;

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

#if JSON_PARSE_SKIP_COMMENT
static bool _skip_comment(json_parse_t *parse_ptr, int *pcnt)
{
    char *str = NULL;
    int cnt = 0;

    if (_get_parse_ptr(parse_ptr, *pcnt, 64, &str) == 1)
        goto end;

    switch (*(str + 1)) {
    case '/':
        str += 2;
        *pcnt += 2;
        while (1) {
            switch (*str) {
            case '\n':
                ++str;
                ++*pcnt;
                goto next;
            case '\0':
                if (_get_parse_ptr(parse_ptr, *pcnt, 64, &str) == 0)
                    goto end;
                break;
            default:
                ++str;
                ++*pcnt;
                break;
            }
        }
        break;
    case '*':
        cnt = 2;
        str += 2;
        while (1) {
            switch (*str) {
            case '*':
                switch (*(str + 1)) {
                case '/':
                    str += 2;
                    cnt += 2;
                    *pcnt += cnt;
                    goto next;
                case '\0':
                    if (_get_parse_ptr(parse_ptr, *pcnt + cnt, 64, (char **)&str) == 1)
                        goto end;

                    if (*(str + 1) == '/') {
                        str += 2;
                        cnt += 2;
                        *pcnt += cnt;
                        goto next;
                    } else {
                        ++str;
                        ++cnt;
                    }
                    break;
                default:
                    ++str;
                    ++cnt;
                    break;
                }
                break;
            case '\0':
                if (_get_parse_ptr(parse_ptr, *pcnt + cnt, 64, (char **)&str) == 0)
                    goto end;
                break;
            default:
                ++str;
                ++cnt;
                break;
            }
        }
        break;
    default:
        goto end;
    }

next:
    return false;
end:
    return true;
}
#endif

static inline void _skip_blank(json_parse_t *parse_ptr)
{
    unsigned char *str = NULL;
    int cnt = 0;

    while (_get_parse_ptr(parse_ptr, cnt, 64, (char **)&str)) {
        while (IS_BLANK(*str)) {
            ++str;
            ++cnt;
        }
        if (likely(*str)) {
#if JSON_PARSE_SKIP_COMMENT
            if (unlikely(*str == '/')) {
                if (!_skip_comment(parse_ptr, &cnt))
                    continue;
            }
#endif
            break;
        }
    }
    _UPDATE_PARSE_OFFSET(cnt);
}

static int _parse_strcpy(char *ptr, const char *str, int nsize)
{
    const char *bak = ptr, *last = str, *end = str + nsize;
    char ch = '\0';
    int size = 0, seq_len = 0;

    while (str < end) {
        if (unlikely(*str++ == '\\')) {
            switch ((*str++)) {
            case 'b' : ch = '\b'; break;
            case 'f' : ch = '\f'; break;
            case 'n' : ch = '\n'; break;
            case 'r' : ch = '\r'; break;
            case 't' : ch = '\t'; break;
            case 'v' : ch = '\v'; break;
            case '\"': ch = '\"'; break;
            case '\'': ch = '\''; break;
            case '\\': ch = '\\'; break;
            case '/' : ch = '/' ; break;
#if JSON_PARSE_SPECIAL_CHAR
            case '\r':
                size = (int)(str - last - 2);
                memcpy(ptr, last, size);
                ptr += size;
                if (*str == '\n')
                    ++str;
                last = str;
                continue;

            case '\n':
                size = (int)(str - last - 2);
                memcpy(ptr, last, size);
                ptr += size;
                last = str;
                continue;
#endif
            case 'u' :
                str -= 2;
                size = (int)(str - last);
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

            size = (int)(str - last - 2);
            memcpy(ptr, last, size);
            ptr += size;
            *ptr++ = ch;
            last = str;
        }
    }

    size = (int)(str - last);
    memcpy(ptr, last, size);
    ptr += size;
    *ptr = '\0';
    return (int)(ptr - bak);
}

static int _parse_strlen(json_parse_t *parse_ptr, char end_ch UNUSED_END_CH, int *escaped)
{
#define PARSE_READ_SIZE    128
    char *str = NULL, *bak = NULL;
    int total = 0, rsize = 0;
    char c = '\0';

    _get_parse_ptr(parse_ptr, 0, PARSE_READ_SIZE, &str);
    bak = str;

    while (1) {
        switch ((c = *str++)) {
        case '\"':
#if JSON_PARSE_SPECIAL_QUOTES
        case '\'':
        case ':':
            if (c == end_ch)
#endif
            {
                total += (int)(str - bak - 1);
                return total;
            }
#if JSON_PARSE_SPECIAL_QUOTES
            if (c == '\"' && !*escaped)
                *escaped = -1;
            break;
#endif
        case '\\':
            if (likely(rsize != (str - bak))) {
                ++str;
                *escaped = 1;
            } else {
                --str;
                total += (int)(str - bak);
                if (unlikely((rsize = _get_parse_ptr(parse_ptr, total, PARSE_READ_SIZE, &str)) < 2)) {
                    JsonErr("last char is slash!\n");
                    goto err;
                }
                bak = str;
            }
            break;
        case '\0':
            --str;
            total += (int)(str - bak);
            if (unlikely((rsize = _get_parse_ptr(parse_ptr, total, PARSE_READ_SIZE, &str)) < 1)) {
                JsonErr("No more string!\n");
                goto err;
            }
            bak = str;
            break;
#if !JSON_PARSE_SPECIAL_CHAR
        case '\b': case '\f': case '\n': case '\r': case '\t': case '\v':
            JsonErr("tab and linebreak can't be existed in string in standard json!\n");
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

static int _json_parse_string(json_parse_t *parse_ptr, char end_ch, json_string_t *jstr, json_mem_mgr_t *mgr)
{
    char *ptr = NULL, *str = NULL;
    int escaped = 0;
    int len = 0, total = 0;

    memset(jstr, 0, sizeof(json_string_t));
    if (unlikely((total = _parse_strlen(parse_ptr, end_ch, &escaped)) < 0)) {
        return -1;
    }
    len = total;
    _get_parse_ptr(parse_ptr, 0, total, &str);

    if (unlikely((ptr = (char *)_parse_alloc(parse_ptr, len + 1, mgr)) == NULL)) {
        JsonErr("malloc failed!\n");
        return -1;
    }

    if (likely(escaped != 1)) {
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

    jstr->escaped = escaped != 0;
    jstr->len = len;
    jstr->str = ptr;
    return len;

err:
    JsonPareseErr("parse string failed!");
    return -1;
}

static int _json_parse_key(json_parse_t *parse_ptr, json_string_t *jkey)
{
    char *str = NULL;
    char end_ch = '\"';

    parse_ptr->skip_blank(parse_ptr);
    _get_parse_ptr(parse_ptr, 0, 2, &str);

    switch (*str) {
    case '\"':
#if JSON_PARSE_SPECIAL_QUOTES
    case '\'':
#endif
        if (unlikely(str[1] == str[0])) {
#if !JSON_PARSE_EMPTY_KEY
            JsonPareseErr("key is empty!");
            goto err;
#else
            _UPDATE_PARSE_OFFSET(2);
#endif
        } else {
            end_ch = *str;
            _UPDATE_PARSE_OFFSET(1);
            if (unlikely(parse_ptr->parse_string(parse_ptr, end_ch, jkey, &parse_ptr->mem->key_mgr) < 0)) {
                goto err;
            }
            _UPDATE_PARSE_OFFSET(1);
        }
        parse_ptr->skip_blank(parse_ptr);
        _get_parse_ptr(parse_ptr, 0, 1, &str);
        if (unlikely(*str != ':')) {
            JsonPareseErr("key is not before ':'");
            goto err;
        }
        _UPDATE_PARSE_OFFSET(1);
        break;

    default:
#if JSON_PARSE_SPECIAL_QUOTES
        if (unlikely(*str == ':')) {
#if !JSON_PARSE_EMPTY_KEY
            JsonPareseErr("key is empty!");
            goto err;
#else
            _UPDATE_PARSE_OFFSET(1);
#endif
        } else {
            end_ch = ':';
            if (unlikely(parse_ptr->parse_string(parse_ptr, end_ch, jkey, &parse_ptr->mem->key_mgr) < 0)) {
                goto err;
            }
            _UPDATE_PARSE_OFFSET(1);

            while (IS_BLANK((unsigned char)jkey->str[jkey->len - 1]))
                --jkey->len;
            jkey->str[jkey->len] = '\0';
        }
        break;
#else
        JsonPareseErr("key is not started with quotes!");
        goto err;
#endif
    }
    return 0;
err:
    return -1;
}

static int _json_parse_single_value(json_parse_t *parse_ptr, char *str, json_string_t *jkey, json_value_t *value)
{
    char end_ch = '\"';
    char *bak = NULL;

    switch (*str) {
    case '\"':
#if JSON_PARSE_SPECIAL_QUOTES
    case '\'':
#endif
        jkey->type = JSON_STRING;
        if (unlikely(str[1] == str[0])) {
            memset(&value->vstr, 0, sizeof(json_string_t));
            _UPDATE_PARSE_OFFSET(2);
        } else {
            end_ch = *str;
            _UPDATE_PARSE_OFFSET(1);
            if (unlikely(parse_ptr->parse_string(parse_ptr, end_ch, &value->vstr, &parse_ptr->mem->str_mgr) < 0)) {
                goto err;
            }
            _UPDATE_PARSE_OFFSET(1);
        }
        break;

#if JSON_PARSE_SPECIAL_NUM
    case '+':
#endif
    case '-':
#if JSON_PARSE_SPECIAL_DOUBLE
        if (strncmp("inf", str + 1, 3) == 0) {
            jkey->type = JSON_DOUBLE;
            value->vnum.vlhex = (*str == '-' ? 0xFFF0000000000000 : 0x7FF0000000000000);
            _UPDATE_PARSE_OFFSET(9);
            break;
        }
        FALLTHROUGH_ATTR;
#endif
#if JSON_PARSE_SPECIAL_NUM
    case '.':
#endif
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        bak = str;
        if (unlikely((jkey->type = _json_parse_number((const char **)&str, &value->vnum)) == JSON_NULL)) {
            JsonPareseErr("Not number!");
            goto err;
        }
        _UPDATE_PARSE_OFFSET(str - bak);
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
            break;
        }
#if JSON_PARSE_SPECIAL_DOUBLE
        if (likely(parse_ptr->size - parse_ptr->offset >= 3 && memcmp("nan", str, 3) == 0)) {
            jkey->type = JSON_DOUBLE;
            value->vnum.vlhex = 0x7FF0000000000001;
            _UPDATE_PARSE_OFFSET(3);
            break;
        }
#endif
        JsonPareseErr("invalid next ptr!");
        goto err;

#if JSON_PARSE_SPECIAL_DOUBLE
    case 'i':
        if (likely(parse_ptr->size - parse_ptr->offset >= 3 && memcmp("inf", str, 3) == 0)) {
            jkey->type = JSON_DOUBLE;
            value->vnum.vlhex = 0x7FF0000000000000;
            _UPDATE_PARSE_OFFSET(8);
        } else {
            JsonPareseErr("invalid next ptr!");
            goto err;
        }
        break;
#endif
    default:
        JsonPareseErr("invalid next ptr!");
        goto err;
    }

    return 0;
err:
    return -1;
}

static int _json_parse_value(json_parse_t *parse_ptr, json_object **root)
{
    json_string_t jkey = {0};
    char *str = NULL;
    json_object *item = NULL, *parent = NULL;
    json_object **item_array = NULL;
    int item_depth = -1, item_total = 0;

    goto next3;

next1:
    if (unlikely(item_depth >= item_total - 1)) {
        item_total += JSON_ITEM_NUM_PLUS_DEF;
        json_object **new_array = (json_object **)json_realloc(item_array, sizeof(json_object *) * item_total);
        if (new_array) {
            item_array = new_array;
        } else {
            JsonErr("malloc failed!\n");
            json_free(item_array);
            item_array = NULL;
            goto err;
        }
    }
    item_array[++item_depth] = item;
    parent = item;
    *root = *item_array;
    if (parent->type_member == JSON_ARRAY)
        goto next3;

next2:
    if (unlikely(_json_parse_key(parse_ptr, &jkey) < 0)) {
        goto err;
    }

next3:
    if (unlikely((item = (json_object *)_parse_alloc(parse_ptr, sizeof(json_object), &parse_ptr->mem->obj_mgr)) == NULL)) {
        JsonErr("malloc failed!\n");
        goto err;
    }
    memcpy(&item->jkey, &jkey, sizeof(json_string_t));
    memset(&jkey, 0, sizeof(json_string_t));

    parse_ptr->skip_blank(parse_ptr);
    _get_parse_ptr(parse_ptr, 0, 128, &str);

    switch (*str) {
    case '{':
        item->type_member = JSON_OBJECT;
        INIT_JSON_LIST_HEAD(&item->value.head);
        _UPDATE_PARSE_OFFSET(1);

        parse_ptr->skip_blank(parse_ptr);
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

        parse_ptr->skip_blank(parse_ptr);
        _get_parse_ptr(parse_ptr, 0, 1, &str);
        if (likely(*str != ']')) {
            if (parent)
                json_list_add_tail(&item->list, &parent->value.head);
            goto next1;
        } else {
            _UPDATE_PARSE_OFFSET(1);
        }
        break;

    default:
        if (_json_parse_single_value(parse_ptr, str, &item->jkey, &item->value) < 0)
            goto err;
        break;
    }

    if (parent) {
        json_list_add_tail(&item->list, &parent->value.head);
        item = NULL;
    }

next4:
    if (likely(item_depth >= 0)) {
        parse_ptr->skip_blank(parse_ptr);
        _get_parse_ptr(parse_ptr, 0, 1, &str);
        if (parent->type_member == JSON_OBJECT) {
            switch (*str) {
            case ',':
                _UPDATE_PARSE_OFFSET(1);
#if !JSON_PARSE_LAST_COMMA
                goto next2;
#else
                parse_ptr->skip_blank(parse_ptr);
                _get_parse_ptr(parse_ptr, 0, 1, &str);
                if (*str != '}')
                    goto next2;
                FALLTHROUGH_ATTR;
#endif
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
#if !JSON_PARSE_LAST_COMMA
                goto next3;
#else
                parse_ptr->skip_blank(parse_ptr);
                _get_parse_ptr(parse_ptr, 0, 1, &str);
                if (*str != ']')
                    goto next3;
                FALLTHROUGH_ATTR;
#endif
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

#if JSON_PARSE_SKIP_COMMENT
static bool _skip_comment_rapid(json_parse_t *parse_ptr, int *pcnt)
{
    char *str = parse_ptr->str + parse_ptr->offset + *pcnt;
    int cnt = 0;

    switch (*(str + 1)) {
    case '/':
        str += 2;
        *pcnt += 2;
        while (1) {
            switch (*str) {
            case '\n':
                ++str;
                ++*pcnt;
                goto next;
            case '\0':
                goto end;
            default:
                ++str;
                ++*pcnt;
                break;
            }
        }
        break;
    case '*':
        cnt = 2;
        str += 2;
        while (1) {
            switch (*str) {
            case '*':
                if (*(str + 1) == '/') {
                    str += 2;
                    cnt += 2;
                    *pcnt += cnt;
                    goto next;
                } else {
                    ++str;
                    ++cnt;
                }
                break;
            case '\0':
                goto end;
            default:
                ++str;
                ++cnt;
                break;
            }
        }
        break;
    default:
        goto end;
    }
next:
    return false;
end:
    return true;

}
#endif

static inline void _skip_blank_rapid(json_parse_t *parse_ptr)
{
    unsigned char *str, *bak;

#if JSON_PARSE_SKIP_COMMENT
next:
#endif
    str = (unsigned char *)(parse_ptr->str + parse_ptr->offset);
    bak = str;
    while (IS_BLANK(*str))
        ++str;
    _UPDATE_PARSE_OFFSET(str - bak);

#if JSON_PARSE_SKIP_COMMENT
    if (unlikely(*str == '/')) {
        int cnt = 0;
        if (!_skip_comment_rapid(parse_ptr, &cnt)) {
            _UPDATE_PARSE_OFFSET(cnt);
            goto next;
        }
        _UPDATE_PARSE_OFFSET(cnt);
    }
#endif
}

static int _json_parse_string_reuse(json_parse_t *parse_ptr, char end_ch UNUSED_END_CH, json_string_t *jstr, json_mem_mgr_t *mgr UNUSED_ATTR)
{
    char *str = NULL, *end = NULL, *last = NULL, *bak = NULL, *ptr= NULL;
    int len = 0, seq_len = 0, size = 0;
    char c = '\0', ch = '\0';
    int escaped = 0;

    memset(jstr, 0, sizeof(json_string_t));
    end = parse_ptr->str + parse_ptr->size;
    str = parse_ptr->str + parse_ptr->offset;
    bak = str;
    ptr = str;
    jstr->str = str;

    while (1) {
        c = *str++;
        if (unlikely(_is_special_char((uint8_t)c))) {
            switch (c) {
            case '\"':
#if JSON_PARSE_SPECIAL_QUOTES
            case '\'':
            case ':':
                if (c == end_ch)
#endif
                {
                    len = (int)(str - bak - 1);
                    _UPDATE_PARSE_OFFSET(len);
                    ptr[len] = '\0';
                    jstr->escaped = escaped != 0;
                    jstr->len = len;
                    return len;
                }
#if JSON_PARSE_SPECIAL_QUOTES
                if (c == '\"' && !escaped)
                    escaped = -1;
                break;
#endif

            case '\0':
                goto err;
            case '\\':
                --str;
                last = str;
                ptr += str - bak;
                goto next;
            default:
#if !JSON_PARSE_SPECIAL_CHAR
                /* case '\b': case '\f': case '\n': case '\r': case '\t': case '\v': */
                JsonErr("tab and linebreak can't be existed in string in standard json!\n");
                goto err;
#else
                break;
#endif
            }
        }
    }

next:
    while (1) {
        c = *str++;
        if (unlikely(_is_special_char((uint8_t)c))) {
            switch (c) {
            case '\"':
#if JSON_PARSE_SPECIAL_QUOTES
            case '\'':
            case ':':
                if (c == end_ch)
#endif
                {
                    size = (int)(str - last - 1);
                    memmove(ptr, last, size);
                    ptr += size;
                    *ptr = '\0';
                    len = (int)(str - bak - 1);
                    _UPDATE_PARSE_OFFSET(len);
                    jstr->escaped = 1;
                    jstr->len = (uint32_t)(ptr - bak);
                    return jstr->len;
                }
                break;

            case '\0':
                goto err;
            case '\\':
                switch ((*str++)) {
                case 'b' : ch = '\b'; break;
                case 'f' : ch = '\f'; break;
                case 'n' : ch = '\n'; break;
                case 'r' : ch = '\r'; break;
                case 't' : ch = '\t'; break;
                case 'v' : ch = '\v'; break;
                case '\"': ch = '\"'; break;
                case '\'': ch = '\''; break;
                case '\\': ch = '\\'; break;
                case '/' : ch = '/' ; break;
#if JSON_PARSE_SPECIAL_CHAR
                case '\r':
                    size = (int)(str - last - 2);
                    memmove(ptr, last, size);
                    ptr += size;
                    if (*str == '\n')
                        ++str;
                    last = str;
                    continue;

                case '\n':
                    size = (int)(str - last - 2);
                    memmove(ptr, last, size);
                    ptr += size;
                    last = str;
                    continue;
#endif
                case 'u' :
                    str -= 2;
                    size = (int)(str - last);
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

                size = (int)(str - last - 2);
                memmove(ptr, last, size);
                ptr += size;
                *ptr++ = ch;
                last = str;
                break;
            default:
#if !JSON_PARSE_SPECIAL_CHAR
                /* case '\b': case '\f': case '\n': case '\r': case '\t': case '\v': */
                JsonErr("tab and linebreak can't be existed in string in standard json!\n");
                goto err;
#else
                break;
#endif
            }
        }
    }

err:
    memset(jstr, 0, sizeof(json_string_t));
    JsonPareseErr("parse string failed!");
    return -1;
}

static int _json_parse_string_rapid(json_parse_t *parse_ptr, char end_ch UNUSED_END_CH, json_string_t *jstr, json_mem_mgr_t *mgr)
{
    char *str = NULL, *bak = NULL, *ptr= NULL;
    char c = '\0';
    int len = 0, total = 0;
    int escaped = 0;

    memset(jstr, 0, sizeof(json_string_t));
    str = parse_ptr->str + parse_ptr->offset;
    bak = str;

    while (1) {
        switch ((c = *str++)) {
        case '\"':
#if JSON_PARSE_SPECIAL_QUOTES
        case '\'':
        case ':':
            if (c == end_ch)
#endif
            {
                goto next;
            }
#if JSON_PARSE_SPECIAL_QUOTES
            if (c == '\"' && !escaped)
                escaped = -1;
            break;
#endif

        case '\0':
            goto err;
        case '\\':
            ++str;
            escaped = 1;
            break;
#if !JSON_PARSE_SPECIAL_CHAR
        case '\b': case '\f': case '\n': case '\r': case '\t': case '\v':
            JsonErr("tab and linebreak can't be existed in string in standard json!\n");
            goto err;
#endif
        default:
            break;
        }
    }

next:
    total = (int)(str - bak - 1);
    len = total;
    if (unlikely((ptr = (char *)_parse_alloc(parse_ptr, len + 1, mgr)) == NULL)) {
        JsonErr("malloc failed!\n");
        goto err;
    }

    if (likely(escaped != 1)) {
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

    jstr->escaped = escaped != 0;
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
    char end_ch = '\"';
    json_object *item = NULL, *parent = NULL;
    json_object **item_array = NULL;
    int item_depth = -1, item_total = 0;

    goto next3;

next1:
    if (unlikely(item_depth >= item_total - 1)) {
        item_total += JSON_ITEM_NUM_PLUS_DEF;
        json_object **new_array = (json_object **)json_realloc(item_array, sizeof(json_object *) * item_total);
        if (new_array) {
            item_array = new_array;
        } else {
            JsonErr("malloc failed!\n");
            json_free(item_array);
            item_array = NULL;
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

    switch (*str) {
    case '\"':
#if JSON_PARSE_SPECIAL_QUOTES
    case '\'':
#endif
        if (unlikely(str[1] == str[0])) {
#if !JSON_PARSE_EMPTY_KEY
            JsonPareseErr("key is empty!");
            goto err;
#else
            _UPDATE_PARSE_OFFSET(2);
#endif
        } else {
            end_ch = *str;
            _UPDATE_PARSE_OFFSET(1);
            if (unlikely(parse_ptr->parse_string(parse_ptr, end_ch, &jkey, &parse_ptr->mem->key_mgr) < 0)) {
                goto err;
            }
            _UPDATE_PARSE_OFFSET(1);
        }
        _skip_blank_rapid(parse_ptr);
        str = parse_ptr->str + parse_ptr->offset;
        if (unlikely(*str != ':')) {
            JsonPareseErr("key is not before ':'");
            goto err;
        }
        _UPDATE_PARSE_OFFSET(1);
        break;

    default:
#if JSON_PARSE_SPECIAL_QUOTES
        if (unlikely(*str == ':')) {
#if !JSON_PARSE_EMPTY_KEY
            JsonPareseErr("key is empty!");
            goto err;
#else
            _UPDATE_PARSE_OFFSET(1);
#endif
        } else {
            end_ch = ':';
            if (unlikely(parse_ptr->parse_string(parse_ptr, end_ch, &jkey, &parse_ptr->mem->key_mgr) < 0)) {
                goto err;
            }
            _UPDATE_PARSE_OFFSET(1);

            while (IS_BLANK((unsigned char)jkey.str[jkey.len - 1]))
                --jkey.len;
            jkey.str[jkey.len] = '\0';
        }
        break;
#else
        JsonPareseErr("key is not started with quotes!");
        goto err;
#endif
    }

next3:
    if (unlikely((item = (json_object *)_parse_alloc(parse_ptr, sizeof(json_object), &parse_ptr->mem->obj_mgr)) == NULL)) {
        JsonErr("malloc failed!\n");
        goto err;
    }
    memcpy(&item->jkey, &jkey, sizeof(json_string_t));
    memset(&jkey, 0, sizeof(json_string_t));

    _skip_blank_rapid(parse_ptr);
    str = parse_ptr->str + parse_ptr->offset;

    switch (*str) {
    case '\"':
#if JSON_PARSE_SPECIAL_QUOTES
    case '\'':
#endif
        item->type_member = JSON_STRING;
        if (unlikely(str[1] == str[0])) {
            memset(&item->value.vstr, 0, sizeof(json_string_t));
            _UPDATE_PARSE_OFFSET(2);
        } else {
            end_ch = *str;
            _UPDATE_PARSE_OFFSET(1);
            if (unlikely(parse_ptr->parse_string(parse_ptr, end_ch, &item->value.vstr, &parse_ptr->mem->str_mgr) < 0)) {
                goto err;
            }
            _UPDATE_PARSE_OFFSET(1);
        }
        break;

#if JSON_PARSE_SPECIAL_NUM
    case '+':
#endif
    case '-':
#if JSON_PARSE_SPECIAL_DOUBLE
        if (strncmp("inf", str + 1, 3) == 0) {
            item->type_member = JSON_DOUBLE;
            item->value.vnum.vlhex = (*str == '-' ? 0xFFF0000000000000 : 0x7FF0000000000000);
            _UPDATE_PARSE_OFFSET(9);
            break;
        }
        FALLTHROUGH_ATTR;
#endif
#if JSON_PARSE_SPECIAL_NUM
    case '.':
#endif
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        bak = str;
        if (unlikely((item->type_member = _json_parse_number((const char **)&str, &item->value.vnum)) == JSON_NULL)) {
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
            break;
        }
#if JSON_PARSE_SPECIAL_DOUBLE
        if (likely(parse_ptr->size - parse_ptr->offset >= 3 && memcmp("nan", str, 3) == 0)) {
            item->type_member = JSON_DOUBLE;
            item->value.vnum.vlhex = 0x7FF0000000000001;
            _UPDATE_PARSE_OFFSET(3);
            break;
        }
#endif
        JsonPareseErr("invalid next ptr!");
        goto err;

#if JSON_PARSE_SPECIAL_DOUBLE
    case 'i':
        if (likely(parse_ptr->size - parse_ptr->offset >= 3 && memcmp("inf", str, 3) == 0)) {
            item->type_member = JSON_DOUBLE;
            item->value.vnum.vlhex = 0x7FF0000000000000;
            _UPDATE_PARSE_OFFSET(8);
        } else {
            JsonPareseErr("invalid next ptr!");
            goto err;
        }
        break;
#endif
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
#if !JSON_PARSE_LAST_COMMA
                goto next2;
#else
                _skip_blank_rapid(parse_ptr);
                str = parse_ptr->str + parse_ptr->offset;
                if (*str != '}')
                    goto next2;
                FALLTHROUGH_ATTR;
#endif
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
#if !JSON_PARSE_LAST_COMMA
                goto next3;
#else
                _skip_blank_rapid(parse_ptr);
                str = parse_ptr->str + parse_ptr->offset;
                if (*str != ']')
                    goto next3;
                FALLTHROUGH_ATTR;
#endif
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
        if ((parse_val.fd = open(choice->path, O_RDONLY|O_BINARY)) < 0) {
            JsonErr("open(%s) failed!\n", choice->path);
            return NULL;
        }
        if (choice->str_len) {
            total_size = choice->str_len;
        } else {
            total_size = lseek(parse_val.fd, 0, SEEK_END);
            lseek(parse_val.fd, 0, SEEK_SET);
        }
        parse_val.skip_blank = _skip_blank;
        parse_val.parse_string = _json_parse_string;
        parse_val.parse_value = _json_parse_value;
    } else {
        parse_val.str = choice->str;
        total_size = choice->str_len ? choice->str_len : strlen(choice->str);
        parse_val.size = total_size;
        if (choice->mem) {
            parse_val.reuse_flag = choice->reuse_flag;
        }
        parse_val.skip_blank = _skip_blank_rapid;
        if (parse_val.reuse_flag) {
            parse_val.parse_string = _json_parse_string_reuse;
        } else {
            parse_val.parse_string = _json_parse_string_rapid;
        }
        parse_val.parse_value = _json_parse_value_rapid;
    }

    if (choice->mem && !choice->mem->valid) {
        pjson_memory_init(choice->mem);
        mem_size = total_size / JSON_PARSE_NUM_DIV_DEF;
        if (mem_size < choice->mem_size)
            mem_size = choice->mem_size;
        choice->mem->obj_mgr.mem_size = mem_size;
        choice->mem->key_mgr.mem_size = mem_size;
        choice->mem->str_mgr.mem_size = mem_size;
    }

#if !JSON_PARSE_SINGLE_VALUE
    parse_val.skip_blank(&parse_val);
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

#if !JSON_PARSE_FINISHED_CHAR
    parse_val.skip_blank(&parse_val);
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
static inline int _json_sax_parse_string(json_parse_t *parse_ptr, char end_ch, json_string_t *jstr, json_mem_mgr_t *mgr)
{
    char *ptr = NULL, *str = NULL;
    int len = 0, total = 0;
    int escaped = 0;

    memset(jstr, 0, sizeof(json_string_t));
    if (unlikely((total = _parse_strlen(parse_ptr, end_ch, &escaped)) < 0)) {
        return -1;
    }
    len = total;
    _get_parse_ptr(parse_ptr, 0, total, &str);

    if (likely(escaped != 1)) {
        if (!(parse_ptr->fd >= 0 && mgr == &parse_ptr->mem->key_mgr)) {
            jstr->escaped = escaped != 0;
            jstr->alloced = 0;
            jstr->len = len;
            jstr->str = str;
        } else {
            if (unlikely((ptr = (char *)json_malloc(len+1)) == NULL)) {
                JsonErr("malloc failed!\n");
                return -1;
            }
            memcpy(ptr, str, len);
            ptr[len] = '\0';

            jstr->escaped = escaped != 0;
            jstr->alloced = 1;
            jstr->len = len;
            jstr->str = ptr;
        }
    } else {
        if (unlikely((ptr = (char *)json_malloc(len+1)) == NULL)) {
            JsonErr("malloc failed!\n");
            return -1;
        }
        if (unlikely((len = _parse_strcpy(ptr, str, len)) < 0)) {
            JsonErr("_parse_strcpy failed!\n");
            json_free(ptr);
            goto err;
        }

        jstr->escaped = 1;
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
    char *str = NULL;
    json_string_t *jkey = NULL, *parent = NULL;
    json_value_t *value = &parse_ptr->parser.value;
    json_string_t *tarray = NULL;
    int i = 0;

    memset(value, 0, sizeof(*value));
    parse_ptr->parser.total += JSON_ITEM_NUM_PLUS_DEF;
    if (unlikely((parse_ptr->parser.array = (json_string_t *)json_malloc(sizeof(json_string_t) * parse_ptr->parser.total)) == NULL)) {
        JsonErr("malloc failed!\n");
        return -1;
    }
    memset(parse_ptr->parser.array, 0, sizeof(json_string_t));
    goto next3;

next1:
    if (unlikely(parse_ptr->parser.index >= parse_ptr->parser.total - 1)) {
        parse_ptr->parser.total += JSON_ITEM_NUM_PLUS_DEF;
        if (unlikely((tarray = (json_string_t *)json_malloc(sizeof(json_string_t) * parse_ptr->parser.total)) == NULL)) {
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
    if (unlikely(_json_parse_key(parse_ptr, jkey) < 0)) {
        goto err;
    }

next3:
    jkey = parse_ptr->parser.array + parse_ptr->parser.index;
    parse_ptr->skip_blank(parse_ptr);
    _get_parse_ptr(parse_ptr, 0, 128, &str);

    switch (*str) {
    case '{':
        jkey->type = JSON_OBJECT;
        value->vcmd = JSON_SAX_START;
        _UPDATE_PARSE_OFFSET(1);
        parse_ptr->ret = parse_ptr->cb(&parse_ptr->parser);
        if (unlikely(parse_ptr->ret == JSON_SAX_PARSE_STOP)) {
            goto end;
        }

        parse_ptr->skip_blank(parse_ptr);
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

        parse_ptr->skip_blank(parse_ptr);
        _get_parse_ptr(parse_ptr, 0, 1, &str);
        if (likely(*str != ']')) {
            goto next1;
        } else {
            jkey->type = JSON_ARRAY;
            value->vcmd = JSON_SAX_FINISH;
            _UPDATE_PARSE_OFFSET(1);
        }
        break;

    default:
        if (_json_parse_single_value(parse_ptr, str, jkey, value) < 0)
            goto err;
        break;
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
        parse_ptr->skip_blank(parse_ptr);
        _get_parse_ptr(parse_ptr, 0, 1, &str);
        /* parse_ptr->parser.index > 0, parent is definitely not NULL. */
        if (parent->type == JSON_OBJECT) {
            switch (*str) {
            case ',':
                _UPDATE_PARSE_OFFSET(1);
#if !JSON_PARSE_LAST_COMMA
                goto next2;
#else
                parse_ptr->skip_blank(parse_ptr);
                _get_parse_ptr(parse_ptr, 0, 1, &str);
                if (*str != '}')
                    goto next2;
                FALLTHROUGH_ATTR;
#endif
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
#if !JSON_PARSE_LAST_COMMA
                goto next3;
#else
                parse_ptr->skip_blank(parse_ptr);
                _get_parse_ptr(parse_ptr, 0, 1, &str);
                if (*str != ']')
                    goto next3;
                FALLTHROUGH_ATTR;
#endif
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
    parse_val.mem = &s_invalid_json_mem;
    parse_val.fd = -1;
    if (choice->path) {
        if ((parse_val.fd = open(choice->path, O_RDONLY|O_BINARY)) < 0) {
            JsonErr("open(%s) failed!\n", choice->path);
            return -1;
        }
        parse_val.skip_blank = _skip_blank;
    } else {
        parse_val.str = choice->str;
        parse_val.size = choice->str_len ? choice->str_len : strlen(choice->str);
        parse_val.skip_blank = _skip_blank_rapid;
    }
    parse_val.parse_string = _json_sax_parse_string;
    parse_val.cb = choice->cb;

#if !JSON_PARSE_SINGLE_VALUE
    parse_val.skip_blank(&parse_val);
    if (parse_val.str[parse_val.offset] != '{' && parse_val.str[parse_val.offset] != '[') {
        JsonErr("The first object isn't object or array!\n");
        goto end;
    }
#endif

    ret = _json_sax_parse_value(&parse_val);
#if !JSON_PARSE_FINISHED_CHAR
    if (ret == 0) {
        parse_val.skip_blank(&parse_val);
        if (parse_val.str[parse_val.offset]) {
            JsonErr("Extra trailing characters!\n%s\n", parse_val.str + parse_val.offset);
            ret = -1;
        }
    }
#endif

#if !JSON_PARSE_SINGLE_VALUE
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
