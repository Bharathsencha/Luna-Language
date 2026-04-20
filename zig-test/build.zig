const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const test_module = b.createModule(.{
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });

    const unit_tests = b.addTest(.{
        .root_module = test_module,
    });

    test_module.linkSystemLibrary("m", .{});
    test_module.linkSystemLibrary("rt", .{});
    test_module.linkSystemLibrary("gcc_s", .{});
    test_module.linkSystemLibrary("unwind", .{});
    test_module.addIncludePath(b.path("../include"));
    test_module.addCSourceFiles(.{
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
        },
        .flags = &.{
            "-std=c11",
            "-D_POSIX_C_SOURCE=200809L",
            "-Iinclude",
        },
    });
    test_module.addObjectFile(b.path("../lib/libluna_memory_rt.a"));

    const run_unit_tests = b.addRunArtifact(unit_tests);

    const test_step = b.step("test", "Run the Zig Luna test suite");
    test_step.dependOn(&run_unit_tests.step);
}
