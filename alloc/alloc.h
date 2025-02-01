// alloc.h
//
// Created on: Sep 28, 2016
//     Author: Jeff Manzione

#ifndef ALLOC_ALLOC_H_
#define ALLOC_ALLOC_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Initializes allocation system.
void alloc_init();
// If alloc was inited.
// If false, it can be initted by alloc_init().
bool alloc_ready();
// Finalizes allocation system and frees any unfreed memory.
void alloc_finalize();
// Allocation will output to stdout when memory is requested.
void alloc_set_verbose(bool);
// Prints out all items in memory in CSV format.
void alloc_to_csv(FILE *);

// Allocates a solid memory block of size: [sizeof(type)*count].
//
// Details:
//   - This function always clears memory.
//   - Can only bee used after alloc_init() has been called.
//
// Usage:
//   MyStruct *arr = ALLOC_ARRAY(MyStruct, 20);
#ifdef DEBUG_MEMORY
#define ALLOC_ARRAY(type, count)                                               \
  (type *)__alloc(/*type=*/sizeof(type), /*count=*/(count), (__LINE__),        \
                  (__func__), (__FILE__), (#type))
#else
#define ALLOC_ARRAY(type, count) (type *)calloc((count), sizeof(type))
#endif

// Same as ALLOC_ARRAY.
#define CNEW_ARR(type, count) ALLOC_ARRAY(type, count)

// Allocates a solid memory block of size: [sizeof(type)*count].
//
//
// Details:
//   - This function does not guarantee that the allocated memory will
//     be cleared. If that guarnatee is required, use ALLOC_ARRAY().
//   - Can only bee used after alloc_init() has been called.
//
// Usage:
//   MyStruct *arr = ALLOC_ARRAY2(MyStruct, 20);
#ifdef DEBUG_MEMORY
#define ALLOC_ARRAY2(type, count)                                              \
  (type *)__alloc(/*type=*/sizeof(type), /*count=*/(count), (__LINE__),        \
                  (__func__), (__FILE__), (#type))
#else
#define ALLOC_ARRAY2(type, count) (type *)malloc((count) * sizeof(type))
#endif

// Same as ALLOC_ARRAY2.
#define MNEW_ARR(type, count) ALLOC_ARRAY2(type, count)

// Allocates a solid memory block of size: [type_sz*count].
//
// Details:
//   - This function does not guarantee that the allocated memory will
//     be cleared. If that guarnatee is required, use ALLOC_ARRAY().
//   - Can only bee used after alloc_init() has been called.
//
// Usage:
//   MyStruct *arr =
//       (MyStruct *) ALLOC_ARRAY_SZ("MyStruct", sizeof(MyStruct), 20);
#ifdef DEBUG_MEMORY
#define ALLOC_ARRAY_SZ(type_name, type_sz, count)                              \
  __alloc(/*type=*/(type_sz), /*count=*/(count), (__LINE__), (__func__),       \
          (__FILE__), (type_name))
#else
#define ALLOC_ARRAY_SZ(type_name, type_sz, count) malloc((count) * (type_sz))
#endif

// Same as ALLOC_ARRAY_SZ.
#define MNEW_ARR_SZ(type_name, type_sz, count)                                 \
  ALLOC_ARRAY_SZ(type_name, type_sz, count)

// Allocates a new solid memory block of size: [type_sz*count] and
// copies the existing data at [ptr] to the newly allocated data, freeing
// the previously allocated block.
//
// Details:
//   - Can only bee used after alloc_init() has been called.
//
// Usage:
//   MyStruct *arr = ALLOC_ARRAY2(MyStruct, 20);
//   arr = REALLOC_SZ(arr, sizeof(MyStruct), 50);
#ifdef DEBUG_MEMORY
#define REALLOC_SZ(ptr, type_sz, count)                                        \
  (void *)__realloc(/*ptr=*/(ptr), /*type=*/(type_sz), /*count=*/(count),      \
                    (__LINE__), (__func__), (__FILE__))
#else
#define REALLOC_SZ(ptr, type_sz, count)                                        \
  (void *)realloc((ptr), (type_sz) * (count))
#endif

// Allocates a new solid memory block of size: [sizeof(type)*count] and
// copies the existing data at [ptr] to the newly allocated data, freeing
// the previously allocated block.
//
// Details:
//   - Can only bee used after alloc_init() has been called.
//
// Usage:
//   MyStruct *arr = ALLOC_ARRAY2(MyStruct, 20);
//   arr = REALLOC(arr, MyStruct, 50);
#ifdef DEBUG_MEMORY
#define REALLOC(ptr, type, count)                                              \
  (type *)REALLOC_SZ((ptr), sizeof(type), (count))
#else
#define REALLOC(ptr, type, count) (type *)realloc((ptr), sizeof(type) * (count))
#endif

// Frees a new solid memory block located at [ptr].
//
// Details:
//   - Can only bee used after alloc_init() has been called.
//   - [ptr] must be allocated by calls to ALLOC_* or REALLOC*.
//
// Usage:
//   MyStruct *arr = ALLOC_ARRAY2(MyStruct, 20);
//   DEALLOC(arr);
#ifdef DEBUG_MEMORY
#define DEALLOC(ptr)                                                           \
  __dealloc((void **)&(ptr), (__LINE__), (__func__), (__FILE__))
#else
#define DEALLOC(ptr) free((void *)(ptr))
#endif

// Same as DEALLOC.
#define RELEASE(ptr) DEALLOC(ptr)

// Allocates a solid memory block of size: [sizeof(type)].
//
// Details:
//   - This function always clears memory.
//   - Can only bee used after alloc_init() has been called.
//
// Usage:
//   MyStruct *arr = ALLOC(MyStruct);
#define ALLOC(type) ALLOC_ARRAY(type, 1)

// Same as ALLOC.
#define CNEW(type) ALLOC(type)

// Allocates a solid memory block of size: [sizeof(type)].
//
//
// Details:
//   - This function does not guarantee that the allocated memory will
//     be cleared. If that guarnatee is required, use ALLOC().
//   - Can only bee used after alloc_init() has been called.
//
// Usage:
//   MyStruct *arr = ALLOC2(MyStruct);
#define ALLOC2(type) ALLOC_ARRAY2(type, 1)

// Same as ALLOC2.
#define MNEW(type) ALLOC2(type)

// Copies [str].
//
// Details:
//   - Can only bee used after alloc_init() has been called.
//   - Returns a null-terminated string copy of [str].
//   - The allocated char[] will be strlen(str) + 1.
//
// Usage:
//   char *cpy = ALLOC_STRDUP(src_str);
//   DEALLOC(cpy);
#ifdef DEBUG_MEMORY
#define ALLOC_STRDUP(str)                                                      \
  __strndup((char *)str, strlen(str), (__LINE__), (__func__), (__FILE__))
#else
#define ALLOC_STRDUP(str) strndup((char *)(str), strlen(str))
#endif

// Source: https://linux.die.net/man/3/strndup
#if (POSIX_C_SOURCE >= 200809L || _XOPEN_SOURCE >= 700)
#define STRNDUP_AVAILABLE
#endif

// Copies up to [len] characters of [str].
//
// Details:
//   - Can only bee used after alloc_init() has been called.
//   - Returns a null-terminated string copy of the first [len] characters of
//     [str].
//   - The allocated char[] will be len + 1.
//
// Usage:
//   char *cpy = ALLOC_STRNDUP(src_str, 6);
//   DEALLOC(cpy);
#ifdef DEBUG_MEMORY
#define ALLOC_STRNDUP(str, len)                                                \
  __strndup((char *)str, len, (__LINE__), (__func__), (__FILE__))
#else
#define ALLOC_STRNDUP(str, len) strndup((char *)(str), len)
#endif

// Functions that are wrapped by the macros and should not be called directly.
#ifdef DEBUG_MEMORY
void *__alloc(uint32_t elt_size, uint32_t count, uint32_t line,
              const char func[], const char file[], const char type_name[]);
void *__realloc(void *, uint32_t elt_size, uint32_t count, uint32_t line,
                const char func[], const char file[]);
void __dealloc(void **, uint32_t line, const char func[], const char file[]);
char *__strndup(char *, size_t len, uint32_t line, const char func[],
                const char file[]);

#elif !defined(STRNDUP_AVAILABLE)
char *strndup(const char *s, size_t n);
#endif

#endif /* ALLOC_ALLOC_H_ */
