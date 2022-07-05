// arena.c
//
// Created on: Feb 7, 2018
//     Author: Jeff Manzione

#include "alloc/arena/arena.h"

#include <math.h>
#include <stdlib.h>

#include "alloc/alloc.h"
#include "debug/debug.h"

#define DEFAULT_ELTS_IN_CHUNK 128

// Treates a void* as a char* to make Windows CC happy.
#define _CHAR_POINTER(void_ptr) ((char *)(void_ptr))

int descriptor_sz;

typedef struct {
  void *prev_freed;
} Descriptor;

struct __Subarena {
  _Subarena *prev;
  void *block;
  size_t block_sz;
};

_Subarena *_subarena_create(_Subarena *prev, size_t sz) {
  _Subarena *sa = ALLOC2(_Subarena);
  sa->block_sz = sz * DEFAULT_ELTS_IN_CHUNK;
  sa->block = malloc(sa->block_sz);
  sa->prev = prev;
  return sa;
}

void _subarena_delete(_Subarena *sa) {
  if (NULL != sa->prev) {
    _subarena_delete(sa->prev);
  }
  free(sa->block);
  DEALLOC(sa);
}

void __arena_init(__Arena *arena, size_t sz, const char name[]) {
  ASSERT_NOT_NULL(arena);
  descriptor_sz = ((int)ceil(((float)sizeof(Descriptor)) / 4)) * 4;
  arena->name = name;
  arena->item_sz = sz;
  arena->alloc_sz = sz + descriptor_sz;
  arena->last = _subarena_create(NULL, arena->alloc_sz);
  arena->next = arena->last->block;
  arena->end = _CHAR_POINTER(arena->last->block) + arena->last->block_sz;
  arena->last_freed = NULL;
  arena->item_count = 0;
}

void __arena_finalize(__Arena *arena) {
  ASSERT_NOT_NULL(arena);
  _subarena_delete(arena->last);
}

void *__arena_alloc(__Arena *arena) {
  ASSERT_NOT_NULL(arena);
  arena->item_count++;
  // Use up space that was already freed.
  if (NULL != arena->last_freed) {
    void *free_spot = arena->last_freed;
    arena->last_freed = ((Descriptor *)free_spot)->prev_freed;
    return _CHAR_POINTER(free_spot) + descriptor_sz;
  }
  // Allocate a new subarena if the current one is full.
  if (arena->next == arena->end) {
    _Subarena *new_sa = _subarena_create(arena->last, arena->alloc_sz);
    new_sa->prev = arena->last;
    arena->last = new_sa;
    arena->next = arena->last->block;
    arena->end = _CHAR_POINTER(arena->last->block) + arena->last->block_sz;
  }
  void *spot = arena->next;
  arena->next = _CHAR_POINTER(arena->next) + arena->alloc_sz;
  return _CHAR_POINTER(spot) + descriptor_sz;
}

void __arena_dealloc(__Arena *arena, void *ptr) {
  ASSERT(NOT_NULL(arena), NOT_NULL(ptr));
  arena->item_count--;
  Descriptor *d = (Descriptor *)(_CHAR_POINTER(ptr) - descriptor_sz);
  d->prev_freed = arena->last_freed;
  arena->last_freed = (void *)d;
}

uint32_t __arena_item_size(__Arena *arena) { return arena->item_sz; }

uint32_t __arena_capacity(__Arena *arena) {
  return __arena_subarena_count(arena) * __arena_subarena_capacity(arena);
}

uint32_t __arena_item_count(__Arena *arena) { return arena->item_count; }

uint32_t __arena_subarena_capacity(__Arena *arena) {
  return DEFAULT_ELTS_IN_CHUNK;
}

uint32_t __arena_subarena_count(__Arena *arena) {
  uint32_t subarena_count = 0;
  _Subarena *sarena = arena->last;
  while (NULL != sarena) {
    subarena_count++;
    sarena = sarena->prev;
  }
  return subarena_count;
}