load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "test_lib",
    srcs = [
        "file.cpp",
        "session.cpp",
        "utils.cpp",
    ],
    hdrs = [
        "tests.h"
    ],
    deps = [
        "//src/client:file_lib",
        "//src/dht:dht_lib",
        "//src/utils:utils_lib",
    ],
)

cc_binary(
    name = "test_prog",
    srcs = [
        "main.cpp",
    ],
    deps = [
        ":test_lib",
    ],
)