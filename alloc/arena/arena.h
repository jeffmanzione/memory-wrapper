// arena.h
//
// Created on: Feb 7, 2018
//     Author: Jeff Manzione
//
// Performs cheap allocation of structs needed in the same context in reserved
// blocks of memory to take advantage of freeing all of the memory instantly
// when out of that context.
//
// See: https://en.wikipedia.org/wiki/Region-based_memory_management
//
// ARENA_DEFINE(MyType);
// void fn() {
//   ARENA_INIT(MyType);
//   ...
//   MyType *t = ARENA_ALLOC(MyType).
//   ...
//   ARENA_FINALIZE(MyType);  // All freed at once.
// }

#ifndef ALLOC_ARENA_ARENA_H_
#define ALLOC_ARENA_ARENA_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Declares an arena for the given type.
//
// This can go in a relevant header file. If the arena does not need to be
// shared via header, it is not necessary to declare an arena.
//
// Usage:
//   ARENA_DECLARE(MyType);
#define ARENA_DECLARE(typename) extern __Arena __ARENA__##typename

// Defines an arena for a given type.
//
// This creates all the necessary stuff to reference your arena. This should
// generally be placed ina .c file and not in a header.
//
// Usage:
//   ARENA_DEFINE(MyType);
#define ARENA_DEFINE(typename) __Arena __ARENA__##typename = {.inited = false};

// Initializes an arena so that it can allocate memory.
//
// Usage:
//   ARENA_DEFINE(MyType);
//
//   int main(int argc, char *argv[]) {
//     ARENA_INIT(MyType);
//     ...
//   }
#define ARENA_INIT(typename)                                                   \
  __arena_init(&__ARENA__##typename, sizeof(typename), #typename)

// Finalizes and does any tyding up related to an arena, freeing all memory at
// once.
//
// Usage:
//   ARENA_DEFINE(MyType);
//
//   int main(int argc, char *argv[]) {
//     ARENA_INIT(MyType);
//     ...
//     ARENA_FINALIZE(MyType);
//   }
#define ARENA_FINALIZE(typename) __arena_finalize(&__ARENA__##typename)

// Allocates a block in the arena and returns a pointer to it.
//
// Usage:
//   MyType *t = ARENA_ALLOC(MyType);
#define ARENA_ALLOC(typename) (typename *)__arena_alloc(&__ARENA__##typename)

// Deallocates a block in this arena.
//
// Generally, this is not the desired behavior of an arena, but this
// functionality is still provided.
//
// Usage:
//  MyType *t = ARENA_ALLOC(MyType);
//  ARENA_DEALLOC(MyType, t);
#define ARENA_DEALLOC(typename, ptr) __arena_dealloc(&__ARENA__##typename, ptr)

typedef struct __Subarena _Subarena;

typedef struct {
  bool inited;
  const char *name;
  _Subarena *last;
  size_t item_sz;
  size_t alloc_sz;
  void *next, *end;
  void *last_freed;
  uint32_t item_count;
} __Arena;

// Do not call these function directly.
void __arena_init(__Arena *arena, size_t sz, const char name[]);
void __arena_finalize(__Arena *arena);
void *__arena_alloc(__Arena *arena);
void __arena_dealloc(__Arena *arena, void *ptr);

uint32_t __arena_item_size(__Arena *arena);
uint32_t __arena_capacity(__Arena *arena);
uint32_t __arena_item_count(__Arena *arena);
uint32_t __arena_subarena_capacity(__Arena *arena);
uint32_t __arena_subarena_count(__Arena *arena);

#endif /* ALLOC_ARENA_ARENA_H_ */
