//
// Created by AregevDev on 23/04/2020.
//

#include <iostream>

#include "rvpt.h"

int main()
{
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

    // Camera movement callback
    window.add_key_callback([&](int keycode, Window::Action action){
        glm::vec3 movement;
        double frameDelta = rvpt.time.time_since_start();
        switch(keycode)
        {
            case 87:
                movement.z -= 0.00005 * frameDelta;
                break;
            case 83:
                movement.z += 0.00005 * frameDelta;
                break;
            case 68:
                movement.x -= 0.00005 * frameDelta;
                break;
            case 65:
                movement.x += 0.00005 * frameDelta;
                break;
            case 265:
                rvpt.scene_camera.rotate(-0.005, 0);
                break;
            case 264:
                rvpt.scene_camera.rotate(0.005, 0);
                break;
            case 263:
                rvpt.scene_camera.rotate(0, -0.005);
                break;
            case 262:
                rvpt.scene_camera.rotate(0, 0.005);
                break;

        }
        rvpt.scene_camera.move(movement.x, movement.y, movement.z);
    });

    while (!window.should_close())
    {
        window.poll_events();
        rvpt.update();
        rvpt.draw();
    }
    rvpt.shutdown();

    return 0;
}
