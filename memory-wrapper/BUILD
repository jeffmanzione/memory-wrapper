load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "memory_graph",
    srcs = ["memory_graph.c"],
    hdrs = ["memory_graph.h"],
    deps = [
        "@jeffmanzione_c_data_structures//c-data-structures:maplike",
        "@jeffmanzione_c_data_structures//c-data-structures:setlike",
        "@jeffmanzione_rzalloc//rzalloc",
    ],
)

cc_test(
    name = "memory_graph_test",
    size = "small",
    srcs = ["memory_graph_test.cc"],
    deps = [
        ":memory_graph",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
