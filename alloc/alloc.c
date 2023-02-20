// alloc.c
//
// Created on: Sep 28, 2016
//     Author: Jeff Manzione

#include "alloc/alloc.h"

#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "debug/debug.h"
#include "struct/map.h"
#include "struct/set.h"

// Relevant information related to an allocation/reallocation event.
typedef struct {
  uint32_t elt_size;
  uint32_t count;
  uint32_t line;
  char *type_name;
  char *func;
  char *file;
} _AllocInfo;

// Whether memory allocation events should be outputted to stdout.
static bool _is_verbose = false;
// Stores pointers to allocated memory.
static Set *_in_mem = NULL;
// Avoid self-initialized memory allocation within Set.
static volatile bool _alloc_busy = false;
// True if inited.
static volatile bool _is_inited = false;

// Wraps calloc to match the Alloc typedef.
void *_calloc(size_t count, size_t size, const char type[]) {
  return calloc(count, size);
}

// Wraps free to match the Dealloc typedef.
void _free(void **ptr) {
  if (NULL == ptr) {
    return;
  }
  free(*ptr);
}

void alloc_init() {
  _alloc_busy = true;
  ASSERT_NULL(_in_mem);
  _in_mem =
      set_create(32781, default_hasher, default_comparator, _calloc, _free);
  _alloc_busy = false;
  _is_inited = true;
}

bool alloc_ready() { return _is_inited; }

_AllocInfo _alloc_info(uint32_t elt_size, uint32_t count, uint32_t line,
                       const char type_name[], const char func[],
                       const char file[]) {
  _AllocInfo info = {.elt_size = elt_size, .count = count, .line = line};
  info.type_name = malloc(sizeof(char) * strlen(type_name) + 1);
  strcpy(info.type_name, type_name);
  info.func = malloc(sizeof(char) * strlen(func) + 1);
  strcpy(info.func, func);
  info.file = malloc(sizeof(char) * strlen(file) + 1);
  strcpy(info.file, file);
  return info;
}

void _alloc_info_delete(_AllocInfo *info, uint32_t line, const char func[],
                        const char file[]) {
  if (NULL == info || NULL == info->type_name || NULL == info->file) {
    __error(line, func, file, "Memory management error.");
  }
  ASSERT(NOT_NULL(info), NOT_NULL(info->type_name), NOT_NULL(info->file),
         NOT_NULL(info->func));
  free(info->type_name);
  free(info->file);
  f free(info->func);
}

int _alloc_info_size() {
  return ((int)ceil(((float)sizeof(_AllocInfo)) / 8)) * 8;
}

void alloc_finalize() {
  volatile bool alloc_val = _alloc_busy;
  _alloc_busy = true;
  ASSERT_NOT_NULL(_in_mem);
  M_iter iter = set_iter(_in_mem);
  for (; has(&iter); inc(&iter)) {
    void *ptr = value(&iter);
    ASSERT_NOT_NULL(ptr);
    _AllocInfo *info = (_AllocInfo *)((char *)ptr - _alloc_info_size());
    fprintf(stderr, "Forgot to free %p(%sx%d) allocated at %s:%d in %s(...)\n",
            ptr, info->type_name, info->count, info->file, info->line,
            info->func);
    fflush(stderr);
    DEALLOC(ptr);
    _is_inited = false;
  }
  set_delete(_in_mem);
  _alloc_busy = alloc_val;
}

void alloc_to_csv(FILE *file) {
  volatile bool alloc_val = _alloc_busy;
  _alloc_busy = true;
  ASSERT_NOT_NULL(_in_mem);
  M_iter iter = set_iter(_in_mem);
  fprintf(file, "type_name,type_size,count,file,line,func,ptr\n");
  for (; has(&iter); inc(&iter)) {
    void *ptr = value(&iter);
    ASSERT_NOT_NULL(ptr);
    _AllocInfo *info = (_AllocInfo *)((char *)ptr - _alloc_info_size());
    fprintf(file, "%s,%d,%d,%s,%d,%s,%p\n", info->type_name, info->elt_size,
            info->count, info->file, info->line, info->func, ptr);
  }
  fflush(file);
  _alloc_busy = alloc_val;
}

void _alloc_register(void *ptr, uint32_t elt_size, uint32_t count,
                     uint32_t line, const char func[], const char file[],
                     const char type_name[]) {
  if (!_alloc_busy) {
    _alloc_busy = true;
    if (!set_insert(_in_mem, ptr)) {
      // __error(
      //     line, func, file,
      //     "Attempting to allocate %p(%sx%d), but it is already allocated.\n",
      //     ptr, type_name, count);
    }
    _alloc_busy = false;
  }
}

void _alloc_unregister(void *ptr, uint32_t line, const char func[],
                       const char file[]) {
  if (!_alloc_busy) {
    _alloc_busy = true;
    if (!set_remove(_in_mem, ptr)) {
      __error(line, func, file,
              "Attempting to free %p, but it is not allocated.\n", ptr);
    }
    _alloc_busy = false;
  }
}

void alloc_set_verbose(bool verbose) { _is_verbose = verbose; }

void _log_alloc(uint32_t line, const char func[], const char file[],
                char format[], ...) {
  if (_is_verbose) {
    va_list args;
    va_start(args, format);
    fprintf(stdout, "At %s:%d in %s(...):\t", file, line, func);
    vfprintf(stdout, format, args);
    fprintf(stdout, "\n");
    fflush(stdout);
    va_end(args);
  }
}

// Allocates a new block of memory and registers it.
void *__alloc(uint32_t elt_size, uint32_t count, uint32_t line,
              const char func[], const char file[], const char type_name[]) {
  if (0 == elt_size || 0 == count) {
    __error(line, func, file,
            "Either allocated array is of 0 elements or it is"
            " an array of type sizeof(0).");
  }
  int info_space = _alloc_info_size();
  void *info_ptr = calloc(1, info_space + count * elt_size);
  ASSERT(NOT_NULL(info_ptr));
  *((_AllocInfo *)info_ptr) =
      _alloc_info(elt_size, count, line, type_name, func, file);
  if (NULL == info_ptr) {
    __error(line, func, file, "Failed to allocate memory.");
  }
  void *ptr = (char *)info_ptr + info_space;
  _alloc_register(ptr, elt_size, count, line, func, file, type_name);
  _log_alloc(line, func, file, "Allocated a %s[%d] at %p", type_name, count,
             ptr);
  return ptr;
}

// Moves memory to a new location and re-registers it.
void *__realloc(void *ptr, uint32_t elt_size, uint32_t count, uint32_t line,
                const char func[], const char file[]) {
  if (NULL == ptr) {
    __error(line, func, file, "Pointer argument was null.");
  }
  int new_size = elt_size * count;
  if (0 == new_size) {
    __error(line, func, file, "Tried to realloc to an empty array.");
  }
  int info_space = _alloc_info_size();
  void *info_ptr = (char *)ptr - info_space;
  _AllocInfo old_info = *((_AllocInfo *)((char *)ptr - info_space));
  int old_size = old_info.elt_size * old_info.count;
  void *new_info_ptr = realloc(info_ptr, info_space + new_size);
  if (NULL == new_info_ptr) {
    __error(line, func, file, "Failed to reallocate memory.");
  }
  *((_AllocInfo *)new_info_ptr) =
      _alloc_info(elt_size, count, line, old_info.type_name, func, file);
  void *new_ptr = (char *)new_info_ptr + info_space;
  if (new_size > old_size) {
    size_t diff = new_size - old_size;
    void *start = ((char *)new_ptr) + old_size;
    memset(start, 0, diff);
  }
  _alloc_info_delete(&old_info, line, func, file);
  _alloc_unregister(ptr, line, func, file);
  _alloc_register(new_ptr, elt_size, count, line, func, file,
                  ((_AllocInfo *)new_info_ptr)->type_name);
  _log_alloc(line, func, file, "Reallocated memory from %p to %p.", ptr,
             new_ptr);
  return new_ptr;
}

// Deletes any memory associated with the given pointer and unregisters it.
void __dealloc(void **ptr, uint32_t line, const char func[],
               const char file[]) {
  if (NULL == ptr || NULL == *ptr) {
    __error(line, func, file, "Pointer argument was null.");
  }
  int info_space = _alloc_info_size();
  void *info_ptr = *((char **)ptr) - info_space;
  _alloc_info_delete((_AllocInfo *)info_ptr, line, func, file);
  free(info_ptr);
  _alloc_unregister(*ptr, line, func, file);
  _log_alloc(line, func, file, "Deallocated memory from %p", *ptr);
  *ptr = NULL;
}

// Copies a string.
char *__strndup(char *str, size_t len, uint32_t line, const char func[],
                const char file[]) {
  if (NULL == str) {
    __error(line, func, file, "Pointer argument was null.");
  }
  char *cpy = ALLOC_ARRAY2(char, len + 1);
  cpy[len] = '\0';
  return strncpy(cpy, str, len);
}

#ifndef STRNDUP_AVAILABLE
char *strndup(const char *str, size_t chars) {
  char *buffer;
  int n;
  buffer = (char *)malloc(sizeof(char) * (chars + 1));
  if (buffer) {
    for (n = 0; ((n < chars) && (str[n] != 0)); n++) {
      buffer[n] = str[n];
    }
    buffer[n] = '\0';
  }
  return buffer;
}
#endif