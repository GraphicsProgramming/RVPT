#include "rvpt.h"

#include <cstdlib>

#include <algorithm>
#include <fstream>

#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <nlohmann/json.hpp>
#include <imgui.h>

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

    glm::vec3 translation(0, 0, -20);

    spheres.emplace_back(glm::vec3(0, 10005, 0) + translation, 10000.f, 0);
    spheres.emplace_back(glm::vec3(5, 0, -3) + translation, 3.f, 1);

    triangles.emplace_back(glm::vec3(-3, 0.5, -3), glm::vec3(3, 0.5, -3), glm::vec3(-3, 0.5, 3), 2);

    materials.emplace_back(glm::vec4(1, 1, 1, 0), glm::vec4(0, 0, 0, 0), Material::Type::LAMBERT);
    materials.emplace_back(glm::vec4(0, 0, 0, 0), glm::vec4(.0, .7, .7, 0),
                           Material::Type::LAMBERT);
    materials.emplace_back(glm::vec4(0.5, 0.5, 0.5, 0), glm::vec4(0, .1, .1, .1),
                           Material::Type::LAMBERT);
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

    fullscreen_tri_render_pass = VK::create_render_pass(vk_device, vkb_swapchain.image_format);

    imgui_impl.emplace(vk_device, *graphics_queue, pipeline_builder, memory_allocator,
                       fullscreen_tri_render_pass, vkb_swapchain.extent, MAX_FRAMES_IN_FLIGHT);

    create_framebuffers();

    rendering_resources = create_rendering_resources();

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        add_per_frame_data();
    }

    return init;
}
bool RVPT::update()
{
    bool moved = last_camera_mat != scene_camera.matrix;
    if (moved)
    {
        render_settings.current_frame = 0;
        last_camera_mat = scene_camera.matrix;
    }
    else
    {
        render_settings.current_frame++;
    }

    for (auto& r : random_numbers) r = (distribution(random_generator));

    per_frame_data[current_frame_index].raytrace_work_fence.wait();
    per_frame_data[current_frame_index].raytrace_work_fence.reset();

    per_frame_data[current_frame_index].camera_uniform.copy_to(scene_camera.get_data());
    per_frame_data[current_frame_index].random_buffer.copy_to(random_numbers);
    per_frame_data[current_frame_index].settings_uniform.copy_to(render_settings);

    float delta = static_cast<float>(time.since_last_frame());

    // haha sphere go up and down go brrrrrrrr
    //    spheres[1].origin.y += sin(delta * .1);

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
            per_frame_data[current_frame_index].debug_vertex_buffer =
                VK::Buffer(vk_device, memory_allocator, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                           vert_byte_size, VK::MemoryUsage::cpu_to_gpu);
        }
        per_frame_data[current_frame_index].debug_vertex_buffer.copy_to(debug_triangles);
        per_frame_data[current_frame_index].debug_camera_uniform.copy_to(
            scene_camera.get_debug_data());
    }

    return true;
}

void RVPT::update_imgui()
{
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
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({160, 120});
    if (ImGui::Begin("Stats", &show_stats))
    {
        ImGui::Text("Frame Time %.4f", time.average_frame_time());
        ImGui::Text("FPS %.2f", 1.0 / time.average_frame_time());
        ImGui::SliderInt("AA", &render_settings.aa, 1, 64);
        ImGui::SliderInt("Max Bounce", &render_settings.max_bounces, 1, 64);
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
        framebuffers.emplace_back(vk_device, fullscreen_tri_render_pass, vkb_swapchain.extent,
                                  image_views);
    }
}

RVPT::RenderingResources RVPT::create_rendering_resources()
{
    std::vector<VkDescriptorSetLayoutBinding> layout_bindings = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};

    auto image_pool = VK::DescriptorPool(vk_device, layout_bindings, MAX_FRAMES_IN_FLIGHT * 2);
    std::vector<VkDescriptorSetLayoutBinding> compute_layout_bindings = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };

    auto raytrace_descriptor_pool =
        VK::DescriptorPool(vk_device, compute_layout_bindings, MAX_FRAMES_IN_FLIGHT);

    auto fullscreen_triangle_pipeline_layout =
        pipeline_builder.create_layout({image_pool.layout()}, {});

    VK::GraphicsPipelineDetails fullscreen_details;
    fullscreen_details.pipeline_layout = fullscreen_triangle_pipeline_layout;
    fullscreen_details.vert_shader = "fullscreen_tri.vert.spv";
    fullscreen_details.frag_shader = "tex_sample.frag.spv";
    fullscreen_details.render_pass = fullscreen_tri_render_pass;
    fullscreen_details.extent = vkb_swapchain.extent;

    auto fullscreen_triangle_pipeline = pipeline_builder.create_pipeline(fullscreen_details);

    auto raytrace_pipeline_layout =
        pipeline_builder.create_layout({raytrace_descriptor_pool.layout()}, {});

    VK::ComputePipelineDetails raytrace_details;
    raytrace_details.pipeline_layout = raytrace_pipeline_layout;
    raytrace_details.compute_shader = "compute_pass.comp.spv";

    auto raytrace_pipeline = pipeline_builder.create_pipeline(raytrace_details);

    std::vector<VkDescriptorSetLayoutBinding> debug_layout_bindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}};

    auto debug_descriptor_pool =
        VK::DescriptorPool(vk_device, debug_layout_bindings, MAX_FRAMES_IN_FLIGHT);
    auto debug_pipeline_layout =
        pipeline_builder.create_layout({debug_descriptor_pool.layout()}, {});

    std::vector<VkVertexInputBindingDescription> binding_desc = {
        {0, sizeof(DebugVertex), VK_VERTEX_INPUT_RATE_VERTEX}};

    std::vector<VkVertexInputAttributeDescription> attribute_desc = {
        {0, binding_desc[0].binding, VK_FORMAT_R32G32B32_SFLOAT, 0},
        {1, binding_desc[0].binding, VK_FORMAT_R32G32B32_SFLOAT, 4}};

    VK::GraphicsPipelineDetails debug_details;
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

    return RVPT::RenderingResources{std::move(image_pool),
                                    std::move(raytrace_descriptor_pool),
                                    std::move(debug_descriptor_pool),
                                    fullscreen_triangle_pipeline_layout,
                                    fullscreen_triangle_pipeline,
                                    raytrace_pipeline_layout,
                                    raytrace_pipeline,
                                    debug_pipeline_layout,
                                    opaque,
                                    wireframe};
}

void RVPT::add_per_frame_data()
{
    auto output_image = VK::Image(
        vk_device, memory_allocator, *graphics_queue, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL, window_ref.get_settings().width, window_ref.get_settings().height,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_LAYOUT_GENERAL,
        static_cast<VkDeviceSize>(window_ref.get_settings().width *
                                  window_ref.get_settings().height * 4),
        VK::MemoryUsage::gpu);

    auto temporal_storage_image = VK::Image(
        vk_device, memory_allocator, *graphics_queue, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL, window_ref.get_settings().width, window_ref.get_settings().height,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_LAYOUT_GENERAL,
        static_cast<VkDeviceSize>(window_ref.get_settings().width *
                                  window_ref.get_settings().height * 4),
        VK::MemoryUsage::gpu);

    auto temp_camera_data = scene_camera.get_data();
    auto camera_uniform =
        VK::Buffer(vk_device, memory_allocator, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                   sizeof(decltype(temp_camera_data)::value_type) * temp_camera_data.size(),
                   VK::MemoryUsage::cpu_to_gpu);
    auto random_buffer =
        VK::Buffer(vk_device, memory_allocator, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                   sizeof(decltype(random_numbers)::value_type) * random_numbers.size(),
                   VK::MemoryUsage::cpu_to_gpu);
    auto settings_uniform =
        VK::Buffer(vk_device, memory_allocator, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                   sizeof(RenderSettings), VK::MemoryUsage::cpu_to_gpu);
    auto sphere_buffer = VK::Buffer(vk_device, memory_allocator, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                    sizeof(Sphere) * spheres.size(), VK::MemoryUsage::cpu_to_gpu);
    auto triangle_buffer =
        VK::Buffer(vk_device, memory_allocator, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                   sizeof(Triangle) * triangles.size(), VK::MemoryUsage::cpu_to_gpu);
    auto material_buffer =
        VK::Buffer(vk_device, memory_allocator, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                   sizeof(Material) * materials.size(), VK::MemoryUsage::cpu_to_gpu);
    auto raytrace_command_buffer =
        VK::CommandBuffer(vk_device, compute_queue.has_value() ? *compute_queue : *graphics_queue);
    auto raytrace_work_fence = VK::Fence(vk_device);

    // descriptor sets
    auto image_descriptor_set = VK::DescriptorSet(rendering_resources->image_pool.allocate());
    auto temporal_image_descriptor_set =
        VK::DescriptorSet(rendering_resources->image_pool.allocate());
    auto raytracing_descriptor_set =
        VK::DescriptorSet(rendering_resources->raytrace_descriptor_pool.allocate());

    // update descriptor sets with resources
    std::vector<VkDescriptorImageInfo> image_descriptor_info = {output_image.descriptor_info()};
    VK::DescriptorUse descriptor_use{0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                     image_descriptor_info};

    auto write_descriptor = descriptor_use.get_write_descriptor_set(image_descriptor_set.set);
    vkUpdateDescriptorSets(vk_device, 1, &write_descriptor, 0, nullptr);

    std::vector<VkDescriptorImageInfo> temporal_image_descriptor_info = {
        temporal_storage_image.descriptor_info()};

    VK::DescriptorUse image_descriptor_use{0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                           image_descriptor_info};

    VK::DescriptorUse temporal_descriptor_use{1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                              temporal_image_descriptor_info};
    auto temporal_image_descriptor =
        temporal_descriptor_use.get_write_descriptor_set(temporal_image_descriptor_set.set);

    std::vector<VkDescriptorBufferInfo> camera_buffer_descriptor_info = {
        camera_uniform.descriptor_info()};
    VK::DescriptorUse camera_buffer_descriptor_use{2, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                   camera_buffer_descriptor_info};

    std::vector<VkDescriptorBufferInfo> random_buffer_descriptor_info = {
        random_buffer.descriptor_info()};
    VK::DescriptorUse random_buffer_descriptor_use{3, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                   random_buffer_descriptor_info};

    std::vector<VkDescriptorBufferInfo> frame_settings_descriptor_info = {
        settings_uniform.descriptor_info()};
    VK::DescriptorUse frame_settings_descriptor_use{4, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                    frame_settings_descriptor_info};

    std::vector<VkDescriptorBufferInfo> sphere_buffer_descriptor_info = {
        sphere_buffer.descriptor_info()};
    VK::DescriptorUse sphere_buffer_descriptor_use{5, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                   sphere_buffer_descriptor_info};

    std::vector<VkDescriptorBufferInfo> triangle_buffer_descriptor_info = {
        triangle_buffer.descriptor_info()};
    VK::DescriptorUse triangle_buffer_descriptor_use{6, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                     triangle_buffer_descriptor_info};

    std::vector<VkDescriptorBufferInfo> material_buffer_descriptor_info = {
        material_buffer.descriptor_info()};
    VK::DescriptorUse material_buffer_descriptor_use{7, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                     material_buffer_descriptor_info};

    std::vector<VkWriteDescriptorSet> write_descriptors = {
        image_descriptor_use.get_write_descriptor_set(raytracing_descriptor_set.set),
        temporal_descriptor_use.get_write_descriptor_set(raytracing_descriptor_set.set),
        camera_buffer_descriptor_use.get_write_descriptor_set(raytracing_descriptor_set.set),
        random_buffer_descriptor_use.get_write_descriptor_set(raytracing_descriptor_set.set),
        frame_settings_descriptor_use.get_write_descriptor_set(raytracing_descriptor_set.set),
        sphere_buffer_descriptor_use.get_write_descriptor_set(raytracing_descriptor_set.set),
        triangle_buffer_descriptor_use.get_write_descriptor_set(raytracing_descriptor_set.set),
        material_buffer_descriptor_use.get_write_descriptor_set(raytracing_descriptor_set.set)};

    vkUpdateDescriptorSets(vk_device, static_cast<uint32_t>(write_descriptors.size()),
                           write_descriptors.data(), 0, nullptr);

    // Debug vis
    auto debug_camera_uniform =
        VK::Buffer(vk_device, memory_allocator, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                   sizeof(glm::mat4), VK::MemoryUsage::cpu_to_gpu);
    auto debug_vertex_buffer =
        VK::Buffer(vk_device, memory_allocator, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                   1000 * sizeof(DebugVertex), VK::MemoryUsage::cpu_to_gpu);

    auto debug_descriptor_set =
        VK::DescriptorSet(rendering_resources->debug_descriptor_pool.allocate());

    std::vector<VkDescriptorBufferInfo> debug_camera_uniform_descriptor_info = {
        debug_camera_uniform.descriptor_info()};
    VK::DescriptorUse debug_camera_uniform_descriptor_use{0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                          debug_camera_uniform_descriptor_info};

    auto debug_write_descriptor =
        debug_camera_uniform_descriptor_use.get_write_descriptor_set(debug_descriptor_set.set);
    vkUpdateDescriptorSets(vk_device, 1, &debug_write_descriptor, 0, nullptr);

    per_frame_data.push_back(RVPT::PerFrameData{
        std::move(output_image), std::move(temporal_storage_image), std::move(camera_uniform),
        std::move(random_buffer), std::move(settings_uniform), std::move(sphere_buffer),
        std::move(triangle_buffer), std::move(material_buffer), std::move(raytrace_command_buffer),
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

        vkCmdDraw(cmd_buf, triangles.size() * 3, 1, 0, 0);
    }

    imgui_impl->draw(cmd_buf, current_frame_index);

    vkCmdEndRenderPass(cmd_buf);
    current_frame.command_buffer.end();
}

void RVPT::record_compute_command_buffer()
{
    auto& command_buffer = per_frame_data[current_frame_index].raytrace_command_buffer;
    command_buffer.begin();
    VkCommandBuffer cmd_buf = command_buffer.get();

    VkImageMemoryBarrier imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageMemoryBarrier.image =
        per_frame_data[current_frame_index].temporal_storage_image.image.handle;
    imageMemoryBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK::FLAGS_NONE, 0, nullptr, 0,
                         nullptr, 1, &imageMemoryBarrier);

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE,
                      pipeline_builder.get_pipeline(rendering_resources->raytrace_pipeline));
    vkCmdBindDescriptorSets(
        cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, rendering_resources->raytrace_pipeline_layout, 0,
        1, &per_frame_data[current_frame_index].raytracing_descriptor_sets.set, 0, 0);

    vkCmdDispatch(cmd_buf, per_frame_data[current_frame_index].output_image.width / 16,
                  per_frame_data[current_frame_index].output_image.height / 16, 1);

    command_buffer.end();
}
