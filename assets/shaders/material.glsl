void apply_record(inout Ray ray, inout Record record)
{
    Material mat = record.mat;

    vec3 normal = record.normal;
    if(dot(normal, ray.direction) > 0) normal *= -1;

    if(mat.data.x == 0) // Lambert
    {
        ray.origin = record.intersection;
        float u = rand();
        float v = rand();
		ray.direction = normalize(map_cosine_hemisphere_simple(u, v, normal));
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
        ray.direction = reflect(ray.direction, normal); //normalize(random_unit_sphere_point()), mat.data.y);
        record.reflectiveness = 1.f;
    }
}
