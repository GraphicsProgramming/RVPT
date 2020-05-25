struct Sphere
{
    vec3 origin;
    float radius;
};

struct Ray
{
    vec3 origin;
    vec3 direction;
};

struct Camera
{
    vec3 origin;
    vec3 center;
    vec3 horizontal;
    vec3 vertical;
};
