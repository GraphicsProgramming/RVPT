Ray get_ray(vec2 pixel)
{
    vec4 origin = cam.matrix * vec4(cam.origin, 1);
    vec4 direction = cam.matrix * vec4(normalize((cam.center + cam.horizontal * pixel.x +cam.vertical * pixel.y) - cam.origin), 0);
    return Ray(origin.xyz, direction.xyz);
}
uint base_index = gl_GlobalInvocationID.x + gl_GlobalInvocationID.y % 1024;
int index = 0;
float rand()
{
    return random.nums[index++];
}
