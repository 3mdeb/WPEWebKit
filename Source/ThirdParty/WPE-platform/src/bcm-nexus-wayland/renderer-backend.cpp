#include <wpe/renderer-backend-egl.h>

#include <EGL/egl.h>

extern "C" {

struct wpe_renderer_backend_egl_interface bcm_nexus_wayland_renderer_backend_egl_interface = {
    // create
    []() -> void*
    {
        return nullptr;
    },
    // destroy
    [](void* data)
    {
    },
    // get_native_display
    [](void* data) -> EGLNativeDisplayType
    {
        return nullptr;
    },
};

struct wpe_renderer_backend_egl_target_interface bcm_nexus_wayland_renderer_backend_egl_target_interface = {
    // create
    [](struct wpe_renderer_backend_egl_target* target, int host_fd) -> void*
    {
        return nullptr;
    },
    // destroy
    [](void* data)
    {
    },
    // initialize
    [](void* data, void* backend_data, uint32_t width, uint32_t height)
    {
    },
    // get_native_window
    [](void* data) -> EGLNativeWindowType
    {
        return nullptr;
    },
    // resize
    [](void* data, uint32_t width, uint32_t height)
    {
    },
    // frame_will_render
    [](void* data)
    {
    },
    // frame_rendered
    [](void* data)
    {
    },
};

struct wpe_renderer_backend_egl_offscreen_target_interface bcm_nexus_wayland_renderer_backend_egl_offscreen_target_interface = {
    // create
    []() -> void*
    {
        return nullptr;
    },
    // destroy
    [](void* data)
    {
    },
    // initialize
    [](void* data, void* backend_data)
    {
    },
    // get_native_window
    [](void* data) -> EGLNativeWindowType
    {
        return nullptr;
    },
};

}
