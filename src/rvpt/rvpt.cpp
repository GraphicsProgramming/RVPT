#include "rvpt.h"

#include <glm/glm.hpp>
#include <glm/glm/ext.hpp>

RVPT::RVPT(Window& window) : window_ref(window), scene_camera(window.get_aspect_ratio()) {}

RVPT::~RVPT() {}

bool RVPT::initialize()
{
    bool init = context_init();
    pipeline_builder = VK::PipelineBuilder(vk_device);
    memory_allocator =
        VK::MemoryAllocator(context.device.physical_device.physical_device, vk_device);

    init &= swapchain_init();
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        sync_resources.emplace_back(vk_device, graphics_queue.value(), present_queue.value(),
                                    vkb_swapchain.swapchain);
    }
    frames_inflight_fences.resize(vkb_swapchain.image_count, nullptr);

    fullscreen_tri_render_pass = VK::create_render_pass(vk_device, vkb_swapchain.image_format);

    create_framebuffers();

    // output image

    sampled_image.emplace(
        vk_device, memory_allocator, *graphics_queue, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL, 512, 512, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
        VK_IMAGE_LAYOUT_GENERAL, static_cast<VkDeviceSize>(512 * 512 * 4), VK::MemoryUsage::gpu);

    std::vector<VkDescriptorSetLayoutBinding> layout_bindings = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};

    sampled_image_pool.emplace(vk_device, layout_bindings, 1);
    sampled_image_descriptor_set = sampled_image_pool->allocate();

    std::vector<VkDescriptorImageInfo> image_descriptor_info = {sampled_image->descriptor_info()};
    VK::DescriptorUse descriptor_use{0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                     image_descriptor_info};

    auto write_descriptor =
        descriptor_use.get_write_descriptor_set(sampled_image_descriptor_set->set);
    vkUpdateDescriptorSets(vk_device, 1, &write_descriptor, 0, nullptr);

    fullscreen_triangle_pipeline = pipeline_builder.create_graphics_pipeline(
        "fullscreen_tri.vert.spv", "tex_sample.frag.spv", {sampled_image_descriptor_set->layout},
        fullscreen_tri_render_pass, vkb_swapchain.extent);

    // Compute

    camera_matrix_uniform_buffer.emplace(vk_device, memory_allocator,
                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 48,
                           VK::MemoryUsage::cpu_to_gpu);
    camera_matrix_uniform_buffer->map();
    camera_matrix_uniform_buffer->copy_to(scene_camera.get_data());

    std::vector<VkDescriptorSetLayoutBinding> compute_layout_bindings = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}};

    compute_descriptor_pool.emplace(vk_device, compute_layout_bindings, 1);
    compute_descriptor_set = compute_descriptor_pool->allocate();

    VK::DescriptorUse image_descriptor_use{0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                           image_descriptor_info};

    std::vector<VkDescriptorBufferInfo> buffer_descriptor_info = {
        camera_matrix_uniform_buffer->descriptor_info()};
    VK::DescriptorUse buffer_descriptor_use{
        1, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, buffer_descriptor_info};

    VkWriteDescriptorSet write_descriptors[] = {
        image_descriptor_use.get_write_descriptor_set(compute_descriptor_set->set),
        buffer_descriptor_use.get_write_descriptor_set(compute_descriptor_set->set)};

    vkUpdateDescriptorSets(vk_device, 2, write_descriptors, 0, nullptr);

    compute_pipeline = pipeline_builder.create_compute_pipeline("compute_pass.comp.spv",
                                                                {compute_descriptor_set->layout});

    compute_command_buffer.emplace(vk_device,
                                   compute_queue.has_value() ? *compute_queue : *graphics_queue);
    compute_work_fence.emplace(vk_device);

    return init;
}
bool RVPT::update() { return true; }

RVPT::draw_return RVPT::draw()
{
    compute_work_fence->wait();
    compute_work_fence->reset();

//    camera_matrix_uniform_buffer->copy_to();

    record_compute_command_buffer();

    VK::Queue& compute_submit = compute_queue.has_value() ? *compute_queue : *graphics_queue;
    compute_submit.submit(compute_command_buffer.value(), compute_work_fence.value());

    auto& current_frame = sync_resources[current_frame_index];

    current_frame.command_fence.wait();
    current_frame.command_buffer.reset();

    uint32_t swapchain_image_index;
    VkResult result = vkAcquireNextImageKHR(vk_device, vkb_swapchain.swapchain, UINT64_MAX,
                                            current_frame.image_avail_sem.get(), VK_NULL_HANDLE,
                                            &swapchain_image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        swapchain_reinit();
        return draw_return::swapchain_out_of_date;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        std::cerr << "Failed to acquire next swapchain image\n";
        assert(false);
    }
    record_command_buffer(current_frame, swapchain_image_index);

    if (frames_inflight_fences[swapchain_image_index] != nullptr)
    {
        vkWaitForFences(vk_device, 1, &frames_inflight_fences[swapchain_image_index], VK_TRUE,
                        UINT64_MAX);
    }
    frames_inflight_fences[swapchain_image_index] = current_frame.command_fence.get();

    current_frame.command_fence.reset();
    current_frame.submit();

    result = current_frame.present(swapchain_image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebuffer_resized)
    {
        framebuffer_resized = false;
        swapchain_reinit();
    }
    else if (result != VK_SUCCESS)
    {
        std::cerr << "Failed to present swapchain image\n";
        assert(false);
    }
    current_frame_index = (current_frame_index + 1) % sync_resources.size();
    return draw_return::success;
}

void RVPT::shutdown()
{
    if (compute_queue) compute_queue->wait_idle();
    graphics_queue->wait_idle();
    present_queue->wait_idle();

    camera_matrix_uniform_buffer.reset();

    compute_work_fence.reset();
    compute_command_buffer.reset();

    compute_descriptor_pool.reset();

    sampled_image_pool.reset();
    sampled_image.reset();

    VK::destroy_render_pass(vk_device, fullscreen_tri_render_pass);

    framebuffers.clear();

    sync_resources.clear();
    vkb_swapchain.destroy_image_views(swapchain_image_views);

    memory_allocator.shutdown();
    pipeline_builder.shutdown();
    vkb::destroy_swapchain(vkb_swapchain);
    vkb::destroy_device(context.device);
    vkDestroySurfaceKHR(context.inst.instance, context.surf, nullptr);
    vkb::destroy_instance(context.inst);
}

// Private functions //
bool RVPT::context_init()
{
    vkb::InstanceBuilder inst_builder;
    auto inst_ret = inst_builder.set_app_name(window_ref.get_settings().title)
                        .request_validation_layers()
                        .use_default_debug_messenger()
                        .build();

    if (!inst_ret)
    {
        std::cerr << "Failed to create an instance: " << inst_ret.error().message() << '\n';
        return false;
    }

    context.inst = inst_ret.value();

    VkResult surf_res = glfwCreateWindowSurface(
        context.inst.instance, window_ref.get_window_pointer(), nullptr, &context.surf);
    if (surf_res != VK_SUCCESS)
    {
        std::cerr << "Failed to create a surface" << '\n';
        return false;
    }
    VkPhysicalDeviceFeatures required_features{};
    required_features.samplerAnisotropy = true;

    vkb::PhysicalDeviceSelector selector(context.inst);
    auto phys_ret = selector.set_surface(context.surf)
                        .set_required_features(required_features)
                        .set_minimum_version(1, 1)
                        .select();

    if (!phys_ret)
    {
        std::cerr << "Failed to find a physical device: " << phys_ret.error().message() << '\n';
        return false;
    }

    vkb::DeviceBuilder dev_builder(phys_ret.value());
    auto dev_ret = dev_builder.build();
    if (!dev_ret)
    {
        std::cerr << "Failed create a device: " << dev_ret.error().message() << '\n';
        return false;
    }

    context.device = dev_ret.value();
    vk_device = dev_ret.value().device;

    auto graphics_queue_index_ret = context.device.get_queue_index(vkb::QueueType::graphics);
    if (!graphics_queue_index_ret)
    {
        std::cerr << "Failed get the graphics queue: " << dev_ret.error().message() << '\n';

        return false;
    }
    graphics_queue.emplace(vk_device, graphics_queue_index_ret.value());

    auto present_queue_index_ret = context.device.get_queue_index(vkb::QueueType::present);
    if (!present_queue_index_ret)
    {
        std::cerr << "Failed get the present queue: " << dev_ret.error().message() << '\n';

        return false;
    }
    present_queue.emplace(vk_device, present_queue_index_ret.value());

    auto compute_queue_index_ret =
        context.device.get_dedicated_queue_index(vkb::QueueType::compute);
    if (compute_queue_index_ret)
    {
        compute_queue.emplace(vk_device, compute_queue_index_ret.value());
    }
    return true;
}

bool RVPT::swapchain_init()
{
    vkb::SwapchainBuilder swapchain_builder(context.device);
    auto ret = swapchain_builder.build();
    if (!ret)
    {
        std::cerr << "Failed to create a swapchain:" << ret.error().message() << '\n';
        return false;
    }
    vkb_swapchain = ret.value();
    return swapchain_get_images();
}

bool RVPT::swapchain_reinit()
{
    framebuffers.clear();
    vkb_swapchain.destroy_image_views(swapchain_image_views);

    vkb::SwapchainBuilder swapchain_builder(context.device);
    auto ret = swapchain_builder.recreate(vkb_swapchain);
    if (!ret)
    {
        std::cerr << "Failed to recreate swapchain:" << ret.error().message() << '\n';
        return false;
    }
    vkb_swapchain = ret.value();
    bool out_bool = swapchain_get_images();
    create_framebuffers();
    return out_bool;
}

bool RVPT::swapchain_get_images()
{
    auto swapchain_images_ret = vkb_swapchain.get_images();
    if (!swapchain_images_ret)
    {
        return false;
    }
    swapchain_images = swapchain_images_ret.value();

    auto swapchain_image_views_ret = vkb_swapchain.get_image_views();
    if (!swapchain_image_views_ret)
    {
        return false;
    }
    swapchain_image_views = swapchain_image_views_ret.value();

    return true;
}

void RVPT::create_framebuffers()
{
    framebuffers.clear();
    for (uint32_t i = 0; i < vkb_swapchain.image_count; i++)
    {
        std::vector<VkImageView> image_views = {swapchain_image_views[i]};
        framebuffers.emplace_back(vk_device, fullscreen_tri_render_pass, vkb_swapchain.extent,
                                  image_views);
    }
}

void RVPT::record_command_buffer(VK::SyncResources& current_frame, uint32_t swapchain_image_index)
{
    current_frame.command_buffer.begin();
    VkCommandBuffer cmd_buf = current_frame.command_buffer.get();

    // Image memory barrier to make sure that compute shader writes are finished
    // before sampling from the texture
    VkImageMemoryBarrier imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageMemoryBarrier.image = sampled_image->image.handle;
    imageMemoryBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK::FLAGS_NONE, 0, nullptr, 0,
                         nullptr, 1, &imageMemoryBarrier);

    VkClearValue background_color = {0.0f, 0.0f, 0.0f, 1.0f};

    VkRenderPassBeginInfo rp_begin_info{};
    rp_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin_info.renderPass = fullscreen_tri_render_pass;
    rp_begin_info.framebuffer = framebuffers.at(swapchain_image_index).framebuffer.handle;
    rp_begin_info.renderArea.offset = {0, 0};
    rp_begin_info.renderArea.extent = vkb_swapchain.extent;
    rp_begin_info.clearValueCount = 1;
    rp_begin_info.pClearValues = &background_color;

    vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{0.0f,
                        0.0f,
                        static_cast<float>(vkb_swapchain.extent.width),
                        static_cast<float>(vkb_swapchain.extent.height),
                        0.0f,
                        1.0f};
    vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, vkb_swapchain.extent};
    vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      fullscreen_triangle_pipeline.pipeline);
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            fullscreen_triangle_pipeline.layout, 0, 1,
                            &sampled_image_descriptor_set->set, 0, nullptr);
    vkCmdDraw(cmd_buf, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd_buf);
    current_frame.command_buffer.end();
}

void RVPT::record_compute_command_buffer()
{
    compute_command_buffer->begin();
    VkCommandBuffer cmd_buf = compute_command_buffer->get();
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline.pipeline);
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline.layout, 0, 1,
                            &compute_descriptor_set->set, 0, 0);

    vkCmdDispatch(cmd_buf, sampled_image->width / 16, sampled_image->height / 16, 1);

    compute_command_buffer->end();
}
