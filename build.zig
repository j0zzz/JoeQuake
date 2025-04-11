const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = std.builtin.OptimizeMode.ReleaseSmall });

    const libminizip = b.addStaticLibrary(.{
        .name = "joequake_minizip",
        .target = target,
        .optimize = optimize,
    });

    libminizip.linkLibC();
    libminizip.addIncludePath(b.path("trunk"));
    libminizip.addCSourceFiles(.{
        .files = &.{
            "trunk/ioapi.c",
            "trunk/unzip.c",
        },
        .flags = &.{ "-Wall", "-DNOUNCRYPT", "-std=c99" },
    });
    libminizip.linkSystemLibrary("z");

    const exe = b.addExecutable(.{
        .name = "joequake-gl-zig",
        .target = target,
        .optimize = optimize,
    });

    exe.addCSourceFiles(.{
        .files = &.{
            "trunk/bgmusic.c",
            "trunk/cd_linux.c",
            "trunk/chase.c",
            "trunk/cl_demo.c",
            "trunk/cl_demoui.c",
            "trunk/cl_dzip.c",
            "trunk/cl_input.c",
            "trunk/cl_main.c",
            "trunk/cl_parse.c",
            "trunk/cl_slist.c",
            "trunk/cl_tent.c",
            "trunk/cmd.c",
            "trunk/common.c",
            "trunk/console.c",
            "trunk/crc.c",
            "trunk/cvar.c",
            "trunk/fmod.c",
            "trunk/gl_decals.c",
            "trunk/gl_draw.c",
            "trunk/gl_fog.c",
            "trunk/gl_mesh.c",
            "trunk/gl_model.c",
            "trunk/gl_refrag.c",
            "trunk/gl_rlight.c",
            "trunk/gl_rmain.c",
            "trunk/gl_rmisc.c",
            "trunk/gl_rpart.c",
            "trunk/gl_rsurf.c",
            "trunk/gl_screen.c",
            "trunk/gl_warp.c",
            "trunk/host.c",
            "trunk/host_cmd.c",
            "trunk/image.c",
            "trunk/in_sdl.c",
            "trunk/iplog.c",
            "trunk/keys.c",
            "trunk/mathlib.c",
            "trunk/menu.c",
            "trunk/nehahra.c",
            "trunk/net_bsd.c",
            "trunk/net_dgrm.c",
            "trunk/net_loop.c",
            "trunk/net_main.c",
            "trunk/net_udp.c",
            "trunk/net_vcr.c",
            "trunk/pr_cmds.c",
            "trunk/pr_edict.c",
            "trunk/pr_exec.c",
            "trunk/r_part.c",
            "trunk/sbar.c",
            "trunk/security.c",
            "trunk/snd_codec.c",
            "trunk/snd_dma.c",
            "trunk/snd_sdl.c",
            "trunk/snd_mem.c",
            "trunk/snd_mix.c",
            "trunk/snd_mp3.c",
            "trunk/snd_mp3tag.c",
            "trunk/snd_vorbis.c",
            "trunk/sv_main.c",
            "trunk/sv_move.c",
            "trunk/sv_phys.c",
            "trunk/sv_user.c",
            "trunk/sys_linux.c",
            "trunk/version.c",
            "trunk/vid_common_gl.c",
            "trunk/vid_sdl.c",
            "trunk/view.c",
            "trunk/wad.c",
            "trunk/world.c",
            "trunk/zone.c",
            "trunk/demoparse.c",
            "trunk/demoseekparse.c",
            "trunk/democam.c",
            "trunk/practice.c",
            "trunk/ghost/ghost.c",
            "trunk/ghost/ghostparse.c",
            "trunk/ghost/demosummary.c",
            "trunk/qcurses/cJSON.c",
            "trunk/qcurses/set.c",
            "trunk/qcurses/qcurses.c",
            "trunk/qcurses/browser.c",
            "trunk/qcurses/browser_local.c",
            "trunk/qcurses/browser_news.c",
            "trunk/qcurses/browser_curl.c",
        },
        .flags = &.{ "-Wall", "-DGLQUAKE", "-DSDL2", "-DUSE_CODEC_VORBIS", "-DUSE_CODEC_MP3" },
    });

    exe.addIncludePath(b.path("trunk"));
    exe.addIncludePath(std.Build.LazyPath{ .cwd_relative = "/usr/include/SDL2/" });
    exe.addIncludePath(b.path("trunk/ghost"));
    exe.addIncludePath(b.path("trunk/qcurses"));
    //exe.addIncludePath(b.path("trunk/bhop"));

    exe.linkLibC();
    exe.linkSystemLibrary("png");
    exe.linkSystemLibrary("jpeg");
    exe.linkSystemLibrary("curl");
    exe.linkSystemLibrary("m");
    exe.linkSystemLibrary("dl");
    exe.linkSystemLibrary("vorbisfile");
    exe.linkSystemLibrary("vorbis");
    exe.linkSystemLibrary("ogg");
    exe.linkSystemLibrary("mad");
    exe.linkSystemLibrary("GL");
    exe.linkSystemLibrary("SDL2");
    exe.linkLibrary(libminizip);
    exe.root_module.sanitize_c = false;

    b.installArtifact(exe);
}
