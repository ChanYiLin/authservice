load("@com_envoyproxy_protoc_gen_validate//bazel:pgv_proto_library.bzl", "pgv_cc_proto_library")

pgv_cc_proto_library(
    name = "config_cc",
    cc_deps = ["//config/common:config_cc"],
    linkstatic = True,
    visibility = ["//visibility:public"],
    deps = [
        "//config/oidc:config_proto",
    ],
)

proto_library(
    name = "config_proto",
    srcs = ["config.proto"],
    visibility = ["//visibility:public"],
    deps = [
        "//config/common:config_proto",
        "@com_envoyproxy_protoc_gen_validate//validate:validate_proto",
    ],
)
