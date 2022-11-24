// debug.h
//
// Created on: Sep 28, 2016
//     Author: Jeff Manzione
//
// This header provides functions for debugging and throwing fatal errors.
//
// These macros (with the exception of FATALF()), only have an effect when DEBUG
// is defined. If DEBUG is not defined, then usages of these headers generally
// discarded in preprocessing.

#ifndef DEBUG_DEBUG_H_
#define DEBUG_DEBUG_H_

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// Global values for debug functions.
extern bool __NEST_DEBUG;
extern int __LINE_NUM;
extern const char *__FUNC_NAME;
extern const char *__FILE_NAME;

// DEBUGF(fmt, ...)
//
// Outputs a message with the given [fmt] just like pritnf but with some
// additional debug infor such as line number, function, and file and
// immediately flushes the output.
//
// Usage:
//   DEBUGF("Oh no, value=%d.", value);
#ifdef DEBUG

#define DEBUGF(fmt, ...)                                                       \
  __debugf(__LINE__, __func__, __FILE__, fmt, ##__VA_ARGS__)

#else

#define DEBUGF(fmt, ...)                                                       \
  do {                                                                         \
  } while (0)

#endif

// FATALF(fmt, ...)
//
// Fatally terminates the program after writing the specified formatted message
// to stderr.
//
// Usage:
//   FATALF("Oh no fatal error, value=%d.", value);

#define FATALF(fmt, ...)                                                       \
  do {                                                                         \
    if (__NEST_DEBUG) {                                                        \
      __error_nest(__LINE__, __func__, __FILE__, __LINE_NUM, __FUNC_NAME,      \
                   __FILE_NAME, fmt, ##__VA_ARGS__);                           \
    } else {                                                                   \
      __error(__LINE__, __func__, __FILE__, fmt, ##__VA_ARGS__);               \
    }                                                                          \
  } while (0)

// DEB_FN(), CALL_FN()
//
// Allows you to source 1 additional level of error on a function call.
//
// Usage:
//   DEB_FN(void, my_fn, int a, int b) { ... }
//   #define my_fn(a, b) CALL_FN(__my_fn, a, b)
//
//   my_fn(5, 5);
//
//   If there is an error in my_fn, then the error propagated will include the
//   line in my_fn and the line where my_fn was called.
#ifdef DEBUG

#define DEB_FN(ret, fn, ...)                                                   \
  ret fn##__(int __LINE_NUM, const char __FUNC_NAME[],                         \
             const char __FILE_NAME[], bool __NEST_DEBUG, __VA_ARGS__)
#define CALL_FN(fn, ...) fn(__LINE__, __func__, __FILE__, true, __VA_ARGS__)

#else

#define DEB_FN(ret, fn, ...) ret fn##__(__VA_ARGS__)
#define CALL_FN(fn, ...) fn(__VA_ARGS__)

#endif

// ASSERT(...)
//
// Asserts that the input condition is true, fatally terminating the program if
// the assertion fails.
//
// Assertions are processed in order from left to right.
//
// Usage:
//   ASSERT(NOT_NULL(a), *a > 5, b != 66);
#define __GET_MACRO(_1, _2, _3, _4, NAME, ...) NAME
#define ASSERT2(exp1, exp2)                                                    \
  ASSERT1(exp1);                                                               \
  ASSERT1(exp2)
#define ASSERT3(exp1, exp2, exp3)                                              \
  ASSERT1(exp1);                                                               \
  ASSERT2(exp2, exp3)
#define ASSERT4(exp1, exp2, exp3, exp4)                                        \
  ASSERT2(exp1, exp2);                                                         \
  ASSERT2(exp3, exp4)
#define S_ASSERT(...)                                                          \
  __GET_MACRO(__VA_ARGS__, ASSERT4, ASSERT3, ASSERT2, ASSERT1)(__VA_ARGS__)
#define IS_NULL(exp) (NULL == (exp))
#define NOT_NULL(exp) (NULL != (exp))
#define ASSERT_NOT_NULL(exp) ASSERT(NOT_NULL(exp))
#define ASSERT_NULL(exp) ASSERT(IS_NULL(exp))

#ifdef DEBUG
#define ASSERT(...) S_ASSERT(__VA_ARGS__)
#define ASSERT1(exp)                                                           \
  do {                                                                         \
    if (!(exp)) {                                                              \
      FATALF("Assertion failed. Expression( %s ) evaluated to false.", #exp);  \
    }                                                                          \
  } while (0)
#else
#define ASSERT(...)
#define ASSERT1(exp)                                                           \
  do {                                                                         \
    (void)(exp);                                                               \
  } while (0)
#endif

// Do not touch these.
extern bool __NEST_DEBUG;
extern int __LINE_NUM;
extern const char *__FUNC_NAME;
extern const char *__FILE_NAME;

// There should be no reason to call these functions directly, so please prefer
// their macro counterparts.

#ifdef DEBUG
void __debugf(int line_num, const char func_name[], const char file_name[],
              const char fmt[], ...);
#endif
void __errorf(int line_num, const char func_name[], const char file_name[],
              const char fmt[], ...);
void __errorf_nest(int line_num, const char func_name[], const char file_name[],
                   int nested_line_num, const char nested_func_name[],
                   const char nested_file_name[], const char fmr[], ...);

#endif /* DEBUG_DEBUG_H_ */
