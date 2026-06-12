// memory_graph.c
//
// Created on: May 5, 2020
//     Author: Jeff Manzione

#include "memory-wrapper/memory_graph.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "c-data-structures/maplike.h"

// Considerably-large prime number.
#define DEFAULT_NODE_TABLE_SZ 997
#define DEFAULT_ROOT_TABLE_SZ 7
#define DEFAULT_CHILDREN_TABLE_SZ 17

IMPL_SETLIKE(NodeSet, Node *);

typedef struct {
  uint32_t int_id;
} Id_;

typedef struct {
  Node *node;
  uint32_t ref_count;
} Edge_;

DEFINE_MAPLIKE(NodeEdgeMap, Node *, Edge_ *);
IMPL_MAPLIKE(NodeEdgeMap, Node *, Edge_ *);

struct Node_ {
  Id_ id;
  Ref ptr;
  MGDeleter del;
  NodeEdgeMap children;
  NodeEdgeMap parents;
};

uint32_t node_id_(MGraph *mg);
Node *node_create_(MGraph *mg, Ref ptr, MGDeleter del);
void node_delete_(MGraph *mg, Node *node, bool delete_edges, bool delete_node);
Edge_ *edge_create_(MGraph *mg, Node *node);
void edge_delete_(MGraph *mg, Edge_ *edge);

uint32_t hash_node_(const Node *n, uint32_t size) {
  return (uint32_t)(intptr_t)n;
}

int32_t compare_nodes_(const Node *n1, uint32_t n1_size, const Node *n2,
                       uint32_t n2_size) {
  return ((intptr_t)n1) - ((intptr_t)n2);
}

bool mgraph_init(MGraph *mg, const MGraphConf *const config) {
  assert(mg != NULL);
  assert(config != NULL);
  mg->config = *config;
  if (!arena_init(&mg->node_arena, sizeof(Node))) {
    return false;
  }
  if (!arena_init(&mg->edge_arena, sizeof(Edge_))) {
    return false;
  }
  if (!NodeSet_init_capacity(&mg->nodes, DEFAULT_NODE_TABLE_SZ, hash_node_,
                             compare_nodes_)) {
    return false;
  }
  if (!NodeSet_init_capacity(&mg->roots, DEFAULT_ROOT_TABLE_SZ, hash_node_,
                             compare_nodes_)) {
    return false;
  }
  mg->node_count = 0;
  return true;
}

void mgraph_finalize(MGraph *mg) {
  assert(mg != NULL);
  NodeSetIterator iter;
  NodeSet_iterator(&iter, &mg->nodes);
  for (; NodeSet_has_next(&iter); NodeSet_next(&iter)) {
    node_delete_(mg, *NodeSet_mutable_value(&iter), /*delete_edges=*/false,
                 /*delete_node=*/false);
  }
  arena_clear(&mg->node_arena);
  arena_clear(&mg->edge_arena);
  NodeSet_finalize(&mg->nodes);
  NodeSet_finalize(&mg->roots);
}

Node *mgraph_insert(MGraph *mg, Ref ptr, MGDeleter del) {
  assert(mg != NULL);
  assert(ptr != NULL);
  assert(del != NULL);
  Node *node = node_create_(mg, ptr, del);
  NodeSet_insert(&mg->nodes, node, sizeof(Node *));
  return node;
}

void mgraph_root(MGraph *mg, Node *node) {
  assert(mg != NULL);
  assert(node != NULL);
  NodeSet_insert(&mg->roots, node, sizeof(Node *));
}

void mgraph_inc(MGraph *mg, Node *parent, Node *child) {
  assert(mg != NULL);
  assert(parent != NULL);
  assert(child != NULL);
  Edge_ *p2c = NodeEdgeMap_find(&parent->children, child, sizeof(Node *), NULL);
  if (NULL == p2c) {
    NodeEdgeMap_insert(&parent->children, child, sizeof(Node *),
                       edge_create_(mg, child));
  } else {
    p2c->ref_count++;
  }

  Edge_ *c2p = NodeEdgeMap_find(&child->parents, parent, sizeof(Node *), NULL);
  if (NULL == c2p) {
    NodeEdgeMap_insert(&child->parents, parent, sizeof(Node *),
                       edge_create_(mg, parent));
  } else {
    c2p->ref_count++;
  }
}

void mgraph_dec(MGraph *mg, Node *parent, Node *child) {
  assert(mg != NULL);
  assert(parent != NULL);
  assert(child != NULL);
  Edge_ *p2c = NodeEdgeMap_find(&parent->children, child, sizeof(Node *), NULL);
  if (NULL == p2c || p2c->ref_count < 1) {
    fprintf(stderr,
            "Removing reference from parent %p to %p which did not exist.\n",
            parent->ptr, child->ptr);
    exit(1);
  }
  p2c->ref_count--;
  Edge_ *c2p = NodeEdgeMap_find(&child->parents, parent, sizeof(Node *), NULL);
  if (NULL == c2p || c2p->ref_count < 1) {
    fprintf(stderr,
            "Removing reference from child %p to %p which did not exist.\n",
            child->ptr, parent->ptr);
    exit(1);
  }
  c2p->ref_count--;
}

void process_node_(Node *node, NodeSet *marked) {
  if (!NodeSet_insert(marked, node, sizeof(Node *))) {
    // Node already processed
    return;
  }
  NodeEdgeMapIterator child_iter;
  NodeEdgeMap_iterator(&child_iter, &node->children);
  for (; NodeEdgeMap_has_entry(&child_iter);
       NodeEdgeMap_next_entry(&child_iter)) {
    const Edge_ *e = *NodeEdgeMap_value(&child_iter);
    if (e->ref_count > 0) {
      process_node_((Node *)NodeEdgeMap_key(&child_iter), marked);
    }
  }
}

uint32_t mgraph_collect_garbage(MGraph *mg) {
  assert(mg != NULL);
  uint32_t deleted_nodes_count = 0;
  NodeSet marked;
  NodeSet_init_capacity(&marked, NodeSet_size(&mg->nodes) * 2, hash_node_,
                        compare_nodes_);
  NodeSetIterator root_iter;
  NodeSet_iterator(&root_iter, &mg->roots);
  for (; NodeSet_has_next(&root_iter); NodeSet_next(&root_iter)) {
    process_node_(*NodeSet_mutable_value(&root_iter), &marked);
  }
  NodeSetIterator node_iter;
  NodeSet_iterator(&node_iter, &mg->nodes);
  for (; NodeSet_has_next(&node_iter); NodeSet_next(&node_iter)) {
    Node *node = *NodeSet_mutable_value(&node_iter);
    if (NodeSet_contains(&marked, node, sizeof(Node *))) {
      continue;
    }
    node_delete_(mg, node, mg->config.eager_delete_edges,
                 mg->config.eager_delete_nodes);
    NodeSet_remove(&mg->nodes, node, sizeof(Node *));
    deleted_nodes_count++;
  }
  NodeSet_finalize(&marked);
  return deleted_nodes_count;
}

uint32_t node_id_(MGraph *mg) {
  assert(mg != NULL);
  return mg->node_count++;
}

Node *node_create_(MGraph *mg, Ref ptr, MGDeleter del) {
  assert(mg != NULL);
  assert(ptr != NULL);
  assert(del != NULL);
  Node *node = (Node *)arena_malloc(&mg->node_arena);
  node->id.int_id = node_id_(mg);
  node->ptr = ptr;
  node->del = del;
  NodeEdgeMap_init_capacity(&node->children, DEFAULT_CHILDREN_TABLE_SZ,
                            hash_node_, compare_nodes_);
  NodeEdgeMap_init_capacity(&node->parents, DEFAULT_CHILDREN_TABLE_SZ,
                            hash_node_, compare_nodes_);
  return node;
}

void node_delete_(MGraph *mg, Node *node, bool delete_edges, bool delete_node) {
  assert(mg != NULL);
  assert(node != NULL);
  if (node->del != NULL) {
    node->del(node->ptr, mg->config.ctx);
  }
  if (delete_edges) {
    NodeEdgeMapIterator children;
    NodeEdgeMap_iterator(&children, &node->children);
    for (; NodeEdgeMap_has_entry(&children);
         NodeEdgeMap_next_entry(&children)) {
      edge_delete_(mg, *NodeEdgeMap_mutable_value(&children));
    }
    NodeEdgeMapIterator parents;
    NodeEdgeMap_iterator(&parents, &node->parents);
    for (; NodeEdgeMap_has_entry(&parents); NodeEdgeMap_next_entry(&parents)) {
      edge_delete_(mg, *NodeEdgeMap_mutable_value(&parents));
    }
  }
  NodeEdgeMap_finalize(&node->children);
  NodeEdgeMap_finalize(&node->parents);
  if (delete_node) {
    arena_free(&mg->node_arena, node);
  }
}

Edge_ *edge_create_(MGraph *mg, Node *node) {
  Edge_ *edge = (Edge_ *)arena_malloc(&mg->edge_arena);
  edge->node = node;
  edge->ref_count = 1;
  return edge;
}

void edge_delete_(MGraph *mg, Edge_ *edge) {
  arena_free(&mg->edge_arena, edge);
}

uint32_t mgraph_node_count(const MGraph *const mg) {
  return NodeSet_size(&mg->nodes);
}

const NodeSet *mgraph_nodes(const MGraph *const mg) { return &mg->nodes; }

const void *node_ptr(const Node *node) { return node->ptr; }