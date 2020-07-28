/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/*                                                                          */
/*                                                                          */
/*                                MATERIALS					                */
/*                   									                    */
/*                                                                          */
/*--------------------------------------------------------------------------*/



/*--------------------------------------------------------------------------*/

vec3 mat_eval_Lambert

	(vec3 diffuse)
	
{
	return diffuse * PI;
}

/*--------------------------------------------------------------------------*/

vec3 mat_scatter_Lambert

	(vec3 normal)
	 
{
	vec3 dir = map_cosine_hemisphere_simple (rand(), rand(), normal);
	return dir;
}

/*--------------------------------------------------------------------------*/

vec3 mat_eval_mirror

	(vec3 tint)
	
{
	return tint;
}

/*--------------------------------------------------------------------------*/

vec3 mat_scatter_mirror

	(vec3 dir_in,
	 vec3 normal)
	 
{
	return reflect(dir_in, normal);
}

/*--------------------------------------------------------------------------*/

vec3 handle_material

	(Material_new mat,
	 vec3 dir_in,
	 vec3 normal,
	 out vec3 dir_out)
	
{
	switch (mat.type)
	{
	case 0: /* Lambertian */
		dir_out = mat_scatter_Lambert(normal);
		return mat_eval_Lambert(mat.base_color);
		
	case 1: /* perfect mirror */
		dir_out = mat_scatter_mirror(dir_in, normal);
		return mat_eval_mirror(mat.base_color);
		
	default:
		dir_out = vec3(0);
		return vec3(0.0);
	}
}

/*--------------------------------------------------------------------------*/

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
