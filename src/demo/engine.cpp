//
// Created by howto on 10/7/2022.
//

#include <array>
#include "engine.h"
bool demo::engine::context_init()
{
    vkb::InstanceBuilder inst_builder;
    auto inst_ret =
        inst_builder.set_app_name(_window->title().data())
            .request_validation_layers(true)
            .set_debug_callback(
                [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                   VkDebugUtilsMessageTypeFlagsEXT messageType,
                   const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                   void* pUserData) -> VkBool32
                {
                    auto severity = vkb::to_string_message_severity(messageSeverity);
                    auto type = vkb::to_string_message_type(messageType);
                    fmt::print("[{}: {}] {}\n", severity, type, pCallbackData->pMessage);
                    return VK_FALSE;
                })
            .build();

    if (!inst_ret)
    {
        fmt::print(stderr, "failed to create an instance: {}\n", inst_ret.error().message());
        return false;
    }

    _context.instance = inst_ret.value();

    VkResult surf_res = glfwCreateWindowSurface(_context.instance.instance, _window->handle(),
                                                nullptr, &_context.surface);
    if (surf_res != VK_SUCCESS)
    {
        fmt::print(stderr, "Failed to create a surface: Error Code{}\n", surf_res);
        return false;
    }
    VkPhysicalDeviceFeatures required_features{};
    required_features.samplerAnisotropy = true;
    required_features.fillModeNonSolid = true;

    vkb::PhysicalDeviceSelector selector(_context.instance);
    auto phys_ret = selector.set_surface(_context.surface)
                        .set_required_features(required_features)
                        .set_minimum_version(1, 1)
                        .select();

    if (!phys_ret)
    {
        fmt::print(stderr, "Failed to find a physical device: \n", phys_ret.error().message());
        return false;
    }

    vkb::DeviceBuilder dev_builder(phys_ret.value());
    auto dev_ret = dev_builder.build();
    if (!dev_ret)
    {
        fmt::print(stderr, "Failed create a device: \n", dev_ret.error().message());
        return false;
    }

    _context.device = dev_ret.value();
    _vk_device = dev_ret.value().device;

    auto graphics_queue_index_ret = _context.device.get_queue_index(vkb::QueueType::graphics);
    if (!graphics_queue_index_ret)
    {
        fmt::print(stderr, "Failed to get the graphics queue: \n",
                   graphics_queue_index_ret.error().message());
        return false;
    }
    _graphics_queue.emplace(_vk_device, graphics_queue_index_ret.value(), "graphics_queue");

    auto present_queue_index_ret = _context.device.get_queue_index(vkb::QueueType::present);
    if (!present_queue_index_ret)
    {
        fmt::print(stderr, "Failed to get the present queue: \n",
                   present_queue_index_ret.error().message());
        return false;
    }
    _present_queue.emplace(_vk_device, present_queue_index_ret.value(), "present_queue");

    VK::setup_debug_util_helper(_vk_device);

    return true;
}

bool demo::engine::swapchain_init()
{
    vkb::SwapchainBuilder swapchain_builder(_context.device);
    auto ret = swapchain_builder.build();
    if (!ret)
    {
        fmt::print(stderr, "Failed to create a swapchain: \n", ret.error().message());
        return false;
    }
    _vkb_swapchain = ret.value();
    return swapchain_get_images();
}

bool demo::engine::swapchain_reinit()
{
    framebuffers.clear();
    _vkb_swapchain.destroy_image_views(_swapchain_image_views);

    vkb::SwapchainBuilder swapchain_builder(_context.device);
    auto ret = swapchain_builder.set_old_swapchain(_vkb_swapchain).build();
    if (!ret)
    {
        fmt::print(stderr, "Failed to recreate a swapchain: \n", ret.error().message());
        return false;
    }
    vkb::destroy_swapchain(_vkb_swapchain);
    _vkb_swapchain = ret.value();
    bool out_bool = swapchain_get_images();
    create_framebuffers();
    return out_bool;
}
bool demo::engine::swapchain_get_images()
{
    auto swapchain_images_ret = _vkb_swapchain.get_images();
    if (!swapchain_images_ret)
    {
        return false;
    }
    _swapchain_images = swapchain_images_ret.value();

    auto swapchain_image_views_ret = _vkb_swapchain.get_image_views();
    if (!swapchain_image_views_ret)
    {
        return false;
    }
    _swapchain_image_views = swapchain_image_views_ret.value();

    return true;
}
void demo::engine::create_framebuffers()
{
    framebuffers.clear();
    for (uint32_t i = 0; i < _vkb_swapchain.image_count; i++)
    {
        std::vector<VkImageView> image_views = {
            _swapchain_image_views[i], _rendering_resources->depth_buffer.image_view.handle};
        VK::debug_utils_helper.set_debug_object_name(VK_OBJECT_TYPE_IMAGE_VIEW,
                                                     _swapchain_image_views[i],
                                                     "swapchain_image_view_" + std::to_string(i));

        framebuffers.emplace_back(_vk_device, fullscreen_tri_render_pass, _vkb_swapchain.extent,
                                  image_views, "swapchain_framebuffer_" + std::to_string(i));
    }
}
demo::engine::RenderingResources demo::engine::create_rendering_resources()
{
    std::vector<VkDescriptorSetLayoutBinding> layout_bindings = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};

    std::vector<VkDescriptorSetLayoutBinding> debug_layout_bindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}};

    auto debug_descriptor_pool = VK::DescriptorPool(_vk_device, debug_layout_bindings,
                                                    MAX_FRAMES_IN_FLIGHT, "debug_descriptor_pool");
    auto debug_pipeline_layout = _pipeline_builder.create_layout({debug_descriptor_pool.layout()},
                                                                 {}, "debug_vis_pipeline_layout");

    std::vector<VkVertexInputBindingDescription> binding_desc = {
        {0, sizeof(float) * 3, VK_VERTEX_INPUT_RATE_VERTEX}};

    std::vector<VkVertexInputAttributeDescription> attribute_desc = {
        {0, binding_desc[0].binding, VK_FORMAT_R32G32B32_SFLOAT, 0}};

    VkPipelineDepthStencilStateCreateInfo depth_stencil_info{};
    depth_stencil_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    VK::GraphicsPipelineDetails debug_details;
    debug_details.name = "debug_raster_view_pipeline";
    debug_details.pipeline_layout = debug_pipeline_layout;
    debug_details.vert_shader = "debug_vis.vert.spv";
    debug_details.frag_shader = "debug_vis.frag.spv";
    debug_details.render_pass = fullscreen_tri_render_pass;
    debug_details.extent = _vkb_swapchain.extent;
    debug_details.binding_desc = binding_desc;
    debug_details.attribute_desc = attribute_desc;
    debug_details.cull_mode = VK_CULL_MODE_NONE;
    debug_details.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    debug_details.enable_depth = true;

    auto opaque = _pipeline_builder.create_pipeline(debug_details);
    debug_details.polygon_mode = VK_POLYGON_MODE_LINE;

    VkFormat depth_format =
        VK::get_depth_image_format(_context.device.physical_device.physical_device);

    const auto window_size = _window->size();

    auto depth_image = VK::Image(_vk_device, _memory_allocator, *_graphics_queue, "depth_image",
                                 depth_format, VK_IMAGE_TILING_OPTIMAL, window_size.x,
                                 window_size.y, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                 VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
                                 static_cast<VkDeviceSize>(window_size.x * window_size.y * 4),
                                 VK::MemoryUsage::gpu);

    return RenderingResources{std::move(debug_descriptor_pool),
                              debug_pipeline_layout,
                              opaque,
                              std::move(depth_image)};
}
void demo::engine::add_per_frame_data(int index)
{
    // Debug vis
    auto debug_camera_uniform = VK::Buffer(
        _vk_device, _memory_allocator, "debug_camera_uniform_" + std::to_string(index),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(glm::mat4), VK::MemoryUsage::cpu_to_gpu);
    auto debug_vertex_buffer = VK::Buffer(
        _vk_device, _memory_allocator, "debug_vertecies_buffer_" + std::to_string(index),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 1000 * sizeof(float) * 3, VK::MemoryUsage::cpu_to_gpu);

    auto debug_descriptor_set = _rendering_resources->debug_descriptor_pool.allocate(
        "debug_descriptor_set_" + std::to_string(index));

    std::vector<VK::DescriptorUseVector> debug_descriptors;
    debug_descriptors.push_back(std::vector{debug_camera_uniform.descriptor_info()});
    _rendering_resources->debug_descriptor_pool.update_descriptor_sets(debug_descriptor_set,
                                                                       debug_descriptors);

    _per_frame_data.push_back(PerFrameData{std::move(debug_camera_uniform),
                                           std::move(debug_vertex_buffer), debug_descriptor_set});
}

void demo::engine::record_command_buffer(VK::SyncResources& current_frame,
                                         uint32_t swapchain_image_index)
{
    current_frame.command_buffer.begin();
    VkCommandBuffer cmd_buf = current_frame.command_buffer.get();

    VkRenderPassBeginInfo rp_begin_info{};
    rp_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin_info.renderPass = fullscreen_tri_render_pass;
    rp_begin_info.framebuffer = framebuffers.at(swapchain_image_index).framebuffer.handle;
    rp_begin_info.renderArea.offset = {0, 0};
    rp_begin_info.renderArea.extent = _vkb_swapchain.extent;
    std::array<VkClearValue, 2> clear_values;
    clear_values[0] = {0.0f, 0.0f, 0.0f, 1.0f};
    clear_values[1] = {1.0f, 0};
    rp_begin_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
    rp_begin_info.pClearValues = clear_values.data();

    vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{0.0f,
                        0.0f,
                        static_cast<float>(_vkb_swapchain.extent.width),
                        static_cast<float>(_vkb_swapchain.extent.height),
                        0.0f,
                        1.0f};
    vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, _vkb_swapchain.extent};
    vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    auto pipeline = _pipeline_builder.get_pipeline(_rendering_resources->debug_opaque_pipeline);
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    vkCmdBindDescriptorSets(
        cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, _rendering_resources->debug_pipeline_layout, 0, 1,
        &_per_frame_data[_current_frame_index].debug_descriptor_sets.set, 0, nullptr);

    bind_vertex_buffer(cmd_buf, _per_frame_data[_current_frame_index].debug_vertex_buffer);

    vkCmdDraw(cmd_buf, _triangle_count * 3, 1, 0, 0);

    vkCmdEndRenderPass(cmd_buf);
    current_frame.command_buffer.end();
}

demo::engine::engine(demo::window* window)
    : _camera(window->size().y / window->size().x), _window(window)
{
}

void demo::engine::init()
{
    context_init();

    _pipeline_builder = VK::PipelineBuilder(_vk_device, "");
    _memory_allocator =
        VK::MemoryAllocator(_context.device.physical_device.physical_device, _vk_device);

    swapchain_init();
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        _sync_resources.emplace_back(_vk_device, _graphics_queue.value(), _present_queue.value(),
                                     _vkb_swapchain.swapchain);
    }
    _frames_inflight_fences.resize(_vkb_swapchain.image_count, VK_NULL_HANDLE);

    fullscreen_tri_render_pass = VK::create_render_pass(
        _vk_device, _vkb_swapchain.image_format,
        VK::get_depth_image_format(_context.device.physical_device.physical_device),
        "fullscreen_image_copy_render_pass");

    _rendering_resources = create_rendering_resources();

    create_framebuffers();

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        add_per_frame_data(i);
    }
}

void demo::engine::update(const std::vector<glm::vec3> &triangles, const glm::vec3 &translation, const glm::vec3 &rotation)
{
    _camera.translate(translation);
    _camera.rotate(rotation);

    auto camera_data = _camera.get_data();

    std::vector<glm::vec3> debug_triangles;
    debug_triangles.reserve(triangles.size());
    for (auto& tri : triangles)
    {
        debug_triangles.push_back(tri);
    }
    size_t vert_byte_size = debug_triangles.size() * sizeof(glm::vec3);
    if (_per_frame_data[_current_frame_index].debug_vertex_buffer.size() < vert_byte_size)
    {
        _per_frame_data[_current_frame_index].debug_vertex_buffer = VK::Buffer(
            _vk_device, _memory_allocator, "debug_vertex_buffer", VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            vert_byte_size, VK::MemoryUsage::cpu_to_gpu);
    }
    _per_frame_data[_current_frame_index].debug_vertex_buffer.copy_to(debug_triangles);
    _per_frame_data[_current_frame_index].debug_camera_uniform.copy_to(_camera.get_pv_matrix());
}

void demo::engine::draw()
{
    auto& current_frame = _sync_resources[_current_sync_index];
    uint32_t swapchain_image_index;
    VkResult result = vkAcquireNextImageKHR(_vk_device, _vkb_swapchain.swapchain, UINT64_MAX,
                                            current_frame.image_avail_sem.get(), VK_NULL_HANDLE,
                                            &swapchain_image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        swapchain_reinit();
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        fmt::print(stderr, "Failed to acquire next swapchain image\n");
        assert(false);
    }

    record_command_buffer(current_frame, swapchain_image_index);

    if (_frames_inflight_fences[swapchain_image_index] != VK_NULL_HANDLE)
    {
        vkWaitForFences(_vk_device, 1, &_frames_inflight_fences[swapchain_image_index], VK_TRUE,
                        UINT64_MAX);
    }
    _frames_inflight_fences[swapchain_image_index] = current_frame.command_fence.get();

    current_frame.command_fence.reset();
    current_frame.submit();

    result = current_frame.present(swapchain_image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        swapchain_reinit();
    }
    else if (result != VK_SUCCESS)
    {
        fmt::print(stderr, "Failed to present swapchain image\n");
        assert(false);
    }
    _current_sync_index = (_current_sync_index + 1) % _sync_resources.size();
    _current_frame_index = (_current_frame_index + 1) % _per_frame_data.size();
}
