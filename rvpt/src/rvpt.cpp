#include "rvpt/rvpt.h"

#include <cstdlib>

#include <algorithm>
#include <fstream>

#include "rvpt_config.h"

struct DebugVertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
};

bool RVPT::PreviousFrameState::operator==(RVPT::PreviousFrameState const& right)
{
    return glm::equal(settings.split_ratio, right.settings.split_ratio) == glm::bvec2(true, true) &&
           settings.top_left_render_mode == right.settings.top_left_render_mode &&
           settings.top_right_render_mode == right.settings.top_right_render_mode &&
           settings.bottom_left_render_mode == right.settings.bottom_left_render_mode &&
           settings.bottom_right_render_mode == right.settings.bottom_right_render_mode &&
           settings.camera_mode == right.settings.camera_mode && camera_data == right.camera_data;
}

RVPT::RVPT(float aspect_ratio) noexcept
    : scene_camera(aspect_ratio), random_generator(std::random_device{}()), distribution(0.0f, 1.0f)
{
    source_folder = RVPT_SHADER_SOURCE_DIR;
    random_numbers.resize(20480);
}

RVPT::~RVPT() noexcept {}

bool RVPT::initialize(VkPhysicalDevice _physical_device, VkDevice _device,
                      uint32_t _graphics_queue_index, uint32_t _compute_queue_index,
                      VkExtent2D _extent)
{
    extent = _extent;
    physical_device = _physical_device;
    device = _device;
    graphics_queue.emplace(device, _graphics_queue_index, "g_queue");
    compute_queue.emplace(device, _compute_queue_index, "c_queue");

    pipeline_builder = VK::PipelineBuilder(device, source_folder);
    memory_allocator = VK::MemoryAllocator(physical_device, device);

    fullscreen_tri_render_pass = VK::create_intermediate_render_pass(
        device, VK_FORMAT_R8G8B8A8_UNORM, VK::get_depth_image_format(physical_device),
        "fullscreen_image_copy_render_pass");

    rendering_resources = create_rendering_resources();

    // Bvh Stuff
    top_level_bvh = bvh_builder.build_bvh(triangles);
    depth_bvh_bounds = top_level_bvh.collect_aabbs_by_depth();
    sorted_triangles = top_level_bvh.permute_primitives(triangles);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        add_per_frame_data(i);
    }

    return true;
}

void RVPT::update()
{
    time.frame_start();
    auto camera_data = scene_camera.get_data();

    render_settings.camera_mode = scene_camera.get_camera_mode();

    if (!(previous_frame_state == RVPT::PreviousFrameState{render_settings, camera_data}))
    {
        render_settings.current_frame = 0;
        previous_frame_state.settings = render_settings;
        previous_frame_state.camera_data = camera_data;
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
    per_frame_data[current_frame_index].camera_uniform.copy_to(camera_data);

    // float delta = static_cast<float>(time.since_last_frame());

    per_frame_data[current_frame_index].bvh_buffer.copy_to(top_level_bvh.nodes);
    per_frame_data[current_frame_index].triangle_buffer.copy_to(sorted_triangles);
    per_frame_data[current_frame_index].material_buffer.copy_to(materials);

    if (debug_overlay_enabled)
    {
        std::vector<DebugVertex> debug_triangles;
        debug_triangles.reserve(triangles.size());
        for (auto& tri : triangles)
        {
            glm::vec3 normal{tri.vertex0.w, tri.vertex1.w, tri.vertex2.w};
            glm::vec3 color{materials[static_cast<size_t>(tri.material_id.x)].albedo};
            debug_triangles.push_back({glm::vec3(tri.vertex0), normal, color});
            debug_triangles.push_back({glm::vec3(tri.vertex1), normal, color});
            debug_triangles.push_back({glm::vec3(tri.vertex2), normal, color});
        }
        size_t vert_byte_size = debug_triangles.size() * sizeof(DebugVertex);
        if (per_frame_data[current_frame_index].debug_vertex_buffer.size() < vert_byte_size)
        {
            per_frame_data[current_frame_index].debug_vertex_buffer = VK::Buffer(
                device, memory_allocator, "debug_vertex_buffer", VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                vert_byte_size, VK::MemoryUsage::cpu_to_gpu);
        }
        per_frame_data[current_frame_index].debug_vertex_buffer.copy_to(debug_triangles);
        per_frame_data[current_frame_index].debug_camera_uniform.copy_to(
            scene_camera.get_pv_matrix());
    }

    if (debug_bvh_enabled)
    {
        bvh_vertex_count = 0;
        std::vector<DebugVertex> bvh_debug_vertices;

        for (int i = view_previous_depths ? 0 : max_bvh_view_depth - 1; i < max_bvh_view_depth; i++)
        {
            const std::vector<AABB>& bounding_boxes = depth_bvh_bounds[i];

            const glm::vec3 colour = {1, 1, 1};
            const glm::vec3 normal = {0, 0, 0};

            if (!bounding_boxes.empty())
            {
                for (const auto& bound : bounding_boxes)
                {
                    bvh_vertex_count += 24;
                    // Doing the vertical lines
                    bvh_debug_vertices.push_back(
                        {{bound.min.x, bound.min.y, bound.min.z}, normal, colour});
                    bvh_debug_vertices.push_back(
                        {{bound.min.x, bound.max.y, bound.min.z}, normal, colour});
                    bvh_debug_vertices.push_back(
                        {{bound.min.x, bound.min.y, bound.max.z}, normal, colour});
                    bvh_debug_vertices.push_back(
                        {{bound.min.x, bound.max.y, bound.max.z}, normal, colour});
                    bvh_debug_vertices.push_back(
                        {{bound.max.x, bound.min.y, bound.min.z}, normal, colour});
                    bvh_debug_vertices.push_back(
                        {{bound.max.x, bound.max.y, bound.min.z}, normal, colour});
                    bvh_debug_vertices.push_back(
                        {{bound.max.x, bound.min.y, bound.max.z}, normal, colour});
                    bvh_debug_vertices.push_back(
                        {{bound.max.x, bound.max.y, bound.max.z}, normal, colour});

                    // Doing the "top" of the box
                    bvh_debug_vertices.push_back(
                        {{bound.min.x, bound.max.y, bound.min.z}, normal, colour});
                    bvh_debug_vertices.push_back(
                        {{bound.min.x, bound.max.y, bound.max.z}, normal, colour});
                    bvh_debug_vertices.push_back(
                        {{bound.max.x, bound.max.y, bound.min.z}, normal, colour});
                    bvh_debug_vertices.push_back(
                        {{bound.max.x, bound.max.y, bound.max.z}, normal, colour});
                    bvh_debug_vertices.push_back(
                        {{bound.min.x, bound.max.y, bound.max.z}, normal, colour});
                    bvh_debug_vertices.push_back(
                        {{bound.max.x, bound.max.y, bound.max.z}, normal, colour});
                    bvh_debug_vertices.push_back(
                        {{bound.min.x, bound.max.y, bound.min.z}, normal, colour});
                    bvh_debug_vertices.push_back(
                        {{bound.max.x, bound.max.y, bound.min.z}, normal, colour});

                    // Doing the "bottom" of the box
                    bvh_debug_vertices.push_back(
                        {{bound.min.x, bound.min.y, bound.min.z}, normal, colour});
                    bvh_debug_vertices.push_back(
                        {{bound.min.x, bound.min.y, bound.max.z}, normal, colour});
                    bvh_debug_vertices.push_back(
                        {{bound.max.x, bound.min.y, bound.min.z}, normal, colour});
                    bvh_debug_vertices.push_back(
                        {{bound.max.x, bound.min.y, bound.max.z}, normal, colour});
                    bvh_debug_vertices.push_back(
                        {{bound.min.x, bound.min.y, bound.max.z}, normal, colour});
                    bvh_debug_vertices.push_back(
                        {{bound.max.x, bound.min.y, bound.max.z}, normal, colour});
                    bvh_debug_vertices.push_back(
                        {{bound.min.x, bound.min.y, bound.min.z}, normal, colour});
                    bvh_debug_vertices.push_back(
                        {{bound.max.x, bound.min.y, bound.min.z}, normal, colour});
                }
            }
        }

        size_t debug_vert_size = bvh_debug_vertices.size() * sizeof(DebugVertex);
        if (per_frame_data[current_frame_index].debug_bvh_vertex_buffer.size() < debug_vert_size)
        {
            per_frame_data[current_frame_index].debug_bvh_vertex_buffer = VK::Buffer(
                device, memory_allocator, "debug_bvh_buffer", VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                debug_vert_size, VK::MemoryUsage::cpu_to_gpu);
        }
        per_frame_data[current_frame_index].debug_bvh_vertex_buffer.copy_to(bvh_debug_vertices);
        per_frame_data[current_frame_index].debug_bvh_camera_uniform.copy_to(
            scene_camera.get_pv_matrix());
    }
}

void RVPT::draw()
{
    record_compute_command_buffer();

    VK::Queue& compute_submit = compute_queue.has_value() ? *compute_queue : *graphics_queue;
    compute_submit.submit(per_frame_data[current_frame_index].raytrace_command_buffer,
                          per_frame_data[current_frame_index].raytrace_work_fence);

    time.frame_stop();
}

VkImage RVPT::get_image() { return per_frame_data[current_frame_index].output_image.image.handle; }

void RVPT::shutdown()
{
    vkDeviceWaitIdle(device);

    VK::destroy_render_pass(device, fullscreen_tri_render_pass);

    memory_allocator.shutdown();
    pipeline_builder.shutdown();
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
        fmt::print("source_folder not set, unable to reload shaders\n");
        return;
    }
    fmt::print("Compiling Shaders:\n");
#ifdef WIN32
    auto double_backslash = source_folder;
    replace_all(double_backslash, "/", "\\\\");
    std::string str = fmt::format(
        "cd {0}\\\\assets\\\\shaders && {0}\\\\scripts\\\\compile_shaders.bat", double_backslash);
    std::system(str.c_str());
#elif __unix__
    std::string str =
        fmt::format("cd {0}/assets/shaders && bash {0}/scripts/compile_shaders.sh", source_folder);
    std::system(str.c_str());
#endif
    vkDeviceWaitIdle(device);

    pipeline_builder.recompile_pipelines();
}

void RVPT::toggle_debug() { debug_overlay_enabled = !debug_overlay_enabled; }
void RVPT::toggle_wireframe_debug() { debug_wireframe_mode = !debug_wireframe_mode; }
void RVPT::toggle_bvh_debug() { debug_bvh_enabled = !debug_bvh_enabled; }
void RVPT::toggle_view_last_bvh_depths() { view_previous_depths = !view_previous_depths; }
void RVPT::set_raytrace_mode(int mode) { render_settings.top_left_render_mode = mode; }

RVPT::RenderingResources RVPT::create_rendering_resources()
{
    std::vector<VkDescriptorSetLayoutBinding> layout_bindings = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};

    auto image_pool = VK::DescriptorPool(device, layout_bindings, MAX_FRAMES_IN_FLIGHT * 2,
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
        device, compute_layout_bindings, MAX_FRAMES_IN_FLIGHT, "raytrace_descriptor_pool");

    auto fullscreen_triangle_pipeline_layout = pipeline_builder.create_layout(
        {image_pool.layout()}, {}, "fullscreen_triangle_pipeline_layout");

    VK::GraphicsPipelineDetails fullscreen_details;
    fullscreen_details.name = "fullscreen_pipeline";
    fullscreen_details.pipeline_layout = fullscreen_triangle_pipeline_layout;
    fullscreen_details.vert_shader = "fullscreen_tri.vert.spv";
    fullscreen_details.frag_shader = "tex_sample.frag.spv";
    fullscreen_details.render_pass = fullscreen_tri_render_pass;
    fullscreen_details.extent = extent;

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

    auto debug_descriptor_pool = VK::DescriptorPool(device, debug_layout_bindings,
                                                    MAX_FRAMES_IN_FLIGHT, "debug_descriptor_pool");
    auto debug_pipeline_layout = pipeline_builder.create_layout({debug_descriptor_pool.layout()},
                                                                {}, "debug_vis_pipeline_layout");

    std::vector<VkVertexInputBindingDescription> binding_desc = {
        {0, sizeof(DebugVertex), VK_VERTEX_INPUT_RATE_VERTEX}};

    std::vector<VkVertexInputAttributeDescription> attribute_desc = {
        {0, binding_desc[0].binding, VK_FORMAT_R32G32B32_SFLOAT, 0},
        {1, binding_desc[0].binding, VK_FORMAT_R32G32B32_SFLOAT, 12},
        {2, binding_desc[0].binding, VK_FORMAT_R32G32B32_SFLOAT, 24}};

    VkPipelineDepthStencilStateCreateInfo depth_stencil_info{};
    depth_stencil_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    VK::GraphicsPipelineDetails debug_details;
    debug_details.name = "debug_raster_view_pipeline";
    debug_details.pipeline_layout = debug_pipeline_layout;
    debug_details.vert_shader = "debug_vis.vert.spv";
    debug_details.frag_shader = "debug_vis.frag.spv";
    debug_details.render_pass = fullscreen_tri_render_pass;
    debug_details.extent = extent;
    debug_details.binding_desc = binding_desc;
    debug_details.attribute_desc = attribute_desc;
    debug_details.cull_mode = VK_CULL_MODE_NONE;
    debug_details.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    debug_details.enable_depth = true;

    auto opaque = pipeline_builder.create_pipeline(debug_details);
    debug_details.polygon_mode = VK_POLYGON_MODE_LINE;
    auto wireframe = pipeline_builder.create_pipeline(debug_details);

    /*
     * Start BVH Debug Setup
     */

    std::vector<VkDescriptorSetLayoutBinding> debug_bvh_layout_bindings = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}};

    auto debug_bvh_descriptor_pool = VK::DescriptorPool(
        device, debug_layout_bindings, MAX_FRAMES_IN_FLIGHT, "debug_bvh_descriptor_pool");
    auto debug_bvh_pipeline_layout = pipeline_builder.create_layout(
        {debug_descriptor_pool.layout()}, {}, "debug_bvh_pipeline_layout");

    std::vector<VkVertexInputBindingDescription> bvh_binding_desc = {
        {0, sizeof(DebugVertex), VK_VERTEX_INPUT_RATE_VERTEX}};

    std::vector<VkVertexInputAttributeDescription> bvh_attribute_desc = {
        {0, binding_desc[0].binding, VK_FORMAT_R32G32B32_SFLOAT, 0},
        {1, binding_desc[0].binding, VK_FORMAT_R32G32B32_SFLOAT, 12},
        {2, binding_desc[0].binding, VK_FORMAT_R32G32B32_SFLOAT, 24}};

    VK::GraphicsPipelineDetails bvh_debug_details;
    bvh_debug_details.name = "debug_bvh_view_pipeline";
    bvh_debug_details.pipeline_layout = debug_bvh_pipeline_layout;
    bvh_debug_details.vert_shader = "debug_vis.vert.spv";
    bvh_debug_details.frag_shader = "debug_vis.frag.spv";
    bvh_debug_details.render_pass = fullscreen_tri_render_pass;
    bvh_debug_details.extent = extent;
    bvh_debug_details.binding_desc = bvh_binding_desc;
    bvh_debug_details.attribute_desc = bvh_attribute_desc;
    bvh_debug_details.polygon_mode = VK_POLYGON_MODE_LINE;
    bvh_debug_details.primitive_topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    bvh_debug_details.cull_mode = VK_CULL_MODE_NONE;
    bvh_debug_details.enable_depth = true;

    auto bvh_pipline = pipeline_builder.create_pipeline(bvh_debug_details);

    /*
     * End BVH Debug Setup
     */

    auto temporal_storage_image =
        VK::Image(device, memory_allocator, *graphics_queue, "temporal_storage_image",
                  VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, extent,
                  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                  VK_IMAGE_ASPECT_COLOR_BIT, VK::MemoryUsage::gpu);

    VkFormat depth_format = VK::get_depth_image_format(physical_device);

    auto depth_image =
        VK::Image(device, memory_allocator, *graphics_queue, "depth_image", depth_format,
                  VK_IMAGE_TILING_OPTIMAL, extent, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                  VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, VK::MemoryUsage::gpu);

    return RVPT::RenderingResources{std::move(image_pool),
                                    std::move(raytrace_descriptor_pool),
                                    std::move(debug_descriptor_pool),
                                    std::move(debug_bvh_descriptor_pool),
                                    fullscreen_triangle_pipeline_layout,
                                    fullscreen_triangle_pipeline,
                                    raytrace_pipeline_layout,
                                    raytrace_pipeline,
                                    debug_pipeline_layout,
                                    opaque,
                                    wireframe,
                                    debug_bvh_pipeline_layout,
                                    bvh_pipline,
                                    std::move(temporal_storage_image),
                                    std::move(depth_image)};
}

void RVPT::add_per_frame_data(int index)
{
    auto settings_uniform = VK::Buffer(
        device, memory_allocator, "settings_buffer_" + std::to_string(index),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(RenderSettings), VK::MemoryUsage::cpu_to_gpu);
    auto output_image = VK::Image(
        device, memory_allocator, *graphics_queue, "raytrace_output_image_" + std::to_string(index),
        VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, extent,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_STORAGE_BIT,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, VK::MemoryUsage::gpu);
    auto random_buffer =
        VK::Buffer(device, memory_allocator, "random_data_uniform_" + std::to_string(index),
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                   sizeof(decltype(random_numbers)::value_type) * random_numbers.size(),
                   VK::MemoryUsage::cpu_to_gpu);

    auto temp_camera_data = scene_camera.get_data();
    auto camera_uniform =
        VK::Buffer(device, memory_allocator, "camera_uniform_" + std::to_string(index),
                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                   sizeof(decltype(temp_camera_data)::value_type) * temp_camera_data.size(),
                   VK::MemoryUsage::cpu_to_gpu);
    auto bvh_buffer =
        VK::Buffer(device, memory_allocator, "bvh_buffer_" + std::to_string(index),
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(BvhNode) * top_level_bvh.nodes.size(),
                   VK::MemoryUsage::cpu_to_gpu);
    auto triangle_buffer =
        VK::Buffer(device, memory_allocator, "triangles_buffer_" + std::to_string(index),
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(Triangle) * triangles.size(),
                   VK::MemoryUsage::cpu_to_gpu);
    auto material_buffer =
        VK::Buffer(device, memory_allocator, "materials_buffer_" + std::to_string(index),
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(Material) * materials.size(),
                   VK::MemoryUsage::cpu_to_gpu);
    auto raytrace_command_buffer =
        VK::CommandBuffer(device, compute_queue.has_value() ? *compute_queue : *graphics_queue,
                          "raytrace_command_buffer_" + std::to_string(index));
    auto raytrace_work_fence = VK::Fence(device, "raytrace_work_fence_" + std::to_string(index),
                                         VK_FENCE_CREATE_SIGNALED_BIT);

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
    raytracing_descriptors.push_back(std::vector{bvh_buffer.descriptor_info()});
    raytracing_descriptors.push_back(std::vector{triangle_buffer.descriptor_info()});
    raytracing_descriptors.push_back(std::vector{material_buffer.descriptor_info()});

    rendering_resources->raytrace_descriptor_pool.update_descriptor_sets(raytracing_descriptor_set,
                                                                         raytracing_descriptors);

    // Debug vis
    auto debug_camera_uniform = VK::Buffer(
        device, memory_allocator, "debug_camera_uniform_" + std::to_string(index),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(glm::mat4), VK::MemoryUsage::cpu_to_gpu);
    auto debug_vertex_buffer = VK::Buffer(
        device, memory_allocator, "debug_vertex_buffer_" + std::to_string(index),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 1000 * sizeof(DebugVertex), VK::MemoryUsage::cpu_to_gpu);

    auto debug_descriptor_set = rendering_resources->debug_descriptor_pool.allocate(
        "debug_descriptor_set_" + std::to_string(index));

    std::vector<VK::DescriptorUseVector> debug_descriptors;
    debug_descriptors.push_back(std::vector{debug_camera_uniform.descriptor_info()});
    rendering_resources->debug_descriptor_pool.update_descriptor_sets(debug_descriptor_set,
                                                                      debug_descriptors);

    // BVH vis
    auto debug_bvh_vertex_buffer = VK::Buffer(
        device, memory_allocator, "debug_bvh_vertex_buffer_" + std::to_string(index),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 1000 * sizeof(DebugVertex), VK::MemoryUsage::cpu_to_gpu);

    auto debug_bvh_camera_uniform = VK::Buffer(
        device, memory_allocator, "debug_bvh_camera_uniform_" + std::to_string(index),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(glm::mat4), VK::MemoryUsage::cpu_to_gpu);

    auto debug_bvh_descriptor_set = rendering_resources->debug_bvh_descriptor_pool.allocate(
        "debug_bvh_descriptor_set_" + std::to_string(index));

    std::vector<VK::DescriptorUseVector> debug_bvh_descriptors;
    debug_bvh_descriptors.push_back(std::vector{debug_bvh_camera_uniform.descriptor_info()});
    rendering_resources->debug_bvh_descriptor_pool.update_descriptor_sets(debug_bvh_descriptor_set,
                                                                          debug_bvh_descriptors);

    auto framebuffer = VK::Framebuffer(
        device, fullscreen_tri_render_pass, extent,
        {output_image.image_view.handle, rendering_resources->depth_buffer.image_view.handle},
        "rvpt_raster_framebuffer_" + std::to_string(index));

    per_frame_data.push_back(RVPT::PerFrameData{
        std::move(settings_uniform), std::move(output_image), std::move(random_buffer),
        std::move(camera_uniform), std::move(bvh_buffer), std::move(triangle_buffer),
        std::move(material_buffer), std::move(raytrace_command_buffer),
        std::move(raytrace_work_fence), image_descriptor_set, raytracing_descriptor_set,
        std::move(debug_camera_uniform), std::move(debug_vertex_buffer), debug_descriptor_set,
        std::move(debug_bvh_camera_uniform), std::move(debug_bvh_vertex_buffer),
        debug_bvh_descriptor_set, std::move(framebuffer)});
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

    in_temporal_image_barrier.dstQueueFamilyIndex = graphics_queue->get_family();
    in_temporal_image_barrier.srcQueueFamilyIndex =
        compute_queue.has_value() ? compute_queue->get_family() : graphics_queue->get_family();

    vkCmdPipelineBarrier(cmd_buf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK::FLAGS_NONE, 0, nullptr, 0,
                         nullptr, 1, &in_temporal_image_barrier);

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE,
                      pipeline_builder.get_pipeline(rendering_resources->raytrace_pipeline));
    vkCmdBindDescriptorSets(
        cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, rendering_resources->raytrace_pipeline_layout, 0,
        1, &per_frame_data[current_frame_index].raytracing_descriptor_sets.set, 0, 0);

    vkCmdDispatch(cmd_buf, per_frame_data[current_frame_index].output_image.extent.width / 16,
                  per_frame_data[current_frame_index].output_image.extent.height / 16, 1);

    command_buffer.end();
}

void RVPT::record_graphics_command_buffer(VK::SyncResources& current_frame)
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

    VkRenderPassBeginInfo rp_begin_info{};
    rp_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin_info.renderPass = fullscreen_tri_render_pass;
    rp_begin_info.framebuffer = per_frame_data[current_frame_index].framebuffer.framebuffer.handle;
    rp_begin_info.renderArea.offset = {0, 0};
    rp_begin_info.renderArea.extent = extent;
    std::array<VkClearValue, 2> clear_values;
    clear_values[0] = {0.0f, 0.0f, 0.0f, 1.0f};
    clear_values[1] = {1.0f, 0};
    rp_begin_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
    rp_begin_info.pClearValues = clear_values.data();

    vkCmdBeginRenderPass(cmd_buf, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{
        0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height),
        0.0f, 1.0f};
    vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, extent};
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

    if (debug_bvh_enabled)
    {
        auto pipeline = pipeline_builder.get_pipeline(rendering_resources->debug_bvh_pipeline);
        vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        vkCmdBindDescriptorSets(
            cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rendering_resources->debug_bvh_layout, 0, 1,
            &per_frame_data[current_frame_index].debug_bvh_descriptor_set.set, 0, nullptr);

        bind_vertex_buffer(cmd_buf, per_frame_data[current_frame_index].debug_bvh_vertex_buffer);

        vkCmdDraw(cmd_buf, (uint32_t)bvh_vertex_count, 1, 0, 0);
    }

    vkCmdEndRenderPass(cmd_buf);
    current_frame.command_buffer.end();
}

void RVPT::add_material(Material material) { materials.emplace_back(material); }

void RVPT::add_triangle(Triangle triangle) { triangles.emplace_back(triangle); }