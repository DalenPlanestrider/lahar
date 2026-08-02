/* Repeated setup/teardown test.
 *
 * Runs the full lahar lifecycle (init -> window -> build -> pipeline ->
 * render a few frames -> deinit) several times in a row within one process.
 * This mimics what a game does when it rebuilds the renderer to apply new
 * graphics settings mid-play.
 *
 * LAHAR_NO_AUTO_DEP is defined so that the app owns GLFW and the window:
 * the window survives across lahar teardown/setup cycles instead of being
 * destroyed by lahar_deinit(), which is what a real game would want.
 */

#define LAHAR_USE_GLFW
#define LAHAR_NO_AUTO_DEP
#define LAHAR_IMPLEMENTATION
#include "../lahar.h"

// Triangle shader spirv
#include "../shaders.h"

#define NUM_CYCLES 5
#define FRAMES_PER_CYCLE 30

#define tassert(cond, msg) if (!(cond)) { printf("\tAssert failed: %s\n", msg); return false; }

static uint32_t create_pipeline(LaharWindow* window, VkPipeline* pipeline, VkPipelineLayout* layout) {
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

    return lahar_shader_build(&info, pipeline, layout);
}

static bool run_cycle(GLFWwindow* window, uint64_t cycle) {
    uint32_t err = LAHAR_ERR_SUCCESS;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;

    if ((err = lahar_init())) {
        printf("\tCycle %lu: lahar_init failed: %s\n", cycle, lahar_err_name(err));
        return false;
    }

    lahar_builder_request_validation_layers();

    if ((err = lahar_builder_window_register(window, LAHAR_WINPROF_COLOR))) {
        printf("\tCycle %lu: window register failed: %s\n", cycle, lahar_err_name(err));
        return false;
    }

    if ((err = lahar_builder_extension_add_required_device(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME))) {
        printf("\tCycle %lu: extension add failed: %s\n", cycle, lahar_err_name(err));
        return false;
    }

    lahar_builder_request_command_buffers();

    if ((err = lahar_build())) {
        printf("\tCycle %lu: lahar_build failed: %s\n", cycle, lahar_err_name(err));
        return false;
    }

    tassert(lahar->vkresult == VK_SUCCESS, "vkresult not VK_SUCCESS after build");
    tassert(lahar->instance != VK_NULL_HANDLE, "instance is null after build");
    tassert(lahar->device != VK_NULL_HANDLE, "device is null after build");

    const LaharWindowState* winstate = lahar_window_state(window);
    tassert(winstate, "window state is null after build");
    tassert(winstate->swapchain != VK_NULL_HANDLE, "swapchain is null after build");

    if ((err = create_pipeline(window, &pipeline, &layout))) {
        printf("\tCycle %lu: pipeline build failed: %s\n", cycle, lahar_err_name(err));
        return false;
    }

    for (uint64_t frame = 0; frame < FRAMES_PER_CYCLE; frame++) {
        glfwPollEvents();

        if ((err = lahar_window_frame_begin(window))) {
            printf("\tCycle %lu: frame_begin failed: %s\n", cycle, lahar_err_name(err));
            return false;
        }

        VkCommandBuffer cmd = lahar_window_command_buffer(window);
        tassert(cmd != VK_NULL_HANDLE, "command buffer is null");

        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };

        vkResetCommandBuffer(cmd, 0);
        vkBeginCommandBuffer(cmd, &begin_info);

        lahar_cmd_attachment_transition(cmd, window, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        lahar_cmd_begin_rendering(cmd, window);

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

        if ((err = lahar_window_submit(window, cmd))) {
            printf("\tCycle %lu: submit failed: %s\n", cycle, lahar_err_name(err));
            return false;
        }

        if ((err = lahar_window_present(window))) {
            printf("\tCycle %lu: present failed: %s\n", cycle, lahar_err_name(err));
            return false;
        }
    }

    lahar_window_wait_inactive(window);

    vkDestroyPipeline(lahar->device, pipeline, lahar->vkalloc);
    vkDestroyPipelineLayout(lahar->device, layout, lahar->vkalloc);

    // Tears down the swapchain, device, and instance, but with
    // LAHAR_NO_AUTO_DEP the window itself is left alone
    lahar_deinit();

    return true;
}

int main() {
    // We own GLFW and the window; they persist across all cycles
    if (!glfwInit()) {
        printf("GLFW failed to init\n");
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* window = glfwCreateWindow(640, 480, "Cycle Test", NULL, NULL);
    if (!window) {
        printf("GLFW failed to create the window\n");
        glfwTerminate();
        return 1;
    }

    for (uint64_t i = 0; i < NUM_CYCLES; i++) {
        if (run_cycle(window, i)) {
            printf("Cycle %lu passed\n", i + 1);
        }
        else {
            printf("Cycle %lu failed\n", i + 1);
            glfwDestroyWindow(window);
            glfwTerminate();
            return 1;
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    printf("All %d setup/teardown cycles passed\n", NUM_CYCLES);
    return 0;
}
