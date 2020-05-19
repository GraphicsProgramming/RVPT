//
// Created by AregevDev on 23/04/2020.
//

#include <iostream>

#include "rvpt.h"

int main()
{
    RVPT::Settings settings;
    settings.height = 512;
    settings.width = 512;
    RVPT rvpt(settings);
    bool rvpt_init_ret = rvpt.initialize();
    if (!rvpt_init_ret)
    {
        std::cout << "failed to initialize RVPT\n";
        return 0;
    }
    auto glfw_window = rvpt.get_window();

    while (!glfwWindowShouldClose(glfw_window))
    {
        glfwPollEvents();
    }

    return 0;
}
