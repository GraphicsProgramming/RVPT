Ray get_ray(vec2 pixel)
{
    vec4 origin = cam.matrix * vec4(cam.origin, 1);
    vec4 direction =
        cam.matrix *
        vec4(normalize((cam.center + cam.horizontal * pixel.x + cam.vertical * pixel.y) -
                       cam.origin),
             0);
    return Ray(origin.xyz, direction.xyz);
}
ivec2 image_size = imageSize(result_image);
    uint base_index = (gl_GlobalInvocationID.x + gl_GlobalInvocationID.y * image_size.x) ^ 237283 * image_size.y;
    uint index = 0;

    float rand () {
       return random_source[(base_index ^ ++index * 3927492) % random_source.length()];
    }

vec3 random_unit_sphere_point()
{
    vec3 point;
        point = vec3(rand() * 2 - 1, rand() * 2 - 1, rand() * 2 - 1);

    return point;
}

//float rand() { return (float(base_index + index++ % 100) / 100.0f) * .39472093 * .2937452; }