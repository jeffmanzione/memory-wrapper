// memory_graph.h
//
// Created on: May 5, 2020
//     Author: Jeff Manzione
//
// A library for maintaining relationships between entities and
// deleting them a garbage collection is requested and there no longer exists a
// reference to them.

#ifndef ALLOC_MEMORY_GRAPH_MEMORY_GRAPH_H_
#define ALLOC_MEMORY_GRAPH_MEMORY_GRAPH_H_

#include <stdbool.h>
#include <stdint.h>

#include "struct/set.h"

// Useful for changing is type in the future.
typedef void *Ref;

// A funciton which takes a void pointer and we trust will delete it.
typedef void (*Deleter)(void *ptr, void *ctx);

typedef struct __Node Node;
typedef struct __MGraph MGraph;

// Configuration to tell the MGraph how to behave.
typedef struct {
  // Memory for _Edges will be freed when the node entity is deleted.
  bool eager_delete_edges;
  // Memory for Nodes will be freed when the node entity is deleted.
  bool eager_delete_nodes;
  // Context needed during deletion operations.
  void *ctx;
} MGraphConf;

// Creates a MGraph based on the given [config].
MGraph *mgraph_create(const MGraphConf *const config);

// Deletes the MGraph and frees all relevant memory.
void mgraph_delete(MGraph *mg);

// Makes the specified [node] a root in the graph. All node retained by this
// node will be retained.
void mgraph_root(MGraph *mg, Node *node);

// Creates a new node for the specified [ptr] reference.
//
// Passing a Ref and Deleter to this method contractually hands over the
// lifecycle of that reference to the graph. The graph is now responsible for
// calling [del] to delete the entity when it is no longer needed. Manually
// deleting the underlying entity associated with [ptr] has undefined behavior.
Node *mgraph_insert(MGraph *mg, Ref ptr, Deleter del);

// Adds to the reference counter between [parent] and [child].
void mgraph_inc(MGraph *mg, Node *parent, Node *child);

// Decreses the references count between [parent] and [child].
void mgraph_dec(MGraph *mg, Node *parent, Node *child);

// Deletes any nodes which no longer have a target reference and potentially
// freeing up their space to be reused. Returns the number of nodes which were
// freed.
uint32_t mgraph_collect_garbage(MGraph *mg);

// The number of nodes currently in the graph.
uint32_t mgraph_node_count(const MGraph *const mg);

// Returns the nodes of the graph.
const Set *mgraph_nodes(const MGraph *const mg);

const void *node_ptr(const Node *node);

#endif /* ALLOC_MEMORY_GRAPH_MEMORY_GRAPH_H_ */