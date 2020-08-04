#ifndef __MSVC_STDATOMIC_H
#define __MSVC_STDATOMIC_H

#if defined(_MSC_VER) && !defined(__cplusplus)

#include <stdbool.h>
#include <stdint.h>
#include <uchar.h>

#define ATOMIC_VAR_INIT(value)      (value)
#define atomic_init(object, value)  (void)(*(object) = (value))

#ifdef __GNUC__
#define atomic_store(object, desired)   (void)( *(volatile typeof(*(object)) *)(object) = (desired) )
#define atomic_load(object)             *(volatile typeof(*(object)) *)(object)
#else
#define atomic_store(object, desired)   (void)(*(object) = (desired))
#define atomic_load(object)             *(object)
#endif

#define ATOMIC_FLAG_INIT { 0 }

typedef enum memory_order {
    memory_order_relaxed,
    memory_order_consume,
    memory_order_acquire,
    memory_order_release,
    memory_order_acq_rel,
    memory_order_seq_cst
} memory_order;

#define atomic_load_explicit(object, order)                 atomic_load(object)
#define atomic_store_explicit(object, desired, order)       atomic_store((object), (desired))


typedef struct atomic_flag { bool _Value; } atomic_flag;

typedef bool                atomic_bool;
typedef char                atomic_char;
typedef unsigned char       atomic_uchar;
typedef signed char         atomic_schar;
typedef short               atomic_short;
typedef unsigned short      atomic_ushort;
typedef int                 atomic_int;
typedef unsigned            atomic_uint;
typedef long                atomic_long;
typedef unsigned long       atomic_ulong;
typedef long long           atomic_llong;
typedef unsigned long long  atomic_ullong;
typedef intptr_t            atomic_intptr_t;
typedef uintptr_t           atomic_uintptr_t;
typedef size_t              atomic_size_t;
typedef ptrdiff_t           atomic_ptrdiff_t;
typedef intmax_t            atomic_intmax_t;
typedef uintmax_t           atomic_uintmax_t;
typedef char16_t            atomic_char16_t;
typedef char32_t            atomic_char32_t;
typedef wchar_t             atomic_wchar_t;


extern bool atomic_flag_test_and_set(volatile atomic_flag* object);
extern bool atomic_flag_test_and_set_explicit(volatile atomic_flag* object, memory_order order);

extern void atomic_flag_clear(volatile atomic_flag* object);
extern void atomic_flag_clear_explicit(volatile atomic_flag* object, memory_order order);


#define atomic_fetch_add(object, operand)   ((sizeof(*(object)) == 1) ? zenny_atomic_fetch_add8((volatile atomic_schar*)(object), (int8_t)(operand)) : \
                                            ((sizeof(*(object)) == 2) ? zenny_atomic_fetch_add16((volatile atomic_short*)(object), (int16_t)(operand)) : \
                                            ((sizeof(*(object)) == 4) ? zenny_atomic_fetch_add32((volatile atomic_long*)(object), (int32_t)(operand)) : \
                                             zenny_atomic_fetch_add64((volatile atomic_llong*)(object), (int64_t)(operand)) )))

#define atomic_fetch_add_explicit(object, operand, order)   atomic_fetch_add((object), (operand))

extern int8_t zenny_atomic_fetch_add8(volatile atomic_schar* object, int8_t operand);
extern int16_t zenny_atomic_fetch_add16(volatile atomic_short* object, int16_t operand);
extern int32_t zenny_atomic_fetch_add32(volatile atomic_long* object, int32_t operand);
extern int64_t zenny_atomic_fetch_add64(volatile atomic_llong* object, int64_t operand);


#define atomic_fetch_sub(object, operand)   ((sizeof(*(object)) == 1) ? zenny_atomic_fetch_sub8((volatile atomic_schar*)(object), (int8_t)(operand)) : \
                                            ((sizeof(*(object)) == 2) ? zenny_atomic_fetch_sub16((volatile atomic_short*)(object), (int16_t)(operand)) : \
                                            ((sizeof(*(object)) == 4) ? zenny_atomic_fetch_sub32((volatile atomic_long*)(object), (int32_t)(operand)) : \
                                             zenny_atomic_fetch_sub64((volatile atomic_llong*)(object), (int64_t)(operand)) )))

#define atomic_fetch_sub_explicit(object, operand, order)   atomic_fetch_sub((object), (operand))

extern int8_t zenny_atomic_fetch_sub8(volatile atomic_schar* object, int8_t operand);
extern int16_t zenny_atomic_fetch_sub16(volatile atomic_short* object, int16_t operand);
extern int32_t zenny_atomic_fetch_sub32(volatile atomic_long* object, int32_t operand);
extern int64_t zenny_atomic_fetch_sub64(volatile atomic_llong* object, int64_t operand);


#define atomic_fetch_or(object, operand)    ((sizeof(*(object)) == 1) ? zenny_atomic_fetch_or8((volatile atomic_schar*)(object), (int8_t)(operand)) : \
                                            ((sizeof(*(object)) == 2) ? zenny_atomic_fetch_or16((volatile atomic_short*)(object), (int16_t)(operand)) : \
                                            ((sizeof(*(object)) == 4) ? zenny_atomic_fetch_or32((volatile atomic_long*)(object), (int32_t)(operand)) : \
                                             zenny_atomic_fetch_or64((volatile atomic_llong*)(object), (int64_t)(operand))   )))

#define atomic_fetch_or_explicit(object, operand, order)   atomic_fetch_or((object), (operand))

extern int8_t zenny_atomic_fetch_or8(volatile atomic_schar* object, int8_t operand);
extern int16_t zenny_atomic_fetch_or16(volatile atomic_short* object, int16_t operand);
extern int32_t zenny_atomic_fetch_or32(volatile atomic_long* object, int32_t operand);
extern int64_t zenny_atomic_fetch_or64(volatile atomic_llong* object, int64_t operand);


#define atomic_fetch_xor(object, operand)   ((sizeof(*(object)) == 1) ? zenny_atomic_fetch_xor8((volatile atomic_schar*)(object), (int8_t)(operand)) : \
                                            ((sizeof(*(object)) == 2) ? zenny_atomic_fetch_xor16((volatile atomic_short*)(object), (int16_t)(operand)) : \
                                            ((sizeof(*(object)) == 4) ? zenny_atomic_fetch_xor32((volatile atomic_long*)(object), (int32_t)(operand)) : \
                                              zenny_atomic_fetch_xor64((volatile atomic_llong*)(object), (int64_t)(operand)) )))

#define atomic_fetch_xor_explicit(object, operand, order)   atomic_fetch_xor((object), (operand))

extern int8_t zenny_atomic_fetch_xor8(volatile atomic_schar* object, int8_t operand);
extern int16_t zenny_atomic_fetch_xor16(volatile atomic_short* object, int16_t operand);
extern int32_t zenny_atomic_fetch_xor32(volatile atomic_long* object, int32_t operand);
extern int64_t zenny_atomic_fetch_xor64(volatile atomic_llong* object, int64_t operand);


#define atomic_fetch_and(object, operand)   ((sizeof(*(object)) == 1) ? zenny_atomic_fetch_and8((volatile atomic_schar*)(object), (int8_t)(operand)) : \
                                            ((sizeof(*(object)) == 2) ? zenny_atomic_fetch_and16((volatile atomic_short*)(object), (int16_t)(operand)) : \
                                            ((sizeof(*(object)) == 4) ? zenny_atomic_fetch_and32((volatile atomic_long*)(object), (int32_t)(operand)) : \
                                             zenny_atomic_fetch_and64((volatile atomic_llong*)(object), (int64_t)(operand)) )))

#define atomic_fetch_and_explicit(object, operand, order)   atomic_fetch_and((object), (operand))

extern int8_t zenny_atomic_fetch_and8(volatile atomic_schar* object, int8_t operand);
extern int16_t zenny_atomic_fetch_and16(volatile atomic_short* object, int16_t operand);
extern int32_t zenny_atomic_fetch_and32(volatile atomic_long* object, int32_t operand);
extern int64_t zenny_atomic_fetch_and64(volatile atomic_llong* object, int64_t operand);


#define atomic_exchange(object, desired)    ((sizeof(*(object)) == 1) ? zenny_atomic_exchange8((volatile atomic_schar*)(object), (int8_t)(desired)) : \
                                            ((sizeof(*(object)) == 2) ? zenny_atomic_exchange16((volatile atomic_short*)(object), (int16_t)(desired)) : \
                                            ((sizeof(*(object)) == 4) ? zenny_atomic_exchange32((volatile atomic_long*)(object), (int32_t)(desired)) : \
                                             zenny_atomic_exchange64((volatile atomic_llong*)(object), (int64_t)(desired)) )))

#define atomic_exchange_explicit(object, desired, order)    atomic_exchange((object), (desired))

extern int8_t zenny_atomic_exchange8(volatile atomic_schar* object, int8_t desired);
extern int16_t zenny_atomic_exchange16(volatile atomic_short* object, int16_t desired);
extern int32_t zenny_atomic_exchange32(volatile atomic_long* object, int32_t desired);
extern int64_t zenny_atomic_exchange64(volatile atomic_llong* object, int64_t desired);


#define atomic_compare_exchange_strong(object, expected, desired)   (sizeof(*(object)) == 1 ? zenny_atomic_compare_exchange8((volatile atomic_schar*)(object), (int8_t*)(expected), (int8_t)(desired)) : \
                                                                    (sizeof(*(object)) == 2 ? zenny_atomic_compare_exchange16((volatile atomic_short*)(object), (int16_t*)(expected), (int16_t)(desired)) : \
                                                                    (sizeof(*(object)) == 4 ? zenny_atomic_compare_exchange32((volatile atomic_long *)(object), (int32_t*)(expected), (int32_t)(desired)) : \
                                                                     zenny_atomic_compare_exchange64((volatile atomic_llong*)(object), (int64_t*)(expected), (int64_t)(desired)) )))

#define atomic_compare_exchange_weak    atomic_compare_exchange_strong

#define atomic_compare_exchange_strong_explicit(object, expected, desired, success, failure)    atomic_compare_exchange_strong((object), (expected), (desired))
#define atomic_compare_exchange_weak_explicit(object, expected, desired, success, failure)      atomic_compare_exchange_weak((object), (expected), (desired))

extern bool zenny_atomic_compare_exchange8(volatile atomic_schar* object, int8_t* expected, int8_t desired);
extern bool zenny_atomic_compare_exchange16(volatile atomic_short* object, int16_t* expected, int16_t desired);
extern bool zenny_atomic_compare_exchange32(volatile atomic_long* object, int32_t* expected, int32_t desired);
extern bool zenny_atomic_compare_exchange64(volatile atomic_llong* object, int64_t* expected, int64_t desired);

#endif // #if defined(_MSC_VER) && !defined(__cplusplus)

#endif