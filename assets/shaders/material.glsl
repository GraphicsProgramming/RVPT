void apply_record(inout Ray ray, inout Record record)
{
    Material mat = record.mat;

    vec3 normal = record.normal;
    if(dot(normal, ray.direction) > 0) normal *= -1;

    if(mat.data.x == 0) // Lambert
    {
        ray.origin = record.intersection;
        float x = rand();
        float y = rand();
        float phi =  2.0f * PI * x;
        float cosTheta = 2.0f * y - 1.0f;
        float a = sqrt(1.0f - cosTheta * cosTheta);
        ray.direction = normalize(normal + vec3(a * cos(phi), a * sin(phi), cosTheta));
        record.reflectiveness = max(0, dot(normal, ray.direction));
    }
    else if (mat.data.x == 1) // Glass
    {
        ray.origin = record.intersection;
        float nit;
        vec3 normal = record.normal;
        if(dot(normal, ray.direction) > 0)
        {
            normal *= -1;
            nit = 1.5f;
        }
        else
        {
            nit = 1. / 1.5f;
        }
        ray.direction = refract(ray.direction, normal, nit);
        record.reflectiveness = 1.f;
    }
    else if (mat.data.x == 2) // Dynamic
    {
        ray.origin = record.intersection;
        ray.direction = mix( reflect(ray.direction, normal), normalize(random_unit_sphere_point()), mat.data.y);
        record.reflectiveness = 1.f;
    }
}
