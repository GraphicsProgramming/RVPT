//
// Created by AregevDev on 23/04/2020.
//

#include <imgui.h>
#include <fmt/core.h>
#include "rvpt/rvpt.h"

#include "demo.h"

#define TINYOBJLOADER_IMPLEMENTATION  // define this in only *one* .cc
#include "tinyobjloader/tiny_obj_loader.h"

#include "rvpt_demo_config.h"

void load_model(RVPT& rvpt, std::string inputfile, int material_id)
{
    auto asset_path = std::string(RVPT_ASSET_SOURCE_DIR) + "/" + inputfile;
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string warn;
    std::string err;

    tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, asset_path.c_str());

    if (!warn.empty())
    {
        fmt::print("[{}: {}] {}\n", "WARNING", "MODEL-LOADING", warn);
    }

    if (!err.empty())
    {
        fmt::print("[{}: {}] {}\n", "ERROR", "MODEL-LOADING", err);
        exit(-1);
    }

    // Loop over shapes
    for (auto& shape : shapes)
    {
        // Loop over faces(polygon)
        size_t index_offset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++)
        {
            const auto fv = shape.mesh.num_face_vertices[f];

            // Loop over vertices in the face.
            if (fv != 3)
            {
                fmt::print("Shape had a face with more than 3 vertices, skipping");
                continue;
            }
            glm::vec3 vertices[3];

            for (size_t v = 0; v < fv; v++)
            {
                tinyobj::index_t idx = shape.mesh.indices[index_offset + v];
                vertices[v].x = attrib.vertices[3 * idx.vertex_index + 0];
                vertices[v].y = attrib.vertices[3 * idx.vertex_index + 1];
                vertices[v].z = attrib.vertices[3 * idx.vertex_index + 2];
            }
            index_offset += fv;

            rvpt.add_triangle(Triangle(vertices[0], vertices[1], vertices[2], material_id));

            // per-face material
            // shape.mesh.material_ids[f];
        }
    }
}

void update_camera(RVPTDemo& demo, Camera& camera)
{
    glm::vec3 movement{};
    double frameDelta = demo.time.since_last_frame();

    if (demo.window.is_key_held(Window::KeyCode::KEY_LEFT_SHIFT)) frameDelta *= 5;
    if (demo.window.is_key_held(Window::KeyCode::SPACE)) movement.y += 3.0f;
    if (demo.window.is_key_held(Window::KeyCode::KEY_LEFT_CONTROL)) movement.y -= 3.0f;
    if (demo.window.is_key_held(Window::KeyCode::KEY_W)) movement.z += 3.0f;
    if (demo.window.is_key_held(Window::KeyCode::KEY_S)) movement.z -= 3.0f;
    if (demo.window.is_key_held(Window::KeyCode::KEY_D)) movement.x += 3.0f;
    if (demo.window.is_key_held(Window::KeyCode::KEY_A)) movement.x -= 3.0f;

    camera.translate(static_cast<float>(frameDelta) * movement);

    glm::vec3 rotation{};
    float rot_speed = 0.3f;
    if (demo.window.is_key_down(Window::KeyCode::KEY_RIGHT)) rotation.x = rot_speed;
    if (demo.window.is_key_down(Window::KeyCode::KEY_LEFT)) rotation.x = -rot_speed;
    if (demo.window.is_key_down(Window::KeyCode::KEY_UP)) rotation.y = -rot_speed;
    if (demo.window.is_key_down(Window::KeyCode::KEY_DOWN)) rotation.y = rot_speed;
    camera.rotate(rotation);
}

int main()
{
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // WARNING: THIS CODE IS BROKEN AND SHOULD ONLY BE RAN IF YOU KNOW EXACTLY WHAT YOU'RE GETTING
    // YOURSELF INTO //
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

    Window::Settings settings;
    settings.width = 1024;
    settings.height = 512;

    RVPTDemo demo(settings);
    demo.initialize();

    load_model(*demo.rvpt, "models/rabbit.obj", 1);

    // Setup Demo Scene
    demo.rvpt->add_material(
        Material(glm::vec4(1, 1, 1, 0), glm::vec4(0.1, 0.4, 0.6, 0), Material::Type::LAMBERT));
    demo.rvpt->add_material(
        Material(glm::vec4(1.0, 1.0, 1.0, 0), glm::vec4(0), Material::Type::LAMBERT));

    demo.initialize_rvpt();

    demo.window.setup_imgui();
    demo.window.add_mouse_move_callback([&demo](double x, double y) {
        if (demo.window.is_mouse_locked_to_window())
        {
            demo.rvpt->scene_camera.rotate(glm::vec3(x * 0.3f, -y * 0.3f, 0));
        }
    });

    demo.window.add_mouse_click_callback([&demo](Window::Mouse button, Window::Action action) {
        if (button == Window::Mouse::LEFT && action == Window::Action::RELEASE &&
            demo.window.is_mouse_locked_to_window())
        {
            demo.window.set_mouse_window_lock(false);
        }
        else if (button == Window::Mouse::LEFT && action == Window::Action::RELEASE &&
                 !demo.window.is_mouse_locked_to_window())
        {
            if (!ImGui::GetIO().WantCaptureMouse)
            {
                demo.window.set_mouse_window_lock(true);
            }
        }
    });
    while (!demo.window.should_close())
    {
        demo.window.poll_events();
        if (demo.window.is_key_down(Window::KeyCode::KEY_ESCAPE)) demo.window.set_close();
        if (demo.window.is_key_down(Window::KeyCode::KEY_R)) demo.rvpt->reload_shaders();
        if (demo.window.is_key_down(Window::KeyCode::KEY_V)) demo.rvpt->toggle_debug();
        if (demo.window.is_key_up(Window::KeyCode::KEY_ENTER))
        {
            demo.window.set_mouse_window_lock(!demo.window.is_mouse_locked_to_window());
        }

        update_camera(demo, demo.rvpt->scene_camera);
        ImGui::NewFrame();
        demo.update_imgui();
        demo.update();
        demo.draw();
    }
    demo.shutdown();

    return 0;
}
