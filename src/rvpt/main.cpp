//
// Created by AregevDev on 23/04/2020.
//

#include <imgui.h>
#include <fmt/core.h>

#include "rvpt.h"

void update_camera(Window& window, RVPT& rvpt)
{
    glm::vec3 movement{};
    double frameDelta = rvpt.time.since_last_frame();

    if (window.is_key_held(Window::KeyCode::KEY_LEFT_SHIFT)) frameDelta *= 5;
    if (window.is_key_held(Window::KeyCode::SPACE)) movement.y += 3.0f;
    if (window.is_key_held(Window::KeyCode::KEY_LEFT_CONTROL)) movement.y -= 3.0f;
    if (window.is_key_held(Window::KeyCode::KEY_W)) movement.z += 3.0f;
    if (window.is_key_held(Window::KeyCode::KEY_S)) movement.z -= 3.0f;
    if (window.is_key_held(Window::KeyCode::KEY_D)) movement.x += 3.0f;
    if (window.is_key_held(Window::KeyCode::KEY_A)) movement.x -= 3.0f;

    rvpt.scene_camera.move(static_cast<float>(frameDelta) * movement);

    glm::vec3 rotation{};
    float rot_speed = 0.3f;
    if (window.is_key_down(Window::KeyCode::KEY_RIGHT)) rotation.x = rot_speed;
    if (window.is_key_down(Window::KeyCode::KEY_LEFT)) rotation.x = -rot_speed;
    if (window.is_key_down(Window::KeyCode::KEY_UP)) rotation.y = -rot_speed;
    if (window.is_key_down(Window::KeyCode::KEY_DOWN)) rotation.y = rot_speed;
    rvpt.scene_camera.rotate(rotation);
}

int main()
{
    Window::Settings settings;
    settings.width = 1024;
    settings.height = 512;
    Window window(settings);

    RVPT rvpt(window);
    // Setup Demo Scene
    rvpt.add_material(Material(glm::vec4(0.3, 0.7, 0.1, 0), glm::vec4(0, 0, 0, 0),
                               Material::Type::LAMBERT));
    rvpt.add_sphere(Sphere(glm::vec3(0, -101, 0), 100.f, 0));

    rvpt.add_material(Material(glm::vec4(1.0, 1.0, 1.0, 1.5), glm::vec4(0, 0, 0, 0),
                               Material::Type::DIELECTRIC));
    rvpt.add_material(Material(glm::vec4(1.0, 1.0, 1.0, 1.0 / 1.5), glm::vec4(0, 0, 0, 0),
                               Material::Type::DIELECTRIC));

    rvpt.add_sphere(Sphere(glm::vec3(0, 1.5, 0), 1.0f, 1));
    rvpt.add_sphere(Sphere(glm::vec3(0, 1, 5), 0.5f, 1));
    rvpt.add_sphere(Sphere(glm::vec3(0, 1, 5), 0.45f, 2));

    rvpt.add_triangle(Triangle(glm::vec3(-2, 0, -2), glm::vec3(-2, 0, 2), glm::vec3(-2, 2, 2), 3));
    rvpt.add_triangle(Triangle(glm::vec3(2, 0, -2), glm::vec3(2, 0, 2), glm::vec3(2, 2, 2), 4));
    rvpt.add_triangle(Triangle(glm::vec3(-2, 0, -2), glm::vec3(2, 0, -2), glm::vec3(2, 2, -2), 5));
    rvpt.add_triangle(Triangle(glm::vec3(-2, 0, 2), glm::vec3(2, 0, 2), glm::vec3(2, 2, 2), 6));

    rvpt.add_material(Material(glm::vec4(1.0, 0.0, 0.0, 0), glm::vec4(0), Material::Type::LAMBERT));
    rvpt.add_material(Material(glm::vec4(0.0, 1.0, 0.0, 0), glm::vec4(0), Material::Type::LAMBERT));
    rvpt.add_material(Material(glm::vec4(0.0, 0.0, 1.0, 0), glm::vec4(0), Material::Type::LAMBERT));
    rvpt.add_material(Material(glm::vec4(1.0, 1.0, 1.0, 0), glm::vec4(0), Material::Type::MIRROR));

    bool rvpt_init_ret = rvpt.initialize();
    if (!rvpt_init_ret)
    {
        fmt::print("failed to initialize RVPT\n");
        return 0;
    }


    window.setup_imgui();
    window.add_mouse_move_callback([&window, &rvpt](double x, double y) {
        if (window.is_mouse_locked_to_window())
        {
            rvpt.scene_camera.rotate(glm::vec3(x * 0.3f, -y * 0.3f, 0));
        }
    });

    window.add_mouse_click_callback([&window](Window::Mouse button, Window::Action action) {
        if (button == Window::Mouse::LEFT && action == Window::Action::RELEASE &&
            window.is_mouse_locked_to_window())
        {
            window.set_mouse_window_lock(false);
        }
        else if (button == Window::Mouse::LEFT && action == Window::Action::RELEASE &&
                 !window.is_mouse_locked_to_window())
        {
            if (!ImGui::GetIO().WantCaptureMouse)
            {
                window.set_mouse_window_lock(true);
            }
        }
    });
    while (!window.should_close())
    {
        window.poll_events();
        if (window.is_key_down(Window::KeyCode::KEY_ESCAPE)) window.set_close();
        if (window.is_key_down(Window::KeyCode::KEY_R)) rvpt.reload_shaders();
        if (window.is_key_down(Window::KeyCode::KEY_V)) rvpt.toggle_debug();
        if (window.is_key_up(Window::KeyCode::KEY_ENTER))
        {
            window.set_mouse_window_lock(!window.is_mouse_locked_to_window());
        }

        update_camera(window, rvpt);
        ImGui::NewFrame();
        rvpt.update_imgui();
        rvpt.update();
        rvpt.draw();
    }
    rvpt.shutdown();

    return 0;
}
