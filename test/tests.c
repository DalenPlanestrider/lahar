#define LAHAR_IMPLEMENTATION
#include "../lahar.h"

#include <stdlib.h>
#include <unistd.h>

/* Shader dir is resolved from this file's own location where possible, so the
 * tests run from any working directory and any checkout path. Override with the
 * LAHAR_TEST_SHADER_DIR env var, or pass a dir as argv[1]. */
/* Which shader pair to exercise. "nasty" is the stress case: many vars, odd
 * types, and interleaved decorations. */
static const char* shader_name = "nasty";

static char shader_vert_buf[1024];
static char shader_frag_buf[1024];

const char* shader_vert = shader_vert_buf;
const char* shader_frag = shader_frag_buf;

/* Strip the trailing filename off __FILE__ to get the dir holding this source */
static void shader_paths_init(const char* dir_override) {
    char dir[900];

    if (dir_override) {
        snprintf(dir, sizeof(dir), "%s", dir_override);
    }
    else {
        const char* env = getenv("LAHAR_TEST_SHADER_DIR");

        if (env) {
            snprintf(dir, sizeof(dir), "%s", env);
        }
        else {
            /* __FILE__ is only absolute if the compiler was invoked with an
             * absolute path, so fall back to searching upward from the cwd for a
             * test/shader dir. Covers building from the repo root, from build/,
             * or from an IDE's out-of-tree dir. */
            const char* file = __FILE__;
            const char* slash = strrchr(file, '/');

            dir[0] = '\0';

            if (slash && file[0] == '/') {
                const int len = (int)(slash - file);
                snprintf(dir, sizeof(dir), "%.*s/shader", len, file);
            }

            if (!dir[0] || access(dir, R_OK) != 0) {
                static const char* candidates[] = {
                    "test/shader",
                    "shader",
                    "../test/shader",
                    "../../test/shader",
                    "../shader",
                };

                for (uint64_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
                    if (access(candidates[i], R_OK) == 0) {
                        snprintf(dir, sizeof(dir), "%s", candidates[i]);
                        break;
                    }
                }
            }

            if (!dir[0]) {
                printf("Could not locate the test shader dir. Pass it as argv[1], or set LAHAR_TEST_SHADER_DIR.\n");
                snprintf(dir, sizeof(dir), "test/shader");
            }
        }
    }

    snprintf(shader_vert_buf, sizeof(shader_vert_buf), "%s/%s_vert.spv", dir, shader_name);
    snprintf(shader_frag_buf, sizeof(shader_frag_buf), "%s/%s_frag.spv", dir, shader_name);

    printf("Shader dir: %s (pair: %s)\n", dir, shader_name);
}

#define tassert(cond, msg) if (!(cond)) { printf("\tAssert failed: %s\n", msg); return false; }

bool read_file(const char* path, uint8_t** mem, uint64_t* size) {
    FILE* file = fopen(path, "r");
    if (!file) { return false; }

    fseek(file, 0, SEEK_END);
    uint64_t file_size = ftell(file);

    if (size) {
        *size = file_size;
    }

    if (mem) {
        uint8_t* b = malloc(file_size);
        if (!b) { fclose(file); return false; }

        fseek(file, 0, SEEK_SET);
        fread(b, 1, file_size, file);

        *mem = b;
    }

    fclose(file);

    return true;
}

bool test1() {
    uint8_t* file;
    uint64_t size;

    tassert(read_file(shader_vert, &file, &size), "Failed to read file");
    tassert(file && size, "Invalid results");

    free(file);

    return true;
}

bool test2() {
    uint8_t* vert, *frag;
    uint64_t vert_size, frag_size;

    read_file(shader_vert, &vert, &vert_size);
    read_file(shader_frag, &frag, &frag_size);

    LaharShaderStage stages[] = {
        {
            .code = vert,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .length = vert_size 
        },
        {
            .code = frag,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .length = frag_size
        }
    };

    uint32_t num_infos = 0;
    LaharShaderVarInfo* infos = NULL;
    uint32_t err = lahar_shader_introspect(stages, 2, &num_infos, infos);

    tassert(err == LAHAR_ERR_SUCCESS, "Failed to count vars");

    infos = malloc(num_infos * sizeof(*infos));
    err = lahar_shader_introspect(stages, 2, &num_infos, infos);

    tassert(err == LAHAR_ERR_SUCCESS, "Failed to output vars");

    lahar_shader_introspection_print(infos, num_infos);

    free(infos);
    free(vert);
    free(frag);

    return err == LAHAR_ERR_SUCCESS;
}

bool test3() {
    uint8_t* vert, *frag;
    uint64_t vert_size, frag_size;

    read_file(shader_vert, &vert, &vert_size);
    read_file(shader_frag, &frag, &frag_size);

    tassert(vert_size % sizeof(uint32_t) == 0, "vert wrong size");
    tassert(frag_size % sizeof(uint32_t) == 0, "frag wrong size");

    LaharShaderStage stages[] = {
        {
            .code = vert,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .length = vert_size 
        },
        {
            .code = frag,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .length = frag_size 
        },
    };

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;

    LaharShaderBuilder info = {0};
    info.force_dynamic = true;

    lahar_shader_builder_set_stages(&info, stages, 2);
    lahar_shader_builder_set_formats(&info, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED);
    lahar_shader_builder_set_blend_states(&info, (VkPipelineColorBlendAttachmentState[]){ {0} }, 1);
    lahar_shader_builder_set_vertex_input(&info, 
        (VkVertexInputAttributeDescription[]){
            {
                .binding = 0,
                .location = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = 0
            },
            {
                .binding = 0,
                .location = 1,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = 12
            }
        },
        (VkVertexInputBindingDescription[]){
            {
                .binding = 0,
                .stride = 20,
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
            }
        }, 
        2,
        1
    );

    bool success = lahar_shader_build(&info, &pipeline, &layout) == LAHAR_ERR_SUCCESS;

    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(lahar->device, pipeline, lahar->vkalloc);
    }

    if (layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(lahar->device, layout, lahar->vkalloc);
    }

    return success;
}

int main(int argc, char** argv) {
    /* argv[1], if given, is the dir holding the .spv files */
    shader_paths_init(argc > 1 ? argv[1] : NULL);

    uint32_t err;

    if ((err = lahar_init())) {
        printf("Lahar failed to init: %s\n", lahar_err_name(err));
        return 1;
    }

    lahar_builder_set_debug_level(LAHAR_DEBUG_TRACE);
    lahar_builder_request_validation_layers();
    lahar_builder_extension_add_required_device(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);

    if ((err = lahar_build())) {
        printf("Lahar failed to build: %s\n", lahar_err_name(err));
        return 1;
    }

    bool (*tests[])() = {
        test1,
        test2,
        test3,
    };

    for (uint64_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        if (tests[i]()) {
            printf("Test %lu passed\n", i + 1);
        }
        else {
            printf("Test %lu failed\n", i + 1);
        }
    }

    lahar_deinit();
    return 0;
}
