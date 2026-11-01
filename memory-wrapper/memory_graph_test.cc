#include "memory-wrapper/memory_graph.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdint.h>

namespace {

using testing::Contains;
using testing::Eq;
using testing::NotNull;

void free_ptr(void *ptr, void *ctx) { free(ptr); }

/* Test fixture to ensure proper setup / teardown */
class MGraphTest : public ::testing::Test {
 protected:
  MGraph mg{};
  MGraphConf cfg{
      .eager_delete_edges = true,
      .eager_delete_nodes = true,
      .ctx = NULL,
  };

  void SetUp() override { EXPECT_TRUE(mgraph_init(&mg, &cfg)); }

  void TearDown() override { mgraph_finalize(&mg); }
};

TEST_F(MGraphTest, SimpleRootRetainsNodes) {
  char *alice = strdup("alice");
  char *bob = strdup("bob");
  std::vector<char *> people{alice, bob};

  Node *alice_node = mgraph_insert(&mg, alice, free_ptr);
  Node *bob_node = mgraph_insert(&mg, bob, free_ptr);

  mgraph_root(&mg, alice_node);
  mgraph_inc(&mg, alice_node, bob_node);

  EXPECT_EQ(mgraph_node_count(&mg), 2);

  NodeSetIterator it;
  NodeSet_iterator(&it, (NodeSet *)mgraph_nodes(&mg));

  for (; NodeSet_has_next(&it); NodeSet_next(&it)) {
    const Node *n = *NodeSet_value(&it);
    EXPECT_THAT(n, NotNull());
    const char *n_ptr = (char *)node_ptr(n);
    EXPECT_THAT(people, Contains(Eq(n_ptr)).Times(1));
  }

  EXPECT_EQ(mgraph_collect_garbage(&mg), 0);
}

TEST_F(MGraphTest, NoRootDeletesNodes) {
  char *alice = strdup("alice");
  char *bob = strdup("bob");
  std::vector<char *> people{alice, bob};

  Node *alice_node = mgraph_insert(&mg, alice, free_ptr);
  Node *bob_node = mgraph_insert(&mg, bob, free_ptr);

  // mgraph_root(&mg, alice_node);
  mgraph_inc(&mg, alice_node, bob_node);

  EXPECT_EQ(mgraph_node_count(&mg), 2);

  NodeSetIterator it;
  NodeSet_iterator(&it, (NodeSet *)mgraph_nodes(&mg));

  for (; NodeSet_has_next(&it); NodeSet_next(&it)) {
    const Node *n = *NodeSet_value(&it);
    EXPECT_THAT(n, NotNull());
    const char *n_ptr = (char *)node_ptr(n);
    EXPECT_THAT(people, Contains(Eq(n_ptr)).Times(1));
  }

  EXPECT_EQ(mgraph_collect_garbage(&mg), 2);

  EXPECT_EQ(mgraph_node_count(&mg), 0);
}

TEST_F(MGraphTest, SimpleRootRetainsOnlyConnectedNodes) {
  char *alice = strdup("alice");
  char *bob = strdup("bob");
  std::vector<char *> people{alice, bob};

  Node *alice_node = mgraph_insert(&mg, alice, free_ptr);
  Node *bob_node = mgraph_insert(&mg, bob, free_ptr);

  mgraph_root(&mg, alice_node);
  mgraph_inc(&mg, alice_node, bob_node);

  EXPECT_EQ(mgraph_node_count(&mg), 2);

  mgraph_dec(&mg, alice_node, bob_node);

  EXPECT_EQ(mgraph_node_count(&mg), 2);

  EXPECT_EQ(mgraph_collect_garbage(&mg), 1);

  EXPECT_EQ(mgraph_node_count(&mg), 1);
}

TEST_F(MGraphTest, ReferenceCanBeReaddedAfterRemoval) {
  Node *alice_node = mgraph_insert(&mg, strdup("alice"), free_ptr);
  Node *bob_node = mgraph_insert(&mg, strdup("bob"), free_ptr);
  mgraph_root(&mg, alice_node);

  for (int i = 0; i < 1000; ++i) {
    mgraph_inc(&mg, alice_node, bob_node);
    mgraph_inc(&mg, alice_node, bob_node);
    mgraph_dec(&mg, alice_node, bob_node);
    mgraph_dec(&mg, alice_node, bob_node);
  }
  mgraph_inc(&mg, alice_node, bob_node);
  EXPECT_EQ(mgraph_collect_garbage(&mg), 0);

  mgraph_dec(&mg, alice_node, bob_node);
  EXPECT_EQ(mgraph_collect_garbage(&mg), 1);
  EXPECT_EQ(mgraph_node_count(&mg), 1);
}

// A long-lived node referenced by many short-lived ones, which are collected
// each round, as an object's class or a shared value is.
TEST_F(MGraphTest, CollectedReferrersOfLiveNode) {
  Node *root_node = mgraph_insert(&mg, strdup("root"), free_ptr);
  Node *shared_node = mgraph_insert(&mg, strdup("shared"), free_ptr);
  mgraph_root(&mg, root_node);
  mgraph_inc(&mg, root_node, shared_node);

  for (int round = 0; round < 1000; ++round) {
    for (int i = 0; i < 10; ++i) {
      Node *temp_node = mgraph_insert(&mg, strdup("temp"), free_ptr);
      mgraph_inc(&mg, temp_node, shared_node);
      // Also referenced by the root for a while.
      mgraph_inc(&mg, root_node, temp_node);
      mgraph_dec(&mg, root_node, temp_node);
    }
    EXPECT_EQ(mgraph_collect_garbage(&mg), 10);
    EXPECT_EQ(mgraph_node_count(&mg), 2);
  }

  mgraph_dec(&mg, root_node, shared_node);
  EXPECT_EQ(mgraph_collect_garbage(&mg), 1);
  EXPECT_EQ(mgraph_node_count(&mg), 1);
}

}  // namespace