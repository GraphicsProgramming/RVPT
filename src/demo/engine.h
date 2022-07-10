#pragma once

#include <optional>

#include <VkBootstrap.h>

#include "vk_util.h"

#include "window.h"
#include "camera.h"

namespace demo
{
class engine
{
private:
    static constexpr uint8_t MAX_FRAMES_IN_FLIGHT = 2;

    Camera _camera;

    window *_window;

    struct Context
    {
        VkSurfaceKHR surface {};
        vkb::Instance instance {};
        vkb::Device device;
    } _context;
    VkDevice _vk_device{};

    std::optional<VK::Queue> _graphics_queue;
    std::optional<VK::Queue> _present_queue;

    VK::PipelineBuilder _pipeline_builder;
    VK::MemoryAllocator _memory_allocator;

    vkb::Swapchain _vkb_swapchain;
    std::vector<VkImage> _swapchain_images;
    std::vector<VkImageView> _swapchain_image_views;

    uint32_t _current_sync_index = 0;
    std::vector<VK::SyncResources> _sync_resources;
    std::vector<VkFence> _frames_inflight_fences;

    VkRenderPass fullscreen_tri_render_pass{};

    std::vector<VK::Framebuffer> framebuffers;

    struct RenderingResources
    {
        VK::DescriptorPool debug_descriptor_pool;

        VkPipelineLayout debug_pipeline_layout;
        VK::GraphicsPipelineHandle debug_opaque_pipeline;

        VK::Image depth_buffer;
    };

    std::optional<RenderingResources> _rendering_resources;

    uint32_t _current_frame_index = 0;
    struct PerFrameData
    {
        VK::Buffer debug_camera_uniform;
        VK::Buffer debug_vertex_buffer;
        VK::DescriptorSet debug_descriptor_sets;
    };
    std::vector<PerFrameData> _per_frame_data;

    uint32_t _triangle_count;

    // helper functions
    bool context_init();
    bool swapchain_init();
    bool swapchain_reinit();
    bool swapchain_get_images();
    void create_framebuffers();

    [[nodiscard]] RenderingResources create_rendering_resources();
    void add_per_frame_data(int index);

    void record_command_buffer(VK::SyncResources& current_frame, uint32_t swapchain_image_index);

public:
    engine() = delete;

    explicit engine(window *window);

    void init();

    void update(const std::vector<glm::vec3> &triangles, const glm::vec3 &translation, const glm::vec3 &rotation);
    void draw();

};

}
