const std = @import("std");
const zcc = @import("compile_commands");

pub fn build(b: *std.Build) void {
    const optimize = b.standardOptimizeOption(.{});
    const target = b.standardTargetOptions(.{});

    const exe = b.addExecutable(.{
        .name = "my_app",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });
    exe.root_module.addCSourceFiles(.{
        .files = &.{
            "src/allocator.c",    "src/common.c",
            "src/main.c",         "vendor/munit/munit.c",
            "src/hashmap_test.c",
        },
    });
    b.installArtifact(exe);

    // TODO: FIXME
    // const munit = b.dependency("munit", .{});
    // exe.root_module.addSystemIncludePath(munit.path("munit.h"));
    // exe.root_module.addCSourceFile(.{ .file = munit.path("munit.c") });

    exe.root_module.addCMacro("MUNIT_ENABLE_ASSERT_ALIASES", "1");
    exe.root_module.addSystemIncludePath(b.path("vendor/"));

    const cc_gen_step = b.step("compile-commands", "Generate 'compile_commands.json'");
    const cc_gen_cmd = zcc.createStep(b, target, &.{exe});
    cc_gen_step.dependOn(cc_gen_cmd);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }
    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);

    // TODO:
    // const run_tests = b.addRunArtifact(b.addTest(.{
    //     .root_module = exe.root_module,
    // }));
    // const test_step = b.step("test", "Run tests");
    // test_step.dependOn(&run_tests.step);
}
