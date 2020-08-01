#include "rvpt.h"

#include <cstdlib>

#include <algorithm>
#include <fstream>

#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <nlohmann/json.hpp>
#include <imgui.h>

constexpr int RenderModesCount = IM_ARRAYSIZE(RenderModes);

struct DebugVertex
{
    glm::vec3 pos;
    glm::vec3 norm;
};

RVPT::RVPT(Window& window)
    : window_ref(window),
      scene_camera(window.get_aspect_ratio()),
      random_generator(std::random_device{}()),
      distribution(0.0f, 1.0f)
{
    std::ifstream input("project_configuration.json");
    nlohmann::json json;
    input >> json;
    if (json.contains("project_source_dir"))
    {
        source_folder = json["project_source_dir"];
    }

    random_numbers.resize(20480);

    materials.emplace_back(glm::vec4(0.3,0.7,0.1, 0), glm::vec4(0, 0, 0, 0), Material::Type::LAMBERT);
    spheres.emplace_back(glm::vec3(0, -101, 0), 100.f, 0);

    materials.emplace_back(glm::vec4(1.0,1.0,1.0, 1.5), glm::vec4(0,0,0, 0),
                           Material::Type::DIELECTRIC);
    materials.emplace_back(glm::vec4(1.0,1.0,1.0, 1.0/1.5), glm::vec4(0,0,0, 0),
                           Material::Type::DIELECTRIC);
    spheres.emplace_back(glm::vec3(0, 1.5, 0), 1.0f, 1);

    spheres.emplace_back(glm::vec3(0, 1, 5), 0.5f, 1);
    spheres.emplace_back(glm::vec3(0, 1, 5), 0.45f, 2);


    triangles.emplace_back(glm::vec3(-2, 0, -2), glm::vec3(-2, 0, 2), glm::vec3(-2, 2, 2), 3);
    triangles.emplace_back(glm::vec3(2, 0, -2), glm::vec3(2, 0, 2), glm::vec3(2, 2, 2), 4);
    triangles.emplace_back(glm::vec3(-2, 0, -2), glm::vec3(2, 0, -2), glm::vec3(2, 2, -2), 5);
    triangles.emplace_back(glm::vec3(-2, 0, 2), glm::vec3(2, 0, 2), glm::vec3(2, 2, 2), 6);

    materials.emplace_back(glm::vec4(1.0, 0.0, 0.0, 0), glm::vec4(0),
                           Material::Type::LAMBERT);
    materials.emplace_back(glm::vec4(0.0, 1.0, 0.0, 0), glm::vec4(0),
                           Material::Type::LAMBERT);
    materials.emplace_back(glm::vec4(0.0, 0.0, 1.0, 0), glm::vec4(0),
                           Material::Type::LAMBERT);
    materials.emplace_back(glm::vec4(1.0, 1.0, 1.0, 0), glm::vec4(0),
                           Material::Type::MIRROR);
}

RVPT::~RVPT() {}

void RVPT::addRectangle(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, int mat)
{
    triangles.emplace_back(a, b, d, mat);
    triangles.emplace_back(c, a, d, mat);
}

bool RVPT::initialize()
{
    bool init = context_init();
    pipeline_builder = VK::PipelineBuilder(vk_device, source_folder);
    memory_allocator =
        VK::MemoryAllocator(context.device.physical_device.physical_device, vk_device);

    init &= swapchain_init();
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        sync_resources.emplace_back(vk_device, graphics_queue.value(), present_queue.value(),
                                    vkb_swapchain.swapchain);
    }
    frames_inflight_fences.resize(vkb_swapchain.image_count, nullptr);

    fullscreen_tri_render_pass = VK::create_render_pass(vk_device, vkb_swapchain.image_format,
                                                        "fullscreen_image_copy_render_pass");

    imgui_impl.emplace(vk_device, *graphics_queue, pipeline_builder, memory_allocator,
                       fullscreen_tri_render_pass, vkb_swapchain.extent, MAX_FRAMES_IN_FLIGHT);

    create_framebuffers();

    rendering_resources = create_rendering_resources();

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        add_per_frame_data(i);
    }

    return init;
}
bool RVPT::update()
{
    bool moved = last_camera_mat != scene_camera.get_camera_matrix();
    if (moved)
    {
        render_settings.current_frame = 0;
        last_camera_mat = scene_camera.get_camera_matrix();
    }
    else
    {
        render_settings.current_frame++;
    }

    for (auto& r : random_numbers) r = (distribution(random_generator));

    per_frame_data[current_frame_index].raytrace_work_fence.wait();
    per_frame_data[current_frame_index].raytrace_work_fence.reset();

    per_frame_data[current_frame_index].settings_uniform.copy_to(render_settings);
    per_frame_data[current_frame_index].random_buffer.copy_to(random_numbers);
    per_frame_data[current_frame_index].camera_uniform.copy_to(scene_camera.get_data());

    float delta = static_cast<float>(time.since_last_frame());

    per_frame_data[current_frame_index].sphere_buffer.copy_to(spheres);
    per_frame_data[current_frame_index].triangle_buffer.copy_to(triangles);
    per_frame_data[current_frame_index].material_buffer.copy_to(materials);

    if (debug_overlay_enabled)
    {
        std::vector<DebugVertex> debug_triangles;
        debug_triangles.reserve(triangles.size());
        for (auto& tri : triangles)
        {
            glm::vec3 normal{tri.vertex0.w, tri.vertex1.w, tri.vertex2.w};
            debug_triangles.push_back({glm::vec3(tri.vertex0), glm::vec3(1.f, 1.f, 0.f)});
            debug_triangles.push_back({glm::vec3(tri.vertex1), glm::vec3(1.f, 0.f, 1.f)});
            debug_triangles.push_back({glm::vec3(tri.vertex2), glm::vec3(0.f, 1.f, 1.f)});
        }
        size_t vert_byte_size = debug_triangles.size() * sizeof(DebugVertex);
        if (per_frame_data[current_frame_index].debug_vertex_buffer.size() < vert_byte_size)
        {
            per_frame_data[current_frame_index].debug_vertex_buffer = VK::Buffer(
                vk_device, memory_allocator, "debug_vertex_buffer",
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vert_byte_size, VK::MemoryUsage::cpu_to_gpu);
        }
        per_frame_data[current_frame_index].debug_vertex_buffer.copy_to(debug_triangles);
        per_frame_data[current_frame_index].debug_camera_uniform.copy_to(
            scene_camera.get_pv_matrix());
    }

    return true;
}

void RVPT::update_imgui()
{
    if (!show_imgui) return;

    ImGuiIO& io = ImGui::GetIO();

    // Setup display size (every frame to accommodate for window resizing)
    int w, h;
    int display_w, display_h;
    auto win_ptr = window_ref.get_window_pointer();
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
    ImGui::SetNextWindowSize({200, 205}, ImGuiCond_Once);
    if (ImGui::Begin("Stats", &show_stats))
    {
        ImGui::PushItemWidth(80);
        ImGui::Text("Frame Time %.4f", time.average_frame_time());
        ImGui::Text("FPS %.2f", 1.0 / time.average_frame_time());
        ImGui::SliderInt("AA", &render_settings.aa, 1, 64);
        ImGui::SliderInt("Max Bounce", &render_settings.max_bounces, 1, 64);

        ImGuiStyle& style = ImGui::GetStyle();
        float w = ImGui::CalcItemWidth();
        float spacing = style.ItemInnerSpacing.x;
        float button_sz = ImGui::GetFrameHeight();

        static const char* current_item_text = RenderModes[render_settings.render_mode];

        ImGui::Text("Render Mode");
        if (ImGui::ArrowButton("##l", ImGuiDir_Left))
        {
            render_settings.render_mode =
                (render_settings.render_mode - 1 + RenderModesCount) % RenderModesCount;
            current_item_text = RenderModes[render_settings.render_mode];
        }
        ImGui::SameLine(0, style.ItemInnerSpacing.x);
        if (ImGui::ArrowButton("##r", ImGuiDir_Right))
        {
            render_settings.render_mode =
                (render_settings.render_mode + 1 + RenderModesCount) % RenderModesCount;
            current_item_text = RenderModes[render_settings.render_mode];
        }
        ImGui::SameLine(0, style.ItemInnerSpacing.x);
        ImGui::PushItemWidth(120);
        if (ImGui::BeginCombo("##custom combo", current_item_text, ImGuiComboFlags_NoArrowButton))
        {
            for (int n = 0; n < IM_ARRAYSIZE(RenderModes); n++)
            {
                bool is_selected = (current_item_text == RenderModes[n]);
                if (ImGui::Selectable(RenderModes[n], is_selected))
                    current_item_text = RenderModes[n];
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PushItemWidth(0);

        if (ImGui::Button("Debug Viz")) toggle_debug();
        if (ImGui::Button("Wireframe Viz")) toggle_wireframe_debug();
    }
    ImGui::End();

    scene_camera.update_imgui();
}

RVPT::draw_return RVPT::draw()
{
    time.frame_start();

    record_compute_command_buffer();

    VK::Queue& compute_submit = compute_queue.has_value() ? *compute_queue : *graphics_queue;
    compute_submit.submit(per_frame_data[current_frame_index].raytrace_command_buffer,
                          per_frame_data[current_frame_index].raytrace_work_fence);

    auto& current_frame = sync_resources[current_sync_index];

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
    current_sync_index = (current_sync_index + 1) % sync_resources.size();
    current_frame_index = (current_frame_index + 1) % per_frame_data.size();

    time.frame_stop();
    return draw_return::success;
}

void RVPT::shutdown()
{
    if (compute_queue) compute_queue->wait_idle();
    graphics_queue->wait_idle();
    present_queue->wait_idle();

    per_frame_data.clear();
    rendering_resources.reset();

    imgui_impl.reset();

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

void replace_all(std::string& str, const std::string& from, const std::string& to)
{
    if (from.empty()) return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();  // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}

void RVPT::reload_shaders()
{
    if (source_folder == "")
    {
        std::cout << "source_folder not set, unable to reload shaders\n";
        return;
    }

#ifdef WIN32
    auto double_backslash = source_folder;
    replace_all(double_backslash, "/", "\\\\");
    auto str = std::string("cd ") + double_backslash + "\\\\assets\\\\shaders && " +
               double_backslash + "\\\\scripts\\\\compile_shaders.bat";
    std::cout << str << "\n";
    std::system(str.c_str());
#elif __unix__
    auto str = std::string("cd ") + source_folder + "/assets/shaders && bash " + source_folder +
               "/scripts/compile_shaders.sh";
    std::system(str.c_str());
#endif
    if (compute_queue) compute_queue->wait_idle();
    graphics_queue->wait_idle();
    present_queue->wait_idle();

    pipeline_builder.recompile_pipelines();
}

void RVPT::toggle_debug() { debug_overlay_enabled = !debug_overlay_enabled; }
void RVPT::toggle_wireframe_debug() { debug_wireframe_mode = !debug_wireframe_mode; }
void RVPT::set_raytrace_mode(int mode) { render_settings.render_mode = mode; }

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
    required_features.fillModeNonSolid = true;

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
    graphics_queue.emplace(vk_device, graphics_queue_index_ret.value(), "graphics_queue");

    auto present_queue_index_ret = context.device.get_queue_index(vkb::QueueType::present);
    if (!present_queue_index_ret)
    {
        std::cerr << "Failed get the present queue: " << dev_ret.error().message() << '\n';

        return false;
    }
    present_queue.emplace(vk_device, present_queue_index_ret.value(), "present_queue");

    auto compute_queue_index_ret =
        context.device.get_dedicated_queue_index(vkb::QueueType::compute);
    if (compute_queue_index_ret)
    {
        compute_queue.emplace(vk_device, compute_queue_index_ret.value(), "compute_queue");
    }

    VK::setup_debug_util_helper(vk_device);

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
    auto ret = swapchain_builder.set_old_swapchain(vkb_swapchain).build();
    if (!ret)
    {
        std::cerr << "Failed to recreate swapchain:" << ret.error().message() << '\n';
        return false;
    }
    vkb::destroy_swapchain(vkb_swapchain);
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
        VK::debug_utils_helper.set_debug_object_name(VK_OBJECT_TYPE_IMAGE_VIEW,
                                                     swapchain_image_views[i],
                                                     "swapchain_image_view_" + std::to_string(i));

        framebuffers.emplace_back(vk_device, fullscreen_tri_render_pass, vkb_swapchain.extent,
                                  image_views, "swapchain_framebuffer_" + std::to_string(i));
    }
}

RVPT::RenderingResources RVPT::create_rendering_resources()
{
    std::vector<VkDescriptorSetLayoutBinding> layout_bindings = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};

    auto image_pool = VK::DescriptorPool(vk_device, layout_bindings, MAX_FRAMES_IN_FLIGHT * 2,
                                         "image_descriptor_pool");
    std::vector<VkDescriptorSetLayoutBinding> compute_layout_bindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };

    auto raytrace_descriptor_pool = VK::DescriptorPool(
        vk_device, compute_layout_bindings, MAX_FRAMES_IN_FLIGHT, "raytrace_descriptor_pool");

    auto fullscreen_triangle_pipeline_layout = pipeline_builder.create_layout(
        {image_pool.layout()}, {}, "fullscreen_triangle_pipeline_layout");

    VK::GraphicsPipelineDetails fullscreen_details;
    fullscreen_details.name = "fullscreen_pipeline";
    fullscreen_details.pipeline_layout = fullscreen_triangle_pipeline_layout;
    fullscreen_details.vert_shader = "fullscreen_tri.vert.spv";
    fullscreen_details.frag_shader = "tex_sample.frag.spv";
    fullscreen_details.render_pass = fullscreen_tri_render_pass;
    fullscreen_details.extent = vkb_swapchain.extent;

    auto fullscreen_triangle_pipeline = pipeline_builder.create_pipeline(fullscreen_details);

    auto raytrace_pipeline_layout = pipeline_builder.create_layout(
        {raytrace_descriptor_pool.layout()}, {}, "raytrace_pipeline_layout");

    VK::ComputePipelineDetails raytrace_details;
    raytrace_details.name = "raytrace_compute_pipeline";
    raytrace_details.pipeline_layout = raytrace_pipeline_layout;
    raytrace_details.compute_shader = "compute_pass.comp.spv";

    auto raytrace_pipeline = pipeline_builder.create_pipeline(raytrace_details);

    std::vector<VkDescriptorSetLayoutBinding> debug_layout_bindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}};

    auto debug_descriptor_pool = VK::DescriptorPool(vk_device, debug_layout_bindings,
                                                    MAX_FRAMES_IN_FLIGHT, "debug_descriptor_pool");
    auto debug_pipeline_layout = pipeline_builder.create_layout({debug_descriptor_pool.layout()},
                                                                {}, "debug_vis_pipeline_layout");

    std::vector<VkVertexInputBindingDescription> binding_desc = {
        {0, sizeof(DebugVertex), VK_VERTEX_INPUT_RATE_VERTEX}};

    std::vector<VkVertexInputAttributeDescription> attribute_desc = {
        {0, binding_desc[0].binding, VK_FORMAT_R32G32B32_SFLOAT, 0},
        {1, binding_desc[0].binding, VK_FORMAT_R32G32B32_SFLOAT, 4}};

    VK::GraphicsPipelineDetails debug_details;
    debug_details.name = "debug_raster_view_pipeline";
    debug_details.pipeline_layout = debug_pipeline_layout;
    debug_details.vert_shader = "debug_vis.vert.spv";
    debug_details.frag_shader = "debug_vis.frag.spv";
    debug_details.render_pass = fullscreen_tri_render_pass;
    debug_details.extent = vkb_swapchain.extent;
    debug_details.binding_desc = binding_desc;
    debug_details.attribute_desc = attribute_desc;
    debug_details.cull_mode = VK_CULL_MODE_NONE;

    auto opaque = pipeline_builder.create_pipeline(debug_details);
    debug_details.polygon_mode = VK_POLYGON_MODE_LINE;
    auto wireframe = pipeline_builder.create_pipeline(debug_details);

    auto temporal_storage_image =
        VK::Image(vk_device, memory_allocator, *graphics_queue, "temporal_storage_image",
                  VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                  window_ref.get_settings().width, window_ref.get_settings().height,
                  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                  static_cast<VkDeviceSize>(window_ref.get_settings().width *
                                            window_ref.get_settings().height * 4),
                  VK::MemoryUsage::gpu);

    return RVPT::RenderingResources{std::move(image_pool),
                                    std::move(raytrace_descriptor_pool),
                                    std::move(debug_descriptor_pool),
                                    fullscreen_triangle_pipeline_layout,
                                    fullscreen_triangle_pipeline,
                                    raytrace_pipeline_layout,
                                    raytrace_pipeline,
                                    debug_pipeline_layout,
                                    opaque,
                                    wireframe,
                                    std::move(temporal_storage_image)};
}

void RVPT::add_per_frame_data(int index)
{
    auto settings_uniform = VK::Buffer(
        vk_device, memory_allocator, "settings_buffer_" + std::to_string(index),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(RenderSettings), VK::MemoryUsage::cpu_to_gpu);
    auto output_image = VK::Image(
        vk_device, memory_allocator, *graphics_queue,
        "raytrace_output_image_" + std::to_string(index), VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL, window_ref.get_settings().width, window_ref.get_settings().height,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_LAYOUT_GENERAL,
        static_cast<VkDeviceSize>(window_ref.get_settings().width *
                                  window_ref.get_settings().height * 4),
        VK::MemoryUsage::gpu);
    auto random_buffer =
        VK::Buffer(vk_device, memory_allocator, "random_data_uniform_" + std::to_string(index),
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                   sizeof(decltype(random_numbers)::value_type) * random_numbers.size(),
                   VK::MemoryUsage::cpu_to_gpu);

    auto temp_camera_data = scene_camera.get_data();
    auto camera_uniform =
        VK::Buffer(vk_device, memory_allocator, "camera_uniform_" + std::to_string(index),
                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                   sizeof(decltype(temp_camera_data)::value_type) * temp_camera_data.size(),
                   VK::MemoryUsage::cpu_to_gpu);
    auto sphere_buffer =
        VK::Buffer(vk_device, memory_allocator, "spheres_buffer_" + std::to_string(index),
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(Sphere) * spheres.size(),
                   VK::MemoryUsage::cpu_to_gpu);
    auto triangle_buffer =
        VK::Buffer(vk_device, memory_allocator, "triangles_buffer_" + std::to_string(index),
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(Triangle) * triangles.size(),
                   VK::MemoryUsage::cpu_to_gpu);
    auto material_buffer =
        VK::Buffer(vk_device, memory_allocator, "materials_buffer_" + std::to_string(index),
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(Material) * materials.size(),
                   VK::MemoryUsage::cpu_to_gpu);
    auto raytrace_command_buffer =
        VK::CommandBuffer(vk_device, compute_queue.has_value() ? *compute_queue : *graphics_queue,
                          "raytrace_command_buffer_" + std::to_string(index));
    auto raytrace_work_fence = VK::Fence(vk_device, "raytrace_work_fence_" + std::to_string(index));

    // descriptor sets
    auto image_descriptor_set = rendering_resources->image_pool.allocate(
        "output_image_descriptor_set_" + std::to_string(index));
    auto temporal_image_descriptor_set = rendering_resources->image_pool.allocate(
        "temporal_image_descriptor_set_" + std::to_string(index));
    auto raytracing_descriptor_set = rendering_resources->raytrace_descriptor_pool.allocate(
        "raytrace_descriptor_set_" + std::to_string(index));

    // update descriptor sets with resources
    std::vector<VK::DescriptorUseVector> image_descriptors;
    image_descriptors.push_back(std::vector{output_image.descriptor_info()});
    rendering_resources->image_pool.update_descriptor_sets(image_descriptor_set, image_descriptors);

    std::vector<VK::DescriptorUseVector> raytracing_descriptors;
    raytracing_descriptors.push_back(std::vector{settings_uniform.descriptor_info()});
    raytracing_descriptors.push_back(std::vector{output_image.descriptor_info()});
    raytracing_descriptors.push_back(
        std::vector{rendering_resources->temporal_storage_image.descriptor_info()});
    raytracing_descriptors.push_back(std::vector{random_buffer.descriptor_info()});
    raytracing_descriptors.push_back(std::vector{camera_uniform.descriptor_info()});
    raytracing_descriptors.push_back(std::vector{sphere_buffer.descriptor_info()});
    raytracing_descriptors.push_back(std::vector{triangle_buffer.descriptor_info()});
    raytracing_descriptors.push_back(std::vector{material_buffer.descriptor_info()});

    rendering_resources->raytrace_descriptor_pool.update_descriptor_sets(raytracing_descriptor_set,
                                                                         raytracing_descriptors);

    // Debug vis
    auto debug_camera_uniform = VK::Buffer(
        vk_device, memory_allocator, "debug_camera_uniform_" + std::to_string(index),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(glm::mat4), VK::MemoryUsage::cpu_to_gpu);
    auto debug_vertex_buffer = VK::Buffer(
        vk_device, memory_allocator, "debug_vertecies_buffer_" + std::to_string(index),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 1000 * sizeof(DebugVertex), VK::MemoryUsage::cpu_to_gpu);

    auto debug_descriptor_set = rendering_resources->debug_descriptor_pool.allocate(
        "debug_descriptor_set_" + std::to_string(index));

    std::vector<VK::DescriptorUseVector> debug_descriptors;
    debug_descriptors.push_back(std::vector{debug_camera_uniform.descriptor_info()});
    rendering_resources->debug_descriptor_pool.update_descriptor_sets(debug_descriptor_set,
                                                                      debug_descriptors);

    per_frame_data.push_back(RVPT::PerFrameData{
        std::move(settings_uniform), std::move(output_image), std::move(random_buffer),
        std::move(camera_uniform), std::move(sphere_buffer), std::move(triangle_buffer),
        std::move(material_buffer), std::move(raytrace_command_buffer),
        std::move(raytrace_work_fence), image_descriptor_set, raytracing_descriptor_set,
        std::move(debug_camera_uniform), std::move(debug_vertex_buffer), debug_descriptor_set});
}

void RVPT::record_command_buffer(VK::SyncResources& current_frame, uint32_t swapchain_image_index)
{
    current_frame.command_buffer.begin();
    VkCommandBuffer cmd_buf = current_frame.command_buffer.get();

    // Image memory barrier to make sure that compute shader writes are
    // finished before sampling from the texture
    VkImageMemoryBarrier imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageMemoryBarrier.image = per_frame_data[current_frame_index].output_image.image.handle;
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

    auto fullscreen_pipe =
        pipeline_builder.get_pipeline(rendering_resources->fullscreen_triangle_pipeline);
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, fullscreen_pipe);

    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            rendering_resources->fullscreen_triangle_pipeline_layout, 0, 1,
                            &per_frame_data[current_frame_index].image_descriptor_set.set, 0,
                            nullptr);
    vkCmdDraw(cmd_buf, 3, 1, 0, 0);

    if (debug_overlay_enabled)
    {
        auto pipeline = pipeline_builder.get_pipeline(
            debug_wireframe_mode ? rendering_resources->debug_wireframe_pipeline
                                 : rendering_resources->debug_opaque_pipeline);
        vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        vkCmdBindDescriptorSets(
            cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rendering_resources->debug_pipeline_layout, 0,
            1, &per_frame_data[current_frame_index].debug_descriptor_sets.set, 0, nullptr);

        bind_vertex_buffer(cmd_buf, per_frame_data[current_frame_index].debug_vertex_buffer);

        vkCmdDraw(cmd_buf, (uint32_t)triangles.size() * 3, 1, 0, 0);
    }

    if (show_imgui){
        imgui_impl->draw(cmd_buf, current_frame_index);
    }

    vkCmdEndRenderPass(cmd_buf);
    current_frame.command_buffer.end();
}

void RVPT::record_compute_command_buffer()
{
    auto& command_buffer = per_frame_data[current_frame_index].raytrace_command_buffer;
    command_buffer.begin();
    VkCommandBuffer cmd_buf = command_buffer.get();

    VkImageMemoryBarrier in_temporal_image_barrier = {};
    in_temporal_image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    in_temporal_image_barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    in_temporal_image_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    in_temporal_image_barrier.image = rendering_resources->temporal_storage_image.image.handle;
    in_temporal_image_barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    in_temporal_image_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    in_temporal_image_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    uint32_t queue_family =
        compute_queue.has_value() ? compute_queue->get_family() : graphics_queue->get_family();
    in_temporal_image_barrier.dstQueueFamilyIndex = queue_family;
    in_temporal_image_barrier.srcQueueFamilyIndex = queue_family;

    vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK::FLAGS_NONE, 0, nullptr, 0,
                         nullptr, 1, &in_temporal_image_barrier);

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE,
                      pipeline_builder.get_pipeline(rendering_resources->raytrace_pipeline));
    vkCmdBindDescriptorSets(
        cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, rendering_resources->raytrace_pipeline_layout, 0,
        1, &per_frame_data[current_frame_index].raytracing_descriptor_sets.set, 0, 0);

    vkCmdDispatch(cmd_buf, per_frame_data[current_frame_index].output_image.width / 16,
                  per_frame_data[current_frame_index].output_image.height / 16, 1);

    command_buffer.end();
}
