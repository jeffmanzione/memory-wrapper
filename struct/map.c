// map.c
//
// Created on: Feb 5, 2018
//     Author: Jeff Manzione

#include "struct/map.h"

#include <stdbool.h>
#include <stdint.h>

#include "debug/debug.h"
#include "util/util.h"

struct __Entry {
  Pair pair;
  uint32_t hash_value;
  int32_t num_probes;
  _Entry *prev, *next;
};

#define pos(hval, num_probes, table_sz)                                        \
  (((hval) + ((num_probes) * (num_probes))) % (table_sz))
#define calculate_new_size(current_sz) (((current_sz)*2) + 1)
#define calculate_thresh(table_sz) ((int)((table_sz) / 2.f))

typedef void (*EntryAction)(_Entry *me);

void _resize_table(Map *map);

Map *map_create(uint32_t size, Hasher hasher, Comparator comparator,
                Alloc alloc, Dealloc dealloc) {
  Map *map = (Map *)alloc(sizeof(Map), 1, "Map");
  map_init(map, size, hasher, comparator, alloc, dealloc);
  return map;
}

void map_init(Map *map, uint32_t size, Hasher hasher, Comparator comparator,
              Alloc alloc, Dealloc dealloc) {
  map->hash = hasher;
  map->compare = comparator;
  map->alloc = alloc;
  map->dealloc = dealloc;
  map->table_sz = size;
  map->entries_thresh = calculate_thresh(size);
  map->table = alloc(sizeof(_Entry), size, "_Entry");
  map->first = NULL;
  map->last = NULL;
  map->num_entries = 0;
}

void map_finalize(Map *map) {
  ASSERT(NOT_NULL(map), NOT_NULL(map->dealloc), NOT_NULL(map->table));
  map->dealloc((void **)&map->table);
}

void map_delete(Map *map) {
  ASSERT(NOT_NULL(map));
  map_finalize(map);
  map->dealloc((void **)&map);
}

bool _map_insert_helper(Map *map, const void *key, const void *value,
                        uint32_t hval, _Entry *table, uint32_t table_sz,
                        _Entry **first, _Entry **last, bool *too_many_inserts) {
  ASSERT(NOT_NULL(map));
  int num_probes = 0;
  int num_previously_used = 0;
  _Entry *first_empty = NULL;
  int num_probes_at_first_empty = -1;
  while (true) {
    int table_index = pos(hval, num_probes, table_sz);
    num_probes++;
    _Entry *me = table + table_index;
    // Position is vacant.
    if (0 == me->num_probes) {
      // Use the previously empty slot if we don't find our element.
      if (first_empty != NULL) {
        me = first_empty;
        num_probes = num_probes_at_first_empty;
      }
      // Take the vacant spot.
      me->pair.key = key;
      me->pair.value = (void *)value;
      me->hash_value = hval;
      me->num_probes = num_probes;
      me->prev = *last;
      me->next = NULL;
      if (NULL != *last) {
        me->prev->next = me;
      }
      *last = me;
      if (NULL == *first) {
        *first = me;
      }
      return true;
    }
    // Spot is vacant but previously used, mark it so we can use it later.
    if (-1 == me->num_probes) {
      num_previously_used++;
      // Returns early if there is a severe performance bottleneck so the table
      // can be rehashed.
      if (num_previously_used > (table_sz / 2)) {
        *too_many_inserts = true;
        return false;
      }
      if (NULL == first_empty) {
        first_empty = me;
        num_probes_at_first_empty = num_probes;
      }
      continue;
    }
    // Pair is already present in the table, so the mission is accomplished.
    if (hval == me->hash_value) {
      if (0 == map->compare(key, me->pair.key)) {
        return false;
      }
    }
    // Rob this entry if it did fewer probes.
    if (me->num_probes < num_probes) {
      _Entry tmp_me = *me;
      // Take its spot.
      me->pair.key = key;
      me->pair.value = (void *)value;
      me->hash_value = hval;
      me->num_probes = num_probes;
      // It is the new insertion.
      key = tmp_me.pair.key;
      value = tmp_me.pair.value;
      hval = tmp_me.hash_value;
      num_probes = tmp_me.num_probes;
      first_empty = NULL;
      num_probes_at_first_empty = -1;
    }
  }
}

bool map_insert(Map *map, const void *key, const void *value) {
  ASSERT(NOT_NULL(map));
  if (map->num_entries > map->entries_thresh) {
    _resize_table(map);
  }
  bool too_many_inserts = false;
  bool was_inserted = _map_insert_helper(map, key, value, map->hash(key),
                                         map->table, map->table_sz, &map->first,
                                         &map->last, &too_many_inserts);
  // Maps may have a lot of removed spots. If this causes a performance
  // slowdown, then it is better to rehash the map.
  if (too_many_inserts) {
    _resize_table(map);
    too_many_inserts = false;
    was_inserted = _map_insert_helper(map, key, value, map->hash(key),
                                      map->table, map->table_sz, &map->first,
                                      &map->last, &too_many_inserts);
    if (too_many_inserts) {
      FATALF("THIS SHOULD NEVER HAPPEN.");
    }
  }
  if (was_inserted) {
    map->num_entries++;
  }
  return was_inserted;
}

_Entry *_map_lookup_entry(const Map *map, const void *key, _Entry *table,
                          uint32_t table_sz) {
  ASSERT(NOT_NULL(map));
  uint32_t hval = map->hash(key);
  int num_probes = 0;
  while (true) {
    int table_index = pos(hval, num_probes, table_sz);
    ++num_probes;
    _Entry *me = table + table_index;
    if (0 == me->num_probes) {
      // Found empty.
      return NULL;
    }
    if (-1 == me->num_probes) {
      // Found previously used.
      continue;
    }
    if (hval == me->hash_value) {
      if (0 == map->compare(key, me->pair.key)) {
        return me;
      }
    }
  }
}

Pair map_remove(Map *map, const void *key) {
  ASSERT(NOT_NULL(map));
  _Entry *me = _map_lookup_entry(map, key, map->table, map->table_sz);
  if (NULL == me) {
    Pair pair = {key, NULL};
    return pair;
  }
  if (map->last == me) {
    map->last = me->prev;
  } else {
    me->next->prev = me->prev;
  }
  if (map->first == me) {
    map->first = me->next;
  } else {
    me->prev->next = me->next;
  }
  me->num_probes = -1;
  map->num_entries--;
  return me->pair;
}

void *map_lookup(const Map *map, const void *key) {
  ASSERT(NOT_NULL(map));
  _Entry *me = _map_lookup_entry(map, key, map->table, map->table_sz);
  if (NULL == me) {
    return NULL;
  }
  return me->pair.value;
}

void map_iterate(const Map *map, PairAction action) {
  ASSERT(NOT_NULL(map));
  M_iter iter;
  for (iter = map_iter((Map *)map); has(&iter); inc(&iter)) {
    action(&iter.__entry->pair);
  }
}

uint32_t map_size(const Map *map) { return map->num_entries; }

void _resize_table(Map *map) {
  ASSERT(NOT_NULL(map));
  uint32_t new_table_sz = calculate_new_size(map->table_sz);
  _Entry *new_table = map->alloc(sizeof(_Entry), new_table_sz, "_Entry");
  _Entry *new_first = NULL;
  _Entry *new_last = NULL;

  M_iter iter = map_iter(map);
  for (; has(&iter); inc(&iter)) {
    _Entry *me = iter.__entry;
    bool too_many_inserts = false;
    _map_insert_helper(map, me->pair.key, me->pair.value, me->hash_value,
                       new_table, new_table_sz, &new_first, &new_last,
                       &too_many_inserts);
    if (too_many_inserts) {
      FATALF("THIS SHOULD NEVER HAPPEN.");
    }
  }

  map->dealloc((void **)&map->table);
  map->table = new_table;
  map->table_sz = new_table_sz;
  map->first = new_first;
  map->last = new_last;
  map->entries_thresh = calculate_thresh(new_table_sz);
}

void inc(M_iter *iter) {
  ASSERT(NOT_NULL(iter), NOT_NULL(iter->__entry));
  iter->__entry = iter->__entry->next;
}

bool has(M_iter *iter) {
  ASSERT(NOT_NULL(iter));
  return NULL != iter->__entry;
}

Pair *pair(M_iter *iter) {
  ASSERT(NOT_NULL(iter));
  return (NULL == iter->__entry) ? NULL : &iter->__entry->pair;
}

const void *key(M_iter *iter) {
  ASSERT(NOT_NULL(iter));
  return (NULL == iter->__entry) ? NULL : iter->__entry->pair.key;
}

void *value(M_iter *iter) {
  ASSERT(NOT_NULL(iter));
  return (NULL == iter->__entry) ? NULL : iter->__entry->pair.value;
}

M_iter map_iter(Map *map) {
  ASSERT(NOT_NULL(map));
  M_iter iter = {.__entry = map->first};
  return iter;
}