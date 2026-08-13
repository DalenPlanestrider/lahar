# Lahar

Lahar is a minimal, un-opinionated pure C header-only utility for simplifying
starting Vulkan renderer projects. The objective is to get you to drawing
faster, without over-abstracting or getting in your way. Nothing is hidden,
everything is accessible, but the most repetitive and similar aspects of many
apps can be handled for you so you can focus on the aspects that make your
application unique.

The library is targeted towards developers with at least some experience with
Vulkan. This is because Lahar does not manage quite a few Vulkan objects
intentionally, leaving them fully as your responsibility. Those new to Vulkan
may find some benefit in the brevity, but may struggle if they run into edge
cases that Lahar does not handle automatically.

## Requirements
* Vulkan 1.0 or later. You do **not** need to link against the Vulkan loader
  library — Lahar includes a volk-style function loader — but you do need the
  Vulkan headers available at compile time.
* A C99 compiler, or any C++ compiler whose standard can consume C99 headers
  (in practice: all of them).
* No other dependencies. Window libraries (GLFW, SDL2/3) and VMA are optional
  integrations, not requirements.

## Features
* Self-contained volk-style Vulkan loader, no other dependencies required.
* Manages the entire instance set up and device selection process, with
  configurable options. Device selection uses a simple scoring function by
  default; you can supply your own scoring callback via
  `lahar_builder_device_set_scoring` for full control.
* Optionally creates your window surfaces and attachments. Multiple windows
  are supported (though less battle-tested than single-window use). You can
  also skip windows entirely and use Lahar purely as a loader + instance/device
  bootstrapper.
* Has some utilities to automate tedious tasks like submission, presentation,
  layout transitions, dynamic rendering, and shader creation. All of these are
  opt-in; only the loader and instance/device setup are inseparable from Lahar
  itself.
* Self-contained SPIR-V introspection interface. This is not a
  "hand me a cursor" reflection API: you hand it all of your stages and it
  performs full std140/std430 resolution, producing a complete merged pipeline
  layout — or the pipeline itself — in one shot. Hooks for adding support for
  non-SPIR-V languages as well.
* Integration with popular window libraries like GLFW, SDL2/3, or bring your
  own window implementation (define `LAHAR_CUSTOM_WINDOW` and implement a
  small set of `lahar_window_*` functions).
* Integration with VMA for the bit of allocation it needs to do, or bring your
  own allocator by filling out the `LaharAllocator` vtable.
* A simple Vulkan tlsf freelist allocator as the default GPU allocator. Lahar
  itself only uses it for one thing: allocating window attachments other than
  color (color-only setups never touch it). Beyond that, treat it as a utility
  your application is free to use — or ignore.
* Compiles without issue in a C++ environment.
* Safe to set up and tear down repeatedly within a single process, e.g. for
  applying graphics settings changes mid-play.

## Things Outside Lahar's Scope
* Asset, scene, and lifetime management.
* Command buffer recording, textures, render passes.
* Simplifying or hiding Vulkan concepts.
* CPU-side synchronization.

## Installation
Lahar is header-only. Simply include `lahar.h` and define `LAHAR_IMPLEMENTATION`
in exactly one source file before the include.

Configuration is done with `#define`s before the include. The full list lives
in the preamble of `lahar.h`; the highlights:

* `LAHAR_USE_GLFW`, `LAHAR_USE_SDL2`, `LAHAR_USE_SDL3` — pick your windowing
  integration, or `LAHAR_CUSTOM_WINDOW` to roll your own.
* `LAHAR_USE_VMA` — use VMA as the GPU allocator (also auto-detected if
  `vk_mem_alloc.h` was included first).
* `LAHAR_NO_AUTO_DEPS` — by default, Lahar initializes and tears down its
  third-party dependencies (e.g. `glfwInit`/`glfwTerminate`) for you, and takes
  ownership of registered windows. Define this if you'd rather manage them
  yourself.
* `lahar_malloc`/`lahar_realloc`/`lahar_free` — redirect Lahar's host memory
  usage.

### Coexisting with other loaders
Lahar has no mechanism to adopt externally-loaded Vulkan functions; the loader
and instance/device setup are the two non-separable pieces. It can coexist
with volk in the same process — each will use its own function pointers — but
there is little reason to run both.

## Error Handling
Every Lahar function returns a `lahar_err` code, without exception. If that
code is `LAHAR_ERR_VK_ERR`, it means a Vulkan operation failed generically;
inspect `lahar->vkresult` if you want the underlying `VkResult`. Swapchain
out-of-date conditions are never reported as a generic `LAHAR_ERR_VK_ERR` —
see below.

## Windows, Frames, and Synchronization
* `lahar_builder_window_register` takes a window profile.
  `LAHAR_WINPROF_COLOR` gives you a color-only setup;
  `LAHAR_WINPROF_COLOR_DEPTH_STENCIL` adds a depth+stencil attachment. For
  anything fancier, `lahar_builder_window_register_ex` accepts a full config
  with explicit attachment descriptions. Attachment index 0 is always the
  color attachment; indices after that are whatever you configured.
* `lahar_window_frame_begin` acquires the next swapchain image, performs the
  appropriate fence waits/resets, and advances an internal per-window state
  machine that prevents you from calling frame functions out of order. It is
  only required if you plan to use `lahar_window_submit`/
  `lahar_window_present` — manual submission remains fully possible.
* GPU-side synchronization (image-available/render-finished semaphores and
  in-flight fences) is managed internally. The maximum number of frames in
  flight is configurable per window (default: 2).
* If the swapchain goes out of date (typically a resize), Lahar's default
  behavior is to recreate it for you automatically — the explicit resize
  callback in the example below is technically overkill. If you disable auto
  resizing, you get `LAHAR_ERR_SWAPCHAIN_OUT_OF_DATE` back and handle it
  yourself.

## Ownership and Cleanup
* Unless `LAHAR_NO_AUTO_DEPS` is defined, Lahar takes ownership of registered
  windows and destroys them (and tears down the window library) during
  `lahar_deinit()`. With `LAHAR_NO_AUTO_DEPS`, the window remains yours.
* The shader/pipeline builders are utilities: **you own their output**. Any
  pipelines, layouts, or other Vulkan objects you create — via Lahar utilities
  or otherwise — must be destroyed before `lahar_deinit()`.

## Getting Started
The following example demonstrates using dynamic rendering with a basic
color-only window in Lahar

```c
#define LAHAR_USE_GLFW
#define LAHAR_IMPLEMENTATION
#include "lahar.h"

// Just the shader spirv code
#include "shaders.h"

#include <time.h>

// 16.6 ms in ns, ~60 fps
#define MIN_FRAME_NS 16666667

VkPipeline pipeline = VK_NULL_HANDLE;
VkPipelineLayout layout = VK_NULL_HANDLE;


uint32_t create_pipeline(LaharWindow* window);
uint64_t time_ns(void);
void sleep_ns(uint64_t ns);

int main() {
    uint32_t err = LAHAR_ERR_SUCCESS;

    // Note: no glfwInit() needed. Since LAHAR_NO_AUTO_DEPS isn't defined,
    // lahar_init() initializes GLFW for us, and lahar_deinit() tears it down.
    if ((err = lahar_init())) {
        printf("Lahar failed to init: %s\n", lahar_err_name(err));
        return 1;
    }

    lahar_builder_request_validation_layers();

    GLFWwindow* window = glfwCreateWindow(800, 600, "Test", NULL, NULL);
    if (!window)  {
        printf("GLFW failed to create the window\n");
        return 1;
    }

    // Note: no resize handling needed. By default, lahar detects an
    // out-of-date swapchain during frame_begin/present and recreates it
    // automatically. (Disable via the window config's no_auto_swap_resize
    // to handle LAHAR_ERR_SWAPCHAIN_OUT_OF_DATE yourself.)

    // Window management is optional, you can use lahar like a vk-bootstrap/volk replacement alone, if desired 
    // Also, the window you use is up to you. Native support for glfw and sdl3. Plus an api for defining custom
    // windows if you're rolling your own
    if ((err = lahar_builder_window_register(window, LAHAR_WINPROF_COLOR))) {
        printf("Lahar window failed to register: %s\n", lahar_err_name(err));
        return 1;
    }

    err = lahar_builder_extension_add_required_device(
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
    );

    if (err) {
        printf("Lahar failed to append required extension: %s\n", lahar_err_name(err));
        return 1;
    }

    // Opt into having lahar make a primary buffer per swapchain image
    // You can make your own after build, if you prefer
    lahar_builder_request_command_buffers();

    if ((err = lahar_build())) {
        printf("Lahar failed to build: %s\n", lahar_err_name(err));

        // Every lahar function returns a lahar_err code. LAHAR_ERR_VK_ERR
        // means "a vulkan call failed"; the raw VkResult is available if
        // you want the details:
        if (err == LAHAR_ERR_VK_ERR) {
            printf("VkResult: %d\n", lahar->vkresult);
        }

        return 1;
    }

    // If you need to access the internal state of lahar, it's available as a global pointer
    // this is where you'll find the device and other such useful things

    // The window state contains anything specific to the window, such
    // as attachments, the swapchain, the index of the current frame, and so on
    const LaharWindowState* winstate = lahar_window_state(window);

    /* Optional pipeline setup utilities are available */
    if ((err = create_pipeline(window))) {
        return 1;
    }

    while (!glfwWindowShouldClose(window)) {
        const uint64_t start = time_ns();

        glfwPollEvents();

        // Acquires the image and does the fence waits/resets for you.
        // Only needed because we use lahar_window_submit/present below.
        lahar_window_frame_begin(window);

        VkCommandBuffer cmd = lahar_window_command_buffer(window);

        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };

        vkResetCommandBuffer(cmd, 0);
        vkBeginCommandBuffer(cmd, &begin_info);

        /* These are utility functions that just automate format transitions for you, entirely optional.
         * Attachment index 0 is always the color attachment. */
        lahar_cmd_attachment_transition(cmd, window, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        /* Again, optional util for dynamic rendering */
        lahar_cmd_begin_rendering(cmd, window);

        /* Just do normal vulkan draw stuff here */

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        VkRect2D scissor = {
            .extent = {winstate->width, winstate->height},
            .offset = {0, 0}
        };

        VkViewport viewport = {
            .height = (float)winstate->height,
            .width = (float)winstate->width,
            .maxDepth = 1.0f
        };

        vkCmdSetScissor(cmd, 0, 1, &scissor);
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdEndRendering(cmd);

        lahar_cmd_attachment_transition(cmd, window, 0, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        vkEndCommandBuffer(cmd);

        /* Similar story for both submission and present, fully optional, one can do it manually */
        lahar_window_submit(window, cmd);
        lahar_window_present(window);

        const uint64_t end = time_ns();
        const uint64_t elapsed = end - start;

        if (elapsed < MIN_FRAME_NS) {
            sleep_ns(MIN_FRAME_NS - elapsed);
        }
    }

    lahar_window_wait_inactive(window);

    // Any Vulkan objects you created you must destroy before calling lahar deinit.
    // This includes the output of lahar's shader utilities: they're utilities,
    // you own what they produce.
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(lahar->device, pipeline, lahar->vkalloc);
    }

    if (layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(lahar->device, layout, lahar->vkalloc);
    }

    // Destroys the window (lahar owns it, since LAHAR_NO_AUTO_DEPS isn't
    // defined) and terminates GLFW as well.
    lahar_deinit();

    return 0;
}

uint32_t create_pipeline(LaharWindow* window) {
    /* Shaders work via builder. They default to sane values,
     * and have an API for easy settings, like quickly setting
     * dynamic values, culling, or blend modes, for example. 
     * You can also pass VkPipelineLayoutCreate values directly, 
     * if need be. There's hooks to handle compilation if you'd
     * like to support non-spirv languages.
     *
     * A spir-v reflection implementation is included. Hand it
     * all your stages and it resolves the full merged layout
     * for you -- no manual descriptor bookkeeping needed.
     */

    LaharShaderBuilder info = {0};

    lahar_shader_builder_set_window(&info, window);

    lahar_shader_builder_add_stage(&info, &(LaharShaderStage){
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .code = triangle_vert_spv,
        .length = triangle_vert_spv_len
    });

    lahar_shader_builder_add_stage(&info, &(LaharShaderStage){
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .code = triangle_frag_spv,
        .length = triangle_frag_spv_len
    });

    lahar_shader_builder_set_cull_mode(&info, LAHAR_SCM_OFF);

    return lahar_shader_build(&info, &pipeline, &layout);
}

uint64_t time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

void sleep_ns(uint64_t ns) {
    struct timespec ts = {
        .tv_sec  = ns / 1000000000ULL,
        .tv_nsec = ns % 1000000000ULL
    };
    nanosleep(&ts, NULL);
}
```

## Documentation
The authoritative source of Lahar's documentation is the header itself. See the
preamble at the beginning for the overview, as well as compile-time
configuration options. Every public function has doc comments.

## License
Lahar is released under the zlib license
