// debug.c
//
// Created on: Sep 28, 2016
//     Author: Jeff Manzione

#include "debug/debug.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

bool __NEST_DEBUG = false;
int __LINE_NUM = 0;
const char *__FUNC_NAME = NULL;
const char *__FILE_NAME = NULL;

void __error(int line_num, const char func_name[], const char file_name[],
             const char fmt[], ...) {
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "Error in file(%s) in function(%s) at line(%d): ", file_name,
          func_name, line_num);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  fflush(stderr);
  va_end(args);
  exit(1);
}

void __error_nest(int line_num, const char func_name[], const char file_name[],
                  int nested_line_num, const char nested_func_name[],
                  const char nested_file_name[], const char fmt[], ...) {
  va_list args;
  va_start(args, fmt);
  fprintf(stderr,
          "From file(%s) in function(%s) at line(%d):\n+-> Error in file(%s) "
          "in function(%s) at line(%d): ",
          nested_file_name, nested_func_name, nested_line_num, file_name,
          func_name, line_num);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  fflush(stderr);
  va_end(args);
  exit(1);
}

#ifdef DEBUG
void __debugf(int line_num, const char func_name[], const char file_name[],
              const char fmt[], ...) {
  va_list args;
  va_start(args, fmt);
  fprintf(stdout, "[D] %s:%d(%s): ", file_name, line_num, func_name);
  vfprintf(stdout, fmt, args);
  fprintf(stdout, "\n");
  fflush(stdout);
  va_end(args);
}
#endif