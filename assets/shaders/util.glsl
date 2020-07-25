vec3 right = vec3(1, 0, 0);
vec3 up = vec3(0, 1, 0);
vec3 forward = vec3(0, 0, 1);

Ray get_ray(vec2 pixel)
{
    vec4 origin = cam.matrix * vec4(-forward, 1);
    vec4 direction = cam.matrix * vec4(normalize((-forward + right * cam.displacement.x * pixel.x +
                                                  up * cam.displacement.y * pixel.y)),
                                       0);
    origin.y *= -1.f;
    direction.y *= -1.f;

    return Ray(origin.xyz, direction.xyz);
}
ivec2 image_size = imageSize(result_image);
uint base_index =
    (gl_GlobalInvocationID.x + gl_GlobalInvocationID.y * image_size.x) ^ 237283 * image_size.y;
uint index = 0;

float rand() { return random_source[(base_index ^ ++index * 3927492) % random_source.length()]; }

vec3 random_unit_sphere_point()
{
    vec3 point;
    point = vec3(rand() * 2 - 1, rand() * 2 - 1, rand() * 2 - 1);

    return point;
}

// float rand() { return (float(base_index + index++ % 100) / 100.0f) * .39472093 * .2937452; }