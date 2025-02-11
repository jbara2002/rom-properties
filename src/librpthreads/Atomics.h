/***************************************************************************
 * ROM Properties Page shell extension. (librpthreads)                     *
 * Atomics.h: Atomic function macros.                                      *
 *                                                                         *
 * Copyright (c) 2016-2022 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-2.0-or-later                               *
 ***************************************************************************/

#ifndef __ROMPROPERTIES_LIBRPTHREADS_ATOMICS_H__
#define __ROMPROPERTIES_LIBRPTHREADS_ATOMICS_H__

// REINTERPRET_CAST() macro that matches shlwapi.h's STATIC_CAST().
#ifndef REINTERPRET_CAST
# ifdef __cplusplus
#  define REINTERPRET_CAST(typ) reinterpret_cast<typ>
# else
#  define REINTERPRET_CAST(typ) (typ)
# endif
#endif /* REINTERPRET_CAST */

// NOTE: We could eliminate an instruction by using the fetch_op variants
// in gcc/clang instead of op_fetch, but MSVC doesn't have those, so we'd
// have to use compiler-specific #ifdefs in the calling code...

// Atomic function macros.
// TODO: C++11 atomic support; test all of this.
#if defined(__clang__)
# if 0 && (__has_feature(c_atomic) || __has_extension(c_atomic))
   /* Clang: Use prefixed C11-style atomics. */
   /* FIXME: Not working... (clang-3.9.0 complains that it's not declared.) */
#  define ATOMIC_INC_FETCH(ptr)			__c11_atomic_add_fetch(ptr, 1, __ATOMIC_SEQ_CST)
#  define ATOMIC_DEC_FETCH(ptr)			__c11_atomic_dec_fetch(ptr, 1, __ATOMIC_SEQ_CST)
#  define ATOMIC_OR_FETCH(ptr, val)		__c11_atomic_or_fetch(ptr, val, __ATOMIC_SEQ_CST)
   /* NOTE: C11 version of cmpxchg requires pointers, so we'll use the Itanium-style version. */
#  define ATOMIC_CMPXCHG(ptr, cmp, xchg)	__sync_val_compare_and_swap(ptr, cmp, xchg)
#  define ATOMIC_EXCHANGE(ptr, val)		__c11_atomic_exchange(ptr, val)
# else
   /* Use Itanium-style atomics. */
#  define ATOMIC_INC_FETCH(ptr)			__sync_add_and_fetch(ptr, 1)
#  define ATOMIC_DEC_FETCH(ptr)			__sync_sub_and_fetch(ptr, 1)
#  define ATOMIC_OR_FETCH(ptr, val)		__sync_or_and_fetch(ptr, val)
#  define ATOMIC_CMPXCHG(ptr, cmp, xchg)	__sync_val_compare_and_swap(ptr, cmp, xchg)
#  define ATOMIC_EXCHANGE(ptr, val)		__sync_lock_test_and_set(ptr, val)
# endif
#elif defined(__GNUC__)
# if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7))
   /* gcc-4.7: Use prefixed C11-style atomics. */
#  define ATOMIC_INC_FETCH(ptr)			__atomic_add_fetch(ptr, 1, __ATOMIC_SEQ_CST)
#  define ATOMIC_DEC_FETCH(ptr)			__atomic_sub_fetch(ptr, 1, __ATOMIC_SEQ_CST)
#  define ATOMIC_OR_FETCH(ptr, val)		__atomic_or_fetch(ptr, val, __ATOMIC_SEQ_CST)
   /* NOTE: C11 version of cmpxchg requires pointers, so we'll use the Itanium-style version. */
#  define ATOMIC_CMPXCHG(ptr, cmp, xchg)	__sync_val_compare_and_swap(ptr, cmp, xchg)
#  define ATOMIC_EXCHANGE(ptr, val)		__sync_lock_test_and_set(ptr, val)
# else
   /* gcc-4.6 and earlier: Use Itanium-style atomics. */
#  define ATOMIC_INC_FETCH(ptr)			__sync_add_and_fetch(ptr, 1)
#  define ATOMIC_DEC_FETCH(ptr)			__sync_sub_and_fetch(ptr, 1)
#  define ATOMIC_OR_FETCH(ptr, val)		__sync_or_and_fetch(ptr, val)
#  define ATOMIC_CMPXCHG(ptr, cmp, xchg)	__sync_val_compare_and_swap(ptr, cmp, xchg)
#  define ATOMIC_EXCHANGE(ptr, val)		__sync_lock_test_and_set(ptr, val)
# endif
#elif defined(_MSC_VER)
# include <intrin.h>
static __inline int ATOMIC_INC_FETCH(volatile int *ptr)
{
	return _InterlockedIncrement(REINTERPRET_CAST(volatile long*)(ptr));
}
static __inline int ATOMIC_DEC_FETCH(volatile int *ptr)
{
	return _InterlockedDecrement(REINTERPRET_CAST(volatile long*)(ptr));
}
static __inline int ATOMIC_OR_FETCH(volatile int *ptr, int val)
{
	return _InterlockedOr(REINTERPRET_CAST(volatile long*)(ptr), val);
}
static __inline int ATOMIC_CMPXCHG(volatile int *ptr, int cmp, int xchg)
{
	return _InterlockedCompareExchange(REINTERPRET_CAST(volatile long*)(ptr), xchg, cmp);
}
static __inline int ATOMIC_EXCHANGE(volatile int *ptr, int val)
{
	return _InterlockedExchange(REINTERPRET_CAST(volatile long*)(ptr), val);
}
#else
# error Atomic functions not defined for this compiler.
#endif

#endif /* __ROMPROPERTIES_LIBRPTHREADS_ATOMICS_H__ */
