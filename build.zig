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
            "src/allocator.c", "src/common.c", "src/main.c",
        },
    });
    b.installArtifact(exe);

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

    const run_tests = b.addRunArtifact(b.addTest(.{
        .root_module = exe.root_module,
    }));
    const test_step = b.step("test", "Run tests");
    test_step.dependOn(&run_tests.step);
}
