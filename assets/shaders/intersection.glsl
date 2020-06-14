bool intersect_spheres(Ray ray, inout Record record)
{
    float lowest = record.distance;
    for (int i = 0; i < spheres.length(); i++)
    {
        Sphere sphere = spheres[i];
        vec3 oc = ray.origin - sphere.origin;
        float b = dot(oc, ray.direction);
        float c = dot(oc, oc) - sphere.radius * sphere.radius;
        float delta = b * b - c;

        if (delta > 0.0f)
        {
            delta = sqrt(delta);
            float t0 = (-b - delta);
            float t1 = (-b + delta);
            float distance = min(t0, t1);
            if (!(t0 < 0 || t1 < 0) && RAY_MIN_DIST < distance && (distance < lowest || lowest < 0))
            {
                lowest = distance;
                record.hit = true;
                record.distance = distance;
                record.intersection = ray.origin + ray.direction * distance;
                record.normal = normalize(record.intersection - sphere.origin);
                record.mat = materials[int(sphere.mat_id.x)];
                record.albedo = materials[int(sphere.mat_id.x)].albedo.xyz;
                record.emission = materials[int(sphere.mat_id).x].emission.xyz;
            }
        }
    }
    return lowest > 0;
}

bool intersect_triangles(Ray ray, inout Record record)
{
    float lowest = record.distance;
    for(int i = 0; i < triangles.length(); i++){
        Triangle triangle = triangles[i];

        vec3 o = triangle.vert0.xyz;
        vec3 e0 = triangle.vert1.xyz - o;
        vec3 e1 = triangle.vert2.xyz - o;
        vec3 intersectionMat[3] = {ray.direction * -1, e0, e1};

        vec3 c01 = cross(intersectionMat[0], intersectionMat[1]);
        vec3 c12 = cross(intersectionMat[1],intersectionMat[2]);
        vec3 c20 = cross(intersectionMat[2], intersectionMat[0]);

        float det = dot(intersectionMat[0], c12);
        float inverseDet = 1.0f / det;

        vec3 inverseTransposedMat[3] = { c12*inverseDet, c20*inverseDet, c01*inverseDet };

        vec3 dir = ray.origin - o;
        vec3 tuv = vec3(
        dot(inverseTransposedMat[0], dir),
        dot(inverseTransposedMat[1], dir),
        dot(inverseTransposedMat[2], dir));

        if(0 < tuv.x && 0.0f < tuv.y && 0.0f < tuv.z && tuv.y + tuv.z < 1.0f){
            float t = tuv.x;
            if((RAY_MIN_DIST < t && (t < lowest || lowest < 0)))
            {
                lowest = t;
                record.intersection = ray.origin + ray.direction * t;
                record.distance = t;
                record.normal = vec3(triangle.vert0.w, triangle.vert1.w, triangle.vert2.w);
                record.hit = true;
                record.mat = materials[int(triangle.mat_id.x)];
                record.albedo = materials[int(triangle.mat_id.x)].albedo.xyz;
                record.emission = materials[int(triangle.mat_id).x].emission.xyz;
                //            rec.u = tuv.y * vertices[1].u + tuv.z * vertices[2].u + (1.0f - tuv.y - tuv.z) * vertices[0].u;
                //            rec.v = tuv.y * vertices[1].v + tuv.z * vertices[2].v + (1.0f - tuv.y - tuv.z) * vertices[0].v;
            }
        }
    }
    return lowest > 0;
}