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

3. Create a Lahar instance. stack, heap, static, doesn't matter as long as it doesn't move

4. Call lahar_init() on the instance

5. Use the suite of lahar_builder_* configuration functions to configure what you want (or poke
   the struct internals, they're fairly well commented)

6. Use lahar_build()

7. Your normal vulkan flow. Set up pipelines, enter render loop, etc

8. (optional) lahar has some utilities to simplify command submission, window presentation,
   and layout transitions, if you want.    



Compile-Time Configuration options:

    LAHAR_NO_AUTO_DEPS
        If defined, third party dependencies like GLFW will _not_ be initialized or
        cleaned up by lahar

    LAHAR_NO_AUTO_INCLUDE
        If defined, third party includes like SDL will not be included automatically.

    LAHAR_M_ARENA_SIZE [positive integer]
        For some operations, lahar uses a fixed size arena. If for some reason your
        environment is exhausting that arena, it will trip an assertion. You can
        fix this by enlarging the arena.

    LAHAR_MAX_DEVICE_ENTRIES [positive integer]
        This determines how many surface formats/present modes a device can
        have associated with it.

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

    LAHAR_IMPLEMENTATION
        Put the lahar implementation in this source file

    lahar_malloc
    lahar_realloc
    lahar_free
        A set of macros with the stdlib-like apis, for if you'd like to
        redirect lahar's memory usage
*/











#ifndef __cplusplus
    #include <stdbool.h>
    #include <stdint.h>
    #include <string.h>
    #include <stdlib.h>
    #include <assert.h>
    #include <stdio.h>
    #include <stddef.h>
#else
    #include <cstdint>
    #include <cstring>
    #include <cstdlib>
    #include <cassert>
    #include <cstdio>
    #include <cstddef>
#endif

#ifdef VULKAN_H_
    #error "Lahar manages vulkan for you! There is no need to include vulkan.h"
#endif

#ifdef _glfw3_h_
    #error "Due to how GLFW handles vulkan, you can't include it before lahar. Instead, #define LAHAR_USE_GLFW"
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
    #ifdef LAHAR_NO_AUTO_INCLUDE
        #error "GLFW does not support LAHAR_NO_AUTO_INCLUDE due to how its vulkan support works"
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
    #endif

    #define LaharWindow SDL_Window
#else
    #define LaharWindow void

    #if !defined(__cplusplus) && __STDC_VERSION__ >= 202311L
        #warning "Lahar is not using a windowing interface, no windows will be available"
    #endif
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
    struct LaharAllocation;
    typedef struct LaharAllocation LaharAllocation;

    struct LaharAllocation {
        VkDeviceMemory device_memory;
        VkDeviceSize alloc_size;
        VkDeviceSize alloc_offset;
    };
#endif

#if defined(_WIN32)
    #define LaharLibray HMODULE
#else
    #define LaharLibrary void*
#endif

#ifndef LAHAR_MAX_DEVICE_ENTRIES
    #define LAHAR_MAX_DEVICE_ENTRIES 16
#endif


#define LAHAR_ERR_SUCCESS 0                             // All good in the neighborhood
#define LAHAR_ERR_ILLEGAL_PARAMS 0x01000001             // Wrong stuff for this function
#define LAHAR_ERR_LOAD_FAILURE 0x01000002               // We couldn't load vulkan
#define LAHAR_ERR_INVALID_CONFIGURATION 0x01000003      // You built a configuration that doesn't make sense
#define LAHAR_ERR_MISSING_EXTENSION 0x01000004          // Missing an extension we needed
#define LAHAR_ERR_NO_SUITABLE_DEVICE 0x01000005         // No device that fits our criteria
#define LAHAR_ERR_DEPENDENCY_FAILED 0x01000006          // A third party lib failed
#define LAHAR_ERR_ALLOC_FAILED 0x01000007               // We couldn't allocate
#define LAHAR_ERR_INVALID_STATE 0x01000008              // The internal state was invalid, this is a bug!
#define LAHAR_ERR_VK_ERR 0x01000009                     // A vulkan operation failed, see lahar.vkresult
#define LAHAR_ERR_INVALID_WINDOW 0x0100000A             // This isn't a window known to lahar
#define LAHAR_ERR_NO_COMMAND_BUFFER 0x0100000B          // You tried to present a frame without submitting any command buffers
#define LAHAR_ERR_TIMEOUT 0x0100000C                    // A wait operation timed out          
#define LAHAR_ERR_SWAPCHAIN_OUT_OF_DATE 0x0100000D      // The swapchain needs updated
#define LAHAR_ERR_INVALID_FRAME_STATE 0x0100000E        // You did things out of order (must always be frame_start -> submit -> present)
#define LAHAR_ERR_ATTACHMENT_WO_ALLOCATOR 0x0100000F    // You requested non-color attachments for a window, but provided no allocator  

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

#if !defined(__cplusplus)
enum LaharWindowProfile;
typedef enum LaharWindowProfile LaharWindowProfile;

enum LaharFramePhase;
typedef enum LaharFramePhase LaharFramePhase;
#endif

typedef PFN_vkVoidFunction (*LaharLoaderFunc)(Lahar*, const char*);
typedef int64_t (*LaharDeviceScoreFunc)(const LaharDeviceInfo*);
typedef uint32_t (*LaharSurfaceFormatChooseFunc)(Lahar*, LaharWindowState*, LaharDeviceInfo*, VkSurfaceFormatKHR* surface_fmt_out);
typedef uint32_t (*LaharSurfacePresentModeChooseFunc)(Lahar*, LaharWindowState*, LaharDeviceInfo*, VkPresentModeKHR* present_mode_out);
typedef uint32_t (*LaharSurfaceResizeFunc)(Lahar*, LaharWindow* window);

typedef uint32_t (*LaharAllocImageFunc)(void* self, Lahar* lahar, const VkImageCreateInfo* info, VkImage* img_out, LaharAllocation* alloc_out);
typedef uint32_t (*LaharFreeImageFunc)(void* self, Lahar* lahar, VkImage* img, LaharAllocation* alloc);

struct LaharAllocator {
    LaharAllocImageFunc alloc_image;
    LaharFreeImageFunc free_image;
};

struct LaharDeviceInfo {
    VkPhysicalDevice physdev;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceMemoryProperties memprops;

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

struct LaharAttachmentConfig {
    VkImageUsageFlags usage;                // The image's usage. This affects 1. The color attachment's usage in the swapchain, and 2. the subresourcerange while transitioning via a lookup table
    VkAttachmentDescription description;    // The attachment description.
    VkImageCreateInfo img_info;             // The image info. This is passed verbatim to create image (except the extent width/height is set automatically)
    VkImageViewCreateInfo view_info;        // The image view info. This is passed verbatim to create image view (except the image is set automatically)
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
    LAHAR_WINPROF_COLOR,            // Create the window with only a color attachment
    LAHAR_WINPROF_COLOR_DEPTH,      // Create the window with a color, and a stencil+depth attachment
};

#define LAHAR_ATT_COLOR_INDEX 0             // The index of the color attachment ALWAYS
#define LAHAR_ATT_STENCIL_DEPTH_INDEX 1     // The index of the stencil/depth attachment if created with a window profile

struct LaharAttachment {
    LaharAllocation img_allocation;         // The allocation for the image
    VkImage image;                          // The attachment's image
    VkImageView view;                       // The attachment's image view
    VkImageLayout layout;                   // The _current_ layout. The transition utilty checks this! If you're transitioning manually, but still want to use the utility, you must update this
};

struct LaharWindowState {
    LaharWindow* window;                    // The window
    uint32_t width, height;                 // The width and height
    uint32_t desired_img_count;             // The desired number of images in the swapchain
    uint32_t max_in_flight;                 // The max number of images in flight
    LaharFramePhase frame_phase;            // Used to track where we are in the phase, making it so you can submit multiple times before swap
    VkCompositeAlphaFlagBitsKHR alpha;      // The compositing alpha flags
    bool auto_recreate_swap;                // Automatically recreate the swapchain
    LaharSurfaceResizeFunc resize_callback; // An optional callback to handle surface resizes

    VkSurfaceFormatKHR surface_format;      // The selected surface format
    VkSurfaceKHR surface;                   // The surface
    VkSwapchainKHR swapchain;               // The swapchain
    uint32_t swap_size;                     // The actual size of the swapchain
    VkSemaphore* image_available;           // The sync semaphors for the images being available
    VkSemaphore* render_finished;           // The sync semaphors for rendering being complete
    VkFence* in_flight;                     // The fences for if this frame is in flight

    uint32_t flight_index;                  // The logical index of the frame in flight. Use this to index sync primitives, or anything "per frame in flight"
    uint32_t frame_index;                   // The index of the current swapchain image, set by window_frame_begin

    size_t attachment_count;                // The number of attachment types this window has
    LaharAttachmentConfig* attachment_configs;  // The configurations for the attachments, in order of [ATTACHMENT_TYPE]
    LaharAttachment** attachments;          // Attachments are in a 2D array, of [ATTACHMENT_TYPE][FRAME_INDEX]

    VkCommandBuffer* commands;              // Will be null unless specifically requested
};

struct Lahar {
    LaharLibrary libvulkan;                                 // This is the platform's library handle
    VkResult vkresult;                                      // If any vulkan operation fails, the error code is saved here
    uint32_t vkversion;                                     // Pre-init, this is the requested version. Post-init, it's the selected version
    uint32_t appversion;                                    // An optional setting for the app's version
    const char* appname;                                    // An optional setting for the app's name
    bool wantvalidation;                                    // True if validation layers were requested
    bool wantcommands;                                      // True if the window command buffers were requested
    VkAllocationCallbacks* vkalloc;                         // One can set the vulkan CPU allocator, if one desires
    PFN_vkDebugUtilsMessengerCallbackEXT debug_callback;    // One can set the debug messenger callback, if one desires
    void* user_data;                                        // A user supplied pointer
    LaharAllocator* gpu_allocator;                          // A user supplied (or VMA backed, if enabled) Vulkan allocator

    char* device_name;                                      // An optional lock to the specific device name
    LaharDeviceScoreFunc score_func;                        // An optional custom scoring function to invoke on physical devices
    LaharSurfaceFormatChooseFunc format_chooser;            // An optional custom callback to choose the surface format
    LaharSurfacePresentModeChooseFunc present_chooser;      // An optional custom callback to choose the surface present mode

    /* Useful Vulkan variables */

    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    LaharDeviceInfo physdev_info;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkCommandPool pool;                                     // Will be null unless specifically requested

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
    bool vma_created;
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
uint32_t lahar_init(Lahar* lahar);

/** Stick a user data pointer on the lahar instance */
void lahar_set_user_data(Lahar* lahar, void* user_data);

/** Get the user data on the lahar instance */
void* lahar_get_user_data(Lahar* lahar);

/** Cleanup the entirety of lahar */
void lahar_deinit(Lahar* lahar);

/** Configuration is done, setup and prepare for rendering */
uint32_t lahar_build(Lahar* lahar);

/** Set a vulkan allocator for lahar to use. This is only
 * required if you want additional attachments beyond color,
 * and you haven't enabled the VMA support.
 */
uint32_t lahar_builder_allocator_set(Lahar* lahar, LaharAllocator* allocator);

#if defined(LAHAR_USE_VMA)
/** If using VMA, instead of supplying a full LaharAllocator, you can
 * simply supply the VMA allocator. If you don't, lahar will create
 * one and store it in the Lahar instance.
 */
uint32_t lahar_vma_set_allocator(Lahar* lahar, VmaAllocator allocator);
#endif


/** Set the version of vulkan you'd like to load */
void lahar_builder_set_vulkan_version(Lahar* lahar, uint32_t version);

/** Inform lahar to load the validation layers, if available */ 
void lahar_builder_request_validation_layers(Lahar* lahar);

/** Add an extension to the list of required instance extensions
 * @param lahar The lahar instance
 * @param extensions The extension to add
 */
uint32_t lahar_builder_extension_add_required_instance(Lahar* lahar, const char* extension);

/** Add an extension to the list of required device extensions
 * @param lahar The lahar instance
 * @param extensions The extension to add
 */
uint32_t lahar_builder_extension_add_required_device(Lahar* lahar, const char* extension);

/** Add an extension to the list of optional instance extensions
 * @param lahar The lahar instance
 * @param extensions The extension to add
 */
uint32_t lahar_builder_extension_add_optional_instance(Lahar* lahar, const char* extension);

/** Add an extension to the list of optional device extensions
 * @param lahar The lahar instance
 * @param extensions The extension to add
 */
uint32_t lahar_builder_extension_add_optional_device(Lahar* lahar, const char* extension);

/** Set a debug callback for vulkan */
void lahar_builder_set_debug_callback(Lahar* lahar, PFN_vkDebugUtilsMessengerCallbackEXT callback);

/** Set a specific device to use. Failure to find the device will
 * always cause finalize to return LAHAR_ERR_NO_SUITABLE_DEVICE
 * 
 * @param lahar The lahar instance
 * @param name The device name
 */
uint32_t lahar_builder_device_use(Lahar* lahar, const char* name);

/** Set a custom scoring metric for device. The callback will be invoked
 * for all devices. Any device with a negative score is ineligble. The
 * device with the highest score is chosen. If not set, the default
 * scoring function is used.
 * 
 * @param lahar The lahar instance
 * @param scorefunc The scoring callback
 */
uint32_t lahar_builder_device_set_scoring(Lahar* lahar, LaharDeviceScoreFunc scorefunc);


/** Tell lahar to create the utility command buffers in the windows.
 * Not needed if you plan to create your own */
void lahar_builder_request_command_buffers(Lahar* lahar);









/** Register a window with lahar. When finalized, this window will have its surface/swapchain/attachments created. 
 * 
 * NOTE: UNLESS you've defined LAHAR_NO_AUTO_DEPS, lahar will take ownership of the window,
 * and destroy it when lahar is deinited.
 * 
 * @param Lahar The lahar instance
 * @param window The window to register
 * @param winprofile The quick profile to use. For more control, see lahar_window_register_ex
*/
uint32_t lahar_builder_window_register(Lahar* lahar, LaharWindow* window, LaharWindowProfile winprofile);

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
 * @param lahar The lahar instance
 * @param window The window
 * @param winconfig The config
 * 
 */
uint32_t lahar_builder_window_register_ex(Lahar* lahar, LaharWindow* window, const LaharWindowConfig* winconfig);




/** Check if an optional instance extension was loaded
 * @param lahar The lahar instance
 * @param extension The extension to check for
 */
bool lahar_extension_has_instance(Lahar* lahar, const char* extension);

/** Check if an optional device extension was loaded
 * @param lahar The lahar instance
 * @param extension The extension to check for
 */
bool lahar_extension_has_device(Lahar* lahar, const char* extension);

/** Begin a frame, preparing for rendering. You only need to use this
 * if you plan on using lahar_window_submit, or lahar_window_present
 * 
 */
uint32_t lahar_window_frame_begin(Lahar* lahar, LaharWindow* window);

/** Submit a command buffer to a window. */
uint32_t lahar_window_submit(Lahar* lahar, LaharWindow* window, VkCommandBuffer cmd);

/** Submit multiple command buffers to a window */
uint32_t lahar_window_submit_all(Lahar* lahar, LaharWindow* window, VkCommandBuffer* cmds, uint32_t cmd_count);

/** Swap the window's visual buffers */
uint32_t lahar_window_present(Lahar* lahar, LaharWindow* window);

/** Resize a window's swapchain when the window changes size
 *
 * @param lahar The lahar instance
 * @param window The window to resize
 */
uint32_t lahar_window_swapchain_resize(Lahar* lahar, LaharWindow* window);

/** THIS IS ONE OF THE CUSTOM WINDOW FUNCTIONS.
 * 
 * If using one of the window libraries supported by lahar, this is automatically implemented for you.
 * If you're using a custom window implementation, YOU must supply an implementation of this, or
 * you will get linker errors.
 * 
 * Create the vulkan surface.
 * 
 * @param lahar The lahar instance
 * @param window The window
 * @param surface (out) The created surface
 * 
 * @returns 0 for success, or any non-zero value to indicate failure, preferably a LAHAR_ERR_*
 */
uint32_t lahar_window_surface_create(Lahar* lahar, LaharWindow* window, VkSurfaceKHR* surface);

/** THIS IS ONE OF THE CUSTOM WINDOW FUNCTIONS.
 * 
 * If using one of the window libraries supported by lahar, this is automatically implemented for you.
 * If you're using a custom window implementation, YOU must supply an implementation of this, or
 * you will get linker errors.
 * 
 * Retrieve the window's size. You likely want this to return the framebuffer size
 * specifically.
 * 
 * @param lahar The lahar instance
 * @param window The window
 * @param width (out) The window's width
 * @param height (out) The window's height 
 * 
 * @returns 0 for success, or any non-zero value to indicate failure, preferably a LAHAR_ERR_*
 */
uint32_t lahar_window_get_size(Lahar* lahar, LaharWindow* window, uint32_t* width, uint32_t* height);

/** THIS IS ONE OF THE CUSTOM WINDOW FUNCTIONS.
 * 
 * If using one of the window libraries supported by lahar, this is automatically implemented for you.
 * If you're using a custom window implementation, YOU must supply an implementation of this, or
 * you will get linker errors.
 * 
 * Retrieve the extensions the window needs in order to function
 * 
 * @param lahar The lahar instance
 * @param window The window
 * @param ext_count (out) The number of extensions the window needs
 * @param extensions (out) The array to write to. This MUST support NULL
 * 
 * @returns 0 for success, or any non-zero value to indicate failure, preferably a LAHAR_ERR_*
 */

uint32_t lahar_window_get_extensions(Lahar* lahar, LaharWindow* window, uint32_t* ext_count, const char** extensions);


/** A utility to record the command to transition a the layout of a window's
 * attachment. Useful if you're doing dynamic rendering.
 * 
 * @param lahar The lahar instance
 * @param window The window
 * @param attachment_index The index of the attachment to transition, as specified in your original attachment array (or see defaults)
 * @param layout The layout to transition to
 * @param cmd The command buffer to record to
 * 
 */
uint32_t lahar_window_attachment_transition(Lahar* lahar, LaharWindow* window, uint32_t attachment_index, VkImageLayout layout, VkCommandBuffer cmd);

/** Get the lahar window state struct for this window. NULL if not found. */
LaharWindowState* lahar_window_state(Lahar* lahar, LaharWindow* window);

/** Wait until a particular window is inactive */
uint32_t lahar_window_wait_inactive(Lahar* lahar, LaharWindow* window);


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











































#define LAHAR_IMPLEMENTATION
#ifdef LAHAR_IMPLEMENTATION






// #######################
// ## LAHAR SOURCE CODE ##
// #######################

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
    #define LAHAR_M_ARENA_SIZE 32768
#endif

#ifndef LAHAR_M_CHECK_CT
    #define LAHAR_M_CHECK_CT 16
#endif


#ifdef __linux__
#include <signal.h>

#define LAHAR_ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "LAHAR_ASSERT failed: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
        raise(SIGTRAP); \
    } \
} while(0)
#else

#define LAHAR_ASSERT(cond) assert(cond)

#endif


char* lahar_strdup(const char* str) {
    size_t len = strlen(str);
    char* cpy = (char*) lahar_malloc(len + 1);

    if (!cpy) { return NULL; }

    memcpy(cpy, str, len + 1);

    return cpy;
}

void* lahar_alloc_or_resize(void* existing, size_t targetsize) {
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




#if defined(_WIN32)
    /** Open the handle to the vulkan lib */
    static uint32_t __lahar_open_libvk(Lahar* lahar) {
        HMODULE module = LoadLibraryA("vulkan-1.dll");

        lahar->libvulkan = module;
        return module ? LAHAR_ERR_SUCCESS : LAHAR_ERR_LOAD_FAILURE;
    }

    /** Loader callback for loading a function from the vulkan lib using native loading mechanisms */
    static PFN_vkVoidFunction lahar_loader_sym(Lahar* lahar, const char* name) {
        return GetProcAddress(lahar->libvulkan, name);
    }
#else
    #include <dlfcn.h>

    /** Open the handle to the vulkan lib */
    static uint32_t __lahar_open_libvk(Lahar* lahar) {
        void* module = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);

        if (!module) {
            module = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
        }

        lahar->libvulkan = module;
        return module ? LAHAR_ERR_SUCCESS : LAHAR_ERR_LOAD_FAILURE;
    }

    /** Loader callback for loading a function from the vulkan lib using native loading mechanisms */
    static PFN_vkVoidFunction lahar_loader_sym(Lahar* lahar, const char* name) {
        return (PFN_vkVoidFunction)dlsym(lahar->libvulkan, name);
    }

#endif

/** Loader callback for loading instance level vulkan functions */
static PFN_vkVoidFunction __lahar_loader_inst(Lahar* lahar, const char* name) {
    return vkGetInstanceProcAddr(lahar->instance, name);
}

/** Loader callback for loading device level vulkan functions */
static PFN_vkVoidFunction __lahar_loader_dev(Lahar* lahar, const char* name) {
    return vkGetDeviceProcAddr(lahar->device, name);
}



#if defined(LAHAR_USE_GLFW)
    uint32_t lahar_window_surface_create(Lahar* lahar, LaharWindow* window, VkSurfaceKHR* surface) {
        if ((lahar->vkresult = glfwCreateWindowSurface(lahar->instance, window, lahar->vkalloc, surface)) != VK_SUCCESS) {
            return LAHAR_ERR_DEPENDENCY_FAILED;
        }

        return LAHAR_ERR_SUCCESS;
    }

    uint32_t lahar_window_get_size(Lahar* lahar, LaharWindow* window, uint32_t* width, uint32_t* height) {
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);

        *width = (uint32_t)w;
        *height = (uint32_t)h;
        return LAHAR_ERR_SUCCESS;
    }


    uint32_t lahar_window_get_extensions(Lahar* lahar, LaharWindow* window, uint32_t* ext_count, const char** extensions) {
        const char** ext = glfwGetRequiredInstanceExtensions(ext_count);

        if (extensions) {
            for (size_t i = 0; i < *ext_count; i++) {
                extensions[i] = ext[i];
            }
        }

        return LAHAR_ERR_SUCCESS;
    }

#endif

#if defined(LAHAR_USE_SDL2) || defined(LAHAR_USE_SDL3)
    uint32_t lahar_window_surface_create(Lahar* lahar, LaharWindow* window, VkSurfaceKHR* surface) {
        if (!SDL_Vulkan_CreateSurface(window, lahar->instance, surface)) {
            fprintf(stderr, "%s\n", SDL_GetError());
            return LAHAR_ERR_DEPENDENCY_FAILED;
        }

        return LAHAR_ERR_SUCCESS;
    }

    uint32_t lahar_window_get_size(Lahar* lahar, LaharWindow* window, uint32_t* width, uint32_t* height) {
        int w, h;

        SDL_Vulkan_GetDrawableSize(window, &w, &h);

        *width = (uint32_t)w;
        *height = (uint32_t)h;
        return LAHAR_ERR_SUCCESS;
    }
#endif

#if defined(LAHAR_USE_SDL2)
    uint32_t lahar_window_get_extensions(Lahar* lahar, LaharWindow* window, uint32_t* ext_count, const char** extensions) {
        if (!SDL_Vulkan_GetInstanceExtensions(window, ext_count, extensions)) {
            return LAHAR_ERR_DEPENDENCY_FAILED;
        }

        return LAHAR_ERR_SUCCESS;
    }
#elif defined(LAHAR_USE_SDL3)
    uint32_t lahar_window_get_extensions(Lahar* lahar, LaharWindow* window, uint32_t* ext_count, const char** extensions) {
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
#endif


#if defined(LAHAR_USE_VMA)
    static uint32_t __lahar_vma_alloc_img(void* self, Lahar* lahar, const VkImageCreateInfo* info, VkImage* image, VmaAllocation* allocation) {
        if (!self || !lahar) { return LAHAR_ERR_INVALID_STATE; }
        if (!lahar->vma) { return LAHAR_ERR_INVALID_CONFIGURATION; }
        if (!info || !image || !allocation) { return LAHAR_ERR_ILLEGAL_PARAMS; }

        VmaAllocationCreateInfo alloc_create = {};
        VmaAllocationInfo alloc_info = {};

        if ((lahar->vkresult = vmaCreateImage(lahar->vma, info, &alloc_create, image, allocation, &alloc_info)) != VK_SUCCESS) {
            return LAHAR_ERR_DEPENDENCY_FAILED;
        }

        return LAHAR_ERR_SUCCESS;
    }

    static uint32_t __lahar_vma_free_img(void* self, Lahar* lahar, VkImage* image, VmaAllocation* allocation) {
        if (!self || !lahar) { return LAHAR_ERR_INVALID_STATE; }
        if (!lahar->vma) { return LAHAR_ERR_INVALID_CONFIGURATION; }
        if (!image || !allocation) { return LAHAR_ERR_ILLEGAL_PARAMS; }

        vmaDestroyImage(lahar->vma, *image, *allocation);

        return LAHAR_ERR_SUCCESS;
    }

    static LaharAllocator __lahar_vma_adapter = {
        .alloc_image = __lahar_vma_alloc_img,
        .free_image = __lahar_vma_free_img
    };

    uint32_t lahar_vma_set_allocator(Lahar* lahar, VmaAllocator allocator) {
        if (!lahar || !allocator) { return LAHAR_ERR_ILLEGAL_PARAMS; }

        lahar->vma = allocator;
        return LAHAR_ERR_SUCCESS;
    }

    static uint32_t __lahar_init_vma(Lahar* lahar) {
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

            lahar->vma_created = true;
        }

        return LAHAR_ERR_SUCCESS;
    }

    static void __lahar_deinit_vma(Lahar* lahar) {
        if (lahar->vma_created) {
            vmaDestroyAllocator(lahar->vma);
        }
    }
#endif



static uint32_t lahar_load_loader(Lahar* lahar, LaharLoaderFunc loadfn);
static uint32_t lahar_load_instance(Lahar* lahar, LaharLoaderFunc loadfn);
static uint32_t lahar_load_device(Lahar* lahar, LaharLoaderFunc loadfn);


static uint8_t __marena[LAHAR_M_ARENA_SIZE];
static size_t __mpos = 0;
static size_t __mcheck[LAHAR_M_CHECK_CT];
static size_t __mchkct = 0;

void lahar_temp_mcheck() {
    LAHAR_ASSERT(__mchkct < sizeof(__mcheck) / sizeof(__mcheck[0])); // if this trips, Lahar is broken
    __mcheck[__mchkct++] = __mpos;
}

void lahar_temp_mpop() {
    if (__mchkct > 0) {
        __mchkct--;
        __mpos = __mcheck[__mchkct];
    }
}

void* lahar_temp_alloc(size_t bytes) {
    size_t remain = sizeof(__marena) - __mpos;
    LAHAR_ASSERT(remain >= bytes); // if this trips, you need to define LAHAR_M_ARENA_SIZE bigger

    void* ret = (void*)&__marena[__mpos];
    __mpos += bytes;
    return ret;
}

char* lahar_temp_strdup(const char* str) {
    size_t len = strlen(str);
    char* buf = (char*)lahar_temp_alloc(len + 1);
    memcpy(buf, str, len + 1);
    return buf;
}


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
        default:
            break;
    }

    return VK_FALSE;
}

static int64_t __lahar_default_scorer(const LaharDeviceInfo* devinfo) {
    if (!devinfo->has_graphics_queue || !devinfo->has_present_queue) { return -1; }

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
    size_t remapped_memory = (local_memory / 107374182400.0) * 1000;
    score += remapped_memory;

    return score;
}

static uint32_t __lahar_default_surface_format_chooser(Lahar* lahar, LaharWindowState* window_state, LaharDeviceInfo* physdev_info, VkSurfaceFormatKHR* surface_fmt_out){

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

static uint32_t __lahar_default_surface_present_mode_chooser(Lahar* lahar, LaharWindowState* window_state, LaharDeviceInfo* physdev_info, VkPresentModeKHR* present_mode_out) {
    uint32_t err = LAHAR_ERR_SUCCESS;

    for (size_t i = 0; i < physdev_info->present_mode_count; i++) {
        if (physdev_info->present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            *present_mode_out = VK_PRESENT_MODE_MAILBOX_KHR;
            return LAHAR_ERR_SUCCESS;
        }
    }

    *present_mode_out = VK_PRESENT_MODE_FIFO_KHR;
    return LAHAR_ERR_SUCCESS;
}

static uint32_t __lahar_default_resizer(Lahar* lahar, LaharWindow* window) {
    LaharWindowState* winstate = lahar_window_state(lahar, window);
    if (!winstate) { return LAHAR_ERR_INVALID_WINDOW; }

    lahar_temp_mcheck();

    uint32_t err = LAHAR_ERR_SUCCESS;
    uint32_t image_count = 0;
    VkImage* swap_imgs = NULL;
    VkImageView* swap_views = NULL;
    uint32_t old_swap_size = winstate->swap_size;
    LaharSurfaceFormatChooseFunc choose_format = lahar->format_chooser ? lahar->format_chooser : __lahar_default_surface_format_chooser;
    LaharSurfacePresentModeChooseFunc choose_mode = lahar->present_chooser ? lahar->present_chooser : __lahar_default_surface_present_mode_chooser;
    VkSurfaceCapabilitiesKHR surface_caps = {};
    LaharAttachmentConfig* color_conf = &winstate->attachment_configs[LAHAR_ATT_COLOR_INDEX];
    uint32_t queue_indices[2] = { lahar->physdev_info.graphics_queue_index, lahar->physdev_info.present_queue_index };
    uint32_t queue_index_count = queue_indices[0] == queue_indices[1] ? 0 : 2;
    VkSwapchainCreateInfoKHR create_info = {};

    if ((err = lahar_window_wait_inactive(lahar, window))) {
        goto end;
    }

    if (winstate->attachment_count > 1 && !lahar->gpu_allocator) {
        err = LAHAR_ERR_INVALID_STATE;
        goto end;
    }

    for (size_t i = 0; i < winstate->swap_size; i++) {
        LaharAttachment* attachment = &winstate->attachments[LAHAR_ATT_COLOR_INDEX][i];

        if (attachment->view != VK_NULL_HANDLE && vkDestroyImageView) {
            vkDestroyImageView(lahar->device, attachment->view, lahar->vkalloc);
        }
    }

    for (size_t i = 1; i < winstate->attachment_count; i++) {
        LaharAttachment* attachment_list = winstate->attachments[i];

        for (size_t j = 0; j < winstate->swap_size; j++) {
            LaharAttachment* attachment = &attachment_list[j];

            if (attachment->view != VK_NULL_HANDLE&& vkDestroyImageView) {
                vkDestroyImageView(lahar->device, attachment->view, lahar->vkalloc);
            }

            if (attachment->image != VK_NULL_HANDLE) {
                lahar->gpu_allocator->free_image(lahar->gpu_allocator, lahar, &attachment->image, &attachment->img_allocation);
            }
        }
    }

    vkDestroySwapchainKHR(lahar->device, winstate->swapchain, lahar->vkalloc);

    if (winstate->desired_img_count == 0) {
        winstate->desired_img_count = 2;
    }

    if ((lahar->vkresult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(lahar->physdev_info.physdev, winstate->surface, &surface_caps))) {
        err = LAHAR_ERR_VK_ERR;
        goto end;
    }

    if ((err = choose_format(lahar, winstate, &lahar->physdev_info, &winstate->surface_format))) {
        err = LAHAR_ERR_VK_ERR;
        goto end;
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
    create_info.oldSwapchain = VK_NULL_HANDLE;

    if ((err = lahar_window_get_size(lahar, winstate->window, &create_info.imageExtent.width, &create_info.imageExtent.height))) {
        goto end;
    }

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

    if ((err = choose_mode(lahar, winstate, &lahar->physdev_info, &create_info.presentMode))) {
        goto end;
    }

    image_count = winstate->desired_img_count;

    if (surface_caps.maxImageCount > 0 && image_count > surface_caps.maxImageCount) {
        image_count = surface_caps.maxImageCount;
    }
    else if (surface_caps.minImageCount > 0 && image_count < surface_caps.minImageCount) {
        image_count = surface_caps.minImageCount;
    }

    create_info.minImageCount = image_count;

    if ((lahar->vkresult = vkCreateSwapchainKHR(lahar->device, &create_info, lahar->vkalloc, &winstate->swapchain)) != VK_SUCCESS) {
        goto end;
    }

    vkGetSwapchainImagesKHR(lahar->device, winstate->swapchain, &winstate->swap_size, NULL);

    // TODO: handle potential swapchain image count changes
    LAHAR_ASSERT(old_swap_size == winstate->swap_size);

    swap_imgs = (VkImage*)lahar_temp_alloc(winstate->swap_size * sizeof(VkImage));
    swap_views = (VkImageView*)lahar_temp_alloc(winstate->swap_size * sizeof(VkImageView));

    vkGetSwapchainImagesKHR(lahar->device, winstate->swapchain, &winstate->swap_size, swap_imgs);

    for (size_t j = 0; j < winstate->swap_size; j++) {
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

        winstate->attachments[LAHAR_ATT_COLOR_INDEX][j].image = swap_imgs[j];
        winstate->attachments[LAHAR_ATT_COLOR_INDEX][j].view = swap_views[j];
        winstate->attachments[LAHAR_ATT_COLOR_INDEX][j].layout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    if (winstate->attachment_count > 1 && !lahar->gpu_allocator) {
        err = LAHAR_ERR_INVALID_CONFIGURATION;
        goto end;
    }

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

            if ((err = lahar->gpu_allocator->alloc_image(
                lahar->gpu_allocator,
                lahar,
                &attachment_config->img_info,
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


            attachment->layout = VK_IMAGE_LAYOUT_UNDEFINED;
        }
    }

end:
    lahar_temp_mpop();
    return err;
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
        default: return "LAHAR_UNKNOWN_ERROR";
    }
}



void* lahar_get_user_data(Lahar* lahar) {
    return lahar->user_data;
}

void lahar_set_user_data(Lahar* lahar, void* user_data) {
    lahar->user_data = user_data;
}


uint32_t lahar_init(Lahar* lahar) {
    if (!lahar) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    memset(lahar, 0, sizeof(*lahar));

    #if !defined(LAHAR_NO_AUTO_DEP)
        #if defined(LAHAR_USE_GLFW)

        if (!glfwInit()) {
            return LAHAR_ERR_DEPENDENCY_FAILED;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        #elif defined(LAHAR_USE_SDL2) || defined(LAHAR_USE_SDL3)

        if (SDL_Init(SDL_INIT_EVERYTHING)) {
            return LAHAR_ERR_DEPENDENCY_FAILED;
        }

        #endif
    #endif


    uint32_t err = LAHAR_ERR_SUCCESS;
    
    if ((err = __lahar_open_libvk(lahar))) {
        return err;
    }

    if ((err = lahar_load_loader(lahar, lahar_loader_sym))) {
        return err;
    }

    return LAHAR_ERR_SUCCESS;
}

uint32_t lahar_builder_allocator_set(Lahar* lahar, LaharAllocator* allocator) {
    if (!lahar || !allocator || !allocator->alloc_image || !allocator->free_image) {
        return LAHAR_ERR_ILLEGAL_PARAMS;
    }

    lahar->gpu_allocator = allocator;
    return LAHAR_ERR_SUCCESS;
}

void lahar_builder_set_vulkan_version(Lahar* lahar, uint32_t version) {
    if (lahar->instance == VK_NULL_HANDLE) {
        lahar->vkversion = version;
    }
}

void lahar_builder_request_validation_layers(Lahar* lahar) {
    lahar->wantvalidation = true;
}

uint32_t lahar_builder_extension_add_required_instance(Lahar* lahar, const char* extension) {
    if (!lahar || !extension) { return LAHAR_ERR_ILLEGAL_PARAMS; }

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

uint32_t lahar_builder_extension_add_required_device(Lahar* lahar, const char* extension) {
    if (!lahar || !extension) { return LAHAR_ERR_ILLEGAL_PARAMS; }

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

uint32_t lahar_builder_extension_add_optional_instance(Lahar* lahar, const char* extension) {
    if (!lahar || !extension) { return LAHAR_ERR_ILLEGAL_PARAMS; }

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

uint32_t lahar_builder_extension_add_optional_device(Lahar* lahar, const char* extension) {
    if (!lahar || !extension) { return LAHAR_ERR_ILLEGAL_PARAMS; }

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

    size_t count = lahar->extensions.ode_count;

    lahar->extensions.opt_dev_exts[count] = cpy;
    lahar->extensions.opt_dev_exts_present[count] = false;

    lahar->extensions.ode_count++;

    return LAHAR_ERR_SUCCESS;
}

void lahar_builder_set_debug_callback(Lahar *lahar, PFN_vkDebugUtilsMessengerCallbackEXT callback) {
    lahar->debug_callback = callback;
}

uint32_t lahar_builder_device_use(Lahar* lahar, const char* name) {
    if (!lahar || !name || name[0] == '\0') { return LAHAR_ERR_ILLEGAL_PARAMS; }

    char* cpy = (char*)lahar_strdup(name);
    if (!cpy) { return LAHAR_ERR_ALLOC_FAILED; }

    lahar->device_name = cpy;
    return LAHAR_ERR_SUCCESS;
}

uint32_t lahar_builder_device_set_scoring(Lahar* lahar, LaharDeviceScoreFunc scorefunc) {
    if (!lahar || !scorefunc) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    lahar->score_func = scorefunc;
    return LAHAR_ERR_SUCCESS;
}

void lahar_builder_request_command_buffers(Lahar* lahar) {
    lahar->wantcommands = true;
}

uint32_t lahar_builder_window_register_ex(Lahar* lahar, LaharWindow* window, const LaharWindowConfig* winconf) {
    if (!lahar || !window || !winconf || winconf->attachment_count == 0) { return LAHAR_ERR_ILLEGAL_PARAMS; }

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

    if ((err = lahar_window_get_size(lahar, window, &window_state->width, &window_state->height))) {
        goto end;
    }

    window_state->attachment_count = winconf->attachment_count;
    window_state->attachment_configs = (LaharAttachmentConfig*)lahar_malloc(window_state->attachment_count * sizeof(LaharAttachmentConfig));

    memcpy(window_state->attachment_configs, winconf->attachments, window_state->attachment_count * sizeof(LaharAttachmentConfig));

    window_state->desired_img_count = winconf->desired_swap_size ? winconf->desired_swap_size : 2;
    window_state->max_in_flight = winconf->max_in_flight ? winconf->max_in_flight : 2;

    window_state->attachments = (LaharAttachment**)lahar_malloc(window_state->attachment_count * sizeof(void*));

    window_state->auto_recreate_swap = !winconf->no_auto_swap_resize;

    // We can't create these until after the swap chain, as we don't know how
    // big to make these arrays yet
    for (size_t i = 0; i < window_state->attachment_count; i++) {
        window_state->attachments[i] = NULL;
    }

end:
    return err;
}

uint32_t lahar_builder_window_register(Lahar* lahar, LaharWindow* window, LaharWindowProfile winprof) {
    if (!lahar || !window) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    switch(winprof) {
        case LAHAR_WINPROF_COLOR: {
            // Attachment 0 is ignored anyway, but must be specified for color
            LaharAttachmentConfig attachments = {0};

            LaharWindowConfig conf = {
                .attachment_count = 1,
                .attachments = &attachments
            };

            return lahar_builder_window_register_ex(lahar, window, &conf);

        } break;

        case LAHAR_WINPROF_COLOR_DEPTH: {
            // Attachment 0 is ignored anyway, but must be specified for color
            LaharAttachmentConfig attachments[] = {
                {0},
                {
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

            return lahar_builder_window_register_ex(lahar, window, &conf);
        } break;

        default:
            return LAHAR_ERR_ILLEGAL_PARAMS;
    }


}


bool lahar_extension_has_instance(Lahar* lahar, const char* extension) {
    if (!lahar || !extension) { return false; }

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

bool lahar_extension_has_device(Lahar* lahar, const char* extension) {
    if (!lahar || !extension) { return false; }

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

void lahar_deinit(Lahar* lahar) {
    if (vkDeviceWaitIdle) {
        vkDeviceWaitIdle(lahar->device);
    }

    for (size_t i = 0; i < lahar->window_count; i++) {
        LaharWindowState* state = &lahar->windows[i];

        if (state->commands) {
            lahar_free(state->commands);
        }

        if (state->image_available) {
            for (size_t j = 0; j < state->max_in_flight; j++) {
                if (state->image_available != VK_NULL_HANDLE && vkDestroySemaphore) {
                    vkDestroySemaphore(lahar->device, state->image_available[j], lahar->vkalloc);
                }
            }

            lahar_free(state->image_available);
        }

        if (state->render_finished) {
            for (size_t j = 0; j < state->max_in_flight; j++) {
                if (state->render_finished != VK_NULL_HANDLE && vkDestroySemaphore) {
                    vkDestroySemaphore(lahar->device, state->render_finished[j], lahar->vkalloc);
                }
            }

            lahar_free(state->render_finished);
        }

        if (state->in_flight) {
            for (size_t j = 0; j < state->max_in_flight; j++) {
                if (state->in_flight != VK_NULL_HANDLE && vkDestroyFence) {
                    vkDestroyFence(lahar->device, state->in_flight[j], lahar->vkalloc);
                }
            }

            lahar_free(state->in_flight);
        }


        // Special pass required to destroy _just_ the views in the color attachment
        for (size_t j = 0; j < state->swap_size; j++) {
            LaharAttachment* attachment = &state->attachments[LAHAR_ATT_COLOR_INDEX][j];

            if (attachment->view != VK_NULL_HANDLE && vkDestroyImageView) {
                vkDestroyImageView(lahar->device, attachment->view, lahar->vkalloc);
            }
        }

        lahar_free(state->attachments[LAHAR_ATT_COLOR_INDEX]);

        for (size_t j = 1; j < state->attachment_count; j++) {
            LaharAttachment* attachment_list = state->attachments[j];

            for (size_t k = 0; k < state->swap_size; k++) {
                LaharAttachment* attachment = &state->attachments[j][k];

                if (attachment->view != VK_NULL_HANDLE && vkDestroyImageView) {
                    vkDestroyImageView(lahar->device, attachment->view, lahar->vkalloc);
                }

                if (attachment->image != VK_NULL_HANDLE && lahar->gpu_allocator) {
                    lahar->gpu_allocator->free_image(lahar->gpu_allocator, lahar, &attachment->image, &attachment->img_allocation);
                }
            }

            lahar_free(attachment_list);
        }

        lahar_free(state->attachments);
        lahar_free(state->attachment_configs);

        if (state->swapchain != VK_NULL_HANDLE && vkDestroySwapchainKHR) {
            vkDestroySwapchainKHR(lahar->device, state->swapchain, lahar->vkalloc);
        }

        if (state->surface != VK_NULL_HANDLE && vkDestroySurfaceKHR) {
            vkDestroySurfaceKHR(lahar->instance, lahar->windows[i].surface, lahar->vkalloc);
        }


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
    __lahar_deinit_vma(lahar);
    #endif

    if (lahar->pool != VK_NULL_HANDLE && vkDestroyCommandPool) {
        vkDestroyCommandPool(lahar->device, lahar->pool, lahar->vkalloc);
    }

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

    memset(lahar, 0, sizeof(*lahar));
}



/** This builds a complete list of the instance level extensions required */
uint32_t __lahar_temp_extensions(Lahar* lahar, LaharWindow* window, uint32_t* count, char*** ext_out) {
    uint32_t err = LAHAR_ERR_SUCCESS;

    uint32_t ext_count = lahar->extensions.rie_count;
    char** win_exts = NULL;
    char** extensions = NULL;
    size_t i = 0;

    if (lahar->wantvalidation) {
        ext_count++;
    }

    uint32_t win_count = 0;
    if ((err = lahar_window_get_extensions(lahar, lahar->windows[0].window, &win_count, NULL))) {
        goto end;
    }

    ext_count += win_count;

    win_exts = (char**)lahar_temp_alloc(sizeof(char*) * win_count);

    if ((err = lahar_window_get_extensions(lahar, lahar->windows[0].window, &win_count, (const char**)win_exts))) {
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


uint32_t __lahar_build_inst_extensions(Lahar* lahar) {
    uint32_t err = LAHAR_ERR_SUCCESS;
    lahar_temp_mcheck();

    uint32_t ext_count;
    char** extensions = NULL;
    uint32_t prop_count;
    VkExtensionProperties* props = NULL;

    // Assume the first window is sufficient
    __lahar_temp_extensions(lahar, lahar->windows[0].window, &ext_count, &extensions);

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

uint32_t __lahar_build_instance(Lahar* lahar) {
    uint32_t err = LAHAR_ERR_SUCCESS;
    lahar_temp_mcheck();

    uint32_t ext_count;
    char** extensions = NULL;
    uint32_t avail_layer_count;
    VkLayerProperties* layer_props = NULL;
    const char* dbg_layer_name = "VK_LAYER_KHRONOS_validation";
    bool dbg_layer_found = false;

    // Assume the first window is sufficient
    __lahar_temp_extensions(lahar, lahar->windows[0].window, &ext_count, &extensions);

    VkApplicationInfo appinfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = lahar->appname ? lahar->appname : "Lahar",
        .applicationVersion = lahar->appversion != 0 ? lahar->appversion : VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "None",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = lahar->vkversion != 0 ? lahar->vkversion : VK_API_VERSION_1_3
    };

    VkInstanceCreateInfo createinfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appinfo,
        .enabledExtensionCount = (uint32_t)ext_count,
        .ppEnabledExtensionNames = (const char* const *)extensions
    };

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


    if ((err = lahar_load_instance(lahar, __lahar_loader_inst))) {
        goto end;
    }

    if (vkEnumerateInstanceVersion) {
        if ((lahar->vkresult = vkEnumerateInstanceVersion(&lahar->vkversion))) {
            goto end;
        }
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

uint32_t __lahar_build_early_surface(Lahar* lahar) {
    uint32_t err = LAHAR_ERR_SUCCESS;

    for (size_t i = 0; i < lahar->window_count; i++) {
        LaharWindowState* winstate = &lahar->windows[i];

        if ((err = lahar_window_surface_create(lahar, winstate->window, &winstate->surface))) {
            return err;
        }
    }

    return err;
}

uint32_t __lahar_build_physdev(Lahar* lahar) {
    uint32_t err = LAHAR_ERR_SUCCESS;
    lahar_temp_mcheck();

    uint32_t dev_count;
    VkPhysicalDevice* devices = NULL;
    LaharDeviceInfo* dev_infos = NULL;
    int64_t* dev_scores = NULL;
    LaharDeviceScoreFunc scorer = lahar->score_func ? lahar->score_func : __lahar_default_scorer;
    int64_t best_dev = -1;
    int64_t best_score = -1;

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

    if (lahar->wantvalidation) {
        VkDebugUtilsMessengerCallbackDataEXT cbdata = {};
        LaharDeviceInfo* info = &dev_infos[best_dev];

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

    lahar->physdev_info = dev_infos[best_dev];

end:
    lahar_temp_mpop();
    return err;
}

uint32_t __lahar_build_device(Lahar* lahar) {
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

    VkPhysicalDeviceFeatures device_features = {};

    const char* dbg_layer_name = "VK_LAYER_KHRONOS_validation";
    bool has_dbg_layer = false;

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

    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = queue_create_count,
        .pQueueCreateInfos = queue_create_infos,
        .enabledLayerCount = enabled_layer_count,
        .ppEnabledLayerNames = &dbg_layer_name,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = &swap_ext_name,
        .pEnabledFeatures = &device_features,
    };

    if ((lahar->vkresult = vkCreateDevice(lahar->physdev_info.physdev, &create_info, lahar->vkalloc, &lahar->device)) != VK_SUCCESS) {
        goto end;
    }

    if ((err = lahar_load_device(lahar, __lahar_loader_dev))) {
        goto end;
    }

    vkGetDeviceQueue(lahar->device, lahar->physdev_info.graphics_queue_index, 0, &lahar->graphicsQueue);
    vkGetDeviceQueue(lahar->device, lahar->physdev_info.present_queue_index, 0, &lahar->presentQueue);

    if (lahar->wantcommands) {
        VkCommandPoolCreateInfo pool_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = lahar->physdev_info.graphics_queue_index
        };

        if ((lahar->vkresult = vkCreateCommandPool(lahar->device, &pool_info, lahar->vkalloc, &lahar->pool)) != VK_SUCCESS) {
            err = LAHAR_ERR_VK_ERR;
            goto end;
        }
    }

end:
    lahar_temp_mpop();
    return err;
}

uint32_t __lahar_build_swapchain(Lahar* lahar) {
    uint32_t err = LAHAR_ERR_SUCCESS;
    lahar_temp_mcheck();

    LaharSurfaceFormatChooseFunc choose_format = lahar->format_chooser ? lahar->format_chooser : __lahar_default_surface_format_chooser;
    LaharSurfacePresentModeChooseFunc choose_mode = lahar->present_chooser ? lahar->present_chooser : __lahar_default_surface_present_mode_chooser;

    #if defined(LAHAR_USE_VMA)
    if ((err = __lahar_init_vma(lahar))) {
        goto end;
    }
    #endif

    for (size_t i = 0; i < lahar->window_count; i++) {
        LaharWindowState* winstate = &lahar->windows[i];
        VkSurfaceCapabilitiesKHR surface_caps = {};

        if (winstate->desired_img_count == 0) {
            winstate->desired_img_count = 2;
        }

        if ((lahar->vkresult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(lahar->physdev_info.physdev, winstate->surface, &surface_caps))) {
            err = LAHAR_ERR_VK_ERR;
            goto end;
        }

        if ((err = choose_format(lahar, winstate, &lahar->physdev_info, &winstate->surface_format))) {
            goto end;
        }

        LaharAttachmentConfig* color_conf = &winstate->attachment_configs[LAHAR_ATT_COLOR_INDEX];

        uint32_t queue_indices[2] = { lahar->physdev_info.graphics_queue_index, lahar->physdev_info.present_queue_index };
        uint32_t queue_index_count = queue_indices[0] == queue_indices[1] ? 0 : 2;

        VkSwapchainCreateInfoKHR create_info = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = winstate->surface,
            .imageFormat = winstate->surface_format.format,
            .imageColorSpace = winstate->surface_format.colorSpace,
            .imageArrayLayers = 1,
            .imageUsage = color_conf->usage ? color_conf->usage : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 
            .imageSharingMode = queue_indices[0] == queue_indices[1] ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
            .queueFamilyIndexCount = queue_index_count,
            .pQueueFamilyIndices = queue_indices,
            .preTransform = surface_caps.currentTransform,
            .compositeAlpha = winstate->alpha ? winstate->alpha : VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .clipped = VK_TRUE,
            .oldSwapchain = VK_NULL_HANDLE,
        };

        if ((err = lahar_window_get_size(lahar, winstate->window, &create_info.imageExtent.width, &create_info.imageExtent.height))) {
            goto end;
        }

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

        if ((err = choose_mode(lahar, winstate, &lahar->physdev_info, &create_info.presentMode))) {
            goto end;
        }

        uint32_t image_count = winstate->desired_img_count;

        if (surface_caps.maxImageCount > 0 && image_count > surface_caps.maxImageCount) {
            image_count = surface_caps.maxImageCount;
        }
        else if (surface_caps.minImageCount > 0 && image_count < surface_caps.minImageCount) {
            image_count = surface_caps.minImageCount;
        }

        create_info.minImageCount = image_count;

        if ((lahar->vkresult = vkCreateSwapchainKHR(lahar->device, &create_info, lahar->vkalloc, &winstate->swapchain)) != VK_SUCCESS) {
            err = LAHAR_ERR_VK_ERR;
            goto end;
        }

        vkGetSwapchainImagesKHR(lahar->device, winstate->swapchain, &winstate->swap_size, NULL);

        for (size_t j = 0; j < winstate->attachment_count; j++) {
            size_t bytes = winstate->swap_size * sizeof(LaharAttachment);
            winstate->attachments[j] = (LaharAttachment*)lahar_malloc(bytes);
            memset(winstate->attachments[j], 0, bytes);
        }

        VkImage* swap_imgs = (VkImage*)lahar_temp_alloc(winstate->swap_size * sizeof(VkImage));
        VkImageView* swap_views = (VkImageView*)lahar_temp_alloc(winstate->swap_size * sizeof(VkImageView));

        vkGetSwapchainImagesKHR(lahar->device, winstate->swapchain, &winstate->swap_size, swap_imgs);

        for (size_t j = 0; j < winstate->swap_size; j++) {
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

            winstate->attachments[LAHAR_ATT_COLOR_INDEX][j].image = swap_imgs[j];
            winstate->attachments[LAHAR_ATT_COLOR_INDEX][j].view = swap_views[j];
        }

        if (winstate->attachment_count > 1 && !lahar->gpu_allocator) {
            err = LAHAR_ERR_INVALID_CONFIGURATION;
            goto end;
        }

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

                if ((err = lahar->gpu_allocator->alloc_image(
                    lahar->gpu_allocator,
                    lahar,
                    &attachment_config->img_info,
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

        if (lahar->wantcommands) {
            winstate->commands = (VkCommandBuffer*)lahar_malloc(winstate->swap_size * sizeof(VkCommandBuffer));

            VkCommandBufferAllocateInfo buffer_alloc = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = lahar->pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = winstate->swap_size
            };

            if ((lahar->vkresult = vkAllocateCommandBuffers(lahar->device, &buffer_alloc, winstate->commands)) != VK_SUCCESS) {
                err = LAHAR_ERR_VK_ERR;
                goto end;
            }
        }
    }

end:
    lahar_temp_mpop();
    return err;
}

uint32_t __lahar_build_sync(Lahar* lahar) {
    uint32_t err = LAHAR_ERR_SUCCESS;

    VkSemaphoreCreateInfo sem_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    for (size_t i = 0; i < lahar->window_count; i++) {
        LaharWindowState* winstate = &lahar->windows[i];

        winstate->image_available = (VkSemaphore*)lahar_malloc(winstate->max_in_flight * sizeof(VkSemaphore));
        winstate->render_finished = (VkSemaphore*)lahar_malloc(winstate->max_in_flight * sizeof(VkSemaphore));
        winstate->in_flight = (VkFence*)lahar_malloc(winstate->max_in_flight * sizeof(VkFence));

        for (size_t j = 0; j < winstate->max_in_flight; j++) {
            if ((lahar->vkresult = vkCreateSemaphore(lahar->device, &sem_info, lahar->vkalloc, &winstate->image_available[j])) != VK_SUCCESS) {
                return LAHAR_ERR_VK_ERR;
            }

            if ((lahar->vkresult = vkCreateSemaphore(lahar->device, &sem_info, lahar->vkalloc, &winstate->render_finished[j])) != VK_SUCCESS) {
                return LAHAR_ERR_VK_ERR;
            }

            if ((lahar->vkresult = vkCreateFence(lahar->device, &fence_info, lahar->vkalloc, &winstate->in_flight[j])) != VK_SUCCESS) {
                return LAHAR_ERR_VK_ERR;
            }
        }
    }

    return err;
}

uint32_t lahar_build(Lahar* lahar) {
    uint32_t err = LAHAR_ERR_SUCCESS;

    if ((err = __lahar_build_inst_extensions(lahar))) { goto end; }
    if ((err = __lahar_build_instance(lahar))) { goto end; }
    if ((err = __lahar_build_early_surface(lahar))) { goto end; }
    if ((err = __lahar_build_physdev(lahar))) { goto end; }
    if ((err = __lahar_build_device(lahar))) { goto end; }
    if ((err = __lahar_build_swapchain(lahar))) { goto end; }
    if ((err = __lahar_build_sync(lahar))) { goto end; }

end:
    if (err) {
        lahar_deinit(lahar);
    }

    return err;
}

LaharWindowState* lahar_window_state(Lahar* lahar, LaharWindow* window) {
    for (size_t i = 0; i < lahar->window_count; i++) {
        if (lahar->windows[i].window == window) {
            return &lahar->windows[i];
        }
    }

    return NULL;
}

uint32_t lahar_window_swapchain_resize(Lahar* lahar, LaharWindow* window) {
    LaharWindowState* winstate = lahar_window_state(lahar, window);

    if (!winstate) { return LAHAR_ERR_INVALID_WINDOW; }

    LaharSurfaceResizeFunc resizer = winstate->resize_callback ? winstate->resize_callback : __lahar_default_resizer;
    return resizer(lahar, window);
}

uint32_t lahar_window_frame_begin(Lahar* lahar, LaharWindow* window) {
    if (!lahar || !window) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    LaharWindowState* winstate = lahar_window_state(lahar, window);

    if (!winstate) { return LAHAR_ERR_INVALID_WINDOW; }

    if (winstate->frame_phase != LAHAR_FRAME_PHASE_BEGIN) {
        return LAHAR_ERR_INVALID_FRAME_STATE;
    }

    vkWaitForFences(lahar->device, 1, &winstate->in_flight[winstate->flight_index], VK_TRUE, UINT64_MAX);

    VkResult res = vkAcquireNextImageKHR(lahar->device, winstate->swapchain, UINT64_MAX, winstate->image_available[winstate->flight_index], VK_NULL_HANDLE, &winstate->frame_index);

    if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) {
        if (winstate->auto_recreate_swap) {

            uint32_t err;
            if ((err = lahar_window_swapchain_resize(lahar, window))) {
                return err;
            }

            return lahar_window_frame_begin(lahar, window);
        }
        else {
            return LAHAR_ERR_SWAPCHAIN_OUT_OF_DATE;
        }
    }
    else if (res != VK_SUCCESS) {
        lahar->vkresult = res;
        return LAHAR_ERR_VK_ERR;
    }

    vkResetFences(lahar->device, 1, &winstate->in_flight[winstate->flight_index]);
    winstate->frame_phase = LAHAR_FRAME_PHASE_DRAW;

    return LAHAR_ERR_SUCCESS;
}

uint32_t lahar_window_submit_all(Lahar* lahar, LaharWindow* window, VkCommandBuffer* cmds, uint32_t cmd_count) {
    if (!lahar || !window || !cmds || cmd_count == 0) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    LaharWindowState* winstate = lahar_window_state(lahar, window);

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
        .pSignalSemaphores = &winstate->render_finished[winstate->flight_index],
    };

    if ((lahar->vkresult = vkQueueSubmit(lahar->graphicsQueue, 1, &submit_info, winstate->in_flight[winstate->flight_index])) != VK_SUCCESS) {
        return LAHAR_ERR_VK_ERR;
    }

    winstate->frame_phase = LAHAR_FRAME_PHASE_PRESENT;

    return LAHAR_ERR_SUCCESS;
}

uint32_t lahar_window_submit(Lahar* lahar, LaharWindow* window, VkCommandBuffer cmd) {
    return lahar_window_submit_all(lahar, window, &cmd, 1);
}


uint32_t lahar_window_present(Lahar* lahar, LaharWindow* window) {
    LaharWindowState* winstate = lahar_window_state(lahar, window);

    if (!winstate) { return LAHAR_ERR_INVALID_WINDOW; }

    VkPresentInfoKHR present_info = {};

    if (winstate->frame_phase == LAHAR_FRAME_PHASE_BEGIN) {
        return LAHAR_ERR_INVALID_FRAME_STATE;
    }
    else if (winstate->frame_phase == LAHAR_FRAME_PHASE_DRAW) { // nothing has been rendered to this frame
        return LAHAR_ERR_NO_COMMAND_BUFFER; // TODO: look into submitting an empty command buffer
    }
    else if (winstate->frame_phase == LAHAR_FRAME_PHASE_PRESENT) { // A render command buffer has been submitted
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &winstate->render_finished[winstate->flight_index];
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &winstate->swapchain;
        present_info.pImageIndices = &winstate->frame_index;
    }

    if ((lahar->vkresult = vkQueuePresentKHR(lahar->presentQueue, &present_info)) != VK_SUCCESS) {
        return LAHAR_ERR_VK_ERR;
    }

    winstate->flight_index = (winstate->flight_index + 1) % winstate->max_in_flight;

    winstate->frame_phase = LAHAR_FRAME_PHASE_BEGIN;

    return LAHAR_ERR_SUCCESS;
}


VkAccessFlags __lahar_access_mask(VkImageLayout layout) {
    switch (layout) {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            return 0;
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

VkPipelineStageFlags __lahar_pipeline_stage(VkImageLayout layout) {
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

uint32_t lahar_window_attachment_transition(Lahar* lahar, LaharWindow* window, uint32_t attachment_index, VkImageLayout layout, VkCommandBuffer cmd) {
    if (!lahar || !window || cmd == VK_NULL_HANDLE) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    LaharWindowState* winstate = lahar_window_state(lahar, window);

    if (!winstate) { return LAHAR_ERR_INVALID_WINDOW; }
    if (attachment_index >= winstate->attachment_count) { return LAHAR_ERR_ILLEGAL_PARAMS; }

    LaharAttachment* attachment = &winstate->attachments[attachment_index][winstate->frame_index];
    LaharAttachmentConfig* conf = &winstate->attachment_configs[attachment_index];

    if (attachment->layout != layout) {
        VkImageMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = __lahar_access_mask(attachment->layout),
            .dstAccessMask = __lahar_access_mask(attachment->layout),
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
        VkPipelineStageFlags dstStage = __lahar_pipeline_stage(attachment->layout);

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

uint32_t lahar_window_wait_inactive(Lahar* lahar, LaharWindow* window) {
    LaharWindowState* winstate = lahar_window_state(lahar, window);
    if (!winstate) { return LAHAR_ERR_INVALID_WINDOW; }

    if ((lahar->vkresult = vkWaitForFences(lahar->device, winstate->max_in_flight, winstate->in_flight, VK_TRUE, UINT64_MAX)) != VK_SUCCESS) {
        return LAHAR_ERR_VK_ERR;   
    }

    return LAHAR_ERR_SUCCESS;
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

#define lahar_load(lahar, name) name = (PFN_##name)loadfn(lahar, #name)

static uint32_t lahar_load_loader(Lahar* lahar, LaharLoaderFunc loadfn) {
/* LAHAR_VK_LOAD_LOADER */
#if defined(VK_VERSION_1_0)
    lahar_load(lahar, vkCreateInstance);
    lahar_load(lahar, vkEnumerateInstanceExtensionProperties);
    lahar_load(lahar, vkEnumerateInstanceLayerProperties);
#endif /* defined(VK_VERSION_1_0) */
#if defined(VK_VERSION_1_1)
    lahar_load(lahar, vkEnumerateInstanceVersion);
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

    lahar_load(lahar, vkGetInstanceProcAddr);
    if (!vkGetInstanceProcAddr) { return LAHAR_ERR_LOAD_FAILURE; }

    return LAHAR_ERR_SUCCESS;
}

static uint32_t lahar_load_instance(Lahar* lahar, LaharLoaderFunc loadfn) {
/* LAHAR_VK_LOAD_INSTANCE */
#if defined(VK_VERSION_1_0)
    lahar_load(lahar, vkCreateDevice);
    lahar_load(lahar, vkDestroyInstance);
    lahar_load(lahar, vkEnumerateDeviceExtensionProperties);
    lahar_load(lahar, vkEnumerateDeviceLayerProperties);
    lahar_load(lahar, vkEnumeratePhysicalDevices);
    lahar_load(lahar, vkGetDeviceProcAddr);
    lahar_load(lahar, vkGetPhysicalDeviceFeatures);
    lahar_load(lahar, vkGetPhysicalDeviceFormatProperties);
    lahar_load(lahar, vkGetPhysicalDeviceImageFormatProperties);
    lahar_load(lahar, vkGetPhysicalDeviceMemoryProperties);
    lahar_load(lahar, vkGetPhysicalDeviceProperties);
    lahar_load(lahar, vkGetPhysicalDeviceQueueFamilyProperties);
    lahar_load(lahar, vkGetPhysicalDeviceSparseImageFormatProperties);
#endif /* defined(VK_VERSION_1_0) */
#if defined(VK_VERSION_1_1)
    lahar_load(lahar, vkEnumeratePhysicalDeviceGroups);
    lahar_load(lahar, vkGetPhysicalDeviceExternalBufferProperties);
    lahar_load(lahar, vkGetPhysicalDeviceExternalFenceProperties);
    lahar_load(lahar, vkGetPhysicalDeviceExternalSemaphoreProperties);
    lahar_load(lahar, vkGetPhysicalDeviceFeatures2);
    lahar_load(lahar, vkGetPhysicalDeviceFormatProperties2);
    lahar_load(lahar, vkGetPhysicalDeviceImageFormatProperties2);
    lahar_load(lahar, vkGetPhysicalDeviceMemoryProperties2);
    lahar_load(lahar, vkGetPhysicalDeviceProperties2);
    lahar_load(lahar, vkGetPhysicalDeviceQueueFamilyProperties2);
    lahar_load(lahar, vkGetPhysicalDeviceSparseImageFormatProperties2);
#endif /* defined(VK_VERSION_1_1) */
#if defined(VK_VERSION_1_3)
    lahar_load(lahar, vkGetPhysicalDeviceToolProperties);
#endif /* defined(VK_VERSION_1_3) */
#if defined(VK_ARM_data_graph)
    lahar_load(lahar, vkGetPhysicalDeviceQueueFamilyDataGraphProcessingEnginePropertiesARM);
    lahar_load(lahar, vkGetPhysicalDeviceQueueFamilyDataGraphPropertiesARM);
#endif /* defined(VK_ARM_data_graph) */
#if defined(VK_ARM_tensors)
    lahar_load(lahar, vkGetPhysicalDeviceExternalTensorPropertiesARM);
#endif /* defined(VK_ARM_tensors) */
#if defined(VK_EXT_acquire_drm_display)
    lahar_load(lahar, vkAcquireDrmDisplayEXT);
    lahar_load(lahar, vkGetDrmDisplayEXT);
#endif /* defined(VK_EXT_acquire_drm_display) */
#if defined(VK_EXT_acquire_xlib_display)
    lahar_load(lahar, vkAcquireXlibDisplayEXT);
    lahar_load(lahar, vkGetRandROutputDisplayEXT);
#endif /* defined(VK_EXT_acquire_xlib_display) */
#if defined(VK_EXT_calibrated_timestamps)
    lahar_load(lahar, vkGetPhysicalDeviceCalibrateableTimeDomainsEXT);
#endif /* defined(VK_EXT_calibrated_timestamps) */
#if defined(VK_EXT_debug_report)
    lahar_load(lahar, vkCreateDebugReportCallbackEXT);
    lahar_load(lahar, vkDebugReportMessageEXT);
    lahar_load(lahar, vkDestroyDebugReportCallbackEXT);
#endif /* defined(VK_EXT_debug_report) */
#if defined(VK_EXT_debug_utils)
    lahar_load(lahar, vkCmdBeginDebugUtilsLabelEXT);
    lahar_load(lahar, vkCmdEndDebugUtilsLabelEXT);
    lahar_load(lahar, vkCmdInsertDebugUtilsLabelEXT);
    lahar_load(lahar, vkCreateDebugUtilsMessengerEXT);
    lahar_load(lahar, vkDestroyDebugUtilsMessengerEXT);
    lahar_load(lahar, vkQueueBeginDebugUtilsLabelEXT);
    lahar_load(lahar, vkQueueEndDebugUtilsLabelEXT);
    lahar_load(lahar, vkQueueInsertDebugUtilsLabelEXT);
    lahar_load(lahar, vkSetDebugUtilsObjectNameEXT);
    lahar_load(lahar, vkSetDebugUtilsObjectTagEXT);
    lahar_load(lahar, vkSubmitDebugUtilsMessageEXT);
#endif /* defined(VK_EXT_debug_utils) */
#if defined(VK_EXT_direct_mode_display)
    lahar_load(lahar, vkReleaseDisplayEXT);
#endif /* defined(VK_EXT_direct_mode_display) */
#if defined(VK_EXT_directfb_surface)
    lahar_load(lahar, vkCreateDirectFBSurfaceEXT);
    lahar_load(lahar, vkGetPhysicalDeviceDirectFBPresentationSupportEXT);
#endif /* defined(VK_EXT_directfb_surface) */
#if defined(VK_EXT_display_surface_counter)
    lahar_load(lahar, vkGetPhysicalDeviceSurfaceCapabilities2EXT);
#endif /* defined(VK_EXT_display_surface_counter) */
#if defined(VK_EXT_full_screen_exclusive)
    lahar_load(lahar, vkGetPhysicalDeviceSurfacePresentModes2EXT);
#endif /* defined(VK_EXT_full_screen_exclusive) */
#if defined(VK_EXT_headless_surface)
    lahar_load(lahar, vkCreateHeadlessSurfaceEXT);
#endif /* defined(VK_EXT_headless_surface) */
#if defined(VK_EXT_metal_surface)
    lahar_load(lahar, vkCreateMetalSurfaceEXT);
#endif /* defined(VK_EXT_metal_surface) */
#if defined(VK_EXT_sample_locations)
    lahar_load(lahar, vkGetPhysicalDeviceMultisamplePropertiesEXT);
#endif /* defined(VK_EXT_sample_locations) */
#if defined(VK_EXT_tooling_info)
    lahar_load(lahar, vkGetPhysicalDeviceToolPropertiesEXT);
#endif /* defined(VK_EXT_tooling_info) */
#if defined(VK_FUCHSIA_imagepipe_surface)
    lahar_load(lahar, vkCreateImagePipeSurfaceFUCHSIA);
#endif /* defined(VK_FUCHSIA_imagepipe_surface) */
#if defined(VK_GGP_stream_descriptor_surface)
    lahar_load(lahar, vkCreateStreamDescriptorSurfaceGGP);
#endif /* defined(VK_GGP_stream_descriptor_surface) */
#if defined(VK_KHR_android_surface)
    lahar_load(lahar, vkCreateAndroidSurfaceKHR);
#endif /* defined(VK_KHR_android_surface) */
#if defined(VK_KHR_calibrated_timestamps)
    lahar_load(lahar, vkGetPhysicalDeviceCalibrateableTimeDomainsKHR);
#endif /* defined(VK_KHR_calibrated_timestamps) */
#if defined(VK_KHR_cooperative_matrix)
    lahar_load(lahar, vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR);
#endif /* defined(VK_KHR_cooperative_matrix) */
#if defined(VK_KHR_device_group_creation)
    lahar_load(lahar, vkEnumeratePhysicalDeviceGroupsKHR);
#endif /* defined(VK_KHR_device_group_creation) */
#if defined(VK_KHR_display)
    lahar_load(lahar, vkCreateDisplayModeKHR);
    lahar_load(lahar, vkCreateDisplayPlaneSurfaceKHR);
    lahar_load(lahar, vkGetDisplayModePropertiesKHR);
    lahar_load(lahar, vkGetDisplayPlaneCapabilitiesKHR);
    lahar_load(lahar, vkGetDisplayPlaneSupportedDisplaysKHR);
    lahar_load(lahar, vkGetPhysicalDeviceDisplayPlanePropertiesKHR);
    lahar_load(lahar, vkGetPhysicalDeviceDisplayPropertiesKHR);
#endif /* defined(VK_KHR_display) */
#if defined(VK_KHR_external_fence_capabilities)
    lahar_load(lahar, vkGetPhysicalDeviceExternalFencePropertiesKHR);
#endif /* defined(VK_KHR_external_fence_capabilities) */
#if defined(VK_KHR_external_memory_capabilities)
    lahar_load(lahar, vkGetPhysicalDeviceExternalBufferPropertiesKHR);
#endif /* defined(VK_KHR_external_memory_capabilities) */
#if defined(VK_KHR_external_semaphore_capabilities)
    lahar_load(lahar, vkGetPhysicalDeviceExternalSemaphorePropertiesKHR);
#endif /* defined(VK_KHR_external_semaphore_capabilities) */
#if defined(VK_KHR_fragment_shading_rate)
    lahar_load(lahar, vkGetPhysicalDeviceFragmentShadingRatesKHR);
#endif /* defined(VK_KHR_fragment_shading_rate) */
#if defined(VK_KHR_get_display_properties2)
    lahar_load(lahar, vkGetDisplayModeProperties2KHR);
    lahar_load(lahar, vkGetDisplayPlaneCapabilities2KHR);
    lahar_load(lahar, vkGetPhysicalDeviceDisplayPlaneProperties2KHR);
    lahar_load(lahar, vkGetPhysicalDeviceDisplayProperties2KHR);
#endif /* defined(VK_KHR_get_display_properties2) */
#if defined(VK_KHR_get_physical_device_properties2)
    lahar_load(lahar, vkGetPhysicalDeviceFeatures2KHR);
    lahar_load(lahar, vkGetPhysicalDeviceFormatProperties2KHR);
    lahar_load(lahar, vkGetPhysicalDeviceImageFormatProperties2KHR);
    lahar_load(lahar, vkGetPhysicalDeviceMemoryProperties2KHR);
    lahar_load(lahar, vkGetPhysicalDeviceProperties2KHR);
    lahar_load(lahar, vkGetPhysicalDeviceQueueFamilyProperties2KHR);
    lahar_load(lahar, vkGetPhysicalDeviceSparseImageFormatProperties2KHR);
#endif /* defined(VK_KHR_get_physical_device_properties2) */
#if defined(VK_KHR_get_surface_capabilities2)
    lahar_load(lahar, vkGetPhysicalDeviceSurfaceCapabilities2KHR);
    lahar_load(lahar, vkGetPhysicalDeviceSurfaceFormats2KHR);
#endif /* defined(VK_KHR_get_surface_capabilities2) */
#if defined(VK_KHR_performance_query)
    lahar_load(lahar, vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR);
    lahar_load(lahar, vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR);
#endif /* defined(VK_KHR_performance_query) */
#if defined(VK_KHR_surface)
    lahar_load(lahar, vkDestroySurfaceKHR);
    lahar_load(lahar, vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    lahar_load(lahar, vkGetPhysicalDeviceSurfaceFormatsKHR);
    lahar_load(lahar, vkGetPhysicalDeviceSurfacePresentModesKHR);
    lahar_load(lahar, vkGetPhysicalDeviceSurfaceSupportKHR);
#endif /* defined(VK_KHR_surface) */
#if defined(VK_KHR_video_encode_queue)
    lahar_load(lahar, vkGetPhysicalDeviceVideoEncodeQualityLevelPropertiesKHR);
#endif /* defined(VK_KHR_video_encode_queue) */
#if defined(VK_KHR_video_queue)
    lahar_load(lahar, vkGetPhysicalDeviceVideoCapabilitiesKHR);
    lahar_load(lahar, vkGetPhysicalDeviceVideoFormatPropertiesKHR);
#endif /* defined(VK_KHR_video_queue) */
#if defined(VK_KHR_wayland_surface)
    lahar_load(lahar, vkCreateWaylandSurfaceKHR);
    lahar_load(lahar, vkGetPhysicalDeviceWaylandPresentationSupportKHR);
#endif /* defined(VK_KHR_wayland_surface) */
#if defined(VK_KHR_win32_surface)
    lahar_load(lahar, vkCreateWin32SurfaceKHR);
    lahar_load(lahar, vkGetPhysicalDeviceWin32PresentationSupportKHR);
#endif /* defined(VK_KHR_win32_surface) */
#if defined(VK_KHR_xcb_surface)
    lahar_load(lahar, vkCreateXcbSurfaceKHR);
    lahar_load(lahar, vkGetPhysicalDeviceXcbPresentationSupportKHR);
#endif /* defined(VK_KHR_xcb_surface) */
#if defined(VK_KHR_xlib_surface)
    lahar_load(lahar, vkCreateXlibSurfaceKHR);
    lahar_load(lahar, vkGetPhysicalDeviceXlibPresentationSupportKHR);
#endif /* defined(VK_KHR_xlib_surface) */
#if defined(VK_MVK_ios_surface)
    lahar_load(lahar, vkCreateIOSSurfaceMVK);
#endif /* defined(VK_MVK_ios_surface) */
#if defined(VK_MVK_macos_surface)
    lahar_load(lahar, vkCreateMacOSSurfaceMVK);
#endif /* defined(VK_MVK_macos_surface) */
#if defined(VK_NN_vi_surface)
    lahar_load(lahar, vkCreateViSurfaceNN);
#endif /* defined(VK_NN_vi_surface) */
#if defined(VK_NV_acquire_winrt_display)
    lahar_load(lahar, vkAcquireWinrtDisplayNV);
    lahar_load(lahar, vkGetWinrtDisplayNV);
#endif /* defined(VK_NV_acquire_winrt_display) */
#if defined(VK_NV_cooperative_matrix)
    lahar_load(lahar, vkGetPhysicalDeviceCooperativeMatrixPropertiesNV);
#endif /* defined(VK_NV_cooperative_matrix) */
#if defined(VK_NV_cooperative_matrix2)
    lahar_load(lahar, vkGetPhysicalDeviceCooperativeMatrixFlexibleDimensionsPropertiesNV);
#endif /* defined(VK_NV_cooperative_matrix2) */
#if defined(VK_NV_cooperative_vector)
    lahar_load(lahar, vkGetPhysicalDeviceCooperativeVectorPropertiesNV);
#endif /* defined(VK_NV_cooperative_vector) */
#if defined(VK_NV_coverage_reduction_mode)
    lahar_load(lahar, vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV);
#endif /* defined(VK_NV_coverage_reduction_mode) */
#if defined(VK_NV_external_memory_capabilities)
    lahar_load(lahar, vkGetPhysicalDeviceExternalImageFormatPropertiesNV);
#endif /* defined(VK_NV_external_memory_capabilities) */
#if defined(VK_NV_optical_flow)
    lahar_load(lahar, vkGetPhysicalDeviceOpticalFlowImageFormatsNV);
#endif /* defined(VK_NV_optical_flow) */
#if defined(VK_OHOS_surface)
    lahar_load(lahar, vkCreateSurfaceOHOS);
#endif /* defined(VK_OHOS_surface) */
#if defined(VK_QNX_screen_surface)
    lahar_load(lahar, vkCreateScreenSurfaceQNX);
    lahar_load(lahar, vkGetPhysicalDeviceScreenPresentationSupportQNX);
#endif /* defined(VK_QNX_screen_surface) */
#if (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1))
    lahar_load(lahar, vkGetPhysicalDevicePresentRectanglesKHR);
#endif /* (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1)) */
/* LAHAR_VK_LOAD_INSTANCE */

    return LAHAR_ERR_SUCCESS; 
}

static uint32_t lahar_load_device(Lahar* lahar, LaharLoaderFunc loadfn) {
/* LAHAR_VK_LOAD_DEVICE */
#if defined(VK_VERSION_1_0)
    lahar_load(lahar, vkAllocateCommandBuffers);
    lahar_load(lahar, vkAllocateDescriptorSets);
    lahar_load(lahar, vkAllocateMemory);
    lahar_load(lahar, vkBeginCommandBuffer);
    lahar_load(lahar, vkBindBufferMemory);
    lahar_load(lahar, vkBindImageMemory);
    lahar_load(lahar, vkCmdBeginQuery);
    lahar_load(lahar, vkCmdBeginRenderPass);
    lahar_load(lahar, vkCmdBindDescriptorSets);
    lahar_load(lahar, vkCmdBindIndexBuffer);
    lahar_load(lahar, vkCmdBindPipeline);
    lahar_load(lahar, vkCmdBindVertexBuffers);
    lahar_load(lahar, vkCmdBlitImage);
    lahar_load(lahar, vkCmdClearAttachments);
    lahar_load(lahar, vkCmdClearColorImage);
    lahar_load(lahar, vkCmdClearDepthStencilImage);
    lahar_load(lahar, vkCmdCopyBuffer);
    lahar_load(lahar, vkCmdCopyBufferToImage);
    lahar_load(lahar, vkCmdCopyImage);
    lahar_load(lahar, vkCmdCopyImageToBuffer);
    lahar_load(lahar, vkCmdCopyQueryPoolResults);
    lahar_load(lahar, vkCmdDispatch);
    lahar_load(lahar, vkCmdDispatchIndirect);
    lahar_load(lahar, vkCmdDraw);
    lahar_load(lahar, vkCmdDrawIndexed);
    lahar_load(lahar, vkCmdDrawIndexedIndirect);
    lahar_load(lahar, vkCmdDrawIndirect);
    lahar_load(lahar, vkCmdEndQuery);
    lahar_load(lahar, vkCmdEndRenderPass);
    lahar_load(lahar, vkCmdExecuteCommands);
    lahar_load(lahar, vkCmdFillBuffer);
    lahar_load(lahar, vkCmdNextSubpass);
    lahar_load(lahar, vkCmdPipelineBarrier);
    lahar_load(lahar, vkCmdPushConstants);
    lahar_load(lahar, vkCmdResetEvent);
    lahar_load(lahar, vkCmdResetQueryPool);
    lahar_load(lahar, vkCmdResolveImage);
    lahar_load(lahar, vkCmdSetBlendConstants);
    lahar_load(lahar, vkCmdSetDepthBias);
    lahar_load(lahar, vkCmdSetDepthBounds);
    lahar_load(lahar, vkCmdSetEvent);
    lahar_load(lahar, vkCmdSetLineWidth);
    lahar_load(lahar, vkCmdSetScissor);
    lahar_load(lahar, vkCmdSetStencilCompareMask);
    lahar_load(lahar, vkCmdSetStencilReference);
    lahar_load(lahar, vkCmdSetStencilWriteMask);
    lahar_load(lahar, vkCmdSetViewport);
    lahar_load(lahar, vkCmdUpdateBuffer);
    lahar_load(lahar, vkCmdWaitEvents);
    lahar_load(lahar, vkCmdWriteTimestamp);
    lahar_load(lahar, vkCreateBuffer);
    lahar_load(lahar, vkCreateBufferView);
    lahar_load(lahar, vkCreateCommandPool);
    lahar_load(lahar, vkCreateComputePipelines);
    lahar_load(lahar, vkCreateDescriptorPool);
    lahar_load(lahar, vkCreateDescriptorSetLayout);
    lahar_load(lahar, vkCreateEvent);
    lahar_load(lahar, vkCreateFence);
    lahar_load(lahar, vkCreateFramebuffer);
    lahar_load(lahar, vkCreateGraphicsPipelines);
    lahar_load(lahar, vkCreateImage);
    lahar_load(lahar, vkCreateImageView);
    lahar_load(lahar, vkCreatePipelineCache);
    lahar_load(lahar, vkCreatePipelineLayout);
    lahar_load(lahar, vkCreateQueryPool);
    lahar_load(lahar, vkCreateRenderPass);
    lahar_load(lahar, vkCreateSampler);
    lahar_load(lahar, vkCreateSemaphore);
    lahar_load(lahar, vkCreateShaderModule);
    lahar_load(lahar, vkDestroyBuffer);
    lahar_load(lahar, vkDestroyBufferView);
    lahar_load(lahar, vkDestroyCommandPool);
    lahar_load(lahar, vkDestroyDescriptorPool);
    lahar_load(lahar, vkDestroyDescriptorSetLayout);
    lahar_load(lahar, vkDestroyDevice);
    lahar_load(lahar, vkDestroyEvent);
    lahar_load(lahar, vkDestroyFence);
    lahar_load(lahar, vkDestroyFramebuffer);
    lahar_load(lahar, vkDestroyImage);
    lahar_load(lahar, vkDestroyImageView);
    lahar_load(lahar, vkDestroyPipeline);
    lahar_load(lahar, vkDestroyPipelineCache);
    lahar_load(lahar, vkDestroyPipelineLayout);
    lahar_load(lahar, vkDestroyQueryPool);
    lahar_load(lahar, vkDestroyRenderPass);
    lahar_load(lahar, vkDestroySampler);
    lahar_load(lahar, vkDestroySemaphore);
    lahar_load(lahar, vkDestroyShaderModule);
    lahar_load(lahar, vkDeviceWaitIdle);
    lahar_load(lahar, vkEndCommandBuffer);
    lahar_load(lahar, vkFlushMappedMemoryRanges);
    lahar_load(lahar, vkFreeCommandBuffers);
    lahar_load(lahar, vkFreeDescriptorSets);
    lahar_load(lahar, vkFreeMemory);
    lahar_load(lahar, vkGetBufferMemoryRequirements);
    lahar_load(lahar, vkGetDeviceMemoryCommitment);
    lahar_load(lahar, vkGetDeviceQueue);
    lahar_load(lahar, vkGetEventStatus);
    lahar_load(lahar, vkGetFenceStatus);
    lahar_load(lahar, vkGetImageMemoryRequirements);
    lahar_load(lahar, vkGetImageSparseMemoryRequirements);
    lahar_load(lahar, vkGetImageSubresourceLayout);
    lahar_load(lahar, vkGetPipelineCacheData);
    lahar_load(lahar, vkGetQueryPoolResults);
    lahar_load(lahar, vkGetRenderAreaGranularity);
    lahar_load(lahar, vkInvalidateMappedMemoryRanges);
    lahar_load(lahar, vkMapMemory);
    lahar_load(lahar, vkMergePipelineCaches);
    lahar_load(lahar, vkQueueBindSparse);
    lahar_load(lahar, vkQueueSubmit);
    lahar_load(lahar, vkQueueWaitIdle);
    lahar_load(lahar, vkResetCommandBuffer);
    lahar_load(lahar, vkResetCommandPool);
    lahar_load(lahar, vkResetDescriptorPool);
    lahar_load(lahar, vkResetEvent);
    lahar_load(lahar, vkResetFences);
    lahar_load(lahar, vkSetEvent);
    lahar_load(lahar, vkUnmapMemory);
    lahar_load(lahar, vkUpdateDescriptorSets);
    lahar_load(lahar, vkWaitForFences);
#endif /* defined(VK_VERSION_1_0) */
#if defined(VK_VERSION_1_1)
    lahar_load(lahar, vkBindBufferMemory2);
    lahar_load(lahar, vkBindImageMemory2);
    lahar_load(lahar, vkCmdDispatchBase);
    lahar_load(lahar, vkCmdSetDeviceMask);
    lahar_load(lahar, vkCreateDescriptorUpdateTemplate);
    lahar_load(lahar, vkCreateSamplerYcbcrConversion);
    lahar_load(lahar, vkDestroyDescriptorUpdateTemplate);
    lahar_load(lahar, vkDestroySamplerYcbcrConversion);
    lahar_load(lahar, vkGetBufferMemoryRequirements2);
    lahar_load(lahar, vkGetDescriptorSetLayoutSupport);
    lahar_load(lahar, vkGetDeviceGroupPeerMemoryFeatures);
    lahar_load(lahar, vkGetDeviceQueue2);
    lahar_load(lahar, vkGetImageMemoryRequirements2);
    lahar_load(lahar, vkGetImageSparseMemoryRequirements2);
    lahar_load(lahar, vkTrimCommandPool);
    lahar_load(lahar, vkUpdateDescriptorSetWithTemplate);
#endif /* defined(VK_VERSION_1_1) */
#if defined(VK_VERSION_1_2)
    lahar_load(lahar, vkCmdBeginRenderPass2);
    lahar_load(lahar, vkCmdDrawIndexedIndirectCount);
    lahar_load(lahar, vkCmdDrawIndirectCount);
    lahar_load(lahar, vkCmdEndRenderPass2);
    lahar_load(lahar, vkCmdNextSubpass2);
    lahar_load(lahar, vkCreateRenderPass2);
    lahar_load(lahar, vkGetBufferDeviceAddress);
    lahar_load(lahar, vkGetBufferOpaqueCaptureAddress);
    lahar_load(lahar, vkGetDeviceMemoryOpaqueCaptureAddress);
    lahar_load(lahar, vkGetSemaphoreCounterValue);
    lahar_load(lahar, vkResetQueryPool);
    lahar_load(lahar, vkSignalSemaphore);
    lahar_load(lahar, vkWaitSemaphores);
#endif /* defined(VK_VERSION_1_2) */
#if defined(VK_VERSION_1_3)
    lahar_load(lahar, vkCmdBeginRendering);
    lahar_load(lahar, vkCmdBindVertexBuffers2);
    lahar_load(lahar, vkCmdBlitImage2);
    lahar_load(lahar, vkCmdCopyBuffer2);
    lahar_load(lahar, vkCmdCopyBufferToImage2);
    lahar_load(lahar, vkCmdCopyImage2);
    lahar_load(lahar, vkCmdCopyImageToBuffer2);
    lahar_load(lahar, vkCmdEndRendering);
    lahar_load(lahar, vkCmdPipelineBarrier2);
    lahar_load(lahar, vkCmdResetEvent2);
    lahar_load(lahar, vkCmdResolveImage2);
    lahar_load(lahar, vkCmdSetCullMode);
    lahar_load(lahar, vkCmdSetDepthBiasEnable);
    lahar_load(lahar, vkCmdSetDepthBoundsTestEnable);
    lahar_load(lahar, vkCmdSetDepthCompareOp);
    lahar_load(lahar, vkCmdSetDepthTestEnable);
    lahar_load(lahar, vkCmdSetDepthWriteEnable);
    lahar_load(lahar, vkCmdSetEvent2);
    lahar_load(lahar, vkCmdSetFrontFace);
    lahar_load(lahar, vkCmdSetPrimitiveRestartEnable);
    lahar_load(lahar, vkCmdSetPrimitiveTopology);
    lahar_load(lahar, vkCmdSetRasterizerDiscardEnable);
    lahar_load(lahar, vkCmdSetScissorWithCount);
    lahar_load(lahar, vkCmdSetStencilOp);
    lahar_load(lahar, vkCmdSetStencilTestEnable);
    lahar_load(lahar, vkCmdSetViewportWithCount);
    lahar_load(lahar, vkCmdWaitEvents2);
    lahar_load(lahar, vkCmdWriteTimestamp2);
    lahar_load(lahar, vkCreatePrivateDataSlot);
    lahar_load(lahar, vkDestroyPrivateDataSlot);
    lahar_load(lahar, vkGetDeviceBufferMemoryRequirements);
    lahar_load(lahar, vkGetDeviceImageMemoryRequirements);
    lahar_load(lahar, vkGetDeviceImageSparseMemoryRequirements);
    lahar_load(lahar, vkGetPrivateData);
    lahar_load(lahar, vkQueueSubmit2);
    lahar_load(lahar, vkSetPrivateData);
#endif /* defined(VK_VERSION_1_3) */
#if defined(VK_VERSION_1_4)
    lahar_load(lahar, vkCmdBindDescriptorSets2);
    lahar_load(lahar, vkCmdBindIndexBuffer2);
    lahar_load(lahar, vkCmdPushConstants2);
    lahar_load(lahar, vkCmdPushDescriptorSet);
    lahar_load(lahar, vkCmdPushDescriptorSet2);
    lahar_load(lahar, vkCmdPushDescriptorSetWithTemplate);
    lahar_load(lahar, vkCmdPushDescriptorSetWithTemplate2);
    lahar_load(lahar, vkCmdSetLineStipple);
    lahar_load(lahar, vkCmdSetRenderingAttachmentLocations);
    lahar_load(lahar, vkCmdSetRenderingInputAttachmentIndices);
    lahar_load(lahar, vkCopyImageToImage);
    lahar_load(lahar, vkCopyImageToMemory);
    lahar_load(lahar, vkCopyMemoryToImage);
    lahar_load(lahar, vkGetDeviceImageSubresourceLayout);
    lahar_load(lahar, vkGetImageSubresourceLayout2);
    lahar_load(lahar, vkGetRenderingAreaGranularity);
    lahar_load(lahar, vkMapMemory2);
    lahar_load(lahar, vkTransitionImageLayout);
    lahar_load(lahar, vkUnmapMemory2);
#endif /* defined(VK_VERSION_1_4) */
#if defined(VK_AMDX_shader_enqueue)
    lahar_load(lahar, vkCmdDispatchGraphAMDX);
    lahar_load(lahar, vkCmdDispatchGraphIndirectAMDX);
    lahar_load(lahar, vkCmdDispatchGraphIndirectCountAMDX);
    lahar_load(lahar, vkCmdInitializeGraphScratchMemoryAMDX);
    lahar_load(lahar, vkCreateExecutionGraphPipelinesAMDX);
    lahar_load(lahar, vkGetExecutionGraphPipelineNodeIndexAMDX);
    lahar_load(lahar, vkGetExecutionGraphPipelineScratchSizeAMDX);
#endif /* defined(VK_AMDX_shader_enqueue) */
#if defined(VK_AMD_anti_lag)
    lahar_load(lahar, vkAntiLagUpdateAMD);
#endif /* defined(VK_AMD_anti_lag) */
#if defined(VK_AMD_buffer_marker)
    lahar_load(lahar, vkCmdWriteBufferMarkerAMD);
#endif /* defined(VK_AMD_buffer_marker) */
#if defined(VK_AMD_buffer_marker) && (defined(VK_VERSION_1_3) || defined(VK_KHR_synchronization2))
    lahar_load(lahar, vkCmdWriteBufferMarker2AMD);
#endif /* defined(VK_AMD_buffer_marker) && (defined(VK_VERSION_1_3) || defined(VK_KHR_synchronization2)) */
#if defined(VK_AMD_display_native_hdr)
    lahar_load(lahar, vkSetLocalDimmingAMD);
#endif /* defined(VK_AMD_display_native_hdr) */
#if defined(VK_AMD_draw_indirect_count)
    lahar_load(lahar, vkCmdDrawIndexedIndirectCountAMD);
    lahar_load(lahar, vkCmdDrawIndirectCountAMD);
#endif /* defined(VK_AMD_draw_indirect_count) */
#if defined(VK_AMD_shader_info)
    lahar_load(lahar, vkGetShaderInfoAMD);
#endif /* defined(VK_AMD_shader_info) */
#if defined(VK_ANDROID_external_memory_android_hardware_buffer)
    lahar_load(lahar, vkGetAndroidHardwareBufferPropertiesANDROID);
    lahar_load(lahar, vkGetMemoryAndroidHardwareBufferANDROID);
#endif /* defined(VK_ANDROID_external_memory_android_hardware_buffer) */
#if defined(VK_ARM_data_graph)
    lahar_load(lahar, vkBindDataGraphPipelineSessionMemoryARM);
    lahar_load(lahar, vkCmdDispatchDataGraphARM);
    lahar_load(lahar, vkCreateDataGraphPipelineSessionARM);
    lahar_load(lahar, vkCreateDataGraphPipelinesARM);
    lahar_load(lahar, vkDestroyDataGraphPipelineSessionARM);
    lahar_load(lahar, vkGetDataGraphPipelineAvailablePropertiesARM);
    lahar_load(lahar, vkGetDataGraphPipelinePropertiesARM);
    lahar_load(lahar, vkGetDataGraphPipelineSessionBindPointRequirementsARM);
    lahar_load(lahar, vkGetDataGraphPipelineSessionMemoryRequirementsARM);
#endif /* defined(VK_ARM_data_graph) */
#if defined(VK_ARM_tensors)
    lahar_load(lahar, vkBindTensorMemoryARM);
    lahar_load(lahar, vkCmdCopyTensorARM);
    lahar_load(lahar, vkCreateTensorARM);
    lahar_load(lahar, vkCreateTensorViewARM);
    lahar_load(lahar, vkDestroyTensorARM);
    lahar_load(lahar, vkDestroyTensorViewARM);
    lahar_load(lahar, vkGetDeviceTensorMemoryRequirementsARM);
    lahar_load(lahar, vkGetTensorMemoryRequirementsARM);
#endif /* defined(VK_ARM_tensors) */
#if defined(VK_ARM_tensors) && defined(VK_EXT_descriptor_buffer)
    lahar_load(lahar, vkGetTensorOpaqueCaptureDescriptorDataARM);
    lahar_load(lahar, vkGetTensorViewOpaqueCaptureDescriptorDataARM);
#endif /* defined(VK_ARM_tensors) && defined(VK_EXT_descriptor_buffer) */
#if defined(VK_EXT_attachment_feedback_loop_dynamic_state)
    lahar_load(lahar, vkCmdSetAttachmentFeedbackLoopEnableEXT);
#endif /* defined(VK_EXT_attachment_feedback_loop_dynamic_state) */
#if defined(VK_EXT_buffer_device_address)
    lahar_load(lahar, vkGetBufferDeviceAddressEXT);
#endif /* defined(VK_EXT_buffer_device_address) */
#if defined(VK_EXT_calibrated_timestamps)
    lahar_load(lahar, vkGetCalibratedTimestampsEXT);
#endif /* defined(VK_EXT_calibrated_timestamps) */
#if defined(VK_EXT_color_write_enable)
    lahar_load(lahar, vkCmdSetColorWriteEnableEXT);
#endif /* defined(VK_EXT_color_write_enable) */
#if defined(VK_EXT_conditional_rendering)
    lahar_load(lahar, vkCmdBeginConditionalRenderingEXT);
    lahar_load(lahar, vkCmdEndConditionalRenderingEXT);
#endif /* defined(VK_EXT_conditional_rendering) */
#if defined(VK_EXT_debug_marker)
    lahar_load(lahar, vkCmdDebugMarkerBeginEXT);
    lahar_load(lahar, vkCmdDebugMarkerEndEXT);
    lahar_load(lahar, vkCmdDebugMarkerInsertEXT);
    lahar_load(lahar, vkDebugMarkerSetObjectNameEXT);
    lahar_load(lahar, vkDebugMarkerSetObjectTagEXT);
#endif /* defined(VK_EXT_debug_marker) */
#if defined(VK_EXT_depth_bias_control)
    lahar_load(lahar, vkCmdSetDepthBias2EXT);
#endif /* defined(VK_EXT_depth_bias_control) */
#if defined(VK_EXT_descriptor_buffer)
    lahar_load(lahar, vkCmdBindDescriptorBufferEmbeddedSamplersEXT);
    lahar_load(lahar, vkCmdBindDescriptorBuffersEXT);
    lahar_load(lahar, vkCmdSetDescriptorBufferOffsetsEXT);
    lahar_load(lahar, vkGetBufferOpaqueCaptureDescriptorDataEXT);
    lahar_load(lahar, vkGetDescriptorEXT);
    lahar_load(lahar, vkGetDescriptorSetLayoutBindingOffsetEXT);
    lahar_load(lahar, vkGetDescriptorSetLayoutSizeEXT);
    lahar_load(lahar, vkGetImageOpaqueCaptureDescriptorDataEXT);
    lahar_load(lahar, vkGetImageViewOpaqueCaptureDescriptorDataEXT);
    lahar_load(lahar, vkGetSamplerOpaqueCaptureDescriptorDataEXT);
#endif /* defined(VK_EXT_descriptor_buffer) */
#if defined(VK_EXT_descriptor_buffer) && (defined(VK_KHR_acceleration_structure) || defined(VK_NV_ray_tracing))
    lahar_load(lahar, vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT);
#endif /* defined(VK_EXT_descriptor_buffer) && (defined(VK_KHR_acceleration_structure) || defined(VK_NV_ray_tracing)) */
#if defined(VK_EXT_device_fault)
    lahar_load(lahar, vkGetDeviceFaultInfoEXT);
#endif /* defined(VK_EXT_device_fault) */
#if defined(VK_EXT_device_generated_commands)
    lahar_load(lahar, vkCmdExecuteGeneratedCommandsEXT);
    lahar_load(lahar, vkCmdPreprocessGeneratedCommandsEXT);
    lahar_load(lahar, vkCreateIndirectCommandsLayoutEXT);
    lahar_load(lahar, vkCreateIndirectExecutionSetEXT);
    lahar_load(lahar, vkDestroyIndirectCommandsLayoutEXT);
    lahar_load(lahar, vkDestroyIndirectExecutionSetEXT);
    lahar_load(lahar, vkGetGeneratedCommandsMemoryRequirementsEXT);
    lahar_load(lahar, vkUpdateIndirectExecutionSetPipelineEXT);
    lahar_load(lahar, vkUpdateIndirectExecutionSetShaderEXT);
#endif /* defined(VK_EXT_device_generated_commands) */
#if defined(VK_EXT_discard_rectangles)
    lahar_load(lahar, vkCmdSetDiscardRectangleEXT);
#endif /* defined(VK_EXT_discard_rectangles) */
#if defined(VK_EXT_discard_rectangles) && VK_EXT_DISCARD_RECTANGLES_SPEC_VERSION >= 2
    lahar_load(lahar, vkCmdSetDiscardRectangleEnableEXT);
    lahar_load(lahar, vkCmdSetDiscardRectangleModeEXT);
#endif /* defined(VK_EXT_discard_rectangles) && VK_EXT_DISCARD_RECTANGLES_SPEC_VERSION >= 2 */
#if defined(VK_EXT_display_control)
    lahar_load(lahar, vkDisplayPowerControlEXT);
    lahar_load(lahar, vkGetSwapchainCounterEXT);
    lahar_load(lahar, vkRegisterDeviceEventEXT);
    lahar_load(lahar, vkRegisterDisplayEventEXT);
#endif /* defined(VK_EXT_display_control) */
#if defined(VK_EXT_external_memory_host)
    lahar_load(lahar, vkGetMemoryHostPointerPropertiesEXT);
#endif /* defined(VK_EXT_external_memory_host) */
#if defined(VK_EXT_external_memory_metal)
    lahar_load(lahar, vkGetMemoryMetalHandleEXT);
    lahar_load(lahar, vkGetMemoryMetalHandlePropertiesEXT);
#endif /* defined(VK_EXT_external_memory_metal) */
#if defined(VK_EXT_fragment_density_map_offset)
    lahar_load(lahar, vkCmdEndRendering2EXT);
#endif /* defined(VK_EXT_fragment_density_map_offset) */
#if defined(VK_EXT_full_screen_exclusive)
    lahar_load(lahar, vkAcquireFullScreenExclusiveModeEXT);
    lahar_load(lahar, vkReleaseFullScreenExclusiveModeEXT);
#endif /* defined(VK_EXT_full_screen_exclusive) */
#if defined(VK_EXT_full_screen_exclusive) && (defined(VK_KHR_device_group) || defined(VK_VERSION_1_1))
    lahar_load(lahar, vkGetDeviceGroupSurfacePresentModes2EXT);
#endif /* defined(VK_EXT_full_screen_exclusive) && (defined(VK_KHR_device_group) || defined(VK_VERSION_1_1)) */
#if defined(VK_EXT_hdr_metadata)
    lahar_load(lahar, vkSetHdrMetadataEXT);
#endif /* defined(VK_EXT_hdr_metadata) */
#if defined(VK_EXT_host_image_copy)
    lahar_load(lahar, vkCopyImageToImageEXT);
    lahar_load(lahar, vkCopyImageToMemoryEXT);
    lahar_load(lahar, vkCopyMemoryToImageEXT);
    lahar_load(lahar, vkTransitionImageLayoutEXT);
#endif /* defined(VK_EXT_host_image_copy) */
#if defined(VK_EXT_host_query_reset)
    lahar_load(lahar, vkResetQueryPoolEXT);
#endif /* defined(VK_EXT_host_query_reset) */
#if defined(VK_EXT_image_drm_format_modifier)
    lahar_load(lahar, vkGetImageDrmFormatModifierPropertiesEXT);
#endif /* defined(VK_EXT_image_drm_format_modifier) */
#if defined(VK_EXT_line_rasterization)
    lahar_load(lahar, vkCmdSetLineStippleEXT);
#endif /* defined(VK_EXT_line_rasterization) */
#if defined(VK_EXT_mesh_shader)
    lahar_load(lahar, vkCmdDrawMeshTasksEXT);
    lahar_load(lahar, vkCmdDrawMeshTasksIndirectEXT);
#endif /* defined(VK_EXT_mesh_shader) */
#if defined(VK_EXT_mesh_shader) && (defined(VK_KHR_draw_indirect_count) || defined(VK_VERSION_1_2))
    lahar_load(lahar, vkCmdDrawMeshTasksIndirectCountEXT);
#endif /* defined(VK_EXT_mesh_shader) && (defined(VK_KHR_draw_indirect_count) || defined(VK_VERSION_1_2)) */
#if defined(VK_EXT_metal_objects)
    lahar_load(lahar, vkExportMetalObjectsEXT);
#endif /* defined(VK_EXT_metal_objects) */
#if defined(VK_EXT_multi_draw)
    lahar_load(lahar, vkCmdDrawMultiEXT);
    lahar_load(lahar, vkCmdDrawMultiIndexedEXT);
#endif /* defined(VK_EXT_multi_draw) */
#if defined(VK_EXT_opacity_micromap)
    lahar_load(lahar, vkBuildMicromapsEXT);
    lahar_load(lahar, vkCmdBuildMicromapsEXT);
    lahar_load(lahar, vkCmdCopyMemoryToMicromapEXT);
    lahar_load(lahar, vkCmdCopyMicromapEXT);
    lahar_load(lahar, vkCmdCopyMicromapToMemoryEXT);
    lahar_load(lahar, vkCmdWriteMicromapsPropertiesEXT);
    lahar_load(lahar, vkCopyMemoryToMicromapEXT);
    lahar_load(lahar, vkCopyMicromapEXT);
    lahar_load(lahar, vkCopyMicromapToMemoryEXT);
    lahar_load(lahar, vkCreateMicromapEXT);
    lahar_load(lahar, vkDestroyMicromapEXT);
    lahar_load(lahar, vkGetDeviceMicromapCompatibilityEXT);
    lahar_load(lahar, vkGetMicromapBuildSizesEXT);
    lahar_load(lahar, vkWriteMicromapsPropertiesEXT);
#endif /* defined(VK_EXT_opacity_micromap) */
#if defined(VK_EXT_pageable_device_local_memory)
    lahar_load(lahar, vkSetDeviceMemoryPriorityEXT);
#endif /* defined(VK_EXT_pageable_device_local_memory) */
#if defined(VK_EXT_pipeline_properties)
    lahar_load(lahar, vkGetPipelinePropertiesEXT);
#endif /* defined(VK_EXT_pipeline_properties) */
#if defined(VK_EXT_private_data)
    lahar_load(lahar, vkCreatePrivateDataSlotEXT);
    lahar_load(lahar, vkDestroyPrivateDataSlotEXT);
    lahar_load(lahar, vkGetPrivateDataEXT);
    lahar_load(lahar, vkSetPrivateDataEXT);
#endif /* defined(VK_EXT_private_data) */
#if defined(VK_EXT_sample_locations)
    lahar_load(lahar, vkCmdSetSampleLocationsEXT);
#endif /* defined(VK_EXT_sample_locations) */
#if defined(VK_EXT_shader_module_identifier)
    lahar_load(lahar, vkGetShaderModuleCreateInfoIdentifierEXT);
    lahar_load(lahar, vkGetShaderModuleIdentifierEXT);
#endif /* defined(VK_EXT_shader_module_identifier) */
#if defined(VK_EXT_shader_object)
    lahar_load(lahar, vkCmdBindShadersEXT);
    lahar_load(lahar, vkCreateShadersEXT);
    lahar_load(lahar, vkDestroyShaderEXT);
    lahar_load(lahar, vkGetShaderBinaryDataEXT);
#endif /* defined(VK_EXT_shader_object) */
#if defined(VK_EXT_swapchain_maintenance1)
    lahar_load(lahar, vkReleaseSwapchainImagesEXT);
#endif /* defined(VK_EXT_swapchain_maintenance1) */
#if defined(VK_EXT_transform_feedback)
    lahar_load(lahar, vkCmdBeginQueryIndexedEXT);
    lahar_load(lahar, vkCmdBeginTransformFeedbackEXT);
    lahar_load(lahar, vkCmdBindTransformFeedbackBuffersEXT);
    lahar_load(lahar, vkCmdDrawIndirectByteCountEXT);
    lahar_load(lahar, vkCmdEndQueryIndexedEXT);
    lahar_load(lahar, vkCmdEndTransformFeedbackEXT);
#endif /* defined(VK_EXT_transform_feedback) */
#if defined(VK_EXT_validation_cache)
    lahar_load(lahar, vkCreateValidationCacheEXT);
    lahar_load(lahar, vkDestroyValidationCacheEXT);
    lahar_load(lahar, vkGetValidationCacheDataEXT);
    lahar_load(lahar, vkMergeValidationCachesEXT);
#endif /* defined(VK_EXT_validation_cache) */
#if defined(VK_FUCHSIA_buffer_collection)
    lahar_load(lahar, vkCreateBufferCollectionFUCHSIA);
    lahar_load(lahar, vkDestroyBufferCollectionFUCHSIA);
    lahar_load(lahar, vkGetBufferCollectionPropertiesFUCHSIA);
    lahar_load(lahar, vkSetBufferCollectionBufferConstraintsFUCHSIA);
    lahar_load(lahar, vkSetBufferCollectionImageConstraintsFUCHSIA);
#endif /* defined(VK_FUCHSIA_buffer_collection) */
#if defined(VK_FUCHSIA_external_memory)
    lahar_load(lahar, vkGetMemoryZirconHandleFUCHSIA);
    lahar_load(lahar, vkGetMemoryZirconHandlePropertiesFUCHSIA);
#endif /* defined(VK_FUCHSIA_external_memory) */
#if defined(VK_FUCHSIA_external_semaphore)
    lahar_load(lahar, vkGetSemaphoreZirconHandleFUCHSIA);
    lahar_load(lahar, vkImportSemaphoreZirconHandleFUCHSIA);
#endif /* defined(VK_FUCHSIA_external_semaphore) */
#if defined(VK_GOOGLE_display_timing)
    lahar_load(lahar, vkGetPastPresentationTimingGOOGLE);
    lahar_load(lahar, vkGetRefreshCycleDurationGOOGLE);
#endif /* defined(VK_GOOGLE_display_timing) */
#if defined(VK_HUAWEI_cluster_culling_shader)
    lahar_load(lahar, vkCmdDrawClusterHUAWEI);
    lahar_load(lahar, vkCmdDrawClusterIndirectHUAWEI);
#endif /* defined(VK_HUAWEI_cluster_culling_shader) */
#if defined(VK_HUAWEI_invocation_mask)
    lahar_load(lahar, vkCmdBindInvocationMaskHUAWEI);
#endif /* defined(VK_HUAWEI_invocation_mask) */
#if defined(VK_HUAWEI_subpass_shading) && VK_HUAWEI_SUBPASS_SHADING_SPEC_VERSION >= 2
    lahar_load(lahar, vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI);
#endif /* defined(VK_HUAWEI_subpass_shading) && VK_HUAWEI_SUBPASS_SHADING_SPEC_VERSION >= 2 */
#if defined(VK_HUAWEI_subpass_shading)
    lahar_load(lahar, vkCmdSubpassShadingHUAWEI);
#endif /* defined(VK_HUAWEI_subpass_shading) */
#if defined(VK_INTEL_performance_query)
    lahar_load(lahar, vkAcquirePerformanceConfigurationINTEL);
    lahar_load(lahar, vkCmdSetPerformanceMarkerINTEL);
    lahar_load(lahar, vkCmdSetPerformanceOverrideINTEL);
    lahar_load(lahar, vkCmdSetPerformanceStreamMarkerINTEL);
    lahar_load(lahar, vkGetPerformanceParameterINTEL);
    lahar_load(lahar, vkInitializePerformanceApiINTEL);
    lahar_load(lahar, vkQueueSetPerformanceConfigurationINTEL);
    lahar_load(lahar, vkReleasePerformanceConfigurationINTEL);
    lahar_load(lahar, vkUninitializePerformanceApiINTEL);
#endif /* defined(VK_INTEL_performance_query) */
#if defined(VK_KHR_acceleration_structure)
    lahar_load(lahar, vkBuildAccelerationStructuresKHR);
    lahar_load(lahar, vkCmdBuildAccelerationStructuresIndirectKHR);
    lahar_load(lahar, vkCmdBuildAccelerationStructuresKHR);
    lahar_load(lahar, vkCmdCopyAccelerationStructureKHR);
    lahar_load(lahar, vkCmdCopyAccelerationStructureToMemoryKHR);
    lahar_load(lahar, vkCmdCopyMemoryToAccelerationStructureKHR);
    lahar_load(lahar, vkCmdWriteAccelerationStructuresPropertiesKHR);
    lahar_load(lahar, vkCopyAccelerationStructureKHR);
    lahar_load(lahar, vkCopyAccelerationStructureToMemoryKHR);
    lahar_load(lahar, vkCopyMemoryToAccelerationStructureKHR);
    lahar_load(lahar, vkCreateAccelerationStructureKHR);
    lahar_load(lahar, vkDestroyAccelerationStructureKHR);
    lahar_load(lahar, vkGetAccelerationStructureBuildSizesKHR);
    lahar_load(lahar, vkGetAccelerationStructureDeviceAddressKHR);
    lahar_load(lahar, vkGetDeviceAccelerationStructureCompatibilityKHR);
    lahar_load(lahar, vkWriteAccelerationStructuresPropertiesKHR);
#endif /* defined(VK_KHR_acceleration_structure) */
#if defined(VK_KHR_bind_memory2)
    lahar_load(lahar, vkBindBufferMemory2KHR);
    lahar_load(lahar, vkBindImageMemory2KHR);
#endif /* defined(VK_KHR_bind_memory2) */
#if defined(VK_KHR_buffer_device_address)
    lahar_load(lahar, vkGetBufferDeviceAddressKHR);
    lahar_load(lahar, vkGetBufferOpaqueCaptureAddressKHR);
    lahar_load(lahar, vkGetDeviceMemoryOpaqueCaptureAddressKHR);
#endif /* defined(VK_KHR_buffer_device_address) */
#if defined(VK_KHR_calibrated_timestamps)
    lahar_load(lahar, vkGetCalibratedTimestampsKHR);
#endif /* defined(VK_KHR_calibrated_timestamps) */
#if defined(VK_KHR_copy_commands2)
    lahar_load(lahar, vkCmdBlitImage2KHR);
    lahar_load(lahar, vkCmdCopyBuffer2KHR);
    lahar_load(lahar, vkCmdCopyBufferToImage2KHR);
    lahar_load(lahar, vkCmdCopyImage2KHR);
    lahar_load(lahar, vkCmdCopyImageToBuffer2KHR);
    lahar_load(lahar, vkCmdResolveImage2KHR);
#endif /* defined(VK_KHR_copy_commands2) */
#if defined(VK_KHR_create_renderpass2)
    lahar_load(lahar, vkCmdBeginRenderPass2KHR);
    lahar_load(lahar, vkCmdEndRenderPass2KHR);
    lahar_load(lahar, vkCmdNextSubpass2KHR);
    lahar_load(lahar, vkCreateRenderPass2KHR);
#endif /* defined(VK_KHR_create_renderpass2) */
#if defined(VK_KHR_deferred_host_operations)
    lahar_load(lahar, vkCreateDeferredOperationKHR);
    lahar_load(lahar, vkDeferredOperationJoinKHR);
    lahar_load(lahar, vkDestroyDeferredOperationKHR);
    lahar_load(lahar, vkGetDeferredOperationMaxConcurrencyKHR);
    lahar_load(lahar, vkGetDeferredOperationResultKHR);
#endif /* defined(VK_KHR_deferred_host_operations) */
#if defined(VK_KHR_descriptor_update_template)
    lahar_load(lahar, vkCreateDescriptorUpdateTemplateKHR);
    lahar_load(lahar, vkDestroyDescriptorUpdateTemplateKHR);
    lahar_load(lahar, vkUpdateDescriptorSetWithTemplateKHR);
#endif /* defined(VK_KHR_descriptor_update_template) */
#if defined(VK_KHR_device_group)
    lahar_load(lahar, vkCmdDispatchBaseKHR);
    lahar_load(lahar, vkCmdSetDeviceMaskKHR);
    lahar_load(lahar, vkGetDeviceGroupPeerMemoryFeaturesKHR);
#endif /* defined(VK_KHR_device_group) */
#if defined(VK_KHR_display_swapchain)
    lahar_load(lahar, vkCreateSharedSwapchainsKHR);
#endif /* defined(VK_KHR_display_swapchain) */
#if defined(VK_KHR_draw_indirect_count)
    lahar_load(lahar, vkCmdDrawIndexedIndirectCountKHR);
    lahar_load(lahar, vkCmdDrawIndirectCountKHR);
#endif /* defined(VK_KHR_draw_indirect_count) */
#if defined(VK_KHR_dynamic_rendering)
    lahar_load(lahar, vkCmdBeginRenderingKHR);
    lahar_load(lahar, vkCmdEndRenderingKHR);
#endif /* defined(VK_KHR_dynamic_rendering) */
#if defined(VK_KHR_dynamic_rendering_local_read)
    lahar_load(lahar, vkCmdSetRenderingAttachmentLocationsKHR);
    lahar_load(lahar, vkCmdSetRenderingInputAttachmentIndicesKHR);
#endif /* defined(VK_KHR_dynamic_rendering_local_read) */
#if defined(VK_KHR_external_fence_fd)
    lahar_load(lahar, vkGetFenceFdKHR);
    lahar_load(lahar, vkImportFenceFdKHR);
#endif /* defined(VK_KHR_external_fence_fd) */
#if defined(VK_KHR_external_fence_win32)
    lahar_load(lahar, vkGetFenceWin32HandleKHR);
    lahar_load(lahar, vkImportFenceWin32HandleKHR);
#endif /* defined(VK_KHR_external_fence_win32) */
#if defined(VK_KHR_external_memory_fd)
    lahar_load(lahar, vkGetMemoryFdKHR);
    lahar_load(lahar, vkGetMemoryFdPropertiesKHR);
#endif /* defined(VK_KHR_external_memory_fd) */
#if defined(VK_KHR_external_memory_win32)
    lahar_load(lahar, vkGetMemoryWin32HandleKHR);
    lahar_load(lahar, vkGetMemoryWin32HandlePropertiesKHR);
#endif /* defined(VK_KHR_external_memory_win32) */
#if defined(VK_KHR_external_semaphore_fd)
    lahar_load(lahar, vkGetSemaphoreFdKHR);
    lahar_load(lahar, vkImportSemaphoreFdKHR);
#endif /* defined(VK_KHR_external_semaphore_fd) */
#if defined(VK_KHR_external_semaphore_win32)
    lahar_load(lahar, vkGetSemaphoreWin32HandleKHR);
    lahar_load(lahar, vkImportSemaphoreWin32HandleKHR);
#endif /* defined(VK_KHR_external_semaphore_win32) */
#if defined(VK_KHR_fragment_shading_rate)
    lahar_load(lahar, vkCmdSetFragmentShadingRateKHR);
#endif /* defined(VK_KHR_fragment_shading_rate) */
#if defined(VK_KHR_get_memory_requirements2)
    lahar_load(lahar, vkGetBufferMemoryRequirements2KHR);
    lahar_load(lahar, vkGetImageMemoryRequirements2KHR);
    lahar_load(lahar, vkGetImageSparseMemoryRequirements2KHR);
#endif /* defined(VK_KHR_get_memory_requirements2) */
#if defined(VK_KHR_line_rasterization)
    lahar_load(lahar, vkCmdSetLineStippleKHR);
#endif /* defined(VK_KHR_line_rasterization) */
#if defined(VK_KHR_maintenance1)
    lahar_load(lahar, vkTrimCommandPoolKHR);
#endif /* defined(VK_KHR_maintenance1) */
#if defined(VK_KHR_maintenance3)
    lahar_load(lahar, vkGetDescriptorSetLayoutSupportKHR);
#endif /* defined(VK_KHR_maintenance3) */
#if defined(VK_KHR_maintenance4)
    lahar_load(lahar, vkGetDeviceBufferMemoryRequirementsKHR);
    lahar_load(lahar, vkGetDeviceImageMemoryRequirementsKHR);
    lahar_load(lahar, vkGetDeviceImageSparseMemoryRequirementsKHR);
#endif /* defined(VK_KHR_maintenance4) */
#if defined(VK_KHR_maintenance5)
    lahar_load(lahar, vkCmdBindIndexBuffer2KHR);
    lahar_load(lahar, vkGetDeviceImageSubresourceLayoutKHR);
    lahar_load(lahar, vkGetImageSubresourceLayout2KHR);
    lahar_load(lahar, vkGetRenderingAreaGranularityKHR);
#endif /* defined(VK_KHR_maintenance5) */
#if defined(VK_KHR_maintenance6)
    lahar_load(lahar, vkCmdBindDescriptorSets2KHR);
    lahar_load(lahar, vkCmdPushConstants2KHR);
#endif /* defined(VK_KHR_maintenance6) */
#if defined(VK_KHR_maintenance6) && defined(VK_KHR_push_descriptor)
    lahar_load(lahar, vkCmdPushDescriptorSet2KHR);
    lahar_load(lahar, vkCmdPushDescriptorSetWithTemplate2KHR);
#endif /* defined(VK_KHR_maintenance6) && defined(VK_KHR_push_descriptor) */
#if defined(VK_KHR_maintenance6) && defined(VK_EXT_descriptor_buffer)
    lahar_load(lahar, vkCmdBindDescriptorBufferEmbeddedSamplers2EXT);
    lahar_load(lahar, vkCmdSetDescriptorBufferOffsets2EXT);
#endif /* defined(VK_KHR_maintenance6) && defined(VK_EXT_descriptor_buffer) */
#if defined(VK_KHR_map_memory2)
    lahar_load(lahar, vkMapMemory2KHR);
    lahar_load(lahar, vkUnmapMemory2KHR);
#endif /* defined(VK_KHR_map_memory2) */
#if defined(VK_KHR_performance_query)
    lahar_load(lahar, vkAcquireProfilingLockKHR);
    lahar_load(lahar, vkReleaseProfilingLockKHR);
#endif /* defined(VK_KHR_performance_query) */
#if defined(VK_KHR_pipeline_binary)
    lahar_load(lahar, vkCreatePipelineBinariesKHR);
    lahar_load(lahar, vkDestroyPipelineBinaryKHR);
    lahar_load(lahar, vkGetPipelineBinaryDataKHR);
    lahar_load(lahar, vkGetPipelineKeyKHR);
    lahar_load(lahar, vkReleaseCapturedPipelineDataKHR);
#endif /* defined(VK_KHR_pipeline_binary) */
#if defined(VK_KHR_pipeline_executable_properties)
    lahar_load(lahar, vkGetPipelineExecutableInternalRepresentationsKHR);
    lahar_load(lahar, vkGetPipelineExecutablePropertiesKHR);
    lahar_load(lahar, vkGetPipelineExecutableStatisticsKHR);
#endif /* defined(VK_KHR_pipeline_executable_properties) */
#if defined(VK_KHR_present_wait)
    lahar_load(lahar, vkWaitForPresentKHR);
#endif /* defined(VK_KHR_present_wait) */
#if defined(VK_KHR_present_wait2)
    lahar_load(lahar, vkWaitForPresent2KHR);
#endif /* defined(VK_KHR_present_wait2) */
#if defined(VK_KHR_push_descriptor)
    lahar_load(lahar, vkCmdPushDescriptorSetKHR);
#endif /* defined(VK_KHR_push_descriptor) */
#if defined(VK_KHR_ray_tracing_maintenance1) && defined(VK_KHR_ray_tracing_pipeline)
    lahar_load(lahar, vkCmdTraceRaysIndirect2KHR);
#endif /* defined(VK_KHR_ray_tracing_maintenance1) && defined(VK_KHR_ray_tracing_pipeline) */
#if defined(VK_KHR_ray_tracing_pipeline)
    lahar_load(lahar, vkCmdSetRayTracingPipelineStackSizeKHR);
    lahar_load(lahar, vkCmdTraceRaysIndirectKHR);
    lahar_load(lahar, vkCmdTraceRaysKHR);
    lahar_load(lahar, vkCreateRayTracingPipelinesKHR);
    lahar_load(lahar, vkGetRayTracingCaptureReplayShaderGroupHandlesKHR);
    lahar_load(lahar, vkGetRayTracingShaderGroupHandlesKHR);
    lahar_load(lahar, vkGetRayTracingShaderGroupStackSizeKHR);
#endif /* defined(VK_KHR_ray_tracing_pipeline) */
#if defined(VK_KHR_sampler_ycbcr_conversion)
    lahar_load(lahar, vkCreateSamplerYcbcrConversionKHR);
    lahar_load(lahar, vkDestroySamplerYcbcrConversionKHR);
#endif /* defined(VK_KHR_sampler_ycbcr_conversion) */
#if defined(VK_KHR_shared_presentable_image)
    lahar_load(lahar, vkGetSwapchainStatusKHR);
#endif /* defined(VK_KHR_shared_presentable_image) */
#if defined(VK_KHR_swapchain)
    lahar_load(lahar, vkAcquireNextImageKHR);
    lahar_load(lahar, vkCreateSwapchainKHR);
    lahar_load(lahar, vkDestroySwapchainKHR);
    lahar_load(lahar, vkGetSwapchainImagesKHR);
    lahar_load(lahar, vkQueuePresentKHR);
#endif /* defined(VK_KHR_swapchain) */
#if defined(VK_KHR_swapchain_maintenance1)
    lahar_load(lahar, vkReleaseSwapchainImagesKHR);
#endif /* defined(VK_KHR_swapchain_maintenance1) */
#if defined(VK_KHR_synchronization2)
    lahar_load(lahar, vkCmdPipelineBarrier2KHR);
    lahar_load(lahar, vkCmdResetEvent2KHR);
    lahar_load(lahar, vkCmdSetEvent2KHR);
    lahar_load(lahar, vkCmdWaitEvents2KHR);
    lahar_load(lahar, vkCmdWriteTimestamp2KHR);
    lahar_load(lahar, vkQueueSubmit2KHR);
#endif /* defined(VK_KHR_synchronization2) */
#if defined(VK_KHR_timeline_semaphore)
    lahar_load(lahar, vkGetSemaphoreCounterValueKHR);
    lahar_load(lahar, vkSignalSemaphoreKHR);
    lahar_load(lahar, vkWaitSemaphoresKHR);
#endif /* defined(VK_KHR_timeline_semaphore) */
#if defined(VK_KHR_video_decode_queue)
    lahar_load(lahar, vkCmdDecodeVideoKHR);
#endif /* defined(VK_KHR_video_decode_queue) */
#if defined(VK_KHR_video_encode_queue)
    lahar_load(lahar, vkCmdEncodeVideoKHR);
    lahar_load(lahar, vkGetEncodedVideoSessionParametersKHR);
#endif /* defined(VK_KHR_video_encode_queue) */
#if defined(VK_KHR_video_queue)
    lahar_load(lahar, vkBindVideoSessionMemoryKHR);
    lahar_load(lahar, vkCmdBeginVideoCodingKHR);
    lahar_load(lahar, vkCmdControlVideoCodingKHR);
    lahar_load(lahar, vkCmdEndVideoCodingKHR);
    lahar_load(lahar, vkCreateVideoSessionKHR);
    lahar_load(lahar, vkCreateVideoSessionParametersKHR);
    lahar_load(lahar, vkDestroyVideoSessionKHR);
    lahar_load(lahar, vkDestroyVideoSessionParametersKHR);
    lahar_load(lahar, vkGetVideoSessionMemoryRequirementsKHR);
    lahar_load(lahar, vkUpdateVideoSessionParametersKHR);
#endif /* defined(VK_KHR_video_queue) */
#if defined(VK_NVX_binary_import)
    lahar_load(lahar, vkCmdCuLaunchKernelNVX);
    lahar_load(lahar, vkCreateCuFunctionNVX);
    lahar_load(lahar, vkCreateCuModuleNVX);
    lahar_load(lahar, vkDestroyCuFunctionNVX);
    lahar_load(lahar, vkDestroyCuModuleNVX);
#endif /* defined(VK_NVX_binary_import) */
#if defined(VK_NVX_image_view_handle)
    lahar_load(lahar, vkGetImageViewHandleNVX);
#endif /* defined(VK_NVX_image_view_handle) */
#if defined(VK_NVX_image_view_handle) && VK_NVX_IMAGE_VIEW_HANDLE_SPEC_VERSION >= 3
    lahar_load(lahar, vkGetImageViewHandle64NVX);
#endif /* defined(VK_NVX_image_view_handle) && VK_NVX_IMAGE_VIEW_HANDLE_SPEC_VERSION >= 3 */
#if defined(VK_NVX_image_view_handle) && VK_NVX_IMAGE_VIEW_HANDLE_SPEC_VERSION >= 2
    lahar_load(lahar, vkGetImageViewAddressNVX);
#endif /* defined(VK_NVX_image_view_handle) && VK_NVX_IMAGE_VIEW_HANDLE_SPEC_VERSION >= 2 */
#if defined(VK_NV_clip_space_w_scaling)
    lahar_load(lahar, vkCmdSetViewportWScalingNV);
#endif /* defined(VK_NV_clip_space_w_scaling) */
#if defined(VK_NV_cluster_acceleration_structure)
    lahar_load(lahar, vkCmdBuildClusterAccelerationStructureIndirectNV);
    lahar_load(lahar, vkGetClusterAccelerationStructureBuildSizesNV);
#endif /* defined(VK_NV_cluster_acceleration_structure) */
#if defined(VK_NV_cooperative_vector)
    lahar_load(lahar, vkCmdConvertCooperativeVectorMatrixNV);
    lahar_load(lahar, vkConvertCooperativeVectorMatrixNV);
#endif /* defined(VK_NV_cooperative_vector) */
#if defined(VK_NV_copy_memory_indirect)
    lahar_load(lahar, vkCmdCopyMemoryIndirectNV);
    lahar_load(lahar, vkCmdCopyMemoryToImageIndirectNV);
#endif /* defined(VK_NV_copy_memory_indirect) */
#if defined(VK_NV_cuda_kernel_launch)
    lahar_load(lahar, vkCmdCudaLaunchKernelNV);
    lahar_load(lahar, vkCreateCudaFunctionNV);
    lahar_load(lahar, vkCreateCudaModuleNV);
    lahar_load(lahar, vkDestroyCudaFunctionNV);
    lahar_load(lahar, vkDestroyCudaModuleNV);
    lahar_load(lahar, vkGetCudaModuleCacheNV);
#endif /* defined(VK_NV_cuda_kernel_launch) */
#if defined(VK_NV_device_diagnostic_checkpoints)
    lahar_load(lahar, vkCmdSetCheckpointNV);
    lahar_load(lahar, vkGetQueueCheckpointDataNV);
#endif /* defined(VK_NV_device_diagnostic_checkpoints) */
#if defined(VK_NV_device_diagnostic_checkpoints) && (defined(VK_VERSION_1_3) || defined(VK_KHR_synchronization2))
    lahar_load(lahar, vkGetQueueCheckpointData2NV);
#endif /* defined(VK_NV_device_diagnostic_checkpoints) && (defined(VK_VERSION_1_3) || defined(VK_KHR_synchronization2)) */
#if defined(VK_NV_device_generated_commands)
    lahar_load(lahar, vkCmdBindPipelineShaderGroupNV);
    lahar_load(lahar, vkCmdExecuteGeneratedCommandsNV);
    lahar_load(lahar, vkCmdPreprocessGeneratedCommandsNV);
    lahar_load(lahar, vkCreateIndirectCommandsLayoutNV);
    lahar_load(lahar, vkDestroyIndirectCommandsLayoutNV);
    lahar_load(lahar, vkGetGeneratedCommandsMemoryRequirementsNV);
#endif /* defined(VK_NV_device_generated_commands) */
#if defined(VK_NV_device_generated_commands_compute)
    lahar_load(lahar, vkCmdUpdatePipelineIndirectBufferNV);
    lahar_load(lahar, vkGetPipelineIndirectDeviceAddressNV);
    lahar_load(lahar, vkGetPipelineIndirectMemoryRequirementsNV);
#endif /* defined(VK_NV_device_generated_commands_compute) */
#if defined(VK_NV_external_compute_queue)
    lahar_load(lahar, vkCreateExternalComputeQueueNV);
    lahar_load(lahar, vkDestroyExternalComputeQueueNV);
    lahar_load(lahar, vkGetExternalComputeQueueDataNV);
#endif /* defined(VK_NV_external_compute_queue) */
#if defined(VK_NV_external_memory_rdma)
    lahar_load(lahar, vkGetMemoryRemoteAddressNV);
#endif /* defined(VK_NV_external_memory_rdma) */
#if defined(VK_NV_external_memory_win32)
    lahar_load(lahar, vkGetMemoryWin32HandleNV);
#endif /* defined(VK_NV_external_memory_win32) */
#if defined(VK_NV_fragment_shading_rate_enums)
    lahar_load(lahar, vkCmdSetFragmentShadingRateEnumNV);
#endif /* defined(VK_NV_fragment_shading_rate_enums) */
#if defined(VK_NV_low_latency2)
    lahar_load(lahar, vkGetLatencyTimingsNV);
    lahar_load(lahar, vkLatencySleepNV);
    lahar_load(lahar, vkQueueNotifyOutOfBandNV);
    lahar_load(lahar, vkSetLatencyMarkerNV);
    lahar_load(lahar, vkSetLatencySleepModeNV);
#endif /* defined(VK_NV_low_latency2) */
#if defined(VK_NV_memory_decompression)
    lahar_load(lahar, vkCmdDecompressMemoryIndirectCountNV);
    lahar_load(lahar, vkCmdDecompressMemoryNV);
#endif /* defined(VK_NV_memory_decompression) */
#if defined(VK_NV_mesh_shader)
    lahar_load(lahar, vkCmdDrawMeshTasksIndirectNV);
    lahar_load(lahar, vkCmdDrawMeshTasksNV);
#endif /* defined(VK_NV_mesh_shader) */
#if defined(VK_NV_mesh_shader) && (defined(VK_KHR_draw_indirect_count) || defined(VK_VERSION_1_2))
    lahar_load(lahar, vkCmdDrawMeshTasksIndirectCountNV);
#endif /* defined(VK_NV_mesh_shader) && (defined(VK_KHR_draw_indirect_count) || defined(VK_VERSION_1_2)) */
#if defined(VK_NV_optical_flow)
    lahar_load(lahar, vkBindOpticalFlowSessionImageNV);
    lahar_load(lahar, vkCmdOpticalFlowExecuteNV);
    lahar_load(lahar, vkCreateOpticalFlowSessionNV);
    lahar_load(lahar, vkDestroyOpticalFlowSessionNV);
#endif /* defined(VK_NV_optical_flow) */
#if defined(VK_NV_partitioned_acceleration_structure)
    lahar_load(lahar, vkCmdBuildPartitionedAccelerationStructuresNV);
    lahar_load(lahar, vkGetPartitionedAccelerationStructuresBuildSizesNV);
#endif /* defined(VK_NV_partitioned_acceleration_structure) */
#if defined(VK_NV_ray_tracing)
    lahar_load(lahar, vkBindAccelerationStructureMemoryNV);
    lahar_load(lahar, vkCmdBuildAccelerationStructureNV);
    lahar_load(lahar, vkCmdCopyAccelerationStructureNV);
    lahar_load(lahar, vkCmdTraceRaysNV);
    lahar_load(lahar, vkCmdWriteAccelerationStructuresPropertiesNV);
    lahar_load(lahar, vkCompileDeferredNV);
    lahar_load(lahar, vkCreateAccelerationStructureNV);
    lahar_load(lahar, vkCreateRayTracingPipelinesNV);
    lahar_load(lahar, vkDestroyAccelerationStructureNV);
    lahar_load(lahar, vkGetAccelerationStructureHandleNV);
    lahar_load(lahar, vkGetAccelerationStructureMemoryRequirementsNV);
    lahar_load(lahar, vkGetRayTracingShaderGroupHandlesNV);
#endif /* defined(VK_NV_ray_tracing) */
#if defined(VK_NV_scissor_exclusive) && VK_NV_SCISSOR_EXCLUSIVE_SPEC_VERSION >= 2
    lahar_load(lahar, vkCmdSetExclusiveScissorEnableNV);
#endif /* defined(VK_NV_scissor_exclusive) && VK_NV_SCISSOR_EXCLUSIVE_SPEC_VERSION >= 2 */
#if defined(VK_NV_scissor_exclusive)
    lahar_load(lahar, vkCmdSetExclusiveScissorNV);
#endif /* defined(VK_NV_scissor_exclusive) */
#if defined(VK_NV_shading_rate_image)
    lahar_load(lahar, vkCmdBindShadingRateImageNV);
    lahar_load(lahar, vkCmdSetCoarseSampleOrderNV);
    lahar_load(lahar, vkCmdSetViewportShadingRatePaletteNV);
#endif /* defined(VK_NV_shading_rate_image) */
#if defined(VK_QCOM_tile_memory_heap)
    lahar_load(lahar, vkCmdBindTileMemoryQCOM);
#endif /* defined(VK_QCOM_tile_memory_heap) */
#if defined(VK_QCOM_tile_properties)
    lahar_load(lahar, vkGetDynamicRenderingTilePropertiesQCOM);
    lahar_load(lahar, vkGetFramebufferTilePropertiesQCOM);
#endif /* defined(VK_QCOM_tile_properties) */
#if defined(VK_QCOM_tile_shading)
    lahar_load(lahar, vkCmdBeginPerTileExecutionQCOM);
    lahar_load(lahar, vkCmdDispatchTileQCOM);
    lahar_load(lahar, vkCmdEndPerTileExecutionQCOM);
#endif /* defined(VK_QCOM_tile_shading) */
#if defined(VK_QNX_external_memory_screen_buffer)
    lahar_load(lahar, vkGetScreenBufferPropertiesQNX);
#endif /* defined(VK_QNX_external_memory_screen_buffer) */
#if defined(VK_VALVE_descriptor_set_host_mapping)
    lahar_load(lahar, vkGetDescriptorSetHostMappingVALVE);
    lahar_load(lahar, vkGetDescriptorSetLayoutHostMappingInfoVALVE);
#endif /* defined(VK_VALVE_descriptor_set_host_mapping) */
#if (defined(VK_EXT_depth_clamp_control)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_depth_clamp_control))
    lahar_load(lahar, vkCmdSetDepthClampRangeEXT);
#endif /* (defined(VK_EXT_depth_clamp_control)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_depth_clamp_control)) */
#if (defined(VK_EXT_extended_dynamic_state)) || (defined(VK_EXT_shader_object))
    lahar_load(lahar, vkCmdBindVertexBuffers2EXT);
    lahar_load(lahar, vkCmdSetCullModeEXT);
    lahar_load(lahar, vkCmdSetDepthBoundsTestEnableEXT);
    lahar_load(lahar, vkCmdSetDepthCompareOpEXT);
    lahar_load(lahar, vkCmdSetDepthTestEnableEXT);
    lahar_load(lahar, vkCmdSetDepthWriteEnableEXT);
    lahar_load(lahar, vkCmdSetFrontFaceEXT);
    lahar_load(lahar, vkCmdSetPrimitiveTopologyEXT);
    lahar_load(lahar, vkCmdSetScissorWithCountEXT);
    lahar_load(lahar, vkCmdSetStencilOpEXT);
    lahar_load(lahar, vkCmdSetStencilTestEnableEXT);
    lahar_load(lahar, vkCmdSetViewportWithCountEXT);
#endif /* (defined(VK_EXT_extended_dynamic_state)) || (defined(VK_EXT_shader_object)) */
#if (defined(VK_EXT_extended_dynamic_state2)) || (defined(VK_EXT_shader_object))
    lahar_load(lahar, vkCmdSetDepthBiasEnableEXT);
    lahar_load(lahar, vkCmdSetLogicOpEXT);
    lahar_load(lahar, vkCmdSetPatchControlPointsEXT);
    lahar_load(lahar, vkCmdSetPrimitiveRestartEnableEXT);
    lahar_load(lahar, vkCmdSetRasterizerDiscardEnableEXT);
#endif /* (defined(VK_EXT_extended_dynamic_state2)) || (defined(VK_EXT_shader_object)) */
#if (defined(VK_EXT_extended_dynamic_state3)) || (defined(VK_EXT_shader_object))
    lahar_load(lahar, vkCmdSetAlphaToCoverageEnableEXT);
    lahar_load(lahar, vkCmdSetAlphaToOneEnableEXT);
    lahar_load(lahar, vkCmdSetColorBlendEnableEXT);
    lahar_load(lahar, vkCmdSetColorBlendEquationEXT);
    lahar_load(lahar, vkCmdSetColorWriteMaskEXT);
    lahar_load(lahar, vkCmdSetDepthClampEnableEXT);
    lahar_load(lahar, vkCmdSetLogicOpEnableEXT);
    lahar_load(lahar, vkCmdSetPolygonModeEXT);
    lahar_load(lahar, vkCmdSetRasterizationSamplesEXT);
    lahar_load(lahar, vkCmdSetSampleMaskEXT);
#endif /* (defined(VK_EXT_extended_dynamic_state3)) || (defined(VK_EXT_shader_object)) */
#if (defined(VK_EXT_extended_dynamic_state3) && (defined(VK_KHR_maintenance2) || defined(VK_VERSION_1_1))) || (defined(VK_EXT_shader_object))
    lahar_load(lahar, vkCmdSetTessellationDomainOriginEXT);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && (defined(VK_KHR_maintenance2) || defined(VK_VERSION_1_1))) || (defined(VK_EXT_shader_object)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_transform_feedback)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_transform_feedback))
    lahar_load(lahar, vkCmdSetRasterizationStreamEXT);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_transform_feedback)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_transform_feedback)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_conservative_rasterization)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_conservative_rasterization))
    lahar_load(lahar, vkCmdSetConservativeRasterizationModeEXT);
    lahar_load(lahar, vkCmdSetExtraPrimitiveOverestimationSizeEXT);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_conservative_rasterization)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_conservative_rasterization)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_depth_clip_enable)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_depth_clip_enable))
    lahar_load(lahar, vkCmdSetDepthClipEnableEXT);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_depth_clip_enable)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_depth_clip_enable)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_sample_locations)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_sample_locations))
    lahar_load(lahar, vkCmdSetSampleLocationsEnableEXT);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_sample_locations)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_sample_locations)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_blend_operation_advanced)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_blend_operation_advanced))
    lahar_load(lahar, vkCmdSetColorBlendAdvancedEXT);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_blend_operation_advanced)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_blend_operation_advanced)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_provoking_vertex)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_provoking_vertex))
    lahar_load(lahar, vkCmdSetProvokingVertexModeEXT);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_provoking_vertex)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_provoking_vertex)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_line_rasterization)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_line_rasterization))
    lahar_load(lahar, vkCmdSetLineRasterizationModeEXT);
    lahar_load(lahar, vkCmdSetLineStippleEnableEXT);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_line_rasterization)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_line_rasterization)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_depth_clip_control)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_depth_clip_control))
    lahar_load(lahar, vkCmdSetDepthClipNegativeOneToOneEXT);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_EXT_depth_clip_control)) || (defined(VK_EXT_shader_object) && defined(VK_EXT_depth_clip_control)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_clip_space_w_scaling)) || (defined(VK_EXT_shader_object) && defined(VK_NV_clip_space_w_scaling))
    lahar_load(lahar, vkCmdSetViewportWScalingEnableNV);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_clip_space_w_scaling)) || (defined(VK_EXT_shader_object) && defined(VK_NV_clip_space_w_scaling)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_viewport_swizzle)) || (defined(VK_EXT_shader_object) && defined(VK_NV_viewport_swizzle))
    lahar_load(lahar, vkCmdSetViewportSwizzleNV);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_viewport_swizzle)) || (defined(VK_EXT_shader_object) && defined(VK_NV_viewport_swizzle)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_fragment_coverage_to_color)) || (defined(VK_EXT_shader_object) && defined(VK_NV_fragment_coverage_to_color))
    lahar_load(lahar, vkCmdSetCoverageToColorEnableNV);
    lahar_load(lahar, vkCmdSetCoverageToColorLocationNV);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_fragment_coverage_to_color)) || (defined(VK_EXT_shader_object) && defined(VK_NV_fragment_coverage_to_color)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_framebuffer_mixed_samples)) || (defined(VK_EXT_shader_object) && defined(VK_NV_framebuffer_mixed_samples))
    lahar_load(lahar, vkCmdSetCoverageModulationModeNV);
    lahar_load(lahar, vkCmdSetCoverageModulationTableEnableNV);
    lahar_load(lahar, vkCmdSetCoverageModulationTableNV);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_framebuffer_mixed_samples)) || (defined(VK_EXT_shader_object) && defined(VK_NV_framebuffer_mixed_samples)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_shading_rate_image)) || (defined(VK_EXT_shader_object) && defined(VK_NV_shading_rate_image))
    lahar_load(lahar, vkCmdSetShadingRateImageEnableNV);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_shading_rate_image)) || (defined(VK_EXT_shader_object) && defined(VK_NV_shading_rate_image)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_representative_fragment_test)) || (defined(VK_EXT_shader_object) && defined(VK_NV_representative_fragment_test))
    lahar_load(lahar, vkCmdSetRepresentativeFragmentTestEnableNV);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_representative_fragment_test)) || (defined(VK_EXT_shader_object) && defined(VK_NV_representative_fragment_test)) */
#if (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_coverage_reduction_mode)) || (defined(VK_EXT_shader_object) && defined(VK_NV_coverage_reduction_mode))
    lahar_load(lahar, vkCmdSetCoverageReductionModeNV);
#endif /* (defined(VK_EXT_extended_dynamic_state3) && defined(VK_NV_coverage_reduction_mode)) || (defined(VK_EXT_shader_object) && defined(VK_NV_coverage_reduction_mode)) */
#if (defined(VK_EXT_host_image_copy)) || (defined(VK_EXT_image_compression_control))
    lahar_load(lahar, vkGetImageSubresourceLayout2EXT);
#endif /* (defined(VK_EXT_host_image_copy)) || (defined(VK_EXT_image_compression_control)) */
#if (defined(VK_EXT_shader_object)) || (defined(VK_EXT_vertex_input_dynamic_state))
    lahar_load(lahar, vkCmdSetVertexInputEXT);
#endif /* (defined(VK_EXT_shader_object)) || (defined(VK_EXT_vertex_input_dynamic_state)) */
#if (defined(VK_KHR_descriptor_update_template) && defined(VK_KHR_push_descriptor)) || (defined(VK_KHR_push_descriptor) && (defined(VK_VERSION_1_1) || defined(VK_KHR_descriptor_update_template)))
    lahar_load(lahar, vkCmdPushDescriptorSetWithTemplateKHR);
#endif /* (defined(VK_KHR_descriptor_update_template) && defined(VK_KHR_push_descriptor)) || (defined(VK_KHR_push_descriptor) && (defined(VK_VERSION_1_1) || defined(VK_KHR_descriptor_update_template))) */
#if (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1))
    lahar_load(lahar, vkGetDeviceGroupPresentCapabilitiesKHR);
    lahar_load(lahar, vkGetDeviceGroupSurfacePresentModesKHR);
#endif /* (defined(VK_KHR_device_group) && defined(VK_KHR_surface)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1)) */
#if (defined(VK_KHR_device_group) && defined(VK_KHR_swapchain)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1))
    lahar_load(lahar, vkAcquireNextImage2KHR);
#endif /* (defined(VK_KHR_device_group) && defined(VK_KHR_swapchain)) || (defined(VK_KHR_swapchain) && defined(VK_VERSION_1_1)) */
/* LAHAR_VK_LOAD_DEVICE */

    return LAHAR_ERR_SUCCESS;
}



#endif // LAHAR_IMPLEMENTATION

#endif //LAHAR_H




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
