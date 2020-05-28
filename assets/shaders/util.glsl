Ray get_ray(vec2 pixel)
{
    return Ray(cam.origin, normalize((cam.center + cam.horizontal * pixel.x +cam.vertical * pixel.y) - cam.origin));
}