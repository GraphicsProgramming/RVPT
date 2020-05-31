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

struct Record
{
    bool hit;
    float distance;
    float reflectiveness;
    vec3 emission;
    vec3 albedo;
    vec3 normal;
    vec3 intersection;
};
