/* Test suite for the LaharAllocator vtable.
 *
 * These tests intentionally go through the lahar->gpu_allocator function
 * pointers rather than calling any backend directly, so the same suite can
 * be pointed at any LaharAllocator implementation. Right now only the
 * VMA-backed adapter is exercised; the native freelist allocator is still a
 * work in progress and its tests will come back once it is complete.
 *
 * To test your own implementation, point custom_allocator at it below.
 *
 * Define LAHAR_TEST_FREELIST to run the suite against the native freelist
 * allocator (allocator.c) instead of VMA.
 *
 * Define LAHAR_TEST_SPY to instrument the raw Vulkan entry points. Lahar is
 * the loader, so vkAllocateMemory et al are plain global PFN variables that
 * we can swap for wrappers after lahar_build. This checks driver-level usage
 * (alloc/free pairing, sane parameters, real suballocation) that the vtable's
 * return codes can't express. Works against any backend, including VMA.
 *
 * Runs fully headless: no window is registered, so it works on a pure
 * compute/llvmpipe setup.
 */

/* Enable lahar's internal invariant assertions and the freelist's
 * whole-heap validator. Must precede the lahar.h include: LAHAR_ASSERT is
 * fixed at that point. */
#define LAHAR_DEBUG

#define LAHAR_USE_VMA
#define VMA_IMPLEMENTATION

#define LAHAR_IMPLEMENTATION
#include "../lahar.h"

#define LAHAR_TEST_FREELIST

#include <stdio.h>
#include <string.h>

/* The allocator under test. NULL means lahar's built-in default. */
static LaharAllocator* custom_allocator = NULL;
#ifdef LAHAR_TEST_FREELIST
static const char* allocator_name = "freelist";
#else
static const char* allocator_name = "VMA";
#endif

#define tassert(cond, msg) \
    do { if (!(cond)) { printf("\tAssert failed (%s:%d): %s\n", __FILE__, __LINE__, msg); return false; } } while (0)

#define tassert_ok(expr, msg) \
    do { uint32_t __e = (expr); if (__e != LAHAR_ERR_SUCCESS) { \
        printf("\tAssert failed (%s:%d): %s -> %s\n", __FILE__, __LINE__, msg, lahar_err_name(__e)); return false; } } while (0)

static LaharAllocator* A(void) { return lahar->gpu_allocator; }

/* ------------------------------------------------------------------- spy */

#define LAHAR_TEST_SPY

#ifdef LAHAR_TEST_SPY

static PFN_vkAllocateMemory  real_vkAllocateMemory;
static PFN_vkFreeMemory      real_vkFreeMemory;
static PFN_vkBindImageMemory real_vkBindImageMemory;
static PFN_vkBindBufferMemory real_vkBindBufferMemory;

static int g_dev_allocs;     /* successful vkAllocateMemory calls */
static int g_dev_frees;      /* vkFreeMemory calls with a non-null handle */
static int g_spy_violations; /* driver-level contract violations observed */

/* Spies can't fail the test from inside a driver call; record and report. */
#define spy_check(cond, msg) \
    do { if (!(cond)) { printf("\tSpy violation (%s:%d): %s\n", __FILE__, __LINE__, msg); g_spy_violations++; } } while (0)

static VkResult VKAPI_PTR spy_vkAllocateMemory(
    VkDevice device, const VkMemoryAllocateInfo* info,
    const VkAllocationCallbacks* cb, VkDeviceMemory* out
) {
    spy_check(info != NULL, "vkAllocateMemory with NULL info");
    spy_check(out != NULL, "vkAllocateMemory with NULL out");

    if (info) {
        spy_check(info->sType == VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, "vkAllocateMemory with bad sType");
        spy_check(info->allocationSize > 0, "vkAllocateMemory with zero size");
        spy_check(info->memoryTypeIndex < lahar->physdev_info.memprops.memoryTypeCount,
            "vkAllocateMemory with out-of-range memoryTypeIndex");
    }

    VkResult res = real_vkAllocateMemory(device, info, cb, out);
    if (res == VK_SUCCESS) { g_dev_allocs++; }
    return res;
}

static void VKAPI_PTR spy_vkFreeMemory(
    VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* cb
) {
    if (memory != VK_NULL_HANDLE) { g_dev_frees++; }
    real_vkFreeMemory(device, memory, cb);
}

/* Bind tracking: which VkDeviceMemory objects received buffer binds vs image
 * binds. Used by the granularity segregation test. Fixed-size and saturating;
 * tests that use these should reset first via spy_reset_bind_log(). */
#define SPY_BIND_LOG_CAP 256
static VkDeviceMemory g_buf_bind_mems[SPY_BIND_LOG_CAP];
static VkDeviceMemory g_img_bind_mems[SPY_BIND_LOG_CAP];
static int g_buf_bind_count, g_img_bind_count;

static void spy_reset_bind_log(void) {
    g_buf_bind_count = 0;
    g_img_bind_count = 0;
}

static VkResult VKAPI_PTR spy_vkBindImageMemory(
    VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize offset
) {
    spy_check(memory != VK_NULL_HANDLE, "vkBindImageMemory with NULL memory");
    VkMemoryRequirements reqs;
    vkGetImageMemoryRequirements(device, image, &reqs);
    spy_check(offset % reqs.alignment == 0, "vkBindImageMemory offset not aligned to requirements");

    if (g_img_bind_count < SPY_BIND_LOG_CAP) { g_img_bind_mems[g_img_bind_count++] = memory; }

    return real_vkBindImageMemory(device, image, memory, offset);
}

static VkResult VKAPI_PTR spy_vkBindBufferMemory(
    VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize offset
) {
    spy_check(memory != VK_NULL_HANDLE, "vkBindBufferMemory with NULL memory");
    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(device, buffer, &reqs);
    spy_check(offset % reqs.alignment == 0, "vkBindBufferMemory offset not aligned to requirements");

    if (g_buf_bind_count < SPY_BIND_LOG_CAP) { g_buf_bind_mems[g_buf_bind_count++] = memory; }

    return real_vkBindBufferMemory(device, buffer, memory, offset);
}

/* Lahar loads the PFN globals during lahar_build, so install after that. */
static void spy_install(void) {
    real_vkAllocateMemory   = vkAllocateMemory;
    real_vkFreeMemory       = vkFreeMemory;
    real_vkBindImageMemory  = vkBindImageMemory;
    real_vkBindBufferMemory = vkBindBufferMemory;

    vkAllocateMemory   = spy_vkAllocateMemory;
    vkFreeMemory       = spy_vkFreeMemory;
    vkBindImageMemory  = spy_vkBindImageMemory;
    vkBindBufferMemory = spy_vkBindBufferMemory;
}

#endif /* LAHAR_TEST_SPY */

/* ---------------------------------------------------------------- helpers */

static VkBufferCreateInfo buffer_info(VkDeviceSize size, VkBufferUsageFlags usage) {
    VkBufferCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = size;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    return info;
}

static VkImageCreateInfo image_info(uint32_t width, uint32_t height) {
    VkImageCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = VK_FORMAT_R8G8B8A8_UNORM;
    info.extent = { width, height, 1 };
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    return info;
}

/* Role is intentionally left blank -- the usage class is all these tests care
 * about, and implementations are allowed to ignore role entirely. */
static LaharAllocationCreateInfo alloc_info(LaharMemoryUsage usage) {
    LaharAllocationCreateInfo info = {};
    info.usage = usage;
    return info;
}

/* ------------------------------------------------------------------ tests */

/* The allocator must exist post-build and have a complete vtable */
static bool test_vtable_complete(void) {
    tassert(A() != NULL, "gpu_allocator is NULL after lahar_build");
    tassert(A()->alloc_image != NULL, "alloc_image is NULL");
    tassert(A()->free_image != NULL, "free_image is NULL");
    tassert(A()->alloc_buffer != NULL, "alloc_buffer is NULL");
    tassert(A()->free_buffer != NULL, "free_buffer is NULL");
    tassert(A()->map != NULL, "map is NULL");
    tassert(A()->unmap != NULL, "unmap is NULL");
    tassert(A()->flush != NULL, "flush is NULL");
    tassert(A()->invalidate != NULL, "invalidate is NULL");
    return true;
}

/* Simple buffer allocation + free for every memory usage class */
static bool test_buffer_alloc_free_all_usages(void) {
    const LaharMemoryUsage usages[] = {
        LAHAR_MU_DEVICE_ONLY, LAHAR_MU_STAGING_SEQUENTIAL, LAHAR_MU_UPLOAD_DIRECT, LAHAR_MU_READBACK
    };

    for (size_t i = 0; i < sizeof(usages) / sizeof(usages[0]); i++) {
        VkBufferCreateInfo info = buffer_info(4096, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        LaharAllocationCreateInfo ainfo = alloc_info(usages[i]);
        VkBuffer buffer = VK_NULL_HANDLE;
        LaharAllocation alloc = {};

        tassert_ok(A()->alloc_buffer(A(), &info, &ainfo, &buffer, &alloc), "alloc_buffer failed");
        tassert(buffer != VK_NULL_HANDLE, "alloc_buffer succeeded but buffer is VK_NULL_HANDLE");

        tassert_ok(A()->free_buffer(A(), buffer, alloc), "free_buffer failed");
    }

    return true;
}

/* Image allocation + free */
static bool test_image_alloc_free(void) {
    VkImageCreateInfo info = image_info(64, 64);
    LaharAllocationCreateInfo ainfo = alloc_info(LAHAR_MU_DEVICE_ONLY);
    VkImage image = VK_NULL_HANDLE;
    LaharAllocation alloc = {};

    tassert_ok(A()->alloc_image(A(), &info, &ainfo, &image, &alloc), "alloc_image failed");
    tassert(image != VK_NULL_HANDLE, "alloc_image succeeded but image is VK_NULL_HANDLE");

    tassert_ok(A()->free_image(A(), image, alloc), "free_image failed");

    return true;
}

/* Map an upload buffer, write, flush, unmap */
static bool test_map_write_flush_unmap(void) {
    VkBufferCreateInfo info = buffer_info(4096, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    LaharAllocationCreateInfo ainfo = alloc_info(LAHAR_MU_STAGING_SEQUENTIAL);
    VkBuffer buffer = VK_NULL_HANDLE;
    LaharAllocation alloc = {};

    tassert_ok(A()->alloc_buffer(A(), &info, &ainfo, &buffer, &alloc), "alloc_buffer failed");

    void* mapped = NULL;
    tassert_ok(A()->map(A(), alloc, &mapped), "map failed");
    tassert(mapped != NULL, "map succeeded but pointer is NULL");

    memset(mapped, 0xAB, 4096);

    tassert_ok(A()->flush(A(), alloc, 0, 4096), "flush failed");
    tassert_ok(A()->flush(A(), alloc, 0, VK_WHOLE_SIZE), "flush with VK_WHOLE_SIZE failed");
    tassert_ok(A()->flush(A(), alloc, 1024, 2048), "flush with offset failed");

    tassert_ok(A()->unmap(A(), alloc), "unmap failed");
    tassert_ok(A()->free_buffer(A(), buffer, alloc), "free_buffer failed");

    return true;
}

/* Written data must survive an unmap/remap cycle */
static bool test_data_persists_across_remap(void) {
    const uint64_t size = 1024;
    VkBufferCreateInfo info = buffer_info(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    LaharAllocationCreateInfo ainfo = alloc_info(LAHAR_MU_STAGING_SEQUENTIAL);
    VkBuffer buffer = VK_NULL_HANDLE;
    LaharAllocation alloc = {};

    tassert_ok(A()->alloc_buffer(A(), &info, &ainfo, &buffer, &alloc), "alloc_buffer failed");

    uint8_t* mapped = NULL;
    tassert_ok(A()->map(A(), alloc, (void**)&mapped), "map failed");
    for (uint64_t i = 0; i < size; i++) { mapped[i] = (uint8_t)(i * 7); }
    tassert_ok(A()->flush(A(), alloc, 0, VK_WHOLE_SIZE), "flush failed");
    tassert_ok(A()->unmap(A(), alloc), "unmap failed");

    mapped = NULL;
    tassert_ok(A()->map(A(), alloc, (void**)&mapped), "remap failed");
    for (uint64_t i = 0; i < size; i++) {
        if (mapped[i] != (uint8_t)(i * 7)) {
            printf("\tData mismatch at byte %lu: expected %u, got %u\n",
                (unsigned long)i, (uint8_t)(i * 7), mapped[i]);
            A()->unmap(A(), alloc);
            A()->free_buffer(A(), buffer, alloc);
            return false;
        }
    }
    tassert_ok(A()->unmap(A(), alloc), "unmap failed");
    tassert_ok(A()->free_buffer(A(), buffer, alloc), "free_buffer failed");

    return true;
}

/* Full GPU roundtrip: upload -> vkCmdCopyBuffer -> readback */
static bool test_gpu_copy_roundtrip(void) {
    const uint64_t size = 65536;

    VkBufferCreateInfo src_info = buffer_info(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    VkBufferCreateInfo dst_info = buffer_info(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    LaharAllocationCreateInfo src_ainfo = alloc_info(LAHAR_MU_STAGING_SEQUENTIAL);
    LaharAllocationCreateInfo dst_ainfo = alloc_info(LAHAR_MU_READBACK);

    VkBuffer src = VK_NULL_HANDLE, dst = VK_NULL_HANDLE;
    LaharAllocation src_alloc = {}, dst_alloc = {};

    tassert_ok(A()->alloc_buffer(A(), &src_info, &src_ainfo, &src, &src_alloc), "src alloc failed");
    tassert_ok(A()->alloc_buffer(A(), &dst_info, &dst_ainfo, &dst, &dst_alloc), "dst alloc failed");

    /* Fill the source with a pattern */
    uint8_t* mapped = NULL;
    tassert_ok(A()->map(A(), src_alloc, (void**)&mapped), "src map failed");
    for (uint64_t i = 0; i < size; i++) { mapped[i] = (uint8_t)(i ^ (i >> 8)); }
    tassert_ok(A()->flush(A(), src_alloc, 0, VK_WHOLE_SIZE), "src flush failed");
    tassert_ok(A()->unmap(A(), src_alloc), "src unmap failed");

    /* Copy on the GPU. Command pools are per-window state now and we run
     * headless, so the test owns a transient pool of its own. */
    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pool_info.queueFamilyIndex = lahar->physdev_info.graphics_queue_index;

    VkCommandPool pool = VK_NULL_HANDLE;
    tassert(vkCreateCommandPool(lahar->device, &pool_info, lahar->vkalloc, &pool) == VK_SUCCESS,
        "command pool creation failed");

    VkCommandBufferAllocateInfo cmd_info = {};
    cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_info.commandPool = pool;
    cmd_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_info.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    tassert(vkAllocateCommandBuffers(lahar->device, &cmd_info, &cmd) == VK_SUCCESS, "command buffer alloc failed");

    VkCommandBufferBeginInfo begin = {};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);

    VkBufferCopy region = { 0, 0, size };
    vkCmdCopyBuffer(cmd, src, dst, 1, &region);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    tassert(vkQueueSubmit(lahar->graphicsQueue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS, "queue submit failed");
    vkQueueWaitIdle(lahar->graphicsQueue);
    vkDestroyCommandPool(lahar->device, pool, lahar->vkalloc);

    /* Read it back and verify */
    mapped = NULL;
    tassert_ok(A()->map(A(), dst_alloc, (void**)&mapped), "dst map failed");

    bool ok = true;
    for (uint64_t i = 0; i < size; i++) {
        if (mapped[i] != (uint8_t)(i ^ (i >> 8))) {
            printf("\tRoundtrip mismatch at byte %lu: expected %u, got %u\n",
                (unsigned long)i, (uint8_t)(i ^ (i >> 8)), mapped[i]);
            ok = false;
            break;
        }
    }

    A()->unmap(A(), dst_alloc);
    A()->free_buffer(A(), src, src_alloc);
    A()->free_buffer(A(), dst, dst_alloc);

    return ok;
}

/* Many live allocations at once, freed out of order */
static bool test_many_allocations_interleaved(void) {
    enum { COUNT = 64 };
    VkBuffer buffers[COUNT];
    LaharAllocation allocs[COUNT];

#ifdef LAHAR_TEST_SPY
    const int allocs_before = g_dev_allocs;
#endif

    for (int i = 0; i < COUNT; i++) {
        /* Vary the sizes to stress suballocation */
        VkBufferCreateInfo info = buffer_info(256ull << (i % 6), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        LaharAllocationCreateInfo ainfo = alloc_info(LAHAR_MU_DEVICE_ONLY);
        buffers[i] = VK_NULL_HANDLE;
        allocs[i] = {};

        tassert_ok(A()->alloc_buffer(A(), &info, &ainfo, &buffers[i], &allocs[i]), "bulk alloc failed");
        tassert(buffers[i] != VK_NULL_HANDLE, "bulk alloc produced null buffer");
    }

    /* Free evens, then odds, so the allocator has to handle holes */
    for (int i = 0; i < COUNT; i += 2) {
        tassert_ok(A()->free_buffer(A(), buffers[i], allocs[i]), "bulk free (even) failed");
    }
    for (int i = 1; i < COUNT; i += 2) {
        tassert_ok(A()->free_buffer(A(), buffers[i], allocs[i]), "bulk free (odd) failed");
    }

#ifdef LAHAR_TEST_SPY
    /* A real suballocator must not do one device allocation per request.
     * These are all small (<= 8 KiB) buffers of one usage class. */
    const int delta = g_dev_allocs - allocs_before;
    tassert(delta < COUNT / 2, "allocator hit vkAllocateMemory per-request; not suballocating");
#endif

    return true;
}

/* Alloc a buffer + image simultaneously, free in reverse order */
static bool test_mixed_image_and_buffer(void) {
    VkBufferCreateInfo binfo = buffer_info(8192, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    VkImageCreateInfo iinfo = image_info(128, 128);

    LaharAllocationCreateInfo bainfo = alloc_info(LAHAR_MU_STAGING_SEQUENTIAL);
    LaharAllocationCreateInfo iainfo = alloc_info(LAHAR_MU_DEVICE_ONLY);

    VkBuffer buffer = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    LaharAllocation balloc = {}, ialloc = {};

    tassert_ok(A()->alloc_buffer(A(), &binfo, &bainfo, &buffer, &balloc), "alloc_buffer failed");
    tassert_ok(A()->alloc_image(A(), &iinfo, &iainfo, &image, &ialloc), "alloc_image failed");

    tassert_ok(A()->free_image(A(), image, ialloc), "free_image failed");
    tassert_ok(A()->free_buffer(A(), buffer, balloc), "free_buffer failed");

    return true;
}

/* Freeing every other allocation and then allocating something that only
 * fits if the freed neighbors merged proves coalescing actually works.
 * Without this, a broken merge still passes the interleaved test (it frees
 * everything and never reuses). */
static bool test_coalesce_and_reuse(void) {
    enum { COUNT = 16 };
    const VkDeviceSize small_size = 64 * 1024;
    VkBuffer buffers[COUNT];
    LaharAllocation allocs[COUNT];

    VkBufferCreateInfo info = buffer_info(small_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    LaharAllocationCreateInfo ainfo = alloc_info(LAHAR_MU_STAGING_SEQUENTIAL);

    for (int i = 0; i < COUNT; i++) {
        buffers[i] = VK_NULL_HANDLE;
        allocs[i] = {};
        tassert_ok(A()->alloc_buffer(A(), &info, &ainfo, &buffers[i], &allocs[i]), "setup alloc failed");
    }

#ifdef LAHAR_TEST_SPY
    const int allocs_before = g_dev_allocs;
#endif

    /* free a contiguous run in shuffled order; their blocks must merge */
    const int run_begin = 4, run_end = 12; /* 8 blocks = 512 KiB merged */
    const int order[] = { 7, 4, 10, 11, 5, 9, 6, 8 };

    for (size_t i = 0; i < sizeof(order) / sizeof(order[0]); i++) {
        const int j = order[i];
        tassert_ok(A()->free_buffer(A(), buffers[j], allocs[j]), "interleaved free failed");
        buffers[j] = VK_NULL_HANDLE;
    }

    /* now allocate 6x the small size: only fits the hole if merging worked.
     * (6x not 8x: the run's blocks may be padded, and the big request's own
     * alignment padding eats some of the hole too.) */
    VkBufferCreateInfo big_info = buffer_info(small_size * 6, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    VkBuffer big = VK_NULL_HANDLE;
    LaharAllocation big_alloc = {};

    tassert_ok(A()->alloc_buffer(A(), &big_info, &ainfo, &big, &big_alloc), "post-coalesce alloc failed");

#ifdef LAHAR_TEST_SPY
    tassert(g_dev_allocs == allocs_before,
        "allocation after coalescible frees hit vkAllocateMemory; blocks did not merge");
#endif

    tassert_ok(A()->free_buffer(A(), big, big_alloc), "big free failed");

    for (int i = 0; i < COUNT; i++) {
        if (buffers[i] == VK_NULL_HANDLE) { continue; }
        tassert_ok(A()->free_buffer(A(), buffers[i], allocs[i]), "cleanup free failed");
    }

    return true;
}

/* Two suballocations of one chunk mapped at once, written, unmapped in
 * reverse order. Exercises shared-mapping refcounting; also proves the
 * mapped pointers do not alias. */
static bool test_concurrent_maps(void) {
    VkBufferCreateInfo info = buffer_info(4096, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    LaharAllocationCreateInfo ainfo = alloc_info(LAHAR_MU_STAGING_SEQUENTIAL);

    VkBuffer buf_a = VK_NULL_HANDLE, buf_b = VK_NULL_HANDLE;
    LaharAllocation alloc_a = {}, alloc_b = {};

    tassert_ok(A()->alloc_buffer(A(), &info, &ainfo, &buf_a, &alloc_a), "alloc a failed");
    tassert_ok(A()->alloc_buffer(A(), &info, &ainfo, &buf_b, &alloc_b), "alloc b failed");

    void* map_a = NULL;
    void* map_b = NULL;

    tassert_ok(A()->map(A(), alloc_a, &map_a), "map a failed");
    tassert_ok(A()->map(A(), alloc_b, &map_b), "map b failed");
    tassert(map_a && map_b, "map succeeded but pointer is NULL");
    tassert(map_a != map_b, "two allocations mapped to the same pointer");

    memset(map_a, 0x11, 4096);
    memset(map_b, 0x22, 4096);

    /* writes through one mapping must not clobber the other */
    tassert(((uint8_t*)map_a)[0] == 0x11 && ((uint8_t*)map_a)[4095] == 0x11, "mapping a was clobbered");

    tassert_ok(A()->flush(A(), alloc_a, 0, VK_WHOLE_SIZE), "flush a failed");
    tassert_ok(A()->flush(A(), alloc_b, 0, VK_WHOLE_SIZE), "flush b failed");

    /* unmap in reverse order of mapping */
    tassert_ok(A()->unmap(A(), alloc_b), "unmap b failed");

    /* a's mapping must still be alive and writable after b unmaps */
    memset(map_a, 0x33, 4096);
    tassert_ok(A()->flush(A(), alloc_a, 0, VK_WHOLE_SIZE), "flush a after b unmap failed");

    tassert_ok(A()->unmap(A(), alloc_a), "unmap a failed");

    tassert_ok(A()->free_buffer(A(), buf_a, alloc_a), "free a failed");
    tassert_ok(A()->free_buffer(A(), buf_b, alloc_b), "free b failed");

    return true;
}

/* Interleave buffers (always linear) with optimal-tiling images. On devices
 * with bufferImageGranularity > 1, mixing classes within a page is UB; the
 * spy check proves the allocator keeps them apart at the memory-object level.
 * On granularity-1 devices there is no page-sharing hazard, so allocators are
 * free to mix classes in one VkDeviceMemory and the segregation check does
 * not apply. */
static bool test_granularity_interleave(void) {
    enum { PAIRS = 8 };
    VkBuffer buffers[PAIRS];
    VkImage images[PAIRS];
    LaharAllocation ballocs[PAIRS], iallocs[PAIRS];

#ifdef LAHAR_TEST_SPY
    spy_reset_bind_log();
#endif

    VkBufferCreateInfo binfo = buffer_info(4096, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    LaharAllocationCreateInfo bainfo = alloc_info(LAHAR_MU_DEVICE_ONLY);
    LaharAllocationCreateInfo iainfo = alloc_info(LAHAR_MU_DEVICE_ONLY);

    for (int i = 0; i < PAIRS; i++) {
        VkImageCreateInfo iinfo = image_info(32 + 16 * (uint32_t)i, 32);

        buffers[i] = VK_NULL_HANDLE;
        images[i] = VK_NULL_HANDLE;
        ballocs[i] = {};
        iallocs[i] = {};

        tassert_ok(A()->alloc_buffer(A(), &binfo, &bainfo, &buffers[i], &ballocs[i]), "alloc_buffer failed");
        tassert_ok(A()->alloc_image(A(), &iinfo, &iainfo, &images[i], &iallocs[i]), "alloc_image failed");
    }

#if defined(LAHAR_TEST_SPY) && defined(LAHAR_TEST_FREELIST)
    /* Freelist policy: linear and non-linear never share a VkDeviceMemory,
     * but only where the device requires it. At granularity 1 the freelist
     * intentionally mixes classes for occupancy, which is legal.
     * (Policy check, not contract: VMA may legally mix on granularity-1.) */
    if (lahar->physdev_info.properties.limits.bufferImageGranularity > 1) {
        for (int b = 0; b < g_buf_bind_count; b++) {
            for (int m = 0; m < g_img_bind_count; m++) {
                tassert(g_buf_bind_mems[b] != g_img_bind_mems[m],
                    "a VkDeviceMemory received both a buffer and an optimal-image bind");
            }
        }
    }
#endif

    /* free image-first then buffer-first, alternating, to churn both classes */
    for (int i = 0; i < PAIRS; i++) {
        if (i % 2 == 0) {
            tassert_ok(A()->free_image(A(), images[i], iallocs[i]), "free_image failed");
            tassert_ok(A()->free_buffer(A(), buffers[i], ballocs[i]), "free_buffer failed");
        }
        else {
            tassert_ok(A()->free_buffer(A(), buffers[i], ballocs[i]), "free_buffer failed");
            tassert_ok(A()->free_image(A(), images[i], iallocs[i]), "free_image failed");
        }
    }

    return true;
}

/* Flush/invalidate range handling: VK_WHOLE_SIZE from a nonzero offset,
 * ranges not aligned to nonCoherentAtomSize, and out-of-range rejection. */
static bool test_flush_ranges(void) {
    const VkDeviceSize size = 8192;
    VkBufferCreateInfo info = buffer_info(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    LaharAllocationCreateInfo ainfo = alloc_info(LAHAR_MU_STAGING_SEQUENTIAL);
    VkBuffer buffer = VK_NULL_HANDLE;
    LaharAllocation alloc = {};

    tassert_ok(A()->alloc_buffer(A(), &info, &ainfo, &buffer, &alloc), "alloc failed");

    void* mapped = NULL;
    tassert_ok(A()->map(A(), alloc, &mapped), "map failed");
    memset(mapped, 0x5A, size);

    tassert_ok(A()->flush(A(), alloc, 4096, VK_WHOLE_SIZE), "flush WHOLE_SIZE from offset failed");
    tassert_ok(A()->flush(A(), alloc, 1, 1), "flush of unaligned 1-byte range failed");
    tassert_ok(A()->flush(A(), alloc, 4095, 2), "flush straddling an atom boundary failed");
    tassert_ok(A()->flush(A(), alloc, size, 0), "flush of empty range at end failed");
    tassert_ok(A()->invalidate(A(), alloc, 4096, VK_WHOLE_SIZE), "invalidate WHOLE_SIZE from offset failed");
    tassert_ok(A()->invalidate(A(), alloc, 1, 1), "invalidate of unaligned range failed");

#ifdef LAHAR_TEST_FREELIST
    /* Freelist-only: rejection of out-of-range flushes is stricter than the
     * vtable contract. VMA clamps such ranges and succeeds. */
    tassert(A()->flush(A(), alloc, size + 1, VK_WHOLE_SIZE) != LAHAR_ERR_SUCCESS,
        "flush accepted offset beyond the allocation");
    tassert(A()->flush(A(), alloc, 0, size * 4) != LAHAR_ERR_SUCCESS,
        "flush accepted size beyond the allocation");
#endif

    tassert_ok(A()->unmap(A(), alloc), "unmap failed");
    tassert_ok(A()->free_buffer(A(), buffer, alloc), "free failed");

    return true;
}

/* Alignment stress: interleave tiny buffers with buffers whose usage flags
 * demand the device's worst-case alignments (uniform/storage/texel), then
 * verify via map that each got a distinct, working placement. The bind spy
 * independently checks every offset against the driver's requirements. */
static bool test_alignment_stress(void) {
    static const VkBufferUsageFlags usages[] = {
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };
    /* deliberately awkward sizes so blocks end at unaligned-ish offsets */
    static const VkDeviceSize sizes[] = { 257, 4096, 33, 1025, 512, 65, 16384, 100 };

    enum { COUNT = sizeof(usages) / sizeof(usages[0]) };
    VkBuffer buffers[COUNT];
    LaharAllocation allocs[COUNT];

    LaharAllocationCreateInfo ainfo = alloc_info(LAHAR_MU_STAGING_SEQUENTIAL);

    for (int i = 0; i < COUNT; i++) {
        VkBufferCreateInfo info = buffer_info(sizes[i], usages[i]);
        buffers[i] = VK_NULL_HANDLE;
        allocs[i] = {};
        tassert_ok(A()->alloc_buffer(A(), &info, &ainfo, &buffers[i], &allocs[i]), "alloc failed");
    }

    /* write a distinct byte through each mapping; verify all after, so any
     * block overlap from bad alignment padding shows up as a stomp */
    void* maps[COUNT];

    for (int i = 0; i < COUNT; i++) {
        maps[i] = NULL;
        tassert_ok(A()->map(A(), allocs[i], &maps[i]), "map failed");
        memset(maps[i], 0x40 + i, (size_t)sizes[i]);
    }

    for (int i = 0; i < COUNT; i++) {
        const uint8_t* p = (const uint8_t*)maps[i];
        tassert(p[0] == 0x40 + i && p[sizes[i] - 1] == 0x40 + i,
            "allocation contents stomped by a neighbor; blocks overlap");
        tassert_ok(A()->unmap(A(), allocs[i]), "unmap failed");
    }

    /* free odd-first to churn the coalescer with mixed-size neighbors */
    for (int i = 1; i < COUNT; i += 2) {
        tassert_ok(A()->free_buffer(A(), buffers[i], allocs[i]), "free failed");
    }
    for (int i = 0; i < COUNT; i += 2) {
        tassert_ok(A()->free_buffer(A(), buffers[i], allocs[i]), "free failed");
    }

    return true;
}

#ifdef LAHAR_TEST_FREELIST
/* Unmap with no live map must be rejected, not underflow a refcount.
 * Freelist-only: this is stricter than the vtable contract. VMA treats
 * unbalanced unmap as caller UB and asserts in debug builds. */
static bool test_unmap_without_map(void) {
    VkBufferCreateInfo info = buffer_info(1024, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    LaharAllocationCreateInfo ainfo = alloc_info(LAHAR_MU_STAGING_SEQUENTIAL);
    VkBuffer buffer = VK_NULL_HANDLE;
    LaharAllocation alloc = {};

    tassert_ok(A()->alloc_buffer(A(), &info, &ainfo, &buffer, &alloc), "alloc failed");

    tassert(A()->unmap(A(), alloc) != LAHAR_ERR_SUCCESS, "unmap without map succeeded");

    /* and the rejection must not have poisoned real map/unmap */
    void* mapped = NULL;
    tassert_ok(A()->map(A(), alloc, &mapped), "map after bogus unmap failed");
    tassert_ok(A()->unmap(A(), alloc), "unmap after bogus unmap failed");
    tassert(A()->unmap(A(), alloc) != LAHAR_ERR_SUCCESS, "double unmap succeeded");

    tassert_ok(A()->free_buffer(A(), buffer, alloc), "free failed");

    return true;
}
#endif /* LAHAR_TEST_FREELIST */

/* The vtable must reject garbage parameters instead of crashing */
static bool test_rejects_bad_params(void) {
    VkBufferCreateInfo binfo = buffer_info(1024, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    VkImageCreateInfo iinfo = image_info(16, 16);
    LaharAllocationCreateInfo bainfo = alloc_info(LAHAR_MU_STAGING_SEQUENTIAL);
    LaharAllocationCreateInfo iainfo = alloc_info(LAHAR_MU_DEVICE_ONLY);
    VkBuffer buffer = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    LaharAllocation alloc = {};

    tassert(A()->alloc_buffer(A(), NULL, &bainfo, &buffer, &alloc) != LAHAR_ERR_SUCCESS,
        "alloc_buffer accepted NULL info");
    tassert(A()->alloc_buffer(A(), &binfo, &bainfo, NULL, &alloc) != LAHAR_ERR_SUCCESS,
        "alloc_buffer accepted NULL buffer out");
    tassert(A()->alloc_buffer(A(), &binfo, &bainfo, &buffer, NULL) != LAHAR_ERR_SUCCESS,
        "alloc_buffer accepted NULL allocation out");
    tassert(A()->alloc_buffer(A(), &binfo, NULL, &buffer, &alloc) != LAHAR_ERR_SUCCESS,
        "alloc_buffer accepted NULL allocation create info");

    tassert(A()->alloc_image(A(), NULL, &iainfo, &image, &alloc) != LAHAR_ERR_SUCCESS,
        "alloc_image accepted NULL info");
    tassert(A()->alloc_image(A(), &iinfo, &iainfo, NULL, &alloc) != LAHAR_ERR_SUCCESS,
        "alloc_image accepted NULL image out");
    tassert(A()->alloc_image(A(), &iinfo, &iainfo, &image, NULL) != LAHAR_ERR_SUCCESS,
        "alloc_image accepted NULL allocation out");
    tassert(A()->alloc_image(A(), &iinfo, NULL, &image, &alloc) != LAHAR_ERR_SUCCESS,
        "alloc_image accepted NULL allocation create info");

    tassert(A()->free_buffer(A(), NULL, alloc) != LAHAR_ERR_SUCCESS,
        "free_buffer accepted NULL buffer");
    tassert(A()->free_image(A(), NULL, alloc) != LAHAR_ERR_SUCCESS,
        "free_image accepted NULL image");

    LaharAllocation real = {};
    tassert_ok(A()->alloc_buffer(A(), &binfo, &bainfo, &buffer, &real), "setup alloc failed");
    tassert(A()->map(A(), real, NULL) != LAHAR_ERR_SUCCESS, "map accepted NULL out");
    tassert_ok(A()->free_buffer(A(), buffer, real), "cleanup free failed");

    /* NULL self should be rejected, not dereferenced */
    tassert(A()->alloc_buffer(NULL, &binfo, &bainfo, &buffer, &alloc) != LAHAR_ERR_SUCCESS,
        "alloc_buffer accepted NULL self");

    return true;
}

/* An unknown memory usage value must be rejected */
static bool test_rejects_bad_usage(void) {
    VkBufferCreateInfo info = buffer_info(1024, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    LaharAllocationCreateInfo ainfo = alloc_info((LaharMemoryUsage)0xDEAD);
    VkBuffer buffer = VK_NULL_HANDLE;
    LaharAllocation alloc = {};

    tassert(A()->alloc_buffer(A(), &info, &ainfo, &buffer, &alloc) != LAHAR_ERR_SUCCESS,
        "alloc_buffer accepted a garbage LaharMemoryUsage");
    tassert(buffer == VK_NULL_HANDLE, "failed alloc still wrote a buffer handle");

    return true;
}

/* ----------------------------------------------------------------- runner */

typedef bool (*TestFunc)(void);

typedef struct {
    const char* name;
    TestFunc func;
} TestCase;

static const TestCase tests[] = {
    { "vtable is complete",                 test_vtable_complete },
    { "buffer alloc/free, every usage",     test_buffer_alloc_free_all_usages },
    { "image alloc/free",                   test_image_alloc_free },
    { "map/write/flush/unmap",              test_map_write_flush_unmap },
    { "data persists across remap",         test_data_persists_across_remap },
    { "gpu copy roundtrip",                 test_gpu_copy_roundtrip },
    { "many interleaved allocations",       test_many_allocations_interleaved },
    { "mixed image + buffer",               test_mixed_image_and_buffer },
    { "coalesce and reuse",                 test_coalesce_and_reuse },
    { "concurrent maps on one chunk",       test_concurrent_maps },
    { "granularity class interleave",       test_granularity_interleave },
    { "flush/invalidate ranges",            test_flush_ranges },
    { "alignment stress",                   test_alignment_stress },
#ifdef LAHAR_TEST_FREELIST
    { "unmap without map",                  test_unmap_without_map },
#endif
    { "rejects bad params",                 test_rejects_bad_params },
    { "rejects bad usage enum",             test_rejects_bad_usage },
};

int main(void) {
    uint32_t err;

    if ((err = lahar_init())) {
        printf("Lahar failed to init: %s\n", lahar_err_name(err));
        return 1;
    }

    /* Warn level so leak reports are visible in the output */
    lahar_builder_set_debug_level(LAHAR_DEBUG_WARNING);

#ifdef LAHAR_TEST_FREELIST
    custom_allocator = lahar_allocator_freelist_init();
    if (!custom_allocator) {
        printf("Failed to create freelist allocator\n");
        return 1;
    }
#endif

    if (custom_allocator) {
        if ((err = lahar_builder_allocator_set(custom_allocator))) {
            printf("Failed to set custom allocator: %s\n", lahar_err_name(err));
            return 1;
        }
    }

    if ((err = lahar_build())) {
        printf("Lahar failed to build: %s\n", lahar_err_name(err));
        return 1;
    }

    printf("Device: %s\n", lahar->physdev_info.properties.deviceName);
    printf("Allocator: %s\n\n", allocator_name);

#ifdef LAHAR_TEST_SPY
    spy_install();
#endif

    int failed = 0;
    const size_t count = sizeof(tests) / sizeof(tests[0]);

    for (size_t i = 0; i < count; i++) {
        printf("[%zu/%zu] %s\n", i + 1, count, tests[i].name);

        if (!tests[i].func()) {
            printf("\tFAILED\n");
            failed++;
        }
    }

    vkDeviceWaitIdle(lahar->device);


#ifdef LAHAR_TEST_FREELIST
    lahar_allocator_freelist_deinit(custom_allocator);
#endif

#ifdef LAHAR_TEST_SPY
    /* Backend teardown is done (freelist deinit above runs before device
     * destruction in lahar_deinit below): every device allocation made while
     * the spy was installed must have been returned by now. */
    printf("\nSpy: %d device allocs, %d frees, %d violations\n",
        g_dev_allocs, g_dev_frees, g_spy_violations);

    if (g_dev_allocs != g_dev_frees) {
        printf("Spy: vkAllocateMemory/vkFreeMemory mismatch -- leaked VkDeviceMemory\n");
        failed++;
    }

    failed += g_spy_violations;
#endif

    lahar_deinit();

    printf("\n%zu/%zu tests passed\n", count - failed, count);

    return failed ? 1 : 0;
}
