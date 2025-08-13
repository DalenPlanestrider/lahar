# Lahar

Lahar is a minimal, un-opinionated pure C header-only utility for simplifying starting Vulkan renderer projects. The objective is to get you to drawing faster, without over-abstracting or getting in your way. Nothing is hidden, everything is accessible, but the most repetitive and similar aspects of many apps can be handled for you so you can focus on the aspects that make your application unique.

The library is targeted towards developers with at least some experience with Vulkan. This is because Lahar does not manage quite a few Vulkan objects intentionally, leaving them fully as your responsibility. Those new to Vulkan may find some benefit in the brevity, but may struggle if they run into edge cases that Lahar does not handle automatically.

## Features
* Self-contained Vulkan loader, no other dependencies required
* Manages the entire instance set up and device selection process, with configurable options
* Optionally creates your window surfaces and attachments
* Has some utilities to automate tedious tasks like submission, presentation, and layout transitions
* Integration with popular window libraries like GLFW, SDL2/3, or bring your own window implementation
* Integration with VMA for the bit of allocation it needs to do, or bring your own allocator
* Compiles without issue in a C++ environment

## Things Outside Lahar's Scope
* Vulkan memory management
* Asset, scene, and lifetime management
* Shaders/pipelines, command buffers, textures, render passes 
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

Lahar instance;
Lahar* lahar = &instance;
bool window_out_of_date = false;

void window_resize(GLFWwindow* window, int width, int height) {
    window_out_of_date = true;
}

int main() {
    uint32_t err;

    if ((err = lahar_init(lahar))) {
        printf("Lahar failed to init: %s\n", lahar_err_name(err));
        return 1;
    }

    lahar_builder_request_validation_layers(lahar);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Test", NULL, NULL);
    if (!window)  {
        printf("GLFW failed to create the window\n");
        return 1;
    }

    glfwSetWindowSizeCallback(window, window_resize);

    /* Window management is optional, you can use lahar like a
       vk-bootstrap/volk replacement alone, if desired. Also, the
       window you use is up to you. Native support for glfw and sdl3.
       Plus an api for defining custom windows if you're rolling your own
    */
    if ((err = lahar_builder_window_register(lahar, window, LAHAR_WINPROF_COLOR))) {
        printf("Lahar window failed to register: %s\n", lahar_err_name(err));
        return 1;
    }

    err = lahar_builder_extension_add_required_device(
        lahar,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
    );

    if (err) {
        printf("Lahar failed to append required extension: %s\n", lahar_err_name(err));
        return 1;
    }

    // Opt into having lahar make a primary buffer per swapchain image
    // You can make your own after build, if you prefer
    lahar_builder_request_command_buffers(lahar);

    if ((err = lahar_build(lahar))) {
        printf("Lahar failed to build: %s\n", lahar_err_name(err));
        return 1;
    }

    LaharWindowState* winstate = lahar_window_state(lahar, window);

    /* Pipeline setup or whatever */

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        if (window_out_of_date) {
            if ((err = lahar_window_swapchain_resize(lahar, window))) {
                printf("Lahar failed to resize swapchain: %s\n", lahar_err_name(err));
            }
        }

        lahar_window_frame_begin(lahar, window);

        VkCommandBuffer cmd = winstate->commands[winstate->frame_index];

        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };

        vkResetCommandBuffer(cmd, 0);
        vkBeginCommandBuffer(cmd, &begin_info);

        /* These are utility functions that just automate
           format transitions for you, entirely optional */
        lahar_window_attachment_transition(
            lahar,
            window,
            LAHAR_ATT_COLOR_INDEX,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            cmd
        );

        /* Just do normal vulkan draw stuff here */

        lahar_window_attachment_transition(
            lahar,
            window,
            LAHAR_ATT_COLOR_INDEX,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            cmd
        );

        vkEndCommandBuffer(cmd);

        /* Similar story for both submission and present,
           fully optional, one can do it manually */
        lahar_window_submit(lahar, window, cmd);
        lahar_window_present(lahar, window);
    }

    // Any Vulkan objects you created you must destroy before calling lahar deinit
    lahar_deinit(lahar);

    return 0;
}
```

## Documentation
The authoritative source of Lahar's documentation is the header itself. See the preamble at the beginning for the overview, as well as compile-time configuration options. Every public function has doc comments.

## License
Lahar is released under the zlib license
