load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load("@rules_cc//cc:defs.bzl", "cc_proto_library", "cc_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

cc_library(
    name = "dht_lib",
    srcs = [
        "session.cpp",
        "router.cpp",
        "rpc.cpp",
    ],
    hdrs = [
        "session.h",
        "router.h",
    ],
    deps = [
        "//src/utils:utils_lib",
        ":dht_grpc_cpp",
        "@com_github_gabime_spdlog//:spdlog",
        "@com_github_grpc_grpc//:grpc++"
    ],
)

cc_grpc_library(
    name = "dht_grpc_cpp",
    srcs = [":dht_proto"],
    grpc_only = True,
    deps = [":dht_proto_cpp"],
)

cc_proto_library(
    name = "dht_proto_cpp",
    deps = [":dht_proto"],
)

proto_library(
    name = "dht_proto",
    srcs = ["dht.proto"],
)

