load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "file_lib",
    srcs = [
        "file.cpp",
    ],
    hdrs = [
        "file.h",
    ],
    deps = [
        "//src/dht:dht_lib",
        "//src/utils:utils_lib",
    ],
    visibility = [
        "//tests:__pkg__",
    ]
)

cc_library(
    name = "client_lib",
    srcs = [
        "cmd.cpp",
        "daemon.cpp",
        "interactive.cpp",
    ],
    hdrs = [
        "client.h",
    ],
    deps = [
        ":file_lib",
        "//src/dht:dht_lib",
        "//src/utils:utils_lib",
    ],
)

cc_binary(
    name = "distft_founder",
    srcs = [
        "founder.cpp",
    ],
    deps = [
        ":client_lib",
    ],
)

cc_binary(
    name = "distft_transient",
    srcs = [
        "transient.cpp",
    ],
    deps = [
        ":client_lib",
    ],
)
