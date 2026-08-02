/* Test suite for the LaharAllocator vtable.
 *
 * These tests intentionally go through the lahar->gpu_allocator function
 * pointers rather than calling any backend directly, so the same suite can
 * be pointed at any LaharAllocator implementation. Select the backend at
 * compile time:
 *
 *   -DTEST_ALLOCATOR_VMA        VMA-backed adapter (default)
 *   -DTEST_ALLOCATOR_FREELIST   lahar's native freelist allocator
 *
 * To test your own implementation, add a branch below that points
 * custom_allocator at it.
 *
 * Runs fully headless: no window is registered, so it works on a pure
 * compute/llvmpipe setup.
 */

#if !defined(TEST_ALLOCATOR_VMA) && !defined(TEST_ALLOCATOR_FREELIST)
    #define TEST_ALLOCATOR_VMA
#endif

#if defined(TEST_ALLOCATOR_VMA)
    #define LAHAR_USE_VMA
    #define VMA_IMPLEMENTATION
#endif

#define LAHAR_IMPLEMENTATION
#include "../lahar.h"

#include <stdio.h>
#include <string.h>

/* The allocator under test. NULL means lahar's built-in default. */
#if defined(TEST_ALLOCATOR_FREELIST)
static LaharAllocator* custom_allocator = lahar_allocator_freelist();
static const char* allocator_name = "freelist";
#else
static LaharAllocator* custom_allocator = NULL;
static const char* allocator_name = "VMA";
#endif

#define tassert(cond, msg) \
    do { if (!(cond)) { printf("\tAssert failed (%s:%d): %s\n", __FILE__, __LINE__, msg); return false; } } while (0)

#define tassert_ok(expr, msg) \
    do { uint32_t __e = (expr); if (__e != LAHAR_ERR_SUCCESS) { \
        printf("\tAssert failed (%s:%d): %s -> %s\n", __FILE__, __LINE__, msg, lahar_err_name(__e)); return false; } } while (0)

static LaharAllocator* A(void) { return lahar->gpu_allocator; }

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
    return true;
}

/* Simple buffer allocation + free for every memory usage class */
static bool test_buffer_alloc_free_all_usages(void) {
    const LaharMemoryUsage usages[] = {
        LAHAR_MU_GPU_ONLY, LAHAR_MU_UPLOAD, LAHAR_MU_UPLOAD_DEVICE, LAHAR_MU_READBACK
    };

    for (size_t i = 0; i < sizeof(usages) / sizeof(usages[0]); i++) {
        VkBufferCreateInfo info = buffer_info(4096, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        VkBuffer buffer = VK_NULL_HANDLE;
        LaharAllocation alloc = {};

        tassert_ok(A()->alloc_buffer(A(), lahar, &info, usages[i], &buffer, &alloc), "alloc_buffer failed");
        tassert(buffer != VK_NULL_HANDLE, "alloc_buffer succeeded but buffer is VK_NULL_HANDLE");

        tassert_ok(A()->free_buffer(A(), lahar, &buffer, &alloc), "free_buffer failed");
    }

    return true;
}

/* Image allocation + free */
static bool test_image_alloc_free(void) {
    VkImageCreateInfo info = image_info(64, 64);
    VkImage image = VK_NULL_HANDLE;
    LaharAllocation alloc = {};

    tassert_ok(A()->alloc_image(A(), lahar, &info, &image, &alloc), "alloc_image failed");
    tassert(image != VK_NULL_HANDLE, "alloc_image succeeded but image is VK_NULL_HANDLE");

    tassert_ok(A()->free_image(A(), lahar, &image, &alloc), "free_image failed");

    return true;
}

/* Map an upload buffer, write, flush, unmap */
static bool test_map_write_flush_unmap(void) {
    VkBufferCreateInfo info = buffer_info(4096, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    VkBuffer buffer = VK_NULL_HANDLE;
    LaharAllocation alloc = {};

    tassert_ok(A()->alloc_buffer(A(), lahar, &info, LAHAR_MU_UPLOAD, &buffer, &alloc), "alloc_buffer failed");

    void* mapped = NULL;
    tassert_ok(A()->map(A(), lahar, alloc, &mapped), "map failed");
    tassert(mapped != NULL, "map succeeded but pointer is NULL");

    memset(mapped, 0xAB, 4096);

    tassert_ok(A()->flush(A(), lahar, alloc, 0, 4096), "flush failed");
    tassert_ok(A()->flush(A(), lahar, alloc, 0, VK_WHOLE_SIZE), "flush with VK_WHOLE_SIZE failed");
    tassert_ok(A()->flush(A(), lahar, alloc, 1024, 2048), "flush with offset failed");

    tassert_ok(A()->unmap(A(), lahar, alloc), "unmap failed");
    tassert_ok(A()->free_buffer(A(), lahar, &buffer, &alloc), "free_buffer failed");

    return true;
}

/* Written data must survive an unmap/remap cycle */
static bool test_data_persists_across_remap(void) {
    const uint64_t size = 1024;
    VkBufferCreateInfo info = buffer_info(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    VkBuffer buffer = VK_NULL_HANDLE;
    LaharAllocation alloc = {};

    tassert_ok(A()->alloc_buffer(A(), lahar, &info, LAHAR_MU_UPLOAD, &buffer, &alloc), "alloc_buffer failed");

    uint8_t* mapped = NULL;
    tassert_ok(A()->map(A(), lahar, alloc, (void**)&mapped), "map failed");
    for (uint64_t i = 0; i < size; i++) { mapped[i] = (uint8_t)(i * 7); }
    tassert_ok(A()->flush(A(), lahar, alloc, 0, VK_WHOLE_SIZE), "flush failed");
    tassert_ok(A()->unmap(A(), lahar, alloc), "unmap failed");

    mapped = NULL;
    tassert_ok(A()->map(A(), lahar, alloc, (void**)&mapped), "remap failed");
    for (uint64_t i = 0; i < size; i++) {
        if (mapped[i] != (uint8_t)(i * 7)) {
            printf("\tData mismatch at byte %lu: expected %u, got %u\n",
                (unsigned long)i, (uint8_t)(i * 7), mapped[i]);
            A()->unmap(A(), lahar, alloc);
            A()->free_buffer(A(), lahar, &buffer, &alloc);
            return false;
        }
    }
    tassert_ok(A()->unmap(A(), lahar, alloc), "unmap failed");
    tassert_ok(A()->free_buffer(A(), lahar, &buffer, &alloc), "free_buffer failed");

    return true;
}

/* Full GPU roundtrip: upload -> vkCmdCopyBuffer -> readback */
static bool test_gpu_copy_roundtrip(void) {
    const uint64_t size = 65536;

    VkBufferCreateInfo src_info = buffer_info(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    VkBufferCreateInfo dst_info = buffer_info(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    VkBuffer src = VK_NULL_HANDLE, dst = VK_NULL_HANDLE;
    LaharAllocation src_alloc = {}, dst_alloc = {};

    tassert_ok(A()->alloc_buffer(A(), lahar, &src_info, LAHAR_MU_UPLOAD, &src, &src_alloc), "src alloc failed");
    tassert_ok(A()->alloc_buffer(A(), lahar, &dst_info, LAHAR_MU_READBACK, &dst, &dst_alloc), "dst alloc failed");

    /* Fill the source with a pattern */
    uint8_t* mapped = NULL;
    tassert_ok(A()->map(A(), lahar, src_alloc, (void**)&mapped), "src map failed");
    for (uint64_t i = 0; i < size; i++) { mapped[i] = (uint8_t)(i ^ (i >> 8)); }
    tassert_ok(A()->flush(A(), lahar, src_alloc, 0, VK_WHOLE_SIZE), "src flush failed");
    tassert_ok(A()->unmap(A(), lahar, src_alloc), "src unmap failed");

    /* Copy on the GPU. The command pool exists because we requested it pre-build */
    VkCommandBufferAllocateInfo cmd_info = {};
    cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_info.commandPool = lahar->pool;
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
    vkFreeCommandBuffers(lahar->device, lahar->pool, 1, &cmd);

    /* Read it back and verify */
    mapped = NULL;
    tassert_ok(A()->map(A(), lahar, dst_alloc, (void**)&mapped), "dst map failed");

    bool ok = true;
    for (uint64_t i = 0; i < size; i++) {
        if (mapped[i] != (uint8_t)(i ^ (i >> 8))) {
            printf("\tRoundtrip mismatch at byte %lu: expected %u, got %u\n",
                (unsigned long)i, (uint8_t)(i ^ (i >> 8)), mapped[i]);
            ok = false;
            break;
        }
    }

    A()->unmap(A(), lahar, dst_alloc);
    A()->free_buffer(A(), lahar, &src, &src_alloc);
    A()->free_buffer(A(), lahar, &dst, &dst_alloc);

    return ok;
}

/* Many live allocations at once, freed out of order */
static bool test_many_allocations_interleaved(void) {
    enum { COUNT = 64 };
    VkBuffer buffers[COUNT];
    LaharAllocation allocs[COUNT];

    for (int i = 0; i < COUNT; i++) {
        /* Vary the sizes to stress suballocation */
        VkBufferCreateInfo info = buffer_info(256ull << (i % 6), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        buffers[i] = VK_NULL_HANDLE;
        allocs[i] = {};

        tassert_ok(A()->alloc_buffer(A(), lahar, &info, LAHAR_MU_GPU_ONLY, &buffers[i], &allocs[i]), "bulk alloc failed");
        tassert(buffers[i] != VK_NULL_HANDLE, "bulk alloc produced null buffer");
    }

    /* Free evens, then odds, so the allocator has to handle holes */
    for (int i = 0; i < COUNT; i += 2) {
        tassert_ok(A()->free_buffer(A(), lahar, &buffers[i], &allocs[i]), "bulk free (even) failed");
    }
    for (int i = 1; i < COUNT; i += 2) {
        tassert_ok(A()->free_buffer(A(), lahar, &buffers[i], &allocs[i]), "bulk free (odd) failed");
    }

    return true;
}

/* Alloc a buffer + image simultaneously, free in reverse order */
static bool test_mixed_image_and_buffer(void) {
    VkBufferCreateInfo binfo = buffer_info(8192, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    VkImageCreateInfo iinfo = image_info(128, 128);

    VkBuffer buffer = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    LaharAllocation balloc = {}, ialloc = {};

    tassert_ok(A()->alloc_buffer(A(), lahar, &binfo, LAHAR_MU_UPLOAD, &buffer, &balloc), "alloc_buffer failed");
    tassert_ok(A()->alloc_image(A(), lahar, &iinfo, &image, &ialloc), "alloc_image failed");

    tassert_ok(A()->free_image(A(), lahar, &image, &ialloc), "free_image failed");
    tassert_ok(A()->free_buffer(A(), lahar, &buffer, &balloc), "free_buffer failed");

    return true;
}

/* The vtable must reject garbage parameters instead of crashing */
static bool test_rejects_bad_params(void) {
    VkBufferCreateInfo binfo = buffer_info(1024, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    VkImageCreateInfo iinfo = image_info(16, 16);
    VkBuffer buffer = VK_NULL_HANDLE;
    VkImage image = VK_NULL_HANDLE;
    LaharAllocation alloc = {};
    void* mapped = NULL;

    tassert(A()->alloc_buffer(A(), lahar, NULL, LAHAR_MU_UPLOAD, &buffer, &alloc) != LAHAR_ERR_SUCCESS,
        "alloc_buffer accepted NULL info");
    tassert(A()->alloc_buffer(A(), lahar, &binfo, LAHAR_MU_UPLOAD, NULL, &alloc) != LAHAR_ERR_SUCCESS,
        "alloc_buffer accepted NULL buffer out");
    tassert(A()->alloc_buffer(A(), lahar, &binfo, LAHAR_MU_UPLOAD, &buffer, NULL) != LAHAR_ERR_SUCCESS,
        "alloc_buffer accepted NULL allocation out");

    tassert(A()->alloc_image(A(), lahar, NULL, &image, &alloc) != LAHAR_ERR_SUCCESS,
        "alloc_image accepted NULL info");
    tassert(A()->alloc_image(A(), lahar, &iinfo, NULL, &alloc) != LAHAR_ERR_SUCCESS,
        "alloc_image accepted NULL image out");
    tassert(A()->alloc_image(A(), lahar, &iinfo, &image, NULL) != LAHAR_ERR_SUCCESS,
        "alloc_image accepted NULL allocation out");

    tassert(A()->free_buffer(A(), lahar, NULL, &alloc) != LAHAR_ERR_SUCCESS,
        "free_buffer accepted NULL buffer");
    tassert(A()->free_image(A(), lahar, NULL, &alloc) != LAHAR_ERR_SUCCESS,
        "free_image accepted NULL image");

    LaharAllocation real = {};
    tassert_ok(A()->alloc_buffer(A(), lahar, &binfo, LAHAR_MU_UPLOAD, &buffer, &real), "setup alloc failed");
    tassert(A()->map(A(), lahar, real, NULL) != LAHAR_ERR_SUCCESS, "map accepted NULL out");
    tassert_ok(A()->free_buffer(A(), lahar, &buffer, &real), "cleanup free failed");

    /* NULL self should be rejected, not dereferenced */
    tassert(A()->alloc_buffer(NULL, lahar, &binfo, LAHAR_MU_UPLOAD, &buffer, &alloc) != LAHAR_ERR_SUCCESS,
        "alloc_buffer accepted NULL self");
    tassert(A()->alloc_buffer(A(), NULL, &binfo, LAHAR_MU_UPLOAD, &buffer, &alloc) != LAHAR_ERR_SUCCESS,
        "alloc_buffer accepted NULL lahar");
    (void)mapped;

    return true;
}

/* An unknown memory usage value must be rejected */
static bool test_rejects_bad_usage(void) {
    VkBufferCreateInfo info = buffer_info(1024, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    VkBuffer buffer = VK_NULL_HANDLE;
    LaharAllocation alloc = {};

    tassert(A()->alloc_buffer(A(), lahar, &info, (LaharMemoryUsage)0xDEAD, &buffer, &alloc) != LAHAR_ERR_SUCCESS,
        "alloc_buffer accepted a garbage LaharMemoryUsage");
    tassert(buffer == VK_NULL_HANDLE, "failed alloc still wrote a buffer handle");

    return true;
}

/* ----------------------------------------------- freelist-specific tests */

#if defined(TEST_ALLOCATOR_FREELIST)

/* Stats must track alloc/free exactly */
static bool test_fl_stats_track_allocs(void) {
    LaharFreelistStats before = {}, during = {}, after = {};
    tassert_ok(lahar_freelist_stats(&before), "stats query failed");

    VkBufferCreateInfo info = buffer_info(4096, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    VkBuffer buffer = VK_NULL_HANDLE;
    LaharAllocation alloc = {};

    tassert_ok(A()->alloc_buffer(A(), lahar, &info, LAHAR_MU_UPLOAD, &buffer, &alloc), "alloc failed");
    tassert_ok(lahar_freelist_stats(&during), "stats query failed");

    tassert(during.live_allocations == before.live_allocations + 1, "live count didn't go up by one");
    tassert(during.used >= before.used + 4096, "used didn't grow by at least the request");
    tassert(during.reserved >= during.used, "reserved is smaller than used");

    tassert_ok(A()->free_buffer(A(), lahar, &buffer, &alloc), "free failed");
    tassert_ok(lahar_freelist_stats(&after), "stats query failed");

    tassert(after.live_allocations == before.live_allocations, "live count didn't return to baseline");
    tassert(after.used == before.used, "used didn't return to baseline");

    return true;
}

/* Allocating more than one block's worth must spill into new blocks,
 * and freeing everything must release the blocks back to the driver */
static bool test_fl_multi_block_spill(void) {
    /* Big enough that a few overflow a block, small enough to stay
     * suballocated (threshold is BLOCK_SIZE / 4) */
    const VkDeviceSize chunk = LAHAR_FL_BLOCK_SIZE / 5;
    enum { COUNT = 12 };

    VkBuffer buffers[COUNT];
    LaharAllocation allocs[COUNT];

    LaharFreelistStats before = {};
    tassert_ok(lahar_freelist_stats(&before), "stats query failed");

    for (int i = 0; i < COUNT; i++) {
        VkBufferCreateInfo info = buffer_info(chunk, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        buffers[i] = VK_NULL_HANDLE;
        allocs[i] = {};

        tassert_ok(A()->alloc_buffer(A(), lahar, &info, LAHAR_MU_GPU_ONLY, &buffers[i], &allocs[i]), "spill alloc failed");
    }

    LaharFreelistStats during = {};
    tassert_ok(lahar_freelist_stats(&during), "stats query failed");
    tassert(during.block_count > before.block_count + 1, "12 x BLOCK/5 didn't spill past one new block");
    tassert(during.dedicated_count == before.dedicated_count, "sub-threshold alloc went dedicated");

    for (int i = 0; i < COUNT; i++) {
        tassert_ok(A()->free_buffer(A(), lahar, &buffers[i], &allocs[i]), "spill free failed");
    }

    LaharFreelistStats after = {};
    tassert_ok(lahar_freelist_stats(&after), "stats query failed");
    tassert(after.block_count == before.block_count, "empty blocks weren't released");
    tassert(after.reserved == before.reserved, "reserved didn't return to baseline");

    return true;
}

/* Past the threshold, allocations must go dedicated */
static bool test_fl_dedicated_threshold(void) {
    LaharFreelistStats before = {};
    tassert_ok(lahar_freelist_stats(&before), "stats query failed");

    VkBufferCreateInfo info = buffer_info(LAHAR_FL_DEDICATED_THRESHOLD, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    VkBuffer buffer = VK_NULL_HANDLE;
    LaharAllocation alloc = {};

    tassert_ok(A()->alloc_buffer(A(), lahar, &info, LAHAR_MU_GPU_ONLY, &buffer, &alloc), "dedicated alloc failed");

    LaharFreelistStats during = {};
    tassert_ok(lahar_freelist_stats(&during), "stats query failed");
    tassert(during.dedicated_count == before.dedicated_count + 1, "threshold-size alloc wasn't dedicated");
    tassert(during.block_count == before.block_count, "dedicated alloc created a block");

    /* Dedicated host-visible memory must still map/flush correctly */
    VkBufferCreateInfo hinfo = buffer_info(LAHAR_FL_DEDICATED_THRESHOLD, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    VkBuffer hbuffer = VK_NULL_HANDLE;
    LaharAllocation halloc = {};

    tassert_ok(A()->alloc_buffer(A(), lahar, &hinfo, LAHAR_MU_UPLOAD, &hbuffer, &halloc), "dedicated upload alloc failed");

    void* mapped = NULL;
    tassert_ok(A()->map(A(), lahar, halloc, &mapped), "dedicated map failed");
    tassert(mapped != NULL, "dedicated map returned NULL");
    memset(mapped, 0x5A, 4096);
    tassert_ok(A()->flush(A(), lahar, halloc, 0, VK_WHOLE_SIZE), "dedicated flush failed");
    tassert_ok(A()->unmap(A(), lahar, halloc), "dedicated unmap failed");

    tassert_ok(A()->free_buffer(A(), lahar, &hbuffer, &halloc), "dedicated upload free failed");
    tassert_ok(A()->free_buffer(A(), lahar, &buffer, &alloc), "dedicated free failed");

    LaharFreelistStats after = {};
    tassert_ok(lahar_freelist_stats(&after), "stats query failed");
    tassert(after.dedicated_count == before.dedicated_count, "dedicated count didn't return to baseline");

    return true;
}

/* Coalescing: free half a block's worth in pieces, then one allocation
 * must be able to span the reunified space */
static bool test_fl_coalesce_regrow(void) {
    const VkDeviceSize piece = 1024 * 1024;
    enum { COUNT = 8 };

    VkBuffer buffers[COUNT];
    LaharAllocation allocs[COUNT];

    for (int i = 0; i < COUNT; i++) {
        VkBufferCreateInfo info = buffer_info(piece, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        buffers[i] = VK_NULL_HANDLE;
        allocs[i] = {};
        tassert_ok(A()->alloc_buffer(A(), lahar, &info, LAHAR_MU_GPU_ONLY, &buffers[i], &allocs[i]), "piece alloc failed");
    }

    LaharFreelistStats saturated = {};
    tassert_ok(lahar_freelist_stats(&saturated), "stats query failed");

    /* Free all pieces out of order; the holes must coalesce */
    for (int i = 0; i < COUNT; i += 2) {
        tassert_ok(A()->free_buffer(A(), lahar, &buffers[i], &allocs[i]), "piece free failed");
    }
    for (int i = 1; i < COUNT; i += 2) {
        tassert_ok(A()->free_buffer(A(), lahar, &buffers[i], &allocs[i]), "piece free failed");
    }

    /* One allocation the size of all pieces combined must not need a new
     * block (nor go dedicated -- 8MB is under the threshold) */
    VkBufferCreateInfo big = buffer_info(piece * COUNT, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    VkBuffer bigbuf = VK_NULL_HANDLE;
    LaharAllocation bigalloc = {};

    tassert_ok(A()->alloc_buffer(A(), lahar, &big, LAHAR_MU_GPU_ONLY, &bigbuf, &bigalloc), "coalesced alloc failed");

    LaharFreelistStats after = {};
    tassert_ok(lahar_freelist_stats(&after), "stats query failed");
    tassert(after.block_count <= saturated.block_count, "coalesced alloc created a new block; holes didn't merge");

    tassert_ok(A()->free_buffer(A(), lahar, &bigbuf, &bigalloc), "coalesced free failed");

    return true;
}

/* Allocation naming: must accept a name, reject NULL alloc */
static bool test_fl_allocation_name(void) {
    VkBufferCreateInfo info = buffer_info(1024, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    VkBuffer buffer = VK_NULL_HANDLE;
    LaharAllocation alloc = {};

    tassert_ok(A()->alloc_buffer(A(), lahar, &info, LAHAR_MU_UPLOAD, &buffer, &alloc), "alloc failed");

    tassert_ok(lahar_freelist_allocation_name(alloc, "test buffer"), "naming failed");
    tassert(lahar_freelist_allocation_name(NULL, "nope") != LAHAR_ERR_SUCCESS, "named a NULL allocation");

    tassert_ok(A()->free_buffer(A(), lahar, &buffer, &alloc), "free failed");

    return true;
}

/* Deliberately leak a named allocation and verify the deinit leak report
 * catches it. Runs at the very end (see main), after the regular suite */
static bool leak_check(void) {
    VkBufferCreateInfo info = buffer_info(2048, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    VkBuffer buffer = VK_NULL_HANDLE;
    LaharAllocation alloc = {};

    if (A()->alloc_buffer(A(), lahar, &info, LAHAR_MU_UPLOAD, &buffer, &alloc) != LAHAR_ERR_SUCCESS) {
        printf("\tleak check setup alloc failed\n");
        return false;
    }

    lahar_freelist_allocation_name(alloc, "deliberate leak (this warning is expected)");

    LaharFreelistStats stats = {};
    lahar_freelist_stats(&stats);

    if (stats.live_allocations != 1) {
        printf("\tExpected exactly 1 live allocation before deinit, got %llu\n",
            (unsigned long long)stats.live_allocations);
        return false;
    }

    /* The buffer handle is leaked too; destroy it so validation stays quiet
     * about the VkBuffer and the report is purely about the memory */
    vkDestroyBuffer(lahar->device, buffer, lahar->vkalloc);

    printf("\tExpect a leak warning for 'deliberate leak' below:\n");
    lahar_freelist_deinit();

    lahar_freelist_stats(&stats);

    if (stats.live_allocations != 0 || stats.reserved != 0) {
        printf("\tDeinit didn't zero the allocator state\n");
        return false;
    }

    return true;
}

#endif /* TEST_ALLOCATOR_FREELIST */

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
    { "rejects bad params",                 test_rejects_bad_params },
    { "rejects bad usage enum",             test_rejects_bad_usage },

    #if defined(TEST_ALLOCATOR_FREELIST)
    { "fl: stats track allocs",             test_fl_stats_track_allocs },
    { "fl: multi-block spill",              test_fl_multi_block_spill },
    { "fl: dedicated threshold",            test_fl_dedicated_threshold },
    { "fl: coalesce and regrow",            test_fl_coalesce_regrow },
    { "fl: allocation naming",              test_fl_allocation_name },
    #endif
};

int main(void) {
    uint32_t err;

    if ((err = lahar_init())) {
        printf("Lahar failed to init: %s\n", lahar_err_name(err));
        return 1;
    }

    /* Warn level so leak reports are visible in the output */
    lahar_builder_set_debug_level(LAHAR_DEBUG_WARNING);

    /* Headless: no windows, but we want a command pool for the copy test */
    lahar_builder_request_command_buffers();

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

    #if defined(TEST_ALLOCATOR_FREELIST)
    /* Last test: leak a named allocation on purpose and verify the deinit
     * report catches it. Runs outside the table because it tears the
     * allocator down. (Once folded into lahar.h, lahar_deinit() will do
     * the teardown; this then just tests the report.) */
    printf("[leak] deinit leak report\n");
    if (!leak_check()) {
        printf("\tFAILED\n");
        failed++;
    }
    #endif

    lahar_deinit();

    printf("\n%zu/%zu tests passed\n", count - failed, count);

    return failed ? 1 : 0;
}
