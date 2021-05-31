#include "demo.h"

#include <iostream>

#define GLM_DEPTH_ZERO_TO_ONE
#include <glm/ext.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <imgui.h>

#include "imgui_helpers.h"
#include "imgui_internal.h"

RVPTDemo::RVPTDemo(Window::Settings settings) noexcept : window(settings) {}
RVPTDemo::~RVPTDemo() noexcept {}
bool RVPTDemo::initialize()
{
    ImGui::CreateContext();

    bool init = context_init();
    pipeline_builder = VK::PipelineBuilder(device, "");
    memory_allocator = VK::MemoryAllocator(physical_device, device);

    init &= swapchain_init();
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        sync_resources.emplace_back(device, *graphics_queue, *present_queue,
                                    vkb_swapchain.swapchain);
    }
    frames_inflight_fences.resize(vkb_swapchain.image_count, VK_NULL_HANDLE);

    render_pass =
        VK::create_render_pass(device, vkb_swapchain.image_format, "imgui_render_onto_output_pass");

    create_framebuffers();

    imgui_impl.emplace(device, *graphics_queue, pipeline_builder, memory_allocator, render_pass,
                       vkb_swapchain.extent, MAX_FRAMES_IN_FLIGHT);

    rvpt.emplace(window.get_aspect_ratio());
    return init;
}

void RVPTDemo::initialize_rvpt()
{
    rvpt->initialize(
        physical_device, device, graphics_queue->get_family(),
        compute_queue.has_value() ? compute_queue->get_family() : graphics_queue->get_family(),
        vkb_swapchain.extent);
}

void RVPTDemo::shutdown()
{
    graphics_queue->wait_idle();
    present_queue->wait_idle();

    imgui_impl.reset();

    sync_resources.clear();
    vkb_swapchain.destroy_image_views(swapchain_image_views);
    VK::destroy_render_pass(device, render_pass);
    pipeline_builder.shutdown();
    memory_allocator.shutdown();
    vkb::destroy_swapchain(vkb_swapchain);
    vkb::destroy_device(vkb_device);
    vkDestroySurfaceKHR(instance.instance, surface, nullptr);
    vkb::destroy_instance(instance);
}

bool RVPTDemo::context_init()
{
#if defined(DEBUG) || defined(_DEBUG)
    bool use_validation = true;
#else
    bool use_validation = false;
#endif

    vkb::InstanceBuilder inst_builder;
    auto inst_ret =
        inst_builder.set_app_name(window.get_settings().title)
            .request_validation_layers(use_validation)
            .set_debug_callback([](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                   VkDebugUtilsMessageTypeFlagsEXT messageType,
                                   const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                   [[maybe_unused]] void* pUserData) -> VkBool32 {
                auto severity = vkb::to_string_message_severity(messageSeverity);
                auto type = vkb::to_string_message_type(messageType);
                fmt::print("[{}: {}] {}\n", severity, type, pCallbackData->pMessage);
                std::string message(pCallbackData->pMessage);
                std::cerr << message << std::endl;
                return VK_FALSE;
            })
            .build();

    if (!inst_ret)
    {
        fmt::print(stderr, "Failed to create an instance: {}\n", inst_ret.error().message());
        return false;
    }

    instance = inst_ret.value();

    VkResult surf_res =
        glfwCreateWindowSurface(instance.instance, window.get_window_pointer(), nullptr, &surface);
    if (surf_res != VK_SUCCESS)
    {
        fmt::print(stderr, "Failed to create a surface: Error Code{}\n", surf_res);
        return false;
    }
    VkPhysicalDeviceFeatures required_features{};
    required_features.samplerAnisotropy = true;
    required_features.fillModeNonSolid = true;
    required_features.shaderFloat64 = true;

    vkb::PhysicalDeviceSelector selector(instance);
    auto phys_ret = selector.set_surface(surface)
                        .set_required_features(required_features)
                        .set_minimum_version(1, 1)
                        .select();

    if (!phys_ret)
    {
        fmt::print(stderr, "Failed to find a physical device: \n", phys_ret.error().message());
        return false;
    }
    physical_device = phys_ret.value().physical_device;
    vkb::DeviceBuilder dev_builder(phys_ret.value());
    auto dev_ret = dev_builder.build();
    if (!dev_ret)
    {
        fmt::print(stderr, "Failed create a device: \n", dev_ret.error().message());
        return false;
    }

    vkb_device = dev_ret.value();
    device = vkb_device.device;

    auto graphics_queue_index_ret = vkb_device.get_queue_index(vkb::QueueType::graphics);
    if (!graphics_queue_index_ret)
    {
        fmt::print(stderr, "Failed to get the graphics queue: \n",
                   graphics_queue_index_ret.error().message());
        return false;
    }
    graphics_queue.emplace(device, graphics_queue_index_ret.value(), "graphics_queue");

    auto present_queue_index_ret = vkb_device.get_queue_index(vkb::QueueType::present);
    if (!present_queue_index_ret)
    {
        fmt::print(stderr, "Failed to get the present queue: \n",
                   present_queue_index_ret.error().message());
        return false;
    }
    present_queue.emplace(device, present_queue_index_ret.value(), "present_queue");

    auto compute_queue_index_ret = vkb_device.get_dedicated_queue_index(vkb::QueueType::compute);
    if (compute_queue_index_ret)
    {
        compute_queue.emplace(device, compute_queue_index_ret.value(), "compute_queue");
    }

    VK::setup_debug_util_helper(device);

    return true;
}

bool RVPTDemo::swapchain_init()
{
    vkb::SwapchainBuilder swapchain_builder(vkb_device);
    auto ret = swapchain_builder.build();
    if (!ret)
    {
        fmt::print(stderr, "Failed to create a swapchain: \n", ret.error().message());
        return false;
    }
    vkb_swapchain = ret.value();
    return swapchain_get_images();
}

bool RVPTDemo::swapchain_reinit()
{
    framebuffers.clear();
    vkb_swapchain.destroy_image_views(swapchain_image_views);

    vkb::SwapchainBuilder swapchain_builder(vkb_device);
    auto ret = swapchain_builder.set_old_swapchain(vkb_swapchain).build();
    if (!ret)
    {
        fmt::print(stderr, "Failed to recreate a swapchain: \n", ret.error().message());
        return false;
    }
    vkb::destroy_swapchain(vkb_swapchain);
    vkb_swapchain = ret.value();
    bool out_bool = swapchain_get_images();
    create_framebuffers();
    return out_bool;
}

bool RVPTDemo::swapchain_get_images()
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

void RVPTDemo::create_framebuffers()
{
    framebuffers.clear();
    for (uint32_t i = 0; i < vkb_swapchain.image_count; i++)
    {
        std::vector<VkImageView> image_views = {swapchain_image_views[i]};
        VK::debug_utils_helper.set_debug_object_name(VK_OBJECT_TYPE_IMAGE_VIEW,
                                                     swapchain_image_views[i],
                                                     "swapchain_image_view_" + std::to_string(i));

        framebuffers.emplace_back(device, render_pass, vkb_swapchain.extent, image_views,
                                  "swapchain_framebuffer_" + std::to_string(i));
    }
}

void RVPTDemo::update_camera_imgui(Camera& camera)
{
    static bool is_active = true;
    ImGui::SetNextWindowPos({0, 265}, ImGuiCond_Once);
    ImGui::SetNextWindowSize({200, 210}, ImGuiCond_Once);

    if (ImGui::Begin("Camera Data", &is_active))
    {
        ImGui::PushItemWidth(125);
        ImGui::DragFloat3("position", glm::value_ptr(camera.translation), 0.2f);
        ImGui::DragFloat3("rotation", glm::value_ptr(camera.rotation), 0.2f);
        ImGui::Text("Reset");
        ImGui::SameLine();
        if (ImGui::Button("Pos")) camera.translation = {};

        ImGui::SameLine();
        if (ImGui::Button("Rot")) camera.rotation = {};

        ImGui::Text("Projection");
        dropdown_helper("camera_mode", camera.mode, CameraModes);
        if (camera.mode == 0)
        {
            ImGui::SliderFloat("fov", &camera.fov, 1, 179);
        }
        else if (camera.mode == 1)
        {
            ImGui::SliderFloat("scale", &camera.scale, 0.1f, 20);
        }

        ImGui::Checkbox("Clamp Vertical Rot", &camera.vertical_view_angle_clamp);

        static bool show_view_matrix = false;
        ImGui::Checkbox("Show View Matrix", &show_view_matrix);
        if (show_view_matrix)
        {
            ImGui::PushItemWidth(170);
            ImGui::DragFloat4("", glm::value_ptr(camera.camera_matrix[0]), 0.05f);
            ImGui::DragFloat4("", glm::value_ptr(camera.camera_matrix[1]), 0.05f);
            ImGui::DragFloat4("", glm::value_ptr(camera.camera_matrix[2]), 0.05f);
            ImGui::DragFloat4("", glm::value_ptr(camera.camera_matrix[3]), 0.05f);
        }
    }
    ImGui::End();
    camera._update_values();
}

void RVPTDemo::update_imgui()
{
    if (!show_imgui) return;

    ImGuiIO& io = ImGui::GetIO();

    // Setup display size (every frame to accommodate for window resizing)
    int w, h;
    int display_w, display_h;
    auto win_ptr = window.get_window_pointer();
    glfwGetWindowSize(win_ptr, &w, &h);
    glfwGetFramebufferSize(win_ptr, &display_w, &display_h);
    io.DisplaySize = ImVec2((float)w, (float)h);
    if (w > 0 && h > 0)
        io.DisplayFramebufferScale = ImVec2((float)display_w / w, (float)display_h / h);

    // Setup time step
    io.DeltaTime = static_cast<float>(time.since_last_frame());

    // imgui back end can't show 2 windows
    static bool show_stats = true;
    ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Once);
    ImGui::SetNextWindowSize({200, 65}, ImGuiCond_Once);
    if (ImGui::Begin("Stats", &show_stats))
    {
        ImGui::Text("Frame Time %.4f", time.average_frame_time());
        ImGui::Text("FPS %.2f", 1.0 / time.average_frame_time());
    }
    ImGui::End();
    static bool show_render_settings = true;
    ImGui::SetNextWindowPos({0, 65}, ImGuiCond_Once);
    ImGui::SetNextWindowSize({200, 190}, ImGuiCond_Once);
    if (ImGui::Begin("Render Settings", &show_stats))
    {
        ImGui::PushItemWidth(80);
        ImGui::SliderInt("AA", &rvpt->render_settings.aa, 1, 64);
        ImGui::SliderInt("Max Bounce", &rvpt->render_settings.max_bounces, 1, 64);

        ImGui::Checkbox("Debug Raster", &rvpt->debug_overlay_enabled);

        // indent "Wireframe" checkbox, grayed out if debug raster disabled
        if (!rvpt->debug_overlay_enabled)
        {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        }
        ImGui::Indent();
        ImGui::Checkbox("Wireframe", &rvpt->debug_wireframe_mode);
        ImGui::Unindent();
        if (!rvpt->debug_overlay_enabled)
        {
            ImGui::PopItemFlag();
            ImGui::PopStyleVar();
        }

        if (ImGui::Button("BVH Debug")) rvpt->toggle_bvh_debug();
        ImGui::SliderInt("Depth", &rvpt->max_bvh_view_depth, 1,
                         static_cast<int>(rvpt->depth_bvh_bounds.size()));
        ImGui::SameLine();
        if (ImGui::Button("View Last Depths")) rvpt->toggle_view_last_bvh_depths();

        static bool horizontal_split = false;
        static bool vertical_split = false;
        if (ImGui::Checkbox("Split", &horizontal_split))
        {
            rvpt->render_settings.split_ratio.x = 0.5f;
        }
        if (horizontal_split)
        {
            ImGui::SameLine();
            if (ImGui::Checkbox("4-way", &vertical_split))
                rvpt->render_settings.split_ratio.y = 0.5f;
        }
        ImGui::Text("Render Mode");
        ImGui::PushItemWidth(0);
        dropdown_helper("top_left", rvpt->render_settings.top_left_render_mode, RenderModes);
        if (horizontal_split)
        {
            dropdown_helper("top_right", rvpt->render_settings.top_right_render_mode, RenderModes);
            if (vertical_split)
            {
                dropdown_helper("bottom_left", rvpt->render_settings.bottom_left_render_mode,
                                RenderModes);
                dropdown_helper("bottom_right", rvpt->render_settings.bottom_right_render_mode,
                                RenderModes);
            }
            ImGui::SliderFloat("X Split", &rvpt->render_settings.split_ratio.x, 0.f, 1.f);
            if (vertical_split)
                ImGui::SliderFloat("Y Split", &rvpt->render_settings.split_ratio.y, 0.f, 1.f);
        }
        if (!vertical_split)
        {
            rvpt->render_settings.bottom_left_render_mode =
                rvpt->render_settings.top_left_render_mode;
            rvpt->render_settings.bottom_right_render_mode =
                rvpt->render_settings.top_right_render_mode;
        }
        if (!horizontal_split)
        {
            rvpt->render_settings.top_right_render_mode =
                rvpt->render_settings.top_left_render_mode;
            rvpt->render_settings.bottom_left_render_mode =
                rvpt->render_settings.top_left_render_mode;
            rvpt->render_settings.bottom_right_render_mode =
                rvpt->render_settings.top_left_render_mode;
        }
    }
    ImGui::End();

    update_camera_imgui(rvpt->scene_camera);
}

void RVPTDemo::update()
{
    update_imgui();
    rvpt->update();
}

void RVPTDemo::draw()
{
    time.frame_start();
    rvpt->draw();

    auto& current_frame = sync_resources[current_sync_index];

    current_frame.command_fence.wait();
    current_frame.command_buffer.reset();

    uint32_t swapchain_image_index;
    VkResult result = vkAcquireNextImageKHR(device, vkb_swapchain.swapchain, UINT64_MAX,
                                            current_frame.image_avail_sem.get(), VK_NULL_HANDLE,
                                            &swapchain_image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        swapchain_reinit();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        fmt::print(stderr, "Failed to acquire next swapchain image\n");
        assert(false);
    }

    VkImage rvpt_image = rvpt->get_image();

    record_command_buffer(current_frame, swapchain_image_index, rvpt_image);

    if (frames_inflight_fences[swapchain_image_index] != VK_NULL_HANDLE)
    {
        vkWaitForFences(device, 1, &frames_inflight_fences[swapchain_image_index], VK_TRUE,
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
        fmt::print(stderr, "Failed to present swapchain image\n");
        assert(false);
    }
    current_sync_index = (current_sync_index + 1) % sync_resources.size();
    current_frame_index = (current_frame_index + 1) % MAX_FRAMES_IN_FLIGHT;

    time.frame_stop();
}

void RVPTDemo::record_command_buffer(VK::SyncResources& current_frame,
                                     uint32_t swapchain_image_index, VkImage rvpt_image)
{
    current_frame.command_buffer.begin();
    VkCommandBuffer cmd_buf = current_frame.command_buffer.get();

    // Image memory barrier to make sure that compute shader writes are
    // finished before sampling from the texture
    VkImageMemoryBarrier imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageMemoryBarrier.image = rvpt_image;
    imageMemoryBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK::FLAGS_NONE, 0, nullptr, 0,
                         nullptr, 1, &imageMemoryBarrier);

    VkRenderPassBeginInfo rp_begin_info{};
    rp_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin_info.renderPass = render_pass;
    rp_begin_info.framebuffer = framebuffers.at(swapchain_image_index).framebuffer.handle;
    rp_begin_info.renderArea.offset = {0, 0};
    rp_begin_info.renderArea.extent = vkb_swapchain.extent;
    std::array<VkClearValue, 1> clear_values;
    clear_values[0] = {0.0f, 0.0f, 0.0f, 1.0f};
    rp_begin_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
    rp_begin_info.pClearValues = clear_values.data();

    vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    if (show_imgui)
    {
        imgui_impl->draw(cmd_buf, current_frame_index);
    }

    vkCmdEndRenderPass(cmd_buf);
    current_frame.command_buffer.end();
}