/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_LIST_H
#define SOUNDIO_LIST_H

#include "util.h"
#include "soundio_internal.h"

#include <assert.h>

#define SOUNDIO_LIST_STATIC static
#define SOUNDIO_LIST_NOT_STATIC

#define SOUNDIO_MAKE_LIST_STRUCT(Type, Name, static_kw) \
    struct Name { \
        Type *items; \
        int length; \
        int capacity; \
    };

#define SOUNDIO_MAKE_LIST_PROTO(Type, Name, static_kw) \
    static_kw void Name##_deinit(struct Name *s); \
    static_kw int SOUNDIO_ATTR_WARN_UNUSED_RESULT Name##_append(struct Name *s, Type item); \
    static_kw Type Name##_val_at(struct Name *s, int index); \
    static_kw Type * Name##_ptr_at(struct Name *s, int index); \
    static_kw Type Name##_pop(struct Name *s); \
    static_kw int SOUNDIO_ATTR_WARN_UNUSED_RESULT Name##_add_one(struct Name *s); \
    static_kw Type Name##_last_val(struct Name *s); \
    static_kw Type *Name##_last_ptr(struct Name *s); \
    static_kw int SOUNDIO_ATTR_WARN_UNUSED_RESULT Name##_resize(struct Name *s, int new_length); \
    static_kw void Name##_clear(struct Name *s); \
    static_kw int SOUNDIO_ATTR_WARN_UNUSED_RESULT \
        Name##_ensure_capacity(struct Name *s, int new_capacity); \
    static_kw Type Name##_swap_remove(struct Name *s, int index);


#define SOUNDIO_MAKE_LIST_DEF(Type, Name, static_kw) \
    SOUNDIO_ATTR_UNUSED \
    static_kw void Name##_deinit(struct Name *s) { \
        free(s->items); \
    } \
\
    SOUNDIO_ATTR_UNUSED \
    SOUNDIO_ATTR_WARN_UNUSED_RESULT \
    static_kw int Name##_ensure_capacity(struct Name *s, int new_capacity) { \
        int better_capacity = soundio_int_max(s->capacity, 16); \
        while (better_capacity < new_capacity) \
            better_capacity = better_capacity * 2; \
        if (better_capacity != s->capacity) { \
            Type *new_items = REALLOCATE_NONZERO(Type, s->items, better_capacity); \
            if (!new_items) \
                return SoundIoErrorNoMem; \
            s->items = new_items; \
            s->capacity = better_capacity; \
        } \
        return 0; \
    } \
\
    SOUNDIO_ATTR_UNUSED \
    SOUNDIO_ATTR_WARN_UNUSED_RESULT \
    static_kw int Name##_append(struct Name *s, Type item) { \
        int err = Name##_ensure_capacity(s, s->length + 1); \
        if (err) \
            return err; \
        s->items[s->length] = item; \
        s->length += 1; \
        return 0; \
    } \
\
    SOUNDIO_ATTR_UNUSED \
    static_kw Type Name##_val_at(struct Name *s, int index) {                                            \
        assert(index >= 0);                                                              \
        assert(index < s->length);                                                          \
        return s->items[index];                                                             \
    } \
\
    /* remember that the pointer to this item is invalid after you \
     * modify the length of the list \
     */ \
    SOUNDIO_ATTR_UNUSED \
    static_kw Type * Name##_ptr_at(struct Name *s, int index) { \
        assert(index >= 0); \
        assert(index < s->length); \
        return &s->items[index]; \
    } \
\
    SOUNDIO_ATTR_UNUSED \
    static_kw Type Name##_pop(struct Name *s) { \
        assert(s->length >= 1); \
        s->length -= 1; \
        return s->items[s->length]; \
    }                                                                                    \
\
    SOUNDIO_ATTR_UNUSED \
    SOUNDIO_ATTR_WARN_UNUSED_RESULT \
    static_kw int Name##_resize(struct Name *s, int new_length) {    \
        assert(new_length >= 0);                                                         \
        int err = Name##_ensure_capacity(s, new_length);                                           \
        if (err)                                                                         \
            return err;                                                                  \
        s->length = new_length;                                                             \
        return 0;                                                                        \
    }                                                                                    \
\
    SOUNDIO_ATTR_UNUSED \
    SOUNDIO_ATTR_WARN_UNUSED_RESULT \
    static_kw int Name##_add_one(struct Name *s) { \
        return Name##_resize(s, s->length + 1); \
    } \
\
    SOUNDIO_ATTR_UNUSED \
    static_kw Type Name##_last_val(struct Name *s) {                                                   \
        assert(s->length >= 1);                                                             \
        return s->items[s->length - 1];                                                        \
    }                                                                                    \
\
    SOUNDIO_ATTR_UNUSED \
    static_kw Type *Name##_last_ptr(struct Name *s) {                                                  \
        assert(s->length >= 1);                                                             \
        return &s->items[s->length - 1];                                                       \
    }                                                                                    \
\
    SOUNDIO_ATTR_UNUSED \
    static_kw void Name##_clear(struct Name *s) {                                                      \
        s->length = 0;                                                                      \
    }                                                                                    \
\
    SOUNDIO_ATTR_UNUSED \
    static_kw Type Name##_swap_remove(struct Name *s, int index) { \
        assert(index >= 0); \
        assert(index < s->length); \
        Type last = Name##_pop(s); \
        if (index == s->length) \
            return last; \
        Type item = s->items[index]; \
        s->items[index] = last; \
        return item; \
    }

#endif
