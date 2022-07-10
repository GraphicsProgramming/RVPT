#include "window.h"

int main()
{
    auto window = demo::window("RVPT Demo", 1920, 1080);

    while (!window.should_close())
    {
        window.poll();

    }

}