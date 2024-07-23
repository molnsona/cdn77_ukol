load("@bazel_skylib//lib:selects.bzl", "selects")
load(
    "//bazel:envoy_build_system.bzl",
    "envoy_package",
)
load("//bazel/wasm:wasm.bzl", "envoy_wasm_cc_binary")

licenses(["notice"])  # Apache 2

envoy_package()

selects.config_setting_group(
    name = "include_wasm_config",
    match_all = [
        "//bazel:x86",
        "//bazel:wasm_v8",
    ],
)

filegroup(
    name = "configs",
    srcs = glob([
        "**/*.wasm",
    ]) + select({
        ":include_wasm_config": glob(
            [
                "**/*.yaml",
            ],
            exclude = [
                "**/*docker-compose*.yaml",
            ],
        ),
        "//conditions:default": [],
    }),
)

envoy_wasm_cc_binary(
    name = "envoy_wasm_cache.wasm",
    srcs = ["envoy_wasm_cache.cc"],
    copts = [   
        "-std=c++17", # C++17 standard
        "-Wall",  # Enable warnings
        "-O2",    # Optimize for speed      
    ],
)

filegroup(
    name = "files",
    srcs = glob(["**/*"]),
)
