/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef SOUNDIO_LIST_HPP
#define SOUNDIO_LIST_HPP

#include "util.hpp"
#include "soundio.h"

#include <assert.h>

template<typename T>
struct SoundIoList {
    SoundIoList() {
        length = 0;
        capacity = 0;
        items = nullptr;
    }
    ~SoundIoList() {
        deallocate(items, capacity);
    }
    int __attribute__((warn_unused_result)) append(T item) {
        int err = ensure_capacity(length + 1);
        if (err)
            return err;
        items[length++] = item;
        return 0;
    }
    // remember that the pointer to this item is invalid after you
    // modify the length of the list
    const T & at(int index) const {
        assert(index >= 0);
        assert(index < length);
        return items[index];
    }
    T & at(int index) {
        assert(index >= 0);
        assert(index < length);
        return items[index];
    }
    T pop() {
        assert(length >= 1);
        return items[--length];
    }

    int __attribute__((warn_unused_result)) add_one() {
        return resize(length + 1);
    }

    const T & last() const {
        assert(length >= 1);
        return items[length - 1];
    }

    T & last() {
        assert(length >= 1);
        return items[length - 1];
    }

    int __attribute__((warn_unused_result)) resize(int length) {
        assert(length >= 0);
        int err = ensure_capacity(length);
        if (err)
            return err;
        length = length;
        return 0;
    }

    T swap_remove(int index) {
        assert(index >= 0);
        assert(index < length);
        if (index == length - 1)
            return pop();

        T last = pop();
        T item = items[index];
        items[index] = last;
        return item;
    }

    void remove_range(int start, int end) {
        assert(0 <= start);
        assert(start <= end);
        assert(end <= length);
        int del_count = end - start;
        for (int i = start; i < length - del_count; i += 1) {
            items[i] = items[i + del_count];
        }
        length -= del_count;
    }

    int __attribute__((warn_unused_result)) insert_space(int pos, int size) {
        int old_length = length;
        assert(pos >= 0 && pos <= old_length);
        int err = resize(old_length + size);
        if (err)
            return err;

        for (int i = old_length - 1; i >= pos; i -= 1) {
            items[i + size] = items[i];
        }

        return 0;
    }

    void fill(T value) {
        for (int i = 0; i < length; i += 1) {
            items[i] = value;
        }
    }

    void clear() {
        length = 0;
    }

    int __attribute__((warn_unused_result)) ensure_capacity(int new_capacity) {
        int better_capacity = max(capacity, 16);
        while (better_capacity < new_capacity)
            better_capacity = better_capacity * 2;
        if (better_capacity != capacity) {
            T *new_items = reallocate_nonzero(items, capacity, better_capacity);
            if (!new_items)
                return SoundIoErrorNoMem;
            items = new_items;
            capacity = better_capacity;
        }
        return 0;
    }

    int allocated_size() const {
        return capacity * sizeof(T);
    }

    T * items;
    int length;
    int capacity;
};

#endif
