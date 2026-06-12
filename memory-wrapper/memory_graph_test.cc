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

}  // namespace