# Changelog

## 4.3

- Added an optional device-wide timeline semaphore (`lahar->timeline`), created at build when
  timeline semaphores are enabled via the `VK_KHR_timeline_semaphore` extension or the 1.2 core
  feature bit (same resolution dance as dynamic rendering)
- Every `lahar_window_submit*` now signals the timeline semaphore with a monotonic tick,
  tracked in `lahar->timeline_value`, usable for asset lifetime tracking
- Fixed pre-1.2/1.3 header support: dynamic rendering and timeline semaphore feature resolution
  now compile (with KHR fallbacks or as no-ops) against Vulkan 1.0/1.1 headers
- Broke optional feature resolution out of `__lahar_build_device` into per-feature helpers
- Window command buffers are now allocated per frame in flight rather than per swapchain image,
  and `lahar_window_command_buffer` indexes by flight index
- `VK_SUBOPTIMAL_KHR` on acquire no longer skips the frame; the frame draws and presents
  normally, and the swapchain recreates on the way out of present
- Default depth attachment now uses `VK_FORMAT_D32_SFLOAT_S8_UINT` with the stencil aspect
- Attachment aspect masks are derived from the attachment's own format instead of the surface format
- Track forced retire queue flushes in `lahar->stats.forced_retire_flushes`

## 4.2

- Swapchain resizing/recreation, including automatic recreation on out-of-date swapchains
- Async window destruction: destroyed windows enter a retire queue (`LAHAR_MAX_WINDOW_RETIRES`)
  and are reclaimed once no longer in use, rather than stalling the device
- Optional `VK_EXT_swapchain_maintenance1` support (`lahar->swapchain_maintenance1`): when
  enabled, retired swapchains reclaim via present fences instead of waiting for device idle

## 4.1

- Memory helpers: `lahar_buffer_create[_simple]`/`lahar_buffer_destroy`,
  `lahar_image_create`/`lahar_image_destroy`, and `lahar_memory_map`/`unmap`/`flush`/
  `invalidate`/`write`/`read`
- Added `lahar_window_frame_cancel` for backing out of a begun frame
- Descriptor set layout reflection: `lahar_shader_reflect_set_layouts` and
  `lahar_shader_builder_set_descriptor_set_layouts`
- Internal functions are now static
- Docs fixes

## 4.0

- Full allocator rewrite to a TLSF (two-level segregated fit) implementation:
  `lahar_allocator_freelist_init`/`lahar_allocator_freelist_deinit`
- Added `lahar_builder_set_vulkan_version` for requesting (or hard-requiring) a Vulkan version
- New error codes for allocation failures, unsatisfiable memory types, and unsatisfiable
  Vulkan versions

## 3.0.1

- Fix operator precedence issues in the SPIRV introspector

## 3.0

- Expanded GPU allocator interface, with a built-in freelist fallback implementation used
  when no allocator is supplied (`lahar_allocator_freelist`, `lahar_freelist_stats`,
  `lahar_freelist_allocation_name`)
- Allocator test suite, including tests against VMA for comparison

## 2.1

- Add error types
- Add missing compiler registration implementation (`lahar_shader_register_compiler`)

## 2.0

- Implement shader building and SPIRV parsing: `LaharShaderBuilder` pipeline construction
  (`lahar_shader_build`), SPIRV introspection (`lahar_shader_introspect`), and pluggable
  shader compilers
- Window attachment API: per-frame color/depth/stencil/indexed attachments with
  configuration accessors, plus `lahar_cmd_attachment_transition` and
  `lahar_cmd_begin_rendering` helpers
