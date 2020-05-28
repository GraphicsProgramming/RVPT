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

    window.add_key_callback([&](int keycode, Window::Action action) {
        if (keycode == GLFW_KEY_ESCAPE && action == Window::Action::RELEASE)
        {
            window.set_close();
        }
        if (keycode == GLFW_KEY_R && action == Window::Action::RELEASE)
        {
            rvpt.reload_shaders();
        }
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
