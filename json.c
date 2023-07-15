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
 * Algorithm to convert double to string
 * 0: ljson dtoa developed by Jing Leng, fastest and best
 * 1: sprintf of C standard library, slowest
 * 2: grisu2 dtoa developed by Google
 * 3: dragonbox dtoa developed by Raffaello Giulietti, faster than grisu2
 */
#ifndef JSON_DTOA_ALGORITHM
#define JSON_DTOA_ALGORITHM             0
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

#if !FAST_FILL_DIGITS_EXISTED
#define FAST_FILL_DIGITS_EXISTED  1

static const char ch_100_lut[200] = {
    '0','0','0','1','0','2','0','3','0','4','0','5','0','6','0','7','0','8','0','9',
    '1','0','1','1','1','2','1','3','1','4','1','5','1','6','1','7','1','8','1','9',
    '2','0','2','1','2','2','2','3','2','4','2','5','2','6','2','7','2','8','2','9',
    '3','0','3','1','3','2','3','3','3','4','3','5','3','6','3','7','3','8','3','9',
    '4','0','4','1','4','2','4','3','4','4','4','5','4','6','4','7','4','8','4','9',
    '5','0','5','1','5','2','5','3','5','4','5','5','5','6','5','7','5','8','5','9',
    '6','0','6','1','6','2','6','3','6','4','6','5','6','6','6','7','6','8','6','9',
    '7','0','7','1','7','2','7','3','7','4','7','5','7','6','7','7','7','8','7','9',
    '8','0','8','1','8','2','8','3','8','4','8','5','8','6','8','7','8','8','8','9',
    '9','0','9','1','9','2','9','3','9','4','9','5','9','6','9','7','9','8','9','9',
};

static const uint8_t tz_100_lut[100] = {
    2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

#define FAST_DIV10(n)       (ch_100_lut[(n) << 1] - '0')                    /* 0 <= n < 100 */
#define FAST_DIV100(n)      (((n) * 5243) >> 19)                            /* 0 <= n < 10000 */
#define FAST_DIV10000(n)    ((uint32_t)(((uint64_t)(n) * 109951163) >> 40)) /* 0 <= n < 100000000 */

static inline int32_t fill_1_4_digits(char *buffer, uint32_t digits, int32_t *ptz)
{
    char *s = buffer;

    if (digits < 100) {
        if (digits >= 10) {
            *ptz = tz_100_lut[digits];
            memcpy(s, &ch_100_lut[digits<<1], 2);
            s += 2;
        } else {
            *ptz = 0;
            *s++ = digits + '0';
        }
    } else {
        uint32_t q = FAST_DIV100(digits);
        uint32_t r = digits - q * 100;

        if (q >= 10) {
            *ptz = tz_100_lut[q];
            memcpy(s, &ch_100_lut[q<<1], 2);
            s += 2;
        } else {
            *ptz = 0;
            *s++ = q + '0';
        }

        if (!r) {
            *ptz += 2;
            memset(s, '0', 2);
            s += 2;
        } else {
            *ptz = tz_100_lut[r];
            memcpy(s, &ch_100_lut[r<<1], 2);
            s += 2;
        }
    }

    return s - buffer;
}

static inline int32_t fill_t_4_digits(char *buffer, uint32_t digits, int32_t *ptz)
{
    char *s = buffer;
    uint32_t q = FAST_DIV100(digits);
    uint32_t r = digits - q * 100;

    memcpy(s, &ch_100_lut[q<<1], 2);
    memcpy(s + 2, &ch_100_lut[r<<1], 2);

    if (!r) {
        *ptz = tz_100_lut[q] + 2;
    } else {
        *ptz = tz_100_lut[r];
    }

    return 4;
}

static inline int32_t fill_1_8_digits(char *buffer, uint32_t digits, int32_t *ptz)
{
    char *s = buffer;

    if (digits < 10000) {
        return fill_1_4_digits(s, digits, ptz);
    } else {
        uint32_t q = FAST_DIV10000(digits);
        uint32_t r = digits - q * 10000;

        s += fill_1_4_digits(s, q, ptz);
        if (!r) {
            *ptz += 4;
            memset(s, '0', 4);
            s += 4;
        } else {
            s += fill_t_4_digits(s, r, ptz);
        }
    }

    return s - buffer;
}

static inline int32_t fill_t_8_digits(char *buffer, uint32_t digits, int32_t *ptz)
{
    char *s = buffer;

    if (digits < 10000) {
        memset(s, '0', 4);
        fill_t_4_digits(s + 4, digits, ptz);
    } else {
        uint32_t q = FAST_DIV10000(digits);
        uint32_t r = digits - q * 10000;

        fill_t_4_digits(s, q, ptz);
        if (!r) {
            memset(s + 4, '0', 4);
            *ptz += 4;
        } else {
            fill_t_4_digits(s + 4, r, ptz);
        }
    }

    return 8;
}

static inline int32_t fill_1_16_digits(char *buffer, uint64_t digits, int32_t *ptz)
{
    char *s = buffer;

    if (digits < 100000000llu) {
        return fill_1_8_digits(s, (uint32_t)digits, ptz);
    } else {
        uint32_t q = (uint32_t)(digits / 100000000);
        uint32_t r = (uint32_t)(digits - (uint64_t)q * 100000000);

        s += fill_1_8_digits(s, q, ptz);
        if (!r) {
            *ptz += 8;
            memset(s, '0', 8);
            s += 8;
        } else {
            s += fill_t_8_digits(s, r, ptz);
        }
    }

    return s - buffer;
}

static inline int32_t fill_t_16_digits(char *buffer, uint64_t digits, int32_t *ptz)
{
    char *s = buffer;

    if (digits < 100000000llu) {
        memset(s, '0', 8);
        fill_t_8_digits(s + 8, digits, ptz);
    } else {
        uint32_t q = (uint32_t)(digits / 100000000);
        uint32_t r = (uint32_t)(digits - (uint64_t)q * 100000000);

        fill_t_8_digits(s, q, ptz);
        if (!r) {
            memset(s + 8, '0', 8);
            *ptz += 8;
        } else {
            fill_t_8_digits(s + 8, r, ptz);
        }
    }

    return 16;
}

static inline int32_t fill_1_20_digits(char *buffer, uint64_t digits, int32_t *ptz)
{
    char *s = buffer;

    if (digits < 10000000000000000llu) {
        return fill_1_16_digits(s, digits, ptz);
    } else {
        uint32_t q = (uint32_t)(digits / 10000000000000000llu);
        uint64_t r = (digits - (uint64_t)q * 10000000000000000llu);

        s += fill_1_4_digits(s, q, ptz);
        if (!r) {
            memset(s, '0', 16);
            s += 16;
            *ptz = 16;
        } else {
            s += fill_t_16_digits(s, r, ptz);
        }
    }

    return s - buffer;
}

#endif

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

#if INT_MAX > 2147483647U
INT_TO_STRING_FUNC(int_to_string, int)
#else
static int int_to_string(int num, char *buffer)
{
    char *s = buffer;
    uint32_t n = 0;
    int32_t tz = 0;

    if (num == 0) {
        memcpy(s, "0", 2);
        return 1;
    }

    if (num < 0) {
        n = -num;
        *s++ = '-';
    } else {
        n = num;
    }

    if (n < 100000000) {
        s += fill_1_8_digits(s, (uint32_t)n, &tz);
    } else {
        uint32_t q = n / 100000000;
        uint32_t r = n - q * 100000000;

        if (q >= 10) {
            tz = tz_100_lut[q];
            memcpy(s, &ch_100_lut[q<<1], 2);
            s += 2;
        } else {
            tz = 0;
            *s++ = q + '0';
        }

        if (!r) {
            tz += 8;
            memset(s, '0', 8);
            s += 8;
        } else {
            s += fill_t_8_digits(s, r, &tz);
        }
    }

    return s - buffer;
}
#endif

#if LLONG_MAX > 9223372036854775807LLU
INT_TO_STRING_FUNC(lint_to_string, long long int)
#else
static int lint_to_string(long long int num, char *buffer)
{
    char *s = buffer;
    uint64_t n = 0;
    int32_t tz = 0;

    if (num == 0) {
        memcpy(s, "0", 2);
        return 1;
    }

    if (num < 0) {
        n = -num;
        *s++ = '-';
    } else {
        n = num;
    }

    s += fill_1_20_digits(buffer, n, &tz);

    return s - buffer;
}
#endif

HEX_TO_STRING_FUNC(hex_to_string, unsigned int)
#if JSON_LONG_LONG_SUPPORT
HEX_TO_STRING_FUNC(lhex_to_string, unsigned long long int)
#endif

#if JSON_DTOA_ALGORITHM == 1

static inline int double_to_string(double value, char* buffer)
{
    return sprintf(buffer, "%1.16g", value);
}

#elif JSON_DTOA_ALGORITHM == 2

#include "grisu2.h"
#define double_to_string grisu2_dtoa

#elif JSON_DTOA_ALGORITHM == 3

#include "dragonbox.h"
#define double_to_string dragonbox_dtoa

#else

#define DIY_SIGNIFICAND_SIZE        64                  /* Symbol: 1 bit, Exponent, 11 bits, Mantissa, 52 bits */
#define DP_SIGNIFICAND_SIZE         52                  /* Mantissa, 52 bits */
#define DP_EXPONENT_OFFSET          0x3FF               /* Exponent offset is 0x3FF */
#define DP_EXPONENT_MAX             0x7FF               /* Max Exponent value */
#define DP_EXPONENT_MASK            0x7FF0000000000000  /* Exponent Mask, 11 bits */
#define DP_SIGNIFICAND_MASK         0x000FFFFFFFFFFFFF  /* Mantissa Mask, 52 bits */
#define DP_HIDDEN_BIT               0x0010000000000000  /* Integer bit for Mantissa */
#define LSHIFT_RESERVED_BIT         11

typedef struct {
    uint64_t f;
    int32_t e;
} diy_fp_t;

static inline uint64_t u128_calc(uint64_t x, uint64_t y)
{
    const uint64_t div = 10000000000000000000llu;
    uint64_t hi, lo, ret;
    double val;

#if defined(_MSC_VER) && defined(_M_AMD64)
    lo = _umul128(x, y, &hi);
#elif (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6) || __clang_major__ >= 9) && (__WORDSIZE == 64)
    __extension__ typedef unsigned __int128 uint128;
    const uint128 p = (uint128)x * (uint128)y;
    hi = (uint64_t)(p >> 64);
    lo = (uint64_t)p;
#else
    const uint64_t M32 = 0XFFFFFFFF;
    const uint64_t a = x >> 32;
    const uint64_t b = x & M32;
    const uint64_t c = y >> 32;
    const uint64_t d = y & M32;

    const uint64_t ac = a * c;
    const uint64_t bc = b * c;
    const uint64_t ad = a * d;
    const uint64_t bd = b * d;

    const uint64_t mid1 = ad + (bd >> 32);
    const uint64_t mid2 = bc + (mid1 & M32);

    hi = ac + (mid1 >> 32) + (mid2 >> 32);
    lo = (bd & M32) | (mid2 & M32) << 32;
#endif

    val = hi * 1.8446744073709551616; /* 1<<64 = 18446744073709551616 */
    ret = (uint64_t)val;
    if (lo >= div)
        ++hi;
    if (val - ret >= 0.49)
        ++hi;

    return ret;
}

static inline int32_t u64_pz_get(uint64_t f)
{
#if defined(_MSC_VER) && defined(_M_AMD64)
    unsigned long index;
    _BitScanReverse64(&index, f);
    return 63 - index;
#elif defined(__GNUC__) || defined(__clang__)
    return __builtin_clzll(f);
#else
    int32_t index = DP_SIGNIFICAND_SIZE; /* max value of f is smaller than pow(2, 53) */
    while (!(f & ((uint64_t)1 << index)))
        --index;
    return 63 - index;
#endif
}

/*
# python to get lut

def print_pow_array(unit, index, end, positive):
    cmp_value = 10000000000000000000 # ULONG_MAX = 18446744073709551615
    base_lut = []
    index_lut = []
    for i in range(end + 1):
        a = unit ** i
        b = a
        j = 0
        if a < cmp_value:
            while b < cmp_value:
                j += 1
                b = a * (10 ** j)
            j -= 1
            b = a * (10 ** j)
            j = i * index + j

        else:
            while b >= cmp_value:
                j += 1
                b = a // (10 ** j)
            b = a // (10 ** j)
            if b != cmp_value:
                b += 1;
            j = i * index - j

        #print('%-3d: %d 0x%016x %d' % (i, j, b, b))
        base_lut.append(b)
        index_lut.append(j)

    print('static const uint64_t %s_base_lut[%d] = {' % ('positive' if positive else 'negative', end + 1), end='')
    for i in range(end + 1):
        if i % 4 == 0:
            print()
            print('    ', end='')
        print('0x%016x' % (base_lut[i]), end='')
        if i != end:
            print(', ', end='');
        else:
            print()
            print('};')

    print()
    print('static const int8_t %s_index_lut[%d] = {' % ('positive' if positive else 'negative', end + 1), end='')
    for i in range(end + 1):
        if i % 20 == 0:
            print()
            print('    ', end='')
        print('%-3d' % (index_lut[i]), end='')
        if i != end:
            print(', ', end='');
        else:
            print()
            print('};')

def print_positive_array():
    # 1 * (2 ** 4) = 1.6 * (10 ** 1)
    # 16 = 1.6 * (10 ** 1)
    # 242 = (1023 - 52) / 4
    print_pow_array(16, 1, 242, True)

def print_negative_array():
    # 1 * (2 ** -4) = 0.625 * (10 ** -1)
    # 625 = 0.625 * (10 ** 3)
    # 282 = (1022 + 52 + (63 - 11)) / 4 + 1
    print_pow_array(625, 3, 282, False)

print_positive_array()
print()
print_negative_array()
*/

static inline diy_fp_t positive_diy_fp(int32_t e)
{
    static const uint64_t positive_base_lut[243] = {
        0x0de0b6b3a7640000, 0x16345785d8a00000, 0x2386f26fc1000000, 0x38d7ea4c68000000,
        0x5af3107a40000000, 0x0e8d4a5100000000, 0x174876e800000000, 0x2540be4000000000,
        0x3b9aca0000000000, 0x5f5e100000000000, 0x0f42400000000000, 0x186a000000000000,
        0x2710000000000000, 0x3e80000000000000, 0x6400000000000000, 0x1000000000000000,
        0x199999999999999a, 0x28f5c28f5c28f5c3, 0x4189374bc6a7ef9e, 0x68db8bac710cb296,
        0x10c6f7a0b5ed8d37, 0x1ad7f29abcaf4858, 0x2af31dc4611873c0, 0x44b82fa09b5a52cc,
        0x6df37f675ef6eae0, 0x119799812dea111a, 0x1c25c268497681c3, 0x2d09370d42573604,
        0x480ebe7b9d58566d, 0x734aca5f6226f0ae, 0x12725dd1d243aba1, 0x1d83c94fb6d2ac35,
        0x2f394219248446bb, 0x4b8ed0283a6d3df8, 0x78e480405d7b9659, 0x1357c299a88ea76b,
        0x1ef2d0f5da7dd8ab, 0x318481895d962777, 0x4f3a68dbc8f03f25, 0x7ec3daf941806507,
        0x14484bfeebc29f87, 0x2073accb12d0ff3e, 0x33ec47ab514e652f, 0x5313a5dee87d6eb1,
        0x84ec3c97da624ab5, 0x154484932d2e725b, 0x22073a8515171d5e, 0x3671f73b54f1c896,
        0x571cbec554b60dbc, 0x0df01e85f912e37b, 0x164cfda3281e38c4, 0x23ae629ea696c139,
        0x391704310a8acec2, 0x5b5806b4ddaae469, 0x0e9d71b689dde71b, 0x17624f8a762fd82c,
        0x256a18dd89e626ac, 0x3bdcf495a9703de0, 0x5fc7edbc424d2fcc, 0x0f53304714d9265e,
        0x18851a0b548ea3ca, 0x273b5cdeedb10610, 0x3ec56164af81a34c, 0x646f023ab2690546,
        0x1011c2eaabe7d7e3, 0x19b604aaaca62637, 0x29233aaaadd6a38b, 0x41d1f7777c8a9f45,
        0x694ff258c7443208, 0x10d9976a5d52975e, 0x1af5bf109550f22f, 0x2b22cb4dbbb4b6b2,
        0x4504787c5f878ab6, 0x6e6d8d93cc0c1123, 0x11ab20e472914a6c, 0x1c45016d841baa47,
        0x2d3b357c0692aa0b, 0x485ebbf9a41ddcdd, 0x73cac65c39c96162, 0x1286d80ec190dc62,
        0x1da48ce468e7c703, 0x2f6dae3a4172d804, 0x4be2b05d35848cd3, 0x796ab3c855a0e152,
        0x136d3b7c36a919d0, 0x1f152bf9f10e8fb3, 0x31bb798fe8174c51, 0x4f925c1973587a1c,
        0x7f50935bebc0c35f, 0x145ecfe5bf520ac8, 0x2097b309321cde0c, 0x3425eb41e9c7c9ad,
        0x536fdecfdc72dc48, 0x857fcae62d8493a6, 0x155c2076bf9a5511, 0x222d00bdff5d54e7,
        0x36ae679665622172, 0x577d728a3bd03582, 0x0dff9772470297ec, 0x1665bf1d3e6a8cad,
        0x23d5fe9530aa7aae, 0x39566421e7772ab0, 0x5bbd6d030bf1dde6, 0x0eadab0aba3b2dbf,
        0x177c44ddf6c515fe, 0x2593a163246e8996, 0x3c1f689ea0b0dc23, 0x603240fdcde7c69d,
        0x0f64335bcf065d38, 0x18a0522c7e709527, 0x2766e9e0ca4dbb71, 0x3f0b0fce107c5f1a,
        0x64de7fb01a60982a, 0x1023998cd1053711, 0x19d28f47b4d524e8, 0x2950e53f87bb6e40,
        0x421b0865a5f8b066, 0x69c4da3c3cc11a3d, 0x10ec4be0ad8f8952, 0x1b13ac9aaf4c0ee9,
        0x2b52adc44bace4a8, 0x45511606df7b0773, 0x6ee8233e325e7251, 0x11bebdf578b2f392,
        0x1c6463225ab7ec1d, 0x2d6d6b6a2abfe02f, 0x48af1243779966b1, 0x744b506bf28f0ab4,
        0x129b69070816e2fe, 0x1dc574d80cf16b30, 0x2fa2548ce182451a, 0x4c36edae359d3b5c,
        0x79f17c49ef61f894, 0x1382cc34ca2427c6, 0x1f37ad21436d0c70, 0x31f2ae9b9f14e0b3,
        0x4feab0f8fe87cdea, 0x7fdde7f4ca72e310, 0x14756ccb01abfb5f, 0x20bbe144cf799232,
        0x345fced47f28e9e9, 0x53cc7e20cb74a974, 0x8613fd0145877586, 0x1573d68f903ea22a,
        0x2252f0e5b39769dd, 0x36eb1b091f58a961, 0x57de91a832277568, 0x0e0f218b8d25088c,
        0x167e9c127b6e7413, 0x23fdc683f8b0b9b8, 0x39960a6cc11ac2bf, 0x5c2343e134f79dfe,
        0x0ebdf661791d60f6, 0x179657025b6234bc, 0x25bd5803c569edfa, 0x3c62266c6f0fe329,
        0x609d0a4718196b74, 0x0f7549530e188c13, 0x18bba884e35a79b8, 0x2792a73b055d8f8c,
        0x3f510b91a22f4c13, 0x654e78e9037ee01e, 0x103583fc527ab338, 0x19ef3993b72ab85a,
        0x297ec285f1ddf3c3, 0x42646a6fe9631f9e, 0x6a3a43e642383296, 0x10ff151a99f482fa,
        0x1b31bb5dc320d18f, 0x2b82c562d1ce1c18, 0x459e089e1c7cf9c0, 0x6f6340fcfa618f99,
        0x11d270cc51055ea8, 0x1c83e7ad4e6efdda, 0x2d9fd9154a4b2fc3, 0x48ffc1bbaa11e604,
        0x74cc692c434fd66c, 0x12b010d3e1cf5582, 0x1de6815302e5559d, 0x2fd735519e3bbc2e,
        0x4c8b888296c5f9e3, 0x7a78da6a8ad65c9e, 0x139874ddd8c6234d, 0x1f5a549627a36bae,
        0x322a20f03f6bdf7d, 0x504367e6cbdfcbfa, 0x806bd9714632dff7, 0x148c22ca71a1bd70,
        0x20e037aa4f692f19, 0x3499f2aa18a84b5a, 0x542984435aa6def6, 0x86a8d39ef77164bd,
        0x158ba6fab6f36c48, 0x22790b2abe5246d9, 0x372811ddfd50715a, 0x58401c96621a4ef7,
        0x0e1ebce4dc7f16e0, 0x169794a160cb57cd, 0x2425ba9bce122614, 0x39d5f75fb01d09ba,
        0x5c898bcc4cfb42c3, 0x0ece53cec4a314ec, 0x17b08617a104ee47, 0x25e73cf29b3b16d7,
        0x3ca52e50f85e8af2, 0x61084a1b26fdab1c, 0x0f867241c8cc6d4d, 0x18d71d360e13e214,
        0x27be952349b969b9, 0x3f97550542c242c1, 0x65beee6ed136d135, 0x1047824f2bb6d9cb,
        0x1a0c03b1df8af612, 0x29acd2b63277f01c, 0x42ae1df050bfe694, 0x6ab02fe6e79970ec,
        0x1111f32f2f4bc026, 0x1b4feb7eb212cd0a, 0x2bb31264501e14dc, 0x45eb50a08030215f,
        0x6fdee76733803565, 0x11e6398126f5cb1b, 0x1ca38f350b22de91, 0x2dd27ebb4504974e,
        0x4950cac53b3a8bb0, 0x754e113b91f745e6, 0x12c4cf8ea6b6ec77, 0x1e07b27dd78b13f2,
        0x300c50c958de864f, 0x4ce0814227ca707e, 0x7b00ced03faa4d96, 0x13ae3591f5b4d937,
        0x1f7d228322baf525, 0x3261d0d1d12b21d4, 0x509c814fb511cfba, 0x80fa687f881c7f8f,
        0x14a2f1ffecd15c17, 0x2104b66647b56025, 0x34d4570a0c5566a1, 0x5486f1a9ad557102,
        0x873e4f75e2224e69, 0x15a391d56bdc876d, 0x229f4fbbdfc73f15
    };

    static const int8_t positive_index_lut[243] = {
        18 , 18 , 18 , 18 , 18 , 17 , 17 , 17 , 17 , 17 , 16 , 16 , 16 , 16 , 16 , 15 , 15 , 15 , 15 , 15 ,
        14 , 14 , 14 , 14 , 14 , 13 , 13 , 13 , 13 , 13 , 12 , 12 , 12 , 12 , 12 , 11 , 11 , 11 , 11 , 11 ,
        10 , 10 , 10 , 10 , 10 , 9  , 9  , 9  , 9  , 8  , 8  , 8  , 8  , 8  , 7  , 7  , 7  , 7  , 7  , 6  ,
        6  , 6  , 6  , 6  , 5  , 5  , 5  , 5  , 5  , 4  , 4  , 4  , 4  , 4  , 3  , 3  , 3  , 3  , 3  , 2  ,
        2  , 2  , 2  , 2  , 1  , 1  , 1  , 1  , 1  , 0  , 0  , 0  , 0  , 0  , -1 , -1 , -1 , -1 , -2 , -2 ,
        -2 , -2 , -2 , -3 , -3 , -3 , -3 , -3 , -4 , -4 , -4 , -4 , -4 , -5 , -5 , -5 , -5 , -5 , -6 , -6 ,
        -6 , -6 , -6 , -7 , -7 , -7 , -7 , -7 , -8 , -8 , -8 , -8 , -8 , -9 , -9 , -9 , -9 , -9 , -10, -10,
        -10, -10, -10, -11, -11, -11, -11, -12, -12, -12, -12, -12, -13, -13, -13, -13, -13, -14, -14, -14,
        -14, -14, -15, -15, -15, -15, -15, -16, -16, -16, -16, -16, -17, -17, -17, -17, -17, -18, -18, -18,
        -18, -18, -19, -19, -19, -19, -19, -20, -20, -20, -20, -20, -21, -21, -21, -21, -22, -22, -22, -22,
        -22, -23, -23, -23, -23, -23, -24, -24, -24, -24, -24, -25, -25, -25, -25, -25, -26, -26, -26, -26,
        -26, -27, -27, -27, -27, -27, -28, -28, -28, -28, -28, -29, -29, -29, -29, -29, -30, -30, -30, -30,
        -30, -31, -31
    };

    const diy_fp_t v = { .f = positive_base_lut[e], .e = positive_index_lut[e] };
    return v;
}

static inline diy_fp_t negative_diy_fp(int32_t e)
{
    static const uint64_t negative_base_lut[283] = {
        0x0de0b6b3a7640000, 0x56bc75e2d6310000, 0x3635c9adc5dea000, 0x21e19e0c9bab2400,
        0x152d02c7e14af680, 0x84595161401484a0, 0x52b7d2dcc80cd2e4, 0x33b2e3c9fd0803cf,
        0x204fce5e3e250262, 0x1431e0fae6d7217d, 0x7e37be2022c0914c, 0x4ee2d6d415b85acf,
        0x314dc6448d9338c2, 0x1ed09bead87c0379, 0x13426172c74d822c, 0x785ee10d5da46d91,
        0x4b3b4ca85a86c47b, 0x2f050fe938943acd, 0x1d6329f1c35ca4c0, 0x125dfa371a19e6f8,
        0x72cb5bd86321e38d, 0x47bf19673df52e38, 0x2cd76fe086b93ce3, 0x1c06a5ec5433c60e,
        0x118427b3b4a05bc9, 0x6d79f82328ea3da7, 0x446c3b15f9926688, 0x2ac3a4edbbfb8015,
        0x1aba4714957d300e, 0x10b46c6cdd6e3e09, 0x6867a5a867f103b3, 0x4140c78940f6a250,
        0x28c87cb5c89a2572, 0x197d4df19d605768, 0x0fee50b7025c36a1, 0x63917877cec0556c,
        0x3e3aeb4ae1383563, 0x26e4d30eccc3215e, 0x184f03e93ff9f4db, 0x0f316271c7fc3909,
        0x5ef4a74721e86477, 0x3b58e88c75313eca, 0x25179157c93ec73f, 0x172ebad6ddc73c87,
        0x0e7d34c64a9c85d5, 0x5a8e89d75252446f, 0x3899162693736ac6, 0x235fadd81c2822bc,
        0x161bcca7119915b6, 0x8a2dbf142dfcc7ac, 0x565c976c9cbdfccc, 0x35f9dea3e1f6bdff,
        0x21bc2b266d3a36c0, 0x15159af804446238, 0x83c7088e1aab65dc, 0x525c6558d0ab1faa,
        0x3379bf57826af3ca, 0x202c1796b182d85f, 0x141b8ebe2ef1c73b, 0x7dac3c24a5671d30,
        0x4e8ba596e760723e, 0x3117477e509c4767, 0x1eae8caef261aca1, 0x132d17ed577d0be5,
        0x77d9d58b62cd8a52, 0x4ae825771dc07673, 0x2ed1176a72984a08, 0x1d42aea2879f2e45,
        0x1249ad2594c37cec, 0x724c7a2ae1c5ccbe, 0x476fcc5acd1b9ff7, 0x2ca5dfb8c03143fa,
        0x1be7abd3781eca7d, 0x1170cb642b133e8e, 0x6d00f7320d3846f5, 0x44209a7f48432c5a,
        0x2a94608f8d29fbb8, 0x1a9cbc59b83a3d53, 0x10a1f5b813246654, 0x67f43fbe77a37f8c,
        0x40f8a7d70ac62fb8, 0x289b68e666bbddd3, 0x1961219000356aa4, 0x0fdcb4fa002162a7,
        0x63236b1a80d0a88f, 0x3df622f09082695a, 0x26b9d5d65a5181d8, 0x183425a5f872f127,
        0x0f209787bb47d6b9, 0x5e8bb3105280fe00, 0x3b174fea33909ec0, 0x24ee91f2603a6338,
        0x17151b377c247e03, 0x0e6d3102ad96cec2, 0x5a2a7250bcee8c3c, 0x385a8772761517a6,
        0x233894a789cd2ec8, 0x16035ce8b6203d3d, 0x899504ae72497ebb, 0x55fd22ed076def35,
        0x35be35d424a4b581, 0x2196e1a496e6f171, 0x14fe4d06de5056e7, 0x8335616aed761f20,
        0x52015ce2d469d374, 0x3340da0dc4c22429, 0x200888489af9569a, 0x1405552d60dbd620,
        0x7d21545b9d5dfa47, 0x4e34d4b9425abc6c, 0x30e104f3c978b5c4, 0x1e8ca3185deb719b,
        0x1317e5ef3ab32701, 0x77555d172edfb3c3, 0x4a955a2e7d4bd05a, 0x2e9d585d0e4f6238,
        0x1d22573a28f19d63, 0x123576845997025e, 0x71ce24bb2fefcecb, 0x4720d6f4fdf5e13f,
        0x2c7486591eb9acc8, 0x1bc8d3f7b3340bfd, 0x115d847ad000877e, 0x6c887bff94034ed3,
        0x43d54d7fbc821144, 0x2a65506fd5d14acb, 0x1a7f5245e5a2cebf, 0x108f936baf85c137,
        0x678159610903f798, 0x40b0d7dca5a27abf, 0x286e86e9e7858cb8, 0x1945145230b377f3,
        0x0fcb2cb35e702af8, 0x62b5d7610e3d0c8c, 0x3db1a69ca8e627d7, 0x268f0821e98fd8e7,
        0x1819651531f9e790, 0x0f0fdf2d3f3c30ba, 0x5e2332dacb38308b, 0x3ad5ffc8bf031e57,
        0x24c5bfdd7761f2f7, 0x16fb97ea6a9d37da, 0x0e5d3ef282a242e9, 0x59c6c96bb076222b,
        0x381c3de34e49d55b, 0x2311a6ae10ee2559, 0x15eb082cca94d758, 0x88fcf317f22241e3,
        0x559e17eef755692e, 0x3582cef55a9561bd, 0x2171c159589d5d16, 0x14e718d7d7625a2e,
        0x82a45b450226b39d, 0x51a6b90b21583043, 0x330833a6f4d71e2a, 0x1fe52048590672da,
        0x13ef342d37a407c9, 0x7c97061a9bc130a3, 0x4dde63d0a158be66, 0x30aafe6264d77700,
        0x1e6adefd7f06aa60, 0x1302cb5e6f642a7c, 0x76d1770e38320987, 0x4a42ea68e31f45f4,
        0x2e69d2818df38bb9, 0x1d022390f8b83754, 0x1221563a9b732295, 0x71505aee4b8f981e,
        0x46d238d4ef39bf13, 0x2c4363851584176c, 0x1baa1e332d728ea4, 0x114a52dffc679926,
        0x6c1085f7e9877d2e, 0x438a53baf1f4ae3d, 0x2a367454d738ece6, 0x1a6208b506839410,
        0x107d457124123c8a, 0x670ef2032171fa5d, 0x40695741f4e73c7a, 0x2841d689391085cd,
        0x19292615c3aa53a0, 0x0fb9b7cd9a4a7444, 0x6248bcc5045156a8, 0x3d6d75fb22b2d629,
        0x266469bcf5afc5da, 0x17fec216198ddba8, 0x0eff394dcff8a949, 0x5dbb262653d22208,
        0x3a94f7d7f4635545, 0x249d1ae6f8be154c, 0x16e230d05b76cd4f, 0x0e4d5e82392a4052,
        0x59638eade54811fd, 0x37de392caf4d0b3e, 0x22eae3bbed902707, 0x15d2ce55747a1865,
        0x8865899617fb1872, 0x553f75fdcefcef47, 0x3547a9bea15e158d, 0x214cca1724dacd78,
        0x14cffe4e7708c06b, 0x8213f56a67f6b29c, 0x514c796280fa2fa2, 0x32cfcbdd909c5dc5,
        0x1fc1df6a7a61ba9b, 0x13d92ba28c7d14a1, 0x7c0d50b7ee0dc0ee, 0x4d885272f4c89895,
        0x30753387d8fd5f5d, 0x1e494034e79e5b9a, 0x12edc82110c2f941, 0x764e22cea8c295d2,
        0x49f0d5c129799da3, 0x2e368598b9ec0286, 0x1ce2137f74338194, 0x120d4c2fa8a030fd,
        0x70d31c29dde93229, 0x4683f19a2ab1bf5a, 0x2c1277005aaf1798, 0x1b8b8a6038ad6ebf,
        0x1137367c236c6538, 0x6b991487dd65789a, 0x433facd4ea5f6b61, 0x2a07cc05127ba31d,
        0x1a44df832b8d45f2, 0x106b0bb1fb384bb7, 0x669d0918621fd938, 0x402225af3d53e7c3,
        0x2815578d865470da, 0x190d56b873f4c689, 0x0fa856334878fc16, 0x61dc1ac084f42784,
        0x3d2990b8531898b3, 0x2639fa7333ef5f70, 0x17e43c8800759ba6, 0x0eeea5d500498148,
        0x5d538c7341cb67ff, 0x3a5437c8091f2100, 0x2474a2dd05b374a0, 0x16c8e5ca239028e4,
        0x0e3d8f9e563a198f, 0x5900c19d9aeb1fba, 0x37a0790280d2f3d4, 0x22c44ba19083d865,
        0x15baaf44fa52673f, 0x87cec76f1c830549, 0x54e13ca571d1e34e, 0x350cc5e767232e11,
        0x2127fbb0a075fccb, 0x14b8fd4e6449bdff, 0x81842f29f2cce376, 0x50f29d7a37c00e2a,
        0x3297a26c62d808db, 0x1f9ec583bdc70589, 0x13c33b72569c6376, 0x7b84338a9d516d9d,
        0x4d32a036a252e482, 0x303fa4222573ced2, 0x1e27c69557686143, 0x12d8dc1d56a13cca,
        0x75cb5fb75d6fbbed, 0x499f1bd29a65d574, 0x2e037163a07fa569, 0x1cc226de444fc762,
        0x11f9584aeab1dc9d, 0x705667d43ad7a2d4, 0x463600e4a4c6c5c5, 0x2be1c08ee6fc3b9b,
        0x1b6d1859505da541, 0x11242f37d23a8749, 0x6b22271ce1edcd85, 0x42f558720d34a073,
        0x29d957474840e448, 0x1a27d68c8d288ead, 0x1058e617d839592d, 0x662b9e1507666d54,
        0x3fdb42cd24a00455, 0x27e909c036e402b5, 0x18f1a618224e81b1, 0x0f9707cf1571110f,
        0x616ff0ce4602aa9b, 0x3ce5f680ebc1aaa1, 0x260fba1093590aa5
    };

    static const int8_t negative_index_lut[283] = {
        18 , 19 , 19 , 19 , 19 , 20 , 20 , 20 , 20 , 20 , 21 , 21 , 21 , 21 , 21 , 22 , 22 , 22 , 22 , 22 ,
        23 , 23 , 23 , 23 , 23 , 24 , 24 , 24 , 24 , 24 , 25 , 25 , 25 , 25 , 25 , 26 , 26 , 26 , 26 , 26 ,
        27 , 27 , 27 , 27 , 27 , 28 , 28 , 28 , 28 , 29 , 29 , 29 , 29 , 29 , 30 , 30 , 30 , 30 , 30 , 31 ,
        31 , 31 , 31 , 31 , 32 , 32 , 32 , 32 , 32 , 33 , 33 , 33 , 33 , 33 , 34 , 34 , 34 , 34 , 34 , 35 ,
        35 , 35 , 35 , 35 , 36 , 36 , 36 , 36 , 36 , 37 , 37 , 37 , 37 , 37 , 38 , 38 , 38 , 38 , 39 , 39 ,
        39 , 39 , 39 , 40 , 40 , 40 , 40 , 40 , 41 , 41 , 41 , 41 , 41 , 42 , 42 , 42 , 42 , 42 , 43 , 43 ,
        43 , 43 , 43 , 44 , 44 , 44 , 44 , 44 , 45 , 45 , 45 , 45 , 45 , 46 , 46 , 46 , 46 , 46 , 47 , 47 ,
        47 , 47 , 47 , 48 , 48 , 48 , 48 , 49 , 49 , 49 , 49 , 49 , 50 , 50 , 50 , 50 , 50 , 51 , 51 , 51 ,
        51 , 51 , 52 , 52 , 52 , 52 , 52 , 53 , 53 , 53 , 53 , 53 , 54 , 54 , 54 , 54 , 54 , 55 , 55 , 55 ,
        55 , 55 , 56 , 56 , 56 , 56 , 56 , 57 , 57 , 57 , 57 , 57 , 58 , 58 , 58 , 58 , 59 , 59 , 59 , 59 ,
        59 , 60 , 60 , 60 , 60 , 60 , 61 , 61 , 61 , 61 , 61 , 62 , 62 , 62 , 62 , 62 , 63 , 63 , 63 , 63 ,
        63 , 64 , 64 , 64 , 64 , 64 , 65 , 65 , 65 , 65 , 65 , 66 , 66 , 66 , 66 , 66 , 67 , 67 , 67 , 67 ,
        67 , 68 , 68 , 68 , 68 , 69 , 69 , 69 , 69 , 69 , 70 , 70 , 70 , 70 , 70 , 71 , 71 , 71 , 71 , 71 ,
        72 , 72 , 72 , 72 , 72 , 73 , 73 , 73 , 73 , 73 , 74 , 74 , 74 , 74 , 74 , 75 , 75 , 75 , 75 , 75 ,
        76 , 76 , 76
    };

    const diy_fp_t v = { .f = negative_base_lut[e], .e = negative_index_lut[e] };
    return v;
}

static inline void ldouble_convert(diy_fp_t *v)
{
    uint64_t f = v->f;
    int32_t e = v->e, t = v->e;
    diy_fp_t x;

    e >>= 2;
    t -= e << 2;
    f <<= t;
    if (e >= 0) {
        x = positive_diy_fp(e);
    } else {
        x = negative_diy_fp(-e);
    }

    v->f = u128_calc(x.f, f);
    v->e = e - x.e + 19;
}

static inline int32_t fill_significand(char *buffer, uint64_t digits, int32_t *ptz)
{
   return fill_1_20_digits(buffer, digits, ptz);
}

static inline int32_t fill_exponent(int32_t K, char* buffer)
{
    int32_t i = 0, k = 0;

    if (K < 0) {
        buffer[i++] = '-';
        K = -K;
    } else {
        buffer[i++] = '+';
    }

    if (K < 100) {
        if (K < 10) {
            buffer[i++] = '0' + K;
        } else {
            memcpy(&buffer[i], &ch_100_lut[K<<1], 2);
            i += 2;
        }
    } else {
        k = FAST_DIV100(K);
        K -= k * 100;
        buffer[i++] = '0' + k;
        memcpy(&buffer[i], &ch_100_lut[K<<1], 2);
        i += 2;
    }

    return i;
}

static inline char* ldouble_format(char* buffer, uint64_t digits, int32_t decimal_exponent)
{
    int32_t num_digits, trailing_zeros, vnum_digits, decimal_point;

    num_digits = fill_significand(buffer + 1, digits + 2, &trailing_zeros);
    if (buffer[num_digits] >= '5') {
        buffer[num_digits] -= 2;
    } else {
        buffer[num_digits] = '0';
        for (trailing_zeros = 1; trailing_zeros < num_digits; ++trailing_zeros) {
            if (buffer[num_digits - trailing_zeros] != '0')
                break;
        }
    }
    vnum_digits = num_digits - trailing_zeros;
    decimal_point = num_digits + decimal_exponent;

    switch (decimal_point) {
    case -6: case -5: case -4: case -3: case -2: case -1: case 0:
         /* 0.[000]digits */
        memmove(buffer + 2 - decimal_point, buffer + 1, vnum_digits);
        memset(buffer, '0', 2 - decimal_point);
        buffer[1] = '.';
        buffer += 2 - decimal_point + vnum_digits;
        break;

    case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9:
    case 10: case 11: case 12: case 13: case 14: case 15: case 16: case 17:
        if (decimal_point < vnum_digits) {
            /* dig.its */
            memmove(buffer, buffer + 1, decimal_point);
            buffer[decimal_point] = '.';
            buffer += vnum_digits + 1;
        } else {
            /* digits[000] */
            memmove(buffer, buffer + 1, decimal_point);
            buffer += decimal_point;
            memcpy(buffer, ".0", 2);
            buffer += 2;
        }
        break;

    default:
        buffer[0] = buffer[1];
        ++buffer;
        if (vnum_digits != 1) {
            /* d.igitsE+123 */
            *buffer++ = '.';
            buffer += vnum_digits + 1;
        } else {
            /* dE+123 */
        }
        *buffer++ = 'e';
        buffer += fill_exponent(decimal_point - 1, buffer);
        break;
    }

    *buffer = '\0';
    return buffer;
}

static int ldouble_dtoa(double value, char* buffer)
{
    diy_fp_t v;
    char *s = buffer;
    union {double d; uint64_t n;} u = {.d = value};
    int32_t signbit = u.n >> (DIY_SIGNIFICAND_SIZE - 1);
    int32_t exponent = (u.n & DP_EXPONENT_MASK) >> DP_SIGNIFICAND_SIZE; /* Exponent */
    uint64_t significand = u.n & DP_SIGNIFICAND_MASK; /* Mantissa */

    if (signbit) {
        *s++ = '-';
    }

    switch (exponent) {
    case DP_EXPONENT_MAX:
        if (significand) {
            memcpy(buffer, "nan", 4);
            return 3;
        } else {
            memcpy(s, "inf", 4);
            return signbit + 3;
        }
        break;

    case 0:
        if (!significand) {
            /* no-normalized double */
            v.f = significand; /* Non-normalized double doesn't have a extra integer bit for Mantissa */
            v.e = 1 - DP_EXPONENT_OFFSET - DP_SIGNIFICAND_SIZE; /* Fixed Exponent: -1022, divisor of Mantissa: pow(2,52) */

            /* The smallest e is (-1022 - 52 - (63 - 11)) = -1126 */
            int32_t lshiftbit = u64_pz_get(v.f) - LSHIFT_RESERVED_BIT;
            v.f <<= lshiftbit;
            v.e -= lshiftbit; /* The smallest e is (-1022 - 52 - (63 - 11)) = -1126 */
        } else {
            memcpy(s, "0.0", 4);
            return signbit + 3;
        }
        break;

    default:
        /* normalized double */
        v.f = significand + DP_HIDDEN_BIT; /* Normalized double has a extra integer bit for Mantissa */
        v.e = exponent - DP_EXPONENT_OFFSET - DP_SIGNIFICAND_SIZE; /* Exponent offset: -1023, divisor of Mantissa: pow(2,52) */

        if (0 <= -v.e && -v.e <= DP_SIGNIFICAND_SIZE && ((v.f & (((uint64_t)1 << -v.e) - 1)) == 0)) {
            /* small integer. */
            int32_t tz, n;
            n = fill_significand(s, v.f >> -v.e, &tz);
            memcpy(s + n, ".0", 3);
            return n + 2 + signbit;
        } else {
            /* The largest e is (1023 - 52) = 971 */
        }
        break;
    }

    ldouble_convert(&v);
    s = ldouble_format(s, v.f, v.e);
    return s - buffer;
}
#define double_to_string ldouble_dtoa

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
    int sign = 1, k = 0;
    unsigned long long int m = 0, n = 0;
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
        static const double div10_lut[20] = {
            1    , 1e-1 , 1e-2 , 1e-3 , 1e-4 , 1e-5 , 1e-6 , 1e-7 , 1e-8 , 1e-9 ,
            1e-10, 1e-11, 1e-12, 1e-13, 1e-14, 1e-15, 1e-16, 1e-17, 1e-18, 1e-19,
        };

        switch (*num) {
        case '-': ++num; sign = -1; break;
        case '+': ++num; break;
        default: break;
        }

        while (*num == '0')
            ++num;

        k = 0;
        while (_dec_char_check(*num)) {
            m = (m << 3) + (m << 1) + (*num++ - '0');
            ++k;
        }
        if (k >= 20)
            return 0xff;

        if (*num == '.') {
            ++num;
            k = 0;
            while (_dec_char_check(*num)) {
                n = (n << 3) + (n << 1) + (*num++ - '0');
                ++k;
            }
            if (k >= 20)
                return 0xff;

            d = m + n * div10_lut[k];

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
    int ret = JSON_NULL;
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
    case 0xff:
        vnum->vdbl = strtod(num, sstr);
        return JSON_DOUBLE;
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
