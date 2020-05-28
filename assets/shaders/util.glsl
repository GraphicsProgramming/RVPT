Ray get_ray(vec2 pixel)
{
    vec4 origin = cam.matrix * vec4(cam.origin, 1);
    vec4 direction = cam.matrix * vec4(normalize((cam.center + cam.horizontal * pixel.x +cam.vertical * pixel.y) - cam.origin), 0);
    return Ray(origin.xyz, direction.xyz);
}