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

    /* Window management is optional, you can use lahar like a vk-bootstrap/volk replacement alone, if desired 
       Also, the window you use is up to you. Native support for glfw and sdl3. Plus an api for defining custom
       windows if you're rolling your own
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

        /* These are utility functions that just automate format transitions for you, entirely optional */
        lahar_window_attachment_transition(lahar, window, LAHAR_ATT_COLOR_INDEX, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, cmd);

        /* Just do normal vulkan draw stuff here */

        lahar_window_attachment_transition(lahar, window, LAHAR_ATT_COLOR_INDEX, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, cmd);

        vkEndCommandBuffer(cmd);

        /* Similar story for both submission and present, fully optional, one can do it manually */
        lahar_window_submit(lahar, window, cmd);
        lahar_window_present(lahar, window);
    }

    // Any Vulkan objects you created you must destroy before calling lahar deinit
    lahar_deinit(lahar);

    return 0;
}
