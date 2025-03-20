// memory_graph.c
//
// Created on: May 5, 2020
//     Author: Jeff Manzione

#include "alloc/memory_graph/memory_graph.h"

#include <stdint.h>

#include "alloc/alloc.h"
#include "alloc/arena/arena.h"
#include "debug/debug.h"
#include "struct/map.h"
#include "struct/set.h"
#include "struct/struct_defaults.h"

// Considerably-large prime number.
#define DEFAULT_NODE_TABLE_SZ 997
#define DEFAULT_ROOT_TABLE_SZ DEFAULT_TABLE_SZ
#define DEFAULT_CHILDREN_TABLE_SZ 17

typedef Node *(*NProducer)();

struct __MGraph {
  MGraphConf config;
  __Arena node_arena; // Node
  __Arena edge_arena; // _Edge
  Set nodes;          // Node
  Set roots;          // Node
  uint32_t node_count;
};

typedef struct {
  uint32_t int_id;
} _Id;

struct __Node {
  _Id id;
  Ref ptr;
  Deleter del;
  Map children; // key: Node, vale: _Edge
  Map parents;  // key: Node, vale: _Edge
};

typedef struct {
  Node *node;
  uint32_t ref_count;
} _Edge;

uint32_t _node_id(MGraph *mg);
Node *_node_create(MGraph *mg, Ref ptr, Deleter del);
void _node_delete(MGraph *mg, Node *node, bool delete_edges, bool delete_node);
_Edge *_edge_create(MGraph *mg, Node *node);
void _edge_delete(MGraph *mg, _Edge *edge);

MGraph *mgraph_create(const MGraphConf *const config) {
  ASSERT(NOT_NULL(config));
  MGraph *mg = ALLOC2(MGraph);
  mg->config = *config;
  __arena_init(&mg->node_arena, sizeof(Node), "Node");
  __arena_init(&mg->edge_arena, sizeof(_Edge), "_Edge");
  set_init_custom_comparator(&mg->nodes, DEFAULT_NODE_TABLE_SZ, default_hasher,
                             default_comparator);
  set_init_custom_comparator(&mg->roots, DEFAULT_ROOT_TABLE_SZ, default_hasher,
                             default_comparator);
  mg->node_count = 0;
  return mg;
}

void mgraph_delete(MGraph *mg) {
  ASSERT(NOT_NULL(mg));
  M_iter iter = set_iter(&mg->nodes);
  for (; has(&iter); inc(&iter)) {
    _node_delete(mg, (Node *)value(&iter), /*delete_edges=*/false,
                 /*delete_node=*/false);
  }
  __arena_finalize(&mg->node_arena);
  __arena_finalize(&mg->edge_arena);
  set_finalize(&mg->nodes);
  set_finalize(&mg->roots);
  DEALLOC(mg);
}

Node *mgraph_insert(MGraph *mg, Ref ptr, Deleter del) {
  ASSERT(NOT_NULL(mg), NOT_NULL(ptr), NOT_NULL(del));
  Node *node = _node_create(mg, ptr, del);
  set_insert(&mg->nodes, node);
  return node;
}

void mgraph_root(MGraph *mg, Node *node) {
  ASSERT(NOT_NULL(mg), NOT_NULL(node));
  set_insert(&mg->roots, node);
}

void mgraph_inc(MGraph *mg, Node *parent, Node *child) {
  ASSERT(NOT_NULL(mg), NOT_NULL(parent), NOT_NULL(child));
  // printf("mgraph_inc(parent=%p, child=%p)\n", parent->ptr, child->ptr);
  // if (NULL == set_lookup(&mg->nodes, child)) {
  //   printf("child not in graph (%p)-->(%p)\n", parent->ptr, child->ptr);
  // }
  _Edge *p2c = map_lookup(&parent->children, child);
  if (NULL == p2c) {
    map_insert(&parent->children, child, _edge_create(mg, child));
  } else {
    p2c->ref_count++;
  }
  _Edge *c2p = map_lookup(&child->parents, parent);
  if (NULL == c2p) {
    map_insert(&child->parents, parent, _edge_create(mg, parent));
  } else {
    c2p->ref_count++;
  }
}

void mgraph_dec(MGraph *mg, Node *parent, Node *child) {
  ASSERT(NOT_NULL(mg), NOT_NULL(parent), NOT_NULL(child));
  _Edge *p2c = map_lookup(&parent->children, child);
  if (NULL == p2c || p2c->ref_count < 1) {
    FATALF("Removing reference from parent %p to %p which did not exist.",
           parent->ptr, child->ptr);
  }
  p2c->ref_count--;
  _Edge *c2p = map_lookup(&child->parents, parent);
  if (NULL == c2p || c2p->ref_count < 1) {
    FATALF("Removing reference from child %p to %p which did not exist.",
           child->ptr, parent->ptr);
  }
  c2p->ref_count--;
}

void _process_node(Node *node, Set *marked) {
  // printf("encountered %p\n", node->ptr);

  if (!set_insert(marked, node)) {
    // Node already processed
    return;
  }
  M_iter child_iter = map_iter(&node->children);
  for (; has(&child_iter); inc(&child_iter)) {
    _Edge *e = (_Edge *)value(&child_iter);
    // printf("\tchild %p@%u\n", e->node->ptr, e->ref_count);
    if (e->ref_count > 0) {
      _process_node((void *)key(&child_iter), marked); // blessed.
    }
  }
}

uint32_t mgraph_collect_garbage(MGraph *mg) {
  ASSERT(NOT_NULL(mg));
  uint32_t deleted_nodes_count = 0;
  Set marked;
  set_init_custom_comparator(&marked, set_size(&mg->nodes) * 2, default_hasher,
                             default_comparator);
  M_iter root_iter = set_iter(&mg->roots);
  for (; has(&root_iter); inc(&root_iter)) {
    _process_node((Node *)value(&root_iter), &marked);
  }
  M_iter node_iter = set_iter(&mg->nodes);
  for (; has(&node_iter); inc(&node_iter)) {
    Node *node = (Node *)value(&node_iter);
    if (set_lookup(&marked, node)) {
      continue;
    }
    _node_delete(mg, node, mg->config.eager_delete_edges,
                 mg->config.eager_delete_nodes);
    set_remove(&mg->nodes, node);
    deleted_nodes_count++;
  }
  set_finalize(&marked);

  // printf("Nodes:\n\titem_size=%u\n\tcapacity=%u\n\titem_count=%u\n\tsubarena_"
  //        "capacity=%u\n\tsubarena_count=%u\n",
  //        __arena_item_size(&mg->node_arena),
  //        __arena_capacity(&mg->node_arena),
  //        __arena_item_count(&mg->node_arena),
  //        __arena_subarena_capacity(&mg->node_arena),
  //        __arena_subarena_count(&mg->node_arena));

  // printf("Edges:\n\titem_size=%u\n\tcapacity=%u\n\titem_count=%u\n\tsubarena_"
  //        "capacity=%u\n\tsubarena_count=%u\n",
  //        __arena_item_size(&mg->edge_arena),
  //        __arena_capacity(&mg->edge_arena),
  //        __arena_item_count(&mg->edge_arena),
  //        __arena_subarena_capacity(&mg->edge_arena),
  //        __arena_subarena_count(&mg->edge_arena));

  return deleted_nodes_count;
}

uint32_t _node_id(MGraph *mg) {
  ASSERT(NOT_NULL(mg));
  return mg->node_count++;
}

Node *_node_create(MGraph *mg, Ref ptr, Deleter del) {
  ASSERT(NOT_NULL(mg), NOT_NULL(ptr), NOT_NULL(del));
  Node *node = (Node *)__arena_alloc(&mg->node_arena);
  node->id.int_id = _node_id(mg);
  node->ptr = ptr;
  node->del = del;
  map_init_custom_comparator(&node->children, DEFAULT_CHILDREN_TABLE_SZ,
                             default_hasher, default_comparator);
  map_init_custom_comparator(&node->parents, DEFAULT_CHILDREN_TABLE_SZ,
                             default_hasher, default_comparator);
  return node;
}

void _node_delete(MGraph *mg, Node *node, bool delete_edges, bool delete_node) {
  ASSERT(NOT_NULL(mg), NOT_NULL(node));
  if (NULL != node->del) {
    node->del(node->ptr, mg->config.ctx);
  }
  if (delete_edges) {
    M_iter children = map_iter(&node->children);
    for (; has(&children); inc(&children)) {
      _edge_delete(mg, (_Edge *)value(&children));
    }
    M_iter parents = map_iter(&node->parents);
    for (; has(&parents); inc(&parents)) {
      _edge_delete(mg, (_Edge *)value(&parents));
    }
  }
  map_finalize(&node->children);
  map_finalize(&node->parents);
  if (delete_node) {
    __arena_dealloc(&mg->node_arena, node);
  }
}

_Edge *_edge_create(MGraph *mg, Node *node) {
  _Edge *edge = (_Edge *)__arena_alloc(&mg->edge_arena);
  edge->node = node;
  edge->ref_count = 1;
  return edge;
}

void _edge_delete(MGraph *mg, _Edge *edge) {
  __arena_dealloc(&mg->edge_arena, edge);
}

uint32_t mgraph_node_count(const MGraph *const mg) {
  return set_size(&mg->nodes);
}

const Set *mgraph_nodes(const MGraph *const mg) { return &mg->nodes; }

const void *node_ptr(const Node *node) { return node->ptr; }