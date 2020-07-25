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

    if (window.is_key_down(Window::KeyCode::KEY_LEFT_SHIFT)) frameDelta *= 5;
    if (window.is_key_down(Window::KeyCode::SPACE)) movement.y += 3.0f;
    if (window.is_key_down(Window::KeyCode::KEY_LEFT_CONTROL)) movement.y -= 3.0f;
    if (window.is_key_down(Window::KeyCode::KEY_W)) movement.z += 3.0f;
    if (window.is_key_down(Window::KeyCode::KEY_S)) movement.z -= 3.0f;
    if (window.is_key_down(Window::KeyCode::KEY_D)) movement.x += 3.0f;
    if (window.is_key_down(Window::KeyCode::KEY_A)) movement.x -= 3.0f;

    rvpt.scene_camera.move(static_cast<float>(frameDelta) * movement);

    glm::vec3 rotation{};
    if (window.is_key_down(Window::KeyCode::KEY_RIGHT)) rotation.x = 1.f;
    if (window.is_key_down(Window::KeyCode::KEY_LEFT)) rotation.x = -1.f;
    if (window.is_key_down(Window::KeyCode::KEY_UP)) rotation.y = 1.f;
    if (window.is_key_down(Window::KeyCode::KEY_DOWN)) rotation.y = -1.f;
    rvpt.scene_camera.rotate(rotation);
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
        if (window.is_key_down(Window::KeyCode::KEY_V)) rvpt.toggle_debug();

        update_camera(window, rvpt);
        ImGui::NewFrame();
        rvpt.update_imgui();
        rvpt.update();
        rvpt.draw();
    }
    rvpt.shutdown();

    return 0;
}
