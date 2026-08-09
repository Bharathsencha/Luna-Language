const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const unit_tests = b.addTest(.{
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
    });

    unit_tests.linkLibC();
    unit_tests.linkSystemLibrary("m");
    unit_tests.linkSystemLibrary("rt");
    unit_tests.linkSystemLibrary("gcc_s");
    unit_tests.linkSystemLibrary("unwind");
    unit_tests.addIncludePath(b.path("../include"));
    unit_tests.addCSourceFiles(.{
        .root = b.path(".."),
        .files = &.{
            "src/lexer.c",
            "src/token.c",
            "src/util.c",
            "src/ast.c",
            "src/parser.c",
            "src/interpreter.c",
            "src/value.c",
            "src/gc.c",
            "src/gc_visit.c",
            "src/error.c",
            "src/env.c",
            "src/unsafe_runtime.c",
            "src/vec_lib.c",
            "src/arena.c",
            "src/intern.c",
            "src/luna_runtime.c",
            "src/luna_test.c",
            "src/data_runtime.c",
        },
        .flags = &.{
            "-std=c11",
            "-D_POSIX_C_SOURCE=200809L",
            "-Iinclude",
        },
    });
    unit_tests.addObjectFile(b.path("../lib/libluna_memory_rt.a"));
    unit_tests.addObjectFile(b.path("../lib/libluna_data_rt.a"));

    const run_unit_tests = b.addRunArtifact(unit_tests);

    const test_step = b.step("test", "Run the Zig Luna test suite");
    test_step.dependOn(&run_unit_tests.step);
}
