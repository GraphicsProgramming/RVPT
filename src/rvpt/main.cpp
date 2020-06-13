//
// Created by AregevDev on 23/04/2020.
//

#include <iostream>

#include <imgui.h>

#include "rvpt.h"

void update_camera(Window& window, RVPT& rvpt)
{
    glm::vec3 movement{};
    double frameDelta = rvpt.time.since_last_frame();
    if (window.is_key_down(Window::KeyCode::KEY_LEFT_SHIFT)) frameDelta *= 2;
    if (window.is_key_down(Window::KeyCode::KEY_LEFT_CONTROL))
        movement.y += static_cast<float>(3.0 * frameDelta);
    if (window.is_key_down(Window::KeyCode::SPACE))
        movement.y -= static_cast<float>(3.0 * frameDelta);
    if (window.is_key_down(Window::KeyCode::KEY_W))
        movement.z -= static_cast<float>(3.0 * frameDelta);
    if (window.is_key_down(Window::KeyCode::KEY_S))
        movement.z += static_cast<float>(3.0 * frameDelta);
    if (window.is_key_down(Window::KeyCode::KEY_D))
        movement.x -= static_cast<float>(3.0 * frameDelta);
    if (window.is_key_down(Window::KeyCode::KEY_A))
        movement.x += static_cast<float>(3.0 * frameDelta);
    rvpt.scene_camera.move(movement.x, movement.y, movement.z);
    if (window.is_key_down(Window::KeyCode::KEY_RIGHT)) rvpt.scene_camera.rotate(0, 0.03f);
    if (window.is_key_down(Window::KeyCode::KEY_LEFT)) rvpt.scene_camera.rotate(0, -0.03f);
    if (window.is_key_down(Window::KeyCode::KEY_UP)) rvpt.scene_camera.rotate(-0.03f, 0);
    if (window.is_key_down(Window::KeyCode::KEY_DOWN)) rvpt.scene_camera.rotate(0.03f, 0);
}

int main()
{
    ImGui::CreateContext();

    Window::Settings settings;
    settings.width = 1024;
    settings.height = 512;
    Window window(settings);

    RVPT rvpt(window);

    bool rvpt_init_ret = rvpt.initialize();
    if (!rvpt_init_ret)
    {
        std::cout << "failed to initialize RVPT\n";
        return 0;
    }

    while (!window.should_close())
    {
        window.poll_events();
        if (window.is_key_down(Window::KeyCode::KEY_ESCAPE)) window.set_close();
        if (window.is_key_down(Window::KeyCode::KEY_R)) rvpt.reload_shaders();
        update_camera(window, rvpt);
        ImGui::NewFrame();
        rvpt.update_imgui();
        rvpt.update();
        rvpt.draw();
    }
    rvpt.shutdown();

    return 0;
}
