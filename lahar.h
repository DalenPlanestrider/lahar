/* Lahar is licensed under the permissive zlib license. See the bottom of the file for details. */

#ifndef LAHAR_H
#define LAHAR_H

/*

General use:

Lahar is a library for simplifying Vulkan setup. Think of it as taking the place
of volk and vk-bootstrap, and optionally offering the ability to manage your windows
and presentation, if you'd like. If you're not using it for that, however, all the
internals are fully exposed for you to use as you see fit.

The general flow is:

1. (optional) #define your window lib of choice, for example, #define LAHAR_USE_GLFW

2. #include <lahar.h>

3. Call lahar_init()

4. Use the suite of lahar_builder_* configuration functions to configure what you want (or poke
   the struct internals, they're fairly well commented)

5. Use lahar_build()

6. Your normal vulkan flow. Set up pipelines, enter render loop, etc

7. (optional) lahar has some utilities to simplify command submission, window presentation,
   layout transitions, and pipleline creation, if you want.



Compile-Time Configuration options:

    LAHAR_NO_AUTO_DEPS
        If defined, third party dependencies like GLFW will _not_ be initialized or
        cleaned up by lahar

    LAHAR_NO_AUTO_INCLUDE
        If defined, third party includes like SDL will not be included automatically.

    LAHAR_M_ARENA_SIZE [positive integer]
        For some operations, lahar uses a fixed size arena. If for some reason your
        environment is exhausting that arena, it will trip an assertion. You can
        fix this by enlarging the arena. Default: 65kb

    LAHAR_MAX_DEVICE_ENTRIES [positive integer]
        This determines how many surface formats/present modes a device can
        have associated with it. Default: 16

    LAHAR_MAX_SHADER_STAGES [positive integer]
        This controls how many stages a shader can include. Default: 8

    LAHAR_MAX_ATTACHMENTS [positive integer]
        This controls how many attachments a window can have. Default: 8

    LAHAR_MAX_SHADER_COMPILERS [positive integer]
        The maximum number of shader compilers you can register. Default: 4

    LAHAR_CUSTOM_WINDOW [type without pointer]
        If you need to support a custom window interface. You must _also_ implement
        the functions. If you don't, you'll get linker errors.
             lahar_window_surface_create
             lahar_window_get_size
             lahar_window_get_extensions

    LAHAR_C_LINKAGE
        Lahar is designed to be safely compiled in a C or C++ context. But if for some
        reason you need to avoid the mangling, you can define LAHAR_C_LINKAGE to get
        the header functions declared as extern "C"

    LAHAR_USE_GLFW
        Use GLFW as your windowing library

    LAHAR_USE_SDL2
        Use SDL2 as your windowing library

    LAHAR_USE_SDL3
        Use SDL3 as your windowing library

    LAHAR_USE_VMA
        Use VMA as your allocator. May only work in a C++ context.

    LAHAR_DEFAULT_VK_VERSION
        Used when you don't specify a vulkan version as the fallback. Default: 1.3

    LAHAR_IMPLEMENTATION
        Put the lahar implementation in this source file

    lahar_malloc
    lahar_realloc
    lahar_free
        A set of macros with the stdlib-like apis, for if you'd like to
        redirect lahar's memory usage

    LAHAR_DEFAULT_ALIGNMENT [positive integer expression] - default alignof(max_align_t)

    LAHAR_DEBUG_BREAK [expression] - default raise(SIGTRAP)

    LAHAR_DEBUG - if defined, enable some extra internal debug checks and assertions
*/











#ifndef __cplusplus
    #include <stdbool.h>
    #include <stdint.h>
    #include <string.h>
    #include <stdlib.h>
    #include <assert.h>
    #include <stdio.h>
    #include <stddef.h>

    #define LAHAR_ALIGNOF(T) _Alignof(T)
    #define lahar_static_assert(c, m) _Static_assert(c, m)
#else
    #include <cstdint>
    #include <cstring>
    #include <cstdlib>
    #include <cassert>
    #include <cstdio>
    #include <cstddef>

    #define LAHAR_ALIGNOF(T) alignof(T)
    #define lahar_static_assert(c, m) static_assert(c, m)
#endif

#if defined(VULKAN_H_) && !defined(VK_NO_PROTOTYPES)
    #error "Lahar manages vulkan for you! If you must include vulkan.h before lahar (e.g. via vk_mem_alloc.h), define VK_NO_PROTOTYPES first so it doesn't conflict with lahar's function loading"
#endif

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#if defined(AMD_VULKAN_MEMORY_ALLOCATOR_H) && !defined(LAHAR_USE_VMA)
    #define LAHAR_USE_VMA
#endif

#if defined(LAHAR_CUSTOM_WINDOW)
    #define LaharWindow LAHAR_CUSTOM_WINDOW
#elif defined(LAHAR_USE_GLFW)
    #if defined(LAHAR_NO_AUTO_INCLUDE) && defined(__cplusplus) && __STDC_VERSION__ >= 202311L
        #define LAHAR_GLFW_WARN 1
    #endif

    #include <GLFW/glfw3.h>
    #define LaharWindow GLFWwindow
#elif defined(LAHAR_USE_SDL2)
    #ifndef LAHAR_NO_AUTO_INCLUDE
        #include <SDL2/SDL.h>
        #include <SDL2/SDL_vulkan.h>
    #endif

    #define LaharWindow SDL_Window
#elif defined(LAHAR_USE_SDL3)
    #ifndef LAHAR_NO_AUTO_INCLUDE
        #include <SDL3/SDL.h>
        #include <SDL3/SDL_vulkan.h>
    #endif

    #define LaharWindow SDL_Window
#else
    #define LAHAR_WINDOW_WARN 1
    #define LaharWindow void
#endif
// TODO: investigate if Raylib integration is possible. They don't have a window struct,
// so I might have to poke around in their gutty works for that

#if defined(LAHAR_ALLOCATION_TYPE)
    #define LaharAllocation LAHAR_ALLOCATION_TYPE
#elif defined(LAHAR_USE_VMA)
    #if !defined(LAHAR_NO_AUTO_INCLUDE) && !defined(AMD_VULKAN_MEMORY_ALLOCATOR_H)
        #include <vk_mem_alloc.h>
    #endif

    #define LaharAllocation VmaAllocation
#else
    /* An opaque allocation handle. Allocator implementations point this at
     * their own internal bookkeeping for the allocation. NULL means "no
     * allocation", so zero-initialized state is always valid. */
    typedef void* LaharAllocation;
#endif

#if defined(_WIN32)
    #define LaharLibrary HMODULE
#else
    #define LaharLibrary void*
#endif

#ifndef LAHAR_MAX_DEVICE_ENTRIES
    #define LAHAR_MAX_DEVICE_ENTRIES 16
#endif

#ifndef LAHAR_MAX_SHADER_STAGES
    #define LAHAR_MAX_SHADER_STAGES 8
#endif

#ifndef LAHAR_MAX_ATTACHMENTS
    #define LAHAR_MAX_ATTACHMENTS 8
#endif

#ifndef LAHAR_MAX_SHADER_COMPILERS
    #define LAHAR_MAX_SHADER_COMPILERS 4
#endif

#ifndef LAHAR_MAX_WINDOW_RETIRES
    #define LAHAR_MAX_WINDOW_RETIRES 8
#endif

#define LAHAR_ERR_SUCCESS 0                             // All good in the neighborhood
#define LAHAR_ERR_ILLEGAL_PARAMS 0x00020001             // Wrong stuff for this function
#define LAHAR_ERR_LOAD_FAILURE 0x00020002               // We couldn't load vulkan
#define LAHAR_ERR_INVALID_CONFIGURATION 0x00020003      // You built a configuration that doesn't make sense
#define LAHAR_ERR_MISSING_EXTENSION 0x00020004          // Missing an extension we needed
#define LAHAR_ERR_NO_SUITABLE_DEVICE 0x00020005         // No device that fits our criteria
#define LAHAR_ERR_DEPENDENCY_FAILED 0x00020006          // A third party lib failed
#define LAHAR_ERR_ALLOC_FAILED 0x00020007               // We couldn't allocate dynamically
#define LAHAR_ERR_INVALID_STATE 0x00020008              // The internal state was invalid, this is a bug in lahar!
#define LAHAR_ERR_VK_ERR 0x00020009                     // A vulkan operation failed, see lahar.vkresult
#define LAHAR_ERR_INVALID_WINDOW 0x0002000A             // This isn't a window known to lahar
#define LAHAR_ERR_NO_COMMAND_BUFFER 0x0002000B          // You tried to present a frame without submitting any command buffers
#define LAHAR_ERR_TIMEOUT 0x0002000C                    // A wait operation timed out
#define LAHAR_ERR_SWAPCHAIN_OUT_OF_DATE 0x0002000D      // The swapchain needs updated
#define LAHAR_ERR_INVALID_FRAME_STATE 0x0002000E        // You did things out of order (must always be frame_start -> submit -> present)
#define LAHAR_ERR_ATTACHMENT_WO_ALLOCATOR 0x0002000F    // You requested non-color attachments for a window, but provided no allocator
#define LAHAR_ERR_UNKNOWN_LANGUAGE 0x00020010           // Don't know this shader language
#define LAHAR_ERR_MALFORMED_CODE 0x00020011             // This shader code is invalid
#define LAHAR_ERR_ID_NOT_FOUND 0x00020012               // Couldn't find data on this ID
#define LAHAR_ERR_INVALID_TYPE 0x00020013               // This type can't be used this way
#define LAHAR_ERR_COMPILATION_FAILED 0x00020014         // Compilation of the provided source failed
#define LAHAR_ERR_OUT_OF_SPACE 0x00020015               // A fixed buffer ran out of space, update your defines and recompile
#define LAHAR_ERR_MEMORY_UNSATISFIABLE 0x00020016       // You requested a type of GPU memory we don't have
#define LAHAR_ERR_VERSION_UNSATISFIABLE 0x00020017      // The vulkan version could not be satisfied by this loader or device
#define LAHAR_ERR_NOT_IMPLEMENTED 0x00020018            // Haven't gotten around to this yet

struct Lahar;
typedef struct Lahar Lahar;

struct LaharDeviceInfo;
typedef struct LaharDeviceInfo LaharDeviceInfo;

struct LaharAttachment;
typedef struct LaharAttachment LaharAttachment;

struct LaharWindowState;
typedef struct LaharWindowState LaharWindowState;

struct LaharWindowConfig;
typedef struct LaharWindowConfig LaharWindowConfig;

struct LaharAttachmentConfig;
typedef struct LaharAttachmentConfig LaharAttachmentConfig;

struct LaharAllocator;
typedef struct LaharAllocator LaharAllocator;

struct LaharShaderVarInfo;
typedef struct LaharShaderVarInfo LaharShaderVarInfo;

struct LaharShaderStage;
typedef struct LaharShaderStage LaharShaderStage;

struct LaharShaderBuilder;
typedef struct LaharShaderBuilder LaharShaderBuilder;

struct LaharShaderCompiler;
typedef struct LaharShaderCompiler LaharShaderCompiler;

struct LaharAllocationCreateInfo;
typedef struct LaharAllocationCreateInfo LaharAllocationCreateInfo;

#if !defined(__cplusplus)
enum LaharWindowProfile;
typedef enum LaharWindowProfile LaharWindowProfile;

enum LaharFramePhase;
typedef enum LaharFramePhase LaharFramePhase;

enum LaharDebugLevel;
typedef enum LaharDebugLevel LaharDebugLevel;

enum LaharShaderVarType;
typedef enum LaharShaderVarType LaharShaderVarType;

enum LaharShaderVarStorageClass;
typedef enum LaharShaderVarStorageClass LaharShaderVarStorageClass;

enum LaharShaderBlendMode;
typedef enum LaharShaderBlendMode LaharShaderBlendMode;

enum LaharShaderTopology;
typedef enum LaharShaderTopology LaharShaderTopology;

enum LaharShaderDepthTestMode;
typedef enum LaharShaderDepthTestMode LaharShaderDepthTestMode;

enum LaharShaderDepthWriteMode;
typedef enum LaharShaderDepthWriteMode LaharShaderDepthWriteMode;

enum LaharShaderDepthCompareOp;
typedef enum LaharShaderDepthCompareOp LaharShaderDepthCompareOp;

enum LaharShaderCullMode;
typedef enum LaharShaderCullMode LaharShaderCullMode;

enum LaharShaderDynamicFlags;
typedef enum LaharShaderDynamicFlags LaharShaderDynamicFlags;

enum LaharShaderFaceMode;
typedef enum LaharShaderFaceMode LaharShaderFaceMode;

enum LaharAttachmentRole;
typedef enum LaharAttachmentRole LaharAttachmentRole;

enum LaharMemoryUsage;
typedef enum LaharMemoryUsage LaharMemoryUsage;

enum LaharAllocationRole;
typedef enum LaharAllocationRole LaharAllocationRole;
#endif

enum LaharMemoryUsage {
    LAHAR_MU_DONT_KNOW = 0,
    LAHAR_MU_DEVICE_ONLY,
    LAHAR_MU_STAGING_SEQUENTIAL,
    LAHAR_MU_UPLOAD_DIRECT,
    LAHAR_MU_READBACK
};

enum LaharAllocationRole {
    LAHAR_AR_DONT_KNOW = 0,
    LAHAR_AR_VERTEX_BUFFER,
    LAHAR_AR_INDEX_BUFFER,
    LAHAR_AR_UNIFORM_BUFFER,
    LAHAR_AR_STORAGE_BUFFER,
    LAHAR_AR_INDIRECT_BUFFER,
    LAHAR_AR_STAGING_BUFFER,
    LAHAR_AR_QUERY_RESULT_BUFFER,
    LAHAR_AR_SHADER_BINDING_TABLE,
    LAHAR_AR_ACCELERATION_STRUCTURE,
    LAHAR_AR_DEVICE_ADDRESS,
    LAHAR_AR_TRANSFORM_FEEDBACK,
    LAHAR_AR_SAMPLED_IMAGE,
    LAHAR_AR_STORAGE_IMAGE,
    LAHAR_AR_TRANSFER_IMAGE,
    LAHAR_AR_COLOR_ATTACHMENT,
    LAHAR_AR_DEPTH_STENCIL_ATTACHMENT,
    LAHAR_AR_INPUT_ATTACHMENT,
    LAHAR_AR_TRANSIENT_ATTACHMENT,
    LAHAR_AR_OTHER_1,
    LAHAR_AR_OTHER_2,
    LAHAR_AR_OTHER_3,
    LAHAR_AR_OTHER_4,
    LAHAR_AR_OTHER_5,
};

struct LaharAllocationCreateInfo {
    LaharMemoryUsage usage;     // Determines the memory flags to use
    LaharAllocationRole role;   // Finer grained usage, optional
    VkFlags required_flags;     // flags the allocation memory type must have
    VkFlags preferred_flags;    // flags the allocation memory type would prefer to have
    void* pNext;                // extension data
};

/* Behaviors of the default allocator:
 * It totally ignores role
 * If usage is 0/DONT_KNOW, and no flags are supplied, usage defaults to DEVICE_ONLY
 * If usage is 0/DONT_KNOW, and required flags are supplied, only the flags are used
 */

typedef PFN_vkVoidFunction (*LaharLoaderFunc)(const char*);
typedef int64_t (*LaharDeviceScoreFunc)(const LaharDeviceInfo*);
typedef uint32_t (*LaharSurfaceFormatChooseFunc)(LaharWindowState*, LaharDeviceInfo*, VkSurfaceFormatKHR* surface_fmt_out);
typedef uint32_t (*LaharSurfacePresentModeChooseFunc)(LaharWindowState*, LaharDeviceInfo*, VkPresentModeKHR* present_mode_out);
typedef uint32_t (*LaharSurfaceResizeFunc)(LaharWindow* window);
typedef uint32_t (*LaharShaderCompileFunc)(void* data, const LaharShaderStage* stage, uint32_t** spv_out, uint64_t* len_out);
typedef uint32_t (*LaharShaderCompileReleaseFunc)(void* data, uint32_t* spv, uint64_t len);

typedef uint32_t (*LaharAllocImageFunc)(void* self, const VkImageCreateInfo* info, const LaharAllocationCreateInfo* alloc_info,  VkImage* img_out, LaharAllocation* alloc_out);
typedef uint32_t (*LaharFreeImageFunc)(void* self, VkImage img, LaharAllocation alloc);
typedef uint32_t (*LaharAllocBufferFunc)(void* self, const VkBufferCreateInfo* info, const LaharAllocationCreateInfo* alloc_info, VkBuffer*, LaharAllocation*);
typedef uint32_t (*LaharFreeBufferFunc)(void* self, VkBuffer, LaharAllocation);
typedef uint32_t (*LaharMapFunc)(void* self, LaharAllocation, void** out);
typedef uint32_t (*LaharUnmapFunc)(void* self, LaharAllocation);
typedef uint32_t (*LaharFlushFunc)(void* self, LaharAllocation, uint64_t off, uint64_t size);
typedef uint32_t (*LaharInvalidateFunc)(void* self, LaharAllocation, uint64_t off, uint64_t size);

struct LaharAllocator {
    LaharAllocImageFunc alloc_image;
    LaharFreeImageFunc free_image;
    LaharAllocBufferFunc alloc_buffer;
    LaharFreeBufferFunc free_buffer;
    LaharMapFunc map;
    LaharUnmapFunc unmap;
    LaharFlushFunc flush;
    LaharInvalidateFunc invalidate;
};

struct LaharDeviceInfo {
    VkPhysicalDevice physdev;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties memprops;
    VkPhysicalDeviceLimits limits;

    VkSurfaceFormatKHR surface_formats[LAHAR_MAX_DEVICE_ENTRIES];
    VkPresentModeKHR present_modes[LAHAR_MAX_DEVICE_ENTRIES];
    uint32_t surface_fmt_count;
    uint32_t present_mode_count;

    uint32_t graphics_queue_index;
    uint32_t present_queue_index;
    bool has_graphics_queue;
    bool has_present_queue;
};

enum LaharFramePhase {
    LAHAR_FRAME_PHASE_BEGIN = 0,
    LAHAR_FRAME_PHASE_DRAW = 1,
    LAHAR_FRAME_PHASE_PRESENT = 2,
};

enum LaharDebugLevel {
    LAHAR_DEBUG_UNSET = 0,
    LAHAR_DEBUG_TRACE = 1,
    LAHAR_DEBUG_INFO = 2,
    LAHAR_DEBUG_WARNING = 3,
    LAHAR_DEBUG_ERROR = 4,
    LAHAR_DEBUG_DISABLED = 5
};

enum LaharAttachmentRole {
    LAHAR_ATTROLE_COLOR,        // The primary color attachment
    LAHAR_ATTROLE_USER,         // A user-specific attachment. Functions as a color attachment
    LAHAR_ATTROLE_DEPTH,        // A depth attachment
    LAHAR_ATTROLE_STENCIL,      // A stencil attachment
    LAHAR_ATTROLE_DEPTH_STENCIL // A combined depth + stencil attachment
};

struct LaharAttachmentConfig {
    LaharAttachmentRole role;               // The role this attachment plays, color, user, depth, stencil
    VkImageUsageFlags usage;                // The image's usage. This affects 1. The color attachment's usage in the swapchain, and 2. the subresourcerange while transitioning via a lookup table
    VkAttachmentDescription description;    // The attachment description.
    VkImageCreateInfo img_info;             // The image info. This is passed verbatim to create image (except the extent width/height is set automatically)
    VkImageViewCreateInfo view_info;        // The image view info. This is passed verbatim to create image view (except the image is set automatically)
    VkClearValue clear_value;               // The value this attachment will be cleared to
};

struct LaharWindowConfig {
    uint32_t attachment_count;          // The number of attachments in the below array
    LaharAttachmentConfig* attachments; // The configuration for the attachments
    uint32_t desired_swap_size;         // How many images you would ideally like in the swapchain [default: 2]
    uint32_t max_in_flight;             // How many frames the system can be rendering at once [default: 2]
    VkCompositeAlphaFlagBitsKHR alpha;  // The compositing alpha flags [default: OPAQUE_BIT]
    bool no_auto_swap_resize;           // If true, automatic swap resizing will be disabled [default: false]
};

enum LaharWindowProfile {
    LAHAR_WINPROF_COLOR,                // Create the window with only a color attachment
    LAHAR_WINPROF_COLOR_DEPTH_STENCIL,  // Create the window with a color, and a depth+stencil attachment
};

struct LaharAttachment {
    LaharAllocation img_allocation;         // The allocation for the image
    VkImage image;                          // The attachment's image
    VkImageView view;                       // The attachment's image view
    VkImageLayout layout;                   // The _current_ layout. The transition utilty checks this! If you're transitioning manually, but still want to use the utility, you must update this
};

struct LaharWindowState {
    LaharWindow* window;                    // The window
    uint64_t index;                         // The window's index
    uint32_t width, height;                 // The width and height, in windowing system units
    VkExtent2D extent;                      // The swapchain's extent
    uint32_t desired_img_count;             // The desired number of images in the swapchain
    uint32_t max_in_flight;                 // The max number of images in flight
    LaharFramePhase frame_phase;            // Used to track where we are in the phase, making it so you can submit multiple times before swap
    VkCompositeAlphaFlagBitsKHR alpha;      // The compositing alpha flags
    bool auto_recreate_swap;                // Automatically recreate the swapchain
    bool queued_destruction;                // Queue destruction on resize instead of halt-the-world
    LaharSurfaceResizeFunc resize_callback; // An optional callback to handle surface resizes

    VkSurfaceFormatKHR surface_format;      // The selected surface format
    VkSurfaceKHR surface;                   // The surface
    VkSwapchainKHR swapchain;               // The swapchain
    uint32_t swap_size;                     // The actual size of the swapchain

    uint32_t image_available_size;
    VkSemaphore* image_available;           // The sync semaphores for the images being available
    uint32_t render_finished_size;
    VkSemaphore* render_finished;           // The sync semaphores for rendering being complete
    uint32_t in_flight_size;
    VkFence* in_flight;                     // The fences for if this frame is in flight
    uint32_t present_fences_size;
    VkFence* present_fences;                // Per swapchain image; signaled when its last present retired. Only attached to presents when swapchain_maintenance1 is enabled, otherwise created signaled and left alone

    uint32_t flight_index;                  // The logical index of the frame in flight. Use this to index sync primitives, or anything "per frame in flight"
    uint32_t frame_index;                   // The index of the current swapchain image, set by window_frame_begin

    uint32_t attachment_count;                  // The number of attachment types this window has
    LaharAttachmentConfig* attachment_configs;  // The configurations for the attachments, in order of [ATTACHMENT_TYPE]
    LaharAttachment** attachments;              // Attachments are in a 2D array, of [ATTACHMENT_TYPE][FRAME_INDEX]

    VkCommandPool pool;                     // Will be null unless specifically requested
    VkCommandBuffer* commands;              // Will be null unless specifically requested
};

struct LaharShaderCompiler {
    const char* language;
    void* user_data;
    LaharShaderCompileFunc compile;
    LaharShaderCompileReleaseFunc release;
};

struct Lahar {
    /* Configuration values */
    LaharDebugLevel debug_level;
    LaharLibrary libvulkan;                                 // This is the platform's library handle
    VkResult vkresult;                                      // If any vulkan operation fails, the error code is saved here
    uint32_t vkversion;                                     // Pre-init, this is the requested instance version. Post-init, it's the effective min(instance, device, requested) version
    bool require_min_version;                               // If true, the version is hard required to be at least the requested version, and will fail otherwise
    uint32_t appversion;                                    // An optional setting for the app's version
    const char* appname;                                    // An optional setting for the app's name
    bool wantvalidation;                                    // True if validation layers were requested
    bool wantcommands;                                      // True if the window command buffers were requested
    bool dynamic_rendering;                                 // True if dynamic rendering is available AND enabled, whether via the extension or 1.3 core. Only valid after build
    bool swapchain_maintenance1;                            // True if the swapchain_maintenance1 feature is enabled (extension + feature bit). Only valid after build
    VkAllocationCallbacks* vkalloc;                         // One can set the vulkan CPU allocator, if one desires
    PFN_vkDebugUtilsMessengerCallbackEXT debug_callback;    // One can set the debug messenger callback, if one desires
    void* user_data;                                        // A user supplied pointer
    LaharAllocator* gpu_allocator;                          // A user supplied (or VMA backed, if enabled) Vulkan allocator
    bool gpu_allocator_defaulted;                           // True if we created the default allocator, and we need to clean it up
    void* device_create_pnext;                              // The pNext to pass to VkDeviceCreateInfo - used for enabling optional features

    char* device_name;                                      // An optional lock to the specific device name
    LaharDeviceScoreFunc score_func;                        // An optional custom scoring function to invoke on physical devices
    LaharSurfaceFormatChooseFunc format_chooser;            // An optional custom callback to choose the surface format
    LaharSurfacePresentModeChooseFunc present_chooser;      // An optional custom callback to choose the surface present mode
    LaharShaderCompiler shader_compilers[LAHAR_MAX_SHADER_COMPILERS];
    LaharWindowState window_retire_queue[LAHAR_MAX_WINDOW_RETIRES];
    uint32_t window_retire_count;

    /* Useful Vulkan variables */

    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    LaharDeviceInfo physdev_info;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;

    LaharWindowState* windows;
    size_t window_count, window_cap;

    struct {
        const char** req_inst_exts;
        size_t rie_count, rie_cap;

        const char** req_dev_exts;
        size_t rde_count, rde_cap;

        const char** opt_inst_exts;
        bool* opt_inst_exts_present;
        size_t oie_count, oie_cap;

        const char** opt_dev_exts;
        bool* opt_dev_exts_present;
        size_t ode_count, ode_cap;
    } extensions;

    #if defined(LAHAR_USE_VMA)
    VmaAllocator vma;
    #endif
};

#if defined(__cplusplus) && defined(LAHAR_C_LINKAGE)
extern "C" {
#endif


/** Get a pretty printable version of an error's name
 *
 * @param code The error code
 */
const char* lahar_err_name(uint32_t code);

/** Create an inhstance of the Lahar library
 * @param lahar The library to initialize
*/
uint32_t lahar_init(void);

/** Stick a user data pointer on the lahar instance */
void lahar_set_user_data(void* user_data);

/** Get the user data on the lahar instance */
void* lahar_get_user_data(void);

/** Cleanup the entirety of lahar */
void lahar_deinit(void);










/** Configuration is done, setup and prepare for rendering */
uint32_t lahar_build(void);

/** Set a vulkan allocator for lahar to use. This is only
 * required if you want additional attachments beyond color,
 * and you haven't enabled the VMA support.
 */
uint32_t lahar_builder_allocator_set(LaharAllocator* allocator);

#if defined(LAHAR_USE_VMA)
/** If using VMA, instead of supplying a full LaharAllocator, you can
 * simply supply the VMA allocator. If you don't, lahar will create
 * one and store it in the Lahar instance.
 */
uint32_t lahar_vma_set_allocator(VmaAllocator allocator);
#endif

/** Set Lahar's internal debug level */
void lahar_builder_set_debug_level(LaharDebugLevel level);

/** Set the version of vulkan you'd like to load */
void lahar_builder_set_vulkan_version(uint32_t version, bool required);

/** Inform lahar to load the validation layers, if available */
void lahar_builder_request_validation_layers(void);

/** Add an extension to the list of required instance extensions
 * @param extensions The extension to add
 */
uint32_t lahar_builder_extension_add_required_instance(const char* extension);

/** Add an extension to the list of required device extensions
 * @param extensions The extension to add
 */
uint32_t lahar_builder_extension_add_required_device(const char* extension);

/** Add an extension to the list of optional instance extensions
 * @param extensions The extension to add
 */
uint32_t lahar_builder_extension_add_optional_instance(const char* extension);

/** Add an extension to the list of optional device extensions
 * @param extensions The extension to add
 */
uint32_t lahar_builder_extension_add_optional_device(const char* extension);

/** Set a debug callback for vulkan */
void lahar_builder_set_debug_callback(PFN_vkDebugUtilsMessengerCallbackEXT callback);

/** Set a specific device to use. Failure to find the device will
 * always cause finalize to return LAHAR_ERR_NO_SUITABLE_DEVICE
 *
 * @param name The device name
 */
uint32_t lahar_builder_device_use(const char* name);

/** Set a custom scoring metric for device. The callback will be invoked
 * for all devices. Any device with a negative score is ineligble. The
 * device with the highest score is chosen. If not set, the default
 * scoring function is used.
 *
 * @param scorefunc The scoring callback
 */
uint32_t lahar_builder_device_set_scoring(LaharDeviceScoreFunc scorefunc);

/** Tell lahar to create the utility command buffers in the windows.
 * Not needed if you plan to create your own */
void lahar_builder_request_command_buffers(void);

/** Set the pNext value that will be passed to VkDeviceCreateInfo.
 * This is useful for enabling device features, such as dynamic rendering */
void lahar_builder_set_device_create_pnext(void* pnext);

/** Register a window with lahar. When finalized, this window will have its surface/swapchain/attachments created.
 *
 * NOTE: UNLESS you've defined LAHAR_NO_AUTO_DEPS, lahar will take ownership of the window,
 * and destroy it when lahar is deinited.
 *
 * @param window The window to register
 * @param winprofile The quick profile to use. For more control, see lahar_window_register_ex
*/
uint32_t lahar_builder_window_register(LaharWindow* window, LaharWindowProfile winprofile);

/** Register a window with lahar. When finalized, this window will have its surface/swapchain/attachments created.
 *
 * The config MUST contain at least one attachment config. The attachment config at index 0 _must_
 * be the config for the color attachment. Some of its fields may be ignored, but the usage flag
 * will be respected, if it is set.
 *
 * Any attachment configs you use _after_ that will be reflected in the LaharWindowState.attachments
 * array.
 *
 * NOTE: UNLESS you've defined LAHAR_NO_AUTO_DEPS, lahar will take ownership of the window,
 * and destroy it when lahar is deinited.
 *
 * @param window The window
 * @param winconfig The config
 */
uint32_t lahar_builder_window_register_ex(LaharWindow* window, const LaharWindowConfig* winconfig);










/** Check if an optional instance extension was loaded
 * @param extension The extension to check for
 */
bool lahar_extension_has_instance(const char* extension);

/** Check if an optional device extension was loaded
 * @param extension The extension to check for
 */
bool lahar_extension_has_device(const char* extension);










/** Begin a frame, preparing for rendering. You only need to use this
 * if you plan on using lahar_window_submit, or lahar_window_present
 */
uint32_t lahar_window_frame_begin(LaharWindow* window);

/** Submit a command buffer to a window. */
uint32_t lahar_window_submit(LaharWindow* window, VkCommandBuffer cmd);

/** Submit multiple command buffers to a window */
uint32_t lahar_window_submit_all(LaharWindow* window, VkCommandBuffer* cmds, uint32_t cmd_count);

/** Abandon the frame that lahar_window_frame_begin started, without drawing to it.
 *
 * Useful when you begin a frame and then discover you have nothing to draw: the
 * window is minimised, the app lost focus, a resource is still loading, and so on.
 *
 * You cannot simply stop calling submit/present and start a new frame instead. A
 * begun frame owns real GPU state: the image_available semaphore has a pending
 * signal that something must wait on, the in_flight fence has been reset and only
 * a queue submit will ever signal it again, and the acquired swapchain image is
 * held by your process until it is presented. Dropping a frame on the floor
 * leaks a swapchain image and deadlocks the next wait on that fence.
 *
 * So this does the minimum real work to retire the frame: under dynamic
 * rendering it transitions the color attachment to PRESENT_SRC, then submits and
 * presents. The contents are whatever the swapchain image already held, which is
 * why this is for frames you were never going to show anyway.
 *
 * If you are using render passes rather than dynamic rendering, lahar does not
 * track the attachment layout (vkCmdBeginRenderPass moves it outside lahar's
 * knowledge), so no transition is recorded and you must pass a command buffer
 * that leaves the color attachment in PRESENT_SRC yourself.
 *
 * Cheaper still is to check whether you have anything to draw BEFORE calling
 * frame_begin, and simply not begin a frame. Prefer that when you can.
 *
 * Calling this in the BEGIN phase is a no-op success, so it is safe to call
 * defensively. Calling it after you have already submitted is an error, since
 * at that point the frame can only be presented.
 *
 * @param window The window whose frame should be abandoned
 * @param cmd A command buffer to record the transition into, or VK_NULL_HANDLE to
 *        use the window's own command buffer (requires request_command_buffers)
 */
uint32_t lahar_window_frame_cancel(LaharWindow* window, VkCommandBuffer cmd);

/** Swap the window's visual buffers.
 *
 * If the swapchain went out of date (a resize, usually) this either recreates it
 * and returns that call's result, or returns LAHAR_ERR_SWAPCHAIN_OUT_OF_DATE if
 * auto swap resizing is off. Same handling as lahar_window_frame_begin, so a
 * resize is never reported as a generic LAHAR_ERR_VK_ERR.
 *
 * The frame always ends, even on failure: the command buffers were already
 * submitted, so the frame phase advances regardless and it is safe to go
 * straight into the next lahar_window_frame_begin.
 *
 * @param window The window to present
 */
uint32_t lahar_window_present(LaharWindow* window);

/** Resize a window's swapchain when the window changes size
 *
 * @param window The window to resize
 */
uint32_t lahar_window_swapchain_resize(LaharWindow* window);

/** THIS IS ONE OF THE CUSTOM WINDOW FUNCTIONS.
 *
 * If using one of the window libraries supported by lahar, this is automatically implemented for you.
 * If you're using a custom window implementation, YOU must supply an implementation of this, or
 * you will get linker errors.
 *
 * Create the vulkan surface.
 *
 * @param window The window
 * @param surface (out) The created surface
 *
 * @returns 0 for success, or any non-zero value to indicate failure, preferably a LAHAR_ERR_*
 */
uint32_t lahar_window_surface_create(LaharWindow* window, VkSurfaceKHR* surface);

/** THIS IS ONE OF THE CUSTOM WINDOW FUNCTIONS.
 *
 * If using one of the window libraries supported by lahar, this is automatically implemented for you.
 * If you're using a custom window implementation, YOU must supply an implementation of this, or
 * you will get linker errors.
 *
 * Retrieve the window's size. You likely want this to return the framebuffer size
 * specifically.
 *
 * @param window The window
 * @param width (out) The window's width
 * @param height (out) The window's height
 *
 * @returns 0 for success, or any non-zero value to indicate failure, preferably a LAHAR_ERR_*
 */
uint32_t lahar_window_get_size(LaharWindow* window, uint32_t* width, uint32_t* height);

/** THIS IS ONE OF THE CUSTOM WINDOW FUNCTIONS.
 *
 * If using one of the window libraries supported by lahar, this is automatically implemented for you.
 * If you're using a custom window implementation, YOU must supply an implementation of this, or
 * you will get linker errors.
 *
 * Retrieve the extensions the window needs in order to function
 *
 * @param window The window
 * @param ext_count (out) The number of extensions the window needs
 * @param extensions (out) The array to write to. This MUST support NULL
 *
 * @returns 0 for success, or any non-zero value to indicate failure, preferably a LAHAR_ERR_*
 */

uint32_t lahar_window_get_extensions(LaharWindow* window, uint32_t* ext_count, const char** extensions);

/** Get the lahar window state struct for this window. NULL if not found. */
LaharWindowState* lahar_window_state(LaharWindow* window);

/** Wait until a particular window is inactive */
uint32_t lahar_window_wait_inactive(LaharWindow* window);

/** Get the configurations used during attachment creations for by index.
 *
 * @param window The window
 * @param index The attachment index
 * @return The config pointer, or NULL if not found
*/
LaharAttachmentConfig* lahar_window_attachment_config_index(LaharWindow* window, uint32_t index);

/** Get the first configuration used during attachment creation that has the color role
 *
 * @param window The window
 * @return The config pointer, or NULL if not found
*/
LaharAttachmentConfig* lahar_window_attachment_config_color(LaharWindow* window);

/** Get the first configuration used during attachment creation that has the depth or depth+stencil roles
 *
 * @param window The window
 * @return The config pointer, or NULL if not found
*/
LaharAttachmentConfig* lahar_window_attachment_config_depth(LaharWindow* window);

/** Get the first configuration used during attachment creation that has the stencil or depth+stencil roles
 *
 * @param window The window
 * @return The config pointer, or NULL if not found
*/
LaharAttachmentConfig* lahar_window_attachment_config_stencil(LaharWindow* window);


/** Get the actual attachment data for a window for an attachment at a specifc index,
 * and at a specific frame
 *
 * @param window The window
 * @param index The attachment index
 * @return The config pointer, or NULL if not found
*/
LaharAttachment* lahar_window_attachment_index(LaharWindow* window, uint32_t index, uint32_t frameno);

/** Get the actual attachment data for a window for the first attachment with the color role
 * and at a specific frame
 *
 * @param window The window
 * @param index The attachment index
 * @return The config pointer, or NULL if not found
*/
LaharAttachment* lahar_window_attachment_color(LaharWindow* window, uint32_t frameno);

/** Get the actual attachment data for a window for the first attachment with the depth or depth+stencil
 * and at a specific frame
 *
 * @param window The window
 * @param index The attachment index
 * @return The config pointer, or NULL if not found
*/
LaharAttachment* lahar_window_attachment_depth(LaharWindow* window, uint32_t frameno);

/** Get the actual attachment data for a window for the first attachment with the stencil or depth+stencil
 * and at a specific frame
 *
 * @param window The window
 * @param index The attachment index
 * @return The config pointer, or NULL if not found
*/
LaharAttachment* lahar_window_attachment_stencil(LaharWindow* window, uint32_t frameno);

/** Get the command buffer for this frame. Only works after calling window_frame_begin
 *
 * @return The command buffer for the current frame, or VK_NULL_HANDLE if no command buffers were created
 */
VkCommandBuffer lahar_window_command_buffer(LaharWindow* window);








/** A utility to record the command to transition a the layout of a window's
 * attachment. Useful if you're doing dynamic rendering.
 *
 * @param cmd The command buffer to record to
 * @param window The window
 * @param attachment_index The index of the attachment to transition, as specified in your original attachment array (or see defaults)
 * @param layout The layout to transition to
 *
 */
uint32_t lahar_cmd_attachment_transition(VkCommandBuffer cmd, LaharWindow* window, uint32_t attachment_index, VkImageLayout layout);

/** A utility to make calling vkCmdBeginRendering easier. Handles
 * marshalling all the attachment info from your window.
 *
 * REQUIRES dynamic rendering (see lahar->dynamic_rendering). Calling this
 * without it is a programming error and fails a fatal assert, since the
 * underlying entry point was never loaded.
 *
 * @param cmd The command buffer to record to
 * @param window The window to begin rendering with
 */
uint32_t lahar_cmd_begin_rendering(VkCommandBuffer cmd, LaharWindow* window);










/** Register a custom language compiler with lahar. Whatever
 * language is set should also be used on the shader stages
 * to trigger this compiler.
 *
 * @param language The name of the language to use
 * @param compiler_func The function to call to compile the shader
 * @param release_func The function to call to release resources generated by the compiler
 */
uint32_t lahar_shader_register_compiler(
    const char* language,
    void* user_data,
    LaharShaderCompileFunc compiler_func,
    LaharShaderCompileReleaseFunc release_func
);

struct LaharShaderStage {
    VkShaderStageFlagBits stage;
    const void* code;
    uint64_t length;
    const char* lang;
    const char* entrypoint;
};

enum LaharShaderBlendMode {
    LAHAR_SBM_DEFAULT = 0,
    LAHAR_SBM_OPAQUE = 1,
    LAHAR_SBM_ALPHA = 2,
    LAHAR_SBM_ADDITIVE = 3,
    LAHAR_SBM_PREMULTIPLIED = 4,
    LAHAR_SBM_MULTIPLICATIVE = 5
};

// is intentionally the base vulkan set + 1
enum LaharShaderTopology {
    LAHAR_ST_DEFAULT = 0,
    LAHAR_ST_POINT_LIST = 1,
    LAHAR_ST_LINE_LIST = 2,
    LAHAR_ST_LINE_STRIP = 3,
    LAHAR_ST_TRIANGLE_LIST = 4,
    LAHAR_ST_TRIANGLE_STRIP = 5,
    LAHAR_ST_TRIANGLE_FAN = 6,
    LAHAR_ST_LINE_LIST_WITH_ADJACENCY = 7,
    LAHAR_ST_LINE_STRIP_WITH_ADJACENCY = 8,
    LAHAR_ST_TRIANGLE_LIST_WITH_ADJACENCY = 9,
    LAHAR_ST_TRIANGLE_STRIP_WITH_ADJACENCY = 10,
    LAHAR_ST_PATCH_LIST = 11,
};

enum LaharShaderDepthTestMode {
    LAHAR_SDTM_DEFAULT = 0,
    LAHAR_SDTM_OFF = 1,
    LAHAR_SDTM_ON = 2,
};

enum LaharShaderDepthWriteMode {
    LAHAR_SDWM_DEFAULT = 0,
    LAHAR_SDWM_OFF = 1,
    LAHAR_SDWM_ON = 2,
};

enum LaharShaderDepthCompareOp {
    LAHAR_SDCO_DEFAULT = 0,
    LAHAR_SDCO_NEVER = 1,
    LAHAR_SDCO_LESS = 2,
    LAHAR_SDCO_EQUAL = 3,
    LAHAR_SDCO_LESS_OR_EQUAL = 4,
    LAHAR_SDCO_GREATER = 5,
    LAHAR_SDCO_NOT_EQUAL = 6,
    LAHAR_SDCO_GREATER_OR_EQUAL = 7,
    LAHAR_SDCO_ALWAYS = 8,
};

enum LaharShaderCullMode {
    LAHAR_SCM_DEFAULT = 0,
    LAHAR_SCM_BACK = 1,
    LAHAR_SCM_FRONT = 2,
    LAHAR_SCM_OFF = 3,
};

enum LaharShaderFaceMode {
    LAHAR_SFM_DEFAULT = 0,
    LAHAR_SFM_FRONT_CCW = 1,
    LAHAR_SFM_FRONT_CW = 2,
};

enum LaharShaderDynamicFlags {
    LAHAR_SDF_DEFAULT = 0,
    LAHAR_SDF_BASIC = 1 << 0,           // The viewport and the scissor, the default
    LAHAR_SDF_DEPTH_BIAS = 1 << 1,      // The depth bias
    LAHAR_SDF_BLEND_CONST = 1 << 2,     // The blend constants
    LAHAR_SDF_DEPTH_STENCIL = 1 << 3,   // depth [test enable, write enable, compare op] + stencil [reference, compare mask, write mask]
    LAHAR_SDF_CULL = 1 << 4,            // cull mode and front face

    LAHAR_SDF_ALL = 0x7FFFFFFF
};

struct LaharShaderBuilder {
    LaharShaderStage stages[LAHAR_MAX_SHADER_STAGES];
    uint32_t stage_count;

    VkVertexInputAttributeDescription* attrib_descriptions;
    VkVertexInputBindingDescription* binding_descriptions;
    uint32_t attrib_count;
    uint32_t binding_count;

    VkFormat surface_color_format;
    VkFormat surface_depth_format;
    VkFormat surface_stencil_format;
    VkPipelineColorBlendAttachmentState blend_states[LAHAR_MAX_ATTACHMENTS];
    uint32_t blend_state_count;

    LaharShaderBlendMode blend;
    LaharShaderTopology topology;
    LaharShaderDepthTestMode depth_test;
    LaharShaderDepthWriteMode depth_write;
    LaharShaderDepthCompareOp depth_compare_op;
    LaharShaderCullMode cull_mode;
    LaharShaderFaceMode face_mode;
    LaharShaderDynamicFlags dynamic_mode;
    bool wireframe;
    bool all_dynamic;
    bool skip_validation;
    bool force_dynamic;

    void* pNext;
    VkPipelineCreateFlags flags;
    const VkPipelineVertexInputStateCreateInfo* pVertexInputState;
    const VkPipelineInputAssemblyStateCreateInfo* pInputAssemblyState;
    const VkPipelineTessellationStateCreateInfo* pTessellationState;
    const VkPipelineViewportStateCreateInfo* pViewportState;
    const VkPipelineRasterizationStateCreateInfo* pRasterizationState;
    const VkPipelineMultisampleStateCreateInfo* pMultisampleState;
    const VkPipelineDepthStencilStateCreateInfo* pDepthStencilState;
    const VkPipelineColorBlendStateCreateInfo* pColorBlendState;
    const VkPipelineDynamicStateCreateInfo* pDynamicState;
    VkPipelineLayout layout;
    VkRenderPass renderPass;
    uint32_t subpass;
    bool use_subpass_value; // have to set this if subpass is set, since 0 is a valid default

    const VkDescriptorSetLayout* set_layouts;   // Optional caller-owned set layouts to build the pipeline layout from
    uint32_t set_layout_count;
};

enum LaharShaderVarStorageClass {
    LAHAR_SVSC_UNKNOWN = 0,
    LAHAR_SVSC_INPUT = 1,
    LAHAR_SVSC_UNIFORM_BUFFER = 2,
    LAHAR_SVSC_STORAGE_BUFFER = 3,
    LAHAR_SVSC_PUSH_CONSTANT = 4,
    LAHAR_SVSC_UNIFORM_CONSTANT = 5,
};

enum LaharShaderVarType {
    LAHAR_SVT_UNKNOWN = 0,
    LAHAR_SVT_VOID,
    LAHAR_SVT_BOOL,
    LAHAR_SVT_INT,
    LAHAR_SVT_UINT,
    LAHAR_SVT_HALF,
    LAHAR_SVT_FLOAT,
    LAHAR_SVT_DOUBLE,
    LAHAR_SVT_BVEC2,
    LAHAR_SVT_BVEC3,
    LAHAR_SVT_BVEC4,
    LAHAR_SVT_IVEC2,
    LAHAR_SVT_IVEC3,
    LAHAR_SVT_IVEC4,
    LAHAR_SVT_UVEC2,
    LAHAR_SVT_UVEC3,
    LAHAR_SVT_UVEC4,
    LAHAR_SVT_DVEC2,
    LAHAR_SVT_DVEC3,
    LAHAR_SVT_DVEC4,
    LAHAR_SVT_VEC2,
    LAHAR_SVT_VEC3,
    LAHAR_SVT_VEC4,
    LAHAR_SVT_MAT2X3,
    LAHAR_SVT_MAT2X4,
    LAHAR_SVT_MAT3X2,
    LAHAR_SVT_MAT3X4,
    LAHAR_SVT_MAT4X2,
    LAHAR_SVT_MAT4X3,
    LAHAR_SVT_MAT2,
    LAHAR_SVT_MAT3,
    LAHAR_SVT_MAT4,
    LAHAR_SVT_DMAT2X3,
    LAHAR_SVT_DMAT2X4,
    LAHAR_SVT_DMAT3X2,
    LAHAR_SVT_DMAT3X4,
    LAHAR_SVT_DMAT4X2,
    LAHAR_SVT_DMAT4X3,
    LAHAR_SVT_DMAT2,
    LAHAR_SVT_DMAT3,
    LAHAR_SVT_DMAT4,
    LAHAR_SVT_SAMPLER,
    LAHAR_SVT_IMAGE,
    LAHAR_SVT_SAMPLER_IMAGE,
    LAHAR_SVT_STRUCT,
    LAHAR_SVT_ARRAY,
    LAHAR_SVT_POINTER,
};

struct LaharShaderVarInfo {
    const char* path;                           // A path to the variable, such as mesh.transform, invalidated on next call into lahar
    LaharShaderVarStorageClass storage_class;   // The spir-v storage class

    uint32_t offset;                            // The offset in bytes inside the parent type
    uint32_t size;                              // The size in bytes
    uint32_t stride;                            // The stride, if a matrix or array type

    uint32_t set;                               // The set the uniform belongs to
    uint32_t binding;                           // The binding the uniform belongs to
    uint32_t location;                          // The location of the vertex input

    LaharShaderVarType type;                    // The deduced shader var type
    VkShaderStageFlagBits stages;               // The stages this uniform is used in
    uint32_t array_size;                        // The array size, 0 for runtime arrays

    uint32_t parent_var;                        // The parent of this var/type, UINT32_MAX if none
    uint32_t child_vars_begin;                  // The beginning of the child vars, UINT32_MAX if none
    uint32_t child_vars_end;                    // The end of the child vars, UINT32_MAX if none

    uint32_t spv_var_id;                        // The variable ID from the spir-v
    uint32_t spv_type_id;                       // The type ID from the spir-v
};


/* ============================================================================
 * Shader Builder Functions
 * ============================================================================
 * These functions provide a builder pattern API for configuring shader pipelines.
 * Usage:
 *   1. Zero-init a LaharShaderBuilder struct
 *   2. Configure using lahar_shader_builder_*() functions
 *   3. Call lahar_shader_build() to create the pipeline
 */


/* Stages must be set. Vertex inputs must be set if the shader uses them. */

/** Set the stages, overwriting any that are currently stored */
void lahar_shader_builder_set_stages(LaharShaderBuilder* builder, LaharShaderStage* stages, uint32_t count);
/** Add a stage to the stages currently stored */
void lahar_shader_builder_add_stage(LaharShaderBuilder* builder, LaharShaderStage* stage);

/** Set the vertx input attributes and bindings. This just stores the pointers, so it must
 * survive until build is called.
 */
void lahar_shader_builder_set_vertex_input(
    LaharShaderBuilder* builder,
    VkVertexInputAttributeDescription* attribs,
    VkVertexInputBindingDescription* bindings,
    uint32_t attrib_count,
    uint32_t binding_count
);

/* Must call set window OR set formats + set blend states */

/** Set the window the shader must be compatible with. This infers the formats and blend states. */
void lahar_shader_builder_set_window(LaharShaderBuilder* builder, LaharWindow* window);

/** Set the surface formats of the surface the shader must be compatible with */
void lahar_shader_builder_set_formats(LaharShaderBuilder* builder, VkFormat color, VkFormat depth, VkFormat stencil);
/** Set the blend states for the attachments the shader will use */
void lahar_shader_builder_set_blend_states(LaharShaderBuilder* builder, VkPipelineColorBlendAttachmentState* states, uint32_t state_count);

/* Simplified parameters for quick configuration of common options */

/** Set the simplified blend mode. See LaharShaderBlendMode for options */
void lahar_shader_builder_set_blend_mode(LaharShaderBuilder* builder, LaharShaderBlendMode blend);
/** Set the topology. See LaharShaderTopology for options */
void lahar_shader_builder_set_topology(LaharShaderBuilder* builder, LaharShaderTopology topology);
/** Set the depth testing mode. See LaharShaderDepthTestMode for options */
void lahar_shader_builder_set_depth_test(LaharShaderBuilder* builder, LaharShaderDepthTestMode depth_test);
/** Set the depth write mode. See LaharShaderDepthWriteMode for options */
void lahar_shader_builder_set_depth_write(LaharShaderBuilder* builder, LaharShaderDepthWriteMode depth_write);
/** Set the depth compare op. See LaharShaderDepthCompareOp for options */
void lahar_shader_builder_set_depth_compare_op(LaharShaderBuilder* builder, LaharShaderDepthCompareOp depth_compare_op);
/** Set the cull mode. See LaharShaderCullMode for options */
void lahar_shader_builder_set_cull_mode(LaharShaderBuilder* builder, LaharShaderCullMode cull_mode);
/** Set the cull face. See LaharShaderFaceMode for options */
void lahar_shader_builder_set_face_mode(LaharShaderBuilder* builder, LaharShaderFaceMode face_mode);
/** Set the what parts of a shader are dynamic. See LaharShaderDynamicFlags to see options. NOTE THAT IT DEFAULTS TO SOME DYNAMIC STATE */
void lahar_shader_builder_set_dynamic_flags(LaharShaderBuilder* builder, LaharShaderDynamicFlags dynamic_mode);
/** Enable literally every possible dynamic state in a shader. You _probably_ don't want this. */
void lahar_shader_builder_enable_all_dynamic(LaharShaderBuilder* builder, bool all_dynamic);
/** Set the shader to draw in wireframe mode */
void lahar_shader_builder_set_wireframe(LaharShaderBuilder* builder, bool wireframe);
/** Disable validation of inputs against the SPIRV */
void lahar_shader_builder_skip_validation(LaharShaderBuilder* builder, bool skip_validation);

/* Parameters that will be passed verbatim to the VkPiplineCreateInfo, overriding other above settings */

void lahar_shader_builder_set_pnext(LaharShaderBuilder* builder, void* pNext);
void lahar_shader_builder_set_flags(LaharShaderBuilder* builder, VkPipelineCreateFlags flags);
void lahar_shader_builder_set_vertex_input_state(LaharShaderBuilder* builder, const VkPipelineVertexInputStateCreateInfo* pVertexInputState);
void lahar_shader_builder_set_input_assembly_state(LaharShaderBuilder* builder, const VkPipelineInputAssemblyStateCreateInfo* pInputAssemblyState);
void lahar_shader_builder_set_tessellation_state(LaharShaderBuilder* builder, const VkPipelineTessellationStateCreateInfo* pTessellationState);
void lahar_shader_builder_set_viewport_state(LaharShaderBuilder* builder, const VkPipelineViewportStateCreateInfo* pViewportState);
void lahar_shader_builder_set_rasterization_state(LaharShaderBuilder* builder, const VkPipelineRasterizationStateCreateInfo* pRasterizationState);
void lahar_shader_builder_set_multisample_state(LaharShaderBuilder* builder, const VkPipelineMultisampleStateCreateInfo* pMultisampleState);
void lahar_shader_builder_set_depth_stencil_state(LaharShaderBuilder* builder, const VkPipelineDepthStencilStateCreateInfo* pDepthStencilState);
void lahar_shader_builder_set_color_blend_state(LaharShaderBuilder* builder, const VkPipelineColorBlendStateCreateInfo* pColorBlendState);
void lahar_shader_builder_set_dynamic_state(LaharShaderBuilder* builder, const VkPipelineDynamicStateCreateInfo* pDynamicState);
void lahar_shader_builder_set_layout(LaharShaderBuilder* builder, VkPipelineLayout layout);
/** Build the pipeline layout from these caller-owned descriptor set layouts instead of
 * reflecting its own. Typically the output of lahar_shader_reflect_set_layouts(), kept so
 * you can also allocate descriptor sets from them. Only stores the pointer, so the array
 * must survive until build is called. Ignored if set_layout() was used. The push constant
 * range is still taken from reflection. */
void lahar_shader_builder_set_descriptor_set_layouts(LaharShaderBuilder* builder, const VkDescriptorSetLayout* layouts, uint32_t count);
void lahar_shader_builder_set_render_pass(LaharShaderBuilder* builder, VkRenderPass renderPass);
void lahar_shader_builder_set_subpass(LaharShaderBuilder* builder, uint32_t subpass);

/** Builds a Vulkan graphics pipeline from the configured shader info.
 *
 * This is the finalization step of the shader builder pattern. After configuring
 * a LaharShaderBuilder struct using lahar_shader_builder_*() functions, call this
 * function to create the actual Vulkan pipeline.
 *
 * Required fields (must be set via builder functions):
 *   - stages (via lahar_shader_builder_set_stages)
 *   - window (via lahar_shader_builder_set_window)
 *
 * All other fields are optional and will use sensible defaults if not set.
 * Verbatim Vulkan state structs (pVertexInputState, pRasterizationState, etc.)
 * always override the corresponding simplified parameters when set.
 *
 * @param info The configured shader info struct
 * @param pipeline (out) The created pipeline, or VK_NULL_HANDLE if failed
 * @param layout (out) (nullable) The layout, _IF_ one was created by this function, else
 *        VK_NULL_HANDLE. This is an ownership signal, not just a handle: a non-null result
 *        is a layout you are now responsible for destroying. It is deliberately NOT set to
 *        a layout you supplied via set_layout(), so that destroying whatever comes back is
 *        always correct and never double-frees a layout you share between pipelines.
 *        Pass NULL if you supplied the layout yourself and have nothing to collect.
 * @return Error code (LAHAR_ERR_SUCCESS on success)
 */
uint32_t lahar_shader_build(LaharShaderBuilder* builder, VkPipeline* pipeline, VkPipelineLayout* layout);

/** Get the string representation of a shader var type*/
const char* lahar_shader_var_type_string(LaharShaderVarType svt);

/** Get the string representation of a VkFormat */
const char* lahar_vkformat_string(VkFormat format);

/* Get the corresponding VkFormat for a var type used as a vertex input* */
uint32_t lahar_shader_var_type_to_input_type(LaharShaderVarType svt, VkFormat* format_out);

/** Given a set of stages, introspect all the inputs, descriptors, and push constants.
 * The path field of the shader vars are only guaranteed to live until the next call into lahar
 *
 * @param stages A list of stages
 * @param num_stages The number of stages you're passing in
 * @param num_infos (out) The number of info structs that will be/has been written
 * @param info (out) (nullable) The array to write the info structs into
 */
uint32_t lahar_shader_introspect(const LaharShaderStage* stages, uint32_t num_stages, uint32_t* num_infos, LaharShaderVarInfo* infos);

/**
 * A convenience for pretty-printing the results of an introspected shader. Useful
 * for verifying your shader is what you think it is, and that Lahar introspected it
 * correctly.
 *
 * @param infos The output of lahar_shader_introspect
 * @param count The number of infos, also the output of lahar_shader_introspect
 */
void lahar_shader_introspection_print(const LaharShaderVarInfo* infos, uint32_t count);

/** Create the descriptor set layouts implied by introspected shader vars.
 *
 * Two-pass, like lahar_shader_introspect: call once with layouts_out NULL to
 * learn how many sets there are, allocate, then call again to create them.
 *
 * The returned handles are owned by YOU. Destroy them with
 * vkDestroyDescriptorSetLayout once the pipeline layouts using them are built
 * and you are done allocating descriptor sets from them.
 *
 * Sets are dense and indexed by set number: if a shader uses only set 2, this
 * reports 3 sets and creates empty layouts for 0 and 1, so that layouts_out
 * can be handed straight to VkPipelineLayoutCreateInfo::pSetLayouts.
 *
 * Pass the results to lahar_shader_builder_set_descriptor_set_layouts() to have
 * the pipeline built against these exact layouts, so the same handles you keep
 * for vkAllocateDescriptorSets are the ones the pipeline was created with.
 *
 * @param vars The output of lahar_shader_introspect
 * @param var_count The number of vars
 * @param set_count (out) The number of sets that will be/has been written
 * @param layouts_out (out) (nullable) Array of at least *set_count layouts to create into
 */
uint32_t lahar_shader_reflect_set_layouts(
    const LaharShaderVarInfo* vars,
    uint32_t var_count,
    uint32_t* set_count,
    VkDescriptorSetLayout* layouts_out
);
















/** Create a freelist vulkan allocator */
LaharAllocator* lahar_allocator_freelist_init(void);

/** Destroy a freelist vulkan allocator */
void lahar_allocator_freelist_deinit(LaharAllocator* allocator);






/* ============================================================================
 * GPU Memory
 * ============================================================================
 * Thin wrappers over lahar->gpu_allocator, so that the common cases do not
 * require reaching through the vtable and passing the allocator to itself.
 * Whichever allocator is in use (the built in freelist, VMA, or your own) these
 * dispatch to it, so calling code does not have to care which.
 *
 * All of these require a built lahar and return LAHAR_ERR_INVALID_STATE if no
 * allocator exists yet.
 *
 * Nothing here is owned by lahar: every successful create pairs with a destroy
 * that you are responsible for, before lahar_deinit.
 */

/** Create a buffer and back it with memory.
 *
 * @param info The buffer create info, passed verbatim to vkCreateBuffer
 * @param alloc_info How the memory should be selected
 * @param buffer_out (out) The created buffer
 * @param alloc_out (out) The allocation backing it, needed to map or free
 */
uint32_t lahar_buffer_create(
    const VkBufferCreateInfo* info,
    const LaharAllocationCreateInfo* alloc_info,
    VkBuffer* buffer_out,
    LaharAllocation* alloc_out
);

/** Create a buffer without spelling out a VkBufferCreateInfo. Covers the common
 * case of a single queue, non-sparse, non-aliased buffer.
 *
 * @param size Size in bytes
 * @param usage What the buffer will be used for
 * @param mem_usage Where the memory should live
 * @param role Finer grained hint, optional, may be 0
 * @param buffer_out (out) The created buffer
 * @param alloc_out (out) The allocation backing it
 */
uint32_t lahar_buffer_create_simple(
    uint64_t size,
    VkBufferUsageFlags usage,
    LaharMemoryUsage mem_usage,
    LaharAllocationRole role,
    VkBuffer* buffer_out,
    LaharAllocation* alloc_out
);

/** Destroy a buffer and release its memory. Safe to call with VK_NULL_HANDLE. */
uint32_t lahar_buffer_destroy(VkBuffer buffer, LaharAllocation alloc);

/** Create an image and back it with memory.
 *
 * @param info The image create info, passed verbatim to vkCreateImage
 * @param alloc_info How the memory should be selected
 * @param image_out (out) The created image
 * @param alloc_out (out) The allocation backing it
 */
uint32_t lahar_image_create(
    const VkImageCreateInfo* info,
    const LaharAllocationCreateInfo* alloc_info,
    VkImage* image_out,
    LaharAllocation* alloc_out
);

/** Destroy an image and release its memory. Safe to call with VK_NULL_HANDLE. */
uint32_t lahar_image_destroy(VkImage image, LaharAllocation alloc);

/** Map an allocation into host address space. Only valid for host visible memory.
 * Nested maps of the same allocation are reference counted, so every map needs a
 * matching unmap.
 *
 * @param alloc The allocation to map
 * @param out (out) The mapped pointer, already offset to this allocation
 */
uint32_t lahar_memory_map(LaharAllocation alloc, void** out);

/** Unmap an allocation previously mapped with lahar_memory_map */
uint32_t lahar_memory_unmap(LaharAllocation alloc);

/** Flush host writes so the device can see them. A no-op on coherent memory, but
 * always safe (and correct) to call.
 *
 * @param alloc The allocation
 * @param offset Byte offset within the allocation
 * @param size Bytes to flush, or VK_WHOLE_SIZE
 */
uint32_t lahar_memory_flush(LaharAllocation alloc, uint64_t offset, uint64_t size);

/** Invalidate the host cache so host reads see device writes. A no-op on coherent
 * memory, but always safe to call.
 *
 * @param alloc The allocation
 * @param offset Byte offset within the allocation
 * @param size Bytes to invalidate, or VK_WHOLE_SIZE
 */
uint32_t lahar_memory_invalidate(LaharAllocation alloc, uint64_t offset, uint64_t size);

/** Copy data into a host visible allocation: map, copy, flush, unmap.
 *
 * This is the whole upload for memory the host can write to. It is NOT valid for
 * device only memory; that needs a staging buffer and a transfer command.
 *
 * @param alloc The destination allocation
 * @param offset Byte offset within the allocation to write at
 * @param data The bytes to copy
 * @param size How many bytes to copy
 */
uint32_t lahar_memory_write(LaharAllocation alloc, uint64_t offset, const void* data, uint64_t size);

/** Copy data out of a host visible allocation: map, invalidate, copy, unmap.
 *
 * @param alloc The source allocation
 * @param offset Byte offset within the allocation to read from
 * @param data (out) Where to copy the bytes to
 * @param size How many bytes to copy
 */
uint32_t lahar_memory_read(LaharAllocation alloc, uint64_t offset, void* data, uint64_t size);






extern Lahar __lahar_instance;
extern Lahar* lahar;
#define LAHAR_VERSION VK_MAKE_VERSION(4, 2, 0)

#if defined(__cplusplus) && defined(LAHAR_C_LINKAGE)
}
#endif
















/* LAHAR_VK_PROTOTYPES_H */
#if defined(VK_VERSION_1_0)
extern PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
extern PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
extern PFN_vkAllocateMemory vkAllocateMemory;
extern PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
extern PFN_vkBindBufferMemory vkBindBufferMemory;
extern PFN_vkBindImageMemory vkBindImageMemory;
extern PFN_vkCmdBeginQuery vkCmdBeginQuery;
extern PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass;
extern PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
extern PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer;
extern PFN_vkCmdBindPipeline vkCmdBindPipeline;
extern PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers;
extern PFN_vkCmdBlitImage vkCmdBlitImage;
extern PFN_vkCmdClearAttachments vkCmdClearAttachments;
extern PFN_vkCmdClearColorImage vkCmdClearColorImage;
extern PFN_vkCmdClearDepthStencilImage vkCmdClearDepthStencilImage;
extern PFN_vkCmdCopyBuffer vkCmdCopyBuffer;
extern PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage;
extern PFN_vkCmdCopyImage vkCmdCopyImage;
extern PFN_vkCmdCopyImageToBuffer vkCmdCopyImageToBuffer;
extern PFN_vkCmdCopyQueryPoolResults vkCmdCopyQueryPoolResults;
extern PFN_vkCmdDispatch vkCmdDispatch;
extern PFN_vkCmdDispatchIndirect vkCmdDispatchIndirect;
extern PFN_vkCmdDraw vkCmdDraw;
extern PFN_vkCmdDrawIndexed vkCmdDrawIndexed;
extern PFN_vkCmdDrawIndexedIndirect vkCmdDrawIndexedIndirect;
extern PFN_vkCmdDrawIndirect vkCmdDrawIndirect;
extern PFN_vkCmdEndQuery vkCmdEndQuery;
extern PFN_vkCmdEndRenderPass vkCmdEndRenderPass;
extern PFN_vkCmdExecuteCommands vkCmdExecuteCommands;
extern PFN_vkCmdFillBuffer vkCmdFillBuffer;
extern PFN_vkCmdNextSubpass vkCmdNextSubpass;
extern PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
extern PFN_vkCmdPushConstants vkCmdPushConstants;
extern PFN_vkCmdResetEvent vkCmdResetEvent;
extern PFN_vkCmdResetQueryPool vkCmdResetQueryPool;
extern PFN_vkCmdResolveImage vkCmdResolveImage;
extern PFN_vkCmdSetBlendConstants vkCmdSetBlendConstants;
extern PFN_vkCmdSetDepthBias vkCmdSetDepthBias;
extern PFN_vkCmdSetDepthBounds vkCmdSetDepthBounds;
extern PFN_vkCmdSetEvent vkCmdSetEvent;
extern PFN_vkCmdSetLineWidth vkCmdSetLineWidth;
extern PFN_vkCmdSetScissor vkCmdSetScissor;
extern PFN_vkCmdSetStencilCompareMask vkCmdSetStencilCompareMask;
extern PFN_vkCmdSetStencilReference vkCmdSetStencilReference;
extern PFN_vkCmdSetStencilWriteMask vkCmdSetStencilWriteMask;
extern PFN_vkCmdSetViewport vkCmdSetViewport;
extern PFN_vkCmdUpdateBuffer vkCmdUpdateBuffer;
extern PFN_vkCmdWaitEvents vkCmdWaitEvents;
extern PFN_vkCmdWriteTimestamp vkCmdWriteTimestamp;
extern PFN_vkCreateBuffer vkCreateBuffer;
extern PFN_vkCreateBufferView vkCreateBufferView;
extern PFN_vkCreateCommandPool vkCreateCommandPool;
extern PFN_vkCreateComputePipelines vkCreateComputePipelines;
extern PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
extern PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
extern PFN_vkCreateDevice vkCreateDevice;
extern PFN_vkCreateEvent vkCreateEvent;
extern PFN_vkCreateFence vkCreateFence;
extern PFN_vkCreateFramebuffer vkCreateFramebuffer;
extern PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines;
extern PFN_vkCreateImage vkCreateImage;
extern PFN_vkCreateImageView vkCreateImageView;
extern PFN_vkCreateInstance vkCreateInstance;
extern PFN_vkCreatePipelineCache vkCreatePipelineCache;
extern PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
extern PFN_vkCreateQueryPool vkCreateQueryPool;
extern PFN_vkCreateRenderPass vkCreateRenderPass;
extern PFN_vkCreateSampler vkCreateSampler;
extern PFN_vkCreateSemaphore vkCreateSemaphore;
extern PFN_vkCreateShaderModule vkCreateShaderModule;
extern PFN_vkDestroyBuffer vkDestroyBuffer;
extern PFN_vkDestroyBufferView vkDestroyBufferView;
extern PFN_vkDestroyCommandPool vkDestroyCommandPool;
extern PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
extern PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
extern PFN_vkDestroyDevice vkDestroyDevice;
extern PFN_vkDestroyEvent vkDestroyEvent;
extern PFN_vkDestroyFence vkDestroyFence;
extern PFN_vkDestroyFramebuffer vkDestroyFramebuffer;
extern PFN_vkDestroyImage vkDestroyImage;
extern PFN_vkDestroyImageView vkDestroyImageView;
extern PFN_vkDestroyInstance vkDestroyInstance;
extern PFN_vkDestroyPipeline vkDestroyPipeline;
extern PFN_vkDestroyPipelineCache vkDestroyPipelineCache;
extern PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
extern PFN_vkDestroyQueryPool vkDestroyQueryPool;
extern PFN_vkDestroyRenderPass vkDestroyRenderPass;
extern PFN_vkDestroySampler vkDestroySampler;
extern PFN_vkDestroySemaphore vkDestroySemaphore;
extern PFN_vkDestroyShaderModule vkDestroyShaderModule;
extern PFN_vkDeviceWaitIdle vkDeviceWaitIdle;
extern PFN_vkEndCommandBuffer vkEndCommandBuffer;
extern PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties;
extern PFN_vkEnumerateDeviceLayerProperties vkEnumerateDeviceLayerProperties;
extern PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties;
extern PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties;
extern PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
extern PFN_vkFlushMappedMemoryRanges vkFlushMappedMemoryRanges;
extern PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
extern PFN_vkFreeDescriptorSets vkFreeDescriptorSets;
extern PFN_vkFreeMemory vkFreeMemory;
extern PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
extern PFN_vkGetDeviceMemoryCommitment vkGetDeviceMemoryCommitment;
extern PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
extern PFN_vkGetDeviceQueue vkGetDeviceQueue;
extern PFN_vkGetEventStatus vkGetEventStatus;
extern PFN_vkGetFenceStatus vkGetFenceStatus;
extern PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
extern PFN_vkGetImageSparseMemoryRequirements vkGetImageSparseMemoryRequirements;
extern PFN_vkGetImageSubresourceLayout vkGetImageSubresourceLayout;
extern PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
extern PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures;
extern PFN_vkGetPhysicalDeviceFormatProperties vkGetPhysicalDeviceFormatProperties;
extern PFN_vkGetPhysicalDeviceImageFormatProperties vkGetPhysicalDeviceImageFormatProperties;
extern PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
extern PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
extern PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
extern PFN_vkGetPhysicalDeviceSparseImageFormatProperties vkGetPhysicalDeviceSparseImageFormatProperties;
extern PFN_vkGetPipelineCacheData vkGetPipelineCacheData;
extern PFN_vkGetQueryPoolResults vkGetQueryPoolResults;
extern PFN_vkGetRenderAreaGranularity vkGetRenderAreaGranularity;
extern PFN_vkInvalidateMappedMemoryRanges vkInvalidateMappedMemoryRanges;
extern PFN_vkMapMemory vkMapMemory;
extern PFN_vkMergePipelineCaches vkMergePipelineCaches;
extern PFN_vkQueueBindSparse vkQueueBindSparse;
extern PFN_vkQueueSubmit vkQueueSubmit;
extern PFN_vkQueueWaitIdle vkQueueWaitIdle;
extern PFN_vkResetCommandBuffer vkResetCommandBuffer;
extern PFN_vkResetCommandPool vkResetCommandPool;
extern PFN_vkResetDescriptorPool vkResetDescriptorPool;
extern PFN_vkResetEvent vkResetEvent;
extern PFN_vkResetFences vkResetFences;
extern PFN_vkSetEvent vkSetEvent;
extern PFN_vkUnmapMemory vkUnmapMemory;
extern PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
extern PFN_vkWaitForFences vkWaitForFences;
#endif /* defined(VK_VERSION_1_0) */
#if defined(VK_VERSION_1_1)
extern PFN_vkBindBufferMemory2 vkBindBufferMemory2;
extern PFN_vkBindImageMemory2 vkBindImageMemory2;
extern PFN_vkCmdDispatchBase vkCmdDispatchBase;
extern PFN_vkCmdSetDeviceMask vkCmdSetDeviceMask;
extern PFN_vkCreateDescriptorUpdateTemplate vkCreateDescriptorUpdateTemplate;
extern PFN_vkCreateSamplerYcbcrConversion vkCreateSamplerYcbcrConversion;
extern PFN_vkDestroyDescriptorUpdateTemplate vkDestroyDescriptorUpdateTemplate;
extern PFN_vkDestroySamplerYcbcrConversion vkDestroySamplerYcbcrConversion;
extern PFN_vkEnumerateInstanceVersion vkEnumerateInstanceVersion;
extern PFN_vkEnumeratePhysicalDeviceGroups vkEnumeratePhysicalDeviceGroups;
extern PFN_vkGetBufferMemoryRequirements2 vkGetBufferMemoryRequirements2;
extern PFN_vkGetDescriptorSetLayoutSupport vkGetDescriptorSetLayoutSupport;
extern PFN_vkGetDeviceGroupPeerMemoryFeatures vkGetDeviceGroupPeerMemoryFeatures;
extern PFN_vkGetDeviceQueue2 vkGetDeviceQueue2;
extern PFN_vkGetImageMemoryRequirements2 vkGetImageMemoryRequirements2;
extern PFN_vkGetImageSparseMemoryRequirements2 vkGetImageSparseMemoryRequirements2;
extern PFN_vkGetPhysicalDeviceExternalBufferProperties vkGetPhysicalDeviceExternalBufferProperties;
extern PFN_vkGetPhysicalDeviceExternalFenceProperties vkGetPhysicalDeviceExternalFenceProperties;
extern PFN_vkGetPhysicalDeviceExternalSemaphoreProperties vkGetPhysicalDeviceExternalSemaphoreProperties;
extern PFN_vkGetPhysicalDeviceFeatures2 vkGetPhysicalDeviceFeatures2;
extern PFN_vkGetPhysicalDeviceFormatProperties2 vkGetPhysicalDeviceFormatProperties2;
extern PFN_vkGetPhysicalDeviceImageFormatProperties2 vkGetPhysicalDeviceImageFormatProperties2;
extern PFN_vkGetPhysicalDeviceMemoryProperties2 vkGetPhysicalDeviceMemoryProperties2;
extern PFN_vkGetPhysicalDeviceProperties2 vkGetPhysicalDeviceProperties2;
extern PFN_vkGetPhysicalDeviceQueueFamilyProperties2 vkGetPhysicalDeviceQueueFamilyProperties2;
extern PFN_vkGetPhysicalDeviceSparseImageFormatProperties2 vkGetPhysicalDeviceSparseImageFormatProperties2;
extern PFN_vkTrimCommandPool vkTrimCommandPool;
extern PFN_vkUpdateDescriptorSetWithTemplate vkUpdateDescriptorSetWithTemplate;
#endif /* defined(VK_VERSION_1_1) */
#if defined(VK_VERSION_1_2)
extern PFN_vkCmdBeginRenderPass2 vkCmdBeginRenderPass2;
extern PFN_vkCmdDrawIndexedIndirectCount vkCmdDrawIndexedIndirectCount;
extern PFN_vkCmdDrawIndirectCount vkCmdDrawIndirectCount;
extern PFN_vkCmdEndRenderPass2 vkCmdEndRenderPass2;
extern PFN_vkCmdNextSubpass2 vkCmdNextSubpass2;
extern PFN_vkCreateRenderPass2 vkCreateRenderPass2;
extern PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress;
extern PFN_vkGetBufferOpaqueCaptureAddress vkGetBufferOpaqueCaptureAddress;
extern PFN_vkGetDeviceMemoryOpaqueCaptureAddress vkGetDeviceMemoryOpaqueCaptureAddress;
extern PFN_vkGetSemaphoreCounterValue vkGetSemaphoreCounterValue;
extern PFN_vkResetQueryPool vkResetQueryPool;
extern PFN_vkSignalSemaphore vkSignalSemaphore;
extern PFN_vkWaitSemaphores vkWaitSemaphores;
#endif /* defined(VK_VERSION_1_2) */
#if defined(VK_VERSION_1_3)
extern PFN_vkCmdBeginRendering vkCmdBeginRendering;
extern PFN_vkCmdBindVertexBuffers2 vkCmdBindVertexBuffers2;
extern PFN_vkCmdBlitImage2 vkCmdBlitImage2;
extern PFN_vkCmdCopyBuffer2 vkCmdCopyBuffer2;
extern PFN_vkCmdCopyBufferToImage2 vkCmdCopyBufferToImage2;
extern PFN_vkCmdCopyImage2 vkCmdCopyImage2;
extern PFN_vkCmdCopyImageToBuffer2 vkCmdCopyImageToBuffer2;
extern PFN_vkCmdEndRendering vkCmdEndRendering;
extern PFN_vkCmdPipelineBarrier2 vkCmdPipelineBarrier2;
extern PFN_vkCmdResetEvent2 vkCmdResetEvent2;
extern PFN_vkCmdResolveImage2 vkCmdResolveImage2;
extern PFN_vkCmdSetCullMode vkCmdSetCullMode;
extern PFN_vkCmdSetDepthBiasEnable vkCmdSetDepthBiasEnable;
extern PFN_vkCmdSetDepthBoundsTestEnable vkCmdSetDepthBoundsTestEnable;
extern PFN_vkCmdSetDepthCompareOp vkCmdSetDepthCompareOp;
extern PFN_vkCmdSetDepthTestEnable vkCmdSetDepthTestEnable;
extern PFN_vkCmdSetDepthWriteEnable vkCmdSetDepthWriteEnable;
extern PFN_vkCmdSetEvent2 vkCmdSetEvent2;
extern PFN_vkCmdSetFrontFace vkCmdSetFrontFace;
extern PFN_vkCmdSetPrimitiveRestartEnable vkCmdSetPrimitiveRestartEnable;
extern PFN_vkCmdSetPrimitiveTopology vkCmdSetPrimitiveTopology;
extern PFN_vkCmdSetRasterizerDiscardEnable vkCmdSetRasterizerDiscardEnable;
extern PFN_vkCmdSetScissorWithCount vkCmdSetScissorWithCount;
extern PFN_vkCmdSetStencilOp vkCmdSetStencilOp;
extern PFN_vkCmdSetStencilTestEnable vkCmdSetStencilTestEnable;
extern PFN_vkCmdSetViewportWithCount vkCmdSetViewportWithCount;
extern PFN_vkCmdWaitEvents2 vkCmdWaitEvents2;
extern PFN_vkCmdWriteTimestamp2 vkCmdWriteTimestamp2;
extern PFN_vkCreatePrivateDataSlot vkCreatePrivateDataSlot;
extern PFN_vkDestroyPrivateDataSlot vkDestroyPrivateDataSlot;
extern PFN_vkGetDeviceBufferMemoryRequirements vkGetDeviceBufferMemoryRequirements;
extern PFN_vkGetDeviceImageMemoryRequirements vkGetDeviceImageMemoryRequirements;
extern PFN_vkGetDeviceImageSparseMemoryRequirements vkGetDeviceImageSparseMemoryRequirements;
extern PFN_vkGetPhysicalDeviceToolProperties vkGetPhysicalDeviceToolProperties;
extern PFN_vkGetPrivateData vkGetPrivateData;
extern PFN_vkQueueSubmit2 vkQueueSubmit2;
extern PFN_vkSetPrivateData vkSetPrivateData;
#endif /* defined(VK_VERSION_1_3) */
#if defined(VK_VERSION_1_4)
extern PFN_vkCmdBindDescriptorSets2 vkCmdBindDescriptorSets2;
extern PFN_vkCmdBindIndexBuffer2 vkCmdBindIndexBuffer2;
extern PFN_vkCmdPushConstants2 vkCmdPushConstants2;
extern PFN_vkCmdPushDescriptorSet vkCmdPushDescriptorSet;
extern PFN_vkCmdPushDescriptorSet2 vkCmdPushDescriptorSet2;
extern PFN_vkCmdPushDescriptorSetWithTemplate vkCmdPushDescriptorSetWithTemplate;
extern PFN_vkCmdPushDescriptorSetWithTemplate2 vkCmdPushDescriptorSetWithTemplate2;
extern PFN_vkCmdSetLineStipple vkCmdSetLineStipple;
extern PFN_vkCmdSetRenderingAttachmentLocations vkCmdSetRenderingAttachmentLocations;
extern PFN_vkCmdSetRenderingInputAttachmentIndices vkCmdSetRenderingInputAttachmentIndices;
extern PFN_vkCopyImageToImage vkCopyImageToImage;
extern PFN_vkCopyImageToMemory vkCopyImageToMemory;
extern PFN_vkCopyMemoryToImage vkCopyMemoryToImage;
extern PFN_vkGetDeviceImageSubresourceLayout vkGetDeviceImageSubresourceLayout;
extern PFN_vkGetImageSubresourceLayout2 vkGetImageSubresourceLayout2;
extern PFN_vkGetRenderingAreaGranularity vkGetRenderingAreaGranularity;
extern PFN_vkMapMemory2 vkMapMemory2;
extern PFN_vkTransitionImageLayout vkTransitionImageLayout;
extern PFN_vkUnmapMemory2 vkUnmapMemory2;
#endif /* defined(VK_VERSION_1_4) */
#if defined(VK_AMDX_shader_enqueue)
extern PFN_vkCmdDispatchGraphAMDX vkCmdDispatchGraphAMDX;
extern PFN_vkCmdDispatchGraphIndirectAMDX vkCmdDispatchGraphIndirectAMDX;
extern PFN_vkCmdDispatchGraphIndirectCountAMDX vkCmdDispatchGraphIndirectCountAMDX;
extern PFN_vkCmdInitializeGraphScratchMemoryAMDX vkCmdInitializeGraphScratchMemoryAMDX;
extern PFN_vkCreateExecutionGraphPipelinesAMDX vkCreateExecutionGraphPipelinesAMDX;
extern PFN_vkGetExecutionGraphPipelineNodeIndexAMDX vkGetExecutionGraphPipelineNodeIndexAMDX;
extern PFN_vkGetExecutionGraphPipelineScratchSizeAMDX vkGetExecutionGraphPipelineScratchSizeAMDX;
#endif /* defined(VK_AMDX_shader_enqueue) */
#if defined(VK_AMD_anti_lag)
extern PFN_vkAntiLagUpdateAMD vkAntiLagUpdateAMD;
#endif /* defined(VK_AMD_anti_lag) */
#if defined(VK_AMD_buffer_marker)
extern PFN_vkCmdWriteBufferMarkerAMD vkCmdWriteBufferMarkerAMD;
#endif /* defined(VK_AMD_buffer_marker) */
#if defined(VK_AMD_buffer_marker) && (defined(VK_VERSION_1_3) || defined(VK_KHR_synchronization2))
extern PFN_vkCmdWriteBufferMarker2AMD vkCmdWriteBufferMarker2AMD;
#endif /* defined(VK_AMD_buffer_marker) && (defined(VK_VERSION_1_3) || defined(VK_KHR_synchronization2)) */
#if defined(VK_AMD_display_native_hdr)
extern PFN_vkSetLocalDimmingAMD vkSetLocalDimmingAMD;
#endif /* defined(VK_AMD_display_native_hdr) */
#if defined(VK_AMD_draw_indirect_count)
extern PFN_vkCmdDrawIndexedIndirectCountAMD vkCmdDrawIndexedIndirectCountAMD;
extern PFN_vkCmdDrawIndirectCountAMD vkCmdDrawIndirectCountAMD;
#endif /* defined(VK_AMD_draw_indirect_count) */
#if defined(VK_AMD_shader_info)
extern PFN_vkGetShaderInfoAMD vkGetShaderInfoAMD;
#endif /* defined(VK_AMD_shader_info) */
#if defined(VK_ANDROID_external_memory_android_hardware_buffer)
extern PFN_vkGetAndroidHardwareBufferPropertiesANDROID vkGetAndroidHardwareBufferPropertiesANDROID;
extern PFN_vkGetMemoryAndroidHardwareBufferANDROID vkGetMemoryAndroidHardwareBufferANDROID;
#endif /* defined(VK_ANDROID_external_memory_android_hardware_buffer) */
#if defined(VK_ARM_data_graph)
extern PFN_vkBindDataGraphPipelineSessionMemoryARM vkBindDataGraphPipelineSessionMemoryARM;
extern PFN_vkCmdDispatchDataGraphARM vkCmdDispatchDataGraphARM;
extern PFN_vkCreateDataGraphPipelineSessionARM vkCreateDataGraphPipelineSessionARM;
extern PFN_vkCreateDataGraphPipelinesARM vkCreateDataGraphPipelinesARM;
extern PFN_vkDestroyDataGraphPipelineSessionARM vkDestroyDataGraphPipelineSessionARM;
extern PFN_vkGetDataGraphPipelineAvailablePropertiesARM vkGetDataGraphPipelineAvailablePropertiesARM;
extern PFN_vkGetDataGraphPipelinePropertiesARM vkGetDataGraphPipelinePropertiesARM;
extern PFN_vkGetDataGraphPipelineSessionBindPointRequirementsARM vkGetDataGraphPipelineSessionBindPointRequirementsARM;
extern PFN_vkGetDataGraphPipelineSessionMemoryRequirementsARM vkGetDataGraphPipelineSessionMemoryRequirementsARM;
extern PFN_vkGetPhysicalDeviceQueueFamilyDataGraphProcessingEnginePropertiesARM vkGetPhysicalDeviceQueueFamilyDataGraphProcessingEnginePropertiesARM;
extern PFN_vkGetPhysicalDeviceQueueFamilyDataGraphPropertiesARM vkGetPhysicalDeviceQueueFamilyDataGraphPropertiesARM;
#endif /* defined(VK_ARM_data_graph) */
#if defined(VK_ARM_tensors)
extern PFN_vkBindTensorMemoryARM vkBindTensorMemoryARM;
extern PFN_vkCmdCopyTensorARM vkCmdCopyTensorARM;
extern PFN_vkCreateTensorARM vkCreateTensorARM;
extern PFN_vkCreateTensorViewARM vkCreateTensorViewARM;
extern PFN_vkDestroyTensorARM vkDestroyTensorARM;
extern PFN_vkDestroyTensorViewARM vkDestroyTensorViewARM;
extern PFN_vkGetDeviceTensorMemoryRequirementsARM vkGetDeviceTensorMemoryRequirementsARM;
extern PFN_vkGetPhysicalDeviceExternalTensorPropertiesARM vkGetPhysicalDeviceExternalTensorPropertiesARM;
extern PFN_vkGetTensorMemoryRequirementsARM vkGetTensorMemoryRequirementsARM;
#endif /* defined(VK_ARM_tensors) */
#if defined(VK_ARM_tensors) && defined(VK_EXT_descriptor_buffer)
extern PFN_vkGetTensorOpaqueCaptureDescriptorDataARM vkGetTensorOpaqueCaptureDescriptorDataARM;
extern PFN_vkGetTensorViewOpaqueCaptureDescriptorDataARM vkGetTensorViewOpaqueCaptureDescriptorDataARM;
#endif /* defined(VK_ARM_tensors) && defined(VK_EXT_descriptor_buffer) */
#if defined(VK_EXT_acquire_drm_display)
extern PFN_vkAcquireDrmDisplayEXT vkAcquireDrmDisplayEXT;
extern PFN_vkGetDrmDisplayEXT vkGetDrmDisplayEXT;
#endif /* defined(VK_EXT_acquire_drm_display) */
#if defined(VK_EXT_acquire_xlib_display)
extern PFN_vkAcquireXlibDisplayEXT vkAcquireXlibDisplayEXT;
extern PFN_vkGetRandROutputDisplayEXT vkGetRandROutputDisplayEXT;
#endif /* defined(VK_EXT_acquire_xlib_display) */
#if defined(VK_EXT_attachment_feedback_loop_dynamic_state)
extern PFN_vkCmdSetAttachmentFeedbackLoopEnableEXT vkCmdSetAttachmentFeedbackLoopEnableEXT;
#endif /* defined(VK_EXT_attachment_feedback_loop_dynamic_state) */
#if defined(VK_EXT_buffer_device_address)
extern PFN_vkGetBufferDeviceAddressEXT vkGetBufferDeviceAddressEXT;
#endif /* defined(VK_EXT_buffer_device_address) */
#if defined(VK_EXT_calibrated_timestamps)
extern PFN_vkGetCalibratedTimestampsEXT vkGetCalibratedTimestampsEXT;
extern PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT vkGetPhysicalDeviceCalibrateableTimeDomainsEXT;
#endif /* defined(VK_EXT_calibrated_timestamps) */
#if defined(VK_EXT_color_write_enable)
extern PFN_vkCmdSetColorWriteEnableEXT vkCmdSetColorWriteEnableEXT;
#endif /* defined(VK_EXT_color_write_enable) */
#if defined(VK_EXT_conditional_rendering)
extern PFN_vkCmdBeginConditionalRenderingEXT vkCmdBeginConditionalRenderingEXT;
extern PFN_vkCmdEndConditionalRenderingEXT vkCmdEndConditionalRenderingEXT;
#endif /* defined(VK_EXT_conditional_rendering) */
#if defined(VK_EXT_debug_marker)
extern PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBeginEXT;
extern PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEndEXT;
extern PFN_vkCmdDebugMarkerInsertEXT vkCmdDebugMarkerInsertEXT;
extern PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectNameEXT;
extern PFN_vkDebugMarkerSetObjectTagEXT vkDebugMarkerSetObjectTagEXT;
#endif /* defined(VK_EXT_debug_marker) */
#if defined(VK_EXT_debug_report)
extern PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
extern PFN_vkDebugReportMessageEXT vkDebugReportMessageEXT;
extern PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;
#endif /* defined(VK_EXT_debug_report) */
#if defined(VK_EXT_debug_utils)
extern PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT;
extern PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT;
extern PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT;
extern PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
extern PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
extern PFN_vkQueueBeginDebugUtilsLabelEXT vkQueueBeginDebugUtilsLabelEXT;
extern PFN_vkQueueEndDebugUtilsLabelEXT vkQueueEndDebugUtilsLabelEXT;
extern PFN_vkQueueInsertDebugUtilsLabelEXT vkQueueInsertDebugUtilsLabelEXT;
extern PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;
extern PFN_vkSetDebugUtilsObjectTagEXT vkSetDebugUtilsObjectTagEXT;
extern PFN_vkSubmitDebugUtilsMessageEXT vkSubmitDebugUtilsMessageEXT;
#endif /* defined(VK_EXT_debug_utils) */
#if defined(VK_EXT_depth_bias_control)
extern PFN_vkCmdSetDepthBias2EXT vkCmdSetDepthBias2EXT;
#endif /* defined(VK_EXT_depth_bias_control) */
#if defined(VK_EXT_descriptor_buffer)
extern PFN_vkCmdBindDescriptorBufferEmbeddedSamplersEXT vkCmdBindDescriptorBufferEmbeddedSamplersEXT;
extern PFN_vkCmdBindDescriptorBuffersEXT vkCmdBindDescriptorBuffersEXT;
extern PFN_vkCmdSetDescriptorBufferOffsetsEXT vkCmdSetDescriptorBufferOffsetsEXT;
extern PFN_vkGetBufferOpaqueCaptureDescriptorDataEXT vkGetBufferOpaqueCaptureDescriptorDataEXT;
extern PFN_vkGetDescriptorEXT vkGetDescriptorEXT;
extern PFN_vkGetDescriptorSetLayoutBindingOffsetEXT vkGetDescriptorSetLayoutBindingOffsetEXT;
extern PFN_vkGetDescriptorSetLayoutSizeEXT vkGetDescriptorSetLayoutSizeEXT;
extern PFN_vkGetImageOpaqueCaptureDescriptorDataEXT vkGetImageOpaqueCaptureDescriptorDataEXT;
extern PFN_vkGetImageViewOpaqueCaptureDescriptorDataEXT vkGetImageViewOpaqueCaptureDescriptorDataEXT;
extern PFN_vkGetSamplerOpaqueCaptureDescriptorDataEXT vkGetSamplerOpaqueCaptureDescriptorDataEXT;
#endif /* defined(VK_EXT_descriptor_buffer) */
#if defined(VK_EXT_descriptor_buffer) && (defined(VK_KHR_acceleration_structure) || defined(VK_NV_ray_tracing))
extern PFN_vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT;
#endif /* defined(VK_EXT_descriptor_buffer) && (defined(VK_KHR_acceleration_structure) || defined(VK_NV_ray_tracing)) */
#if defined(VK_EXT_device_fault)
extern PFN_vkGetDeviceFaultInfoEXT vkGetDeviceFaultInfoEXT;
#endif /* defined(VK_EXT_device_fault) */
#if defined(VK_EXT_device_generated_commands)
extern PFN_vkCmdExecuteGeneratedCommandsEXT vkCmdExecuteGeneratedCommandsEXT;
extern PFN_vkCmdPreprocessGeneratedCommandsEXT vkCmdPreprocessGeneratedCommandsEXT;
extern PFN_vkCreateIndirectCommandsLayoutEXT vkCreateIndirectCommandsLayoutEXT;
extern PFN_vkCreateIndirectExecutionSetEXT vkCreateIndirectExecutionSetEXT;
extern PFN_vkDestroyIndirectCommandsLayoutEXT vkDestroyIndirectCommandsLayoutEXT;
extern PFN_vkDestroyIndirectExecutionSetEXT vkDestroyIndirectExecutionSetEXT;
extern PFN_vkGetGeneratedCommandsMemoryRequirementsEXT vkGetGeneratedCommandsMemoryRequirementsEXT;
extern PFN_vkUpdateIndirectExecutionSetPipelineEXT vkUpdateIndirectExecutionSetPipelineEXT;
extern PFN_vkUpdateIndirectExecutionSetShaderEXT vkUpdateIndirectExecutionSetShaderEXT;
#endif /* defined(VK_EXT_device_generated_commands) */
#if defined(VK_EXT_direct_mode_display)
extern PFN_vkReleaseDisplayEXT vkReleaseDisplayEXT;
#endif /* defined(VK_EXT_direct_mode_display) */
#if defined(VK_EXT_directfb_surface)
extern PFN_vkCreateDirectFBSurfaceEXT vkCreateDirectFBSurfaceEXT;
extern PFN_vkGetPhysicalDeviceDirectFBPresentationSupportEXT vkGetPhysicalDeviceDirectFBPresentationSupportEXT;
#endif /* defined(VK_EXT_directfb_surface) */
#if defined(VK_EXT_discard_rectangles)
extern PFN_vkCmdSetDiscardRectangleEXT vkCmdSetDiscardRectangleEXT;
#endif /* defined(VK_EXT_discard_rectangles) */
#if defined(VK_EXT_discard_rectangles) && VK_EXT_DISCARD_RECTANGLES_SPEC_VERSION >= 2
extern PFN_vkCmdSetDiscardRectangleEnableEXT vkCmdSetDiscardRectangleEnableEXT;
extern PFN_vkCmdSetDiscardRectangleModeEXT vkCmdSetDiscardRectangleModeEXT;
#endif /* defined(VK_EXT_discard_rectangles) && VK_EXT_DISCARD_RECTANGLES_SPEC_VERSION >= 2 */
#if defined(VK_EXT_display_control)
extern PFN_vkDisplayPowerControlEXT vkDisplayPowerControlEXT;
extern PFN_vkGetSwapchainCounterEXT vkGetSwapchainCounterEXT;
extern PFN_vkRegisterDeviceEventEXT vkRegisterDeviceEventEXT;
extern PFN_vkRegisterDisplayEventEXT vkRegisterDisplayEventEXT;
#endif /* defined(VK_EXT_display_control) */
#if defined(VK_EXT_display_surface_counter)
extern PFN_vkGetPhysicalDeviceSurfaceCapabilities2EXT vkGetPhysicalDeviceSurfaceCapabilities2EXT;
#endif /* defined(VK_EXT_display_surface_counter) */
#if defined(VK_EXT_external_memory_host)
extern PFN_vkGetMemoryHostPointerPropertiesEXT vkGetMemoryHostPointerPropertiesEXT;
#endif /* defined(VK_EXT_external_memory_host) */
#if defined(VK_EXT_external_memory_metal)
extern PFN_vkGetMemoryMetalHandleEXT vkGetMemoryMetalHandleEXT;
extern PFN_vkGetMemoryMetalHandlePropertiesEXT vkGetMemoryMetalHandlePropertiesEXT;
#endif /* defined(VK_EXT_external_memory_metal) */
#if defined(VK_EXT_fragment_density_map_offset)
extern PFN_vkCmdEndRendering2EXT vkCmdEndRendering2EXT;
#endif /* defined(VK_EXT_fragment_density_map_offset) */
#if defined(VK_EXT_full_screen_exclusive)
extern PFN_vkAcquireFullScreenExclusiveModeEXT vkAcquireFullScreenExclusiveModeEXT;
extern PFN_vkGetPhysicalDeviceSurfacePresentModes2EXT vkGetPhysicalDeviceSurfacePresentModes2EXT;
extern PFN_vkReleaseFullScreenExclusiveModeEXT vkReleaseFullScreenExclusiveModeEXT;
#endif /* defined(VK_EXT_full_screen_exclusive) */
#if defined(VK_EXT_full_screen_exclusive) && (defined(VK_KHR_device_group) || defined(VK_VERSION_1_1))
extern PFN_vkGetDeviceGroupSurfacePresentModes2EXT vkGetDeviceGroupSurfacePresentModes2EXT;
#endif /* defined(VK_EXT_full_screen_exclusive) && (defined(VK_KHR_device_group) || defined(VK_VERSION_1_1)) */
#if defined(VK_EXT_hdr_metadata)
extern PFN_vkSetHdrMetadataEXT vkSetHdrMetadataEXT;
#endif /* defined(VK_EXT_hdr_metadata) */
#if defined(VK_EXT_headless_surface)
extern PFN_vkCreateHeadlessSurfaceEXT vkCreateHeadlessSurfaceEXT;
#endif /* defined(VK_EXT_headless_surface) */
#if defined(VK_EXT_host_image_copy)
extern PFN_vkCopyImageToImageEXT vkCopyImageToImageEXT;
extern PFN_vkCopyImageToMemoryEXT vkCopyImageToMemoryEXT;
extern PFN_vkCopyMemoryToImageEXT vkCopyMemoryToImageEXT;
extern PFN_vkTransitionImageLayoutEXT vkTransitionImageLayoutEXT;
#endif /* defined(VK_EXT_host_image_copy) */
#if defined(VK_EXT_host_query_reset)
extern PFN_vkResetQueryPoolEXT vkResetQueryPoolEXT;
#endif /* defined(VK_EXT_host_query_reset) */
#if defined(VK_EXT_image_drm_format_modifier)
extern PFN_vkGetImageDrmFormatModifierPropertiesEXT vkGetImageDrmFormatModifierPropertiesEXT;
#endif /* defined(VK_EXT_image_drm_format_modifier) */
#if defined(VK_EXT_line_rasterization)
extern PFN_vkCmdSetLineStippleEXT vkCmdSetLineStippleEXT;
#endif /* defined(VK_EXT_line_rasterization) */
#if defined(VK_EXT_mesh_shader)
extern PFN_vkCmdDrawMeshTasksEXT vkCmdDrawMeshTasksEXT;
extern PFN_vkCmdDrawMeshTasksIndirectEXT vkCmdDrawMeshTasksIndirectEXT;
#endif /* defined(VK_EXT_mesh_shader) */
#if defined(VK_EXT_mesh_shader) && (defined(VK_KHR_draw_indirect_count) || defined(VK_VERSION_1_2))
extern PFN_vkCmdDrawMeshTasksIndirectCountEXT vkCmdDrawMeshTasksIndirectCountEXT;
#endif /* defined(VK_EXT_mesh_shader) && (defined(VK_KHR_draw_indirect_count) || defined(VK_VERSION_1_2)) */
#if defined(VK_EXT_metal_objects)
extern PFN_vkExportMetalObjectsEXT vkExportMetalObjectsEXT;
#endif /* defined(VK_EXT_metal_objects) */
#if defined(VK_EXT_metal_surface)
extern PFN_vkCreateMetalSurfaceEXT vkCreateMetalSurfaceEXT;
#endif /* defined(VK_EXT_metal_surface) */
#if defined(VK_EXT_multi_draw)
extern PFN_vkCmdDrawMultiEXT vkCmdDrawMultiEXT;
extern PFN_vkCmdDrawMultiIndexedEXT vkCmdDrawMultiIndexedEXT;
#endif /* defined(VK_EXT_multi_draw) */
#if defined(VK_EXT_opacity_micromap)
extern PFN_vkBuildMicromapsEXT vkBuildMicromapsEXT;
extern PFN_vkCmdBuildMicromapsEXT vkCmdBuildMicromapsEXT;
extern PFN_vkCmdCopyMemoryToMicromapEXT vkCmdCopyMemoryToMicromapEXT;
extern PFN_vkCmdCopyMicromapEXT vkCmdCopyMicromapEXT;
extern PFN_vkCmdCopyMicromapToMemoryEXT vkCmdCopyMicromapToMemoryEXT;
extern PFN_vkCmdWriteMicromapsPropertiesEXT vkCmdWriteMicromapsPropertiesEXT;
extern PFN_vkCopyMemoryToMicromapEXT vkCopyMemoryToMicromapEXT;
extern PFN_vkCopyMicromapEXT vkCopyMicromapEXT;
extern PFN_vkCopyMicromapToMemoryEXT vkCopyMicromapToMemoryEXT;
extern PFN_vkCreateMicromapEXT vkCreateMicromapEXT;
extern PFN_vkDestroyMicromapEXT vkDestroyMicromapEXT;
extern PFN_vkGetDeviceMicromapCompatibilityEXT vkGetDeviceMicromapCompatibilityEXT;
extern PFN_vkGetMicromapBuildSizesEXT vkGetMicromapBuildSizesEXT;
extern PFN_vkWriteMicromapsPropertiesEXT vkWriteMicromapsPropertiesEXT;
#endif /* defined(VK_EXT_opacity_micromap) */
#if defined(VK_EXT_pageable_device_local_memory)
extern PFN_vkSetDeviceMemoryPriorityEXT vkSetDeviceMemoryPriorityEXT;
#endif /* defined(VK_EXT_pageable_device_local_memory) */
#if defined(VK_EXT_pipeline_properties)
extern PFN_vkGetPipelinePropertiesEXT vkGetPipelinePropertiesEXT;
#endif /* defined(VK_EXT_pipeline_properties) */
#if defined(VK_EXT_private_data)
extern PFN_vkCreatePrivateDataSlotEXT vkCreatePrivateDataSlotEXT;
extern PFN_vkDestroyPrivateDataSlotEXT vkDestroyPrivateDataSlotEXT;
extern PFN_vkGetPrivateDataEXT vkGetPrivateDataEXT;
extern PFN_vkSetPrivateDataEXT vkSetPrivateDataEXT;
#endif /* defined(VK_EXT_private_data) */
#if defined(VK_EXT_sample_locations)
extern PFN_vkCmdSetSampleLocationsEXT vkCmdSetSampleLocationsEXT;
extern PFN_vkGetPhysicalDeviceMultisamplePropertiesEXT vkGetPhysicalDeviceMultisamplePropertiesEXT;
#endif /* defined(VK_EXT_sample_locations) */
#if defined(VK_EXT_shader_module_identifier)
extern PFN_vkGetShaderModuleCreateInfoIdentifierEXT vkGetShaderModuleCreateInfoIdentifierEXT;
extern PFN_vkGetShaderModuleIdentifierEXT vkGetShaderModuleIdentifierEXT;
#endif /* defined(VK_EXT_shader_module_identifier) */
#if defined(VK_EXT_shader_object)
extern PFN_vkCmdBindShadersEXT vkCmdBindShadersEXT;
extern PFN_vkCreateShadersEXT vkCreateShadersEXT;
extern PFN_vkDestroyShaderEXT vkDestroyShaderEXT;
extern PFN_vkGetShaderBinaryDataEXT vkGetShaderBinaryDataEXT;
#endif /* defined(VK_EXT_shader_object) */
#if defined(VK_EXT_swapchain_maintenance1)
extern PFN_vkReleaseSwapchainImagesEXT vkReleaseSwapchainImagesEXT;
#endif /* defined(VK_EXT_swapchain_maintenance1) */
#if defined(VK_EXT_tooling_info)
extern PFN_vkGetPhysicalDeviceToolPropertiesEXT vkGetPhysicalDeviceToolPropertiesEXT;
#endif /* defined(VK_EXT_tooling_info) */
#if defined(VK_EXT_transform_feedback)
extern PFN_vkCmdBeginQueryIndexedEXT vkCmdBeginQueryIndexedEXT;
extern PFN_vkCmdBeginTransformFeedbackEXT vkCmdBeginTransformFeedbackEXT;
extern PFN_vkCmdBindTransformFeedbackBuffersEXT vkCmdBindTransformFeedbackBuffersEXT;
extern PFN_vkCmdDrawIndirectByteCountEXT vkCmdDrawIndirectByteCountEXT;
extern PFN_vkCmdEndQueryIndexedEXT vkCmdEndQueryIndexedEXT;
extern PFN_vkCmdEndTransformFeedbackEXT vkCmdEndTransformFeedbackEXT;
#endif /* defined(VK_EXT_transform_feedback) */
#if defined(VK_EXT_validation_cache)
extern PFN_vkCreateValidationCacheEXT vkCreateValidationCacheEXT;
extern PFN_vkDestroyValidationCacheEXT vkDestroyValidationCacheEXT;
extern PFN_vkGetValidationCacheDataEXT vkGetValidationCacheDataEXT;
extern PFN_vkMergeValidationCachesEXT vkMergeValidationCachesEXT;
#endif /* defined(VK_EXT_validation_cache) */
#if defined(VK_FUCHSIA_buffer_collection)
extern PFN_vkCreateBufferCollectionFUCHSIA vkCreateBufferCollectionFUCHSIA;
extern PFN_vkDestroyBufferCollectionFUCHSIA vkDestroyBufferCollectionFUCHSIA;
extern PFN_vkGetBufferCollectionPropertiesFUCHSIA vkGetBufferCollectionPropertiesFUCHSIA;
extern PFN_vkSetBufferCollectionBufferConstraintsFUCHSIA vkSetBufferCollectionBufferConstraintsFUCHSIA;
extern PFN_vkSetBufferCollectionImageConstraintsFUCHSIA vkSetBufferCollectionImageConstraintsFUCHSIA;
#endif /* defined(VK_FUCHSIA_buffer_collection) */
#if defined(VK_FUCHSIA_external_memory)
extern PFN_vkGetMemoryZirconHandleFUCHSIA vkGetMemoryZirconHandleFUCHSIA;
extern PFN_vkGetMemoryZirconHandlePropertiesFUCHSIA vkGetMemoryZirconHandlePropertiesFUCHSIA;
#endif /* defined(VK_FUCHSIA_external_memory) */
#if defined(VK_FUCHSIA_external_semaphore)
extern PFN_vkGetSemaphoreZirconHandleFUCHSIA vkGetSemaphoreZirconHandleFUCHSIA;
extern PFN_vkImportSemaphoreZirconHandleFUCHSIA vkImportSemaphoreZirconHandleFUCHSIA;
#endif /* defined(VK_FUCHSIA_external_semaphore) */
#if defined(VK_FUCHSIA_imagepipe_surface)
extern PFN_vkCreateImagePipeSurfaceFUCHSIA vkCreateImagePipeSurfaceFUCHSIA;
#endif /* defined(VK_FUCHSIA_imagepipe_surface) */
#if defined(VK_GGP_stream_descriptor_surface)
extern PFN_vkCreateStreamDescriptorSurfaceGGP vkCreateStreamDescriptorSurfaceGGP;
#endif /* defined(VK_GGP_stream_descriptor_surface) */
#if defined(VK_GOOGLE_display_timing)
extern PFN_vkGetPastPresentationTimingGOOGLE vkGetPastPresentationTimingGOOGLE;
extern PFN_vkGetRefreshCycleDurationGOOGLE vkGetRefreshCycleDurationGOOGLE;
#endif /* defined(VK_GOOGLE_display_timing) */
#if defined(VK_HUAWEI_cluster_culling_shader)
extern PFN_vkCmdDrawClusterHUAWEI vkCmdDrawClusterHUAWEI;
extern PFN_vkCmdDrawClusterIndirectHUAWEI vkCmdDrawClusterIndirectHUAWEI;
#endif /* defined(VK_HUAWEI_cluster_culling_shader) */
#if defined(VK_HUAWEI_invocation_mask)
extern PFN_vkCmdBindInvocationMaskHUAWEI vkCmdBindInvocationMaskHUAWEI;
#endif /* defined(VK_HUAWEI_invocation_mask) */
#if defined(VK_HUAWEI_subpass_shading) && VK_HUAWEI_SUBPASS_SHADING_SPEC_VERSION >= 2
extern PFN_vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI;
#endif /* defined(VK_HUAWEI_subpass_shading) && VK_HUAWEI_SUBPASS_SHADING_SPEC_VERSION >= 2 */
#if defined(VK_HUAWEI_subpass_shading)
extern PFN_vkCmdSubpassShadingHUAWEI vkCmdSubpassShadingHUAWEI;
#endif /* defined(VK_HUAWEI_subpass_shading) */
#if defined(VK_INTEL_performance_query)
extern PFN_vkAcquirePerformanceConfigurationINTEL vkAcquirePerformanceConfigurationINTEL;
extern PFN_vkCmdSetPerformanceMarkerINTEL vkCmdSetPerformanceMarkerINTEL;
extern PFN_vkCmdSetPerformanceOverrideINTEL vkCmdSetPerformanceOverrideINTEL;
extern PFN_vkCmdSetPerformanceStreamMarkerINTEL vkCmdSetPerformanceStreamMarkerINTEL;
extern PFN_vkGetPerformanceParameterINTEL vkGetPerformanceParameterINTEL;
extern PFN_vkInitializePerformanceApiINTEL vkInitializePerformanceApiINTEL;
extern PFN_vkQueueSetPerformanceConfigurationINTEL vkQueueSetPerformanceConfigurationINTEL;
extern PFN_vkReleasePerformanceConfigurationINTEL vkReleasePerformanceConfigurationINTEL;
extern PFN_vkUninitializePerformanceApiINTEL vkUninitializePerformanceApiINTEL;
#endif /* defined(VK_INTEL_performance_query) */
#if defined(VK_KHR_acceleration_structure)
extern PFN_vkBuildAccelerationStructuresKHR vkBuildAccelerationStructuresKHR;
extern PFN_vkCmdBuildAccelerationStructuresIndirectKHR vkCmdBuildAccelerationStructuresIndirectKHR;
extern PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
extern PFN_vkCmdCopyAccelerationStructureKHR vkCmdCopyAccelerationStructureKHR;
extern PFN_vkCmdCopyAccelerationStructureToMemoryKHR vkCmdCopyAccelerationStructureToMemoryKHR;
extern PFN_vkCmdCopyMemoryToAccelerationStructureKHR vkCmdCopyMemoryToAccelerationStructureKHR;
extern PFN_vkCmdWriteAccelerationStructuresPropertiesKHR vkCmdWriteAccelerationStructuresPropertiesKHR;
extern PFN_vkCopyAccelerationStructureKHR vkCopyAccelerationStructureKHR;
extern PFN_vkCopyAccelerationStructureToMemoryKHR vkCopyAccelerationStructureToMemoryKHR;
extern PFN_vkCopyMemoryToAccelerationStructureKHR vkCopyMemoryToAccelerationStructureKHR;
extern PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
extern PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
extern PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
extern PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
extern PFN_vkGetDeviceAccelerationStructureCompatibilityKHR vkGetDeviceAccelerationStructureCompatibilityKHR;
extern PFN_vkWriteAccelerationStructuresPropertiesKHR vkWriteAccelerationStructuresPropertiesKHR;
#endif /* defined(VK_KHR_acceleration_structure) */
#if defined(VK_KHR_android_surface)
extern PFN_vkCreateAndroidSurfaceKHR vkCreateAndroidSurfaceKHR;
#endif /* defined(VK_KHR_android_surface) */
#if defined(VK_KHR_bind_memory2)
extern PFN_vkBindBufferMemory2KHR vkBindBufferMemory2KHR;
extern PFN_vkBindImageMemory2KHR vkBindImageMemory2KHR;
#endif /* defined(VK_KHR_bind_memory2) */
#if defined(VK_KHR_buffer_device_address)
extern PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
extern PFN_vkGetBufferOpaqueCaptureAddressKHR vkGetBufferOpaqueCaptureAddressKHR;
extern PFN_vkGetDeviceMemoryOpaqueCaptureAddressKHR vkGetDeviceMemoryOpaqueCaptureAddressKHR;
#endif /* defined(VK_KHR_buffer_device_address) */
#if defined(VK_KHR_calibrated_timestamps)
extern PFN_vkGetCalibratedTimestampsKHR vkGetCalibratedTimestampsKHR;
extern PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR vkGetPhysicalDeviceCalibrateableTimeDomainsKHR;
#endif /* defined(VK_KHR_calibrated_timestamps) */
#if defined(VK_KHR_cooperative_matrix)
extern PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR;
#endif /* defined(VK_KHR_cooperative_matrix) */
#if defined(VK_KHR_copy_commands2)
extern PFN_vkCmdBlitImage2KHR vkCmdBlitImage2KHR;
extern PFN_vkCmdCopyBuffer2KHR vkCmdCopyBuffer2KHR;
extern PFN_vkCmdCopyBufferToImage2KHR vkCmdCopyBufferToImage2KHR;
extern PFN_vkCmdCopyImage2KHR vkCmdCopyImage2KHR;
extern PFN_vkCmdCopyImageToBuffer2KHR vkCmdCopyImageToBuffer2KHR;
extern PFN_vkCmdResolveImage2KHR vkCmdResolveImage2KHR;
#endif /* defined(VK_KHR_copy_commands2) */
#if defined(VK_KHR_create_renderpass2)
extern PFN_vkCmdBeginRenderPass2KHR vkCmdBeginRenderPass2KHR;
extern PFN_vkCmdEndRenderPass2KHR vkCmdEndRenderPass2KHR;
extern PFN_vkCmdNextSubpass2KHR vkCmdNextSubpass2KHR;
extern PFN_vkCreateRenderPass2KHR vkCreateRenderPass2KHR;
#endif /* defined(VK_KHR_create_renderpass2) */
#if defined(VK_KHR_deferred_host_operations)
extern PFN_vkCreateDeferredOperationKHR vkCreateDeferredOperationKHR;
extern PFN_vkDeferredOperationJoinKHR vkDeferredOperationJoinKHR;
extern PFN_vkDestroyDeferredOperationKHR vkDestroyDeferredOperationKHR;
extern PFN_vkGetDeferredOperationMaxConcurrencyKHR vkGetDeferredOperationMaxConcurrencyKHR;
extern PFN_vkGetDeferredOperationResultKHR vkGetDeferredOperationResultKHR;
#endif /* defined(VK_KHR_deferred_host_operations) */
#if defined(VK_KHR_descriptor_update_template)
extern PFN_vkCreateDescriptorUpdateTemplateKHR vkCreateDescriptorUpdateTemplateKHR;
extern PFN_vkDestroyDescriptorUpdateTemplateKHR vkDestroyDescriptorUpdateTemplateKHR;
extern PFN_vkUpdateDescriptorSetWithTemplateKHR vkUpdateDescriptorSetWithTemplateKHR;
#endif /* defined(VK_KHR_descriptor_update_template) */
#if defined(VK_KHR_device_group)
extern PFN_vkCmdDispatchBaseKHR vkCmdDispatchBaseKHR;
extern PFN_vkCmdSetDeviceMaskKHR vkCmdSetDeviceMaskKHR;
extern PFN_vkGetDeviceGroupPeerMemoryFeaturesKHR vkGetDeviceGroupPeerMemoryFeaturesKHR;
#endif /* defined(VK_KHR_device_group) */
#if defined(VK_KHR_device_group_creation)
extern PFN_vkEnumeratePhysicalDeviceGroupsKHR vkEnumeratePhysicalDeviceGroupsKHR;
#endif /* defined(VK_KHR_device_group_creation) */
#if defined(VK_KHR_display)
extern PFN_vkCreateDisplayModeKHR vkCreateDisplayModeKHR;
extern PFN_vkCreateDisplayPlaneSurfaceKHR vkCreateDisplayPlaneSurfaceKHR;
extern PFN_vkGetDisplayModePropertiesKHR vkGetDisplayModePropertiesKHR;
extern PFN_vkGetDisplayPlaneCapabilitiesKHR vkGetDisplayPlaneCapabilitiesKHR;
extern PFN_vkGetDisplayPlaneSupportedDisplaysKHR vkGetDisplayPlaneSupportedDisplaysKHR;
extern PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR vkGetPhysicalDeviceDisplayPlanePropertiesKHR;
extern PFN_vkGetPhysicalDeviceDisplayPropertiesKHR vkGetPhysicalDeviceDisplayPropertiesKHR;
#endif /* defined(VK_KHR_display) */
#if defined(VK_KHR_display_swapchain)
extern PFN_vkCreateSharedSwapchainsKHR vkCreateSharedSwapchainsKHR;
#endif /* defined(VK_KHR_display_swapchain) */
#if defined(VK_KHR_draw_indirect_count)
extern PFN_vkCmdDrawIndexedIndirectCountKHR vkCmdDrawIndexedIndirectCountKHR;
extern PFN_vkCmdDrawIndirectCountKHR vkCmdDrawIndirectCountKHR;
#endif /* defined(VK_KHR_draw_indirect_count) */
#if defined(VK_KHR_dynamic_rendering)
extern PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR;
extern PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR;
#endif /* defined(VK_KHR_dynamic_rendering) */
#if defined(VK_KHR_dynamic_rendering_local_read)
extern PFN_vkCmdSetRenderingAttachmentLocationsKHR vkCmdSetRenderingAttachmentLocationsKHR;
extern PFN_vkCmdSetRenderingInputAttachmentIndicesKHR vkCmdSetRenderingInputAttachmentIndicesKHR;
#endif /* defined(VK_KHR_dynamic_rendering_local_read) */
#if defined(VK_KHR_external_fence_capabilities)
extern PFN_vkGetPhysicalDeviceExternalFencePropertiesKHR vkGetPhysicalDeviceExternalFencePropertiesKHR;
#endif /* defined(VK_KHR_external_fence_capabilities) */
#if defined(VK_KHR_external_fence_fd)
extern PFN_vkGetFenceFdKHR vkGetFenceFdKHR;
extern PFN_vkImportFenceFdKHR vkImportFenceFdKHR;
#endif /* defined(VK_KHR_external_fence_fd) */
#if defined(VK_KHR_external_fence_win32)
extern PFN_vkGetFenceWin32HandleKHR vkGetFenceWin32HandleKHR;
extern PFN_vkImportFenceWin32HandleKHR vkImportFenceWin32HandleKHR;
#endif /* defined(VK_KHR_external_fence_win32) */
#if defined(VK_KHR_external_memory_capabilities)
extern PFN_vkGetPhysicalDeviceExternalBufferPropertiesKHR vkGetPhysicalDeviceExternalBufferPropertiesKHR;
#endif /* defined(VK_KHR_external_memory_capabilities) */
#if defined(VK_KHR_external_memory_fd)
extern PFN_vkGetMemoryFdKHR vkGetMemoryFdKHR;
extern PFN_vkGetMemoryFdPropertiesKHR vkGetMemoryFdPropertiesKHR;
#endif /* defined(VK_KHR_external_memory_fd) */
#if defined(VK_KHR_external_memory_win32)
extern PFN_vkGetMemoryWin32HandleKHR vkGetMemoryWin32HandleKHR;
extern PFN_vkGetMemoryWin32HandlePropertiesKHR vkGetMemoryWin32HandlePropertiesKHR;
#endif /* defined(VK_KHR_external_memory_win32) */
#if defined(VK_KHR_external_semaphore_capabilities)
extern PFN_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR vkGetPhysicalDeviceExternalSemaphorePropertiesKHR;
#endif /* defined(VK_KHR_external_semaphore_capabilities) */
#if defined(VK_KHR_external_semaphore_fd)
extern PFN_vkGetSemaphoreFdKHR vkGetSemaphoreFdKHR;
extern PFN_vkImportSemaphoreFdKHR vkImportSemaphoreFdKHR;
#endif /* defined(VK_KHR_external_semaphore_fd) */
#if defined(VK_KHR_external_semaphore_win32)
extern PFN_vkGetSemaphoreWin32HandleKHR vkGetSemaphoreWin32HandleKHR;
extern PFN_vkImportSemaphoreWin32HandleKHR vkImportSemaphoreWin32HandleKHR;
#endif /* defined(VK_KHR_external_semaphore_win32) */
#if defined(VK_KHR_fragment_shading_rate)
extern PFN_vkCmdSetFragmentShadingRateKHR vkCmdSetFragmentShadingRateKHR;
extern PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR vkGetPhysicalDeviceFragmentShadingRatesKHR;
#endif /* defined(VK_KHR_fragment_shading_rate) */
#if defined(VK_KHR_get_display_properties2)
extern PFN_vkGetDisplayModeProperties2KHR vkGetDisplayModeProperties2KHR;
extern PFN_vkGetDisplayPlaneCapabilities2KHR vkGetDisplayPlaneCapabilities2KHR;
extern PFN_vkGetPhysicalDeviceDisplayPlaneProperties2KHR vkGetPhysicalDeviceDisplayPlaneProperties2KHR;
extern PFN_vkGetPhysicalDeviceDisplayProperties2KHR vkGetPhysicalDeviceDisplayProperties2KHR;
#endif /* defined(VK_KHR_get_display_properties2) */
#if defined(VK_KHR_get_memory_requirements2)
extern PFN_vkGetBufferMemoryRequirements2KHR vkGetBufferMemoryRequirements2KHR;
extern PFN_vkGetImageMemoryRequirements2KHR vkGetImageMemoryRequirements2KHR;
extern PFN_vkGetImageSparseMemoryRequirements2KHR vkGetImageSparseMemoryRequirements2KHR;
#endif /* defined(VK_KHR_get_memory_requirements2) */
#if defined(VK_KHR_get_physical_device_properties2)
extern PFN_vkGetPhysicalDeviceFeatures2KHR vkGetPhysicalDeviceFeatures2KHR;
extern PFN_vkGetPhysicalDeviceFormatProperties2KHR vkGetPhysicalDeviceFormatProperties2KHR;
extern PFN_vkGetPhysicalDeviceImageFormatProperties2KHR vkGetPhysicalDeviceImageFormatProperties2KHR;
extern PFN_vkGetPhysicalDeviceMemoryProperties2KHR vkGetPhysicalDeviceMemoryProperties2KHR;
extern PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR;
extern PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR vkGetPhysicalDeviceQueueFamilyProperties2KHR;
extern PFN_vkGetPhysicalDeviceSparseImageFormatProperties2KHR vkGetPhysicalDeviceSparseImageFormatProperties2KHR;
#endif /* defined(VK_KHR_get_physical_device_properties2) */
#if defined(VK_KHR_get_surface_capabilities2)
extern PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR vkGetPhysicalDeviceSurfaceCapabilities2KHR;
extern PFN_vkGetPhysicalDeviceSurfaceFormats2KHR vkGetPhysicalDeviceSurfaceFormats2KHR;
#endif /* defined(VK_KHR_get_surface_capabilities2) */
#if defined(VK_KHR_line_rasterization)
extern PFN_vkCmdSetLineStippleKHR vkCmdSetLineStippleKHR;
#endif /* defined(VK_KHR_line_rasterization) */
#if defined(VK_KHR_maintenance1)
extern PFN_vkTrimCommandPoolKHR vkTrimCommandPoolKHR;
#endif /* defined(VK_KHR_maintenance1) */
#if defined(VK_KHR_maintenance3)
extern PFN_vkGetDescriptorSetLayoutSupportKHR vkGetDescriptorSetLayoutSupportKHR;
#endif /* defined(VK_KHR_maintenance3) */
#if defined(VK_KHR_maintenance4)
extern PFN_vkGetDeviceBufferMemoryRequirementsKHR vkGetDeviceBufferMemoryRequirementsKHR;
extern PFN_vkGetDeviceImageMemoryRequirementsKHR vkGetDeviceImageMemoryRequirementsKHR;
extern PFN_vkGetDeviceImageSparseMemoryRequirementsKHR vkGetDeviceImageSparseMemoryRequirementsKHR;
#endif /* defined(VK_KHR_maintenance4) */
#if defined(VK_KHR_maintenance5)
extern PFN_vkCmdBindIndexBuffer2KHR vkCmdBindIndexBuffer2KHR;
extern PFN_vkGetDeviceImageSubresourceLayoutKHR vkGetDeviceImageSubresourceLayoutKHR;
extern PFN_vkGetImageSubresourceLayout2KHR vkGetImageSubresourceLayout2KHR;
extern PFN_vkGetRenderingAreaGranularityKHR vkGetRenderingAreaGranularityKHR;
#endif /* defined(VK_KHR_maintenance5) */
#if defined(VK_KHR_maintenance6)
extern PFN_vkCmdBindDescriptorSets2KHR vkCmdBindDescriptorSets2KHR;
extern PFN_vkCmdPushConstants2KHR vkCmdPushConstants2KHR;
#endif /* defined(VK_KHR_maintenance6) */
#if defined(VK_KHR_maintenance6) && defined(VK_KHR_push_descriptor)
extern PFN_vkCmdPushDescriptorSet2KHR vkCmdPushDescriptorSet2KHR;
extern PFN_vkCmdPushDescriptorSetWithTemplate2KHR vkCmdPushDescriptorSetWithTemplate2KHR;
#endif /* defined(VK_KHR_maintenance6) && defined(VK_KHR_push_descriptor) */
#if defined(VK_KHR_maintenance6) && defined(VK_EXT_descriptor_buffer)
extern PFN_vkCmdBindDescriptorBufferEmbeddedSamplers2EXT vkCmdBindDescriptorBufferEmbeddedSamplers2EXT;
extern PFN_vkCmdSetDescriptorBufferOffsets2EXT vkCmdSetDescriptorBufferOffsets2EXT;
#endif /* defined(VK_KHR_maintenance6) && defined(VK_EXT_descriptor_buffer) */
#if defined(VK_KHR_map_memory2)
extern PFN_vkMapMemory2KHR vkMapMemory2KHR;
extern PFN_vkUnmapMemory2KHR vkUnmapMemory2KHR;
#endif /* defined(VK_KHR_map_memory2) */
#if defined(VK_KHR_performance_query)
extern PFN_vkAcquireProfilingLockKHR vkAcquireProfilingLockKHR;
extern PFN_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR;
extern PFN_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR;
extern PFN_vkReleaseProfilingLockKHR vkReleaseProfilingLockKHR;
#endif /* defined(VK_KHR_performance_query) */
#if defined(VK_KHR_pipeline_binary)
extern PFN_vkCreatePipelineBinariesKHR vkCreatePipelineBinariesKHR;
extern PFN_vkDestroyPipelineBinaryKHR vkDestroyPipelineBinaryKHR;
extern PFN_vkGetPipelineBinaryDataKHR vkGetPipelineBinaryDataKHR;
extern PFN_vkGetPipelineKeyKHR vkGetPipelineKeyKHR;
extern PFN_vkReleaseCapturedPipelineDataKHR vkReleaseCapturedPipelineDataKHR;
#endif /* defined(VK_KHR_pipeline_binary) */
#if defined(VK_KHR_pipeline_executable_properties)
extern PFN_vkGetPipelineExecutableInternalRepresentationsKHR vkGetPipelineExecutableInternalRepresentationsKHR;
extern PFN_vkGetPipelineExecutablePropertiesKHR vkGetPipelineExecutablePropertiesKHR;
extern PFN_vkGetPipelineExecutableStatisticsKHR vkGetPipelineExecutableStatisticsKHR;
#endif /* defined(VK_KHR_pipeline_executable_properties) */
#if defined(VK_KHR_present_wait)
extern PFN_vkWaitForPresentKHR vkWaitForPresentKHR;
#endif /* defined(VK_KHR_present_wait) */
#if defined(VK_KHR_present_wait2)
extern PFN_vkWaitForPresent2KHR vkWaitForPresent2KHR;
#endif /* defined(VK_KHR_present_wait2) */
#if defined(VK_KHR_push_descriptor)
extern PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR;
#endif /* defined(VK_KHR_push_descriptor) */
#if defined(VK_KHR_ray_tracing_maintenance1) && defined(VK_KHR_ray_tracing_pipeline)
extern PFN_vkCmdTraceRaysIndirect2KHR vkCmdTraceRaysIndirect2KHR;
#endif /* defined(VK_KHR_ray_tracing_maintenance1) && defined(VK_KHR_ray_tracing_pipeline) */
#if defined(VK_KHR_ray_tracing_pipeline)
extern PFN_vkCmdSetRayTracingPipelineStackSizeKHR vkCmdSetRayTracingPipelineStackSizeKHR;
extern PFN_vkCmdTraceRaysIndirectKHR vkCmdTraceRaysIndirectKHR;
extern PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
extern PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
extern PFN_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR vkGetRayTracingCaptureReplayShaderGroupHandlesKHR;
extern PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
extern PFN_vkGetRayTracingShaderGroupStackSizeKHR vkGetRayTracingShaderGroupStackSizeKHR;
#endif /* defined(VK_KHR_ray_tracing_pipeline) */
#if defined(VK_KHR_sampler_ycbcr_conversion)
extern PFN_vkCreateSamplerYcbcrConversionKHR vkCreateSamplerYcbcrConversionKHR;
extern PFN_vkDestroySamplerYcbcrConversionKHR vkDestroySamplerYcbcrConversionKHR;
#endif /* defined(VK_KHR_sampler_ycbcr_conversion) */
#if defined(VK_KHR_shared_presentable_image)
extern PFN_vkGetSwapchainStatusKHR vkGetSwapchainStatusKHR;
#endif /* defined(VK_KHR_shared_presentable_image) */
#if defined(VK_KHR_surface)
extern PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR;
extern PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
extern PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
extern PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;
extern PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
#endif /* defined(VK_KHR_surface) */
#if defined(VK_KHR_swapchain)
extern PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
extern PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
extern PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
extern PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
extern PFN_vkQueuePresentKHR vkQueuePresentKHR;
#endif /* defined(VK_KHR_swapchain) */
#if defined(VK_KHR_swapchain_maintenance1)
extern PFN_vkReleaseSwapchainImagesKHR vkReleaseSwapchainImagesKHR;
#endif /* defined(VK_KHR_swapchain_maintenance1) */
#if defined(VK_KHR_synchronization2)
extern PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR;
extern PFN_vkCmdResetEvent2KHR vkCmdResetEvent2KHR;
extern PFN_vkCmdSetEvent2KHR vkCmdSetEvent2KHR;
extern PFN_vkCmdWaitEvents2KHR vkCmdWaitEvents2KHR;
extern PFN_vkCmdWriteTimestamp2KHR vkCmdWriteTimestamp2KHR;
extern PFN_vkQueueSubmit2KHR vkQueueSubmit2KHR;
#endif /* defined(VK_KHR_synchronization2) */
#if defined(VK_KHR_timeline_semaphore)
extern PFN_vkGetSemaphoreCounterValueKHR vkGetSemaphoreCounterValueKHR;
extern PFN_vkSignalSemaphoreKHR vkSignalSemaphoreKHR;
extern PFN_vkWaitSemaphoresKHR vkWaitSemaphoresKHR;
#endif /* defined(VK_KHR_timeline_semaphore) */
#if defined(VK_KHR_video_decode_queue)
extern PFN_vkCmdDecodeVideoKHR vkCmdDecodeVideoKHR;
#endif /* defined(VK_KHR_video_decode_queue) */
#if defined(VK_KHR_video_encode_queue)
extern PFN_vkCmdEncodeVideoKHR vkCmdEncodeVideoKHR;
extern PFN_vkGetEncodedVideoSessionParametersKHR vkGetEncodedVideoSessionParametersKHR;
extern PFN_vkGetPhysicalDeviceVideoEncodeQualityLevelPropertiesKHR vkGetPhysicalDeviceVideoEncodeQualityLevelPropertiesKHR;
#endif /* defined(VK_KHR_video_encode_queue) */
#if defined(VK_KHR_video_queue)
extern PFN_vkBindVideoSessionMemoryKHR vkBindVideoSessionMemoryKHR;
extern PFN_vkCmdBeginVideoCodingKHR vkCmdBeginVideoCodingKHR;
extern PFN_vkCmdControlVideoCodingKHR vkCmdControlVideoCodingKHR;
extern PFN_vkCmdEndVideoCodingKHR vkCmdEndVideoCodingKHR;
extern PFN_vkCreateVideoSessionKHR vkCreateVideoSessionKHR;
extern PFN_vkCreateVideoSessionParametersKHR vkCreateVideoSessionParametersKHR;
extern PFN_vkDestroyVideoSessionKHR vkDestroyVideoSessionKHR;
extern PFN_vkDestroyVideoSessionParametersKHR vkDestroyVideoSessionParametersKHR;
extern PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR vkGetPhysicalDeviceVideoCapabilitiesKHR;
extern PFN_vkGetPhysicalDeviceVideoFormatPropertiesKHR vkGetPhysicalDeviceVideoFormatPropertiesKHR;
extern PFN_vkGetVideoSessionMemoryRequirementsKHR vkGetVideoSessionMemoryRequirementsKHR;
extern PFN_vkUpdateVideoSessionParametersKHR vkUpdateVideoSessionParametersKHR;
#endif /* defined(VK_KHR_video_queue) */
#if defined(VK_KHR_wayland_surface)
extern PFN_vkCreateWaylandSurfaceKHR vkCreateWaylandSurfaceKHR;
extern PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR vkGetPhysicalDeviceWaylandPresentationSupportKHR;
#endif /* defined(VK_KHR_wayland_surface) */
#if defined(VK_KHR_win32_surface)
extern PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR;
extern PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR vkGetPhysicalDeviceWin32PresentationSupportKHR;
#endif /* defined(VK_KHR_win32_surface) */
#if defined(VK_KHR_xcb_surface)
extern PFN_vkCreateXcbSurfaceKHR vkCreateXcbSurfaceKHR;
extern PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR vkGetPhysicalDeviceXcbPresentationSupportKHR;
#endif /* defined(VK_KHR_xcb_surface) */
#if defined(VK_KHR_xlib_surface)
extern PFN_vkCreateXlibSurfaceKHR vkCreateXlibSurfaceKHR;
extern PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR vkGetPhysicalDeviceXlibPresentationSupportKHR;
#endif /* defined(VK_KHR_xlib_surface) */
#if defined(VK_MVK_ios_surface)
extern PFN_vkCreateIOSSurfaceMVK vkCreateIOSSurfaceMVK;
#endif /* defined(VK_MVK_ios_surface) */
#if defined(VK_MVK_macos_surface)
extern PFN_vkCreateMacOSSurfaceMVK vkCreateMacOSSurfaceMVK;
#endif /* defined(VK_MVK_macos_surface) */
#if defined(VK_NN_vi_surface)
extern PFN_vkCreateViSurfaceNN vkCreateViSurfaceNN;
#endif /* defined(VK_NN_vi_surface) */
#if defined(VK_NVX_binary_import)
extern PFN_vkCmdCuLaunchKernelNVX vkCmdCuLaunchKernelNVX;
extern PFN_vkCreateCuFunctionNVX vkCreateCuFunctionNVX;
extern PFN_vkCreateCuModuleNVX vkCreateCuModuleNVX;
extern PFN_vkDestroyCuFunctionNVX vkDestroyCuFunctionNVX;
extern PFN_vkDestroyCuModuleNVX vkDestroyCuModuleNVX;
#endif /* defined(VK_NVX_binary_import) */
#if defined(VK_NVX_image_view_handle)
extern PFN_vkGetImageViewHandleNVX vkGetImageViewHandleNVX;
#endif /* defined(VK_NVX_image_view_handle) */
#if defined(VK_NVX_image_view_handle) && VK_NVX_IMAGE_VIEW_HANDLE_SPEC_VERSION >= 3
extern PFN_vkGetImageViewHandle64NVX vkGetImageViewHandle64NVX;
#endif /* defined(VK_NVX_image_view_handle) && VK_NVX_IMAGE_VIEW_HANDLE_SPEC_VERSION >= 3 */
#if defined(VK_NVX_image_view_handle) && VK_NVX_IMAGE_VIEW_HANDLE_SPEC_VERSION >= 2
extern PFN_vkGetImageViewAddressNVX vkGetImageViewAddressNVX;
#endif /* defined(VK_NVX_image_view_handle) && VK_NVX_IMAGE_VIEW_HANDLE_SPEC_VERSION >= 2 */
#if defined(VK_NV_acquire_winrt_display)
extern PFN_vkAcquireWinrtDisplayNV vkAcquireWinrtDisplayNV;
extern PFN_vkGetWinrtDisplayNV vkGetWinrtDisplayNV;
#endif /* defined(VK_NV_acquire_winrt_display) */
#if defined(VK_NV_clip_space_w_scaling)
extern PFN_vkCmdSetViewportWScalingNV vkCmdSetViewportWScalingNV;
#endif /* defined(VK_NV_clip_space_w_scaling) */
#if defined(VK_NV_cluster_acceleration_structure)
extern PFN_vkCmdBuildClusterAccelerationStructureIndirectNV vkCmdBuildClusterAccelerationStructureIndirectNV;
extern PFN_vkGetClusterAccelerationStructureBuildSizesNV vkGetClusterAccelerationStructureBuildSizesNV;
#endif /* defined(VK_NV_cluster_acceleration_structure) */
#if defined(VK_NV_cooperative_matrix)
extern PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV vkGetPhysicalDeviceCooperativeMatrixPropertiesNV;
#endif /* defined(VK_NV_cooperative_matrix) */
#if defined(VK_NV_cooperative_matrix2)
extern PFN_vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV;
#endif /* defined(VK_NV_cooperative_matrix2) */
#if defined(VK_NV_cooperative_vector)
extern PFN_vkCmdConvertCooperativeVectorMatrixNV vkCmdConvertCooperativeVectorMatrixNV;
extern PFN_vkConvertCooperativeVectorMatrixNV vkConvertCooperativeVectorMatrixNV;
extern PFN_vkGetPhysicalDeviceCooperativeVectorPropertiesNV vkGetPhysicalDeviceCooperativeVectorPropertiesNV;
#endif /* defined(VK_NV_cooperative_vector) */
#if defined(VK_NV_copy_memory_indirect)
extern PFN_vkCmdCopyMemoryIndirectNV vkCmdCopyMemoryIndirectNV;
extern PFN_vkCmdCopyMemoryToImageIndirectNV vkCmdCopyMemoryToImageIndirectNV;
#endif /* defined(VK_NV_copy_memory_indirect) */
#if defined(VK_NV_coverage_reduction_mode)
extern PFN_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV;
#endif /* defined(VK_NV_coverage_reduction_mode) */
#if defined(VK_NV_cuda_kernel_launch)
extern PFN_vkCmdCudaLaunchKernelNV vkCmdCudaLaunchKernelNV;
extern PFN_vkCreateCudaFunctionNV vkCreateCudaFunctionNV;
extern PFN_vkCreateCudaModuleNV vkCreateCudaModuleNV;
extern PFN_vkDestroyCudaFunctionNV vkDestroyCudaFunctionNV;
extern PFN_vkDestroyCudaModuleNV vkDestroyCudaModuleNV;
extern PFN_vkGetCudaModuleCacheNV vkGetCudaModuleCacheNV;
#endif /* defined(VK_NV_cuda_kernel_launch) */
#if defined(VK_NV_device_diagnostic_checkpoints)
extern PFN_vkCmdSetCheckpointNV vkCmdSetCheckpointNV;
extern PFN_vkGetQueueCheckpointDataNV vkGetQueueCheckpointDataNV;
#endif /* defined(VK_NV_device_diagnostic_checkpoints) */
#if defined(VK_NV_device_diagnostic_checkpoints) && (defined(VK_VERSION_1_3) || defined(VK_KHR_synchronization2))
extern PFN_vkGetQueueCheckpointData2NV vkGetQueueCheckpointData2NV;
#endif /* defined(VK_NV_device_diagnostic_checkpoints) && (defined(VK_VERSION_1_3) || defined(VK_KHR_synchronization2)) */
#if defined(VK_NV_device_generated_commands)
extern PFN_vkCmdBindPipelineShaderGroupNV vkCmdBindPipelineShaderGroupNV;
extern PFN_vkCmdExecuteGeneratedCommandsNV vkCmdExecuteGeneratedCommandsNV;
extern PFN_vkCmdPreprocessGeneratedCommandsNV vkCmdPreprocessGeneratedCommandsNV;
extern PFN_vkCreateIndirectCommandsLayoutNV vkCreateIndirectCommandsLayoutNV;
extern PFN_vkDestroyIndirectCommandsLayoutNV vkDestroyIndirectCommandsLayoutNV;
extern PFN_vkGetGeneratedCommandsMemoryRequirementsNV vkGetGeneratedCommandsMemoryRequirementsNV;
#endif /* defined(VK_NV_device_generated_commands) */
#if defined(VK_NV_device_generated_commands_compute)
extern PFN_vkCmdUpdatePipelineIndirectBufferNV vkCmdUpdatePipelineIndirectBufferNV;
extern PFN_vkGetPipelineIndirectDeviceAddressNV vkGetPipelineIndirectDeviceAddressNV;
extern PFN_vkGetPipelineIndirectMemoryRequirementsNV vkGetPipelineIndirectMemoryRequirementsNV;
#endif /* defined(VK_NV_device_generated_commands_compute) */
#if defined(VK_NV_external_compute_queue)
extern PFN_vkCreateExternalComputeQueueNV vkCreateExternalComputeQueueNV;
extern PFN_vkDestroyExternalComputeQueueNV vkDestroyExternalComputeQueueNV;
extern PFN_vkGetExternalComputeQueueDataNV vkGetExternalComputeQueueDataNV;
#endif /* defined(VK_NV_external_compute_queue) */
#if defined(VK_NV_external_memory_capabilities)
extern PFN_vkGetPhysicalDeviceExternalImageFormatPropertiesNV vkGetPhysicalDeviceExternalImageFormatPropertiesNV;
#endif /* defined(VK_NV_external_memory_capabilities) */
#if defined(VK_NV_external_memory_rdma)
extern PFN_vkGetMemoryRemoteAddressNV vkGetMemoryRemoteAddressNV;
#endif /* defined(VK_NV_external_memory_rdma) */
#if defined(VK_NV_external_memory_win32)
extern PFN_vkGetMemoryWin32HandleNV vkGetMemoryWin32HandleNV;
#endif /* defined(VK_NV_external_memory_win32) */
#if defined(VK_NV_fragment_shading_rate_enums)
extern PFN_vkCmdSetFragmentShadingRateEnumNV vkCmdSetFragmentShadingRateEnumNV;
#endif /* defined(VK_NV_fragment_shading_rate_enums) */
#if defined(VK_NV_low_latency2)
extern PFN_vkGetLatencyTimingsNV vkGetLatencyTimingsNV;
extern PFN_vkLatencySleepNV vkLatencySleepNV;
extern PFN_vkQueueNotifyOutOfBandNV vkQueueNotifyOutOfBandNV;
extern PFN_vkSetLatencyMarkerNV vkSetLatencyMarkerNV;
extern PFN_vkSetLatencySleepModeNV vkSetLatencySleepModeNV;
#endif /* defined(VK_NV_low_latency2) */
#if defined(VK_NV_memory_decompression)
extern PFN_vkCmdDecompressMemoryIndirectCountNV vkCmdDecompressMemoryIndirectCountNV;
extern PFN_vkCmdDecompressMemoryNV vkCmdDecompressMemoryNV;
#endif /* defined(VK_NV_memory_decompression) */
#if defined(VK_NV_mesh_shader)
extern PFN_vkCmdDrawMeshTasksIndirectNV vkCmdDrawMeshTasksIndirectNV;
extern PFN_vkCmdDrawMeshTasksNV vkCmdDrawMeshTasksNV;
#endif /* defined(VK_NV_mesh_shader) */
#if defined(VK_NV_mesh_shader) && (defined(VK_KHR_draw_indirect_count) || defined(VK_VERSION_1_2))
extern PFN_vkCmdDrawMeshTasksIndirectCountNV vkCmdDrawMeshTasksIndirectCountNV;
#endif /* defined(VK_NV_mesh_shader) && (defined(VK_KHR_draw_indirect_count) || defined(VK_VERSION_1_2)) */
#if defined(VK_NV_optical_flow)
extern PFN_vkBindOpticalFlowSessionImageNV vkBindOpticalFlowSessionImageNV;
extern PFN_vkCmdOpticalFlowExecuteNV vkCmdOpticalFlowExecuteNV;
extern PFN_vkCreateOpticalFlowSessionNV vkCreateOpticalFlowSessionNV;
extern PFN_vkDestroyOpticalFlowSessionNV vkDestroyOpticalFlowSessionNV;
extern PFN_vkGetPhysicalDeviceOpticalFlowImageFormatsNV vkGetPhysicalDeviceOpticalFlowImageFormatsNV;
#endif /* defined(VK_NV_optical_flow) */
#if defined(VK_NV_partitioned_acceleration_structure)
extern PFN_vkCmdBuildPartitionedAccelerationStructuresNV vkCmdBuildPartitionedAccelerationStructuresNV;
extern PFN_vkGetPartitionedAccelerationStructuresBuildSizesNV vkGetPartitionedAccelerationStructuresBuildSizesNV;
#endif /* defined(VK_NV_partitioned_acceleration_structure) */
#if defined(VK_NV_ray_tracing)
extern PFN_vkBindAccelerationStructureMemoryNV vkBindAccelerationStructureMemoryNV;
extern PFN_vkCmdBuildAccelerationStructureNV vkCmdBuildAccelerationStructureNV;
extern PFN_vkCmdCopyAccelerationStructureNV vkCmdCopyAccelerationStructureNV;
extern PFN_vkCmdTraceRaysNV vkCmdTraceRaysNV;
extern PFN_vkCmdWriteAccelerationStructuresPropertiesNV vkCmdWriteAccelerationStructuresPropertiesNV;
extern PFN_vkCompileDeferredNV vkCompileDeferredNV;
extern PFN_vkCreateAccelerationStructureNV vkCreateAccelerationStructureNV;
extern PFN_vkCreateRayTracingPipelinesNV vkCreateRayTracingPipelinesNV;
extern PFN_vkDestroyAccelerationStructureNV vkDestroyAccelerationStructureNV;
extern PFN_vkGetAccelerationStructureHandleNV vkGetAccelerationStructureHandleNV;
extern PFN_vkGetAccelerationStructureMemoryRequirementsNV vkGetAccelerationStructureMemoryRequirementsNV;
extern PFN_vkGetRayTracingShaderGroupHandlesNV vkGetRayTracingShaderGroupHandlesNV;
#endif /* defined(VK_NV_ray_tracing) */
#if defined(VK_NV_scissor_exclusive) && VK_NV_SCISSOR_EXCLUSIVE_SPEC_VERSION >= 2
extern PFN_vkCmdSetExclusiveScissorEnableNV vkCmdSetExclusiveScissorEnableNV;
#endif /* defined(VK_NV_scissor_exclusive) && VK_NV_SCISSOR_EXCLUSIVE_SPEC_VERSION >= 2 */
#if defined(VK_NV_scissor_exclusive)
extern PFN_vkCmdSetExclusiveScissorNV vkCmdSetExclusiveScissorNV;
#endif /* defined(VK_NV_scissor_exclusive) */
#if defined(VK_NV_shading_rate_image)
extern PFN_vkCmdBindShadingRateImageNV vkCmdBindShadingRateImageNV;
extern PFN_vkCmdSetCoarseSampleOrderNV vkCmdSetCoarseSampleOrderNV;
extern PFN_vkCmdSetViewportShadingRatePaletteNV vkCmdSetViewportShadingRatePaletteNV;
#endif /* defined(VK_NV_shading_rate_image) */
#if defined(VK_OHOS_surface)
extern PFN_vkCreateSurfaceOHOS vkCreateSurfaceOHOS;
#endif /* defined(VK_OHOS_surface) */
#if defined(VK_QCOM_tile_memory_heap)
extern PFN_vkCmdBindTileMemoryQCOM vkCmdBindTileMemoryQCOM;
#endif /* defined(VK_QCOM_tile_memory_heap) */
#if defined(VK_QCOM_tile_properties)
extern PFN_vkGetDynamicRenderingTilePropertiesQCOM vkGetDynamicRenderingTilePropertiesQCOM;
extern PFN_vkGetFramebufferTilePropertiesQCOM vkGetFramebufferTilePropertiesQCOM;
#endif /* defined(VK_QCOM_tile_properties) */
#if defined(VK_QCOM_tile_shading)
extern PFN_vkCmdBeginPerTileExecutionQCOM vkCmdBeginPerTileExecutionQCOM;
extern PFN_vkCmdDispatchTileQCOM vkCmdDispatchTileQCOM;
extern PFN_vkCmdEndPerTileExecutionQCOM vkCmdEndPerTileExecutionQCOM;
#endif /* defined(VK_QCOM_tile_shading) */
#if defined(VK_QNX_external_memory_screen_buffer)
extern PFN_vkGetScreenBufferPropertiesQNX vkGetScreenBufferPropertiesQNX;
#endif /* defined(VK_QNX_external_memory_screen_buffer) */
#if defined(VK_QNX_screen_surface)
extern PFN_vkCreateScreenSurfaceQNX vkCreateScreenSurfaceQNX;
extern PFN_vkGetPhysicalDeviceScreenPresentationSupportQNX vkGetPhysicalDeviceScreenPresentationSupportQNX;
#endif /* defined(VK_QNX_screen_surface) */
#if defined(VK_VALVE_descriptor_set_host_mapping)
extern PFN_vkGetDescriptorSetHostMappingVALVE vkGetDescriptorSetHostMappingVALVE;
extern PFN_vkGetDescriptorSetLayoutHostMappingInfoVALVE vkGetDescriptorSetLayoutHostMappingInfoVALVE;
#endif /* defined(VK_VALVE_descriptor_set_host_mapping) */
#if (defined(VK_EXT_depth_clamp_control)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_depth_clamp_control))
extern PFN_vkCmdSetDepthClampRangeEXT vkCmdSetDepthClampRangeEXT;
#endif /* (defined(VK_EXT_depth_clamp_control)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_depth_clamp_control)) */
#if (defined(VK_EXT_extended_dynamic_state)) || (defined(VK_EXT_shader_object))
extern PFN_vkCmdBindVertexBuffers2EXT vkCmdBindVertexBuffers2EXT;
extern PFN_vkCmdSetCullModeEXT vkCmdSetCullModeEXT;
extern PFN_vkCmdSetDepthBoundsTestEnableEXT vkCmdSetDepthBoundsTestEnableEXT;
extern PFN_vkCmdSetDepthCompareOpEXT vkCmdSetDepthCompareOpEXT;
extern PFN_vkCmdSetDepthTestEnableEXT vkCmdSetDepthTestEnableEXT;
extern PFN_vkCmdSetDepthWriteEnableEXT vkCmdSetDepthWriteEnableEXT;
extern PFN_vkCmdSetFrontFaceEXT vkCmdSetFrontFaceEXT;
extern PFN_vkCmdSetPrimitiveTopologyEXT vkCmdSetPrimitiveTopologyEXT;
extern PFN_vkCmdSetScissorWithCountEXT vkCmdSetScissorWithCountEXT;
extern PFN_vkCmdSetStencilOpEXT vkCmdSetStencilOpEXT;
extern PFN_vkCmdSetStencilTestEnableEXT vkCmdSetStencilTestEnableEXT;
extern PFN_vkCmdSetViewportWithCountEXT vkCmdSetViewportWithCountEXT;
#endif /* (defined(VK_EXT_extended_dynamic_state)) || (defined(VK_EXT_shader_object)) */
#if (defined(VK_EXT_extended_dynamic_state2)) || (defined(VK_EXT_shader_object))
extern PFN_vkCmdSetDepthBiasEnableEXT vkCmdSetDepthBiasEnableEXT;
extern PFN_vkCmdSetLogicOpEXT vkCmdSetLogicOpEXT;
extern PFN_vkCmdSetPatchControlPointsEXT vkCmdSetPatchControlPointsEXT;
extern PFN_vkCmdSetPrimitiveRestartEnableEXT vkCmdSetPrimitiveRestartEnableEXT;
extern PFN_vkCmdSetRasterizerDiscardEnableEXT vkCmdSetRasterizerDiscardEnableEXT;
#endif /* (defined(VK_EXT_extended_dynamic_state2)) || (defined(VK_EXT_shader_object)) */
#if (defined(VK_EXT_extended_dynamic_state3)) || (defined(VK_EXT_shader_object))
extern PFN_vkCmdSetAlphaToCoverageEnableEXT vkCmdSetAlphaToCoverageEnableEXT;
extern PFN_vkCmdSetAlphaToOneEnableEXT vkCmdSetAlphaToOneEnableEXT;
extern PFN_vkCmdSetColorBlendEnableEXT vkCmdSetColorBlendEnableEXT;
extern PFN_vkCmdSetColorBlendEquationEXT vkCmdSetColorBlendEquationEXT;
extern PFN_vkCmdSetColorWriteMaskEXT vkCmdSetColorWriteMaskEXT;
extern PFN_vkCmdSetDepthClampEnableEXT vkCmdSetDepthClampEnableEXT;
extern PFN_vkCmdSetLogicOpEnableEXT vkCmdSetLogicOpEnableEXT;
extern PFN_vkCmdSetPolygonModeEXT vkCmdSetPolygonModeEXT;
extern PFN_vkCmdSetRasterizationSamplesEXT vkCmdSetRasterizationSamplesEXT;
extern PFN_vkCmdSetSampleMaskEXT vkCmdSetSampleMaskEXT;
#endif /* (defined(VK_EXT_extended_dynamic_state3)) || (defined(VK_EXT_shader_object)) */
#if (defined(VK_EXT_extended_dynamic_state3) && (defined(VK_KHR_maintenance2) || defined(VK_VERSION_1_1))) || (defined(VK_EXT_shader_object))
extern PFN_vkCmdSetTessellationDomainOriginEXT vkCmdSetTessellationDomainOriginEXT;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && (defined(VK_KHR_maintenance2) || defined(VK_VERSION_1_1))) || (defined(VK_EXT_shader_object)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_transform_feedback)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_transform_feedback))
extern PFN_vkCmdSetRasterizationStreamEXT vkCmdSetRasterizationStreamEXT;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_transform_feedback)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_transform_feedback)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_conservative_rasterization)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_conservative_rasterization))
extern PFN_vkCmdSetConservativeRasterizationModeEXT vkCmdSetConservativeRasterizationModeEXT;
extern PFN_vkCmdSetExtraPrimitiveOverestimationSizeEXT vkCmdSetExtraPrimitiveOverestimationSizeEXT;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_conservative_rasterization)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_conservative_rasterization)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_depth_clip_enable)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_depth_clip_enable))
extern PFN_vkCmdSetDepthClipEnableEXT vkCmdSetDepthClipEnableEXT;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_depth_clip_enable)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_depth_clip_enable)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_sample_locations)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_sample_locations))
extern PFN_vkCmdSetSampleLocationsEnableEXT vkCmdSetSampleLocationsEnableEXT;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_sample_locations)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_sample_locations)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_blend_operation_advanced)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_blend_operation_advanced))
extern PFN_vkCmdSetColorBlendAdvancedEXT vkCmdSetColorBlendAdvancedEXT;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_blend_operation_advanced)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_blend_operation_advanced)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_provoking_vertex)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_provoking_vertex))
extern PFN_vkCmdSetProvokingVertexModeEXT vkCmdSetProvokingVertexModeEXT;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_provoking_vertex)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_provoking_vertex)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_line_rasterization)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_line_rasterization))
extern PFN_vkCmdSetLineRasterizationModeEXT vkCmdSetLineRasterizationModeEXT;
extern PFN_vkCmdSetLineStippleEnableEXT vkCmdSetLineStippleEnableEXT;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_line_rasterization)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_line_rasterization)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_depth_clip_control)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_depth_clip_control))
extern PFN_vkCmdSetDepthClipNegativeOneToOneEXT vkCmdSetDepthClipNegativeOneToOneEXT;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_depth_clip_control)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_depth_clip_control)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_clip_space_w_scaling)) || (defined(VK_EXT_shader_object) && defined(VK_NV_clip_space_w_scaling))
extern PFN_vkCmdSetViewportWScalingEnableNV vkCmdSetViewportWScalingEnableNV;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_clip_space_w_scaling)) || (defined(VK_EXT_shader_object) && defined(VK_NV_clip_space_w_scaling)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_viewport_swizzle)) || (defined(VK_EXT_shader_object) && defined(VK_NV_viewport_swizzle))
extern PFN_vkCmdSetViewportSwizzleNV vkCmdSetViewportSwizzleNV;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_viewport_swizzle)) || (defined(VK_EXT_shader_object) && defined(VK_NV_viewport_swizzle)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_fragment_coverage_to_color)) || (defined(VK_EXT_shader_object) && defined(VK_NV_fragment_coverage_to_color))
extern PFN_vkCmdSetCoverageToColorEnableNV vkCmdSetCoverageToColorEnableNV;
extern PFN_vkCmdSetCoverageToColorLocationNV vkCmdSetCoverageToColorLocationNV;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_fragment_coverage_to_color)) || (defined(VK_EXT_shader_object) && defined(VK_NV_fragment_coverage_to_color)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_framebuffer_mixed_samples)) || (defined(VK_EXT_shader_object) && defined(VK_NV_framebuffer_mixed_samples))
extern PFN_vkCmdSetCoverageModulationModeNV vkCmdSetCoverageModulationModeNV;
extern PFN_vkCmdSetCoverageModulationTableEnableNV vkCmdSetCoverageModulationTableEnableNV;
extern PFN_vkCmdSetCoverageModulationTableNV vkCmdSetCoverageModulationTableNV;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_framebuffer_mixed_samples)) || (defined(VK_EXT_shader_object) && defined(VK_NV_framebuffer_mixed_samples)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_shading_rate_image)) || (defined(VK_EXT_shader_object) && defined(VK_NV_shading_rate_image))
extern PFN_vkCmdSetShadingRateImageEnableNV vkCmdSetShadingRateImageEnableNV;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_shading_rate_image)) || (defined(VK_EXT_shader_object) && defined(VK_NV_shading_rate_image)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_representative_fragment_test)) || (defined(VK_EXT_shader_object) && defined(VK_NV_representative_fragment_test))
extern PFN_vkCmdSetRepresentativeFragmentTestEnableNV vkCmdSetRepresentativeFragmentTestEnableNV;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_representative_fragment_test)) || (defined(VK_EXT_shader_object) && defined(VK_NV_representative_fragment_test)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_coverage_reduction_mode)) || (defined(VK_EXT_shader_object) && defined(VK_NV_coverage_reduction_mode))
extern PFN_vkCmdSetCoverageReductionModeNV vkCmdSetCoverageReductionModeNV;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_coverage_reduction_mode)) || (defined(VK_EXT_shader_object) && defined(VK_NV_coverage_reduction_mode)) */
#if (defined(VK_EXT_host_image_copy)) || (defined(VK_EXT_image_compression_control))
extern PFN_vkGetImageSubresourceLayout2EXT vkGetImageSubresourceLayout2EXT;
#endif /* (defined(VK_EXT_host_image_copy)) || (defined(VK_EXT_image_compression_control)) */
#if (defined(VK_EXT_shader_object)) || (defined(VK_EXT_vertex_input_dynamic_state))
extern PFN_vkCmdSetVertexInputEXT vkCmdSetVertexInputEXT;
#endif /* (defined(VK_EXT_shader_object)) || (defined(VK_EXT_vertex_input_dynamic_state)) */
#if (defined(VK_KHR_descriptor_update_template) && defined(VK_KHR_push_descriptor)) || (defined(VK_KHR_push_descriptor) && (defined(VK_VERSION_1_1) || defined(VK_KHR_descriptor_update_template)))
extern PFN_vkCmdPushDescriptorSetWithTemplateKHR vkCmdPushDescriptorSetWithTemplateKHR;
#endif /* (defined(VK_KHR_descriptor_update_template) && defined(VK_KHR_push_descriptor)) || (defined(VK_KHR_push_descriptor) && (defined(VK_VERSION_1_1) || defined(VK_KHR_descriptor_update_template))) */
#if (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1))
extern PFN_vkGetDeviceGroupPresentCapabilitiesKHR vkGetDeviceGroupPresentCapabilitiesKHR;
extern PFN_vkGetDeviceGroupSurfacePresentModesKHR vkGetDeviceGroupSurfacePresentModesKHR;
extern PFN_vkGetPhysicalDevicePresentRectanglesKHR vkGetPhysicalDevicePresentRectanglesKHR;
#endif /* (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1)) */
#if (defined(VK_KHR_device_group) && defined(VK_KHR_swapchain)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1))
extern PFN_vkAcquireNextImage2KHR vkAcquireNextImage2KHR;
#endif /* (defined(VK_KHR_device_group) && defined(VK_KHR_swapchain)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1)) */
/* LAHAR_VK_PROTOTYPES_H */










































#endif //LAHAR_H

#if defined(LAHAR_IMPLEMENTATION) && !defined(LAHAR_IMPLEMENTATION_INCLUDED)
#define LAHAR_IMPLEMENTATION_INCLUDED


// ============================================================================
// Region: Main Implementation
// ============================================================================


#ifndef lahar_malloc
    #define lahar_malloc(size) malloc(size)
#endif

#ifndef lahar_realloc
    #define lahar_realloc(ptr, size) realloc(ptr, size)
#endif

#ifndef lahar_free
    #define lahar_free(ptr) free(ptr)
#endif

#ifndef LAHAR_M_ARENA_SIZE
    #define LAHAR_M_ARENA_SIZE 65536
#endif

#ifndef LAHAR_M_CHECK_CT
    #define LAHAR_M_CHECK_CT 16
#endif

#ifndef LAHAR_DEFAULT_VK_VERSION
    #define LAHAR_DEFAULT_VK_VERSION VK_API_VERSION_1_3
#endif

#ifndef LAHAR_DEFAULT_ALIGNMENT
    #ifdef __cplusplus
        #define LAHAR_DEFAULT_ALIGNMENT LAHAR_ALIGNOF(max_align_t)
    #else
        #define LAHAR_DEFAULT_ALIGNMENT LAHAR_ALIGNOF(max_align_t)
    #endif
#endif

/* layer 1: emission */
#if defined(_MSC_VER) && !defined(__clang__)
  #define LAHAR_PRAGMA(x)  __pragma(x)
#else
  #define LAHAR_PRAGMA_(x) _Pragma(#x)
  #define LAHAR_PRAGMA(x)  LAHAR_PRAGMA_(x)
#endif

#define LAHAR_STR_(x) #x
#define LAHAR_STR(x)  LAHAR_STR_(x)

/* layer 2: diagnostic spelling */
#if defined(__GNUC__) || defined(__clang__)
  #define LAHAR_MESSAGE(msg) LAHAR_PRAGMA(message(msg))
  #define LAHAR_WARNING(msg) LAHAR_PRAGMA(GCC warning msg)
#elif defined(_MSC_VER)
  #define LAHAR_MESSAGE(msg) \
    LAHAR_PRAGMA(message(__FILE__ "(" LAHAR_STR(__LINE__) "): note: " msg))
  #define LAHAR_WARNING(msg) \
    LAHAR_PRAGMA(message(__FILE__ "(" LAHAR_STR(__LINE__) "): warning: " msg))
#else
  #define LAHAR_MESSAGE(msg)
  #define LAHAR_WARNING(msg)
#endif

#if defined(LAHAR_WINDOW_WARN) && !defined(LAHAR_WINDOW_WARN_OK)
    #pragma message("Lahar is not using a windowing interface, no windows will be available. Define LAHAR_WINDOW_WARN_OK to silence this message.")
#endif

#if defined(LAHAR_GLFW_WARN) && !defined(LAHAR_WINDOW_GLFW_WARN_OK)
    #pragma message("With GLFW and NO_AUTO_INCLUDE, ensure you include vulkan with VK_NO_PROTOTYPES before both lahar and GLFW. Define LAHAR_WINDOW_GLFW_WARN_OK to silence this message.")
#endif

#include <stdarg.h>
#include <inttypes.h>

#ifdef __linux__
    #include <signal.h>

    #ifndef LAHAR_DEBUG_BREAK
        #define LAHAR_DEBUG_BREAK raise(SIGTRAP)
    #endif

#else

    #ifndef LAHAR_DEBUG_BREAK
        #define LAHAR_DEBUG_BREAK
    #endif
#endif


/* Hard runtime guards. Always on, even in release builds: these protect
 * against conditions that depend on runtime data (e.g. arena exhaustion)
 * where proceeding means memory corruption. Define LAHAR_FATAL_ASSERT before
 * including to supply your own handler; it must not return on failure. */
#ifndef LAHAR_FATAL_ASSERT
    #define LAHAR_FATAL_ASSERT(cond) do { \
        if (!(cond)) { \
            fprintf(stderr, "LAHAR_FATAL_ASSERT failed: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
            LAHAR_DEBUG_BREAK; \
            exit(1); \
        } \
    } while(0)
#endif

/* Internal invariant checks. Enabled by LAHAR_DEBUG; compiled out otherwise.
 * Assertion expressions must be side-effect free. Define LAHAR_ASSERT before
 * including to supply your own handler. */
#ifndef LAHAR_ASSERT
    #ifdef LAHAR_DEBUG
        #define LAHAR_ASSERT(cond) LAHAR_FATAL_ASSERT(cond)
    #else
        #define LAHAR_ASSERT(cond)
    #endif
#endif

#ifdef __cplusplus
#define ZINIT {}
#else
#define ZINIT {0}
#endif

Lahar __lahar_instance = ZINIT;
Lahar* lahar = &__lahar_instance;

static char* lahar_strdup(const char* str) {
    size_t len = strlen(str);
    char* cpy = (char*) lahar_malloc(len + 1);

    if (!cpy) { return NULL; }

    memcpy(cpy, str, len + 1);

    return cpy;
}

static void* lahar_alloc_or_resize(void* existing, size_t targetsize) {
    if (!existing) {
        return lahar_malloc(targetsize);
    }
    else {
        return lahar_realloc(existing, targetsize);
    }
}

#ifdef __cplusplus
#define lahar_vec_expand(vec, cap) \
    size_t tgt = (cap) == 0 ? 10 : (cap) * 2; \
    void* newm = lahar_alloc_or_resize(vec, tgt * sizeof(*(vec))); \
    if (newm) { \
        (vec) = static_cast<decltype(vec)>(newm); \
        (cap) = tgt; \
    }
#else
#define lahar_vec_expand(vec, cap) \
    size_t tgt = (cap) == 0 ? 10 : (cap) * 2; \
    void* newm = lahar_alloc_or_resize(vec, tgt * sizeof(*(vec))); \
    if (newm) { \
        (vec) = newm; \
        (cap) = tgt; \
    }
#endif


static void __lahar_trace(const char* msg, ...) {
    if (lahar->debug_level <= LAHAR_DEBUG_TRACE) {
        va_list ap;
        va_start(ap, msg);

        char buffer[4096];
        vsnprintf(buffer, sizeof(buffer) - 1, msg, ap);

        printf("[LHRTRACE] %s\n", buffer);

        va_end(ap);
    }
}

static void __lahar_info(const char* msg, ...) {
    if (lahar->debug_level <= LAHAR_DEBUG_INFO) {
        va_list ap;
        va_start(ap, msg);

        char buffer[4096];
        vsnprintf(buffer, sizeof(buffer) - 1, msg, ap);

        printf("[LHRINFO] %s\n", buffer);

        va_end(ap);
    }
}

static void __lahar_warn(const char* msg, ...) {
    if (lahar->debug_level <= LAHAR_DEBUG_WARNING) {
        va_list ap;
        va_start(ap, msg);

        char buffer[4096];
        vsnprintf(buffer, sizeof(buffer) - 1, msg, ap);

        printf("[LHRWARN] %s\n", buffer);

        va_end(ap);
    }
}

static void __lahar_error(const char* msg, ...) {
    if (lahar->debug_level <= LAHAR_DEBUG_ERROR) {
        va_list ap;
        va_start(ap, msg);

        char buffer[4096];
        vsnprintf(buffer, sizeof(buffer) - 1, msg, ap);

        printf("[LHRERROR] %s\n", buffer);

        va_end(ap);
    }
}

#ifndef LAHAR_NO_LOGGING
    #define lahar_trace(...) __lahar_trace(__VA_ARGS__)
    #define lahar_info(...) __lahar_info(__VA_ARGS__)
    #define lahar_warn(...) __lahar_warn(__VA_ARGS__)
    #define lahar_error(...) __lahar_error(__VA_ARGS__)
#else
    #define lahar_trace(...)
    #define lahar_info(...)
    #define lahar_warn(...)
    #define lahar_error(...)
#endif

#if defined(_WIN32)
    /** Open the handle to the vulkan lib */
    static uint32_t __lahar_open_libvk() {
        HMODULE module = LoadLibraryA("vulkan-1.dll");

        lahar->libvulkan = module;
        return module ? LAHAR_ERR_SUCCESS : LAHAR_ERR_LOAD_FAILURE;
    }

    /** Close the handle to the vulkan lib */
    static void __lahar_close_libvk() {
        if (lahar->libvulkan) {
            FreeLibrary(lahar->libvulkan);
            lahar->libvulkan = NULL;
        }
    }

    /** Loader callback for loading a function from the vulkan lib using native loading mechanisms */
    static PFN_vkVoidFunction lahar_loader_sym(const char* name) {
        return GetProcAddress(lahar->libvulkan, name);
    }
#else
    #include <dlfcn.h>

    /** Open the handle to the vulkan lib */
    static uint32_t __lahar_open_libvk() {
        void* module = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);

        if (!module) {
            module = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
        }

        lahar->libvulkan = module;
        return module ? LAHAR_ERR_SUCCESS : LAHAR_ERR_LOAD_FAILURE;
    }

    /** Close the handle to the vulkan lib */
    static void __lahar_close_libvk() {
        if (lahar->libvulkan) {
            dlclose(lahar->libvulkan);
            lahar->libvulkan = NULL;
        }
    }

    /** Loader callback for loading a function from the vulkan lib using native loading mechanisms */
    static PFN_vkVoidFunction lahar_loader_sym(const char* name) {
        return (PFN_vkVoidFunction)dlsym(lahar->libvulkan, name);
    }

#endif

static uint32_t lahar_load_loader(LaharLoaderFunc loadfn);
static uint32_t lahar_load_instance(LaharLoaderFunc loadfn);
static uint32_t lahar_load_device(LaharLoaderFunc loadfn);

static int64_t __lahar_default_scorer(const LaharDeviceInfo* devinfo);
static uint32_t __lahar_default_surface_format_chooser(LaharWindowState* window_state, LaharDeviceInfo* physdev_info, VkSurfaceFormatKHR* surface_fmt_out);
static uint32_t __lahar_default_surface_present_mode_chooser(LaharWindowState* window_state, LaharDeviceInfo* physdev_info, VkPresentModeKHR* present_mode_out);
static uint32_t __lahar_default_resizer(LaharWindow* window);
static VKAPI_ATTR VkBool32 VKAPI_CALL __lahar_default_dbgcallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* pcallbackdata,
    void* puserdata
);

static uint8_t __marena[LAHAR_M_ARENA_SIZE];
static size_t __mpos = 0;
static size_t __mcheck[LAHAR_M_CHECK_CT];
static size_t __mchkct = 0;

static void lahar_temp_mcheck() {
    LAHAR_FATAL_ASSERT(__mchkct < sizeof(__mcheck) / sizeof(__mcheck[0])); // if this trips, Lahar is broken
    __mcheck[__mchkct++] = __mpos;
}

static void lahar_temp_mpop() {
    if (__mchkct > 0) {
        __mchkct--;
        __mpos = __mcheck[__mchkct];
    }
}


static void* lahar_temp_alloc_aligned(size_t bytes, size_t alignment) {
    uintptr_t current = (uintptr_t)&__marena[__mpos];
    size_t padding = (-(size_t)current) & (alignment - 1);

    size_t real_amt = padding + bytes;

    // Overflow-safe form of __mpos + real_amt <= size; proceeding past this
    // would hand out memory beyond the arena and corrupt whatever follows it.
    LAHAR_FATAL_ASSERT(sizeof(__marena) - __mpos >= real_amt);

    void* ret = &__marena[__mpos + padding];
    __mpos += real_amt;
    memset(ret, 0, bytes);
    return ret;
}

static void* lahar_temp_alloc(size_t bytes) {
    return lahar_temp_alloc_aligned(bytes, LAHAR_DEFAULT_ALIGNMENT);
}

static char* lahar_temp_strdup(const char* str) {
    size_t len = strlen(str);
    char* buf = (char*)lahar_temp_alloc(len + 1);
    memcpy(buf, str, len + 1);
    return buf;
}




/** Loader callback for loading instance level vulkan functions */
static PFN_vkVoidFunction __lahar_loader_inst(const char* name) {
    return vkGetInstanceProcAddr(lahar->instance, name);
}

/** Loader callback for loading device level vulkan functions */
static PFN_vkVoidFunction __lahar_loader_dev(const char* name) {
    return vkGetDeviceProcAddr(lahar->device, name);
}

static VkBaseInStructure* __lahar_pnext_fetch(void* chain, VkStructureType type) {
    VkBaseInStructure* base = (VkBaseInStructure*)chain;

    while (base) {
        if (base->sType == type) { return base; }
        base = (VkBaseInStructure*)base->pNext;
    }

    return NULL;
}

/** This builds a complete list of the instance level extensions required. It
 * writes the strings into temp memory, and the return array in temp memory.
 */
static uint32_t __lahar_temp_extensions(LaharWindow* window, uint32_t* count, char*** ext_out) {
    uint32_t err = LAHAR_ERR_SUCCESS;

    uint32_t ext_count = (uint32_t)lahar->extensions.rie_count;
    char** win_exts = NULL;
    char** extensions = NULL;
    size_t i = 0;

    if (lahar->wantvalidation) {
        ext_count++;
    }

    uint32_t win_count = 0;
    if ((err = lahar_window_get_extensions(window, &win_count, NULL))) {
        goto end;
    }

    ext_count += win_count;

    win_exts = (char**)lahar_temp_alloc(sizeof(char*) * win_count);

    if ((err = lahar_window_get_extensions(window, &win_count, (const char**)win_exts))) {
        goto end;
    }

    extensions = (char**)lahar_temp_alloc(sizeof(char*) * ext_count);

    for (; i < lahar->extensions.rie_count; i++) {
        const char* current = lahar->extensions.req_inst_exts[i];
        extensions[i] = lahar_temp_strdup(current);
    }

    for (size_t j = 0; j < win_count; j++) {
        const char* current = win_exts[j];
        extensions[i++] = lahar_temp_strdup(current);
    }

    if (lahar->wantvalidation) {
        extensions[i++] = lahar_temp_strdup(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    *count = ext_count;
    *ext_out = extensions;

end:
    return err;
}

static uint32_t __lahar_init_window_swapchain(LaharWindowState* winstate, VkSwapchainKHR old_swap) {
    uint32_t err = LAHAR_ERR_SUCCESS;
    lahar_temp_mcheck();

    LaharSurfaceFormatChooseFunc choose_format = lahar->format_chooser ? lahar->format_chooser : __lahar_default_surface_format_chooser;
    LaharSurfacePresentModeChooseFunc choose_mode = lahar->present_chooser ? lahar->present_chooser : __lahar_default_surface_present_mode_chooser;
    LaharAttachmentConfig* color_conf = NULL;
    uint32_t queue_indices[2] = ZINIT;
    uint32_t queue_index_count = 0;

    VkSurfaceCapabilitiesKHR surface_caps = ZINIT;
    VkSwapchainCreateInfoKHR create_info = ZINIT;
    uint32_t image_count = 0;

    VkImage* swap_imgs = NULL;
    VkImageView* swap_views = NULL;

    VkSemaphoreCreateInfo sem_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    uint32_t avail_ct = 0;
    uint32_t fin_ct = 0;
    uint32_t fence_ct = 0;

    if (winstate->attachment_count > 1 && !lahar->gpu_allocator) {
        err = LAHAR_ERR_INVALID_CONFIGURATION;
        goto end;
    }

    color_conf = lahar_window_attachment_config_color(winstate->window);

    LAHAR_ASSERT(color_conf);

    // Step 1: setup the swapchain
    queue_indices[0] = lahar->physdev_info.graphics_queue_index;
    queue_indices[1] = lahar->physdev_info.present_queue_index;
    queue_index_count = queue_indices[0] == queue_indices[1] ? 0 : 2;

    if ((lahar->vkresult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(lahar->physdev_info.physdev, winstate->surface, &surface_caps))) {
        err = LAHAR_ERR_VK_ERR;
        goto end;
    }

    if (winstate->desired_img_count == 0) {
        winstate->desired_img_count = winstate->max_in_flight;
    }

    if ((err = choose_format(winstate, &lahar->physdev_info, &winstate->surface_format))) {
        goto end;
    }

    if ((err = lahar_window_get_size(winstate->window, &winstate->width, &winstate->height))) {
        goto end;
    }

    // Store the reported framebuffer size before clamping
    create_info.imageExtent.width = winstate->width;
    create_info.imageExtent.height = winstate->height;

    if (create_info.imageExtent.width > surface_caps.maxImageExtent.width) {
        create_info.imageExtent.width = surface_caps.maxImageExtent.width;
    }
    else if (create_info.imageExtent.width < surface_caps.minImageExtent.width) {
        create_info.imageExtent.width = surface_caps.minImageExtent.width;
    }

    if (create_info.imageExtent.height > surface_caps.maxImageExtent.height) {
        create_info.imageExtent.height = surface_caps.maxImageExtent.height;
    }
    else if (create_info.imageExtent.height < surface_caps.minImageExtent.height) {
        create_info.imageExtent.height = surface_caps.minImageExtent.height;
    }

    // Then store the actual swap extent
    winstate->extent = create_info.imageExtent;

    if ((err = choose_mode(winstate, &lahar->physdev_info, &create_info.presentMode))) {
        goto end;
    }

    image_count = winstate->desired_img_count;

    if (surface_caps.maxImageCount > 0 && image_count > surface_caps.maxImageCount) {
        image_count = surface_caps.maxImageCount;
    }
    else if (surface_caps.minImageCount > 0 && image_count < surface_caps.minImageCount) {
        image_count = surface_caps.minImageCount;
    }

    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = winstate->surface;
    create_info.imageFormat = winstate->surface_format.format;
    create_info.imageColorSpace = winstate->surface_format.colorSpace;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = color_conf->usage ? color_conf->usage : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.imageSharingMode = queue_indices[0] == queue_indices[1] ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT;
    create_info.queueFamilyIndexCount = queue_index_count;
    create_info.pQueueFamilyIndices = queue_indices;
    create_info.preTransform = surface_caps.currentTransform;
    create_info.compositeAlpha = winstate->alpha ? winstate->alpha : VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = old_swap;
    create_info.minImageCount = image_count;

    if ((lahar->vkresult = vkCreateSwapchainKHR(lahar->device, &create_info, lahar->vkalloc, &winstate->swapchain)) != VK_SUCCESS) {
        err = LAHAR_ERR_VK_ERR;
        goto end;
    }

    // Step 2: allocate the space we need for all the attachments
    vkGetSwapchainImagesKHR(lahar->device, winstate->swapchain, &winstate->swap_size, NULL);

    if (winstate->swap_size == 0) {
        // TODO: this can happen, indicates that we can't do any drawing right now, such as minimized
    }

    lahar_trace("Window %" PRIu64 " had a swapchain of size %lu created", winstate->index, winstate->swap_size);

    LAHAR_ASSERT(!winstate->attachments);

    winstate->attachments = (LaharAttachment**)lahar_malloc(sizeof(*winstate->attachments) * winstate->attachment_count);
    memset(winstate->attachments, 0, sizeof(*winstate->attachments) * winstate->attachment_count);

    for (size_t j = 0; j < winstate->attachment_count; j++) {
        size_t bytes = winstate->swap_size * sizeof(LaharAttachment);

        LAHAR_ASSERT(!winstate->attachments[j]);

        winstate->attachments[j] = (LaharAttachment*)lahar_malloc(bytes);
        memset(winstate->attachments[j], 0, bytes);
    }

    // Step 3: create the swapchain color views
    swap_imgs = (VkImage*)lahar_temp_alloc(winstate->swap_size * sizeof(VkImage));
    swap_views = (VkImageView*)lahar_temp_alloc(winstate->swap_size * sizeof(VkImageView));

    vkGetSwapchainImagesKHR(lahar->device, winstate->swapchain, &winstate->swap_size, swap_imgs);

    for (uint32_t j = 0; j < winstate->swap_size; j++) {
        VkImageViewCreateInfo view_create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swap_imgs[j],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format =  winstate->surface_format.format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        if ((lahar->vkresult = vkCreateImageView(lahar->device, &view_create_info, lahar->vkalloc, &swap_views[j])) != VK_SUCCESS) {
            err = LAHAR_ERR_VK_ERR;
            goto end;
        }

        LaharAttachment* color = lahar_window_attachment_color(winstate->window, j);
        color->image = swap_imgs[j];
        color->view = swap_views[j];
    }

    // Step 4: allocate and create non-color attachments
    for (size_t j = 1; j < winstate->attachment_count; j++) {
        LaharAttachment* attachment_list = winstate->attachments[j];
        LaharAttachmentConfig* attachment_config = &winstate->attachment_configs[j];

        if (attachment_config->img_info.sType != VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO) {
            attachment_config->img_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        }

        attachment_config->img_info.extent.width = winstate->width;
        attachment_config->img_info.extent.height = winstate->height;

        if (attachment_config->img_info.extent.depth == 0) {
            attachment_config->img_info.extent.depth = 1;
        }

        for (size_t k = 0; k < winstate->swap_size; k++) {
            LaharAttachment* attachment = &attachment_list[k];

            LaharAllocationCreateInfo alloc_create_info = {
                .usage = LAHAR_MU_DEVICE_ONLY,
            };

            switch (attachment_config->role) {
                case LAHAR_ATTROLE_COLOR:
                case LAHAR_ATTROLE_USER:
                    alloc_create_info.role = LAHAR_AR_COLOR_ATTACHMENT;
                    break;
                case LAHAR_ATTROLE_DEPTH:
                case LAHAR_ATTROLE_STENCIL:
                case LAHAR_ATTROLE_DEPTH_STENCIL:
                    alloc_create_info.role = LAHAR_AR_DEPTH_STENCIL_ATTACHMENT;
                    break;
                default:
                    break;
            }

            if ((err = lahar->gpu_allocator->alloc_image(
                lahar->gpu_allocator,
                &attachment_config->img_info,
                &alloc_create_info,
                &attachment->image,
                &attachment->img_allocation
            ))) {
                goto end;
            }

            attachment_config->view_info.image = attachment->image;

            if ((lahar->vkresult = vkCreateImageView(lahar->device, &attachment_config->view_info, lahar->vkalloc, &attachment->view))) {
                err = LAHAR_ERR_VK_ERR;
                goto end;
            }
        }
    }

    // Step 5: create command buffers (if requested)
    if (lahar->wantcommands) {
        LAHAR_ASSERT(winstate->commands == NULL);

        VkCommandPoolCreateInfo pool_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = lahar->physdev_info.graphics_queue_index
        };

        if ((lahar->vkresult = vkCreateCommandPool(lahar->device, &pool_info, lahar->vkalloc, &winstate->pool))) {
            err = LAHAR_ERR_VK_ERR;
            goto end;
        }

        winstate->commands = (VkCommandBuffer*)lahar_malloc(winstate->swap_size * sizeof(VkCommandBuffer));

        VkCommandBufferAllocateInfo buffer_alloc = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = winstate->pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = winstate->swap_size
        };

        if ((lahar->vkresult = vkAllocateCommandBuffers(lahar->device, &buffer_alloc, winstate->commands)) != VK_SUCCESS) {
            err = LAHAR_ERR_VK_ERR;
            goto end;
        }
    }

    // Step 6: create the sync primtives
    avail_ct = winstate->max_in_flight;
    fin_ct = winstate->swap_size;
    fence_ct = winstate->max_in_flight;

    LAHAR_ASSERT(winstate->image_available == NULL);
    LAHAR_ASSERT(winstate->render_finished == NULL);
    LAHAR_ASSERT(winstate->in_flight == NULL);
    LAHAR_ASSERT(winstate->present_fences == NULL);

    winstate->image_available = (VkSemaphore*)lahar_malloc(avail_ct * sizeof(VkSemaphore));
    winstate->render_finished = (VkSemaphore*)lahar_malloc(fin_ct * sizeof(VkSemaphore));
    winstate->in_flight = (VkFence*)lahar_malloc(fence_ct * sizeof(VkFence));
    winstate->present_fences = (VkFence*)lahar_malloc(fin_ct * sizeof(VkFence));

    winstate->image_available_size = avail_ct;
    winstate->render_finished_size = fin_ct;
    winstate->in_flight_size = fence_ct;
    winstate->present_fences_size = fin_ct;

    lahar_trace("Window %" PRIu64 " had %lu fences, %lu avail sems, and %lu finished sems created", winstate->index, fence_ct, avail_ct, fence_ct);

    for (size_t j = 0; j < avail_ct; j++) {
        if ((lahar->vkresult = vkCreateSemaphore(lahar->device, &sem_info, lahar->vkalloc, &winstate->image_available[j])) != VK_SUCCESS) {
            err = LAHAR_ERR_VK_ERR;
            goto end;
        }
    }

    for (size_t j = 0; j < fin_ct; j++) {
        if ((lahar->vkresult = vkCreateSemaphore(lahar->device, &sem_info, lahar->vkalloc, &winstate->render_finished[j])) != VK_SUCCESS) {
            err = LAHAR_ERR_VK_ERR;
            goto end;
        }
    }

    for (size_t j = 0; j < fence_ct; j++) {
        if ((lahar->vkresult = vkCreateFence(lahar->device, &fence_info, lahar->vkalloc, &winstate->in_flight[j])) != VK_SUCCESS) {
            err = LAHAR_ERR_VK_ERR;
            goto end;
        }
    }

    for (size_t j = 0; j < fin_ct; j++) {
        if ((lahar->vkresult = vkCreateFence(lahar->device, &fence_info, lahar->vkalloc, &winstate->present_fences[j])) != VK_SUCCESS) {
            err = LAHAR_ERR_VK_ERR;
            goto end;
        }
    }

end:
    lahar_temp_mpop();
    return err;
}

// Called by deinit, so has to check all func pointers before use
static void __lahar_deinit_window_swapchain(LaharWindowState* state) {
    if (!state) { return; }

    if (vkDestroyFence) {
        for (uint32_t i = 0; i < state->in_flight_size; i++) {
            vkDestroyFence(lahar->device, state->in_flight[i], lahar->vkalloc);
        }

        for (uint32_t i = 0; i < state->present_fences_size; i++) {
            vkDestroyFence(lahar->device, state->present_fences[i], lahar->vkalloc);
        }
    }

    lahar_free(state->in_flight);
    lahar_free(state->present_fences);

    if (vkDestroySemaphore) {
        for (uint32_t i = 0; i < state->render_finished_size; i++) {
            vkDestroySemaphore(lahar->device, state->render_finished[i], lahar->vkalloc);
        }

        for (uint32_t i = 0; i < state->image_available_size; i++) {
            vkDestroySemaphore(lahar->device, state->image_available[i], lahar->vkalloc);
        }
    }

    lahar_free(state->render_finished);
    lahar_free(state->image_available);

    if (vkDestroyCommandPool && state->pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(lahar->device, state->pool, lahar->vkalloc);
    }

    lahar_free(state->commands);

    // Special pass to destroy only the views (for the color attachment, images came from swap)

    if (state->attachments) {
        if (state->attachments[0]) {
            for (uint32_t i = 0; i < state->swap_size; i++) {
                LaharAttachment* attachment = &state->attachments[0][i];

                if (attachment && attachment->view != VK_NULL_HANDLE && vkDestroyImageView) {
                    vkDestroyImageView(lahar->device, attachment->view, lahar->vkalloc);
                }
            }
        }

        lahar_free(state->attachments[0]);

        for (uint32_t i = 1; i < state->attachment_count; i++) {
            LaharAttachment* attachment_list = state->attachments[i];
            if (!attachment_list) { continue; }

            for (uint32_t j = 0; j < state->swap_size; j++) {
                LaharAttachment* attachment = &state->attachments[i][j];

                if (attachment->view != VK_NULL_HANDLE && vkDestroyImageView) {
                    vkDestroyImageView(lahar->device, attachment->view, lahar->vkalloc);
                }

                if (attachment->image != VK_NULL_HANDLE && lahar->gpu_allocator) {
                    lahar->gpu_allocator->free_image(lahar->gpu_allocator, attachment->image, attachment->img_allocation);
                }
            }

            lahar_free(attachment_list);
        }
    }

    lahar_free(state->attachments);
    lahar_free(state->attachment_configs);

    if (state->swapchain != VK_NULL_HANDLE && vkDestroySwapchainKHR) {
        vkDestroySwapchainKHR(lahar->device, state->swapchain, lahar->vkalloc);
    }

    if (state->surface != VK_NULL_HANDLE && vkDestroySurfaceKHR) {
        vkDestroySurfaceKHR(lahar->instance, state->surface, lahar->vkalloc);
    }
}

/* A retired window is reclaimable when every in_flight fence it carried is
 * signaled. Fences are only ever reset immediately before a submit that
 * signals them, so unsignaled means the GPU still owns work recorded against
 * this swapchain, and signaled means all submits completed. */
static bool __lahar_window_retire_reclaimable(const LaharWindowState* entry) {
    if (!vkGetFenceStatus) { return false; }

    for (uint32_t i = 0; i < entry->in_flight_size; i++) {
        if (vkGetFenceStatus(lahar->device, entry->in_flight[i]) != VK_SUCCESS) { return false; }
    }

    // Present fences: only ever unsignaled while a present that will signal
    // them is pending (reset happens immediately before vkQueuePresentKHR).
    // Without swapchain_maintenance1 they are never attached, stay signaled
    // from creation, and this loop passes trivially -- the flush's
    // present-queue drain covers the gap in that case.
    for (uint32_t i = 0; i < entry->present_fences_size; i++) {
        if (vkGetFenceStatus(lahar->device, entry->present_fences[i]) != VK_SUCCESS) { return false; }
    }

    return true;
}

/** Destroy retired windows. Forced: device idle, destroy everything.
 * Unforced: early-out if nothing is reclaimable (the common case, no waits).
 * Otherwise destroy the entries whose fences prove their work complete --
 * with swapchain_maintenance1 that proof includes present completion, so no
 * wait; without it, one present-queue idle first (brief: only satisfied
 * presents can remain). Unreclaimable entries stay queued either way. */
static void __lahar_flush_window_retire_queue(bool force) {
    if (lahar->window_retire_count == 0) { return; }

    if (force && vkDeviceWaitIdle) {
        vkDeviceWaitIdle(lahar->device);
    }

    bool reclaim[LAHAR_MAX_WINDOW_RETIRES];
    bool any = false;

    for (uint32_t i = 0; i < lahar->window_retire_count; i++) {
        reclaim[i] = force || __lahar_window_retire_reclaimable(&lahar->window_retire_queue[i]);
        any = any || reclaim[i];
    }

    if (!any) { return; }

    if (!force && !lahar->swapchain_maintenance1) {
        if (vkQueueWaitIdle && lahar->presentQueue != VK_NULL_HANDLE) {
            vkQueueWaitIdle(lahar->presentQueue);
        }
    }

    uint32_t kept = 0;

    for (uint32_t i = 0; i < lahar->window_retire_count; i++) {
        LaharWindowState* entry = &lahar->window_retire_queue[i];

        if (reclaim[i]) {
            __lahar_deinit_window_swapchain(entry);
        }
        else {
            lahar->window_retire_queue[kept++] = *entry;
        }
    }

    lahar->window_retire_count = kept;
}

/** Queue a window for retirement. The pushed entry is never destroyed by
 * this call, so handles copied out of `state` beforehand remain valid until
 * the next flush. */
static void __lahar_retire_window(LaharWindowState* state) {
    if (lahar->window_retire_count >= LAHAR_MAX_WINDOW_RETIRES) {
        __lahar_flush_window_retire_queue(false);

        if (lahar->window_retire_count >= LAHAR_MAX_WINDOW_RETIRES) {
            __lahar_flush_window_retire_queue(true);
        }
    }

    LaharWindowState* retired = &lahar->window_retire_queue[lahar->window_retire_count++];

    LaharAttachmentConfig* configs = state->attachment_configs;
    VkSurfaceKHR surface = state->surface;

    *retired = *state;

    // Remove the stuff that shouldn't be destroyed in a retired window
    retired->surface = VK_NULL_HANDLE;
    retired->attachment_configs = NULL;

    // Reset the input state, but then copy back out the state
    // that retiring shouldn't destroy
    memset(state, 0, sizeof(*state));

    state->window = retired->window;
    state->surface = surface;
    state->index = retired->index;
    state->desired_img_count = retired->desired_img_count;
    state->max_in_flight = retired->max_in_flight;
    state->alpha = retired->alpha;
    state->auto_recreate_swap = retired->auto_recreate_swap;
    state->queued_destruction = retired->queued_destruction;
    state->resize_callback = retired->resize_callback;
    state->attachment_configs = configs;
    state->attachment_count = retired->attachment_count;
}























#if defined(LAHAR_USE_GLFW)
    uint32_t lahar_window_surface_create(LaharWindow* window, VkSurfaceKHR* surface) {
        if ((lahar->vkresult = glfwCreateWindowSurface(lahar->instance, window, lahar->vkalloc, surface)) != VK_SUCCESS) {
            return LAHAR_ERR_DEPENDENCY_FAILED;
        }

        return LAHAR_ERR_SUCCESS;
    }

    uint32_t lahar_window_get_size(LaharWindow* window, uint32_t* width, uint32_t* height) {
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);

        *width = (uint32_t)w;
        *height = (uint32_t)h;
        return LAHAR_ERR_SUCCESS;
    }


    uint32_t lahar_window_get_extensions(LaharWindow* window, uint32_t* ext_count, const char** extensions) {
        const char** ext = glfwGetRequiredInstanceExtensions(ext_count);

        if (extensions) {
            for (size_t i = 0; i < *ext_count; i++) {
                extensions[i] = ext[i];
            }
        }

        return LAHAR_ERR_SUCCESS;
    }
#elif defined(LAHAR_USE_SDL2)
    uint32_t lahar_window_surface_create(LaharWindow* window, VkSurfaceKHR* surface) {
        if (!SDL_Vulkan_CreateSurface(window, lahar->instance, surface)) {
            fprintf(stderr, "%s\n", SDL_GetError());
            return LAHAR_ERR_DEPENDENCY_FAILED;
        }

        return LAHAR_ERR_SUCCESS;
    }

    uint32_t lahar_window_get_size(LaharWindow* window, uint32_t* width, uint32_t* height) {
        int w, h;

        SDL_Vulkan_GetDrawableSize(window, &w, &h);

        *width = (uint32_t)w;
        *height = (uint32_t)h;
        return LAHAR_ERR_SUCCESS;
    }

    uint32_t lahar_window_get_extensions(LaharWindow* window, uint32_t* ext_count, const char** extensions) {
        if (!SDL_Vulkan_GetInstanceExtensions(window, ext_count, extensions)) {
            return LAHAR_ERR_DEPENDENCY_FAILED;
        }

        return LAHAR_ERR_SUCCESS;
    }
#elif defined(LAHAR_USE_SDL3)
    uint32_t lahar_window_surface_create(LaharWindow* window, VkSurfaceKHR* surface) {
        if (!SDL_Vulkan_CreateSurface(window, lahar->instance, lahar->vkalloc, surface)) {
            fprintf(stderr, "%s\n", SDL_GetError());
            return LAHAR_ERR_DEPENDENCY_FAILED;
        }

        return LAHAR_ERR_SUCCESS;
    }

    uint32_t lahar_window_get_size(LaharWindow* window, uint32_t* width, uint32_t* height) {
        int w, h;

        if (!SDL_GetWindowSizeInPixels(window, &w, &h)) {
            return LAHAR_ERR_DEPENDENCY_FAILED;
        }

        *width = (uint32_t)w;
        *height = (uint32_t)h;
        return LAHAR_ERR_SUCCESS;
    }

    uint32_t lahar_window_get_extensions(LaharWindow* window, uint32_t* ext_count, const char** extensions) {
        uint32_t count;
        char const* const* ext = SDL_Vulkan_GetInstanceExtensions(&count);

        *ext_count = count;

        if (extensions) {
            for (size_t i = 0; i < count; i++) {
                extensions[i] = (const char*)ext[i];
            }
        }

        return LAHAR_ERR_SUCCESS;
    }
#elif !defined(LAHAR_CUSTOM_WINDOW)
    uint32_t lahar_window_surface_create(LaharWindow* window, VkSurfaceKHR* surface) {
        if (surface) { *surface = VK_NULL_HANDLE; }
        return LAHAR_ERR_SUCCESS;
    }

    uint32_t lahar_window_get_size(LaharWindow* window, uint32_t* width, uint32_t* height) {
        if (width) { *width = 0; }
        if (height) { *height = 0; }
        return LAHAR_ERR_SUCCESS;
    }

    uint32_t lahar_window_get_extensions(LaharWindow* window, uint32_t* ext_count, const char** extensions) {
        if (ext_count) { *ext_count = 0; }
        return LAHAR_ERR_SUCCESS;
    }
#endif


#if defined(LAHAR_USE_VMA)
    static uint32_t __lahar_vma_alloc_img(
            void* self,
            const VkImageCreateInfo* info,
            const LaharAllocationCreateInfo* alloc_info,
            VkImage* image,
            VmaAllocation* allocation
    ) {
        if (!self) { return LAHAR_ERR_INVALID_STATE; }
        if (!lahar->vma) { return LAHAR_ERR_INVALID_CONFIGURATION; }
        if (!info || !alloc_info || !image || !allocation) { return LAHAR_ERR_ILLEGAL_PARAMS; }

        VmaAllocationCreateInfo alloc_create = ZINIT;
        VmaAllocationInfo vma_alloc_info = ZINIT;

        if ((lahar->vkresult = vmaCreateImage(lahar->vma, info, &alloc_create, image, allocation, &vma_alloc_info)) != VK_SUCCESS) {
            return LAHAR_ERR_DEPENDENCY_FAILED;
        }

        return LAHAR_ERR_SUCCESS;
    }

    static uint32_t __lahar_vma_free_img(
        void* self,
        VkImage image,
        VmaAllocation allocation
    ) {
        if (!self) { return LAHAR_ERR_INVALID_STATE; }
        if (!lahar->vma) { return LAHAR_ERR_INVALID_CONFIGURATION; }
        if (!image || !allocation) { return LAHAR_ERR_ILLEGAL_PARAMS; }

        vmaDestroyImage(lahar->vma, image, allocation);

        return LAHAR_ERR_SUCCESS;
    }

    static uint32_t __lahar_vma_alloc_buffer(
        void* self,
        const VkBufferCreateInfo* info,
        const LaharAllocationCreateInfo* alloc_info,
        VkBuffer* buffer,
        VmaAllocation* allocation
    ) {
        if (!self) { return LAHAR_ERR_INVALID_STATE; }
        if (!lahar->vma) { return LAHAR_ERR_INVALID_CONFIGURATION; }
        if (!info || !alloc_info || !buffer || !allocation) { return LAHAR_ERR_ILLEGAL_PARAMS; }

        VmaAllocationCreateInfo alloc_create = ZINIT;
        VmaAllocationInfo vma_alloc_info = ZINIT;

        switch (alloc_info->usage) {
            case LAHAR_MU_DEVICE_ONLY:
                alloc_create.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
                break;
            case LAHAR_MU_STAGING_SEQUENTIAL:
                alloc_create.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
                alloc_create.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
                break;
            case LAHAR_MU_UPLOAD_DIRECT:
                alloc_create.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
                alloc_create.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
                break;
            case LAHAR_MU_READBACK:
                alloc_create.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
                alloc_create.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
                break;
            default:
                return LAHAR_ERR_ILLEGAL_PARAMS;
        }

        if ((lahar->vkresult = vmaCreateBuffer(lahar->vma, info, &alloc_create, buffer, allocation, &vma_alloc_info)) != VK_SUCCESS) {
            return LAHAR_ERR_DEPENDENCY_FAILED;
        }

        return LAHAR_ERR_SUCCESS;
    }

    static uint32_t __lahar_vma_free_buffer(void* self, VkBuffer buffer, VmaAllocation allocation) {
        if (!self) { return LAHAR_ERR_INVALID_STATE; }
        if (!lahar->vma) { return LAHAR_ERR_INVALID_CONFIGURATION; }
        if (!buffer || !allocation) { return LAHAR_ERR_ILLEGAL_PARAMS; }

        vmaDestroyBuffer(lahar->vma, buffer, allocation);

        return LAHAR_ERR_SUCCESS;
    }

    static uint32_t __lahar_vma_map(void* self, VmaAllocation allocation, void** out) {
        if (!self) { return LAHAR_ERR_INVALID_STATE; }
        if (!lahar->vma) { return LAHAR_ERR_INVALID_CONFIGURATION; }
        if (!allocation || !out) { return LAHAR_ERR_ILLEGAL_PARAMS; }

        if ((lahar->vkresult = vmaMapMemory(lahar->vma, allocation, out)) != VK_SUCCESS) {
            return LAHAR_ERR_DEPENDENCY_FAILED;
        }

        return LAHAR_ERR_SUCCESS;
    }

    static uint32_t __lahar_vma_unmap(void* self, VmaAllocation allocation) {
        if (!self) { return LAHAR_ERR_INVALID_STATE; }
        if (!lahar->vma) { return LAHAR_ERR_INVALID_CONFIGURATION; }
        if (!allocation) { return LAHAR_ERR_ILLEGAL_PARAMS; }

        vmaUnmapMemory(lahar->vma, allocation);

        return LAHAR_ERR_SUCCESS;
    }

    static uint32_t __lahar_vma_flush(void* self, VmaAllocation allocation, uint64_t off, uint64_t size) {
        if (!self) { return LAHAR_ERR_INVALID_STATE; }
        if (!lahar->vma) { return LAHAR_ERR_INVALID_CONFIGURATION; }
        if (!allocation) { return LAHAR_ERR_ILLEGAL_PARAMS; }

        if ((lahar->vkresult = vmaFlushAllocation(lahar->vma, allocation, off, size)) != VK_SUCCESS) {
            return LAHAR_ERR_DEPENDENCY_FAILED;
        }

        return LAHAR_ERR_SUCCESS;
    }

    static uint32_t __lahar_vma_invalidate(void* self, VmaAllocation allocation, uint64_t off, uint64_t size) {
        if (!self) { return LAHAR_ERR_INVALID_STATE; }
        if (!lahar->vma) { return LAHAR_ERR_INVALID_CONFIGURATION; }
        if (!allocation) { return LAHAR_ERR_ILLEGAL_PARAMS; }

        if ((lahar->vkresult = vmaInvalidateAllocation(lahar->vma, allocation, off, size)) != VK_SUCCESS) {
            return LAHAR_ERR_DEPENDENCY_FAILED;
        }

        return LAHAR_ERR_SUCCESS;
    }

    static LaharAllocator __lahar_vma_adapter = {
        .alloc_image = __lahar_vma_alloc_img,
        .free_image = __lahar_vma_free_img,
        .alloc_buffer = __lahar_vma_alloc_buffer,
        .free_buffer = __lahar_vma_free_buffer,
        .map = __lahar_vma_map,
        .unmap = __lahar_vma_unmap,
        .flush = __lahar_vma_flush,
        .invalidate = __lahar_vma_invalidate
    };

    uint32_t lahar_vma_set_allocator(VmaAllocator allocator) {
        if (!allocator) { return LAHAR_ERR_ILLEGAL_PARAMS; }

        lahar->vma = allocator;
        lahar->gpu_allocator_defaulted = false;
        return LAHAR_ERR_SUCCESS;
    }

    static uint32_t __lahar_init_vma() {
        if (!lahar->gpu_allocator) {
            lahar->gpu_allocator = &__lahar_vma_adapter;
        }

        if (!lahar->vma) {
            VmaVulkanFunctions funcs = {
                .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
                .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
                .vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
                .vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
                .vkAllocateMemory = vkAllocateMemory,
                .vkFreeMemory = vkFreeMemory,
                .vkMapMemory = vkMapMemory,
                .vkUnmapMemory = vkUnmapMemory,
                .vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges,
                .vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges,
                .vkBindBufferMemory = vkBindBufferMemory,
                .vkBindImageMemory = vkBindImageMemory,
                .vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements,
                .vkGetImageMemoryRequirements = vkGetImageMemoryRequirements,
                .vkCreateBuffer = vkCreateBuffer,
                .vkDestroyBuffer = vkDestroyBuffer,
                .vkCreateImage = vkCreateImage,
                .vkDestroyImage = vkDestroyImage,
                .vkCmdCopyBuffer = vkCmdCopyBuffer,

                #if VMA_DEDICATED_ALLOCATION || VMA_VULKAN_VERSION >= 1001000
                .vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2KHR,
                .vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2KHR,
                #endif

                #if VMA_BIND_MEMORY2 || VMA_VULKAN_VERSION >= 1001000
                .vkBindBufferMemory2KHR = vkBindBufferMemory2KHR,
                .vkBindImageMemory2KHR = vkBindImageMemory2KHR,
                #endif

                #if VMA_MEMORY_BUDGET || VMA_VULKAN_VERSION >= 1001000
                .vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2KHR,
                #endif

                #if VMA_KHR_MAINTENANCE4 || VMA_VULKAN_VERSION >= 1003000
                .vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements,
                .vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements,
                #endif

                #if defined(VK_KHR_external_memory_win32)
                .vkGetMemoryWin32HandleKHR = vkGetMemoryWin32HandleKHR,
                #endif
            };

            VmaAllocatorCreateInfo alloc_create = {
                .physicalDevice = lahar->physdev_info.physdev,
                .device = lahar->device,
                .pAllocationCallbacks = lahar->vkalloc,
                .pVulkanFunctions = &funcs,
                .instance = lahar->instance,
                .vulkanApiVersion = lahar->vkversion,
            };

            if ((lahar->vkresult = vmaCreateAllocator(&alloc_create, &lahar->vma)) != VK_SUCCESS) {
                return LAHAR_ERR_DEPENDENCY_FAILED;
            }

            lahar->gpu_allocator_defaulted = true;
        }

        return LAHAR_ERR_SUCCESS;
    }

    static void __lahar_deinit_vma() {
        if (lahar->gpu_allocator_defaulted) {
            vmaDestroyAllocator(lahar->vma);
        }
    }
#endif




static VKAPI_ATTR VkBool32 VKAPI_CALL __lahar_default_dbgcallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* pcallbackdata,
    void* puserdata
) {
    switch (severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            printf("[VKTRACE] %s\n", pcallbackdata->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            printf("[VKINFO] %s\n", pcallbackdata->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            printf("[VKWARN] %s\n", pcallbackdata->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            printf("[VKERROR] %s\n", pcallbackdata->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT:
        default:
            break;
    }

    return VK_FALSE;
}

static int64_t __lahar_default_scorer(const LaharDeviceInfo* devinfo) {
    if (!devinfo->has_graphics_queue || !devinfo->has_present_queue) { return -1; }

    if (lahar->require_min_version && devinfo->properties.apiVersion < lahar->vkversion) {
        return -1;
    }

    int64_t score = 0;

    // Heavily favor discrete GPUs, minor bonus to iGPU, CPU gets no bonus
    if (devinfo->properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }
    else if (devinfo->properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        score += 100;
    }

    // A minor bonus if it has one shared queue for simplicity sake
    if (devinfo->present_queue_index == devinfo->graphics_queue_index) {
        score += 50;
    }

    size_t local_memory = 0;

    for (size_t i = 0; i < devinfo->memprops.memoryHeapCount; i++) {
        if (devinfo->memprops.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            local_memory += devinfo->memprops.memoryHeaps[i].size;
        }
    }

    // Rescale it so a 100gb gpu gets 1000 points
    // So for example, an 8gb gpu gets about 80 points
    const double hundred_gib = 107374182400.0;
    const double rescaled_base = (double)local_memory / hundred_gib;
    const double rescaled = rescaled_base * 1000.0;

    const size_t remapped_memory = (size_t)rescaled;
    score += (int64_t)remapped_memory;

    return score;
}

static uint32_t __lahar_default_surface_format_chooser(LaharWindowState* window_state, LaharDeviceInfo* physdev_info, VkSurfaceFormatKHR* surface_fmt_out){
    for (size_t i = 0; i < physdev_info->surface_fmt_count; i++) {
        VkSurfaceFormatKHR* fmt = &physdev_info->surface_formats[i];

        if (fmt->format == VK_FORMAT_B8G8R8A8_SRGB && fmt->colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
            *surface_fmt_out = *fmt;
            return LAHAR_ERR_SUCCESS;
        }
    }

    *surface_fmt_out = physdev_info->surface_formats[0];
    return LAHAR_ERR_SUCCESS;
}

static uint32_t __lahar_default_surface_present_mode_chooser(LaharWindowState* window_state, LaharDeviceInfo* physdev_info, VkPresentModeKHR* present_mode_out) {
    for (size_t i = 0; i < physdev_info->present_mode_count; i++) {
        if (physdev_info->present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            *present_mode_out = VK_PRESENT_MODE_MAILBOX_KHR;
            return LAHAR_ERR_SUCCESS;
        }
    }

    *present_mode_out = VK_PRESENT_MODE_FIFO_KHR;
    return LAHAR_ERR_SUCCESS;
}

static uint32_t __lahar_default_resizer(LaharWindow* window) {
    LaharWindowState* winstate = lahar_window_state(window);
    if (!winstate) { return LAHAR_ERR_INVALID_WINDOW; }

    VkSwapchainKHR old_swap = winstate->swapchain;

    __lahar_retire_window(winstate);

    if (!winstate->queued_destruction) {
        __lahar_flush_window_retire_queue(true);
        old_swap = VK_NULL_HANDLE;
    }

    __lahar_init_window_swapchain(winstate, old_swap);

    return LAHAR_ERR_SUCCESS;
}




const char* lahar_err_name(uint32_t code) {
    switch (code) {
        case LAHAR_ERR_SUCCESS: return "LAHAR_ERR_SUCCESS";
        case LAHAR_ERR_ILLEGAL_PARAMS: return "LAHAR_ERR_ILLEGAL_PARAMS";
        case LAHAR_ERR_LOAD_FAILURE: return "LAHAR_ERR_LOAD_FAILURE";
        case LAHAR_ERR_INVALID_CONFIGURATION: return "LAHAR_ERR_INVALID_CONFIGURATION";
        case LAHAR_ERR_MISSING_EXTENSION: return "LAHAR_ERR_MISSING_EXTENSION";
        case LAHAR_ERR_NO_SUITABLE_DEVICE: return "LAHAR_ERR_NO_SUITABLE_DEVICE";
        case LAHAR_ERR_DEPENDENCY_FAILED: return "LAHAR_ERR_DEPENDENCY_FAILED";
        case LAHAR_ERR_ALLOC_FAILED: return "LAHAR_ERR_ALLOC_FAILED";
        case LAHAR_ERR_INVALID_STATE: return "LAHAR_ERR_INVALID_STATE";
        case LAHAR_ERR_VK_ERR: return "LAHAR_ERR_VK_ERR";
        case LAHAR_ERR_INVALID_WINDOW: return "LAHAR_ERR_INVALID_WINDOW";
        case LAHAR_ERR_NO_COMMAND_BUFFER: return "LAHAR_ERR_NO_COMMAND_BUFFER";
        case LAHAR_ERR_TIMEOUT: return "LAHAR_ERR_TIMEOUT";
        case LAHAR_ERR_SWAPCHAIN_OUT_OF_DATE: return "LAHAR_ERR_SWAPCHAIN_OUT_OF_DATE";
        case LAHAR_ERR_INVALID_FRAME_STATE: return "LAHAR_ERR_INVALID_FRAME_STATE";
        case LAHAR_ERR_ATTACHMENT_WO_ALLOCATOR: return "LAHAR_ERR_ATTACHMENT_WO_ALLOCATOR";
        case LAHAR_ERR_UNKNOWN_LANGUAGE: return "LAHAR_ERR_UNKNOWN_LANGUAGE";
        case LAHAR_ERR_MALFORMED_CODE: return "LAHAR_ERR_MALFORMED_CODE";
        case LAHAR_ERR_ID_NOT_FOUND: return "LAHAR_ERR_ID_NOT_FOUND";
        case LAHAR_ERR_INVALID_TYPE: return "LAHAR_ERR_INVALID_TYPE";
        case LAHAR_ERR_COMPILATION_FAILED: return "LAHAR_ERR_COMPILATION_FAILED";
        case LAHAR_ERR_OUT_OF_SPACE: return "LAHAR_ERR_OUT_OF_SPACE";
        case LAHAR_ERR_MEMORY_UNSATISFIABLE: return "LAHAR_ERR_MEMORY_UNSATISFIABLE";
        case LAHAR_ERR_VERSION_UNSATISFIABLE: return "LAHAR_ERR_VERSION_UNSATISFIABLE";
        case LAHAR_ERR_NOT_IMPLEMENTED: return "LAHAR_ERR_NOT_IMPLEMENTED";
        default: return "LAHAR_UNKNOWN_ERROR";
    }
}



void* lahar_get_user_data(void) {
    return lahar->user_data;
}

void lahar_set_user_data(void* user_data) {
    lahar->user_data = user_data;
}


uint32_t lahar_init(void) {
    memset(lahar, 0, sizeof(*lahar));

    #if !defined(LAHAR_NO_AUTO_DEP)
        #if defined(LAHAR_USE_GLFW)

        if (!glfwInit()) {
            return LAHAR_ERR_DEPENDENCY_FAILED;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        #elif defined(LAHAR_USE_SDL2)

        if (SDL_Init(SDL_INIT_EVERYTHING)) {
            return LAHAR_ERR_DEPENDENCY_FAILED;
        }

        #elif defined(LAHAR_USE_SDL3)

        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
            return LAHAR_ERR_DEPENDENCY_FAILED;
        }

        #endif
    #endif


    uint32_t err = LAHAR_ERR_SUCCESS;

    if ((err = __lahar_open_libvk())) {
        return err;
    }

    if ((err = lahar_load_loader(lahar_loader_sym))) {
        return err;
    }

    return LAHAR_ERR_SUCCESS;
}

uint32_t lahar_builder_allocator_set(LaharAllocator* allocator) {
    if (
        !allocator ||
        !allocator->alloc_image ||
        !allocator->free_image ||
        !allocator->alloc_buffer ||
        !allocator->free_buffer ||
        !allocator->map ||
        !allocator->unmap ||
        !allocator->flush ||
        !allocator->invalidate
    ) {
        return LAHAR_ERR_ILLEGAL_PARAMS;
    }

    lahar->gpu_allocator = allocator;
    return LAHAR_ERR_SUCCESS;
}

void lahar_builder_set_debug_level(LaharDebugLevel level) {
    if (level >= LAHAR_DEBUG_TRACE && level <= LAHAR_DEBUG_DISABLED) {
        lahar->debug_level = level;
    }
}

void lahar_builder_set_vulkan_version(uint32_t version, bool required) {
    if (lahar->instance == VK_NULL_HANDLE) {
        lahar->vkversion = version;
        lahar->require_min_version = required;
    }
}

void lahar_builder_request_validation_layers(void) {
    lahar->wantvalidation = true;
}

uint32_t lahar_builder_extension_add_required_instance(const char* extension) {
    if (!extension) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    if (lahar->extensions.rie_count >= lahar->extensions.rie_cap) {
        lahar_vec_expand(lahar->extensions.req_inst_exts, lahar->extensions.rie_cap) else {
            return LAHAR_ERR_ALLOC_FAILED;
        }
    }

    const char* cpy = lahar_strdup(extension);
    if (!cpy) { return LAHAR_ERR_ALLOC_FAILED; }

    lahar->extensions.req_inst_exts[lahar->extensions.rie_count++] = cpy;

    return LAHAR_ERR_SUCCESS;
}

uint32_t lahar_builder_extension_add_required_device(const char* extension) {
    if (!extension) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    if (lahar->extensions.rde_count >= lahar->extensions.rde_cap) {
        lahar_vec_expand(lahar->extensions.req_dev_exts, lahar->extensions.rde_cap) else {
            return LAHAR_ERR_ALLOC_FAILED;
        }
    }

    const char* cpy = lahar_strdup(extension);
    if (!cpy) { return LAHAR_ERR_ALLOC_FAILED; }

    lahar->extensions.req_dev_exts[lahar->extensions.rde_count++] = cpy;

    return LAHAR_ERR_SUCCESS;
}

uint32_t lahar_builder_extension_add_optional_instance(const char* extension) {
    if (!extension) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    if (lahar->extensions.oie_count >= lahar->extensions.oie_cap) {
        size_t current_cap = lahar->extensions.oie_cap;

        {
            lahar_vec_expand(lahar->extensions.opt_inst_exts, current_cap) else {
                return LAHAR_ERR_ALLOC_FAILED;
            }
        }

        {
            lahar_vec_expand(lahar->extensions.opt_inst_exts_present, lahar->extensions.oie_cap) else {
                return LAHAR_ERR_ALLOC_FAILED;
            }
        }
    }

    const char* cpy = lahar_strdup(extension);
    if (!cpy) { return LAHAR_ERR_ALLOC_FAILED; }

    size_t count = lahar->extensions.oie_count;

    lahar->extensions.opt_inst_exts[count] = cpy;
    lahar->extensions.opt_inst_exts_present[count] = false;

    lahar->extensions.oie_count++;

    return LAHAR_ERR_SUCCESS;
}

uint32_t lahar_builder_extension_add_optional_device(const char* extension) {
    if (!extension) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    if (lahar->extensions.ode_count >= lahar->extensions.ode_cap) {
        size_t current_cap = lahar->extensions.ode_cap;

        {
            lahar_vec_expand(lahar->extensions.opt_dev_exts, current_cap) else {
                return LAHAR_ERR_ALLOC_FAILED;
            }
        }

        {
            lahar_vec_expand(lahar->extensions.opt_dev_exts_present, lahar->extensions.ode_cap) else {
                return LAHAR_ERR_ALLOC_FAILED;
            }
        }
    }

    const char* cpy = lahar_strdup(extension);
    if (!cpy) { return LAHAR_ERR_ALLOC_FAILED; }

    const size_t count = lahar->extensions.ode_count;

    lahar->extensions.opt_dev_exts[count] = cpy;
    lahar->extensions.opt_dev_exts_present[count] = false;

    lahar->extensions.ode_count++;

    return LAHAR_ERR_SUCCESS;
}

void lahar_builder_set_debug_callback(PFN_vkDebugUtilsMessengerCallbackEXT callback) {
    lahar->debug_callback = callback;
}

uint32_t lahar_builder_device_use(const char* name) {
    if (!name || name[0] == '\0') { return LAHAR_ERR_ILLEGAL_PARAMS; }

    char* cpy = (char*)lahar_strdup(name);
    if (!cpy) { return LAHAR_ERR_ALLOC_FAILED; }

    lahar->device_name = cpy;
    return LAHAR_ERR_SUCCESS;
}

uint32_t lahar_builder_device_set_scoring(LaharDeviceScoreFunc scorefunc) {
    if (!scorefunc) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    lahar->score_func = scorefunc;
    return LAHAR_ERR_SUCCESS;
}

void lahar_builder_request_command_buffers(void) {
    lahar->wantcommands = true;
}

void lahar_builder_set_device_create_pnext(void* pnext) {
    lahar->device_create_pnext = pnext;
}

uint32_t lahar_builder_window_register_ex(LaharWindow* window, const LaharWindowConfig* winconf) {
    if (!window || !winconf || winconf->attachment_count == 0) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    uint32_t err = LAHAR_ERR_SUCCESS;
    LaharWindowState* window_state = NULL;

    if (lahar->window_count >= lahar->window_cap) {
        lahar_vec_expand(lahar->windows, lahar->window_cap) else {
            err = LAHAR_ERR_ALLOC_FAILED;
            goto end;
        }
    }

    window_state = &lahar->windows[lahar->window_count++];
    memset(window_state, 0, sizeof(*window_state));

    window_state->window = window;

    if ((err = lahar_window_get_size(window, &window_state->width, &window_state->height))) {
        goto end;
    }

    window_state->attachment_count = winconf->attachment_count;
    window_state->attachment_configs = (LaharAttachmentConfig*)lahar_malloc(window_state->attachment_count * sizeof(LaharAttachmentConfig));

    memcpy(window_state->attachment_configs, winconf->attachments, window_state->attachment_count * sizeof(LaharAttachmentConfig));

    window_state->desired_img_count = winconf->desired_swap_size ? winconf->desired_swap_size : 2;
    window_state->max_in_flight = winconf->max_in_flight ? winconf->max_in_flight : 2;

    //window_state->attachments = (LaharAttachment**)lahar_malloc(window_state->attachment_count * sizeof(void*));

    window_state->auto_recreate_swap = !winconf->no_auto_swap_resize;

    // We can't create these until after the swap chain, as we don't know how
    // big to make these arrays yet
    //for (size_t i = 0; i < window_state->attachment_count; i++) {
    //    window_state->attachments[i] = NULL;
    //}

end:
    return err;
}

uint32_t lahar_builder_window_register(LaharWindow* window, LaharWindowProfile winprof) {
    if (!window) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    switch(winprof) {
        case LAHAR_WINPROF_COLOR: {
            // Attachment 0 is ignored anyway, but must be specified for color
            LaharAttachmentConfig attachments = ZINIT;

            const LaharWindowConfig conf = {
                .attachment_count = 1,
                .attachments = &attachments
            };

            return lahar_builder_window_register_ex(window, &conf);

        } break;

        case LAHAR_WINPROF_COLOR_DEPTH_STENCIL: {
            // Attachment 0 is ignored anyway, but must be specified for color
            LaharAttachmentConfig attachments[] = {
                ZINIT,
                {
                    .role = LAHAR_ATTROLE_DEPTH_STENCIL,
                    .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                    .description = {
                        .format = VK_FORMAT_D32_SFLOAT,
                        .samples = VK_SAMPLE_COUNT_1_BIT,
                        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                    },
                    .img_info = {
                        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                        .imageType = VK_IMAGE_TYPE_2D,
                        .format = VK_FORMAT_D32_SFLOAT,
                        .mipLevels = 1,
                        .arrayLayers = 1,
                        .samples = VK_SAMPLE_COUNT_1_BIT,
                        .tiling = VK_IMAGE_TILING_OPTIMAL,
                        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
                    },
                    .view_info = {
                        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                        .viewType = VK_IMAGE_VIEW_TYPE_2D,
                        .format = VK_FORMAT_D32_SFLOAT,
                        .subresourceRange = {
                            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                            .baseMipLevel = 0,
                            .levelCount = 1,
                            .baseArrayLayer = 0,
                            .layerCount = 1
                        }
                    }
                }
            };

            LaharWindowConfig conf = {
                .attachment_count = sizeof(attachments) / sizeof(attachments[0]),
                .attachments = attachments
            };

            return lahar_builder_window_register_ex(window, &conf);
        } break;

        default:
            return LAHAR_ERR_ILLEGAL_PARAMS;
    }


}


bool lahar_extension_has_instance(const char* extension) {
    if (!extension) { return false; }

    for (size_t i = 0; i < lahar->extensions.oie_count; i++) {
        if (strcmp(extension, lahar->extensions.opt_inst_exts[i]) == 0) {
            return lahar->extensions.opt_inst_exts_present[i];
        }
    }

    for (size_t i = 0; i < lahar->extensions.rie_count; i++) {
        if (strcmp(extension, lahar->extensions.req_inst_exts[i]) == 0) {
            return true;
        }
    }

    return false;
}

bool lahar_extension_has_device(const char* extension) {
    if (!extension) { return false; }

    for (size_t i = 0; i < lahar->extensions.ode_count; i++) {
        if (strcmp(extension, lahar->extensions.opt_dev_exts[i]) == 0) {
            return lahar->extensions.opt_dev_exts_present[i];
        }
    }

    for (size_t i = 0; i < lahar->extensions.rde_count; i++) {
        if (strcmp(extension, lahar->extensions.req_dev_exts[i]) == 0) {
            return true;
        }
    }

    return false;
}




// Note that deinit checks _every_ function pointer before use
// This function must be safe to call in absolutely any possible failure state,
// including entire failure to load

void lahar_deinit(void) {
    lahar_trace("Deiniting lahar");

    if (vkDeviceWaitIdle) {
        vkDeviceWaitIdle(lahar->device);
    }

    __lahar_flush_window_retire_queue(true);

    for (uint64_t i = 0; i < lahar->window_count; i++) {
        LaharWindowState* state = &lahar->windows[i];
        __lahar_deinit_window_swapchain(state);

        #if !defined(LAHAR_NO_AUTO_DEP)

            #if defined(LAHAR_USE_GLFW)
            glfwDestroyWindow(state->window);
            #endif

            #if defined(LAHAR_USE_SDL2) || defined(LAHAR_USE_SDL3)
            SDL_DestroyWindow(state->window);
            #endif

        #endif
    }

    #if defined(LAHAR_USE_VMA)
    __lahar_deinit_vma();
    #else
    if (lahar->gpu_allocator_defaulted && lahar->gpu_allocator) {
        lahar_allocator_freelist_deinit(lahar->gpu_allocator);
    }
    #endif

    if (lahar->device != VK_NULL_HANDLE && vkDestroyDevice) {
        vkDestroyDevice(lahar->device, lahar->vkalloc);
    }

    if (lahar->debug_messenger != VK_NULL_HANDLE && vkDestroyDebugUtilsMessengerEXT) {
        vkDestroyDebugUtilsMessengerEXT(lahar->instance, lahar->debug_messenger, lahar->vkalloc);
    }

    if (lahar->instance != VK_NULL_HANDLE && vkDestroyInstance) {
        vkDestroyInstance(lahar->instance, lahar->vkalloc);
    }

    #if !defined(LAHAR_NO_AUTO_DEP)

        #if defined(LAHAR_USE_GLFW)
        glfwTerminate();
        #endif

        #if defined(LAHAR_USE_SDL2) || defined(LAHAR_USE_SDL3)
        SDL_Quit();
        #endif

    #endif

    for (size_t i = 0; i < lahar->extensions.rie_count; i++) {
        lahar_free((char*)lahar->extensions.req_inst_exts[i]);
    }

    for (size_t i = 0; i < lahar->extensions.rde_count; i++) {
        lahar_free((char*)lahar->extensions.req_dev_exts[i]);
    }

    for (size_t i = 0; i < lahar->extensions.oie_count; i++) {
        lahar_free((char*)lahar->extensions.opt_inst_exts[i]);
    }

    for (size_t i = 0; i < lahar->extensions.ode_count; i++) {
        lahar_free((char*)lahar->extensions.opt_dev_exts[i]);
    }

    lahar_free(lahar->extensions.req_inst_exts);
    lahar_free(lahar->extensions.req_dev_exts);
    lahar_free(lahar->extensions.opt_inst_exts);
    lahar_free(lahar->extensions.opt_dev_exts);
    lahar_free(lahar->extensions.opt_inst_exts_present);
    lahar_free(lahar->extensions.opt_dev_exts_present);

    for (size_t i = 0; i < LAHAR_MAX_SHADER_COMPILERS; i++) {
        if (lahar->shader_compilers[i].language) {
            lahar_free((char*)lahar->shader_compilers[i].language);
        }
    }

    __lahar_close_libvk();

    memset(lahar, 0, sizeof(*lahar));
}




static uint32_t __lahar_build_inst_extensions() {
    uint32_t err = LAHAR_ERR_SUCCESS;
    lahar_temp_mcheck();

    uint32_t ext_count;
    char** extensions = NULL;
    uint32_t prop_count;
    VkExtensionProperties* props = NULL;

    LaharWindow* window = lahar->window_count ? lahar->windows[0].window : NULL;

    // Assume the first window is sufficient
    __lahar_temp_extensions(window, &ext_count, &extensions);

    if ((lahar->vkresult = vkEnumerateInstanceExtensionProperties(NULL, &prop_count, NULL)) != VK_SUCCESS) {
        err = LAHAR_ERR_VK_ERR;
        goto end;
    }

    props = (VkExtensionProperties*)lahar_temp_alloc(prop_count * sizeof(VkExtensionProperties));
    if ((vkEnumerateInstanceExtensionProperties(NULL, &prop_count, props)) != VK_SUCCESS) {
        err = LAHAR_ERR_VK_ERR;
        goto end;
    }

    // validate they exist
    for (size_t i = 0; i < ext_count; i++) {
        const char* ext = extensions[i];
        bool found = false;

        for (size_t j = 0; j < prop_count; j++) {
            const VkExtensionProperties* prop = &props[j];

            if (strcmp(prop->extensionName, ext) == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            err = LAHAR_ERR_MISSING_EXTENSION;
            goto end;
        }
    }

end:
    lahar_temp_mpop();
    return err;
}

static uint32_t __lahar_build_instance(void) {
    uint32_t err = LAHAR_ERR_SUCCESS;
    lahar_temp_mcheck();

    if (lahar->debug_level == LAHAR_DEBUG_UNSET) {
        lahar->debug_level = LAHAR_DEBUG_ERROR;
    }

    uint32_t ext_count;
    char** extensions = NULL;
    uint32_t avail_layer_count;
    VkLayerProperties* layer_props = NULL;
    const char* dbg_layer_name = "VK_LAYER_KHRONOS_validation";
    bool dbg_layer_found = false;

    LaharWindow* window = lahar->window_count ? lahar->windows[0].window : NULL;

    // Assume the first window is sufficient
    __lahar_temp_extensions(window, &ext_count, &extensions);

    uint32_t requested_version = lahar->vkversion != 0 ? lahar->vkversion : LAHAR_DEFAULT_VK_VERSION;
    uint32_t available_version = 0;

    VkApplicationInfo appinfo = ZINIT;
    VkInstanceCreateInfo createinfo = ZINIT;

    if (vkEnumerateInstanceVersion) {
        if ((lahar->vkresult = vkEnumerateInstanceVersion(&available_version))) {
            goto end;
        }

        // Refuse to handle other variants, they're functionally different APIs
        if (VK_API_VERSION_VARIANT(available_version) != 0) {
            err = LAHAR_ERR_VERSION_UNSATISFIABLE;
            goto end;
        }
    }
    else {
        available_version = VK_MAKE_API_VERSION(0, 1, 0, 0);
    }

    if (lahar->require_min_version) {
        if (available_version < requested_version) {
            err = LAHAR_ERR_VERSION_UNSATISFIABLE;
            goto end;
        }
    }

    if (available_version < requested_version) {
        requested_version = available_version;
    }

    lahar->vkversion = requested_version;

    appinfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appinfo.pApplicationName = lahar->appname ? lahar->appname : "Lahar";
    appinfo.applicationVersion = lahar->appversion != 0 ? lahar->appversion : VK_MAKE_VERSION(1, 0, 0);
    appinfo.pEngineName = "None";
    appinfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appinfo.apiVersion = requested_version;

    createinfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createinfo.pApplicationInfo = &appinfo;
    createinfo.enabledExtensionCount = (uint32_t)ext_count;
    createinfo.ppEnabledExtensionNames = (const char* const *)extensions;

    if ((lahar->vkresult = vkEnumerateInstanceLayerProperties(&avail_layer_count, NULL)) != VK_SUCCESS) {
        err = LAHAR_ERR_VK_ERR;
        goto end;
    }

    layer_props = (VkLayerProperties*)lahar_temp_alloc(avail_layer_count * sizeof(VkLayerProperties));
    if ((lahar->vkresult = vkEnumerateInstanceLayerProperties(&avail_layer_count, layer_props)) != VK_SUCCESS) {
        err = LAHAR_ERR_VK_ERR;
        goto end;
    }

    if (lahar->wantvalidation) {
        for (size_t i = 0; i < avail_layer_count; i++) {
            if (strcmp(layer_props[i].layerName, dbg_layer_name) == 0) {
                createinfo.enabledLayerCount = 1;
                createinfo.ppEnabledLayerNames = &dbg_layer_name;
                dbg_layer_found = true;
                break;
            }
        }
    }

    if ((lahar->vkresult = vkCreateInstance(&createinfo, lahar->vkalloc, &lahar->instance)) != VK_SUCCESS) {
        err = LAHAR_ERR_VK_ERR;
        goto end;
    }

    if ((err = lahar_load_instance(__lahar_loader_inst))) {
        goto end;
    }

    if (lahar->wantvalidation) {
        if (!dbg_layer_found) {
            printf("Lahar: failed to find vulkan validation layers. Available layers:\n");
            for (size_t i = 0; i < avail_layer_count; i++) {
                printf("\t%s\n", layer_props[i].layerName);
            }
        }
        else {
            VkDebugUtilsMessengerCreateInfoEXT dbgcreateinfo = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                .pfnUserCallback = lahar->debug_callback ? lahar->debug_callback : __lahar_default_dbgcallback,
                .pUserData = lahar
            };

            if ((lahar->vkresult = vkCreateDebugUtilsMessengerEXT(lahar->instance, &dbgcreateinfo, lahar->vkalloc, &lahar->debug_messenger)) != VK_SUCCESS) {
                err = LAHAR_ERR_VK_ERR;
                goto end;
            }
        }
    }

end:
    lahar_temp_mpop();
    return err;
}

static uint32_t __lahar_build_early_surface(void) {
    uint32_t err = LAHAR_ERR_SUCCESS;

    for (uint64_t i = 0; i < lahar->window_count; i++) {
        LaharWindowState* winstate = &lahar->windows[i];
        winstate->index = i;

        if ((err = lahar_window_surface_create(winstate->window, &winstate->surface))) {
            return err;
        }
    }

    return err;
}

static uint32_t __lahar_build_physdev(void) {
    uint32_t err = LAHAR_ERR_SUCCESS;
    lahar_temp_mcheck();

    uint32_t dev_count;
    VkPhysicalDevice* devices = NULL;
    LaharDeviceInfo* dev_infos = NULL;
    int64_t* dev_scores = NULL;
    LaharDeviceScoreFunc scorer = lahar->score_func ? lahar->score_func : __lahar_default_scorer;
    int64_t best_dev = -1;
    int64_t best_score = -1;
    LaharDeviceInfo* info = NULL;

    if ((lahar->vkresult = vkEnumeratePhysicalDevices(lahar->instance, &dev_count, NULL)) != VK_SUCCESS) {
        err = LAHAR_ERR_VK_ERR;
        goto end;
    }

    devices = (VkPhysicalDevice*)lahar_temp_alloc(dev_count * sizeof(VkPhysicalDevice));
    dev_infos = (LaharDeviceInfo*)lahar_temp_alloc(dev_count * sizeof(LaharDeviceInfo));
    dev_scores = (int64_t*)lahar_temp_alloc(dev_count * sizeof(int64_t));

    if ((lahar->vkresult = vkEnumeratePhysicalDevices(lahar->instance, &dev_count, devices)) != VK_SUCCESS) {
        err = LAHAR_ERR_VK_ERR;
        goto end;
    }

    for (size_t i = 0; i < dev_count; i++) {
        LaharDeviceInfo* devinfo = &dev_infos[i];

        memset(devinfo, 0, sizeof(*devinfo));

        devinfo->physdev = devices[i];
        vkGetPhysicalDeviceProperties(dev_infos[i].physdev, &dev_infos[i].properties);
        vkGetPhysicalDeviceFeatures(dev_infos[i].physdev, &dev_infos[i].features);
        vkGetPhysicalDeviceMemoryProperties(dev_infos[i].physdev, &dev_infos[i].memprops);

        uint32_t queue_fam_ct = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devinfo->physdev, &queue_fam_ct, NULL);

        VkQueueFamilyProperties* queue_fams = (VkQueueFamilyProperties*)lahar_temp_alloc(queue_fam_ct * sizeof(VkQueueFamilyProperties));
        vkGetPhysicalDeviceQueueFamilyProperties(devinfo->physdev, &queue_fam_ct, queue_fams);

        for (uint32_t j = 0; j < queue_fam_ct; j++) {
            if (queue_fams[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                devinfo->graphics_queue_index = j;
                devinfo->has_graphics_queue = true;
            }

            VkBool32 presentSupport = true;

            for (size_t k = 0; k < lahar->window_count; k++) {
                VkBool32 thisWin = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(devinfo->physdev, j, lahar->windows[k].surface, &thisWin);

                if (!thisWin) { presentSupport = false; break; }
            }

            if (presentSupport) {
                devinfo->has_present_queue = true;
                devinfo->present_queue_index = j;
            }

            if (devinfo->has_graphics_queue && devinfo->has_present_queue) {
                break;
            }
        }

        if (lahar->window_count > 0) {
            uint32_t format_ct = 0;
            uint32_t present_ct = 0;

            vkGetPhysicalDeviceSurfaceFormatsKHR(devinfo->physdev, lahar->windows[0].surface, &format_ct, NULL);
            vkGetPhysicalDeviceSurfacePresentModesKHR(devinfo->physdev, lahar->windows[0].surface, &present_ct, NULL);

            VkSurfaceFormatKHR* formats = (VkSurfaceFormatKHR*)lahar_temp_alloc(format_ct * sizeof(VkSurfaceFormatKHR));
            VkPresentModeKHR* present_modes = (VkPresentModeKHR*)lahar_temp_alloc(present_ct * sizeof(VkPresentModeKHR));

            vkGetPhysicalDeviceSurfaceFormatsKHR(devinfo->physdev, lahar->windows[0].surface, &format_ct, formats);
            vkGetPhysicalDeviceSurfacePresentModesKHR(devinfo->physdev, lahar->windows[0].surface, &present_ct, present_modes);

            uint32_t to_copy = format_ct > LAHAR_MAX_DEVICE_ENTRIES ? LAHAR_MAX_DEVICE_ENTRIES : format_ct;
            memcpy(devinfo->surface_formats, formats, to_copy * sizeof(*formats));

            to_copy = present_ct > LAHAR_MAX_DEVICE_ENTRIES ? LAHAR_MAX_DEVICE_ENTRIES : present_ct;
            memcpy(devinfo->present_modes, present_modes, to_copy * sizeof(*present_modes));
        }
    }


    for (size_t i = 0; i < dev_count; i++) {
        dev_scores[i] = scorer(&dev_infos[i]);
    }


    for (int64_t i = 0; i < dev_count; i++) {
        if (dev_scores[i] > best_score) {
            best_dev = i;
            best_score = dev_scores[i];
        }
    }

    if (best_dev == -1) {
        err = LAHAR_ERR_NO_SUITABLE_DEVICE;
        goto end;
    }

    info = &dev_infos[best_dev];

    if (info->properties.apiVersion < lahar->vkversion) {
        lahar->vkversion = info->properties.apiVersion;
    }

    if (lahar->wantvalidation) {
        VkDebugUtilsMessengerCallbackDataEXT cbdata = ZINIT;

        char msgbuf[512];
        memset(msgbuf, 0, sizeof(msgbuf));
        snprintf(msgbuf, sizeof(msgbuf) - 1, "Selected Device: %s", info->properties.deviceName);

        cbdata.pMessage = msgbuf;

        if (lahar->debug_callback) {
            lahar->debug_callback(VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &cbdata, lahar);
        }
        else {
            __lahar_default_dbgcallback(VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &cbdata, lahar);
        }
    }

    lahar->physdev_info = *info;

end:
    lahar_temp_mpop();
    return err;
}

static uint32_t __lahar_build_device(void) {
    uint32_t err = LAHAR_ERR_SUCCESS;
    lahar_temp_mcheck();

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_infos[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = lahar->physdev_info.graphics_queue_index,
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        },
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = lahar->physdev_info.present_queue_index,
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        },
    };

    VkPhysicalDeviceFeatures device_features = ZINIT;

    const char* dbg_layer_name = "VK_LAYER_KHRONOS_validation";
    bool has_dbg_layer = false;

    VkDeviceCreateInfo device_create_info = ZINIT;

    if (lahar->wantvalidation) {
        uint32_t layer_count;
        vkEnumerateDeviceLayerProperties(lahar->physdev_info.physdev, &layer_count, NULL);

        VkLayerProperties* layer_props = (VkLayerProperties*)lahar_temp_alloc(layer_count * sizeof(VkLayerProperties));
        vkEnumerateDeviceLayerProperties(lahar->physdev_info.physdev, &layer_count, layer_props);

        for (size_t i = 0; i < layer_count; i++) {
            if (strcmp(layer_props[i].layerName, dbg_layer_name) == 0) {
                has_dbg_layer = true;
                break;
            }
        }
    }

    const char* swap_ext_name = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    const uint32_t queue_create_count = (lahar->physdev_info.graphics_queue_index == lahar->physdev_info.present_queue_index) ? 1 : 2;
    const uint32_t enabled_layer_count = has_dbg_layer ? 1 : 0;


    // User supplied pnext, might be NULL
    void* pnext = lahar->device_create_pnext;

    VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_feature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
        .dynamicRendering = VK_TRUE
    };

#if defined(VK_EXT_swapchain_maintenance1)
    VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT swap_maint1_feature = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT,
        .swapchainMaintenance1 = VK_TRUE
    };
#endif

    /* Dynamic rendering can arrive three ways: the KHR extension, the 1.3 core
     * feature bit via VkPhysicalDeviceVulkan13Features, or the standalone
     * feature struct. Resolve all of them here into lahar->dynamic_rendering so
     * that nothing downstream has to re-derive it (and so that nothing
     * downstream gets it wrong by only checking the extension). */
    {
        const VkPhysicalDeviceDynamicRenderingFeatures* dyn_feature =
        (const VkPhysicalDeviceDynamicRenderingFeatures* )__lahar_pnext_fetch(pnext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES);

        const VkPhysicalDeviceVulkan13Features* vk13_feature =
        (const VkPhysicalDeviceVulkan13Features* )__lahar_pnext_fetch(pnext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES);

        const bool has_ext = lahar_extension_has_device(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
        const bool is_1_3 = VK_API_VERSION_MINOR(lahar->vkversion) >= 3;

        if (has_ext) {
            if (vk13_feature) {
                if (!vk13_feature->dynamicRendering) {
                    lahar_error("The dynamic rendering extension is enabled, and you passed a Vulkan 1.3 features, but without dynamic rendering on");
                    err = LAHAR_ERR_INVALID_CONFIGURATION;
                    goto end;
                }

                lahar->dynamic_rendering = true;
            }
            else if (!dyn_feature) {
                dynamic_rendering_feature.pNext = pnext;
                pnext = (void*)&dynamic_rendering_feature;
                lahar->dynamic_rendering = true;
            }
            else {
                // User supplied the struct themselves, respect their bit
                lahar->dynamic_rendering = dyn_feature->dynamicRendering == VK_TRUE;
            }
        }
        else if (is_1_3) {
            // Core in 1.3, but the feature still has to be switched on. We do not
            // inject it here: the user did not ask for the extension, so opting
            // them into the feature would be a surprise.
            if (vk13_feature) {
                lahar->dynamic_rendering = vk13_feature->dynamicRendering == VK_TRUE;
            }
            else if (dyn_feature) {
                lahar->dynamic_rendering = dyn_feature->dynamicRendering == VK_TRUE;
            }
        }

        if (lahar->dynamic_rendering) {
            lahar_info("Dynamic rendering is enabled");
        }
        else {
            lahar_info("Dynamic rendering is NOT enabled, render passes are required");
        }
    }

#if defined(VK_EXT_swapchain_maintenance1)
    /* Same resolution as dynamic rendering: extension requested + feature
     * struct chained (injected if the user didn't). Never core, so no
     * version path. */
    {
        const VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT* maint_feature =
        (const VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT*)__lahar_pnext_fetch(pnext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT);

        bool has_ext = lahar_extension_has_device(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
        #if defined(VK_KHR_swapchain_maintenance1)
        has_ext = has_ext || lahar_extension_has_device(VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
        #endif

        if (has_ext) {
            if (!maint_feature) {
                swap_maint1_feature.pNext = pnext;
                pnext = (void*)&swap_maint1_feature;
                lahar->swapchain_maintenance1 = true;
            }
            else {
                // User supplied the struct themselves, respect their bit
                lahar->swapchain_maintenance1 = maint_feature->swapchainMaintenance1 == VK_TRUE;
            }
        }

        if (lahar->swapchain_maintenance1) {
            lahar_info("Swapchain maintenance1 is enabled, retired swapchains reclaim via present fences");
        }
    }
#endif

    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pNext = pnext;
    device_create_info.queueCreateInfoCount = queue_create_count;
    device_create_info.pQueueCreateInfos = queue_create_infos;
    device_create_info.enabledLayerCount = enabled_layer_count;
    device_create_info.ppEnabledLayerNames = &dbg_layer_name;
    device_create_info.enabledExtensionCount = 1;
    device_create_info.ppEnabledExtensionNames = &swap_ext_name;
    device_create_info.pEnabledFeatures = &device_features;

    if ((lahar->vkresult = vkCreateDevice(lahar->physdev_info.physdev, &device_create_info, lahar->vkalloc, &lahar->device)) != VK_SUCCESS) {
        goto end;
    }

    if ((err = lahar_load_device(__lahar_loader_dev))) {
        goto end;
    }

    vkGetDeviceQueue(lahar->device, lahar->physdev_info.graphics_queue_index, 0, &lahar->graphicsQueue);
    vkGetDeviceQueue(lahar->device, lahar->physdev_info.present_queue_index, 0, &lahar->presentQueue);

end:
    lahar_temp_mpop();
    return err;
}

uint32_t lahar_build(void) {
    uint32_t err = LAHAR_ERR_SUCCESS;

    if ((err = __lahar_build_inst_extensions())) { goto end; }
    if ((err = __lahar_build_instance())) { goto end; }
    if ((err = __lahar_build_early_surface())) { goto end; }
    if ((err = __lahar_build_physdev())) { goto end; }
    if ((err = __lahar_build_device())) { goto end; }

    #if defined(LAHAR_USE_VMA)
    if ((err = __lahar_init_vma())) {
        goto end;
    }
    #else
    if (!lahar->gpu_allocator) {
        lahar->gpu_allocator_defaulted = true;
        lahar->gpu_allocator = lahar_allocator_freelist_init();
    }
    #endif

    for (uint32_t i = 0; i < lahar->window_count; i++) {
        LaharWindowState* winstate = &lahar->windows[i];
        __lahar_init_window_swapchain(winstate, VK_NULL_HANDLE);
    }
end:
    if (err) {
        lahar_deinit();
    }

    return err;
}

LaharWindowState* lahar_window_state(LaharWindow* window) {
    for (size_t i = 0; i < lahar->window_count; i++) {
        if (lahar->windows[i].window == window) {
            return &lahar->windows[i];
        }
    }

    return NULL;
}

uint32_t lahar_window_swapchain_resize(LaharWindow* window) {
    LaharWindowState* winstate = lahar_window_state(window);

    if (!winstate) { return LAHAR_ERR_INVALID_WINDOW; }

    LaharSurfaceResizeFunc resizer = winstate->resize_callback ? winstate->resize_callback : __lahar_default_resizer;
    return resizer(window);
}

uint32_t lahar_window_frame_begin(LaharWindow* window) {
    if (!window) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    LaharWindowState* winstate = lahar_window_state(window);

    if (!winstate) { return LAHAR_ERR_INVALID_WINDOW; }

    if (winstate->frame_phase != LAHAR_FRAME_PHASE_BEGIN) {
        return LAHAR_ERR_INVALID_FRAME_STATE;
    }

    __lahar_flush_window_retire_queue(false);

    vkWaitForFences(lahar->device, 1, &winstate->in_flight[winstate->flight_index], VK_TRUE, UINT64_MAX);

    VkResult res = vkAcquireNextImageKHR(lahar->device, winstate->swapchain, UINT64_MAX, winstate->image_available[winstate->flight_index], VK_NULL_HANDLE, &winstate->frame_index);

    if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) {
        lahar_trace("Attempted to begin frame, out of date");

        if (winstate->auto_recreate_swap) {

            uint32_t err;
            if ((err = lahar_window_swapchain_resize(window))) {
                return err;
            }

            return lahar_window_frame_begin(window);
        }
        else {
            return LAHAR_ERR_SWAPCHAIN_OUT_OF_DATE;
        }
    }
    else if (res != VK_SUCCESS) {
        lahar->vkresult = res;
        return LAHAR_ERR_VK_ERR;
    }

    //lahar_trace("Frame began\n\tFlight index: %lu\n\tSwap frame index: %lu", winstate->flight_index, winstate->frame_index);

    // Note: the in_flight fence is NOT reset here. It stays signaled until
    // submit, so a frame abandoned between begin and submit (e.g. the window
    // retires on OUT_OF_DATE) never leaves a forever-unsignaled fence in the
    // retire queue. Invariant: unsignaled <=> a submit that will signal it is
    // pending. lahar_window_submit_all resets it right before vkQueueSubmit.
    winstate->frame_phase = LAHAR_FRAME_PHASE_DRAW;

    return LAHAR_ERR_SUCCESS;
}

uint32_t lahar_window_submit_all(LaharWindow* window, VkCommandBuffer* cmds, uint32_t cmd_count) {
    if (!window || !cmds || cmd_count == 0) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    LaharWindowState* winstate = lahar_window_state(window);

    if (!winstate) { return LAHAR_ERR_INVALID_WINDOW; }

    if (winstate->frame_phase != LAHAR_FRAME_PHASE_DRAW) {
        return LAHAR_ERR_INVALID_FRAME_STATE;
    }

    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &winstate->image_available[winstate->flight_index],
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = cmd_count,
        .pCommandBuffers = cmds,
        .signalSemaphoreCount= 1,
        .pSignalSemaphores = &winstate->render_finished[winstate->frame_index],
    };

    // Reset as late as possible: only a fence with a signal actually pending
    // may be unsignaled. See the note in lahar_window_frame_begin.
    vkResetFences(lahar->device, 1, &winstate->in_flight[winstate->flight_index]);

    if ((lahar->vkresult = vkQueueSubmit(lahar->graphicsQueue, 1, &submit_info, winstate->in_flight[winstate->flight_index])) != VK_SUCCESS) {
        return LAHAR_ERR_VK_ERR;
    }

    winstate->frame_phase = LAHAR_FRAME_PHASE_PRESENT;

    return LAHAR_ERR_SUCCESS;
}

uint32_t lahar_window_submit(LaharWindow* window, VkCommandBuffer cmd) {
    return lahar_window_submit_all(window, &cmd, 1);
}

uint32_t lahar_window_frame_cancel(LaharWindow* window, VkCommandBuffer cmd) {
    if (!window) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    LaharWindowState* winstate = lahar_window_state(window);

    if (!winstate) { return LAHAR_ERR_INVALID_WINDOW; }

    // Nothing was begun, so there is nothing to retire. Safe to call blind.
    if (winstate->frame_phase == LAHAR_FRAME_PHASE_BEGIN) {
        return LAHAR_ERR_SUCCESS;
    }

    // Already submitted: the only legal move left is present. Cancelling here
    // would strand the render_finished signal from that submit.
    if (winstate->frame_phase != LAHAR_FRAME_PHASE_DRAW) {
        return LAHAR_ERR_INVALID_FRAME_STATE;
    }

    uint32_t err = LAHAR_ERR_SUCCESS;

    VkCommandBuffer target = cmd != VK_NULL_HANDLE ? cmd : lahar_window_command_buffer(window);

    if (target == VK_NULL_HANDLE) { return LAHAR_ERR_NO_COMMAND_BUFFER; }

    VkCommandBufferBeginInfo begin_info = ZINIT;
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkResetCommandBuffer(target, 0);

    if ((lahar->vkresult = vkBeginCommandBuffer(target, &begin_info)) != VK_SUCCESS) {
        return LAHAR_ERR_VK_ERR;
    }

    // An acquired image is not in PRESENT_SRC, and present requires that it is,
    // so the "empty" frame still needs this one barrier to be legal.
    //
    // Only under dynamic rendering, though. With render passes the layout is
    // moved by vkCmdBeginRenderPass per the attachment's initial/final layouts,
    // which happens outside lahar's knowledge, so attachment->layout is not a
    // value we can trust to build a barrier from. Users on that path own the
    // transition themselves and should pass a command buffer that does it.
    if (lahar->dynamic_rendering) {
        for (uint32_t i = 0; i < winstate->attachment_count; i++) {
            if (winstate->attachment_configs[i].role != LAHAR_ATTROLE_COLOR) { continue; }

            if ((err = lahar_cmd_attachment_transition(target, window, i, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR))) {
                vkEndCommandBuffer(target);
                return err;
            }
        }
    }

    if ((lahar->vkresult = vkEndCommandBuffer(target)) != VK_SUCCESS) {
        return LAHAR_ERR_VK_ERR;
    }

    // Consumes image_available and signals in_flight + render_finished, which is
    // the whole point: those are what a dropped frame would have stranded.
    if ((err = lahar_window_submit(window, target))) { return err; }

    return lahar_window_present(window);
}


uint32_t lahar_window_present(LaharWindow* window) {
    LaharWindowState* winstate = lahar_window_state(window);

    if (!winstate) { return LAHAR_ERR_INVALID_WINDOW; }

    VkPresentInfoKHR present_info = ZINIT;

    if (winstate->frame_phase == LAHAR_FRAME_PHASE_BEGIN) {
        return LAHAR_ERR_INVALID_FRAME_STATE;
    }
    else if (winstate->frame_phase == LAHAR_FRAME_PHASE_DRAW) { // nothing has been rendered to this frame
        return LAHAR_ERR_NO_COMMAND_BUFFER; // TODO: look into submitting an empty command buffer
    }
    else if (winstate->frame_phase == LAHAR_FRAME_PHASE_PRESENT) { // A render command buffer has been submitted
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &winstate->render_finished[winstate->frame_index];
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &winstate->swapchain;
        present_info.pImageIndices = &winstate->frame_index;
    }

#if defined(VK_EXT_swapchain_maintenance1)
    VkSwapchainPresentFenceInfoEXT present_fence_info = ZINIT;

    if (lahar->swapchain_maintenance1) {
        VkFence* fence = &winstate->present_fences[winstate->frame_index];

        // The spec requires the fence be unsignaled at present, and waiting
        // here also proves this image's previous present has retired before
        // we re-present it. Steady state it signaled long ago; this is free.
        vkWaitForFences(lahar->device, 1, fence, VK_TRUE, UINT64_MAX);

        // Same invariant as in_flight: reset only immediately before the
        // operation that signals it, so unsignaled <=> a signal is pending.
        vkResetFences(lahar->device, 1, fence);

        present_fence_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT;
        present_fence_info.swapchainCount = 1;
        present_fence_info.pFences = fence;

        present_info.pNext = &present_fence_info;
    }
#endif

    lahar->vkresult = vkQueuePresentKHR(lahar->presentQueue, &present_info);

    // The frame is over either way: the command buffers were already submitted,
    // so the phase has to advance even on failure or the window wedges in
    // PRESENT forever and every later frame_begin returns INVALID_FRAME_STATE.
    winstate->flight_index = (winstate->flight_index + 1) % winstate->max_in_flight;
    winstate->frame_phase = LAHAR_FRAME_PHASE_BEGIN;

    // SUBOPTIMAL means the image *was* presented, just not ideally for the
    // surface any more. OUT_OF_DATE means it wasn't.
    if (lahar->vkresult == VK_SUBOPTIMAL_KHR || lahar->vkresult == VK_ERROR_OUT_OF_DATE_KHR) {
        lahar_trace("Presented, swapchain out of date");

        if (winstate->auto_recreate_swap) {
            return lahar_window_swapchain_resize(window);
        }

        return LAHAR_ERR_SWAPCHAIN_OUT_OF_DATE;
    }

    if (lahar->vkresult != VK_SUCCESS) {
        return LAHAR_ERR_VK_ERR;
    }

    return LAHAR_ERR_SUCCESS;
}

VkCommandBuffer lahar_window_command_buffer(LaharWindow* window) {
    if (!window) { return VK_NULL_HANDLE; }

    const LaharWindowState* state = lahar_window_state(window);
    if (!state) { return VK_NULL_HANDLE; }

    if (state->frame_phase != LAHAR_FRAME_PHASE_DRAW) { return VK_NULL_HANDLE; }

    // NULL unless lahar_builder_request_command_buffers() was called
    if (!state->commands) { return VK_NULL_HANDLE; }

    return state->commands[state->frame_index];
}



static VkAccessFlags __lahar_access_mask(VkImageLayout layout) {
    switch (layout) {
        case VK_IMAGE_LAYOUT_UNDEFINED:
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            return 0; // or VK_ACCESS_MEMORY_READ_BIT on some drivers
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_ACCESS_SHADER_READ_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return VK_ACCESS_TRANSFER_READ_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return VK_ACCESS_TRANSFER_WRITE_BIT;
        default:
            return 0;
    }
}

static VkPipelineStageFlags __lahar_pipeline_stage(VkImageLayout layout) {
    switch (layout) {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; // or whatever shader stage
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
        default:
            return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }
}

VkImageAspectFlags __lahar_aspect_mask_from_usage(VkImageUsageFlags usage, VkFormat format) {
    if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
        switch (format) {
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            case VK_FORMAT_D16_UNORM:
            case VK_FORMAT_D32_SFLOAT:
                return VK_IMAGE_ASPECT_DEPTH_BIT;
            case VK_FORMAT_S8_UINT:
                return VK_IMAGE_ASPECT_STENCIL_BIT;
            default:
                return VK_IMAGE_ASPECT_DEPTH_BIT;
        }
    }

    return VK_IMAGE_ASPECT_COLOR_BIT;
}

uint32_t lahar_cmd_attachment_transition(VkCommandBuffer cmd, LaharWindow* window, uint32_t attachment_index, VkImageLayout layout) {
    if (!window || cmd == VK_NULL_HANDLE) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    LaharWindowState* winstate = lahar_window_state(window);

    if (!winstate) { return LAHAR_ERR_INVALID_WINDOW; }
    if (attachment_index >= winstate->attachment_count) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    LaharAttachment* attachment = &winstate->attachments[attachment_index][winstate->frame_index];
    LaharAttachmentConfig* conf = &winstate->attachment_configs[attachment_index];

    if (attachment->layout != layout) {
        VkImageMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = __lahar_access_mask(attachment->layout),
            .dstAccessMask = __lahar_access_mask(layout),
            .oldLayout = attachment->layout,
            .newLayout = layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = attachment->image,
            .subresourceRange = {
                .aspectMask = __lahar_aspect_mask_from_usage(conf->usage, winstate->surface_format.format),
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        VkPipelineStageFlags srcStage = __lahar_pipeline_stage(attachment->layout);
        VkPipelineStageFlags dstStage = __lahar_pipeline_stage(layout);

        vkCmdPipelineBarrier(cmd,
            srcStage, dstStage,
            0, // dependency flags
            0, NULL, // memory barriers
            0, NULL, // buffer barriers
            1, &barrier // image barriers
        );

        attachment->layout = layout;
    }

    return LAHAR_ERR_SUCCESS;
}

uint32_t lahar_window_wait_inactive(LaharWindow* window) {
    LaharWindowState* winstate = lahar_window_state(window);
    if (!winstate) { return LAHAR_ERR_INVALID_WINDOW; }

    if ((lahar->vkresult = vkWaitForFences(lahar->device, winstate->max_in_flight, winstate->in_flight, VK_TRUE, UINT64_MAX)) != VK_SUCCESS) {
        return LAHAR_ERR_VK_ERR;
    }

    return LAHAR_ERR_SUCCESS;
}

uint32_t lahar_cmd_begin_rendering(VkCommandBuffer cmd, LaharWindow* window) {
    if (!lahar->dynamic_rendering) {
        return LAHAR_ERR_INVALID_CONFIGURATION;
    }

    lahar_temp_mcheck();

    uint32_t err = LAHAR_ERR_SUCCESS;

    LaharWindowState* state = lahar_window_state(window);
    uint32_t color_count = 0;

    VkRenderingInfo rendering_info = ZINIT;

    VkRenderingAttachmentInfo* color_attachments = NULL;

    LaharAttachmentConfig* depth_config = NULL;
    LaharAttachmentConfig* stencil_config = NULL;

    VkRenderingAttachmentInfo depth_attachment = ZINIT;
    VkRenderingAttachmentInfo stencil_attachment = ZINIT;

    VkRenderingAttachmentInfo* depth_attachment_ptr = NULL;
    VkRenderingAttachmentInfo* stencil_attachment_ptr = NULL;

    uint32_t frame_index = 0;

    if (!state) {
        err = LAHAR_ERR_INVALID_WINDOW;
        goto error;
    }

    if (state->frame_phase != LAHAR_FRAME_PHASE_DRAW) {
        err = LAHAR_ERR_INVALID_FRAME_STATE;
        goto error;
    }

    frame_index = state->frame_index;

    for (uint32_t i = 0; i < state->attachment_count; i++) {
        if (
            state->attachment_configs[i].role == LAHAR_ATTROLE_COLOR ||
            state->attachment_configs[i].role == LAHAR_ATTROLE_USER
        ) {
            color_count++;
        }
    }

    color_attachments = (VkRenderingAttachmentInfo*)lahar_temp_alloc(color_count * sizeof(*color_attachments));
    color_count = 0;


    for (uint32_t i = 0; i < state->attachment_count; i++) {
        if (
            state->attachment_configs[i].role == LAHAR_ATTROLE_COLOR ||
            state->attachment_configs[i].role == LAHAR_ATTROLE_USER
        ) {
            VkRenderingAttachmentInfo* att = &color_attachments[color_count++];

            att->sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            att->imageView = state->attachments[i][frame_index].view;
            att->imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            att->resolveMode = VK_RESOLVE_MODE_NONE; // TODO: consider this
            att->resolveImageView = VK_NULL_HANDLE; // TODO: consider this
            att->resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED; // TODO: consider this
            att->loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            att->storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            att->clearValue = state->attachment_configs[i].clear_value;
        }
    }

    depth_config = lahar_window_attachment_config_depth(window);
    stencil_config = lahar_window_attachment_config_stencil(window);

    if (depth_config) {
        const LaharAttachment* windepth = lahar_window_attachment_depth(window, frame_index);
        depth_attachment_ptr = &depth_attachment;

        depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depth_attachment.imageView = windepth->view;
        depth_attachment.imageLayout = depth_config == stencil_config ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depth_attachment.resolveMode = VK_RESOLVE_MODE_NONE;
        depth_attachment.resolveImageView = VK_NULL_HANDLE;
        depth_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth_attachment.clearValue = depth_config->clear_value;
    }

    if (stencil_config == depth_config) {
        stencil_attachment_ptr = depth_attachment_ptr;
    }
    else if (stencil_config) {
        const LaharAttachment* winstencil = lahar_window_attachment_stencil(window, frame_index);
        stencil_attachment_ptr = &stencil_attachment;

        stencil_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        stencil_attachment.imageView = winstencil->view;
        stencil_attachment.imageLayout = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
        stencil_attachment.resolveMode = VK_RESOLVE_MODE_NONE;
        stencil_attachment.resolveImageView = VK_NULL_HANDLE;
        stencil_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        stencil_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        stencil_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        stencil_attachment.clearValue = stencil_config->clear_value;
    }

    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.pNext = NULL;
    rendering_info.flags = 0;
    rendering_info.renderArea.offset.x = 0;
    rendering_info.renderArea.offset.y = 0;
    rendering_info.renderArea.extent.width = state->width;
    rendering_info.renderArea.extent.height = state->height;
    rendering_info.layerCount = 1;
    rendering_info.viewMask = 0;
    rendering_info.colorAttachmentCount = color_count;
    rendering_info.pColorAttachments = color_attachments;
    rendering_info.pDepthAttachment = depth_attachment_ptr;
    rendering_info.pStencilAttachment = stencil_attachment_ptr;

    vkCmdBeginRendering(cmd, &rendering_info);
error:
    lahar_temp_mpop();
    return err;
}


LaharAttachmentConfig* lahar_window_attachment_config_index(LaharWindow* window, uint32_t index) {
    LaharWindowState* state = lahar_window_state(window);
    if (!state) { return NULL; }
    if (index >= state->attachment_count) { return NULL; }

    return &state->attachment_configs[index];
}

LaharAttachmentConfig* lahar_window_attachment_config_color(LaharWindow* window) {
    LaharWindowState* state = lahar_window_state(window);
    if (!state) { return NULL; }

    for (uint32_t i = 0; i < state->attachment_count; i++) {
        if (state->attachment_configs[i].role == LAHAR_ATTROLE_COLOR) {
            return &state->attachment_configs[i];
        }
    }

    return NULL;
}

LaharAttachmentConfig* lahar_window_attachment_config_depth(LaharWindow* window) {
    LaharWindowState* state = lahar_window_state(window);
    if (!state) { return NULL; }

    for (uint32_t i = 0; i < state->attachment_count; i++) {
        if (
            state->attachment_configs[i].role == LAHAR_ATTROLE_DEPTH ||
            state->attachment_configs[i].role == LAHAR_ATTROLE_DEPTH_STENCIL
        ) {
            return &state->attachment_configs[i];
        }
    }

    return NULL;
}

LaharAttachmentConfig* lahar_window_attachment_config_stencil(LaharWindow* window) {
    LaharWindowState* state = lahar_window_state(window);
    if (!state) { return NULL; }

    for (uint32_t i = 0; i < state->attachment_count; i++) {
        if (
            state->attachment_configs[i].role == LAHAR_ATTROLE_STENCIL ||
            state->attachment_configs[i].role == LAHAR_ATTROLE_DEPTH_STENCIL
        ) {
            return &state->attachment_configs[i];
        }
    }

    return NULL;
}

LaharAttachment* lahar_window_attachment_index(LaharWindow* window, uint32_t index, uint32_t frameno) {
    LaharWindowState* state = lahar_window_state(window);
    if (!state) { return NULL; }
    if (index >= state->attachment_count) { return NULL; }
    if (frameno >= state->swap_size) { return NULL; }

    return &state->attachments[index][frameno];
}

LaharAttachment* lahar_window_attachment_color(LaharWindow* window, uint32_t frameno) {
    LaharWindowState* state = lahar_window_state(window);
    if (!state) { return NULL; }

    for (uint32_t i = 0; i < state->attachment_count; i++) {
        if (state->attachment_configs[i].role == LAHAR_ATTROLE_COLOR) {
            return &state->attachments[i][frameno];
        }
    }

    return NULL;
}

LaharAttachment* lahar_window_attachment_depth(LaharWindow* window, uint32_t frameno) {
    LaharWindowState* state = lahar_window_state(window);
    if (!state) { return NULL; }

    for (uint32_t i = 0; i < state->attachment_count; i++) {
        if (
            state->attachment_configs[i].role == LAHAR_ATTROLE_DEPTH ||
            state->attachment_configs[i].role == LAHAR_ATTROLE_DEPTH_STENCIL
        ) {
            return &state->attachments[i][frameno];
        }
    }

    return NULL;
}

LaharAttachment* lahar_window_attachment_stencil(LaharWindow* window, uint32_t frameno) {
    LaharWindowState* state = lahar_window_state(window);
    if (!state) { return NULL; }

    for (uint32_t i = 0; i < state->attachment_count; i++) {
        if (
            state->attachment_configs[i].role == LAHAR_ATTROLE_STENCIL ||
            state->attachment_configs[i].role == LAHAR_ATTROLE_DEPTH_STENCIL
        ) {
            return &state->attachments[i][frameno];
        }
    }

    return NULL;
}

uint32_t lahar_shader_register_compiler(
    const char* language,
    void* user_data,
    LaharShaderCompileFunc compiler_func,
    LaharShaderCompileReleaseFunc release_func
) {
    if (!language || !*language || !compiler_func || !release_func) {
        return LAHAR_ERR_ILLEGAL_PARAMS;
    }

    for (uint32_t i = 0; i < LAHAR_MAX_SHADER_COMPILERS; i++) {
        if (!lahar->shader_compilers[i].compile) {
            lahar->shader_compilers[i].language = lahar_strdup(language);
            lahar->shader_compilers[i].user_data = user_data;
            lahar->shader_compilers[i].compile = compiler_func;
            lahar->shader_compilers[i].release = release_func;

            return LAHAR_ERR_SUCCESS;
        }
    }

    return LAHAR_ERR_OUT_OF_SPACE;
}



uint32_t lahar_shader_var_type_to_input_type(LaharShaderVarType svt, VkFormat* format_out) {
    if (!format_out) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    switch (svt) {
        case LAHAR_SVT_BOOL:   *format_out = VK_FORMAT_R32_UINT;              break;
        case LAHAR_SVT_INT:    *format_out = VK_FORMAT_R32_SINT;              break;
        case LAHAR_SVT_UINT:   *format_out = VK_FORMAT_R32_UINT;              break;
        case LAHAR_SVT_HALF:   *format_out = VK_FORMAT_R16_SFLOAT;            break;
        case LAHAR_SVT_FLOAT:  *format_out = VK_FORMAT_R32_SFLOAT;            break;
        case LAHAR_SVT_DOUBLE: *format_out = VK_FORMAT_R64_SFLOAT;            break;
        case LAHAR_SVT_BVEC2:  *format_out = VK_FORMAT_R32G32_UINT;           break;
        case LAHAR_SVT_BVEC3:  *format_out = VK_FORMAT_R32G32B32_UINT;        break;
        case LAHAR_SVT_BVEC4:  *format_out = VK_FORMAT_R32G32B32A32_UINT;     break;
        case LAHAR_SVT_IVEC2:  *format_out = VK_FORMAT_R32G32_SINT;           break;
        case LAHAR_SVT_IVEC3:  *format_out = VK_FORMAT_R32G32B32_SINT;        break;
        case LAHAR_SVT_IVEC4:  *format_out = VK_FORMAT_R32G32B32A32_SINT;     break;
        case LAHAR_SVT_UVEC2:  *format_out = VK_FORMAT_R32G32_UINT;           break;
        case LAHAR_SVT_UVEC3:  *format_out = VK_FORMAT_R32G32B32_UINT;        break;
        case LAHAR_SVT_UVEC4:  *format_out = VK_FORMAT_R32G32B32A32_UINT;     break;
        case LAHAR_SVT_VEC2:   *format_out = VK_FORMAT_R32G32_SFLOAT;         break;
        case LAHAR_SVT_VEC3:   *format_out = VK_FORMAT_R32G32B32_SFLOAT;      break;
        case LAHAR_SVT_VEC4:   *format_out = VK_FORMAT_R32G32B32A32_SFLOAT;   break;
        case LAHAR_SVT_DVEC2:  *format_out = VK_FORMAT_R64G64_SFLOAT;         break;
        case LAHAR_SVT_DVEC3:  *format_out = VK_FORMAT_R64G64B64_SFLOAT;      break;
        case LAHAR_SVT_DVEC4:  *format_out = VK_FORMAT_R64G64B64A64_SFLOAT;   break;
        default:               return LAHAR_ERR_INVALID_TYPE;
    }

    return LAHAR_ERR_SUCCESS;
}

static uint32_t __lahar_binding_info(const LaharShaderVarInfo* shader_vars, const LaharShaderVarInfo* var, VkDescriptorSetLayoutBinding* binding) {
    uint32_t err = LAHAR_ERR_SUCCESS;

    switch (var->storage_class) {
        case LAHAR_SVSC_UNIFORM_BUFFER: {
            binding->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            binding->stageFlags = var->stages;
            binding->descriptorCount = 1;
        } break;
        case LAHAR_SVSC_STORAGE_BUFFER: {
            binding->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            binding->stageFlags = var->stages;
            binding->descriptorCount = 1;
        } break;
        case LAHAR_SVSC_UNIFORM_CONSTANT: {
            binding->stageFlags = var->stages;

            switch (var->type) {
                case LAHAR_SVT_ARRAY: {
                    LAHAR_ASSERT(var->child_vars_begin != UINT32_MAX);
                    const LaharShaderVarInfo* child_var = &shader_vars[var->child_vars_begin];

                    if ((err = __lahar_binding_info(shader_vars, child_var, binding))) {
                        return err;
                    }

                    binding->descriptorCount = var->array_size;
                } break;
                case LAHAR_SVT_IMAGE: {
                    binding->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                } break;
                case LAHAR_SVT_SAMPLER: {
                    binding->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
                } break;
                case LAHAR_SVT_SAMPLER_IMAGE: {
                    binding->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                } break;
                default:
                    lahar_error("Illegal type for uniform constant");
                    return LAHAR_ERR_MALFORMED_CODE;
            }
        } break;
        default:
            lahar_error("Illegal storage class for binding");
            return LAHAR_ERR_MALFORMED_CODE;
    }

    return err;
}

static uint32_t __lahar_shader_set_binding_infos(
    uint32_t shader_var_count,
    LaharShaderVarInfo* shader_vars,
    uint32_t set_number,
    uint32_t* binding_count_out,
    VkDescriptorSetLayoutBinding* bindings_out
) {
    if (!shader_vars || !binding_count_out) {
        return LAHAR_ERR_ILLEGAL_PARAMS;
    }

    uint32_t found_binding_count = 0;

    for (uint32_t i = 0; i < shader_var_count; i++) {
        const LaharShaderVarInfo* var = &shader_vars[i];

        const bool is_uniform =
            var->storage_class == LAHAR_SVSC_UNIFORM_BUFFER ||
            var->storage_class == LAHAR_SVSC_UNIFORM_CONSTANT ||
            var->storage_class == LAHAR_SVSC_STORAGE_BUFFER;

        const bool right_set = var->set == set_number;
        const bool top_level = var->parent_var == UINT32_MAX;

        if (is_uniform && right_set && top_level) {
            found_binding_count++;
        }
    }

    if (!bindings_out) {
        *binding_count_out = found_binding_count;
        return LAHAR_ERR_SUCCESS;
    }

    for (uint32_t binding_number = 0; binding_number < found_binding_count; binding_number++) {
        const LaharShaderVarInfo* binding_var = NULL;

        for (uint32_t i = 0; i < shader_var_count; i++) {
            const LaharShaderVarInfo* var = &shader_vars[i];

            const bool is_uniform =
                var->storage_class == LAHAR_SVSC_UNIFORM_BUFFER ||
                var->storage_class == LAHAR_SVSC_UNIFORM_CONSTANT ||
                var->storage_class == LAHAR_SVSC_STORAGE_BUFFER;

            const bool right_set = var->set == set_number;
            const bool top_level = var->parent_var == UINT32_MAX;

            if (is_uniform && right_set && top_level && var->binding == binding_number) {
                binding_var = var;
                break;
            }
        }

        memset(&bindings_out[binding_number], 0, sizeof(*bindings_out));

        bindings_out[binding_number].binding = binding_number;

        if (binding_var) {
            __lahar_binding_info(shader_vars, binding_var, &bindings_out[binding_number]);
        }
    }

    *binding_count_out = found_binding_count;

    return LAHAR_ERR_SUCCESS;
}

/* Util: the highest set number used by any top level descriptor, or UINT32_MAX if the shader has no descriptors at all. */
static uint32_t __lahar_shader_max_set_number(uint32_t shader_var_count, const LaharShaderVarInfo* shader_vars) {
    uint32_t max_set_number = UINT32_MAX;

    for (uint32_t i = 0; i < shader_var_count; i++) {
        const LaharShaderVarInfo* var = &shader_vars[i];

        const bool is_uniform =
            var->storage_class == LAHAR_SVSC_UNIFORM_BUFFER ||
            var->storage_class == LAHAR_SVSC_UNIFORM_CONSTANT ||
            var->storage_class == LAHAR_SVSC_STORAGE_BUFFER;

        if (is_uniform && var->parent_var == UINT32_MAX) {
            if (max_set_number == UINT32_MAX) {
                max_set_number = var->set;
            }
            else {
                max_set_number = var->set > max_set_number ? var->set : max_set_number;
            }
        }
    }

    return max_set_number;
}

/* Util: find the push constant range implied by the vars, if any */
static bool __lahar_shader_push_range(uint32_t shader_var_count, const LaharShaderVarInfo* shader_vars, VkPushConstantRange* range) {
    for (uint32_t i = 0; i < shader_var_count; i++) {
        const LaharShaderVarInfo* var = &shader_vars[i];

        if (var->storage_class == LAHAR_SVSC_PUSH_CONSTANT && var->parent_var == UINT32_MAX) {
            range->offset = var->offset;
            range->size = var->size;
            range->stageFlags = var->stages;
            return true;
        }
    }

    return false;
}

uint32_t lahar_shader_reflect_set_layouts(
    const LaharShaderVarInfo* vars,
    uint32_t var_count,
    uint32_t* set_count,
    VkDescriptorSetLayout* layouts_out
) {
    if (!vars || !set_count) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    const uint32_t max_set_number = __lahar_shader_max_set_number(var_count, vars);
    const uint32_t total_sets = max_set_number == UINT32_MAX ? 0 : max_set_number + 1;

    if (!layouts_out) {
        *set_count = total_sets;
        return LAHAR_ERR_SUCCESS;
    }

    // Guard against the caller having grown the shader between the two passes
    if (*set_count < total_sets) {
        lahar_error(
            "Set layout array holds %" PRIu32 " sets, but the shaders need %" PRIu32,
            *set_count, total_sets
        );
        return LAHAR_ERR_OUT_OF_SPACE;
    }

    lahar_temp_mcheck();

    uint32_t err = LAHAR_ERR_SUCCESS;
    uint32_t created = 0;

    for (; created < total_sets; created++) {
        uint32_t binding_count = 0;
        VkDescriptorSetLayoutBinding* set_bindings = NULL;

        if ((err = __lahar_shader_set_binding_infos(var_count, (LaharShaderVarInfo*)vars, created, &binding_count, NULL))) {
            goto error;
        }

        set_bindings = (VkDescriptorSetLayoutBinding*)lahar_temp_alloc(binding_count * sizeof(*set_bindings));

        if ((err = __lahar_shader_set_binding_infos(var_count, (LaharShaderVarInfo*)vars, created, &binding_count, set_bindings))) {
            goto error;
        }

        VkDescriptorSetLayoutCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = binding_count,
            .pBindings = set_bindings
        };

        if ((lahar->vkresult = vkCreateDescriptorSetLayout(lahar->device, &create_info, lahar->vkalloc, &layouts_out[created])) != VK_SUCCESS) {
            err = LAHAR_ERR_VK_ERR;
            goto error;
        }
    }

    *set_count = total_sets;

    lahar_temp_mpop();
    return LAHAR_ERR_SUCCESS;

error:
    // Nothing is handed back on failure, so don't leak the partial run
    for (uint32_t i = 0; i < created; i++) {
        vkDestroyDescriptorSetLayout(lahar->device, layouts_out[i], lahar->vkalloc);
        layouts_out[i] = VK_NULL_HANDLE;
    }

    *set_count = 0;

    lahar_temp_mpop();
    return err;
}

static uint32_t __lahar_shader_create_layout(
    uint32_t shader_var_count,
    LaharShaderVarInfo* shader_vars,
    const VkDescriptorSetLayout* provided_sets,
    uint32_t provided_set_count,
    VkPipelineLayout* created
) {
    lahar_temp_mcheck();

    uint32_t err = LAHAR_ERR_SUCCESS;
    uint32_t total_sets = 0;
    VkDescriptorSetLayout* set_layouts = NULL;
    bool owns_set_layouts = false;
    VkPushConstantRange push_constant = ZINIT;
    bool has_push_constant = false;

    VkPipelineLayoutCreateInfo layout_info = ZINIT;

    has_push_constant = __lahar_shader_push_range(shader_var_count, shader_vars, &push_constant);

    if (provided_sets) {
        // Caller owns these; we only borrow them for the create call
        set_layouts = (VkDescriptorSetLayout*)provided_sets;
        total_sets = provided_set_count;
    }
    else {
        if ((err = lahar_shader_reflect_set_layouts(shader_vars, shader_var_count, &total_sets, NULL))) {
            goto end;
        }

        if (total_sets) {
            set_layouts = (VkDescriptorSetLayout*)lahar_temp_alloc(total_sets * sizeof(*set_layouts));

            if ((err = lahar_shader_reflect_set_layouts(shader_vars, shader_var_count, &total_sets, set_layouts))) {
                goto end;
            }

            // A VkPipelineLayout does not retain its set layouts, so these are
            // safe to destroy as soon as it is created. The caller never sees
            // them, so they cannot allocate descriptor sets from them -- that
            // is what set_descriptor_set_layouts() is for.
            owns_set_layouts = true;
        }
    }

    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = total_sets;
    layout_info.pSetLayouts = set_layouts;
    layout_info.pPushConstantRanges = &push_constant;
    layout_info.pushConstantRangeCount = has_push_constant ? 1 : 0;

    if ((lahar->vkresult = vkCreatePipelineLayout(lahar->device, &layout_info, lahar->vkalloc, created)) != VK_SUCCESS) {
        err = LAHAR_ERR_VK_ERR;
        goto end;
    }

end:
    if (owns_set_layouts) {
        for (uint32_t i = 0; i < total_sets; i++) {
            vkDestroyDescriptorSetLayout(lahar->device, set_layouts[i], lahar->vkalloc);
        }
    }

    lahar_temp_mpop();
    return err;
}

static LaharShaderCompiler* __lahar_shader_compiler_for(const char* language) {
    for (uint32_t i = 0; i < LAHAR_MAX_SHADER_COMPILERS; i++) {
        if (strcmp(language, lahar->shader_compilers[i].language) == 0) {
            return &lahar->shader_compilers[i];
        }
    }

    return NULL;
}

uint32_t lahar_shader_build(LaharShaderBuilder* builder, VkPipeline* pipeline, VkPipelineLayout* layout) {
    if (!builder || !pipeline) {
        return LAHAR_ERR_ILLEGAL_PARAMS;
    }

    if (!builder->stage_count) {
        return LAHAR_ERR_INVALID_CONFIGURATION;
    }

    lahar_temp_mcheck();

    // Base vars
    uint32_t err = LAHAR_ERR_SUCCESS;
    VkResult result = VK_SUCCESS;
    bool layout_created = false;

    // Module/stage construction vars
    LaharShaderStage final_stages[LAHAR_MAX_SHADER_STAGES] = ZINIT;
    const LaharShaderCompiler* stage_compilers[LAHAR_MAX_SHADER_STAGES] = ZINIT;

    VkShaderModuleCreateInfo module_infos[LAHAR_MAX_SHADER_STAGES] = ZINIT;
    VkShaderModule modules[LAHAR_MAX_SHADER_STAGES] = ZINIT;
    VkPipelineShaderStageCreateInfo stage_infos[LAHAR_MAX_SHADER_STAGES] = ZINIT;
    uint32_t stage_index = 0;

    /* How many entries step 1 actually produced in final_stages, and therefore how
     * many may need compiler->release() during cleanup.
     *
     * Kept separate from stage_index on purpose. Cleanup used to reuse
     * stage_index, but that counter is reset to 0 and re-driven by the module
     * loop, so after a mid-loop failure in step 2 it holds the index of the
     * FAILING module rather than the compiled count. Every stage past the failure
     * then never got released, leaking whatever a custom compiler allocated.
     *
     * Declared up here with the other locals so that the goto error paths inside
     * step 1 cannot jump over its initialization. */
    uint32_t compiled_count = 0;

    // Dynamic state vars
    uint32_t dynamic_state_count = 0;
    VkDynamicState dynamic_states[64] = ZINIT;

    const size_t max_states = sizeof(dynamic_states) / sizeof(dynamic_states[0]);

    const bool is_vulkan_1_3 = VK_API_VERSION_MINOR(lahar->vkversion) >= 3;
    const bool has_dynamic_1 = lahar_extension_has_device(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
    const bool has_dynamic_2 = lahar_extension_has_device(VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME);
    const bool has_dynamic_3 = lahar_extension_has_device(VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME);

    // Pipeline construction vars
    VkPipelineDynamicStateCreateInfo dynamic_state_info = ZINIT;
    VkPipelineVertexInputStateCreateInfo vertex_input_info = ZINIT;
    VkPipelineInputAssemblyStateCreateInfo input_assembly_info = ZINIT;
    VkPipelineViewportStateCreateInfo viewport_info = ZINIT;
    VkPipelineRasterizationStateCreateInfo rasterizer_info = ZINIT;
    VkPipelineMultisampleStateCreateInfo multisampling_info = ZINIT;
    VkPipelineColorBlendStateCreateInfo blend_state = ZINIT;
    VkPipelineRenderingCreateInfo rendering_info = ZINIT;
    VkPipelineDepthStencilStateCreateInfo depth_stencil_info = ZINIT;
    VkPipelineLayout chosen_layout = VK_NULL_HANDLE;

    const bool wants_dynamic_rendering = lahar->dynamic_rendering || builder->force_dynamic;

    void* final_pnext_chain = builder->pNext;

    VkGraphicsPipelineCreateInfo pipeline_info = ZINIT;
    VkPipeline created_pipeline = VK_NULL_HANDLE;

    // Introspection vars
    uint32_t shader_var_count = 0;
    LaharShaderVarInfo* shader_vars = NULL;

    // Step 1: compile any non-spv stages
    while (stage_index < LAHAR_MAX_SHADER_STAGES && builder->stages[stage_index].code) {
        const LaharShaderStage* stage = &builder->stages[stage_index];
        LaharShaderStage* outstage = &final_stages[stage_index];

        if (
            !stage->lang ||
            strcmp(stage->lang, "spv") == 0 ||
            strcmp(stage->lang, "spirv") == 0 ||
            strcmp(stage->lang, "spir-v") == 0 ||
            strcmp(stage->lang, "SPV") == 0 ||
            strcmp(stage->lang, "SPIRV") == 0 ||
            strcmp(stage->lang, "SPIR-V") == 0
        ) {
            *outstage = *stage;
        }
        else {
            const LaharShaderCompiler* compiler = __lahar_shader_compiler_for(stage->lang);

            if (!compiler) {
                err = LAHAR_ERR_UNKNOWN_LANGUAGE;
                goto error;
            }

            err = compiler->compile(
                compiler->user_data,
                stage,
                (uint32_t**)&outstage->code,
                &outstage->length
            );

            if (err) { goto error; }

            stage_compilers[stage_index] = compiler;

            outstage->stage = stage->stage;
            outstage->lang = "spv";
            outstage->entrypoint = stage->entrypoint;
        }

        stage_index++;
    }

    // Step 1 is done, so stage_index is now the true count of produced stages
    compiled_count = stage_index;

    stage_index = 0;

    // Step 2: build stage modules
    while (stage_index < LAHAR_MAX_SHADER_STAGES && final_stages[stage_index].code) {
        const LaharShaderStage* stage = &final_stages[stage_index];

        module_infos[stage_index].sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        module_infos[stage_index].codeSize = stage->length;
        module_infos[stage_index].pCode = (const uint32_t*)stage->code;

        result = vkCreateShaderModule(lahar->device, &module_infos[stage_index], lahar->vkalloc, &modules[stage_index]);
        if (result != VK_SUCCESS) { err = LAHAR_ERR_VK_ERR; goto error; }

        stage_infos[stage_index].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_infos[stage_index].stage = stage->stage;
        stage_infos[stage_index].module = modules[stage_index];
        stage_infos[stage_index].pName = stage->entrypoint ? stage->entrypoint : "main";

        stage_index++;
    }

    // Step 3: begin pipeline configuration
    if (builder->all_dynamic) {
        static const VkDynamicState base[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
            VK_DYNAMIC_STATE_LINE_WIDTH,
            VK_DYNAMIC_STATE_DEPTH_BIAS,
            VK_DYNAMIC_STATE_BLEND_CONSTANTS,
            VK_DYNAMIC_STATE_DEPTH_BOUNDS,
            VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
            VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
            VK_DYNAMIC_STATE_STENCIL_REFERENCE,
        };

        memcpy(dynamic_states, base, sizeof(base));
        dynamic_state_count = sizeof(base) / sizeof(base[0]);

        static const VkDynamicState dyn1[] = {
            VK_DYNAMIC_STATE_CULL_MODE_EXT,
            VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE_EXT,
            VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT,
            VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT,
            VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT,
            VK_DYNAMIC_STATE_FRONT_FACE_EXT,
            VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT,
            //VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT_EXT,
            VK_DYNAMIC_STATE_STENCIL_OP_EXT,
            VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT,
            VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT,
            //VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT_EXT,
        };

        static const VkDynamicState dyn2[] = {
            VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE_EXT,
            VK_DYNAMIC_STATE_LOGIC_OP_EXT,
            VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT,
            VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE_EXT,
        };

        static const VkDynamicState dyn3[] = {
            VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT,
            VK_DYNAMIC_STATE_ALPHA_TO_ONE_ENABLE_EXT,
            VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT,
            VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT,
            VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT,
            VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT,
            VK_DYNAMIC_STATE_LOGIC_OP_ENABLE_EXT,
            VK_DYNAMIC_STATE_POLYGON_MODE_EXT,
            VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT,
            VK_DYNAMIC_STATE_SAMPLE_MASK_EXT,
        };

        static const size_t dyn1_ct = sizeof(dyn1) / sizeof(dyn1[0]);
        static const size_t dyn2_ct = sizeof(dyn2) / sizeof(dyn2[0]);
        static const size_t dyn3_ct = sizeof(dyn3) / sizeof(dyn3[0]);

        if (is_vulkan_1_3 || has_dynamic_1) {
            for (size_t i = 0; dynamic_state_count < max_states && i < dyn1_ct; dynamic_state_count++, i++) {
                dynamic_states[dynamic_state_count] = dyn1[i];
            }
        }

        if (has_dynamic_2) {
            for (size_t i = 0; dynamic_state_count < max_states && i < dyn2_ct; dynamic_state_count++, i++) {
                dynamic_states[dynamic_state_count] = dyn2[i];
            }
        }

        if (has_dynamic_3) {
            for (size_t i = 0; dynamic_state_count < max_states && i < dyn3_ct; dynamic_state_count++, i++) {
                dynamic_states[dynamic_state_count] = dyn3[i];
            }
        }
    }
    else {
        if (builder->dynamic_mode == LAHAR_SDF_DEFAULT) {
            builder->dynamic_mode = LAHAR_SDF_BASIC;
        }

        if (builder->dynamic_mode & LAHAR_SDF_BASIC) {
            static const VkDynamicState ds[] = {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR,
            };

            memcpy(&dynamic_states[dynamic_state_count], ds, sizeof(ds));
            dynamic_state_count += sizeof(ds) / sizeof(ds[0]);
        }

        if (builder->dynamic_mode & LAHAR_SDF_DEPTH_BIAS) {
            static const VkDynamicState ds[] = {
                VK_DYNAMIC_STATE_DEPTH_BIAS,
            };

            memcpy(&dynamic_states[dynamic_state_count], ds, sizeof(ds));
            dynamic_state_count += sizeof(ds) / sizeof(ds[0]);
        }

        if (builder->dynamic_mode & LAHAR_SDF_BLEND_CONST) {
            static const VkDynamicState ds[] = {
                VK_DYNAMIC_STATE_BLEND_CONSTANTS,
            };

            memcpy(&dynamic_states[dynamic_state_count], ds, sizeof(ds));
            dynamic_state_count += sizeof(ds) / sizeof(ds[0]);
        }

        if (builder->dynamic_mode & LAHAR_SDF_DEPTH_STENCIL) {
            static const VkDynamicState ds[] = {
                VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
                VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
                VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
                VK_DYNAMIC_STATE_STENCIL_REFERENCE,
                VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
                VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
            };

            memcpy(&dynamic_states[dynamic_state_count], ds, sizeof(ds));
            dynamic_state_count += sizeof(ds) / sizeof(ds[0]);
        }

        if (builder->dynamic_mode & LAHAR_SDF_CULL) {
            if (!is_vulkan_1_3 && !has_dynamic_1) {
                err = LAHAR_ERR_MISSING_EXTENSION;
                goto error;
            }

            static const VkDynamicState ds[] = {
                VK_DYNAMIC_STATE_CULL_MODE_EXT,
                VK_DYNAMIC_STATE_FRONT_FACE_EXT,
            };

            memcpy(&dynamic_states[dynamic_state_count], ds, sizeof(ds));
            dynamic_state_count += sizeof(ds) / sizeof(ds[0]);
        }
    }

    dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_info.dynamicStateCount = dynamic_state_count;
    dynamic_state_info.pDynamicStates = dynamic_states;

    depth_stencil_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_info.depthTestEnable  = (builder->depth_test  != LAHAR_SDTM_OFF);
    depth_stencil_info.depthWriteEnable = (builder->depth_write != LAHAR_SDWM_OFF);
    depth_stencil_info.depthCompareOp   = builder->depth_compare_op == LAHAR_SDCO_DEFAULT ? VK_COMPARE_OP_LESS : (VkCompareOp)(builder->depth_compare_op - 1);
    depth_stencil_info.depthBoundsTestEnable = VK_FALSE;
    depth_stencil_info.stencilTestEnable = VK_FALSE;
    depth_stencil_info.minDepthBounds = 0.0f;
    depth_stencil_info.maxDepthBounds = 1.0f;

    input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_info.primitiveRestartEnable = VK_FALSE;
    if (builder->topology == LAHAR_ST_DEFAULT) {
        input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }
    else {
        int num = (int)builder->topology;
        input_assembly_info.topology = (VkPrimitiveTopology)(num - 1);
    }


    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.viewportCount = 1;
    viewport_info.scissorCount = 1;

    VkCullModeFlags cull_mode;
    VkFrontFace front_face;

    switch (builder->cull_mode) {
        case LAHAR_SCM_DEFAULT:
        case LAHAR_SCM_BACK:
            cull_mode = VK_CULL_MODE_BACK_BIT;
            break;
        case LAHAR_SCM_OFF:
            cull_mode = VK_CULL_MODE_NONE;
            break;
        case LAHAR_SCM_FRONT:
            cull_mode = VK_CULL_MODE_FRONT_BIT;
            break;
        default:
            err = LAHAR_ERR_INVALID_CONFIGURATION;
            goto error;
    }

    switch (builder->face_mode) {
        case LAHAR_SFM_DEFAULT:
        case LAHAR_SFM_FRONT_CCW:
            front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            break;
        case LAHAR_SFM_FRONT_CW:
            front_face = VK_FRONT_FACE_CLOCKWISE;
            break;
        default:
            err = LAHAR_ERR_INVALID_CONFIGURATION;
            goto error;
    }

    rasterizer_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer_info.depthClampEnable = VK_FALSE;
    rasterizer_info.rasterizerDiscardEnable = VK_FALSE;
    rasterizer_info.polygonMode = builder->wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
    rasterizer_info.cullMode = cull_mode;
    rasterizer_info.frontFace = front_face;
    rasterizer_info.lineWidth = 1.0f;

    multisampling_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling_info.sampleShadingEnable = VK_FALSE;

    blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_state.attachmentCount = builder->blend_state_count;
    blend_state.pAttachments = builder->blend_states;

    rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachmentFormats = &builder->surface_color_format;
    rendering_info.depthAttachmentFormat = builder->surface_depth_format;
    rendering_info.stencilAttachmentFormat = builder->surface_stencil_format;

    if (wants_dynamic_rendering && !__lahar_pnext_fetch(final_pnext_chain, VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO)) {
        rendering_info.pNext = final_pnext_chain;
        final_pnext_chain = &rendering_info;
    }


    // Step 4: Introspect to get required info for layout
    if ((err = lahar_shader_introspect(final_stages, builder->stage_count, &shader_var_count, NULL))) {
        goto error;
    }

    shader_vars = (LaharShaderVarInfo*)lahar_temp_alloc(shader_var_count * sizeof(*shader_vars));

    if ((err = lahar_shader_introspect(final_stages, builder->stage_count, &shader_var_count, shader_vars))) {
        goto error;
    }

    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    vertex_input_info.vertexAttributeDescriptionCount = builder->attrib_count;
    vertex_input_info.pVertexAttributeDescriptions = builder->attrib_descriptions;
    vertex_input_info.vertexBindingDescriptionCount = builder->binding_count;
    vertex_input_info.pVertexBindingDescriptions = builder->binding_descriptions;

    if (!builder->skip_validation) {
        for (uint32_t j = 0; j < shader_var_count; j++) {
            const LaharShaderVarInfo* var = &shader_vars[j];
            if (var->storage_class != LAHAR_SVSC_INPUT) { continue; }

            bool found = false;
            for (uint32_t i = 0; i < builder->attrib_count; i++) {
                if (builder->attrib_descriptions[i].location == var->location) {
                    found = true;
                    const VkFormat format = builder->attrib_descriptions[i].format;

                    VkFormat expected;

                    if ((err = lahar_shader_var_type_to_input_type(var->type, &expected))) {
                        lahar_error("Location %" PRIu32 " has type Lahar doesn't support for vertex inputs", var->location);
                        goto error;
                    }

                    if (expected != format) {
                        lahar_error(
                            "Location %" PRIu32 " expected format %s, got %s",
                            var->location,
                            lahar_vkformat_string(expected),
                            lahar_vkformat_string(format)
                        );
                        err = LAHAR_ERR_INVALID_CONFIGURATION;
                        goto error;
                    }

                    break;
                }
            }

            if (!found) {
                lahar_error("Shader consumes input location %" PRIu32 " but config does not supply it", var->location);
                err = LAHAR_ERR_INVALID_CONFIGURATION;
                goto error;
            }
        }

        for (uint32_t i = 0; i < builder->attrib_count; i++) {
            const uint32_t location = builder->attrib_descriptions[i].location;
            const VkFormat format = builder->attrib_descriptions[i].format;
            bool found = false;

            for (uint32_t j = 0; j < shader_var_count; j++) {
                const LaharShaderVarInfo* var = &shader_vars[j];

                if (var->storage_class == LAHAR_SVSC_INPUT && var->location == location) {
                    found = true;

                    VkFormat expected;

                    if ((err = lahar_shader_var_type_to_input_type(var->type, &expected))) {
                        lahar_error("Location %" PRIu32 " has type Lahar doesn't support for vertex inputs", location);
                        goto error;
                    }

                    if (expected != format) {
                        lahar_error(
                            "Location %" PRIu32 " expected format %s, got %s",
                            location,
                            lahar_vkformat_string(expected),
                            lahar_vkformat_string(format)
                        );
                        err = LAHAR_ERR_INVALID_CONFIGURATION;
                        goto error;
                    }
                }
            }

            if (!found) {
                lahar_error("Shader consumes input location %" PRIu32 " but config does not supply it", location);
                err = LAHAR_ERR_INVALID_CONFIGURATION;
                goto error;
            }
        }
    }

    if (builder->layout) {
        chosen_layout = builder->layout;
    }
    else {
        if ((err = __lahar_shader_create_layout(
            shader_var_count,
            shader_vars,
            builder->set_layouts,
            builder->set_layout_count,
            &chosen_layout
        ))) {
            goto error;
        }

        layout_created = true;
    }


    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.pNext = final_pnext_chain;
    pipeline_info.flags = builder->flags;
    pipeline_info.stageCount = stage_index;
    pipeline_info.pStages = stage_infos;
    pipeline_info.pVertexInputState = builder->pVertexInputState ? builder->pVertexInputState : &vertex_input_info;
    pipeline_info.pInputAssemblyState = builder->pInputAssemblyState ? builder->pInputAssemblyState : &input_assembly_info;
    pipeline_info.pViewportState = builder->pViewportState ? builder->pViewportState : &viewport_info;
    pipeline_info.pRasterizationState = builder->pRasterizationState ? builder->pRasterizationState : &rasterizer_info;
    pipeline_info.pMultisampleState = builder->pMultisampleState ? builder->pMultisampleState : &multisampling_info;
    pipeline_info.pColorBlendState = builder->pColorBlendState ? builder->pColorBlendState : &blend_state;
    pipeline_info.pDynamicState = builder->pDynamicState ? builder->pDynamicState : &dynamic_state_info;
    pipeline_info.pDepthStencilState = builder->pDepthStencilState ? builder->pDepthStencilState : &depth_stencil_info;
    pipeline_info.layout = chosen_layout;
    pipeline_info.renderPass = builder->renderPass;
    pipeline_info.subpass = builder->use_subpass_value ? builder->subpass : 0;

    result = vkCreateGraphicsPipelines(lahar->device, VK_NULL_HANDLE, 1, &pipeline_info, lahar->vkalloc, &created_pipeline);
    if (result != VK_SUCCESS) {
        err = LAHAR_ERR_VK_ERR;
        goto error;
    }

error:
    /* Two different bounds, deliberately. Compiler outputs exist for every stage
     * step 1 produced, while modules only exist up to wherever step 2 got to.
     * Iterating compiled_count covers both: modules beyond the failure point are
     * still VK_NULL_HANDLE from the ZINIT and are skipped by the check. */
    for (uint32_t i = 0; i < compiled_count; i++) {
        if (stage_compilers[i] && final_stages[i].code) {
            stage_compilers[i]->release(
                stage_compilers[i]->user_data,
                (uint32_t*)final_stages[i].code,
                final_stages[i].length
            );
        }

        if (modules[i] != VK_NULL_HANDLE) {
            vkDestroyShaderModule(lahar->device, modules[i], lahar->vkalloc);
        }
    }

    if (err) {
        lahar->vkresult = result;
        if (pipeline) { *pipeline = VK_NULL_HANDLE; }
        if (layout) { *layout = VK_NULL_HANDLE; }
    }
    else {
        if (pipeline) { *pipeline = created_pipeline; }
        if (layout) {
            if (layout_created) { *layout = chosen_layout; }
            else { *layout = VK_NULL_HANDLE; }
        }
    }

    lahar_temp_mpop();
    return err;
}

void lahar_shader_builder_set_stages(LaharShaderBuilder* builder, LaharShaderStage* stages, uint32_t stage_count) {
    const uint32_t max = sizeof(builder->stages) / sizeof(builder->stages[0]);
    const uint32_t to_copy = stage_count > max ? max : stage_count;

    if (stages) {
        memcpy(builder->stages, stages, to_copy * sizeof(*stages));
    }

    builder->stage_count = stage_count;
}

void lahar_shader_builder_add_stage(LaharShaderBuilder* builder, LaharShaderStage* stage) {
    const uint32_t max = sizeof(builder->stages) / sizeof(builder->stages[0]);

    if (builder->stage_count < max) {
        memcpy(&builder->stages[builder->stage_count++], stage, sizeof(*stage));
    }
}

void lahar_shader_builder_set_window(LaharShaderBuilder* builder, LaharWindow* window) {
    const LaharWindowState* state = lahar_window_state(window);
    builder->surface_color_format = state->surface_format.format;

    const LaharAttachmentConfig* depth = lahar_window_attachment_config_depth(window);
    const LaharAttachmentConfig* stencil = lahar_window_attachment_config_stencil(window);

    builder->surface_depth_format = depth ? depth->img_info.format : VK_FORMAT_UNDEFINED;
    builder->surface_stencil_format = stencil ? stencil->img_info.format : VK_FORMAT_UNDEFINED;

    builder->blend_state_count = state->attachment_count;
    memset(builder->blend_states, 0, sizeof(builder->blend_states));

    for (uint32_t i = 0; i < builder->blend_state_count; i++) {
        builder->blend_states[i].colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT
            | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT
            | VK_COLOR_COMPONENT_A_BIT;
    }
}

void lahar_shader_builder_set_formats(LaharShaderBuilder* builder, VkFormat color, VkFormat depth, VkFormat stencil) {
    builder->surface_color_format = color;
    builder->surface_depth_format = depth;
    builder->surface_stencil_format = stencil;
}

void lahar_shader_builder_set_blend_states(LaharShaderBuilder* builder, VkPipelineColorBlendAttachmentState* states, uint32_t state_count) {
    const uint32_t max = sizeof(builder->blend_states) / sizeof(builder->blend_states[0]);
    builder->blend_state_count = state_count > max ? max : state_count;
    memcpy(builder->blend_states, states, builder->blend_state_count * sizeof(*builder->blend_states));
}

void lahar_shader_builder_set_vertex_input(
    LaharShaderBuilder* builder,
    VkVertexInputAttributeDescription* attribs,
    VkVertexInputBindingDescription* bindings,
    uint32_t attrib_count,
    uint32_t binding_count
) {
    builder->attrib_descriptions = attribs;
    builder->binding_descriptions = bindings;
    builder->attrib_count = attrib_count;
    builder->binding_count = binding_count;
}

void lahar_shader_builder_set_blend_mode(LaharShaderBuilder* builder, LaharShaderBlendMode blend) {
    VkPipelineColorBlendAttachmentState state = ZINIT;

    state.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT
        | VK_COLOR_COMPONENT_G_BIT
        | VK_COLOR_COMPONENT_B_BIT
        | VK_COLOR_COMPONENT_A_BIT;

    switch (blend) {
        case LAHAR_SBM_DEFAULT:
        case LAHAR_SBM_OPAQUE:
            // Source replaces destination. The factors are still filled in so
            // that flipping blendEnable on later yields straight alpha.
            state.blendEnable = VK_FALSE;
            state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            state.colorBlendOp = VK_BLEND_OP_ADD;
            state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            state.alphaBlendOp = VK_BLEND_OP_ADD;
            break;

        case LAHAR_SBM_ALPHA:
            // Straight (non-premultiplied) alpha. What you want for UI and
            // sprites whose color is not already scaled by their alpha.
            state.blendEnable = VK_TRUE;
            state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            state.colorBlendOp = VK_BLEND_OP_ADD;
            state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            state.alphaBlendOp = VK_BLEND_OP_ADD;
            break;

        case LAHAR_SBM_PREMULTIPLIED:
            // Same result as ALPHA, but assumes rgb is already multiplied by a.
            // Cheaper and correct under filtering/mipmapping.
            state.blendEnable = VK_TRUE;
            state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            state.colorBlendOp = VK_BLEND_OP_ADD;
            state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            state.alphaBlendOp = VK_BLEND_OP_ADD;
            break;

        case LAHAR_SBM_ADDITIVE:
            // Light accumulation: glows, sparks, fire. Never darkens.
            state.blendEnable = VK_TRUE;
            state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            state.colorBlendOp = VK_BLEND_OP_ADD;
            state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            state.alphaBlendOp = VK_BLEND_OP_ADD;
            break;

        case LAHAR_SBM_MULTIPLICATIVE:
            // Tinting/shadowing: result = src * dst. Never brightens.
            state.blendEnable = VK_TRUE;
            state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
            state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
            state.colorBlendOp = VK_BLEND_OP_ADD;
            state.srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
            state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            state.alphaBlendOp = VK_BLEND_OP_ADD;
            break;

        default:
            // Fatal in all builds: compiled out, this would silently leave the
            // blend states unconfigured and render wrong instead of failing
            LAHAR_FATAL_ASSERT(false && "Unknown blend mode");
            return;
    }

    builder->blend = blend;

    for (uint32_t i = 0; i < sizeof(builder->blend_states) / sizeof(builder->blend_states[0]); i++) {
        builder->blend_states[i] = state;
    }
}

void lahar_shader_builder_set_topology(LaharShaderBuilder* builder, LaharShaderTopology topology) {
    builder->topology = topology;
}

void lahar_shader_builder_set_depth_test(LaharShaderBuilder* builder, LaharShaderDepthTestMode depth_test) {
    builder->depth_test = depth_test;
}

void lahar_shader_builder_set_depth_write(LaharShaderBuilder* builder, LaharShaderDepthWriteMode depth_write) {
    builder->depth_write = depth_write;
}

void lahar_shader_builder_set_depth_compare_op(LaharShaderBuilder* builder, LaharShaderDepthCompareOp depth_compare_op) {
    builder->depth_compare_op = depth_compare_op;
}

void lahar_shader_builder_set_cull_mode(LaharShaderBuilder* builder, LaharShaderCullMode cull_mode) {
    builder->cull_mode = cull_mode;
}

void lahar_shader_builder_set_face_mode(LaharShaderBuilder* builder, LaharShaderFaceMode face_mode) {
    builder->face_mode = face_mode;
}

void lahar_shader_builder_set_dynamic_flags(LaharShaderBuilder* builder, LaharShaderDynamicFlags dynamic_mode) {
    builder->dynamic_mode = dynamic_mode;
}

void lahar_shader_builder_enable_all_dynamic(LaharShaderBuilder* builder, bool all_dynamic) {
    builder->all_dynamic = all_dynamic;
}

void lahar_shader_builder_set_wireframe(LaharShaderBuilder* builder, bool wireframe) {
    builder->wireframe = wireframe;
}

void lahar_shader_builder_skip_validation(LaharShaderBuilder* builder, bool skip_validation) {
    builder->skip_validation = skip_validation;
}

void lahar_shader_builder_set_pnext(LaharShaderBuilder* builder, void* pNext) {
    builder->pNext = pNext;
}

void lahar_shader_builder_set_flags(LaharShaderBuilder* builder, VkPipelineCreateFlags flags) {
    builder->flags = flags;
}

void lahar_shader_builder_set_vertex_input_state(LaharShaderBuilder* builder, const VkPipelineVertexInputStateCreateInfo* pVertexInputState) {
    builder->pVertexInputState = pVertexInputState;
}

void lahar_shader_builder_set_input_assembly_state(LaharShaderBuilder* builder, const VkPipelineInputAssemblyStateCreateInfo* pInputAssemblyState) {
    builder->pInputAssemblyState = pInputAssemblyState;
}

void lahar_shader_builder_set_tessellation_state(LaharShaderBuilder* builder, const VkPipelineTessellationStateCreateInfo* pTessellationState) {
    builder->pTessellationState = pTessellationState;
}

void lahar_shader_builder_set_viewport_state(LaharShaderBuilder* builder, const VkPipelineViewportStateCreateInfo* pViewportState) {
    builder->pViewportState = pViewportState;
}

void lahar_shader_builder_set_rasterization_state(LaharShaderBuilder* builder, const VkPipelineRasterizationStateCreateInfo* pRasterizationState) {
    builder->pRasterizationState = pRasterizationState;
}

void lahar_shader_builder_set_multisample_state(LaharShaderBuilder* builder, const VkPipelineMultisampleStateCreateInfo* pMultisampleState) {
    builder->pMultisampleState = pMultisampleState;
}

void lahar_shader_builder_set_depth_stencil_state(LaharShaderBuilder* builder, const VkPipelineDepthStencilStateCreateInfo* pDepthStencilState) {
    builder->pDepthStencilState = pDepthStencilState;
}

void lahar_shader_builder_set_color_blend_state(LaharShaderBuilder* builder, const VkPipelineColorBlendStateCreateInfo* pColorBlendState) {
    builder->pColorBlendState = pColorBlendState;
}

void lahar_shader_builder_set_dynamic_state(LaharShaderBuilder* builder, const VkPipelineDynamicStateCreateInfo* pDynamicState) {
    builder->pDynamicState = pDynamicState;
}

void lahar_shader_builder_set_layout(LaharShaderBuilder* builder, VkPipelineLayout layout) {
    builder->layout = layout;
}

void lahar_shader_builder_set_descriptor_set_layouts(LaharShaderBuilder* builder, const VkDescriptorSetLayout* layouts, uint32_t count) {
    builder->set_layouts = layouts;
    builder->set_layout_count = count;
}

void lahar_shader_builder_set_render_pass(LaharShaderBuilder* builder, VkRenderPass renderPass) {
    builder->renderPass = renderPass;
}

void lahar_shader_builder_set_subpass(LaharShaderBuilder* builder, uint32_t subpass) {
    builder->subpass = subpass;
    builder->use_subpass_value = true;
}






























// ============================================================================
// Region: SPIR-V Reflection
// ============================================================================

struct LaharSPVTypeInfo;
typedef struct LaharSPVTypeInfo LaharSPVTypeInfo;

struct LaharSPVDescriptorSize;
typedef struct LaharSPVDescriptorSize LaharSPVDescriptorSize;

struct LaharSPVInfo;
typedef struct LaharSPVInfo LaharSPVInfo;

struct LaharSPVInfo {
    const LaharShaderStage* stage;
    LaharSPVTypeInfo* type_table;
    uint32_t type_count;

    struct {
        uint32_t capabilities;
        uint32_t extensions;
        uint32_t mode_setting;
        uint32_t debug;
        uint32_t annotations;
        uint32_t types;
        uint32_t functions;
    } section_offsets;
};

#define LAHAR_MIN(a, b) a < b ? a : b

// Note that the byte sizes of the types is a best estimate based on logical type,
// and in reality is determined by use context, such as struct padding
typedef struct LaharSPVMemberInfo {
    char* name;
    uint32_t id;
    uint32_t offset;
    uint32_t matrix_stride;
} LaharSPVMemberInfo;

struct LaharSPVTypeInfo {
    uint32_t id;
    uint32_t size_bytes;
    uint32_t size_components;
    uint32_t component_type;
    //uint32_t offset_bytes;
    uint32_t array_stride;
    LaharShaderVarType type;
    bool is_signed;
    LaharSPVMemberInfo* members;
    uint32_t tree_size;
};

struct LaharSPVDescriptorSize {
    uint32_t set;
    uint32_t binding;
    uint32_t tree_size;
};

#define LAHAR_SPV_DECORATION_BLOCK 2
#define LAHAR_SPV_DECORATION_BUFFERBLOCK 3
#define LAHAR_SPV_DECORATION_ROW_MAJOR 4
#define LAHAR_SPV_DECORATION_COL_MAJOR 4
#define LAHAR_SPV_DECORATION_ARRAY_STRIDE 6     // followed by a literal bytes
#define LAHAR_SPV_DECORATION_MATRIX_STRIDE 7    // followed by a literal bytes
#define LAHAR_SPV_DECORATION_BUILTIN 11
#define LAHAR_SPV_DECORATION_LOCATION 30        // folowed by a location enum
#define LAHAR_SPV_DECORATION_BINDING 33         // followed by the biding number
#define LAHAR_SPV_DECORATION_DESCRIPTOR_SET 34  // Followed by the set number
#define LAHAR_SPV_DECORATION_OFFSET 35          // followed by the byte offset number



#define LAHAR_SPV_OP_VARIABLE 59

#define LAHAR_SPV_STORAGE_CLASS_UNIFORM_CONSTANT 0
#define LAHAR_SPV_STORAGE_CLASS_INPUT 1
#define LAHAR_SPV_STORAGE_CLASS_UNIFORM 2
#define LAHAR_SPV_STORAGE_CLASS_PUSH_CONSTANT 9
#define LAHAR_SPV_STORAGE_CLASS_STORAGE_BUFFER 12


// Opcodes

// Debug opcodes
#define LAHAR_SPV_OP_SOURCE_CONTINUED 2
#define LAHAR_SPV_OP_SOURCE 3
#define LAHAR_SPV_OP_SOURCE_EXTENSION 4
#define LAHAR_SPV_OP_NAME 5
#define LAHAR_SPV_OP_MEMBER_NAME 6
#define LAHAR_SPV_OP_STRING 7
#define LAHAR_SPV_OP_LINE 8
#define LAHAR_SPV_OP_NO_LINE 317
#define LAHAR_SPV_OP_MODULE_PROCESSED 330

static bool __lahar_spv_op_is_debug(uint16_t opcode) {
    switch (opcode) {
        case LAHAR_SPV_OP_SOURCE_CONTINUED:
        case LAHAR_SPV_OP_SOURCE:
        case LAHAR_SPV_OP_SOURCE_EXTENSION:
        case LAHAR_SPV_OP_NAME:
        case LAHAR_SPV_OP_MEMBER_NAME:
        case LAHAR_SPV_OP_STRING:
        case LAHAR_SPV_OP_LINE:
        case LAHAR_SPV_OP_NO_LINE:
        case LAHAR_SPV_OP_MODULE_PROCESSED:
            return true;
        default:
            return false;
    }
}

// Annotation instructions
#define LAHAR_SPV_OP_DECORATE 71
#define LAHAR_SPV_OP_MEMBER_DECORATE 72
#define LAHAR_SPV_OP_DECORATION_GROUP 73
#define LAHAR_SPV_OP_GROUP_DECORATE 74
#define LAHAR_SPV_OP_GROUP_MEMBER_DECORATE 75
#define LAHAR_SPV_OP_DECORATE_ID 332
#define LAHAR_SPV_OP_DECORATE_STRING 5632
#define LAHAR_SPV_OP_MEMBER_DECORATE_STRING 5633

static bool __lahar_spv_op_is_annotation(uint16_t opcode) {
    switch (opcode) {
        case LAHAR_SPV_OP_DECORATE:
        case LAHAR_SPV_OP_MEMBER_DECORATE:
        case LAHAR_SPV_OP_DECORATION_GROUP:
        case LAHAR_SPV_OP_GROUP_DECORATE:
        case LAHAR_SPV_OP_GROUP_MEMBER_DECORATE:
        case LAHAR_SPV_OP_DECORATE_ID:
        case LAHAR_SPV_OP_DECORATE_STRING:
        case LAHAR_SPV_OP_MEMBER_DECORATE_STRING:
            return true;
        default:
            return false;
    }
}

// Extension instructions
#define LAHAR_SPV_OP_EXTENSION 10
#define LAHAR_SPV_OP_EXT_INST_IMPORT 11
#define LAHAR_SPV_OP_EXT_INST 12
#define LAHAR_SPV_OP_EXT_INST_WITH_FORWARD_REFS_KHR 4433
#define LAHAR_SPV_OP_CONDITIONAL_EXTENSION_INTEL 6248

static bool __lahar_spv_op_is_extension(uint16_t opcode) {
    switch (opcode) {
        case LAHAR_SPV_OP_EXTENSION:
        case LAHAR_SPV_OP_EXT_INST_IMPORT:
        case LAHAR_SPV_OP_EXT_INST:
        case LAHAR_SPV_OP_EXT_INST_WITH_FORWARD_REFS_KHR:
        case LAHAR_SPV_OP_CONDITIONAL_EXTENSION_INTEL:
            return true;
        default:
            return false;
    }
}

// Mode setting instructions

#define LAHAR_SPV_OP_MEMORY_MODEL 14
#define LAHAR_SPV_OP_ENTRY_POINT 15
#define LAHAR_SPV_OP_EXECUTION_MODE 16
#define LAHAR_SPV_OP_CAPABILITY 17
#define LAHAR_SPV_OP_EXECUTION_MODE_ID 331
#define LAHAR_SPV_OP_CONDITIONAL_ENTRY_POINT_INTEL 6249
#define LAHAR_SPV_OP_CONDITIONAL_CAPABILITY 6250

static bool __lahar_spv_op_is_mode_setting(uint16_t opcode) {
    switch (opcode) {
        case LAHAR_SPV_OP_MEMORY_MODEL:
        case LAHAR_SPV_OP_ENTRY_POINT:
        case LAHAR_SPV_OP_EXECUTION_MODE:
        case LAHAR_SPV_OP_CAPABILITY:
        case LAHAR_SPV_OP_EXECUTION_MODE_ID:
        case LAHAR_SPV_OP_CONDITIONAL_ENTRY_POINT_INTEL:
        case LAHAR_SPV_OP_CONDITIONAL_CAPABILITY:
            return true;
        default:
            return false;
    }
}

// Type opcodes
#define LAHAR_SPV_OP_TYPE_VOID 19
#define LAHAR_SPV_OP_TYPE_BOOL 20
#define LAHAR_SPV_OP_TYPE_INT 21
#define LAHAR_SPV_OP_TYPE_FLOAT 22
#define LAHAR_SPV_OP_TYPE_VECTOR 23
#define LAHAR_SPV_OP_TYPE_MATRIX 24
#define LAHAR_SPV_OP_TYPE_IMAGE 25
#define LAHAR_SPV_OP_TYPE_SAMPLER 26
#define LAHAR_SPV_OP_TYPE_SAMPLED_IMAGE 27
#define LAHAR_SPV_OP_TYPE_ARRAY 28
#define LAHAR_SPV_OP_TYPE_RUNTIME_ARRAY 29
#define LAHAR_SPV_OP_TYPE_STRUCT 30
#define LAHAR_SPV_OP_TYPE_OPAQUE 31
#define LAHAR_SPV_OP_TYPE_POINTER 32
#define LAHAR_SPV_OP_TYPE_FUNCTION 33
#define LAHAR_SPV_OP_TYPE_EVENT 34
#define LAHAR_SPV_OP_TYPE_DEVICE_EVENT 35
#define LAHAR_SPV_OP_TYPE_RESERVE_ID 36
#define LAHAR_SPV_OP_TYPE_QUEUE 37
#define LAHAR_SPV_OP_TYPE_PIPE 38
#define LAHAR_SPV_OP_TYPE_FORWARD_POINTER 39
#define LAHAR_SPV_OP_TYPE_PIPE_STORAGE 322
#define LAHAR_SPV_OP_TYPE_NAMED_BARRIER 327
#define LAHAR_SPV_OP_TYPE_TENSOR_ARM 4163
#define LAHAR_SPV_OP_TYPE_GRAPH_ARM 4190
#define LAHAR_SPV_OP_TYPE_UNTYPED_POINTER_KHR 4417
#define LAHAR_SPV_OP_TYPE_COOPERATIVE_MATRIX_KHR 4456
#define LAHAR_SPV_OP_TYPE_RAY_QUERY_KHR 4472
#define LAHAR_SPV_OP_TYPE_HIT_OBJECT_NV 5281
#define LAHAR_SPV_OP_TYPE_COOPERATIVE_VECTOR_NV 5288
#define LAHAR_SPV_OP_TYPE_ACCELERATION_STRUCTURE_KHR 5341
#define LAHAR_SPV_OP_TYPE_COOPERATIVE_MATRIX_NV 5358
#define LAHAR_SPV_OP_TYPE_TENSOR_LAYOUT_NV 5370
#define LAHAR_SPV_OP_TYPE_TENSOR_VIEW_NV 5371
#define LAHAR_SPV_OP_TYPE_BUFFER_SURFACE_INTEL 6086
#define LAHAR_SPV_OP_TYPE_STRUCT_CONTINUED 6090
#define LAHAR_SPV_OP_TYPE_TASK_SEQUENCE_INTEL 6199

static bool __lahar_spv_op_is_type_declaration(uint16_t opcode) {
    switch (opcode) {
        case LAHAR_SPV_OP_TYPE_VOID:
        case LAHAR_SPV_OP_TYPE_BOOL:
        case LAHAR_SPV_OP_TYPE_INT:
        case LAHAR_SPV_OP_TYPE_FLOAT:
        case LAHAR_SPV_OP_TYPE_VECTOR:
        case LAHAR_SPV_OP_TYPE_MATRIX:
        case LAHAR_SPV_OP_TYPE_IMAGE:
        case LAHAR_SPV_OP_TYPE_SAMPLER:
        case LAHAR_SPV_OP_TYPE_SAMPLED_IMAGE:
        case LAHAR_SPV_OP_TYPE_ARRAY:
        case LAHAR_SPV_OP_TYPE_RUNTIME_ARRAY:
        case LAHAR_SPV_OP_TYPE_STRUCT:
        case LAHAR_SPV_OP_TYPE_OPAQUE:
        case LAHAR_SPV_OP_TYPE_POINTER:
        case LAHAR_SPV_OP_TYPE_FUNCTION:
        case LAHAR_SPV_OP_TYPE_EVENT:
        case LAHAR_SPV_OP_TYPE_DEVICE_EVENT:
        case LAHAR_SPV_OP_TYPE_RESERVE_ID:
        case LAHAR_SPV_OP_TYPE_QUEUE:
        case LAHAR_SPV_OP_TYPE_PIPE:
        case LAHAR_SPV_OP_TYPE_FORWARD_POINTER:
        case LAHAR_SPV_OP_TYPE_PIPE_STORAGE:
        case LAHAR_SPV_OP_TYPE_NAMED_BARRIER:
        case LAHAR_SPV_OP_TYPE_TENSOR_ARM:
        case LAHAR_SPV_OP_TYPE_GRAPH_ARM:
        case LAHAR_SPV_OP_TYPE_UNTYPED_POINTER_KHR:
        case LAHAR_SPV_OP_TYPE_COOPERATIVE_MATRIX_KHR:
        case LAHAR_SPV_OP_TYPE_RAY_QUERY_KHR:
        case LAHAR_SPV_OP_TYPE_HIT_OBJECT_NV:
        case LAHAR_SPV_OP_TYPE_COOPERATIVE_VECTOR_NV:
        case LAHAR_SPV_OP_TYPE_ACCELERATION_STRUCTURE_KHR:
        case LAHAR_SPV_OP_TYPE_COOPERATIVE_MATRIX_NV:
        case LAHAR_SPV_OP_TYPE_TENSOR_LAYOUT_NV:
        case LAHAR_SPV_OP_TYPE_TENSOR_VIEW_NV:
        case LAHAR_SPV_OP_TYPE_BUFFER_SURFACE_INTEL:
        case LAHAR_SPV_OP_TYPE_STRUCT_CONTINUED:
        case LAHAR_SPV_OP_TYPE_TASK_SEQUENCE_INTEL:
            return true;
        default:
            return false;
    }
}

#define LAHAR_SPV_OP_CONSTANT 43
#define LAHAR_SPV_OP_SPEC_CONSTANT 50

#define LAHAR_SPV_OP_FUNCTION 54
#define LAHAR_SPV_OP_FUNCTION_PARAMATER 55
#define LAHAR_SPV_OP_FUNCTION_END 56
#define LAHAR_SPV_OP_FUNCTION_CALL 57

#define LAHAR_SPV_OP_VARIABLE 59















// Lookup util functions. In this context, a lookup function is one that
// takes a module, some id or short list of ids, and returns a simple piece
// of data about that id, usually with a simple linear scan of a section

static bool __lahar_spv_v1_var_lookup_is_builtin(const LaharSPVInfo* info, uint32_t var_id) {
    LAHAR_ASSERT(info);

    const uint32_t* code = (const uint32_t*)info->stage->code;

    for (uint64_t i = info->section_offsets.annotations; i < info->section_offsets.types;) {
        const uint32_t word = code[i];
        const uint16_t opcode = word & 0xFFFF;
        const uint16_t word_count = (uint16_t)(word >> 16);

        if (opcode == LAHAR_SPV_OP_DECORATE) {
            const uint32_t decorated_target = code[i + 1];
            const uint32_t decoration_type = code[i + 2];

            // not the target we're looking for
            if (decorated_target != var_id) { i += word_count; continue; }

            if (decoration_type == LAHAR_SPV_DECORATION_BUILTIN) {
                return true;
            }
        }

        i += word_count;
    }

    return false;
}

static bool __lahar_spv_v1_lookup_is_bufferblock(const LaharSPVInfo* info, uint32_t type_id) {
    LAHAR_ASSERT(info);

    const uint32_t* code = (const uint32_t*)info->stage->code;

    for (uint64_t i = info->section_offsets.annotations; i < info->section_offsets.types;) {
        const uint32_t word = code[i];
        const uint16_t opcode = word & 0xFFFF;
        const uint16_t word_count = (uint16_t)(word >> 16);

        if (opcode == LAHAR_SPV_OP_DECORATE) {
            const uint32_t decorated_target = code[i + 1];
            const uint32_t decoration_type = code[i + 2];

            // not the target we're looking for
            if (decorated_target != type_id) { i += word_count; continue; }

            if (decoration_type == LAHAR_SPV_DECORATION_BUFFERBLOCK) {
                return true;
            }
        }

        i += word_count;
    }

    return false;
}

static uint32_t __lahar_spv_v1_lookup_input_location(const LaharSPVInfo* info, uint32_t input_id, uint32_t* location) {
    LAHAR_ASSERT(info && location);

    const uint32_t* code = (const uint32_t*)info->stage->code;

    LAHAR_ASSERT(info->section_offsets.annotations != UINT32_MAX);

    for (uint64_t i = info->section_offsets.annotations; i < info->section_offsets.types;) {
        const uint32_t word = code[i];
        const uint16_t opcode = word & 0xFFFF;
        const uint16_t word_count = (uint16_t)(word >> 16);

        if (opcode == LAHAR_SPV_OP_DECORATE) {
            const uint32_t target = code[i + 1];
            const uint32_t deco_type = code[i + 2];

            if (
                target != input_id ||
                deco_type != LAHAR_SPV_DECORATION_LOCATION
            ) {
                i += word_count; continue;
            }

            *location = code[i + 3];
            return LAHAR_ERR_SUCCESS;
        }

        i += word_count;
    }

    return LAHAR_ERR_ID_NOT_FOUND;
}

static uint32_t __lahar_spv_v1_lookup_type_array_stride(const LaharSPVInfo* info, uint32_t type_id, uint32_t* stride_out) {
    LAHAR_ASSERT(info && stride_out);

    const uint32_t* code = (const uint32_t*)info->stage->code;

    for (uint64_t i = info->section_offsets.annotations; i < info->section_offsets.types;) {
        const uint32_t word = code[i];
        const uint16_t opcode = word & 0xFFFF;
        const uint16_t word_count = (uint16_t)(word >> 16);

        if (opcode == LAHAR_SPV_OP_DECORATE) {
            const uint32_t target = code[i + 1];
            const uint32_t deco_type = code[i + 2];

            if (deco_type == LAHAR_SPV_DECORATION_ARRAY_STRIDE && target == type_id) {
                *stride_out = code[i + 3];
                return LAHAR_ERR_SUCCESS;
            }
        }

        i += word_count;
    }

    return LAHAR_ERR_ID_NOT_FOUND;
}

static const char* __lahar_spv_v1_lookup_id_name(const LaharSPVInfo* info, uint32_t var_id) {
    if (info->section_offsets.debug == UINT32_MAX) {
        return NULL;
    }

    const uint32_t* code = (const uint32_t*)info->stage->code;

    for (uint64_t i = info->section_offsets.debug; i < info->section_offsets.annotations;) {
        const uint32_t word = code[i];
        const uint16_t opcode = word & 0xFFFF;
        const uint16_t word_count = (uint16_t)(word >> 16);

        if (opcode == LAHAR_SPV_OP_NAME) {
            const uint32_t target = code[i + 1];

            if (target == var_id) {
                return (const char*)&code[i + 2];
            }
        }

        i += word_count;
    }

    return NULL;
}

static uint32_t __lahar_spv_v1_lookup_set_binding(const LaharSPVInfo* info, uint32_t var_id, uint32_t* set_out, uint32_t* binding_out) {
    LAHAR_ASSERT(info && set_out && binding_out);

    const uint32_t* code = (const uint32_t*)info->stage->code;

    uint32_t set = UINT32_MAX, binding = UINT32_MAX;

    for (uint64_t i = info->section_offsets.annotations; i < info->section_offsets.types;) {
        const uint32_t word = code[i];
        const uint16_t opcode = word & 0xFFFF;
        const uint16_t word_count = (uint16_t)(word >> 16);

        if (opcode == LAHAR_SPV_OP_DECORATE) {
            const uint32_t decorated_target = code[i + 1];
            const uint32_t decoration_type = code[i + 2];

            // not the target we're looking for
            if (decorated_target != var_id) { i += word_count; continue; }

            switch (decoration_type) {
                case LAHAR_SPV_DECORATION_DESCRIPTOR_SET: {
                    set = code[i + 3];
                } break;
                case LAHAR_SPV_DECORATION_BINDING: {
                    binding = code[i + 3];
                } break;

                default: break;
            }
        }

        if (set != UINT32_MAX && binding != UINT32_MAX) {
            break;
        }

        i += word_count;
    }

    // Assume this is only called on uniforms, and so anything without
    // a set/binding is malformed
    if (set == UINT32_MAX || binding == UINT32_MAX) {
        return LAHAR_ERR_ID_NOT_FOUND;
    }

    *set_out = set;
    *binding_out = binding;

    return LAHAR_ERR_SUCCESS;
}

static uint32_t __lahar_spv_v1_lookup_member_offset(const LaharSPVInfo* info, uint32_t struct_type_id, uint32_t member_index, uint32_t* offset_out) {
    LAHAR_ASSERT(info && offset_out);

    const uint32_t* code = (const uint32_t*)info->stage->code;

    for (uint64_t i = info->section_offsets.annotations; i < info->section_offsets.types;) {
        const uint32_t word = code[i];
        const uint16_t opcode = word & 0xFFFF;
        const uint16_t word_count = (uint16_t)(word >> 16);

        if (opcode == LAHAR_SPV_OP_MEMBER_DECORATE) {
            const uint32_t struct_target = code[i + 1];
            const uint32_t target_member_index = code [i + 2];
            const uint32_t deco_type = code[i + 3];

            if (
                struct_target == struct_type_id &&
                target_member_index == member_index &&
                deco_type == LAHAR_SPV_DECORATION_OFFSET
            ) {
                *offset_out = code[i + 4];
                return LAHAR_ERR_SUCCESS;
            }
        }

        i += word_count;
    }

    return LAHAR_ERR_ID_NOT_FOUND;
}

static uint32_t __lahar_spv_v1_lookup_member_matrix_stride(const LaharSPVInfo* info, uint32_t struct_type_id, uint32_t member_index, uint32_t* stride_out) {
    LAHAR_ASSERT(info && stride_out);

    const uint32_t* code = (const uint32_t*)info->stage->code;

    for (uint64_t i = info->section_offsets.annotations; i < info->section_offsets.types;) {
        const uint32_t word = code[i];
        const uint16_t opcode = word & 0xFFFF;
        const uint16_t word_count = (uint16_t)(word >> 16);

        if (opcode == LAHAR_SPV_OP_MEMBER_DECORATE) {
            const uint32_t struct_target = code[i + 1];
            const uint32_t target_member_index = code [i + 2];
            const uint32_t deco_type = code[i + 3];

            if (
                struct_target == struct_type_id &&
                target_member_index == member_index &&
                deco_type == LAHAR_SPV_DECORATION_MATRIX_STRIDE
            ) {
                *stride_out = code[i + 4];
                return LAHAR_ERR_SUCCESS;
            }
        }

        i += word_count;
    }

    return LAHAR_ERR_ID_NOT_FOUND;
}

static const char* __lahar_spv_v1_lookup_member_name(const LaharSPVInfo* info, uint32_t struct_type, uint32_t member_index) {
    LAHAR_ASSERT(info);

    if (info->section_offsets.debug == UINT32_MAX) {
        return NULL;
    }

    const uint32_t* code = (const uint32_t*)info->stage->code;

    for (uint64_t i = info->section_offsets.debug; i < info->section_offsets.annotations;) {
        const uint32_t word = code[i];
        const uint16_t opcode = word & 0xFFFF;
        const uint16_t word_count = (uint16_t)(word >> 16);

        if (opcode == LAHAR_SPV_OP_MEMBER_NAME) {
            const uint32_t target_struct = code[i + 1];
            const uint32_t target_member = code[i + 2];
            const char* string_begin = (const char*)&code[i + 3];

            if (target_struct == struct_type && target_member == member_index) {
                return string_begin;
            }
        }

        i += word_count;
    }

    return NULL;
}

static uint32_t __lahar_spv_v1_lookup_constant(const LaharSPVInfo* info, uint32_t constant_id, uint32_t* value_out) {
    LAHAR_ASSERT(info && value_out);

    const uint32_t* code = (const uint32_t*)info->stage->code;

    for (uint64_t i = info->section_offsets.types; i < info->section_offsets.functions;) {
        const uint32_t word = code[i];
        const uint16_t opcode = word & 0xFFFF;
        const uint16_t word_count = (uint16_t)(word >> 16);

        if (
            opcode == LAHAR_SPV_OP_CONSTANT ||
            opcode == LAHAR_SPV_OP_SPEC_CONSTANT
        ) {
            //const uint32_t result_type = code[i + 1];
            const uint32_t result = code[i + 2];
            const uint32_t value = code[i + 3];

            if (result == constant_id) {
                *value_out = value;
                return LAHAR_ERR_SUCCESS;
            }
        }

        i += word_count;
    }

    return LAHAR_ERR_ID_NOT_FOUND;
}



/** Auto-derefences pointer types. So for example, if a descriptor is of type
 * Uniform Struct*, you'll get Struct out as your type.
 */
static LaharSPVTypeInfo* __lahar_spv_get_type(const LaharSPVInfo* info, uint32_t id) {
    for (uint32_t i = 0; i < info->type_count; i++) {
        if (info->type_table[i].id == id) {

            if (info->type_table[i].type == LAHAR_SVT_POINTER) {
                return __lahar_spv_get_type(info, info->type_table[i].component_type);
            }
            else {
                return &info->type_table[i];
            }

        }
    }

    return NULL;
}

static bool __lahar_spv_type_is_matrix(const LaharSPVTypeInfo* type) {
    switch(type->type) {
        case LAHAR_SVT_MAT2X3:
        case LAHAR_SVT_MAT2X4:
        case LAHAR_SVT_MAT3X2:
        case LAHAR_SVT_MAT3X4:
        case LAHAR_SVT_MAT4X2:
        case LAHAR_SVT_MAT4X3:
        case LAHAR_SVT_MAT2:
        case LAHAR_SVT_MAT3:
        case LAHAR_SVT_MAT4:
        case LAHAR_SVT_DMAT2X3:
        case LAHAR_SVT_DMAT2X4:
        case LAHAR_SVT_DMAT3X2:
        case LAHAR_SVT_DMAT3X4:
        case LAHAR_SVT_DMAT4X2:
        case LAHAR_SVT_DMAT4X3:
        case LAHAR_SVT_DMAT2:
        case LAHAR_SVT_DMAT3:
            return true;
        default:
            return false;
    }
}

static uint32_t __lahar_spv_type_size_recurse(
    const LaharSPVInfo* info,
    const LaharSPVTypeInfo* type,
    const LaharSPVMemberInfo* member_info,
    bool include_padding
) {
    LAHAR_ASSERT(info && type);

    switch (type->type) {
        case LAHAR_SVT_UNKNOWN:
        case LAHAR_SVT_VOID:
        case LAHAR_SVT_SAMPLER:
        case LAHAR_SVT_IMAGE:
        case LAHAR_SVT_SAMPLER_IMAGE:
        case LAHAR_SVT_POINTER:
            return 0;

        case LAHAR_SVT_BOOL:
        case LAHAR_SVT_BVEC2:
        case LAHAR_SVT_BVEC3:
        case LAHAR_SVT_BVEC4:
            return 0; // weird edge cases you should never see

        case LAHAR_SVT_INT:
        case LAHAR_SVT_UINT:
        case LAHAR_SVT_HALF:
        case LAHAR_SVT_FLOAT:
        case LAHAR_SVT_DOUBLE:
            return type->size_bytes;

        case LAHAR_SVT_IVEC2:
        case LAHAR_SVT_IVEC3:
        case LAHAR_SVT_IVEC4:
        case LAHAR_SVT_UVEC2:
        case LAHAR_SVT_UVEC3:
        case LAHAR_SVT_UVEC4:
        case LAHAR_SVT_DVEC2:
        case LAHAR_SVT_DVEC3:
        case LAHAR_SVT_DVEC4:
        case LAHAR_SVT_VEC2:
        case LAHAR_SVT_VEC3:
        case LAHAR_SVT_VEC4:
        {
            const LaharSPVTypeInfo* inner_type = __lahar_spv_get_type(info, type->component_type);
            LAHAR_ASSERT(inner_type);

            return type->size_components * inner_type->size_bytes;
        }

        case LAHAR_SVT_MAT2X3:
        case LAHAR_SVT_MAT2X4:
        case LAHAR_SVT_MAT3X2:
        case LAHAR_SVT_MAT3X4:
        case LAHAR_SVT_MAT4X2:
        case LAHAR_SVT_MAT4X3:
        case LAHAR_SVT_MAT2:
        case LAHAR_SVT_MAT3:
        case LAHAR_SVT_MAT4:
        case LAHAR_SVT_DMAT2X3:
        case LAHAR_SVT_DMAT2X4:
        case LAHAR_SVT_DMAT3X2:
        case LAHAR_SVT_DMAT3X4:
        case LAHAR_SVT_DMAT4X2:
        case LAHAR_SVT_DMAT4X3:
        case LAHAR_SVT_DMAT2:
        case LAHAR_SVT_DMAT3:
        case LAHAR_SVT_DMAT4:
            if (include_padding) {
                LAHAR_ASSERT(member_info);
                return member_info->matrix_stride * type->size_components;
            }
            else {
                const LaharSPVTypeInfo* inner_type = __lahar_spv_get_type(info, type->component_type);
                return type->size_components * __lahar_spv_type_size_recurse(info, inner_type, NULL, include_padding);
            }

        case LAHAR_SVT_STRUCT:
        {
            const LaharSPVMemberInfo* last_member = &type->members[type->size_components - 1];
            const LaharSPVTypeInfo* inner_type = __lahar_spv_get_type(info, last_member->id);
            LAHAR_ASSERT(inner_type);

            return last_member->offset + __lahar_spv_type_size_recurse(info, inner_type, last_member, include_padding);
        }

        case LAHAR_SVT_ARRAY:
            return type->array_stride * type->size_components;
    }

    return 0;
}

/** Get the logical size of a type, other than include_padding causes matrix types to include their padding */
static uint32_t __lahar_spv_type_size(const LaharSPVInfo* info, const LaharSPVTypeInfo* type, bool include_padding) {
    LAHAR_ASSERT(!__lahar_spv_type_is_matrix(type));
    return __lahar_spv_type_size_recurse(info, type, NULL, include_padding);
}

/** Matrices can only be described as having a size in the context of a struct, so a member info must be supplied */
static uint32_t __lahar_spv_matrix_type_size(const LaharSPVInfo* info, const LaharSPVTypeInfo* type, const LaharSPVMemberInfo* member_info, bool include_padding) {
    LAHAR_ASSERT(__lahar_spv_type_is_matrix(type));
    return __lahar_spv_type_size_recurse(info, type, member_info, include_padding);
}














static uint32_t __lahar_shader_stage_validate_spv_header(const uint32_t* spv) {
    const uint32_t magic = 0x07230203;

    if (spv[0] != magic) {
        return LAHAR_ERR_MALFORMED_CODE;
    }

    const uint32_t version = spv[1];

    if (version > VK_MAKE_VERSION(1, 0, 0)) {
        lahar_warn("SPIR-V code is newer than Lahar's parser, best effort attempt to follow");
    }

    const uint32_t generator = spv[2];
    const uint32_t tool_id = generator >> 16; //(generator & 0xFFFF0000) >> 16;

    const char* msg = "Unknown Tool";

    switch (tool_id) {
        case 0: msg = "Khronos"; break;
        case 1: msg = "LunarG"; break;
        case 2: msg = "Valve"; break;
        case 3: msg = "Codeplay"; break;
        case 4: msg = "NVIDIA"; break;
        case 5: msg = "ARM"; break;
        case 6: msg = "Khronos LLM/SPIR-V Translator"; break;
        case 7: msg = "Khronos SPIR-V Tools Assembler"; break;
        case 8: msg = "Khronos Glslang Reference Front End"; break;
        case 9: msg = "Qualcomm"; break;
        case 10: msg = "AMD"; break;
        case 11: msg = "Intel"; break;
        case 12: msg = "Imagination"; break;
        case 13: msg = "Google Shaderc over Glslang"; break;
        case 14: msg = "Google spiregg"; break;
        case 15: msg = "Google rspirv"; break;
        case 16: msg = "X-LEGEND Mesa-IR/SPIR-V Translator"; break;
        case 17: msg = "Khronos SPIR-V Tools Linker"; break;
        case 18: msg = "Wine VKD3D Shader Compiler"; break;
        case 19: msg = "Tellusim Clay Shader Compiler"; break;
        case 20: msg = "W3C WebGPU Group WHLSL Shader Translator"; break;
        case 21: msg = "Googl Clspv"; break;
        case 22: msg = "LLVM MLIR SPIR-V Serializer"; break;
        case 23: msg = "Google Tint Compiler"; break;
        case 24: msg = "Google Angle Shader Compiler"; break;
        case 25: msg = "Netease Games Messiah Shader Compiler"; break;
        case 26: msg = "Xenia Emulator Microcode Translator"; break;
        case 27: msg = "Embark Studios Rust GPU Compiler Backend"; break;
        case 28: msg = "gfx-rs community Naga"; break;
        case 29: msg = "Mikkosoft Productions MSP Shader Compiler"; break;
        case 30: msg = "SpvGenTwo community SPIR-V IR Tools"; break;
        case 31: msg = "Google Skia SkSL"; break;
        case 32: msg = "TornadoVM Beehive SPIRV Toolkit"; break;
        case 33: msg = "DragonJoker ShaderWriter"; break;
        case 34: msg = "Rayan Hatout SPIRVSmith"; break;
        case 35: msg = "Saarland University Shady"; break;
        case 36: msg = "Taichi Graphics"; break;
        case 37: msg = "heroseh Hero C Compiler"; break;
        case 38: msg = "Meta SparkSL"; break;
        case 39: msg = "SirLynix Nazara ShaderLang Compiler"; break;
        case 40: msg = "Khronos Slang Compiler"; break;
        case 41: msg = "Zig Software Foundation Zig Compiler"; break;
        case 42: msg = "Rendong Liang sqp"; break;
        case 43: msg = "LLVM SPIR-V Backend"; break;
        case 44: msg = "Robert Kongrad Kongruent"; break;
        case 45: msg = "Kitsunebi Games Nuvk SPIR-V Emitter and DLSL compiler"; break;
        case 46: msg = "Nintendo"; break;
        case 47: msg = "ARM"; break;
        case 48: msg = "Goopax"; break;
        case 49: msg = "Icyllis Milica Arc3D Shader Compiler"; break;
        default: break;
    }

    lahar_trace("SPIR-V generated by: %s", msg);

    //const uint32_t bound = spv[3];
    //const uint32_t schema = spv[4];

    return LAHAR_ERR_SUCCESS;
}

static uint32_t __lahar_spv_v1_build_section_offsets(LaharSPVInfo* info) {
    const uint32_t* code = (const uint32_t*)info->stage->code;
    const uint64_t words = info->stage->length / 4;

    memset(&info->section_offsets, 0, sizeof(info->section_offsets));

    uint32_t index = 5;
    uint32_t phase = 0;
    bool do_check = true;

    do {
        uint32_t word = code[index];
        uint16_t opcode = word & 0xFFFF;
        uint16_t word_count = (uint16_t)(word >> 16);

        // This loop sees every instruction from the header to the functions
        // section before any other pass runs, and every later walker re-walks
        // sub-ranges of the same stream with the same strides. Validating the
        // stride here therefore guarantees termination and in-bounds reads of
        // instruction words for all of them. (Zero word_count is real-world:
        // it otherwise strides by 0 and hangs forever.)
        if (word_count == 0 || index + (uint64_t)word_count > words) {
            return LAHAR_ERR_MALFORMED_CODE;
        }

        switch (phase) {
            case 0: { // 0: in the capabilities
                if (__lahar_spv_op_is_extension(opcode)) {
                    phase++;
                    info->section_offsets.extensions = index;
                }
                else if (__lahar_spv_op_is_mode_setting(opcode)) {
                    phase += 2;
                    info->section_offsets.extensions = UINT32_MAX;
                    info->section_offsets.mode_setting = index;
                }
            } break;
            case 1: { // 1: in the extensions
                if (__lahar_spv_op_is_mode_setting(opcode)) { // mode setting is required, no extra branch
                    phase++;
                    info->section_offsets.mode_setting = index;
                }
            } break;
            case 2: { // 2: in the mode setting instructions
                if (__lahar_spv_op_is_debug(opcode)) {
                    phase++;
                    info->section_offsets.debug = index;
                }
                else if (__lahar_spv_op_is_annotation(opcode)) {
                    phase += 2;
                    info->section_offsets.debug = UINT32_MAX;
                    info->section_offsets.annotations = index;
                }
            } break;
            case 3: { // 3: in the debug instructions
                if (__lahar_spv_op_is_annotation(opcode)) {
                    phase++;
                    info->section_offsets.annotations = index;
                }
                else if (__lahar_spv_op_is_type_declaration(opcode)) {
                    phase += 2;
                    info->section_offsets.annotations = UINT32_MAX;
                    info->section_offsets.types = index;
                }
            } break;
            case 4: { // 4: in the annotations
                if (__lahar_spv_op_is_type_declaration(opcode)) {
                    phase++;
                    info->section_offsets.types = index;
                }
            } break;
            case 5: { // 5: in the type declarations
                if (opcode == LAHAR_SPV_OP_FUNCTION) {
                    phase++;
                    info->section_offsets.functions = index;
                }
            } break;

            default: do_check = false; break;
        }

        index += word_count;
    } while (index < words && do_check);

    return phase == 6 ? LAHAR_ERR_SUCCESS : LAHAR_ERR_MALFORMED_CODE;
}

static uint32_t __lahar_spv_v1_count_types(LaharSPVInfo* info) {
    LAHAR_ASSERT(info);

    const LaharShaderStage* stage = info->stage;
    const uint32_t* code = (const uint32_t*)stage->code;

    if (!stage || !code) {
        return LAHAR_ERR_ILLEGAL_PARAMS;
    }

    info->type_count = 0;

    for (uint64_t i = info->section_offsets.types; i < info->section_offsets.functions;) {
        uint32_t word = code[i];
        uint16_t opcode = word & 0xFFFF;
        uint16_t word_count = (uint16_t)(word >> 16);

        if (word_count == 0) { return LAHAR_ERR_MALFORMED_CODE; }

        if (__lahar_spv_op_is_type_declaration(opcode)) {
            info->type_count++;
        }

        i += word_count;
    }

    return LAHAR_ERR_SUCCESS;
}



static uint32_t __lahar_spv_v1_extract_types(LaharSPVInfo* info) {
    LAHAR_ASSERT(info);

    const LaharShaderStage* stage = info->stage;
    LAHAR_ASSERT(stage);

    const uint32_t* code = (const uint32_t*)stage->code;
    LAHAR_ASSERT(code);

    info->type_table = (LaharSPVTypeInfo*)lahar_temp_alloc(sizeof(*info->type_table) * info->type_count);

    LaharSPVTypeInfo* tinfo = NULL;

    info->type_count = 0;

    uint32_t err = LAHAR_ERR_SUCCESS;

    for (uint64_t i = info->section_offsets.types; i < info->section_offsets.functions;) {
        uint32_t word = code[i];
        uint16_t opcode = word & 0xFFFF;
        uint16_t word_count = (uint16_t)(word >> 16);

        switch (opcode) {
            case LAHAR_SPV_OP_TYPE_VOID: {
                tinfo = &info->type_table[info->type_count++];
                tinfo->id = code[i + 1];
                tinfo->type = LAHAR_SVT_VOID;
                tinfo->tree_size = 0;
            } break;
            case LAHAR_SPV_OP_TYPE_BOOL: {
                tinfo = &info->type_table[info->type_count++];
                tinfo->id = code[i + 1];
                tinfo->type = LAHAR_SVT_BOOL;
                tinfo->tree_size = 1;
            } break;
            case LAHAR_SPV_OP_TYPE_INT: {
                tinfo = &info->type_table[info->type_count++];
                tinfo->id = code[i + 1];
                tinfo->size_bytes = code[i + 2] / 8;
                tinfo->is_signed = (bool)code[i + 3];
                tinfo->tree_size = 1;
                tinfo->type = tinfo->is_signed ? LAHAR_SVT_INT : LAHAR_SVT_UINT;
            } break;
            case LAHAR_SPV_OP_TYPE_FLOAT: {
                tinfo = &info->type_table[info->type_count++];
                tinfo->id = code[i + 1];
                tinfo->size_bytes = code[i + 2] / 8;

                switch (tinfo->size_bytes) {
                    case 2: tinfo->type = LAHAR_SVT_HALF; break;
                    case 8: tinfo->type = LAHAR_SVT_DOUBLE; break;
                    default:
                    case 4: tinfo->type = LAHAR_SVT_FLOAT; break;
                }

                tinfo->is_signed = true;
                tinfo->tree_size = 1;
            } break;
            case LAHAR_SPV_OP_TYPE_VECTOR: {
                tinfo = &info->type_table[info->type_count++];
                tinfo->id = code[i + 1];
                tinfo->tree_size = 1;

                const uint32_t component_id = code[i + 2];
                const uint32_t component_count = code[i + 3];

                const LaharSPVTypeInfo* component_info = __lahar_spv_get_type(info, component_id);

                if (!component_info) {
                    lahar_trace("Failed to find vector component type");
                    return LAHAR_ERR_MALFORMED_CODE;
                }

                tinfo->size_components = component_count;
                tinfo->size_bytes = component_count * component_info->size_bytes;
                tinfo->is_signed = component_info->is_signed;
                tinfo->component_type = component_id;

                switch (component_count) {
                    case 2: {
                        switch (component_info->type) {
                            case LAHAR_SVT_FLOAT: tinfo->type = LAHAR_SVT_VEC2; break;
                            case LAHAR_SVT_INT: tinfo->type = LAHAR_SVT_IVEC2; break;
                            case LAHAR_SVT_UINT: tinfo->type = LAHAR_SVT_UVEC2; break;
                            case LAHAR_SVT_BOOL: tinfo->type = LAHAR_SVT_BVEC2; break;
                            case LAHAR_SVT_DOUBLE: tinfo->type = LAHAR_SVT_DVEC2; break;
                            default: return LAHAR_ERR_MALFORMED_CODE;
                        }
                    } break;
                    case 3: {
                        switch (component_info->type) {
                            case LAHAR_SVT_FLOAT: tinfo->type = LAHAR_SVT_VEC3; break;
                            case LAHAR_SVT_INT: tinfo->type = LAHAR_SVT_IVEC3; break;
                            case LAHAR_SVT_UINT: tinfo->type = LAHAR_SVT_UVEC3; break;
                            case LAHAR_SVT_BOOL: tinfo->type = LAHAR_SVT_BVEC3; break;
                            case LAHAR_SVT_DOUBLE: tinfo->type = LAHAR_SVT_DVEC3; break;
                            default: return LAHAR_ERR_MALFORMED_CODE;
                        }
                    } break;
                    case 4: {
                        switch (component_info->type) {
                            case LAHAR_SVT_FLOAT: tinfo->type = LAHAR_SVT_VEC4; break;
                            case LAHAR_SVT_INT: tinfo->type = LAHAR_SVT_IVEC4; break;
                            case LAHAR_SVT_UINT: tinfo->type = LAHAR_SVT_UVEC4; break;
                            case LAHAR_SVT_BOOL: tinfo->type = LAHAR_SVT_BVEC4; break;
                            case LAHAR_SVT_DOUBLE: tinfo->type = LAHAR_SVT_DVEC4; break;
                            default: return LAHAR_ERR_MALFORMED_CODE;
                        }
                    } break;
                    default:
                        return LAHAR_ERR_MALFORMED_CODE;
                }
            } break;
            case LAHAR_SPV_OP_TYPE_MATRIX: {
                tinfo = &info->type_table[info->type_count++];
                tinfo->id = code[i + 1];
                tinfo->tree_size = 1;

                const uint32_t component_id = code[i + 2];
                const uint32_t component_count = code[i + 3];

                const LaharSPVTypeInfo* component_info = __lahar_spv_get_type(info, component_id);

                if (!component_info) {
                    lahar_trace("Failed to find matrix component type");
                    return LAHAR_ERR_MALFORMED_CODE;
                }

                tinfo->size_components = component_count;
                tinfo->size_bytes = component_count * component_info->size_bytes;
                tinfo->is_signed = component_info->is_signed;
                tinfo->component_type = component_id;

                switch (component_count) {
                    case 2: {
                        switch (component_info->type) {
                            case LAHAR_SVT_VEC2: tinfo->type = LAHAR_SVT_MAT2; break;
                            case LAHAR_SVT_VEC3: tinfo->type = LAHAR_SVT_MAT2X3; break;
                            case LAHAR_SVT_VEC4: tinfo->type = LAHAR_SVT_MAT2X4; break;
                            case LAHAR_SVT_DVEC2: tinfo->type = LAHAR_SVT_DMAT2; break;
                            case LAHAR_SVT_DVEC3: tinfo->type = LAHAR_SVT_DMAT2X3; break;
                            case LAHAR_SVT_DVEC4: tinfo->type = LAHAR_SVT_DMAT2X4; break;
                            default: return LAHAR_ERR_MALFORMED_CODE;
                        }
                    } break;
                    case 3: {
                        switch (component_info->type) {
                            case LAHAR_SVT_VEC2: tinfo->type = LAHAR_SVT_MAT3X2; break;
                            case LAHAR_SVT_VEC3: tinfo->type = LAHAR_SVT_MAT3; break;
                            case LAHAR_SVT_VEC4: tinfo->type = LAHAR_SVT_MAT3X4; break;
                            case LAHAR_SVT_DVEC2: tinfo->type = LAHAR_SVT_DMAT3X2; break;
                            case LAHAR_SVT_DVEC3: tinfo->type = LAHAR_SVT_DMAT3; break;
                            case LAHAR_SVT_DVEC4: tinfo->type = LAHAR_SVT_DMAT3X4; break;
                            default: return LAHAR_ERR_MALFORMED_CODE;
                        }
                    } break;
                    case 4: {
                        switch (component_info->type) {
                            case LAHAR_SVT_VEC2: tinfo->type = LAHAR_SVT_MAT4X2; break;
                            case LAHAR_SVT_VEC3: tinfo->type = LAHAR_SVT_MAT4X3; break;
                            case LAHAR_SVT_VEC4: tinfo->type = LAHAR_SVT_MAT4; break;
                            case LAHAR_SVT_DVEC2: tinfo->type = LAHAR_SVT_DMAT4X2; break;
                            case LAHAR_SVT_DVEC3: tinfo->type = LAHAR_SVT_DMAT4X3; break;
                            case LAHAR_SVT_DVEC4: tinfo->type = LAHAR_SVT_DMAT4; break;
                            default: return LAHAR_ERR_MALFORMED_CODE;
                        }
                    } break;
                    default:
                        return LAHAR_ERR_MALFORMED_CODE;
                }
            } break;
            case LAHAR_SPV_OP_TYPE_IMAGE: {
                tinfo = &info->type_table[info->type_count++];
                tinfo->id = code[i + 1];
                tinfo->type = LAHAR_SVT_IMAGE;
                tinfo->tree_size = 1;
            } break;
            case LAHAR_SPV_OP_TYPE_SAMPLER: {
                tinfo = &info->type_table[info->type_count++];
                tinfo->id = code[i + 1];
                tinfo->type = LAHAR_SVT_SAMPLER;
                tinfo->tree_size = 1;
            } break;
            case LAHAR_SPV_OP_TYPE_SAMPLED_IMAGE: {
                tinfo = &info->type_table[info->type_count++];
                tinfo->id = code[i + 1];
                tinfo->type = LAHAR_SVT_SAMPLER_IMAGE;
                tinfo->tree_size = 1;
            } break;
            case LAHAR_SPV_OP_TYPE_STRUCT: {
                tinfo = &info->type_table[info->type_count++];
                tinfo->id = code[i + 1];
                tinfo->type = LAHAR_SVT_STRUCT;

                const uint32_t member_count = word_count - 2;
                tinfo->size_components = member_count;

                tinfo->members = (LaharSPVMemberInfo*)lahar_temp_alloc(sizeof(*tinfo->members) * member_count);

                tinfo->tree_size = 1;

                for (uint32_t j = 0; j < tinfo->size_components; j++) {
                    LaharSPVMemberInfo* member_info = &tinfo->members[j];
                    member_info->id = code[i + 2 + j];

                    const char* member_name = __lahar_spv_v1_lookup_member_name(info, tinfo->id, j);

                    if (member_name) {
                        member_info->name = lahar_temp_strdup(member_name);
                    }

                    __lahar_spv_v1_lookup_member_offset(info, tinfo->id, j, &member_info->offset);
                    __lahar_spv_v1_lookup_member_matrix_stride(info, tinfo->id, j, &member_info->matrix_stride);

                    const LaharSPVTypeInfo* inner_type = __lahar_spv_get_type(info, member_info->id);
                    if (!inner_type) {
                        return LAHAR_ERR_MALFORMED_CODE;
                    }

                    tinfo->tree_size += inner_type->tree_size;
                }
            } break;
            case LAHAR_SPV_OP_TYPE_ARRAY: {
                tinfo = &info->type_table[info->type_count++];
                tinfo->id = code[i + 1];
                tinfo->type = LAHAR_SVT_ARRAY;


                const uint32_t component_id = code[i + 2];
                const uint32_t component_count_id = code[i + 3];

                const LaharSPVTypeInfo* component_info = __lahar_spv_get_type(info, component_id);

                if (!component_info) {
                    lahar_trace("Failed to find array component type");
                    return LAHAR_ERR_MALFORMED_CODE;
                }

                if ((err = __lahar_spv_v1_lookup_constant(info, component_count_id, &tinfo->size_components))) {
                    if (err == LAHAR_ERR_ID_NOT_FOUND) {
                        lahar_trace("Failed to find array length for type");
                        return LAHAR_ERR_MALFORMED_CODE;
                    }
                    else { return err; }
                }

                // builtins might have no annotation
                if ((err = __lahar_spv_v1_lookup_type_array_stride(info, tinfo->id, &tinfo->array_stride))) {
                    if (err != LAHAR_ERR_ID_NOT_FOUND) {
                        return err;
                    }
                }

                tinfo->component_type = component_id;
                tinfo->size_bytes = tinfo->array_stride * tinfo->size_components;
                tinfo->tree_size = 1 + component_info->tree_size;
            } break;
            case LAHAR_SPV_OP_TYPE_RUNTIME_ARRAY: {
                tinfo = &info->type_table[info->type_count++];
                tinfo->id = code[i + 1];
                tinfo->type = LAHAR_SVT_ARRAY;

                // builtins might have no annotation
                if ((err = __lahar_spv_v1_lookup_type_array_stride(info, tinfo->id, &tinfo->array_stride))) {
                    if (err != LAHAR_ERR_ID_NOT_FOUND) {
                        return err;
                    }
                }

                const uint32_t component_id = code[i + 2];

                const LaharSPVTypeInfo* component_info = __lahar_spv_get_type(info, component_id);

                if (!component_info) {
                    lahar_trace("Failed to find array component type");
                    return LAHAR_ERR_MALFORMED_CODE;
                }

                tinfo->component_type = component_id;
                tinfo->tree_size = 1 + component_info->tree_size;
            } break;
            case LAHAR_SPV_OP_TYPE_POINTER: {
                tinfo = &info->type_table[info->type_count++];
                tinfo->type = LAHAR_SVT_POINTER;

                const uint32_t pointer_type = code[i + 1];
                //const uint32_t storage_class = code[i + 2];
                const uint32_t pointed_type = code[i + 3];

                tinfo->id = pointer_type;
                tinfo->component_type = pointed_type;
            } break;
            case LAHAR_SPV_OP_TYPE_OPAQUE: {} break;
            case LAHAR_SPV_OP_TYPE_FUNCTION: {} break;
            default:
                break;
        }

        i += word_count;
    }

    return LAHAR_ERR_SUCCESS;
}

/** This function is a little unusual, it's designed to be called
 * repeatedly with different infos, but the same slots_out and
 * unique_set_bindings_out, and it expands the usbo and increments
 * the slot count appropriately.
 *
 * It uses temp_alloc oddly to achieve this, using it to grow
 * the last allocation in place
 */
static uint32_t __lahar_spv_v1_count_unique_descriptor_slots(
    LaharSPVInfo* info,
    uint32_t* slots_out,
    LaharSPVDescriptorSize** unique_set_bindings_out
) {
    LAHAR_ASSERT(info && slots_out && unique_set_bindings_out);

    if (info->section_offsets.annotations == UINT32_MAX) {
        // This shader has no descriptors
        return LAHAR_ERR_SUCCESS;
    }

    LAHAR_ASSERT(
        info->section_offsets.types != UINT32_MAX &&
        info->section_offsets.functions != UINT32_MAX
    );

    const uint32_t* code = (const uint32_t*)info->stage->code;

    LaharSPVDescriptorSize* slot_array = *unique_set_bindings_out;
    if (*slots_out) { LAHAR_ASSERT(slot_array); }

    // search for vars
    for (uint64_t i = info->section_offsets.types; i < info->section_offsets.functions;) {
        const uint32_t word = code[i];
        const uint16_t opcode = word & 0xFFFF;
        const uint16_t word_count = (uint16_t)(word >> 16);

        // found a var
        if (opcode == LAHAR_SPV_OP_VARIABLE) {
            const uint32_t var_type = code[i + 1];
            const uint32_t var_id = code[i + 2];
            const uint32_t var_storage_class = code[i + 3];

            if (
                var_storage_class != LAHAR_SPV_STORAGE_CLASS_UNIFORM_CONSTANT &&
                var_storage_class != LAHAR_SPV_STORAGE_CLASS_UNIFORM &&
                var_storage_class != LAHAR_SPV_STORAGE_CLASS_STORAGE_BUFFER
            ) {
                i += word_count;
                continue; // not a uniform
            }

            uint32_t set = UINT32_MAX, binding = UINT32_MAX;
            uint32_t err = __lahar_spv_v1_lookup_set_binding(info, var_id, &set, &binding);

            if (err) {
                if (err == LAHAR_ERR_ID_NOT_FOUND) { return LAHAR_ERR_MALFORMED_CODE; }
                else { return err; }
            }

            LAHAR_ASSERT(set != UINT32_MAX && binding != UINT32_MAX);

            // do a dedup pass to check if we've seen this descriptor already
            bool already_seen = false;

            for (uint64_t j = 0; j < *slots_out; j++) {
                if (slot_array[j].set == set && slot_array[j].binding == binding) {
                    already_seen = true;
                    break;
                }
            }

            if (already_seen) {
                i += word_count;
                continue;
            }

            // We haven't, so create info for it
            LaharSPVDescriptorSize* desc_size = (LaharSPVDescriptorSize*)lahar_temp_alloc_aligned(
                sizeof(*slot_array),
                LAHAR_ALIGNOF(LaharSPVDescriptorSize)
            );

            desc_size->set = set;
            desc_size->binding = binding;

            LaharSPVTypeInfo* var_type_info = __lahar_spv_get_type(info, var_type);
            if (!var_type_info) {
                lahar_trace("Descriptor references invalid type");
                return LAHAR_ERR_MALFORMED_CODE;
            }

            // The struct already counts itself as a 1, so
            // the tree size should be copied directly
            desc_size->tree_size = var_type_info->tree_size;

            if (slot_array == NULL) {
                slot_array = *unique_set_bindings_out = desc_size;
            }

            *slots_out = *slots_out + 1;
        }

        i += word_count;
    }

    return LAHAR_ERR_SUCCESS;
}

static void __lahar_spv_v1_count_input_slots(const LaharSPVInfo* info, uint32_t* count_out) {
    LAHAR_ASSERT(info && count_out);

    *count_out = 0;
    const uint32_t* code = (const uint32_t*)info->stage->code;

    for (uint64_t i = info->section_offsets.types; i < info->section_offsets.functions;) {
        const uint32_t word = code[i];
        const uint16_t opcode = word & 0xFFFF;
        const uint16_t word_count = (uint16_t)(word >> 16);

        if (opcode == LAHAR_SPV_OP_VARIABLE) {
            //const uint32_t var_type = code[i + 1];
            const uint32_t var_id = code[i + 2];
            const uint32_t var_storage_class = code[i + 3];

            if (
                var_storage_class == LAHAR_SPV_STORAGE_CLASS_INPUT &&
                !__lahar_spv_v1_var_lookup_is_builtin(info, var_id)
            ) {
                *count_out += 1;
            }
        }

        i += word_count;
    }
}

static uint32_t __lahar_spv_v1_count_push_contant_slots(LaharSPVInfo* info, uint32_t* count_out, bool* saw) {
    LAHAR_ASSERT(info && count_out && saw);

    *count_out = 0;
    const uint32_t* code = (const uint32_t*)info->stage->code;
    *saw = false;

    for (uint64_t i = info->section_offsets.types; i < info->section_offsets.functions;) {
        const uint32_t word = code[i];
        const uint16_t opcode = word & 0xFFFF;
        const uint16_t word_count = (uint16_t)(word >> 16);

        if (opcode == LAHAR_SPV_OP_VARIABLE) {
            const uint32_t var_type = code[i + 1];
            //const uint32_t var_id = code[i + 2];
            const uint32_t var_storage_class = code[i + 3];

            if (var_storage_class != LAHAR_SPV_STORAGE_CLASS_PUSH_CONSTANT) {
                i += word_count;
                continue;
            }

            LaharSPVTypeInfo* push_block_type = __lahar_spv_get_type(info, var_type);
            if (!push_block_type) {
                lahar_trace("Push block references invalid type");
                return LAHAR_ERR_MALFORMED_CODE;
            }

            *saw = true;
            *count_out = push_block_type->tree_size;
        }

        i += word_count;
    }

    return LAHAR_ERR_SUCCESS;
}

static uint32_t __lahar_spv_v1_extract_inputs(const LaharSPVInfo* info, LaharShaderVarInfo* vars, uint32_t* var_count) {
    LAHAR_ASSERT(info && vars && var_count);

    uint32_t err;
    const uint32_t* code = (const uint32_t*)info->stage->code;

    for (uint64_t i = info->section_offsets.types; i < info->section_offsets.functions;) {
        const uint32_t word = code[i];
        const uint16_t opcode = word & 0xFFFF;
        const uint16_t word_count = (uint16_t)(word >> 16);

        if (opcode == LAHAR_SPV_OP_VARIABLE) {
            const uint32_t var_type = code[i + 1];
            const uint32_t var_id = code[i + 2];
            const uint32_t var_storage_class = code[i + 3];

            if (
                var_storage_class == LAHAR_SPV_STORAGE_CLASS_INPUT &&
                !__lahar_spv_v1_var_lookup_is_builtin(info, var_id)
            ) {
                LaharShaderVarInfo* varinfo = &vars[*var_count];
                *var_count = *var_count + 1;
                memset(varinfo, 0, sizeof(*varinfo));

                varinfo->storage_class = LAHAR_SVSC_INPUT;

                if ((err = __lahar_spv_v1_lookup_input_location(info, var_id, &varinfo->location))) {
                    lahar_trace("Failed to extract input location");
                    return err;
                }

                const LaharSPVTypeInfo* type_info = __lahar_spv_get_type(info, var_type);
                varinfo->type = type_info->type;
                varinfo->size = type_info->size_bytes;
                varinfo->stages = info->stage->stage;

                varinfo->child_vars_begin = UINT32_MAX;
                varinfo->child_vars_end = UINT32_MAX;
                varinfo->parent_var = UINT32_MAX;


                const char* name = __lahar_spv_v1_lookup_id_name(info, var_id);

                if (name) {
                    varinfo->path = lahar_temp_strdup(name);
                }
            }
        }

        i += word_count;
    }

    return LAHAR_ERR_SUCCESS;
}

static uint32_t __lahar_spv_v1_extract_subvars(
    LaharSPVInfo* info,
    const LaharSPVTypeInfo* type_info,
    const LaharSPVMemberInfo* member_info,
    LaharShaderVarInfo* var,
    LaharShaderVarInfo* vars, uint32_t* var_count
) {
    uint32_t err = LAHAR_ERR_SUCCESS;

    if (type_info->type == LAHAR_SVT_ARRAY) {
        const uint32_t start = *var_count;
        const uint32_t end = start + 1;

        const LaharSPVTypeInfo* inner_type = __lahar_spv_get_type(info, type_info->component_type);

        if (!inner_type) {
            lahar_trace("While building the vars for a descriptor, found an array that has unknown type");
            return LAHAR_ERR_MALFORMED_CODE;
        }

        LaharShaderVarInfo* child_var = &vars[*var_count];
        *var_count = *var_count + 1;

        memset(child_var, 0, sizeof(*child_var));

        child_var->type = inner_type->type;
        child_var->storage_class = var->storage_class;
        child_var->spv_var_id = UINT32_MAX;
        child_var->spv_type_id = inner_type->id;
        child_var->parent_var = (uint32_t)(var - vars);

        if (inner_type->type == LAHAR_SVT_ARRAY) {
            child_var->size = inner_type->array_stride * inner_type->size_components;
            child_var->stride = inner_type->array_stride;
        }
        else if (__lahar_spv_type_is_matrix(inner_type)) {
            LAHAR_ASSERT(member_info); // This makes no sense, you can't have a mat4[] at the top level

            child_var->size = __lahar_spv_matrix_type_size(info, inner_type, member_info, true);
            child_var->stride = member_info->matrix_stride;
        }
        else {
            child_var->size = __lahar_spv_type_size(info, inner_type, true);
        }

        if (var->path) {
            uint64_t plen = strlen(var->path);
            uint64_t mlen = 3;

            char* path = (char*)lahar_temp_alloc(plen + mlen + 1);
            child_var->path = path;

            memcpy(path, var->path, plen);
            path += plen;

            memcpy(path, "[0]", mlen);
            path += mlen;

            *path = '\0';
        }

        if ((err = __lahar_spv_v1_extract_subvars(info, inner_type, member_info, child_var, vars, var_count))) {
            return err;
        }

        var->child_vars_begin = start;
        var->child_vars_end = end;
    }
    else if (type_info->type == LAHAR_SVT_STRUCT) {
        if (type_info->size_components == 0) { // empty struct case
            var->child_vars_begin = UINT32_MAX;
            var->child_vars_end = UINT32_MAX;
            return LAHAR_ERR_SUCCESS;
        }

        const uint32_t start = *var_count;
        const uint32_t end = start + type_info->size_components;

        *var_count = *var_count + type_info->size_components;

        for (uint32_t i = start; i < end; i++) {
            LaharShaderVarInfo* child_var = &vars[i];
            memset(child_var, 0, sizeof(*child_var));

            const uint32_t member_index = i - start;
            const LaharSPVMemberInfo* current_member_info = &type_info->members[member_index];

            const LaharSPVTypeInfo* member_type = __lahar_spv_get_type(info, current_member_info->id);

            if (!member_type) {
                lahar_trace("While building the vars for a descriptor, found a member that has unknown type");
                return LAHAR_ERR_MALFORMED_CODE;
            }

            child_var->type = member_type->type;
            child_var->storage_class = var->storage_class;
            child_var->spv_var_id = UINT32_MAX;
            child_var->spv_type_id = current_member_info->id;
            child_var->offset = current_member_info->offset;
            child_var->parent_var = (uint32_t)(var - vars);

            if (member_type->type == LAHAR_SVT_ARRAY) {
                child_var->size = member_type->array_stride * member_type->size_components;
                child_var->stride = member_type->array_stride;
            }
            else if (__lahar_spv_type_is_matrix(member_type)) {
                child_var->size = __lahar_spv_matrix_type_size(info, member_type, current_member_info, true);
                child_var->stride = current_member_info->matrix_stride;
            }
            else {
                child_var->size = __lahar_spv_type_size(info, member_type, true);
            }

            if (var->path && current_member_info->name) {
                uint64_t plen = strlen(var->path);
                uint64_t mlen = strlen(current_member_info->name);

                char* path = (char*)lahar_temp_alloc(plen + mlen + 2);
                child_var->path = path;

                memcpy(path, var->path, plen);
                path += plen;

                *path = '.';
                path++;

                memcpy(path, current_member_info->name, mlen);
                path += mlen;

                *path = '\0';
            }
        }

        var->size = __lahar_spv_type_size(info, type_info, true);

        for (uint32_t i = start; i < end; i++) {
            LaharShaderVarInfo* child_var = &vars[i];

            const uint32_t member_index = i - start;
            const LaharSPVMemberInfo* current_member_info = &type_info->members[member_index];

            const LaharSPVTypeInfo* member_type = __lahar_spv_get_type(info, current_member_info->id);

            if ((err = __lahar_spv_v1_extract_subvars(info, member_type, current_member_info, child_var, vars, var_count))) {
                return err;
            }
        }

        var->child_vars_begin = start;
        var->child_vars_end = end;
    }
    // primitive, do nothing

    return LAHAR_ERR_SUCCESS;
}

static uint32_t __lahar_spv_v1_extract_descriptors(LaharSPVInfo* info, LaharShaderVarInfo* vars, uint32_t* var_count) {
    uint32_t err = LAHAR_ERR_SUCCESS;
    const uint32_t* code = (const uint32_t*)info->stage->code;

    for (uint64_t i = info->section_offsets.types; i < info->section_offsets.functions;) {
        const uint32_t word = code[i];
        const uint16_t opcode = word & 0xFFFF;
        const uint16_t word_count = (uint16_t)(word >> 16);

        if (opcode == LAHAR_SPV_OP_VARIABLE) {
            const uint32_t var_type = code[i + 1];
            const uint32_t var_id = code[i + 2];
            const uint32_t var_storage_class = code[i + 3];

            if (
                var_storage_class != LAHAR_SPV_STORAGE_CLASS_UNIFORM &&
                var_storage_class != LAHAR_SPV_STORAGE_CLASS_UNIFORM_CONSTANT &&
                var_storage_class != LAHAR_SPV_STORAGE_CLASS_STORAGE_BUFFER
            ) {
                i += word_count; // not a descriptor
                continue;
            }

            uint32_t set = UINT32_MAX, binding = UINT32_MAX;

            if ((err = __lahar_spv_v1_lookup_set_binding(info, var_id, &set, &binding))) {
                lahar_trace("Failed to find descriptor set/binding");

                if (err == LAHAR_ERR_ID_NOT_FOUND) { return LAHAR_ERR_MALFORMED_CODE; }
                else { return err; }
            }

            LAHAR_ASSERT(set != UINT32_MAX && binding != UINT32_MAX);

            LaharSPVTypeInfo* descriptor_type = __lahar_spv_get_type(info, var_type);
            if (!descriptor_type) {
                lahar_trace("Failed to find descriptor type");
                return LAHAR_ERR_MALFORMED_CODE;
            }

            bool seen = false;

            for (uint32_t j = 0; j < *var_count; j++) {
                LaharShaderVarInfo* desc = &vars[j];

                bool is_uniform = desc->storage_class == LAHAR_SVSC_UNIFORM_BUFFER || desc->storage_class == LAHAR_SVSC_STORAGE_BUFFER;

                if (is_uniform && desc->set == set && desc->binding == binding) {
                    seen = true;

                    #ifdef __cplusplus
                    int new_stages = desc->stages | info->stage->stage;
                    desc->stages = (VkShaderStageFlagBits)new_stages;
                    #else
                    desc->stages |= info->stage->stage;
                    #endif
                }
            }

            if (seen) { i += word_count; continue; }

            LaharShaderVarInfo* descriptor_var = &vars[*var_count];
            *var_count = *var_count + 1;

            memset(descriptor_var, 0, sizeof(*descriptor_var));

            if (var_storage_class == LAHAR_SPV_STORAGE_CLASS_UNIFORM) {
                // have to read the decoration to know if it's an SSBO in older spirv
                // in newer spirv, this deco doesn't exist, so we'll correctly
                // match it by the storage class instead

                if (__lahar_spv_v1_lookup_is_bufferblock(info, descriptor_type->id)) {
                    descriptor_var->storage_class = LAHAR_SVSC_STORAGE_BUFFER;
                }
                else {
                    descriptor_var->storage_class = LAHAR_SVSC_UNIFORM_BUFFER;
                }
            }
            else if (var_storage_class == LAHAR_SPV_STORAGE_CLASS_STORAGE_BUFFER) {
                descriptor_var->storage_class = LAHAR_SVSC_STORAGE_BUFFER;
            }
            else if (var_storage_class == LAHAR_SPV_STORAGE_CLASS_UNIFORM_CONSTANT) {
                descriptor_var->storage_class = LAHAR_SVSC_UNIFORM_CONSTANT;
            }
            else {
                lahar_trace("Unknown storage class %d", var_storage_class);
                return LAHAR_ERR_MALFORMED_CODE;
            }

            descriptor_var->set = set;
            descriptor_var->binding = binding;
            descriptor_var->type = descriptor_type->type;
            descriptor_var->size = descriptor_type->size_bytes;
            descriptor_var->stages = info->stage->stage;

            if (descriptor_type->type == LAHAR_SVT_ARRAY) {
                descriptor_var->array_size = descriptor_type->size_components;
                descriptor_var->stride = descriptor_type->array_stride;
            }

            descriptor_var->spv_type_id = var_type;
            descriptor_var->spv_var_id = var_id;

            const char* name = __lahar_spv_v1_lookup_id_name(info, var_id);

            if (name) {
                descriptor_var->path = lahar_temp_strdup(name);
            }

            if ((err = __lahar_spv_v1_extract_subvars(info, descriptor_type, NULL, descriptor_var, vars, var_count))) {
                return err;
            }

            descriptor_var->parent_var = UINT32_MAX;
        }

        i += word_count;
    }

    return LAHAR_ERR_SUCCESS;
}

static uint32_t __lahar_spv_v1_extract_push_block(LaharSPVInfo* info, LaharShaderVarInfo* vars, uint32_t* var_count) {
    uint32_t err = LAHAR_ERR_SUCCESS;
    const uint32_t* code = (const uint32_t*)info->stage->code;

    for (uint64_t i = info->section_offsets.types; i < info->section_offsets.functions;) {
        const uint32_t word = code[i];
        const uint16_t opcode = word & 0xFFFF;
        const uint16_t word_count = (uint16_t)(word >> 16);

        if (opcode == LAHAR_SPV_OP_VARIABLE) {
            const uint32_t var_type = code[i + 1];
            const uint32_t var_id = code[i + 2];
            const uint32_t var_storage_class = code[i + 3];

            if (var_storage_class != LAHAR_SPV_STORAGE_CLASS_PUSH_CONSTANT) {
                i += word_count;
                continue;
            }

            LaharSPVTypeInfo* push_block_type = __lahar_spv_get_type(info, var_type);
            if (!push_block_type) {
                lahar_trace("Failed to find push block type");
                return LAHAR_ERR_MALFORMED_CODE;
            }

            LaharShaderVarInfo* push_var = &vars[*var_count];
            *var_count = *var_count + 1;

            memset(push_var, 0, sizeof(*push_var));

            push_var->storage_class = LAHAR_SVSC_PUSH_CONSTANT;
            push_var->type = push_block_type->type;
            push_var->size = push_block_type->size_bytes;
            push_var->stages = info->stage->stage;

            if (push_block_type->type == LAHAR_SVT_ARRAY) {
                push_var->array_size = push_block_type->size_components;
                push_var->stride = push_block_type->array_stride;
            }

            push_var->spv_type_id = var_type;
            push_var->spv_var_id = var_id;

            const char* name = __lahar_spv_v1_lookup_id_name(info, var_id);

            if (name) {
                push_var->path = lahar_temp_strdup(name);
            }

            if ((err = __lahar_spv_v1_extract_subvars(info, push_block_type, NULL, push_var, vars, var_count))) {
                return err;
            }

            push_var->parent_var = UINT32_MAX;
        }

        i += word_count;
    }

    return LAHAR_ERR_SUCCESS;
}

static const char* __lahar_shader_stage_name(const LaharShaderStage* stage) {
    const char* stage_name = "Unknown or mixed";

    switch(stage->stage) {
        case VK_SHADER_STAGE_VERTEX_BIT: stage_name = "vertex"; break;
        case VK_SHADER_STAGE_FRAGMENT_BIT: stage_name = "fragment"; break;
        case VK_SHADER_STAGE_GEOMETRY_BIT: stage_name = "geometry"; break;
        case VK_SHADER_STAGE_COMPUTE_BIT: stage_name = "compute"; break;
        case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT: stage_name = "tesselation control"; break;
        case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: stage_name = "tesselation evaluation"; break;
        default: break;
    }

    return stage_name;
}

static void __lahar_shader_introspection_print_recurse(const LaharShaderVarInfo* infos, uint32_t count, uint32_t index, uint32_t depth) {
    const LaharShaderVarInfo* info = &infos[index];
    const char* type_name = lahar_shader_var_type_string(info->type);

    if (depth > 0) {
        for (uint32_t i = 0; i < depth - 1; i++) {
            printf("│  ");
        }

        printf("├─ ");
    }

    const char* var_name = "(unknown)";

    if (info->path) {
        const char* start = info->path;
        const char* head = start;

        while (*head) { head++; }
        while(head != start && *head != '.') { head--; }

        if (*head == '.') { head++; }

        var_name = head;
    }

    printf("\033[1m%s\033[0m", var_name);

    if (info->type == LAHAR_SVT_ARRAY) {
        if (info->array_size == 0) {
            printf("[]");
        } else {
            printf("[%d]", info->array_size);
        }
    }

    printf(": \033[36m%s\033[0m", type_name + 10);

    if (info->size > 0) {
        printf(" \033[90m(%d bytes)\033[0m", info->size);
    }

    if (depth == 0) {
        //printf("\n");
        //for (uint32_t i = 0; i < depth; i++) { printf("│  "); }
        //printf("  ");

        switch (info->storage_class) {
            case LAHAR_SVSC_UNIFORM_BUFFER:
                printf(" \033[33m▸ UBO\033[0m @ Set %d, Binding %d", info->set, info->binding);
                break;
            case LAHAR_SVSC_STORAGE_BUFFER:
                printf(" \033[33m▸ SSBO\033[0m @ Set %d, Binding %d", info->set, info->binding);
                break;
            case LAHAR_SVSC_INPUT:
                printf(" \033[33m▸ Input\033[0m @ Location %d", info->location);
                break;
            case LAHAR_SVSC_PUSH_CONSTANT:
                printf(" \033[33m▸ Push Constant\033[0m");
                break;
            case LAHAR_SVSC_UNIFORM_CONSTANT:
                printf(" \033[33m▸ Uniform Constant\033[0m");
                break;
            default:
                break;
        }
    }
    else {
        const LaharShaderVarInfo* parent_var = info->parent_var != UINT32_MAX ? &infos[info->parent_var] : NULL;

        if (parent_var == NULL || parent_var->type != LAHAR_SVT_ARRAY) {
            printf(" \033[90m@ offset %d\033[0m", info->offset);
        }
    }

    if (info->type == LAHAR_SVT_ARRAY && info->stride != 0) {
        printf(" \033[90mstride %d\033[0m", info->stride);
    }

    printf("\n");

    if (info->child_vars_begin != UINT32_MAX) {
        for (uint32_t i = info->child_vars_begin; i < info->child_vars_end; i++) {
            __lahar_shader_introspection_print_recurse(infos, count, i, depth + 1);
        }
    }

    /* dunno if I like that or not */
    if (depth == 0) {
        printf("\n");
    }

}

void lahar_shader_introspection_print(const LaharShaderVarInfo* infos, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        const LaharShaderVarInfo* info = &infos[i];

        if (info->parent_var != UINT32_MAX) { continue; } // skip children, let the recurse print them

        __lahar_shader_introspection_print_recurse(infos, count, i, 0);
    }
}

uint32_t lahar_shader_introspect(
    const LaharShaderStage* stages,
    uint32_t num_stages,
    uint32_t* num_infos,
    LaharShaderVarInfo* infos
) {
    lahar_temp_mcheck();

    uint32_t err = LAHAR_ERR_SUCCESS;

    uint32_t produced_info_count = 0;
    LaharSPVInfo* stage_infos = NULL;

    uint32_t input_slot_count = 0;
    uint32_t push_slot_count = 0;

    uint32_t descriptor_count = 0;
    LaharSPVDescriptorSize* unique_descriptors = NULL;

    uint32_t push_block_stage = UINT32_MAX;
    uint32_t push_block_stages = 0;     // union of the stages that declare a push block

    uint32_t total_slots = 0;

    for (uint32_t i = 0; i < num_stages; i++) {
        const LaharShaderStage* stage = &stages[i];

        if (stage->lang && strcmp(stage->lang, "spv") != 0 && strcmp(stage->lang, "spirv") != 0) {
            lahar_trace("Shader stage was not SPIR-V");
            err = LAHAR_ERR_UNKNOWN_LANGUAGE;
            goto cleanup;
        }
    }

    stage_infos = (LaharSPVInfo*)lahar_temp_alloc(sizeof(*stage_infos) * num_stages);

    for (uint32_t i = 0; i < num_stages; i++) {
        LaharSPVInfo* info = &stage_infos[i];
        info->stage = &stages[i];

        const LaharShaderStage* stage = &stages[i];
        lahar_trace("Doing first pass of %s stage", __lahar_shader_stage_name(stage));

        // 5 words of header minimum, and whole words only; everything
        // downstream indexes this as a uint32 stream and trusts length/4
        if (!stage->code || stage->length % 4 != 0 || stage->length / 4 < 5) {
            lahar_trace("Shader stage code length is not valid SPIR-V");
            err = LAHAR_ERR_MALFORMED_CODE;
            goto cleanup;
        }

        if (__lahar_shader_stage_validate_spv_header((const uint32_t*)stage->code) != LAHAR_ERR_SUCCESS) {
            lahar_trace("Shader stage had invalid SPIR-V header");
            err = LAHAR_ERR_MALFORMED_CODE;
            goto cleanup;
        }

        if ((err = __lahar_spv_v1_build_section_offsets(info))) {
            lahar_trace("Failed to build offset table");
            goto cleanup;
        }

        if ((err =  __lahar_spv_v1_count_types(info))) {
            lahar_trace("Failed to count types");
            goto cleanup;
        }

        if ((err = __lahar_spv_v1_extract_types(info))) {
            lahar_trace("Failed to extract types");
            goto cleanup;
        }
    }


    for (uint32_t i = 0; i < num_stages; i++) {
        LaharSPVInfo* info = &stage_infos[i];
        lahar_trace("Doing second pass of %s stage", __lahar_shader_stage_name(info->stage));

        if (info->stage->stage & VK_SHADER_STAGE_VERTEX_BIT) {
            __lahar_spv_v1_count_input_slots(info, &input_slot_count);
        }

        err = __lahar_spv_v1_count_unique_descriptor_slots(
            info,
            &descriptor_count,
            &unique_descriptors
        );

        if (err) {
            lahar_trace("Failed to count unique descriptor slots");
            goto cleanup;
        }

        // Every stage has to be scanned, not just the first one that has a
        // block: a block read by both vertex and fragment needs a range whose
        // stageFlags covers both, or vkCmdPushConstants trips validation.
        {
            bool saw = false;
            uint32_t stage_push_slots = 0;

            if ((err = __lahar_spv_v1_count_push_contant_slots(info, &stage_push_slots, &saw))) {
                lahar_trace("Failed to count push block slots");
                goto cleanup;
            }

            if (saw) {
                push_block_stages |= (uint32_t)info->stage->stage;

                // Only one block is ever emitted, so reserve room for the
                // largest one seen in case the stages disagree in shape.
                if (stage_push_slots > push_slot_count) {
                    push_slot_count = stage_push_slots;
                    push_block_stage = i;
                }
                else if (push_block_stage == UINT32_MAX) {
                    push_block_stage = i;
                }
            }
        }
    }

    lahar_trace("Counted %u input slots, %u descriptors, push block slots: %u", input_slot_count, descriptor_count, push_slot_count);

    total_slots = input_slot_count + push_slot_count;

    for (uint32_t i = 0; i < descriptor_count; i++) {
        lahar_trace("    Descriptor set %u binding %u, %u slots", unique_descriptors[i].set, unique_descriptors[i].binding, unique_descriptors[i].tree_size);
        total_slots += unique_descriptors[i].tree_size;
    }

    if (!infos) {
        *num_infos = total_slots;
        goto cleanup;
    }

    // Do two separate loops to ENSURE
    // that the inputs are first
    for (uint32_t i = 0; i < num_stages; i++) {
        LaharSPVInfo* info = &stage_infos[i];

        if (info->stage->stage & VK_SHADER_STAGE_VERTEX_BIT) {
            lahar_trace("Reading vertex inputs");
            if ((err = __lahar_spv_v1_extract_inputs(info, infos, &produced_info_count))) {
                lahar_trace("Failed to read inputs");
                goto cleanup;
            }

            break;
        }
    }

    for (uint32_t i = 0; i < num_stages; i++) {
        LaharSPVInfo* info = &stage_infos[i];

        if ((err = __lahar_spv_v1_extract_descriptors(info, infos, &produced_info_count))) {
            lahar_trace("Failed to extract descriptors from %s stage", __lahar_shader_stage_name(info->stage));
            goto cleanup;
        }
    }

    if (push_block_stage != UINT32_MAX) {
        LaharSPVInfo* info = &stage_infos[push_block_stage];
        const uint32_t first = produced_info_count;

        if ((err = __lahar_spv_v1_extract_push_block(info, infos, &produced_info_count))) {
            lahar_trace("Failed to extract push descriptor block");
            goto cleanup;
        }

        // Retag the block (and its whole subtree) with every stage that reads
        // it, so the generated push constant range covers them all.
        for (uint32_t i = first; i < produced_info_count; i++) {
            #ifdef __cplusplus
            infos[i].stages = (VkShaderStageFlagBits)push_block_stages;
            #else
            infos[i].stages = push_block_stages;
            #endif
        }
    }

    if (num_infos) {
        *num_infos = produced_info_count;
    }

cleanup:
    LAHAR_FATAL_ASSERT(produced_info_count <= total_slots); // Very bad memory nono

    lahar_temp_mpop();
    return err;
}

const char* lahar_shader_var_type_string(LaharShaderVarType svt) {
    switch (svt) {
        default:
        case LAHAR_SVT_UNKNOWN: return "LAHAR_SVT_UNKNOWN";
        case LAHAR_SVT_VOID: return "LAHAR_SVT_VOID";
        case LAHAR_SVT_BOOL: return "LAHAR_SVT_BOOL";
        case LAHAR_SVT_INT: return "LAHAR_SVT_INT";
        case LAHAR_SVT_UINT: return "LAHAR_SVT_UINT";
        case LAHAR_SVT_FLOAT: return "LAHAR_SVT_FLOAT";
        case LAHAR_SVT_DOUBLE: return "LAHAR_SVT_DOUBLE";
        case LAHAR_SVT_BVEC2: return "LAHAR_SVT_BVEC2";
        case LAHAR_SVT_BVEC3: return "LAHAR_SVT_BVEC3";
        case LAHAR_SVT_BVEC4: return "LAHAR_SVT_BVEC4";
        case LAHAR_SVT_IVEC2: return "LAHAR_SVT_IVEC2";
        case LAHAR_SVT_IVEC3: return "LAHAR_SVT_IVEC3";
        case LAHAR_SVT_IVEC4: return "LAHAR_SVT_IVEC4";
        case LAHAR_SVT_UVEC2: return "LAHAR_SVT_UVEC2";
        case LAHAR_SVT_UVEC3: return "LAHAR_SVT_UVEC3";
        case LAHAR_SVT_UVEC4: return "LAHAR_SVT_UVEC4";
        case LAHAR_SVT_DVEC2: return "LAHAR_SVT_DVEC2";
        case LAHAR_SVT_DVEC3: return "LAHAR_SVT_DVEC3";
        case LAHAR_SVT_DVEC4: return "LAHAR_SVT_DVEC4";
        case LAHAR_SVT_VEC2: return "LAHAR_SVT_VEC2";
        case LAHAR_SVT_VEC3: return "LAHAR_SVT_VEC3";
        case LAHAR_SVT_VEC4: return "LAHAR_SVT_VEC4";
        case LAHAR_SVT_MAT2X3: return "LAHAR_SVT_MAT2X3";
        case LAHAR_SVT_MAT2X4: return "LAHAR_SVT_MAT2X4";
        case LAHAR_SVT_MAT3X2: return "LAHAR_SVT_MAT3X2";
        case LAHAR_SVT_MAT3X4: return "LAHAR_SVT_MAT3X4";
        case LAHAR_SVT_MAT4X2: return "LAHAR_SVT_MAT4X2";
        case LAHAR_SVT_MAT4X3: return "LAHAR_SVT_MAT4X3";
        case LAHAR_SVT_MAT2: return "LAHAR_SVT_MAT2";
        case LAHAR_SVT_MAT3: return "LAHAR_SVT_MAT3";
        case LAHAR_SVT_MAT4: return "LAHAR_SVT_MAT4";
        case LAHAR_SVT_DMAT2X3: return "LAHAR_SVT_DMAT2X3";
        case LAHAR_SVT_DMAT2X4: return "LAHAR_SVT_DMAT2X4";
        case LAHAR_SVT_DMAT3X2: return "LAHAR_SVT_DMAT3X2";
        case LAHAR_SVT_DMAT3X4: return "LAHAR_SVT_DMAT3X4";
        case LAHAR_SVT_DMAT4X2: return "LAHAR_SVT_DMAT4X2";
        case LAHAR_SVT_DMAT4X3: return "LAHAR_SVT_DMAT4X3";
        case LAHAR_SVT_DMAT2: return "LAHAR_SVT_DMAT2";
        case LAHAR_SVT_DMAT3: return "LAHAR_SVT_DMAT3";
        case LAHAR_SVT_DMAT4: return "LAHAR_SVT_DMAT4";
        case LAHAR_SVT_SAMPLER: return "LAHAR_SVT_SAMPLER";
        case LAHAR_SVT_IMAGE: return "LAHAR_SVT_IMAGE";
        case LAHAR_SVT_SAMPLER_IMAGE: return "LAHAR_SVT_SAMPLER_IMAGE";
        case LAHAR_SVT_STRUCT: return "LAHAR_SVT_STRUCT";
        case LAHAR_SVT_ARRAY: return "LAHAR_SVT_ARRAY";
        case LAHAR_SVT_POINTER: return "LAHAR_SVT_POINTER";
    }
}

const char* lahar_vkformat_string(VkFormat format) {
   switch (format) {
        case VK_FORMAT_UNDEFINED: return "VK_FORMAT_UNDEFINED";
        case VK_FORMAT_R4G4_UNORM_PACK8: return "VK_FORMAT_R4G4_UNORM_PACK8";
        case VK_FORMAT_R4G4B4A4_UNORM_PACK16: return "VK_FORMAT_R4G4B4A4_UNORM_PACK16";
        case VK_FORMAT_B4G4R4A4_UNORM_PACK16: return "VK_FORMAT_B4G4R4A4_UNORM_PACK16";
        case VK_FORMAT_R5G6B5_UNORM_PACK16: return "VK_FORMAT_R5G6B5_UNORM_PACK16";
        case VK_FORMAT_B5G6R5_UNORM_PACK16: return "VK_FORMAT_B5G6R5_UNORM_PACK16";
        case VK_FORMAT_R5G5B5A1_UNORM_PACK16: return "VK_FORMAT_R5G5B5A1_UNORM_PACK16";
        case VK_FORMAT_B5G5R5A1_UNORM_PACK16: return "VK_FORMAT_B5G5R5A1_UNORM_PACK16";
        case VK_FORMAT_A1R5G5B5_UNORM_PACK16: return "VK_FORMAT_A1R5G5B5_UNORM_PACK16";
        case VK_FORMAT_R8_UNORM: return "VK_FORMAT_R8_UNORM";
        case VK_FORMAT_R8_SNORM: return "VK_FORMAT_R8_SNORM";
        case VK_FORMAT_R8_USCALED: return "VK_FORMAT_R8_USCALED";
        case VK_FORMAT_R8_SSCALED: return "VK_FORMAT_R8_SSCALED";
        case VK_FORMAT_R8_UINT: return "VK_FORMAT_R8_UINT";
        case VK_FORMAT_R8_SINT: return "VK_FORMAT_R8_SINT";
        case VK_FORMAT_R8_SRGB: return "VK_FORMAT_R8_SRGB";
        case VK_FORMAT_R8G8_UNORM: return "VK_FORMAT_R8G8_UNORM";
        case VK_FORMAT_R8G8_SNORM: return "VK_FORMAT_R8G8_SNORM";
        case VK_FORMAT_R8G8_USCALED: return "VK_FORMAT_R8G8_USCALED";
        case VK_FORMAT_R8G8_SSCALED: return "VK_FORMAT_R8G8_SSCALED";
        case VK_FORMAT_R8G8_UINT: return "VK_FORMAT_R8G8_UINT";
        case VK_FORMAT_R8G8_SINT: return "VK_FORMAT_R8G8_SINT";
        case VK_FORMAT_R8G8_SRGB: return "VK_FORMAT_R8G8_SRGB";
        case VK_FORMAT_R8G8B8_UNORM: return "VK_FORMAT_R8G8B8_UNORM";
        case VK_FORMAT_R8G8B8_SNORM: return "VK_FORMAT_R8G8B8_SNORM";
        case VK_FORMAT_R8G8B8_USCALED: return "VK_FORMAT_R8G8B8_USCALED";
        case VK_FORMAT_R8G8B8_SSCALED: return "VK_FORMAT_R8G8B8_SSCALED";
        case VK_FORMAT_R8G8B8_UINT: return "VK_FORMAT_R8G8B8_UINT";
        case VK_FORMAT_R8G8B8_SINT: return "VK_FORMAT_R8G8B8_SINT";
        case VK_FORMAT_R8G8B8_SRGB: return "VK_FORMAT_R8G8B8_SRGB";
        case VK_FORMAT_B8G8R8_UNORM: return "VK_FORMAT_B8G8R8_UNORM";
        case VK_FORMAT_B8G8R8_SNORM: return "VK_FORMAT_B8G8R8_SNORM";
        case VK_FORMAT_B8G8R8_USCALED: return "VK_FORMAT_B8G8R8_USCALED";
        case VK_FORMAT_B8G8R8_SSCALED: return "VK_FORMAT_B8G8R8_SSCALED";
        case VK_FORMAT_B8G8R8_UINT: return "VK_FORMAT_B8G8R8_UINT";
        case VK_FORMAT_B8G8R8_SINT: return "VK_FORMAT_B8G8R8_SINT";
        case VK_FORMAT_B8G8R8_SRGB: return "VK_FORMAT_B8G8R8_SRGB";
        case VK_FORMAT_R8G8B8A8_UNORM: return "VK_FORMAT_R8G8B8A8_UNORM";
        case VK_FORMAT_R8G8B8A8_SNORM: return "VK_FORMAT_R8G8B8A8_SNORM";
        case VK_FORMAT_R8G8B8A8_USCALED: return "VK_FORMAT_R8G8B8A8_USCALED";
        case VK_FORMAT_R8G8B8A8_SSCALED: return "VK_FORMAT_R8G8B8A8_SSCALED";
        case VK_FORMAT_R8G8B8A8_UINT: return "VK_FORMAT_R8G8B8A8_UINT";
        case VK_FORMAT_R8G8B8A8_SINT: return "VK_FORMAT_R8G8B8A8_SINT";
        case VK_FORMAT_R8G8B8A8_SRGB: return "VK_FORMAT_R8G8B8A8_SRGB";
        case VK_FORMAT_B8G8R8A8_UNORM: return "VK_FORMAT_B8G8R8A8_UNORM";
        case VK_FORMAT_B8G8R8A8_SNORM: return "VK_FORMAT_B8G8R8A8_SNORM";
        case VK_FORMAT_B8G8R8A8_USCALED: return "VK_FORMAT_B8G8R8A8_USCALED";
        case VK_FORMAT_B8G8R8A8_SSCALED: return "VK_FORMAT_B8G8R8A8_SSCALED";
        case VK_FORMAT_B8G8R8A8_UINT: return "VK_FORMAT_B8G8R8A8_UINT";
        case VK_FORMAT_B8G8R8A8_SINT: return "VK_FORMAT_B8G8R8A8_SINT";
        case VK_FORMAT_B8G8R8A8_SRGB: return "VK_FORMAT_B8G8R8A8_SRGB";
        case VK_FORMAT_A8B8G8R8_UNORM_PACK32: return "VK_FORMAT_A8B8G8R8_UNORM_PACK32";
        case VK_FORMAT_A8B8G8R8_SNORM_PACK32: return "VK_FORMAT_A8B8G8R8_SNORM_PACK32";
        case VK_FORMAT_A8B8G8R8_USCALED_PACK32: return "VK_FORMAT_A8B8G8R8_USCALED_PACK32";
        case VK_FORMAT_A8B8G8R8_SSCALED_PACK32: return "VK_FORMAT_A8B8G8R8_SSCALED_PACK32";
        case VK_FORMAT_A8B8G8R8_UINT_PACK32: return "VK_FORMAT_A8B8G8R8_UINT_PACK32";
        case VK_FORMAT_A8B8G8R8_SINT_PACK32: return "VK_FORMAT_A8B8G8R8_SINT_PACK32";
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32: return "VK_FORMAT_A8B8G8R8_SRGB_PACK32";
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32: return "VK_FORMAT_A2R10G10B10_UNORM_PACK32";
        case VK_FORMAT_A2R10G10B10_SNORM_PACK32: return "VK_FORMAT_A2R10G10B10_SNORM_PACK32";
        case VK_FORMAT_A2R10G10B10_USCALED_PACK32: return "VK_FORMAT_A2R10G10B10_USCALED_PACK32";
        case VK_FORMAT_A2R10G10B10_SSCALED_PACK32: return "VK_FORMAT_A2R10G10B10_SSCALED_PACK32";
        case VK_FORMAT_A2R10G10B10_UINT_PACK32: return "VK_FORMAT_A2R10G10B10_UINT_PACK32";
        case VK_FORMAT_A2R10G10B10_SINT_PACK32: return "VK_FORMAT_A2R10G10B10_SINT_PACK32";
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return "VK_FORMAT_A2B10G10R10_UNORM_PACK32";
        case VK_FORMAT_A2B10G10R10_SNORM_PACK32: return "VK_FORMAT_A2B10G10R10_SNORM_PACK32";
        case VK_FORMAT_A2B10G10R10_USCALED_PACK32: return "VK_FORMAT_A2B10G10R10_USCALED_PACK32";
        case VK_FORMAT_A2B10G10R10_SSCALED_PACK32: return "VK_FORMAT_A2B10G10R10_SSCALED_PACK32";
        case VK_FORMAT_A2B10G10R10_UINT_PACK32: return "VK_FORMAT_A2B10G10R10_UINT_PACK32";
        case VK_FORMAT_A2B10G10R10_SINT_PACK32: return "VK_FORMAT_A2B10G10R10_SINT_PACK32";
        case VK_FORMAT_R16_UNORM: return "VK_FORMAT_R16_UNORM";
        case VK_FORMAT_R16_SNORM: return "VK_FORMAT_R16_SNORM";
        case VK_FORMAT_R16_USCALED: return "VK_FORMAT_R16_USCALED";
        case VK_FORMAT_R16_SSCALED: return "VK_FORMAT_R16_SSCALED";
        case VK_FORMAT_R16_UINT: return "VK_FORMAT_R16_UINT";
        case VK_FORMAT_R16_SINT: return "VK_FORMAT_R16_SINT";
        case VK_FORMAT_R16_SFLOAT: return "VK_FORMAT_R16_SFLOAT";
        case VK_FORMAT_R16G16_UNORM: return "VK_FORMAT_R16G16_UNORM";
        case VK_FORMAT_R16G16_SNORM: return "VK_FORMAT_R16G16_SNORM";
        case VK_FORMAT_R16G16_USCALED: return "VK_FORMAT_R16G16_USCALED";
        case VK_FORMAT_R16G16_SSCALED: return "VK_FORMAT_R16G16_SSCALED";
        case VK_FORMAT_R16G16_UINT: return "VK_FORMAT_R16G16_UINT";
        case VK_FORMAT_R16G16_SINT: return "VK_FORMAT_R16G16_SINT";
        case VK_FORMAT_R16G16_SFLOAT: return "VK_FORMAT_R16G16_SFLOAT";
        case VK_FORMAT_R16G16B16_UNORM: return "VK_FORMAT_R16G16B16_UNORM";
        case VK_FORMAT_R16G16B16_SNORM: return "VK_FORMAT_R16G16B16_SNORM";
        case VK_FORMAT_R16G16B16_USCALED: return "VK_FORMAT_R16G16B16_USCALED";
        case VK_FORMAT_R16G16B16_SSCALED: return "VK_FORMAT_R16G16B16_SSCALED";
        case VK_FORMAT_R16G16B16_UINT: return "VK_FORMAT_R16G16B16_UINT";
        case VK_FORMAT_R16G16B16_SINT: return "VK_FORMAT_R16G16B16_SINT";
        case VK_FORMAT_R16G16B16_SFLOAT: return "VK_FORMAT_R16G16B16_SFLOAT";
        case VK_FORMAT_R16G16B16A16_UNORM: return "VK_FORMAT_R16G16B16A16_UNORM";
        case VK_FORMAT_R16G16B16A16_SNORM: return "VK_FORMAT_R16G16B16A16_SNORM";
        case VK_FORMAT_R16G16B16A16_USCALED: return "VK_FORMAT_R16G16B16A16_USCALED";
        case VK_FORMAT_R16G16B16A16_SSCALED: return "VK_FORMAT_R16G16B16A16_SSCALED";
        case VK_FORMAT_R16G16B16A16_UINT: return "VK_FORMAT_R16G16B16A16_UINT";
        case VK_FORMAT_R16G16B16A16_SINT: return "VK_FORMAT_R16G16B16A16_SINT";
        case VK_FORMAT_R16G16B16A16_SFLOAT: return "VK_FORMAT_R16G16B16A16_SFLOAT";
        case VK_FORMAT_R32_UINT: return "VK_FORMAT_R32_UINT";
        case VK_FORMAT_R32_SINT: return "VK_FORMAT_R32_SINT";
        case VK_FORMAT_R32_SFLOAT: return "VK_FORMAT_R32_SFLOAT";
        case VK_FORMAT_R32G32_UINT: return "VK_FORMAT_R32G32_UINT";
        case VK_FORMAT_R32G32_SINT: return "VK_FORMAT_R32G32_SINT";
        case VK_FORMAT_R32G32_SFLOAT: return "VK_FORMAT_R32G32_SFLOAT";
        case VK_FORMAT_R32G32B32_UINT: return "VK_FORMAT_R32G32B32_UINT";
        case VK_FORMAT_R32G32B32_SINT: return "VK_FORMAT_R32G32B32_SINT";
        case VK_FORMAT_R32G32B32_SFLOAT: return "VK_FORMAT_R32G32B32_SFLOAT";
        case VK_FORMAT_R32G32B32A32_UINT: return "VK_FORMAT_R32G32B32A32_UINT";
        case VK_FORMAT_R32G32B32A32_SINT: return "VK_FORMAT_R32G32B32A32_SINT";
        case VK_FORMAT_R32G32B32A32_SFLOAT: return "VK_FORMAT_R32G32B32A32_SFLOAT";
        case VK_FORMAT_R64_UINT: return "VK_FORMAT_R64_UINT";
        case VK_FORMAT_R64_SINT: return "VK_FORMAT_R64_SINT";
        case VK_FORMAT_R64_SFLOAT: return "VK_FORMAT_R64_SFLOAT";
        case VK_FORMAT_R64G64_UINT: return "VK_FORMAT_R64G64_UINT";
        case VK_FORMAT_R64G64_SINT: return "VK_FORMAT_R64G64_SINT";
        case VK_FORMAT_R64G64_SFLOAT: return "VK_FORMAT_R64G64_SFLOAT";
        case VK_FORMAT_R64G64B64_UINT: return "VK_FORMAT_R64G64B64_UINT";
        case VK_FORMAT_R64G64B64_SINT: return "VK_FORMAT_R64G64B64_SINT";
        case VK_FORMAT_R64G64B64_SFLOAT: return "VK_FORMAT_R64G64B64_SFLOAT";
        case VK_FORMAT_R64G64B64A64_UINT: return "VK_FORMAT_R64G64B64A64_UINT";
        case VK_FORMAT_R64G64B64A64_SINT: return "VK_FORMAT_R64G64B64A64_SINT";
        case VK_FORMAT_R64G64B64A64_SFLOAT: return "VK_FORMAT_R64G64B64A64_SFLOAT";
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32: return "VK_FORMAT_B10G11R11_UFLOAT_PACK32";
        case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: return "VK_FORMAT_E5B9G9R9_UFLOAT_PACK32";
        case VK_FORMAT_D16_UNORM: return "VK_FORMAT_D16_UNORM";
        case VK_FORMAT_X8_D24_UNORM_PACK32: return "VK_FORMAT_X8_D24_UNORM_PACK32";
        case VK_FORMAT_D32_SFLOAT: return "VK_FORMAT_D32_SFLOAT";
        case VK_FORMAT_S8_UINT: return "VK_FORMAT_S8_UINT";
        case VK_FORMAT_D16_UNORM_S8_UINT: return "VK_FORMAT_D16_UNORM_S8_UINT";
        case VK_FORMAT_D24_UNORM_S8_UINT: return "VK_FORMAT_D24_UNORM_S8_UINT";
        case VK_FORMAT_D32_SFLOAT_S8_UINT: return "VK_FORMAT_D32_SFLOAT_S8_UINT";
        case VK_FORMAT_BC1_RGB_UNORM_BLOCK: return "VK_FORMAT_BC1_RGB_UNORM_BLOCK";
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK: return "VK_FORMAT_BC1_RGB_SRGB_BLOCK";
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK: return "VK_FORMAT_BC1_RGBA_UNORM_BLOCK";
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: return "VK_FORMAT_BC1_RGBA_SRGB_BLOCK";
        case VK_FORMAT_BC2_UNORM_BLOCK: return "VK_FORMAT_BC2_UNORM_BLOCK";
        case VK_FORMAT_BC2_SRGB_BLOCK: return "VK_FORMAT_BC2_SRGB_BLOCK";
        case VK_FORMAT_BC3_UNORM_BLOCK: return "VK_FORMAT_BC3_UNORM_BLOCK";
        case VK_FORMAT_BC3_SRGB_BLOCK: return "VK_FORMAT_BC3_SRGB_BLOCK";
        case VK_FORMAT_BC4_UNORM_BLOCK: return "VK_FORMAT_BC4_UNORM_BLOCK";
        case VK_FORMAT_BC4_SNORM_BLOCK: return "VK_FORMAT_BC4_SNORM_BLOCK";
        case VK_FORMAT_BC5_UNORM_BLOCK: return "VK_FORMAT_BC5_UNORM_BLOCK";
        case VK_FORMAT_BC5_SNORM_BLOCK: return "VK_FORMAT_BC5_SNORM_BLOCK";
        case VK_FORMAT_BC6H_UFLOAT_BLOCK: return "VK_FORMAT_BC6H_UFLOAT_BLOCK";
        case VK_FORMAT_BC6H_SFLOAT_BLOCK: return "VK_FORMAT_BC6H_SFLOAT_BLOCK";
        case VK_FORMAT_BC7_UNORM_BLOCK: return "VK_FORMAT_BC7_UNORM_BLOCK";
        case VK_FORMAT_BC7_SRGB_BLOCK: return "VK_FORMAT_BC7_SRGB_BLOCK";
        case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK: return "VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK";
        case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK: return "VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK";
        case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK: return "VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK";
        case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK: return "VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK";
        case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK: return "VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK";
        case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK: return "VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK";
        case VK_FORMAT_EAC_R11_UNORM_BLOCK: return "VK_FORMAT_EAC_R11_UNORM_BLOCK";
        case VK_FORMAT_EAC_R11_SNORM_BLOCK: return "VK_FORMAT_EAC_R11_SNORM_BLOCK";
        case VK_FORMAT_EAC_R11G11_UNORM_BLOCK: return "VK_FORMAT_EAC_R11G11_UNORM_BLOCK";
        case VK_FORMAT_EAC_R11G11_SNORM_BLOCK: return "VK_FORMAT_EAC_R11G11_SNORM_BLOCK";
        case VK_FORMAT_ASTC_4x4_UNORM_BLOCK: return "VK_FORMAT_ASTC_4x4_UNORM_BLOCK";
        case VK_FORMAT_ASTC_4x4_SRGB_BLOCK: return "VK_FORMAT_ASTC_4x4_SRGB_BLOCK";
        case VK_FORMAT_ASTC_5x4_UNORM_BLOCK: return "VK_FORMAT_ASTC_5x4_UNORM_BLOCK";
        case VK_FORMAT_ASTC_5x4_SRGB_BLOCK: return "VK_FORMAT_ASTC_5x4_SRGB_BLOCK";
        case VK_FORMAT_ASTC_5x5_UNORM_BLOCK: return "VK_FORMAT_ASTC_5x5_UNORM_BLOCK";
        case VK_FORMAT_ASTC_5x5_SRGB_BLOCK: return "VK_FORMAT_ASTC_5x5_SRGB_BLOCK";
        case VK_FORMAT_ASTC_6x5_UNORM_BLOCK: return "VK_FORMAT_ASTC_6x5_UNORM_BLOCK";
        case VK_FORMAT_ASTC_6x5_SRGB_BLOCK: return "VK_FORMAT_ASTC_6x5_SRGB_BLOCK";
        case VK_FORMAT_ASTC_6x6_UNORM_BLOCK: return "VK_FORMAT_ASTC_6x6_UNORM_BLOCK";
        case VK_FORMAT_ASTC_6x6_SRGB_BLOCK: return "VK_FORMAT_ASTC_6x6_SRGB_BLOCK";
        case VK_FORMAT_ASTC_8x5_UNORM_BLOCK: return "VK_FORMAT_ASTC_8x5_UNORM_BLOCK";
        case VK_FORMAT_ASTC_8x5_SRGB_BLOCK: return "VK_FORMAT_ASTC_8x5_SRGB_BLOCK";
        case VK_FORMAT_ASTC_8x6_UNORM_BLOCK: return "VK_FORMAT_ASTC_8x6_UNORM_BLOCK";
        case VK_FORMAT_ASTC_8x6_SRGB_BLOCK: return "VK_FORMAT_ASTC_8x6_SRGB_BLOCK";
        case VK_FORMAT_ASTC_8x8_UNORM_BLOCK: return "VK_FORMAT_ASTC_8x8_UNORM_BLOCK";
        case VK_FORMAT_ASTC_8x8_SRGB_BLOCK: return "VK_FORMAT_ASTC_8x8_SRGB_BLOCK";
        case VK_FORMAT_ASTC_10x5_UNORM_BLOCK: return "VK_FORMAT_ASTC_10x5_UNORM_BLOCK";
        case VK_FORMAT_ASTC_10x5_SRGB_BLOCK: return "VK_FORMAT_ASTC_10x5_SRGB_BLOCK";
        case VK_FORMAT_ASTC_10x6_UNORM_BLOCK: return "VK_FORMAT_ASTC_10x6_UNORM_BLOCK";
        case VK_FORMAT_ASTC_10x6_SRGB_BLOCK: return "VK_FORMAT_ASTC_10x6_SRGB_BLOCK";
        case VK_FORMAT_ASTC_10x8_UNORM_BLOCK: return "VK_FORMAT_ASTC_10x8_UNORM_BLOCK";
        case VK_FORMAT_ASTC_10x8_SRGB_BLOCK: return "VK_FORMAT_ASTC_10x8_SRGB_BLOCK";
        case VK_FORMAT_ASTC_10x10_UNORM_BLOCK: return "VK_FORMAT_ASTC_10x10_UNORM_BLOCK";
        case VK_FORMAT_ASTC_10x10_SRGB_BLOCK: return "VK_FORMAT_ASTC_10x10_SRGB_BLOCK";
        case VK_FORMAT_ASTC_12x10_UNORM_BLOCK: return "VK_FORMAT_ASTC_12x10_UNORM_BLOCK";
        case VK_FORMAT_ASTC_12x10_SRGB_BLOCK: return "VK_FORMAT_ASTC_12x10_SRGB_BLOCK";
        case VK_FORMAT_ASTC_12x12_UNORM_BLOCK: return "VK_FORMAT_ASTC_12x12_UNORM_BLOCK";
        case VK_FORMAT_ASTC_12x12_SRGB_BLOCK: return "VK_FORMAT_ASTC_12x12_SRGB_BLOCK";

    #if defined(VK_VERSION_1_1)
        /* Core Vulkan 1.1 */
        case VK_FORMAT_G8B8G8R8_422_UNORM: return "VK_FORMAT_G8B8G8R8_422_UNORM";
        case VK_FORMAT_B8G8R8G8_422_UNORM: return "VK_FORMAT_B8G8R8G8_422_UNORM";
        case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM: return "VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM";
        case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM: return "VK_FORMAT_G8_B8R8_2PLANE_420_UNORM";
        case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM: return "VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM";
        case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM: return "VK_FORMAT_G8_B8R8_2PLANE_422_UNORM";
        case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM: return "VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM";
        case VK_FORMAT_R10X6_UNORM_PACK16: return "VK_FORMAT_R10X6_UNORM_PACK16";
        case VK_FORMAT_R10X6G10X6_UNORM_2PACK16: return "VK_FORMAT_R10X6G10X6_UNORM_2PACK16";
        case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16: return "VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16";
        case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16: return "VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16";
        case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16: return "VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16";
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16: return "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16";
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16: return "VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16";
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16: return "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16";
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16: return "VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16";
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16: return "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16";
        case VK_FORMAT_R12X4_UNORM_PACK16: return "VK_FORMAT_R12X4_UNORM_PACK16";
        case VK_FORMAT_R12X4G12X4_UNORM_2PACK16: return "VK_FORMAT_R12X4G12X4_UNORM_2PACK16";
        case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16: return "VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16";
        case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16: return "VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16";
        case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16: return "VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16";
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16: return "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16";
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16: return "VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16";
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16: return "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16";
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16: return "VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16";
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16: return "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16";
        case VK_FORMAT_G16B16G16R16_422_UNORM: return "VK_FORMAT_G16B16G16R16_422_UNORM";
        case VK_FORMAT_B16G16R16G16_422_UNORM: return "VK_FORMAT_B16G16R16G16_422_UNORM";
        case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM: return "VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM";
        case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM: return "VK_FORMAT_G16_B16R16_2PLANE_420_UNORM";
        case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM: return "VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM";
        case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM: return "VK_FORMAT_G16_B16R16_2PLANE_422_UNORM";
        case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM: return "VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM";
    #endif

    #if defined(VK_VERSION_1_3)
        /* Core Vulkan 1.3 */
        case VK_FORMAT_G8_B8R8_2PLANE_444_UNORM: return "VK_FORMAT_G8_B8R8_2PLANE_444_UNORM";
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16: return "VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16";
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16: return "VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16";
        case VK_FORMAT_G16_B16R16_2PLANE_444_UNORM: return "VK_FORMAT_G16_B16R16_2PLANE_444_UNORM";
        case VK_FORMAT_A4R4G4B4_UNORM_PACK16: return "VK_FORMAT_A4R4G4B4_UNORM_PACK16";
        case VK_FORMAT_A4B4G4R4_UNORM_PACK16: return "VK_FORMAT_A4B4G4R4_UNORM_PACK16";
        case VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK";
        case VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK";
        case VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK";
        case VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK";
        case VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK";
        case VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK";
        case VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK";
        case VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK";
        case VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK";
        case VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK";
        case VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK";
        case VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK";
        case VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK";
        case VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK";
    #endif

    #if defined(VK_VERSION_1_4)
        /* Core Vulkan 1.4 */
        case VK_FORMAT_A1B5G5R5_UNORM_PACK16: return "VK_FORMAT_A1B5G5R5_UNORM_PACK16";
        case VK_FORMAT_A8_UNORM: return "VK_FORMAT_A8_UNORM";
    #endif

    /* Fun fact, there's actually not a super great way to gate extension formats */

    default:
        return "VK_FORMAT_UNKNOWN";
    }

}














// ============================================================================
// Region: Allocator
// ============================================================================



// FL - free list
// SL - sub list
#define LAHAR_FREELIST_MAGIC 0xe654fa462a3501cf
#define LAHAR_FREELIST_MEM_TYPES VK_MAX_MEMORY_TYPES
#define LAHAR_FREELIST_FL_COUNT  21   /* fl 0..20 covers 2^8 .. 2^29 */
#define LAHAR_FREELIST_SL_LOG2   5
#define LAHAR_FREELIST_SL_COUNT  (1u << LAHAR_FREELIST_SL_LOG2)
#define LAHAR_FREELIST_MIN_LOG2  8 // min allocation size is 2^8 (256)
#define LAHAR_FREELIST_MIN_BLOCK ((VkDeviceSize)1 << LAHAR_FREELIST_MIN_LOG2)
#define LAHAR_FREELIST_MAX_BLOCK ((VkDeviceSize)1 << (LAHAR_FREELIST_FL_COUNT + LAHAR_FREELIST_MIN_LOG2))
// Largest request that is guaranteed to still map into the last bin (fl 20, sl 31)
// after __lahar_fl_map_up's round-up add. The bins only cover sizes with
// msb <= FL_COUNT + MIN_LOG2 - 1 (i.e. < 2^29), and map_up can add up to
// (MAX_BLOCK/2) >> SL_LOG2 - 1 before remapping, so anything above this
// threshold must take the dedicated path.
#define LAHAR_FREELIST_MAX_ALLOC (LAHAR_FREELIST_MAX_BLOCK - (LAHAR_FREELIST_MAX_BLOCK >> (LAHAR_FREELIST_SL_LOG2 + 1)))
#define LAHAR_FREELIST_PREFERRED_CHUNK_SIZE ((VkDeviceSize)1024 * 1024 * 256) // 256 mib
// A heap at or below this size is "small" (integrated parts, BAR heaps); chunks
// there are sized relative to the heap instead of at the flat preferred size.
#define LAHAR_FREELIST_SMALL_HEAP_SIZE ((VkDeviceSize)1024 * 1024 * 1024) // 1 gib
#define LAHAR_FREELIST_SMALL_HEAP_DIVISOR 8
// How many times the preferred size may be halved when ramping up a type, and
// when retrying a chunk that failed device OOM. 3 gives 1/8, 1/4, 1/2.
#define LAHAR_FREELIST_CHUNK_SHIFT_MAX 3

lahar_static_assert(LAHAR_FREELIST_SL_COUNT <= 32, "sl bitmap must fit in uint32_t");
lahar_static_assert(LAHAR_FREELIST_FL_COUNT <= 32, "fl bitmap must fit in uint32_t");
lahar_static_assert(LAHAR_FREELIST_MIN_LOG2 >= LAHAR_FREELIST_SL_LOG2, "smallest block must have room for sl bits below its msb");

struct LaharFreelistAllocator;
typedef struct LaharFreelistAllocator LaharFreelistAllocator;

struct LaharFreelistMemChunk;
typedef struct LaharFreelistMemChunk LaharFreelistMemChunk;

struct LaharFreelistChunkVec;
typedef struct LaharFreelistChunkVec LaharFreelistChunkVec;

struct LaharFreelistSubAllocation;
typedef struct LaharFreelistSubAllocation LaharFreelistSubAllocation;

struct LaharFreelistMemReqs;
typedef struct LaharFreelistMemReqs LaharFreelistMemReqs;

#if !defined(__cplusplus)
enum LaharGranularityClass;
typedef enum LaharGranularityClass LaharGranularityClass;
#endif

struct LaharFreelistMemReqs {
    VkDeviceSize size;
    VkDeviceSize alignment;
    VkDeviceSize padded_size;
    uint32_t type_mask;
    bool prefers_dedicated;
    bool requires_dedicated;
};

enum LaharGranularityClass {
    LAHAR_GC_LINEAR,
    LAHAR_GC_NONLINEAR,
};

#if defined(__cplusplus)
typedef enum LaharGranularityClass LaharGranularityClass;
#endif

// Every mem chunk is sort of its own universe. We can't coalesce
// across them, and we can't mix types in the tlsf structure,
// so basically just use them like they're completely separate allocator regions
//
// The mapping is chunk-wide and persistent. Vulkan allows only one vkMapMemory per
// VkDeviceMemory, so suballocations share it: the chunk maps whole on
// first request and stays mapped until the last user unmaps.
struct LaharFreelistMemChunk {
    VkDeviceMemory handle;
    VkDeviceSize size;
    uint32_t type_index;
    uint32_t live_count;
    void* mapped;
    uint32_t map_count;
    LaharFreelistSubAllocation* blocks;
    LaharFreelistSubAllocation* bins[LAHAR_FREELIST_FL_COUNT][LAHAR_FREELIST_SL_COUNT];
    uint32_t bin_bitmap;
    uint32_t subbin_bitmap[LAHAR_FREELIST_FL_COUNT];
    LaharGranularityClass granularity_class;
    bool dedicated;
};

struct LaharFreelistSubAllocation {
    LaharFreelistMemChunk* chunk;
    VkDeviceSize offset, size;
    LaharFreelistSubAllocation* prev_phys;
    LaharFreelistSubAllocation* next_phys;
    LaharFreelistSubAllocation* prev_free;
    LaharFreelistSubAllocation* next_free;
    bool free;
};

struct LaharFreelistChunkVec {
    LaharFreelistMemChunk** chunks;
    size_t count, cap;
};


struct LaharFreelistAllocator {
    LaharAllocator vtable;
    uint64_t magic;
    LaharFreelistChunkVec types[LAHAR_FREELIST_MEM_TYPES];
};

// Count leading zeroes - position of the highest set bit
static uint32_t __lahar_clz64(uint64_t num) {
#if defined(__has_builtin)
    #if __has_builtin(__builtin_clzg)
        return (uint32_t)__builtin_clzg(num, 64);
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    return num ? (uint32_t)__builtin_clzll(num) : 64u;
#elif defined(_MSC_VER)
    unsigned long i;
    return _BitScanReverse64(&i, num) ? 63u - (uint32_t)i : 64u;
#endif

    int n = 0;
    if (num == 0) return 64;
    if ((num >> 32) == 0) { n += 32; num <<= 32; }
    if ((num >> 48) == 0) { n += 16; num <<= 16; }
    if ((num >> 56) == 0) { n +=  8; num <<=  8; }
    if ((num >> 60) == 0) { n +=  4; num <<=  4; }
    if ((num >> 62) == 0) { n +=  2; num <<=  2; }
    if ((num >> 63) == 0) { n +=  1; }
    return (uint32_t)n; // TODO: spurious sign casting?
}

// Count trailing zeroes — position of the lowest set bit
static uint32_t __lahar_ctz32(uint32_t num) {
#if defined(__has_builtin)
    #if __has_builtin(__builtin_ctzg)
        return (uint32_t)__builtin_ctzg(num, 32);
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    return num ? (uint32_t)__builtin_ctz(num) : 32u;
#elif defined(_MSC_VER)
    unsigned long i;
    return _BitScanForward(&i, num) ? (uint32_t)i : 32u;
#endif

    if (num == 0) { return 32; }
    uint32_t n = 0;
    if ((num & 0x0000FFFF) == 0) { n += 16; num >>= 16; }
    if ((num & 0x000000FF) == 0) { n +=  8; num >>=  8; }
    if ((num & 0x0000000F) == 0) { n +=  4; num >>=  4; }
    if ((num & 0x00000003) == 0) { n +=  2; num >>=  2; }
    if ((num & 0x00000001) == 0) { n +=  1; }
    return n;
}

// Util: round up to the quantum
static VkDeviceSize __lahar_fl_round_up(VkDeviceSize size) {
    return (size + LAHAR_FREELIST_MIN_BLOCK - 1) & ~(LAHAR_FREELIST_MIN_BLOCK - 1);
}

// Util: whether a chunk of one granularity class may host the other. Mixing
// linear and non-linear resources in one VkDeviceMemory is only hazardous when
// the device reports a bufferImageGranularity above one byte; below that there
// are no shared pages to straddle, so mixing is free occupancy.
static bool __lahar_fl_must_segregate(void) {
    return lahar->physdev_info.properties.limits.bufferImageGranularity > 1;
}

// TLSF: map a size to the (fl, sl) bin, rounding dowards (so this would be what you'd use on a new block insert)
static void __lahar_fl_map_down(VkDeviceSize size, uint32_t* fl, uint32_t* sl) {
    if (size < LAHAR_FREELIST_MIN_BLOCK) {
        size = LAHAR_FREELIST_MIN_BLOCK;
    }

    uint32_t msb = 63 - __lahar_clz64(size); // equivalent to an fls, position of the leading set bit
    *fl = msb - LAHAR_FREELIST_MIN_LOG2; // rebase so that msb == 8, since min tracking is 256 bytes
    *sl = (uint32_t)(size >> (msb - LAHAR_FREELIST_SL_LOG2)) & (LAHAR_FREELIST_SL_COUNT - 1); // drop the bits we don't care about, remove leading 1, effectively rounds down

    // size exceeds bin range; should have been picked up as dedicated allocation
    LAHAR_ASSERT(*fl < LAHAR_FREELIST_FL_COUNT);
}

// TLSF: map a size to the (fl, sl) bin, rounding upwards (so this would be what you'd use on a request)
static void __lahar_fl_map_up(VkDeviceSize size, uint32_t* fl, uint32_t* sl) {
    if (size < LAHAR_FREELIST_MIN_BLOCK) {
        size = LAHAR_FREELIST_MIN_BLOCK;
    }

    uint32_t msb = 63 - __lahar_clz64(size);
    size += ((VkDeviceSize)1 << (msb - LAHAR_FREELIST_SL_LOG2)) - 1;
    __lahar_fl_map_down(size, fl, sl);  // recompute msb — the add can carry
}

// Util: validate state - ouchie my brain
// Only defined under LAHAR_DEBUG; all call sites are gated the same way.
#ifdef LAHAR_DEBUG
static void __lahar_fl_validate_chunk(LaharFreelistMemChunk* chunk) {
    // walk blocks in address order:
    // x  offset % 256 == 0
    // x  size % 256 == 0 && size >= 256
    // x  no two adjacent free blocks (coalescing missed one)
    // x offsets + sizes tile the chunk exactly, no gaps or overlap
    // walk each bin list:
    // x every block's size actually maps to the bin it's in
    // x prev_free/next_free are consistent both directions
    // x bitmap bit set iff list non-empty

    LaharFreelistSubAllocation* block = chunk->blocks;
    VkDeviceSize expected_offset = 0;


    uint32_t live = 0;

    if (chunk->dedicated) {
        LAHAR_ASSERT(block->next_phys == NULL); // More than one physical block in a dedicatd allocation
        LAHAR_ASSERT(block->size == chunk->size); // Didn't map the whole thing
        LAHAR_ASSERT(block->offset == 0); // Not at the start?
        LAHAR_ASSERT(chunk->live_count == (block->free ? 0u : 1u)); // count matches the sole block
        return;
    }

    while (block) {
        LAHAR_ASSERT(block->offset % LAHAR_FREELIST_MIN_BLOCK == 0); // at a multiple of the quantum in offset
        LAHAR_ASSERT(block->size % LAHAR_FREELIST_MIN_BLOCK == 0); // a multiple of the quantum in size
        LAHAR_ASSERT(block->size >= LAHAR_FREELIST_MIN_BLOCK); // bigger than the quantum
        LAHAR_ASSERT(block->offset == expected_offset); // no gaps

        expected_offset = block->offset + block->size;

        if (!block->free) { live++; }

        if (block->next_phys) {
            LAHAR_ASSERT(!(block->free && block->next_phys->free)); // no adjacent free blocks
            LAHAR_ASSERT(block->next_phys->prev_phys == block); // list pointers consistent
        }

        block = block->next_phys;
    }

    LAHAR_ASSERT(expected_offset == chunk->size); // the whole chunk is accounted for
    LAHAR_ASSERT(live == chunk->live_count); // live_count tracks non-free blocks exactly

    for(uint32_t fl = 0; fl < LAHAR_FREELIST_FL_COUNT; fl++) {
        bool had_any = false;

        for(uint32_t sl = 0; sl < LAHAR_FREELIST_SL_COUNT; sl++) {
            bool has_list = chunk->bins[fl][sl] != NULL;
            bool has_bit  = (chunk->subbin_bitmap[fl] & (1u << sl)) != 0;

            LAHAR_ASSERT(has_list == has_bit); // bit and list status match
            had_any |= has_list;

            LaharFreelistSubAllocation* prev = NULL;
            for (LaharFreelistSubAllocation* b = chunk->bins[fl][sl]; b; b = b->next_free) {
                LAHAR_ASSERT(b->prev_free == prev);
                LAHAR_ASSERT(b->free);

                uint32_t bfl, bsl;
                __lahar_fl_map_down(b->size, &bfl, &bsl);
                LAHAR_ASSERT(bfl == fl && bsl == sl);  // filed in the right bin

                prev = b;
            }
        }

        bool has_bit = (chunk->bin_bitmap & (1u << fl)) != 0;

        LAHAR_ASSERT(had_any == has_bit);
    }
}

// Util: validate state
static void __lahar_fl_validate(LaharFreelistAllocator* self) {
    for (uint32_t i = 0; i < LAHAR_FREELIST_MEM_TYPES; i++) {
        LaharFreelistChunkVec* vec = &self->types[i];

        for (uint32_t j = 0; j < vec->count; j++) {
            __lahar_fl_validate_chunk(vec->chunks[j]);
        }
    }
}
#endif // LAHAR_DEBUG


static void __lahar_fl_tlsf_insert(LaharFreelistMemChunk* chunk, LaharFreelistSubAllocation* block) {
    if (chunk->dedicated) { return; }

    uint32_t fl, sl;

    __lahar_fl_map_down(block->size, &fl, &sl);

    LaharFreelistSubAllocation* curhead = chunk->bins[fl][sl];

    if (curhead) {
        curhead->prev_free = block;
    }

    block->prev_free = NULL;
    block->next_free = curhead;
    chunk->bins[fl][sl] = block;

    chunk->bin_bitmap |= 1u << fl;
    chunk->subbin_bitmap[fl] |= 1u << sl;
}

static void __lahar_fl_tlsf_remove(LaharFreelistMemChunk* chunk, LaharFreelistSubAllocation* block) {
    if (chunk->dedicated) { return; }

    LaharFreelistSubAllocation* prev = block->prev_free;
    LaharFreelistSubAllocation* next = block->next_free;

    uint32_t fl, sl;
    __lahar_fl_map_down(block->size, &fl, &sl);

    if (prev) {
        prev->next_free = next;
    }
    else {
        chunk->bins[fl][sl] = next;
    }

    if (next) {
        next->prev_free = prev;
    }

    // if we just cleared the bin, we have to unset the bit
    if (!prev && !next) {
        uint32_t mask = chunk->subbin_bitmap[fl] &= ~(1u << sl); // remove one bit

        if (mask == 0) { // if all subbins are empty, the bin needs set empty
            chunk->bin_bitmap &= ~(1u << fl);
        }
    }

    block->prev_free = NULL;
    block->next_free = NULL;
}

// Util: Standard TLSF good-fit scan. Look for a free block in (fl, sl) or any
// higher sl of the same fl; failing that, take the lowest non-empty sl of
// the next non-empty fl. Assumes (fl, sl) came from __lahar_fl_map_up, so
// any block found is guaranteed large enough.
static LaharFreelistSubAllocation* __lahar_fl_tlsf_search(LaharFreelistMemChunk* chunk, uint32_t fl, uint32_t sl) {
    uint32_t sl_map = chunk->subbin_bitmap[fl] & (~0u << sl);

    if (!sl_map) {
        const uint32_t fl_map = chunk->bin_bitmap & (~0u << (fl + 1));

        if (!fl_map) { return NULL; }

        fl = __lahar_ctz32(fl_map);
        sl_map = chunk->subbin_bitmap[fl];
    }

    sl = __lahar_ctz32(sl_map);

    return chunk->bins[fl][sl];
}

// Util: Carve [offset, offset + size) out of a free block that has already been
// removed from the bins. Front/back remainders get filed back into the bins
// as their own free blocks. offset and size must be quantum multiples within
// the block, which makes any nonzero remainder >= the quantum automatically.
static uint32_t __lahar_fl_block_split(
    LaharFreelistMemChunk* chunk,
    LaharFreelistSubAllocation* block,
    VkDeviceSize offset,
    VkDeviceSize size
) {
    LAHAR_ASSERT(offset % LAHAR_FREELIST_MIN_BLOCK == 0);
    LAHAR_ASSERT(size % LAHAR_FREELIST_MIN_BLOCK == 0);
    LAHAR_ASSERT(offset >= block->offset);
    LAHAR_ASSERT(offset + size <= block->offset + block->size);

    const VkDeviceSize front = offset - block->offset;
    const VkDeviceSize back = (block->offset + block->size) - (offset + size);

    // quantum-aligned inputs mean a remainder is never a sliver too small to track
    LAHAR_ASSERT(front == 0 || front >= LAHAR_FREELIST_MIN_BLOCK);
    LAHAR_ASSERT(back == 0 || back >= LAHAR_FREELIST_MIN_BLOCK);

    LaharFreelistSubAllocation* front_block = NULL;
    LaharFreelistSubAllocation* back_block = NULL;

    // allocate everything up front so failure can't leave a half-split block
    if (front) {
        front_block = (LaharFreelistSubAllocation*)lahar_malloc(sizeof(*front_block));
        if (!front_block) { return LAHAR_ERR_ALLOC_FAILED; }
    }

    if (back) {
        back_block = (LaharFreelistSubAllocation*)lahar_malloc(sizeof(*back_block));
        if (!back_block) {
            if (front_block) { lahar_free(front_block); }
            return LAHAR_ERR_ALLOC_FAILED;
        }
    }

    if (front_block) {
        memset(front_block, 0, sizeof(*front_block));
        front_block->chunk = chunk;
        front_block->offset = block->offset;
        front_block->size = front;
        front_block->free = true;

        front_block->prev_phys = block->prev_phys;
        front_block->next_phys = block;

        if (block->prev_phys) {
            block->prev_phys->next_phys = front_block;
        }
        else {
            chunk->blocks = front_block;
        }

        block->prev_phys = front_block;

        __lahar_fl_tlsf_insert(chunk, front_block);
    }

    if (back_block) {
        memset(back_block, 0, sizeof(*back_block));
        back_block->chunk = chunk;
        back_block->offset = offset + size;
        back_block->size = back;
        back_block->free = true;

        back_block->prev_phys = block;
        back_block->next_phys = block->next_phys;

        if (block->next_phys) {
            block->next_phys->prev_phys = back_block;
        }

        block->next_phys = back_block;

        __lahar_fl_tlsf_insert(chunk, back_block);
    }

    block->offset = offset;
    block->size = size;

    return LAHAR_ERR_SUCCESS;
}

// Util: Return a no-longer-used block to the free bins, coalescing with physically adjacent free neighbors.
static void __lahar_fl_block_release(LaharFreelistMemChunk* chunk, LaharFreelistSubAllocation* block) {
    block->free = true;

    if (chunk->dedicated) { return; }

    LaharFreelistSubAllocation* prev = block->prev_phys;
    LaharFreelistSubAllocation* next = block->next_phys;

    if (prev && prev->free) {
        __lahar_fl_tlsf_remove(chunk, prev);

        prev->size += block->size;
        prev->next_phys = next;

        if (next) { next->prev_phys = prev; }

        lahar_free(block);
        block = prev;
    }

    if (next && next->free) {
        __lahar_fl_tlsf_remove(chunk, next);

        block->size += next->size;
        block->next_phys = next->next_phys;

        if (next->next_phys) { next->next_phys->prev_phys = block; }

        lahar_free(next);
    }

    __lahar_fl_tlsf_insert(chunk, block);
}

// Util: create a new memchunk
static uint32_t __lahar_fl_alloc_memchunk(
    LaharFreelistAllocator* self,
    VkDeviceSize size,
    uint32_t type_index,
    bool dedicated,
    LaharGranularityClass granularity_class,
    void* pNext,
    LaharFreelistMemChunk** chunk_out
) {
    uint32_t err = LAHAR_ERR_SUCCESS;
    VkMemoryAllocateInfo info = ZINIT;
    LaharFreelistMemChunk* chunk = NULL;
    LaharFreelistSubAllocation* block = NULL;
    LaharFreelistChunkVec* vec = NULL;

    if (!dedicated) {
        size = (size + LAHAR_FREELIST_MIN_BLOCK - 1) & ~(LAHAR_FREELIST_MIN_BLOCK - 1);

        // A shared chunk's initial free block is binned, and the bins cannot
        // index a block at or above 2^29. Anything larger has to be dedicated.
        LAHAR_ASSERT(size <= LAHAR_FREELIST_MAX_ALLOC);
    }

    chunk = (LaharFreelistMemChunk*)lahar_malloc(sizeof(*chunk));
    if (!chunk) {
        err = LAHAR_ERR_ALLOC_FAILED;
        goto end;
    }

    memset(chunk, 0, sizeof(*chunk));

    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.allocationSize = size;
    info.memoryTypeIndex = type_index;
    info.pNext = pNext;

    if ((lahar->vkresult = vkAllocateMemory(lahar->device, &info, lahar->vkalloc, &chunk->handle)) != VK_SUCCESS) {
        err = LAHAR_ERR_VK_ERR;
        goto end;
    }

    block = (LaharFreelistSubAllocation*)lahar_malloc(sizeof(*block));
    if (!block) {
        err = LAHAR_ERR_ALLOC_FAILED;
        goto end;
    }

    memset(block, 0, sizeof(*block));
    block->chunk = chunk;
    block->offset = 0;
    block->size = size;
    block->free = true;

    chunk->size = size;
    chunk->type_index = type_index;
    chunk->blocks = block;
    chunk->dedicated = dedicated;
    chunk->granularity_class = granularity_class;

    if (!dedicated) {
        __lahar_fl_tlsf_insert(chunk, block);
    }

    vec = &self->types[type_index];

    if (!(vec->cap && vec->count < vec->cap)) {
        lahar_vec_expand(vec->chunks, vec->cap) else {
            err = LAHAR_ERR_ALLOC_FAILED;
            goto end;
        }
    }

    vec->chunks[vec->count++] = chunk;
    *chunk_out = chunk;
end:
    if (err) {
        if (block) { lahar_free(block); }
        if (chunk) {
            if (chunk->handle != VK_NULL_HANDLE) {
                vkFreeMemory(lahar->device, chunk->handle, lahar->vkalloc);
            }

            lahar_free(chunk);
        }
    }

    return err;
}

// Util: destroy a mem chunk
static void __lahar_fl_free_memchunk(LaharFreelistAllocator* self, LaharFreelistMemChunk* chunk) {
    LAHAR_ASSERT(self && chunk);

    uint32_t index = UINT32_MAX;

    LaharFreelistChunkVec* vec = &self->types[chunk->type_index];

    for (uint32_t i = 0; i < vec->count; i++) {
        if (vec->chunks[i] == chunk) {
            index = i;
            break;
        }
    }

    if (index == UINT32_MAX) { return; }

    if (index != vec->count - 1) {
        vec->chunks[index] = vec->chunks[vec->count - 1];
    }

    vec->chunks[vec->count - 1] = NULL;
    vec->count--;

    LaharFreelistSubAllocation* suballoc = chunk->blocks;

    while(suballoc) {
        LaharFreelistSubAllocation* cur = suballoc;
        suballoc = suballoc->next_phys;
        lahar_free(cur);
    }

    vkFreeMemory(lahar->device, chunk->handle, lahar->vkalloc);
    lahar_free(chunk);
}

// Util: Inverse of __lahar_fl_suballoc: return a live suballocation to the allocator.
// Owns the reclaim policy: currently eager, so a chunk whose last resident
// leaves is freed immediately (dedicated chunks trivially so).
//
// This *should* be the only decrement of live_count, and __lahar_fl_suballoc
// the only increment.
static void __lahar_fl_suballoc_release(
    LaharFreelistAllocator* self,
    LaharFreelistSubAllocation* suballoc
) {
    LAHAR_ASSERT(self && suballoc);

    LaharFreelistMemChunk* chunk = suballoc->chunk;

    LAHAR_ASSERT(chunk && chunk->live_count > 0);
    LAHAR_ASSERT(!suballoc->free);

    chunk->live_count--;

    if (chunk->dedicated || chunk->live_count == 0) {
        __lahar_fl_free_memchunk(self, chunk);
    }
    else {
        __lahar_fl_block_release(chunk, suballoc);
    }
}

// Util: given a type mask, required properties, and preferred properties, pick the best memory type index
static uint32_t __lahar_fl_select_mem(uint32_t type_mask, uint32_t required_props, uint32_t preferred_props, uint32_t* chosen) {
    LAHAR_ASSERT(chosen);

    uint32_t index = UINT32_MAX;
    uint32_t best_score = UINT32_MAX;

    for (uint32_t i = 0; i < lahar->physdev_info.memprops.memoryTypeCount; i++) {
        if (!(type_mask & (1u << i))) { continue; }

        const VkMemoryType* memtype = &lahar->physdev_info.memprops.memoryTypes[i];

        if ((memtype->propertyFlags & required_props) != required_props) { continue; }

        uint32_t score = 0;

        for (uint32_t j = 0; j < sizeof(preferred_props) * 8; j++) {
            uint32_t flag = 1u << j;

            if ((flag & preferred_props) && (flag & memtype->propertyFlags)) {
                score++;
            }
        }

        if (best_score == UINT32_MAX || score > best_score) {
            index = i;
            best_score = score;
        }
    }

    if (index != UINT32_MAX) {
        *chosen = index;
    }

    return index == UINT32_MAX ? LAHAR_ERR_MEMORY_UNSATISFIABLE : LAHAR_ERR_SUCCESS;
}

// Util: map an allocation create info to the required/preferred memory flags
static uint32_t __lahar_fl_resolve_mem_flags(
    const LaharAllocationCreateInfo* alloc_info,
    uint32_t* required_out,
    uint32_t* preferred_out
) {
    LAHAR_ASSERT(alloc_info && required_out && preferred_out);

    LaharMemoryUsage chosen_mem_usage = LAHAR_MU_DONT_KNOW;

    uint32_t required_flags = 0;
    uint32_t preferred_flags = alloc_info->preferred_flags;

    if (alloc_info->usage == LAHAR_MU_DONT_KNOW) {
        if (alloc_info->required_flags) {
            required_flags = alloc_info->required_flags;
        }
        else {
            chosen_mem_usage = LAHAR_MU_DEVICE_ONLY;
        }
    }
    else {
        chosen_mem_usage = alloc_info->usage;
    }

    if (!required_flags) {
        LAHAR_ASSERT(chosen_mem_usage != LAHAR_MU_DONT_KNOW);

        switch (chosen_mem_usage) {
            case LAHAR_MU_DONT_KNOW:
                /* unreachable */
                return LAHAR_ERR_INVALID_STATE;
            case LAHAR_MU_DEVICE_ONLY:
                preferred_flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                break;
            case LAHAR_MU_STAGING_SEQUENTIAL:
                required_flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
                preferred_flags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                break;
            case LAHAR_MU_UPLOAD_DIRECT:
                required_flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
                preferred_flags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                preferred_flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                break;
            case LAHAR_MU_READBACK:
                required_flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
                preferred_flags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                preferred_flags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
                break;
            default:
                return LAHAR_ERR_ILLEGAL_PARAMS;
        }
    }

    *required_out = required_flags;
    *preferred_out = preferred_flags;
    return LAHAR_ERR_SUCCESS;
}

// Util: compute the memory requirements for an image
static void __lahar_fl_image_mem_reqs(VkImage image, LaharFreelistMemReqs* out) {
    LAHAR_ASSERT(image != VK_NULL_HANDLE && out);

    #if defined(VK_VERSION_1_1) || (defined(VK_KHR_get_memory_requirements2) && defined(VK_KHR_dedicated_allocation))
    VkImageMemoryRequirementsInfo2KHR image_requirements2 = ZINIT;
    image_requirements2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR;

    VkMemoryRequirements2KHR requirements2 = ZINIT;
    requirements2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR;

    VkMemoryDedicatedRequirementsKHR dedicated_requirements = ZINIT;
    dedicated_requirements.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR;

    requirements2.pNext = (void*)&dedicated_requirements;
    #endif

    VkMemoryRequirements requirements = ZINIT;

    #if defined(VK_VERSION_1_1)
    if (VK_API_VERSION_MINOR(lahar->vkversion) >= 1 && vkGetImageMemoryRequirements2 != NULL) {
        image_requirements2.image = image;

        vkGetImageMemoryRequirements2(
            lahar->device,
            &image_requirements2,
            &requirements2
        );

        out->size = requirements2.memoryRequirements.size;
        out->alignment = requirements2.memoryRequirements.alignment;
        out->requires_dedicated = dedicated_requirements.requiresDedicatedAllocation;
        out->prefers_dedicated = dedicated_requirements.prefersDedicatedAllocation;
        out->type_mask = requirements2.memoryRequirements.memoryTypeBits;

        goto end;
    }
    #endif

    #if defined(VK_KHR_get_memory_requirements2) && defined(VK_KHR_dedicated_allocation)
    if (
        lahar_extension_has_device(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) &&
        lahar_extension_has_device(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME) &&
        vkGetImageMemoryRequirements2KHR != NULL
    ) {
        image_requirements2.image = image;

        vkGetImageMemoryRequirements2KHR(
            lahar->device,
            &image_requirements2,
            &requirements2
        );

        out->size = requirements2.memoryRequirements.size;
        out->alignment = requirements2.memoryRequirements.alignment;
        out->requires_dedicated = dedicated_requirements.requiresDedicatedAllocation;
        out->prefers_dedicated = dedicated_requirements.prefersDedicatedAllocation;
        out->type_mask = requirements2.memoryRequirements.memoryTypeBits;

        goto end;
    }
    #endif

    vkGetImageMemoryRequirements(
        lahar->device,
        image,
        &requirements
    );

    memset(out, 0, sizeof(*out));

    out->size = requirements.size;
    out->alignment = requirements.alignment;
    out->type_mask = requirements.memoryTypeBits;

end:
    if (out->alignment == 0) { out->alignment = 1; } // paranoia, spec says power of two

    // Search size, padded for alignment. Block offsets are always quantum
    // multiples, so the worst-case padding to align within any free block is
    // (alignment - quantum). Baking that into the searched size means any
    // block the bins hand back is guaranteed to fit after the offset nudge,
    // keeping the search a pure bitmap walk with no fit-retry loop.
    out->padded_size = __lahar_fl_round_up(out->size);

    if (out->alignment > LAHAR_FREELIST_MIN_BLOCK) {
        out->padded_size += out->alignment - LAHAR_FREELIST_MIN_BLOCK;
    }

    // Note: MAX_ALLOC, not MAX_BLOCK. The last bin covers sizes with msb 28,
    // and map_up's round-up can carry a size just below 2^29 into msb 29,
    // which would index fl == FL_COUNT and trip the map_down assert.
    if (out->padded_size > LAHAR_FREELIST_MAX_ALLOC) {
        out->requires_dedicated = true;
    }
}

// Util: compute the memory requirements for a buffer
static void __lahar_fl_buffer_mem_reqs(VkBuffer buffer, LaharFreelistMemReqs* out) {
    LAHAR_ASSERT(buffer != VK_NULL_HANDLE && out);

    #if defined(VK_VERSION_1_1) || (defined(VK_KHR_get_memory_requirements2) && defined(VK_KHR_dedicated_allocation))
    VkBufferMemoryRequirementsInfo2KHR buffer_requirements2 = ZINIT;
    buffer_requirements2.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2_KHR;

    VkMemoryRequirements2KHR requirements2 = ZINIT;
    requirements2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR;

    VkMemoryDedicatedRequirementsKHR dedicated_requirements = ZINIT;
    dedicated_requirements.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR;

    requirements2.pNext = (void*)&dedicated_requirements;
    #endif

    VkMemoryRequirements requirements = ZINIT;

    #if defined(VK_VERSION_1_1)
    if (VK_API_VERSION_MINOR(lahar->vkversion) >= 1 && vkGetBufferMemoryRequirements2 != NULL) {
        buffer_requirements2.buffer = buffer;

        vkGetBufferMemoryRequirements2(
            lahar->device,
            &buffer_requirements2,
            &requirements2
        );

        out->size = requirements2.memoryRequirements.size;
        out->alignment = requirements2.memoryRequirements.alignment;
        out->requires_dedicated = dedicated_requirements.requiresDedicatedAllocation;
        out->prefers_dedicated = dedicated_requirements.prefersDedicatedAllocation;
        out->type_mask = requirements2.memoryRequirements.memoryTypeBits;

        goto end;
    }
    #endif

    #if defined(VK_KHR_get_memory_requirements2) && defined(VK_KHR_dedicated_allocation)
    if (
        lahar_extension_has_device(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) &&
        lahar_extension_has_device(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME) &&
        vkGetBufferMemoryRequirements2KHR != NULL
    ) {
        buffer_requirements2.buffer = buffer;

        vkGetBufferMemoryRequirements2KHR(
            lahar->device,
            &buffer_requirements2,
            &requirements2
        );

        out->size = requirements2.memoryRequirements.size;
        out->alignment = requirements2.memoryRequirements.alignment;
        out->requires_dedicated = dedicated_requirements.requiresDedicatedAllocation;
        out->prefers_dedicated = dedicated_requirements.prefersDedicatedAllocation;
        out->type_mask = requirements2.memoryRequirements.memoryTypeBits;

        goto end;
    }
    #endif

    vkGetBufferMemoryRequirements(
        lahar->device,
        buffer,
        &requirements
    );

    memset(out, 0, sizeof(*out));

    out->size = requirements.size;
    out->alignment = requirements.alignment;
    out->type_mask = requirements.memoryTypeBits;

end:
    if (out->alignment == 0) { out->alignment = 1; } // paranoia, spec says power of two

    // See __lahar_fl_image_mem_reqs for the reasoning on both of these.
    out->padded_size = __lahar_fl_round_up(out->size);

    if (out->alignment > LAHAR_FREELIST_MIN_BLOCK) {
        out->padded_size += out->alignment - LAHAR_FREELIST_MIN_BLOCK;
    }

    if (out->padded_size > LAHAR_FREELIST_MAX_ALLOC) {
        out->requires_dedicated = true;
    }
}


// Util: how big a fresh chunk should be for a request that missed every
// existing chunk. Owns the whole size policy:
//
//   - dedicated chunks are exactly the request (the dedicated allocation
//     contract is 1:1 memory-to-resource, so there is nothing to decide)
//   - shared chunks start from a preferred size that is heap-relative on small
//     heaps, so an integrated part or a BAR heap never hands out a quarter of
//     itself for one small resource
//   - that preferred size is then ramped: while this type/class has no chunk
//     bigger than half the preferred size, open 1/8, then 1/4, then 1/2. A
//     process that allocates two small resources pays two small chunks, while
//     one that keeps allocating converges on full-size chunks and so stays well
//     clear of maxMemoryAllocationCount.
//   - a request larger than the preferred size just opens a chunk that fits it
//
// The result is always clamped to MAX_ALLOC: a chunk's initial free block gets
// binned, and the bins cannot index a block at or above 2^29.
static VkDeviceSize __lahar_fl_chunk_size(
    LaharFreelistAllocator* self,
    uint32_t type_index,
    const LaharFreelistMemReqs* reqs,
    LaharGranularityClass granularity_class,
    bool dedicated
) {
    LAHAR_ASSERT(self && reqs);

    if (dedicated) {
        return reqs->size;
    }

    // Carving at offset 0 satisfies any alignment, so the fresh-chunk floor is
    // the unpadded request.
    const VkDeviceSize needed = __lahar_fl_round_up(reqs->size);

    LAHAR_ASSERT(needed <= LAHAR_FREELIST_MAX_ALLOC); // else reqs should be dedicated

    const uint32_t heap_index = lahar->physdev_info.memprops.memoryTypes[type_index].heapIndex;
    const VkDeviceSize heap_size = lahar->physdev_info.memprops.memoryHeaps[heap_index].size;

    VkDeviceSize preferred = heap_size <= LAHAR_FREELIST_SMALL_HEAP_SIZE ?
        heap_size / LAHAR_FREELIST_SMALL_HEAP_DIVISOR :
        LAHAR_FREELIST_PREFERRED_CHUNK_SIZE;

    if (preferred > LAHAR_FREELIST_MAX_ALLOC) {
        preferred = LAHAR_FREELIST_MAX_ALLOC;
    }

    // Largest shared chunk this type already holds in the classes that can
    // serve this request; the ramp only steps down past sizes we've outgrown.
    VkDeviceSize max_existing = 0;
    LaharFreelistChunkVec* vec = &self->types[type_index];

    for (uint32_t i = 0; i < vec->count; i++) {
        LaharFreelistMemChunk* chunk = vec->chunks[i];

        if (chunk->dedicated) { continue; }

        if (__lahar_fl_must_segregate() && chunk->granularity_class != granularity_class) {
            continue;
        }

        if (chunk->size > max_existing) { max_existing = chunk->size; }
    }

    for (uint32_t i = 0; i < LAHAR_FREELIST_CHUNK_SHIFT_MAX; i++) {
        const VkDeviceSize smaller = preferred / 2;

        // Stop as soon as halving would undercut a chunk we already hold, or
        // leave the request without comfortable room to share the chunk.
        if (smaller > max_existing && smaller >= needed * 2) {
            preferred = smaller;
        }
        else {
            break;
        }
    }

    return needed > preferred ? needed : preferred;
}

// Util: suballocate from the allocator, carving existing chunks, or making a new one
static uint32_t __lahar_fl_suballoc(
    LaharFreelistAllocator* self,
    uint32_t type_index,
    const LaharFreelistMemReqs* reqs,
    LaharGranularityClass granularity_class,
    void* pNext,
    LaharFreelistSubAllocation** out
) {
    LAHAR_ASSERT(self && reqs && out);

    uint32_t err = LAHAR_ERR_SUCCESS;
    LaharFreelistMemChunk* chunk = NULL;
    LaharFreelistSubAllocation* chosen_suballoc = NULL;


    if (!reqs->prefers_dedicated && !reqs->requires_dedicated) {
        LaharFreelistChunkVec* vec = &self->types[type_index];

        uint32_t fl, sl;
        __lahar_fl_map_up(reqs->padded_size, &fl, &sl);

        for (uint32_t i = 0; i < vec->count; i++) {
            // chunks are segregated by granularity class: linear and
            // non-linear resources never share a chunk, which satisfies
            // bufferImageGranularity without any per-block page math
            // but only if the device requires it
            if (__lahar_fl_must_segregate() && vec->chunks[i]->granularity_class != granularity_class) {
                continue;
            }

            LaharFreelistSubAllocation* found = __lahar_fl_tlsf_search(vec->chunks[i], fl, sl);

            if (found) {
                chunk = vec->chunks[i];
                chosen_suballoc = found;
                break;
            }
        }

        if (chosen_suballoc) {
            // padding the searched size means this can't run off the end of the block
            const VkDeviceSize aligned_offset = (chosen_suballoc->offset + reqs->alignment - 1) & ~(reqs->alignment - 1);

            __lahar_fl_tlsf_remove(chunk, chosen_suballoc);

            err = __lahar_fl_block_split(chunk, chosen_suballoc, aligned_offset, __lahar_fl_round_up(reqs->size));

            if (err) {
                // host OOM carving the remainders; put the block back untouched
                __lahar_fl_tlsf_insert(chunk, chosen_suballoc);
                chosen_suballoc = NULL;
                goto end;
            }
        }
    }

    if (!chosen_suballoc) {
        const bool dedicated = reqs->prefers_dedicated || reqs->requires_dedicated;

        // The size the request actually needs out of a fresh chunk. Carving
        // at offset 0 satisfies any alignment, so no padding is needed here.
        const VkDeviceSize needed = __lahar_fl_round_up(reqs->size);

        VkDeviceSize chunk_size = __lahar_fl_chunk_size(self, type_index, reqs, granularity_class, dedicated);

        err = __lahar_fl_alloc_memchunk(
            self,
            chunk_size,
            type_index,
            dedicated,
            granularity_class,
            pNext,
            &chunk
        );

        // A chunk failing device OOM doesn't mean the request can't fit in this
        // memory type; walk the size back down toward what it actually needs
        // before the caller writes the whole type off. Dedicated chunks have no
        // slack to give up, so they fail outright.
        for (uint32_t i = 0; !dedicated && i < LAHAR_FREELIST_CHUNK_SHIFT_MAX; i++) {
            if (err != LAHAR_ERR_VK_ERR || lahar->vkresult != VK_ERROR_OUT_OF_DEVICE_MEMORY) {
                break;
            }

            if (chunk_size <= needed) { break; }

            const VkDeviceSize smaller = chunk_size / 2;
            chunk_size = smaller > needed ? smaller : needed;

            err = __lahar_fl_alloc_memchunk(self, chunk_size, type_index, dedicated, granularity_class, pNext, &chunk);
        }

        if (err) {
            goto end;
        }

        chosen_suballoc = chunk->blocks;

        // no-op for dedicated chunks (nothing is binned)
        __lahar_fl_tlsf_remove(chunk, chosen_suballoc);

        if (!dedicated) {
            // carve the request out of the fresh chunk; the block starts at
            // offset 0, which satisfies any alignment
            err = __lahar_fl_block_split(chunk, chosen_suballoc, 0, __lahar_fl_round_up(reqs->size));

            if (err) {
                // host OOM carving the remainder; eagerly drop the chunk we
                // just opened rather than binning an untouched empty chunk
                __lahar_fl_free_memchunk(self, chunk);
                chosen_suballoc = NULL;
                goto end;
            }
        }
    }

    chosen_suballoc->free = false;
    chosen_suballoc->chunk->live_count++;

end:
    *out = chosen_suballoc;

    return err;
}

// Method
static uint32_t __lahar_fl_alloc_image(
    void* s,
    const VkImageCreateInfo* info,
    const LaharAllocationCreateInfo* alloc_info,
    VkImage* img_out,
    LaharAllocation* alloc_out
) {
    if ( !s ||
        !info ||
        !alloc_info ||
        !img_out ||
        !alloc_out
    ) {
        return LAHAR_ERR_ILLEGAL_PARAMS;
    }

    LaharFreelistAllocator* self = (LaharFreelistAllocator*)s;

    VkImage image = VK_NULL_HANDLE;
    uint32_t err = LAHAR_ERR_SUCCESS;


    #if defined(VK_VERSION_1_1) || (defined(VK_KHR_get_memory_requirements2) && defined(VK_KHR_dedicated_allocation))
    VkMemoryDedicatedAllocateInfoKHR dedicated_info = ZINIT;
    dedicated_info.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR;
    #endif

    void* create_pnext = NULL;
    LaharFreelistMemReqs img_reqs = ZINIT;

    uint32_t type_index = 0;
    uint32_t required_flags = 0;
    uint32_t preferred_flags = 0;
    uint32_t type_mask = 0;
    LaharGranularityClass granularity_class =
        info->tiling == VK_IMAGE_TILING_LINEAR ?
            LAHAR_GC_LINEAR :
            LAHAR_GC_NONLINEAR;

    LaharFreelistSubAllocation* chosen_suballoc = NULL;

    if ((err = __lahar_fl_resolve_mem_flags(alloc_info, &required_flags, &preferred_flags))) {
        goto end;
    }

    if ((lahar->vkresult = vkCreateImage(lahar->device, info, lahar->vkalloc, &image)) != VK_SUCCESS) {
        err = LAHAR_ERR_VK_ERR;
        goto end;
    }

    __lahar_fl_image_mem_reqs(image, &img_reqs);


    #if defined(VK_VERSION_1_1) || (defined(VK_KHR_get_memory_requirements2) && defined(VK_KHR_dedicated_allocation))
    if (img_reqs.requires_dedicated || img_reqs.prefers_dedicated) {
        dedicated_info.image = image;
        create_pnext = (void*)&dedicated_info;
    }
    #endif

    type_mask = img_reqs.type_mask;

    // Retry every memory type until we can allocate something or run out of types
    do {
        if ((err = __lahar_fl_select_mem(type_mask, required_flags, preferred_flags, &type_index))) {
            goto end;
        }

        if (!(err = __lahar_fl_suballoc(self, type_index, &img_reqs, granularity_class, create_pnext, &chosen_suballoc))) {
            break; // got one
        }

        if (err != LAHAR_ERR_VK_ERR || lahar->vkresult != VK_ERROR_OUT_OF_DEVICE_MEMORY) {
            goto end;
        }

        type_mask &= ~(1u << type_index);
    } while (type_mask);

    if (!chosen_suballoc) { goto end; } // mask exhausted, err still holds the OOM

    if ((lahar->vkresult = vkBindImageMemory(lahar->device, image, chosen_suballoc->chunk->handle, chosen_suballoc->offset))) {
        err = LAHAR_ERR_VK_ERR;
        goto end;
    }

end:
    if (err) {
        if (image) {
            vkDestroyImage(lahar->device, image, lahar->vkalloc);
        }

        if (chosen_suballoc) {
            // bind failed after suballocating; hand the block back. Eager
            // reclaim inside release also covers the fresh-chunk case.
            __lahar_fl_suballoc_release(self, chosen_suballoc);
        }
    }
    else {
        *img_out = image;
        *alloc_out = (LaharAllocation)(void*)chosen_suballoc;
    }

    #ifdef LAHAR_DEBUG
    __lahar_fl_validate(self);
    #endif

    return err;
}

// Method
static uint32_t __lahar_fl_free_image(
    void* s,
    VkImage image,
    LaharAllocation allocation
) {
    if (!s || !image || !allocation) {
        return LAHAR_ERR_ILLEGAL_PARAMS;
    }

    LaharFreelistAllocator* self = (LaharFreelistAllocator*)s;
    LaharFreelistSubAllocation* suballoc = (LaharFreelistSubAllocation*)allocation;

    vkDestroyImage(lahar->device, image, lahar->vkalloc);
    __lahar_fl_suballoc_release(self, suballoc);

    #ifdef LAHAR_DEBUG
    __lahar_fl_validate(self);
    #endif

    return LAHAR_ERR_SUCCESS;
}

// Method
static uint32_t __lahar_fl_alloc_buffer(
    void* s,
    const VkBufferCreateInfo* info,
    const LaharAllocationCreateInfo* alloc_info,
    VkBuffer* buffer_out,
    LaharAllocation* alloc_out
) {
    if (!s || !info || !alloc_info || !buffer_out || !alloc_out) {
        return LAHAR_ERR_ILLEGAL_PARAMS;
    }

    uint32_t err = LAHAR_ERR_SUCCESS;

    LaharFreelistAllocator* self = (LaharFreelistAllocator*)s;
    // buffers are always linear resources
    const LaharGranularityClass granularity_class = LAHAR_GC_LINEAR;
    LaharFreelistMemReqs buf_reqs = ZINIT;
    VkBuffer buffer = VK_NULL_HANDLE;
    uint32_t required_flags = 0;
    uint32_t preferred_flags = 0;
    uint32_t type_mask = 0;
    uint32_t type_index = 0;

    #if defined(VK_VERSION_1_1) || (defined(VK_KHR_get_memory_requirements2) && defined(VK_KHR_dedicated_allocation))
    VkMemoryDedicatedAllocateInfoKHR dedicated_info = ZINIT;
    dedicated_info.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR;
    #endif

    void* create_pnext = NULL;
    LaharFreelistSubAllocation* chosen_suballoc = NULL;

    // TODO: if buffer device address support is ever added, chunks backing
    // such buffers need VkMemoryAllocateFlagsInfo with the DEVICE_ADDRESS bit

    if ((err = __lahar_fl_resolve_mem_flags(alloc_info, &required_flags, &preferred_flags))) {
        goto end;
    }

    if ((lahar->vkresult = vkCreateBuffer(lahar->device, info, lahar->vkalloc, &buffer)) != VK_SUCCESS) {
        err = LAHAR_ERR_VK_ERR;
        goto end;
    }

    __lahar_fl_buffer_mem_reqs(buffer, &buf_reqs);

    #if defined(VK_VERSION_1_1) || (defined(VK_KHR_get_memory_requirements2) && defined(VK_KHR_dedicated_allocation))
    if (buf_reqs.requires_dedicated || buf_reqs.prefers_dedicated) {
        dedicated_info.buffer = buffer;
        create_pnext = (void*)&dedicated_info;
    }
    #endif

    type_mask = buf_reqs.type_mask;

    // Retry every memory type until we can allocate something or run out of types
    do {
        if ((err = __lahar_fl_select_mem(type_mask, required_flags, preferred_flags, &type_index))) {
            goto end;
        }

        if (!(err = __lahar_fl_suballoc(self, type_index, &buf_reqs, granularity_class, create_pnext, &chosen_suballoc))) {
            break; // got one
        }

        if (err != LAHAR_ERR_VK_ERR || lahar->vkresult != VK_ERROR_OUT_OF_DEVICE_MEMORY) {
            goto end;
        }

        type_mask &= ~(1u << type_index);
    } while (type_mask);

    if (!chosen_suballoc) { goto end; } // mask exhausted, err still holds the OOM

    if ((lahar->vkresult = vkBindBufferMemory(lahar->device, buffer, chosen_suballoc->chunk->handle, chosen_suballoc->offset))) {
        err = LAHAR_ERR_VK_ERR;
        goto end;
    }

end:
    if (err) {
        if (buffer) {
            vkDestroyBuffer(lahar->device, buffer, lahar->vkalloc);
        }

        if (chosen_suballoc) {
            // bind failed after suballocating; hand the block back. Eager
            // reclaim inside release also covers the fresh-chunk case.
            __lahar_fl_suballoc_release(self, chosen_suballoc);
        }
    }
    else {
        *buffer_out = buffer;
        *alloc_out = (LaharAllocation)(void*)chosen_suballoc;
    }

    #ifdef LAHAR_DEBUG
    __lahar_fl_validate(self);
    #endif

    return err;
}

// Method
static uint32_t __lahar_fl_free_buffer(
    void* s,
    VkBuffer buffer,
    LaharAllocation allocation
) {
    if (!s || !buffer || !allocation) {
        return LAHAR_ERR_ILLEGAL_PARAMS;
    }

    LaharFreelistAllocator* self = (LaharFreelistAllocator*)s;
    LaharFreelistSubAllocation* suballoc = (LaharFreelistSubAllocation*)allocation;

    vkDestroyBuffer(lahar->device, buffer, lahar->vkalloc);
    __lahar_fl_suballoc_release(self, suballoc);

    #ifdef LAHAR_DEBUG
    __lahar_fl_validate(self);
    #endif

    return LAHAR_ERR_SUCCESS;
}

// Method
static uint32_t __lahar_fl_map(
    void* s,
    LaharAllocation allocation,
    void** out
) {
    if (!s || !allocation || !out) {
        return LAHAR_ERR_ILLEGAL_PARAMS;
    }

    LaharFreelistSubAllocation* suballoc = (LaharFreelistSubAllocation*)allocation;
    LaharFreelistMemChunk* chunk = suballoc->chunk;

    LAHAR_ASSERT(chunk && !suballoc->free);

    if (!chunk->mapped) {
        LAHAR_ASSERT(chunk->map_count == 0);

        if ((lahar->vkresult = vkMapMemory(lahar->device, chunk->handle, 0, VK_WHOLE_SIZE, 0, &chunk->mapped)) != VK_SUCCESS) {
            chunk->mapped = NULL; // driver contract, but don't trust it on failure
            return LAHAR_ERR_VK_ERR;
        }
    }

    chunk->map_count++;
    *out = (uint8_t*)chunk->mapped + suballoc->offset;

    return LAHAR_ERR_SUCCESS;
}

// Method
static uint32_t __lahar_fl_unmap(
    void* s,
    LaharAllocation allocation
) {
    if (!s || !allocation) {
        return LAHAR_ERR_ILLEGAL_PARAMS;
    }

    LaharFreelistSubAllocation* suballoc = (LaharFreelistSubAllocation*)allocation;
    LaharFreelistMemChunk* chunk = suballoc->chunk;

    LAHAR_ASSERT(chunk);

    if (chunk->map_count == 0 || !chunk->mapped) {
        return LAHAR_ERR_ILLEGAL_PARAMS; // unmap without a live map
    }

    if (--chunk->map_count == 0) {
        vkUnmapMemory(lahar->device, chunk->handle);
        chunk->mapped = NULL;
    }

    return LAHAR_ERR_SUCCESS;
}

// Util: translate a block-relative range to a chunk-relative VkMappedMemoryRange,
// clamped to the block and aligned to nonCoherentAtomSize as the spec requires
static uint32_t __lahar_fl_mapped_range(
    const LaharFreelistSubAllocation* suballoc,
    uint64_t off,
    uint64_t size,
    VkMappedMemoryRange* out
) {
    const LaharFreelistMemChunk* chunk = suballoc->chunk;

    if (off > suballoc->size) {
        return LAHAR_ERR_ILLEGAL_PARAMS;
    }

    if (size == VK_WHOLE_SIZE) {
        size = suballoc->size - off;
    }

    if (size > suballoc->size - off) {
        return LAHAR_ERR_ILLEGAL_PARAMS;
    }

    const VkDeviceSize atom = lahar->physdev_info.properties.limits.nonCoherentAtomSize;

    VkDeviceSize begin = suballoc->offset + off;
    VkDeviceSize end = begin + size;

    // round begin down and end up to atom boundaries; clamp end to the chunk
    // (block offsets/sizes are quantum-aligned, not atom-aligned)
    begin -= begin % atom;
    end += (atom - (end % atom)) % atom;
    if (end > chunk->size) { end = chunk->size; }

    memset(out, 0, sizeof(*out));
    out->sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    out->memory = chunk->handle;
    out->offset = begin;
    out->size = end - begin;

    return LAHAR_ERR_SUCCESS;
}

// Method
static uint32_t __lahar_fl_flush(
    void* s,
    LaharAllocation allocation,
    uint64_t off,
    uint64_t size
) {
    if (!s || !allocation) {
        return LAHAR_ERR_ILLEGAL_PARAMS;
    }

    LaharFreelistSubAllocation* suballoc = (LaharFreelistSubAllocation*)allocation;
    uint32_t err;
    VkMappedMemoryRange range;

    if ((err = __lahar_fl_mapped_range(suballoc, off, size, &range))) {
        return err;
    }

    if ((lahar->vkresult = vkFlushMappedMemoryRanges(lahar->device, 1, &range)) != VK_SUCCESS) {
        return LAHAR_ERR_VK_ERR;
    }

    return LAHAR_ERR_SUCCESS;
}

// Method
static uint32_t __lahar_fl_invalidate(
    void* s,
    LaharAllocation allocation,
    uint64_t off,
    uint64_t size
) {
    if (!s || !allocation) {
        return LAHAR_ERR_ILLEGAL_PARAMS;
    }

    LaharFreelistSubAllocation* suballoc = (LaharFreelistSubAllocation*)allocation;
    uint32_t err;
    VkMappedMemoryRange range;

    if ((err = __lahar_fl_mapped_range(suballoc, off, size, &range))) {
        return err;
    }

    if ((lahar->vkresult = vkInvalidateMappedMemoryRanges(lahar->device, 1, &range)) != VK_SUCCESS) {
        return LAHAR_ERR_VK_ERR;
    }

    return LAHAR_ERR_SUCCESS;
}

LaharAllocator* lahar_allocator_freelist_init(void) {
    LaharFreelistAllocator* allocator = (LaharFreelistAllocator*)lahar_malloc(sizeof(*allocator));
    if (!allocator) {
        lahar_error("OOM");
        return NULL;
    }

    memset(allocator, 0, sizeof(*allocator));

    allocator->vtable.alloc_image = &__lahar_fl_alloc_image;
    allocator->vtable.free_image = &__lahar_fl_free_image;
    allocator->vtable.alloc_buffer = &__lahar_fl_alloc_buffer;
    allocator->vtable.free_buffer = &__lahar_fl_free_buffer;
    allocator->vtable.map = &__lahar_fl_map;
    allocator->vtable.unmap = &__lahar_fl_unmap;
    allocator->vtable.flush = &__lahar_fl_flush;
    allocator->vtable.invalidate = &__lahar_fl_invalidate;

    allocator->magic = LAHAR_FREELIST_MAGIC;
    return (LaharAllocator*)allocator;
}

void lahar_allocator_freelist_deinit(LaharAllocator* allocator) {
    if (!allocator) { return; }

    LaharFreelistAllocator* cast = (LaharFreelistAllocator*)allocator;
    if (cast->magic != LAHAR_FREELIST_MAGIC) {
        lahar_error("Magic number does not match, likely not a freelist allocator we produced");
        return;
    }

    for (size_t i = 0; i < LAHAR_FREELIST_MEM_TYPES; i++) {
        LaharFreelistChunkVec* vector = &cast->types[i];

        while (vector->count) {
            __lahar_fl_free_memchunk((LaharFreelistAllocator*)allocator, vector->chunks[0]);
        }

        lahar_free(vector->chunks);
    }

    memset(cast, 0, sizeof(*cast));
    lahar_free(cast);
}
































/* ============================================================================
 * GPU Memory
 * ============================================================================
 * Dispatch layer over lahar->gpu_allocator. Deliberately dumb: the only logic
 * here is argument validation and the map/copy/flush/unmap sequencing, so that
 * there is exactly one place that knows how to drive the vtable.
 */

/* Util: fetch the active allocator, or NULL if lahar has not been built */
static LaharAllocator* __lahar_allocator(void) {
    if (!lahar->gpu_allocator) {
        lahar_error("No GPU allocator: lahar_build() has not run, or it failed");
        return NULL;
    }

    return lahar->gpu_allocator;
}

uint32_t lahar_buffer_create(
    const VkBufferCreateInfo* info,
    const LaharAllocationCreateInfo* alloc_info,
    VkBuffer* buffer_out,
    LaharAllocation* alloc_out
) {
    if (!info || !alloc_info || !buffer_out || !alloc_out) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    LaharAllocator* alloc = __lahar_allocator();
    if (!alloc) { return LAHAR_ERR_INVALID_STATE; }

    return alloc->alloc_buffer(alloc, info, alloc_info, buffer_out, alloc_out);
}

uint32_t lahar_buffer_create_simple(
    uint64_t size,
    VkBufferUsageFlags usage,
    LaharMemoryUsage mem_usage,
    LaharAllocationRole role,
    VkBuffer* buffer_out,
    LaharAllocation* alloc_out
) {
    if (!size) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    VkBufferCreateInfo info = ZINIT;
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = size;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    LaharAllocationCreateInfo alloc_info = ZINIT;
    alloc_info.usage = mem_usage;
    alloc_info.role = role;

    return lahar_buffer_create(&info, &alloc_info, buffer_out, alloc_out);
}

uint32_t lahar_buffer_destroy(VkBuffer buffer, LaharAllocation alloc_handle) {
    // Tolerate the null case so teardown paths do not need a guard per resource
    if (buffer == VK_NULL_HANDLE && !alloc_handle) { return LAHAR_ERR_SUCCESS; }
    if (buffer == VK_NULL_HANDLE || !alloc_handle) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    LaharAllocator* alloc = __lahar_allocator();
    if (!alloc) { return LAHAR_ERR_INVALID_STATE; }

    return alloc->free_buffer(alloc, buffer, alloc_handle);
}

uint32_t lahar_image_create(
    const VkImageCreateInfo* info,
    const LaharAllocationCreateInfo* alloc_info,
    VkImage* image_out,
    LaharAllocation* alloc_out
) {
    if (!info || !alloc_info || !image_out || !alloc_out) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    LaharAllocator* alloc = __lahar_allocator();
    if (!alloc) { return LAHAR_ERR_INVALID_STATE; }

    return alloc->alloc_image(alloc, info, alloc_info, image_out, alloc_out);
}

uint32_t lahar_image_destroy(VkImage image, LaharAllocation alloc_handle) {
    if (image == VK_NULL_HANDLE && !alloc_handle) { return LAHAR_ERR_SUCCESS; }
    if (image == VK_NULL_HANDLE || !alloc_handle) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    LaharAllocator* alloc = __lahar_allocator();
    if (!alloc) { return LAHAR_ERR_INVALID_STATE; }

    return alloc->free_image(alloc, image, alloc_handle);
}

uint32_t lahar_memory_map(LaharAllocation alloc_handle, void** out) {
    if (!alloc_handle || !out) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    LaharAllocator* alloc = __lahar_allocator();
    if (!alloc) { return LAHAR_ERR_INVALID_STATE; }

    return alloc->map(alloc, alloc_handle, out);
}

uint32_t lahar_memory_unmap(LaharAllocation alloc_handle) {
    if (!alloc_handle) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    LaharAllocator* alloc = __lahar_allocator();
    if (!alloc) { return LAHAR_ERR_INVALID_STATE; }

    return alloc->unmap(alloc, alloc_handle);
}

uint32_t lahar_memory_flush(LaharAllocation alloc_handle, uint64_t offset, uint64_t size) {
    if (!alloc_handle) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    LaharAllocator* alloc = __lahar_allocator();
    if (!alloc) { return LAHAR_ERR_INVALID_STATE; }

    return alloc->flush(alloc, alloc_handle, offset, size);
}

uint32_t lahar_memory_invalidate(LaharAllocation alloc_handle, uint64_t offset, uint64_t size) {
    if (!alloc_handle) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    LaharAllocator* alloc = __lahar_allocator();
    if (!alloc) { return LAHAR_ERR_INVALID_STATE; }

    return alloc->invalidate(alloc, alloc_handle, offset, size);
}

uint32_t lahar_memory_write(LaharAllocation alloc_handle, uint64_t offset, const void* data, uint64_t size) {
    if (!alloc_handle || !data) { return LAHAR_ERR_ILLEGAL_PARAMS; }
    if (!size) { return LAHAR_ERR_SUCCESS; }

    LaharAllocator* alloc = __lahar_allocator();
    if (!alloc) { return LAHAR_ERR_INVALID_STATE; }

    uint32_t err = LAHAR_ERR_SUCCESS;
    void* mapped = NULL;

    if ((err = alloc->map(alloc, alloc_handle, &mapped))) { return err; }

    memcpy((uint8_t*)mapped + offset, data, (size_t)size);

    // Flush before unmapping: unmap may drop the mapping entirely, and the write
    // is only guaranteed visible to the device once flushed.
    err = alloc->flush(alloc, alloc_handle, offset, size);

    const uint32_t unmap_err = alloc->unmap(alloc, alloc_handle);

    // The flush failure is the interesting one, so it wins if both fail
    return err ? err : unmap_err;
}

uint32_t lahar_memory_read(LaharAllocation alloc_handle, uint64_t offset, void* data, uint64_t size) {
    if (!alloc_handle || !data) { return LAHAR_ERR_ILLEGAL_PARAMS; }
    if (!size) { return LAHAR_ERR_SUCCESS; }

    LaharAllocator* alloc = __lahar_allocator();
    if (!alloc) { return LAHAR_ERR_INVALID_STATE; }

    uint32_t err = LAHAR_ERR_SUCCESS;
    void* mapped = NULL;

    if ((err = alloc->map(alloc, alloc_handle, &mapped))) { return err; }

    // Invalidate first, so what we copy reflects any device writes
    if ((err = alloc->invalidate(alloc, alloc_handle, offset, size))) {
        alloc->unmap(alloc, alloc_handle);
        return err;
    }

    memcpy(data, (const uint8_t*)mapped + offset, (size_t)size);

    return alloc->unmap(alloc, alloc_handle);
}






/* LAHAR_VK_PROTOTYPES_C */
#if defined(VK_VERSION_1_0)
PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
PFN_vkAllocateMemory vkAllocateMemory;
PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
PFN_vkBindBufferMemory vkBindBufferMemory;
PFN_vkBindImageMemory vkBindImageMemory;
PFN_vkCmdBeginQuery vkCmdBeginQuery;
PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass;
PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer;
PFN_vkCmdBindPipeline vkCmdBindPipeline;
PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers;
PFN_vkCmdBlitImage vkCmdBlitImage;
PFN_vkCmdClearAttachments vkCmdClearAttachments;
PFN_vkCmdClearColorImage vkCmdClearColorImage;
PFN_vkCmdClearDepthStencilImage vkCmdClearDepthStencilImage;
PFN_vkCmdCopyBuffer vkCmdCopyBuffer;
PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage;
PFN_vkCmdCopyImage vkCmdCopyImage;
PFN_vkCmdCopyImageToBuffer vkCmdCopyImageToBuffer;
PFN_vkCmdCopyQueryPoolResults vkCmdCopyQueryPoolResults;
PFN_vkCmdDispatch vkCmdDispatch;
PFN_vkCmdDispatchIndirect vkCmdDispatchIndirect;
PFN_vkCmdDraw vkCmdDraw;
PFN_vkCmdDrawIndexed vkCmdDrawIndexed;
PFN_vkCmdDrawIndexedIndirect vkCmdDrawIndexedIndirect;
PFN_vkCmdDrawIndirect vkCmdDrawIndirect;
PFN_vkCmdEndQuery vkCmdEndQuery;
PFN_vkCmdEndRenderPass vkCmdEndRenderPass;
PFN_vkCmdExecuteCommands vkCmdExecuteCommands;
PFN_vkCmdFillBuffer vkCmdFillBuffer;
PFN_vkCmdNextSubpass vkCmdNextSubpass;
PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
PFN_vkCmdPushConstants vkCmdPushConstants;
PFN_vkCmdResetEvent vkCmdResetEvent;
PFN_vkCmdResetQueryPool vkCmdResetQueryPool;
PFN_vkCmdResolveImage vkCmdResolveImage;
PFN_vkCmdSetBlendConstants vkCmdSetBlendConstants;
PFN_vkCmdSetDepthBias vkCmdSetDepthBias;
PFN_vkCmdSetDepthBounds vkCmdSetDepthBounds;
PFN_vkCmdSetEvent vkCmdSetEvent;
PFN_vkCmdSetLineWidth vkCmdSetLineWidth;
PFN_vkCmdSetScissor vkCmdSetScissor;
PFN_vkCmdSetStencilCompareMask vkCmdSetStencilCompareMask;
PFN_vkCmdSetStencilReference vkCmdSetStencilReference;
PFN_vkCmdSetStencilWriteMask vkCmdSetStencilWriteMask;
PFN_vkCmdSetViewport vkCmdSetViewport;
PFN_vkCmdUpdateBuffer vkCmdUpdateBuffer;
PFN_vkCmdWaitEvents vkCmdWaitEvents;
PFN_vkCmdWriteTimestamp vkCmdWriteTimestamp;
PFN_vkCreateBuffer vkCreateBuffer;
PFN_vkCreateBufferView vkCreateBufferView;
PFN_vkCreateCommandPool vkCreateCommandPool;
PFN_vkCreateComputePipelines vkCreateComputePipelines;
PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
PFN_vkCreateDevice vkCreateDevice;
PFN_vkCreateEvent vkCreateEvent;
PFN_vkCreateFence vkCreateFence;
PFN_vkCreateFramebuffer vkCreateFramebuffer;
PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines;
PFN_vkCreateImage vkCreateImage;
PFN_vkCreateImageView vkCreateImageView;
PFN_vkCreateInstance vkCreateInstance;
PFN_vkCreatePipelineCache vkCreatePipelineCache;
PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
PFN_vkCreateQueryPool vkCreateQueryPool;
PFN_vkCreateRenderPass vkCreateRenderPass;
PFN_vkCreateSampler vkCreateSampler;
PFN_vkCreateSemaphore vkCreateSemaphore;
PFN_vkCreateShaderModule vkCreateShaderModule;
PFN_vkDestroyBuffer vkDestroyBuffer;
PFN_vkDestroyBufferView vkDestroyBufferView;
PFN_vkDestroyCommandPool vkDestroyCommandPool;
PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
PFN_vkDestroyDevice vkDestroyDevice;
PFN_vkDestroyEvent vkDestroyEvent;
PFN_vkDestroyFence vkDestroyFence;
PFN_vkDestroyFramebuffer vkDestroyFramebuffer;
PFN_vkDestroyImage vkDestroyImage;
PFN_vkDestroyImageView vkDestroyImageView;
PFN_vkDestroyInstance vkDestroyInstance;
PFN_vkDestroyPipeline vkDestroyPipeline;
PFN_vkDestroyPipelineCache vkDestroyPipelineCache;
PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
PFN_vkDestroyQueryPool vkDestroyQueryPool;
PFN_vkDestroyRenderPass vkDestroyRenderPass;
PFN_vkDestroySampler vkDestroySampler;
PFN_vkDestroySemaphore vkDestroySemaphore;
PFN_vkDestroyShaderModule vkDestroyShaderModule;
PFN_vkDeviceWaitIdle vkDeviceWaitIdle;
PFN_vkEndCommandBuffer vkEndCommandBuffer;
PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties;
PFN_vkEnumerateDeviceLayerProperties vkEnumerateDeviceLayerProperties;
PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties;
PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties;
PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
PFN_vkFlushMappedMemoryRanges vkFlushMappedMemoryRanges;
PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
PFN_vkFreeDescriptorSets vkFreeDescriptorSets;
PFN_vkFreeMemory vkFreeMemory;
PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
PFN_vkGetDeviceMemoryCommitment vkGetDeviceMemoryCommitment;
PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
PFN_vkGetDeviceQueue vkGetDeviceQueue;
PFN_vkGetEventStatus vkGetEventStatus;
PFN_vkGetFenceStatus vkGetFenceStatus;
PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
PFN_vkGetImageSparseMemoryRequirements vkGetImageSparseMemoryRequirements;
PFN_vkGetImageSubresourceLayout vkGetImageSubresourceLayout;
PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures;
PFN_vkGetPhysicalDeviceFormatProperties vkGetPhysicalDeviceFormatProperties;
PFN_vkGetPhysicalDeviceImageFormatProperties vkGetPhysicalDeviceImageFormatProperties;
PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
PFN_vkGetPhysicalDeviceSparseImageFormatProperties vkGetPhysicalDeviceSparseImageFormatProperties;
PFN_vkGetPipelineCacheData vkGetPipelineCacheData;
PFN_vkGetQueryPoolResults vkGetQueryPoolResults;
PFN_vkGetRenderAreaGranularity vkGetRenderAreaGranularity;
PFN_vkInvalidateMappedMemoryRanges vkInvalidateMappedMemoryRanges;
PFN_vkMapMemory vkMapMemory;
PFN_vkMergePipelineCaches vkMergePipelineCaches;
PFN_vkQueueBindSparse vkQueueBindSparse;
PFN_vkQueueSubmit vkQueueSubmit;
PFN_vkQueueWaitIdle vkQueueWaitIdle;
PFN_vkResetCommandBuffer vkResetCommandBuffer;
PFN_vkResetCommandPool vkResetCommandPool;
PFN_vkResetDescriptorPool vkResetDescriptorPool;
PFN_vkResetEvent vkResetEvent;
PFN_vkResetFences vkResetFences;
PFN_vkSetEvent vkSetEvent;
PFN_vkUnmapMemory vkUnmapMemory;
PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
PFN_vkWaitForFences vkWaitForFences;
#endif /* defined(VK_VERSION_1_0) */
#if defined(VK_VERSION_1_1)
PFN_vkBindBufferMemory2 vkBindBufferMemory2;
PFN_vkBindImageMemory2 vkBindImageMemory2;
PFN_vkCmdDispatchBase vkCmdDispatchBase;
PFN_vkCmdSetDeviceMask vkCmdSetDeviceMask;
PFN_vkCreateDescriptorUpdateTemplate vkCreateDescriptorUpdateTemplate;
PFN_vkCreateSamplerYcbcrConversion vkCreateSamplerYcbcrConversion;
PFN_vkDestroyDescriptorUpdateTemplate vkDestroyDescriptorUpdateTemplate;
PFN_vkDestroySamplerYcbcrConversion vkDestroySamplerYcbcrConversion;
PFN_vkEnumerateInstanceVersion vkEnumerateInstanceVersion;
PFN_vkEnumeratePhysicalDeviceGroups vkEnumeratePhysicalDeviceGroups;
PFN_vkGetBufferMemoryRequirements2 vkGetBufferMemoryRequirements2;
PFN_vkGetDescriptorSetLayoutSupport vkGetDescriptorSetLayoutSupport;
PFN_vkGetDeviceGroupPeerMemoryFeatures vkGetDeviceGroupPeerMemoryFeatures;
PFN_vkGetDeviceQueue2 vkGetDeviceQueue2;
PFN_vkGetImageMemoryRequirements2 vkGetImageMemoryRequirements2;
PFN_vkGetImageSparseMemoryRequirements2 vkGetImageSparseMemoryRequirements2;
PFN_vkGetPhysicalDeviceExternalBufferProperties vkGetPhysicalDeviceExternalBufferProperties;
PFN_vkGetPhysicalDeviceExternalFenceProperties vkGetPhysicalDeviceExternalFenceProperties;
PFN_vkGetPhysicalDeviceExternalSemaphoreProperties vkGetPhysicalDeviceExternalSemaphoreProperties;
PFN_vkGetPhysicalDeviceFeatures2 vkGetPhysicalDeviceFeatures2;
PFN_vkGetPhysicalDeviceFormatProperties2 vkGetPhysicalDeviceFormatProperties2;
PFN_vkGetPhysicalDeviceImageFormatProperties2 vkGetPhysicalDeviceImageFormatProperties2;
PFN_vkGetPhysicalDeviceMemoryProperties2 vkGetPhysicalDeviceMemoryProperties2;
PFN_vkGetPhysicalDeviceProperties2 vkGetPhysicalDeviceProperties2;
PFN_vkGetPhysicalDeviceQueueFamilyProperties2 vkGetPhysicalDeviceQueueFamilyProperties2;
PFN_vkGetPhysicalDeviceSparseImageFormatProperties2 vkGetPhysicalDeviceSparseImageFormatProperties2;
PFN_vkTrimCommandPool vkTrimCommandPool;
PFN_vkUpdateDescriptorSetWithTemplate vkUpdateDescriptorSetWithTemplate;
#endif /* defined(VK_VERSION_1_1) */
#if defined(VK_VERSION_1_2)
PFN_vkCmdBeginRenderPass2 vkCmdBeginRenderPass2;
PFN_vkCmdDrawIndexedIndirectCount vkCmdDrawIndexedIndirectCount;
PFN_vkCmdDrawIndirectCount vkCmdDrawIndirectCount;
PFN_vkCmdEndRenderPass2 vkCmdEndRenderPass2;
PFN_vkCmdNextSubpass2 vkCmdNextSubpass2;
PFN_vkCreateRenderPass2 vkCreateRenderPass2;
PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress;
PFN_vkGetBufferOpaqueCaptureAddress vkGetBufferOpaqueCaptureAddress;
PFN_vkGetDeviceMemoryOpaqueCaptureAddress vkGetDeviceMemoryOpaqueCaptureAddress;
PFN_vkGetSemaphoreCounterValue vkGetSemaphoreCounterValue;
PFN_vkResetQueryPool vkResetQueryPool;
PFN_vkSignalSemaphore vkSignalSemaphore;
PFN_vkWaitSemaphores vkWaitSemaphores;
#endif /* defined(VK_VERSION_1_2) */
#if defined(VK_VERSION_1_3)
PFN_vkCmdBeginRendering vkCmdBeginRendering;
PFN_vkCmdBindVertexBuffers2 vkCmdBindVertexBuffers2;
PFN_vkCmdBlitImage2 vkCmdBlitImage2;
PFN_vkCmdCopyBuffer2 vkCmdCopyBuffer2;
PFN_vkCmdCopyBufferToImage2 vkCmdCopyBufferToImage2;
PFN_vkCmdCopyImage2 vkCmdCopyImage2;
PFN_vkCmdCopyImageToBuffer2 vkCmdCopyImageToBuffer2;
PFN_vkCmdEndRendering vkCmdEndRendering;
PFN_vkCmdPipelineBarrier2 vkCmdPipelineBarrier2;
PFN_vkCmdResetEvent2 vkCmdResetEvent2;
PFN_vkCmdResolveImage2 vkCmdResolveImage2;
PFN_vkCmdSetCullMode vkCmdSetCullMode;
PFN_vkCmdSetDepthBiasEnable vkCmdSetDepthBiasEnable;
PFN_vkCmdSetDepthBoundsTestEnable vkCmdSetDepthBoundsTestEnable;
PFN_vkCmdSetDepthCompareOp vkCmdSetDepthCompareOp;
PFN_vkCmdSetDepthTestEnable vkCmdSetDepthTestEnable;
PFN_vkCmdSetDepthWriteEnable vkCmdSetDepthWriteEnable;
PFN_vkCmdSetEvent2 vkCmdSetEvent2;
PFN_vkCmdSetFrontFace vkCmdSetFrontFace;
PFN_vkCmdSetPrimitiveRestartEnable vkCmdSetPrimitiveRestartEnable;
PFN_vkCmdSetPrimitiveTopology vkCmdSetPrimitiveTopology;
PFN_vkCmdSetRasterizerDiscardEnable vkCmdSetRasterizerDiscardEnable;
PFN_vkCmdSetScissorWithCount vkCmdSetScissorWithCount;
PFN_vkCmdSetStencilOp vkCmdSetStencilOp;
PFN_vkCmdSetStencilTestEnable vkCmdSetStencilTestEnable;
PFN_vkCmdSetViewportWithCount vkCmdSetViewportWithCount;
PFN_vkCmdWaitEvents2 vkCmdWaitEvents2;
PFN_vkCmdWriteTimestamp2 vkCmdWriteTimestamp2;
PFN_vkCreatePrivateDataSlot vkCreatePrivateDataSlot;
PFN_vkDestroyPrivateDataSlot vkDestroyPrivateDataSlot;
PFN_vkGetDeviceBufferMemoryRequirements vkGetDeviceBufferMemoryRequirements;
PFN_vkGetDeviceImageMemoryRequirements vkGetDeviceImageMemoryRequirements;
PFN_vkGetDeviceImageSparseMemoryRequirements vkGetDeviceImageSparseMemoryRequirements;
PFN_vkGetPhysicalDeviceToolProperties vkGetPhysicalDeviceToolProperties;
PFN_vkGetPrivateData vkGetPrivateData;
PFN_vkQueueSubmit2 vkQueueSubmit2;
PFN_vkSetPrivateData vkSetPrivateData;
#endif /* defined(VK_VERSION_1_3) */
#if defined(VK_VERSION_1_4)
PFN_vkCmdBindDescriptorSets2 vkCmdBindDescriptorSets2;
PFN_vkCmdBindIndexBuffer2 vkCmdBindIndexBuffer2;
PFN_vkCmdPushConstants2 vkCmdPushConstants2;
PFN_vkCmdPushDescriptorSet vkCmdPushDescriptorSet;
PFN_vkCmdPushDescriptorSet2 vkCmdPushDescriptorSet2;
PFN_vkCmdPushDescriptorSetWithTemplate vkCmdPushDescriptorSetWithTemplate;
PFN_vkCmdPushDescriptorSetWithTemplate2 vkCmdPushDescriptorSetWithTemplate2;
PFN_vkCmdSetLineStipple vkCmdSetLineStipple;
PFN_vkCmdSetRenderingAttachmentLocations vkCmdSetRenderingAttachmentLocations;
PFN_vkCmdSetRenderingInputAttachmentIndices vkCmdSetRenderingInputAttachmentIndices;
PFN_vkCopyImageToImage vkCopyImageToImage;
PFN_vkCopyImageToMemory vkCopyImageToMemory;
PFN_vkCopyMemoryToImage vkCopyMemoryToImage;
PFN_vkGetDeviceImageSubresourceLayout vkGetDeviceImageSubresourceLayout;
PFN_vkGetImageSubresourceLayout2 vkGetImageSubresourceLayout2;
PFN_vkGetRenderingAreaGranularity vkGetRenderingAreaGranularity;
PFN_vkMapMemory2 vkMapMemory2;
PFN_vkTransitionImageLayout vkTransitionImageLayout;
PFN_vkUnmapMemory2 vkUnmapMemory2;
#endif /* defined(VK_VERSION_1_4) */
#if defined(VK_AMDX_shader_enqueue)
PFN_vkCmdDispatchGraphAMDX vkCmdDispatchGraphAMDX;
PFN_vkCmdDispatchGraphIndirectAMDX vkCmdDispatchGraphIndirectAMDX;
PFN_vkCmdDispatchGraphIndirectCountAMDX vkCmdDispatchGraphIndirectCountAMDX;
PFN_vkCmdInitializeGraphScratchMemoryAMDX vkCmdInitializeGraphScratchMemoryAMDX;
PFN_vkCreateExecutionGraphPipelinesAMDX vkCreateExecutionGraphPipelinesAMDX;
PFN_vkGetExecutionGraphPipelineNodeIndexAMDX vkGetExecutionGraphPipelineNodeIndexAMDX;
PFN_vkGetExecutionGraphPipelineScratchSizeAMDX vkGetExecutionGraphPipelineScratchSizeAMDX;
#endif /* defined(VK_AMDX_shader_enqueue) */
#if defined(VK_AMD_anti_lag)
PFN_vkAntiLagUpdateAMD vkAntiLagUpdateAMD;
#endif /* defined(VK_AMD_anti_lag) */
#if defined(VK_AMD_buffer_marker)
PFN_vkCmdWriteBufferMarkerAMD vkCmdWriteBufferMarkerAMD;
#endif /* defined(VK_AMD_buffer_marker) */
#if defined(VK_AMD_buffer_marker) && (defined(VK_VERSION_1_3) || defined(VK_KHR_synchronization2))
PFN_vkCmdWriteBufferMarker2AMD vkCmdWriteBufferMarker2AMD;
#endif /* defined(VK_AMD_buffer_marker) && (defined(VK_VERSION_1_3) || defined(VK_KHR_synchronization2)) */
#if defined(VK_AMD_display_native_hdr)
PFN_vkSetLocalDimmingAMD vkSetLocalDimmingAMD;
#endif /* defined(VK_AMD_display_native_hdr) */
#if defined(VK_AMD_draw_indirect_count)
PFN_vkCmdDrawIndexedIndirectCountAMD vkCmdDrawIndexedIndirectCountAMD;
PFN_vkCmdDrawIndirectCountAMD vkCmdDrawIndirectCountAMD;
#endif /* defined(VK_AMD_draw_indirect_count) */
#if defined(VK_AMD_shader_info)
PFN_vkGetShaderInfoAMD vkGetShaderInfoAMD;
#endif /* defined(VK_AMD_shader_info) */
#if defined(VK_ANDROID_external_memory_android_hardware_buffer)
PFN_vkGetAndroidHardwareBufferPropertiesANDROID vkGetAndroidHardwareBufferPropertiesANDROID;
PFN_vkGetMemoryAndroidHardwareBufferANDROID vkGetMemoryAndroidHardwareBufferANDROID;
#endif /* defined(VK_ANDROID_external_memory_android_hardware_buffer) */
#if defined(VK_ARM_data_graph)
PFN_vkBindDataGraphPipelineSessionMemoryARM vkBindDataGraphPipelineSessionMemoryARM;
PFN_vkCmdDispatchDataGraphARM vkCmdDispatchDataGraphARM;
PFN_vkCreateDataGraphPipelineSessionARM vkCreateDataGraphPipelineSessionARM;
PFN_vkCreateDataGraphPipelinesARM vkCreateDataGraphPipelinesARM;
PFN_vkDestroyDataGraphPipelineSessionARM vkDestroyDataGraphPipelineSessionARM;
PFN_vkGetDataGraphPipelineAvailablePropertiesARM vkGetDataGraphPipelineAvailablePropertiesARM;
PFN_vkGetDataGraphPipelinePropertiesARM vkGetDataGraphPipelinePropertiesARM;
PFN_vkGetDataGraphPipelineSessionBindPointRequirementsARM vkGetDataGraphPipelineSessionBindPointRequirementsARM;
PFN_vkGetDataGraphPipelineSessionMemoryRequirementsARM vkGetDataGraphPipelineSessionMemoryRequirementsARM;
PFN_vkGetPhysicalDeviceQueueFamilyDataGraphProcessingEnginePropertiesARM vkGetPhysicalDeviceQueueFamilyDataGraphProcessingEnginePropertiesARM;
PFN_vkGetPhysicalDeviceQueueFamilyDataGraphPropertiesARM vkGetPhysicalDeviceQueueFamilyDataGraphPropertiesARM;
#endif /* defined(VK_ARM_data_graph) */
#if defined(VK_ARM_tensors)
PFN_vkBindTensorMemoryARM vkBindTensorMemoryARM;
PFN_vkCmdCopyTensorARM vkCmdCopyTensorARM;
PFN_vkCreateTensorARM vkCreateTensorARM;
PFN_vkCreateTensorViewARM vkCreateTensorViewARM;
PFN_vkDestroyTensorARM vkDestroyTensorARM;
PFN_vkDestroyTensorViewARM vkDestroyTensorViewARM;
PFN_vkGetDeviceTensorMemoryRequirementsARM vkGetDeviceTensorMemoryRequirementsARM;
PFN_vkGetPhysicalDeviceExternalTensorPropertiesARM vkGetPhysicalDeviceExternalTensorPropertiesARM;
PFN_vkGetTensorMemoryRequirementsARM vkGetTensorMemoryRequirementsARM;
#endif /* defined(VK_ARM_tensors) */
#if defined(VK_ARM_tensors) && defined(VK_EXT_descriptor_buffer)
PFN_vkGetTensorOpaqueCaptureDescriptorDataARM vkGetTensorOpaqueCaptureDescriptorDataARM;
PFN_vkGetTensorViewOpaqueCaptureDescriptorDataARM vkGetTensorViewOpaqueCaptureDescriptorDataARM;
#endif /* defined(VK_ARM_tensors) && defined(VK_EXT_descriptor_buffer) */
#if defined(VK_EXT_acquire_drm_display)
PFN_vkAcquireDrmDisplayEXT vkAcquireDrmDisplayEXT;
PFN_vkGetDrmDisplayEXT vkGetDrmDisplayEXT;
#endif /* defined(VK_EXT_acquire_drm_display) */
#if defined(VK_EXT_acquire_xlib_display)
PFN_vkAcquireXlibDisplayEXT vkAcquireXlibDisplayEXT;
PFN_vkGetRandROutputDisplayEXT vkGetRandROutputDisplayEXT;
#endif /* defined(VK_EXT_acquire_xlib_display) */
#if defined(VK_EXT_attachment_feedback_loop_dynamic_state)
PFN_vkCmdSetAttachmentFeedbackLoopEnableEXT vkCmdSetAttachmentFeedbackLoopEnableEXT;
#endif /* defined(VK_EXT_attachment_feedback_loop_dynamic_state) */
#if defined(VK_EXT_buffer_device_address)
PFN_vkGetBufferDeviceAddressEXT vkGetBufferDeviceAddressEXT;
#endif /* defined(VK_EXT_buffer_device_address) */
#if defined(VK_EXT_calibrated_timestamps)
PFN_vkGetCalibratedTimestampsEXT vkGetCalibratedTimestampsEXT;
PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT vkGetPhysicalDeviceCalibrateableTimeDomainsEXT;
#endif /* defined(VK_EXT_calibrated_timestamps) */
#if defined(VK_EXT_color_write_enable)
PFN_vkCmdSetColorWriteEnableEXT vkCmdSetColorWriteEnableEXT;
#endif /* defined(VK_EXT_color_write_enable) */
#if defined(VK_EXT_conditional_rendering)
PFN_vkCmdBeginConditionalRenderingEXT vkCmdBeginConditionalRenderingEXT;
PFN_vkCmdEndConditionalRenderingEXT vkCmdEndConditionalRenderingEXT;
#endif /* defined(VK_EXT_conditional_rendering) */
#if defined(VK_EXT_debug_marker)
PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBeginEXT;
PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEndEXT;
PFN_vkCmdDebugMarkerInsertEXT vkCmdDebugMarkerInsertEXT;
PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectNameEXT;
PFN_vkDebugMarkerSetObjectTagEXT vkDebugMarkerSetObjectTagEXT;
#endif /* defined(VK_EXT_debug_marker) */
#if defined(VK_EXT_debug_report)
PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
PFN_vkDebugReportMessageEXT vkDebugReportMessageEXT;
PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;
#endif /* defined(VK_EXT_debug_report) */
#if defined(VK_EXT_debug_utils)
PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT;
PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT;
PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT;
PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
PFN_vkQueueBeginDebugUtilsLabelEXT vkQueueBeginDebugUtilsLabelEXT;
PFN_vkQueueEndDebugUtilsLabelEXT vkQueueEndDebugUtilsLabelEXT;
PFN_vkQueueInsertDebugUtilsLabelEXT vkQueueInsertDebugUtilsLabelEXT;
PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;
PFN_vkSetDebugUtilsObjectTagEXT vkSetDebugUtilsObjectTagEXT;
PFN_vkSubmitDebugUtilsMessageEXT vkSubmitDebugUtilsMessageEXT;
#endif /* defined(VK_EXT_debug_utils) */
#if defined(VK_EXT_depth_bias_control)
PFN_vkCmdSetDepthBias2EXT vkCmdSetDepthBias2EXT;
#endif /* defined(VK_EXT_depth_bias_control) */
#if defined(VK_EXT_descriptor_buffer)
PFN_vkCmdBindDescriptorBufferEmbeddedSamplersEXT vkCmdBindDescriptorBufferEmbeddedSamplersEXT;
PFN_vkCmdBindDescriptorBuffersEXT vkCmdBindDescriptorBuffersEXT;
PFN_vkCmdSetDescriptorBufferOffsetsEXT vkCmdSetDescriptorBufferOffsetsEXT;
PFN_vkGetBufferOpaqueCaptureDescriptorDataEXT vkGetBufferOpaqueCaptureDescriptorDataEXT;
PFN_vkGetDescriptorEXT vkGetDescriptorEXT;
PFN_vkGetDescriptorSetLayoutBindingOffsetEXT vkGetDescriptorSetLayoutBindingOffsetEXT;
PFN_vkGetDescriptorSetLayoutSizeEXT vkGetDescriptorSetLayoutSizeEXT;
PFN_vkGetImageOpaqueCaptureDescriptorDataEXT vkGetImageOpaqueCaptureDescriptorDataEXT;
PFN_vkGetImageViewOpaqueCaptureDescriptorDataEXT vkGetImageViewOpaqueCaptureDescriptorDataEXT;
PFN_vkGetSamplerOpaqueCaptureDescriptorDataEXT vkGetSamplerOpaqueCaptureDescriptorDataEXT;
#endif /* defined(VK_EXT_descriptor_buffer) */
#if defined(VK_EXT_descriptor_buffer) && (defined(VK_KHR_acceleration_structure) || defined(VK_NV_ray_tracing))
PFN_vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT;
#endif /* defined(VK_EXT_descriptor_buffer) && (defined(VK_KHR_acceleration_structure) || defined(VK_NV_ray_tracing)) */
#if defined(VK_EXT_device_fault)
PFN_vkGetDeviceFaultInfoEXT vkGetDeviceFaultInfoEXT;
#endif /* defined(VK_EXT_device_fault) */
#if defined(VK_EXT_device_generated_commands)
PFN_vkCmdExecuteGeneratedCommandsEXT vkCmdExecuteGeneratedCommandsEXT;
PFN_vkCmdPreprocessGeneratedCommandsEXT vkCmdPreprocessGeneratedCommandsEXT;
PFN_vkCreateIndirectCommandsLayoutEXT vkCreateIndirectCommandsLayoutEXT;
PFN_vkCreateIndirectExecutionSetEXT vkCreateIndirectExecutionSetEXT;
PFN_vkDestroyIndirectCommandsLayoutEXT vkDestroyIndirectCommandsLayoutEXT;
PFN_vkDestroyIndirectExecutionSetEXT vkDestroyIndirectExecutionSetEXT;
PFN_vkGetGeneratedCommandsMemoryRequirementsEXT vkGetGeneratedCommandsMemoryRequirementsEXT;
PFN_vkUpdateIndirectExecutionSetPipelineEXT vkUpdateIndirectExecutionSetPipelineEXT;
PFN_vkUpdateIndirectExecutionSetShaderEXT vkUpdateIndirectExecutionSetShaderEXT;
#endif /* defined(VK_EXT_device_generated_commands) */
#if defined(VK_EXT_direct_mode_display)
PFN_vkReleaseDisplayEXT vkReleaseDisplayEXT;
#endif /* defined(VK_EXT_direct_mode_display) */
#if defined(VK_EXT_directfb_surface)
PFN_vkCreateDirectFBSurfaceEXT vkCreateDirectFBSurfaceEXT;
PFN_vkGetPhysicalDeviceDirectFBPresentationSupportEXT vkGetPhysicalDeviceDirectFBPresentationSupportEXT;
#endif /* defined(VK_EXT_directfb_surface) */
#if defined(VK_EXT_discard_rectangles)
PFN_vkCmdSetDiscardRectangleEXT vkCmdSetDiscardRectangleEXT;
#endif /* defined(VK_EXT_discard_rectangles) */
#if defined(VK_EXT_discard_rectangles) && VK_EXT_DISCARD_RECTANGLES_SPEC_VERSION >= 2
PFN_vkCmdSetDiscardRectangleEnableEXT vkCmdSetDiscardRectangleEnableEXT;
PFN_vkCmdSetDiscardRectangleModeEXT vkCmdSetDiscardRectangleModeEXT;
#endif /* defined(VK_EXT_discard_rectangles) && VK_EXT_DISCARD_RECTANGLES_SPEC_VERSION >= 2 */
#if defined(VK_EXT_display_control)
PFN_vkDisplayPowerControlEXT vkDisplayPowerControlEXT;
PFN_vkGetSwapchainCounterEXT vkGetSwapchainCounterEXT;
PFN_vkRegisterDeviceEventEXT vkRegisterDeviceEventEXT;
PFN_vkRegisterDisplayEventEXT vkRegisterDisplayEventEXT;
#endif /* defined(VK_EXT_display_control) */
#if defined(VK_EXT_display_surface_counter)
PFN_vkGetPhysicalDeviceSurfaceCapabilities2EXT vkGetPhysicalDeviceSurfaceCapabilities2EXT;
#endif /* defined(VK_EXT_display_surface_counter) */
#if defined(VK_EXT_external_memory_host)
PFN_vkGetMemoryHostPointerPropertiesEXT vkGetMemoryHostPointerPropertiesEXT;
#endif /* defined(VK_EXT_external_memory_host) */
#if defined(VK_EXT_external_memory_metal)
PFN_vkGetMemoryMetalHandleEXT vkGetMemoryMetalHandleEXT;
PFN_vkGetMemoryMetalHandlePropertiesEXT vkGetMemoryMetalHandlePropertiesEXT;
#endif /* defined(VK_EXT_external_memory_metal) */
#if defined(VK_EXT_fragment_density_map_offset)
PFN_vkCmdEndRendering2EXT vkCmdEndRendering2EXT;
#endif /* defined(VK_EXT_fragment_density_map_offset) */
#if defined(VK_EXT_full_screen_exclusive)
PFN_vkAcquireFullScreenExclusiveModeEXT vkAcquireFullScreenExclusiveModeEXT;
PFN_vkGetPhysicalDeviceSurfacePresentModes2EXT vkGetPhysicalDeviceSurfacePresentModes2EXT;
PFN_vkReleaseFullScreenExclusiveModeEXT vkReleaseFullScreenExclusiveModeEXT;
#endif /* defined(VK_EXT_full_screen_exclusive) */
#if defined(VK_EXT_full_screen_exclusive) && (defined(VK_KHR_device_group) || defined(VK_VERSION_1_1))
PFN_vkGetDeviceGroupSurfacePresentModes2EXT vkGetDeviceGroupSurfacePresentModes2EXT;
#endif /* defined(VK_EXT_full_screen_exclusive) && (defined(VK_KHR_device_group) || defined(VK_VERSION_1_1)) */
#if defined(VK_EXT_hdr_metadata)
PFN_vkSetHdrMetadataEXT vkSetHdrMetadataEXT;
#endif /* defined(VK_EXT_hdr_metadata) */
#if defined(VK_EXT_headless_surface)
PFN_vkCreateHeadlessSurfaceEXT vkCreateHeadlessSurfaceEXT;
#endif /* defined(VK_EXT_headless_surface) */
#if defined(VK_EXT_host_image_copy)
PFN_vkCopyImageToImageEXT vkCopyImageToImageEXT;
PFN_vkCopyImageToMemoryEXT vkCopyImageToMemoryEXT;
PFN_vkCopyMemoryToImageEXT vkCopyMemoryToImageEXT;
PFN_vkTransitionImageLayoutEXT vkTransitionImageLayoutEXT;
#endif /* defined(VK_EXT_host_image_copy) */
#if defined(VK_EXT_host_query_reset)
PFN_vkResetQueryPoolEXT vkResetQueryPoolEXT;
#endif /* defined(VK_EXT_host_query_reset) */
#if defined(VK_EXT_image_drm_format_modifier)
PFN_vkGetImageDrmFormatModifierPropertiesEXT vkGetImageDrmFormatModifierPropertiesEXT;
#endif /* defined(VK_EXT_image_drm_format_modifier) */
#if defined(VK_EXT_line_rasterization)
PFN_vkCmdSetLineStippleEXT vkCmdSetLineStippleEXT;
#endif /* defined(VK_EXT_line_rasterization) */
#if defined(VK_EXT_mesh_shader)
PFN_vkCmdDrawMeshTasksEXT vkCmdDrawMeshTasksEXT;
PFN_vkCmdDrawMeshTasksIndirectEXT vkCmdDrawMeshTasksIndirectEXT;
#endif /* defined(VK_EXT_mesh_shader) */
#if defined(VK_EXT_mesh_shader) && (defined(VK_KHR_draw_indirect_count) || defined(VK_VERSION_1_2))
PFN_vkCmdDrawMeshTasksIndirectCountEXT vkCmdDrawMeshTasksIndirectCountEXT;
#endif /* defined(VK_EXT_mesh_shader) && (defined(VK_KHR_draw_indirect_count) || defined(VK_VERSION_1_2)) */
#if defined(VK_EXT_metal_objects)
PFN_vkExportMetalObjectsEXT vkExportMetalObjectsEXT;
#endif /* defined(VK_EXT_metal_objects) */
#if defined(VK_EXT_metal_surface)
PFN_vkCreateMetalSurfaceEXT vkCreateMetalSurfaceEXT;
#endif /* defined(VK_EXT_metal_surface) */
#if defined(VK_EXT_multi_draw)
PFN_vkCmdDrawMultiEXT vkCmdDrawMultiEXT;
PFN_vkCmdDrawMultiIndexedEXT vkCmdDrawMultiIndexedEXT;
#endif /* defined(VK_EXT_multi_draw) */
#if defined(VK_EXT_opacity_micromap)
PFN_vkBuildMicromapsEXT vkBuildMicromapsEXT;
PFN_vkCmdBuildMicromapsEXT vkCmdBuildMicromapsEXT;
PFN_vkCmdCopyMemoryToMicromapEXT vkCmdCopyMemoryToMicromapEXT;
PFN_vkCmdCopyMicromapEXT vkCmdCopyMicromapEXT;
PFN_vkCmdCopyMicromapToMemoryEXT vkCmdCopyMicromapToMemoryEXT;
PFN_vkCmdWriteMicromapsPropertiesEXT vkCmdWriteMicromapsPropertiesEXT;
PFN_vkCopyMemoryToMicromapEXT vkCopyMemoryToMicromapEXT;
PFN_vkCopyMicromapEXT vkCopyMicromapEXT;
PFN_vkCopyMicromapToMemoryEXT vkCopyMicromapToMemoryEXT;
PFN_vkCreateMicromapEXT vkCreateMicromapEXT;
PFN_vkDestroyMicromapEXT vkDestroyMicromapEXT;
PFN_vkGetDeviceMicromapCompatibilityEXT vkGetDeviceMicromapCompatibilityEXT;
PFN_vkGetMicromapBuildSizesEXT vkGetMicromapBuildSizesEXT;
PFN_vkWriteMicromapsPropertiesEXT vkWriteMicromapsPropertiesEXT;
#endif /* defined(VK_EXT_opacity_micromap) */
#if defined(VK_EXT_pageable_device_local_memory)
PFN_vkSetDeviceMemoryPriorityEXT vkSetDeviceMemoryPriorityEXT;
#endif /* defined(VK_EXT_pageable_device_local_memory) */
#if defined(VK_EXT_pipeline_properties)
PFN_vkGetPipelinePropertiesEXT vkGetPipelinePropertiesEXT;
#endif /* defined(VK_EXT_pipeline_properties) */
#if defined(VK_EXT_private_data)
PFN_vkCreatePrivateDataSlotEXT vkCreatePrivateDataSlotEXT;
PFN_vkDestroyPrivateDataSlotEXT vkDestroyPrivateDataSlotEXT;
PFN_vkGetPrivateDataEXT vkGetPrivateDataEXT;
PFN_vkSetPrivateDataEXT vkSetPrivateDataEXT;
#endif /* defined(VK_EXT_private_data) */
#if defined(VK_EXT_sample_locations)
PFN_vkCmdSetSampleLocationsEXT vkCmdSetSampleLocationsEXT;
PFN_vkGetPhysicalDeviceMultisamplePropertiesEXT vkGetPhysicalDeviceMultisamplePropertiesEXT;
#endif /* defined(VK_EXT_sample_locations) */
#if defined(VK_EXT_shader_module_identifier)
PFN_vkGetShaderModuleCreateInfoIdentifierEXT vkGetShaderModuleCreateInfoIdentifierEXT;
PFN_vkGetShaderModuleIdentifierEXT vkGetShaderModuleIdentifierEXT;
#endif /* defined(VK_EXT_shader_module_identifier) */
#if defined(VK_EXT_shader_object)
PFN_vkCmdBindShadersEXT vkCmdBindShadersEXT;
PFN_vkCreateShadersEXT vkCreateShadersEXT;
PFN_vkDestroyShaderEXT vkDestroyShaderEXT;
PFN_vkGetShaderBinaryDataEXT vkGetShaderBinaryDataEXT;
#endif /* defined(VK_EXT_shader_object) */
#if defined(VK_EXT_swapchain_maintenance1)
PFN_vkReleaseSwapchainImagesEXT vkReleaseSwapchainImagesEXT;
#endif /* defined(VK_EXT_swapchain_maintenance1) */
#if defined(VK_EXT_tooling_info)
PFN_vkGetPhysicalDeviceToolPropertiesEXT vkGetPhysicalDeviceToolPropertiesEXT;
#endif /* defined(VK_EXT_tooling_info) */
#if defined(VK_EXT_transform_feedback)
PFN_vkCmdBeginQueryIndexedEXT vkCmdBeginQueryIndexedEXT;
PFN_vkCmdBeginTransformFeedbackEXT vkCmdBeginTransformFeedbackEXT;
PFN_vkCmdBindTransformFeedbackBuffersEXT vkCmdBindTransformFeedbackBuffersEXT;
PFN_vkCmdDrawIndirectByteCountEXT vkCmdDrawIndirectByteCountEXT;
PFN_vkCmdEndQueryIndexedEXT vkCmdEndQueryIndexedEXT;
PFN_vkCmdEndTransformFeedbackEXT vkCmdEndTransformFeedbackEXT;
#endif /* defined(VK_EXT_transform_feedback) */
#if defined(VK_EXT_validation_cache)
PFN_vkCreateValidationCacheEXT vkCreateValidationCacheEXT;
PFN_vkDestroyValidationCacheEXT vkDestroyValidationCacheEXT;
PFN_vkGetValidationCacheDataEXT vkGetValidationCacheDataEXT;
PFN_vkMergeValidationCachesEXT vkMergeValidationCachesEXT;
#endif /* defined(VK_EXT_validation_cache) */
#if defined(VK_FUCHSIA_buffer_collection)
PFN_vkCreateBufferCollectionFUCHSIA vkCreateBufferCollectionFUCHSIA;
PFN_vkDestroyBufferCollectionFUCHSIA vkDestroyBufferCollectionFUCHSIA;
PFN_vkGetBufferCollectionPropertiesFUCHSIA vkGetBufferCollectionPropertiesFUCHSIA;
PFN_vkSetBufferCollectionBufferConstraintsFUCHSIA vkSetBufferCollectionBufferConstraintsFUCHSIA;
PFN_vkSetBufferCollectionImageConstraintsFUCHSIA vkSetBufferCollectionImageConstraintsFUCHSIA;
#endif /* defined(VK_FUCHSIA_buffer_collection) */
#if defined(VK_FUCHSIA_external_memory)
PFN_vkGetMemoryZirconHandleFUCHSIA vkGetMemoryZirconHandleFUCHSIA;
PFN_vkGetMemoryZirconHandlePropertiesFUCHSIA vkGetMemoryZirconHandlePropertiesFUCHSIA;
#endif /* defined(VK_FUCHSIA_external_memory) */
#if defined(VK_FUCHSIA_external_semaphore)
PFN_vkGetSemaphoreZirconHandleFUCHSIA vkGetSemaphoreZirconHandleFUCHSIA;
PFN_vkImportSemaphoreZirconHandleFUCHSIA vkImportSemaphoreZirconHandleFUCHSIA;
#endif /* defined(VK_FUCHSIA_external_semaphore) */
#if defined(VK_FUCHSIA_imagepipe_surface)
PFN_vkCreateImagePipeSurfaceFUCHSIA vkCreateImagePipeSurfaceFUCHSIA;
#endif /* defined(VK_FUCHSIA_imagepipe_surface) */
#if defined(VK_GGP_stream_descriptor_surface)
PFN_vkCreateStreamDescriptorSurfaceGGP vkCreateStreamDescriptorSurfaceGGP;
#endif /* defined(VK_GGP_stream_descriptor_surface) */
#if defined(VK_GOOGLE_display_timing)
PFN_vkGetPastPresentationTimingGOOGLE vkGetPastPresentationTimingGOOGLE;
PFN_vkGetRefreshCycleDurationGOOGLE vkGetRefreshCycleDurationGOOGLE;
#endif /* defined(VK_GOOGLE_display_timing) */
#if defined(VK_HUAWEI_cluster_culling_shader)
PFN_vkCmdDrawClusterHUAWEI vkCmdDrawClusterHUAWEI;
PFN_vkCmdDrawClusterIndirectHUAWEI vkCmdDrawClusterIndirectHUAWEI;
#endif /* defined(VK_HUAWEI_cluster_culling_shader) */
#if defined(VK_HUAWEI_invocation_mask)
PFN_vkCmdBindInvocationMaskHUAWEI vkCmdBindInvocationMaskHUAWEI;
#endif /* defined(VK_HUAWEI_invocation_mask) */
#if defined(VK_HUAWEI_subpass_shading) && VK_HUAWEI_SUBPASS_SHADING_SPEC_VERSION >= 2
PFN_vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI;
#endif /* defined(VK_HUAWEI_subpass_shading) && VK_HUAWEI_SUBPASS_SHADING_SPEC_VERSION >= 2 */
#if defined(VK_HUAWEI_subpass_shading)
PFN_vkCmdSubpassShadingHUAWEI vkCmdSubpassShadingHUAWEI;
#endif /* defined(VK_HUAWEI_subpass_shading) */
#if defined(VK_INTEL_performance_query)
PFN_vkAcquirePerformanceConfigurationINTEL vkAcquirePerformanceConfigurationINTEL;
PFN_vkCmdSetPerformanceMarkerINTEL vkCmdSetPerformanceMarkerINTEL;
PFN_vkCmdSetPerformanceOverrideINTEL vkCmdSetPerformanceOverrideINTEL;
PFN_vkCmdSetPerformanceStreamMarkerINTEL vkCmdSetPerformanceStreamMarkerINTEL;
PFN_vkGetPerformanceParameterINTEL vkGetPerformanceParameterINTEL;
PFN_vkInitializePerformanceApiINTEL vkInitializePerformanceApiINTEL;
PFN_vkQueueSetPerformanceConfigurationINTEL vkQueueSetPerformanceConfigurationINTEL;
PFN_vkReleasePerformanceConfigurationINTEL vkReleasePerformanceConfigurationINTEL;
PFN_vkUninitializePerformanceApiINTEL vkUninitializePerformanceApiINTEL;
#endif /* defined(VK_INTEL_performance_query) */
#if defined(VK_KHR_acceleration_structure)
PFN_vkBuildAccelerationStructuresKHR vkBuildAccelerationStructuresKHR;
PFN_vkCmdBuildAccelerationStructuresIndirectKHR vkCmdBuildAccelerationStructuresIndirectKHR;
PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
PFN_vkCmdCopyAccelerationStructureKHR vkCmdCopyAccelerationStructureKHR;
PFN_vkCmdCopyAccelerationStructureToMemoryKHR vkCmdCopyAccelerationStructureToMemoryKHR;
PFN_vkCmdCopyMemoryToAccelerationStructureKHR vkCmdCopyMemoryToAccelerationStructureKHR;
PFN_vkCmdWriteAccelerationStructuresPropertiesKHR vkCmdWriteAccelerationStructuresPropertiesKHR;
PFN_vkCopyAccelerationStructureKHR vkCopyAccelerationStructureKHR;
PFN_vkCopyAccelerationStructureToMemoryKHR vkCopyAccelerationStructureToMemoryKHR;
PFN_vkCopyMemoryToAccelerationStructureKHR vkCopyMemoryToAccelerationStructureKHR;
PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
PFN_vkGetDeviceAccelerationStructureCompatibilityKHR vkGetDeviceAccelerationStructureCompatibilityKHR;
PFN_vkWriteAccelerationStructuresPropertiesKHR vkWriteAccelerationStructuresPropertiesKHR;
#endif /* defined(VK_KHR_acceleration_structure) */
#if defined(VK_KHR_android_surface)
PFN_vkCreateAndroidSurfaceKHR vkCreateAndroidSurfaceKHR;
#endif /* defined(VK_KHR_android_surface) */
#if defined(VK_KHR_bind_memory2)
PFN_vkBindBufferMemory2KHR vkBindBufferMemory2KHR;
PFN_vkBindImageMemory2KHR vkBindImageMemory2KHR;
#endif /* defined(VK_KHR_bind_memory2) */
#if defined(VK_KHR_buffer_device_address)
PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
PFN_vkGetBufferOpaqueCaptureAddressKHR vkGetBufferOpaqueCaptureAddressKHR;
PFN_vkGetDeviceMemoryOpaqueCaptureAddressKHR vkGetDeviceMemoryOpaqueCaptureAddressKHR;
#endif /* defined(VK_KHR_buffer_device_address) */
#if defined(VK_KHR_calibrated_timestamps)
PFN_vkGetCalibratedTimestampsKHR vkGetCalibratedTimestampsKHR;
PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR vkGetPhysicalDeviceCalibrateableTimeDomainsKHR;
#endif /* defined(VK_KHR_calibrated_timestamps) */
#if defined(VK_KHR_cooperative_matrix)
PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR;
#endif /* defined(VK_KHR_cooperative_matrix) */
#if defined(VK_KHR_copy_commands2)
PFN_vkCmdBlitImage2KHR vkCmdBlitImage2KHR;
PFN_vkCmdCopyBuffer2KHR vkCmdCopyBuffer2KHR;
PFN_vkCmdCopyBufferToImage2KHR vkCmdCopyBufferToImage2KHR;
PFN_vkCmdCopyImage2KHR vkCmdCopyImage2KHR;
PFN_vkCmdCopyImageToBuffer2KHR vkCmdCopyImageToBuffer2KHR;
PFN_vkCmdResolveImage2KHR vkCmdResolveImage2KHR;
#endif /* defined(VK_KHR_copy_commands2) */
#if defined(VK_KHR_create_renderpass2)
PFN_vkCmdBeginRenderPass2KHR vkCmdBeginRenderPass2KHR;
PFN_vkCmdEndRenderPass2KHR vkCmdEndRenderPass2KHR;
PFN_vkCmdNextSubpass2KHR vkCmdNextSubpass2KHR;
PFN_vkCreateRenderPass2KHR vkCreateRenderPass2KHR;
#endif /* defined(VK_KHR_create_renderpass2) */
#if defined(VK_KHR_deferred_host_operations)
PFN_vkCreateDeferredOperationKHR vkCreateDeferredOperationKHR;
PFN_vkDeferredOperationJoinKHR vkDeferredOperationJoinKHR;
PFN_vkDestroyDeferredOperationKHR vkDestroyDeferredOperationKHR;
PFN_vkGetDeferredOperationMaxConcurrencyKHR vkGetDeferredOperationMaxConcurrencyKHR;
PFN_vkGetDeferredOperationResultKHR vkGetDeferredOperationResultKHR;
#endif /* defined(VK_KHR_deferred_host_operations) */
#if defined(VK_KHR_descriptor_update_template)
PFN_vkCreateDescriptorUpdateTemplateKHR vkCreateDescriptorUpdateTemplateKHR;
PFN_vkDestroyDescriptorUpdateTemplateKHR vkDestroyDescriptorUpdateTemplateKHR;
PFN_vkUpdateDescriptorSetWithTemplateKHR vkUpdateDescriptorSetWithTemplateKHR;
#endif /* defined(VK_KHR_descriptor_update_template) */
#if defined(VK_KHR_device_group)
PFN_vkCmdDispatchBaseKHR vkCmdDispatchBaseKHR;
PFN_vkCmdSetDeviceMaskKHR vkCmdSetDeviceMaskKHR;
PFN_vkGetDeviceGroupPeerMemoryFeaturesKHR vkGetDeviceGroupPeerMemoryFeaturesKHR;
#endif /* defined(VK_KHR_device_group) */
#if defined(VK_KHR_device_group_creation)
PFN_vkEnumeratePhysicalDeviceGroupsKHR vkEnumeratePhysicalDeviceGroupsKHR;
#endif /* defined(VK_KHR_device_group_creation) */
#if defined(VK_KHR_display)
PFN_vkCreateDisplayModeKHR vkCreateDisplayModeKHR;
PFN_vkCreateDisplayPlaneSurfaceKHR vkCreateDisplayPlaneSurfaceKHR;
PFN_vkGetDisplayModePropertiesKHR vkGetDisplayModePropertiesKHR;
PFN_vkGetDisplayPlaneCapabilitiesKHR vkGetDisplayPlaneCapabilitiesKHR;
PFN_vkGetDisplayPlaneSupportedDisplaysKHR vkGetDisplayPlaneSupportedDisplaysKHR;
PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR vkGetPhysicalDeviceDisplayPlanePropertiesKHR;
PFN_vkGetPhysicalDeviceDisplayPropertiesKHR vkGetPhysicalDeviceDisplayPropertiesKHR;
#endif /* defined(VK_KHR_display) */
#if defined(VK_KHR_display_swapchain)
PFN_vkCreateSharedSwapchainsKHR vkCreateSharedSwapchainsKHR;
#endif /* defined(VK_KHR_display_swapchain) */
#if defined(VK_KHR_draw_indirect_count)
PFN_vkCmdDrawIndexedIndirectCountKHR vkCmdDrawIndexedIndirectCountKHR;
PFN_vkCmdDrawIndirectCountKHR vkCmdDrawIndirectCountKHR;
#endif /* defined(VK_KHR_draw_indirect_count) */
#if defined(VK_KHR_dynamic_rendering)
PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR;
PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR;
#endif /* defined(VK_KHR_dynamic_rendering) */
#if defined(VK_KHR_dynamic_rendering_local_read)
PFN_vkCmdSetRenderingAttachmentLocationsKHR vkCmdSetRenderingAttachmentLocationsKHR;
PFN_vkCmdSetRenderingInputAttachmentIndicesKHR vkCmdSetRenderingInputAttachmentIndicesKHR;
#endif /* defined(VK_KHR_dynamic_rendering_local_read) */
#if defined(VK_KHR_external_fence_capabilities)
PFN_vkGetPhysicalDeviceExternalFencePropertiesKHR vkGetPhysicalDeviceExternalFencePropertiesKHR;
#endif /* defined(VK_KHR_external_fence_capabilities) */
#if defined(VK_KHR_external_fence_fd)
PFN_vkGetFenceFdKHR vkGetFenceFdKHR;
PFN_vkImportFenceFdKHR vkImportFenceFdKHR;
#endif /* defined(VK_KHR_external_fence_fd) */
#if defined(VK_KHR_external_fence_win32)
PFN_vkGetFenceWin32HandleKHR vkGetFenceWin32HandleKHR;
PFN_vkImportFenceWin32HandleKHR vkImportFenceWin32HandleKHR;
#endif /* defined(VK_KHR_external_fence_win32) */
#if defined(VK_KHR_external_memory_capabilities)
PFN_vkGetPhysicalDeviceExternalBufferPropertiesKHR vkGetPhysicalDeviceExternalBufferPropertiesKHR;
#endif /* defined(VK_KHR_external_memory_capabilities) */
#if defined(VK_KHR_external_memory_fd)
PFN_vkGetMemoryFdKHR vkGetMemoryFdKHR;
PFN_vkGetMemoryFdPropertiesKHR vkGetMemoryFdPropertiesKHR;
#endif /* defined(VK_KHR_external_memory_fd) */
#if defined(VK_KHR_external_memory_win32)
PFN_vkGetMemoryWin32HandleKHR vkGetMemoryWin32HandleKHR;
PFN_vkGetMemoryWin32HandlePropertiesKHR vkGetMemoryWin32HandlePropertiesKHR;
#endif /* defined(VK_KHR_external_memory_win32) */
#if defined(VK_KHR_external_semaphore_capabilities)
PFN_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR vkGetPhysicalDeviceExternalSemaphorePropertiesKHR;
#endif /* defined(VK_KHR_external_semaphore_capabilities) */
#if defined(VK_KHR_external_semaphore_fd)
PFN_vkGetSemaphoreFdKHR vkGetSemaphoreFdKHR;
PFN_vkImportSemaphoreFdKHR vkImportSemaphoreFdKHR;
#endif /* defined(VK_KHR_external_semaphore_fd) */
#if defined(VK_KHR_external_semaphore_win32)
PFN_vkGetSemaphoreWin32HandleKHR vkGetSemaphoreWin32HandleKHR;
PFN_vkImportSemaphoreWin32HandleKHR vkImportSemaphoreWin32HandleKHR;
#endif /* defined(VK_KHR_external_semaphore_win32) */
#if defined(VK_KHR_fragment_shading_rate)
PFN_vkCmdSetFragmentShadingRateKHR vkCmdSetFragmentShadingRateKHR;
PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR vkGetPhysicalDeviceFragmentShadingRatesKHR;
#endif /* defined(VK_KHR_fragment_shading_rate) */
#if defined(VK_KHR_get_display_properties2)
PFN_vkGetDisplayModeProperties2KHR vkGetDisplayModeProperties2KHR;
PFN_vkGetDisplayPlaneCapabilities2KHR vkGetDisplayPlaneCapabilities2KHR;
PFN_vkGetPhysicalDeviceDisplayPlaneProperties2KHR vkGetPhysicalDeviceDisplayPlaneProperties2KHR;
PFN_vkGetPhysicalDeviceDisplayProperties2KHR vkGetPhysicalDeviceDisplayProperties2KHR;
#endif /* defined(VK_KHR_get_display_properties2) */
#if defined(VK_KHR_get_memory_requirements2)
PFN_vkGetBufferMemoryRequirements2KHR vkGetBufferMemoryRequirements2KHR;
PFN_vkGetImageMemoryRequirements2KHR vkGetImageMemoryRequirements2KHR;
PFN_vkGetImageSparseMemoryRequirements2KHR vkGetImageSparseMemoryRequirements2KHR;
#endif /* defined(VK_KHR_get_memory_requirements2) */
#if defined(VK_KHR_get_physical_device_properties2)
PFN_vkGetPhysicalDeviceFeatures2KHR vkGetPhysicalDeviceFeatures2KHR;
PFN_vkGetPhysicalDeviceFormatProperties2KHR vkGetPhysicalDeviceFormatProperties2KHR;
PFN_vkGetPhysicalDeviceImageFormatProperties2KHR vkGetPhysicalDeviceImageFormatProperties2KHR;
PFN_vkGetPhysicalDeviceMemoryProperties2KHR vkGetPhysicalDeviceMemoryProperties2KHR;
PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR;
PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR vkGetPhysicalDeviceQueueFamilyProperties2KHR;
PFN_vkGetPhysicalDeviceSparseImageFormatProperties2KHR vkGetPhysicalDeviceSparseImageFormatProperties2KHR;
#endif /* defined(VK_KHR_get_physical_device_properties2) */
#if defined(VK_KHR_get_surface_capabilities2)
PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR vkGetPhysicalDeviceSurfaceCapabilities2KHR;
PFN_vkGetPhysicalDeviceSurfaceFormats2KHR vkGetPhysicalDeviceSurfaceFormats2KHR;
#endif /* defined(VK_KHR_get_surface_capabilities2) */
#if defined(VK_KHR_line_rasterization)
PFN_vkCmdSetLineStippleKHR vkCmdSetLineStippleKHR;
#endif /* defined(VK_KHR_line_rasterization) */
#if defined(VK_KHR_maintenance1)
PFN_vkTrimCommandPoolKHR vkTrimCommandPoolKHR;
#endif /* defined(VK_KHR_maintenance1) */
#if defined(VK_KHR_maintenance3)
PFN_vkGetDescriptorSetLayoutSupportKHR vkGetDescriptorSetLayoutSupportKHR;
#endif /* defined(VK_KHR_maintenance3) */
#if defined(VK_KHR_maintenance4)
PFN_vkGetDeviceBufferMemoryRequirementsKHR vkGetDeviceBufferMemoryRequirementsKHR;
PFN_vkGetDeviceImageMemoryRequirementsKHR vkGetDeviceImageMemoryRequirementsKHR;
PFN_vkGetDeviceImageSparseMemoryRequirementsKHR vkGetDeviceImageSparseMemoryRequirementsKHR;
#endif /* defined(VK_KHR_maintenance4) */
#if defined(VK_KHR_maintenance5)
PFN_vkCmdBindIndexBuffer2KHR vkCmdBindIndexBuffer2KHR;
PFN_vkGetDeviceImageSubresourceLayoutKHR vkGetDeviceImageSubresourceLayoutKHR;
PFN_vkGetImageSubresourceLayout2KHR vkGetImageSubresourceLayout2KHR;
PFN_vkGetRenderingAreaGranularityKHR vkGetRenderingAreaGranularityKHR;
#endif /* defined(VK_KHR_maintenance5) */
#if defined(VK_KHR_maintenance6)
PFN_vkCmdBindDescriptorSets2KHR vkCmdBindDescriptorSets2KHR;
PFN_vkCmdPushConstants2KHR vkCmdPushConstants2KHR;
#endif /* defined(VK_KHR_maintenance6) */
#if defined(VK_KHR_maintenance6) && defined(VK_KHR_push_descriptor)
PFN_vkCmdPushDescriptorSet2KHR vkCmdPushDescriptorSet2KHR;
PFN_vkCmdPushDescriptorSetWithTemplate2KHR vkCmdPushDescriptorSetWithTemplate2KHR;
#endif /* defined(VK_KHR_maintenance6) && defined(VK_KHR_push_descriptor) */
#if defined(VK_KHR_maintenance6) && defined(VK_EXT_descriptor_buffer)
PFN_vkCmdBindDescriptorBufferEmbeddedSamplers2EXT vkCmdBindDescriptorBufferEmbeddedSamplers2EXT;
PFN_vkCmdSetDescriptorBufferOffsets2EXT vkCmdSetDescriptorBufferOffsets2EXT;
#endif /* defined(VK_KHR_maintenance6) && defined(VK_EXT_descriptor_buffer) */
#if defined(VK_KHR_map_memory2)
PFN_vkMapMemory2KHR vkMapMemory2KHR;
PFN_vkUnmapMemory2KHR vkUnmapMemory2KHR;
#endif /* defined(VK_KHR_map_memory2) */
#if defined(VK_KHR_performance_query)
PFN_vkAcquireProfilingLockKHR vkAcquireProfilingLockKHR;
PFN_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR;
PFN_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR;
PFN_vkReleaseProfilingLockKHR vkReleaseProfilingLockKHR;
#endif /* defined(VK_KHR_performance_query) */
#if defined(VK_KHR_pipeline_binary)
PFN_vkCreatePipelineBinariesKHR vkCreatePipelineBinariesKHR;
PFN_vkDestroyPipelineBinaryKHR vkDestroyPipelineBinaryKHR;
PFN_vkGetPipelineBinaryDataKHR vkGetPipelineBinaryDataKHR;
PFN_vkGetPipelineKeyKHR vkGetPipelineKeyKHR;
PFN_vkReleaseCapturedPipelineDataKHR vkReleaseCapturedPipelineDataKHR;
#endif /* defined(VK_KHR_pipeline_binary) */
#if defined(VK_KHR_pipeline_executable_properties)
PFN_vkGetPipelineExecutableInternalRepresentationsKHR vkGetPipelineExecutableInternalRepresentationsKHR;
PFN_vkGetPipelineExecutablePropertiesKHR vkGetPipelineExecutablePropertiesKHR;
PFN_vkGetPipelineExecutableStatisticsKHR vkGetPipelineExecutableStatisticsKHR;
#endif /* defined(VK_KHR_pipeline_executable_properties) */
#if defined(VK_KHR_present_wait)
PFN_vkWaitForPresentKHR vkWaitForPresentKHR;
#endif /* defined(VK_KHR_present_wait) */
#if defined(VK_KHR_present_wait2)
PFN_vkWaitForPresent2KHR vkWaitForPresent2KHR;
#endif /* defined(VK_KHR_present_wait2) */
#if defined(VK_KHR_push_descriptor)
PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR;
#endif /* defined(VK_KHR_push_descriptor) */
#if defined(VK_KHR_ray_tracing_maintenance1) && defined(VK_KHR_ray_tracing_pipeline)
PFN_vkCmdTraceRaysIndirect2KHR vkCmdTraceRaysIndirect2KHR;
#endif /* defined(VK_KHR_ray_tracing_maintenance1) && defined(VK_KHR_ray_tracing_pipeline) */
#if defined(VK_KHR_ray_tracing_pipeline)
PFN_vkCmdSetRayTracingPipelineStackSizeKHR vkCmdSetRayTracingPipelineStackSizeKHR;
PFN_vkCmdTraceRaysIndirectKHR vkCmdTraceRaysIndirectKHR;
PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
PFN_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR vkGetRayTracingCaptureReplayShaderGroupHandlesKHR;
PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
PFN_vkGetRayTracingShaderGroupStackSizeKHR vkGetRayTracingShaderGroupStackSizeKHR;
#endif /* defined(VK_KHR_ray_tracing_pipeline) */
#if defined(VK_KHR_sampler_ycbcr_conversion)
PFN_vkCreateSamplerYcbcrConversionKHR vkCreateSamplerYcbcrConversionKHR;
PFN_vkDestroySamplerYcbcrConversionKHR vkDestroySamplerYcbcrConversionKHR;
#endif /* defined(VK_KHR_sampler_ycbcr_conversion) */
#if defined(VK_KHR_shared_presentable_image)
PFN_vkGetSwapchainStatusKHR vkGetSwapchainStatusKHR;
#endif /* defined(VK_KHR_shared_presentable_image) */
#if defined(VK_KHR_surface)
PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR;
PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;
PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
#endif /* defined(VK_KHR_surface) */
#if defined(VK_KHR_swapchain)
PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
PFN_vkQueuePresentKHR vkQueuePresentKHR;
#endif /* defined(VK_KHR_swapchain) */
#if defined(VK_KHR_swapchain_maintenance1)
PFN_vkReleaseSwapchainImagesKHR vkReleaseSwapchainImagesKHR;
#endif /* defined(VK_KHR_swapchain_maintenance1) */
#if defined(VK_KHR_synchronization2)
PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR;
PFN_vkCmdResetEvent2KHR vkCmdResetEvent2KHR;
PFN_vkCmdSetEvent2KHR vkCmdSetEvent2KHR;
PFN_vkCmdWaitEvents2KHR vkCmdWaitEvents2KHR;
PFN_vkCmdWriteTimestamp2KHR vkCmdWriteTimestamp2KHR;
PFN_vkQueueSubmit2KHR vkQueueSubmit2KHR;
#endif /* defined(VK_KHR_synchronization2) */
#if defined(VK_KHR_timeline_semaphore)
PFN_vkGetSemaphoreCounterValueKHR vkGetSemaphoreCounterValueKHR;
PFN_vkSignalSemaphoreKHR vkSignalSemaphoreKHR;
PFN_vkWaitSemaphoresKHR vkWaitSemaphoresKHR;
#endif /* defined(VK_KHR_timeline_semaphore) */
#if defined(VK_KHR_video_decode_queue)
PFN_vkCmdDecodeVideoKHR vkCmdDecodeVideoKHR;
#endif /* defined(VK_KHR_video_decode_queue) */
#if defined(VK_KHR_video_encode_queue)
PFN_vkCmdEncodeVideoKHR vkCmdEncodeVideoKHR;
PFN_vkGetEncodedVideoSessionParametersKHR vkGetEncodedVideoSessionParametersKHR;
PFN_vkGetPhysicalDeviceVideoEncodeQualityLevelPropertiesKHR vkGetPhysicalDeviceVideoEncodeQualityLevelPropertiesKHR;
#endif /* defined(VK_KHR_video_encode_queue) */
#if defined(VK_KHR_video_queue)
PFN_vkBindVideoSessionMemoryKHR vkBindVideoSessionMemoryKHR;
PFN_vkCmdBeginVideoCodingKHR vkCmdBeginVideoCodingKHR;
PFN_vkCmdControlVideoCodingKHR vkCmdControlVideoCodingKHR;
PFN_vkCmdEndVideoCodingKHR vkCmdEndVideoCodingKHR;
PFN_vkCreateVideoSessionKHR vkCreateVideoSessionKHR;
PFN_vkCreateVideoSessionParametersKHR vkCreateVideoSessionParametersKHR;
PFN_vkDestroyVideoSessionKHR vkDestroyVideoSessionKHR;
PFN_vkDestroyVideoSessionParametersKHR vkDestroyVideoSessionParametersKHR;
PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR vkGetPhysicalDeviceVideoCapabilitiesKHR;
PFN_vkGetPhysicalDeviceVideoFormatPropertiesKHR vkGetPhysicalDeviceVideoFormatPropertiesKHR;
PFN_vkGetVideoSessionMemoryRequirementsKHR vkGetVideoSessionMemoryRequirementsKHR;
PFN_vkUpdateVideoSessionParametersKHR vkUpdateVideoSessionParametersKHR;
#endif /* defined(VK_KHR_video_queue) */
#if defined(VK_KHR_wayland_surface)
PFN_vkCreateWaylandSurfaceKHR vkCreateWaylandSurfaceKHR;
PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR vkGetPhysicalDeviceWaylandPresentationSupportKHR;
#endif /* defined(VK_KHR_wayland_surface) */
#if defined(VK_KHR_win32_surface)
PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR;
PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR vkGetPhysicalDeviceWin32PresentationSupportKHR;
#endif /* defined(VK_KHR_win32_surface) */
#if defined(VK_KHR_xcb_surface)
PFN_vkCreateXcbSurfaceKHR vkCreateXcbSurfaceKHR;
PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR vkGetPhysicalDeviceXcbPresentationSupportKHR;
#endif /* defined(VK_KHR_xcb_surface) */
#if defined(VK_KHR_xlib_surface)
PFN_vkCreateXlibSurfaceKHR vkCreateXlibSurfaceKHR;
PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR vkGetPhysicalDeviceXlibPresentationSupportKHR;
#endif /* defined(VK_KHR_xlib_surface) */
#if defined(VK_MVK_ios_surface)
PFN_vkCreateIOSSurfaceMVK vkCreateIOSSurfaceMVK;
#endif /* defined(VK_MVK_ios_surface) */
#if defined(VK_MVK_macos_surface)
PFN_vkCreateMacOSSurfaceMVK vkCreateMacOSSurfaceMVK;
#endif /* defined(VK_MVK_macos_surface) */
#if defined(VK_NN_vi_surface)
PFN_vkCreateViSurfaceNN vkCreateViSurfaceNN;
#endif /* defined(VK_NN_vi_surface) */
#if defined(VK_NVX_binary_import)
PFN_vkCmdCuLaunchKernelNVX vkCmdCuLaunchKernelNVX;
PFN_vkCreateCuFunctionNVX vkCreateCuFunctionNVX;
PFN_vkCreateCuModuleNVX vkCreateCuModuleNVX;
PFN_vkDestroyCuFunctionNVX vkDestroyCuFunctionNVX;
PFN_vkDestroyCuModuleNVX vkDestroyCuModuleNVX;
#endif /* defined(VK_NVX_binary_import) */
#if defined(VK_NVX_image_view_handle)
PFN_vkGetImageViewHandleNVX vkGetImageViewHandleNVX;
#endif /* defined(VK_NVX_image_view_handle) */
#if defined(VK_NVX_image_view_handle) && VK_NVX_IMAGE_VIEW_HANDLE_SPEC_VERSION >= 3
PFN_vkGetImageViewHandle64NVX vkGetImageViewHandle64NVX;
#endif /* defined(VK_NVX_image_view_handle) && VK_NVX_IMAGE_VIEW_HANDLE_SPEC_VERSION >= 3 */
#if defined(VK_NVX_image_view_handle) && VK_NVX_IMAGE_VIEW_HANDLE_SPEC_VERSION >= 2
PFN_vkGetImageViewAddressNVX vkGetImageViewAddressNVX;
#endif /* defined(VK_NVX_image_view_handle) && VK_NVX_IMAGE_VIEW_HANDLE_SPEC_VERSION >= 2 */
#if defined(VK_NV_acquire_winrt_display)
PFN_vkAcquireWinrtDisplayNV vkAcquireWinrtDisplayNV;
PFN_vkGetWinrtDisplayNV vkGetWinrtDisplayNV;
#endif /* defined(VK_NV_acquire_winrt_display) */
#if defined(VK_NV_clip_space_w_scaling)
PFN_vkCmdSetViewportWScalingNV vkCmdSetViewportWScalingNV;
#endif /* defined(VK_NV_clip_space_w_scaling) */
#if defined(VK_NV_cluster_acceleration_structure)
PFN_vkCmdBuildClusterAccelerationStructureIndirectNV vkCmdBuildClusterAccelerationStructureIndirectNV;
PFN_vkGetClusterAccelerationStructureBuildSizesNV vkGetClusterAccelerationStructureBuildSizesNV;
#endif /* defined(VK_NV_cluster_acceleration_structure) */
#if defined(VK_NV_cooperative_matrix)
PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV vkGetPhysicalDeviceCooperativeMatrixPropertiesNV;
#endif /* defined(VK_NV_cooperative_matrix) */
#if defined(VK_NV_cooperative_matrix2)
PFN_vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV;
#endif /* defined(VK_NV_cooperative_matrix2) */
#if defined(VK_NV_cooperative_vector)
PFN_vkCmdConvertCooperativeVectorMatrixNV vkCmdConvertCooperativeVectorMatrixNV;
PFN_vkConvertCooperativeVectorMatrixNV vkConvertCooperativeVectorMatrixNV;
PFN_vkGetPhysicalDeviceCooperativeVectorPropertiesNV vkGetPhysicalDeviceCooperativeVectorPropertiesNV;
#endif /* defined(VK_NV_cooperative_vector) */
#if defined(VK_NV_copy_memory_indirect)
PFN_vkCmdCopyMemoryIndirectNV vkCmdCopyMemoryIndirectNV;
PFN_vkCmdCopyMemoryToImageIndirectNV vkCmdCopyMemoryToImageIndirectNV;
#endif /* defined(VK_NV_copy_memory_indirect) */
#if defined(VK_NV_coverage_reduction_mode)
PFN_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV;
#endif /* defined(VK_NV_coverage_reduction_mode) */
#if defined(VK_NV_cuda_kernel_launch)
PFN_vkCmdCudaLaunchKernelNV vkCmdCudaLaunchKernelNV;
PFN_vkCreateCudaFunctionNV vkCreateCudaFunctionNV;
PFN_vkCreateCudaModuleNV vkCreateCudaModuleNV;
PFN_vkDestroyCudaFunctionNV vkDestroyCudaFunctionNV;
PFN_vkDestroyCudaModuleNV vkDestroyCudaModuleNV;
PFN_vkGetCudaModuleCacheNV vkGetCudaModuleCacheNV;
#endif /* defined(VK_NV_cuda_kernel_launch) */
#if defined(VK_NV_device_diagnostic_checkpoints)
PFN_vkCmdSetCheckpointNV vkCmdSetCheckpointNV;
PFN_vkGetQueueCheckpointDataNV vkGetQueueCheckpointDataNV;
#endif /* defined(VK_NV_device_diagnostic_checkpoints) */
#if defined(VK_NV_device_diagnostic_checkpoints) && (defined(VK_VERSION_1_3) || defined(VK_KHR_synchronization2))
PFN_vkGetQueueCheckpointData2NV vkGetQueueCheckpointData2NV;
#endif /* defined(VK_NV_device_diagnostic_checkpoints) && (defined(VK_VERSION_1_3) || defined(VK_KHR_synchronization2)) */
#if defined(VK_NV_device_generated_commands)
PFN_vkCmdBindPipelineShaderGroupNV vkCmdBindPipelineShaderGroupNV;
PFN_vkCmdExecuteGeneratedCommandsNV vkCmdExecuteGeneratedCommandsNV;
PFN_vkCmdPreprocessGeneratedCommandsNV vkCmdPreprocessGeneratedCommandsNV;
PFN_vkCreateIndirectCommandsLayoutNV vkCreateIndirectCommandsLayoutNV;
PFN_vkDestroyIndirectCommandsLayoutNV vkDestroyIndirectCommandsLayoutNV;
PFN_vkGetGeneratedCommandsMemoryRequirementsNV vkGetGeneratedCommandsMemoryRequirementsNV;
#endif /* defined(VK_NV_device_generated_commands) */
#if defined(VK_NV_device_generated_commands_compute)
PFN_vkCmdUpdatePipelineIndirectBufferNV vkCmdUpdatePipelineIndirectBufferNV;
PFN_vkGetPipelineIndirectDeviceAddressNV vkGetPipelineIndirectDeviceAddressNV;
PFN_vkGetPipelineIndirectMemoryRequirementsNV vkGetPipelineIndirectMemoryRequirementsNV;
#endif /* defined(VK_NV_device_generated_commands_compute) */
#if defined(VK_NV_external_compute_queue)
PFN_vkCreateExternalComputeQueueNV vkCreateExternalComputeQueueNV;
PFN_vkDestroyExternalComputeQueueNV vkDestroyExternalComputeQueueNV;
PFN_vkGetExternalComputeQueueDataNV vkGetExternalComputeQueueDataNV;
#endif /* defined(VK_NV_external_compute_queue) */
#if defined(VK_NV_external_memory_capabilities)
PFN_vkGetPhysicalDeviceExternalImageFormatPropertiesNV vkGetPhysicalDeviceExternalImageFormatPropertiesNV;
#endif /* defined(VK_NV_external_memory_capabilities) */
#if defined(VK_NV_external_memory_rdma)
PFN_vkGetMemoryRemoteAddressNV vkGetMemoryRemoteAddressNV;
#endif /* defined(VK_NV_external_memory_rdma) */
#if defined(VK_NV_external_memory_win32)
PFN_vkGetMemoryWin32HandleNV vkGetMemoryWin32HandleNV;
#endif /* defined(VK_NV_external_memory_win32) */
#if defined(VK_NV_fragment_shading_rate_enums)
PFN_vkCmdSetFragmentShadingRateEnumNV vkCmdSetFragmentShadingRateEnumNV;
#endif /* defined(VK_NV_fragment_shading_rate_enums) */
#if defined(VK_NV_low_latency2)
PFN_vkGetLatencyTimingsNV vkGetLatencyTimingsNV;
PFN_vkLatencySleepNV vkLatencySleepNV;
PFN_vkQueueNotifyOutOfBandNV vkQueueNotifyOutOfBandNV;
PFN_vkSetLatencyMarkerNV vkSetLatencyMarkerNV;
PFN_vkSetLatencySleepModeNV vkSetLatencySleepModeNV;
#endif /* defined(VK_NV_low_latency2) */
#if defined(VK_NV_memory_decompression)
PFN_vkCmdDecompressMemoryIndirectCountNV vkCmdDecompressMemoryIndirectCountNV;
PFN_vkCmdDecompressMemoryNV vkCmdDecompressMemoryNV;
#endif /* defined(VK_NV_memory_decompression) */
#if defined(VK_NV_mesh_shader)
PFN_vkCmdDrawMeshTasksIndirectNV vkCmdDrawMeshTasksIndirectNV;
PFN_vkCmdDrawMeshTasksNV vkCmdDrawMeshTasksNV;
#endif /* defined(VK_NV_mesh_shader) */
#if defined(VK_NV_mesh_shader) && (defined(VK_KHR_draw_indirect_count) || defined(VK_VERSION_1_2))
PFN_vkCmdDrawMeshTasksIndirectCountNV vkCmdDrawMeshTasksIndirectCountNV;
#endif /* defined(VK_NV_mesh_shader) && (defined(VK_KHR_draw_indirect_count) || defined(VK_VERSION_1_2)) */
#if defined(VK_NV_optical_flow)
PFN_vkBindOpticalFlowSessionImageNV vkBindOpticalFlowSessionImageNV;
PFN_vkCmdOpticalFlowExecuteNV vkCmdOpticalFlowExecuteNV;
PFN_vkCreateOpticalFlowSessionNV vkCreateOpticalFlowSessionNV;
PFN_vkDestroyOpticalFlowSessionNV vkDestroyOpticalFlowSessionNV;
PFN_vkGetPhysicalDeviceOpticalFlowImageFormatsNV vkGetPhysicalDeviceOpticalFlowImageFormatsNV;
#endif /* defined(VK_NV_optical_flow) */
#if defined(VK_NV_partitioned_acceleration_structure)
PFN_vkCmdBuildPartitionedAccelerationStructuresNV vkCmdBuildPartitionedAccelerationStructuresNV;
PFN_vkGetPartitionedAccelerationStructuresBuildSizesNV vkGetPartitionedAccelerationStructuresBuildSizesNV;
#endif /* defined(VK_NV_partitioned_acceleration_structure) */
#if defined(VK_NV_ray_tracing)
PFN_vkBindAccelerationStructureMemoryNV vkBindAccelerationStructureMemoryNV;
PFN_vkCmdBuildAccelerationStructureNV vkCmdBuildAccelerationStructureNV;
PFN_vkCmdCopyAccelerationStructureNV vkCmdCopyAccelerationStructureNV;
PFN_vkCmdTraceRaysNV vkCmdTraceRaysNV;
PFN_vkCmdWriteAccelerationStructuresPropertiesNV vkCmdWriteAccelerationStructuresPropertiesNV;
PFN_vkCompileDeferredNV vkCompileDeferredNV;
PFN_vkCreateAccelerationStructureNV vkCreateAccelerationStructureNV;
PFN_vkCreateRayTracingPipelinesNV vkCreateRayTracingPipelinesNV;
PFN_vkDestroyAccelerationStructureNV vkDestroyAccelerationStructureNV;
PFN_vkGetAccelerationStructureHandleNV vkGetAccelerationStructureHandleNV;
PFN_vkGetAccelerationStructureMemoryRequirementsNV vkGetAccelerationStructureMemoryRequirementsNV;
PFN_vkGetRayTracingShaderGroupHandlesNV vkGetRayTracingShaderGroupHandlesNV;
#endif /* defined(VK_NV_ray_tracing) */
#if defined(VK_NV_scissor_exclusive) && VK_NV_SCISSOR_EXCLUSIVE_SPEC_VERSION >= 2
PFN_vkCmdSetExclusiveScissorEnableNV vkCmdSetExclusiveScissorEnableNV;
#endif /* defined(VK_NV_scissor_exclusive) && VK_NV_SCISSOR_EXCLUSIVE_SPEC_VERSION >= 2 */
#if defined(VK_NV_scissor_exclusive)
PFN_vkCmdSetExclusiveScissorNV vkCmdSetExclusiveScissorNV;
#endif /* defined(VK_NV_scissor_exclusive) */
#if defined(VK_NV_shading_rate_image)
PFN_vkCmdBindShadingRateImageNV vkCmdBindShadingRateImageNV;
PFN_vkCmdSetCoarseSampleOrderNV vkCmdSetCoarseSampleOrderNV;
PFN_vkCmdSetViewportShadingRatePaletteNV vkCmdSetViewportShadingRatePaletteNV;
#endif /* defined(VK_NV_shading_rate_image) */
#if defined(VK_OHOS_surface)
PFN_vkCreateSurfaceOHOS vkCreateSurfaceOHOS;
#endif /* defined(VK_OHOS_surface) */
#if defined(VK_QCOM_tile_memory_heap)
PFN_vkCmdBindTileMemoryQCOM vkCmdBindTileMemoryQCOM;
#endif /* defined(VK_QCOM_tile_memory_heap) */
#if defined(VK_QCOM_tile_properties)
PFN_vkGetDynamicRenderingTilePropertiesQCOM vkGetDynamicRenderingTilePropertiesQCOM;
PFN_vkGetFramebufferTilePropertiesQCOM vkGetFramebufferTilePropertiesQCOM;
#endif /* defined(VK_QCOM_tile_properties) */
#if defined(VK_QCOM_tile_shading)
PFN_vkCmdBeginPerTileExecutionQCOM vkCmdBeginPerTileExecutionQCOM;
PFN_vkCmdDispatchTileQCOM vkCmdDispatchTileQCOM;
PFN_vkCmdEndPerTileExecutionQCOM vkCmdEndPerTileExecutionQCOM;
#endif /* defined(VK_QCOM_tile_shading) */
#if defined(VK_QNX_external_memory_screen_buffer)
PFN_vkGetScreenBufferPropertiesQNX vkGetScreenBufferPropertiesQNX;
#endif /* defined(VK_QNX_external_memory_screen_buffer) */
#if defined(VK_QNX_screen_surface)
PFN_vkCreateScreenSurfaceQNX vkCreateScreenSurfaceQNX;
PFN_vkGetPhysicalDeviceScreenPresentationSupportQNX vkGetPhysicalDeviceScreenPresentationSupportQNX;
#endif /* defined(VK_QNX_screen_surface) */
#if defined(VK_VALVE_descriptor_set_host_mapping)
PFN_vkGetDescriptorSetHostMappingVALVE vkGetDescriptorSetHostMappingVALVE;
PFN_vkGetDescriptorSetLayoutHostMappingInfoVALVE vkGetDescriptorSetLayoutHostMappingInfoVALVE;
#endif /* defined(VK_VALVE_descriptor_set_host_mapping) */
#if (defined(VK_EXT_depth_clamp_control)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_depth_clamp_control))
PFN_vkCmdSetDepthClampRangeEXT vkCmdSetDepthClampRangeEXT;
#endif /* (defined(VK_EXT_depth_clamp_control)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_depth_clamp_control)) */
#if (defined(VK_EXT_extended_dynamic_state)) || (defined(VK_EXT_shader_object))
PFN_vkCmdBindVertexBuffers2EXT vkCmdBindVertexBuffers2EXT;
PFN_vkCmdSetCullModeEXT vkCmdSetCullModeEXT;
PFN_vkCmdSetDepthBoundsTestEnableEXT vkCmdSetDepthBoundsTestEnableEXT;
PFN_vkCmdSetDepthCompareOpEXT vkCmdSetDepthCompareOpEXT;
PFN_vkCmdSetDepthTestEnableEXT vkCmdSetDepthTestEnableEXT;
PFN_vkCmdSetDepthWriteEnableEXT vkCmdSetDepthWriteEnableEXT;
PFN_vkCmdSetFrontFaceEXT vkCmdSetFrontFaceEXT;
PFN_vkCmdSetPrimitiveTopologyEXT vkCmdSetPrimitiveTopologyEXT;
PFN_vkCmdSetScissorWithCountEXT vkCmdSetScissorWithCountEXT;
PFN_vkCmdSetStencilOpEXT vkCmdSetStencilOpEXT;
PFN_vkCmdSetStencilTestEnableEXT vkCmdSetStencilTestEnableEXT;
PFN_vkCmdSetViewportWithCountEXT vkCmdSetViewportWithCountEXT;
#endif /* (defined(VK_EXT_extended_dynamic_state)) || (defined(VK_EXT_shader_object)) */
#if (defined(VK_EXT_extended_dynamic_state2)) || (defined(VK_EXT_shader_object))
PFN_vkCmdSetDepthBiasEnableEXT vkCmdSetDepthBiasEnableEXT;
PFN_vkCmdSetLogicOpEXT vkCmdSetLogicOpEXT;
PFN_vkCmdSetPatchControlPointsEXT vkCmdSetPatchControlPointsEXT;
PFN_vkCmdSetPrimitiveRestartEnableEXT vkCmdSetPrimitiveRestartEnableEXT;
PFN_vkCmdSetRasterizerDiscardEnableEXT vkCmdSetRasterizerDiscardEnableEXT;
#endif /* (defined(VK_EXT_extended_dynamic_state2)) || (defined(VK_EXT_shader_object)) */
#if (defined(VK_EXT_extended_dynamic_state3)) || (defined(VK_EXT_shader_object))
PFN_vkCmdSetAlphaToCoverageEnableEXT vkCmdSetAlphaToCoverageEnableEXT;
PFN_vkCmdSetAlphaToOneEnableEXT vkCmdSetAlphaToOneEnableEXT;
PFN_vkCmdSetColorBlendEnableEXT vkCmdSetColorBlendEnableEXT;
PFN_vkCmdSetColorBlendEquationEXT vkCmdSetColorBlendEquationEXT;
PFN_vkCmdSetColorWriteMaskEXT vkCmdSetColorWriteMaskEXT;
PFN_vkCmdSetDepthClampEnableEXT vkCmdSetDepthClampEnableEXT;
PFN_vkCmdSetLogicOpEnableEXT vkCmdSetLogicOpEnableEXT;
PFN_vkCmdSetPolygonModeEXT vkCmdSetPolygonModeEXT;
PFN_vkCmdSetRasterizationSamplesEXT vkCmdSetRasterizationSamplesEXT;
PFN_vkCmdSetSampleMaskEXT vkCmdSetSampleMaskEXT;
#endif /* (defined(VK_EXT_extended_dynamic_state3)) || (defined(VK_EXT_shader_object)) */
#if (defined(VK_EXT_extended_dynamic_state3) && (defined(VK_KHR_maintenance2) || defined(VK_VERSION_1_1))) || (defined(VK_EXT_shader_object))
PFN_vkCmdSetTessellationDomainOriginEXT vkCmdSetTessellationDomainOriginEXT;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && (defined(VK_KHR_maintenance2) || defined(VK_VERSION_1_1))) || (defined(VK_EXT_shader_object)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_transform_feedback)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_transform_feedback))
PFN_vkCmdSetRasterizationStreamEXT vkCmdSetRasterizationStreamEXT;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_transform_feedback)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_transform_feedback)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_conservative_rasterization)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_conservative_rasterization))
PFN_vkCmdSetConservativeRasterizationModeEXT vkCmdSetConservativeRasterizationModeEXT;
PFN_vkCmdSetExtraPrimitiveOverestimationSizeEXT vkCmdSetExtraPrimitiveOverestimationSizeEXT;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_conservative_rasterization)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_conservative_rasterization)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_depth_clip_enable)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_depth_clip_enable))
PFN_vkCmdSetDepthClipEnableEXT vkCmdSetDepthClipEnableEXT;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_depth_clip_enable)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_depth_clip_enable)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_sample_locations)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_sample_locations))
PFN_vkCmdSetSampleLocationsEnableEXT vkCmdSetSampleLocationsEnableEXT;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_sample_locations)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_sample_locations)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_blend_operation_advanced)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_blend_operation_advanced))
PFN_vkCmdSetColorBlendAdvancedEXT vkCmdSetColorBlendAdvancedEXT;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_blend_operation_advanced)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_blend_operation_advanced)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_provoking_vertex)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_provoking_vertex))
PFN_vkCmdSetProvokingVertexModeEXT vkCmdSetProvokingVertexModeEXT;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_provoking_vertex)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_provoking_vertex)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_line_rasterization)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_line_rasterization))
PFN_vkCmdSetLineRasterizationModeEXT vkCmdSetLineRasterizationModeEXT;
PFN_vkCmdSetLineStippleEnableEXT vkCmdSetLineStippleEnableEXT;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_line_rasterization)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_line_rasterization)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_depth_clip_control)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_depth_clip_control))
PFN_vkCmdSetDepthClipNegativeOneToOneEXT vkCmdSetDepthClipNegativeOneToOneEXT;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_depth_clip_control)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_depth_clip_control)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_clip_space_w_scaling)) || (defined(VK_EXT_shader_object) && defined(VK_NV_clip_space_w_scaling))
PFN_vkCmdSetViewportWScalingEnableNV vkCmdSetViewportWScalingEnableNV;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_clip_space_w_scaling)) || (defined(VK_EXT_shader_object) && defined(VK_NV_clip_space_w_scaling)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_viewport_swizzle)) || (defined(VK_EXT_shader_object) && defined(VK_NV_viewport_swizzle))
PFN_vkCmdSetViewportSwizzleNV vkCmdSetViewportSwizzleNV;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_viewport_swizzle)) || (defined(VK_EXT_shader_object) && defined(VK_NV_viewport_swizzle)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_fragment_coverage_to_color)) || (defined(VK_EXT_shader_object) && defined(VK_NV_fragment_coverage_to_color))
PFN_vkCmdSetCoverageToColorEnableNV vkCmdSetCoverageToColorEnableNV;
PFN_vkCmdSetCoverageToColorLocationNV vkCmdSetCoverageToColorLocationNV;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_fragment_coverage_to_color)) || (defined(VK_EXT_shader_object) && defined(VK_NV_fragment_coverage_to_color)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_framebuffer_mixed_samples)) || (defined(VK_EXT_shader_object) && defined(VK_NV_framebuffer_mixed_samples))
PFN_vkCmdSetCoverageModulationModeNV vkCmdSetCoverageModulationModeNV;
PFN_vkCmdSetCoverageModulationTableEnableNV vkCmdSetCoverageModulationTableEnableNV;
PFN_vkCmdSetCoverageModulationTableNV vkCmdSetCoverageModulationTableNV;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_framebuffer_mixed_samples)) || (defined(VK_EXT_shader_object) && defined(VK_NV_framebuffer_mixed_samples)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_shading_rate_image)) || (defined(VK_EXT_shader_object) && defined(VK_NV_shading_rate_image))
PFN_vkCmdSetShadingRateImageEnableNV vkCmdSetShadingRateImageEnableNV;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_shading_rate_image)) || (defined(VK_EXT_shader_object) && defined(VK_NV_shading_rate_image)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_representative_fragment_test)) || (defined(VK_EXT_shader_object) && defined(VK_NV_representative_fragment_test))
PFN_vkCmdSetRepresentativeFragmentTestEnableNV vkCmdSetRepresentativeFragmentTestEnableNV;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_representative_fragment_test)) || (defined(VK_EXT_shader_object) && defined(VK_NV_representative_fragment_test)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_coverage_reduction_mode)) || (defined(VK_EXT_shader_object) && defined(VK_NV_coverage_reduction_mode))
PFN_vkCmdSetCoverageReductionModeNV vkCmdSetCoverageReductionModeNV;
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_coverage_reduction_mode)) || (defined(VK_EXT_shader_object) && defined(VK_NV_coverage_reduction_mode)) */
#if (defined(VK_EXT_host_image_copy)) || (defined(VK_EXT_image_compression_control))
PFN_vkGetImageSubresourceLayout2EXT vkGetImageSubresourceLayout2EXT;
#endif /* (defined(VK_EXT_host_image_copy)) || (defined(VK_EXT_image_compression_control)) */
#if (defined(VK_EXT_shader_object)) || (defined(VK_EXT_vertex_input_dynamic_state))
PFN_vkCmdSetVertexInputEXT vkCmdSetVertexInputEXT;
#endif /* (defined(VK_EXT_shader_object)) || (defined(VK_EXT_vertex_input_dynamic_state)) */
#if (defined(VK_KHR_descriptor_update_template) && defined(VK_KHR_push_descriptor)) || (defined(VK_KHR_push_descriptor) && (defined(VK_VERSION_1_1) || defined(VK_KHR_descriptor_update_template)))
PFN_vkCmdPushDescriptorSetWithTemplateKHR vkCmdPushDescriptorSetWithTemplateKHR;
#endif /* (defined(VK_KHR_descriptor_update_template) && defined(VK_KHR_push_descriptor)) || (defined(VK_KHR_push_descriptor) && (defined(VK_VERSION_1_1) || defined(VK_KHR_descriptor_update_template))) */
#if (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1))
PFN_vkGetDeviceGroupPresentCapabilitiesKHR vkGetDeviceGroupPresentCapabilitiesKHR;
PFN_vkGetDeviceGroupSurfacePresentModesKHR vkGetDeviceGroupSurfacePresentModesKHR;
PFN_vkGetPhysicalDevicePresentRectanglesKHR vkGetPhysicalDevicePresentRectanglesKHR;
#endif /* (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1)) */
#if (defined(VK_KHR_device_group) && defined(VK_KHR_swapchain)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1))
PFN_vkAcquireNextImage2KHR vkAcquireNextImage2KHR;
#endif /* (defined(VK_KHR_device_group) && defined(VK_KHR_swapchain)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1)) */
/* LAHAR_VK_PROTOTYPES_C */

#define lahar_load(name) name = (PFN_##name)loadfn(#name)

static uint32_t lahar_load_loader(LaharLoaderFunc loadfn) {
/* LAHAR_VK_LOAD_LOADER */
#if defined(VK_VERSION_1_0)
    lahar_load(vkCreateInstance);
    lahar_load(vkEnumerateInstanceExtensionProperties);
    lahar_load(vkEnumerateInstanceLayerProperties);
#endif /* defined(VK_VERSION_1_0) */
#if defined(VK_VERSION_1_1)
    lahar_load(vkEnumerateInstanceVersion);
#endif /* defined(VK_VERSION_1_1) */
/* LAHAR_VK_LOAD_LOADER */

#if defined(VK_VERSION_1_0)
    if (!vkCreateInstance) { return LAHAR_ERR_LOAD_FAILURE; }
    if (!vkEnumerateInstanceExtensionProperties) { return LAHAR_ERR_LOAD_FAILURE; }
    if (!vkEnumerateInstanceLayerProperties) { return LAHAR_ERR_LOAD_FAILURE; }
#endif
#if defined(VK_VERSION_1_1)
    if (!vkEnumerateInstanceVersion) { return LAHAR_ERR_LOAD_FAILURE; }
#endif /* defined(VK_VERSION_1_1) */

    lahar_load(vkGetInstanceProcAddr);
    if (!vkGetInstanceProcAddr) { return LAHAR_ERR_LOAD_FAILURE; }

    return LAHAR_ERR_SUCCESS;
}

static uint32_t lahar_load_instance(LaharLoaderFunc loadfn) {
/* LAHAR_VK_LOAD_INSTANCE */
#if defined(VK_VERSION_1_0)
    lahar_load(vkCreateDevice);
    lahar_load(vkDestroyInstance);
    lahar_load(vkEnumerateDeviceExtensionProperties);
    lahar_load(vkEnumerateDeviceLayerProperties);
    lahar_load(vkEnumeratePhysicalDevices);
    lahar_load(vkGetDeviceProcAddr);
    lahar_load(vkGetPhysicalDeviceFeatures);
    lahar_load(vkGetPhysicalDeviceFormatProperties);
    lahar_load(vkGetPhysicalDeviceImageFormatProperties);
    lahar_load(vkGetPhysicalDeviceMemoryProperties);
    lahar_load(vkGetPhysicalDeviceProperties);
    lahar_load(vkGetPhysicalDeviceQueueFamilyProperties);
    lahar_load(vkGetPhysicalDeviceSparseImageFormatProperties);
#endif /* defined(VK_VERSION_1_0) */
#if defined(VK_VERSION_1_1)
    lahar_load(vkEnumeratePhysicalDeviceGroups);
    lahar_load(vkGetPhysicalDeviceExternalBufferProperties);
    lahar_load(vkGetPhysicalDeviceExternalFenceProperties);
    lahar_load(vkGetPhysicalDeviceExternalSemaphoreProperties);
    lahar_load(vkGetPhysicalDeviceFeatures2);
    lahar_load(vkGetPhysicalDeviceFormatProperties2);
    lahar_load(vkGetPhysicalDeviceImageFormatProperties2);
    lahar_load(vkGetPhysicalDeviceMemoryProperties2);
    lahar_load(vkGetPhysicalDeviceProperties2);
    lahar_load(vkGetPhysicalDeviceQueueFamilyProperties2);
    lahar_load(vkGetPhysicalDeviceSparseImageFormatProperties2);
#endif /* defined(VK_VERSION_1_1) */
#if defined(VK_VERSION_1_3)
    lahar_load(vkGetPhysicalDeviceToolProperties);
#endif /* defined(VK_VERSION_1_3) */
#if defined(VK_ARM_data_graph)
    lahar_load(vkGetPhysicalDeviceQueueFamilyDataGraphProcessingEnginePropertiesARM);
    lahar_load(vkGetPhysicalDeviceQueueFamilyDataGraphPropertiesARM);
#endif /* defined(VK_ARM_data_graph) */
#if defined(VK_ARM_tensors)
    lahar_load(vkGetPhysicalDeviceExternalTensorPropertiesARM);
#endif /* defined(VK_ARM_tensors) */
#if defined(VK_EXT_acquire_drm_display)
    lahar_load(vkAcquireDrmDisplayEXT);
    lahar_load(vkGetDrmDisplayEXT);
#endif /* defined(VK_EXT_acquire_drm_display) */
#if defined(VK_EXT_acquire_xlib_display)
    lahar_load(vkAcquireXlibDisplayEXT);
    lahar_load(vkGetRandROutputDisplayEXT);
#endif /* defined(VK_EXT_acquire_xlib_display) */
#if defined(VK_EXT_calibrated_timestamps)
    lahar_load(vkGetPhysicalDeviceCalibrateableTimeDomainsEXT);
#endif /* defined(VK_EXT_calibrated_timestamps) */
#if defined(VK_EXT_debug_report)
    lahar_load(vkCreateDebugReportCallbackEXT);
    lahar_load(vkDebugReportMessageEXT);
    lahar_load(vkDestroyDebugReportCallbackEXT);
#endif /* defined(VK_EXT_debug_report) */
#if defined(VK_EXT_debug_utils)
    lahar_load(vkCmdBeginDebugUtilsLabelEXT);
    lahar_load(vkCmdEndDebugUtilsLabelEXT);
    lahar_load(vkCmdInsertDebugUtilsLabelEXT);
    lahar_load(vkCreateDebugUtilsMessengerEXT);
    lahar_load(vkDestroyDebugUtilsMessengerEXT);
    lahar_load(vkQueueBeginDebugUtilsLabelEXT);
    lahar_load(vkQueueEndDebugUtilsLabelEXT);
    lahar_load(vkQueueInsertDebugUtilsLabelEXT);
    lahar_load(vkSetDebugUtilsObjectNameEXT);
    lahar_load(vkSetDebugUtilsObjectTagEXT);
    lahar_load(vkSubmitDebugUtilsMessageEXT);
#endif /* defined(VK_EXT_debug_utils) */
#if defined(VK_EXT_direct_mode_display)
    lahar_load(vkReleaseDisplayEXT);
#endif /* defined(VK_EXT_direct_mode_display) */
#if defined(VK_EXT_directfb_surface)
    lahar_load(vkCreateDirectFBSurfaceEXT);
    lahar_load(vkGetPhysicalDeviceDirectFBPresentationSupportEXT);
#endif /* defined(VK_EXT_directfb_surface) */
#if defined(VK_EXT_display_surface_counter)
    lahar_load(vkGetPhysicalDeviceSurfaceCapabilities2EXT);
#endif /* defined(VK_EXT_display_surface_counter) */
#if defined(VK_EXT_full_screen_exclusive)
    lahar_load(vkGetPhysicalDeviceSurfacePresentModes2EXT);
#endif /* defined(VK_EXT_full_screen_exclusive) */
#if defined(VK_EXT_headless_surface)
    lahar_load(vkCreateHeadlessSurfaceEXT);
#endif /* defined(VK_EXT_headless_surface) */
#if defined(VK_EXT_metal_surface)
    lahar_load(vkCreateMetalSurfaceEXT);
#endif /* defined(VK_EXT_metal_surface) */
#if defined(VK_EXT_sample_locations)
    lahar_load(vkGetPhysicalDeviceMultisamplePropertiesEXT);
#endif /* defined(VK_EXT_sample_locations) */
#if defined(VK_EXT_tooling_info)
    lahar_load(vkGetPhysicalDeviceToolPropertiesEXT);
#endif /* defined(VK_EXT_tooling_info) */
#if defined(VK_FUCHSIA_imagepipe_surface)
    lahar_load(vkCreateImagePipeSurfaceFUCHSIA);
#endif /* defined(VK_FUCHSIA_imagepipe_surface) */
#if defined(VK_GGP_stream_descriptor_surface)
    lahar_load(vkCreateStreamDescriptorSurfaceGGP);
#endif /* defined(VK_GGP_stream_descriptor_surface) */
#if defined(VK_KHR_android_surface)
    lahar_load(vkCreateAndroidSurfaceKHR);
#endif /* defined(VK_KHR_android_surface) */
#if defined(VK_KHR_calibrated_timestamps)
    lahar_load(vkGetPhysicalDeviceCalibrateableTimeDomainsKHR);
#endif /* defined(VK_KHR_calibrated_timestamps) */
#if defined(VK_KHR_cooperative_matrix)
    lahar_load(vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR);
#endif /* defined(VK_KHR_cooperative_matrix) */
#if defined(VK_KHR_device_group_creation)
    lahar_load(vkEnumeratePhysicalDeviceGroupsKHR);
#endif /* defined(VK_KHR_device_group_creation) */
#if defined(VK_KHR_display)
    lahar_load(vkCreateDisplayModeKHR);
    lahar_load(vkCreateDisplayPlaneSurfaceKHR);
    lahar_load(vkGetDisplayModePropertiesKHR);
    lahar_load(vkGetDisplayPlaneCapabilitiesKHR);
    lahar_load(vkGetDisplayPlaneSupportedDisplaysKHR);
    lahar_load(vkGetPhysicalDeviceDisplayPlanePropertiesKHR);
    lahar_load(vkGetPhysicalDeviceDisplayPropertiesKHR);
#endif /* defined(VK_KHR_display) */
#if defined(VK_KHR_external_fence_capabilities)
    lahar_load(vkGetPhysicalDeviceExternalFencePropertiesKHR);
#endif /* defined(VK_KHR_external_fence_capabilities) */
#if defined(VK_KHR_external_memory_capabilities)
    lahar_load(vkGetPhysicalDeviceExternalBufferPropertiesKHR);
#endif /* defined(VK_KHR_external_memory_capabilities) */
#if defined(VK_KHR_external_semaphore_capabilities)
    lahar_load(vkGetPhysicalDeviceExternalSemaphorePropertiesKHR);
#endif /* defined(VK_KHR_external_semaphore_capabilities) */
#if defined(VK_KHR_fragment_shading_rate)
    lahar_load(vkGetPhysicalDeviceFragmentShadingRatesKHR);
#endif /* defined(VK_KHR_fragment_shading_rate) */
#if defined(VK_KHR_get_display_properties2)
    lahar_load(vkGetDisplayModeProperties2KHR);
    lahar_load(vkGetDisplayPlaneCapabilities2KHR);
    lahar_load(vkGetPhysicalDeviceDisplayPlaneProperties2KHR);
    lahar_load(vkGetPhysicalDeviceDisplayProperties2KHR);
#endif /* defined(VK_KHR_get_display_properties2) */
#if defined(VK_KHR_get_physical_device_properties2)
    lahar_load(vkGetPhysicalDeviceFeatures2KHR);
    lahar_load(vkGetPhysicalDeviceFormatProperties2KHR);
    lahar_load(vkGetPhysicalDeviceImageFormatProperties2KHR);
    lahar_load(vkGetPhysicalDeviceMemoryProperties2KHR);
    lahar_load(vkGetPhysicalDeviceProperties2KHR);
    lahar_load(vkGetPhysicalDeviceQueueFamilyProperties2KHR);
    lahar_load(vkGetPhysicalDeviceSparseImageFormatProperties2KHR);
#endif /* defined(VK_KHR_get_physical_device_properties2) */
#if defined(VK_KHR_get_surface_capabilities2)
    lahar_load(vkGetPhysicalDeviceSurfaceCapabilities2KHR);
    lahar_load(vkGetPhysicalDeviceSurfaceFormats2KHR);
#endif /* defined(VK_KHR_get_surface_capabilities2) */
#if defined(VK_KHR_performance_query)
    lahar_load(vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR);
    lahar_load(vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR);
#endif /* defined(VK_KHR_performance_query) */
#if defined(VK_KHR_surface)
    lahar_load(vkDestroySurfaceKHR);
    lahar_load(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    lahar_load(vkGetPhysicalDeviceSurfaceFormatsKHR);
    lahar_load(vkGetPhysicalDeviceSurfacePresentModesKHR);
    lahar_load(vkGetPhysicalDeviceSurfaceSupportKHR);
#endif /* defined(VK_KHR_surface) */
#if defined(VK_KHR_video_encode_queue)
    lahar_load(vkGetPhysicalDeviceVideoEncodeQualityLevelPropertiesKHR);
#endif /* defined(VK_KHR_video_encode_queue) */
#if defined(VK_KHR_video_queue)
    lahar_load(vkGetPhysicalDeviceVideoCapabilitiesKHR);
    lahar_load(vkGetPhysicalDeviceVideoFormatPropertiesKHR);
#endif /* defined(VK_KHR_video_queue) */
#if defined(VK_KHR_wayland_surface)
    lahar_load(vkCreateWaylandSurfaceKHR);
    lahar_load(vkGetPhysicalDeviceWaylandPresentationSupportKHR);
#endif /* defined(VK_KHR_wayland_surface) */
#if defined(VK_KHR_win32_surface)
    lahar_load(vkCreateWin32SurfaceKHR);
    lahar_load(vkGetPhysicalDeviceWin32PresentationSupportKHR);
#endif /* defined(VK_KHR_win32_surface) */
#if defined(VK_KHR_xcb_surface)
    lahar_load(vkCreateXcbSurfaceKHR);
    lahar_load(vkGetPhysicalDeviceXcbPresentationSupportKHR);
#endif /* defined(VK_KHR_xcb_surface) */
#if defined(VK_KHR_xlib_surface)
    lahar_load(vkCreateXlibSurfaceKHR);
    lahar_load(vkGetPhysicalDeviceXlibPresentationSupportKHR);
#endif /* defined(VK_KHR_xlib_surface) */
#if defined(VK_MVK_ios_surface)
    lahar_load(vkCreateIOSSurfaceMVK);
#endif /* defined(VK_MVK_ios_surface) */
#if defined(VK_MVK_macos_surface)
    lahar_load(vkCreateMacOSSurfaceMVK);
#endif /* defined(VK_MVK_macos_surface) */
#if defined(VK_NN_vi_surface)
    lahar_load(vkCreateViSurfaceNN);
#endif /* defined(VK_NN_vi_surface) */
#if defined(VK_NV_acquire_winrt_display)
    lahar_load(vkAcquireWinrtDisplayNV);
    lahar_load(vkGetWinrtDisplayNV);
#endif /* defined(VK_NV_acquire_winrt_display) */
#if defined(VK_NV_cooperative_matrix)
    lahar_load(vkGetPhysicalDeviceCooperativeMatrixPropertiesNV);
#endif /* defined(VK_NV_cooperative_matrix) */
#if defined(VK_NV_cooperative_matrix2)
    lahar_load(vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV);
#endif /* defined(VK_NV_cooperative_matrix2) */
#if defined(VK_NV_cooperative_vector)
    lahar_load(vkGetPhysicalDeviceCooperativeVectorPropertiesNV);
#endif /* defined(VK_NV_cooperative_vector) */
#if defined(VK_NV_coverage_reduction_mode)
    lahar_load(vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV);
#endif /* defined(VK_NV_coverage_reduction_mode) */
#if defined(VK_NV_external_memory_capabilities)
    lahar_load(vkGetPhysicalDeviceExternalImageFormatPropertiesNV);
#endif /* defined(VK_NV_external_memory_capabilities) */
#if defined(VK_NV_optical_flow)
    lahar_load(vkGetPhysicalDeviceOpticalFlowImageFormatsNV);
#endif /* defined(VK_NV_optical_flow) */
#if defined(VK_OHOS_surface)
    lahar_load(vkCreateSurfaceOHOS);
#endif /* defined(VK_OHOS_surface) */
#if defined(VK_QNX_screen_surface)
    lahar_load(vkCreateScreenSurfaceQNX);
    lahar_load(vkGetPhysicalDeviceScreenPresentationSupportQNX);
#endif /* defined(VK_QNX_screen_surface) */
#if (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1))
    lahar_load(vkGetPhysicalDevicePresentRectanglesKHR);
#endif /* (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1)) */
/* LAHAR_VK_LOAD_INSTANCE */

    return LAHAR_ERR_SUCCESS;
}

static uint32_t lahar_load_device(LaharLoaderFunc loadfn) {
/* LAHAR_VK_LOAD_DEVICE */
#if defined(VK_VERSION_1_0)
    lahar_load(vkAllocateCommandBuffers);
    lahar_load(vkAllocateDescriptorSets);
    lahar_load(vkAllocateMemory);
    lahar_load(vkBeginCommandBuffer);
    lahar_load(vkBindBufferMemory);
    lahar_load(vkBindImageMemory);
    lahar_load(vkCmdBeginQuery);
    lahar_load(vkCmdBeginRenderPass);
    lahar_load(vkCmdBindDescriptorSets);
    lahar_load(vkCmdBindIndexBuffer);
    lahar_load(vkCmdBindPipeline);
    lahar_load(vkCmdBindVertexBuffers);
    lahar_load(vkCmdBlitImage);
    lahar_load(vkCmdClearAttachments);
    lahar_load(vkCmdClearColorImage);
    lahar_load(vkCmdClearDepthStencilImage);
    lahar_load(vkCmdCopyBuffer);
    lahar_load(vkCmdCopyBufferToImage);
    lahar_load(vkCmdCopyImage);
    lahar_load(vkCmdCopyImageToBuffer);
    lahar_load(vkCmdCopyQueryPoolResults);
    lahar_load(vkCmdDispatch);
    lahar_load(vkCmdDispatchIndirect);
    lahar_load(vkCmdDraw);
    lahar_load(vkCmdDrawIndexed);
    lahar_load(vkCmdDrawIndexedIndirect);
    lahar_load(vkCmdDrawIndirect);
    lahar_load(vkCmdEndQuery);
    lahar_load(vkCmdEndRenderPass);
    lahar_load(vkCmdExecuteCommands);
    lahar_load(vkCmdFillBuffer);
    lahar_load(vkCmdNextSubpass);
    lahar_load(vkCmdPipelineBarrier);
    lahar_load(vkCmdPushConstants);
    lahar_load(vkCmdResetEvent);
    lahar_load(vkCmdResetQueryPool);
    lahar_load(vkCmdResolveImage);
    lahar_load(vkCmdSetBlendConstants);
    lahar_load(vkCmdSetDepthBias);
    lahar_load(vkCmdSetDepthBounds);
    lahar_load(vkCmdSetEvent);
    lahar_load(vkCmdSetLineWidth);
    lahar_load(vkCmdSetScissor);
    lahar_load(vkCmdSetStencilCompareMask);
    lahar_load(vkCmdSetStencilReference);
    lahar_load(vkCmdSetStencilWriteMask);
    lahar_load(vkCmdSetViewport);
    lahar_load(vkCmdUpdateBuffer);
    lahar_load(vkCmdWaitEvents);
    lahar_load(vkCmdWriteTimestamp);
    lahar_load(vkCreateBuffer);
    lahar_load(vkCreateBufferView);
    lahar_load(vkCreateCommandPool);
    lahar_load(vkCreateComputePipelines);
    lahar_load(vkCreateDescriptorPool);
    lahar_load(vkCreateDescriptorSetLayout);
    lahar_load(vkCreateEvent);
    lahar_load(vkCreateFence);
    lahar_load(vkCreateFramebuffer);
    lahar_load(vkCreateGraphicsPipelines);
    lahar_load(vkCreateImage);
    lahar_load(vkCreateImageView);
    lahar_load(vkCreatePipelineCache);
    lahar_load(vkCreatePipelineLayout);
    lahar_load(vkCreateQueryPool);
    lahar_load(vkCreateRenderPass);
    lahar_load(vkCreateSampler);
    lahar_load(vkCreateSemaphore);
    lahar_load(vkCreateShaderModule);
    lahar_load(vkDestroyBuffer);
    lahar_load(vkDestroyBufferView);
    lahar_load(vkDestroyCommandPool);
    lahar_load(vkDestroyDescriptorPool);
    lahar_load(vkDestroyDescriptorSetLayout);
    lahar_load(vkDestroyDevice);
    lahar_load(vkDestroyEvent);
    lahar_load(vkDestroyFence);
    lahar_load(vkDestroyFramebuffer);
    lahar_load(vkDestroyImage);
    lahar_load(vkDestroyImageView);
    lahar_load(vkDestroyPipeline);
    lahar_load(vkDestroyPipelineCache);
    lahar_load(vkDestroyPipelineLayout);
    lahar_load(vkDestroyQueryPool);
    lahar_load(vkDestroyRenderPass);
    lahar_load(vkDestroySampler);
    lahar_load(vkDestroySemaphore);
    lahar_load(vkDestroyShaderModule);
    lahar_load(vkDeviceWaitIdle);
    lahar_load(vkEndCommandBuffer);
    lahar_load(vkFlushMappedMemoryRanges);
    lahar_load(vkFreeCommandBuffers);
    lahar_load(vkFreeDescriptorSets);
    lahar_load(vkFreeMemory);
    lahar_load(vkGetBufferMemoryRequirements);
    lahar_load(vkGetDeviceMemoryCommitment);
    lahar_load(vkGetDeviceQueue);
    lahar_load(vkGetEventStatus);
    lahar_load(vkGetFenceStatus);
    lahar_load(vkGetImageMemoryRequirements);
    lahar_load(vkGetImageSparseMemoryRequirements);
    lahar_load(vkGetImageSubresourceLayout);
    lahar_load(vkGetPipelineCacheData);
    lahar_load(vkGetQueryPoolResults);
    lahar_load(vkGetRenderAreaGranularity);
    lahar_load(vkInvalidateMappedMemoryRanges);
    lahar_load(vkMapMemory);
    lahar_load(vkMergePipelineCaches);
    lahar_load(vkQueueBindSparse);
    lahar_load(vkQueueSubmit);
    lahar_load(vkQueueWaitIdle);
    lahar_load(vkResetCommandBuffer);
    lahar_load(vkResetCommandPool);
    lahar_load(vkResetDescriptorPool);
    lahar_load(vkResetEvent);
    lahar_load(vkResetFences);
    lahar_load(vkSetEvent);
    lahar_load(vkUnmapMemory);
    lahar_load(vkUpdateDescriptorSets);
    lahar_load(vkWaitForFences);
#endif /* defined(VK_VERSION_1_0) */
#if defined(VK_VERSION_1_1)
    lahar_load(vkBindBufferMemory2);
    lahar_load(vkBindImageMemory2);
    lahar_load(vkCmdDispatchBase);
    lahar_load(vkCmdSetDeviceMask);
    lahar_load(vkCreateDescriptorUpdateTemplate);
    lahar_load(vkCreateSamplerYcbcrConversion);
    lahar_load(vkDestroyDescriptorUpdateTemplate);
    lahar_load(vkDestroySamplerYcbcrConversion);
    lahar_load(vkGetBufferMemoryRequirements2);
    lahar_load(vkGetDescriptorSetLayoutSupport);
    lahar_load(vkGetDeviceGroupPeerMemoryFeatures);
    lahar_load(vkGetDeviceQueue2);
    lahar_load(vkGetImageMemoryRequirements2);
    lahar_load(vkGetImageSparseMemoryRequirements2);
    lahar_load(vkTrimCommandPool);
    lahar_load(vkUpdateDescriptorSetWithTemplate);
#endif /* defined(VK_VERSION_1_1) */
#if defined(VK_VERSION_1_2)
    lahar_load(vkCmdBeginRenderPass2);
    lahar_load(vkCmdDrawIndexedIndirectCount);
    lahar_load(vkCmdDrawIndirectCount);
    lahar_load(vkCmdEndRenderPass2);
    lahar_load(vkCmdNextSubpass2);
    lahar_load(vkCreateRenderPass2);
    lahar_load(vkGetBufferDeviceAddress);
    lahar_load(vkGetBufferOpaqueCaptureAddress);
    lahar_load(vkGetDeviceMemoryOpaqueCaptureAddress);
    lahar_load(vkGetSemaphoreCounterValue);
    lahar_load(vkResetQueryPool);
    lahar_load(vkSignalSemaphore);
    lahar_load(vkWaitSemaphores);
#endif /* defined(VK_VERSION_1_2) */
#if defined(VK_VERSION_1_3)
    lahar_load(vkCmdBeginRendering);
    lahar_load(vkCmdBindVertexBuffers2);
    lahar_load(vkCmdBlitImage2);
    lahar_load(vkCmdCopyBuffer2);
    lahar_load(vkCmdCopyBufferToImage2);
    lahar_load(vkCmdCopyImage2);
    lahar_load(vkCmdCopyImageToBuffer2);
    lahar_load(vkCmdEndRendering);
    lahar_load(vkCmdPipelineBarrier2);
    lahar_load(vkCmdResetEvent2);
    lahar_load(vkCmdResolveImage2);
    lahar_load(vkCmdSetCullMode);
    lahar_load(vkCmdSetDepthBiasEnable);
    lahar_load(vkCmdSetDepthBoundsTestEnable);
    lahar_load(vkCmdSetDepthCompareOp);
    lahar_load(vkCmdSetDepthTestEnable);
    lahar_load(vkCmdSetDepthWriteEnable);
    lahar_load(vkCmdSetEvent2);
    lahar_load(vkCmdSetFrontFace);
    lahar_load(vkCmdSetPrimitiveRestartEnable);
    lahar_load(vkCmdSetPrimitiveTopology);
    lahar_load(vkCmdSetRasterizerDiscardEnable);
    lahar_load(vkCmdSetScissorWithCount);
    lahar_load(vkCmdSetStencilOp);
    lahar_load(vkCmdSetStencilTestEnable);
    lahar_load(vkCmdSetViewportWithCount);
    lahar_load(vkCmdWaitEvents2);
    lahar_load(vkCmdWriteTimestamp2);
    lahar_load(vkCreatePrivateDataSlot);
    lahar_load(vkDestroyPrivateDataSlot);
    lahar_load(vkGetDeviceBufferMemoryRequirements);
    lahar_load(vkGetDeviceImageMemoryRequirements);
    lahar_load(vkGetDeviceImageSparseMemoryRequirements);
    lahar_load(vkGetPrivateData);
    lahar_load(vkQueueSubmit2);
    lahar_load(vkSetPrivateData);
#endif /* defined(VK_VERSION_1_3) */
#if defined(VK_VERSION_1_4)
    lahar_load(vkCmdBindDescriptorSets2);
    lahar_load(vkCmdBindIndexBuffer2);
    lahar_load(vkCmdPushConstants2);
    lahar_load(vkCmdPushDescriptorSet);
    lahar_load(vkCmdPushDescriptorSet2);
    lahar_load(vkCmdPushDescriptorSetWithTemplate);
    lahar_load(vkCmdPushDescriptorSetWithTemplate2);
    lahar_load(vkCmdSetLineStipple);
    lahar_load(vkCmdSetRenderingAttachmentLocations);
    lahar_load(vkCmdSetRenderingInputAttachmentIndices);
    lahar_load(vkCopyImageToImage);
    lahar_load(vkCopyImageToMemory);
    lahar_load(vkCopyMemoryToImage);
    lahar_load(vkGetDeviceImageSubresourceLayout);
    lahar_load(vkGetImageSubresourceLayout2);
    lahar_load(vkGetRenderingAreaGranularity);
    lahar_load(vkMapMemory2);
    lahar_load(vkTransitionImageLayout);
    lahar_load(vkUnmapMemory2);
#endif /* defined(VK_VERSION_1_4) */
#if defined(VK_AMDX_shader_enqueue)
    lahar_load(vkCmdDispatchGraphAMDX);
    lahar_load(vkCmdDispatchGraphIndirectAMDX);
    lahar_load(vkCmdDispatchGraphIndirectCountAMDX);
    lahar_load(vkCmdInitializeGraphScratchMemoryAMDX);
    lahar_load(vkCreateExecutionGraphPipelinesAMDX);
    lahar_load(vkGetExecutionGraphPipelineNodeIndexAMDX);
    lahar_load(vkGetExecutionGraphPipelineScratchSizeAMDX);
#endif /* defined(VK_AMDX_shader_enqueue) */
#if defined(VK_AMD_anti_lag)
    lahar_load(vkAntiLagUpdateAMD);
#endif /* defined(VK_AMD_anti_lag) */
#if defined(VK_AMD_buffer_marker)
    lahar_load(vkCmdWriteBufferMarkerAMD);
#endif /* defined(VK_AMD_buffer_marker) */
#if defined(VK_AMD_buffer_marker) && (defined(VK_VERSION_1_3) || defined(VK_KHR_synchronization2))
    lahar_load(vkCmdWriteBufferMarker2AMD);
#endif /* defined(VK_AMD_buffer_marker) && (defined(VK_VERSION_1_3) || defined(VK_KHR_synchronization2)) */
#if defined(VK_AMD_display_native_hdr)
    lahar_load(vkSetLocalDimmingAMD);
#endif /* defined(VK_AMD_display_native_hdr) */
#if defined(VK_AMD_draw_indirect_count)
    lahar_load(vkCmdDrawIndexedIndirectCountAMD);
    lahar_load(vkCmdDrawIndirectCountAMD);
#endif /* defined(VK_AMD_draw_indirect_count) */
#if defined(VK_AMD_shader_info)
    lahar_load(vkGetShaderInfoAMD);
#endif /* defined(VK_AMD_shader_info) */
#if defined(VK_ANDROID_external_memory_android_hardware_buffer)
    lahar_load(vkGetAndroidHardwareBufferPropertiesANDROID);
    lahar_load(vkGetMemoryAndroidHardwareBufferANDROID);
#endif /* defined(VK_ANDROID_external_memory_android_hardware_buffer) */
#if defined(VK_ARM_data_graph)
    lahar_load(vkBindDataGraphPipelineSessionMemoryARM);
    lahar_load(vkCmdDispatchDataGraphARM);
    lahar_load(vkCreateDataGraphPipelineSessionARM);
    lahar_load(vkCreateDataGraphPipelinesARM);
    lahar_load(vkDestroyDataGraphPipelineSessionARM);
    lahar_load(vkGetDataGraphPipelineAvailablePropertiesARM);
    lahar_load(vkGetDataGraphPipelinePropertiesARM);
    lahar_load(vkGetDataGraphPipelineSessionBindPointRequirementsARM);
    lahar_load(vkGetDataGraphPipelineSessionMemoryRequirementsARM);
#endif /* defined(VK_ARM_data_graph) */
#if defined(VK_ARM_tensors)
    lahar_load(vkBindTensorMemoryARM);
    lahar_load(vkCmdCopyTensorARM);
    lahar_load(vkCreateTensorARM);
    lahar_load(vkCreateTensorViewARM);
    lahar_load(vkDestroyTensorARM);
    lahar_load(vkDestroyTensorViewARM);
    lahar_load(vkGetDeviceTensorMemoryRequirementsARM);
    lahar_load(vkGetTensorMemoryRequirementsARM);
#endif /* defined(VK_ARM_tensors) */
#if defined(VK_ARM_tensors) && defined(VK_EXT_descriptor_buffer)
    lahar_load(vkGetTensorOpaqueCaptureDescriptorDataARM);
    lahar_load(vkGetTensorViewOpaqueCaptureDescriptorDataARM);
#endif /* defined(VK_ARM_tensors) && defined(VK_EXT_descriptor_buffer) */
#if defined(VK_EXT_attachment_feedback_loop_dynamic_state)
    lahar_load(vkCmdSetAttachmentFeedbackLoopEnableEXT);
#endif /* defined(VK_EXT_attachment_feedback_loop_dynamic_state) */
#if defined(VK_EXT_buffer_device_address)
    lahar_load(vkGetBufferDeviceAddressEXT);
#endif /* defined(VK_EXT_buffer_device_address) */
#if defined(VK_EXT_calibrated_timestamps)
    lahar_load(vkGetCalibratedTimestampsEXT);
#endif /* defined(VK_EXT_calibrated_timestamps) */
#if defined(VK_EXT_color_write_enable)
    lahar_load(vkCmdSetColorWriteEnableEXT);
#endif /* defined(VK_EXT_color_write_enable) */
#if defined(VK_EXT_conditional_rendering)
    lahar_load(vkCmdBeginConditionalRenderingEXT);
    lahar_load(vkCmdEndConditionalRenderingEXT);
#endif /* defined(VK_EXT_conditional_rendering) */
#if defined(VK_EXT_debug_marker)
    lahar_load(vkCmdDebugMarkerBeginEXT);
    lahar_load(vkCmdDebugMarkerEndEXT);
    lahar_load(vkCmdDebugMarkerInsertEXT);
    lahar_load(vkDebugMarkerSetObjectNameEXT);
    lahar_load(vkDebugMarkerSetObjectTagEXT);
#endif /* defined(VK_EXT_debug_marker) */
#if defined(VK_EXT_depth_bias_control)
    lahar_load(vkCmdSetDepthBias2EXT);
#endif /* defined(VK_EXT_depth_bias_control) */
#if defined(VK_EXT_descriptor_buffer)
    lahar_load(vkCmdBindDescriptorBufferEmbeddedSamplersEXT);
    lahar_load(vkCmdBindDescriptorBuffersEXT);
    lahar_load(vkCmdSetDescriptorBufferOffsetsEXT);
    lahar_load(vkGetBufferOpaqueCaptureDescriptorDataEXT);
    lahar_load(vkGetDescriptorEXT);
    lahar_load(vkGetDescriptorSetLayoutBindingOffsetEXT);
    lahar_load(vkGetDescriptorSetLayoutSizeEXT);
    lahar_load(vkGetImageOpaqueCaptureDescriptorDataEXT);
    lahar_load(vkGetImageViewOpaqueCaptureDescriptorDataEXT);
    lahar_load(vkGetSamplerOpaqueCaptureDescriptorDataEXT);
#endif /* defined(VK_EXT_descriptor_buffer) */
#if defined(VK_EXT_descriptor_buffer) && (defined(VK_KHR_acceleration_structure) || defined(VK_NV_ray_tracing))
    lahar_load(vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT);
#endif /* defined(VK_EXT_descriptor_buffer) && (defined(VK_KHR_acceleration_structure) || defined(VK_NV_ray_tracing)) */
#if defined(VK_EXT_device_fault)
    lahar_load(vkGetDeviceFaultInfoEXT);
#endif /* defined(VK_EXT_device_fault) */
#if defined(VK_EXT_device_generated_commands)
    lahar_load(vkCmdExecuteGeneratedCommandsEXT);
    lahar_load(vkCmdPreprocessGeneratedCommandsEXT);
    lahar_load(vkCreateIndirectCommandsLayoutEXT);
    lahar_load(vkCreateIndirectExecutionSetEXT);
    lahar_load(vkDestroyIndirectCommandsLayoutEXT);
    lahar_load(vkDestroyIndirectExecutionSetEXT);
    lahar_load(vkGetGeneratedCommandsMemoryRequirementsEXT);
    lahar_load(vkUpdateIndirectExecutionSetPipelineEXT);
    lahar_load(vkUpdateIndirectExecutionSetShaderEXT);
#endif /* defined(VK_EXT_device_generated_commands) */
#if defined(VK_EXT_discard_rectangles)
    lahar_load(vkCmdSetDiscardRectangleEXT);
#endif /* defined(VK_EXT_discard_rectangles) */
#if defined(VK_EXT_discard_rectangles) && VK_EXT_DISCARD_RECTANGLES_SPEC_VERSION >= 2
    lahar_load(vkCmdSetDiscardRectangleEnableEXT);
    lahar_load(vkCmdSetDiscardRectangleModeEXT);
#endif /* defined(VK_EXT_discard_rectangles) && VK_EXT_DISCARD_RECTANGLES_SPEC_VERSION >= 2 */
#if defined(VK_EXT_display_control)
    lahar_load(vkDisplayPowerControlEXT);
    lahar_load(vkGetSwapchainCounterEXT);
    lahar_load(vkRegisterDeviceEventEXT);
    lahar_load(vkRegisterDisplayEventEXT);
#endif /* defined(VK_EXT_display_control) */
#if defined(VK_EXT_external_memory_host)
    lahar_load(vkGetMemoryHostPointerPropertiesEXT);
#endif /* defined(VK_EXT_external_memory_host) */
#if defined(VK_EXT_external_memory_metal)
    lahar_load(vkGetMemoryMetalHandleEXT);
    lahar_load(vkGetMemoryMetalHandlePropertiesEXT);
#endif /* defined(VK_EXT_external_memory_metal) */
#if defined(VK_EXT_fragment_density_map_offset)
    lahar_load(vkCmdEndRendering2EXT);
#endif /* defined(VK_EXT_fragment_density_map_offset) */
#if defined(VK_EXT_full_screen_exclusive)
    lahar_load(vkAcquireFullScreenExclusiveModeEXT);
    lahar_load(vkReleaseFullScreenExclusiveModeEXT);
#endif /* defined(VK_EXT_full_screen_exclusive) */
#if defined(VK_EXT_full_screen_exclusive) && (defined(VK_KHR_device_group) || defined(VK_VERSION_1_1))
    lahar_load(vkGetDeviceGroupSurfacePresentModes2EXT);
#endif /* defined(VK_EXT_full_screen_exclusive) && (defined(VK_KHR_device_group) || defined(VK_VERSION_1_1)) */
#if defined(VK_EXT_hdr_metadata)
    lahar_load(vkSetHdrMetadataEXT);
#endif /* defined(VK_EXT_hdr_metadata) */
#if defined(VK_EXT_host_image_copy)
    lahar_load(vkCopyImageToImageEXT);
    lahar_load(vkCopyImageToMemoryEXT);
    lahar_load(vkCopyMemoryToImageEXT);
    lahar_load(vkTransitionImageLayoutEXT);
#endif /* defined(VK_EXT_host_image_copy) */
#if defined(VK_EXT_host_query_reset)
    lahar_load(vkResetQueryPoolEXT);
#endif /* defined(VK_EXT_host_query_reset) */
#if defined(VK_EXT_image_drm_format_modifier)
    lahar_load(vkGetImageDrmFormatModifierPropertiesEXT);
#endif /* defined(VK_EXT_image_drm_format_modifier) */
#if defined(VK_EXT_line_rasterization)
    lahar_load(vkCmdSetLineStippleEXT);
#endif /* defined(VK_EXT_line_rasterization) */
#if defined(VK_EXT_mesh_shader)
    lahar_load(vkCmdDrawMeshTasksEXT);
    lahar_load(vkCmdDrawMeshTasksIndirectEXT);
#endif /* defined(VK_EXT_mesh_shader) */
#if defined(VK_EXT_mesh_shader) && (defined(VK_KHR_draw_indirect_count) || defined(VK_VERSION_1_2))
    lahar_load(vkCmdDrawMeshTasksIndirectCountEXT);
#endif /* defined(VK_EXT_mesh_shader) && (defined(VK_KHR_draw_indirect_count) || defined(VK_VERSION_1_2)) */
#if defined(VK_EXT_metal_objects)
    lahar_load(vkExportMetalObjectsEXT);
#endif /* defined(VK_EXT_metal_objects) */
#if defined(VK_EXT_multi_draw)
    lahar_load(vkCmdDrawMultiEXT);
    lahar_load(vkCmdDrawMultiIndexedEXT);
#endif /* defined(VK_EXT_multi_draw) */
#if defined(VK_EXT_opacity_micromap)
    lahar_load(vkBuildMicromapsEXT);
    lahar_load(vkCmdBuildMicromapsEXT);
    lahar_load(vkCmdCopyMemoryToMicromapEXT);
    lahar_load(vkCmdCopyMicromapEXT);
    lahar_load(vkCmdCopyMicromapToMemoryEXT);
    lahar_load(vkCmdWriteMicromapsPropertiesEXT);
    lahar_load(vkCopyMemoryToMicromapEXT);
    lahar_load(vkCopyMicromapEXT);
    lahar_load(vkCopyMicromapToMemoryEXT);
    lahar_load(vkCreateMicromapEXT);
    lahar_load(vkDestroyMicromapEXT);
    lahar_load(vkGetDeviceMicromapCompatibilityEXT);
    lahar_load(vkGetMicromapBuildSizesEXT);
    lahar_load(vkWriteMicromapsPropertiesEXT);
#endif /* defined(VK_EXT_opacity_micromap) */
#if defined(VK_EXT_pageable_device_local_memory)
    lahar_load(vkSetDeviceMemoryPriorityEXT);
#endif /* defined(VK_EXT_pageable_device_local_memory) */
#if defined(VK_EXT_pipeline_properties)
    lahar_load(vkGetPipelinePropertiesEXT);
#endif /* defined(VK_EXT_pipeline_properties) */
#if defined(VK_EXT_private_data)
    lahar_load(vkCreatePrivateDataSlotEXT);
    lahar_load(vkDestroyPrivateDataSlotEXT);
    lahar_load(vkGetPrivateDataEXT);
    lahar_load(vkSetPrivateDataEXT);
#endif /* defined(VK_EXT_private_data) */
#if defined(VK_EXT_sample_locations)
    lahar_load(vkCmdSetSampleLocationsEXT);
#endif /* defined(VK_EXT_sample_locations) */
#if defined(VK_EXT_shader_module_identifier)
    lahar_load(vkGetShaderModuleCreateInfoIdentifierEXT);
    lahar_load(vkGetShaderModuleIdentifierEXT);
#endif /* defined(VK_EXT_shader_module_identifier) */
#if defined(VK_EXT_shader_object)
    lahar_load(vkCmdBindShadersEXT);
    lahar_load(vkCreateShadersEXT);
    lahar_load(vkDestroyShaderEXT);
    lahar_load(vkGetShaderBinaryDataEXT);
#endif /* defined(VK_EXT_shader_object) */
#if defined(VK_EXT_swapchain_maintenance1)
    lahar_load(vkReleaseSwapchainImagesEXT);
#endif /* defined(VK_EXT_swapchain_maintenance1) */
#if defined(VK_EXT_transform_feedback)
    lahar_load(vkCmdBeginQueryIndexedEXT);
    lahar_load(vkCmdBeginTransformFeedbackEXT);
    lahar_load(vkCmdBindTransformFeedbackBuffersEXT);
    lahar_load(vkCmdDrawIndirectByteCountEXT);
    lahar_load(vkCmdEndQueryIndexedEXT);
    lahar_load(vkCmdEndTransformFeedbackEXT);
#endif /* defined(VK_EXT_transform_feedback) */
#if defined(VK_EXT_validation_cache)
    lahar_load(vkCreateValidationCacheEXT);
    lahar_load(vkDestroyValidationCacheEXT);
    lahar_load(vkGetValidationCacheDataEXT);
    lahar_load(vkMergeValidationCachesEXT);
#endif /* defined(VK_EXT_validation_cache) */
#if defined(VK_FUCHSIA_buffer_collection)
    lahar_load(vkCreateBufferCollectionFUCHSIA);
    lahar_load(vkDestroyBufferCollectionFUCHSIA);
    lahar_load(vkGetBufferCollectionPropertiesFUCHSIA);
    lahar_load(vkSetBufferCollectionBufferConstraintsFUCHSIA);
    lahar_load(vkSetBufferCollectionImageConstraintsFUCHSIA);
#endif /* defined(VK_FUCHSIA_buffer_collection) */
#if defined(VK_FUCHSIA_external_memory)
    lahar_load(vkGetMemoryZirconHandleFUCHSIA);
    lahar_load(vkGetMemoryZirconHandlePropertiesFUCHSIA);
#endif /* defined(VK_FUCHSIA_external_memory) */
#if defined(VK_FUCHSIA_external_semaphore)
    lahar_load(vkGetSemaphoreZirconHandleFUCHSIA);
    lahar_load(vkImportSemaphoreZirconHandleFUCHSIA);
#endif /* defined(VK_FUCHSIA_external_semaphore) */
#if defined(VK_GOOGLE_display_timing)
    lahar_load(vkGetPastPresentationTimingGOOGLE);
    lahar_load(vkGetRefreshCycleDurationGOOGLE);
#endif /* defined(VK_GOOGLE_display_timing) */
#if defined(VK_HUAWEI_cluster_culling_shader)
    lahar_load(vkCmdDrawClusterHUAWEI);
    lahar_load(vkCmdDrawClusterIndirectHUAWEI);
#endif /* defined(VK_HUAWEI_cluster_culling_shader) */
#if defined(VK_HUAWEI_invocation_mask)
    lahar_load(vkCmdBindInvocationMaskHUAWEI);
#endif /* defined(VK_HUAWEI_invocation_mask) */
#if defined(VK_HUAWEI_subpass_shading) && VK_HUAWEI_SUBPASS_SHADING_SPEC_VERSION >= 2
    lahar_load(vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI);
#endif /* defined(VK_HUAWEI_subpass_shading) && VK_HUAWEI_SUBPASS_SHADING_SPEC_VERSION >= 2 */
#if defined(VK_HUAWEI_subpass_shading)
    lahar_load(vkCmdSubpassShadingHUAWEI);
#endif /* defined(VK_HUAWEI_subpass_shading) */
#if defined(VK_INTEL_performance_query)
    lahar_load(vkAcquirePerformanceConfigurationINTEL);
    lahar_load(vkCmdSetPerformanceMarkerINTEL);
    lahar_load(vkCmdSetPerformanceOverrideINTEL);
    lahar_load(vkCmdSetPerformanceStreamMarkerINTEL);
    lahar_load(vkGetPerformanceParameterINTEL);
    lahar_load(vkInitializePerformanceApiINTEL);
    lahar_load(vkQueueSetPerformanceConfigurationINTEL);
    lahar_load(vkReleasePerformanceConfigurationINTEL);
    lahar_load(vkUninitializePerformanceApiINTEL);
#endif /* defined(VK_INTEL_performance_query) */
#if defined(VK_KHR_acceleration_structure)
    lahar_load(vkBuildAccelerationStructuresKHR);
    lahar_load(vkCmdBuildAccelerationStructuresIndirectKHR);
    lahar_load(vkCmdBuildAccelerationStructuresKHR);
    lahar_load(vkCmdCopyAccelerationStructureKHR);
    lahar_load(vkCmdCopyAccelerationStructureToMemoryKHR);
    lahar_load(vkCmdCopyMemoryToAccelerationStructureKHR);
    lahar_load(vkCmdWriteAccelerationStructuresPropertiesKHR);
    lahar_load(vkCopyAccelerationStructureKHR);
    lahar_load(vkCopyAccelerationStructureToMemoryKHR);
    lahar_load(vkCopyMemoryToAccelerationStructureKHR);
    lahar_load(vkCreateAccelerationStructureKHR);
    lahar_load(vkDestroyAccelerationStructureKHR);
    lahar_load(vkGetAccelerationStructureBuildSizesKHR);
    lahar_load(vkGetAccelerationStructureDeviceAddressKHR);
    lahar_load(vkGetDeviceAccelerationStructureCompatibilityKHR);
    lahar_load(vkWriteAccelerationStructuresPropertiesKHR);
#endif /* defined(VK_KHR_acceleration_structure) */
#if defined(VK_KHR_bind_memory2)
    lahar_load(vkBindBufferMemory2KHR);
    lahar_load(vkBindImageMemory2KHR);
#endif /* defined(VK_KHR_bind_memory2) */
#if defined(VK_KHR_buffer_device_address)
    lahar_load(vkGetBufferDeviceAddressKHR);
    lahar_load(vkGetBufferOpaqueCaptureAddressKHR);
    lahar_load(vkGetDeviceMemoryOpaqueCaptureAddressKHR);
#endif /* defined(VK_KHR_buffer_device_address) */
#if defined(VK_KHR_calibrated_timestamps)
    lahar_load(vkGetCalibratedTimestampsKHR);
#endif /* defined(VK_KHR_calibrated_timestamps) */
#if defined(VK_KHR_copy_commands2)
    lahar_load(vkCmdBlitImage2KHR);
    lahar_load(vkCmdCopyBuffer2KHR);
    lahar_load(vkCmdCopyBufferToImage2KHR);
    lahar_load(vkCmdCopyImage2KHR);
    lahar_load(vkCmdCopyImageToBuffer2KHR);
    lahar_load(vkCmdResolveImage2KHR);
#endif /* defined(VK_KHR_copy_commands2) */
#if defined(VK_KHR_create_renderpass2)
    lahar_load(vkCmdBeginRenderPass2KHR);
    lahar_load(vkCmdEndRenderPass2KHR);
    lahar_load(vkCmdNextSubpass2KHR);
    lahar_load(vkCreateRenderPass2KHR);
#endif /* defined(VK_KHR_create_renderpass2) */
#if defined(VK_KHR_deferred_host_operations)
    lahar_load(vkCreateDeferredOperationKHR);
    lahar_load(vkDeferredOperationJoinKHR);
    lahar_load(vkDestroyDeferredOperationKHR);
    lahar_load(vkGetDeferredOperationMaxConcurrencyKHR);
    lahar_load(vkGetDeferredOperationResultKHR);
#endif /* defined(VK_KHR_deferred_host_operations) */
#if defined(VK_KHR_descriptor_update_template)
    lahar_load(vkCreateDescriptorUpdateTemplateKHR);
    lahar_load(vkDestroyDescriptorUpdateTemplateKHR);
    lahar_load(vkUpdateDescriptorSetWithTemplateKHR);
#endif /* defined(VK_KHR_descriptor_update_template) */
#if defined(VK_KHR_device_group)
    lahar_load(vkCmdDispatchBaseKHR);
    lahar_load(vkCmdSetDeviceMaskKHR);
    lahar_load(vkGetDeviceGroupPeerMemoryFeaturesKHR);
#endif /* defined(VK_KHR_device_group) */
#if defined(VK_KHR_display_swapchain)
    lahar_load(vkCreateSharedSwapchainsKHR);
#endif /* defined(VK_KHR_display_swapchain) */
#if defined(VK_KHR_draw_indirect_count)
    lahar_load(vkCmdDrawIndexedIndirectCountKHR);
    lahar_load(vkCmdDrawIndirectCountKHR);
#endif /* defined(VK_KHR_draw_indirect_count) */
#if defined(VK_KHR_dynamic_rendering)
    lahar_load(vkCmdBeginRenderingKHR);
    lahar_load(vkCmdEndRenderingKHR);
#endif /* defined(VK_KHR_dynamic_rendering) */
#if defined(VK_KHR_dynamic_rendering_local_read)
    lahar_load(vkCmdSetRenderingAttachmentLocationsKHR);
    lahar_load(vkCmdSetRenderingInputAttachmentIndicesKHR);
#endif /* defined(VK_KHR_dynamic_rendering_local_read) */
#if defined(VK_KHR_external_fence_fd)
    lahar_load(vkGetFenceFdKHR);
    lahar_load(vkImportFenceFdKHR);
#endif /* defined(VK_KHR_external_fence_fd) */
#if defined(VK_KHR_external_fence_win32)
    lahar_load(vkGetFenceWin32HandleKHR);
    lahar_load(vkImportFenceWin32HandleKHR);
#endif /* defined(VK_KHR_external_fence_win32) */
#if defined(VK_KHR_external_memory_fd)
    lahar_load(vkGetMemoryFdKHR);
    lahar_load(vkGetMemoryFdPropertiesKHR);
#endif /* defined(VK_KHR_external_memory_fd) */
#if defined(VK_KHR_external_memory_win32)
    lahar_load(vkGetMemoryWin32HandleKHR);
    lahar_load(vkGetMemoryWin32HandlePropertiesKHR);
#endif /* defined(VK_KHR_external_memory_win32) */
#if defined(VK_KHR_external_semaphore_fd)
    lahar_load(vkGetSemaphoreFdKHR);
    lahar_load(vkImportSemaphoreFdKHR);
#endif /* defined(VK_KHR_external_semaphore_fd) */
#if defined(VK_KHR_external_semaphore_win32)
    lahar_load(vkGetSemaphoreWin32HandleKHR);
    lahar_load(vkImportSemaphoreWin32HandleKHR);
#endif /* defined(VK_KHR_external_semaphore_win32) */
#if defined(VK_KHR_fragment_shading_rate)
    lahar_load(vkCmdSetFragmentShadingRateKHR);
#endif /* defined(VK_KHR_fragment_shading_rate) */
#if defined(VK_KHR_get_memory_requirements2)
    lahar_load(vkGetBufferMemoryRequirements2KHR);
    lahar_load(vkGetImageMemoryRequirements2KHR);
    lahar_load(vkGetImageSparseMemoryRequirements2KHR);
#endif /* defined(VK_KHR_get_memory_requirements2) */
#if defined(VK_KHR_line_rasterization)
    lahar_load(vkCmdSetLineStippleKHR);
#endif /* defined(VK_KHR_line_rasterization) */
#if defined(VK_KHR_maintenance1)
    lahar_load(vkTrimCommandPoolKHR);
#endif /* defined(VK_KHR_maintenance1) */
#if defined(VK_KHR_maintenance3)
    lahar_load(vkGetDescriptorSetLayoutSupportKHR);
#endif /* defined(VK_KHR_maintenance3) */
#if defined(VK_KHR_maintenance4)
    lahar_load(vkGetDeviceBufferMemoryRequirementsKHR);
    lahar_load(vkGetDeviceImageMemoryRequirementsKHR);
    lahar_load(vkGetDeviceImageSparseMemoryRequirementsKHR);
#endif /* defined(VK_KHR_maintenance4) */
#if defined(VK_KHR_maintenance5)
    lahar_load(vkCmdBindIndexBuffer2KHR);
    lahar_load(vkGetDeviceImageSubresourceLayoutKHR);
    lahar_load(vkGetImageSubresourceLayout2KHR);
    lahar_load(vkGetRenderingAreaGranularityKHR);
#endif /* defined(VK_KHR_maintenance5) */
#if defined(VK_KHR_maintenance6)
    lahar_load(vkCmdBindDescriptorSets2KHR);
    lahar_load(vkCmdPushConstants2KHR);
#endif /* defined(VK_KHR_maintenance6) */
#if defined(VK_KHR_maintenance6) && defined(VK_KHR_push_descriptor)
    lahar_load(vkCmdPushDescriptorSet2KHR);
    lahar_load(vkCmdPushDescriptorSetWithTemplate2KHR);
#endif /* defined(VK_KHR_maintenance6) && defined(VK_KHR_push_descriptor) */
#if defined(VK_KHR_maintenance6) && defined(VK_EXT_descriptor_buffer)
    lahar_load(vkCmdBindDescriptorBufferEmbeddedSamplers2EXT);
    lahar_load(vkCmdSetDescriptorBufferOffsets2EXT);
#endif /* defined(VK_KHR_maintenance6) && defined(VK_EXT_descriptor_buffer) */
#if defined(VK_KHR_map_memory2)
    lahar_load(vkMapMemory2KHR);
    lahar_load(vkUnmapMemory2KHR);
#endif /* defined(VK_KHR_map_memory2) */
#if defined(VK_KHR_performance_query)
    lahar_load(vkAcquireProfilingLockKHR);
    lahar_load(vkReleaseProfilingLockKHR);
#endif /* defined(VK_KHR_performance_query) */
#if defined(VK_KHR_pipeline_binary)
    lahar_load(vkCreatePipelineBinariesKHR);
    lahar_load(vkDestroyPipelineBinaryKHR);
    lahar_load(vkGetPipelineBinaryDataKHR);
    lahar_load(vkGetPipelineKeyKHR);
    lahar_load(vkReleaseCapturedPipelineDataKHR);
#endif /* defined(VK_KHR_pipeline_binary) */
#if defined(VK_KHR_pipeline_executable_properties)
    lahar_load(vkGetPipelineExecutableInternalRepresentationsKHR);
    lahar_load(vkGetPipelineExecutablePropertiesKHR);
    lahar_load(vkGetPipelineExecutableStatisticsKHR);
#endif /* defined(VK_KHR_pipeline_executable_properties) */
#if defined(VK_KHR_present_wait)
    lahar_load(vkWaitForPresentKHR);
#endif /* defined(VK_KHR_present_wait) */
#if defined(VK_KHR_present_wait2)
    lahar_load(vkWaitForPresent2KHR);
#endif /* defined(VK_KHR_present_wait2) */
#if defined(VK_KHR_push_descriptor)
    lahar_load(vkCmdPushDescriptorSetKHR);
#endif /* defined(VK_KHR_push_descriptor) */
#if defined(VK_KHR_ray_tracing_maintenance1) && defined(VK_KHR_ray_tracing_pipeline)
    lahar_load(vkCmdTraceRaysIndirect2KHR);
#endif /* defined(VK_KHR_ray_tracing_maintenance1) && defined(VK_KHR_ray_tracing_pipeline) */
#if defined(VK_KHR_ray_tracing_pipeline)
    lahar_load(vkCmdSetRayTracingPipelineStackSizeKHR);
    lahar_load(vkCmdTraceRaysIndirectKHR);
    lahar_load(vkCmdTraceRaysKHR);
    lahar_load(vkCreateRayTracingPipelinesKHR);
    lahar_load(vkGetRayTracingCaptureReplayShaderGroupHandlesKHR);
    lahar_load(vkGetRayTracingShaderGroupHandlesKHR);
    lahar_load(vkGetRayTracingShaderGroupStackSizeKHR);
#endif /* defined(VK_KHR_ray_tracing_pipeline) */
#if defined(VK_KHR_sampler_ycbcr_conversion)
    lahar_load(vkCreateSamplerYcbcrConversionKHR);
    lahar_load(vkDestroySamplerYcbcrConversionKHR);
#endif /* defined(VK_KHR_sampler_ycbcr_conversion) */
#if defined(VK_KHR_shared_presentable_image)
    lahar_load(vkGetSwapchainStatusKHR);
#endif /* defined(VK_KHR_shared_presentable_image) */
#if defined(VK_KHR_swapchain)
    lahar_load(vkAcquireNextImageKHR);
    lahar_load(vkCreateSwapchainKHR);
    lahar_load(vkDestroySwapchainKHR);
    lahar_load(vkGetSwapchainImagesKHR);
    lahar_load(vkQueuePresentKHR);
#endif /* defined(VK_KHR_swapchain) */
#if defined(VK_KHR_swapchain_maintenance1)
    lahar_load(vkReleaseSwapchainImagesKHR);
#endif /* defined(VK_KHR_swapchain_maintenance1) */
#if defined(VK_KHR_synchronization2)
    lahar_load(vkCmdPipelineBarrier2KHR);
    lahar_load(vkCmdResetEvent2KHR);
    lahar_load(vkCmdSetEvent2KHR);
    lahar_load(vkCmdWaitEvents2KHR);
    lahar_load(vkCmdWriteTimestamp2KHR);
    lahar_load(vkQueueSubmit2KHR);
#endif /* defined(VK_KHR_synchronization2) */
#if defined(VK_KHR_timeline_semaphore)
    lahar_load(vkGetSemaphoreCounterValueKHR);
    lahar_load(vkSignalSemaphoreKHR);
    lahar_load(vkWaitSemaphoresKHR);
#endif /* defined(VK_KHR_timeline_semaphore) */
#if defined(VK_KHR_video_decode_queue)
    lahar_load(vkCmdDecodeVideoKHR);
#endif /* defined(VK_KHR_video_decode_queue) */
#if defined(VK_KHR_video_encode_queue)
    lahar_load(vkCmdEncodeVideoKHR);
    lahar_load(vkGetEncodedVideoSessionParametersKHR);
#endif /* defined(VK_KHR_video_encode_queue) */
#if defined(VK_KHR_video_queue)
    lahar_load(vkBindVideoSessionMemoryKHR);
    lahar_load(vkCmdBeginVideoCodingKHR);
    lahar_load(vkCmdControlVideoCodingKHR);
    lahar_load(vkCmdEndVideoCodingKHR);
    lahar_load(vkCreateVideoSessionKHR);
    lahar_load(vkCreateVideoSessionParametersKHR);
    lahar_load(vkDestroyVideoSessionKHR);
    lahar_load(vkDestroyVideoSessionParametersKHR);
    lahar_load(vkGetVideoSessionMemoryRequirementsKHR);
    lahar_load(vkUpdateVideoSessionParametersKHR);
#endif /* defined(VK_KHR_video_queue) */
#if defined(VK_NVX_binary_import)
    lahar_load(vkCmdCuLaunchKernelNVX);
    lahar_load(vkCreateCuFunctionNVX);
    lahar_load(vkCreateCuModuleNVX);
    lahar_load(vkDestroyCuFunctionNVX);
    lahar_load(vkDestroyCuModuleNVX);
#endif /* defined(VK_NVX_binary_import) */
#if defined(VK_NVX_image_view_handle)
    lahar_load(vkGetImageViewHandleNVX);
#endif /* defined(VK_NVX_image_view_handle) */
#if defined(VK_NVX_image_view_handle) && VK_NVX_IMAGE_VIEW_HANDLE_SPEC_VERSION >= 3
    lahar_load(vkGetImageViewHandle64NVX);
#endif /* defined(VK_NVX_image_view_handle) && VK_NVX_IMAGE_VIEW_HANDLE_SPEC_VERSION >= 3 */
#if defined(VK_NVX_image_view_handle) && VK_NVX_IMAGE_VIEW_HANDLE_SPEC_VERSION >= 2
    lahar_load(vkGetImageViewAddressNVX);
#endif /* defined(VK_NVX_image_view_handle) && VK_NVX_IMAGE_VIEW_HANDLE_SPEC_VERSION >= 2 */
#if defined(VK_NV_clip_space_w_scaling)
    lahar_load(vkCmdSetViewportWScalingNV);
#endif /* defined(VK_NV_clip_space_w_scaling) */
#if defined(VK_NV_cluster_acceleration_structure)
    lahar_load(vkCmdBuildClusterAccelerationStructureIndirectNV);
    lahar_load(vkGetClusterAccelerationStructureBuildSizesNV);
#endif /* defined(VK_NV_cluster_acceleration_structure) */
#if defined(VK_NV_cooperative_vector)
    lahar_load(vkCmdConvertCooperativeVectorMatrixNV);
    lahar_load(vkConvertCooperativeVectorMatrixNV);
#endif /* defined(VK_NV_cooperative_vector) */
#if defined(VK_NV_copy_memory_indirect)
    lahar_load(vkCmdCopyMemoryIndirectNV);
    lahar_load(vkCmdCopyMemoryToImageIndirectNV);
#endif /* defined(VK_NV_copy_memory_indirect) */
#if defined(VK_NV_cuda_kernel_launch)
    lahar_load(vkCmdCudaLaunchKernelNV);
    lahar_load(vkCreateCudaFunctionNV);
    lahar_load(vkCreateCudaModuleNV);
    lahar_load(vkDestroyCudaFunctionNV);
    lahar_load(vkDestroyCudaModuleNV);
    lahar_load(vkGetCudaModuleCacheNV);
#endif /* defined(VK_NV_cuda_kernel_launch) */
#if defined(VK_NV_device_diagnostic_checkpoints)
    lahar_load(vkCmdSetCheckpointNV);
    lahar_load(vkGetQueueCheckpointDataNV);
#endif /* defined(VK_NV_device_diagnostic_checkpoints) */
#if defined(VK_NV_device_diagnostic_checkpoints) && (defined(VK_VERSION_1_3) || defined(VK_KHR_synchronization2))
    lahar_load(vkGetQueueCheckpointData2NV);
#endif /* defined(VK_NV_device_diagnostic_checkpoints) && (defined(VK_VERSION_1_3) || defined(VK_KHR_synchronization2)) */
#if defined(VK_NV_device_generated_commands)
    lahar_load(vkCmdBindPipelineShaderGroupNV);
    lahar_load(vkCmdExecuteGeneratedCommandsNV);
    lahar_load(vkCmdPreprocessGeneratedCommandsNV);
    lahar_load(vkCreateIndirectCommandsLayoutNV);
    lahar_load(vkDestroyIndirectCommandsLayoutNV);
    lahar_load(vkGetGeneratedCommandsMemoryRequirementsNV);
#endif /* defined(VK_NV_device_generated_commands) */
#if defined(VK_NV_device_generated_commands_compute)
    lahar_load(vkCmdUpdatePipelineIndirectBufferNV);
    lahar_load(vkGetPipelineIndirectDeviceAddressNV);
    lahar_load(vkGetPipelineIndirectMemoryRequirementsNV);
#endif /* defined(VK_NV_device_generated_commands_compute) */
#if defined(VK_NV_external_compute_queue)
    lahar_load(vkCreateExternalComputeQueueNV);
    lahar_load(vkDestroyExternalComputeQueueNV);
    lahar_load(vkGetExternalComputeQueueDataNV);
#endif /* defined(VK_NV_external_compute_queue) */
#if defined(VK_NV_external_memory_rdma)
    lahar_load(vkGetMemoryRemoteAddressNV);
#endif /* defined(VK_NV_external_memory_rdma) */
#if defined(VK_NV_external_memory_win32)
    lahar_load(vkGetMemoryWin32HandleNV);
#endif /* defined(VK_NV_external_memory_win32) */
#if defined(VK_NV_fragment_shading_rate_enums)
    lahar_load(vkCmdSetFragmentShadingRateEnumNV);
#endif /* defined(VK_NV_fragment_shading_rate_enums) */
#if defined(VK_NV_low_latency2)
    lahar_load(vkGetLatencyTimingsNV);
    lahar_load(vkLatencySleepNV);
    lahar_load(vkQueueNotifyOutOfBandNV);
    lahar_load(vkSetLatencyMarkerNV);
    lahar_load(vkSetLatencySleepModeNV);
#endif /* defined(VK_NV_low_latency2) */
#if defined(VK_NV_memory_decompression)
    lahar_load(vkCmdDecompressMemoryIndirectCountNV);
    lahar_load(vkCmdDecompressMemoryNV);
#endif /* defined(VK_NV_memory_decompression) */
#if defined(VK_NV_mesh_shader)
    lahar_load(vkCmdDrawMeshTasksIndirectNV);
    lahar_load(vkCmdDrawMeshTasksNV);
#endif /* defined(VK_NV_mesh_shader) */
#if defined(VK_NV_mesh_shader) && (defined(VK_KHR_draw_indirect_count) || defined(VK_VERSION_1_2))
    lahar_load(vkCmdDrawMeshTasksIndirectCountNV);
#endif /* defined(VK_NV_mesh_shader) && (defined(VK_KHR_draw_indirect_count) || defined(VK_VERSION_1_2)) */
#if defined(VK_NV_optical_flow)
    lahar_load(vkBindOpticalFlowSessionImageNV);
    lahar_load(vkCmdOpticalFlowExecuteNV);
    lahar_load(vkCreateOpticalFlowSessionNV);
    lahar_load(vkDestroyOpticalFlowSessionNV);
#endif /* defined(VK_NV_optical_flow) */
#if defined(VK_NV_partitioned_acceleration_structure)
    lahar_load(vkCmdBuildPartitionedAccelerationStructuresNV);
    lahar_load(vkGetPartitionedAccelerationStructuresBuildSizesNV);
#endif /* defined(VK_NV_partitioned_acceleration_structure) */
#if defined(VK_NV_ray_tracing)
    lahar_load(vkBindAccelerationStructureMemoryNV);
    lahar_load(vkCmdBuildAccelerationStructureNV);
    lahar_load(vkCmdCopyAccelerationStructureNV);
    lahar_load(vkCmdTraceRaysNV);
    lahar_load(vkCmdWriteAccelerationStructuresPropertiesNV);
    lahar_load(vkCompileDeferredNV);
    lahar_load(vkCreateAccelerationStructureNV);
    lahar_load(vkCreateRayTracingPipelinesNV);
    lahar_load(vkDestroyAccelerationStructureNV);
    lahar_load(vkGetAccelerationStructureHandleNV);
    lahar_load(vkGetAccelerationStructureMemoryRequirementsNV);
    lahar_load(vkGetRayTracingShaderGroupHandlesNV);
#endif /* defined(VK_NV_ray_tracing) */
#if defined(VK_NV_scissor_exclusive) && VK_NV_SCISSOR_EXCLUSIVE_SPEC_VERSION >= 2
    lahar_load(vkCmdSetExclusiveScissorEnableNV);
#endif /* defined(VK_NV_scissor_exclusive) && VK_NV_SCISSOR_EXCLUSIVE_SPEC_VERSION >= 2 */
#if defined(VK_NV_scissor_exclusive)
    lahar_load(vkCmdSetExclusiveScissorNV);
#endif /* defined(VK_NV_scissor_exclusive) */
#if defined(VK_NV_shading_rate_image)
    lahar_load(vkCmdBindShadingRateImageNV);
    lahar_load(vkCmdSetCoarseSampleOrderNV);
    lahar_load(vkCmdSetViewportShadingRatePaletteNV);
#endif /* defined(VK_NV_shading_rate_image) */
#if defined(VK_QCOM_tile_memory_heap)
    lahar_load(vkCmdBindTileMemoryQCOM);
#endif /* defined(VK_QCOM_tile_memory_heap) */
#if defined(VK_QCOM_tile_properties)
    lahar_load(vkGetDynamicRenderingTilePropertiesQCOM);
    lahar_load(vkGetFramebufferTilePropertiesQCOM);
#endif /* defined(VK_QCOM_tile_properties) */
#if defined(VK_QCOM_tile_shading)
    lahar_load(vkCmdBeginPerTileExecutionQCOM);
    lahar_load(vkCmdDispatchTileQCOM);
    lahar_load(vkCmdEndPerTileExecutionQCOM);
#endif /* defined(VK_QCOM_tile_shading) */
#if defined(VK_QNX_external_memory_screen_buffer)
    lahar_load(vkGetScreenBufferPropertiesQNX);
#endif /* defined(VK_QNX_external_memory_screen_buffer) */
#if defined(VK_VALVE_descriptor_set_host_mapping)
    lahar_load(vkGetDescriptorSetHostMappingVALVE);
    lahar_load(vkGetDescriptorSetLayoutHostMappingInfoVALVE);
#endif /* defined(VK_VALVE_descriptor_set_host_mapping) */
#if (defined(VK_EXT_depth_clamp_control)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_depth_clamp_control))
    lahar_load(vkCmdSetDepthClampRangeEXT);
#endif /* (defined(VK_EXT_depth_clamp_control)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_depth_clamp_control)) */
#if (defined(VK_EXT_extended_dynamic_state)) || (defined(VK_EXT_shader_object))
    lahar_load(vkCmdBindVertexBuffers2EXT);
    lahar_load(vkCmdSetCullModeEXT);
    lahar_load(vkCmdSetDepthBoundsTestEnableEXT);
    lahar_load(vkCmdSetDepthCompareOpEXT);
    lahar_load(vkCmdSetDepthTestEnableEXT);
    lahar_load(vkCmdSetDepthWriteEnableEXT);
    lahar_load(vkCmdSetFrontFaceEXT);
    lahar_load(vkCmdSetPrimitiveTopologyEXT);
    lahar_load(vkCmdSetScissorWithCountEXT);
    lahar_load(vkCmdSetStencilOpEXT);
    lahar_load(vkCmdSetStencilTestEnableEXT);
    lahar_load(vkCmdSetViewportWithCountEXT);
#endif /* (defined(VK_EXT_extended_dynamic_state)) || (defined(VK_EXT_shader_object)) */
#if (defined(VK_EXT_extended_dynamic_state2)) || (defined(VK_EXT_shader_object))
    lahar_load(vkCmdSetDepthBiasEnableEXT);
    lahar_load(vkCmdSetLogicOpEXT);
    lahar_load(vkCmdSetPatchControlPointsEXT);
    lahar_load(vkCmdSetPrimitiveRestartEnableEXT);
    lahar_load(vkCmdSetRasterizerDiscardEnableEXT);
#endif /* (defined(VK_EXT_extended_dynamic_state2)) || (defined(VK_EXT_shader_object)) */
#if (defined(VK_EXT_extended_dynamic_state3)) || (defined(VK_EXT_shader_object))
    lahar_load(vkCmdSetAlphaToCoverageEnableEXT);
    lahar_load(vkCmdSetAlphaToOneEnableEXT);
    lahar_load(vkCmdSetColorBlendEnableEXT);
    lahar_load(vkCmdSetColorBlendEquationEXT);
    lahar_load(vkCmdSetColorWriteMaskEXT);
    lahar_load(vkCmdSetDepthClampEnableEXT);
    lahar_load(vkCmdSetLogicOpEnableEXT);
    lahar_load(vkCmdSetPolygonModeEXT);
    lahar_load(vkCmdSetRasterizationSamplesEXT);
    lahar_load(vkCmdSetSampleMaskEXT);
#endif /* (defined(VK_EXT_extended_dynamic_state3)) || (defined(VK_EXT_shader_object)) */
#if (defined(VK_EXT_extended_dynamic_state3) && (defined(VK_KHR_maintenance2) || defined(VK_VERSION_1_1))) || (defined(VK_EXT_shader_object))
    lahar_load(vkCmdSetTessellationDomainOriginEXT);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && (defined(VK_KHR_maintenance2) || defined(VK_VERSION_1_1))) || (defined(VK_EXT_shader_object)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_transform_feedback)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_transform_feedback))
    lahar_load(vkCmdSetRasterizationStreamEXT);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_transform_feedback)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_transform_feedback)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_conservative_rasterization)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_conservative_rasterization))
    lahar_load(vkCmdSetConservativeRasterizationModeEXT);
    lahar_load(vkCmdSetExtraPrimitiveOverestimationSizeEXT);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_conservative_rasterization)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_conservative_rasterization)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_depth_clip_enable)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_depth_clip_enable))
    lahar_load(vkCmdSetDepthClipEnableEXT);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_depth_clip_enable)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_depth_clip_enable)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_sample_locations)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_sample_locations))
    lahar_load(vkCmdSetSampleLocationsEnableEXT);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_sample_locations)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_sample_locations)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_blend_operation_advanced)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_blend_operation_advanced))
    lahar_load(vkCmdSetColorBlendAdvancedEXT);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_blend_operation_advanced)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_blend_operation_advanced)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_provoking_vertex)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_provoking_vertex))
    lahar_load(vkCmdSetProvokingVertexModeEXT);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_provoking_vertex)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_provoking_vertex)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_line_rasterization)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_line_rasterization))
    lahar_load(vkCmdSetLineRasterizationModeEXT);
    lahar_load(vkCmdSetLineStippleEnableEXT);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_line_rasterization)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_line_rasterization)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_depth_clip_control)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_depth_clip_control))
    lahar_load(vkCmdSetDepthClipNegativeOneToOneEXT);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_depth_clip_control)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_depth_clip_control)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_clip_space_w_scaling)) || (defined(VK_EXT_shader_object) && defined(VK_NV_clip_space_w_scaling))
    lahar_load(vkCmdSetViewportWScalingEnableNV);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_clip_space_w_scaling)) || (defined(VK_EXT_shader_object) && defined(VK_NV_clip_space_w_scaling)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_viewport_swizzle)) || (defined(VK_EXT_shader_object) && defined(VK_NV_viewport_swizzle))
    lahar_load(vkCmdSetViewportSwizzleNV);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_viewport_swizzle)) || (defined(VK_EXT_shader_object) && defined(VK_NV_viewport_swizzle)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_fragment_coverage_to_color)) || (defined(VK_EXT_shader_object) && defined(VK_NV_fragment_coverage_to_color))
    lahar_load(vkCmdSetCoverageToColorEnableNV);
    lahar_load(vkCmdSetCoverageToColorLocationNV);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_fragment_coverage_to_color)) || (defined(VK_EXT_shader_object) && defined(VK_NV_fragment_coverage_to_color)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_framebuffer_mixed_samples)) || (defined(VK_EXT_shader_object) && defined(VK_NV_framebuffer_mixed_samples))
    lahar_load(vkCmdSetCoverageModulationModeNV);
    lahar_load(vkCmdSetCoverageModulationTableEnableNV);
    lahar_load(vkCmdSetCoverageModulationTableNV);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_framebuffer_mixed_samples)) || (defined(VK_EXT_shader_object) && defined(VK_NV_framebuffer_mixed_samples)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_shading_rate_image)) || (defined(VK_EXT_shader_object) && defined(VK_NV_shading_rate_image))
    lahar_load(vkCmdSetShadingRateImageEnableNV);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_shading_rate_image)) || (defined(VK_EXT_shader_object) && defined(VK_NV_shading_rate_image)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_representative_fragment_test)) || (defined(VK_EXT_shader_object) && defined(VK_NV_representative_fragment_test))
    lahar_load(vkCmdSetRepresentativeFragmentTestEnableNV);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_representative_fragment_test)) || (defined(VK_EXT_shader_object) && defined(VK_NV_representative_fragment_test)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_coverage_reduction_mode)) || (defined(VK_EXT_shader_object) && defined(VK_NV_coverage_reduction_mode))
    lahar_load(vkCmdSetCoverageReductionModeNV);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_coverage_reduction_mode)) || (defined(VK_EXT_shader_object) && defined(VK_NV_coverage_reduction_mode)) */
#if (defined(VK_EXT_host_image_copy)) || (defined(VK_EXT_image_compression_control))
    lahar_load(vkGetImageSubresourceLayout2EXT);
#endif /* (defined(VK_EXT_host_image_copy)) || (defined(VK_EXT_image_compression_control)) */
#if (defined(VK_EXT_shader_object)) || (defined(VK_EXT_vertex_input_dynamic_state))
    lahar_load(vkCmdSetVertexInputEXT);
#endif /* (defined(VK_EXT_shader_object)) || (defined(VK_EXT_vertex_input_dynamic_state)) */
#if (defined(VK_KHR_descriptor_update_template) && defined(VK_KHR_push_descriptor)) || (defined(VK_KHR_push_descriptor) && (defined(VK_VERSION_1_1) || defined(VK_KHR_descriptor_update_template)))
    lahar_load(vkCmdPushDescriptorSetWithTemplateKHR);
#endif /* (defined(VK_KHR_descriptor_update_template) && defined(VK_KHR_push_descriptor)) || (defined(VK_KHR_push_descriptor) && (defined(VK_VERSION_1_1) || defined(VK_KHR_descriptor_update_template))) */
#if (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1))
    lahar_load(vkGetDeviceGroupPresentCapabilitiesKHR);
    lahar_load(vkGetDeviceGroupSurfacePresentModesKHR);
#endif /* (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1)) */
#if (defined(VK_KHR_device_group) && defined(VK_KHR_swapchain)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1))
    lahar_load(vkAcquireNextImage2KHR);
#endif /* (defined(VK_KHR_device_group) && defined(VK_KHR_swapchain)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1)) */
/* LAHAR_VK_LOAD_DEVICE */

    return LAHAR_ERR_SUCCESS;
}



#endif // LAHAR_IMPLEMENTATION && !LAHAR_IMPLEMENTATION_INCLUDED





/*
  Copyright (C) 2025 DalenPlanestrider

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
