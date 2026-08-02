# Lahar

Lahar is a minimal, un-opinionated pure C header-only utility for simplifying starting Vulkan renderer projects. The objective is to get you to drawing faster, without over-abstracting or getting in your way. Nothing is hidden, everything is accessible, but the most repetitive and similar aspects of many apps can be handled for you so you can focus on the aspects that make your application unique.

The library is targeted towards developers with at least some experience with Vulkan. This is because Lahar does not manage quite a few Vulkan objects intentionally, leaving them fully as your responsibility. Those new to Vulkan may find some benefit in the brevity, but may struggle if they run into edge cases that Lahar does not handle automatically.

## Features
* Self-contained Vulkan loader, no other dependencies required
* Manages the entire instance set up and device selection process, with configurable options
* Optionally creates your window surfaces and attachments
* Has some utilities to automate tedious tasks like submission, presentation, layout transitions, dynamic rendering, and shader creation
* Self contained SPIR-V introspection interface
* Integration with popular window libraries like GLFW, SDL2/3, or bring your own window implementation
* Integration with VMA for the bit of allocation it needs to do, or bring your own allocator
* A dirt simple linear-walk freelist allocator as a fallback
* Compiles without issue in a C++ environment
* Safe to set up and tear down repeatedly within a single process, e.g. for applying graphics settings changes mid-play

## Things Outside Lahar's Scope
* Asset, scene, and lifetime management
* command buffer recording, textures, render passes
* Simplifying or hiding Vulkan concepts

## Installation
Lahar is header-only. Simply include `lahar.h` and define `LAHAR_IMPLEMENTATION` 
in exactly one source file before the include.

## Getting Started
The following example demonstrates using dynamic rendering with a basic color-only window in Lahar

```c
#define LAHAR_USE_GLFW
#define LAHAR_IMPLEMENTATION
#include "lahar.h"

// Just the shader spirv code
#include "shaders.h"

#include <time.h>

// 16.6 ms in ns, ~60 fps
#define MIN_FRAME_NS 16666667

bool window_out_of_date = false;
VkPipeline pipeline = VK_NULL_HANDLE;
VkPipelineLayout layout = VK_NULL_HANDLE;


uint32_t create_pipeline(LaharWindow* window);
uint64_t time_ns(void);
void sleep_ns(uint64_t ns);

void window_resize(GLFWwindow* window, int width, int height) {
    window_out_of_date = true;
}

int main() {
    uint32_t err = LAHAR_ERR_SUCCESS;

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

    glfwSetWindowSizeCallback(window, window_resize);

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
        return 1;
    }

    // If you need to access the internal state of lahar, it's available as a global pointer
    // this is where you'll find the device and other such useful things
    if (lahar->vkresult != VK_SUCCESS) {
        return 1;
    }

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

        if (window_out_of_date) {
            if ((err = lahar_window_swapchain_resize(window))) {
                printf("Lahar failed to resize swapchain: %s\n", lahar_err_name(err));
            }

            window_out_of_date = false;
        }

        lahar_window_frame_begin(window);

        VkCommandBuffer cmd = lahar_window_command_buffer(window);

        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };

        vkResetCommandBuffer(cmd, 0);
        vkBeginCommandBuffer(cmd, &begin_info);

        /* These are utility functions that just automate format transitions for you, entirely optional */
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

    // Any Vulkan objects you created you must destroy before calling lahar deinit
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(lahar->device, pipeline, lahar->vkalloc);
    }

    if (layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(lahar->device, layout, lahar->vkalloc);
    }

    lahar_deinit();

    return 0;
}

uint32_t create_pipeline(LaharWindow* window) {
    /* Shaders work via builder. They default to sane values,
     * and have an API for easy settings, like quickly setting
     * dynamic values, culling, or blend modes, for example. 
     * You can also pass VkPiplineLayoutCreate values directly, 
     * if need be. There's hooks to handle compilation if you'd
     * like to suport non-spirv languages.
     *
     * A spir-v reflection implementation is included.
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
The authoritative source of Lahar's documentation is the header itself. See the preamble at the beginning for the overview, as well as compile-time configuration options. Every public function has doc comments.

## License
Lahar is released under the zlib license
